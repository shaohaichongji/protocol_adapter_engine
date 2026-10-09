// 可选公开组件只封装内部转换 owner、状态和来源副本，不自动接入 PAE 编译或 Codec。
#include <new>
#include <utility>

#include "frontend.h"
#include "pae/yaml_frontend.h"

namespace pae::yaml {
namespace {

ConversionStatus MapStatus(yaml_frontend::Status status) noexcept {
  switch (status) {
    case yaml_frontend::Status::OK:
      return ConversionStatus::OK;
    case yaml_frontend::Status::INPUT_LIMIT:
      return ConversionStatus::INPUT_LIMIT;
    case yaml_frontend::Status::PARSER_BUDGET:
      return ConversionStatus::PARSER_BUDGET;
    case yaml_frontend::Status::AUXILIARY_BUDGET:
      return ConversionStatus::AUXILIARY_BUDGET;
    case yaml_frontend::Status::OUTPUT_LIMIT:
      return ConversionStatus::OUTPUT_LIMIT;
    case yaml_frontend::Status::ALLOCATION_FAILED:
      return ConversionStatus::ALLOCATION_FAILED;
    case yaml_frontend::Status::INVALID_YAML:
      return ConversionStatus::INVALID_YAML;
    case yaml_frontend::Status::PROFILE_REJECTED:
      return ConversionStatus::PROFILE_REJECTED;
  }
  return ConversionStatus::INVALID_YAML;
}

SourceLocation CopyLocation(const yaml_frontend::SourceEntry& entry,
                            bool ancestor_fallback) noexcept {
  // 返回值不借用内部 Entry；祖先回退同时标记两侧近似，不冒充查询属性自身的位置。
  return {entry.key_line,   entry.key_column,   entry.key_approximate || ancestor_fallback,
          entry.value_line, entry.value_column, entry.value_approximate || ancestor_fallback,
          ancestor_fallback};
}

}  // namespace

struct ConversionResult::Impl {
  // 不透明 owner 接管 JSON/映射存储，不保留原文、Parser 或 Tree 指针。
  explicit Impl(yaml_frontend::Result&& value) noexcept : conversion(std::move(value)) {}
  yaml_frontend::Result conversion;
};

TrialResourceLimits TrialResourceLimitsV01() noexcept {
  // 只读公布固定试用约束，不开放内部调参和故障点；不是全进程内存计费报告。
  const yaml_frontend::Limits limits{};
  return {limits.input_bytes, limits.parser_bytes, limits.auxiliary_bytes, limits.json_bytes,
          limits.nodes,       limits.depth,        limits.scalar_bytes};
}

ConversionResult::ConversionResult() noexcept = default;

ConversionResult::ConversionResult(ConversionResult&& other) noexcept
    : impl_(std::move(other.impl_)),
      status_(std::exchange(other.status_, ConversionStatus::INVALID_YAML)),
      reason_(std::exchange(other.reason_, "moved-from")) {}

ConversionResult& ConversionResult::operator=(ConversionResult&& other) noexcept {
  // 转移唯一 owner 并清空来源对象状态；此前借用 view 不应跨移动/赋值继续使用。
  if (this != &other) {
    impl_ = std::move(other.impl_);
    status_ = std::exchange(other.status_, ConversionStatus::INVALID_YAML);
    reason_ = std::exchange(other.reason_, "moved-from");
  }
  return *this;
}

ConversionResult::~ConversionResult() = default;

bool ConversionResult::Succeeded() const noexcept {
  return status_ == ConversionStatus::OK && impl_ != nullptr;
}

ConversionStatus ConversionResult::Status() const noexcept { return status_; }

const char* ConversionResult::Reason() const noexcept { return reason_; }

std::string_view ConversionResult::Json() const noexcept {
  return Succeeded() ? impl_->conversion.Json() : std::string_view{};
}

std::string_view ConversionResult::SourceIdentity() const noexcept {
  return Succeeded() ? impl_->conversion.SourceIdentity() : std::string_view{};
}

std::optional<SourceLocation> ConversionResult::FindSource(
    std::string_view json_pointer) const noexcept {
  if (!Succeeded()) return std::nullopt;
  const auto* entry = impl_->conversion.Find(json_pointer);
  return entry ? std::optional<SourceLocation>(CopyLocation(*entry, false)) : std::nullopt;
}

std::optional<SourceLocation> ConversionResult::FindNearestSource(
    std::string_view json_pointer) const noexcept {
  // 先保留精确命中的位置标记，再对祖先命中显式附加 fallback，二者语义不同。
  if (!Succeeded()) return std::nullopt;
  if (const auto* exact = impl_->conversion.Find(json_pointer)) return CopyLocation(*exact, false);
  const auto* ancestor = impl_->conversion.FindNearest(json_pointer);
  return ancestor ? std::optional<SourceLocation>(CopyLocation(*ancestor, true)) : std::nullopt;
}

ConversionResult ConvertToStrictJson(std::string_view yaml_bytes,
                                     std::string_view source_identity) noexcept {
  auto converted = yaml_frontend::Convert(yaml_bytes, source_identity);
  ConversionResult result;
  result.status_ = MapStatus(converted.status);
  result.reason_ = converted.reason;
  if (!converted.Succeeded()) return result;
  // 转换成功后仍需一次独立公开 owner 分配；此处失败也不发布可用的部分结果。
  result.impl_.reset(new (std::nothrow) ConversionResult::Impl(std::move(converted)));
  if (!result.impl_) {
    result.status_ = ConversionStatus::ALLOCATION_FAILED;
    result.reason_ = "YAML result owner allocation failed";
  }
  return result;
}

}  // namespace pae::yaml
