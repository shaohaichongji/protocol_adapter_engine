#pragma once

#include <array>
#include <limits>

#include "../../src/host_endpoint/host_endpoint.h"
#include "ascii_offline_adapter.h"

namespace pae::protocol_lab::ascii {

struct HostBinding {
  std::string endpoint;
  host_endpoint::Action action = host_endpoint::Action::DECODE;
  std::size_t pipeline_index = 0U;
};

// One immutable active binding table. Replacement is prepared independently by the owner.
// All operations are serialized; results own their bytes, unlike the PAE callbacks.
class HostObserverAdapter final {
 public:
  static std::unique_ptr<HostObserverAdapter> Create(
      config_compiler::CompiledUiArtifacts artifacts, std::vector<HostBinding> bindings,
      std::string& error, const protocol_framing::FramingLimitOverrides& overrides = {});
  const DocumentDescription& Description() const noexcept { return description_; }
  const std::vector<HostBinding>& Bindings() const noexcept { return bindings_; }
  ExecutionResult Inspect(std::size_t binding, std::size_t stream, ExecutionIdentity identity,
                          const std::vector<std::uint8_t>& frame);
  ExecutionResult Encode(std::size_t binding, ExecutionIdentity identity,
                         const std::vector<InputField>& fields);
  StreamStepResult Submit(std::size_t binding, std::size_t stream, ExecutionIdentity identity,
                          const std::vector<std::uint8_t>& chunk);
  StreamStepResult Continue(std::size_t binding, std::size_t stream, ExecutionIdentity identity);
  bool Reset(std::size_t binding, std::size_t stream);
  std::optional<StreamObservation> Observe(std::size_t binding, std::size_t stream) const noexcept;
  bool HasDiscardableState() const noexcept;
  std::size_t AccountedBytes() const noexcept { return accounted_bytes_; }
  static constexpr std::size_t kMaximumAccountedBytes = 128U * 1024U * 1024U;
#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
  void SetCopyLimitForTesting(std::size_t limit) noexcept { copy_limit_ = limit; }
#endif

 private:
  struct Flow {
    host_endpoint::Handle handle;
    std::vector<std::uint8_t> frozen;
    std::size_t cursor = 0U;
    StreamObservation counters;
    bool faulted = false;
    unsigned no_progress = 0U;
  };
  bool Matches(std::size_t binding, std::size_t stream, host_endpoint::Action action,
               const ExecutionIdentity& identity) const noexcept;
  ExecutionResult CopyCandidate(const host_endpoint::Candidate&, ExecutionIdentity) const;
  static host_endpoint::Sink BusinessSink() noexcept;
  std::unique_ptr<host_endpoint::Session> session_;
  DocumentDescription description_;
  std::vector<HostBinding> bindings_;
  std::vector<std::array<Flow, 2>> flows_;
  std::size_t accounted_bytes_ = 0U;
  std::size_t copy_limit_ = (std::numeric_limits<std::size_t>::max)();
};
}  // namespace pae::protocol_lab::ascii
