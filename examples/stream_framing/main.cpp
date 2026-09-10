#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "../../src/config_compiler/config_compiler.h"
#include "../../src/protocol_core/complete_record_codec.h"
#include "../../src/protocol_framing/stream_framer.h"

namespace {

using pae::protocol_framing::ByteView;
using pae::protocol_framing::FrameSink;
using pae::protocol_framing::FrameSinkAction;
using pae::protocol_framing::FramingLimitOverrides;
using pae::protocol_framing::PushStreamChunk;
using pae::protocol_framing::StreamFramingWorkspace;
using pae::protocol_framing::SubmitApiStatus;
using pae::protocol_framing::SubmitResult;
using pae::protocol_framing::SubmitStopReason;

constexpr std::size_t kMaximumObservedFrames = 8U;
constexpr std::size_t kMaximumRetainedFrameBytes = 8U;
constexpr std::size_t kMaximumBusinessFields = 2U;

struct BusinessFieldSnapshot {
  std::size_t message_index = pae::protocol_core::kInvalidIndex;
  std::size_t field_index = pae::protocol_core::kInvalidIndex;
  pae::protocol_core::LogicalValueKind value_kind = pae::protocol_core::LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  bool plan_scope_matches = false;
};

struct LogicalStreamHost {
  LogicalStreamHost(const pae::protocol_plan::PlanBundle& source_plan,
                    std::size_t source_pipeline_index,
                    std::unique_ptr<StreamFramingWorkspace> source_framing_workspace)
      : plan(&source_plan),
        pipeline_index(source_pipeline_index),
        framing_workspace(std::move(source_framing_workspace)),
        codec_workspace(source_plan) {}

  const pae::protocol_plan::PlanBundle* plan = nullptr;
  std::size_t pipeline_index = 0U;
  std::unique_ptr<StreamFramingWorkspace> framing_workspace;
  pae::protocol_core::ExecutionWorkspace codec_workspace;
  std::array<pae::protocol_core::CodecStatus, kMaximumObservedFrames> decode_statuses{};
  std::array<std::array<std::uint8_t, kMaximumRetainedFrameBytes>, kMaximumObservedFrames>
      retained_frames{};
  std::array<std::size_t, kMaximumObservedFrames> retained_sizes{};
  std::array<std::array<BusinessFieldSnapshot, kMaximumBusinessFields>, kMaximumObservedFrames>
      business_fields{};
  std::array<std::size_t, kMaximumObservedFrames> business_field_counts{};
  std::size_t callback_count = 0U;
  std::size_t decoded_frames = 0U;
  bool stop_after_next = false;
  bool observation_overflow = false;
};

FrameSinkAction DecodeAndRetainFrame(ByteView frame, void* user_data) noexcept {
  auto& host = *static_cast<LogicalStreamHost*>(user_data);
  if (host.callback_count >= host.decode_statuses.size() ||
      frame.size > host.retained_frames[0].size()) {
    host.observation_overflow = true;
    return FrameSinkAction::STOP;
  }

  std::array<pae::protocol_core::DecodedFieldSlot, 8U> fields{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *host.plan, host.codec_workspace, host.pipeline_index,
      pae::protocol_core::ByteView{frame.data, frame.size}, fields.data(), fields.size());
  const std::size_t observation_index = host.callback_count++;
  host.decode_statuses[observation_index] = decoded.status;
  host.business_field_counts[observation_index] = decoded.field_count;
  if (decoded.status == pae::protocol_core::CodecStatus::OK) {
    ++host.decoded_frames;
    if (decoded.field_count > host.business_fields[observation_index].size()) {
      host.observation_overflow = true;
      return FrameSinkAction::STOP;
    }
    for (std::size_t index = 0U; index < decoded.field_count; ++index) {
      host.business_fields[observation_index][index] = BusinessFieldSnapshot{
          fields[index].field.message_index,
          fields[index].field.field_index,
          fields[index].value_kind,
          fields[index].uint64_value,
          fields[index].field.plan_scope == host.plan,
      };
    }
  }

  // The Framer's view is borrowed only for this callback. A host that needs the frame later must
  // copy it into storage whose lifetime it owns, as this synthetic observation buffer does.
  for (std::size_t index = 0U; index < frame.size; ++index) {
    host.retained_frames[observation_index][index] = frame.data[index];
  }
  host.retained_sizes[observation_index] = frame.size;

  if (host.stop_after_next) {
    host.stop_after_next = false;
    return FrameSinkAction::STOP;
  }
  return FrameSinkAction::CONTINUE;
}

bool Expect(bool condition, const char* detail) {
  if (!condition) std::cerr << "FAILED: " << detail << '\n';
  return condition;
}

bool RetainedEquals(const LogicalStreamHost& host, std::size_t frame_index,
                    std::initializer_list<std::uint8_t> expected) {
  if (frame_index >= host.callback_count || host.retained_sizes[frame_index] != expected.size()) {
    return false;
  }
  std::size_t byte_index = 0U;
  for (const std::uint8_t byte : expected) {
    if (host.retained_frames[frame_index][byte_index++] != byte) return false;
  }
  return true;
}

bool BusinessFieldEquals(const LogicalStreamHost& host, std::size_t frame_index,
                         std::size_t field_position, std::size_t expected_message_index,
                         std::size_t expected_field_index, std::uint64_t expected_value) {
  if (frame_index >= host.callback_count ||
      field_position >= host.business_field_counts[frame_index]) {
    return false;
  }
  const auto& field = host.business_fields[frame_index][field_position];
  return field.plan_scope_matches && field.message_index == expected_message_index &&
         field.field_index == expected_field_index &&
         field.value_kind == pae::protocol_core::LogicalValueKind::UINT64 &&
         field.uint64_value == expected_value;
}

SubmitResult SubmitOnce(LogicalStreamHost& host, ByteView input) noexcept {
  return PushStreamChunk(*host.plan, *host.framing_workspace, host.pipeline_index, input,
                         FrameSink{DecodeAndRetainFrame, &host});
}

struct BoundedDriveResult {
  bool completed = false;
  bool made_zero_input_progress = false;
  std::size_t bytes_consumed = 0U;
  std::size_t calls = 0U;
  SubmitResult last_result;
};

BoundedDriveResult DriveThroughBudget(LogicalStreamHost& host, ByteView input,
                                      std::size_t maximum_calls) noexcept {
  BoundedDriveResult driven;
  for (; driven.calls < maximum_calls;) {
    const std::size_t remaining = input.size - driven.bytes_consumed;
    const std::uint8_t* remaining_data = input.data;
    if (remaining_data != nullptr) remaining_data += driven.bytes_consumed;
    driven.last_result = SubmitOnce(host, ByteView{remaining_data, remaining});
    ++driven.calls;
    if (driven.last_result.api_status != SubmitApiStatus::OK ||
        driven.last_result.bytes_consumed > remaining) {
      return driven;
    }
    driven.bytes_consumed += driven.last_result.bytes_consumed;
    if (remaining == 0U &&
        (driven.last_result.frames_delivered != 0U || driven.last_result.work_units_used != 0U)) {
      driven.made_zero_input_progress = true;
    }
    if (driven.last_result.stop_reason != SubmitStopReason::WORK_BUDGET_REACHED) {
      driven.completed = driven.bytes_consumed == input.size;
      return driven;
    }
    if (driven.last_result.bytes_consumed == 0U && driven.last_result.frames_delivered == 0U &&
        driven.last_result.work_units_used == 0U) {
      return driven;
    }
  }
  return driven;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!compiled.Succeeded()) return 3;
  auto plan = std::move(compiled).TakePlan();

  auto framing_a = pae::protocol_framing::CreateStreamFramingWorkspace(*plan, 2U);
  auto framing_b = pae::protocol_framing::CreateStreamFramingWorkspace(*plan, 2U);
  if (framing_a.api_status != SubmitApiStatus::OK || framing_b.api_status != SubmitApiStatus::OK) {
    return 4;
  }
  LogicalStreamHost stream_a{*plan, 2U, std::move(framing_a.workspace)};
  LogicalStreamHost stream_b{*plan, 2U, std::move(framing_b.workspace)};
  bool ok = true;
  ok &= Expect(stream_a.plan == stream_b.plan &&
                   stream_a.framing_workspace.get() != stream_b.framing_workspace.get(),
               "two logical streams share one immutable Plan and own distinct workspaces");

  std::array<std::uint8_t, 2U> stream_a_noise_and_partial_sync{{0x00U, 0xC3U}};
  auto result = SubmitOnce(stream_a, ByteView{stream_a_noise_and_partial_sync.data(),
                                              stream_a_noise_and_partial_sync.size()});
  ok &= Expect(result.api_status == SubmitApiStatus::OK && result.bytes_consumed == 2U &&
                   result.bytes_discarded == 1U && result.frames_delivered == 0U &&
                   result.stop_reason == SubmitStopReason::NEED_MORE,
               "stream A retains a split synchronization prefix after discarding noise");

  std::array<std::uint8_t, 4U> stream_b_half_frame{{0xC3U, 0x3CU, 0x06U, 0x99U}};
  result = SubmitOnce(stream_b, ByteView{stream_b_half_frame.data(), stream_b_half_frame.size()});
  ok &= Expect(result.api_status == SubmitApiStatus::OK && result.bytes_consumed == 4U &&
                   result.frames_delivered == 0U &&
                   result.stop_reason == SubmitStopReason::NEED_MORE &&
                   stream_b.framing_workspace->BufferedBytes() == 4U,
               "stream B can pause with an independent half-frame");
  const std::size_t paused_b_bytes = stream_b.framing_workspace->BufferedBytes();

  // The first candidate has an invalid declared length. The next complete candidate is a valid
  // Framer frame but fails Core matching because its final fixed byte is wrong. STOP commits that
  // delivered candidate and leaves the following six-byte input suffix with the host.
  std::array<std::uint8_t, 15U> stream_a_recovery_and_two_frames{{
      0x3CU,
      0x09U,
      0x11U,
      0xC3U,
      0x3CU,
      0x06U,
      0x12U,
      0x34U,
      0x00U,
      0xC3U,
      0x3CU,
      0x06U,
      0x56U,
      0x78U,
      0x55U,
  }};
  stream_a.stop_after_next = true;
  result = SubmitOnce(stream_a, ByteView{stream_a_recovery_and_two_frames.data(),
                                         stream_a_recovery_and_two_frames.size()});
  const std::size_t retained_suffix_offset = result.bytes_consumed;
  ok &= Expect(
      result.api_status == SubmitApiStatus::OK &&
          result.stop_reason == SubmitStopReason::SINK_STOP && result.frames_delivered == 1U &&
          retained_suffix_offset == 9U && stream_a.callback_count == 1U &&
          stream_a.decode_statuses[0] == pae::protocol_core::CodecStatus::UNKNOWN_MESSAGE &&
          stream_a.business_field_counts[0] == 0U &&
          stream_a.framing_workspace->TotalMalformedCandidates() == 1U &&
          stream_a.framing_workspace->TotalDiscardedBytes() == 5U,
      "invalid length recovers, Decode failure stays delivered, and STOP preserves suffix");
  ok &= Expect(stream_b.framing_workspace->BufferedBytes() == paused_b_bytes &&
                   stream_b.callback_count == 0U,
               "advancing stream A does not mutate paused stream B");

  result = SubmitOnce(stream_a,
                      ByteView{stream_a_recovery_and_two_frames.data() + retained_suffix_offset,
                               stream_a_recovery_and_two_frames.size() - retained_suffix_offset});
  ok &= Expect(result.api_status == SubmitApiStatus::OK && result.bytes_consumed == 6U &&
                   result.frames_delivered == 1U &&
                   result.stop_reason == SubmitStopReason::INPUT_EXHAUSTED &&
                   stream_a.callback_count == 2U && stream_a.decoded_frames == 1U &&
                   stream_a.decode_statuses[1] == pae::protocol_core::CodecStatus::OK &&
                   stream_a.business_field_counts[1] == 2U &&
                   BusinessFieldEquals(stream_a, 1U, 0U, 2U, 0U, 6U) &&
                   BusinessFieldEquals(stream_a, 1U, 1U, 2U, 1U, 0x5678U) &&
                   stream_a.framing_workspace->TotalMalformedCandidates() == 1U &&
                   stream_a.framing_workspace->TotalDiscardedBytes() == 5U,
               "host resubmits only the exact suffix and Decode failure does not trigger rescan");
  stream_a_recovery_and_two_frames.fill(0U);
  ok &= Expect(RetainedEquals(stream_a, 1U, {0xC3U, 0x3CU, 0x06U, 0x56U, 0x78U, 0x55U}),
               "host-owned copy outlives the borrowed callback view and source chunk mutation");

  const std::size_t stream_a_callbacks_before_b_reset = stream_a.callback_count;
  const std::size_t stream_a_discarded_before_b_reset =
      stream_a.framing_workspace->TotalDiscardedBytes();
  ok &= Expect(
      pae::protocol_framing::ResetStreamFramingWorkspace(
          *plan, *stream_b.framing_workspace, stream_b.pipeline_index) == SubmitApiStatus::OK &&
          stream_b.framing_workspace->BufferedBytes() == 0U &&
          stream_a.callback_count == stream_a_callbacks_before_b_reset &&
          stream_a.framing_workspace->TotalDiscardedBytes() == stream_a_discarded_before_b_reset,
      "resetting stream B drops its half-frame without touching stream A");

  std::array<std::uint8_t, 6U> stream_b_after_reset{{
      0xC3U,
      0x3CU,
      0x06U,
      0xABU,
      0xCDU,
      0x55U,
  }};
  result = SubmitOnce(stream_b, ByteView{stream_b_after_reset.data(), stream_b_after_reset.size()});
  ok &= Expect(result.api_status == SubmitApiStatus::OK && result.bytes_consumed == 6U &&
                   result.frames_delivered == 1U && stream_b.callback_count == 1U &&
                   stream_b.decoded_frames == 1U &&
                   stream_b.decode_statuses[0] == pae::protocol_core::CodecStatus::OK &&
                   stream_b.business_field_counts[0] == 2U &&
                   BusinessFieldEquals(stream_b, 0U, 0U, 2U, 0U, 6U) &&
                   BusinessFieldEquals(stream_b, 0U, 1U, 2U, 1U, 0xABCDU),
               "stream B accepts a fresh complete frame after reset");

  FramingLimitOverrides single_frame_budget;
  single_frame_budget.max_frames_per_submit = 1U;
  auto framing_budget =
      pae::protocol_framing::CreateStreamFramingWorkspace(*plan, 0U, single_frame_budget);
  if (framing_budget.api_status != SubmitApiStatus::OK) return 4;
  LogicalStreamHost budget_stream{*plan, 0U, std::move(framing_budget.workspace)};
  const std::array<std::uint8_t, 6U> two_fixed_frames{{
      0xAAU,
      0x01U,
      0x02U,
      0xAAU,
      0x03U,
      0x04U,
  }};
  const BoundedDriveResult driven = DriveThroughBudget(
      budget_stream, ByteView{two_fixed_frames.data(), two_fixed_frames.size()}, 16U);
  const bool budget_drained =
      driven.completed && driven.bytes_consumed == two_fixed_frames.size() && driven.calls <= 16U &&
      driven.made_zero_input_progress &&
      driven.last_result.stop_reason == SubmitStopReason::INPUT_EXHAUSTED &&
      budget_stream.callback_count == 2U && budget_stream.decoded_frames == 2U &&
      budget_stream.business_field_counts[0] == 1U &&
      BusinessFieldEquals(budget_stream, 0U, 0U, 0U, 0U, 0x0102U) &&
      budget_stream.business_field_counts[1] == 1U &&
      BusinessFieldEquals(budget_stream, 1U, 0U, 0U, 0U, 0x0304U);
  if (!budget_drained) {
    std::cerr << "budget detail: completed=" << driven.completed
              << " consumed=" << driven.bytes_consumed << " calls=" << driven.calls
              << " zero_input_progress=" << driven.made_zero_input_progress
              << " stop=" << static_cast<int>(driven.last_result.stop_reason)
              << " api=" << static_cast<int>(driven.last_result.api_status)
              << " callbacks=" << budget_stream.callback_count
              << " decoded=" << budget_stream.decoded_frames << '\n';
  }
  ok &= Expect(budget_drained,
               "bounded host continuation drains budget residue without a zero-progress busy loop");

  ok &= Expect(!stream_a.observation_overflow && !stream_b.observation_overflow &&
                   !budget_stream.observation_overflow,
               "all callback observations stay within the explicit example bounds");
  if (!ok) return 5;

  std::cout << "STREAM_FRAMING_HOST_EXAMPLE primary_streams=2 primary_decoded_frames=2 "
               "decode_failures=1 malformed_lengths=1 retained_suffix_bytes=6 reset_half_frame=1 "
               "budget_probe_decoded_frames=2 budget_calls="
            << driven.calls << " network_calls=0\n";
  return 0;
}
