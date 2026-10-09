#pragma once

// 内部有界流式切帧：按冻结 Pipeline 的边界规则形成候选，不执行消息 Decode 或传输。
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../protocol_plan/plan_bundle.h"

namespace pae::protocol_framing {

struct ByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

// 同步回调期间借用 Workspace 缓存；需跨回调保存时复制字节。STOP 不撤销本次交付。
enum class FrameSinkAction { CONTINUE, STOP };
using FrameSinkFunction = FrameSinkAction (*)(ByteView frame, void* user_data) noexcept;

struct FrameSink {
  FrameSinkFunction function = nullptr;
  void* user_data = nullptr;
};

enum class SubmitApiStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_PLAN,
  WORKSPACE_PLAN_MISMATCH,
  WORKSPACE_BUSY,
  REENTRANT_CALL,
  LIMIT_EXCEEDED,
  INTERNAL_ERROR,
};

enum class SubmitStopReason {
  INPUT_EXHAUSTED,
  NEED_MORE,
  WORK_BUDGET_REACHED,
  SINK_STOP,
};

enum class FramingIssue {
  NONE,
  MALFORMED_LENGTH,
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  RECORD_TOO_LONG,
#endif
};

// 本次提交的消费/交付/丢弃与工作计数；API 状态与正常停止原因是不同维度。
// bytes_consumed 是输入前缀字节数，不是缓存量；未消费后缀未被 Workspace 保留。
struct SubmitResult {
  SubmitApiStatus api_status = SubmitApiStatus::OK;
  SubmitStopReason stop_reason = SubmitStopReason::INPUT_EXHAUSTED;
  std::size_t bytes_consumed = 0U;
  std::size_t frames_delivered = 0U;
  std::size_t bytes_discarded = 0U;
  std::size_t malformed_candidates = 0U;
  FramingIssue last_framing_issue = FramingIssue::NONE;
  std::size_t work_units_used = 0U;
};

// 0 选 Profile 默认值；非零请求仍受对应硬上限和创建时预算检查约束。
struct FramingLimitOverrides {
  std::size_t max_submit_bytes = 0U;
  std::size_t max_frames_per_submit = 0U;
  std::size_t max_work_units = 0U;
  std::size_t max_sync_bytes = 0U;
  std::size_t max_session_memory_bytes = 0U;
};

enum class StreamFramingPhase {
  COLLECTING,
  DELIVERY_PENDING,
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  DISCARDING_UNTIL_CRLF,
#endif
};

// 按值观察，不推进状态；半帧不一定能无输入推进，has_internal_work 与缓存量不同。
struct StreamFramingObservation {
  StreamFramingPhase phase = StreamFramingPhase::COLLECTING;
  std::size_t buffered_bytes = 0U;
  bool has_internal_work = false;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
};

class StreamFramingWorkspace;

struct WorkspaceCreateResult {
  SubmitApiStatus api_status = SubmitApiStatus::INVALID_PLAN;
  std::unique_ptr<StreamFramingWorkspace> workspace;
  std::size_t accounted_workspace_bytes = 0U;
  std::size_t effective_session_limit_bytes = 0U;
};

// 独占一个逻辑流的状态，永久借用同地址 Plan 和绑定 Pipeline；不延长 Plan 寿命。
// 缓存在创建时准备。调用/观察/销毁的串行与寿命由宿主保证，不是任意线程安全对象。
class StreamFramingWorkspace final {
 public:
  StreamFramingWorkspace(const StreamFramingWorkspace&) = delete;
  StreamFramingWorkspace& operator=(const StreamFramingWorkspace&) = delete;
  StreamFramingWorkspace(StreamFramingWorkspace&&) = delete;
  StreamFramingWorkspace& operator=(StreamFramingWorkspace&&) = delete;
  ~StreamFramingWorkspace() = default;

  std::size_t AccountedWorkspaceBytes() const noexcept;
  std::size_t BufferedBytes() const noexcept { return buffered_size_; }
  std::size_t TotalDiscardedBytes() const noexcept { return total_discarded_bytes_; }
  std::size_t TotalMalformedCandidates() const noexcept { return total_malformed_candidates_; }

  // The workspace owner may inspect this between serialized Push/Reset calls. This read does not
  // synchronize with concurrent mutation and does not advance the framing state.
  StreamFramingObservation Observe() const noexcept;

 private:
  enum class State {
    COLLECT_FIXED,
    SEARCH_SYNC,
    COPY_SYNC,
    READ_LENGTH,
    COLLECT_DECLARED,
    DELIVER_PENDING,
    RESCAN_INVALID,
    COMPACT_INVALID,
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    COLLECT_ASCII,
    DISCARD_UNTIL_CRLF,
#endif
  };

  StreamFramingWorkspace(const protocol_plan::PlanBundle& plan, std::size_t pipeline_index,
                         const protocol_plan::FrozenFramingPlan& framing,
                         std::size_t frame_capacity, std::size_t max_submit_bytes,
                         std::size_t max_frames_per_submit, std::size_t max_work_units,
                         std::size_t max_session_memory_bytes);
  bool HasInternalWork() const noexcept;
  bool HasHalfFrame() const noexcept;
  void ResetCandidate() noexcept;

  friend WorkspaceCreateResult CreateStreamFramingWorkspace(const protocol_plan::PlanBundle&,
                                                            std::size_t,
                                                            const FramingLimitOverrides&) noexcept;
  friend SubmitResult PushStreamChunk(const protocol_plan::PlanBundle&, StreamFramingWorkspace&,
                                      std::size_t, ByteView, FrameSink) noexcept;
  friend SubmitApiStatus ResetStreamFramingWorkspace(const protocol_plan::PlanBundle&,
                                                     StreamFramingWorkspace&, std::size_t) noexcept;

  const protocol_plan::PlanBundle* plan_ = nullptr;
  const protocol_plan::FrozenFramingPlan* framing_ = nullptr;
  std::size_t pipeline_index_ = 0U;
  // 缓存持有已消费候选字节，计数单位为字节；target_size_ 是目标总帧长。
  std::vector<std::uint8_t> buffer_;
  std::size_t buffered_size_ = 0U;
  std::size_t target_size_ = 0U;
  // 匹配前缀和恢复搬移的进度跨 Push 保留，不是待提交输入后缀的所有权。
  std::size_t sync_match_ = 0U;
  std::size_t copy_sync_index_ = 0U;
  std::size_t rescan_index_ = 0U;
  std::size_t rescan_match_ = 0U;
  std::size_t compact_start_ = 0U;
  std::size_t compact_size_ = 0U;
  std::size_t compact_moved_ = 0U;
  std::size_t max_submit_bytes_ = 0U;
  std::size_t max_frames_per_submit_ = 0U;
  std::size_t max_work_units_ = 0U;
  std::size_t max_session_memory_bytes_ = 0U;
  // Workspace 寿命期累计统计；ResetCandidate/显式 Reset 不清零这两个计数。
  std::size_t total_discarded_bytes_ = 0U;
  std::size_t total_malformed_candidates_ = 0U;
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  bool previous_was_cr_ = false;
#endif
  State state_ = State::COLLECT_FIXED;
  std::atomic_flag in_use_ = ATOMIC_FLAG_INIT;
};

WorkspaceCreateResult CreateStreamFramingWorkspace(
    const protocol_plan::PlanBundle& plan, std::size_t pipeline_index,
    const FramingLimitOverrides& overrides = {}) noexcept;

// 空输入也可推进内部工作；每次仍需有效 sink，预算停止保留状态供后续调用续处理。
SubmitResult PushStreamChunk(const protocol_plan::PlanBundle& plan,
                             StreamFramingWorkspace& workspace, std::size_t pipeline_index,
                             ByteView input, FrameSink sink) noexcept;

SubmitApiStatus ResetStreamFramingWorkspace(const protocol_plan::PlanBundle& plan,
                                            StreamFramingWorkspace& workspace,
                                            std::size_t pipeline_index) noexcept;

}  // namespace pae::protocol_framing
