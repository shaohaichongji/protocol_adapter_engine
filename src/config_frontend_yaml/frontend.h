#pragma once

// 内部受限 YAML 作者源前端：只交付 strict JSON 和来源映射，不编译 Schema 或执行协议。
// 输入只在 Convert 内借用；成功结果自持输出，不依赖 Parser/Tree 或原文继续存活。
#include <cstddef>
#include <memory>
#include <string_view>

namespace pae::yaml_frontend {

// 字节上限分别约束输入、Parser 回调、辅助容量和 JSON；nodes/depth 是数量/层数。
// 节点、深度及解码后标量在解析后检查；这些逻辑拒绝边界不是 RSS 或栈峰值保证。
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

// Pointer 的 offset/length 是自有池中的字节区间；行列为一基，0 表示没有位置。
// approximate 区分无法准确定位的节点起点，不将生成 JSON offset 当作 YAML offset。
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

// JSON 与来源池独立拥有；池前部保存 source identity，Entry 不借用解析树。
// Json/Pointer/SourceIdentity 和 Find 的返回值仍借用本结果，不是可长期保存的副本。
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
  // 逐段移除 Pointer 后缀查最近已记录祖先；内部不将祖先伪装为精确命中。
  [[nodiscard]] const SourceEntry* FindNearest(std::string_view pointer) const noexcept;
};

[[nodiscard]] Result Convert(std::string_view input, const Limits& limits = {}) noexcept;
// source_identity 是调用方标签而非待打开路径；复制到结果池，不执行文件或外部资源读取。
[[nodiscard]] Result Convert(std::string_view input, std::string_view source_identity,
                             const Limits& limits = {}) noexcept;

}  // namespace pae::yaml_frontend
