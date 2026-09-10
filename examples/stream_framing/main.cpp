#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

#include "../../src/config_compiler/config_compiler.h"
#include "../../src/protocol_core/complete_record_codec.h"
#include "../../src/protocol_framing/stream_framer.h"

namespace {

struct DecodeSinkContext {
  const pae::protocol_plan::PlanBundle* plan = nullptr;
  pae::protocol_core::ExecutionWorkspace* codec_workspace = nullptr;
  std::size_t decoded_frames = 0U;
};

pae::protocol_framing::FrameSinkAction DecodeFrame(pae::protocol_framing::ByteView frame,
                                                   void* user_data) noexcept {
  auto& context = *static_cast<DecodeSinkContext*>(user_data);
  std::array<pae::protocol_core::DecodedFieldSlot, 8U> fields{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *context.plan, *context.codec_workspace, 1U,
      pae::protocol_core::ByteView{frame.data, frame.size}, fields.data(), fields.size());
  if (decoded.status == pae::protocol_core::CodecStatus::OK) ++context.decoded_frames;
  return pae::protocol_framing::FrameSinkAction::CONTINUE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!compiled.Succeeded()) return 3;
  auto plan = std::move(compiled).TakePlan();
  auto framing = pae::protocol_framing::CreateStreamFramingWorkspace(*plan, 1U);
  if (framing.api_status != pae::protocol_framing::SubmitApiStatus::OK) return 4;
  pae::protocol_core::ExecutionWorkspace codec_workspace{*plan};
  DecodeSinkContext context{plan.get(), &codec_workspace};
  const std::array<std::uint8_t, 3U> first{{0x00U, 0xA5U, 0x5AU}};
  const std::array<std::uint8_t, 2U> second{{0x01U, 0x02U}};
  const pae::protocol_framing::FrameSink sink{DecodeFrame, &context};
  const auto first_result = pae::protocol_framing::PushStreamChunk(
      *plan, *framing.workspace, 1U, pae::protocol_framing::ByteView{first.data(), first.size()},
      sink);
  const auto second_result = pae::protocol_framing::PushStreamChunk(
      *plan, *framing.workspace, 1U, pae::protocol_framing::ByteView{second.data(), second.size()},
      sink);
  if (first_result.bytes_consumed != first.size() || first_result.bytes_discarded != 1U ||
      second_result.bytes_consumed != second.size() || second_result.frames_delivered != 1U ||
      context.decoded_frames != 1U) {
    return 5;
  }
  std::cout << "STREAM_FRAMING_EXAMPLE decoded_frames=1 network_calls=0\n";
  return 0;
}
