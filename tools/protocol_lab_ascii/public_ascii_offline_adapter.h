#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pae/codec.h"
#include "pae/compiler.h"

namespace pae::protocol_lab_ascii::public_offline {

// A1 非 Qt 离线边界：接收一次公开编译结果，向上游提供自有描述与完整记录 RX/TX 结果。
// 不读取 JSON、不管理通信或线程，也不承担 stream 切帧；Host 组合层复用这里的 owner/DTO。
// 预算是描述、发布结果及实例替换的逻辑计费门禁，不是进程 RSS 硬上限。
struct Limits {
  std::size_t max_frame_bytes = 65536U;
  std::size_t max_fields = 1024U;
  std::size_t max_field_bytes = 1024U * 1024U;
  std::size_t max_description_bytes = 4U * 1024U * 1024U;
  std::size_t max_result_bytes = 8U * 1024U * 1024U;
  std::size_t instance_bytes = 128U * 1024U * 1024U;
  std::size_t replacement_bytes = 256U * 1024U * 1024U;
};

enum class LocalStatus {
  OK,
  INVALID_INPUT,
  RESOURCE_LIMIT,
  PREPARATION_FAILED,
  CODEC_FAILED,
  MATERIALIZATION_FAILED,
  ALLOCATION_FAILED,
};

struct OwnedSegment {
  AsciiSegmentKind kind = AsciiSegmentKind::LITERAL;
  std::vector<std::uint8_t> literal;
  std::optional<std::size_t> field_index;
  std::optional<std::size_t> flat_field_index;
};

struct OwnedAction {
  ByteLengthBounds record_length;
  std::vector<OwnedSegment> segments;
};

struct OwnedFieldDescription {
  std::size_t flat_index = 0U;
  std::size_t field_index = 0U;
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  ByteLengthBounds byte_length;
  bool decode_referenced = false;
  bool encode_referenced = false;
  // Missing means the public description does not expose this constraint; it is not an empty
  // allow-list and never narrows the Codec's accepted domain.
  // nullopt 表示公共 metadata 未暴露该约束，不能解释为空白名单或另加字段过滤。
  std::optional<std::vector<std::uint8_t>> allowed_control_bytes;
};

struct OwnedMessageDescription {
  std::size_t index = 0U;
  std::string id;
  std::string direction_id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::optional<OwnedAction> decode;
  std::optional<OwnedAction> encode;
  std::vector<OwnedFieldDescription> fields;
};

struct OwnedPipelineDescription {
  std::size_t index = 0U;
  std::string id;
  std::string direction_id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::vector<std::size_t> message_indices;
  std::vector<std::size_t> decode_message_indices;
  std::vector<std::size_t> encode_message_indices;
};

// 字符串、literal 和索引集合已复制为自有数据；Description() 返回的引用仍依赖 Adapter 寿命。
struct OwnedDescription {
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::vector<OwnedPipelineDescription> pipelines;
  std::vector<OwnedMessageDescription> messages;
};

// 调用方已选择表示并准备好的原始字段字节；这里不解析转义文本或反算业务值。
struct InputField {
  std::size_t field_index = 0U;
  std::vector<std::uint8_t> bytes;
};

struct OwnedFieldValue {
  std::size_t flat_index = 0U;
  std::size_t field_index = 0U;
  std::string id;
  std::vector<std::uint8_t> bytes;
  ByteRange range;
};

// 值对象拥有 frame/字段字节，可在后续调用或 Adapter 销毁后继续使用。
// local_status 与 codec_called/codec_status 分层：执行成功不保证预算与复制发布成功。
// Decode 失败的 diagnostic_input_frame 仅供诊断，不是可交付的成功 frame。
struct OperationResult {
  LocalStatus local_status = LocalStatus::INVALID_INPUT;
  bool codec_called = false;
  CodecStatus codec_status = CodecStatus::INVALID_ARGUMENT;
  std::optional<std::size_t> matched_message_index;
  std::optional<std::size_t> message_index;
  std::optional<std::string> message_id;
  std::optional<std::size_t> failed_value_index;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  bool output_tainted = false;
  std::vector<std::uint8_t> frame;
  std::vector<OwnedFieldValue> fields;
  std::vector<std::uint8_t> diagnostic_input_frame;
  std::size_t accounted_bytes = 0U;
};

class Adapter;
class HostAdapter;

struct PrepareResult {
  LocalStatus status = LocalStatus::PREPARATION_FAILED;
  CodecStatus codec_status = CodecStatus::INVALID_ARGUMENT;
  std::unique_ptr<Adapter> adapter;
};

// Public-only, complete-record ASCII adapter. It adopts exactly one already compiled owner and
// never reparses Schema or calls Decode/Encode more than once for one operation.
// 外部 CompiledProtocol 移入一次；描述在准备阶段复制，执行时只消费已有冻结计划。
// 调用、描述读取、测试钩子及销毁由宿主串行协调；返回的独立 DTO 不共享执行 Workspace。
class Adapter final {
 public:
  static PrepareResult AdoptCompiled(CompiledProtocol compiled, const Limits& limits = {},
                                     std::size_t previous_instance_bytes = 0U) noexcept;

  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;
  Adapter(Adapter&&) = delete;
  Adapter& operator=(Adapter&&) = delete;
  ~Adapter();

  [[nodiscard]] const OwnedDescription& Description() const noexcept { return description_; }
  [[nodiscard]] std::size_t DescriptionAccountedBytes() const noexcept {
    return description_accounted_bytes_;
  }
  [[nodiscard]] std::size_t InstanceAdmissionBytes() const noexcept { return instance_bytes_; }
  // Internal A2 composition accessors. They expose the same immutable owner and limits without
  // transferring ownership or creating a second compiler/codec execution path.
  // 两个引用均借用本 Adapter；Host 可据此创建独立执行对象，但不能保留过期 metadata view。
  [[nodiscard]] const CompiledProtocol& CompiledForHost() const noexcept { return compiled_; }
  [[nodiscard]] const Limits& LimitsForHost() const noexcept { return limits_; }

  // RX 输入同步借用一个完整记录（含配置要求的终止字节）；复制结束后不再依赖输入 Buffer。
  [[nodiscard]] OperationResult Decode(std::size_t pipeline_index, ByteView frame) noexcept;
  // TX 依据 Encode segments 投影范围，与 RX 规则独立；不会为了展示再对 TX 做 Decode。
  [[nodiscard]] OperationResult Encode(std::size_t pipeline_index, std::size_t message_index,
                                       const std::vector<InputField>& inputs) noexcept;

#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  struct PipelineCapacityObservation {
    std::size_t message_indices = 0U;
    std::size_t decode_message_indices = 0U;
    std::size_t encode_message_indices = 0U;
  };
  [[nodiscard]] std::optional<PipelineCapacityObservation> PipelineCapacitiesForTesting(
      std::size_t pipeline_index) const noexcept;
  void FailNextMaterializationForTesting() noexcept { fail_next_materialization_ = true; }
  void FailNextAllocationForTesting() noexcept { fail_next_allocation_ = true; }
#endif

 private:
  friend class HostAdapter;
  Adapter(CompiledProtocol compiled, std::unique_ptr<CompleteRecordCodec> codec,
          OwnedDescription description, const Limits& limits,
          std::size_t description_accounted_bytes, std::size_t instance_bytes) noexcept;

  [[nodiscard]] const OwnedMessageDescription* Message(std::size_t index) const noexcept;
  [[nodiscard]] bool ConsumeMaterializationFailureHook() noexcept;
  void AllocationCheckpoint();

  // Destruction is reverse: Codec precedes the metadata owner; execution retains frozen state.
  // codec_ 自行保留冻结状态，并非必须借用 compiled_ 的地址/寿命；此成员顺序仍先销毁 Codec。
  // 外部查询的 metadata view 与本对象引用仍有各自借用寿命，不因执行状态保留而延长。
  CompiledProtocol compiled_;
  std::unique_ptr<CompleteRecordCodec> codec_;
  OwnedDescription description_;
  Limits limits_;
  std::size_t description_accounted_bytes_ = 0U;
  std::size_t instance_bytes_ = 0U;
  bool fail_next_materialization_ = false;
  bool fail_next_allocation_ = false;
};

}  // namespace pae::protocol_lab_ascii::public_offline
