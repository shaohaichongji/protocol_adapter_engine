#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "plan_types.h"

namespace pae::protocol_plan {

class PlanBundle;
class PlanBuilder;
class PlanStorageBlock;

enum class PlanMemoryCategory : std::uint8_t {
  OBJECT,
  STRING,
  MATCHER,
  METADATA_CONTAINER,
  EXECUTION_DESCRIPTOR,
  INDEX,
  EXTENSION,
};

class PlanMemoryLayout final {
 public:
  explicit PlanMemoryLayout(std::size_t limit_bytes) noexcept : limit_bytes_(limit_bytes) {}

  bool AddAllocation(std::size_t payload_bytes, std::size_t alignment, PlanMemoryCategory category,
                     std::size_t* aligned_offset = nullptr) noexcept;

  template <typename T>
  bool AddArray(std::size_t count, PlanMemoryCategory category,
                std::size_t* aligned_offset = nullptr) noexcept {
    if (count != 0U && sizeof(T) > (static_cast<std::size_t>(-1) / count)) {
      return false;
    }
    return AddAllocation(sizeof(T) * count, alignof(T), category, aligned_offset);
  }

  const PlanMemoryReport& Report() const noexcept { return report_; }

 private:
  std::size_t limit_bytes_ = 0U;
  PlanMemoryReport report_;
};

namespace test_only {

class PlanMemoryTestProbe final {
 public:
  std::size_t LiveUpstreamBytes() const noexcept { return live_upstream_bytes_; }
  std::size_t LiveUpstreamAllocations() const noexcept { return live_upstream_allocations_; }

 private:
  friend class pae::protocol_plan::PlanStorageBlock;

  void OnAllocate(std::size_t bytes) noexcept;
  void OnRelease(std::size_t bytes) noexcept;

  std::size_t live_upstream_bytes_ = 0U;
  std::size_t live_upstream_allocations_ = 0U;
};

}  // namespace test_only

class PlanStorageBlock final {
 public:
  PlanStorageBlock() noexcept = default;
  PlanStorageBlock(const PlanStorageBlock&) = delete;
  PlanStorageBlock& operator=(const PlanStorageBlock&) = delete;
  PlanStorageBlock(PlanStorageBlock&& other) noexcept;
  PlanStorageBlock& operator=(PlanStorageBlock&& other) noexcept;
  ~PlanStorageBlock();

  static PlanStorageBlock Allocate(std::size_t bytes, bool inject_upstream_failure,
                                   test_only::PlanMemoryTestProbe* probe) noexcept;

  std::byte* Data() noexcept { return storage_.get(); }
  const std::byte* Data() const noexcept { return storage_.get(); }
  std::size_t Size() const noexcept { return size_; }
  explicit operator bool() const noexcept { return storage_ != nullptr; }

 private:
  PlanStorageBlock(std::unique_ptr<std::byte[]> storage, std::size_t size,
                   test_only::PlanMemoryTestProbe* probe) noexcept;
  void Reset() noexcept;

  std::unique_ptr<std::byte[]> storage_;
  std::size_t size_ = 0U;
  test_only::PlanMemoryTestProbe* probe_ = nullptr;
};

class PlanArena final {
 public:
  PlanArena(std::byte* storage, std::size_t capacity_bytes, std::size_t fail_at_allocation) noexcept
      : storage_(storage),
        capacity_bytes_(capacity_bytes),
        fail_at_allocation_(fail_at_allocation),
        layout_(capacity_bytes) {}

  void* Allocate(std::size_t payload_bytes, std::size_t alignment,
                 PlanMemoryCategory category) noexcept;

  template <typename T>
  T* AllocateArray(std::size_t count, PlanMemoryCategory category) noexcept {
    if (count != 0U && sizeof(T) > (static_cast<std::size_t>(-1) / count)) {
      return nullptr;
    }
    return static_cast<T*>(Allocate(sizeof(T) * count, alignof(T), category));
  }

  const PlanMemoryReport& Report() const noexcept { return layout_.Report(); }

 private:
  std::byte* storage_ = nullptr;
  std::size_t capacity_bytes_ = 0U;
  std::size_t fail_at_allocation_ = 0U;
  std::size_t allocation_ordinal_ = 1U;  // Upstream Storage Block is allocation 1.
  PlanMemoryLayout layout_;
};

class PlanOwner final {
 public:
  PlanOwner() noexcept = default;
  PlanOwner(const PlanOwner&) = delete;
  PlanOwner& operator=(const PlanOwner&) = delete;
  PlanOwner(PlanOwner&& other) noexcept;
  PlanOwner& operator=(PlanOwner&& other) noexcept;
  ~PlanOwner();

  const PlanBundle* get() const noexcept { return plan_; }
  const PlanBundle& operator*() const noexcept { return *plan_; }
  const PlanBundle* operator->() const noexcept { return plan_; }
  explicit operator bool() const noexcept { return plan_ != nullptr; }

 private:
  PlanOwner(PlanStorageBlock storage, PlanBundle* plan) noexcept;
  void Reset() noexcept;

  friend class PlanBuilder;

  PlanStorageBlock storage_;
  PlanBundle* plan_ = nullptr;
};

bool PlanMemoryReportsEqual(const PlanMemoryReport& left, const PlanMemoryReport& right) noexcept;

}  // namespace pae::protocol_plan
