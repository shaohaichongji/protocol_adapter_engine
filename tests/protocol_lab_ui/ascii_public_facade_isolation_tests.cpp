#include "ascii_host_adapter.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <type_traits>
#include <vector>

namespace {

std::string ReadConfig() {
  std::ifstream input(std::filesystem::path{PAE_ASCII_TEXT_CONFIG}, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}  // namespace

int main() {
  using namespace pae::protocol_lab_ui;
  static_assert(std::is_same_v<decltype(AsciiHostBinding::action), AsciiHostAction>);
  static_assert(std::is_same_v<decltype(AsciiExecutionResult::status), AsciiAdapterStatus>);

  const auto config = ReadConfig();
  assert(!config.empty());
  auto compilation = pae::CompileProtocolJson(config);
  assert(compilation.Succeeded());

  std::string error;
  auto adapter = AsciiHostAdapter::CreatePublicDirect(
      std::move(compilation).TakeCompiled(), error);
  assert(adapter && error.empty());
  assert(adapter->IsPublicCompleteRecord() && !adapter->IsPublicStream());
  assert(adapter->Description().schema_version == "0.10");
  assert(adapter->Description().pipelines.size() == 1U);
  assert(adapter->Description().messages.size() == 1U);

  const auto decode_binding = adapter->FindBinding(0U, AsciiHostAction::DECODE);
  assert(decode_binding.has_value());
  AsciiExecutionIdentity identity;
  identity.pipeline_index = 0U;
  identity.pipeline_id = "ascii_pipeline";
  const std::vector<std::uint8_t> frame{'R', 'X', ' ', 'A', 'L', 'I', 'C', 'E',
                                        '!', 'O', 'K', '\r', '\n'};
  const auto result = adapter->Inspect(*decode_binding, 0U, identity, frame);

  assert(result.status == AsciiAdapterStatus::OK);
  assert(result.codec_called);
  assert(result.codec_status == pae::CodecStatus::OK);
  assert(AsciiCodecStatusName(result.codec_status) == "OK");
  assert(result.message_index == 0U);
  assert(result.message_id == "greeting");
  assert(result.frame == frame);
  assert(result.fields.size() == 2U);
  assert(result.fields[0].field_id == "name");
  assert(result.fields[0].bytes == std::vector<std::uint8_t>({'A', 'L', 'I', 'C', 'E'}));
  assert(result.fields[1].field_id == "rx_code");
  assert(result.fields[1].bytes == std::vector<std::uint8_t>({'O', 'K'}));
  return 0;
}
