#include "plan_memory.h"

#include <limits>
#include <new>
#include <utility>

#include "plan_bundle.h"

namespace pae::protocol_plan {
namespace {

bool IsPowerOfTwo(std::size_t value) noexcept {
  return value != 0U && (value & (value - 1U)) == 0U;
}

bool AddChecked(std::size_t value, std::size_t& total) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)() - total) {
    return false;
  }
  total += value;
  return true;
}

std::size_t* CategoryBytes(PlanMemoryReport& report, PlanMemoryCategory category) noexcept {
  switch (category) {
    case PlanMemoryCategory::OBJECT:
      return &report.object_bytes;
    case PlanMemoryCategory::STRING:
      return &report.string_bytes;
    case PlanMemoryCategory::MATCHER:
      return &report.matcher_bytes;
    case PlanMemoryCategory::METADATA_CONTAINER:
      return &report.metadata_container_bytes;
    case PlanMemoryCategory::EXECUTION_DESCRIPTOR:
      return &report.execution_descriptor_bytes;
    case PlanMemoryCategory::INDEX:
      return &report.index_bytes;
    case PlanMemoryCategory::EXTENSION:
      return &report.extension_bytes;
  }
  return nullptr;
}

}  // namespace

bool PlanMemoryLayout::AddAllocation(std::size_t payload_bytes, std::size_t alignment,
                                     PlanMemoryCategory category,
                                     std::size_t* aligned_offset) noexcept {
  if (payload_bytes == 0U) {
    if (aligned_offset != nullptr) {
      *aligned_offset = report_.accounted_total_bytes;
    }
    return true;
  }
  if (!IsPowerOfTwo(alignment) || alignment > alignof(std::max_align_t)) {
    return false;
  }
  const std::size_t remainder = report_.accounted_total_bytes & (alignment - 1U);
  const std::size_t padding = remainder == 0U ? 0U : alignment - remainder;
  PlanMemoryReport next_report = report_;
  std::size_t next_total = next_report.accounted_total_bytes;
  std::size_t* category_bytes = CategoryBytes(next_report, category);
  if (category_bytes == nullptr || !AddChecked(padding, next_total) ||
      !AddChecked(payload_bytes, next_total) || next_total > limit_bytes_ ||
      !AddChecked(payload_bytes, *category_bytes) ||
      !AddChecked(padding, next_report.alignment_bytes) ||
      !AddChecked(1U, next_report.allocation_count)) {
    return false;
  }
  if (aligned_offset != nullptr) {
    *aligned_offset = next_report.accounted_total_bytes + padding;
  }
  next_report.accounted_total_bytes = next_total;
  next_report.upstream_allocation_count = 1U;
  report_ = next_report;
  return true;
}

void test_only::PlanMemoryTestProbe::OnAllocate(std::size_t bytes) noexcept {
  live_upstream_bytes_ += bytes;
  ++live_upstream_allocations_;
}

void test_only::PlanMemoryTestProbe::OnRelease(std::size_t bytes) noexcept {
  if (live_upstream_bytes_ >= bytes && live_upstream_allocations_ != 0U) {
    live_upstream_bytes_ -= bytes;
    --live_upstream_allocations_;
  }
}

PlanStorageBlock::PlanStorageBlock(std::unique_ptr<std::byte[]> storage, std::size_t size,
                                   test_only::PlanMemoryTestProbe* probe) noexcept
    : storage_(std::move(storage)), size_(size), probe_(probe) {
  if (probe_ != nullptr && storage_ != nullptr) {
    probe_->OnAllocate(size_);
  }
}

PlanStorageBlock::PlanStorageBlock(PlanStorageBlock&& other) noexcept
    : storage_(std::move(other.storage_)), size_(other.size_), probe_(other.probe_) {
  other.size_ = 0U;
  other.probe_ = nullptr;
}

PlanStorageBlock& PlanStorageBlock::operator=(PlanStorageBlock&& other) noexcept {
  if (this != &other) {
    Reset();
    storage_ = std::move(other.storage_);
    size_ = other.size_;
    probe_ = other.probe_;
    other.size_ = 0U;
    other.probe_ = nullptr;
  }
  return *this;
}

PlanStorageBlock::~PlanStorageBlock() { Reset(); }

PlanStorageBlock PlanStorageBlock::Allocate(std::size_t bytes, bool inject_upstream_failure,
                                            test_only::PlanMemoryTestProbe* probe) noexcept {
  if (bytes == 0U || inject_upstream_failure) {
    return {};
  }
  std::unique_ptr<std::byte[]> storage{new (std::nothrow) std::byte[bytes]};
  if (storage == nullptr) {
    return {};
  }
  return PlanStorageBlock{std::move(storage), bytes, probe};
}

void PlanStorageBlock::Reset() noexcept {
  if (probe_ != nullptr && storage_ != nullptr) {
    probe_->OnRelease(size_);
  }
  storage_.reset();
  size_ = 0U;
  probe_ = nullptr;
}

void* PlanArena::Allocate(std::size_t payload_bytes, std::size_t alignment,
                          PlanMemoryCategory category) noexcept {
  if (payload_bytes == 0U) {
    return nullptr;
  }
  ++allocation_ordinal_;
  if (fail_at_allocation_ != 0U && allocation_ordinal_ == fail_at_allocation_) {
    return nullptr;
  }
  std::size_t offset = 0U;
  if (storage_ == nullptr || !layout_.AddAllocation(payload_bytes, alignment, category, &offset) ||
      offset > capacity_bytes_ || payload_bytes > capacity_bytes_ - offset) {
    return nullptr;
  }
  return storage_ + offset;
}

PlanOwner::PlanOwner(PlanStorageBlock storage, PlanBundle* plan) noexcept
    : storage_(std::move(storage)), plan_(plan) {}

PlanOwner::PlanOwner(PlanOwner&& other) noexcept
    : storage_(std::move(other.storage_)), plan_(other.plan_) {
  other.plan_ = nullptr;
}

PlanOwner& PlanOwner::operator=(PlanOwner&& other) noexcept {
  if (this != &other) {
    Reset();
    storage_ = std::move(other.storage_);
    plan_ = other.plan_;
    other.plan_ = nullptr;
  }
  return *this;
}

PlanOwner::~PlanOwner() { Reset(); }

void PlanOwner::Reset() noexcept {
  if (plan_ != nullptr) {
    plan_->~PlanBundle();
    plan_ = nullptr;
  }
  storage_ = PlanStorageBlock{};
}

bool PlanMemoryReportsEqual(const PlanMemoryReport& left, const PlanMemoryReport& right) noexcept {
  return left.object_bytes == right.object_bytes && left.string_bytes == right.string_bytes &&
         left.matcher_bytes == right.matcher_bytes &&
         left.metadata_container_bytes == right.metadata_container_bytes &&
         left.execution_descriptor_bytes == right.execution_descriptor_bytes &&
         left.index_bytes == right.index_bytes && left.extension_bytes == right.extension_bytes &&
         left.alignment_bytes == right.alignment_bytes &&
         left.allocation_count == right.allocation_count &&
         left.upstream_allocation_count == right.upstream_allocation_count &&
         left.accounted_total_bytes == right.accounted_total_bytes;
}

}  // namespace pae::protocol_plan
