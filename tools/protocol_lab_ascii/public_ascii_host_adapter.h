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

// 一个 endpoint/action 绑定公开 Pipeline；输入形态来自编译后的 framing 描述，不由 UI 猜测。
struct HostBinding {
  std::string endpoint;
  HostAction action = HostAction::DECODE;
  std::size_t pipeline_index = 0U;
  StreamFramerOptions framing_options;
};

// 每个 binding/flow 的快照：Framer 半帧/内部工作与 Adapter 冻结 chunk/cursor 是两层状态。
// generation 是 Reset 代次，step_sequence 才是本 Adapter 的推进次数；不同 Flow 分别维护。
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

// status 表示本地一步是否可靠完成，不等于业务 Decode 成功；失败候选可带 CODEC_FAILED。
// host_called 区分前置拒绝，host 保留真实消费事实；复制失败不得把已消费字节当成未执行。
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

// Public-only Host composition for complete records and ASCII CRLF streams, used by A2 publication.
// The embedded A1 owns compiled metadata; HostEndpoint retains frozen state. Callback views borrow
// synchronously, while copied result DTOs own their data independently.
// 非 Qt 组合层：复用一次移入的编译结果，由公开 Host 执行；不重复编译或旁路再执行 A1 Codec。
// Description()/Bindings() 引用借用本对象；返回 DTO 独立自有，回调 view 只在同步回调内有效。
// Flow 隔离不表示可并发：本对象的调用、观察、重置和销毁须串行；不拥有 Socket 或设备线程。
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
  // 完整记录 DECODE 绑定入口；stream 绑定使用下面的 Submit/Continue，不把 chunk 当完整帧。
  [[nodiscard]] OperationResult Decode(std::size_t binding, std::size_t flow,
                                       ByteView frame) noexcept;
  [[nodiscard]] OperationResult Encode(std::size_t binding, std::size_t message_index,
                                       const std::vector<InputField>& inputs) noexcept;
  // 先复制并冻结有界 chunk；有未消费后缀或 Framer 内部工作时拒绝新 chunk，须先 Continue。
  [[nodiscard]] StreamStepResult SubmitStreamChunk(std::size_t binding, std::size_t flow,
                                                   const std::vector<std::uint8_t>& chunk) noexcept;
  // 推进冻结后缀/内部工作，不重新提交已消费前缀；每步至多发布一个候选 DTO。
  [[nodiscard]] StreamStepResult ContinueStream(std::size_t binding, std::size_t flow) noexcept;
  [[nodiscard]] std::optional<StreamObservation> ObserveStream(std::size_t binding,
                                                               std::size_t flow) const noexcept;
  [[nodiscard]] std::size_t StreamChunkCapacity(std::size_t binding,
                                                std::size_t flow) const noexcept;
  [[nodiscard]] bool StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept;
  // 成功 Reset 后重新 Find 新 handle，清除该 Flow 本地状态；不清除其他 Flow 的半帧。
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
    // 当前 Flow 自有的冻结输入；cursor 只按 Host 实际 bytes_consumed 推进。
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

  // 倒序析构先释放 channel/Host 再释放 metadata owner；不表示 Host 执行依赖外部 owner 寿命。
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
