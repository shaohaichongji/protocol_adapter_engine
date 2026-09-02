#include "plan_builder.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <new>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

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
  PlanBuildResult result;
  result.diagnostic = PlanBuildDiagnostic{code,          framing_index, pipeline_index,
                                          message_index, matcher_index, field_index};
  return result;
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

bool IsIntegerByteOrderValid(ByteOrder byte_order, std::size_t width) noexcept {
  if (width == 1U) {
    return byte_order == ByteOrder::NOT_APPLICABLE || byte_order == ByteOrder::BIG ||
           byte_order == ByteOrder::LITTLE;
  }
  return byte_order == ByteOrder::BIG || byte_order == ByteOrder::LITTLE;
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
         left.total_enum_entry_count == right.total_enum_entry_count;
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

bool FixedMatchersCanIntersect(const MessageExecutionPlan& left,
                               const MessageExecutionPlan& right) noexcept {
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

}  // namespace

PlanBuildResult PlanBuilder::FreezeImpl(PlanDraft draft) {
  const ResourceProfileLimits* limits = GetResourceProfileLimits(draft.resource_profile);
  if (draft.schema_version != "0.1" || !IsStableId(draft.protocol_id) ||
      draft.protocol_version.empty() || limits == nullptr || draft.framing_profiles.empty() ||
      draft.pipelines.empty() || draft.messages.empty()) {
    return Reject(PlanBuildError::INVALID_METADATA);
  }

  ResourceRequirements actual_requirements;
  actual_requirements.framing_profile_count = draft.framing_profiles.size();
  actual_requirements.pipeline_count = draft.pipelines.size();
  actual_requirements.message_count = draft.messages.size();
  if (actual_requirements.framing_profile_count > limits->max_framing_profiles ||
      actual_requirements.pipeline_count > limits->max_pipelines ||
      actual_requirements.message_count > limits->max_messages) {
    return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED);
  }
  for (std::size_t message_index = 0U; message_index < draft.messages.size(); ++message_index) {
    const MessagePlan& message = draft.messages[message_index];
    if (message.frame_length_bytes > limits->max_frame_bytes ||
        message.fields.size() > limits->max_fields_per_message ||
        message.matchers.size() > limits->max_matchers_per_message ||
        !AddSizeChecked(message.fields.size(), actual_requirements.total_field_count) ||
        !AddSizeChecked(message.matchers.size(), actual_requirements.total_matcher_count) ||
        actual_requirements.total_field_count > limits->max_total_fields ||
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
  std::vector<MessageExecutionPlan> message_execution_plans;
  message_execution_plans.reserve(draft.messages.size());
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

    MessageExecutionPlan execution;
    execution.frame_size = frame_size;
    execution.fields.reserve(message.fields.size());
    std::unordered_set<std::string> field_ids;
    field_ids.reserve(message.fields.size());
    std::vector<ByteInterval> field_intervals;
    field_intervals.reserve(message.fields.size());
    std::size_t input_ordinal = 0U;

    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      const FieldPlan& field = message.fields[field_index];
      std::size_t offset = 0U;
      std::size_t width = 0U;
      if (!IsStableId(field.id) || !field_ids.emplace(field.id).second ||
          !ToSize(field.byte_offset, offset) || !ToSize(field.byte_width, width) ||
          !IsRangeWithin(offset, width, frame_size)) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
      }
      field_intervals.push_back(ByteInterval{offset, offset + width, field_index});

      const bool uint64_valid =
          field.value_type == ValueType::UINT64 &&
          field.wire_codec == WireCodec::UNSIGNED_INTEGER && width <= 8U &&
          IsIntegerByteOrderValid(field.byte_order, width) && field.enum_entries.empty() &&
          ((field.encode_source == EncodeSource::INPUT && !field.constant_value.has_value()) ||
           (field.encode_source == EncodeSource::CONSTANT && field.constant_value.has_value() &&
            FitsUnsignedWidth(*field.constant_value, width)));
      const bool bytes_valid = field.value_type == ValueType::BYTES &&
                               field.wire_codec == WireCodec::BYTES &&
                               field.byte_order == ByteOrder::NOT_APPLICABLE &&
                               field.encode_source == EncodeSource::INPUT &&
                               !field.constant_value.has_value() && field.enum_entries.empty();
      const bool enum_shape_valid =
          field.value_type == ValueType::ENUM && field.wire_codec == WireCodec::UNSIGNED_INTEGER &&
          width <= 8U && IsIntegerByteOrderValid(field.byte_order, width) &&
          field.encode_source == EncodeSource::INPUT && !field.constant_value.has_value() &&
          !field.enum_entries.empty() &&
          (field.unknown_enum_policy == UnknownEnumPolicy::REJECT ||
           field.unknown_enum_policy == UnknownEnumPolicy::PRESERVE);
      if (!uint64_valid && !bytes_valid && !enum_shape_valid) {
        return Reject(PlanBuildError::INVALID_FIELD_PLAN, kInvalidPlanBuildIndex,
                      kInvalidPlanBuildIndex, message_index, kInvalidPlanBuildIndex, field_index);
      }

      FieldExecutionPlan field_execution;
      field_execution.offset = offset;
      field_execution.width = width;
      field_execution.input_ordinal = field.encode_source == EncodeSource::INPUT
                                          ? input_ordinal++
                                          : (std::numeric_limits<std::size_t>::max)();
      field_execution.value_type = field.value_type;
      field_execution.byte_order = field.byte_order;
      field_execution.encode_source = field.encode_source;
      field_execution.has_constant = field.constant_value.has_value();
      field_execution.constant_value = field.constant_value.value_or(0U);
      field_execution.unknown_enum_policy = field.value_type == ValueType::ENUM
                                                ? field.unknown_enum_policy
                                                : UnknownEnumPolicy::REJECT;
      field_execution.enum_values_begin = execution.enum_raw_values.size();
      field_execution.enum_lookup_begin = execution.enum_lookup_entries.size();

      if (field.value_type == ValueType::ENUM) {
        std::unordered_set<std::string> enum_ids;
        std::unordered_set<std::uint64_t> enum_values;
        enum_ids.reserve(field.enum_entries.size());
        enum_values.reserve(field.enum_entries.size());
        for (std::size_t entry_index = 0U; entry_index < field.enum_entries.size(); ++entry_index) {
          const EnumEntryPlan& entry = field.enum_entries[entry_index];
          if (!IsStableId(entry.id) || !enum_ids.emplace(entry.id).second ||
              !enum_values.emplace(entry.raw_value).second ||
              !FitsUnsignedWidth(entry.raw_value, width)) {
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
    execution.required_input_count = input_ordinal;
    execution_resource_layout.max_input_fields_per_message =
        (std::max)(execution_resource_layout.max_input_fields_per_message, input_ordinal);

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
    coverage_intervals.reserve(field_intervals.size() + fixed_bytes.size());
    for (const auto& [offset, value] : fixed_bytes) {
      execution.fixed_bytes.push_back(FixedByteExecutionPlan{offset, value});
      coverage_intervals.push_back(ByteInterval{offset, offset + 1U, kInvalidPlanBuildIndex});
    }
    if (!CoversWholeFrame(coverage_intervals, frame_size)) {
      return Reject(PlanBuildError::FRAME_NOT_FULLY_DEFINED, kInvalidPlanBuildIndex,
                    kInvalidPlanBuildIndex, message_index);
    }

    for (const FieldPlan& field : message.fields) {
      if (field.value_type != ValueType::UINT64 || field.encode_source != EncodeSource::CONSTANT ||
          !field.constant_value.has_value()) {
        continue;
      }
      const std::size_t offset = static_cast<std::size_t>(field.byte_offset);
      const std::size_t width = static_cast<std::size_t>(field.byte_width);
      for (std::size_t byte_index = 0U; byte_index < width; ++byte_index) {
        const auto matcher_byte = fixed_bytes.find(offset + byte_index);
        if (matcher_byte != fixed_bytes.end() &&
            matcher_byte->second !=
                EncodeUnsignedByte(*field.constant_value, width, field.byte_order, byte_index)) {
          return Reject(PlanBuildError::INVALID_MATCHER_PLAN, kInvalidPlanBuildIndex,
                        kInvalidPlanBuildIndex, message_index);
        }
      }
    }
    message_execution_plans.push_back(std::move(execution));
  }

  execution_resource_layout.encode_value_index_count =
      execution_resource_layout.max_input_fields_per_message;
  execution_resource_layout.encode_presence_word_count =
      execution_resource_layout.max_input_fields_per_message / kBitsPerAllowedMessageWord +
      (execution_resource_layout.max_input_fields_per_message % kBitsPerAllowedMessageWord == 0U
           ? 0U
           : 1U);
  std::size_t value_index_bytes = 0U;
  std::size_t presence_word_bytes = 0U;
  if (!MultiplySizeChecked(execution_resource_layout.encode_value_index_count, sizeof(std::size_t),
                           value_index_bytes) ||
      !MultiplySizeChecked(execution_resource_layout.encode_presence_word_count,
                           sizeof(std::uint64_t), presence_word_bytes) ||
      !AddSizeChecked(presence_word_bytes, value_index_bytes) ||
      value_index_bytes > limits->max_session_memory_bytes) {
    return Reject(PlanBuildError::RESOURCE_LIMIT_EXCEEDED);
  }
  execution_resource_layout.estimated_workspace_bytes = value_index_bytes;

  std::unordered_set<std::string> pipeline_ids;
  pipeline_ids.reserve(draft.pipelines.size());
  std::vector<PipelineExecutionPlan> pipeline_execution_plans;
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
    PipelineExecutionPlan execution;
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
          CandidateGroupExecutionPlan{frame_size, std::move(message_indices)});
    }
    pipeline_execution_plans.push_back(std::move(execution));
  }

  PlanBuildResult result;
  result.plan = std::unique_ptr<const PlanBundle>(
      new PlanBundle(std::move(draft.schema_version), std::move(draft.protocol_id),
                     std::move(draft.protocol_version), draft.resource_profile, actual_requirements,
                     std::move(draft.framing_profiles), std::move(draft.pipelines),
                     std::move(draft.messages), execution_resource_layout,
                     std::move(message_execution_plans), std::move(pipeline_execution_plans)));
  return result;
}

PlanBuildResult PlanBuilder::Freeze(PlanDraft draft) noexcept {
  try {
    return FreezeImpl(std::move(draft));
  } catch (const std::bad_alloc&) {
    return Reject(PlanBuildError::ALLOCATION_FAILED);
  } catch (...) {
    return Reject(PlanBuildError::INTERNAL_ERROR);
  }
}

}  // namespace pae::protocol_plan
