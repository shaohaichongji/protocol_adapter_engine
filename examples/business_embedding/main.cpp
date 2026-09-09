#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "business_adapter.h"

namespace {

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_business_embedding_example <config.pae.json>\n";
    return 2;
  }
  const std::string config = ReadFile(argv[1]);
  if (config.empty()) {
    std::cerr << "configuration could not be read\n";
    return 3;
  }

  pae::examples::business_embedding::Measurement measurement;
  std::vector<std::uint8_t> bytes;
  std::string error;
  auto adapter = pae::examples::business_embedding::BusinessAdapter::Initialize(
      config,
      {[&](const auto& value) { measurement = value; },
       [&](pae::protocol_core::ByteView value) {
         bytes.assign(value.data, value.data + value.size);
       }},
      error);
  if (!adapter) {
    std::cerr << "initialization failed: " << error << '\n';
    return 4;
  }

  const std::uint8_t received[] = {0xA1U, 0x02U, 0x08U, 0x01U, 0xACU};
  if (!adapter->OnReceivedRecord({received, sizeof(received)}).Succeeded() ||
      measurement.temperature.coefficient != 12 || measurement.temperature.scale != 0 ||
      !measurement.alarm) {
    std::cerr << "measurement example failed\n";
    return 5;
  }
  if (!adapter->SendCommand({{125, 1}}).Succeeded() ||
      bytes != std::vector<std::uint8_t>({0xB2U, 0x00U, 0x19U, 0x5AU, 0x25U})) {
    std::cerr << "command example failed\n";
    return 6;
  }
  std::cout << "BUSINESS_EMBEDDING_EXAMPLE=PASS\n";
  return 0;
}
