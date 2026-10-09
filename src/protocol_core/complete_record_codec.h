#pragma once

// 内部完整记录执行边界：消费冻结描述与显式 Workspace，不读取配置或管理传输。
// 本头的槽、索引和状态是内部执行表示，不因此成为稳定公开 ABI。
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <vector>

#include "../protocol_plan/plan_bundle.h"

namespace pae::protocol_core {

inline constexpr std::size_t kInvalidIndex = std::numeric_limits<std::size_t>::max();

enum class CodecStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_PLAN,
  WORKSPACE_PLAN_MISMATCH,
  WORKSPACE_BUSY,
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
  INTERNAL_ERROR,
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  ASCII_CHARACTER_NOT_ALLOWED,
  ASCII_TERMINATOR_CONFLICT,
  OPERATION_NOT_SUPPORTED,
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  COMPUTED_FIELD_OVERRIDE,
  LENGTH_MISMATCH,
#endif
};

enum class LogicalValueKind {
  UINT64,
  INT64,
  BYTES,
  ENUM,
  BOOL,
  DECIMAL64,
};

struct Decimal64 {
  std::int64_t coefficient = 0;
  std::int32_t scale = 0;
};

enum class ConversionError {
  NONE,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
};

enum class RawIntegerKind {
  UINT64,
  INT64,
};

struct ByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

struct MutableByteBuffer {
  std::uint8_t* data = nullptr;
  std::size_t capacity = 0U;
};

// Plan 地址与消息内索引共同确定归属；同形状的另一个 Plan 也不是相同作用域。
struct FieldRef {
  const protocol_plan::PlanBundle* plan_scope = nullptr;
  std::size_t message_index = kInvalidIndex;
  std::size_t field_index = kInvalidIndex;
};

struct RawIntegerValue {
  FieldRef field;
  RawIntegerKind kind = RawIntegerKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
};

struct EnumValueRef {
  const protocol_plan::PlanBundle* plan_scope = nullptr;
  std::size_t message_index = kInvalidIndex;
  std::size_t field_index = kInvalidIndex;
  std::size_t entry_index = kInvalidIndex;
};

struct DecodedEnumValue {
  bool known = false;
  std::uint64_t raw_value = 0U;
  EnumValueRef reference;
};

// 标签选择有效值成员；BYTES 借用输入，数值/Decimal 按值保存，枚举引用借用 Plan。
struct DecodedFieldSlot {
  FieldRef field;
  LogicalValueKind value_kind = LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  ByteView bytes_value;
  DecodedEnumValue enum_value;
  Decimal64 decimal64_value;
};

struct EncodeFieldValue {
  FieldRef field;
  LogicalValueKind value_kind = LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  ByteView bytes_value;
  EnumValueRef enum_value;
  Decimal64 decimal64_value;
};

// field_count 只在整条记录成功后发布；required_field_count/匹配身份可用于失败诊断。
// 失败不交付槽内内容，不承诺调用方预填槽或内部临时值完全未被改写。
struct DecodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t message_index = kInvalidIndex;
  std::size_t field_count = 0U;
  std::size_t required_field_count = 0U;
  std::size_t failed_field_index = kInvalidIndex;
  bool tainted = false;
  ConversionError conversion_error = ConversionError::NONE;
};

namespace internal {

// Internal COMPLETE_RECORD matcher query used by bounded orchestration layers that must establish
// structural uniqueness before executing integrity or field semantics. This is not a stable public
// protocol API and does not allocate or mutate an ExecutionWorkspace.
struct StructuralMatchResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t message_index = kInvalidIndex;
};

[[nodiscard]] StructuralMatchResult MatchCompleteRecordStructure(
    const protocol_plan::PlanBundle& plan, std::size_t pipeline_index, ByteView input) noexcept;

[[nodiscard]] bool SupportsCompleteRecordSchema(std::string_view schema_version) noexcept;

}  // namespace internal

// bytes_written 是成功交付长度；失败保持 0，但写入后的失败不回滚调用方 Buffer。
struct EncodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t bytes_written = 0U;
  std::size_t required_size = 0U;
  std::size_t failed_value_index = kInvalidIndex;
  std::size_t failed_field_index = kInvalidIndex;
  ConversionError conversion_error = ConversionError::NONE;
};

// 插桩目标的操作访问计数，不是耗时/吞吐指标；生产构建的读取返回零值。
struct CodecOperationCounts {
  std::size_t input_values_visited = 0U;
  std::size_t field_validation_visits = 0U;
  std::size_t alias_validation_visits = 0U;
  std::size_t required_field_scan_visits = 0U;
  std::size_t field_write_visits = 0U;
  std::size_t field_verify_visits = 0U;
  std::size_t fixed_byte_write_visits = 0U;
  std::size_t bytes_write_visits = 0U;
  std::size_t bytes_verify_visits = 0U;
  std::size_t presence_word_clear_visits = 0U;
  std::size_t candidate_group_search_steps = 0U;
  std::size_t candidate_messages_examined = 0U;
  std::size_t matcher_bytes_compared = 0U;
  std::size_t integrity_bytes_accumulated = 0U;
  std::size_t integrity_bytes_verified = 0U;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::size_t computed_length_fields_generated = 0U;
  std::size_t computed_length_fields_verified = 0U;
#endif
  std::size_t enum_search_steps = 0U;
  std::size_t static_plan_validation_visits = 0U;
  std::size_t decimal_conversion_visits = 0U;
};

#if defined(PAE_ENABLE_OPERATION_COUNTERS)
namespace test_only {
// Test-only fault injection for the instrumented target. The production target does not expose
// this declaration or carry the associated branch.
void CorruptIntegrityStorageBeforeFinalReviewOnce() noexcept;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
void CorruptAsciiOutputBeforeFinalReviewOnce() noexcept;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
void CorruptComputedLengthBeforeFinalReviewOnce() noexcept;
#endif
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
void CorruptDecimalFieldBeforeFinalReviewOnce() noexcept;
void FailNextDecimalConversionOnce() noexcept;
void FailNextDecimalFinalReviewOnce() noexcept;
#endif
}  // namespace test_only
#endif

class ExecutionWorkspaceLease;

// A workspace borrows and is permanently bound to one frozen PlanBundle; it does not own or extend
// the PlanBundle lifetime. The PlanBundle must outlive the workspace, and callers must not destroy
// either object while a Codec call is active. Concurrent or reentrant calls that reuse one
// workspace are rejected; distinct workspaces may share a Plan.
class ExecutionWorkspace final {
 public:
  explicit ExecutionWorkspace(const protocol_plan::PlanBundle& plan);

  ExecutionWorkspace(const ExecutionWorkspace&) = delete;
  ExecutionWorkspace& operator=(const ExecutionWorkspace&) = delete;
  ExecutionWorkspace(ExecutionWorkspace&&) = delete;
  ExecutionWorkspace& operator=(ExecutionWorkspace&&) = delete;
  ~ExecutionWorkspace() = default;

  // Read only while no Codec call occupies this workspace (normally after the preceding call has
  // returned); concurrent reads can race with an instrumented call's reset/increments. Production
  // targets return zero without carrying or resetting counter storage; the instrumented test target
  // resets and records the visited operations.
  [[nodiscard]] CodecOperationCounts LastOperationCounts() const noexcept;
  [[nodiscard]] std::size_t LastRawIntegerCount() const noexcept;
  [[nodiscard]] bool GetLastRawInteger(std::size_t index, RawIntegerValue& output) const noexcept;

 private:
  friend class ExecutionWorkspaceLease;
  friend DecodeResult DecodeCompleteRecord(const protocol_plan::PlanBundle&, ExecutionWorkspace&,
                                           std::size_t, ByteView, DecodedFieldSlot*,
                                           std::size_t) noexcept;
  friend EncodeResult EncodeCompleteRecord(const protocol_plan::PlanBundle&, ExecutionWorkspace&,
                                           std::size_t, std::size_t, const EncodeFieldValue*,
                                           std::size_t, MutableByteBuffer) noexcept;

  // 存储按冻结布局一次性准备；这些可变槽不属于共享 Plan，也不能跨调用占用复用。
  const protocol_plan::PlanBundle* plan_scope_ = nullptr;
  std::size_t max_values_per_call_ = 0U;
  std::vector<std::size_t> encode_value_indices_;
  std::vector<std::uint64_t> encode_present_words_;
  std::vector<std::uint64_t> bit_container_values_;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::vector<std::int64_t> decode_decimal_coefficients_;
  std::vector<std::int32_t> decode_decimal_scales_;
  std::vector<std::uint64_t> raw_integer_bits_;
  std::vector<protocol_plan::ValueType> raw_integer_kinds_;
  std::vector<std::size_t> raw_integer_field_indices_;
  std::vector<std::uint64_t> encode_conversion_raw_values_;
  std::size_t raw_integer_count_ = 0U;
  std::size_t raw_integer_message_index_ = kInvalidIndex;
#endif
  std::atomic_flag in_use_ = ATOMIC_FLAG_INIT;
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  CodecOperationCounts operation_counts_;
#endif
};

// Returned BYTES views borrow the input buffer and become invalid when that storage is modified,
// reused, or destroyed. Field and enum references are valid only while the same PlanBundle remains
// alive at the same address. Callers must copy borrowed bytes before retaining them beyond the
// input lifetime.

[[nodiscard]] DecodeResult DecodeCompleteRecord(const protocol_plan::PlanBundle& plan,
                                                ExecutionWorkspace& workspace,
                                                std::size_t pipeline_index, ByteView input,
                                                DecodedFieldSlot* field_slots,
                                                std::size_t field_slot_capacity) noexcept;

[[nodiscard]] EncodeResult EncodeCompleteRecord(
    const protocol_plan::PlanBundle& plan, ExecutionWorkspace& workspace,
    std::size_t pipeline_index, std::size_t message_index, const EncodeFieldValue* values,
    std::size_t value_count, MutableByteBuffer output) noexcept;

}  // namespace pae::protocol_core
