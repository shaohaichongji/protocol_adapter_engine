#pragma once

#include <atomic>
#include <cstddef>
#include <utility>

#include "../config_compiler/config_compiler.h"
#include "pae/compiler.h"

namespace pae::public_api_internal {

class CompiledState final {
 public:
  explicit CompiledState(config_compiler::CompiledProtocolArtifacts artifacts) noexcept
      : artifacts_(std::move(artifacts)) {}

  CompiledState(const CompiledState&) = delete;
  CompiledState& operator=(const CompiledState&) = delete;

  void Retain() noexcept { references_.fetch_add(1U, std::memory_order_relaxed); }

  void Release() noexcept {
    if (references_.fetch_sub(1U, std::memory_order_acq_rel) == 1U) {
      delete this;
    }
  }

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

class CompiledStateRef final {
 public:
  CompiledStateRef() noexcept = default;

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

  [[nodiscard]] const CompiledState* get() const noexcept { return state_; }
  [[nodiscard]] const CompiledState& operator*() const noexcept { return *state_; }
  [[nodiscard]] const CompiledState* operator->() const noexcept { return state_; }
  [[nodiscard]] explicit operator bool() const noexcept { return state_ != nullptr; }

 private:
  CompiledState* state_ = nullptr;
};

}  // namespace pae::public_api_internal

namespace pae {

struct CompiledProtocol::Impl final {
  explicit Impl(config_compiler::CompiledProtocolArtifacts artifacts)
      : state(public_api_internal::CompiledStateRef::Adopt(
            new public_api_internal::CompiledState(std::move(artifacts)))) {}

  public_api_internal::CompiledStateRef state;
};

namespace detail {

class CompiledProtocolAccess final {
 public:
  [[nodiscard]] static public_api_internal::CompiledStateRef Acquire(
      const CompiledProtocol& compiled) noexcept {
    return compiled.impl_ == nullptr ? public_api_internal::CompiledStateRef{}
                                     : compiled.impl_->state;
  }
};

}  // namespace detail
}  // namespace pae
