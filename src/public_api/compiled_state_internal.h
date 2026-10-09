#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "../config_compiler/config_compiler.h"
#include "pae/compiler.h"

namespace pae::public_api_internal {

// 公开 facade 与执行对象共享的冻结产物 owner：一起拥有 Plan 和 metadata，不持有输入 JSON。
// 计数保护的是共享状态寿命，不是外部 CompiledProtocol、Ref 或 Workspace 的任意并发操作。
class CompiledState final {
 public:
  explicit CompiledState(config_compiler::CompiledProtocolArtifacts artifacts) noexcept
      : artifacts_(std::move(artifacts)) {}

  CompiledState(const CompiledState&) = delete;
  CompiledState& operator=(const CompiledState&) = delete;

  // 已有合法持有者才可增加引用；不能用已释放的裸指针重新取得状态。
  void Retain() noexcept { references_.fetch_add(1U, std::memory_order_relaxed); }

  // 最后一个持有者释放时同时销毁冻结产物；调用方不再使用该 state 指针。
  void Release() noexcept {
    if (references_.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
      delete this;
    }
  }

  // 借用状态内部的只读产物；返回引用本身不增加引用计数。
  [[nodiscard]] const config_compiler::CompiledProtocolArtifacts& Artifacts() const noexcept {
    return artifacts_;
  }

  [[nodiscard]] static constexpr std::size_t FacadeBytes() noexcept {
    return sizeof(CompiledState);
  }

 private:
  ~CompiledState() = default;

  std::atomic<std::size_t> references_{1U};
  config_compiler::CompiledProtocolArtifacts artifacts_;
};

// 内部 RAII 强引用：复制 Retain，移动只转移指针并清空源，析构 Release。
// 独立 Ref 可共同维持状态寿命；同一个 Ref 的复制/赋值仍需避免并发读写。
class CompiledStateRef final {
 public:
  CompiledStateRef() noexcept = default;

  // 接管调用方已经拥有的一次引用，不额外 Retain；new CompiledState 初始计数为 1。
  // 不可对同一次引用重复 Adopt，也不可接管栈对象或未拥有引用的借用指针。
  static CompiledStateRef Adopt(CompiledState* state) noexcept {
    CompiledStateRef result;
    result.state_ = state;
    return result;
  }

  CompiledStateRef(const CompiledStateRef& other) noexcept : state_(other.state_) {
    if (state_ != nullptr) state_->Retain();
  }

  CompiledStateRef& operator=(const CompiledStateRef& other) noexcept {
    if (this == &other) return *this;
    // 先持有替代状态再释放旧状态，两个 Ref 指向同一 state 时也不会提前销毁它。
    CompiledState* replacement = other.state_;
    if (replacement != nullptr) replacement->Retain();
    if (state_ != nullptr) state_->Release();
    state_ = replacement;
    return *this;
  }

  CompiledStateRef(CompiledStateRef&& other) noexcept : state_(other.state_) {
    other.state_ = nullptr;
  }

  CompiledStateRef& operator=(CompiledStateRef&& other) noexcept {
    if (this == &other) return *this;
    if (state_ != nullptr) state_->Release();
    state_ = other.state_;
    other.state_ = nullptr;
    return *this;
  }

  ~CompiledStateRef() {
    if (state_ != nullptr) state_->Release();
  }

  // get() 返回借用裸指针，不产生新持有者；解引用前须确保 Ref 非空且仍存活。
  [[nodiscard]] const CompiledState* get() const noexcept { return state_; }
  [[nodiscard]] const CompiledState& operator*() const noexcept { return *state_; }
  [[nodiscard]] const CompiledState* operator->() const noexcept { return state_; }
  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }

 private:
  CompiledState* state_ = nullptr;
};

}  // namespace pae::public_api_internal

namespace pae {

// facade 自有的实现层。这里的 new 仍可抛分配异常，因此公开编译入口不是 noexcept。
struct CompiledProtocol::Impl final {
  explicit Impl(config_compiler::CompiledProtocolArtifacts artifacts)
      : state(public_api_internal::CompiledStateRef::Adopt(
            new public_api_internal::CompiledState(std::move(artifacts)))) {}

  public_api_internal::CompiledStateRef state;
};

namespace detail {

class CompiledProtocolAccess final {
 public:
  // 从有效 facade 复制强引用，供 Codec/Framer/Host 保留冻结状态；空或移走的 owner 返回空。
  // Acquire 不延长外部 facade 对象寿命，不允许与同一个 compiled 的移动/销毁并发。
  [[nodiscard]] static public_api_internal::CompiledStateRef Acquire(
      const CompiledProtocol& compiled) noexcept {
    return compiled.impl_ == nullptr ? public_api_internal::CompiledStateRef{}
                                     : compiled.impl_->state;
  }
};

}  // namespace detail
}  // namespace pae
