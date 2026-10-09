#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "../protocol_plan/plan_bundle.h"
#include "schema_ir.h"
#include "protocol_metadata.h"

namespace pae::config_compiler {

using protocol_plan::EnumEntryPlan;
using protocol_plan::FieldPlan;
using protocol_plan::FramingPlan;
using protocol_plan::MatcherPlan;
using protocol_plan::MessagePlan;
using protocol_plan::PipelinePlan;
using protocol_plan::PlanBundle;

// 内部失败发生阶段；与公开层显式映射，不靠两个枚举的整数值偶然一致。
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
  INTEGRITY_RANGE_OUT_OF_BOUNDS,
  INTEGRITY_STORAGE_OUT_OF_BOUNDS,
  INTEGRITY_SELF_INCLUDED,
  INTEGRITY_STORAGE_CONFLICT,
  AMBIGUOUS_MATCHER,
  RESOURCE_LIMIT_EXCEEDED,
  COMPILER_ALLOCATION_FAILED,
  INTERNAL_CONTRACT_VIOLATION,
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  ASCII_LITERAL_INVALID,
  ASCII_CONTROL_BYTE_INVALID,
  ASCII_FIELD_LENGTH_INVALID,
  ASCII_FIELD_REFERENCE_INVALID,
  ASCII_FIELD_BOUNDARY_AMBIGUOUS,
  ASCII_TEMPLATE_EMPTY,
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  ASCII_STREAM_TERMINATOR_INVALID,
  ASCII_STREAM_BOUNDARY_UNPROVEN,
  ASCII_STREAM_PROFILE_MISMATCH,
#endif
#endif
};

enum class ResourceKind {
  NONE,
  PLAN_ACCOUNTED_MEMORY,
  UI_DESCRIPTION_ACCOUNTED_MEMORY,
};

// 自有诊断文本；json_pointer 定位配置节点，byte_offset（若有）定位输入字节。
// resource_kind 决定计费字段是否有意义；预算是逻辑计费，不是进程 RSS 硬上限。
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

// Plan-only 入口的 move-only 结果，工厂构造时为 Plan 或诊断二选一，不含 metadata。
// Plan()/Diagnostic() 都借用本结果；移动或提取后不能继续使用原来的产物指针。
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
  // 成功后以右值转移 Plan 所有权；失败提取得到空 owner，正常流程先检查 Succeeded()。
  protocol_plan::PlanOwner TakePlan() && noexcept { return std::move(plan_); }

 private:
  explicit CompileResult(protocol_plan::PlanOwner plan) noexcept : plan_(std::move(plan)) {}
  explicit CompileResult(CompileDiagnostic diagnostic) : diagnostic_(std::move(diagnostic)) {}

  protocol_plan::PlanOwner plan_;
  std::optional<CompileDiagnostic> diagnostic_;
};

// 带描述入口的自有产物包：冻结 Plan、metadata 存储及构造时计费快照。
// 发布路径在组合前核验 Plan/metadata 对应关系，非空检查本身不替代该核验。
class CompiledProtocolArtifacts final {
 public:
  CompiledProtocolArtifacts() = delete;
  CompiledProtocolArtifacts(const CompiledProtocolArtifacts&) = delete;
  CompiledProtocolArtifacts& operator=(const CompiledProtocolArtifacts&) = delete;
  CompiledProtocolArtifacts(CompiledProtocolArtifacts&&) noexcept = default;
  CompiledProtocolArtifacts& operator=(CompiledProtocolArtifacts&&) noexcept = default;
  ~CompiledProtocolArtifacts() = default;

  const PlanBundle* Plan() const noexcept { return plan_.get(); }
  const ProtocolMetadataStorage& Description() const noexcept { return description_; }
  const DescriptionMemoryReport& DescriptionMemory() const noexcept { return description_memory_; }
  // 拆取会使本包不再完整；快照不是“拆取后仍拥有多少内存”的动态观察。
  // Description() 借用内部存储，拆取/移动后需要由新 owner 重新取得视图。
  protocol_plan::PlanOwner TakePlan() noexcept { return std::move(plan_); }
  ProtocolMetadataStorage TakeDescription() noexcept { return std::move(description_); }

 private:
  friend class CompileProtocolArtifactsResult;

  CompiledProtocolArtifacts(protocol_plan::PlanOwner plan, ProtocolMetadataStorage description) noexcept
      : plan_(std::move(plan)),
        description_memory_(description.MemoryReport()),
        description_(std::move(description)) {}

  protocol_plan::PlanOwner plan_;
  DescriptionMemoryReport description_memory_;
  ProtocolMetadataStorage description_;
};

// 正常构造时自有完整产物或自有诊断二选一；工厂拒绝空 Plan/空 metadata 的伪成功。
// 这是内部阶段结果，不是可以任意重复提取的共享句柄。
class CompileProtocolArtifactsResult final {
 public:
  CompileProtocolArtifactsResult() = delete;
  CompileProtocolArtifactsResult(const CompileProtocolArtifactsResult&) = delete;
  CompileProtocolArtifactsResult& operator=(const CompileProtocolArtifactsResult&) = delete;
  CompileProtocolArtifactsResult(CompileProtocolArtifactsResult&&) noexcept = default;
  CompileProtocolArtifactsResult& operator=(CompileProtocolArtifactsResult&&) noexcept = default;
  ~CompileProtocolArtifactsResult() = default;

  static CompileProtocolArtifactsResult Success(protocol_plan::PlanOwner plan,
                                          ProtocolMetadataStorage description) {
    if (!plan || description.empty()) {
      return Failure(CompileDiagnostic{CompileStage::INTERNAL,
                                       CompileError::INTERNAL_CONTRACT_VIOLATION, "", std::nullopt,
                                       "protocol metadata compiler success result is incomplete"});
    }
    return CompileProtocolArtifactsResult{CompiledProtocolArtifacts{std::move(plan), std::move(description)}};
  }
  static CompileProtocolArtifactsResult Failure(CompileDiagnostic diagnostic) {
    return CompileProtocolArtifactsResult{std::move(diagnostic)};
  }

  bool Succeeded() const noexcept { return artifacts_.has_value() && !diagnostic_.has_value(); }
  const CompiledProtocolArtifacts* Artifacts() const noexcept {
    return artifacts_.has_value() ? &*artifacts_ : nullptr;
  }
  const CompileDiagnostic* Diagnostic() const noexcept {
    return diagnostic_.has_value() ? &*diagnostic_ : nullptr;
  }
  // 必须先确认 Succeeded()，再一次性以右值提取；失败路径解引用空 optional 非法。
  // 移动 contained value 不会 reset optional，之后的 Succeeded() 不能证明产物仍完整。
  CompiledProtocolArtifacts TakeArtifacts() && { return std::move(*artifacts_); }

 private:
  explicit CompileProtocolArtifactsResult(CompiledProtocolArtifacts artifacts)
      : artifacts_(std::move(artifacts)) {}
  explicit CompileProtocolArtifactsResult(CompileDiagnostic diagnostic)
      : diagnostic_(std::move(diagnostic)) {}

  std::optional<CompiledProtocolArtifacts> artifacts_;
  std::optional<CompileDiagnostic> diagnostic_;
};

// Internal compiler entry points. These are deliberately not installed or exported.
// 内部 Plan-only 路径不构造 metadata；带描述路径在预算后先构造 metadata，再组装/冻结 Plan，
// 最后核验对应关系。二者不能混称为相同的产物发布入口。
// 内部顶层捕获阶段异常并尝试返回诊断；构造诊断仍可能分配，不承诺永不抛异常。
CompileResult CompileJsonToPlan(std::string_view json_bytes);
CompileProtocolArtifactsResult CompileJsonToPlanWithMetadata(
    std::string_view json_bytes, std::size_t description_memory_limit_bytes);
// 测试/观察用的自有文本快照，不是冻结 Plan 或业务执行成功的替代物。
std::string MakeDeterministicPlanSnapshot(const PlanBundle& plan);

}  // namespace pae::config_compiler
