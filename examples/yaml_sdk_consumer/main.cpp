#include <pae/codec.h>
#include <pae/compiler.h>
#include <pae/yaml_frontend.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <utility>

namespace {

std::optional<std::string> ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool Run(const char* path) {
  const auto source = ReadFile(path);
  if (!source) return false;
  auto converted = pae::yaml::ConvertToStrictJson(*source, "synthetic_fixed_message.pae.yaml");
  if (!converted.Succeeded() || converted.Json().empty() ||
      converted.SourceIdentity() != "synthetic_fixed_message.pae.yaml") {
    std::cerr << "YAML conversion failed: " << converted.Reason() << '\n';
    return false;
  }
  const auto source_location = converted.FindSource("/messages/0/fields/0/value_type");
  if (!source_location || source_location->value_line == 0U || source_location->ancestor_fallback) {
    return false;
  }

  auto compiled_result = pae::CompileProtocolJson(converted.Json());
  if (!compiled_result.Succeeded()) {
    const auto* diagnostic = compiled_result.Diagnostic();
    std::cerr << "compile failed";
    if (diagnostic) std::cerr << " at " << diagnostic->json_pointer;
    std::cerr << '\n';
    return false;
  }
  auto compiled = std::move(compiled_result).TakeCompiled();
  const auto protocol = compiled.Protocol();
  const auto message_index = compiled.PipelineMessageIndex(0U, 0U);
  if (!protocol || protocol->id != "synthetic_yaml_sdk" || !message_index) return false;
  const auto message = compiled.Message(*message_index);
  if (!message || message->field_count != 1U) return false;
  const auto field = compiled.Field(message->field_begin);
  if (!field || field->id != "synthetic_value" || field->value_kind != pae::ValueKind::UINT64)
    return false;

  auto created = pae::CreateCompleteRecordCodec(compiled);
  if (created.status != pae::CodecStatus::OK || !created.codec) return false;
  constexpr std::array<std::uint8_t, 3U> frame{0xAAU, 0x00U, 0x07U};
  const auto decoded = created.codec->Decode(0U, {frame.data(), frame.size()});
  const auto decoded_field = decoded.record.Field(0U);
  if (decoded.status != pae::CodecStatus::OK || decoded.matched_message_index != message_index ||
      !decoded_field || decoded_field->UInt64() != 7U) {
    return false;
  }
  const std::array values{pae::EncodeValue::UInt64({*message_index, field->index_in_message}, 7U)};
  std::array<std::uint8_t, 3U> encoded{};
  const auto encode = created.codec->Encode(0U, *message_index, values.data(), values.size(),
                                            {encoded.data(), encoded.size()});
  if (encode.status != pae::CodecStatus::OK || encode.bytes_written != frame.size() ||
      encoded != frame) {
    return false;
  }
  std::cout << "PAE_YAML_SDK_CONSUMER_PASS\n";
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_yaml_sdk_consumer <synthetic_fixed_message.pae.yaml>\n";
    return 2;
  }
  return Run(argv[1]) ? 0 : 1;
}
