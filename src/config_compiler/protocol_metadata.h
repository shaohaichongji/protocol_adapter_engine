#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include "../protocol_plan/plan_types.h"

namespace pae::config_compiler {

inline constexpr std::size_t kCompilerDecodedStringHardLimitBytes = 2U * 1024U * 1024U;

// 相对于整块 metadata storage 起点的字节偏移和长度，不是字符串区内偏移或字符数。
// 文本按明确长度保存，不额外附加 NUL；span 本身不拥有或延长文本寿命。
struct DescriptionStringSpan {
  std::size_t offset = 0U;
  std::size_t size = 0U;
};

struct ProtocolMetadata {
  DescriptionStringSpan display_name;
  DescriptionStringSpan description;
  DescriptionStringSpan source_ref;
};

struct PipelineMetadata {
  DescriptionStringSpan display_name;
  DescriptionStringSpan description;
  DescriptionStringSpan source_ref;
};

// Message 顺序与 Plan 一致；字段在全局 Fields 表中占连续 [field_begin, begin+count) 区间。
struct MessageMetadata {
  DescriptionStringSpan display_name;
  DescriptionStringSpan description;
  DescriptionStringSpan source_ref;
  std::size_t field_begin = 0U;
  std::size_t field_count = 0U;
};

// 枚举同样按字段顺序展平；区间指向全局 Enums 表，不是该字段 raw 值的上下界。
struct FieldMetadata {
  DescriptionStringSpan display_name;
  DescriptionStringSpan description;
  DescriptionStringSpan source_ref;
  std::size_t enum_begin = 0U;
  std::size_t enum_count = 0U;
};

struct EnumMetadata {
  DescriptionStringSpan display_name;
};

// 按实际类型大小和对齐计费的单块存储报告，不是编译峰值或进程 RSS。
// 当前索引嵌在描述对象内，计入 object_bytes；index_bytes 不额外重复计费。
struct DescriptionMemoryReport {
  std::size_t object_bytes = 0U;
  std::size_t string_bytes = 0U;
  std::size_t index_bytes = 0U;
  std::size_t alignment_bytes = 0U;
  std::size_t allocation_count = 0U;
  std::size_t accounted_total_bytes = 0U;
};

// 内部只读借用视图，不增加 owner 引用；operator[] 不检查越界，调用方先核对 size。
template <typename T>
class DescriptionArrayView final {
 public:
  DescriptionArrayView() = default;
  DescriptionArrayView(const T* data, std::size_t size) noexcept : data_(data), size_(size) {}

  const T* data() const noexcept { return data_; }
  std::size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0U; }
  const T& operator[](std::size_t index) const noexcept { return data_[index]; }
  const T* begin() const noexcept { return data_; }
  const T* end() const noexcept { return data_ == nullptr ? nullptr : data_ + size_; }

 private:
  const T* data_ = nullptr;
  std::size_t size_ = 0U;
};

class ProtocolMetadataBuilder;
struct ProtocolMetadataTestProbe;

// move-only owner，自有一块包含描述数组和复制文本的内存，不借用原 JSON/SchemaIr。
// 返回引用、数组视图和 string_view 均借用该存储；替换或销毁后不得使用旧视图。
// 移走后的对象为空，应从新 owner 重新取得视图，不用旧计费快照判断是否有效。
class ProtocolMetadataStorage final {
 public:
  ProtocolMetadataStorage() = default;
  ProtocolMetadataStorage(const ProtocolMetadataStorage&) = delete;
  ProtocolMetadataStorage& operator=(const ProtocolMetadataStorage&) = delete;
  ProtocolMetadataStorage(ProtocolMetadataStorage&&) noexcept = default;
  ProtocolMetadataStorage& operator=(ProtocolMetadataStorage&&) noexcept = default;
  ~ProtocolMetadataStorage() = default;

  bool empty() const noexcept { return storage_ == nullptr; }
  // 内部 Protocol/数组查询要求非空且布局有效；公开 facade 先做 HasValue/索引检查。
  const ProtocolMetadata& Protocol() const noexcept;
  DescriptionArrayView<PipelineMetadata> Pipelines() const noexcept;
  DescriptionArrayView<MessageMetadata> Messages() const noexcept;
  DescriptionArrayView<FieldMetadata> Fields() const noexcept;
  DescriptionArrayView<EnumMetadata> Enums() const noexcept;
  // 仅防御空存储、加法溢出和整块范围越界；字符串区归属由 Builder::Audit 核对。
  std::string_view Resolve(DescriptionStringSpan span) const noexcept;
  const DescriptionMemoryReport& MemoryReport() const noexcept { return memory_report_; }

 private:
  friend class ProtocolMetadataBuilder;

  struct StorageDeleter final {
    void operator()(std::byte* storage) const noexcept;

    ProtocolMetadataTestProbe* test_probe = nullptr;
    std::size_t accounted_bytes = 0U;
  };
  using StorageOwner = std::unique_ptr<std::byte[], StorageDeleter>;

  ProtocolMetadataStorage(StorageOwner storage, std::size_t storage_size,
                       DescriptionMemoryReport memory_report) noexcept
      : storage_(std::move(storage)), storage_size_(storage_size), memory_report_(memory_report) {}

  StorageOwner storage_;
  std::size_t storage_size_ = 0U;
  DescriptionMemoryReport memory_report_;
};

// ABI-specific upper bound derived from the shared Compiler string budget and profile limits.
// 按目标 ABI、profile 数量上限及编译器文本预算推导逻辑上限，不是通用分配器硬限制。
std::size_t DerivedProtocolMetadataMemoryLimit(
    protocol_plan::ResourceProfile resource_profile) noexcept;

}  // namespace pae::config_compiler
