#pragma once

#include <string>
#include <vector>

#include "config_compiler.h"
#include "plan_bundle.h"

namespace pae::test_support {

// Serializes synthetic plan components into the strict JSON slice and invokes the real compiler.
// This keeps tests on the same Structural -> Domain -> Budget -> PlanBuilder capability chain as
// production instead of retaining a raw PlanDraft bypass.
config_compiler::CompileResult CompileTestPlan(
    std::string protocol_id, protocol_plan::ResourceProfile resource_profile,
    std::vector<protocol_plan::FramingPlan> framing_profiles,
    std::vector<protocol_plan::PipelinePlan> pipelines,
    std::vector<protocol_plan::MessagePlan> messages);

config_compiler::CompileResult CloneTestPlan(const protocol_plan::PlanBundle& source,
                                             std::vector<protocol_plan::PipelinePlan> pipelines,
                                             std::vector<protocol_plan::MessagePlan> messages);

std::vector<protocol_plan::PipelinePlan> CloneMutablePipelines(
    const protocol_plan::PlanBundle& source);

std::vector<protocol_plan::MessagePlan> CloneMutableMessages(
    const protocol_plan::PlanBundle& source);

}  // namespace pae::test_support
