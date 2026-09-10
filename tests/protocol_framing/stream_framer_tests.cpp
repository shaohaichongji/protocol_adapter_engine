#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <thread>

namespace {
std::atomic<bool> g_track_allocations{false};
std::atomic<std::size_t> g_tracked_allocations{0U};
}  // namespace

void* operator new(std::size_t size) {
  if (g_track_allocations.load(std::memory_order_relaxed)) {
    g_tracked_allocations.fetch_add(1U, std::memory_order_relaxed);
  }
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

#include "../../src/config_compiler/config_compiler.h"
#include "../../src/protocol_core/complete_record_codec.h"
#include "../../src/protocol_framing/stream_framer.h"

namespace {

using pae::protocol_framing::ByteView;
using pae::protocol_framing::CreateStreamFramingWorkspace;
using pae::protocol_framing::FrameSink;
using pae::protocol_framing::FrameSinkAction;
using pae::protocol_framing::FramingIssue;
using pae::protocol_framing::FramingLimitOverrides;
using pae::protocol_framing::PushStreamChunk;
using pae::protocol_framing::ResetStreamFramingWorkspace;
using pae::protocol_framing::StreamFramingWorkspace;
using pae::protocol_framing::SubmitApiStatus;
using pae::protocol_framing::SubmitStopReason;

struct CapturedFrames {
  std::array<std::array<std::uint8_t, 32U>, 16U> bytes{};
  std::array<std::size_t, 16U> sizes{};
  std::size_t count = 0U;
  bool stop_after_next = false;
};

FrameSinkAction Capture(ByteView frame, void* user_data) noexcept {
  auto& output = *static_cast<CapturedFrames*>(user_data);
  if (output.count >= output.bytes.size() || frame.size > output.bytes[0].size()) {
    return FrameSinkAction::STOP;
  }
  for (std::size_t index = 0U; index < frame.size; ++index) {
    output.bytes[output.count][index] = frame.data[index];
  }
  output.sizes[output.count] = frame.size;
  ++output.count;
  if (output.stop_after_next) {
    output.stop_after_next = false;
    return FrameSinkAction::STOP;
  }
  return FrameSinkAction::CONTINUE;
}

bool Expect(bool condition, const char* detail) {
  if (!condition) std::cerr << "FAILED: " << detail << '\n';
  return condition;
}

std::string ReplaceOnce(std::string input, const std::string& from, const std::string& to) {
  const std::size_t position = input.find(from);
  if (position != std::string::npos) input.replace(position, from.size(), to);
  return input;
}

bool Equals(const CapturedFrames& frames, std::size_t index,
            std::initializer_list<std::uint8_t> expected) {
  if (index >= frames.count || frames.sizes[index] != expected.size()) return false;
  std::size_t offset = 0U;
  for (const std::uint8_t byte : expected) {
    if (frames.bytes[index][offset++] != byte) return false;
  }
  return true;
}

struct ReentrantContext {
  const pae::protocol_plan::PlanBundle* plan = nullptr;
  StreamFramingWorkspace* workspace = nullptr;
  SubmitApiStatus nested_status = SubmitApiStatus::OK;
};

struct DecodeContext {
  const pae::protocol_plan::PlanBundle* plan = nullptr;
  pae::protocol_core::ExecutionWorkspace* workspace = nullptr;
  std::size_t pipeline_index = 0U;
  pae::protocol_core::CodecStatus status = pae::protocol_core::CodecStatus::INVALID_ARGUMENT;
  std::size_t field_count = 0U;
  std::array<pae::protocol_core::CodecStatus, 4U> statuses{};
  std::array<std::size_t, 4U> field_counts{};
  std::size_t call_count = 0U;
};

FrameSinkAction DecodeFrame(ByteView frame, void* user_data) noexcept {
  auto& context = *static_cast<DecodeContext*>(user_data);
  std::array<pae::protocol_core::DecodedFieldSlot, 8U> slots{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *context.plan, *context.workspace, context.pipeline_index,
      pae::protocol_core::ByteView{frame.data, frame.size}, slots.data(), slots.size());
  context.status = decoded.status;
  context.field_count = decoded.field_count;
  if (context.call_count < context.statuses.size()) {
    context.statuses[context.call_count] = decoded.status;
    context.field_counts[context.call_count] = decoded.field_count;
  }
  ++context.call_count;
  return FrameSinkAction::CONTINUE;
}

struct BlockingSinkContext {
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
};

FrameSinkAction BlockSink(ByteView, void* user_data) noexcept {
  auto& context = *static_cast<BlockingSinkContext*>(user_data);
  context.entered.store(true, std::memory_order_release);
  while (!context.release.load(std::memory_order_acquire)) std::this_thread::yield();
  return FrameSinkAction::CONTINUE;
}

FrameSinkAction Reenter(ByteView, void* user_data) noexcept {
  auto& context = *static_cast<ReentrantContext*>(user_data);
  const auto nested = PushStreamChunk(*context.plan, *context.workspace, 0U, ByteView{},
                                      FrameSink{Capture, nullptr});
  context.nested_status = nested.api_status;
  return FrameSinkAction::CONTINUE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!Expect(compiled.Succeeded(), "Schema 0.9 synthetic config compiles")) {
    if (compiled.Diagnostic() != nullptr) {
      std::cerr << compiled.Diagnostic()->json_pointer << ": " << compiled.Diagnostic()->detail
                << '\n';
    }
    return 1;
  }
  auto plan = std::move(compiled).TakePlan();
  bool ok = true;
  ok &= Expect(plan->SchemaVersion() == "0.9", "Plan retains Schema 0.9");
  ok &= Expect(plan->FramingProfiles().size() == 3U, "all framing profiles freeze");
  ok &= Expect(plan->GetResourceRequirements().max_stream_frame_bytes == 8U,
               "maximum stream frame is budgeted");
  ok &= Expect(plan->GetResourceRequirements().max_sync_bytes == 2U,
               "actual maximum sync bytes are budgeted");
  ok &= Expect(plan->GetResourceRequirements().max_framing_buffer_bytes == 8U,
               "maximum per-Pipeline framing buffer is budgeted without claiming object bytes");

  auto with_sidecar = pae::config_compiler::CompileJsonToPlanWithUiDescription(json, 1024U * 1024U);
  ok &= Expect(with_sidecar.Succeeded() && with_sidecar.Artifacts() != nullptr &&
                   with_sidecar.Artifacts()->Plan()->SchemaVersion() == "0.9",
               "Schema 0.9 Plan and existing description sidecar publish atomically");
  const std::string old_schema =
      ReplaceOnce(json, "\"schema_version\": \"0.9\"", "\"schema_version\": \"0.8\"");
  auto old_reject = pae::config_compiler::CompileJsonToPlan(old_schema);
  ok &= Expect(
      !old_reject.Succeeded() && old_reject.Diagnostic() != nullptr &&
          old_reject.Diagnostic()->stage == pae::config_compiler::CompileStage::STRUCTURAL &&
          old_reject.Diagnostic()->code == pae::config_compiler::CompileError::UNKNOWN_PROPERTY,
      "Schema 0.8 keeps rejecting stream framing properties");
  const std::string invalid_width = ReplaceOnce(json, "\"byte_width\": 1", "\"byte_width\": 3");
  auto width_reject = pae::config_compiler::CompileJsonToPlan(invalid_width);
  ok &= Expect(
      !width_reject.Succeeded() && width_reject.Diagnostic() != nullptr &&
          width_reject.Diagnostic()->stage == pae::config_compiler::CompileStage::STRUCTURAL &&
          width_reject.Diagnostic()->code ==
              pae::config_compiler::CompileError::INVALID_ENUM_VALUE &&
          width_reject.Diagnostic()->json_pointer == "/framing_profiles/2/length_field/byte_width",
      "invalid stream length width fails at its exact JSON Pointer");
  const auto expect_compile_failure = [&ok](const std::string& candidate,
                                            pae::config_compiler::CompileError code,
                                            const char* pointer, const char* detail) {
    auto rejected = pae::config_compiler::CompileJsonToPlan(candidate);
    ok &= Expect(
        !rejected.Succeeded() && rejected.Diagnostic() != nullptr &&
            rejected.Diagnostic()->stage == pae::config_compiler::CompileStage::STRUCTURAL &&
            rejected.Diagnostic()->code == code && rejected.Diagnostic()->json_pointer == pointer,
        detail);
  };
  expect_compile_failure(
      ReplaceOnce(json, "\"frame_length_bytes\": 3", "\"frame_length_bytes\": 3, \"extra\": 1"),
      pae::config_compiler::CompileError::UNKNOWN_PROPERTY, "/framing_profiles/0/extra",
      "unknown framing member fails at its exact JSON Pointer");
  expect_compile_failure(ReplaceOnce(json, "\"sync_bytes\": \"A5 5A\"", "\"sync_bytes\": null"),
                         pae::config_compiler::CompileError::TYPE_MISMATCH,
                         "/framing_profiles/1/sync_bytes",
                         "null sync header fails as a structural type error");
  expect_compile_failure(ReplaceOnce(json, "\"frame_length_bytes\": 3",
                                     "\"frame_length_bytes\": 3, \"sync_bytes\": \"AA\""),
                         pae::config_compiler::CompileError::UNKNOWN_PROPERTY,
                         "/framing_profiles/0/sync_bytes",
                         "foreign strategy member fails at the member JSON Pointer");

  const auto test_length_variant =
      [&ok](std::string variant, std::initializer_list<std::uint8_t> frame, const char* detail) {
        auto compiled_variant = pae::config_compiler::CompileJsonToPlan(variant);
        if (!Expect(compiled_variant.Succeeded(), detail)) {
          ok = false;
          if (compiled_variant.Diagnostic() != nullptr) {
            std::cerr << compiled_variant.Diagnostic()->json_pointer << ": "
                      << compiled_variant.Diagnostic()->detail << '\n';
          }
          return;
        }
        auto variant_plan = std::move(compiled_variant).TakePlan();
        auto workspace = CreateStreamFramingWorkspace(*variant_plan, 2U);
        CapturedFrames captured;
        std::array<std::uint8_t, 8U> bytes{};
        std::size_t size = 0U;
        for (const std::uint8_t byte : frame) bytes[size++] = byte;
        const auto submitted =
            PushStreamChunk(*variant_plan, *workspace.workspace, 2U, ByteView{bytes.data(), size},
                            FrameSink{Capture, &captured});
        ok &= Expect(submitted.api_status == SubmitApiStatus::OK &&
                         submitted.frames_delivered == 1U && Equals(captured, 0U, frame),
                     detail);
      };
  std::string length2_big =
      ReplaceOnce(json, "\"byte_offset\": 2, \"byte_width\": 1}",
                  "\"byte_offset\": 2, \"byte_width\": 2, \"byte_order\": \"big_endian\"}");
  length2_big =
      ReplaceOnce(length2_big, "\"byte_offset\": 2, \"byte_width\": 1},\n          \"encode\"",
                  "\"byte_offset\": 2, \"byte_width\": 2, \"byte_order\": \"big_endian\"},\n       "
                  "   \"encode\"");
  length2_big = ReplaceOnce(length2_big,
                            "\"byte_offset\": 3, \"byte_width\": 2, \"byte_order\": \"big_endian\"",
                            "\"byte_offset\": 4, \"byte_width\": 1");
  test_length_variant(length2_big, {0xC3U, 0x3CU, 0x00U, 0x06U, 0x11U, 0x55U},
                      "two-byte big-endian total length is decoded independently");
  std::string length2_little = ReplaceOnce(
      length2_big,
      "\"length_field\": {\"byte_offset\": 2, \"byte_width\": 2, \"byte_order\": \"big_endian\"}",
      "\"length_field\": {\"byte_offset\": 2, \"byte_width\": 2, \"byte_order\": "
      "\"little_endian\"}");
  length2_little = ReplaceOnce(
      length2_little,
      "\"wire\": {\"codec\": \"unsigned_integer\", \"byte_offset\": 2, \"byte_width\": 2, "
      "\"byte_order\": \"big_endian\"},\n          \"encode\": {\"source\": \"computed\"}",
      "\"wire\": {\"codec\": \"unsigned_integer\", \"byte_offset\": 2, \"byte_width\": 2, "
      "\"byte_order\": \"little_endian\"},\n          \"encode\": {\"source\": \"computed\"}");
  test_length_variant(length2_little, {0xC3U, 0x3CU, 0x06U, 0x00U, 0x11U, 0x55U},
                      "two-byte little-endian total length is decoded independently");

  std::string length4_big =
      ReplaceOnce(json, "\"byte_offset\": 2, \"byte_width\": 1}",
                  "\"byte_offset\": 2, \"byte_width\": 4, \"byte_order\": \"big_endian\"}");
  length4_big =
      ReplaceOnce(length4_big, "\"minimum_frame_length\": 4", "\"minimum_frame_length\": 6");
  length4_big = ReplaceOnce(length4_big, "\"frame_length_bytes\": 6", "\"frame_length_bytes\": 8");
  length4_big = ReplaceOnce(length4_big, "\"byte_offset\": 5, \"bytes\": \"55\"",
                            "\"byte_offset\": 7, \"bytes\": \"55\"");
  length4_big =
      ReplaceOnce(length4_big, "\"byte_offset\": 2, \"byte_width\": 1},\n          \"encode\"",
                  "\"byte_offset\": 2, \"byte_width\": 4, \"byte_order\": \"big_endian\"},\n       "
                  "   \"encode\"");
  length4_big = ReplaceOnce(length4_big,
                            "\"byte_offset\": 3, \"byte_width\": 2, \"byte_order\": \"big_endian\"",
                            "\"byte_offset\": 6, \"byte_width\": 1");
  test_length_variant(length4_big, {0xC3U, 0x3CU, 0x00U, 0x00U, 0x00U, 0x08U, 0x11U, 0x55U},
                      "four-byte big-endian total length is decoded independently");
  std::string length4_little = ReplaceOnce(
      length4_big,
      "\"length_field\": {\"byte_offset\": 2, \"byte_width\": 4, \"byte_order\": \"big_endian\"}",
      "\"length_field\": {\"byte_offset\": 2, \"byte_width\": 4, \"byte_order\": "
      "\"little_endian\"}");
  length4_little = ReplaceOnce(
      length4_little,
      "\"wire\": {\"codec\": \"unsigned_integer\", \"byte_offset\": 2, \"byte_width\": 4, "
      "\"byte_order\": \"big_endian\"},\n          \"encode\": {\"source\": \"computed\"}",
      "\"wire\": {\"codec\": \"unsigned_integer\", \"byte_offset\": 2, \"byte_width\": 4, "
      "\"byte_order\": \"little_endian\"},\n          \"encode\": {\"source\": \"computed\"}");
  test_length_variant(length4_little, {0xC3U, 0x3CU, 0x08U, 0x00U, 0x00U, 0x00U, 0x11U, 0x55U},
                      "four-byte little-endian total length is decoded independently");

  auto fixed_create = CreateStreamFramingWorkspace(*plan, 0U);
  ok &= Expect(fixed_create.api_status == SubmitApiStatus::OK && fixed_create.workspace,
               "fixed workspace creates");
  ok &= Expect(
      fixed_create.accounted_workspace_bytes == fixed_create.workspace->AccountedWorkspaceBytes() &&
          fixed_create.accounted_workspace_bytes == sizeof(StreamFramingWorkspace) + 3U,
      "workspace accounting includes the object and exact bound Pipeline buffer");
  FramingLimitOverrides exact_session;
  exact_session.max_session_memory_bytes = fixed_create.accounted_workspace_bytes;
  const auto exact_workspace = CreateStreamFramingWorkspace(*plan, 0U, exact_session);
  ok &= Expect(exact_workspace.api_status == SubmitApiStatus::OK && exact_workspace.workspace,
               "workspace memory admission accepts the exact required byte count");
  FramingLimitOverrides one_byte_short_session;
  one_byte_short_session.max_session_memory_bytes = fixed_create.accounted_workspace_bytes - 1U;
  const auto one_byte_short_workspace =
      CreateStreamFramingWorkspace(*plan, 0U, one_byte_short_session);
  ok &= Expect(one_byte_short_workspace.api_status == SubmitApiStatus::LIMIT_EXCEEDED &&
                   !one_byte_short_workspace.workspace,
               "workspace memory admission rejects exact requirement minus one");
  FramingLimitOverrides short_sync_limit;
  short_sync_limit.max_sync_bytes = 1U;
  const auto sync_limit_workspace = CreateStreamFramingWorkspace(*plan, 1U, short_sync_limit);
  ok &= Expect(sync_limit_workspace.api_status == SubmitApiStatus::INVALID_PLAN &&
                   !sync_limit_workspace.workspace,
               "workspace creation rejects a Plan sync header one byte above the host limit");
  CapturedFrames fixed_frames;
  const std::array<std::uint8_t, 1U> fixed_a{{0xAAU}};
  g_tracked_allocations.store(0U, std::memory_order_relaxed);
  g_track_allocations.store(true, std::memory_order_relaxed);
  auto result =
      PushStreamChunk(*plan, *fixed_create.workspace, 0U, ByteView{fixed_a.data(), fixed_a.size()},
                      FrameSink{Capture, &fixed_frames});
  g_track_allocations.store(false, std::memory_order_relaxed);
  ok &= Expect(g_tracked_allocations.load(std::memory_order_relaxed) == 0U,
               "first push performs no allocation");
  ok &= Expect(result.bytes_consumed == 1U && result.frames_delivered == 0U &&
                   result.stop_reason == SubmitStopReason::NEED_MORE,
               "fixed first fragment is retained exactly once");
  const std::array<std::uint8_t, 5U> fixed_b{{0x01U, 0x02U, 0xAAU, 0x03U, 0x04U}};
  g_tracked_allocations.store(0U, std::memory_order_relaxed);
  g_track_allocations.store(true, std::memory_order_relaxed);
  result =
      PushStreamChunk(*plan, *fixed_create.workspace, 0U, ByteView{fixed_b.data(), fixed_b.size()},
                      FrameSink{Capture, &fixed_frames});
  g_track_allocations.store(false, std::memory_order_relaxed);
  ok &= Expect(g_tracked_allocations.load(std::memory_order_relaxed) == 0U,
               "repeated push performs no allocation");
  ok &= Expect(result.bytes_consumed == 5U && result.frames_delivered == 2U &&
                   result.stop_reason == SubmitStopReason::INPUT_EXHAUSTED &&
                   Equals(fixed_frames, 0U, {0xAAU, 0x01U, 0x02U}) &&
                   Equals(fixed_frames, 1U, {0xAAU, 0x03U, 0x04U}),
               "fixed chunk yields two independent handwritten frames");

  auto sync_create = CreateStreamFramingWorkspace(*plan, 1U);
  CapturedFrames sync_frames;
  const std::array<std::uint8_t, 2U> sync_a{{0x00U, 0xA5U}};
  result =
      PushStreamChunk(*plan, *sync_create.workspace, 1U, ByteView{sync_a.data(), sync_a.size()},
                      FrameSink{Capture, &sync_frames});
  ok &= Expect(result.bytes_consumed == 2U && result.bytes_discarded == 1U &&
                   result.stop_reason == SubmitStopReason::NEED_MORE,
               "sync prefix crosses chunks while garbage is accounted");
  const std::array<std::uint8_t, 8U> sync_b{
      {0x5AU, 0x01U, 0x02U, 0xFFU, 0xA5U, 0x5AU, 0x03U, 0x04U}};
  result =
      PushStreamChunk(*plan, *sync_create.workspace, 1U, ByteView{sync_b.data(), sync_b.size()},
                      FrameSink{Capture, &sync_frames});
  ok &= Expect(result.bytes_consumed == 8U && result.frames_delivered == 2U &&
                   result.bytes_discarded == 1U &&
                   Equals(sync_frames, 0U, {0xA5U, 0x5AU, 0x01U, 0x02U}) &&
                   Equals(sync_frames, 1U, {0xA5U, 0x5AU, 0x03U, 0x04U}),
               "sync fixed recovers after one garbage byte");

  auto length_create = CreateStreamFramingWorkspace(*plan, 2U);
  CapturedFrames length_frames;
  const std::array<std::uint8_t, 8U> malformed_then_valid{
      {0xC3U, 0x3CU, 0x09U, 0x00U, 0xC3U, 0x3CU, 0x04U, 0x00U}};
  result = PushStreamChunk(*plan, *length_create.workspace, 2U,
                           ByteView{malformed_then_valid.data(), malformed_then_valid.size()},
                           FrameSink{Capture, &length_frames});
  ok &= Expect(result.bytes_consumed == 8U && result.frames_delivered == 1U &&
                   result.malformed_candidates == 1U && result.bytes_discarded == 4U &&
                   result.last_framing_issue == FramingIssue::MALFORMED_LENGTH &&
                   Equals(length_frames, 0U, {0xC3U, 0x3CU, 0x04U, 0x00U}),
               "invalid total length resumes from the candidate's next byte");

  auto zero_length_workspace = CreateStreamFramingWorkspace(*plan, 2U);
  CapturedFrames zero_length_frames;
  const std::array<std::uint8_t, 8U> zero_then_valid{
      {0xC3U, 0x3CU, 0x00U, 0xFFU, 0xC3U, 0x3CU, 0x04U, 0x00U}};
  result = PushStreamChunk(*plan, *zero_length_workspace.workspace, 2U,
                           ByteView{zero_then_valid.data(), zero_then_valid.size()},
                           FrameSink{Capture, &zero_length_frames});
  ok &= Expect(result.frames_delivered == 1U && result.malformed_candidates == 1U &&
                   Equals(zero_length_frames, 0U, {0xC3U, 0x3CU, 0x04U, 0x00U}),
               "zero declared length is rejected immediately and the next sync recovers");

  auto short_length_workspace = CreateStreamFramingWorkspace(*plan, 2U);
  CapturedFrames short_length_frames;
  const std::array<std::uint8_t, 8U> short_then_valid{
      {0xC3U, 0x3CU, 0x03U, 0xFFU, 0xC3U, 0x3CU, 0x04U, 0x00U}};
  result = PushStreamChunk(*plan, *short_length_workspace.workspace, 2U,
                           ByteView{short_then_valid.data(), short_then_valid.size()},
                           FrameSink{Capture, &short_length_frames});
  ok &= Expect(result.frames_delivered == 1U && result.malformed_candidates == 1U &&
                   Equals(short_length_frames, 0U, {0xC3U, 0x3CU, 0x04U, 0x00U}),
               "nonzero length below the minimum is rejected before buffering its payload");

  auto maximum_workspace = CreateStreamFramingWorkspace(*plan, 2U);
  CapturedFrames maximum_frames;
  const std::array<std::uint8_t, 8U> maximum_frame{
      {0xC3U, 0x3CU, 0x08U, 0xC3U, 0x3CU, 0x11U, 0x22U, 0x33U}};
  result = PushStreamChunk(*plan, *maximum_workspace.workspace, 2U,
                           ByteView{maximum_frame.data(), maximum_frame.size() - 1U},
                           FrameSink{Capture, &maximum_frames});
  ok &= Expect(result.frames_delivered == 0U && result.stop_reason == SubmitStopReason::NEED_MORE,
               "maximum legal frame remains a half-frame without scanning payload sync");
  result = PushStreamChunk(*plan, *maximum_workspace.workspace, 2U,
                           ByteView{maximum_frame.data() + maximum_frame.size() - 1U, 1U},
                           FrameSink{Capture, &maximum_frames});
  ok &= Expect(
      result.frames_delivered == 1U &&
          Equals(maximum_frames, 0U, {0xC3U, 0x3CU, 0x08U, 0xC3U, 0x3CU, 0x11U, 0x22U, 0x33U}),
      "maximum legal frame including payload sync is delivered exactly once");

  CapturedFrames stop_frames;
  stop_frames.stop_after_next = true;
  auto stop_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  const std::array<std::uint8_t, 6U> two_fixed{{0xAAU, 0x10U, 0x11U, 0xAAU, 0x12U, 0x13U}};
  result = PushStreamChunk(*plan, *stop_workspace.workspace, 0U,
                           ByteView{two_fixed.data(), two_fixed.size()},
                           FrameSink{Capture, &stop_frames});
  ok &= Expect(result.bytes_consumed == 3U && result.frames_delivered == 1U &&
                   result.stop_reason == SubmitStopReason::SINK_STOP,
               "sink STOP does not pre-consume the remaining suffix");
  result = PushStreamChunk(*plan, *stop_workspace.workspace, 0U,
                           ByteView{two_fixed.data() + 3U, 3U}, FrameSink{Capture, &stop_frames});
  ok &= Expect(result.frames_delivered == 1U && stop_frames.count == 2U &&
                   Equals(stop_frames, 1U, {0xAAU, 0x12U, 0x13U}),
               "host-resubmitted suffix is delivered once");

  FramingLimitOverrides one_frame_limit;
  one_frame_limit.max_frames_per_submit = 1U;
  auto capped_workspace = CreateStreamFramingWorkspace(*plan, 0U, one_frame_limit);
  CapturedFrames capped_frames;
  result = PushStreamChunk(*plan, *capped_workspace.workspace, 0U,
                           ByteView{two_fixed.data(), two_fixed.size()},
                           FrameSink{Capture, &capped_frames});
  ok &= Expect(result.bytes_consumed == two_fixed.size() && result.frames_delivered == 1U &&
                   result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED,
               "callback cap retains one complete frame as internal pending work");
  result = PushStreamChunk(*plan, *capped_workspace.workspace, 0U, ByteView{},
                           FrameSink{Capture, &capped_frames});
  ok &= Expect(
      result.bytes_consumed == 0U && result.frames_delivered == 1U && capped_frames.count == 2U,
      "empty input delivers a callback-capped frame exactly once");

  std::ifstream compaction_input(argv[2], std::ios::binary);
  const std::string compaction_json((std::istreambuf_iterator<char>(compaction_input)),
                                    std::istreambuf_iterator<char>());
  auto compaction_compiled = pae::config_compiler::CompileJsonToPlan(compaction_json);
  ok &= Expect(compaction_compiled.Succeeded(),
               "late length-field compaction config is a legal Schema 0.9 Plan");
  if (!compaction_compiled.Succeeded()) return 1;
  auto compaction_plan = std::move(compaction_compiled).TakePlan();
  FramingLimitOverrides compaction_budget;
  compaction_budget.max_work_units = 5U;
  auto compaction_create = CreateStreamFramingWorkspace(*compaction_plan, 0U, compaction_budget);
  ok &= Expect(
      compaction_create.api_status == SubmitApiStatus::OK && compaction_create.workspace &&
          compaction_create.accounted_workspace_bytes == sizeof(StreamFramingWorkspace) + 16U,
      "minimum work budget accepts exact workspace object and buffer accounting");
  FramingLimitOverrides exact_compaction_session = compaction_budget;
  exact_compaction_session.max_session_memory_bytes = compaction_create.accounted_workspace_bytes;
  const auto exact_compaction_workspace =
      CreateStreamFramingWorkspace(*compaction_plan, 0U, exact_compaction_session);
  ok &= Expect(exact_compaction_workspace.api_status == SubmitApiStatus::OK &&
                   exact_compaction_workspace.workspace,
               "compaction cursor accounting accepts the exact session byte requirement");
  FramingLimitOverrides short_compaction_session = exact_compaction_session;
  --short_compaction_session.max_session_memory_bytes;
  const auto short_compaction_workspace =
      CreateStreamFramingWorkspace(*compaction_plan, 0U, short_compaction_session);
  ok &= Expect(short_compaction_workspace.api_status == SubmitApiStatus::LIMIT_EXCEEDED &&
                   !short_compaction_workspace.workspace,
               "compaction cursor accounting rejects the exact requirement minus one");

  CapturedFrames compacted_frames;
  const std::array<std::uint8_t, 10U> invalid_with_later_sync{{
      0xC3U,
      0x3CU,
      0x10U,
      0x20U,
      0xC3U,
      0x3CU,
      0x30U,
      0x40U,
      0x00U,
      0x00U,
  }};
  std::size_t invalid_offset = 0U;
  std::size_t bounded_calls = 0U;
  while (invalid_offset < invalid_with_later_sync.size() && bounded_calls < 16U) {
    g_tracked_allocations.store(0U, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_relaxed);
    result = PushStreamChunk(*compaction_plan, *compaction_create.workspace, 0U,
                             ByteView{invalid_with_later_sync.data() + invalid_offset,
                                      invalid_with_later_sync.size() - invalid_offset},
                             FrameSink{Capture, &compacted_frames});
    g_track_allocations.store(false, std::memory_order_relaxed);
    ok &= Expect(g_tracked_allocations.load(std::memory_order_relaxed) == 0U,
                 "budgeted recovery input push performs no allocation");
    ok &= Expect(result.work_units_used <= compaction_budget.max_work_units,
                 "budgeted recovery input push stays within its work limit");
    invalid_offset += result.bytes_consumed;
    ++bounded_calls;
  }
  ok &= Expect(invalid_offset == invalid_with_later_sync.size(),
               "caller can resubmit every unconsumed invalid-input suffix");
  bool saw_compaction_budget_stop = false;
  while (result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED && bounded_calls < 32U) {
    g_tracked_allocations.store(0U, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_relaxed);
    result = PushStreamChunk(*compaction_plan, *compaction_create.workspace, 0U, ByteView{},
                             FrameSink{Capture, &compacted_frames});
    g_track_allocations.store(false, std::memory_order_relaxed);
    ok &= Expect(g_tracked_allocations.load(std::memory_order_relaxed) == 0U,
                 "empty compaction continuation performs no allocation");
    ok &= Expect(
        result.bytes_consumed == 0U && result.work_units_used <= compaction_budget.max_work_units,
        "empty compaction continuation preserves consumption and work bounds");
    saw_compaction_budget_stop |= result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED &&
                                  result.work_units_used == compaction_budget.max_work_units;
    ++bounded_calls;
  }
  ok &= Expect(saw_compaction_budget_stop && result.stop_reason == SubmitStopReason::NEED_MORE &&
                   compaction_create.workspace->BufferedBytes() == 6U &&
                   compaction_create.workspace->TotalMalformedCandidates() == 1U &&
                   compaction_create.workspace->TotalDiscardedBytes() == 4U,
               "overlapping segmented compaction reaches NEED_MORE in finitely many empty pushes");

  const std::array<std::uint8_t, 6U> valid_suffix{{
      0x50U,
      0x60U,
      0x00U,
      0x0CU,
      0x70U,
      0x80U,
  }};
  std::size_t suffix_offset = 0U;
  while ((suffix_offset < valid_suffix.size() ||
          result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED) &&
         bounded_calls < 48U) {
    const ByteView suffix =
        suffix_offset < valid_suffix.size()
            ? ByteView{valid_suffix.data() + suffix_offset, valid_suffix.size() - suffix_offset}
            : ByteView{};
    result = PushStreamChunk(*compaction_plan, *compaction_create.workspace, 0U, suffix,
                             FrameSink{Capture, &compacted_frames});
    ok &= Expect(result.work_units_used <= compaction_budget.max_work_units,
                 "post-compaction delivery stays within the work limit");
    suffix_offset += result.bytes_consumed;
    ++bounded_calls;
  }
  ok &= Expect(
      suffix_offset == valid_suffix.size() && compacted_frames.count == 1U &&
          Equals(compacted_frames, 0U,
                 {0xC3U, 0x3CU, 0x30U, 0x40U, 0x00U, 0x00U, 0x50U, 0x60U, 0x00U, 0x0CU, 0x70U,
                  0x80U}) &&
          compaction_create.workspace->TotalDiscardedBytes() == 4U,
      "segmented overlap preserves every retained byte and delivers the next frame exactly once");

  auto reset_during_compaction =
      CreateStreamFramingWorkspace(*compaction_plan, 0U, compaction_budget);
  std::size_t reset_offset = 0U;
  std::size_t reset_calls = 0U;
  do {
    const ByteView remainder{invalid_with_later_sync.data() + reset_offset,
                             invalid_with_later_sync.size() - reset_offset};
    result = PushStreamChunk(*compaction_plan, *reset_during_compaction.workspace, 0U, remainder,
                             FrameSink{Capture, &compacted_frames});
    reset_offset += result.bytes_consumed;
    ++reset_calls;
  } while ((reset_offset < invalid_with_later_sync.size() ||
            reset_during_compaction.workspace->TotalMalformedCandidates() == 0U ||
            result.work_units_used != compaction_budget.max_work_units) &&
           reset_calls < 16U);
  ok &= Expect(reset_offset == invalid_with_later_sync.size() &&
                   result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED &&
                   result.work_units_used == compaction_budget.max_work_units &&
                   reset_during_compaction.workspace->BufferedBytes() == 10U &&
                   reset_during_compaction.workspace->TotalDiscardedBytes() == 0U,
               "test reaches a partially moved compaction before explicit reset");
  ok &= Expect(ResetStreamFramingWorkspace(*compaction_plan, *reset_during_compaction.workspace,
                                           0U) == SubmitApiStatus::OK &&
                   reset_during_compaction.workspace->BufferedBytes() == 0U,
               "explicit reset clears a partially moved compaction");
  CapturedFrames after_reset_frames;
  const std::array<std::uint8_t, 12U> valid_after_reset{{
      0xC3U,
      0x3CU,
      0x01U,
      0x02U,
      0x03U,
      0x04U,
      0x05U,
      0x06U,
      0x00U,
      0x0CU,
      0x07U,
      0x08U,
  }};
  std::size_t after_reset_offset = 0U;
  for (std::size_t call = 0U; call < 16U && after_reset_frames.count == 0U; ++call) {
    const ByteView remainder = after_reset_offset < valid_after_reset.size()
                                   ? ByteView{valid_after_reset.data() + after_reset_offset,
                                              valid_after_reset.size() - after_reset_offset}
                                   : ByteView{};
    result = PushStreamChunk(*compaction_plan, *reset_during_compaction.workspace, 0U, remainder,
                             FrameSink{Capture, &after_reset_frames});
    after_reset_offset += result.bytes_consumed;
    ok &= Expect(result.work_units_used <= compaction_budget.max_work_units,
                 "post-reset submission stays within the work limit");
  }
  ok &= Expect(after_reset_offset == valid_after_reset.size() && after_reset_frames.count == 1U &&
                   Equals(after_reset_frames, 0U,
                          {0xC3U, 0x3CU, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x00U, 0x0CU,
                           0x07U, 0x08U}),
               "workspace accepts one complete frame after resetting partial compaction");

  FramingLimitOverrides submit_limit;
  submit_limit.max_submit_bytes = 4U;
  auto limited_workspace = CreateStreamFramingWorkspace(*plan, 0U, submit_limit);
  CapturedFrames limited_frames;
  result = PushStreamChunk(*plan, *limited_workspace.workspace, 0U,
                           ByteView{fixed_a.data(), fixed_a.size()},
                           FrameSink{Capture, &limited_frames});
  const std::size_t buffered_before_reject = limited_workspace.workspace->BufferedBytes();
  result = PushStreamChunk(*plan, *limited_workspace.workspace, 0U,
                           ByteView{fixed_b.data(), fixed_b.size()},
                           FrameSink{Capture, &limited_frames});
  ok &= Expect(result.api_status == SubmitApiStatus::LIMIT_EXCEEDED &&
                   result.bytes_consumed == 0U && result.frames_delivered == 0U &&
                   limited_workspace.workspace->BufferedBytes() == buffered_before_reject,
               "submit limit rejects before changing a half-frame");
  ok &= Expect(PushStreamChunk(*plan, *limited_workspace.workspace, 1U, ByteView{},
                               FrameSink{Capture, &limited_frames})
                       .api_status == SubmitApiStatus::WORKSPACE_PLAN_MISMATCH,
               "workspace cannot be reused for another pipeline");
  ok &= Expect(
      ResetStreamFramingWorkspace(*plan, *limited_workspace.workspace, 0U) == SubmitApiStatus::OK &&
          limited_workspace.workspace->BufferedBytes() == 0U,
      "explicit reset discards a fixed half-frame without fabricating recovery");

  FramingLimitOverrides too_small_session;
  too_small_session.max_session_memory_bytes = 1U;
  const auto rejected_workspace = CreateStreamFramingWorkspace(*plan, 0U, too_small_session);
  ok &= Expect(rejected_workspace.api_status == SubmitApiStatus::LIMIT_EXCEEDED &&
                   !rejected_workspace.workspace,
               "workspace memory admission fails atomically at limit minus one");

  FramingLimitOverrides small_budget;
  small_budget.max_work_units = 5U;
  auto budget_workspace = CreateStreamFramingWorkspace(*plan, 1U, small_budget);
  CapturedFrames budget_frames;
  const std::array<std::uint8_t, 2U> sync_only{{0xA5U, 0x5AU}};
  result = PushStreamChunk(*plan, *budget_workspace.workspace, 1U,
                           ByteView{sync_only.data(), sync_only.size()},
                           FrameSink{Capture, &budget_frames});
  ok &= Expect(
      result.bytes_consumed == 2U && result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED,
      "work budget leaves only internal sync-copy residue");
  result = PushStreamChunk(*plan, *budget_workspace.workspace, 1U, ByteView{},
                           FrameSink{Capture, &budget_frames});
  ok &= Expect(result.bytes_consumed == 0U && result.frames_delivered == 0U &&
                   result.stop_reason == SubmitStopReason::NEED_MORE,
               "empty input advances internal residue without polling for data");

  auto reentrant_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  ReentrantContext reentrant{plan.get(), reentrant_workspace.workspace.get()};
  const std::array<std::uint8_t, 3U> one_fixed{{0xAAU, 0x20U, 0x21U}};
  result =
      PushStreamChunk(*plan, *reentrant_workspace.workspace, 0U,
                      ByteView{one_fixed.data(), one_fixed.size()}, FrameSink{Reenter, &reentrant});
  ok &= Expect(
      result.frames_delivered == 1U && reentrant.nested_status == SubmitApiStatus::REENTRANT_CALL,
      "same-workspace callback reentry fails closed");

  auto integration_workspace = CreateStreamFramingWorkspace(*plan, 1U);
  pae::protocol_core::ExecutionWorkspace codec_workspace{*plan};
  DecodeContext decode_context{plan.get(), &codec_workspace, 1U};
  const std::array<std::uint8_t, 4U> valid_core_frame{{0xA5U, 0x5AU, 0x01U, 0x02U}};
  result = PushStreamChunk(*plan, *integration_workspace.workspace, 1U,
                           ByteView{valid_core_frame.data(), valid_core_frame.size()},
                           FrameSink{DecodeFrame, &decode_context});
  ok &= Expect(result.frames_delivered == 1U &&
                   decode_context.status == pae::protocol_core::CodecStatus::OK &&
                   decode_context.field_count == 1U,
               "host callback synchronously hands the framed record to existing Core");

  auto codec_failure_workspace = CreateStreamFramingWorkspace(*plan, 2U);
  pae::protocol_core::ExecutionWorkspace failure_codec_workspace{*plan};
  DecodeContext failure_decode_context{plan.get(), &failure_codec_workspace, 2U};
  const std::array<std::uint8_t, 12U> failed_then_valid_core_frames{{
      0xC3U,
      0x3CU,
      0x06U,
      0x11U,
      0x22U,
      0x00U,
      0xC3U,
      0x3CU,
      0x06U,
      0x11U,
      0x22U,
      0x55U,
  }};
  result = PushStreamChunk(
      *plan, *codec_failure_workspace.workspace, 2U,
      ByteView{failed_then_valid_core_frames.data(), failed_then_valid_core_frames.size()},
      FrameSink{DecodeFrame, &failure_decode_context});
  ok &= Expect(
      result.bytes_consumed == failed_then_valid_core_frames.size() &&
          result.frames_delivered == 2U && failure_decode_context.call_count == 2U &&
          failure_decode_context.statuses[0] == pae::protocol_core::CodecStatus::UNKNOWN_MESSAGE &&
          failure_decode_context.field_counts[0] == 0U &&
          failure_decode_context.statuses[1] == pae::protocol_core::CodecStatus::OK &&
          failure_decode_context.field_counts[1] == 2U,
      "Codec failure does not rescan its delivered frame or block the next frame");

  auto busy_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  auto independent_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  BlockingSinkContext blocking;
  std::thread active([&] {
    static_cast<void>(PushStreamChunk(*plan, *busy_workspace.workspace, 0U,
                                      ByteView{one_fixed.data(), one_fixed.size()},
                                      FrameSink{BlockSink, &blocking}));
  });
  while (!blocking.entered.load(std::memory_order_acquire)) std::this_thread::yield();
  const auto busy_result = PushStreamChunk(*plan, *busy_workspace.workspace, 0U, ByteView{},
                                           FrameSink{Capture, &fixed_frames});
  CapturedFrames independent_frames;
  const auto independent_result = PushStreamChunk(*plan, *independent_workspace.workspace, 0U,
                                                  ByteView{one_fixed.data(), one_fixed.size()},
                                                  FrameSink{Capture, &independent_frames});
  blocking.release.store(true, std::memory_order_release);
  active.join();
  ok &= Expect(busy_result.api_status == SubmitApiStatus::WORKSPACE_BUSY &&
                   independent_result.frames_delivered == 1U,
               "same workspace rejects concurrency while another workspace on the Plan proceeds");

  return ok ? 0 : 1;
}
