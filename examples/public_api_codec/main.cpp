// 两个独立人工配置的公开 Codec 消费示例：Binary 编码后解析、ASCII 独立发送模板编码。
// 不做传输或切帧，不覆盖生产输入限制、完整错误诊断或真实协议验收。
#include <pae/codec.h>
#include <pae/compiler.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

// 简化读取：文件打不开时返回空串，随后表现为编译失败，不单独区分 I/O 错误。
std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

// 在本 Message 的字段区间按 id 查询；返回描述中的字符串仍借用 compiled。
std::optional<pae::FieldDescription> FindField(const pae::CompiledProtocol& compiled,
                                               std::size_t message_index,
                                               std::string_view id) noexcept {
  const auto message = compiled.Message(message_index);
  if (!message.has_value()) return std::nullopt;
  for (std::size_t ordinal = 0U; ordinal < message->field_count; ++ordinal) {
    const auto field = compiled.Field(message->field_begin + ordinal);
    if (field.has_value() && field->id == id) return field;
  }
  return std::nullopt;
}

// 枚举条目从该字段的全局区间查询，Encode 使用返回的字段内 entry 索引。
std::optional<pae::EnumDescription> FindEnum(const pae::CompiledProtocol& compiled,
                                             const pae::FieldDescription& field,
                                             std::string_view id) noexcept {
  for (std::size_t ordinal = 0U; ordinal < field.enum_count; ++ordinal) {
    const auto item = compiled.Enum(field.enum_begin + ordinal);
    if (item.has_value() && item->id == id) return item;
  }
  return std::nullopt;
}

// 对应 synthetic_lab_exchange_slice；第 0 个 Pipeline/关联及字段 id 均是样例约定。
bool RunBinary(const char* config_path) {
  auto compiled_result = pae::CompileProtocolJson(ReadFile(config_path));
  if (!compiled_result.Succeeded()) return false;
  pae::CompiledProtocol compiled = std::move(compiled_result).TakeCompiled();
  const auto message_index = compiled.PipelineMessageIndex(0U, 0U);
  if (!message_index.has_value()) return false;
  const auto action = compiled.PipelineMessageExecution(0U, *message_index);
  const auto length = FindField(compiled, *message_index, "record_length");
  const auto transaction = FindField(compiled, *message_index, "transaction_id");
  const auto payload_field = FindField(compiled, *message_index, "payload_tag");
  const auto mode = FindField(compiled, *message_index, "operating_mode");
  if (!action.has_value() || !action->decode_available || !action->encode_available ||
      action->encode_output_size_kind != pae::EncodeOutputSizeKind::EXACT || !length.has_value() ||
      length->value_kind != pae::ValueKind::UINT64 ||
      length->encode_value_source != pae::EncodeValueSource::CALLER_INPUT ||
      !transaction.has_value() || !payload_field.has_value() || !mode.has_value()) {
    return false;
  }
  const auto active = FindEnum(compiled, *mode, "mode_active");
  if (!active.has_value()) return false;
  auto created = pae::CreateCompleteRecordCodec(compiled);
  if (created.status != pae::CodecStatus::OK || !created.codec) return false;

  // record_length=13 是普通动态值，不是自动回填；operation_code 常量由 Plan 写入，故不传。
  // Bytes 只借用 payload，其寿命覆盖同步 Encode；数值与枚举选择按值保存在 values 中。
  const std::array<std::uint8_t, 4> payload{0x11U, 0x22U, 0x33U, 0x44U};
  const std::array values{
      pae::EncodeValue::UInt64({*message_index, length->index_in_message}, 13U),
      pae::EncodeValue::UInt64({*message_index, transaction->index_in_message}, 0x030201U),
      pae::EncodeValue::Bytes({*message_index, payload_field->index_in_message},
                              {payload.data(), payload.size()}),
      pae::EncodeValue::Enum({*message_index, mode->index_in_message, active->index_in_field}),
  };
  // 此方向为 EXACT 尺寸。frame 为调用方自有输出，仅成功后按 bytes_written 读取。
  std::vector<std::uint8_t> frame(action->encode_output_size);
  const auto encoded = created.codec->Encode(0U, *message_index, values.data(), values.size(),
                                             {frame.data(), frame.size()});
  if (encoded.status != pae::CodecStatus::OK) return false;
  // 这里只证明编码结果能解析且有 5 个字段，不宣称独立逐字段期望或全部负例覆盖。
  const auto decoded = created.codec->Decode(0U, {frame.data(), encoded.bytes_written});
  return decoded.status == pae::CodecStatus::OK && decoded.record.FieldCount() == 5U;
}

// 对应 synthetic_ascii_text_slice；RX 与 TX 模板不同，不能把 TX 输出当作 RX 输入往返。
bool RunAscii(const char* config_path) {
  auto compiled_result = pae::CompileProtocolJson(ReadFile(config_path));
  if (!compiled_result.Succeeded()) return false;
  pae::CompiledProtocol compiled = std::move(compiled_result).TakeCompiled();
  const auto message_index = compiled.PipelineMessageIndex(0U, 0U);
  if (!message_index.has_value()) return false;
  const auto action = compiled.PipelineMessageExecution(0U, *message_index);
  const auto name_field = FindField(compiled, *message_index, "name");
  const auto tag_field = FindField(compiled, *message_index, "tx_tag");
  if (!action.has_value() || !action->encode_available || action->encode_output_size == 0U ||
      !name_field.has_value() || name_field->value_kind != pae::ValueKind::BYTES ||
      name_field->encode_value_source != pae::EncodeValueSource::CALLER_INPUT ||
      !tag_field.has_value() ||
      tag_field->encode_value_source != pae::EncodeValueSource::CALLER_INPUT) {
    return false;
  }
  auto created = pae::CreateCompleteRecordCodec(compiled);
  if (created.status != pae::CodecStatus::OK || !created.codec) return false;

  // name/tx_tag 是发送方向输入；rx_code 只在接收模板中使用，不应加入本次 Values。
  const std::string name = "ALICE";
  const std::string tag = "Z";
  const std::array values{
      pae::EncodeValue::Bytes({*message_index, name_field->index_in_message},
                              {reinterpret_cast<const std::uint8_t*>(name.data()), name.size()}),
      pae::EncodeValue::Bytes({*message_index, tag_field->index_in_message},
                              {reinterpret_cast<const std::uint8_t*>(tag.data()), tag.size()}),
  };
  std::vector<std::uint8_t> frame(action->encode_output_size);
  const auto encoded = created.codec->Encode(0U, *message_index, values.data(), values.size(),
                                             {frame.data(), frame.size()});
  if (encoded.status != pae::CodecStatus::OK) return false;
  // 模板上界用于分配容量，实际长度看 bytes_written；独立期望包含 CRLF，不发送网络。
  const std::string_view expected = "TX ALICE!Z\r\n";
  return std::string_view(reinterpret_cast<const char*>(frame.data()), encoded.bytes_written) ==
         expected;
}

}  // namespace

// 依次接收 Binary/ASCII 配置路径；PASS 只表示两个样例检查通过，不是通用协议认证。
int main(int argc, char** argv) {
  if (argc != 3 || !RunBinary(argv[1]) || !RunAscii(argv[2])) {
    std::cerr << "PUBLIC_CODEC_EXAMPLE gate=FAIL\n";
    return 1;
  }
  std::cout << "PUBLIC_CODEC_EXAMPLE gate=PASS\n";
  return 0;
}
