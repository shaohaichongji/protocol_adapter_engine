#include "plan_bundle.h"

#include <utility>

namespace pae::protocol_plan {

PlanBundle::PlanBundle(std::string schema_version, std::string protocol_id,
                       std::string protocol_version, ResourceProfile resource_profile,
                       ResourceRequirements resource_requirements,
                       std::vector<FramingPlan> framing_profiles,
                       std::vector<PipelinePlan> pipelines, std::vector<MessagePlan> messages,
                       ExecutionResourceLayout execution_resource_layout,
                       std::vector<MessageExecutionPlan> message_execution_plans,
                       std::vector<PipelineExecutionPlan> pipeline_execution_plans)
    : schema_version_(std::move(schema_version)),
      protocol_id_(std::move(protocol_id)),
      protocol_version_(std::move(protocol_version)),
      resource_profile_(resource_profile),
      resource_requirements_(resource_requirements),
      framing_profiles_(std::move(framing_profiles)),
      pipelines_(std::move(pipelines)),
      messages_(std::move(messages)),
      execution_resource_layout_(execution_resource_layout),
      message_execution_plans_(std::move(message_execution_plans)),
      pipeline_execution_plans_(std::move(pipeline_execution_plans)) {}

const std::string& PlanBundle::SchemaVersion() const noexcept { return schema_version_; }

const std::string& PlanBundle::ProtocolId() const noexcept { return protocol_id_; }

const std::string& PlanBundle::ProtocolVersion() const noexcept { return protocol_version_; }

ResourceProfile PlanBundle::GetResourceProfile() const noexcept { return resource_profile_; }

const ResourceRequirements& PlanBundle::GetResourceRequirements() const noexcept {
  return resource_requirements_;
}

const std::vector<FramingPlan>& PlanBundle::FramingProfiles() const noexcept {
  return framing_profiles_;
}

const std::vector<PipelinePlan>& PlanBundle::Pipelines() const noexcept { return pipelines_; }

const std::vector<MessagePlan>& PlanBundle::Messages() const noexcept { return messages_; }

const ExecutionResourceLayout& PlanBundle::GetExecutionResourceLayout() const noexcept {
  return execution_resource_layout_;
}

const std::vector<MessageExecutionPlan>& PlanBundle::MessageExecutionPlans() const noexcept {
  return message_execution_plans_;
}

const std::vector<PipelineExecutionPlan>& PlanBundle::PipelineExecutionPlans() const noexcept {
  return pipeline_execution_plans_;
}

}  // namespace pae::protocol_plan
