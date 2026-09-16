#include <pae/compiler.h>
#include <pae/host_endpoint.h>

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
  std::size_t candidates = 0U;
  std::size_t successes = 0U;
  std::size_t encoded_size = 0U;
  std::array<std::uint8_t, 16U> encoded{};
};

pae::HostCallbackAction Observe(const pae::HostCandidateView& candidate, void* opaque) {
  auto& consumer = *static_cast<Consumer*>(opaque);
  ++consumer.candidates;
  return candidate.decode_status == pae::CodecStatus::OK ? pae::HostCallbackAction::CONTINUE
                                                         : pae::HostCallbackAction::STOP;
}

pae::HostCallbackAction Deliver(const pae::HostOutputView& output, void* opaque) {
  auto& consumer = *static_cast<Consumer*>(opaque);
  ++consumer.successes;
  if (output.action == pae::HostAction::ENCODE) {
    consumer.encoded_size = output.bytes.size;
    for (std::size_t index = 0U; index < output.bytes.size && index < consumer.encoded.size();
         ++index) {
      consumer.encoded[index] = output.bytes.data[index];
    }
  }
  return pae::HostCallbackAction::CONTINUE;
}

pae::ByteView Bytes(std::string_view value) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_public_host_example <ascii-stream-config.pae.json>\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>(input), {});
  auto compilation = pae::CompileProtocolJson(json);
  if (!compilation.Succeeded()) return 1;
  pae::CompiledProtocol compiled = std::move(compilation).TakeCompiled();

  const std::array<pae::HostBindingSpec, 2U> bindings{
      {{"device", pae::HostAction::DECODE, 0U, 1U, {}},
       {"device", pae::HostAction::ENCODE, 0U, 1U, {}}}};
  auto created = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  if (created.status != pae::HostStatus::OK || !created.host) return 1;

  const auto receive = created.host->Find("device", pae::HostAction::DECODE);
  auto transmit = created.host->Find("device", pae::HostAction::ENCODE);
  if (receive.status != pae::HostStatus::OK || transmit.status != pae::HostStatus::OK) return 1;

  Consumer consumer;
  const auto first = created.host->Push(receive.handle, Bytes("RX A!OK\r"), {Deliver, &consumer},
                                        {Observe, &consumer});
  const auto second =
      created.host->Push(receive.handle, Bytes("\n"), {Deliver, &consumer}, {Observe, &consumer});
  if (first.status != pae::HostStatus::OK || first.decode_attempts != 0U ||
      second.status != pae::HostStatus::OK || second.decode_successes != 1U ||
      consumer.candidates != 1U || consumer.successes != 1U) {
    return 1;
  }

  if (created.host->Reset(receive.handle) != pae::HostStatus::OK ||
      created.host->Observe(receive.handle).status != pae::HostStatus::STALE_HANDLE ||
      created.host->Find("device", pae::HostAction::DECODE).status != pae::HostStatus::OK) {
    return 1;
  }

  const std::array<std::uint8_t, 1U> name{{'A'}};
  const std::array<std::uint8_t, 1U> tag{{'Z'}};
  const std::array<pae::EncodeValue, 2U> values{
      {pae::EncodeValue::Bytes({0U, 0U}, {name.data(), name.size()}),
       pae::EncodeValue::Bytes({0U, 2U}, {tag.data(), tag.size()})}};
  const auto encoded =
      created.host->Encode(transmit.handle, 0U, values.data(), values.size(), {Deliver, &consumer});
  const std::string expected = "TX A!Z\r\n";
  bool equal = encoded.status == pae::HostStatus::OK && encoded.bytes_produced == expected.size() &&
               consumer.encoded_size == expected.size();
  for (std::size_t index = 0U; equal && index < expected.size(); ++index) {
    equal = consumer.encoded[index] == static_cast<std::uint8_t>(expected[index]);
  }
  if (!equal) return 1;

  const auto literal = created.host->Encode(transmit.handle, 2U, nullptr, 0U, {Deliver, &consumer});
  const std::string literal_expected = "SEND\r\n";
  equal = literal.status == pae::HostStatus::OK &&
          literal.bytes_produced == literal_expected.size() &&
          consumer.encoded_size == literal_expected.size();
  for (std::size_t index = 0U; equal && index < literal_expected.size(); ++index) {
    equal = consumer.encoded[index] == static_cast<std::uint8_t>(literal_expected[index]);
  }
  if (!equal) return 1;

  std::cout << "PUBLIC_HOST_CONSUMER_PASS candidates=" << consumer.candidates
            << " successes=" << consumer.successes << " greeting_bytes=" << encoded.bytes_produced
            << " literal_bytes=" << literal.bytes_produced << '\n';
  return 0;
}
