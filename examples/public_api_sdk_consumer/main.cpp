#include <pae/codec.h>
#include <pae/compiler.h>
#include <pae/host_endpoint.h>
#include <pae/stream_framer.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

template <std::size_t Size>
pae::ByteView Bytes(const std::array<std::uint8_t, Size>& bytes) noexcept {
  return {bytes.data(), bytes.size()};
}

pae::ByteView Bytes(std::string_view text) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(text.data()), text.size()};
}

struct FrameCapture {
  std::size_t count = 0U;
  std::array<std::uint8_t, 3U> last{};
};

pae::FrameSinkAction CaptureFrame(pae::FrameCandidateView candidate, void* opaque) noexcept {
  auto& capture = *static_cast<FrameCapture*>(opaque);
  ++capture.count;
  if (candidate.bytes.size == capture.last.size()) {
    for (std::size_t index = 0U; index < capture.last.size(); ++index) {
      capture.last[index] = candidate.bytes.data[index];
    }
  }
  return pae::FrameSinkAction::CONTINUE;
}

struct HostCapture {
  std::size_t decode_count = 0U;
  std::size_t encode_count = 0U;
  std::size_t message_index = static_cast<std::size_t>(-1);
  std::size_t decode_message_index = static_cast<std::size_t>(-1);
  std::size_t candidate_count = 0U;
  std::optional<std::size_t> candidate_message_index;
  pae::CodecStatus candidate_status = pae::CodecStatus::INVALID_ARGUMENT;
  bool candidate_record_valid = false;
  std::array<std::uint8_t, 16U> bytes{};
  std::size_t byte_count = 0U;
};

pae::HostCallbackAction CaptureCandidate(const pae::HostCandidateView& candidate, void* opaque) {
  auto& capture = *static_cast<HostCapture*>(opaque);
  ++capture.candidate_count;
  capture.candidate_message_index = candidate.matched_message_index;
  capture.candidate_status = candidate.decode_status;
  capture.candidate_record_valid = candidate.record.HasValue();
  return pae::HostCallbackAction::CONTINUE;
}

pae::HostCallbackAction CaptureHost(const pae::HostOutputView& output, void* opaque) {
  auto& capture = *static_cast<HostCapture*>(opaque);
  capture.message_index = output.message_index;
  if (output.action == pae::HostAction::DECODE) {
    ++capture.decode_count;
    capture.decode_message_index = output.message_index;
  } else {
    ++capture.encode_count;
    capture.byte_count = output.bytes.size;
    for (std::size_t index = 0U; index < output.bytes.size && index < capture.bytes.size();
         ++index) {
      capture.bytes[index] = output.bytes.data[index];
    }
  }
  return pae::HostCallbackAction::CONTINUE;
}

bool Equals(const HostCapture& capture, std::string_view expected) noexcept {
  if (capture.byte_count != expected.size()) return false;
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    if (capture.bytes[index] != static_cast<std::uint8_t>(expected[index])) return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: pae_sdk_stage3_consumer <binary-config> <ascii-stream-config>\n";
    return 2;
  }

  auto binary_result = pae::CompileProtocolJson(ReadFile(argv[1]));
  auto ascii_result = pae::CompileProtocolJson(ReadFile(argv[2]));
  if (!binary_result.Succeeded() || !ascii_result.Succeeded()) return 3;
  pae::CompiledProtocol binary = std::move(binary_result).TakeCompiled();
  pae::CompiledProtocol ascii = std::move(ascii_result).TakeCompiled();
  if (!binary.Protocol() || binary.PipelineCount() != 3U || binary.MessageCount() != 3U ||
      !ascii.PipelineMessageExecution(0U, 0U)->encode_available ||
      !ascii.PipelineMessageExecution(0U, 2U)->encode_available) {
    return 4;
  }
  const auto binary_representation = binary.MessageRepresentation(0U);
  const auto binary_message = binary.MessagePhysical(0U);
  const auto binary_field = binary.ResolveFieldPhysical(0U, 3U);
  const auto ascii_representation = ascii.MessageRepresentation(0U);
  const auto ascii_rx = ascii.AsciiAction(0U, pae::AsciiAction::DECODE);
  const auto ascii_tx = ascii.AsciiAction(0U, pae::AsciiAction::ENCODE);
  const auto ascii_rx_field = ascii.AsciiSegment(0U, pae::AsciiAction::DECODE, 1U);
  const auto ascii_tx_first = ascii.AsciiSegment(0U, pae::AsciiAction::ENCODE, 0U);
  const auto ascii_name = ascii.AsciiField(0U);
  if (binary_representation.status != pae::PhysicalQueryStatus::OK ||
      binary_representation.value != pae::RecordRepresentation::BINARY ||
      binary_message.status != pae::PhysicalQueryStatus::OK || !binary_message.value ||
      binary_message.value->record_length.minimum != 3U ||
      binary_message.value->record_length.maximum != 3U ||
      binary_field.status != pae::PhysicalQueryStatus::OK || !binary_field.value ||
      !binary_field.value->byte_range || binary_field.value->byte_range->offset != 1U ||
      binary_field.value->byte_range->length != 2U ||
      ascii_representation.status != pae::PhysicalQueryStatus::OK ||
      ascii_representation.value != pae::RecordRepresentation::ASCII_TEXT ||
      ascii.MessagePhysical(0U).status != pae::PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED ||
      ascii_rx.status != pae::AsciiQueryStatus::OK || !ascii_rx.value ||
      ascii_rx.value->segment_count != 5U || ascii_tx.status != pae::AsciiQueryStatus::OK ||
      !ascii_tx.value || ascii_tx.value->segment_count != 5U ||
      ascii_rx_field.status != pae::AsciiQueryStatus::OK || !ascii_rx_field.value ||
      ascii_rx_field.value->kind != pae::AsciiSegmentKind::FIELD ||
      ascii_rx_field.value->field_index != 0U || ascii_rx_field.value->flat_field_index != 0U ||
      ascii_tx_first.status != pae::AsciiQueryStatus::OK || !ascii_tx_first.value ||
      ascii_tx_first.value->kind != pae::AsciiSegmentKind::LITERAL ||
      !ascii_tx_first.value->literal || ascii_tx_first.value->literal->size != 3U ||
      ascii_tx_first.value->literal->data[0] != 'T' ||
      ascii_name.status != pae::AsciiQueryStatus::OK || !ascii_name.value ||
      !ascii_name.value->decode_referenced || !ascii_name.value->encode_referenced) {
    return 12;
  }

  auto codec_result = pae::CreateCompleteRecordCodec(binary);
  if (codec_result.status != pae::CodecStatus::OK || !codec_result.codec) return 5;
  const std::array<pae::EncodeValue, 1U> binary_values{{pae::EncodeValue::UInt64({0U, 0U}, 7U)}};
  std::array<std::uint8_t, 3U> binary_frame{};
  const auto encoded =
      codec_result.codec->Encode(0U, 0U, binary_values.data(), binary_values.size(),
                                 {binary_frame.data(), binary_frame.size()});
  const std::array<std::uint8_t, 3U> binary_expected{{0xAAU, 0x00U, 0x07U}};
  const auto decoded = codec_result.codec->Decode(0U, Bytes(binary_expected));
  const auto decoded_field = decoded.record.Field(0U);
  if (encoded.status != pae::CodecStatus::OK || binary_frame != binary_expected ||
      decoded.status != pae::CodecStatus::OK || decoded.matched_message_index != 0U ||
      !decoded_field || decoded_field->UInt64() != 7U) {
    return 6;
  }

  auto ascii_codec_result = pae::CreateCompleteRecordCodec(ascii);
  if (ascii_codec_result.status != pae::CodecStatus::OK || !ascii_codec_result.codec) return 13;
  const std::string ascii_frame = "RX A!OK\r\n";
  const auto ascii_decoded = ascii_codec_result.codec->Decode(0U, Bytes(ascii_frame));
  const auto ascii_decoded_name = ascii_decoded.record.Field(0U);
  if (ascii_decoded.status != pae::CodecStatus::OK || ascii_decoded.matched_message_index != 0U ||
      !ascii_decoded_name || !ascii_decoded_name->Bytes() ||
      ascii_decoded_name->Bytes()->data != Bytes(ascii_frame).data + 3U ||
      ascii_decoded_name->Bytes()->size != 1U) {
    return 14;
  }

  auto framer_result = pae::CreateStreamFramer(binary, 0U);
  if (framer_result.status != pae::StreamFramerStatus::OK || !framer_result.framer) return 7;
  FrameCapture frame_capture;
  const std::array<std::uint8_t, 6U> two_frames{{0xAAU, 0x00U, 0x01U, 0xAAU, 0x00U, 0x02U}};
  const auto framed = framer_result.framer->Push(Bytes(two_frames), {CaptureFrame, &frame_capture});
  if (framed.status != pae::StreamFramerStatus::OK || framed.candidates_delivered != 2U ||
      frame_capture.count != 2U || frame_capture.last[2] != 0x02U) {
    return 8;
  }

  const std::array<pae::HostBindingSpec, 2U> bindings{
      {{"sdk-device", pae::HostAction::DECODE, 0U, 1U, {}},
       {"sdk-device", pae::HostAction::ENCODE, 0U, 1U, {}}}};
  auto host_result = pae::CreateHostEndpoint(ascii, bindings.data(), bindings.size());
  if (host_result.status != pae::HostStatus::OK || !host_result.host) return 9;
  const auto receive = host_result.host->Find("sdk-device", pae::HostAction::DECODE);
  const auto transmit = host_result.host->Find("sdk-device", pae::HostAction::ENCODE);
  if (receive.status != pae::HostStatus::OK || transmit.status != pae::HostStatus::OK) return 10;

  HostCapture host_capture;
  const auto host_decoded =
      host_result.host->Push(receive.handle, Bytes("RX A!OK\r\n"), {CaptureHost, &host_capture},
                             {CaptureCandidate, &host_capture});
  const std::array<std::uint8_t, 1U> name{{'A'}};
  const std::array<std::uint8_t, 1U> tag{{'Z'}};
  const std::array<pae::EncodeValue, 2U> ascii_values{
      {pae::EncodeValue::Bytes({0U, 0U}, Bytes(name)),
       pae::EncodeValue::Bytes({0U, 2U}, Bytes(tag))}};
  const auto greeting = host_result.host->Encode(transmit.handle, 0U, ascii_values.data(),
                                                 ascii_values.size(), {CaptureHost, &host_capture});
  const bool greeting_equal = Equals(host_capture, "TX A!Z\r\n");
  const auto literal =
      host_result.host->Encode(transmit.handle, 2U, nullptr, 0U, {CaptureHost, &host_capture});
  if (host_decoded.status != pae::HostStatus::OK || host_capture.decode_count != 1U ||
      host_capture.candidate_count != 1U || host_capture.candidate_message_index != 0U ||
      host_capture.candidate_status != pae::CodecStatus::OK ||
      !host_capture.candidate_record_valid || host_capture.decode_message_index != 0U ||
      greeting.status != pae::HostStatus::OK || !greeting_equal ||
      literal.status != pae::HostStatus::OK || host_capture.message_index != 2U ||
      !Equals(host_capture, "SEND\r\n") || host_capture.encode_count != 2U) {
    return 11;
  }

  std::cout << "PAE_SDK_STAGE3_CONSUMER_PASS compiler=1 metadata=1 physical_query=1 "
               "ascii_facts=1 codec=1 framer=1 host=1 multi_message_encode=1\n";
  return 0;
}
