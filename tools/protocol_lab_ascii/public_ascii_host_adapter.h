#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pae/host_endpoint.h"
#include "public_ascii_offline_adapter.h"

namespace pae::protocol_lab_ascii::public_offline {

struct HostBinding {
  std::string endpoint;
  HostAction action = HostAction::DECODE;
  std::size_t pipeline_index = 0U;
  StreamFramerOptions framing_options;
};

struct StreamObservation {
  StreamFramingPhase phase = StreamFramingPhase::COLLECTING;
  std::size_t buffered_bytes = 0U;
  bool has_internal_work = false;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
  std::size_t maximum_candidate_frame_bytes = 0U;
  std::size_t frozen_input_bytes = 0U;
  std::size_t frozen_cursor = 0U;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_observer_callbacks = 0U;
  std::uint64_t total_business_callbacks = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
};

enum class StreamDiagnostic {
  NONE,
  CHANNEL_NOT_ASCII_STREAM,
  RESET_REQUIRED,
  INVALID_CHUNK_OR_CONTINUE_REQUIRED,
  CHUNK_ALLOCATION_FAILED,
  CHUNK_MATERIALIZATION_FAILED,
  NO_FROZEN_SUFFIX_OR_INTERNAL_WORK,
  CANDIDATE_COPY_FAILED_RESET_REQUIRED,
  STREAM_CONTRACT_VIOLATION_RESET_REQUIRED,
};

struct StreamStepResult {
  LocalStatus status = LocalStatus::INVALID_INPUT;
  bool host_called = false;
  HostOperationResult host;
  StreamObservation before;
  StreamObservation after;
  std::optional<OperationResult> candidate;
  StreamDiagnostic diagnostic = StreamDiagnostic::NONE;
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
  [[nodiscard]] StreamStepResult SubmitStreamChunk(std::size_t binding, std::size_t flow,
                                                   const std::vector<std::uint8_t>& chunk) noexcept;
  [[nodiscard]] StreamStepResult ContinueStream(std::size_t binding, std::size_t flow) noexcept;
  [[nodiscard]] std::optional<StreamObservation> ObserveStream(std::size_t binding,
                                                               std::size_t flow) const noexcept;
  [[nodiscard]] std::size_t StreamChunkCapacity(std::size_t binding,
                                                std::size_t flow) const noexcept;
  [[nodiscard]] bool StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept;
  [[nodiscard]] bool Reset(std::size_t binding, std::size_t flow) noexcept;
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  void FailNextCallbackAllocationForTesting() noexcept { fail_next_callback_allocation_ = true; }
#endif

 private:
  struct Channel {
    HostChannelHandle handle;
    bool stream = false;
    std::size_t maximum_candidate_frame_bytes = 0U;
    std::size_t stream_capacity = 0U;
    std::vector<std::uint8_t> frozen_input;
    std::size_t cursor = 0U;
    bool faulted = false;
    std::size_t no_progress_steps = 0U;
    std::uint64_t step_sequence = 0U;
    std::uint64_t total_candidates = 0U;
    std::uint64_t total_decode_successes = 0U;
    std::uint64_t total_observer_callbacks = 0U;
    std::uint64_t total_business_callbacks = 0U;
    std::size_t total_discarded_bytes = 0U;
    std::size_t total_malformed_candidates = 0U;
  };

  HostAdapter(std::unique_ptr<Adapter> owner, std::unique_ptr<HostEndpoint> host,
              std::vector<HostBinding> bindings, std::vector<std::vector<Channel>> channels,
              std::size_t accounted_bytes) noexcept;
  [[nodiscard]] StreamStepResult RunStreamStep(std::size_t binding, std::size_t flow) noexcept;

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
