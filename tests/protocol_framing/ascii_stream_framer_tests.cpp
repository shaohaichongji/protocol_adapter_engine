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
#include <vector>

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

using namespace pae::protocol_framing;

struct CaptureState {
  std::array<std::array<std::uint8_t, 32U>, 16U> frames{};
  std::array<std::size_t, 16U> sizes{};
  std::size_t count = 0U;
  bool stop = false;
};

FrameSinkAction Capture(ByteView frame, void* opaque) noexcept {
  auto& state = *static_cast<CaptureState*>(opaque);
  if (state.count >= state.frames.size() || frame.size > state.frames[0].size()) {
    return FrameSinkAction::STOP;
  }
  for (std::size_t index = 0U; index < frame.size; ++index) {
    state.frames[state.count][index] = frame.data[index];
  }
  state.sizes[state.count++] = frame.size;
  if (state.stop) {
    state.stop = false;
    return FrameSinkAction::STOP;
  }
  return FrameSinkAction::CONTINUE;
}

bool Equals(const CaptureState& state, std::size_t index, std::string_view expected) {
  if (index >= state.count || state.sizes[index] != expected.size()) return false;
  for (std::size_t offset = 0U; offset < expected.size(); ++offset) {
    if (state.frames[index][offset] != static_cast<std::uint8_t>(expected[offset])) return false;
  }
  return true;
}

bool Expect(bool condition, std::string_view detail) {
  if (!condition) std::cerr << "FAILED: " << detail << '\n';
  return condition;
}

std::string ReplaceOnce(std::string input, std::string_view from, std::string_view to) {
  const std::size_t position = input.find(from);
  if (position != std::string::npos) input.replace(position, from.size(), to);
  return input;
}

struct DecodeState {
  const pae::protocol_plan::PlanBundle* plan = nullptr;
  pae::protocol_core::ExecutionWorkspace* workspace = nullptr;
  std::array<pae::protocol_core::CodecStatus, 8U> statuses{};
  std::size_t count = 0U;
};

FrameSinkAction Decode(ByteView frame, void* opaque) noexcept {
  auto& state = *static_cast<DecodeState*>(opaque);
  std::array<pae::protocol_core::DecodedFieldSlot, 4U> fields{};
  const auto result = pae::protocol_core::DecodeCompleteRecord(
      *state.plan, *state.workspace, 0U, pae::protocol_core::ByteView{frame.data, frame.size},
      fields.data(), fields.size());
  if (state.count < state.statuses.size()) state.statuses[state.count] = result.status;
  ++state.count;
  return FrameSinkAction::CONTINUE;
}

bool ExpectCompileFailure(const std::string& json, pae::config_compiler::CompileStage stage,
                          pae::config_compiler::CompileError code, std::string_view pointer,
                          std::string_view detail) {
  const auto result = pae::config_compiler::CompileJsonToPlan(json);
  const bool matches = !result.Succeeded() && result.Diagnostic() != nullptr &&
                       result.Diagnostic()->stage == stage && result.Diagnostic()->code == code &&
                       result.Diagnostic()->json_pointer == pointer;
  if (!matches && result.Diagnostic() != nullptr) {
    std::cerr << "ACTUAL stage=" << static_cast<int>(result.Diagnostic()->stage)
              << " code=" << static_cast<int>(result.Diagnostic()->code)
              << " pointer=" << result.Diagnostic()->json_pointer
              << " detail=" << result.Diagnostic()->detail << '\n';
  }
  return Expect(matches, detail);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!Expect(compiled.Succeeded(), "public Schema 0.11 stream sample compiles")) {
    if (compiled.Diagnostic() != nullptr) {
      std::cerr << compiled.Diagnostic()->json_pointer << ": " << compiled.Diagnostic()->detail
                << '\n';
    }
    return 1;
  }
  auto plan = std::move(compiled).TakePlan();
  bool ok = true;
  const std::string snapshot = pae::config_compiler::MakeDeterministicPlanSnapshot(*plan);
  ok &= Expect(plan->SchemaVersion() == "0.11" && plan->FramingProfiles().size() == 2U &&
                   plan->Pipelines().size() == 3U,
               "Schema 0.11 profiles and action-gating pipelines freeze");
  ok &= Expect(snapshot.find("\"snapshot_format\":\"pae_plan_bundle_v0.11_ascii_stream_slice\"") !=
                       std::string::npos &&
                   snapshot.find("\"strategy\":\"ascii_crlf\"") != std::string::npos &&
                   snapshot.find("\"terminator_text\":\"\\r\\n\"") != std::string::npos &&
                   snapshot.find("\"maximum_frame_length\":12") != std::string::npos,
               "Schema 0.11 snapshot owns its framing domain and exact CRLF facts");
  ok &= Expect(
      snapshot.find(
          "\"text_decode\":{\"min_record_length\":9,\"max_record_length\":12,\"segments\":["
          "{\"kind\":\"literal\",\"bytes\":\"52 58 20\",\"prefix_table\":[0,0,0]},"
          "{\"kind\":\"field\",\"field_index\":0}") != std::string::npos &&
          snapshot.find(
              "\"text_encode\":{\"min_record_length\":8,\"max_record_length\":11,\"segments\":["
              "{\"kind\":\"literal\",\"bytes\":\"54 58 20\",\"prefix_table\":[0,0,0]},"
              "{\"kind\":\"field\",\"field_index\":0}") != std::string::npos &&
          snapshot.find("\"id\":\"decode_only\",\"direction_id\":\"synthetic\","
                        "\"frame_length_bytes\":6,\"text_decode\":{\"min_record_length\":6,"
                        "\"max_record_length\":6,\"segments\":[{\"kind\":\"literal\","
                        "\"bytes\":\"4F 4E 4C 59 0D 0A\",\"prefix_table\":[0,0,0,0,0,0]}]},"
                        "\"text_encode\":null") != std::string::npos &&
          snapshot.find("\"id\":\"encode_only\",\"direction_id\":\"synthetic\","
                        "\"frame_length_bytes\":6,\"text_decode\":null,\"text_encode\":{"
                        "\"min_record_length\":6,\"max_record_length\":6,\"segments\":[{"
                        "\"kind\":\"literal\",\"bytes\":\"53 45 4E 44 0D 0A\","
                        "\"prefix_table\":[0,0,0,0,0,0]}]}") != std::string::npos,
      "Schema 0.11 snapshot retains Decode/Encode literals, fields, lengths, prefixes, and null "
      "actions");

  const auto expect_template_snapshot_difference =
      [&json, &snapshot, &ok](std::string_view from, std::string_view to, std::string_view detail) {
        auto changed = pae::config_compiler::CompileJsonToPlan(ReplaceOnce(json, from, to));
        const bool differs =
            changed.Succeeded() &&
            pae::config_compiler::MakeDeterministicPlanSnapshot(*changed.Plan()) != snapshot;
        ok &= Expect(differs, detail);
      };
  expect_template_snapshot_difference("\"text\": \"RX \"", "\"text\": \"RY \"",
                                      "same-length valid RX literal changes the Snapshot");
  expect_template_snapshot_difference("\"text\": \"TX \"", "\"text\": \"TY \"",
                                      "same-length valid TX literal changes the Snapshot");
  ok &= Expect(plan->GetResourceRequirements().max_stream_frame_bytes == 12U &&
                   plan->GetResourceRequirements().max_sync_bytes == 2U &&
                   plan->GetResourceRequirements().max_framing_buffer_bytes == 12U &&
                   plan->GetPlanMemoryReport().accounted_total_bytes != 0U,
               "terminator, stream buffer, descriptors, and text segments are accounted");

  ok &= ExpectCompileFailure(
      ReplaceOnce(json, "\"terminator_text\": \"\\r\\n\"", "\"terminator_text\": \"\\n\""),
      pae::config_compiler::CompileStage::STRUCTURAL,
      pae::config_compiler::CompileError::INVALID_STRING_LENGTH,
      "/framing_profiles/0/terminator_text", "non-CRLF terminator fails at its exact property");
  ok &= ExpectCompileFailure(
      ReplaceOnce(json, "\"maximum_frame_length\": 12", "\"maximum_frame_length\": null"),
      pae::config_compiler::CompileStage::STRUCTURAL,
      pae::config_compiler::CompileError::TYPE_MISMATCH, "/framing_profiles/0/maximum_frame_length",
      "wrong maximum length type fails structurally");
  ok &= ExpectCompileFailure(ReplaceOnce(json, "\"maximum_frame_length\": 12",
                                         "\"maximum_frame_length\": 12, \"sync_bytes\": \"0D 0A\""),
                             pae::config_compiler::CompileStage::STRUCTURAL,
                             pae::config_compiler::CompileError::UNKNOWN_PROPERTY,
                             "/framing_profiles/0/sync_bytes",
                             "foreign strategy member fails before domain validation");
  ok &=
      ExpectCompileFailure(ReplaceOnce(json, "\"max_byte_length\": 4", "\"max_byte_length\": 20"),
                           pae::config_compiler::CompileStage::DOMAIN_VALIDATION,
                           pae::config_compiler::CompileError::ASCII_STREAM_PROFILE_MISMATCH,
                           "/pipelines/0/message_ids", "Decode maximum larger than M is rejected");
  const std::string internal_crlf =
      ReplaceOnce(ReplaceOnce(json, "\"maximum_frame_length\": 12", "\"maximum_frame_length\": 20"),
                  "{\"kind\": \"literal\", \"text\": \"RX \"}",
                  "{\"kind\": \"literal\", \"text\": \"RX\\r\\n\"}");
  ok &= ExpectCompileFailure(internal_crlf, pae::config_compiler::CompileStage::DOMAIN_VALIDATION,
                             pae::config_compiler::CompileError::ASCII_STREAM_BOUNDARY_UNPROVEN,
                             "/messages/0/layout", "internal literal CRLF is rejected");
  std::string field_crlf =
      ReplaceOnce(json, "\"max_byte_length\": 4}",
                  "\"max_byte_length\": 4, \"allowed_control_bytes\": \"0A 0D\"}");
  ok &=
      ExpectCompileFailure(field_crlf, pae::config_compiler::CompileStage::DOMAIN_VALIDATION,
                           pae::config_compiler::CompileError::ASCII_STREAM_BOUNDARY_UNPROVEN,
                           "/messages/0/layout", "field-produced CRLF is conservatively rejected");
  std::string empty_bridge = ReplaceOnce(json, "{\"kind\": \"literal\", \"text\": \"RX \"}",
                                         "{\"kind\": \"literal\", \"text\": \"RX \\r\"}");
  empty_bridge = ReplaceOnce(empty_bridge, "\"min_byte_length\": 1, \"max_byte_length\": 4",
                             "\"min_byte_length\": 0, \"max_byte_length\": 1");
  empty_bridge = ReplaceOnce(empty_bridge, "{\"kind\": \"literal\", \"text\": \"!\"}",
                             "{\"kind\": \"literal\", \"text\": \"\\n!\"}");
  ok &= ExpectCompileFailure(empty_bridge, pae::config_compiler::CompileStage::DOMAIN_VALIDATION,
                             pae::config_compiler::CompileError::ASCII_STREAM_BOUNDARY_UNPROVEN,
                             "/messages/0/layout",
                             "zero-length field cannot hide CRLF formed across adjacent segments");

  auto created = CreateStreamFramingWorkspace(*plan, 0U);
  ok &=
      Expect(created.api_status == SubmitApiStatus::OK && created.workspace &&
                 created.accounted_workspace_bytes == sizeof(StreamFramingWorkspace) + 12U &&
                 created.accounted_workspace_bytes == created.workspace->AccountedWorkspaceBytes(),
             "workspace allocates exactly M bytes and accounts its state object");
  FramingLimitOverrides exact_memory;
  exact_memory.max_session_memory_bytes = created.accounted_workspace_bytes;
  ok &= Expect(
      CreateStreamFramingWorkspace(*plan, 0U, exact_memory).api_status == SubmitApiStatus::OK,
      "exact session memory admission succeeds");
  --exact_memory.max_session_memory_bytes;
  ok &= Expect(CreateStreamFramingWorkspace(*plan, 0U, exact_memory).api_status ==
                   SubmitApiStatus::LIMIT_EXCEEDED,
               "exact session memory minus one fails");
  FramingLimitOverrides submit_limit;
  submit_limit.max_submit_bytes = 4U;
  auto submit_limited = CreateStreamFramingWorkspace(*plan, 0U, submit_limit);
  CaptureState submit_frames;
  const std::string oversized_submit = "ONLY\r\n";
  const auto submit_reject =
      PushStreamChunk(*plan, *submit_limited.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(oversized_submit.data()),
                               oversized_submit.size()},
                      FrameSink{Capture, &submit_frames});
  ok &= Expect(submit_reject.api_status == SubmitApiStatus::LIMIT_EXCEEDED &&
                   submit_reject.bytes_consumed == 0U && submit_reject.frames_delivered == 0U &&
                   submit_limited.workspace->BufferedBytes() == 0U,
               "oversized submit is rejected before changing stream state");

  CaptureState captured;
  const std::string first = "RX A!OK\r\n";
  for (const char byte : first) {
    const std::uint8_t value = static_cast<std::uint8_t>(byte);
    g_tracked_allocations.store(0U, std::memory_order_relaxed);
    g_track_allocations.store(true, std::memory_order_relaxed);
    const auto result = PushStreamChunk(*plan, *created.workspace, 0U, ByteView{&value, 1U},
                                        FrameSink{Capture, &captured});
    g_track_allocations.store(false, std::memory_order_relaxed);
    ok &= Expect(result.api_status == SubmitApiStatus::OK &&
                     g_tracked_allocations.load(std::memory_order_relaxed) == 0U,
                 "first and repeated byte pushes do not allocate");
  }
  ok &= Expect(captured.count == 1U && Equals(captured, 0U, first),
               "per-byte chunks including split CRLF deliver once");

  FramingLimitOverrides small_work;
  small_work.max_work_units = 5U;
  auto budget_workspace = CreateStreamFramingWorkspace(*plan, 0U, small_work);
  CaptureState budget_frames;
  const std::string budget_input = "ONLY\r\n";
  std::size_t budget_offset = 0U;
  std::size_t budget_calls = 0U;
  bool saw_budget_stop = false;
  while (budget_offset < budget_input.size() && budget_calls < 8U) {
    const auto result = PushStreamChunk(
        *plan, *budget_workspace.workspace, 0U,
        ByteView{reinterpret_cast<const std::uint8_t*>(budget_input.data()) + budget_offset,
                 budget_input.size() - budget_offset},
        FrameSink{Capture, &budget_frames});
    ok &= Expect(result.work_units_used <= small_work.max_work_units,
                 "each continuation remains inside the work budget");
    saw_budget_stop |= result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED;
    budget_offset += result.bytes_consumed;
    ++budget_calls;
  }
  ok &= Expect(saw_budget_stop && budget_offset == budget_input.size() &&
                   budget_frames.count == 1U && Equals(budget_frames, 0U, budget_input),
               "budget exhaustion resumes from the exact suffix without duplicate delivery");

  auto empty_candidate_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState empty_candidate;
  const std::string crlf = "\r\n";
  const auto crlf_result =
      PushStreamChunk(*plan, *empty_candidate_workspace.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(crlf.data()), crlf.size()},
                      FrameSink{Capture, &empty_candidate});
  ok &= Expect(crlf_result.frames_delivered == 1U && Equals(empty_candidate, 0U, crlf),
               "bare CRLF remains a candidate for Core rather than being silently skipped");

  const std::string glued = "ONLY\r\nRX BC!OK\r\nRX C";
  const auto glued_result =
      PushStreamChunk(*plan, *created.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(glued.data()), glued.size()},
                      FrameSink{Capture, &captured});
  ok &= Expect(glued_result.bytes_consumed == glued.size() && glued_result.frames_delivered == 2U &&
                   glued_result.stop_reason == SubmitStopReason::NEED_MORE &&
                   Equals(captured, 1U, "ONLY\r\n") && Equals(captured, 2U, "RX BC!OK\r\n"),
               "glued frames deliver and trailing half-record is retained");
  ok &= Expect(ResetStreamFramingWorkspace(*plan, *created.workspace, 0U) == SubmitApiStatus::OK &&
                   created.workspace->BufferedBytes() == 0U,
               "Reset discards a half-record without delivery");

  const std::string exact = "ABCDEFGHIJ\r\n";
  auto exact_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState exact_frames;
  const auto exact_result =
      PushStreamChunk(*plan, *exact_workspace.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(exact.data()), exact.size()},
                      FrameSink{Capture, &exact_frames});
  ok &= Expect(exact_result.frames_delivered == 1U && exact_result.malformed_candidates == 0U &&
                   Equals(exact_frames, 0U, exact),
               "exact M-byte CRLF record is delivered");

  auto long_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState recovered;
  const std::string long_prefix(12U, 'A');
  auto long_result = PushStreamChunk(
      *plan, *long_workspace.workspace, 0U,
      ByteView{reinterpret_cast<const std::uint8_t*>(long_prefix.data()), long_prefix.size()},
      FrameSink{Capture, &recovered});
  ok &= Expect(long_result.bytes_consumed == 12U && long_result.frames_delivered == 0U &&
                   long_result.malformed_candidates == 1U && long_result.bytes_discarded == 12U &&
                   long_result.last_framing_issue == FramingIssue::RECORD_TOO_LONG &&
                   long_result.stop_reason == SubmitStopReason::NEED_MORE,
               "M bytes without terminator becomes one recoverable overlong candidate");
  const std::string recover = "\r\nONLY\r\n";
  long_result = PushStreamChunk(
      *plan, *long_workspace.workspace, 0U,
      ByteView{reinterpret_cast<const std::uint8_t*>(recover.data()), recover.size()},
      FrameSink{Capture, &recovered});
  ok &= Expect(long_result.bytes_discarded == 2U && long_result.frames_delivered == 1U &&
                   Equals(recovered, 0U, "ONLY\r\n") &&
                   long_workspace.workspace->TotalMalformedCandidates() == 1U &&
                   long_workspace.workspace->TotalDiscardedBytes() == 14U,
               "overlong discard consumes its CRLF then recovers the following frame");

  auto reset_discard_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState reset_discard_frames;
  static_cast<void>(PushStreamChunk(
      *plan, *reset_discard_workspace.workspace, 0U,
      ByteView{reinterpret_cast<const std::uint8_t*>(long_prefix.data()), long_prefix.size()},
      FrameSink{Capture, &reset_discard_frames}));
  ok &= Expect(ResetStreamFramingWorkspace(*plan, *reset_discard_workspace.workspace, 0U) ==
                   SubmitApiStatus::OK,
               "Reset leaves discard mode without inventing a boundary");
  const std::string after_reset_record = "ONLY\r\n";
  const auto after_discard_reset =
      PushStreamChunk(*plan, *reset_discard_workspace.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(after_reset_record.data()),
                               after_reset_record.size()},
                      FrameSink{Capture, &reset_discard_frames});
  ok &= Expect(after_discard_reset.frames_delivered == 1U &&
                   Equals(reset_discard_frames, 0U, after_reset_record),
               "a valid record is accepted immediately after resetting discard state");

  auto tail_cr_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState tail_cr_frames;
  const std::string tail_cr = "ABCDEFGHIJK\r";
  long_result = PushStreamChunk(
      *plan, *tail_cr_workspace.workspace, 0U,
      ByteView{reinterpret_cast<const std::uint8_t*>(tail_cr.data()), tail_cr.size()},
      FrameSink{Capture, &tail_cr_frames});
  const std::string tail_recover = "\nONLY\r\n";
  long_result = PushStreamChunk(
      *plan, *tail_cr_workspace.workspace, 0U,
      ByteView{reinterpret_cast<const std::uint8_t*>(tail_recover.data()), tail_recover.size()},
      FrameSink{Capture, &tail_cr_frames});
  ok &= Expect(long_result.bytes_discarded == 1U && long_result.frames_delivered == 1U &&
                   Equals(tail_cr_frames, 0U, "ONLY\r\n"),
               "Mth-byte CR joins the next LF only as the discarded record boundary");

  auto stop_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState stopped;
  stopped.stop = true;
  const std::string two = "ONLY\r\nRX D!OK\r\n";
  auto stop_result =
      PushStreamChunk(*plan, *stop_workspace.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(two.data()), two.size()},
                      FrameSink{Capture, &stopped});
  ok &= Expect(stop_result.stop_reason == SubmitStopReason::SINK_STOP &&
                   stop_result.bytes_consumed == 6U && stop_result.frames_delivered == 1U,
               "STOP commits the current frame without consuming the suffix");
  stop_result = PushStreamChunk(
      *plan, *stop_workspace.workspace, 0U,
      ByteView{reinterpret_cast<const std::uint8_t*>(two.data() + 6U), two.size() - 6U},
      FrameSink{Capture, &stopped});
  ok &= Expect(stop_result.frames_delivered == 1U && stopped.count == 2U &&
                   Equals(stopped, 1U, "RX D!OK\r\n"),
               "host resubmits only the unconsumed suffix with no duplicate delivery");

  FramingLimitOverrides one_frame;
  one_frame.max_frames_per_submit = 1U;
  auto pending_workspace = CreateStreamFramingWorkspace(*plan, 0U, one_frame);
  CaptureState pending;
  auto pending_result =
      PushStreamChunk(*plan, *pending_workspace.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(two.data()), two.size()},
                      FrameSink{Capture, &pending});
  ok &=
      Expect(pending_result.bytes_consumed == two.size() && pending_result.frames_delivered == 1U &&
                 pending_result.stop_reason == SubmitStopReason::WORK_BUDGET_REACHED,
             "callback budget retains one completed pending frame");
  pending_result = PushStreamChunk(*plan, *pending_workspace.workspace, 0U, ByteView{},
                                   FrameSink{Capture, &pending});
  ok &= Expect(pending_result.frames_delivered == 1U && pending.count == 2U,
               "empty push delivers pending frame exactly once");

  auto first_flow = CreateStreamFramingWorkspace(*plan, 0U);
  auto second_flow = CreateStreamFramingWorkspace(*plan, 0U);
  CaptureState flows;
  const std::string half = "ON";
  static_cast<void>(
      PushStreamChunk(*plan, *first_flow.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(half.data()), half.size()},
                      FrameSink{Capture, &flows}));
  const std::string whole = "ONLY\r\n";
  const auto flow_result =
      PushStreamChunk(*plan, *second_flow.workspace, 0U,
                      ByteView{reinterpret_cast<const std::uint8_t*>(whole.data()), whole.size()},
                      FrameSink{Capture, &flows});
  ok &= Expect(flow_result.frames_delivered == 1U && first_flow.workspace->BufferedBytes() == 2U &&
                   second_flow.workspace->BufferedBytes() == 0U,
               "two streams share the Plan while preserving independent state");

  pae::protocol_core::ExecutionWorkspace codec_workspace{*plan};
  DecodeState decoded{plan.get(), &codec_workspace};
  auto decode_workspace = CreateStreamFramingWorkspace(*plan, 0U);
  const std::array<std::uint8_t, 20U> decode_bytes{{'R',  'X',  ' ', 0x80U, '!', 'O', 'K',
                                                    '\r', '\n', 'N', 'O',   'P', 'E', '\r',
                                                    '\n', 'O',  'N', 'L',   'Y', '\r'}};
  static_cast<void>(PushStreamChunk(*plan, *decode_workspace.workspace, 0U,
                                    ByteView{decode_bytes.data(), 15U},
                                    FrameSink{Decode, &decoded}));
  const std::uint8_t final_lf = '\n';
  static_cast<void>(PushStreamChunk(*plan, *decode_workspace.workspace, 0U,
                                    ByteView{decode_bytes.data() + 15U, 5U},
                                    FrameSink{Decode, &decoded}));
  static_cast<void>(PushStreamChunk(*plan, *decode_workspace.workspace, 0U, ByteView{&final_lf, 1U},
                                    FrameSink{Decode, &decoded}));
  ok &= Expect(
      decoded.count == 3U &&
          decoded.statuses[0] == pae::protocol_core::CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
          decoded.statuses[1] == pae::protocol_core::CodecStatus::UNKNOWN_MESSAGE &&
          decoded.statuses[2] == pae::protocol_core::CodecStatus::OK,
      "framer counts candidates independently and Core failures do not block recovery");

  pae::protocol_core::ExecutionWorkspace action_workspace{*plan};
  std::array<pae::protocol_core::DecodedFieldSlot, 1U> fields{};
  const std::string send = "SEND\r\n";
  const auto missing_decode = pae::protocol_core::DecodeCompleteRecord(
      *plan, action_workspace, 1U,
      pae::protocol_core::ByteView{reinterpret_cast<const std::uint8_t*>(send.data()), send.size()},
      fields.data(), fields.size());
  std::array<std::uint8_t, 16U> output{};
  const auto missing_encode = pae::protocol_core::EncodeCompleteRecord(
      *plan, action_workspace, 2U, 1U, nullptr, 0U,
      pae::protocol_core::MutableByteBuffer{output.data(), output.size()});
  ok &=
      Expect(missing_decode.status == pae::protocol_core::CodecStatus::OPERATION_NOT_SUPPORTED &&
                 missing_encode.status == pae::protocol_core::CodecStatus::OPERATION_NOT_SUPPORTED,
             "Schema 0.11 preserves missing Decode and missing Encode action gates");

  const std::uint8_t name_value = 'A';
  const std::uint8_t tag_value = 'Z';
  std::array<pae::protocol_core::EncodeFieldValue, 2U> values{};
  values[0].field = {plan.get(), 0U, 0U};
  values[0].value_kind = pae::protocol_core::LogicalValueKind::BYTES;
  values[0].bytes_value = {&name_value, 1U};
  values[1].field = {plan.get(), 0U, 2U};
  values[1].value_kind = pae::protocol_core::LogicalValueKind::BYTES;
  values[1].bytes_value = {&tag_value, 1U};
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, action_workspace, 0U, 0U, values.data(), values.size(),
      pae::protocol_core::MutableByteBuffer{output.data(), output.size()});
  ok &=
      Expect(encoded.status == pae::protocol_core::CodecStatus::OK && encoded.bytes_written == 8U &&
                 std::string_view{reinterpret_cast<const char*>(output.data()),
                                  encoded.bytes_written} == "TX A!Z\r\n",
             "Schema 0.11 Encode consumes the TX-only field and does not require RX-only input");
  values[1].field.field_index = 1U;
  const auto rx_override = pae::protocol_core::EncodeCompleteRecord(
      *plan, action_workspace, 0U, 0U, values.data(), values.size(),
      pae::protocol_core::MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(rx_override.status == pae::protocol_core::CodecStatus::FIELD_REFERENCE_MISMATCH,
               "Schema 0.11 rejects a Decode-only field supplied to Encode");

  return ok ? 0 : 1;
}
