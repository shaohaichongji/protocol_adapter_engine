// 最小公开 API 消费：读人工配置 -> 编译 -> 查询索引/权限 -> 创建 Codec -> Decode -> Encode。
// 仅验证指定 Schema 0.9 样例的完整记录；不调用 StreamFramer，也不打开网络或真实设备。
#include <pae/codec.h>
#include <pae/compiler.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace {

// 以原始字节读取配置，失败交给 Run 处理；示例不实现生产级文件大小限制或异常恢复。
std::optional<std::string> ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return std::nullopt;
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

template <std::size_t Size>
std::string Hex(const std::array<std::uint8_t, Size>& bytes) {
  std::ostringstream output;
  output << std::uppercase << std::hex << std::setfill('0');
  for (std::size_t index = 0U; index < bytes.size(); ++index) {
    if (index != 0U) output << ' ';
    output << std::setw(2) << static_cast<unsigned int>(bytes[index]);
  }
  return output.str();
}

// 参数必须指向 synthetic_stream_framing_slice 的公开人工配置，而不是任意协议配置。
bool Run(const char* config_path) {
  const auto config = ReadFile(config_path);
  if (!config.has_value()) {
    std::cerr << "cannot read config: " << config_path << '\n';
    return false;
  }

  auto compile_result = pae::CompileProtocolJson(*config);
  if (!compile_result.Succeeded()) {
    const auto* diagnostic = compile_result.Diagnostic();
    std::cerr << "compile failed";
    if (diagnostic != nullptr) std::cerr << " at " << diagnostic->json_pointer;
    std::cerr << '\n';
    return false;
  }

  // Description string_views borrow from compiled, so use them only while compiled remains alive.
  // 提取 owner 后才查询借用 metadata；第 0 个 Pipeline/关联是本样例假设，不是通用选择算法。
  pae::CompiledProtocol compiled = std::move(compile_result).TakeCompiled();
  const auto protocol = compiled.Protocol();
  const auto pipeline = compiled.Pipeline(0U);
  const auto message_index = compiled.PipelineMessageIndex(0U, 0U);
  if (!protocol.has_value() || !pipeline.has_value() || !message_index.has_value()) return false;

  const auto message = compiled.Message(*message_index);
  if (!message.has_value() || message->field_count != 1U) return false;
  // field_begin 是全局字段起点，index_in_message 才是后续 Encode 的消息内选择位置。
  const auto field = compiled.Field(message->field_begin);
  const auto execution = compiled.PipelineMessageExecution(0U, *message_index);
  if (!field.has_value() || !execution.has_value() || protocol->schema_version != "0.9" ||
      protocol->id != "synthetic_stream_framing" || pipeline->id != "fixed_rx" ||
      message->id != "fixed_message" || field->id != "fixed_payload" ||
      field->value_kind != pae::ValueKind::UINT64 || !execution->decode_available ||
      !execution->encode_available ||
      execution->encode_output_size_kind != pae::EncodeOutputSizeKind::EXACT ||
      execution->encode_output_size != 3U) {
    return false;
  }

  // Codec 保留冻结状态，metadata 仍借用外部 compiled；这里两者都保留到 Run 返回。
  auto create_result = pae::CreateCompleteRecordCodec(compiled);
  if (create_result.status != pae::CodecStatus::OK || !create_result.codec) return false;

  // 独立期望：固定 AA + 两字节大端整数 7；不靠被测 Encode 生成 Decode 输入。
  constexpr std::array<std::uint8_t, 3U> kExpectedFrame{{0xAAU, 0x00U, 0x07U}};
  const auto decoded =
      create_result.codec->Decode(0U, {kExpectedFrame.data(), kExpectedFrame.size()});
  const auto decoded_field = decoded.record.Field(0U);
  if (decoded.status != pae::CodecStatus::OK || decoded.matched_message_index != message_index ||
      !decoded_field.has_value() || decoded_field->UInt64() != 7U) {
    return false;
  }

  // Decode views borrow from the Codec (and BYTES would also borrow the input). Read them before
  // the next guarded Decode/Encode call invalidates them.
  // 在 Encode 前复制数值，避免使用已失效的 Decode 视图；复制 BYTES 视图则还不够。
  const std::uint64_t decoded_value = *decoded_field->UInt64();
  const std::array values{
      pae::EncodeValue::UInt64({*message_index, field->index_in_message}, decoded_value)};
  std::array<std::uint8_t, 3U> encoded_frame{};
  const auto encoded = create_result.codec->Encode(0U, *message_index, values.data(), values.size(),
                                                   {encoded_frame.data(), encoded_frame.size()});
  // 先检查状态与有效长度再比较字节；失败 Buffer 即使已写入也不构成交付。
  if (encoded.status != pae::CodecStatus::OK || encoded.bytes_written != encoded_frame.size() ||
      encoded_frame != kExpectedFrame) {
    return false;
  }

  std::cout << "CONFIG schema=" << protocol->schema_version << " protocol=" << protocol->id << '\n';
  std::cout << "PIPELINE id=" << pipeline->id << " message=" << message->id << '\n';
  std::cout << "DECODE frame=" << Hex(kExpectedFrame) << " field=" << field->id
            << " value=" << decoded_value << '\n';
  std::cout << "ENCODE value=" << decoded_value << " frame=" << Hex(encoded_frame) << '\n';
  std::cout << "GETTING_STARTED_BINARY_PASS\n";
  return true;
}

}  // namespace

// 退出 2 表示参数不符，1 表示样例检查失败，0 仅表示此人工向量通过。
int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: pae_getting_started <synthetic_stream_framing_slice.pae.json>\n";
    return 2;
  }
  return Run(argv[1]) ? 0 : 1;
}
