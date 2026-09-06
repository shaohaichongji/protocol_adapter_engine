#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
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
};

enum class LogicalValueKind {
  UINT64,
  INT64,
  BYTES,
  ENUM,
  BOOL,
};

struct ByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

struct MutableByteBuffer {
  std::uint8_t* data = nullptr;
  std::size_t capacity = 0U;
};

struct FieldRef {
  const protocol_plan::PlanBundle* plan_scope = nullptr;
  std::size_t message_index = kInvalidIndex;
  std::size_t field_index = kInvalidIndex;
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

struct DecodedFieldSlot {
  FieldRef field;
  LogicalValueKind value_kind = LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  ByteView bytes_value;
  DecodedEnumValue enum_value;
};

struct EncodeFieldValue {
  FieldRef field;
  LogicalValueKind value_kind = LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  ByteView bytes_value;
  EnumValueRef enum_value;
};

struct DecodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t message_index = kInvalidIndex;
  std::size_t field_count = 0U;
  std::size_t required_field_count = 0U;
  std::size_t failed_field_index = kInvalidIndex;
  bool tainted = false;
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

}  // namespace internal

struct EncodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t bytes_written = 0U;
  std::size_t required_size = 0U;
  std::size_t failed_value_index = kInvalidIndex;
  std::size_t failed_field_index = kInvalidIndex;
};

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
  std::size_t enum_search_steps = 0U;
  std::size_t static_plan_validation_visits = 0U;
};

#if defined(PAE_ENABLE_OPERATION_COUNTERS)
namespace test_only {
// Test-only fault injection for the instrumented target. The production target does not expose
// this declaration or carry the associated branch.
void CorruptIntegrityStorageBeforeFinalReviewOnce() noexcept;
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

 private:
  friend class ExecutionWorkspaceLease;
  friend DecodeResult DecodeCompleteRecord(const protocol_plan::PlanBundle&, ExecutionWorkspace&,
                                           std::size_t, ByteView, DecodedFieldSlot*,
                                           std::size_t) noexcept;
  friend EncodeResult EncodeCompleteRecord(const protocol_plan::PlanBundle&, ExecutionWorkspace&,
                                           std::size_t, std::size_t, const EncodeFieldValue*,
                                           std::size_t, MutableByteBuffer) noexcept;

  const protocol_plan::PlanBundle* plan_scope_ = nullptr;
  std::size_t max_values_per_call_ = 0U;
  std::vector<std::size_t> encode_value_indices_;
  std::vector<std::uint64_t> encode_present_words_;
  std::vector<std::uint64_t> bit_container_values_;
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
