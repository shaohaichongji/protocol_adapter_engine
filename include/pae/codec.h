#pragma once

// 完整记录的同步编解码 facade：消费冻结配置，内部持有独立 Workspace。
// 调用方提供完整 Frame/Values/Buffer；本接口不做切帧、收发或业务路由。

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "pae/export.h"
#include "pae/protocol_description.h"

namespace pae {

class CompiledProtocol;
class HostEndpoint;

// 检查 status 再消费结果；候选匹配、完整性、类型及容量失败是不同层次，不等同未知消息。
enum class CodecStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_COMPILED_PROTOCOL,
  RESOURCE_LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  WORKSPACE_BUSY,
  INPUT_VALUES_TOO_MANY,
  UNKNOWN_MESSAGE,
  AMBIGUOUS_MESSAGE,
  OUTPUT_SLOTS_TOO_SMALL,
  INTEGRITY_FAILED,
  MESSAGE_NOT_ALLOWED,
  FIELD_REFERENCE_MISMATCH,
  DUPLICATE_FIELD,
  MISSING_FIELD,
  TYPE_MISMATCH,
  VALUE_NOT_REPRESENTABLE,
  BYTES_LENGTH_MISMATCH,
  UNKNOWN_ENUM_VALUE,
  ENUM_REFERENCE_MISMATCH,
  CONSTANT_FIELD_OVERRIDE,
  INPUT_OUTPUT_OVERLAP,
  BUFFER_TOO_SMALL,
  FINAL_REVIEW_FAILED,
  ASCII_CHARACTER_NOT_ALLOWED,
  ASCII_TERMINATOR_CONFLICT,
  OPERATION_NOT_SUPPORTED,
  COMPUTED_FIELD_OVERRIDE,
  LENGTH_MISMATCH,
  INTERNAL_ERROR,
};

enum class ConversionError {
  NONE,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
};

enum class RawIntegerKind { UINT64, INT64 };

// 非拥有的连续字节视图；调用方负责数据寿命，size 单位为字节，不以 NUL 结尾。
struct ByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

// 调用方拥有的可写区域，capacity 为字节容量而不是本次有效输出长度。
struct MutableByteBuffer {
  std::uint8_t* data = nullptr;
  std::size_t capacity = 0U;
};

// 精确十进制逻辑值 coefficient * 10^(-scale)，不是 double；支持尺度 0..18。
// 是否能编码仍由字段转换及 Wire 范围决定，不因逻辑值可表示就保证能精确落到原始整数。
struct Decimal64 {
  std::int64_t coefficient = 0;
  std::int32_t scale = 0;
};

// 同一编译配置内的零基位置：全局 Message 索引 + 消息内字段索引（不是 FlatFieldIndex）。
// 默认无效哨兵不指向字段；不能把另一配置的索引当作当前配置的稳定业务标识。
struct FieldSelector {
  std::size_t message_index = static_cast<std::size_t>(-1);
  std::size_t field_index = static_cast<std::size_t>(-1);
};

// entry_index 是该字段内的枚举条目序号，不是原始数值或全局枚举索引。
struct EnumSelector {
  std::size_t message_index = static_cast<std::size_t>(-1);
  std::size_t field_index = static_cast<std::size_t>(-1);
  std::size_t entry_index = static_cast<std::size_t>(-1);
};

// 工厂构造带类型标签的输入，实际合法性在 Encode 时检查；没有隐式有符号/无符号转换。
// 数值按值保存，Bytes 仅保存借用视图，输入字节必须保持有效且不变直至 Encode 返回。
class PAE_API EncodeValue final {
 public:
  static EncodeValue UInt64(FieldSelector field, std::uint64_t value) noexcept;
  static EncodeValue Int64(FieldSelector field, std::int64_t value) noexcept;
  static EncodeValue Bool(FieldSelector field, bool value) noexcept;
  static EncodeValue Bytes(FieldSelector field, ByteView value) noexcept;
  static EncodeValue Enum(EnumSelector value) noexcept;
  static EncodeValue Decimal(FieldSelector field, Decimal64 value) noexcept;

 private:
  EncodeValue() noexcept = default;

  FieldSelector field_;
  EnumSelector enum_;
  ValueKind kind_ = ValueKind::UINT64;
  std::uint64_t uint64_value_ = 0U;
  std::int64_t int64_value_ = 0;
  bool bool_value_ = false;
  ByteView bytes_value_;
  Decimal64 decimal64_value_;

  friend class CompleteRecordCodec;
};

inline constexpr std::size_t kUsePlanDecodedFieldCapacity = 0U;
inline constexpr std::size_t kUseDefaultExecutionMemoryLimit = static_cast<std::size_t>(-1);

// 创建时确定容量，不在每帧扩容。字段容量 0 采用 Plan 最大需求，显式容量不能超过该最大值；
// 较小容量可创建成功，但 Decode 对较大记录返回 OUTPUT_SLOTS_TOO_SMALL。
struct CompleteRecordCodecOptions {
  std::size_t decoded_field_capacity = kUsePlanDecodedFieldCapacity;
  // 执行对象的逻辑字节预算，默认取 size_t 最大值；不包含共享 Plan 的重复计费。
  std::size_t execution_memory_limit_bytes = kUseDefaultExecutionMemoryLimit;
};

// 创建时的逻辑计费，字节字段不等于 RSS；allocation_count 是报告的创建分配数，不是吞吐指标。
struct ExecutionMemoryReport {
  // 分别为 Core 工作区、结果槽、Encode 映射及 facade；total 为这四类之和。
  std::size_t core_workspace_bytes = 0U;
  std::size_t decoded_slot_bytes = 0U;
  std::size_t encode_mapping_bytes = 0U;
  std::size_t facade_bytes = 0U;
  std::size_t codec_accounted_total_bytes = 0U;
  // 保留的共享编译状态 facade 单列，不能据此认为整个 Plan 又按每个 Codec 复制计费。
  std::size_t retained_compiled_state_facade_bytes = 0U;
  std::size_t allocation_count = 0U;
};

class CompleteRecordCodec;
struct CompleteRecordCodecCreateResult;

// 借用 Codec 最近一次成功发布的字段槽，不拥有结果。先检查 HasValue，再读取类型对应的
// optional；类型不匹配返回空。Kind 在无效视图上的默认值不是字段类型证据。
// owner 销毁后连 HasValue 也不得调用：它不是可探测悬空指针的共享所有权句柄。
class PAE_API DecodedFieldView final {
 public:
  DecodedFieldView() noexcept = default;

  [[nodiscard]] bool HasValue() const noexcept;
  [[nodiscard]] std::size_t FlatFieldIndex() const noexcept;
  [[nodiscard]] FieldSelector Field() const noexcept;
  [[nodiscard]] ValueKind Kind() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> UInt64() const noexcept;
  [[nodiscard]] std::optional<std::int64_t> Int64() const noexcept;
  [[nodiscard]] std::optional<bool> Bool() const noexcept;
  // On a successful ASCII Decode, BYTES are a contiguous slice of this call's input. For a
  // nonempty input, even a zero-length field points at its real offset (possibly one-past-end).
  // The view expires with this Codec result and additionally borrows the unchanged input bytes.
  // BYTES 还借用本次 Decode 输入；复制 ByteView 不复制字节，跨调用保存需复制底层数据。
  [[nodiscard]] std::optional<ByteView> Bytes() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> EnumRawValue() const noexcept;
  // 未知枚举允许保留时，原始值可用而 KnownEnumFlatIndex 为空；不要强行映射已知条目。
  [[nodiscard]] std::optional<std::size_t> KnownEnumFlatIndex() const noexcept;
  [[nodiscard]] std::optional<Decimal64> Decimal() const noexcept;
  // 返回当前 Decode 留存的转换前原始整数，不从 Decimal 反算；无相应记录时为空。
  [[nodiscard]] std::optional<RawIntegerKind> ConversionRawKind() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> ConversionRawUInt64() const noexcept;
  [[nodiscard]] std::optional<std::int64_t> ConversionRawInt64() const noexcept;

 private:
  DecodedFieldView(const CompleteRecordCodec* owner, std::uint64_t owner_epoch,
                   std::uint64_t generation, std::size_t ordinal) noexcept;

  const CompleteRecordCodec* owner_ = nullptr;
  // owner 移动后的失效检查，不延长其寿命。
  std::uint64_t owner_epoch_ = 0U;
  // 已发布调用的代次，下一次受保护调用使旧代失效。
  std::uint64_t generation_ = 0U;
  // 本次交付中的字段序号，不是全局字段索引。
  std::size_t ordinal_ = 0U;

  friend class DecodedRecordView;
};

// 成功 Decode 的借用记录，可再查询借用字段；自身复制不冻结结果，寿命规则同字段视图。
class PAE_API DecodedRecordView final {
 public:
  DecodedRecordView() noexcept = default;

  [[nodiscard]] bool HasValue() const noexcept;
  [[nodiscard]] std::size_t MessageIndex() const noexcept;
  [[nodiscard]] std::size_t FieldCount() const noexcept;
  // ordinal 是交付序号；视图无效或越界返回空，不交付失败前部分解析的字段。
  [[nodiscard]] std::optional<DecodedFieldView> Field(std::size_t ordinal) const noexcept;

 private:
  DecodedRecordView(const CompleteRecordCodec* owner, std::uint64_t owner_epoch,
                    std::uint64_t generation) noexcept;

  const CompleteRecordCodec* owner_ = nullptr;
  std::uint64_t owner_epoch_ = 0U;
  std::uint64_t generation_ = 0U;

  friend class CompleteRecordCodec;
};

// status == OK 才有可消费 record；匹配身份和诊断数值不能替代成功字段交付。
struct DecodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  DecodedRecordView record;
  // Unique structural match from this call, not a success marker. Pre-match errors, unknown and
  // ambiguous frames leave it empty; post-match failure never publishes a usable record.
  std::optional<std::size_t> matched_message_index;
  std::size_t required_field_count = 0U;  // 容量需求，不是失败时已交付字段数。
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  bool output_tainted = false;  // 如保留未知枚举可在 OK 时为真；不是简单的失败标志。
};

// 只有 OK 时 [0, bytes_written) 是有效 Frame；required_size 是字节需求，不是交付长度。
struct EncodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t bytes_written = 0U;
  std::size_t required_size = 0U;
  std::optional<std::size_t> failed_value_index;  // 调用方 values 数组中的序号。
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
};

// 创建成功后保留冻结编译状态，外部 CompiledProtocol 可先销毁；metadata 借用视图的寿命
// 不因此延长。各 Codec 有独立 Workspace，可共享同一冻结状态；同实例重入返回
// WORKSPACE_BUSY，但这不允许与结果读取、移动或销毁并发。
class PAE_API CompleteRecordCodec final {
 public:
  CompleteRecordCodec(const CompleteRecordCodec&) = delete;
  CompleteRecordCodec& operator=(const CompleteRecordCodec&) = delete;
  CompleteRecordCodec(CompleteRecordCodec&& other) noexcept;
  CompleteRecordCodec& operator=(CompleteRecordCodec&& other) noexcept;
  ~CompleteRecordCodec();

  // Returned record/field views are borrowed from this instance. They expire on the next
  // successfully guarded Decode/Encode call, move construction/assignment of the owner, or owner
  // destruction. Reusing a moved-from owner does not revive its old views. BYTES additionally
  // borrow the Decode input. Reading views concurrently with a call or move is invalid.
  // 输入必须是一个完整记录，pipeline_index 为本配置全局索引。取得调用保护后，即使本次
  // 返回失败也使前次视图失效；未取得保护的忙状态不代表旧视图可以并发读取。
  [[nodiscard]] DecodeResult Decode(std::size_t pipeline_index, ByteView frame) noexcept;

  // The output buffer remains caller-owned. On failure bytes_written is zero and modified buffer
  // contents are not a deliverable result.
  // values/输出只在同步调用中借用，不能重叠；常量和计算字段不应由调用方覆盖。
  // 失败虽交付长度为 0，Buffer 仍可能已改写，不能发送或当作上一次成功 Frame 使用。
  [[nodiscard]] EncodeResult Encode(std::size_t pipeline_index, std::size_t message_index,
                                    const EncodeValue* values, std::size_t value_count,
                                    MutableByteBuffer output) noexcept;

  [[nodiscard]] ExecutionMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit CompleteRecordCodec(std::unique_ptr<Impl> impl) noexcept;
  void AdvanceViewEpoch() noexcept;
  std::unique_ptr<Impl> impl_;
  std::uint64_t view_epoch_ = 1U;

  friend class DecodedFieldView;
  friend class DecodedRecordView;
  friend class HostEndpoint;
  friend struct CompleteRecordCodecCreateResult;
  friend PAE_API CompleteRecordCodecCreateResult
  CreateCompleteRecordCodec(const CompiledProtocol&, const CompleteRecordCodecOptions&) noexcept;
};

// 成功时 codec 为调用方自有执行对象；失败时为空。memory 可携带预算失败的需求报告。
struct CompleteRecordCodecCreateResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::unique_ptr<CompleteRecordCodec> codec;
  ExecutionMemoryReport memory;
};

// 一次性准备执行存储并保留冻结状态；失败以 status 返回，不发布半成品执行对象。
[[nodiscard]] PAE_API CompleteRecordCodecCreateResult CreateCompleteRecordCodec(
    const CompiledProtocol& compiled, const CompleteRecordCodecOptions& options = {}) noexcept;

}  // namespace pae
