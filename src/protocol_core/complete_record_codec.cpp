#include "complete_record_codec.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "decimal_conversion_internal.h"

namespace pae::protocol_core {
namespace {

using protocol_plan::ByteOrder;
using protocol_plan::CandidateGroupExecutionPlan;
using protocol_plan::EncodeSource;
using protocol_plan::FieldExecutionPlan;
using protocol_plan::FrozenIntegrityPlan;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
using protocol_plan::ComputedLengthScope;
using protocol_plan::FrozenComputedLengthPlan;
#endif
using protocol_plan::MessageExecutionPlan;
using protocol_plan::PipelineExecutionPlan;
using protocol_plan::PlanBundle;
using protocol_plan::UnknownEnumPolicy;
using protocol_plan::ValueType;
using protocol_plan::WireCodec;

struct AddressRange {
  std::uintptr_t begin = 0U;
  std::uintptr_t end = 0U;
};

struct MatchOutcome {
  std::size_t match_count = 0U;
  std::size_t message_index = kInvalidIndex;
};

constexpr std::size_t kPresenceWordBits = 64U;

#if defined(PAE_ENABLE_OPERATION_COUNTERS)
std::atomic<bool> g_corrupt_integrity_storage_before_final_review{false};
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
std::atomic<bool> g_corrupt_computed_length_before_final_review{false};
#endif
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
std::atomic<bool> g_corrupt_decimal_field_before_final_review{false};
std::atomic<bool> g_fail_next_decimal_conversion{false};
std::atomic<bool> g_fail_next_decimal_final_review{false};
#endif
#endif

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

bool FitsUnsignedBits(std::uint64_t value, std::size_t bit_width) noexcept {
  return bit_width == 64U || (bit_width != 0U && bit_width < 64U &&
                              value < (std::uint64_t{1U} << static_cast<unsigned>(bit_width)));
}

bool FitsSignedWidth(std::int64_t value, std::size_t byte_width) noexcept {
  if (byte_width == 0U || byte_width > 8U) {
    return false;
  }
  if (byte_width == 8U) {
    return true;
  }
  const unsigned magnitude_bits = static_cast<unsigned>(byte_width * 8U - 1U);
  const std::int64_t minimum = -static_cast<std::int64_t>(std::uint64_t{1U} << magnitude_bits);
  const std::int64_t maximum =
      static_cast<std::int64_t>((std::uint64_t{1U} << magnitude_bits) - 1U);
  return value >= minimum && value <= maximum;
}

bool InterpretSigned(std::uint64_t raw, std::size_t byte_width, std::int64_t& value) noexcept {
  if (byte_width == 0U || byte_width > 8U) {
    return false;
  }
  const unsigned bits = static_cast<unsigned>(byte_width * 8U);
  const std::uint64_t sign = std::uint64_t{1U} << (bits - 1U);
  if ((raw & sign) == 0U) {
    value = static_cast<std::int64_t>(raw);
    return true;
  }
  const std::uint64_t mask =
      bits == 64U ? (std::numeric_limits<std::uint64_t>::max)() : (std::uint64_t{1U} << bits) - 1U;
  const std::uint64_t magnitude = ((~raw) & mask) + 1U;
  if (magnitude == (std::uint64_t{1U} << 63U)) {
    value = (std::numeric_limits<std::int64_t>::min)();
  } else {
    value = -static_cast<std::int64_t>(magnitude);
  }
  return true;
}

std::uint64_t ExtractBitfield(std::uint64_t container, const FieldExecutionPlan& field) noexcept {
  return (container & field.bit_mask) >> field.bit_shift;
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

bool IntegrityDescriptorValid(const MessageExecutionPlan& message) noexcept {
  if (!message.integrity.has_value()) {
    return true;
  }
  const auto& integrity = *message.integrity;
  std::size_t storage_width = 1U;
  bool algorithm_valid = integrity.algorithm == protocol_plan::IntegrityAlgorithm::SUM8;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (integrity.algorithm == protocol_plan::IntegrityAlgorithm::CRC) {
    storage_width = static_cast<std::size_t>(integrity.crc_width / 8U);
    const std::uint32_t width_mask = integrity.crc_width == 16U ? 0xFFFFU : 0xFFFFFFFFU;
    algorithm_valid = (integrity.crc_width == 16U || integrity.crc_width == 32U) &&
                      integrity.crc_polynomial != 0U && (integrity.crc_polynomial & 1U) != 0U &&
                      (integrity.crc_polynomial & ~width_mask) == 0U &&
                      (integrity.crc_initial_value & ~width_mask) == 0U &&
                      (integrity.crc_xor_output & ~width_mask) == 0U &&
                      (integrity.storage_byte_order == ByteOrder::BIG ||
                       integrity.storage_byte_order == ByteOrder::LITTLE);
  }
#endif
  return algorithm_valid && integrity.range_length != 0U &&
         integrity.range_offset <= message.frame_size &&
         integrity.range_length <= message.frame_size - integrity.range_offset &&
         integrity.storage_offset <= message.frame_size &&
         storage_width <= message.frame_size - integrity.storage_offset &&
         !(integrity.storage_offset < integrity.range_offset + integrity.range_length &&
           integrity.range_offset < integrity.storage_offset + storage_width);
}

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
bool ComputedLengthDescriptorValid(const MessageExecutionPlan& message) noexcept {
  if (!message.computed_length.has_value()) {
    for (const FieldExecutionPlan& field : message.fields) {
      if (field.encode_source == EncodeSource::COMPUTED) return false;
    }
    return true;
  }
  const FrozenComputedLengthPlan& computed = *message.computed_length;
  if (computed.field_index >= message.fields.size()) return false;
  const FieldExecutionPlan& field = message.fields[computed.field_index];
  const bool width_valid =
      computed.storage_width == 1U || computed.storage_width == 2U || computed.storage_width == 4U;
  const bool order_valid =
      computed.storage_width == 1U
          ? computed.byte_order == ByteOrder::NOT_APPLICABLE
          : (computed.byte_order == ByteOrder::BIG || computed.byte_order == ByteOrder::LITTLE);
  const bool frame_scope = computed.scope == ComputedLengthScope::FRAME;
  const bool region_scope = computed.scope == ComputedLengthScope::REGION;
  const std::size_t expected = frame_scope ? message.frame_size : computed.range_length;
  return width_valid && order_valid && computed.storage_offset <= message.frame_size &&
         computed.storage_width <= message.frame_size - computed.storage_offset &&
         (frame_scope || region_scope) &&
         (!frame_scope || (computed.range_offset == 0U && computed.range_length == 0U)) &&
         (!region_scope ||
          (computed.range_length != 0U && computed.range_offset <= message.frame_size &&
           computed.range_length <= message.frame_size - computed.range_offset)) &&
         computed.expected_value == expected &&
         FitsUnsignedWidth(expected, computed.storage_width) &&
         field.value_type == ValueType::UINT64 && field.encode_source == EncodeSource::COMPUTED &&
         field.offset == computed.storage_offset && field.width == computed.storage_width &&
         field.byte_order == computed.byte_order && !field.has_constant &&
         field.bit_container_index == kInvalidIndex;
}

bool ComputedLengthMatches(const MessageExecutionPlan& message, const std::uint8_t* data,
                           std::uint64_t& actual PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  if (!message.computed_length.has_value()) return true;
  const FrozenComputedLengthPlan& computed = *message.computed_length;
  PAE_INCREMENT_OPERATION_COUNT(computed_length_fields_verified);
  return LoadUnsigned(data + computed.storage_offset, computed.storage_width, computed.byte_order,
                      actual) &&
         actual == computed.expected_value;
}
#endif

std::uint8_t Sum8(const std::uint8_t* data, std::size_t offset, std::size_t length,
                  bool review PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  std::uint8_t sum = 0U;
  for (std::size_t index = 0U; index < length; ++index) {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
    if (review) {
      ++counts.integrity_bytes_verified;
    } else {
      ++counts.integrity_bytes_accumulated;
    }
#else
    static_cast<void>(review);
#endif
    sum = static_cast<std::uint8_t>(sum + data[offset + index]);
  }
  return sum;
}

#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
std::uint32_t ReverseLowBits(std::uint32_t value, std::uint8_t width) noexcept {
  std::uint32_t reversed = 0U;
  for (std::uint8_t index = 0U; index < width; ++index) {
    reversed = static_cast<std::uint32_t>((reversed << 1U) | (value & 1U));
    value >>= 1U;
  }
  return reversed;
}

std::uint32_t Crc(const FrozenIntegrityPlan& integrity, const std::uint8_t* data,
                  bool review PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  const std::uint32_t mask = integrity.crc_width == 16U ? 0xFFFFU : 0xFFFFFFFFU;
  const std::uint32_t top_bit = std::uint32_t{1U} << (integrity.crc_width - 1U);
  std::uint32_t value = integrity.crc_initial_value;
  for (std::size_t index = 0U; index < integrity.range_length; ++index) {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
    if (review) {
      ++counts.integrity_bytes_verified;
    } else {
      ++counts.integrity_bytes_accumulated;
    }
#else
    static_cast<void>(review);
#endif
    std::uint32_t input = data[integrity.range_offset + index];
    if (integrity.crc_reflect_input) {
      input = ReverseLowBits(input, 8U);
    }
    value ^= input << (integrity.crc_width - 8U);
    for (std::uint8_t bit = 0U; bit < 8U; ++bit) {
      const bool high = (value & top_bit) != 0U;
      value = static_cast<std::uint32_t>((value << 1U) & mask);
      if (high) {
        value ^= integrity.crc_polynomial;
      }
    }
  }
  if (integrity.crc_reflect_output) {
    value = ReverseLowBits(value, integrity.crc_width);
  }
  return static_cast<std::uint32_t>((value ^ integrity.crc_xor_output) & mask);
}
#endif

bool IntegrityMatches(const MessageExecutionPlan& message,
                      const std::uint8_t* data PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  if (!IntegrityDescriptorValid(message)) {
    return false;
  }
  if (!message.integrity.has_value()) {
    return true;
  }
  const auto& integrity = *message.integrity;
  if (integrity.algorithm == protocol_plan::IntegrityAlgorithm::SUM8) {
    return Sum8(data, static_cast<std::size_t>(integrity.range_offset),
                static_cast<std::size_t>(integrity.range_length),
                true PAE_OPERATION_COUNTS_ARGUMENT) == data[integrity.storage_offset];
  }
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  std::uint64_t stored = 0U;
  const std::size_t storage_width = static_cast<std::size_t>(integrity.crc_width / 8U);
  return LoadUnsigned(data + integrity.storage_offset, storage_width, integrity.storage_byte_order,
                      stored) &&
         Crc(integrity, data, true PAE_OPERATION_COUNTS_ARGUMENT) == stored;
#else
  return false;
#endif
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
    case ValueType::INT64:
      return LogicalValueKind::INT64;
    case ValueType::BYTES:
      return LogicalValueKind::BYTES;
    case ValueType::ENUM:
      return LogicalValueKind::ENUM;
    case ValueType::BOOL:
      return LogicalValueKind::BOOL;
  }
  return LogicalValueKind::UINT64;
}

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
bool HasConversion(const FieldExecutionPlan& field) noexcept {
  return field.conversion_index != kInvalidIndex;
}

ConversionError ToConversionError(detail::DecimalArithmeticStatus status) noexcept {
  switch (status) {
    case detail::DecimalArithmeticStatus::DECIMAL_SCALE_OUT_OF_RANGE:
      return ConversionError::DECIMAL_SCALE_OUT_OF_RANGE;
    case detail::DecimalArithmeticStatus::RAW_NOT_INTEGRAL:
      return ConversionError::RAW_NOT_INTEGRAL;
    case detail::DecimalArithmeticStatus::RAW_OUT_OF_RANGE:
      return ConversionError::RAW_OUT_OF_RANGE;
    case detail::DecimalArithmeticStatus::LOGICAL_OUT_OF_RANGE:
      return ConversionError::LOGICAL_OUT_OF_RANGE;
    case detail::DecimalArithmeticStatus::OK:
    case detail::DecimalArithmeticStatus::INTERNAL_ERROR:
      return ConversionError::NONE;
  }
  return ConversionError::NONE;
}

CodecStatus ToCodecStatus(detail::DecimalArithmeticStatus status) noexcept {
  switch (status) {
    case detail::DecimalArithmeticStatus::DECIMAL_SCALE_OUT_OF_RANGE:
      return CodecStatus::INVALID_ARGUMENT;
    case detail::DecimalArithmeticStatus::INTERNAL_ERROR:
      return CodecStatus::INTERNAL_ERROR;
    case detail::DecimalArithmeticStatus::RAW_NOT_INTEGRAL:
    case detail::DecimalArithmeticStatus::RAW_OUT_OF_RANGE:
    case detail::DecimalArithmeticStatus::LOGICAL_OUT_OF_RANGE:
      return CodecStatus::VALUE_NOT_REPRESENTABLE;
    case detail::DecimalArithmeticStatus::OK:
      return CodecStatus::OK;
  }
  return CodecStatus::INTERNAL_ERROR;
}

bool ConversionPlanValid(const PlanBundle& plan, const FieldExecutionPlan& field) noexcept {
  if (!HasConversion(field)) return true;
  return (plan.SchemaVersion() == "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
          || plan.SchemaVersion() == "0.6"
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
          || plan.SchemaVersion() == "0.7"
#endif
          ) &&
         field.conversion_index < plan.Conversions().size() &&
         field.conversion_slot != kInvalidIndex && field.bit_container_index == kInvalidIndex &&
         field.encode_source == EncodeSource::INPUT &&
         plan.Conversions()[field.conversion_index].raw_value_type == field.value_type;
}

detail::DecimalArithmeticStatus ConvertEncode(
    const protocol_plan::LinearConversionDescriptor& conversion, Decimal64 logical,
    std::size_t width, std::uint64_t& raw) noexcept {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  if (g_fail_next_decimal_conversion.exchange(false, std::memory_order_relaxed)) {
    return detail::DecimalArithmeticStatus::INTERNAL_ERROR;
  }
#endif
  return detail::EncodeDecimal(conversion, logical, width, raw);
}

detail::DecimalArithmeticStatus ConvertDecode(
    const protocol_plan::LinearConversionDescriptor& conversion, std::uint64_t raw,
    std::size_t width, Decimal64& logical) noexcept {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  if (g_fail_next_decimal_conversion.exchange(false, std::memory_order_relaxed)) {
    return detail::DecimalArithmeticStatus::INTERNAL_ERROR;
  }
#endif
  return detail::DecodeDecimal(conversion, raw, width, logical);
}
#endif

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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                                std::vector<std::uint64_t>& conversion_raw_values,
#endif
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
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      if (field.encode_source == EncodeSource::COMPUTED) {
        return CodecStatus::COMPUTED_FIELD_OVERRIDE;
      }
#endif
      return CodecStatus::CONSTANT_FIELD_OVERRIDE;
    }
    if (field.input_ordinal >= message.required_input_count) {
      return CodecStatus::INVALID_PLAN;
    }
    if (IsPresent(present_words, field.input_ordinal)) {
      return CodecStatus::DUPLICATE_FIELD;
    }
    const bool defer_value_validation = plan.SchemaVersion() == "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
                                        || plan.SchemaVersion() == "0.6"
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                                        || plan.SchemaVersion() == "0.7"
#endif
        ;
    if (!defer_value_validation && !ValueKindMatches(value.value_kind, field.value_type)) {
      return CodecStatus::TYPE_MISMATCH;
    }

    if (!defer_value_validation && field.value_type == ValueType::UINT64) {
      if (!(field.bit_container_index != kInvalidIndex
                ? FitsUnsignedBits(value.uint64_value, field.bit_width)
                : FitsUnsignedWidth(value.uint64_value, field.width))) {
        return CodecStatus::VALUE_NOT_REPRESENTABLE;
      }
    } else if (!defer_value_validation && field.value_type == ValueType::INT64) {
      if (!FitsSignedWidth(value.int64_value, field.width)) {
        return CodecStatus::VALUE_NOT_REPRESENTABLE;
      }
    } else if (!defer_value_validation && field.value_type == ValueType::BYTES) {
      if (value.bytes_value.size != field.width) {
        return CodecStatus::BYTES_LENGTH_MISMATCH;
      }
      AddressRange unused;
      if (!GetAddressRange(value.bytes_value.data, value.bytes_value.size, unused)) {
        return CodecStatus::INVALID_ARGUMENT;
      }
    } else if (!defer_value_validation && field.value_type == ValueType::BOOL) {
      // Native bool has no alternate numeric or string representation in the Core API.
    } else if (!defer_value_validation &&
               (value.enum_value.plan_scope != &plan ||
                value.enum_value.message_index != message_index ||
                value.enum_value.field_index != value.field.field_index ||
                value.enum_value.entry_index >= field.enum_values_count)) {
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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (plan.SchemaVersion() == "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      || plan.SchemaVersion() == "0.6"
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      || plan.SchemaVersion() == "0.7"
#endif
  ) {
    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      const FieldExecutionPlan& field = message.fields[field_index];
      if (field.encode_source != EncodeSource::INPUT) continue;
      result.failed_field_index = field_index;
      const std::size_t value_index = value_indices[field.input_ordinal];
      result.failed_value_index = value_index;
      const EncodeFieldValue& value = values[value_index];
      if (!ConversionPlanValid(plan, field)) return CodecStatus::INVALID_PLAN;
      if (HasConversion(field)) {
        if (field.conversion_slot >= conversion_raw_values.size()) return CodecStatus::INVALID_PLAN;
        if (value.value_kind != LogicalValueKind::DECIMAL64) return CodecStatus::TYPE_MISMATCH;
        PAE_INCREMENT_OPERATION_COUNT(decimal_conversion_visits);
        const detail::DecimalArithmeticStatus status =
            ConvertEncode(plan.Conversions()[field.conversion_index], value.decimal64_value,
                          field.width, conversion_raw_values[field.conversion_slot]);
        if (status != detail::DecimalArithmeticStatus::OK) {
          result.conversion_error = ToConversionError(status);
          return ToCodecStatus(status);
        }
        continue;
      }
      if (!ValueKindMatches(value.value_kind, field.value_type)) return CodecStatus::TYPE_MISMATCH;
      if (field.value_type == ValueType::UINT64) {
        if (!(field.bit_container_index != kInvalidIndex
                  ? FitsUnsignedBits(value.uint64_value, field.bit_width)
                  : FitsUnsignedWidth(value.uint64_value, field.width))) {
          return CodecStatus::VALUE_NOT_REPRESENTABLE;
        }
      } else if (field.value_type == ValueType::INT64) {
        if (!FitsSignedWidth(value.int64_value, field.width)) {
          return CodecStatus::VALUE_NOT_REPRESENTABLE;
        }
      } else if (field.value_type == ValueType::BYTES) {
        if (value.bytes_value.size != field.width) return CodecStatus::BYTES_LENGTH_MISMATCH;
        AddressRange unused;
        if (!GetAddressRange(value.bytes_value.data, value.bytes_value.size, unused)) {
          return CodecStatus::INVALID_ARGUMENT;
        }
      } else if (field.value_type == ValueType::ENUM &&
                 (value.enum_value.plan_scope != &plan ||
                  value.enum_value.message_index != message_index ||
                  value.enum_value.field_index != field_index ||
                  value.enum_value.entry_index >= field.enum_values_count)) {
        return CodecStatus::ENUM_REFERENCE_MISMATCH;
      }
    }
  }
#endif
  result.failed_field_index = kInvalidIndex;
  result.failed_value_index = kInvalidIndex;
  return CodecStatus::OK;
}

std::uint64_t GetEncodeRawValue(const MessageExecutionPlan& message,
                                const FieldExecutionPlan& field, const EncodeFieldValue* values,
                                const std::vector<std::size_t>& value_indices
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                                ,
                                const std::vector<std::uint64_t>& conversion_raw_values
#endif
                                ) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (HasConversion(field)) return conversion_raw_values[field.conversion_slot];
#endif
  if (field.value_type == ValueType::INT64) {
    const std::int64_t signed_value = field.encode_source == EncodeSource::CONSTANT
                                          ? field.signed_constant_value
                                          : values[value_indices[field.input_ordinal]].int64_value;
    std::uint64_t raw = static_cast<std::uint64_t>(signed_value);
    if (field.width < 8U) {
      raw &= (std::uint64_t{1U} << static_cast<unsigned>(field.width * 8U)) - 1U;
    }
    return raw;
  }
  if (field.encode_source == EncodeSource::CONSTANT) {
    return field.constant_value;
  }
  const EncodeFieldValue& value = values[value_indices[field.input_ordinal]];
  if (field.value_type == ValueType::ENUM) {
    return message.enum_raw_values[field.enum_values_begin + value.enum_value.entry_index];
  }
  if (field.value_type == ValueType::BOOL) {
    return value.bool_value ? 1U : 0U;
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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                 const std::vector<std::uint64_t>& conversion_raw_values,
#endif
                 std::vector<std::uint64_t>& container_values,
                 std::uint8_t* output PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  if (message.bit_containers.size() > container_values.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < message.bit_containers.size(); ++index) {
    container_values[index] = message.bit_containers[index].base_value;
  }
  for (const FieldExecutionPlan& field : message.fields) {
    PAE_INCREMENT_OPERATION_COUNT(field_write_visits);
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (field.encode_source == EncodeSource::COMPUTED) {
      continue;
    }
#endif
    if (field.value_type == ValueType::BYTES) {
      const ByteView bytes = values[value_indices[field.input_ordinal]].bytes_value;
      for (std::size_t index = 0U; index < field.width; ++index) {
        PAE_INCREMENT_OPERATION_COUNT(bytes_write_visits);
        output[field.offset + index] = bytes.data[index];
      }
      continue;
    }
    const std::uint64_t raw_value = GetEncodeRawValue(message, field, values, value_indices
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                                                      ,
                                                      conversion_raw_values
#endif
    );
    if (field.bit_container_index != kInvalidIndex) {
      std::uint64_t& container = container_values[field.bit_container_index];
      container = (container & ~field.bit_mask) | ((raw_value << field.bit_shift) & field.bit_mask);
      continue;
    }
    if (!StoreUnsigned(raw_value, output + field.offset, field.width, field.byte_order)) {
      return false;
    }
  }
  for (std::size_t index = 0U; index < message.bit_containers.size(); ++index) {
    const auto& container = message.bit_containers[index];
    if (!StoreUnsigned(container_values[index], output + container.offset, container.width,
                       container.byte_order)) {
      return false;
    }
  }
  return true;
}

enum class FieldVerificationStatus {
  MATCH,
  MISMATCH,
  INTERNAL_ERROR,
};

FieldVerificationStatus VerifyFields(
    const MessageExecutionPlan& message, const EncodeFieldValue* values,
    const std::vector<std::size_t>& value_indices,
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    const PlanBundle& plan, const std::vector<std::uint64_t>& conversion_raw_values,
#endif
    const std::uint8_t* output,
    std::size_t& failed_field_index PAE_OPERATION_COUNTS_PARAMETER) noexcept {
  for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
    PAE_INCREMENT_OPERATION_COUNT(field_verify_visits);
    const FieldExecutionPlan& field = message.fields[field_index];
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (field.encode_source == EncodeSource::COMPUTED) {
      std::uint64_t actual = 0U;
      if (!message.computed_length.has_value() ||
          message.computed_length->field_index != field_index ||
          !LoadUnsigned(output + field.offset, field.width, field.byte_order, actual) ||
          actual != message.computed_length->expected_value) {
        failed_field_index = field_index;
        return FieldVerificationStatus::MISMATCH;
      }
      continue;
    }
#endif
    if (field.value_type == ValueType::BYTES) {
      const ByteView bytes = values[value_indices[field.input_ordinal]].bytes_value;
      for (std::size_t index = 0U; index < field.width; ++index) {
        PAE_INCREMENT_OPERATION_COUNT(bytes_verify_visits);
        if (output[field.offset + index] != bytes.data[index]) {
          failed_field_index = field_index;
          return FieldVerificationStatus::MISMATCH;
        }
      }
      continue;
    }
    std::uint64_t actual = 0U;
    const std::uint64_t expected = GetEncodeRawValue(message, field, values, value_indices
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                                                     ,
                                                     conversion_raw_values
#endif
    );
    if (field.bit_container_index != kInvalidIndex) {
      const auto& container = message.bit_containers[field.bit_container_index];
      std::uint64_t container_value = 0U;
      if (!LoadUnsigned(output + container.offset, container.width, container.byte_order,
                        container_value)) {
        failed_field_index = field_index;
        return FieldVerificationStatus::MISMATCH;
      }
      actual = ExtractBitfield(container_value, field);
    } else if (!LoadUnsigned(output + field.offset, field.width, field.byte_order, actual)) {
      failed_field_index = field_index;
      return FieldVerificationStatus::MISMATCH;
    }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    if (HasConversion(field)) {
      Decimal64 actual_logical;
      PAE_INCREMENT_OPERATION_COUNT(decimal_conversion_visits);
      detail::DecimalArithmeticStatus conversion_status;
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
      if (g_fail_next_decimal_final_review.exchange(false, std::memory_order_relaxed)) {
        conversion_status = detail::DecimalArithmeticStatus::INTERNAL_ERROR;
      } else
#endif
      {
        conversion_status = ConvertDecode(plan.Conversions()[field.conversion_index], actual,
                                          field.width, actual_logical);
      }
      if (conversion_status == detail::DecimalArithmeticStatus::INTERNAL_ERROR) {
        failed_field_index = field_index;
        return FieldVerificationStatus::INTERNAL_ERROR;
      }
      if (conversion_status != detail::DecimalArithmeticStatus::OK ||
          !detail::DecimalEqual(actual_logical,
                                values[value_indices[field.input_ordinal]].decimal64_value)) {
        failed_field_index = field_index;
        return FieldVerificationStatus::MISMATCH;
      }
    } else
#endif
        if (field.value_type == ValueType::INT64) {
      std::int64_t signed_actual = 0;
      const std::int64_t signed_expected =
          field.encode_source == EncodeSource::CONSTANT
              ? field.signed_constant_value
              : values[value_indices[field.input_ordinal]].int64_value;
      if (!InterpretSigned(actual, field.width, signed_actual) ||
          signed_actual != signed_expected) {
        failed_field_index = field_index;
        return FieldVerificationStatus::MISMATCH;
      }
    } else if (actual != expected) {
      failed_field_index = field_index;
      return FieldVerificationStatus::MISMATCH;
    }
  }
  return FieldVerificationStatus::MATCH;
}

}  // namespace

bool internal::SupportsCompleteRecordSchema(std::string_view schema_version) noexcept {
  if (schema_version == "0.1" || schema_version == "0.2" || schema_version == "0.3" ||
      schema_version == "0.4") {
    return true;
  }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (schema_version == "0.5") return true;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (schema_version == "0.6") return true;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (schema_version == "0.7") return true;
#endif
#endif
  return false;
}

internal::StructuralMatchResult internal::MatchCompleteRecordStructure(const PlanBundle& plan,
                                                                       std::size_t pipeline_index,
                                                                       ByteView input) noexcept {
  StructuralMatchResult result;
  if (!SupportsCompleteRecordSchema(plan.SchemaVersion())) {
    result.status = CodecStatus::INVALID_PLAN;
    return result;
  }
  if (input.data == nullptr && input.size != 0U) {
    return result;
  }
  const auto& pipelines = plan.PipelineExecutionPlans();
  if (pipeline_index >= pipelines.size()) {
    return result;
  }
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  CodecOperationCounts counts;
#endif
  const MatchOutcome match = FindPipelineMatch(
      pipelines[pipeline_index], plan.MessageExecutionPlans(), input PAE_OPERATION_COUNTS_ARGUMENT);
  if (match.match_count == 0U) {
    result.status = CodecStatus::UNKNOWN_MESSAGE;
  } else if (match.match_count != 1U) {
    result.status = CodecStatus::AMBIGUOUS_MESSAGE;
  } else {
    result.status = CodecStatus::OK;
    result.message_index = match.message_index;
  }
  return result;
}

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
                            std::uint64_t{0U}),
      bit_container_values_(plan.GetExecutionResourceLayout().bit_container_value_count,
                            std::uint64_t{0U})
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      ,
      decode_decimal_coefficients_(plan.GetExecutionResourceLayout().conversion_value_count, 0),
      decode_decimal_scales_(plan.GetExecutionResourceLayout().conversion_value_count, 0),
      raw_integer_bits_(plan.GetExecutionResourceLayout().conversion_value_count, 0U),
      raw_integer_kinds_(plan.GetExecutionResourceLayout().conversion_value_count,
                         ValueType::UINT64),
      raw_integer_field_indices_(plan.GetExecutionResourceLayout().conversion_value_count,
                                 kInvalidIndex),
      encode_conversion_raw_values_(plan.GetExecutionResourceLayout().conversion_value_count, 0U)
#endif
{
}

CodecOperationCounts ExecutionWorkspace::LastOperationCounts() const noexcept {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  return operation_counts_;
#else
  return CodecOperationCounts{};
#endif
}

std::size_t ExecutionWorkspace::LastRawIntegerCount() const noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  return raw_integer_count_;
#else
  return 0U;
#endif
}

bool ExecutionWorkspace::GetLastRawInteger(std::size_t index,
                                           RawIntegerValue& output) const noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (index >= raw_integer_count_) return false;
  const std::size_t field_index = raw_integer_field_indices_[index];
  if (raw_integer_kinds_[index] != ValueType::UINT64 &&
      raw_integer_kinds_[index] != ValueType::INT64) {
    return false;
  }
  output = RawIntegerValue{};
  output.field = FieldRef{plan_scope_, raw_integer_message_index_, field_index};
  output.kind = raw_integer_kinds_[index] == ValueType::INT64 ? RawIntegerKind::INT64
                                                              : RawIntegerKind::UINT64;
  if (output.kind == RawIntegerKind::UINT64) {
    output.uint64_value = raw_integer_bits_[index];
  } else {
    const std::size_t width =
        plan_scope_->MessageExecutionPlans()[raw_integer_message_index_].fields[field_index].width;
    InterpretSigned(raw_integer_bits_[index], width, output.int64_value);
  }
  return true;
#else
  static_cast<void>(index);
  static_cast<void>(output);
  return false;
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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  workspace.raw_integer_count_ = 0U;
  workspace.raw_integer_message_index_ = kInvalidIndex;
#endif
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  workspace.operation_counts_ = CodecOperationCounts{};
  CodecOperationCounts& counts = workspace.operation_counts_;
#endif
  if (workspace.plan_scope_ != &plan) {
    result.status = CodecStatus::WORKSPACE_PLAN_MISMATCH;
    return result;
  }
  if (!internal::SupportsCompleteRecordSchema(plan.SchemaVersion())) {
    result.status = CodecStatus::INVALID_PLAN;
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

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (!ComputedLengthDescriptorValid(message)) {
    result.status = CodecStatus::INVALID_PLAN;
    return result;
  }
  std::uint64_t actual_length = 0U;
  if (!ComputedLengthMatches(message, input.data, actual_length PAE_OPERATION_COUNTS_ARGUMENT)) {
    result.status = CodecStatus::LENGTH_MISMATCH;
    if (message.computed_length.has_value()) {
      result.failed_field_index = message.computed_length->field_index;
    }
    return result;
  }
#endif

  if (!IntegrityDescriptorValid(message)) {
    result.status = CodecStatus::INVALID_PLAN;
    return result;
  }
  if (!IntegrityMatches(message, input.data PAE_OPERATION_COUNTS_ARGUMENT)) {
    result.status = CodecStatus::INTEGRITY_FAILED;
    return result;
  }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t converted_field_count = 0U;
#endif

  if (message.bit_containers.size() > workspace.bit_container_values_.size()) {
    result.status = CodecStatus::INVALID_PLAN;
    return result;
  }
  for (std::size_t index = 0U; index < message.bit_containers.size(); ++index) {
    const auto& container = message.bit_containers[index];
    if (!LoadUnsigned(input.data + container.offset, container.width, container.byte_order,
                      workspace.bit_container_values_[index])) {
      result.status = CodecStatus::INVALID_PLAN;
      return result;
    }
  }

  bool tainted = false;
  for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
    PAE_INCREMENT_OPERATION_COUNT(field_validation_visits);
    const FieldExecutionPlan& field = message.fields[field_index];
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    if (plan.SchemaVersion() == "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
        || plan.SchemaVersion() == "0.6"
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
        || plan.SchemaVersion() == "0.7"
#endif
    ) {
      if (!ConversionPlanValid(plan, field)) {
        result.status = CodecStatus::INVALID_PLAN;
        result.failed_field_index = field_index;
        return result;
      }
      if (HasConversion(field)) {
        if (field.conversion_slot >= workspace.decode_decimal_coefficients_.size() ||
            converted_field_count >= workspace.raw_integer_bits_.size()) {
          result.status = CodecStatus::INVALID_PLAN;
          result.failed_field_index = field_index;
          return result;
        }
        std::uint64_t raw_value = 0U;
        if (!LoadUnsigned(input.data + field.offset, field.width, field.byte_order, raw_value)) {
          result.status = CodecStatus::INVALID_PLAN;
          result.failed_field_index = field_index;
          return result;
        }
        Decimal64 logical;
        PAE_INCREMENT_OPERATION_COUNT(decimal_conversion_visits);
        const detail::DecimalArithmeticStatus status = ConvertDecode(
            plan.Conversions()[field.conversion_index], raw_value, field.width, logical);
        if (status != detail::DecimalArithmeticStatus::OK) {
          result.status = ToCodecStatus(status);
          result.conversion_error = ToConversionError(status);
          result.failed_field_index = field_index;
          return result;
        }
        workspace.decode_decimal_coefficients_[field.conversion_slot] = logical.coefficient;
        workspace.decode_decimal_scales_[field.conversion_slot] = logical.scale;
        workspace.raw_integer_bits_[converted_field_count] = raw_value;
        workspace.raw_integer_kinds_[converted_field_count] = field.value_type;
        workspace.raw_integer_field_indices_[converted_field_count] = field_index;
        ++converted_field_count;
        continue;
      }
    }
#endif
    if (field.value_type != ValueType::ENUM) {
      continue;
    }
    std::uint64_t raw_value = 0U;
    if (field.bit_container_index != kInvalidIndex) {
      raw_value =
          ExtractBitfield(workspace.bit_container_values_[field.bit_container_index], field);
    } else if (!LoadUnsigned(input.data + field.offset, field.width, field.byte_order, raw_value)) {
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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    if (HasConversion(field)) {
      slot.value_kind = LogicalValueKind::DECIMAL64;
      slot.decimal64_value =
          Decimal64{workspace.decode_decimal_coefficients_[field.conversion_slot],
                    workspace.decode_decimal_scales_[field.conversion_slot]};
      field_slots[field_index] = slot;
      continue;
    }
#endif
    if (field.value_type == ValueType::BYTES) {
      slot.bytes_value = ByteView{input.data + field.offset, field.width};
    } else {
      std::uint64_t raw_value = 0U;
      if (field.bit_container_index != kInvalidIndex) {
        raw_value =
            ExtractBitfield(workspace.bit_container_values_[field.bit_container_index], field);
      } else if (!LoadUnsigned(input.data + field.offset, field.width, field.byte_order,
                               raw_value)) {
        result.status = CodecStatus::INVALID_PLAN;
        result.failed_field_index = field_index;
        return result;
      }
      if (field.value_type == ValueType::UINT64) {
        slot.uint64_value = raw_value;
      } else if (field.value_type == ValueType::INT64) {
        if (!InterpretSigned(raw_value, field.width, slot.int64_value)) {
          result.status = CodecStatus::INVALID_PLAN;
          result.failed_field_index = field_index;
          return result;
        }
      } else if (field.value_type == ValueType::BOOL) {
        slot.bool_value = raw_value != 0U;
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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  workspace.raw_integer_count_ = converted_field_count;
  workspace.raw_integer_message_index_ = match.message_index;
#endif
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
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  workspace.raw_integer_count_ = 0U;
  workspace.raw_integer_message_index_ = kInvalidIndex;
#endif
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  workspace.operation_counts_ = CodecOperationCounts{};
  CodecOperationCounts& counts = workspace.operation_counts_;
#endif
  if (workspace.plan_scope_ != &plan) {
    result.status = CodecStatus::WORKSPACE_PLAN_MISMATCH;
    return result;
  }
  if (!internal::SupportsCompleteRecordSchema(plan.SchemaVersion())) {
    result.status = CodecStatus::INVALID_PLAN;
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
  const CodecStatus value_status =
      PrepareEncodeInputs(plan, message, message_index, values, value_count,
                          workspace.encode_value_indices_, workspace.encode_present_words_,
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                          workspace.encode_conversion_raw_values_,
#endif
                          result PAE_OPERATION_COUNTS_ARGUMENT);
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

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (!ComputedLengthDescriptorValid(message)) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }
#endif

  WriteFixedBytes(message, output.data PAE_OPERATION_COUNTS_ARGUMENT);
  if (!WriteFields(message, values, workspace.encode_value_indices_,
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                   workspace.encode_conversion_raw_values_,
#endif
                   workspace.bit_container_values_, output.data PAE_OPERATION_COUNTS_ARGUMENT)) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (message.computed_length.has_value()) {
    const FrozenComputedLengthPlan& computed = *message.computed_length;
    PAE_INCREMENT_OPERATION_COUNT(computed_length_fields_generated);
    if (!StoreUnsigned(computed.expected_value, output.data + computed.storage_offset,
                       computed.storage_width, computed.byte_order)) {
      result.status = CodecStatus::FINAL_REVIEW_FAILED;
      result.failed_field_index = computed.field_index;
      return result;
    }
  }
#endif

  if (!IntegrityDescriptorValid(message)) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }
  if (message.integrity.has_value()) {
    const auto& integrity = *message.integrity;
    if (integrity.algorithm == protocol_plan::IntegrityAlgorithm::SUM8) {
      output.data[integrity.storage_offset] = Sum8(
          output.data, static_cast<std::size_t>(integrity.range_offset),
          static_cast<std::size_t>(integrity.range_length), false PAE_OPERATION_COUNTS_ARGUMENT);
    }
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    else {
      const std::size_t storage_width = static_cast<std::size_t>(integrity.crc_width / 8U);
      const std::uint32_t crc = Crc(integrity, output.data, false PAE_OPERATION_COUNTS_ARGUMENT);
      if (!StoreUnsigned(crc, output.data + integrity.storage_offset, storage_width,
                         integrity.storage_byte_order)) {
        result.status = CodecStatus::FINAL_REVIEW_FAILED;
        return result;
      }
    }
#endif
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
    if (g_corrupt_integrity_storage_before_final_review.exchange(false,
                                                                 std::memory_order_relaxed)) {
      output.data[integrity.storage_offset] ^= 0x01U;
    }
#endif
    if (!IntegrityMatches(message, output.data PAE_OPERATION_COUNTS_ARGUMENT)) {
      result.status = CodecStatus::FINAL_REVIEW_FAILED;
      return result;
    }
  }

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  if (message.computed_length.has_value() &&
      g_corrupt_computed_length_before_final_review.exchange(false, std::memory_order_relaxed)) {
    output.data[message.computed_length->storage_offset] ^= 0x01U;
  }
#endif
  std::uint64_t actual_length = 0U;
  if (!ComputedLengthMatches(message, output.data, actual_length PAE_OPERATION_COUNTS_ARGUMENT)) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    if (message.computed_length.has_value()) {
      result.failed_field_index = message.computed_length->field_index;
    }
    return result;
  }
#endif

#if defined(PAE_ENABLE_OPERATION_COUNTERS) && defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (g_corrupt_decimal_field_before_final_review.exchange(false, std::memory_order_relaxed)) {
    for (const FieldExecutionPlan& field : message.fields) {
      if (HasConversion(field)) {
        output.data[field.offset] ^= 0x01U;
        break;
      }
    }
  }
#endif

  const ByteView encoded{output.data, message.frame_size};
  const MatchOutcome final_match =
      FindPipelineMatch(pipeline, messages, encoded PAE_OPERATION_COUNTS_ARGUMENT);
  if (final_match.match_count != 1U || final_match.message_index != message_index) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }
  const FieldVerificationStatus verification_status =
      VerifyFields(message, values, workspace.encode_value_indices_,
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                   plan, workspace.encode_conversion_raw_values_,
#endif
                   output.data, result.failed_field_index PAE_OPERATION_COUNTS_ARGUMENT);
  if (verification_status == FieldVerificationStatus::INTERNAL_ERROR) {
    result.status = CodecStatus::INTERNAL_ERROR;
    return result;
  }
  if (verification_status == FieldVerificationStatus::MISMATCH) {
    result.status = CodecStatus::FINAL_REVIEW_FAILED;
    return result;
  }

  result.status = CodecStatus::OK;
  result.bytes_written = message.frame_size;
  result.failed_value_index = kInvalidIndex;
  result.failed_field_index = kInvalidIndex;
  return result;
}

#if defined(PAE_ENABLE_OPERATION_COUNTERS)
namespace test_only {

void CorruptIntegrityStorageBeforeFinalReviewOnce() noexcept {
  g_corrupt_integrity_storage_before_final_review.store(true, std::memory_order_relaxed);
}

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
void CorruptComputedLengthBeforeFinalReviewOnce() noexcept {
  g_corrupt_computed_length_before_final_review.store(true, std::memory_order_relaxed);
}
#endif

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
void CorruptDecimalFieldBeforeFinalReviewOnce() noexcept {
  g_corrupt_decimal_field_before_final_review.store(true, std::memory_order_relaxed);
}

void FailNextDecimalConversionOnce() noexcept {
  g_fail_next_decimal_conversion.store(true, std::memory_order_relaxed);
}

void FailNextDecimalFinalReviewOnce() noexcept {
  g_fail_next_decimal_final_review.store(true, std::memory_order_relaxed);
}
#endif

}  // namespace test_only
#endif

#undef PAE_INCREMENT_OPERATION_COUNT
#undef PAE_OPERATION_COUNTS_ARGUMENT
#undef PAE_OPERATION_COUNTS_PARAMETER

}  // namespace pae::protocol_core
