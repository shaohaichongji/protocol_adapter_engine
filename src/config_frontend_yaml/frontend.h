#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

namespace pae::yaml_frontend {

struct Limits {
  std::size_t input_bytes = 16 * 1024;
  std::size_t parser_bytes = 128 * 1024;
  std::size_t auxiliary_bytes = 128 * 1024;
  std::size_t json_bytes = 32 * 1024;
  std::size_t nodes = 512;
  std::size_t depth = 16;
  std::size_t scalar_bytes = 4 * 1024;
  // Test-only allocation fault point; zero disables injection.
  std::size_t fail_parser_allocation = 0;
  std::size_t fail_frontend_allocation = 0;
};

enum class Status {
  OK,
  INPUT_LIMIT,
  PARSER_BUDGET,
  AUXILIARY_BUDGET,
  OUTPUT_LIMIT,
  ALLOCATION_FAILED,
  INVALID_YAML,
  PROFILE_REJECTED,
};

struct SourceEntry {
  std::size_t pointer_offset = 0;
  std::size_t pointer_length = 0;
  std::size_t key_line = 0;
  std::size_t key_column = 0;
  bool key_approximate = false;
  std::size_t value_line = 0;
  std::size_t value_column = 0;
  bool value_approximate = false;
};

struct Result {
  Status status = Status::INVALID_YAML;
  const char* reason = "not converted";
  std::unique_ptr<char[]> json_storage;
  std::size_t json_length = 0;
  std::unique_ptr<char[]> pointer_storage;
  std::size_t source_identity_length = 0;
  std::unique_ptr<SourceEntry[]> entries;
  std::size_t entry_count = 0;
  std::size_t parser_peak_bytes = 0;
  std::size_t parser_allocation_calls = 0;
  std::size_t parser_retained_bytes = 0;
  std::size_t frontend_allocation_calls = 0;

  [[nodiscard]] bool Succeeded() const noexcept { return status == Status::OK; }
  [[nodiscard]] std::string_view Json() const noexcept {
    return Succeeded() ? std::string_view(json_storage.get(), json_length) : std::string_view{};
  }
  [[nodiscard]] std::string_view Pointer(const SourceEntry& entry) const noexcept {
    return {pointer_storage.get() + entry.pointer_offset, entry.pointer_length};
  }
  [[nodiscard]] std::string_view SourceIdentity() const noexcept {
    return Succeeded() ? std::string_view(pointer_storage.get(), source_identity_length)
                       : std::string_view{};
  }
  [[nodiscard]] const SourceEntry* Find(std::string_view pointer) const noexcept;
  [[nodiscard]] const SourceEntry* FindNearest(std::string_view pointer) const noexcept;
};

[[nodiscard]] Result Convert(std::string_view input, const Limits& limits = {}) noexcept;
[[nodiscard]] Result Convert(std::string_view input, std::string_view source_identity,
                             const Limits& limits = {}) noexcept;

}  // namespace pae::yaml_frontend
