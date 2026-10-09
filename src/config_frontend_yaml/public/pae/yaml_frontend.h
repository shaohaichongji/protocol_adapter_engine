#pragma once

// 独立可选 YAML 作者源入口；JSON 仍是规范编译输入，此接口不替代 Schema/领域校验。
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>

namespace pae::yaml {

enum class ConversionStatus {
  OK,
  INPUT_LIMIT,
  PARSER_BUDGET,
  AUXILIARY_BUDGET,
  OUTPUT_LIMIT,
  ALLOCATION_FAILED,
  INVALID_YAML,
  PROFILE_REJECTED,
};

// Trial resource constraints V0.1. These are rejection boundaries, not an RSS guarantee.
// 字节数、节点数和层数分开计；固定试用约束不开放调参，也不包含公开 owner 全部开销。
struct TrialResourceLimits {
  std::size_t input_bytes;
  std::size_t parser_bytes;
  std::size_t auxiliary_bytes;
  std::size_t json_bytes;
  std::size_t nodes;
  std::size_t depth;
  std::size_t scalar_bytes;
};

[[nodiscard]] TrialResourceLimits TrialResourceLimitsV01() noexcept;

// Values are copied from the owned source map. ancestor_fallback means this position belongs
// to the closest recorded ancestor, not the queried JSON Pointer itself.
// 一基行列定位节点起点，0 为无位置；近似及祖先回退不意味着原文逐字符精确映射。
struct SourceLocation {
  std::size_t key_line = 0;
  std::size_t key_column = 0;
  bool key_approximate = false;
  std::size_t value_line = 0;
  std::size_t value_column = 0;
  bool value_approximate = false;
  bool ancestor_fallback = false;
};

// move-only 结果自持 JSON、来源标签和映射；SourceLocation 返回值不借用内部存储。
class ConversionResult final {
 public:
  ConversionResult() noexcept;
  ConversionResult(const ConversionResult&) = delete;
  ConversionResult& operator=(const ConversionResult&) = delete;
  ConversionResult(ConversionResult&& other) noexcept;
  ConversionResult& operator=(ConversionResult&& other) noexcept;
  ~ConversionResult();

  [[nodiscard]] bool Succeeded() const noexcept;
  [[nodiscard]] ConversionStatus Status() const noexcept;
  [[nodiscard]] const char* Reason() const noexcept;
  // Views borrow this owner and become invalid after move, assignment, or destruction.
  // 保存配置文本时须在借用期内复制；移动后对象可安全查询但不再提供成功 payload。
  [[nodiscard]] std::string_view Json() const noexcept;
  [[nodiscard]] std::string_view SourceIdentity() const noexcept;
  [[nodiscard]] std::optional<SourceLocation> FindSource(
      std::string_view json_pointer) const noexcept;
  [[nodiscard]] std::optional<SourceLocation> FindNearestSource(
      std::string_view json_pointer) const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  ConversionStatus status_ = ConversionStatus::INVALID_YAML;
  const char* reason_ = "not converted";

  friend ConversionResult ConvertToStrictJson(std::string_view, std::string_view) noexcept;
};

// Input is borrowed only during this call. Success owns the generated strict JSON and source map.
// Failure has no usable JSON or source map. Schema validation still belongs to CompileProtocolJson.
// source_identity 仅是原文标签，不读取该路径；转换成功后由调用方显式选择是否编译 JSON。
[[nodiscard]] ConversionResult ConvertToStrictJson(std::string_view yaml_bytes,
                                                   std::string_view source_identity = {}) noexcept;

}  // namespace pae::yaml
