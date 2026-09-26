#pragma once

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
struct SourceLocation {
  std::size_t key_line = 0;
  std::size_t key_column = 0;
  bool key_approximate = false;
  std::size_t value_line = 0;
  std::size_t value_column = 0;
  bool value_approximate = false;
  bool ancestor_fallback = false;
};

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
[[nodiscard]] ConversionResult ConvertToStrictJson(std::string_view yaml_bytes,
                                                   std::string_view source_identity = {}) noexcept;

}  // namespace pae::yaml
