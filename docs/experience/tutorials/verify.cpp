#include <pae/compiler.h>
#include <pae/codec.h>
#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

int main(int argc, char** argv) {
  if (argc != 3) { std::cerr << "Usage: config_tutorial <config.json> <binary|ascii|missing-property|unknown-reference>\n"; return 2; }
  const std::string mode = argv[2];
  if (mode != "binary" && mode != "ascii" && mode != "missing-property" && mode != "unknown-reference") return 2;
  std::ifstream file(argv[1], std::ios::binary);
  if (!file) { std::cerr << "Cannot open configuration\n"; return 2; }
  std::string json{std::istreambuf_iterator<char>(file), {}};
  // 故障只注入内存副本，不修改磁盘上的合法配置。
  const bool fault = mode == "missing-property" || mode == "unknown-reference";
  if (fault) {
    const std::string before = mode == "missing-property"
        ? "\"protocol_id\": \"tutorial_binary\","
        : "\"message_ids\": [\"value_record\"]";
    const std::string after = mode == "missing-property"
        ? "" : "\"message_ids\": [\"missing_record\"]";
    const auto position = json.find(before);
    if (position == std::string::npos) return 10;
    json.replace(position, before.size(), after);
  }
  auto result = pae::CompileProtocolJson(json);
  if (fault) {
    const auto* diagnostic = result.Diagnostic();
    const auto expected = mode == "missing-property" ? pae::CompileError::MISSING_PROPERTY : pae::CompileError::UNKNOWN_REFERENCE;
    if (result.Succeeded() || diagnostic == nullptr || diagnostic->code != expected || diagnostic->json_pointer.empty()) return 11;
    std::cout << "EXPECTED " << (mode == "missing-property" ? "MISSING_PROPERTY" : "UNKNOWN_REFERENCE")
              << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail << "\nTUTORIAL PASS\n";
    return 0;
  }
  if (!result.Succeeded()) {
    if (const auto* diagnostic = result.Diagnostic()) {
      std::cerr << "compile failed stage=" << static_cast<int>(diagnostic->stage)
                << " code=" << static_cast<int>(diagnostic->code)
                << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail << '\n';
    }
    return 3;
  }
  auto compiled = std::move(result).TakeCompiled();
  auto created = pae::CreateCompleteRecordCodec(compiled);
  if (!created.codec) return 4;
  const bool binary = mode == "binary";
  // 本教学配置只有一条管线和一条消息，索引0是显式约定；通用宿主应查询ID对应的索引。
  const std::array<std::uint8_t, 3> input{0xAA, 1, 2};
  const std::array<std::uint8_t, 6> ping{'P','I','N','G',13,10};
  auto decoded = created.codec->Decode(0, binary ? pae::ByteView{input.data(), input.size()} : pae::ByteView{ping.data(), ping.size()});
  if (decoded.status != pae::CodecStatus::OK || decoded.record.FieldCount() != (binary ? 1U : 0U)) return 5;
  if (binary && (!decoded.record.Field(0).has_value() || decoded.record.Field(0)->UInt64() != 258U)) return 9;
  std::array<std::uint8_t, 6> output{};
  const auto value = pae::EncodeValue::UInt64({0, 0}, 258);
  auto encoded = created.codec->Encode(0, 0, binary ? &value : nullptr, binary ? 1 : 0, {output.data(), output.size()});
  const std::array<std::uint8_t, 6> pong{'P','O','N','G',13,10};
  if (encoded.status != pae::CodecStatus::OK || encoded.bytes_written != (binary ? 3U : 6U)) return 6;
  for (std::size_t i = 0; i < encoded.bytes_written; ++i) {
    if (output[i] != (binary ? input[i] : pong[i])) return 7;
  }
  const std::array<std::uint8_t, 3> bad{0xAB, 0, 7};
  if (binary && created.codec->Decode(0, {bad.data(), bad.size()}).status == pae::CodecStatus::OK) return 8;
  if (binary && created.codec->Decode(0, {input.data(), 2}).status == pae::CodecStatus::OK) return 12;
  // Encode/下一次Decode前已读取完旧借用视图，不跨调用保存decoded.record。
  std::cout << "TUTORIAL PASS\n";
}
