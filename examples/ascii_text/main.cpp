#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "complete_record_codec.h"
#include "config_compiler.h"

namespace {

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

pae::protocol_core::ByteView Bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_ascii_text_example <synthetic-config>\n";
    return 2;
  }
  auto compiled = pae::config_compiler::CompileJsonToPlan(ReadFile(argv[1]));
  if (!compiled.Succeeded()) {
    std::cerr << "configuration compilation failed\n";
    return 3;
  }
  auto plan = std::move(compiled).TakePlan();
  pae::protocol_core::ExecutionWorkspace workspace{*plan};

  const std::string name = "ALICE";
  const std::string tag = "Z";
  std::array<pae::protocol_core::EncodeFieldValue, 2> values{};
  values[0].field = {plan.get(), 0U, 0U};
  values[0].value_kind = pae::protocol_core::LogicalValueKind::BYTES;
  values[0].bytes_value = Bytes(name);
  values[1].field = {plan.get(), 0U, 2U};
  values[1].value_kind = pae::protocol_core::LogicalValueKind::BYTES;
  values[1].bytes_value = Bytes(tag);
  std::array<std::uint8_t, 32> output{};
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(), {output.data(), output.size()});
  const std::string_view expected_tx = "TX ALICE!Z\r\n";
  if (encoded.status != pae::protocol_core::CodecStatus::OK ||
      encoded.bytes_written != expected_tx.size() ||
      std::string_view{reinterpret_cast<const char*>(output.data()), encoded.bytes_written} !=
          expected_tx) {
    std::cerr << "independent TX vector failed\n";
    return 4;
  }

  const std::string rx = "RX ALICE!OK\r\n";
  std::array<pae::protocol_core::DecodedFieldSlot, 2> slots{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(*plan, workspace, 0U, Bytes(rx),
                                                                slots.data(), slots.size());
  if (decoded.status != pae::protocol_core::CodecStatus::OK || decoded.field_count != 2U ||
      std::string_view{reinterpret_cast<const char*>(slots[0].bytes_value.data),
                       slots[0].bytes_value.size} != "ALICE" ||
      std::string_view{reinterpret_cast<const char*>(slots[1].bytes_value.data),
                       slots[1].bytes_value.size} != "OK") {
    std::cerr << "independent RX vector failed\n";
    return 5;
  }
  std::cout << "ASCII complete-record example passed\n";
  return 0;
}
