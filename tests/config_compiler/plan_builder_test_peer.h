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
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
protocol_plan::BudgetedPlanDraft MakeCrcDraftWithInvalidWidth();
protocol_plan::BudgetedPlanDraft MakeCrcDraftWithEvenPolynomial();
protocol_plan::BudgetedPlanDraft MakeCrcDraftWithInvalidStorageOrder();
protocol_plan::BudgetedPlanDraft MakeCrcDraftWithOldSchema();
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
protocol_plan::BudgetedPlanDraft MakeLengthDraftWithIncorrectExpectedValue();
protocol_plan::BudgetedPlanDraft MakeLengthDraftWithInvalidScope();
protocol_plan::BudgetedPlanDraft MakeLengthDraftWithOldSchema();
protocol_plan::BudgetedPlanDraft MakeLengthDraftWithResourceCountMismatch();
#endif
protocol_plan::BudgetedPlanDraft MakeInt64DraftWithOutOfRangeConstant();
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
enum class ConversionDraftMutation {
  CORRUPTED_COEFFICIENT,
  INVALID_INDEX,
  RAW_TYPE_MISMATCH,
  UNREFERENCED_DESCRIPTOR,
  DUPLICATE_REFERENCE,
  OLD_SCHEMA_RESIDUE,
  BITFIELD_REFERENCE,
  CONSTANT_REFERENCE,
  RESOURCE_COUNT_MISMATCH,
};

protocol_plan::BudgetedPlanDraft MutateConversionDraft(protocol_plan::BudgetedPlanDraft draft,
                                                       ConversionDraftMutation mutation);
#endif
protocol_plan::BudgetedPlanDraft SetDraftSchemaVersion(protocol_plan::BudgetedPlanDraft draft,
                                                       const char* version);
protocol_plan::BudgetedPlanDraft InjectSignedConstant(protocol_plan::BudgetedPlanDraft draft);

protocol_plan::BudgetedPlanDraft ConfigurePlanMemoryFailure(
    protocol_plan::BudgetedPlanDraft draft, std::size_t fail_at_allocation,
    protocol_plan::test_only::PlanMemoryTestProbe* probe);

protocol_plan::BudgetedPlanDraft CorruptApprovedPlanMemory(protocol_plan::BudgetedPlanDraft draft);

}  // namespace pae::test_support
