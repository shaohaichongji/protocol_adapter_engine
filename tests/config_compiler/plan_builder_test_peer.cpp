#include "plan_builder_test_peer.h"

#include <memory>
#include <utility>

#include "../../src/protocol_plan/plan_draft_internal.h"

namespace pae::protocol_plan::test_only {

class PlanBuilderTestPeer final {
 public:
  static BudgetedPlanDraft MakeCorruptedBudgetedPlanDraft() {
    auto draft = std::make_unique<detail::PlanDraftData>();
    draft->schema_version = "0.1";
    draft->protocol_id = "corrupted_test_draft";
    draft->protocol_version = "1";
    draft->resource_profile = ResourceProfile::DESKTOP;
    return BudgetedPlanDraft{std::move(draft)};
  }

  static BudgetedPlanDraft ConfigurePlanMemoryFailure(BudgetedPlanDraft draft,
                                                      std::size_t fail_at_allocation,
                                                      test_only::PlanMemoryTestProbe* probe) {
    if (draft.draft_ != nullptr) {
      draft.draft_->test_fail_at_allocation = fail_at_allocation;
      draft.draft_->test_memory_probe = probe;
    }
    return draft;
  }

  static BudgetedPlanDraft CorruptApprovedPlanMemory(BudgetedPlanDraft draft) {
    if (draft.draft_ != nullptr) {
      ++draft.draft_->approved_plan_memory.string_bytes;
    }
    return draft;
  }
};

}  // namespace pae::protocol_plan::test_only

namespace pae::test_support {

protocol_plan::BudgetedPlanDraft MakeCorruptedBudgetedPlanDraft() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCorruptedBudgetedPlanDraft();
}

protocol_plan::BudgetedPlanDraft ConfigurePlanMemoryFailure(
    protocol_plan::BudgetedPlanDraft draft, std::size_t fail_at_allocation,
    protocol_plan::test_only::PlanMemoryTestProbe* probe) {
  return protocol_plan::test_only::PlanBuilderTestPeer::ConfigurePlanMemoryFailure(
      std::move(draft), fail_at_allocation, probe);
}

protocol_plan::BudgetedPlanDraft CorruptApprovedPlanMemory(protocol_plan::BudgetedPlanDraft draft) {
  return protocol_plan::test_only::PlanBuilderTestPeer::CorruptApprovedPlanMemory(std::move(draft));
}

}  // namespace pae::test_support
