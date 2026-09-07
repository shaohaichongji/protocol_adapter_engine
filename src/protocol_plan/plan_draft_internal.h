#pragma once

#include <string>
#include <vector>

#include "plan_bundle.h"

namespace pae::protocol_plan::detail {

// Raw plan material is an internal payload. Production PlanBuilder entry points never accept this
// type directly; it can only be wrapped by the configuration compiler after validation and budget
// admission have both succeeded.
struct PlanDraftData {
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
  ResourceRequirements resource_requirements;
  std::vector<FramingPlan> framing_profiles;
  std::vector<PipelinePlan> pipelines;
  std::vector<MessagePlan> messages;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::vector<LinearConversionDescriptor> conversions;
#endif
  PlanMemoryReport approved_plan_memory;
  std::size_t plan_memory_limit_bytes = 0U;
  std::size_t test_fail_at_allocation = 0U;
  test_only::PlanMemoryTestProbe* test_memory_probe = nullptr;
};

struct PreparedMessageExecutionPlan {
  std::size_t frame_size = 0U;
  std::size_t required_input_count = 0U;
  std::optional<FrozenIntegrityPlan> integrity;
  std::vector<FixedByteExecutionPlan> fixed_bytes;
  std::vector<BitContainerExecutionPlan> bit_containers;
  std::vector<FieldExecutionPlan> fields;
  std::vector<std::uint64_t> enum_raw_values;
  std::vector<EnumLookupExecutionPlan> enum_lookup_entries;
};

struct PreparedCandidateGroupExecutionPlan {
  std::size_t frame_size = 0U;
  std::vector<std::size_t> message_indices;
};

struct PreparedPipelineExecutionPlan {
  std::size_t framing_profile_index = 0U;
  std::vector<std::uint64_t> allowed_message_words;
  std::vector<PreparedCandidateGroupExecutionPlan> candidate_groups;
};

}  // namespace pae::protocol_plan::detail
