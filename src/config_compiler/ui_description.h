#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include "../protocol_plan/plan_types.h"

namespace pae::config_compiler {

inline constexpr std::size_t kCompilerDecodedStringHardLimitBytes = 2U * 1024U * 1024U;

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

struct MessageMetadata {
  DescriptionStringSpan display_name;
  DescriptionStringSpan description;
  DescriptionStringSpan source_ref;
  std::size_t field_begin = 0U;
  std::size_t field_count = 0U;
};

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

struct DescriptionMemoryReport {
  std::size_t object_bytes = 0U;
  std::size_t string_bytes = 0U;
  std::size_t index_bytes = 0U;
  std::size_t alignment_bytes = 0U;
  std::size_t allocation_count = 0U;
  std::size_t accounted_total_bytes = 0U;
};

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

class UiDescriptionBuilder;
struct UiDescriptionTestProbe;

class UiDescriptionSidecar final {
 public:
  UiDescriptionSidecar() = default;
  UiDescriptionSidecar(const UiDescriptionSidecar&) = delete;
  UiDescriptionSidecar& operator=(const UiDescriptionSidecar&) = delete;
  UiDescriptionSidecar(UiDescriptionSidecar&&) noexcept = default;
  UiDescriptionSidecar& operator=(UiDescriptionSidecar&&) noexcept = default;
  ~UiDescriptionSidecar() = default;

  bool empty() const noexcept { return storage_ == nullptr; }
  const ProtocolMetadata& Protocol() const noexcept;
  DescriptionArrayView<PipelineMetadata> Pipelines() const noexcept;
  DescriptionArrayView<MessageMetadata> Messages() const noexcept;
  DescriptionArrayView<FieldMetadata> Fields() const noexcept;
  DescriptionArrayView<EnumMetadata> Enums() const noexcept;
  std::string_view Resolve(DescriptionStringSpan span) const noexcept;
  const DescriptionMemoryReport& MemoryReport() const noexcept { return memory_report_; }

 private:
  friend class UiDescriptionBuilder;

  struct StorageDeleter final {
    void operator()(std::byte* storage) const noexcept;

    UiDescriptionTestProbe* test_probe = nullptr;
    std::size_t accounted_bytes = 0U;
  };
  using StorageOwner = std::unique_ptr<std::byte[], StorageDeleter>;

  UiDescriptionSidecar(StorageOwner storage, std::size_t storage_size,
                       DescriptionMemoryReport memory_report) noexcept
      : storage_(std::move(storage)), storage_size_(storage_size), memory_report_(memory_report) {}

  StorageOwner storage_;
  std::size_t storage_size_ = 0U;
  DescriptionMemoryReport memory_report_;
};

// ABI-specific upper bound derived from the shared Compiler string budget and profile limits.
std::size_t DerivedUiDescriptionMemoryLimit(
    protocol_plan::ResourceProfile resource_profile) noexcept;

}  // namespace pae::config_compiler
