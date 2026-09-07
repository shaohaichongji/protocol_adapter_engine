#include "plan_bundle.h"

#include <utility>

namespace pae::protocol_plan {

PlanBundle::PlanBundle(FrozenString schema_version, FrozenString protocol_id,
                       FrozenString protocol_version, ResourceProfile resource_profile,
                       ResourceRequirements resource_requirements,
                       FrozenArray<FrozenFramingPlan> framing_profiles,
                       FrozenArray<FrozenPipelinePlan> pipelines,
                       FrozenArray<FrozenMessagePlan> messages,
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                       FrozenArray<LinearConversionDescriptor> conversions,
#endif
                       ExecutionResourceLayout execution_resource_layout,
                       FrozenArray<MessageExecutionPlan> message_execution_plans,
                       FrozenArray<PipelineExecutionPlan> pipeline_execution_plans,
                       PlanMemoryReport memory_report) noexcept
    : schema_version_(std::move(schema_version)),
      protocol_id_(std::move(protocol_id)),
      protocol_version_(std::move(protocol_version)),
      resource_profile_(resource_profile),
      resource_requirements_(resource_requirements),
      framing_profiles_(std::move(framing_profiles)),
      pipelines_(std::move(pipelines)),
      messages_(std::move(messages)),
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      conversions_(std::move(conversions)),
#endif
      execution_resource_layout_(execution_resource_layout),
      message_execution_plans_(std::move(message_execution_plans)),
      pipeline_execution_plans_(std::move(pipeline_execution_plans)),
      memory_report_(memory_report) {
}

std::string_view PlanBundle::SchemaVersion() const noexcept { return schema_version_.View(); }

std::string_view PlanBundle::ProtocolId() const noexcept { return protocol_id_.View(); }

std::string_view PlanBundle::ProtocolVersion() const noexcept { return protocol_version_.View(); }

ResourceProfile PlanBundle::GetResourceProfile() const noexcept { return resource_profile_; }

const ResourceRequirements& PlanBundle::GetResourceRequirements() const noexcept {
  return resource_requirements_;
}

const FrozenArray<FrozenFramingPlan>& PlanBundle::FramingProfiles() const noexcept {
  return framing_profiles_;
}

const FrozenArray<FrozenPipelinePlan>& PlanBundle::Pipelines() const noexcept { return pipelines_; }

const FrozenArray<FrozenMessagePlan>& PlanBundle::Messages() const noexcept { return messages_; }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
const FrozenArray<LinearConversionDescriptor>& PlanBundle::Conversions() const noexcept {
  return conversions_;
}
#endif

const ExecutionResourceLayout& PlanBundle::GetExecutionResourceLayout() const noexcept {
  return execution_resource_layout_;
}

const FrozenArray<MessageExecutionPlan>& PlanBundle::MessageExecutionPlans() const noexcept {
  return message_execution_plans_;
}

const FrozenArray<PipelineExecutionPlan>& PlanBundle::PipelineExecutionPlans() const noexcept {
  return pipeline_execution_plans_;
}

const PlanMemoryReport& PlanBundle::GetPlanMemoryReport() const noexcept { return memory_report_; }

}  // namespace pae::protocol_plan
