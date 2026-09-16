#include "public_binary_decode.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  auto prepared = pae::protocol_lab_binary::public_decode::Adapter::Create(json, "rx", "ui_pipeline");
  if (prepared.status != pae::protocol_lab_binary::public_decode::LocalStatus::OK ||
      !prepared.adapter) return 3;
  const std::uint8_t frame[]{0x80U, 0x0DU, 3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU};
  const auto& result = prepared.adapter->Decode(0U, {frame, sizeof(frame)});
  if (result.host.status != pae::HostStatus::OK || result.host.decode_attempts != 1U ||
      !result.candidate || !result.candidate->success ||
      result.candidate->fields.size() != 9U ||
      result.candidate->fields[7].conversion_raw_int64 != 5) return 4;
  std::cout << "PUBLIC_BINARY_H1_STATIC_CONSUMER_PASS\n";
}
