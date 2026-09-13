#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "../../src/config_compiler/config_compiler.h"
#include "host_endpoint.h"

namespace host = pae::host_endpoint;
int main(int argc, char** argv) {
  if (argc != 2) return 1;
  std::ifstream file(argv[1], std::ios::binary);
  const std::string json{std::istreambuf_iterator<char>(file), {}};
  auto compiled = pae::config_compiler::CompileJsonToPlan(json);
  if (!compiled.Succeeded()) return 2;
  const host::BindingSpec specs[] = {{"device", host::Action::DECODE, "ascii_pipeline", 2U},
                                     {"device", host::Action::ENCODE, "ascii_pipeline"}};
  auto created = host::Session::Create(std::move(compiled).TakePlan(), specs, 2U);
  if (!created.session) return 3;
  auto& session = *created.session;
  struct Captured {
    std::size_t decoded = 0U;
    std::string encoded;
  } captured;
  host::Sink sink{[](const host::Output& output, void* data) {
                    auto& value = *static_cast<Captured*>(data);
                    if (output.action == host::Action::DECODE)
                      ++value.decoded;
                    else
                      value.encoded.assign(reinterpret_cast<const char*>(output.bytes.data),
                                           output.bytes.size);
                    return host::SinkAction::STOP;
                  },
                  &captured};
  const auto rx = session.Find("device", host::Action::DECODE);
  const std::string input = "ONLY\r\nONLY\r\n";
  std::size_t offset = 0U;
  // Host owns the suffix. Bounded explicit driving, no hidden loop inside Session::Push.
  for (std::size_t step = 0U; step < 16U; ++step) {
    const auto before = session.Observe(rx);
    if (offset == input.size() && !before.framing.has_internal_work) break;
    const auto result = session.Push(
        rx, {reinterpret_cast<const std::uint8_t*>(input.data()) + offset, input.size() - offset},
        sink);
    if (result.status != host::Status::OK || result.framing.bytes_consumed > input.size() - offset)
      return 4;
    offset += result.framing.bytes_consumed;
    if (!result.framing.bytes_consumed && !result.framing.frames_delivered &&
        !result.framing.work_units_used)
      return 5;
  }
  const auto encoded = session.Encode(session.Find("device", host::Action::ENCODE), "encode_only",
                                      nullptr, 0U, sink);
  if (offset != input.size() || captured.decoded != 2U || encoded.status != host::Status::OK ||
      captured.encoded != "SEND\r\n" || session.Observe(rx).framing.has_internal_work)
    return 6;
  std::cout << "HOST_ENDPOINT_EXAMPLE=PASS (offline synthetic only)\n";
  return 0;
}
