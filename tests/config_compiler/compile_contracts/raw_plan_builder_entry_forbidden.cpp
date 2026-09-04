#include <utility>

#include "../../../src/protocol_plan/plan_builder.h"

auto BypassPlanBuilder() {
  pae::protocol_plan::PlanDraft draft;
  return pae::protocol_plan::PlanBuilder::Freeze(std::move(draft));
}
