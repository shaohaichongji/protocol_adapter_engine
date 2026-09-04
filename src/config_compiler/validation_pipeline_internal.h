#pragma once

#include <optional>
#include <utility>

#include "../protocol_plan/plan_builder.h"
#include "config_compiler.h"

namespace pae::config_compiler {

template <typename Capability>
class CapabilityResult final {
 public:
  CapabilityResult() = delete;
  CapabilityResult(const CapabilityResult&) = delete;
  CapabilityResult& operator=(const CapabilityResult&) = delete;
  CapabilityResult(CapabilityResult&&) noexcept = default;
  CapabilityResult& operator=(CapabilityResult&&) noexcept = default;
  ~CapabilityResult() = default;

  static CapabilityResult Success(Capability capability) {
    return CapabilityResult{std::move(capability), std::nullopt};
  }

  static CapabilityResult Failure(CompileDiagnostic diagnostic) {
    return CapabilityResult{std::nullopt, std::move(diagnostic)};
  }

  bool Succeeded() const noexcept { return capability_.has_value() && !diagnostic_.has_value(); }
  const CompileDiagnostic* Diagnostic() const noexcept {
    return diagnostic_.has_value() ? &*diagnostic_ : nullptr;
  }

  Capability TakeCapability() && { return std::move(*capability_); }
  CompileDiagnostic TakeDiagnostic() && { return std::move(*diagnostic_); }

 private:
  CapabilityResult(std::optional<Capability> capability,
                   std::optional<CompileDiagnostic> diagnostic)
      : capability_(std::move(capability)), diagnostic_(std::move(diagnostic)) {}

  std::optional<Capability> capability_;
  std::optional<CompileDiagnostic> diagnostic_;
};

using DomainValidationResult = CapabilityResult<ValidatedSchemaIr>;
using ResourceBudgetResult = CapabilityResult<BudgetedSchemaIr>;
using PlanDraftAssemblyResult = CapabilityResult<protocol_plan::BudgetedPlanDraft>;

class DomainValidator final {
 public:
  DomainValidator() = delete;
  static DomainValidationResult Validate(SchemaIr schema);
};

class ResourceBudgetValidator final {
 public:
  ResourceBudgetValidator() = delete;
  static ResourceBudgetResult Validate(ValidatedSchemaIr validated);
  static ResourceBudgetResult ValidateForTest(ValidatedSchemaIr validated,
                                              std::size_t plan_memory_limit_bytes);

 private:
  static ResourceBudgetResult ValidateImpl(ValidatedSchemaIr validated,
                                           std::optional<std::size_t> plan_memory_limit_bytes);
};

class PlanDraftAssembler final {
 public:
  PlanDraftAssembler() = delete;
  static PlanDraftAssemblyResult Assemble(BudgetedSchemaIr budgeted);
};

CompileResult FreezeBudgetedPlanDraft(protocol_plan::BudgetedPlanDraft draft);

}  // namespace pae::config_compiler
