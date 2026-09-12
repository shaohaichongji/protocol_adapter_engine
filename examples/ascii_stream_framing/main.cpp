#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "../../src/config_compiler/config_compiler.h"
#include "../../src/protocol_core/complete_record_codec.h"
#include "../../src/protocol_framing/stream_framer.h"

namespace {

struct HostState {
  const pae::protocol_plan::PlanBundle* plan = nullptr;
  pae::protocol_core::ExecutionWorkspace* codec_workspace = nullptr;
  std::array<std::uint8_t, 16U> retained_frame{};
  std::array<std::uint8_t, 8U> retained_value{};
  std::size_t retained_frame_size = 0U;
  std::size_t retained_value_size = 0U;
  pae::protocol_core::CodecStatus decode_status = pae::protocol_core::CodecStatus::INVALID_ARGUMENT;
  std::size_t candidate_count = 0U;
};

pae::protocol_framing::FrameSinkAction DecodeCandidate(pae::protocol_framing::ByteView frame,
                                                       void* opaque) noexcept {
  auto& host = *static_cast<HostState*>(opaque);
  ++host.candidate_count;
  std::array<pae::protocol_core::DecodedFieldSlot, 4U> fields{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *host.plan, *host.codec_workspace, 0U, pae::protocol_core::ByteView{frame.data, frame.size},
      fields.data(), fields.size());
  host.decode_status = decoded.status;
  if (frame.size <= host.retained_frame.size()) {
    for (std::size_t index = 0U; index < frame.size; ++index) {
      host.retained_frame[index] = frame.data[index];
    }
    host.retained_frame_size = frame.size;
  }
  if (decoded.status == pae::protocol_core::CodecStatus::OK && decoded.field_count >= 1U &&
      fields[0].bytes_value.size <= host.retained_value.size()) {
    for (std::size_t index = 0U; index < fields[0].bytes_value.size; ++index) {
      host.retained_value[index] = fields[0].bytes_value.data[index];
    }
    host.retained_value_size = fields[0].bytes_value.size;
  }
  return pae::protocol_framing::FrameSinkAction::CONTINUE;
}

bool SubmitAcceptedPrefix(const pae::protocol_plan::PlanBundle& plan,
                          pae::protocol_framing::StreamFramingWorkspace& workspace,
                          pae::protocol_framing::ByteView bytes, HostState& host) {
  std::size_t offset = 0U;
  for (std::size_t calls = 0U; calls < 16U; ++calls) {
    const auto result = pae::protocol_framing::PushStreamChunk(
        plan, workspace, 0U,
        pae::protocol_framing::ByteView{bytes.data == nullptr ? nullptr : bytes.data + offset,
                                        bytes.size - offset},
        pae::protocol_framing::FrameSink{DecodeCandidate, &host});
    if (result.api_status != pae::protocol_framing::SubmitApiStatus::OK ||
        result.bytes_consumed > bytes.size - offset) {
      return false;
    }
    offset += result.bytes_consumed;
    if (result.stop_reason != pae::protocol_framing::SubmitStopReason::WORK_BUDGET_REACHED) {
      return offset == bytes.size;
    }
    if (result.bytes_consumed == 0U && result.frames_delivered == 0U &&
        result.work_units_used == 0U) {
      return false;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream stream(argv[1], std::ios::binary);
  const std::string json((std::istreambuf_iterator<char>(stream)),
                         std::istreambuf_iterator<char>());
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!compiled.Succeeded()) return 3;
  auto plan = std::move(compiled).TakePlan();
  auto framing = pae::protocol_framing::CreateStreamFramingWorkspace(*plan, 0U);
  if (framing.api_status != pae::protocol_framing::SubmitApiStatus::OK || !framing.workspace) {
    return 4;
  }
  pae::protocol_core::ExecutionWorkspace codec_workspace{*plan};
  HostState host{plan.get(), &codec_workspace};
  const std::array<std::uint8_t, 8U> first{{'R', 'X', ' ', 'A', '!', 'O', 'K', '\r'}};
  const std::array<std::uint8_t, 1U> second{{'\n'}};
  if (!SubmitAcceptedPrefix(*plan, *framing.workspace,
                            pae::protocol_framing::ByteView{first.data(), first.size()}, host) ||
      host.candidate_count != 0U ||
      !SubmitAcceptedPrefix(*plan, *framing.workspace,
                            pae::protocol_framing::ByteView{second.data(), second.size()}, host)) {
    return 5;
  }
  const bool ok = host.candidate_count == 1U &&
                  host.decode_status == pae::protocol_core::CodecStatus::OK &&
                  host.retained_frame_size == 9U && host.retained_value_size == 1U &&
                  host.retained_value[0] == 'A';
  if (ok) std::cout << "ASCII_STREAM_HOST_EXAMPLE_PASS candidates=1 decoded=1\n";
  return ok ? 0 : 6;
}
