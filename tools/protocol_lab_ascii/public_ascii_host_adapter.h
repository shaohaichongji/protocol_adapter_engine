#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "pae/host_endpoint.h"
#include "public_ascii_offline_adapter.h"

namespace pae::protocol_lab_ascii::public_offline {

struct HostBinding {
  std::string endpoint;
  HostAction action = HostAction::DECODE;
  std::size_t pipeline_index = 0U;
};

struct HostPrepareResult;

// Public-only complete-record Host owner used by the Qt A2 publication path. The embedded A1
// adapter owns the one compiled protocol; HostEndpoint and all copied results borrow only during
// synchronous calls.
class HostAdapter final {
 public:
  static HostPrepareResult Create(CompiledProtocol compiled, std::vector<HostBinding> bindings,
                                  const Limits& limits = {},
                                  std::size_t previous_instance_bytes = 0U) noexcept;

  HostAdapter(const HostAdapter&) = delete;
  HostAdapter& operator=(const HostAdapter&) = delete;
  ~HostAdapter();

  [[nodiscard]] const OwnedDescription& Description() const noexcept;
  [[nodiscard]] const std::vector<HostBinding>& Bindings() const noexcept { return bindings_; }
  [[nodiscard]] std::size_t AccountedBytes() const noexcept { return accounted_bytes_; }
  [[nodiscard]] std::size_t FlowCount(std::size_t binding) const noexcept;
  [[nodiscard]] OperationResult Decode(std::size_t binding, std::size_t flow,
                                       ByteView frame) noexcept;
  [[nodiscard]] OperationResult Encode(std::size_t binding, std::size_t message_index,
                                       const std::vector<InputField>& inputs) noexcept;
  [[nodiscard]] bool Reset(std::size_t binding, std::size_t flow) noexcept;
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  void FailNextCallbackAllocationForTesting() noexcept { fail_next_callback_allocation_ = true; }
#endif

 private:
  struct Channel {
    HostChannelHandle handle;
  };

  HostAdapter(std::unique_ptr<Adapter> owner, std::unique_ptr<HostEndpoint> host,
              std::vector<HostBinding> bindings, std::vector<std::vector<Channel>> channels,
              std::size_t accounted_bytes) noexcept;

  std::unique_ptr<Adapter> owner_;
  std::unique_ptr<HostEndpoint> host_;
  std::vector<HostBinding> bindings_;
  std::vector<std::vector<Channel>> channels_;
  std::size_t accounted_bytes_ = 0U;
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  bool fail_next_callback_allocation_ = false;
#endif
};

struct HostPrepareResult {
  LocalStatus status = LocalStatus::PREPARATION_FAILED;
  HostStatus host_status = HostStatus::INVALID_ARGUMENT;
  std::unique_ptr<HostAdapter> adapter;
};

}  // namespace pae::protocol_lab_ascii::public_offline
