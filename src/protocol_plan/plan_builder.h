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

// 诊断中未定位到相应描述时使用哨兵，不是可访问的数组索引。
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

// 索引定位 Draft 内的对象，不是原 JSON Pointer；编译器边界再映射为阶段诊断。
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
// 正常生产链路由 PlanDraftAssembler 接管预算凭证后构造；测试友元可注入损坏 Draft。
// 包装拥有可变草稿，不意味着已经冻结；移动转移唯一所有权，空源不能再次消费。
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

// 成功结果拥有 PlanOwner；Plan() 借用随 owner 存活，Diagnostic() 借用随结果存活。
// TakePlan 后源 Succeeded() 为 false；Plan 地址不变，已有借用的寿命改由新 owner 保障。
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
  // 消费唯一 Draft，复核内部不变量、预计算和布局后才发布完整 owner。
  // noexcept 边界把 bad_alloc 映射为 ALLOCATION_FAILED，其余异常映射为 INTERNAL_ERROR。
  [[nodiscard]] static PlanBuildResult Freeze(BudgetedPlanDraft draft) noexcept;

 private:
  static PlanBuildResult FreezeImpl(detail::PlanDraftData draft);
};

}  // namespace pae::protocol_plan
