#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "complete_record_codec.h"
#include "plan_builder.h"

namespace {

using pae::protocol_core::ByteView;
using pae::protocol_core::CodecOperationCounts;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodeCompleteRecord;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::EncodeCompleteRecord;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;
using pae::protocol_plan::ByteOrder;
using pae::protocol_plan::EncodeSource;
using pae::protocol_plan::EnumEntryPlan;
using pae::protocol_plan::FieldPlan;
using pae::protocol_plan::FramingPlan;
using pae::protocol_plan::InputKind;
using pae::protocol_plan::MatcherKind;
using pae::protocol_plan::MatcherPlan;
using pae::protocol_plan::MessagePlan;
using pae::protocol_plan::PipelinePlan;
using pae::protocol_plan::PlanBuilder;
using pae::protocol_plan::PlanBuildResult;
using pae::protocol_plan::PlanDraft;
using pae::protocol_plan::ResourceProfile;
using pae::protocol_plan::ResourceRequirements;
using pae::protocol_plan::UnknownEnumPolicy;
using pae::protocol_plan::ValueType;
using pae::protocol_plan::WireCodec;

constexpr std::array<std::string_view, 4U> kExpectedCaseIds{
    "linear_field_operation_counts",
    "frame_length_candidate_group_operation_counts",
    "fixed_and_bytes_operation_counts",
    "logarithmic_enum_lookup_preserves_entry_index",
};

class TestRunner final {
 public:
  void Record(std::string_view case_id, bool passed, std::string_view detail) {
    const auto found = std::find(kExpectedCaseIds.begin(), kExpectedCaseIds.end(), case_id);
    if (found == kExpectedCaseIds.end() ||
        seen_[static_cast<std::size_t>(found - kExpectedCaseIds.begin())]) {
      ++failed_;
      std::cerr << "FAIL case=" << case_id << " detail=unexpected or duplicate case ID\n";
      return;
    }
    seen_[static_cast<std::size_t>(found - kExpectedCaseIds.begin())] = true;
    if (passed) {
      ++passed_;
      std::cout << "PASS case=" << case_id << '\n';
      return;
    }
    ++failed_;
    std::cerr << "FAIL case=" << case_id << " detail=" << detail << '\n';
  }

  int Finish() const {
    const bool all_seen = std::all_of(seen_.begin(), seen_.end(), [](bool value) { return value; });
    const bool passed = failed_ == 0U && passed_ == kExpectedCaseIds.size() && all_seen;
    std::cout << "OPERATION_COUNT_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " expected=" << kExpectedCaseIds.size() << " gate=" << (passed ? "PASS" : "FAIL")
              << '\n';
    return passed ? 0 : 1;
  }

 private:
  std::array<bool, kExpectedCaseIds.size()> seen_{};
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

FieldPlan MakeByteField(std::string id, std::size_t offset, ValueType type = ValueType::UINT64) {
  FieldPlan field;
  field.id = std::move(id);
  field.value_type = type;
  field.wire_codec = WireCodec::UNSIGNED_INTEGER;
  field.byte_offset = offset;
  field.byte_width = 1U;
  field.byte_order = ByteOrder::NOT_APPLICABLE;
  field.encode_source = EncodeSource::INPUT;
  return field;
}

PlanDraft MakeDraft(std::vector<MessagePlan> messages, std::vector<std::size_t> message_indices) {
  PipelinePlan pipeline;
  pipeline.id = "operation_pipeline";
  pipeline.direction_id = "operation_direction";
  pipeline.framing_profile_index = 0U;
  pipeline.message_indices = std::move(message_indices);

  ResourceRequirements requirements;
  requirements.framing_profile_count = 1U;
  requirements.pipeline_count = 1U;
  requirements.message_count = messages.size();
  for (const MessagePlan& message : messages) {
    requirements.max_frame_bytes =
        (std::max)(requirements.max_frame_bytes, message.frame_length_bytes);
    requirements.total_field_count += message.fields.size();
    requirements.total_matcher_count += message.matchers.size();
    for (const FieldPlan& field : message.fields) {
      requirements.total_enum_entry_count += field.enum_entries.size();
    }
  }

  PlanDraft draft;
  draft.schema_version = "0.1";
  draft.protocol_id = "operation_count_protocol";
  draft.protocol_version = "1";
  draft.resource_profile = ResourceProfile::DESKTOP;
  draft.resource_requirements = requirements;
  draft.framing_profiles.push_back(FramingPlan{"complete_record", InputKind::COMPLETE_RECORD});
  draft.pipelines.push_back(std::move(pipeline));
  draft.messages = std::move(messages);
  return draft;
}

MatcherPlan LengthMatcher(std::size_t frame_size) {
  MatcherPlan matcher;
  matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  matcher.length_bytes = frame_size;
  return matcher;
}

bool CheckLinearFieldCount(std::size_t field_count) {
  MessagePlan message;
  message.id = "linear_fields_message";
  message.direction_id = "operation_direction";
  message.frame_length_bytes = field_count;
  message.matchers.push_back(LengthMatcher(field_count));
  for (std::size_t index = 0U; index < field_count; ++index) {
    message.fields.push_back(MakeByteField("field_" + std::to_string(index), index));
  }

  PlanBuildResult frozen = PlanBuilder::Freeze(MakeDraft({std::move(message)}, {0U}));
  if (!frozen.Succeeded()) {
    return false;
  }
  ExecutionWorkspace workspace{*frozen.plan};
  std::vector<EncodeFieldValue> values(field_count);
  std::vector<std::uint8_t> output(field_count, 0xCCU);
  for (std::size_t index = 0U; index < field_count; ++index) {
    values[index].field = FieldRef{frozen.plan.get(), 0U, index};
    values[index].value_kind = LogicalValueKind::UINT64;
    values[index].uint64_value = static_cast<std::uint64_t>(index & 0xFFU);
  }
  const auto encoded =
      EncodeCompleteRecord(*frozen.plan, workspace, 0U, 0U, values.data(), values.size(),
                           MutableByteBuffer{output.data(), output.size()});
  const CodecOperationCounts encode_counts = workspace.LastOperationCounts();
  const std::size_t expected_presence_words =
      field_count / 64U + (field_count % 64U == 0U ? 0U : 1U);
  if (encoded.status != CodecStatus::OK || encoded.bytes_written != field_count ||
      encode_counts.input_values_visited != field_count ||
      encode_counts.field_validation_visits != field_count ||
      encode_counts.alias_validation_visits != field_count ||
      encode_counts.required_field_scan_visits != field_count ||
      encode_counts.field_write_visits != field_count ||
      encode_counts.field_verify_visits != field_count ||
      encode_counts.fixed_byte_write_visits != 0U || encode_counts.bytes_write_visits != 0U ||
      encode_counts.bytes_verify_visits != 0U ||
      encode_counts.presence_word_clear_visits != expected_presence_words ||
      encode_counts.candidate_group_search_steps != 1U ||
      encode_counts.candidate_messages_examined != 1U ||
      encode_counts.static_plan_validation_visits != 0U) {
    return false;
  }

  std::vector<DecodedFieldSlot> slots(field_count);
  const auto decoded =
      DecodeCompleteRecord(*frozen.plan, workspace, 0U, ByteView{output.data(), output.size()},
                           slots.data(), slots.size());
  const CodecOperationCounts decode_counts = workspace.LastOperationCounts();
  return decoded.status == CodecStatus::OK && decoded.field_count == field_count &&
         decode_counts.field_validation_visits == field_count &&
         decode_counts.alias_validation_visits == 0U &&
         decode_counts.field_write_visits == field_count &&
         decode_counts.candidate_group_search_steps == 1U &&
         decode_counts.candidate_messages_examined == 1U &&
         decode_counts.static_plan_validation_visits == 0U;
}

std::size_t CeilLog2(std::size_t value) {
  std::size_t result = 0U;
  std::size_t power = 1U;
  while (power < value) {
    power <<= 1U;
    ++result;
  }
  return result;
}

bool CheckCandidateGroupCount(std::size_t message_count, std::size_t selected_length) {
  std::vector<MessagePlan> messages;
  std::vector<std::size_t> indices;
  messages.reserve(message_count);
  indices.reserve(message_count);
  for (std::size_t index = 0U; index < message_count; ++index) {
    const std::size_t frame_size = index + 1U;
    MessagePlan message;
    message.id = "message_" + std::to_string(index);
    message.direction_id = "operation_direction";
    message.frame_length_bytes = frame_size;
    message.matchers.push_back(LengthMatcher(frame_size));
    FieldPlan payload;
    payload.id = "payload";
    payload.value_type = ValueType::BYTES;
    payload.wire_codec = WireCodec::BYTES;
    payload.byte_offset = 0U;
    payload.byte_width = frame_size;
    payload.byte_order = ByteOrder::NOT_APPLICABLE;
    payload.encode_source = EncodeSource::INPUT;
    message.fields.push_back(std::move(payload));
    messages.push_back(std::move(message));
    indices.push_back(index);
  }
  PlanBuildResult frozen = PlanBuilder::Freeze(MakeDraft(std::move(messages), std::move(indices)));
  if (!frozen.Succeeded()) {
    return false;
  }
  ExecutionWorkspace workspace{*frozen.plan};
  std::vector<std::uint8_t> frame(selected_length);
  std::array<DecodedFieldSlot, 1U> slots{};
  const auto decoded =
      DecodeCompleteRecord(*frozen.plan, workspace, 0U, ByteView{frame.data(), frame.size()},
                           slots.data(), slots.size());
  const CodecOperationCounts counts = workspace.LastOperationCounts();
  const std::size_t maximum_search_steps = CeilLog2(message_count) + 1U;
  return decoded.status == CodecStatus::OK && decoded.message_index == selected_length - 1U &&
         counts.candidate_group_search_steps != 0U &&
         counts.candidate_group_search_steps <= maximum_search_steps &&
         counts.candidate_messages_examined == 1U && counts.static_plan_validation_visits == 0U;
}

bool CheckFixedAndBytesCount(std::size_t payload_size) {
  constexpr std::size_t kFixedByteCount = 4U;
  const std::size_t frame_size = kFixedByteCount + payload_size;
  MessagePlan message;
  message.id = "bytes_message";
  message.direction_id = "operation_direction";
  message.frame_length_bytes = frame_size;
  message.matchers.push_back(LengthMatcher(frame_size));

  MatcherPlan fixed_matcher;
  fixed_matcher.kind = MatcherKind::FIXED_BYTES;
  fixed_matcher.byte_offset = 0U;
  fixed_matcher.bytes = {0xA5U, 0x5AU, 0x11U, 0x22U};
  message.matchers.push_back(std::move(fixed_matcher));

  FieldPlan payload;
  payload.id = "payload";
  payload.value_type = ValueType::BYTES;
  payload.wire_codec = WireCodec::BYTES;
  payload.byte_offset = kFixedByteCount;
  payload.byte_width = payload_size;
  payload.byte_order = ByteOrder::NOT_APPLICABLE;
  payload.encode_source = EncodeSource::INPUT;
  message.fields.push_back(std::move(payload));

  PlanBuildResult frozen = PlanBuilder::Freeze(MakeDraft({std::move(message)}, {0U}));
  if (!frozen.Succeeded()) {
    return false;
  }
  ExecutionWorkspace workspace{*frozen.plan};
  std::vector<std::uint8_t> payload_bytes(payload_size);
  for (std::size_t index = 0U; index < payload_size; ++index) {
    payload_bytes[index] = static_cast<std::uint8_t>(index & 0xFFU);
  }
  EncodeFieldValue value;
  value.field = FieldRef{frozen.plan.get(), 0U, 0U};
  value.value_kind = LogicalValueKind::BYTES;
  value.bytes_value = ByteView{payload_bytes.data(), payload_bytes.size()};
  std::vector<std::uint8_t> output(frame_size, 0xCCU);
  const auto encoded = EncodeCompleteRecord(*frozen.plan, workspace, 0U, 0U, &value, 1U,
                                            MutableByteBuffer{output.data(), output.size()});
  const CodecOperationCounts counts = workspace.LastOperationCounts();
  return encoded.status == CodecStatus::OK && encoded.bytes_written == frame_size &&
         counts.input_values_visited == 1U && counts.field_validation_visits == 1U &&
         counts.alias_validation_visits == 1U && counts.required_field_scan_visits == 1U &&
         counts.field_write_visits == 1U && counts.field_verify_visits == 1U &&
         counts.fixed_byte_write_visits == kFixedByteCount &&
         counts.bytes_write_visits == payload_size && counts.bytes_verify_visits == payload_size &&
         counts.presence_word_clear_visits == 1U && counts.candidate_group_search_steps == 1U &&
         counts.candidate_messages_examined == 1U &&
         counts.matcher_bytes_compared == kFixedByteCount &&
         counts.static_plan_validation_visits == 0U;
}

bool CheckEnumLookup() {
  constexpr std::size_t kEntryCount = 256U;
  MessagePlan message;
  message.id = "enum_message";
  message.direction_id = "operation_direction";
  message.frame_length_bytes = 1U;
  message.matchers.push_back(LengthMatcher(1U));
  FieldPlan field = MakeByteField("enum_value", 0U, ValueType::ENUM);
  field.unknown_enum_policy = UnknownEnumPolicy::REJECT;
  field.enum_entries.reserve(kEntryCount);
  for (std::size_t index = 0U; index < kEntryCount; ++index) {
    field.enum_entries.push_back(
        EnumEntryPlan{"entry_" + std::to_string(index), kEntryCount - 1U - index});
  }
  message.fields.push_back(std::move(field));

  PlanBuildResult frozen = PlanBuilder::Freeze(MakeDraft({std::move(message)}, {0U}));
  if (!frozen.Succeeded()) {
    return false;
  }
  ExecutionWorkspace workspace{*frozen.plan};
  constexpr std::array<std::uint8_t, 1U> frame{0xFFU};
  std::array<DecodedFieldSlot, 1U> slots{};
  const auto decoded =
      DecodeCompleteRecord(*frozen.plan, workspace, 0U, ByteView{frame.data(), frame.size()},
                           slots.data(), slots.size());
  const CodecOperationCounts counts = workspace.LastOperationCounts();
  const std::size_t maximum_two_search_steps = 2U * (CeilLog2(kEntryCount) + 1U);
  return decoded.status == CodecStatus::OK && slots[0].enum_value.known &&
         slots[0].enum_value.reference.entry_index == 0U &&
         slots[0].enum_value.raw_value == 0xFFU && counts.enum_search_steps != 0U &&
         counts.enum_search_steps <= maximum_two_search_steps &&
         counts.static_plan_validation_visits == 0U;
}

}  // namespace

int main() {
  TestRunner runner;
  runner.Record(
      "linear_field_operation_counts",
      CheckLinearFieldCount(16U) && CheckLinearFieldCount(64U) && CheckLinearFieldCount(256U),
      "field-dependent Encode/Decode visits were not linear or static validation ran");
  runner.Record("frame_length_candidate_group_operation_counts",
                CheckCandidateGroupCount(8U, 7U) && CheckCandidateGroupCount(32U, 31U),
                "Decode examined messages outside the selected frame-length candidate group");
  runner.Record("fixed_and_bytes_operation_counts",
                CheckFixedAndBytesCount(8U) && CheckFixedAndBytesCount(128U),
                "fixed matcher or BYTES Encode/verify visits were not exact and linear");
  runner.Record("logarithmic_enum_lookup_preserves_entry_index", CheckEnumLookup(),
                "Enum lookup was not bounded binary search or lost the configured entry index");
  return runner.Finish();
}
