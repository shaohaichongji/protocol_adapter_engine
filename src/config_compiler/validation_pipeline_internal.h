#pragma once

#include <optional>
#include <utility>

#include "../protocol_plan/plan_builder.h"
#include "config_compiler.h"

namespace pae::config_compiler {

// move-only 阶段结果：正常构造时是凭证或诊断二选一，不能凭原始 IR 绕过校验阶段。
// 凭证拥有相应阶段数据，取出后应移交下一阶段，不把内部引用跨移动保存。
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

  // 成功才可取 Capability，失败且有诊断才可取 Diagnostic；空 optional 不可解引用。
  // Take 只移动 contained value、不 reset 标记；提取后不要再靠 Succeeded() 重复消费。
  Capability TakeCapability() && { return std::move(*capability_); }
  CompileDiagnostic TakeDiagnostic() && { return std::move(*diagnostic_); }

 private:
  CapabilityResult(std::optional<Capability> capability,
                   std::optional<CompileDiagnostic> diagnostic)
      : capability_(std::move(capability)), diagnostic_(std::move(diagnostic)) {}

  std::optional<Capability> capability_;
  std::optional<CompileDiagnostic> diagnostic_;
};

// 类型链限定允许推进的顺序：SchemaIr → ValidatedSchemaIr → BudgetedSchemaIr → BudgetedPlanDraft。
// 各阶段构造权限由对应 IR/Draft 类型控制，不是命名约定或可随意复制的“已通过”布尔值。
using DomainValidationResult = CapabilityResult<ValidatedSchemaIr>;
using ResourceBudgetResult = CapabilityResult<BudgetedSchemaIr>;
using PlanDraftAssemblyResult = CapabilityResult<protocol_plan::BudgetedPlanDraft>;

// 接管结构 IR，验证引用、布局与领域约束；成功发布 Validated 凭证，尚未批准资源预算。
class DomainValidator final {
 public:
  DomainValidator() = delete;
  static DomainValidationResult Validate(SchemaIr schema);
};

// 仅接收 Validated 凭证，估算并批准 Plan 逻辑预算；不是执行 Workspace 或 RSS 计费。
// ValidateForTest 的显式上限用于现有内部测试，不是生产公共配置入口。
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

// 接管 Budgeted 凭证并组装可冻结草稿；这里还没有发布不可变 Plan。
class PlanDraftAssembler final {
 public:
  PlanDraftAssembler() = delete;
  static PlanDraftAssemblyResult Assemble(BudgetedSchemaIr budgeted);
};

// 最终由 PlanBuilder 冻结并复核；把 Builder 失败映射为编译诊断，成功仅发布 Plan owner。
CompileResult FreezeBudgetedPlanDraft(protocol_plan::BudgetedPlanDraft draft);

std::size_t JsonParserPoolUpperBoundForTest(std::size_t input_size) noexcept;

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
CompileResult CompileJsonToPlanWithPlanMemoryLimitForTest(std::string_view json_bytes,
                                                          std::size_t plan_memory_limit_bytes);
#endif

}  // namespace pae::config_compiler
