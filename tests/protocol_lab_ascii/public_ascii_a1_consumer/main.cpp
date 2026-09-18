#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "public_ascii_offline_adapter.h"

using namespace pae::protocol_lab_ascii::public_offline;

namespace {

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()),
          reinterpret_cast<const std::uint8_t*>(text.data()) + text.size()};
}

std::string_view Text(const std::vector<std::uint8_t>& bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto compiled = pae::CompileProtocolJson(json);
  if (!compiled.Succeeded()) return 3;
  auto prepared = Adapter::AdoptCompiled(std::move(compiled).TakeCompiled());
  if (prepared.status != LocalStatus::OK || !prepared.adapter) return 4;
  auto frame = Bytes("RX SDK!OK\r\n");
  const auto decoded = prepared.adapter->Decode(0U, {frame.data(), frame.size()});
  const auto encoded = prepared.adapter->Encode(0U, 0U, {{0U, Bytes("SDK")}, {2U, Bytes("Z")}});
  if (decoded.local_status != LocalStatus::OK || decoded.fields.size() != 2U ||
      decoded.fields[0].range.offset != 3U || Text(decoded.fields[0].bytes) != "SDK" ||
      encoded.local_status != LocalStatus::OK || Text(encoded.frame) != "TX SDK!Z\r\n" ||
      encoded.fields.size() != 2U || encoded.fields[0].range.offset != 3U) {
    return 5;
  }
  std::cout << "PUBLIC_ASCII_A1_PACKAGE_CONSUMER_PASS\n";
  return 0;
}
