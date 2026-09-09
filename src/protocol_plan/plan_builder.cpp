#include "plan_builder.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <new>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "decimal_conversion_internal.h"
#include "plan_draft_internal.h"

namespace pae::protocol_plan {
namespace {

constexpr std::size_t kBitsPerAllowedMessageWord = 64U;
constexpr std::size_t kMaxStableIdCharacters = 128U;

struct ByteInterval {
  std::size_t begin = 0U;
  std::size_t end = 0U;
  std::size_t source_index = kInvalidPlanBuildIndex;
};

PlanBuildResult Reject(PlanBuildError code, std::size_t framing_index = kInvalidPlanBuildIndex,
                       std::size_t pipeline_index = kInvalidPlanBuildIndex,
                       std::size_t message_index = kInvalidPlanBuildIndex,
                       std::size_t matcher_index = kInvalidPlanBuildIndex,
                       std::size_t field_index = kInvalidPlanBuildIndex) noexcept {
  return PlanBuildResult::Failure(PlanBuildDiagnostic{code, framing_index, pipeline_index,
                                                      message_index, matcher_index, field_index});
}

PlanBuildResult RejectMemory(PlanBuildError code, std::size_t required_bytes,
                             std::size_t limit_bytes, ResourceProfile profile) noexcept {
  PlanBuildDiagnostic diagnostic;
  diagnostic.code = code;
  diagnostic.required_bytes = required_bytes;
  diagnostic.limit_bytes = limit_bytes;
  diagnostic.resource_profile = profile;
  return PlanBuildResult::Failure(diagnostic);
}

bool AddSizeChecked(std::size_t value, std::size_t& total) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)() - total) {
    return false;
  }
  total += value;
  return true;
}

bool MultiplySizeChecked(std::size_t left, std::size_t right, std::size_t& output) noexcept {
  if (left != 0U && right > (std::numeric_limits<std::size_t>::max)() / left) {
    return false;
  }
  output = left * right;
  return true;
}

bool IsStableId(std::string_view value) noexcept {
  if (value.empty() || value.size() > kMaxStableIdCharacters || value.front() < 'a' ||
      value.front() > 'z') {
    return false;
  }
  for (const char character : value) {
    const bool lowercase = character >= 'a' && character <= 'z';
    const bool digit = character >= '0' && character <= '9';
    if (!lowercase && !digit && character != '_') {
      return false;
    }
  }
  return true;
}

bool ToSize(std::uint64_t value, std::size_t& output) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)()) {
    return false;
  }
  output = static_cast<std::size_t>(value);
  return true;
}

bool IsRangeWithin(std::size_t offset, std::size_t width, std::size_t capacity) noexcept {
  return width != 0U && offset <= capacity && width <= capacity - offset;
}

bool FitsUnsignedWidth(std::uint64_t value, std::size_t width) noexcept {
  if (width == 0U || width > 8U) {
    return false;
  }
  if (width == 8U) {
    return true;
  }
  return value < (std::uint64_t{1U} << static_cast<unsigned>(width * 8U));
}

bool FitsUnsignedBits(std::uint64_t value, std::size_t width) noexcept {
  return width == 64U || (width != 0U && width < 64U &&
                          value < (std::uint64_t{1U} << static_cast<unsigned>(width)));
}

bool FitsSignedWidth(std::int64_t value, std::size_t width) noexcept {
  if (width == 0U || width > 8U) {
    return false;
  }
  if (width == 8U) {
    return true;
  }
  const unsigned magnitude_bits = static_cast<unsigned>(width * 8U - 1U);
  const std::int64_t minimum = -static_cast<std::int64_t>(std::uint64_t{1U} << magnitude_bits);
  const std::int64_t maximum =
      static_cast<std::int64_t>((std::uint64_t{1U} << magnitude_bits) - 1U);
  return value >= minimum && value <= maximum;
}

bool IsIntegerByteOrderValid(ByteOrder byte_order, std::size_t width) noexcept {
  if (width == 1U) {
    return byte_order == ByteOrder::NOT_APPLICABLE || byte_order == ByteOrder::BIG ||
           byte_order == ByteOrder::LITTLE;
  }
  return byte_order == ByteOrder::BIG || byte_order == ByteOrder::LITTLE;
}

bool IsBitContainerByteOrderValid(ByteOrder byte_order, std::size_t width) noexcept {
  return width == 1U ? byte_order == ByteOrder::NOT_APPLICABLE
                     : byte_order == ByteOrder::BIG || byte_order == ByteOrder::LITTLE;
}

bool IsBitNumberingValid(BitNumbering bit_numbering) noexcept {
  return bit_numbering == BitNumbering::LSB0 || bit_numbering == BitNumbering::MSB0;
}

std::uint64_t WidthMask(std::size_t width) noexcept {
  return width == 8U ? (std::numeric_limits<std::uint64_t>::max)()
                     : (std::uint64_t{1U} << static_cast<unsigned>(width * 8U)) - 1U;
}

std::uint8_t EncodeUnsignedByte(std::uint64_t value, std::size_t width, ByteOrder byte_order,
                                std::size_t byte_index) noexcept {
  const std::size_t encoded_index =
      byte_order == ByteOrder::BIG ? width - 1U - byte_index : byte_index;
  const auto shift = static_cast<unsigned>(encoded_index * 8U);
  return static_cast<std::uint8_t>((value >> shift) & 0xFFU);
}

bool RequirementsEqual(const ResourceRequirements& left,
                       const ResourceRequirements& right) noexcept {
  return left.max_frame_bytes == right.max_frame_bytes &&
         left.framing_profile_count == right.framing_profile_count &&
         left.pipeline_count == right.pipeline_count && left.message_count == right.message_count &&
         left.total_field_count == right.total_field_count &&
         left.total_matcher_count == right.total_matcher_count &&
         left.total_enum_entry_count == right.total_enum_entry_count &&
         left.total_bit_container_count == right.total_bit_container_count &&
         left.total_integrity_rule_count == right.total_integrity_rule_count
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
         && left.total_conversion_count == right.total_conversion_count
#endif
      ;
}

bool CoversWholeFrame(std::vector<ByteInterval>& intervals, std::size_t frame_size) {
  if (intervals.empty()) {
    return false;
  }
  std::sort(intervals.begin(), intervals.end(),
            [](const ByteInterval& left, const ByteInterval& right) {
              if (left.begin != right.begin) {
                return left.begin < right.begin;
              }
              return left.end < right.end;
            });
  std::size_t covered_end = 0U;
  for (const ByteInterval& interval : intervals) {
    if (interval.begin > covered_end) {
      return false;
    }
    covered_end = (std::max)(covered_end, interval.end);
  }
  return covered_end == frame_size;
}

bool FixedMatchersCanIntersect(const detail::PreparedMessageExecutionPlan& left,
                               const detail::PreparedMessageExecutionPlan& right) noexcept {
  if (left.frame_size != right.frame_size) {
    return false;
  }
  std::size_t left_index = 0U;
  std::size_t right_index = 0U;
  while (left_index < left.fixed_bytes.size() && right_index < right.fixed_bytes.size()) {
    const FixedByteExecutionPlan& left_byte = left.fixed_bytes[left_index];
    const FixedByteExecutionPlan& right_byte = right.fixed_bytes[right_index];
    if (left_byte.offset < right_byte.offset) {
      ++left_index;
    } else if (right_byte.offset < left_byte.offset) {
      ++right_index;
    } else {
      if (left_byte.value != right_byte.value) {
        return false;
      }
      ++left_index;
      ++right_index;
    }
  }
  return true;
}

bool AddStringLayout(PlanMemoryLayout& layout, std::string_view value) noexcept {
  return value.empty() || layout.AddArray<char>(value.size(), PlanMemoryCategory::STRING, nullptr);
}

bool EstimatePreparedPlanMemory(
    const detail::PlanDraftData& draft,
    const std::vector<detail::PreparedMessageExecutionPlan>& message_execution_plans,
    const std::vector<detail::PreparedPipelineExecutionPlan>& pipeline_execution_plans,
    std::size_t limit_bytes, PlanMemoryReport& output) noexcept {
  PlanMemoryLayout layout{limit_bytes};
  if (!layout.AddArray<PlanBundle>(1U, PlanMemoryCategory::OBJECT) ||
      !AddStringLayout(layout, draft.schema_version) ||
      !AddStringLayout(layout, draft.protocol_id) ||
      !AddStringLayout(layout, draft.protocol_version) ||
      !layout.AddArray<FrozenFramingPlan>(draft.framing_profiles.size(),
                                          PlanMemoryCategory::METADATA_CONTAINER)) {
    return false;
  }
  for (const FramingPlan& framing : draft.framing_profiles) {
    if (!AddStringLayout(layout, framing.id)) {
      return false;
    }
  }
  if (!layout.AddArray<FrozenPipelinePlan>(draft.pipelines.size(),
                                           PlanMemoryCategory::METADATA_CONTAINER)) {
    return false;
  }
  for (const PipelinePlan& pipeline : draft.pipelines) {
    if (!AddStringLayout(layout, pipeline.id) || !AddStringLayout(layout, pipeline.direction_id) ||
        !layout.AddArray<std::size_t>(pipeline.message_indices.size(),
                                      PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
  }
  if (!layout.AddArray<FrozenMessagePlan>(draft.messages.size(),
                                          PlanMemoryCategory::METADATA_CONTAINER)) {
    return false;
  }
  for (const MessagePlan& message : draft.messages) {
    if (!AddStringLayout(layout, message.id) || !AddStringLayout(layout, message.direction_id) ||
        !layout.AddArray<FrozenMatcherPlan>(message.matchers.size(),
                                            PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
    for (const MatcherPlan& matcher : message.matchers) {
      if (!layout.AddArray<std::uint8_t>(matcher.bytes.size(), PlanMemoryCategory::MATCHER)) {
        return false;
      }
    }
    if (!layout.AddArray<FrozenBitContainerPlan>(message.bit_containers.size(),
                                                 PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
    for (const BitContainerPlan& container : message.bit_containers) {
      if (!AddStringLayout(layout, container.id)) {
        return false;
      }
    }
    if (!layout.AddArray<FrozenFieldPlan>(message.fields.size(),
                                          PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
    for (const FieldPlan& field : message.fields) {
      if (!AddStringLayout(layout, field.id) ||
          !layout.AddArray<FrozenEnumEntryPlan>(field.enum_entries.size(),
                                                PlanMemoryCategory::METADATA_CONTAINER)) {
        return false;
      }
      for (const EnumEntryPlan& entry : field.enum_entries) {
        if (!AddStringLayout(layout, entry.id)) {
          return false;
        }
      }
    }
  }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (!layout.AddArray<LinearConversionDescriptor>(draft.conversions.size(),
                                                   PlanMemoryCategory::EXTENSION)) {
    return false;
  }
#endif
  if (!layout.AddArray<MessageExecutionPlan>(message_execution_plans.size(),
                                             PlanMemoryCategory::EXECUTION_DESCRIPTOR)) {
    return false;
  }
  for (const detail::PreparedMessageExecutionPlan& message : message_execution_plans) {
    if (!layout.AddArray<FixedByteExecutionPlan>(message.fixed_bytes.size(),
                                                 PlanMemoryCategory::MATCHER) ||
        !layout.AddArray<BitContainerExecutionPlan>(message.bit_containers.size(),
                                                    PlanMemoryCategory::EXECUTION_DESCRIPTOR) ||
        !layout.AddArray<FieldExecutionPlan>(message.fields.size(),
                                             PlanMemoryCategory::EXECUTION_DESCRIPTOR) ||
        !layout.AddArray<std::uint64_t>(message.enum_raw_values.size(),
                                        PlanMemoryCategory::INDEX) ||
        !layout.AddArray<EnumLookupExecutionPlan>(message.enum_lookup_entries.size(),
                                                  PlanMemoryCategory::INDEX)) {
      return false;
    }
  }
  if (!layout.AddArray<PipelineExecutionPlan>(pipeline_execution_plans.size(),
                                              PlanMemoryCategory::EXECUTION_DESCRIPTOR)) {
    return false;
  }
  for (const detail::PreparedPipelineExecutionPlan& pipeline : pipeline_execution_plans) {
    if (!layout.AddArray<std::uint64_t>(pipeline.allowed_message_words.size(),
                                        PlanMemoryCategory::INDEX) ||
        !layout.AddArray<CandidateGroupExecutionPlan>(pipeline.candidate_groups.size(),
                                                      PlanMemoryCategory::EXECUTION_DESCRIPTOR)) {
      return false;
    }
    for (const detail::PreparedCandidateGroupExecutionPlan& group : pipeline.candidate_groups) {
      if (!layout.AddArray<std::size_t>(group.message_indices.size(), PlanMemoryCategory::INDEX)) {
        return false;
      }
    }
  }
  output = layout.Report();
  return true;
}

bool FreezeString(PlanArena& arena, std::string_view source, FrozenString& output) noexcept {
  if (source.empty()) {
    output = FrozenString{};
    return true;
  }
  char* destination = arena.AllocateArray<char>(source.size(), PlanMemoryCategory::STRING);
  if (destination == nullptr) {
    return false;
  }
  std::memcpy(destination, source.data(), source.size());
  output = FrozenString{destination, source.size()};
  return true;
}

template <typename T, typename Factory>
bool FreezeObjectArray(PlanArena& arena, std::size_t count, PlanMemoryCategory category,
                       Factory&& factory, FrozenArray<T>& output) {
  if (count == 0U) {
    output = FrozenArray<T>{};
    return true;
  }
  T* destination = arena.AllocateArray<T>(count, category);
  if (destination == nullptr) {
    return false;
  }
  std::size_t constructed = 0U;
  for (; constructed < count; ++constructed) {
    T value;
    if (!factory(constructed, value)) {
      for (std::size_t rollback = constructed; rollback != 0U; --rollback) {
        destination[rollback - 1U].~T();
      }
      return false;
    }
    new (destination + constructed) T(std::move(value));
  }
  output = FrozenArray<T>::Adopt(destination, count);
  return true;
}

template <typename T>
bool FreezePodArray(PlanArena& arena, const std::vector<T>& source, PlanMemoryCategory category,
                    FrozenArray<T>& output) {
  return FreezeObjectArray<T>(
      arena, source.size(), category,
      [&source](std::size_t index, T& value) {
        value = source[index];
        return true;
      },
      output);
}

}  // namespace

BudgetedPlanDraft::BudgetedPlanDraft(std::unique_ptr<detail::PlanDraftData> draft) noexcept
    : draft_(std::move(draft)) {}

BudgetedPlanDraft::BudgetedPlanDraft(BudgetedPlanDraft&& other) noexcept = default;

BudgetedPlanDraft& BudgetedPlanDraft::operator=(BudgetedPlanDraft&& other) noexcept = default;

BudgetedPlanDraft::~BudgetedPlanDraft() = default;

PlanBuildResult PlanBuilder::FreezeImpl(detail::PlanDraftData draft) {
  const ResourceProfileLimits* limits = GetResourceProfileLimits(draft.resource_profile);
  if ((draft.schema_version != "0.1" && draft.schema_version != "0.2" &&
       draft.schema_version != "0.3" && draft.schema_version != "0.4"
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
       && draft.schema_version != "0.5"
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
       && draft.schema_version != "0.6"
#endif
       ) ||
      !IsStableId(draft.protocol_id) || draft.protocol_version.empty() || limits == nullptr ||
      draft.framing_profiles.empty() || draft.pipelines.empty() || draft.messages.empty()) {
    return Reject(PlanBuildError::INVALID_METADATA);
  }

  ResourceRequirements actual_requirements;
  actual_requirements.framing_profile_count = draft.framing_profiles.size();
  actual_requirements.pipeline_count = draft.pipelines.size();
  actual_requirements.message_count = draft.messages.size();
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  actual_requirements.total_conversion_count = draft.conversions.size();
  if (draft.conversions.size() > limits->max_total_fields) {
    return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED);
  }
  for (const LinearConversionDescriptor& conversion : draft.conversions) {
    LinearConversionDescriptor recomputed;
    if ((draft.schema_version != "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
         && draft.schema_version != "0.6"
#endif
         ) ||
        (conversion.raw_value_type != ValueType::UINT64 &&
         conversion.raw_value_type != ValueType::INT64) ||
        detail::DeriveLinearConversion(conversion.raw_value_type, conversion.scale_numerator,
                                       conversion.scale_denominator, conversion.bias_numerator,
                                       conversion.bias_denominator,
                                       recomputed) != detail::DecimalDescriptorError::NONE ||
        !detail::LinearConversionEqual(conversion, recomputed)) {
      return Reject(PlanBuildError::INVALID_FIELD_PLAN);
    }
  }
#endif
  if (actual_requirements.framing_profile_count > limits->max_framing_profiles ||
      actual_requirements.pipeline_count > limits->max_pipelines ||
      actual_requirements.message_count > limits->max_messages) {
    return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED);
  }
  for (std::size_t message_index = 0U; message_index < draft.messages.size(); ++message_index) {
    const MessagePlan& message = draft.messages[message_index];
    if (message.frame_length_bytes > limits->max_frame_bytes ||
        message.fields.size() > limits->max_fields_per_message ||
        message.bit_containers.size() > limits->max_fields_per_message ||
        message.matchers.size() > limits->max_matchers_per_message ||
        !AddSizeChecked(message.fields.size(), actual_requirements.total_field_count) ||
        !AddSizeChecked(message.matchers.size(), actual_requirements.total_matcher_count) ||
        !AddSizeChecked(message.bit_containers.size(),
                        actual_requirements.total_bit_container_count) ||
        (message.integrity.has_value() &&
         !AddSizeChecked(1U, actual_requirements.total_integrity_rule_count)) ||
        actual_requirements.total_field_count > limits->max_total_fields ||
        actual_requirements.total_bit_container_count > limits->max_total_fields ||
        actual_requirements.total_integrity_rule_count > limits->max_messages ||
        actual_requirements.total_matcher_count > limits->max_total_matchers) {
      return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED, kInvalidPlanBuildIndex,
                    kInvalidPlanBuildIndex, message_index);
    }
    actual_requirements.max_frame_bytes =
        (std::max)(actual_requirements.max_frame_bytes, message.frame_length_bytes);
    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      const std::size_t enum_entry_count = message.fields[field_index].enum_entries.size();
      if (enum_entry_count > limits->max_enum_entries_per_field ||
          !AddSizeChecked(enum_entry_count, actual_requirements.total_enum_entry_count) ||
          actual_requirements.total_enum_entry_count > limits->max_total_enum_entries) {
        return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
      }
    }
  }
  if (!RequirementsEqual(draft.resource_requirements, actual_requirements)) {
    return Reject(PlanBuildError::RESOURCE_REQUIREMENTS_MISMATCH);
  }

  std::unordered_set<std::string> framing_ids;
  framing_ids.reserve(draft.framing_profiles.size());
  for (std::size_t framing_index = 0U; framing_index < draft.framing_profiles.size();
       ++framing_index) {
    const FramingPlan& framing = draft.framing_profiles[framing_index];
    if (!IsStableId(framing.id) || framing.input_kind != InputKind::COMPLETE_RECORD ||
        !framing_ids.emplace(framing.id).second) {
      return Reject(PlanBuildError::INVALID_FRAMING_PLAN, framing_index);
    }
  }

  ExecutionResourceLayout execution_resource_layout;
  std::vector<detail::PreparedMessageExecutionPlan> message_execution_plans;
  message_execution_plans.reserve(draft.messages.size());
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::vector<std::size_t> conversion_reference_counts(draft.conversions.size(), 0U);
#endif
  std::unordered_set<std::string> message_ids;
  message_ids.reserve(draft.messages.size());

  for (std::size_t message_index = 0U; message_index < draft.messages.size(); ++message_index) {
    const MessagePlan& message = draft.messages[message_index];
    std::size_t frame_size = 0U;
    if (!IsStableId(message.id) || !IsStableId(message.direction_id) ||
        !message_ids.emplace(message.id).second ||
        !ToSize(message.frame_length_bytes, frame_size) || frame_size == 0U ||
        message.matchers.empty() || message.fields.empty()) {
      return Reject(PlanBuildError::INVALID_MESSAGE_PLAN, kInvalidPlanBuildIndex,
                    kInvalidPlanBuildIndex, message_index);
    }
    execution_resource_layout.max_fields_per_message =
        (std::max)(execution_resource_layout.max_fields_per_message, message.fields.size());

    detail::PreparedMessageExecutionPlan execution;
    execution.frame_size = frame_size;
    if (message.integrity.has_value()) {
      std::size_t range_offset = 0U;
      std::size_t range_length = 0U;
      std::size_t storage_offset = 0U;
      const IntegrityPlan& integrity = *message.integrity;
      std::size_t storage_width = 1U;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      const bool crc = integrity.algorithm == IntegrityAlgorithm::CRC;
      if (crc) {
        storage_width = static_cast<std::size_t>(integrity.crc_width / 8U);
      }
      const std::uint32_t crc_width_mask = integrity.crc_width == 16U ? 0xFFFFU : 0xFFFFFFFFU;
      const bool crc_valid = crc && draft.schema_version == "0.6" &&
                             (integrity.crc_width == 16U || integrity.crc_width == 32U) &&
                             integrity.crc_polynomial != 0U &&
                             (integrity.crc_polynomial & 1U) != 0U &&
                             (integrity.crc_polynomial & ~crc_width_mask) == 0U &&
                             (integrity.crc_initial_value & ~crc_width_mask) == 0U &&
                             (integrity.crc_xor_output & ~crc_width_mask) == 0U &&
                             (integrity.storage_byte_order == ByteOrder::BIG ||
                              integrity.storage_byte_order == ByteOrder::LITTLE);
#endif
      if ((draft.schema_version != "0.3" && draft.schema_version != "0.4"
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
           && draft.schema_version != "0.5"
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
           && draft.schema_version != "0.6"
#endif
           ) ||
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
          (integrity.algorithm != IntegrityAlgorithm::SUM8 && !crc_valid) ||
          (integrity.algorithm == IntegrityAlgorithm::SUM8 && draft.schema_version == "0.6" &&
           (integrity.crc_width != 0U ||
            integrity.storage_byte_order != ByteOrder::NOT_APPLICABLE)) ||
#else
          integrity.algorithm != IntegrityAlgorithm::SUM8 ||
#endif
          !ToSize(integrity.range_offset, range_offset) ||
          !ToSize(integrity.range_length, range_length) ||
          !ToSize(integrity.storage_offset, storage_offset) || range_length == 0U ||
          !IsRangeWithin(range_offset, range_length, frame_size) ||
          !IsRangeWithin(storage_offset, storage_width, frame_size) ||
          (storage_offset < range_offset + range_length &&
           range_offset < storage_offset + storage_width)) {
        return Reject(PlanBuildError::INVALID_MESSAGE_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index);
      }
      FrozenIntegrityPlan frozen_integrity;
      frozen_integrity.algorithm = integrity.algorithm;
      frozen_integrity.range_offset = range_offset;
      frozen_integrity.range_length = range_length;
      frozen_integrity.storage_offset = storage_offset;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      frozen_integrity.crc_width = integrity.crc_width;
      frozen_integrity.crc_polynomial = integrity.crc_polynomial;
      frozen_integrity.crc_initial_value = integrity.crc_initial_value;
      frozen_integrity.crc_xor_output = integrity.crc_xor_output;
      frozen_integrity.crc_reflect_input = integrity.crc_reflect_input;
      frozen_integrity.crc_reflect_output = integrity.crc_reflect_output;
      frozen_integrity.storage_byte_order = integrity.storage_byte_order;
#endif
      execution.integrity = frozen_integrity;
    }
    execution.fields.reserve(message.fields.size());
    std::unordered_set<std::string> field_ids;
    field_ids.reserve(message.fields.size());
    std::vector<ByteInterval> field_intervals;
    field_intervals.reserve(message.fields.size() + message.bit_containers.size());
    execution.bit_containers.reserve(message.bit_containers.size());
    std::unordered_set<std::string> container_ids;
    std::vector<std::uint64_t> member_masks(message.bit_containers.size(), 0U);
    std::vector<std::uint64_t> determined_masks(message.bit_containers.size(), 0U);
    std::vector<std::uint64_t> determined_values(message.bit_containers.size(), 0U);
    std::vector<std::size_t> member_counts(message.bit_containers.size(), 0U);
    for (std::size_t container_index = 0U; container_index < message.bit_containers.size();
         ++container_index) {
      const BitContainerPlan& container = message.bit_containers[container_index];
      std::size_t offset = 0U;
      std::size_t width = 0U;
      if (!IsStableId(container.id) || !container_ids.emplace(container.id).second ||
          !ToSize(container.byte_offset, offset) || !ToSize(container.byte_width, width) ||
          (width != 1U && width != 2U && width != 4U && width != 8U) ||
          !IsRangeWithin(offset, width, frame_size) ||
          !IsBitContainerByteOrderValid(container.byte_order, width) ||
          !IsBitNumberingValid(container.bit_numbering) ||
          !FitsUnsignedWidth(container.base_value, width)) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index);
      }
      field_intervals.push_back(ByteInterval{offset, offset + width, container_index});
      execution.bit_containers.push_back(
          BitContainerExecutionPlan{offset, width, container.byte_order, container.base_value});
      determined_masks[container_index] = WidthMask(width);
      determined_values[container_index] = container.base_value;
    }
    std::size_t input_ordinal = 0U;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    std::size_t conversion_slot = 0U;
#endif

    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      const FieldPlan& field = message.fields[field_index];
      std::size_t offset = 0U;
      std::size_t width = 0U;
      const bool bitfield = field.wire_codec == WireCodec::BITFIELD;
      if (!IsStableId(field.id) || !field_ids.emplace(field.id).second ||
          (!bitfield && (!ToSize(field.byte_offset, offset) || !ToSize(field.byte_width, width) ||
                         !IsRangeWithin(offset, width, frame_size))) ||
          (bitfield && field.bit_container_index >= message.bit_containers.size())) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
      }
      if (bitfield) {
        const BitContainerPlan& container = message.bit_containers[field.bit_container_index];
        offset = static_cast<std::size_t>(container.byte_offset);
        width = static_cast<std::size_t>(container.byte_width);
      } else {
        field_intervals.push_back(ByteInterval{offset, offset + width, field_index});
      }

      const bool uint64_valid =
          field.value_type == ValueType::UINT64 &&
          field.wire_codec == WireCodec::UNSIGNED_INTEGER && width <= 8U &&
          IsIntegerByteOrderValid(field.byte_order, width) && field.enum_entries.empty() &&
          !field.signed_constant_value.has_value() &&
          ((field.encode_source == EncodeSource::INPUT && !field.constant_value.has_value()) ||
           (field.encode_source == EncodeSource::CONSTANT && field.constant_value.has_value() &&
            FitsUnsignedWidth(*field.constant_value, width)));
      const bool int64_valid = (draft.schema_version == "0.4"
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                                || draft.schema_version == "0.5"
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
                                || draft.schema_version == "0.6"
#endif
                                ) &&
                               field.value_type == ValueType::INT64 &&
                               field.wire_codec == WireCodec::UNSIGNED_INTEGER && width <= 8U &&
                               IsIntegerByteOrderValid(field.byte_order, width) &&
                               field.enum_entries.empty() && !field.constant_value.has_value() &&
                               ((field.encode_source == EncodeSource::INPUT &&
                                 !field.signed_constant_value.has_value()) ||
                                (field.encode_source == EncodeSource::CONSTANT &&
                                 field.signed_constant_value.has_value() &&
                                 FitsSignedWidth(*field.signed_constant_value, width)));
      const bool bytes_valid =
          field.value_type == ValueType::BYTES && field.wire_codec == WireCodec::BYTES &&
          field.byte_order == ByteOrder::NOT_APPLICABLE &&
          field.encode_source == EncodeSource::INPUT && !field.constant_value.has_value() &&
          !field.signed_constant_value.has_value() && field.enum_entries.empty();
      const bool enum_shape_valid =
          field.value_type == ValueType::ENUM && field.wire_codec == WireCodec::UNSIGNED_INTEGER &&
          width <= 8U && IsIntegerByteOrderValid(field.byte_order, width) &&
          field.encode_source == EncodeSource::INPUT && !field.constant_value.has_value() &&
          !field.signed_constant_value.has_value() && !field.enum_entries.empty() &&
          (field.unknown_enum_policy == UnknownEnumPolicy::REJECT ||
           field.unknown_enum_policy == UnknownEnumPolicy::PRESERVE);
      const std::size_t container_bits = bitfield ? width * 8U : 0U;
      const bool bit_range_valid = bitfield && field.bit_width != 0U && field.bit_width <= 64U &&
                                   field.bit_offset <= container_bits &&
                                   field.bit_width <= container_bits - field.bit_offset;
      const bool bit_type_valid =
          bit_range_valid &&
          (field.value_type == ValueType::UINT64 || field.value_type == ValueType::BOOL ||
           field.value_type == ValueType::ENUM) &&
          (field.value_type != ValueType::BOOL || field.bit_width == 1U);
      const bool bit_encode_valid =
          bitfield && bit_type_valid && !field.signed_constant_value.has_value() &&
          ((field.encode_source == EncodeSource::INPUT && !field.constant_value.has_value()) ||
           (field.value_type == ValueType::UINT64 &&
            field.encode_source == EncodeSource::CONSTANT && field.constant_value.has_value() &&
            !field.signed_constant_value.has_value() &&
            FitsUnsignedBits(*field.constant_value, static_cast<std::size_t>(field.bit_width))));
      if ((!bitfield && !uint64_valid && !int64_valid && !bytes_valid && !enum_shape_valid) ||
          (bitfield && (!bit_encode_valid ||
                        (field.value_type == ValueType::ENUM && field.enum_entries.empty())))) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
      }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      const bool has_conversion = field.conversion_index != kInvalidPlanBuildIndex;
      if (((draft.schema_version != "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
            && draft.schema_version != "0.6"
#endif
            ) &&
           has_conversion) ||
          (has_conversion &&
           (field.conversion_index >= draft.conversions.size() || bitfield ||
            field.encode_source != EncodeSource::INPUT ||
            (field.value_type != ValueType::UINT64 && field.value_type != ValueType::INT64) ||
            draft.conversions[field.conversion_index].raw_value_type != field.value_type))) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
      }
      if (has_conversion) {
        ++conversion_reference_counts[field.conversion_index];
      }
#endif

      FieldExecutionPlan field_execution;
      field_execution.offset = offset;
      field_execution.width = width;
      field_execution.input_ordinal = field.encode_source == EncodeSource::INPUT
                                          ? input_ordinal++
                                          : (std::numeric_limits<std::size_t>::max)();
      field_execution.value_type = field.value_type;
      field_execution.byte_order = field.byte_order;
      field_execution.encode_source = field.encode_source;
      field_execution.has_constant =
          field.constant_value.has_value() || field.signed_constant_value.has_value();
      field_execution.constant_value = field.constant_value.value_or(0U);
      field_execution.signed_constant_value = field.signed_constant_value.value_or(0);
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      field_execution.conversion_index = field.conversion_index;
      if (has_conversion) {
        field_execution.conversion_slot = conversion_slot++;
      }
#endif
      field_execution.unknown_enum_policy = field.value_type == ValueType::ENUM
                                                ? field.unknown_enum_policy
                                                : UnknownEnumPolicy::REJECT;
      field_execution.enum_values_begin = execution.enum_raw_values.size();
      field_execution.enum_lookup_begin = execution.enum_lookup_entries.size();
      field_execution.bit_container_index = field.bit_container_index;
      field_execution.bit_width = static_cast<std::uint8_t>(field.bit_width);
      if (bitfield) {
        const BitContainerPlan& container = message.bit_containers[field.bit_container_index];
        const std::size_t shift = container.bit_numbering == BitNumbering::LSB0
                                      ? static_cast<std::size_t>(field.bit_offset)
                                      : width * 8U - static_cast<std::size_t>(field.bit_offset) -
                                            static_cast<std::size_t>(field.bit_width);
        const std::uint64_t low_mask =
            field.bit_width == 64U
                ? (std::numeric_limits<std::uint64_t>::max)()
                : (std::uint64_t{1U} << static_cast<unsigned>(field.bit_width)) - 1U;
        field_execution.bit_shift = static_cast<std::uint8_t>(shift);
        field_execution.bit_mask = low_mask << static_cast<unsigned>(shift);
        if ((member_masks[field.bit_container_index] & field_execution.bit_mask) != 0U) {
          return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
        }
        member_masks[field.bit_container_index] |= field_execution.bit_mask;
        if (field.encode_source == EncodeSource::INPUT) {
          determined_masks[field.bit_container_index] &= ~field_execution.bit_mask;
        } else {
          determined_values[field.bit_container_index] =
              (determined_values[field.bit_container_index] & ~field_execution.bit_mask) |
              ((*field.constant_value << static_cast<unsigned>(shift)) & field_execution.bit_mask);
        }
        ++member_counts[field.bit_container_index];
      }

      if (field.value_type == ValueType::ENUM) {
        std::unordered_set<std::string> enum_ids;
        std::unordered_set<std::uint64_t> enum_values;
        enum_ids.reserve(field.enum_entries.size());
        enum_values.reserve(field.enum_entries.size());
        for (std::size_t entry_index = 0U; entry_index < field.enum_entries.size(); ++entry_index) {
          const EnumEntryPlan& entry = field.enum_entries[entry_index];
          if (!IsStableId(entry.id) || !enum_ids.emplace(entry.id).second ||
              !enum_values.emplace(entry.raw_value).second ||
              !(bitfield
                    ? FitsUnsignedBits(entry.raw_value, static_cast<std::size_t>(field.bit_width))
                    : FitsUnsignedWidth(entry.raw_value, width))) {
            return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                          kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex,
                          field_index);
          }
          execution.enum_raw_values.push_back(entry.raw_value);
          execution.enum_lookup_entries.push_back(
              EnumLookupExecutionPlan{entry.raw_value, entry_index});
        }
        field_execution.enum_values_count = field.enum_entries.size();
        field_execution.enum_lookup_count = field.enum_entries.size();
        auto lookup_begin = execution.enum_lookup_entries.begin() +
                            static_cast<std::ptrdiff_t>(field_execution.enum_lookup_begin);
        std::sort(lookup_begin, execution.enum_lookup_entries.end(),
                  [](const EnumLookupExecutionPlan& left, const EnumLookupExecutionPlan& right) {
                    return left.raw_value < right.raw_value;
                  });
      }
      execution.fields.push_back(field_execution);
    }
    for (const std::size_t count : member_counts) {
      if (count == 0U) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index);
      }
    }
    std::sort(field_intervals.begin(), field_intervals.end(),
              [](const ByteInterval& left, const ByteInterval& right) {
                if (left.begin != right.begin) {
                  return left.begin < right.begin;
                }
                return left.end < right.end;
              });
    for (std::size_t field_interval_index = 1U; field_interval_index < field_intervals.size();
         ++field_interval_index) {
      if (field_intervals[field_interval_index].begin <
          field_intervals[field_interval_index - 1U].end) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex,
                      field_intervals[field_interval_index].source_index);
      }
    }
    if (execution.integrity.has_value()) {
      std::size_t integrity_storage_width = 1U;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      if (execution.integrity->algorithm == IntegrityAlgorithm::CRC) {
        integrity_storage_width = static_cast<std::size_t>(execution.integrity->crc_width / 8U);
      }
#endif
      for (const ByteInterval& interval : field_intervals) {
        if (execution.integrity->storage_offset < interval.end &&
            interval.begin < execution.integrity->storage_offset + integrity_storage_width) {
          return Reject(PlanBuildError::INVALID_MESSAGE_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index);
        }
      }
    }
    execution.required_input_count = input_ordinal;
    execution_resource_layout.max_input_fields_per_message =
        (std::max)(execution_resource_layout.max_input_fields_per_message, input_ordinal);
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    execution_resource_layout.conversion_value_count =
        (std::max)(execution_resource_layout.conversion_value_count, conversion_slot);
#endif

    std::map<std::size_t, std::uint8_t> fixed_bytes;
    for (std::size_t matcher_index = 0U; matcher_index < message.matchers.size(); ++matcher_index) {
      const MatcherPlan& matcher = message.matchers[matcher_index];
      if (matcher.kind == MatcherKind::FRAME_LENGTH_EQUALS) {
        std::size_t matcher_length = 0U;
        if (!ToSize(matcher.length_bytes, matcher_length) || matcher_length != frame_size) {
          return Reject(PlanBuildError::INVALID_MATCHER_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index, matcher_index);
        }
        continue;
      }
      std::size_t matcher_offset = 0U;
      if (matcher.kind != MatcherKind::FIXED_BYTES ||
          !ToSize(matcher.byte_offset, matcher_offset) ||
          !IsRangeWithin(matcher_offset, matcher.bytes.size(), frame_size)) {
        return Reject(PlanBuildError::INVALID_MATCHER_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, matcher_index);
      }
      for (std::size_t byte_index = 0U; byte_index < matcher.bytes.size(); ++byte_index) {
        const auto [iterator, inserted] =
            fixed_bytes.emplace(matcher_offset + byte_index, matcher.bytes[byte_index]);
        if (!inserted && iterator->second != matcher.bytes[byte_index]) {
          return Reject(PlanBuildError::INVALID_MATCHER_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index, matcher_index);
        }
      }
    }

    execution.fixed_bytes.reserve(fixed_bytes.size());
    std::vector<ByteInterval> coverage_intervals = field_intervals;
    coverage_intervals.reserve(field_intervals.size() + fixed_bytes.size() +
                               (execution.integrity.has_value() ? 1U : 0U));
    for (const auto& [offset, value] : fixed_bytes) {
      execution.fixed_bytes.push_back(FixedByteExecutionPlan{offset, value});
      coverage_intervals.push_back(ByteInterval{offset, offset + 1U, kInvalidPlanBuildIndex});
    }
    if (execution.integrity.has_value()) {
      std::size_t integrity_storage_width = 1U;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      if (execution.integrity->algorithm == IntegrityAlgorithm::CRC) {
        integrity_storage_width = static_cast<std::size_t>(execution.integrity->crc_width / 8U);
      }
#endif
      for (std::size_t index = 0U; index < integrity_storage_width; ++index) {
        if (fixed_bytes.find(execution.integrity->storage_offset + index) != fixed_bytes.end()) {
          return Reject(PlanBuildError::INVALID_MESSAGE_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index);
        }
      }
      coverage_intervals.push_back(ByteInterval{
          execution.integrity->storage_offset,
          execution.integrity->storage_offset + integrity_storage_width, kInvalidPlanBuildIndex});
    }
    if (!CoversWholeFrame(coverage_intervals, frame_size)) {
      return Reject(PlanBuildError::FRAME_NOT_FULLY_DEFINED, kInvalidPlanBuildIndex,
                    kInvalidPlanBuildIndex, message_index);
    }

    for (std::size_t container_index = 0U; container_index < message.bit_containers.size();
         ++container_index) {
      const BitContainerPlan& container = message.bit_containers[container_index];
      const std::size_t offset = static_cast<std::size_t>(container.byte_offset);
      const std::size_t width = static_cast<std::size_t>(container.byte_width);
      for (std::size_t byte_index = 0U; byte_index < width; ++byte_index) {
        const auto matcher_byte = fixed_bytes.find(offset + byte_index);
        if (matcher_byte == fixed_bytes.end()) {
          continue;
        }
        const std::size_t logical_byte =
            container.byte_order == ByteOrder::BIG ? width - 1U - byte_index : byte_index;
        const auto shift = static_cast<unsigned>(logical_byte * 8U);
        const auto mask = static_cast<std::uint8_t>(determined_masks[container_index] >> shift);
        const auto value = static_cast<std::uint8_t>(determined_values[container_index] >> shift);
        if (((matcher_byte->second ^ value) & mask) != 0U) {
          return Reject(PlanBuildError::INVALID_MATCHER_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index);
        }
      }
    }

    for (const FieldPlan& field : message.fields) {
      if (field.wire_codec == WireCodec::BITFIELD) {
        continue;
      }
      if ((field.value_type != ValueType::UINT64 && field.value_type != ValueType::INT64) ||
          field.encode_source != EncodeSource::CONSTANT ||
          (!field.constant_value.has_value() && !field.signed_constant_value.has_value())) {
        continue;
      }
      const std::uint64_t encoded = field.value_type == ValueType::INT64
                                        ? static_cast<std::uint64_t>(*field.signed_constant_value)
                                        : *field.constant_value;
      const std::size_t offset = static_cast<std::size_t>(field.byte_offset);
      const std::size_t width = static_cast<std::size_t>(field.byte_width);
      for (std::size_t byte_index = 0U; byte_index < width; ++byte_index) {
        const auto matcher_byte = fixed_bytes.find(offset + byte_index);
        if (matcher_byte != fixed_bytes.end() &&
            matcher_byte->second !=
                EncodeUnsignedByte(encoded, width, field.byte_order, byte_index)) {
          return Reject(PlanBuildError::INVALID_MATCHER_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index);
        }
      }
    }
    message_execution_plans.push_back(std::move(execution));
  }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  for (const std::size_t count : conversion_reference_counts) {
    if (count != 1U) {
      return Reject(PlanBuildError::INVALID_FIELD_PLAN);
    }
  }
#endif

  execution_resource_layout.encode_value_index_count =
      execution_resource_layout.max_input_fields_per_message;
  execution_resource_layout.encode_presence_word_count =
      execution_resource_layout.max_input_fields_per_message / kBitsPerAllowedMessageWord +
      (execution_resource_layout.max_input_fields_per_message % kBitsPerAllowedMessageWord == 0U
           ? 0U
           : 1U);
  for (const MessagePlan& message : draft.messages) {
    execution_resource_layout.bit_container_value_count =
        (std::max)(execution_resource_layout.bit_container_value_count,
                   message.bit_containers.size());
  }
  std::size_t value_index_bytes = 0U;
  std::size_t presence_word_bytes = 0U;
  std::size_t bit_container_bytes = 0U;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t conversion_workspace_bytes = 0U;
  constexpr std::size_t kConversionWorkspaceBytesPerSlot =
      sizeof(std::int64_t) + sizeof(std::int32_t) + sizeof(std::uint64_t) + sizeof(ValueType) +
      sizeof(std::size_t) + sizeof(std::uint64_t);
#endif
  if (!MultiplySizeChecked(execution_resource_layout.encode_value_index_count, sizeof(std::size_t),
                           value_index_bytes) ||
      !MultiplySizeChecked(execution_resource_layout.encode_presence_word_count,
                           sizeof(std::uint64_t), presence_word_bytes) ||
      !MultiplySizeChecked(execution_resource_layout.bit_container_value_count,
                           sizeof(std::uint64_t), bit_container_bytes) ||
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      !MultiplySizeChecked(execution_resource_layout.conversion_value_count,
                           kConversionWorkspaceBytesPerSlot, conversion_workspace_bytes) ||
#endif
      !AddSizeChecked(presence_word_bytes, value_index_bytes) ||
      !AddSizeChecked(bit_container_bytes, value_index_bytes) ||
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      !AddSizeChecked(conversion_workspace_bytes, value_index_bytes) ||
#endif
      value_index_bytes > limits->max_session_memory_bytes) {
    return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED);
  }
  execution_resource_layout.estimated_workspace_bytes = value_index_bytes;

  std::unordered_set<std::string> pipeline_ids;
  pipeline_ids.reserve(draft.pipelines.size());
  std::vector<detail::PreparedPipelineExecutionPlan> pipeline_execution_plans;
  pipeline_execution_plans.reserve(draft.pipelines.size());
  const std::size_t allowed_word_count =
      draft.messages.size() / kBitsPerAllowedMessageWord +
      (draft.messages.size() % kBitsPerAllowedMessageWord == 0U ? 0U : 1U);
  for (std::size_t pipeline_index = 0U; pipeline_index < draft.pipelines.size(); ++pipeline_index) {
    const PipelinePlan& pipeline = draft.pipelines[pipeline_index];
    if (!IsStableId(pipeline.id) || !IsStableId(pipeline.direction_id) ||
        !pipeline_ids.emplace(pipeline.id).second || pipeline.message_indices.empty() ||
        pipeline.framing_profile_index >= draft.framing_profiles.size() ||
        draft.framing_profiles[pipeline.framing_profile_index].input_kind !=
            InputKind::COMPLETE_RECORD) {
      return Reject(PlanBuildError::INVALID_PIPELINE_PLAN, kInvalidPlanBuildIndex, pipeline_index);
    }
    detail::PreparedPipelineExecutionPlan execution;
    execution.framing_profile_index = pipeline.framing_profile_index;
    execution.allowed_message_words.assign(allowed_word_count, 0U);
    std::map<std::size_t, std::vector<std::size_t>> candidate_groups;
    std::unordered_set<std::size_t> seen_message_indices;
    seen_message_indices.reserve(pipeline.message_indices.size());
    for (const std::size_t message_index : pipeline.message_indices) {
      if (message_index >= draft.messages.size() ||
          !seen_message_indices.emplace(message_index).second ||
          draft.messages[message_index].direction_id != pipeline.direction_id) {
        return Reject(PlanBuildError::INVALID_PIPELINE_PLAN, kInvalidPlanBuildIndex, pipeline_index,
                      message_index);
      }
      for (const auto& [frame_size, earlier_indices] : candidate_groups) {
        if (frame_size != message_execution_plans[message_index].frame_size) {
          continue;
        }
        for (const std::size_t earlier_index : earlier_indices) {
          if (FixedMatchersCanIntersect(message_execution_plans[earlier_index],
                                        message_execution_plans[message_index])) {
            return Reject(PlanBuildError::AMBIGUOUS_MATCHER, kInvalidPlanBuildIndex, pipeline_index,
                          message_index);
          }
        }
      }
      const std::size_t word_index = message_index / kBitsPerAllowedMessageWord;
      const auto bit_index = static_cast<unsigned>(message_index % kBitsPerAllowedMessageWord);
      execution.allowed_message_words[word_index] |= std::uint64_t{1U} << bit_index;
      candidate_groups[message_execution_plans[message_index].frame_size].push_back(message_index);
    }
    execution.candidate_groups.reserve(candidate_groups.size());
    for (auto& [frame_size, message_indices] : candidate_groups) {
      execution.candidate_groups.push_back(
          detail::PreparedCandidateGroupExecutionPlan{frame_size, std::move(message_indices)});
    }
    pipeline_execution_plans.push_back(std::move(execution));
  }

  const std::size_t effective_plan_memory_limit =
      draft.plan_memory_limit_bytes == 0U
          ? (std::min)(limits->max_plan_memory_bytes, kV01MaxPlanMemoryHardLimit)
          : (std::min)(draft.plan_memory_limit_bytes, kV01MaxPlanMemoryHardLimit);
  PlanMemoryReport exact_estimate;
  if (!EstimatePreparedPlanMemory(draft, message_execution_plans, pipeline_execution_plans,
                                  (std::numeric_limits<std::size_t>::max)(), exact_estimate)) {
    return RejectMemory(PlanBuildError::PLAN_MEMORY_LIMIT_EXCEEDED,
                        (std::numeric_limits<std::size_t>::max)(), effective_plan_memory_limit,
                        draft.resource_profile);
  }
  if (exact_estimate.accounted_total_bytes > effective_plan_memory_limit) {
    return RejectMemory(PlanBuildError::PLAN_MEMORY_LIMIT_EXCEEDED,
                        exact_estimate.accounted_total_bytes, effective_plan_memory_limit,
                        draft.resource_profile);
  }
  if (!PlanMemoryReportsEqual(draft.approved_plan_memory, exact_estimate)) {
    return RejectMemory(PlanBuildError::PLAN_MEMORY_ESTIMATE_MISMATCH,
                        exact_estimate.accounted_total_bytes,
                        draft.approved_plan_memory.accounted_total_bytes, draft.resource_profile);
  }

  PlanStorageBlock storage =
      PlanStorageBlock::Allocate(exact_estimate.accounted_total_bytes,
                                 draft.test_fail_at_allocation == 1U, draft.test_memory_probe);
  if (!storage) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }
  PlanArena arena{storage.Data(), storage.Size(), draft.test_fail_at_allocation};
  PlanBundle* plan_storage = arena.AllocateArray<PlanBundle>(1U, PlanMemoryCategory::OBJECT);
  if (plan_storage == nullptr) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

  FrozenString schema_version;
  FrozenString protocol_id;
  FrozenString protocol_version;
  if (!FreezeString(arena, draft.schema_version, schema_version) ||
      !FreezeString(arena, draft.protocol_id, protocol_id) ||
      !FreezeString(arena, draft.protocol_version, protocol_version)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

  FrozenArray<FrozenFramingPlan> frozen_framings;
  if (!FreezeObjectArray<FrozenFramingPlan>(
          arena, draft.framing_profiles.size(), PlanMemoryCategory::METADATA_CONTAINER,
          [&arena, &draft](std::size_t index, FrozenFramingPlan& output) {
            output.input_kind = draft.framing_profiles[index].input_kind;
            return FreezeString(arena, draft.framing_profiles[index].id, output.id);
          },
          frozen_framings)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

  FrozenArray<FrozenPipelinePlan> frozen_pipelines;
  if (!FreezeObjectArray<FrozenPipelinePlan>(
          arena, draft.pipelines.size(), PlanMemoryCategory::METADATA_CONTAINER,
          [&arena, &draft](std::size_t index, FrozenPipelinePlan& output) {
            const PipelinePlan& source = draft.pipelines[index];
            output.framing_profile_index = source.framing_profile_index;
            return FreezeString(arena, source.id, output.id) &&
                   FreezeString(arena, source.direction_id, output.direction_id) &&
                   FreezePodArray(arena, source.message_indices,
                                  PlanMemoryCategory::METADATA_CONTAINER, output.message_indices);
          },
          frozen_pipelines)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

  FrozenArray<FrozenMessagePlan> frozen_messages;
  if (!FreezeObjectArray<FrozenMessagePlan>(
          arena, draft.messages.size(), PlanMemoryCategory::METADATA_CONTAINER,
          [&arena, &draft](std::size_t message_index, FrozenMessagePlan& output) {
            const MessagePlan& source = draft.messages[message_index];
            output.frame_length_bytes = source.frame_length_bytes;
            if (source.integrity.has_value()) {
              FrozenIntegrityPlan integrity;
              integrity.algorithm = source.integrity->algorithm;
              integrity.range_offset = source.integrity->range_offset;
              integrity.range_length = source.integrity->range_length;
              integrity.storage_offset = source.integrity->storage_offset;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
              integrity.crc_width = source.integrity->crc_width;
              integrity.crc_polynomial = source.integrity->crc_polynomial;
              integrity.crc_initial_value = source.integrity->crc_initial_value;
              integrity.crc_xor_output = source.integrity->crc_xor_output;
              integrity.crc_reflect_input = source.integrity->crc_reflect_input;
              integrity.crc_reflect_output = source.integrity->crc_reflect_output;
              integrity.storage_byte_order = source.integrity->storage_byte_order;
#endif
              output.integrity = integrity;
            }
            if (!FreezeString(arena, source.id, output.id) ||
                !FreezeString(arena, source.direction_id, output.direction_id) ||
                !FreezeObjectArray<FrozenMatcherPlan>(
                    arena, source.matchers.size(), PlanMemoryCategory::METADATA_CONTAINER,
                    [&arena, &source](std::size_t matcher_index, FrozenMatcherPlan& matcher) {
                      const MatcherPlan& matcher_source = source.matchers[matcher_index];
                      matcher.kind = matcher_source.kind;
                      matcher.length_bytes = matcher_source.length_bytes;
                      matcher.byte_offset = matcher_source.byte_offset;
                      return FreezePodArray(arena, matcher_source.bytes,
                                            PlanMemoryCategory::MATCHER, matcher.bytes);
                    },
                    output.matchers) ||
                !FreezeObjectArray<FrozenBitContainerPlan>(
                    arena, source.bit_containers.size(), PlanMemoryCategory::METADATA_CONTAINER,
                    [&arena, &source](std::size_t index, FrozenBitContainerPlan& output_container) {
                      const BitContainerPlan& input = source.bit_containers[index];
                      output_container.byte_offset = input.byte_offset;
                      output_container.byte_width = input.byte_width;
                      output_container.byte_order = input.byte_order;
                      output_container.bit_numbering = input.bit_numbering;
                      output_container.base_value = input.base_value;
                      return FreezeString(arena, input.id, output_container.id);
                    },
                    output.bit_containers)) {
              return false;
            }
            return FreezeObjectArray<FrozenFieldPlan>(
                arena, source.fields.size(), PlanMemoryCategory::METADATA_CONTAINER,
                [&arena, &source](std::size_t field_index, FrozenFieldPlan& field) {
                  const FieldPlan& field_source = source.fields[field_index];
                  field.value_type = field_source.value_type;
                  field.wire_codec = field_source.wire_codec;
                  field.byte_offset = field_source.byte_offset;
                  field.byte_width = field_source.byte_width;
                  field.byte_order = field_source.byte_order;
                  field.encode_source = field_source.encode_source;
                  field.constant_value = field_source.constant_value;
                  field.signed_constant_value = field_source.signed_constant_value;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                  field.conversion_index = field_source.conversion_index;
#endif
                  field.unknown_enum_policy = field_source.unknown_enum_policy;
                  field.bit_container_index = field_source.bit_container_index;
                  field.bit_offset = field_source.bit_offset;
                  field.bit_width = field_source.bit_width;
                  if (!FreezeString(arena, field_source.id, field.id)) {
                    return false;
                  }
                  return FreezeObjectArray<FrozenEnumEntryPlan>(
                      arena, field_source.enum_entries.size(),
                      PlanMemoryCategory::METADATA_CONTAINER,
                      [&arena, &field_source](std::size_t entry_index, FrozenEnumEntryPlan& entry) {
                        entry.raw_value = field_source.enum_entries[entry_index].raw_value;
                        return FreezeString(arena, field_source.enum_entries[entry_index].id,
                                            entry.id);
                      },
                      field.enum_entries);
                },
                output.fields);
          },
          frozen_messages)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  FrozenArray<LinearConversionDescriptor> frozen_conversions;
  if (!FreezePodArray(arena, draft.conversions, PlanMemoryCategory::EXTENSION,
                      frozen_conversions)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }
#endif

  FrozenArray<MessageExecutionPlan> frozen_message_execution;
  if (!FreezeObjectArray<MessageExecutionPlan>(
          arena, message_execution_plans.size(), PlanMemoryCategory::EXECUTION_DESCRIPTOR,
          [&arena, &message_execution_plans](std::size_t index, MessageExecutionPlan& output) {
            const detail::PreparedMessageExecutionPlan& source = message_execution_plans[index];
            output.frame_size = source.frame_size;
            output.required_input_count = source.required_input_count;
            output.integrity = source.integrity;
            return FreezePodArray(arena, source.fixed_bytes, PlanMemoryCategory::MATCHER,
                                  output.fixed_bytes) &&
                   FreezePodArray(arena, source.bit_containers,
                                  PlanMemoryCategory::EXECUTION_DESCRIPTOR,
                                  output.bit_containers) &&
                   FreezePodArray(arena, source.fields, PlanMemoryCategory::EXECUTION_DESCRIPTOR,
                                  output.fields) &&
                   FreezePodArray(arena, source.enum_raw_values, PlanMemoryCategory::INDEX,
                                  output.enum_raw_values) &&
                   FreezePodArray(arena, source.enum_lookup_entries, PlanMemoryCategory::INDEX,
                                  output.enum_lookup_entries);
          },
          frozen_message_execution)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

  FrozenArray<PipelineExecutionPlan> frozen_pipeline_execution;
  if (!FreezeObjectArray<PipelineExecutionPlan>(
          arena, pipeline_execution_plans.size(), PlanMemoryCategory::EXECUTION_DESCRIPTOR,
          [&arena, &pipeline_execution_plans](std::size_t index, PipelineExecutionPlan& output) {
            const detail::PreparedPipelineExecutionPlan& source = pipeline_execution_plans[index];
            output.framing_profile_index = source.framing_profile_index;
            if (!FreezePodArray(arena, source.allowed_message_words, PlanMemoryCategory::INDEX,
                                output.allowed_message_words)) {
              return false;
            }
            return FreezeObjectArray<CandidateGroupExecutionPlan>(
                arena, source.candidate_groups.size(), PlanMemoryCategory::EXECUTION_DESCRIPTOR,
                [&arena, &source](std::size_t group_index, CandidateGroupExecutionPlan& group) {
                  group.frame_size = source.candidate_groups[group_index].frame_size;
                  return FreezePodArray(arena, source.candidate_groups[group_index].message_indices,
                                        PlanMemoryCategory::INDEX, group.message_indices);
                },
                output.candidate_groups);
          },
          frozen_pipeline_execution)) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  }

  const PlanMemoryReport final_report = arena.Report();
  if (!PlanMemoryReportsEqual(exact_estimate, final_report) ||
      final_report.accounted_total_bytes != storage.Size()) {
    return RejectMemory(PlanBuildError::PLAN_MEMORY_ESTIMATE_MISMATCH,
                        final_report.accounted_total_bytes, exact_estimate.accounted_total_bytes,
                        draft.resource_profile);
  }
  PlanBundle* plan = new (plan_storage) PlanBundle(
      schema_version, protocol_id, protocol_version, draft.resource_profile, actual_requirements,
      std::move(frozen_framings), std::move(frozen_pipelines), std::move(frozen_messages),
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      std::move(frozen_conversions),
#endif
      execution_resource_layout, std::move(frozen_message_execution),
      std::move(frozen_pipeline_execution), final_report);
  return PlanBuildResult::Success(PlanOwner{std::move(storage), plan});
}

PlanBuildResult PlanBuilder::Freeze(BudgetedPlanDraft draft) noexcept {
  try {
    if (draft.draft_ == nullptr) {
      return Reject(PlanBuildError::INTERNAL_ERROR);
    }
    std::unique_ptr<detail::PlanDraftData> raw_draft = std::move(draft.draft_);
    return FreezeImpl(std::move(*raw_draft));
  } catch (const std::bad_alloc&) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  } catch (...) {
    return Reject(PlanBuildError::INTERNAL_ERROR);
  }
}

}  // namespace pae::protocol_plan
