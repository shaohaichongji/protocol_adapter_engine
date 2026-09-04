#include "complete_record_codec.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace pae::protocol_core {
namespace {

using protocol_plan::ByteOrder;
using protocol_plan::CandidateGroupExecutionPlan;
using protocol_plan::EncodeSource;
using protocol_plan::FieldExecutionPlan;
using protocol_plan::MessageExecutionPlan;
using protocol_plan::PipelineExecutionPlan;
using protocol_plan::PlanBundle;
using protocol_plan::UnknownEnumPolicy;
using protocol_plan::ValueType;

struct AddressRange {
  std::uintptr_t begin = 0U;
  std::uintptr_t end = 0U;
};

struct MatchOutcome {
  std::size_t match_count = 0U;
  std::size_t message_index = kInvalidIndex;
};

constexpr std::size_t kPresenceWordBits = 64U;

std::size_t PresenceWordCount(std::size_t bit_count) noexcept {
  return bit_count / kPresenceWordBits + (bit_count % kPresenceWordBits == 0U ? 0U : 1U);
}

#if defined(PAE_ENABLE_OPERATION_COUNTERS)
#define PAE_OPERATION_COUNTS_PARAMETER , CodecOperationCounts& counts
#define PAE_OPERATION_COUNTS_ARGUMENT , counts
#define PAE_INCREMENT_OPERATION_COUNT(member) (++counts.member)
#else
#define PAE_OPERATION_COUNTS_PARAMETER
#define PAE_OPERATION_COUNTS_ARGUMENT
#define PAE_INCREMENT_OPERATION_COUNT(member) ((void)0)
#endif

bool GetAddressRange(const void* data, std::size_t size, AddressRange& range) noexcept {
  if (size == 0U) {
    range = AddressRange{};
    return true;
  }
  if (data == nullptr) {
    return false;
  }
  const auto begin = reinterpret_cast<std::uintptr_t>(data);
  if (size > std::numeric_limits<std::uintptr_t>::max() - begin) {
    return false;
  }
  range.begin = begin;
  range.end = begin + size;
  return true;
}

bool GetArrayAddressRange(const void* data, std::size_t count, std::size_t element_size,
                          AddressRange& range) noexcept {
  if (element_size != 0U && count > std::numeric_limits<std::size_t>::max() / element_size) {
    return false;
  }
  return GetAddressRange(data, count * element_size, range);
}

bool RangesOverlap(const AddressRange& left, const AddressRange& right) noexcept {
  if (left.begin == left.end || right.begin == right.end) {
    return false;
  }
  return left.begin < right.end && right.begin < left.end;
}

bool FitsUnsignedWidth(std::uint64_t value, std::size_t byte_width) noexcept {
  if (byte_width == 0U || byte_width > 8U) {
    return false;
  }
  if (byte_width == 8U) {
    return true;
  }
  const auto bit_count = static_cast<unsigned>(byte_width * 8U);
  return value < (std::uint64_t{1U} << bit_count);
}

bool IsIntegerByteOrderValid(ByteOrder order, std::size_t byte_width) noexcept {
  if (byte_width == 1U) {
    return order == ByteOrder::NOT_APPLICABLE || order == ByteOrder::BIG ||
           order == ByteOrder::LITTLE;
  }
  return order == ByteOrder::BIG || order == ByteOrder::LITTLE;
}

bool LoadUnsigned(const std::uint8_t* data, std::size_t byte_width, ByteOrder order,
                  std::uint64_t& value) noexcept {
  if (data == nullptr || byte_width == 0U || byte_width > 8U ||
      !IsIntegerByteOrderValid(order, byte_width)) {
    return false;
  }
  value = 0U;
  if (byte_width == 1U || order == ByteOrder::BIG) {
    for (std::size_t index = 0U; index < byte_width; ++index) {
      value = (value << 8U) | data[index];
    }
    return true;
  }
  for (std::size_t index = 0U; index < byte_width; ++index) {
    const auto shift = static_cast<unsigned>(index * 8U);
    value |= std::uint64_t{data[index]} << shift;
  }
  return true;
}

bool StoreUnsigned(std::uint64_t value, std::uint8_t* data, std::size_t byte_width,
                   ByteOrder order) noexcept {
  if (data == nullptr || !IsIntegerByteOrderValid(order, byte_width) ||
      !FitsUnsignedWidth(value, byte_width)) {
    return false;
  }
  for (std::size_t index = 0U; index < byte_width; ++index) {
    const std::size_t source_index = order == ByteOrder::BIG ? byte_width - 1U - index : index;
    const auto shift = static_cast<unsigned>(source_index * 8U);
    data[index] = static_cast<std::uint8_t>((value >> shift) & 0xFFU);
  }
  return true;
}

const CandidateGroupExecutionPlan* FindCandidateGroup(const PipelineExecutionPlan& pipeline,
                                                      std::size_t frame_size
                                                          PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  std::size_t first = 0U;
  std::size_t last = pipeline.candidate_groups.size();
  while (first < last) {
    PAE_INCREMENT_OPERATION_COUNT(candidate_group_search_steps);
    const std::size_t middle = first + (last - first) / 2U;
    if (pipeline.candidate_groups[middle].frame_size < frame_size) {
      first = middle + 1U;
    } else {
      last = middle;
    }
  }
  if (first == pipeline.candidate_groups.size() ||
      pipeline.candidate_groups[first].frame_size != frame_size) {
    return nullptr;
  }
  return &pipeline.candidate_groups[first];
}

bool MessageMatches(const MessageExecutionPlan& message,
                    ByteView input PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  if (input.size != message.frame_size) {
    return false;
  }
  for (const auto& fixed_byte : message.fixed_bytes) {
    PAE_INCREMENT_OPERATION_COUNT(matcher_bytes_compared);
    if (input.data[fixed_byte.offset] != fixed_byte.value) {
      return false;
    }
  }
  return true;
}

MatchOutcome FindPipelineMatch(const PipelineExecutionPlan& pipeline,
                               const protocol_plan::FrozenArray<MessageExecutionPlan>& messages,
                               ByteView input PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  MatchOutcome outcome;
  const CandidateGroupExecutionPlan* group =
      FindCandidateGroup(pipeline, input.size PAE_OPERATION_COUNTS_ARGUMENT);
  if (group == nullptr) {
    return outcome;
  }
  for (const std::size_t message_index : group->message_indices) {
    PAE_INCREMENT_OPERATION_COUNT(candidate_messages_examined);
    if (!MessageMatches(messages[message_index], input PAE_OPERATION_COUNTS_ARGUMENT)) {
      continue;
    }
    ++outcome.match_count;
    if (outcome.match_count == 1U) {
      outcome.message_index = message_index;
    }
  }
  return outcome;
}

bool MessageIsAllowed(const PipelineExecutionPlan& pipeline, std::size_t message_index) noexcept {
  const std::size_t word_index = message_index / kPresenceWordBits;
  const auto mask = std::uint64_t{1U} << (message_index % kPresenceWordBits);
  return (pipeline.allowed_message_words[word_index] & mask) != 0U;
}

std::size_t FindEnumEntry(const MessageExecutionPlan& message, const FieldExecutionPlan& field,
                          std::uint64_t raw_value PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  std::size_t first = field.enum_lookup_begin;
  std::size_t last = field.enum_lookup_begin + field.enum_lookup_count;
  while (first < last) {
    PAE_INCREMENT_OPERATION_COUNT(enum_search_steps);
    const std::size_t middle = first + (last - first) / 2U;
    const auto& entry = message.enum_lookup_entries[middle];
    if (entry.raw_value < raw_value) {
      first = middle + 1U;
    } else {
      last = middle;
    }
  }
  const std::size_t end = field.enum_lookup_begin + field.enum_lookup_count;
  if (first == end || message.enum_lookup_entries[first].raw_value != raw_value) {
    return kInvalidIndex;
  }
  return message.enum_lookup_entries[first].entry_index;
}

LogicalValueKind GetLogicalValueKind(ValueType value_type) noexcept {
  switch (value_type) {
    case ValueType::UINT64:
      return LogicalValueKind::UINT64;
    case ValueType::BYTES:
      return LogicalValueKind::BYTES;
    case ValueType::ENUM:
      return LogicalValueKind::ENUM;
  }
  return LogicalValueKind::UINT64;
}

bool ValueKindMatches(LogicalValueKind value_kind, ValueType value_type) noexcept {
  return value_kind == GetLogicalValueKind(value_type);
}

bool IsPresent(const std::vector<std::uint64_t>& words, std::size_t ordinal) noexcept {
  const std::size_t word_index = ordinal / kPresenceWordBits;
  const auto mask = std::uint64_t{1U} << (ordinal % kPresenceWordBits);
  return (words[word_index] & mask) != 0U;
}

void MarkPresent(std::vector<std::uint64_t>& words, std::size_t ordinal) noexcept {
  const std::size_t word_index = ordinal / kPresenceWordBits;
  const auto mask = std::uint64_t{1U} << (ordinal % kPresenceWordBits);
  words[word_index] |= mask;
}

CodecStatus PrepareEncodeInputs(const PlanBundle& plan, const MessageExecutionPlan& message,
                                std::size_t message_index, const EncodeFieldValue* values,
                                std::size_t value_count, std::vector<std::size_t>& value_indices,
                                std::vector<std::uint64_t>& present_words,
                                EncodeResult& result PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  if (message.required_input_count > value_indices.size() ||
      PresenceWordCount(message.required_input_count) > present_words.size()) {
    return CodecStatus::INVALID_PLAN;
  }
  const std::size_t presence_word_count = PresenceWordCount(message.required_input_count);
  for (std::size_t word_index = 0U; word_index < presence_word_count; ++word_index) {
    PAE_INCREMENT_OPERATION_COUNT(presence_word_clear_visits);
    present_words[word_index] = 0U;
  }

  for (std::size_t value_index = 0U; value_index < value_count; ++value_index) {
    PAE_INCREMENT_OPERATION_COUNT(input_values_visited);
    const EncodeFieldValue& value = values[value_index];
    result.failed_value_index = value_index;
    if (value.field.plan_scope != &plan || value.field.message_index != message_index ||
        value.field.field_index >= message.fields.size()) {
      return CodecStatus::FIELD_REFERENCE_MISMATCH;
    }

    result.failed_field_index = value.field.field_index;
    PAE_INCREMENT_OPERATION_COUNT(field_validation_visits);
    const FieldExecutionPlan& field = message.fields[value.field.field_index];
    if (field.encode_source != EncodeSource::INPUT) {
      return CodecStatus::CONSTANT_FIELD_OVERRIDE;
    }
    if (field.input_ordinal >= message.required_input_count) {
      return CodecStatus::INVALID_PLAN;
    }
    if (IsPresent(present_words, field.input_ordinal)) {
      return CodecStatus::DUPLICATE_FIELD;
    }
    if (!ValueKindMatches(value.value_kind, field.value_type)) {
      return CodecStatus::TYPE_MISMATCH;
    }

    if (field.value_type == ValueType::UINT64) {
      if (!FitsUnsignedWidth(value.uint64_value, field.width)) {
        return CodecStatus::VALUE_NOT_REPRESENTABLE;
      }
    } else if (field.value_type == ValueType::BYTES) {
      if (value.bytes_value.size != field.width) {
        return CodecStatus::BYTES_LENGTH_MISMATCH;
      }
      AddressRange unused;
      if (!GetAddressRange(value.bytes_value.data, value.bytes_value.size, unused)) {
        return CodecStatus::INVALID_ARGUMENT;
      }
    } else if (value.enum_value.plan_scope != &plan ||
               value.enum_value.message_index != message_index ||
               value.enum_value.field_index != value.field.field_index ||
               value.enum_value.entry_index >= field.enum_values_count) {
      return CodecStatus::ENUM_REFERENCE_MISMATCH;
    }

    value_indices[field.input_ordinal] = value_index;
    MarkPresent(present_words, field.input_ordinal);
  }

  result.failed_value_index = kInvalidIndex;
  for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
    PAE_INCREMENT_OPERATION_COUNT(required_field_scan_visits);
    const FieldExecutionPlan& field = message.fields[field_index];
    result.failed_field_index = field_index;
    if (field.encode_source == EncodeSource::INPUT &&
        !IsPresent(present_words, field.input_ordinal)) {
      return CodecStatus::MISSING_FIELD;
    }
  }
  result.failed_field_index = kInvalidIndex;
  return CodecStatus::OK;
}

std::uint64_t GetEncodeRawValue(const MessageExecutionPlan& message,
                                const FieldExecutionPlan& field, const EncodeFieldValue* values,
                                const std::vector<std::size_t>& value_indices) noexcept {
  if (field.encode_source == EncodeSource::CONSTANT) {
    return field.constant_value;
  }
  const EncodeFieldValue& value = values[value_indices[field.input_ordinal]];
  if (field.value_type == ValueType::ENUM) {
    return message.enum_raw_values[field.enum_values_begin + value.enum_value.entry_index];
  }
  return value.uint64_value;
}

void WriteFixedBytes(const MessageExecutionPlan& message,
                     std::uint8_t* output PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  for (const auto& fixed_byte : message.fixed_bytes) {
    PAE_INCREMENT_OPERATION_COUNT(fixed_byte_write_visits);
    output[fixed_byte.offset] = fixed_byte.value;
  }
}

bool WriteFields(const MessageExecutionPlan& message, const EncodeFieldValue* values,
                 const std::vector<std::size_t>& value_indices,
                 std::uint8_t* output PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  for (const FieldExecutionPlan& field : message.fields) {
    PAE_INCREMENT_OPERATION_COUNT(field_write_visits);
    if (field.value_type == ValueType::BYTES) {
      const ByteView bytes = values[value_indices[field.input_ordinal]].bytes_value;
      for (std::size_t index = 0U; index < field.width; ++index) {
        PAE_INCREMENT_OPERATION_COUNT(bytes_write_visits);
        output[field.offset + index] = bytes.data[index];
      }
      continue;
    }
    const std::uint64_t raw_value = GetEncodeRawValue(message, field, values, value_indices);
    if (!StoreUnsigned(raw_value, output + field.offset, field.width, field.byte_order)) {
      return false;
    }
  }
  return true;
}

bool VerifyFields(const MessageExecutionPlan& message, const EncodeFieldValue* values,
                  const std::vector<std::size_t>& value_indices, const std::uint8_t* output,
                  std::size_t& failed_field_index PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
    PAE_INCREMENT_OPERATION_COUNT(field_verify_visits);
    const FieldExecutionPlan& field = message.fields[field_index];
    if (field.value_type == ValueType::BYTES) {
      const ByteView bytes = values[value_indices[field.input_ordinal]].bytes_value;
      for (std::size_t index = 0U; index < field.width; ++index) {
        PAE_INCREMENT_OPERATION_COUNT(bytes_verify_visits);
        if (output[field.offset + index] != bytes.data[index]) {
          failed_field_index = field_index;
          return false;
        }
      }
      continue;
    }
    std::uint64_t actual = 0U;
    const std::uint64_t expected = GetEncodeRawValue(message, field, values, value_indices);
    if (!LoadUnsigned(output + field.offset, field.width, field.byte_order, actual) ||
        actual != expected) {
      failed_field_index = field_index;
      return false;
    }
  }
  return true;
}

}  // namespace

class ExecutionWorkspaceLease final {
 public:
  explicit ExecutionWorkspaceLease(ExecutionWorkspace& workspace) noexcept
      : workspace_(workspace),
        acquired_(!workspace_.in_use_.test_and_set(std::memory_order_acquire)) {}

  ExecutionWorkspaceLease(const ExecutionWorkspaceLease&) = delete;
  ExecutionWorkspaceLease& operator=(const ExecutionWorkspaceLease&) = delete;

  ~ExecutionWorkspaceLease() {
    if (acquired_) {
      workspace_.in_use_.clear(std::memory_order_release);
    }
  }

  [[nodiscard]] bool Acquired() const noexcept { return acquired_; }

 private:
  ExecutionWorkspace& workspace_;
  bool acquired_ = false;
};

ExecutionWorkspace::ExecutionWorkspace(const PlanBundle& plan)
    : plan_scope_(&plan),
      max_values_per_call_(plan.GetExecutionResourceLayout().max_fields_per_message),
      encode_value_indices_(plan.GetExecutionResourceLayout().encode_value_index_count,
                            kInvalidIndex),
      encode_present_words_(plan.GetExecutionResourceLayout().encode_presence_word_count,
                            std::uint64_t{0U}) {}

CodecOperationCounts ExecutionWorkspace::LastOperationCounts() const noexcept {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  return operation_counts_;
#else
  return CodecOperationCounts{};
#endif
}

DecodeResult DecodeCompleteRecord(const PlanBundle& plan, ExecutionWorkspace& workspace,
                                  std::size_t pipeline_index, ByteView input,
                                  DecodedFieldSlot* field_slots,
                                  std::size_t field_slot_capacity) noexcept {
  DecodeResult result;
  ExecutionWorkspaceLease lease(workspace);
  if (!lease.Acquired()) {
    result.status = CodecStatus::WORKSPACE_BUSY;
    return result;
  }
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  workspace.operation_counts_ = CodecOperationCounts{};
  CodecOperationCounts& counts = workspace.operation_counts_;
#endif
  if (workspace.plan_scope_ != &plan) {
    result.status = CodecStatus::WORKSPACE_PLAN_MISMATCH;
    return result;
  }
  if ((input.data == nullptr && input.size != 0U) ||
      (field_slots == nullptr && field_slot_capacity != 0U)) {
    return result;
  }

  const auto& pipelines = plan.PipelineExecutionPlans();
  if (pipeline_index >= pipelines.size()) {
    return result;
  }
  const PipelineExecutionPlan& pipeline = pipelines[pipeline_index];
  const auto& messages = plan.MessageExecutionPlans();
  const MatchOutcome match =
      FindPipelineMatch(pipeline, messages, input PAE_OPERATION_COUNTS_ARGUMENT);
  if (match.match_count == 0U) {
    result.status = CodecStatus::UNKNOWN_MESSAGE;
    return result;
  }
  if (match.match_count != 1U) {
    result.status = CodecStatus::AMBIGUOUS_MESSAGE;
    return result;
  }

  result.message_index = match.message_index;
  const MessageExecutionPlan& message = messages[match.message_index];
  result.required_field_count = message.fields.size();
  if (message.fields.size() > field_slot_capacity) {
    result.status = CodecStatus::OUTPUT_SLOTS_TOO_SMALL;
    return result;
  }

  AddressRange input_range;
  AddressRange slot_range;
  if (!GetAddressRange(input.data, input.size, input_range) ||
      !GetArrayAddressRange(field_slots, message.fields.size(), sizeof(DecodedFieldSlot),
                            slot_range)) {
    result.status = CodecStatus::INVALID_ARGUMENT;
    return result;
  }
  if (RangesOverlap(input_range, slot_range)) {
    result.status = CodecStatus::INPUT_OUTPUT_OVERLAP;
    return result;
  }

  bool tainted = false;
  for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
    PAE_INCREMENT_OPERATION_COUNT(field_validation_visits);
    const FieldExecutionPlan& field = message.fields[field_index];
    if (field.value_type != ValueType::ENUM) {
      continue;
    }
    std::uint64_t raw_value = 0U;
    if (!LoadUnsigned(input.data + field.offset, field.width, field.byte_order, raw_value)) {
      result.status = CodecStatus::INVALID_PLAN;
      result.failed_field_index = field_index;
      return result;
    }
    if (FindEnumEntry(message, field, raw_value PAE_OPERATION_COUNTS_ARGUMENT) == kInvalidIndex) {
      if (field.unknown_enum_policy == UnknownEnumPolicy::REJECT) {
        result.status = CodecStatus::UNKNOWN_ENUM_VALUE;
        result.failed_field_index = field_index;
        return result;
      }
      tainted = true;
    }
  }

  for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
    PAE_INCREMENT_OPERATION_COUNT(field_write_visits);
    const FieldExecutionPlan& field = message.fields[field_index];
    DecodedFieldSlot slot;
    slot.field = FieldRef{&plan, match.message_index, field_index};
    slot.value_kind = GetLogicalValueKind(field.value_type);
    if (field.value_type == ValueType::BYTES) {
      slot.bytes_value = ByteView{input.data + field.offset, field.width};
    } else {
      std::uint64_t raw_value = 0U;
      if (!LoadUnsigned(input.data + field.offset, field.width, field.byte_order, raw_value)) {
        result.status = CodecStatus::INVALID_PLAN;
        result.failed_field_index = field_index;
        return result;
      }
      if (field.value_type == ValueType::UINT64) {
        slot.uint64_value = raw_value;
      } else {
        const std::size_t entry_index =
            FindEnumEntry(message, field, raw_value PAE_OPERATION_COUNTS_ARGUMENT);
        slot.enum_value.raw_value = raw_value;
        slot.enum_value.known = entry_index != kInvalidIndex;
        if (slot.enum_value.known) {
          slot.enum_value.reference =
              EnumValueRef{&plan, match.message_index, field_index, entry_index};
        }
      }
    }
    field_slots[field_index] = slot;
  }

  result.status = CodecStatus::OK;
  result.field_count = message.fields.size();
  result.failed_field_index = kInvalidIndex;
  result.tainted = tainted;
  return result;
}

EncodeResult EncodeCompleteRecord(const PlanBundle& plan, ExecutionWorkspace& workspace,
                                  std::size_t pipeline_index, std::size_t message_index,
                                  const EncodeFieldValue* values, std::size_t value_count,
                                  MutableByteBuffer output) noexcept {
  EncodeResult result;
  ExecutionWorkspaceLease lease(workspace);
  if (!lease.Acquired()) {
    result.status = CodecStatus::WORKSPACE_BUSY;
    return result;
  }
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  workspace.operation_counts_ = CodecOperationCounts{};
  CodecOperationCounts& counts = workspace.operation_counts_;
#endif
  if (workspace.plan_scope_ != &plan) {
    result.status = CodecStatus::WORKSPACE_PLAN_MISMATCH;
    return result;
  }
  if (values == nullptr && value_count != 0U) {
    return result;
  }

  AddressRange value_descriptor_range;
  if (!GetArrayAddressRange(values, value_count, sizeof(EncodeFieldValue),
                            value_descriptor_range) ||
      value_count > workspace.max_values_per_call_) {
    result.status = CodecStatus::INVALID_ARGUMENT;
    return result;
  }

  const auto& pipelines = plan.PipelineExecutionPlans();
  if (pipeline_index >= pipelines.size()) {
    return result;
  }
  const PipelineExecutionPlan& pipeline = pipelines[pipeline_index];
  const auto& messages = plan.MessageExecutionPlans();
  if (message_index >= messages.size() || !MessageIsAllowed(pipeline, message_index)) {
    result.status = CodecStatus::MESSAGE_NOT_ALLOWED;
    return result;
  }

  const MessageExecutionPlan& message = messages[message_index];
  result.required_size = message.frame_size;
  const CodecStatus value_status = PrepareEncodeInputs(
      plan, message, message_index, values, value_count, workspace.encode_value_indices_,
      workspace.encode_present_words_, result PAE_OPERATION_COUNTS_ARGUMENT);
  if (value_status != CodecStatus::OK) {
    result.status = value_status;
    result.required_size = 0U;
    return result;
  }

  if (output.data == nullptr) {
    result.status = CodecStatus::INVALID_ARGUMENT;
    return result;
  }
  if (output.capacity < message.frame_size) {
    result.status = CodecStatus::BUFFER_TOO_SMALL;
    return result;
  }

  AddressRange output_range;
  if (!GetAddressRange(output.data, message.frame_size, output_range)) {
    result.status = CodecStatus::INVALID_ARGUMENT;
    return result;
  }
  if (RangesOverlap(value_descriptor_range, output_range)) {
    result.status = CodecStatus::INPUT_OUTPUT_OVERLAP;
    return result;
  }
  for (std::size_t value_index = 0U; value_index < value_count; ++value_index) {
    PAE_INCREMENT_OPERATION_COUNT(alias_validation_visits);
    if (values[value_index].value_kind != LogicalValueKind::BYTES) {
      continue;
    }
    AddressRange input_range;
    if (!GetAddressRange(values[value_index].bytes_value.data, values[value_index].bytes_value.size,
                         input_range)) {
      result.status = CodecStatus::INVALID_ARGUMENT;
      result.failed_value_index = value_index;
      result.failed_field_index = values[value_index].field.field_index;
      return result;
    }
    if (RangesOverlap(input_range, output_range)) {
      result.status = CodecStatus::INPUT_OUTPUT_OVERLAP;
      result.failed_value_index = value_index;
      result.failed_field_index = values[value_index].field.field_index;
      return result;
    }
  }

  WriteFixedBytes(message, output.data PAE_OPERATION_COUNTS_ARGUMENT);
  if (!WriteFields(message, values, workspace.encode_value_indices_,
                   output.data PAE_OPERATION_COUNTS_ARGUMENT)) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }

  const ByteView encoded{output.data, message.frame_size};
  const MatchOutcome final_match =
      FindPipelineMatch(pipeline, messages, encoded PAE_OPERATION_COUNTS_ARGUMENT);
  if (final_match.match_count != 1U || final_match.message_index != message_index ||
      !VerifyFields(message, values, workspace.encode_value_indices_, output.data,
                    result.failed_field_index PAE_OPERATION_COUNTS_ARGUMENT)) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }

  result.status = CodecStatus::OK;
  result.bytes_written = message.frame_size;
  result.failed_value_index = kInvalidIndex;
  result.failed_field_index = kInvalidIndex;
  return result;
}

#undef PAE_INCREMENT_OPERATION_COUNT
#undef PAE_OPERATION_COUNTS_ARGUMENT
#undef PAE_OPERATION_COUNTS_PARAMETER

}  // namespace pae::protocol_core
