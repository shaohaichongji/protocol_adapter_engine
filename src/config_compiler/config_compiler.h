#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "../protocol_plan/plan_bundle.h"
#include "schema_ir.h"

namespace pae::config_compiler {

using protocol_plan::EnumEntryPlan;
using protocol_plan::FieldPlan;
using protocol_plan::FramingPlan;
using protocol_plan::MatcherPlan;
using protocol_plan::MessagePlan;
using protocol_plan::PipelinePlan;
using protocol_plan::PlanBundle;

enum class CompileStage {
  INPUT_PROFILE,
  JSON_SYNTAX,
  JSON_RESOURCE,
  STRUCTURAL,
  DOMAIN_VALIDATION,
  RESOURCE_BUDGET,
  PLAN_BUILD,
  INTERNAL,
};

enum class CompileError {
  NONE,
  EMPTY_INPUT,
  INPUT_LIMIT_EXCEEDED,
  UTF8_BOM_NOT_ALLOWED,
  INVALID_UTF8,
  INVALID_UNICODE_ESCAPE,
  NESTING_DEPTH_LIMIT_EXCEEDED,
  JSON_SYNTAX_ERROR,
  JSON_ALLOCATION_FAILED,
  JSON_PARSER_MEMORY_LIMIT_EXCEEDED,
  JSON_NODE_LIMIT_EXCEEDED,
  OBJECT_MEMBER_LIMIT_EXCEEDED,
  ARRAY_ELEMENT_LIMIT_EXCEEDED,
  STRING_LIMIT_EXCEEDED,
  DECODED_STRING_BUDGET_EXCEEDED,
  NUMBER_TOKEN_LIMIT_EXCEEDED,
  DUPLICATE_KEY,
  ROOT_MUST_BE_OBJECT,
  MISSING_PROPERTY,
  UNKNOWN_PROPERTY,
  TYPE_MISMATCH,
  INVALID_STRING_LENGTH,
  INVALID_ID,
  INVALID_ENUM_VALUE,
  INTEGER_NOT_EXACT,
  INTEGER_OUT_OF_RANGE,
  INVALID_HEX_BYTES,
  EMPTY_ARRAY,
  UNSUPPORTED_FEATURE,
  DUPLICATE_ID,
  DUPLICATE_REFERENCE,
  UNKNOWN_REFERENCE,
  DIRECTION_MISMATCH,
  FIELD_OUT_OF_BOUNDS,
  FIELD_OVERLAP,
  FRAME_NOT_FULLY_DEFINED,
  VALUE_NOT_REPRESENTABLE,
  MATCHER_OUT_OF_BOUNDS,
  MATCHER_CONFLICT,
  AMBIGUOUS_MATCHER,
  RESOURCE_LIMIT_EXCEEDED,
  COMPILER_ALLOCATION_FAILED,
  INTERNAL_CONTRACT_VIOLATION,
};

enum class ResourceKind {
  NONE,
  PLAN_ACCOUNTED_MEMORY,
};

struct CompileDiagnostic {
  CompileStage stage = CompileStage::INTERNAL;
  CompileError code = CompileError::INTERNAL_CONTRACT_VIOLATION;
  std::string json_pointer;
  std::optional<std::size_t> byte_offset;
  std::string detail;
  ResourceKind resource_kind = ResourceKind::NONE;
  std::size_t required_bytes = 0U;
  std::size_t limit_bytes = 0U;
  protocol_plan::ResourceProfile resource_profile = protocol_plan::ResourceProfile::DESKTOP;
};

class CompileResult final {
 public:
  CompileResult() = delete;
  CompileResult(const CompileResult&) = delete;
  CompileResult& operator=(const CompileResult&) = delete;
  CompileResult(CompileResult&&) noexcept = default;
  CompileResult& operator=(CompileResult&&) noexcept = default;
  ~CompileResult() = default;

  static CompileResult Success(protocol_plan::PlanOwner plan) {
    if (!plan) {
      return Failure(CompileDiagnostic{CompileStage::INTERNAL,
                                       CompileError::INTERNAL_CONTRACT_VIOLATION, "", std::nullopt,
                                       "compiler success result has no plan"});
    }
    return CompileResult{std::move(plan)};
  }
  static CompileResult Failure(CompileDiagnostic diagnostic) {
    return CompileResult{std::move(diagnostic)};
  }

  bool Succeeded() const noexcept { return static_cast<bool>(plan_); }
  const PlanBundle* Plan() const noexcept { return plan_.get(); }
  const CompileDiagnostic* Diagnostic() const noexcept {
    return diagnostic_.has_value() ? &*diagnostic_ : nullptr;
  }
  protocol_plan::PlanOwner TakePlan() && noexcept { return std::move(plan_); }

 private:
  explicit CompileResult(protocol_plan::PlanOwner plan) noexcept : plan_(std::move(plan)) {}
  explicit CompileResult(CompileDiagnostic diagnostic) : diagnostic_(std::move(diagnostic)) {}

  protocol_plan::PlanOwner plan_;
  std::optional<CompileDiagnostic> diagnostic_;
};

// Internal V0.1 vertical slice. This is deliberately not installed or exported.
CompileResult CompileJsonToPlan(std::string_view json_bytes);
std::string MakeDeterministicPlanSnapshot(const PlanBundle& plan);

}  // namespace pae::config_compiler
