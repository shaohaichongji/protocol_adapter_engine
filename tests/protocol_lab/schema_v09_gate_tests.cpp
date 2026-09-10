#include <filesystem>
#include <string>
#include <vector>

#include "evidence_bundle.h"
#include "protocol_lab.h"
#include "udp_exchange.h"

namespace {

class CountingUdpAdapter final : public pae::protocol_lab::IUdpExchangeAdapter {
 public:
  pae::protocol_lab::UdpExchangeResponse Exchange(
      const pae::protocol_lab::UdpExchangeRequest&) override {
    ++call_count;
    return {};
  }
  std::size_t call_count = 0U;
};

class CountingExecutionObserver final : public pae::protocol_lab::IProtocolLabExecutionObserver {
 public:
  void OnProtocolOperation(const char*) override { ++call_count; }
  std::size_t call_count = 0U;
};

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  const std::filesystem::path data{argv[1]};
  const std::filesystem::path root{argv[2]};
  std::filesystem::remove_all(root);
  std::vector<std::string> arguments{
      "pae_protocol_lab",   "udp-exchange",
      "--config",           (data / "synthetic_stream_framing_slice.pae.json").string(),
      "--values",           (data / "valid_command.values.pae-lab.json").string(),
      "--remote",           "127.0.0.1:9",
      "--receive-pipeline", "sync_fixed_rx",
      "--record-root",      root.string(),
      "--output",           "json"};
  std::vector<char*> raw;
  raw.reserve(arguments.size());
  for (std::string& argument : arguments) raw.push_back(argument.data());
  pae::protocol_lab::StandardRecordFileSystem file_system;
  CountingUdpAdapter udp;
  CountingExecutionObserver execution;
  const int exit_code = pae::protocol_lab::RunApplicationWithDependencies(
      static_cast<int>(raw.size()), raw.data(), file_system, udp, &execution);
  const bool files_exist =
      std::filesystem::exists(root) && std::filesystem::recursive_directory_iterator(root) !=
                                           std::filesystem::recursive_directory_iterator{};
  return exit_code == 4 && udp.call_count == 0U && execution.call_count == 0U && !files_exist ? 0
                                                                                              : 1;
}
