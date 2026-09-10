#pragma once

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

enum class FramingIssue { NONE, MALFORMED_LENGTH };

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

struct FramingLimitOverrides {
  std::size_t max_submit_bytes = 0U;
  std::size_t max_frames_per_submit = 0U;
  std::size_t max_work_units = 0U;
  std::size_t max_sync_bytes = 0U;
  std::size_t max_session_memory_bytes = 0U;
};

class StreamFramingWorkspace;

struct WorkspaceCreateResult {
  SubmitApiStatus api_status = SubmitApiStatus::INVALID_PLAN;
  std::unique_ptr<StreamFramingWorkspace> workspace;
  std::size_t accounted_workspace_bytes = 0U;
  std::size_t effective_session_limit_bytes = 0U;
};

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
  std::vector<std::uint8_t> buffer_;
  std::size_t buffered_size_ = 0U;
  std::size_t target_size_ = 0U;
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
  std::size_t total_discarded_bytes_ = 0U;
  std::size_t total_malformed_candidates_ = 0U;
  State state_ = State::COLLECT_FIXED;
  std::atomic_flag in_use_ = ATOMIC_FLAG_INIT;
};

WorkspaceCreateResult CreateStreamFramingWorkspace(
    const protocol_plan::PlanBundle& plan, std::size_t pipeline_index,
    const FramingLimitOverrides& overrides = {}) noexcept;

SubmitResult PushStreamChunk(const protocol_plan::PlanBundle& plan,
                             StreamFramingWorkspace& workspace, std::size_t pipeline_index,
                             ByteView input, FrameSink sink) noexcept;

SubmitApiStatus ResetStreamFramingWorkspace(const protocol_plan::PlanBundle& plan,
                                            StreamFramingWorkspace& workspace,
                                            std::size_t pipeline_index) noexcept;

}  // namespace pae::protocol_framing
