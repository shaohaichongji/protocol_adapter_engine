#include <pae/codec.h>
#include <pae/compiler.h>
#include <pae/stream_framer.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

namespace {

struct Consumer {
  pae::CompleteRecordCodec* codec = nullptr;
  std::size_t pipeline = 0U;
  std::size_t candidates = 0U;
  std::size_t decode_calls = 0U;
  std::size_t successes = 0U;
};

pae::FrameSinkAction DecodeCandidate(pae::FrameCandidateView candidate, void* opaque) noexcept {
  auto& consumer = *static_cast<Consumer*>(opaque);
  ++consumer.candidates;
  ++consumer.decode_calls;
  const auto decoded = consumer.codec->Decode(consumer.pipeline, candidate.bytes);
  if (decoded.status == pae::CodecStatus::OK) ++consumer.successes;
  return pae::FrameSinkAction::CONTINUE;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_public_framer_example <stream-config.pae.json>\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>(input), {});
  auto compilation = pae::CompileProtocolJson(json);
  if (!compilation.Succeeded()) return 1;
  pae::CompiledProtocol compiled = std::move(compilation).TakeCompiled();

  const auto capability = pae::QueryStreamFramingCapability(compiled, 0U);
  if (capability.status != pae::StreamFramerStatus::OK || !capability.available) return 1;
  auto framer = pae::CreateStreamFramer(compiled, 0U);
  auto codec = pae::CreateCompleteRecordCodec(compiled);
  if (framer.status != pae::StreamFramerStatus::OK || !framer.framer ||
      codec.status != pae::CodecStatus::OK || !codec.codec) {
    return 1;
  }

  Consumer consumer{codec.codec.get(), 0U};
  const std::array<std::uint8_t, 6U> input_bytes{{0xAAU, 0x00U, 0x01U, 0xAAU, 0x00U, 0x02U}};
  const auto submitted =
      framer.framer->Push({input_bytes.data(), input_bytes.size()}, {DecodeCandidate, &consumer});
  if (submitted.status != pae::StreamFramerStatus::OK ||
      submitted.bytes_consumed != input_bytes.size() || submitted.candidates_delivered != 2U ||
      consumer.candidates != 2U || consumer.decode_calls != 2U || consumer.successes != 2U) {
    return 1;
  }

  std::cout << "PUBLIC_FRAMER_CONSUMER_PASS candidates=" << consumer.candidates
            << " decode_calls=" << consumer.decode_calls << '\n';
  return 0;
}
