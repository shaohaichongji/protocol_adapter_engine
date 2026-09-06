#pragma once

#include "../../src/protocol_plan/plan_builder.h"

namespace pae::test_support {

// Test-only fault injection. This declaration is compiled only into the config-compiler contract
// runner and is never part of a product target or installed/exported header set.
protocol_plan::BudgetedPlanDraft MakeCorruptedBudgetedPlanDraft();
protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithInvalidByteOrder();
protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithInvalidBitNumbering();
protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithMatcherConflict();
protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithTooManyMessageContainers();
protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithTooManyTotalContainers();
protocol_plan::BudgetedPlanDraft MakeIntegrityDraftWithUnknownAlgorithm();
protocol_plan::BudgetedPlanDraft MakeIntegrityDraftWithSelfIncludedStorage();
protocol_plan::BudgetedPlanDraft MakeIntegrityDraftWithFieldStorageConflict();

protocol_plan::BudgetedPlanDraft ConfigurePlanMemoryFailure(
    protocol_plan::BudgetedPlanDraft draft, std::size_t fail_at_allocation,
    protocol_plan::test_only::PlanMemoryTestProbe* probe);

protocol_plan::BudgetedPlanDraft CorruptApprovedPlanMemory(protocol_plan::BudgetedPlanDraft draft);

}  // namespace pae::test_support
