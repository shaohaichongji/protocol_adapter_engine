#include "stream_framer.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <new>

namespace pae::protocol_framing {
namespace {

thread_local StreamFramingWorkspace* g_callback_workspace = nullptr;

std::size_t SelectLimit(std::size_t requested, std::size_t profile_default, std::size_t hard_limit,
                        bool& valid) noexcept {
  const std::size_t selected = requested == 0U ? profile_default : requested;
  if (selected == 0U || selected > hard_limit) valid = false;
  return selected;
}

class UseGuard final {
 public:
  explicit UseGuard(std::atomic_flag& flag) noexcept : flag_(flag) {}
  ~UseGuard() { flag_.clear(std::memory_order_release); }

 private:
  std::atomic_flag& flag_;
};

}  // namespace

StreamFramingWorkspace::StreamFramingWorkspace(
    const protocol_plan::PlanBundle& plan, std::size_t pipeline_index,
    const protocol_plan::FrozenFramingPlan& framing, std::size_t frame_capacity,
    std::size_t max_submit_bytes, std::size_t max_frames_per_submit, std::size_t max_work_units,
    std::size_t max_session_memory_bytes)
    : plan_(&plan),
      framing_(&framing),
      pipeline_index_(pipeline_index),
      buffer_(frame_capacity),
      max_submit_bytes_(max_submit_bytes),
      max_frames_per_submit_(max_frames_per_submit),
      max_work_units_(max_work_units),
      max_session_memory_bytes_(max_session_memory_bytes) {
  ResetCandidate();
}

bool StreamFramingWorkspace::HasInternalWork() const noexcept {
  return state_ == State::COPY_SYNC || state_ == State::RESCAN_INVALID ||
         state_ == State::COMPACT_INVALID || state_ == State::DELIVER_PENDING ||
         (state_ == State::READ_LENGTH &&
          buffered_size_ >= framing_->length_field_offset + framing_->length_field_width);
}

bool StreamFramingWorkspace::HasHalfFrame() const noexcept {
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  if (state_ == State::COLLECT_ASCII) return buffered_size_ != 0U;
  if (state_ == State::DISCARD_UNTIL_CRLF) return true;
#endif
  if (state_ == State::COLLECT_FIXED || state_ == State::COLLECT_DECLARED ||
      state_ == State::READ_LENGTH) {
    return buffered_size_ != 0U;
  }
  return state_ == State::SEARCH_SYNC && sync_match_ != 0U;
}

void StreamFramingWorkspace::ResetCandidate() noexcept {
  buffered_size_ = 0U;
  target_size_ = 0U;
  sync_match_ = 0U;
  copy_sync_index_ = 0U;
  rescan_index_ = 0U;
  rescan_match_ = 0U;
  compact_start_ = 0U;
  compact_size_ = 0U;
  compact_moved_ = 0U;
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  previous_was_cr_ = false;
  if (framing_->strategy == protocol_plan::FramingStrategy::ASCII_CRLF) {
    state_ = State::COLLECT_ASCII;
    return;
  }
#endif
  state_ = framing_->strategy == protocol_plan::FramingStrategy::FIXED_LENGTH ? State::COLLECT_FIXED
                                                                              : State::SEARCH_SYNC;
  if (state_ == State::COLLECT_FIXED) {
    target_size_ = static_cast<std::size_t>(framing_->frame_length_bytes);
  }
}

std::size_t StreamFramingWorkspace::AccountedWorkspaceBytes() const noexcept {
  return sizeof(StreamFramingWorkspace) + buffer_.capacity() * sizeof(std::uint8_t);
}

StreamFramingObservation StreamFramingWorkspace::Observe() const noexcept {
  StreamFramingObservation observation;
  if (state_ == State::DELIVER_PENDING) {
    observation.phase = StreamFramingPhase::DELIVERY_PENDING;
  }
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  else if (state_ == State::DISCARD_UNTIL_CRLF) {
    observation.phase = StreamFramingPhase::DISCARDING_UNTIL_CRLF;
  }
#endif
  observation.buffered_bytes = buffered_size_;
  observation.has_internal_work = HasInternalWork();
  observation.effective_max_submit_bytes = max_submit_bytes_;
  observation.effective_max_work_units = max_work_units_;
  return observation;
}

WorkspaceCreateResult CreateStreamFramingWorkspace(
    const protocol_plan::PlanBundle& plan, std::size_t pipeline_index,
    const FramingLimitOverrides& overrides) noexcept {
  WorkspaceCreateResult result;
  if ((plan.SchemaVersion() != "0.9"
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
       && plan.SchemaVersion() != "0.11"
#endif
       ) ||
      pipeline_index >= plan.Pipelines().size()) {
    return result;
  }
  const auto& pipeline = plan.Pipelines()[pipeline_index];
  if (pipeline.framing_profile_index >= plan.FramingProfiles().size()) return result;
  const auto& framing = plan.FramingProfiles()[pipeline.framing_profile_index];
  if (framing.input_kind != protocol_plan::InputKind::STREAM_CHUNK) return result;
  const auto* profile = protocol_plan::GetResourceProfileLimits(plan.GetResourceProfile());
  if (profile == nullptr) return result;

  bool valid = true;
  const std::size_t max_submit = SelectLimit(overrides.max_submit_bytes, profile->max_submit_bytes,
                                             protocol_plan::kMaxStreamSubmitBytesHardLimit, valid);
  const std::size_t max_frames =
      SelectLimit(overrides.max_frames_per_submit, profile->max_frames_per_submit,
                  protocol_plan::kMaxStreamFramesPerSubmitHardLimit, valid);
  const std::size_t max_work = SelectLimit(overrides.max_work_units, profile->max_framer_work_units,
                                           protocol_plan::kMaxStreamWorkUnitsHardLimit, valid);
  const std::size_t max_sync = SelectLimit(overrides.max_sync_bytes, profile->max_sync_bytes,
                                           protocol_plan::kMaxStreamSyncBytesHardLimit, valid);
  const std::size_t max_session =
      SelectLimit(overrides.max_session_memory_bytes, profile->max_session_memory_bytes,
                  protocol_plan::kMaxStreamSessionMemoryBytesHardLimit, valid);
  const std::uint64_t frame_capacity64 =
      framing.strategy == protocol_plan::FramingStrategy::SYNC_LENGTH_FIELD
          ? framing.maximum_frame_length
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
      : framing.strategy == protocol_plan::FramingStrategy::ASCII_CRLF
          ? framing.maximum_frame_length
#endif
          : framing.frame_length_bytes;
  if (!valid || frame_capacity64 == 0U ||
      frame_capacity64 > protocol_plan::kMaxStreamFrameBytesHardLimit ||
      frame_capacity64 > (std::numeric_limits<std::size_t>::max)() ||
      framing.sync_bytes.size() > max_sync ||
      framing.sync_prefix_table.size() != framing.sync_bytes.size()) {
    return result;
  }
  const std::size_t frame_capacity = static_cast<std::size_t>(frame_capacity64);
  if (frame_capacity > (std::numeric_limits<std::size_t>::max)() - sizeof(StreamFramingWorkspace)) {
    return result;
  }
  const std::size_t accounted = sizeof(StreamFramingWorkspace) + frame_capacity;
  result.accounted_workspace_bytes = accounted;
  result.effective_session_limit_bytes = max_session;
  const std::size_t minimum_progress_budget =
      (std::max)(std::size_t{4U}, std::size_t{1U} + 2U * framing.sync_bytes.size());
  if (accounted > max_session || max_work < minimum_progress_budget) {
    result.api_status = SubmitApiStatus::LIMIT_EXCEEDED;
    return result;
  }
  try {
    result.workspace.reset(new StreamFramingWorkspace(plan, pipeline_index, framing, frame_capacity,
                                                      max_submit, max_frames, max_work,
                                                      max_session));
  } catch (...) {
    result.api_status = SubmitApiStatus::INTERNAL_ERROR;
    return result;
  }
  result.api_status = SubmitApiStatus::OK;
  return result;
}

SubmitResult PushStreamChunk(const protocol_plan::PlanBundle& plan,
                             StreamFramingWorkspace& workspace, std::size_t pipeline_index,
                             ByteView input, FrameSink sink) noexcept {
  SubmitResult result;
  if ((input.data == nullptr && input.size != 0U) || sink.function == nullptr) {
    result.api_status = SubmitApiStatus::INVALID_ARGUMENT;
    return result;
  }
  if ((plan.SchemaVersion() != "0.9"
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
       && plan.SchemaVersion() != "0.11"
#endif
       ) ||
      pipeline_index >= plan.Pipelines().size()) {
    result.api_status = SubmitApiStatus::INVALID_PLAN;
    return result;
  }
  if (workspace.plan_ != &plan || workspace.pipeline_index_ != pipeline_index) {
    result.api_status = SubmitApiStatus::WORKSPACE_PLAN_MISMATCH;
    return result;
  }
  if (g_callback_workspace == &workspace) {
    result.api_status = SubmitApiStatus::REENTRANT_CALL;
    return result;
  }
  if (workspace.in_use_.test_and_set(std::memory_order_acquire)) {
    result.api_status = SubmitApiStatus::WORKSPACE_BUSY;
    return result;
  }
  UseGuard use_guard{workspace.in_use_};
  if (input.size > workspace.max_submit_bytes_) {
    result.api_status = SubmitApiStatus::LIMIT_EXCEEDED;
    return result;
  }

  const auto charge = [&result, &workspace](std::size_t amount) noexcept {
    if (amount > workspace.max_work_units_ - result.work_units_used) return false;
    result.work_units_used += amount;
    return true;
  };
  const auto budget_stop = [&result]() noexcept {
    result.stop_reason = SubmitStopReason::WORK_BUDGET_REACHED;
    return result;
  };
  const auto record_discard = [&result, &workspace](std::size_t amount) noexcept {
    result.bytes_discarded += amount;
    workspace.total_discarded_bytes_ += amount;
  };
  std::size_t input_index = 0U;
  for (;;) {
    using State = StreamFramingWorkspace::State;
    const auto state = workspace.state_;
    if (state == State::DELIVER_PENDING) {
      if (result.frames_delivered >= workspace.max_frames_per_submit_) return budget_stop();
      StreamFramingWorkspace* previous = g_callback_workspace;
      g_callback_workspace = &workspace;
      const FrameSinkAction action = sink.function(
          ByteView{workspace.buffer_.data(), workspace.buffered_size_}, sink.user_data);
      g_callback_workspace = previous;
      ++result.frames_delivered;
      workspace.ResetCandidate();
      if (action == FrameSinkAction::STOP) {
        result.stop_reason = SubmitStopReason::SINK_STOP;
        return result;
      }
      continue;
    }

#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    if (state == State::COLLECT_ASCII) {
      if (input_index == input.size) break;
      if (!charge(2U)) return budget_stop();
      if (workspace.buffered_size_ >= workspace.buffer_.size()) {
        result.api_status = SubmitApiStatus::INTERNAL_ERROR;
        return result;
      }
      const std::uint8_t byte = input.data[input_index++];
      workspace.buffer_[workspace.buffered_size_++] = byte;
      ++result.bytes_consumed;
      const bool terminated = workspace.buffered_size_ >= 2U &&
                              workspace.buffer_[workspace.buffered_size_ - 2U] == 0x0DU &&
                              byte == 0x0AU;
      if (terminated) {
        workspace.state_ = State::DELIVER_PENDING;
      } else if (workspace.buffered_size_ == workspace.buffer_.size()) {
        ++result.malformed_candidates;
        ++workspace.total_malformed_candidates_;
        result.last_framing_issue = FramingIssue::RECORD_TOO_LONG;
        workspace.previous_was_cr_ = byte == 0x0DU;
        record_discard(workspace.buffered_size_);
        workspace.buffered_size_ = 0U;
        workspace.state_ = State::DISCARD_UNTIL_CRLF;
      }
      continue;
    }

    if (state == State::DISCARD_UNTIL_CRLF) {
      if (input_index == input.size) break;
      if (!charge(2U)) return budget_stop();
      const std::uint8_t byte = input.data[input_index++];
      ++result.bytes_consumed;
      record_discard(1U);
      const bool terminated = workspace.previous_was_cr_ && byte == 0x0AU;
      workspace.previous_was_cr_ = byte == 0x0DU;
      if (terminated) {
        workspace.previous_was_cr_ = false;
        workspace.state_ = State::COLLECT_ASCII;
      }
      continue;
    }
#endif

    if (state == State::COPY_SYNC) {
      while (workspace.copy_sync_index_ < workspace.framing_->sync_bytes.size()) {
        if (!charge(1U)) return budget_stop();
        if (workspace.buffered_size_ >= workspace.buffer_.size()) {
          result.api_status = SubmitApiStatus::INTERNAL_ERROR;
          return result;
        }
        workspace.buffer_[workspace.buffered_size_++] =
            workspace.framing_->sync_bytes[workspace.copy_sync_index_++];
      }
      workspace.copy_sync_index_ = 0U;
      if (workspace.framing_->strategy == protocol_plan::FramingStrategy::SYNC_FIXED_LENGTH) {
        workspace.target_size_ = static_cast<std::size_t>(workspace.framing_->frame_length_bytes);
        workspace.state_ = workspace.buffered_size_ == workspace.target_size_
                               ? State::DELIVER_PENDING
                               : State::COLLECT_DECLARED;
      } else {
        workspace.state_ = State::READ_LENGTH;
      }
      continue;
    }

    if (state == State::RESCAN_INVALID) {
      const auto& sync = workspace.framing_->sync_bytes;
      const auto& prefix = workspace.framing_->sync_prefix_table;
      while (workspace.rescan_index_ < workspace.buffered_size_) {
        const std::uint8_t byte = workspace.buffer_[workspace.rescan_index_];
        std::size_t next_match = workspace.rescan_match_;
        std::size_t units = 1U;
        while (next_match != 0U && byte != sync[next_match]) {
          units += 2U;
          next_match = prefix[next_match - 1U];
        }
        ++units;
        if (byte == sync[next_match]) ++next_match;
        if (!charge(units)) return budget_stop();
        workspace.rescan_match_ = next_match;
        ++workspace.rescan_index_;
        if (workspace.rescan_match_ == sync.size()) {
          workspace.compact_start_ = workspace.rescan_index_ - sync.size();
          workspace.compact_size_ = workspace.buffered_size_ - workspace.compact_start_;
          workspace.compact_moved_ = 0U;
          workspace.state_ = State::COMPACT_INVALID;
          break;
        }
      }
      if (workspace.state_ == State::COMPACT_INVALID) continue;
      const std::size_t retained = workspace.rescan_match_;
      record_discard(workspace.buffered_size_ - retained);
      workspace.buffered_size_ = 0U;
      workspace.sync_match_ = retained;
      workspace.rescan_index_ = 0U;
      workspace.rescan_match_ = 0U;
      workspace.state_ = State::SEARCH_SYNC;
      continue;
    }

    if (state == State::COMPACT_INVALID) {
      const std::size_t remaining = workspace.compact_size_ - workspace.compact_moved_;
      const std::size_t available = workspace.max_work_units_ - result.work_units_used;
      if (remaining != 0U) {
        const std::size_t chunk = (std::min)(remaining, available);
        if (chunk == 0U) return budget_stop();
        if (!charge(chunk)) return budget_stop();
        std::memmove(workspace.buffer_.data() + workspace.compact_moved_,
                     workspace.buffer_.data() + workspace.compact_start_ + workspace.compact_moved_,
                     chunk);
        workspace.compact_moved_ += chunk;
        if (workspace.compact_moved_ != workspace.compact_size_) return budget_stop();
      }
      record_discard(workspace.compact_start_);
      workspace.buffered_size_ = workspace.compact_size_;
      workspace.compact_start_ = 0U;
      workspace.compact_size_ = 0U;
      workspace.compact_moved_ = 0U;
      workspace.rescan_index_ = 0U;
      workspace.rescan_match_ = 0U;
      workspace.state_ = State::READ_LENGTH;
      continue;
    }

    if (state == State::READ_LENGTH) {
      const std::size_t header_end = static_cast<std::size_t>(
          workspace.framing_->length_field_offset + workspace.framing_->length_field_width);
      if (workspace.buffered_size_ < header_end) {
        if (input_index == input.size) break;
        if (!charge(2U)) return budget_stop();
        if (workspace.buffered_size_ >= workspace.buffer_.size()) {
          result.api_status = SubmitApiStatus::INTERNAL_ERROR;
          return result;
        }
        workspace.buffer_[workspace.buffered_size_++] = input.data[input_index++];
        ++result.bytes_consumed;
        continue;
      }
      const std::size_t width = static_cast<std::size_t>(workspace.framing_->length_field_width);
      if (!charge(width)) return budget_stop();
      std::uint64_t declared = 0U;
      for (std::size_t index = 0U; index < width; ++index) {
        const std::size_t encoded_index =
            workspace.framing_->length_field_byte_order == protocol_plan::ByteOrder::LITTLE
                ? width - 1U - index
                : index;
        declared =
            (declared << 8U) |
            workspace.buffer_[static_cast<std::size_t>(workspace.framing_->length_field_offset) +
                              encoded_index];
      }
      if (declared < workspace.framing_->minimum_frame_length ||
          declared > workspace.framing_->maximum_frame_length ||
          declared > workspace.buffer_.size()) {
        ++result.malformed_candidates;
        ++workspace.total_malformed_candidates_;
        result.last_framing_issue = FramingIssue::MALFORMED_LENGTH;
        workspace.rescan_index_ = 1U;
        workspace.rescan_match_ = 0U;
        workspace.state_ = State::RESCAN_INVALID;
        continue;
      }
      workspace.target_size_ = static_cast<std::size_t>(declared);
      workspace.state_ = workspace.buffered_size_ == workspace.target_size_
                             ? State::DELIVER_PENDING
                             : State::COLLECT_DECLARED;
      continue;
    }

    if (state == State::COLLECT_FIXED || state == State::COLLECT_DECLARED) {
      if (workspace.buffered_size_ == workspace.target_size_) {
        workspace.state_ = State::DELIVER_PENDING;
        continue;
      }
      if (input_index == input.size) break;
      if (!charge(2U)) return budget_stop();
      if (workspace.buffered_size_ >= workspace.buffer_.size()) {
        result.api_status = SubmitApiStatus::INTERNAL_ERROR;
        return result;
      }
      workspace.buffer_[workspace.buffered_size_++] = input.data[input_index++];
      ++result.bytes_consumed;
      continue;
    }

    if (state == State::SEARCH_SYNC) {
      if (input_index == input.size) break;
      const auto& sync = workspace.framing_->sync_bytes;
      const auto& prefix = workspace.framing_->sync_prefix_table;
      const std::uint8_t byte = input.data[input_index];
      const std::size_t old_match = workspace.sync_match_;
      std::size_t next_match = old_match;
      std::size_t units = 1U;
      while (next_match != 0U && byte != sync[next_match]) {
        units += 2U;
        next_match = prefix[next_match - 1U];
      }
      ++units;
      if (byte == sync[next_match]) ++next_match;
      if (!charge(units)) return budget_stop();
      workspace.sync_match_ = next_match;
      ++input_index;
      ++result.bytes_consumed;
      if (workspace.sync_match_ == sync.size()) {
        workspace.sync_match_ = 0U;
        workspace.buffered_size_ = 0U;
        workspace.copy_sync_index_ = 0U;
        workspace.state_ = State::COPY_SYNC;
      } else {
        record_discard(old_match + 1U - workspace.sync_match_);
      }
      continue;
    }

    result.api_status = SubmitApiStatus::INTERNAL_ERROR;
    return result;
  }

  if (workspace.HasInternalWork()) {
    result.stop_reason = SubmitStopReason::WORK_BUDGET_REACHED;
  } else if (workspace.HasHalfFrame()) {
    result.stop_reason = SubmitStopReason::NEED_MORE;
  } else {
    result.stop_reason = SubmitStopReason::INPUT_EXHAUSTED;
  }
  return result;
}

SubmitApiStatus ResetStreamFramingWorkspace(const protocol_plan::PlanBundle& plan,
                                            StreamFramingWorkspace& workspace,
                                            std::size_t pipeline_index) noexcept {
  if (workspace.plan_ != &plan || workspace.pipeline_index_ != pipeline_index) {
    return SubmitApiStatus::WORKSPACE_PLAN_MISMATCH;
  }
  if (g_callback_workspace == &workspace) return SubmitApiStatus::REENTRANT_CALL;
  if (workspace.in_use_.test_and_set(std::memory_order_acquire)) {
    return SubmitApiStatus::WORKSPACE_BUSY;
  }
  UseGuard use_guard{workspace.in_use_};
  workspace.ResetCandidate();
  return SubmitApiStatus::OK;
}

}  // namespace pae::protocol_framing
