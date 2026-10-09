#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace pae {

// 业务值类型，不是报文上的字节宽度；具体物理布局需通过 Physical 查询取得。
enum class ValueKind { UINT64, INT64, BOOL, BYTES, ENUM, DECIMAL64 };

// Encode 的取值来源；常量来源不自动成为 Decode 的业务值约束。
enum class EncodeValueSource { CALLER_INPUT, CONSTANT, COMPUTED, NOT_REFERENCED };

// Encode 容量单位为字节；UPPER_BOUND 是容量上界，不是本次输出的有效长度。
enum class EncodeOutputSizeKind { NOT_AVAILABLE, EXACT, UPPER_BOUND };

enum class RecordRepresentation { BINARY, ASCII_TEXT };

enum class PhysicalQueryStatus {
  OK,
  INVALID_COMPILED_PROTOCOL,
  INDEX_OUT_OF_RANGE,
  FRAME_SIZE_MISMATCH,
  REPRESENTATION_NOT_SUPPORTED,
  INTERNAL_CONTRACT_VIOLATION,
};

enum class FieldPhysicalKind { BYTE_RANGE, BIT_MASKS };

enum class AsciiAction { DECODE, ENCODE };
enum class AsciiSegmentKind { LITERAL, FIELD };
enum class AsciiQueryStatus {
  OK,
  INVALID_COMPILED_PROTOCOL,
  INDEX_OUT_OF_RANGE,
  INVALID_SELECTOR,
  REPRESENTATION_NOT_SUPPORTED,
  ACTION_NOT_AVAILABLE,
  INTERNAL_CONTRACT_VIOLATION,
};

// offset 与 length 均以字节计，offset 相对于完整记录起点。
struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};

struct ByteLengthBounds {
  std::size_t minimum = 0U;
  std::size_t maximum = 0U;
};

// Literal bytes borrow the CompiledProtocol's frozen state. Copy them before the owner is moved,
// replaced, or destroyed. The explicit size preserves embedded NUL bytes.
// 借用外部 CompiledProtocol 的数据；owner 移动后需重新查询，长期保存需复制字节。
// size 是实际字节数，不能使用 strlen()，因为字面量可包含 NUL。
struct AsciiLiteralView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

// 描述 Message 的 ASCII 模板；不代表某 Pipeline 允许该方向，也不代表已匹配输入。
struct AsciiActionDescription {
  std::size_t message_index = 0U;
  AsciiAction action = AsciiAction::DECODE;
  ByteLengthBounds record_length;
  std::size_t segment_count = 0U;
};

// ordinal 是模板内段序号。LITERAL 仅设置 literal；FIELD 设置字段索引而不设置 literal。
// field_index 是 Message 内索引，flat_field_index 是全局字段索引；复制结构仍会借用 literal。
struct AsciiSegmentDescription {
  std::size_t message_index = 0U;
  AsciiAction action = AsciiAction::DECODE;
  std::size_t ordinal = 0U;
  AsciiSegmentKind kind = AsciiSegmentKind::LITERAL;
  std::optional<AsciiLiteralView> literal;
  std::optional<std::size_t> field_index;
  std::optional<std::size_t> flat_field_index;
};

// referenced 表示模板是否引用字段，不替代 Pipeline 的 Decode/Encode 权限检查。
struct AsciiFieldDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  ByteLengthBounds byte_length;
  bool decode_referenced = false;
  bool encode_referenced = false;
};

// 查询结果须先检查 status == OK 及 value；默认枚举或索引值不是成功证据。
struct AsciiActionQueryResult {
  AsciiQueryStatus status = AsciiQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<AsciiActionDescription> value;
};

struct AsciiSegmentQueryResult {
  AsciiQueryStatus status = AsciiQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<AsciiSegmentDescription> value;
};

struct AsciiFieldQueryResult {
  AsciiQueryStatus status = AsciiQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<AsciiFieldDescription> value;
};

// frame_byte_index 是完整记录内的绝对字节偏移；mask 指出该字节中属于字段的位。
struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t mask = 0U;
};

inline constexpr std::size_t kMaximumFieldPhysicalBitMasks = 8U;

// Physical descriptions are copied values from the immutable compiled Plan. They do not prove
// that any frame matched, passed integrity checks, or decoded successfully. Indices are meaningful
// only for the CompiledProtocol that produced them.
// 物理描述是自有数值副本，不借用字符串；索引仍只适用于产生它的 compiled 实例。
// maximum_integrity_storage 是最大合法帧长对应的校验存储位置，不是校验计算值。
// computed_length_storage 同样描述存储位置，而不是计算出的长度。
struct MessagePhysicalDescription {
  std::size_t message_index = 0U;
  ByteLengthBounds record_length;
  std::optional<ByteRange> maximum_integrity_storage;
  bool integrity_storage_offset_depends_on_frame_size = false;
  std::optional<ByteRange> computed_length_storage;
};

// 按调用方给定的 frame_size 解析位置；查询本身不读取 Frame，也不验证完整性或 Matcher。
struct ResolvedMessagePhysicalDescription {
  std::size_t message_index = 0U;
  std::size_t frame_size = 0U;
  std::optional<ByteRange> integrity_storage;
  std::optional<ByteRange> computed_length_storage;
};

// BYTE_RANGE 使用 maximum_byte_range；BIT_MASKS 仅使用数组前 bit_mask_count 项。
// 位成员不应被解释成连续整字节所有权。可变 payload 的实际范围需按帧长 Resolve。
// flat_field_index / message_index 为全局索引，field_index 为 Message 内索引。
struct FieldPhysicalDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  FieldPhysicalKind physical_kind = FieldPhysicalKind::BYTE_RANGE;
  std::optional<ByteRange> maximum_byte_range;
  std::optional<ByteLengthBounds> byte_value_length;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
  bool byte_range_length_depends_on_frame_size = false;
};

// byte_range 是指定帧长下的范围；位掩码仍仅以前 bit_mask_count 项为有效数据。
struct ResolvedFieldPhysicalDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  FieldPhysicalKind physical_kind = FieldPhysicalKind::BYTE_RANGE;
  std::optional<ByteRange> byte_range;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
};

struct MessageRepresentationQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<RecordRepresentation> value;
};

struct MessagePhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<MessagePhysicalDescription> value;
};

struct ResolvedMessagePhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<ResolvedMessagePhysicalDescription> value;
};

struct FieldPhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<FieldPhysicalDescription> value;
};

struct ResolvedFieldPhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<ResolvedFieldPhysicalDescription> value;
};

// 下列 metadata 的 string_view 借用外部 CompiledProtocol；复制描述不会取得字符串所有权。
// owner 移动、替换或销毁前应复制需保留的文本，移动后重新查询。
struct ProtocolDescription {
  std::string_view schema_version;
  std::string_view id;
  std::string_view version;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
};

// index 是全局 Pipeline 索引；direction_id 是逻辑方向标识，不是网络端点或路由。
// message_count 是关联数量，关联序号需经 PipelineMessageIndex() 映射到全局 Message。
struct PipelineDescription {
  std::size_t index = 0U;
  std::string_view id;
  std::string_view direction_id;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
  std::size_t message_count = 0U;
};

// index 为全局 Message 索引；字段位于全局字段表 [field_begin, field_begin + field_count)。
// 字面量专用 Message 可以没有业务字段，field_count == 0 不表示无效。
struct MessageDescription {
  std::size_t index = 0U;
  std::string_view id;
  std::string_view direction_id;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
  std::size_t field_begin = 0U;
  std::size_t field_count = 0U;
};

// flat_index 为全局字段索引，index_in_message 为消息内索引。
// enum_begin / enum_count 指定全局枚举表中的区间，不是该字段的原始数值范围。
struct FieldDescription {
  std::size_t flat_index = 0U;
  std::size_t message_index = 0U;
  std::size_t index_in_message = 0U;
  std::string_view id;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
  ValueKind value_kind = ValueKind::UINT64;
  EncodeValueSource encode_value_source = EncodeValueSource::NOT_REFERENCED;
  std::size_t enum_begin = 0U;
  std::size_t enum_count = 0U;
};

// 特定 Pipeline 与全局 Message 关联的执行能力；available 不保证任意输入执行成功。
// encode_output_size 仅在相应 size_kind 下解释，最终有效长度以 Encode 结果为准。
struct MessageExecutionDescription {
  std::size_t pipeline_index = 0U;
  std::size_t message_index = 0U;
  bool decode_available = false;
  bool encode_available = false;
  EncodeOutputSizeKind encode_output_size_kind = EncodeOutputSizeKind::NOT_AVAILABLE;
  std::size_t encode_output_size = 0U;
};

// flat_index 是全局枚举索引；index_in_field 是字段内枚举序号，可用于 EnumSelector。
// raw_value 为数值副本，id/display_name 仍遵循 metadata 字符串的借用寿命。
struct EnumDescription {
  std::size_t flat_index = 0U;
  std::size_t field_flat_index = 0U;
  std::size_t index_in_field = 0U;
  std::string_view id;
  std::string_view display_name;
  std::uint64_t raw_value = 0U;
};

// 编译产物的逻辑计费及创建分配数；不包含执行 Workspace，也不是进程 RSS 硬上限。
struct CompileMemoryReport {
  std::size_t plan_accounted_bytes = 0U;
  std::size_t metadata_accounted_bytes = 0U;
  std::size_t metadata_allocation_count = 0U;
  std::size_t facade_allocation_bytes = 0U;
};

}  // namespace pae
