#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "plan_bundle.h"

namespace pae::protocol_plan {
class BudgetedPlanDraft;

namespace test_only {
class PlanBuilderTestPeer;
}
}  // namespace pae::protocol_plan

namespace pae::config_compiler {
class PlanDraftAssembler;
}

namespace pae::protocol_plan {

namespace detail {
struct PlanDraftData;
}

inline constexpr std::size_t kInvalidPlanBuildIndex = (std::numeric_limits<std::size_t>::max)();

enum class PlanBuildError {
  NONE,
  INVALID_METADATA,
  RESOURCE_LIMIT_EXCEEDED,
  RESOURCE_REQUIREMENTS_MISMATCH,
  INVALID_FRAMING_PLAN,
  INVALID_PIPELINE_PLAN,
  INVALID_MESSAGE_PLAN,
  INVALID_MATCHER_PLAN,
  INVALID_FIELD_PLAN,
  FRAME_NOT_FULLY_DEFINED,
  AMBIGUOUS_MATCHER,
  PLAN_MEMORY_LIMIT_EXCEEDED,
  PLAN_MEMORY_ESTIMATE_MISMATCH,
  ALLOCATION_FAILED,
  INTERNAL_ERROR,
};

struct PlanBuildDiagnostic {
  PlanBuildError code = PlanBuildError::INTERNAL_ERROR;
  std::size_t framing_index = kInvalidPlanBuildIndex;
  std::size_t pipeline_index = kInvalidPlanBuildIndex;
  std::size_t message_index = kInvalidPlanBuildIndex;
  std::size_t matcher_index = kInvalidPlanBuildIndex;
  std::size_t field_index = kInvalidPlanBuildIndex;
  std::size_t required_bytes = 0U;
  std::size_t limit_bytes = 0U;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
};

// A move-only capability minted only after domain validation and resource-budget validation.
// The raw PlanDraft payload is intentionally hidden from the production PlanBuilder API.
class BudgetedPlanDraft final {
 public:
  BudgetedPlanDraft() = delete;
  BudgetedPlanDraft(const BudgetedPlanDraft&) = delete;
  BudgetedPlanDraft& operator=(const BudgetedPlanDraft&) = delete;
  BudgetedPlanDraft(BudgetedPlanDraft&& other) noexcept;
  BudgetedPlanDraft& operator=(BudgetedPlanDraft&& other) noexcept;
  ~BudgetedPlanDraft();

 private:
  explicit BudgetedPlanDraft(std::unique_ptr<detail::PlanDraftData> draft) noexcept;

  friend class pae::config_compiler::PlanDraftAssembler;
  friend class PlanBuilder;
  friend class test_only::PlanBuilderTestPeer;

  std::unique_ptr<detail::PlanDraftData> draft_;
};

class PlanBuildResult final {
 public:
  PlanBuildResult() = delete;
  PlanBuildResult(const PlanBuildResult&) = delete;
  PlanBuildResult& operator=(const PlanBuildResult&) = delete;
  PlanBuildResult(PlanBuildResult&&) noexcept = default;
  PlanBuildResult& operator=(PlanBuildResult&&) noexcept = default;
  ~PlanBuildResult() = default;

  static PlanBuildResult Success(PlanOwner plan) noexcept {
    if (!plan) {
      return Failure(PlanBuildDiagnostic{PlanBuildError::INTERNAL_ERROR});
    }
    return PlanBuildResult{std::move(plan)};
  }
  static PlanBuildResult Failure(PlanBuildDiagnostic diagnostic) {
    return PlanBuildResult{std::move(diagnostic)};
  }

  bool Succeeded() const noexcept { return static_cast<bool>(plan_); }
  const PlanBundle* Plan() const noexcept { return plan_.get(); }
  const PlanBuildDiagnostic* Diagnostic() const noexcept {
    return diagnostic_.has_value() ? &*diagnostic_ : nullptr;
  }
  PlanOwner TakePlan() && noexcept { return std::move(plan_); }

 private:
  explicit PlanBuildResult(PlanOwner plan) noexcept : plan_(std::move(plan)) {}
  explicit PlanBuildResult(PlanBuildDiagnostic diagnostic) : diagnostic_(std::move(diagnostic)) {}

  PlanOwner plan_;
  std::optional<PlanBuildDiagnostic> diagnostic_;
};

class PlanBuilder final {
 public:
  [[nodiscard]] static PlanBuildResult Freeze(BudgetedPlanDraft draft) noexcept;

 private:
  static PlanBuildResult FreezeImpl(detail::PlanDraftData draft);
};

}  // namespace pae::protocol_plan
