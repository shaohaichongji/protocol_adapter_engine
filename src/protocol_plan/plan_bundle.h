#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "plan_types.h"

namespace pae::protocol_plan {

class PlanBuilder;

struct FramingPlan {
  std::string id;
  InputKind input_kind = InputKind::COMPLETE_RECORD;
};

struct MatcherPlan {
  MatcherKind kind = MatcherKind::FRAME_LENGTH_EQUALS;
  std::uint64_t length_bytes = 0U;
  std::uint64_t byte_offset = 0U;
  std::vector<std::uint8_t> bytes;
};

struct EnumEntryPlan {
  std::string id;
  std::uint64_t raw_value = 0U;
};

struct FieldPlan {
  std::string id;
  ValueType value_type = ValueType::UINT64;
  WireCodec wire_codec = WireCodec::UNSIGNED_INTEGER;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  EncodeSource encode_source = EncodeSource::INPUT;
  std::optional<std::uint64_t> constant_value;
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  std::vector<EnumEntryPlan> enum_entries;
};

struct MessagePlan {
  std::string id;
  std::string direction_id;
  std::uint64_t frame_length_bytes = 0U;
  std::vector<MatcherPlan> matchers;
  std::vector<FieldPlan> fields;
};

struct PipelinePlan {
  std::string id;
  std::string direction_id;
  std::size_t framing_profile_index = 0U;
  std::vector<std::size_t> message_indices;
};

struct FixedByteExecutionPlan {
  std::size_t offset = 0U;
  std::uint8_t value = 0U;
};

struct EnumLookupExecutionPlan {
  std::uint64_t raw_value = 0U;
  std::size_t entry_index = 0U;
};

struct FieldExecutionPlan {
  std::size_t offset = 0U;
  std::size_t width = 0U;
  std::size_t input_ordinal = 0U;
  ValueType value_type = ValueType::UINT64;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  EncodeSource encode_source = EncodeSource::INPUT;
  bool has_constant = false;
  std::uint64_t constant_value = 0U;
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  std::size_t enum_values_begin = 0U;
  std::size_t enum_values_count = 0U;
  std::size_t enum_lookup_begin = 0U;
  std::size_t enum_lookup_count = 0U;
};

struct MessageExecutionPlan {
  std::size_t frame_size = 0U;
  std::size_t required_input_count = 0U;
  std::vector<FixedByteExecutionPlan> fixed_bytes;
  std::vector<FieldExecutionPlan> fields;
  std::vector<std::uint64_t> enum_raw_values;
  std::vector<EnumLookupExecutionPlan> enum_lookup_entries;
};

struct CandidateGroupExecutionPlan {
  std::size_t frame_size = 0U;
  std::vector<std::size_t> message_indices;
};

struct PipelineExecutionPlan {
  std::size_t framing_profile_index = 0U;
  std::vector<std::uint64_t> allowed_message_words;
  std::vector<CandidateGroupExecutionPlan> candidate_groups;
};

struct ExecutionResourceLayout {
  std::size_t max_fields_per_message = 0U;
  std::size_t max_input_fields_per_message = 0U;
  std::size_t encode_value_index_count = 0U;
  std::size_t encode_presence_word_count = 0U;
  std::size_t estimated_workspace_bytes = 0U;
};

class PlanBundle final {
 public:
  PlanBundle(const PlanBundle&) = delete;
  PlanBundle& operator=(const PlanBundle&) = delete;
  PlanBundle(PlanBundle&&) noexcept = delete;
  PlanBundle& operator=(PlanBundle&&) noexcept = delete;
  ~PlanBundle() = default;

  const std::string& SchemaVersion() const noexcept;
  const std::string& ProtocolId() const noexcept;
  const std::string& ProtocolVersion() const noexcept;
  ResourceProfile GetResourceProfile() const noexcept;
  const ResourceRequirements& GetResourceRequirements() const noexcept;
  const std::vector<FramingPlan>& FramingProfiles() const noexcept;
  const std::vector<PipelinePlan>& Pipelines() const noexcept;
  const std::vector<MessagePlan>& Messages() const noexcept;
  const ExecutionResourceLayout& GetExecutionResourceLayout() const noexcept;
  const std::vector<MessageExecutionPlan>& MessageExecutionPlans() const noexcept;
  const std::vector<PipelineExecutionPlan>& PipelineExecutionPlans() const noexcept;

 private:
  friend class PlanBuilder;

  PlanBundle(std::string schema_version, std::string protocol_id, std::string protocol_version,
             ResourceProfile resource_profile, ResourceRequirements resource_requirements,
             std::vector<FramingPlan> framing_profiles, std::vector<PipelinePlan> pipelines,
             std::vector<MessagePlan> messages, ExecutionResourceLayout execution_resource_layout,
             std::vector<MessageExecutionPlan> message_execution_plans,
             std::vector<PipelineExecutionPlan> pipeline_execution_plans);

  std::string schema_version_;
  std::string protocol_id_;
  std::string protocol_version_;
  ResourceProfile resource_profile_ = ResourceProfile::DESKTOP;
  ResourceRequirements resource_requirements_;
  std::vector<FramingPlan> framing_profiles_;
  std::vector<PipelinePlan> pipelines_;
  std::vector<MessagePlan> messages_;
  ExecutionResourceLayout execution_resource_layout_;
  std::vector<MessageExecutionPlan> message_execution_plans_;
  std::vector<PipelineExecutionPlan> pipeline_execution_plans_;
};

}  // namespace pae::protocol_plan
