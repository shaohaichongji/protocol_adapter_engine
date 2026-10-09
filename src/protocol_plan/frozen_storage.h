#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

namespace pae::protocol_plan {

// 只借用显式长度的字符区，不拥有或释放字节，也不保证以 NUL 结束。
// 复制该描述不会延长存储寿命；Plan 中的字符串依赖相应 PlanOwner 的存储仍然有效。
class FrozenString final {
 public:
  FrozenString() noexcept = default;
  FrozenString(const char* data, std::size_t size) noexcept : data_(data), size_(size) {}

  std::string_view View() const noexcept { return std::string_view{data_, size_}; }
  operator std::string_view() const noexcept { return View(); }
  const char* data() const noexcept { return data_; }
  std::size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0U; }

 private:
  const char* data_ = nullptr;
  std::size_t size_ = 0U;
};

// move-only 数组描述：接管元素析构责任，但不 delete Arena 内存；不是普通可复制 span。
// PlanOwner 必须在释放存储前析构这些元素。移动清空源，已有的外部借用不会被自动更新。
template <typename T>
class FrozenArray final {
 public:
  FrozenArray() noexcept = default;
  FrozenArray(const FrozenArray&) = delete;
  FrozenArray& operator=(const FrozenArray&) = delete;
  FrozenArray(FrozenArray&& other) noexcept : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0U;
  }
  FrozenArray& operator=(FrozenArray&& other) noexcept {
    if (this != &other) {
      Reset();
      data_ = other.data_;
      size_ = other.size_;
      other.data_ = nullptr;
      other.size_ = 0U;
    }
    return *this;
  }
  ~FrozenArray() { Reset(); }

  const T* data() const noexcept { return data_; }
  std::size_t size() const noexcept { return size_; }
  bool empty() const noexcept { return size_ == 0U; }
  // 访问不检查边界；[] 必须在 size 内，front/back 要求非空。
  const T& operator[](std::size_t index) const noexcept { return data_[index]; }
  const T& front() const noexcept { return data_[0U]; }
  const T& back() const noexcept { return data_[size_ - 1U]; }
  const T* begin() const noexcept { return data_; }
  const T* end() const noexcept { return size_ == 0U ? data_ : data_ + size_; }

  // data 指向 size 个已构造元素，且存储覆盖其析构期；同批元素不能重复 Adopt。
  static FrozenArray Adopt(T* data, std::size_t size) noexcept { return FrozenArray{data, size}; }

 private:
  FrozenArray(T* data, std::size_t size) noexcept : data_(data), size_(size) {}

  void Reset() noexcept {
    // 逆序析构可递归释放嵌套数组的元素责任；底层字节由外层存储 owner 统一释放。
    for (std::size_t index = size_; index != 0U; --index) {
      data_[index - 1U].~T();
    }
    data_ = nullptr;
    size_ = 0U;
  }

  T* data_ = nullptr;
  std::size_t size_ = 0U;
};

inline bool operator==(FrozenString left, std::string_view right) noexcept {
  return left.View() == right;
}

inline bool operator==(std::string_view left, FrozenString right) noexcept {
  return left == right.View();
}

inline bool operator!=(FrozenString left, std::string_view right) noexcept {
  return !(left == right);
}

}  // namespace pae::protocol_plan
