#include "pae/stream_framer.h"

#include <atomic>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "../protocol_framing/stream_framer.h"
#include "compiled_state_internal.h"

namespace pae {
namespace {

namespace framing = protocol_framing;
namespace internal = public_api_internal;
namespace plan = protocol_plan;

struct CallbackScopeNode {
  const StreamFramer* owner = nullptr;
  const CallbackScopeNode* previous = nullptr;
};

thread_local const CallbackScopeNode* g_callback_scope = nullptr;

bool IsCallbackOwnerActive(const StreamFramer* owner) noexcept {
  for (const CallbackScopeNode* scope = g_callback_scope; scope != nullptr;
       scope = scope->previous) {
    if (scope->owner == owner) return true;
  }
  return false;
}

class CallbackScope final {
 public:
  explicit CallbackScope(const StreamFramer* owner) noexcept : node_{owner, g_callback_scope} {
    g_callback_scope = &node_;
  }
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope& operator=(const CallbackScope&) = delete;
  ~CallbackScope() { g_callback_scope = node_.previous; }

 private:
  CallbackScopeNode node_;
};

bool AddChecked(std::size_t value, std::size_t& total) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)() - total) return false;
  total += value;
  return true;
}

class FacadeLease final {
 public:
  explicit FacadeLease(std::atomic_flag& in_use) noexcept
      : in_use_(in_use), acquired_(!in_use_.test_and_set(std::memory_order_acquire)) {}
  FacadeLease(const FacadeLease&) = delete;
  FacadeLease& operator=(const FacadeLease&) = delete;
  ~FacadeLease() {
    if (acquired_) in_use_.clear(std::memory_order_release);
  }
  [[nodiscard]] bool Acquired() const noexcept { return acquired_; }

 private:
  std::atomic_flag& in_use_;
  bool acquired_ = false;
};

StreamFramerStatus MapStatus(framing::SubmitApiStatus status) noexcept {
  switch (status) {
    case framing::SubmitApiStatus::OK:
      return StreamFramerStatus::OK;
    case framing::SubmitApiStatus::INVALID_ARGUMENT:
      return StreamFramerStatus::INVALID_ARGUMENT;
    case framing::SubmitApiStatus::INVALID_PLAN:
    case framing::SubmitApiStatus::WORKSPACE_PLAN_MISMATCH:
      return StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
    case framing::SubmitApiStatus::WORKSPACE_BUSY:
      return StreamFramerStatus::WORKSPACE_BUSY;
    case framing::SubmitApiStatus::REENTRANT_CALL:
      return StreamFramerStatus::REENTRANT_CALL;
    case framing::SubmitApiStatus::LIMIT_EXCEEDED:
      return StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED;
    case framing::SubmitApiStatus::INTERNAL_ERROR:
      return StreamFramerStatus::INTERNAL_ERROR;
  }
  return StreamFramerStatus::INTERNAL_ERROR;
}

StreamFramerStopReason MapStopReason(framing::SubmitStopReason reason) noexcept {
  switch (reason) {
    case framing::SubmitStopReason::INPUT_EXHAUSTED:
      return StreamFramerStopReason::INPUT_EXHAUSTED;
    case framing::SubmitStopReason::NEED_MORE:
      return StreamFramerStopReason::NEED_MORE;
    case framing::SubmitStopReason::WORK_BUDGET_REACHED:
      return StreamFramerStopReason::WORK_BUDGET_REACHED;
    case framing::SubmitStopReason::SINK_STOP:
      return StreamFramerStopReason::SINK_STOP;
  }
  return StreamFramerStopReason::INPUT_EXHAUSTED;
}

StreamFramingIssue MapIssue(framing::FramingIssue issue) noexcept {
  switch (issue) {
    case framing::FramingIssue::NONE:
      return StreamFramingIssue::NONE;
    case framing::FramingIssue::MALFORMED_LENGTH:
      return StreamFramingIssue::MALFORMED_LENGTH;
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    case framing::FramingIssue::RECORD_TOO_LONG:
      return StreamFramingIssue::RECORD_TOO_LONG;
#endif
  }
  return StreamFramingIssue::NONE;
}

StreamFramingPhase MapPhase(framing::StreamFramingPhase phase) noexcept {
  switch (phase) {
    case framing::StreamFramingPhase::COLLECTING:
      return StreamFramingPhase::COLLECTING;
    case framing::StreamFramingPhase::DELIVERY_PENDING:
      return StreamFramingPhase::DELIVERY_PENDING;
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    case framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF:
      return StreamFramingPhase::DISCARDING_UNTIL_CRLF;
#endif
  }
  return StreamFramingPhase::COLLECTING;
}

bool OptionsWithinHardLimits(const StreamFramerOptions& options) noexcept {
  return (options.max_submit_bytes == 0U ||
          options.max_submit_bytes <= plan::kMaxStreamSubmitBytesHardLimit) &&
         (options.max_frames_per_submit == 0U ||
          options.max_frames_per_submit <= plan::kMaxStreamFramesPerSubmitHardLimit) &&
         (options.max_work_units == 0U ||
          options.max_work_units <= plan::kMaxStreamWorkUnitsHardLimit) &&
         (options.max_sync_bytes == 0U ||
          options.max_sync_bytes <= plan::kMaxStreamSyncBytesHardLimit) &&
         (options.max_session_memory_bytes == 0U ||
          options.max_session_memory_bytes <= plan::kMaxStreamSessionMemoryBytesHardLimit);
}

}  // namespace

struct StreamFramer::Impl final {
  Impl(internal::CompiledStateRef shared_state, std::size_t bound_pipeline,
       std::unique_ptr<framing::StreamFramingWorkspace> bound_workspace,
       StreamFramingMemoryReport memory_report) noexcept
      : state(std::move(shared_state)),
        pipeline_index(bound_pipeline),
        workspace(std::move(bound_workspace)),
        memory(memory_report) {}

  internal::CompiledStateRef state;
  std::size_t pipeline_index = 0U;
  std::unique_ptr<framing::StreamFramingWorkspace> workspace;
  StreamFramingMemoryReport memory;
  mutable std::atomic_flag facade_in_use = ATOMIC_FLAG_INIT;
};

StreamFramer::StreamFramer(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

StreamFramer::StreamFramer(StreamFramer&& other) noexcept : impl_(std::move(other.impl_)) {}

StreamFramer& StreamFramer::operator=(StreamFramer&& other) noexcept {
  if (this != &other) impl_ = std::move(other.impl_);
  return *this;
}

StreamFramer::~StreamFramer() = default;

StreamSubmitResult StreamFramer::Push(ByteView input, FrameSink sink) noexcept {
  StreamSubmitResult result;
  if (impl_ == nullptr) {
    result.status = StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }
  if ((input.data == nullptr && input.size != 0U) || sink.function == nullptr) return result;
  if (IsCallbackOwnerActive(this)) {
    result.status = StreamFramerStatus::REENTRANT_CALL;
    return result;
  }
  FacadeLease lease(impl_->facade_in_use);
  if (!lease.Acquired()) {
    result.status = StreamFramerStatus::WORKSPACE_BUSY;
    return result;
  }

  struct CallbackContext {
    StreamFramer* owner;
    FrameSink sink;
    std::size_t pipeline_index;
  };
  CallbackContext context{this, sink, impl_->pipeline_index};
  const framing::SubmitResult submitted = framing::PushStreamChunk(
      *impl_->state->Artifacts().Plan(), *impl_->workspace, impl_->pipeline_index,
      framing::ByteView{input.data, input.size},
      framing::FrameSink{
          [](framing::ByteView frame, void* opaque) noexcept {
            auto& callback = *static_cast<CallbackContext*>(opaque);
            CallbackScope callback_scope{callback.owner};
            const FrameSinkAction action = callback.sink.function(
                FrameCandidateView{ByteView{frame.data, frame.size}, callback.pipeline_index},
                callback.sink.context);
            return action == FrameSinkAction::CONTINUE ? framing::FrameSinkAction::CONTINUE
                                                       : framing::FrameSinkAction::STOP;
          },
          &context});

  result.status = MapStatus(submitted.api_status);
  result.stop_reason = MapStopReason(submitted.stop_reason);
  result.bytes_consumed = submitted.bytes_consumed;
  result.candidates_delivered = submitted.frames_delivered;
  result.bytes_discarded = submitted.bytes_discarded;
  result.malformed_candidates = submitted.malformed_candidates;
  result.last_issue = MapIssue(submitted.last_framing_issue);
  result.work_units_used = submitted.work_units_used;
  return result;
}

StreamSubmitResult StreamFramer::Continue(FrameSink sink) noexcept {
  return Push(ByteView{}, sink);
}

StreamFramerStatus StreamFramer::Reset() noexcept {
  if (impl_ == nullptr) return StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
  if (IsCallbackOwnerActive(this)) return StreamFramerStatus::REENTRANT_CALL;
  FacadeLease lease(impl_->facade_in_use);
  if (!lease.Acquired()) return StreamFramerStatus::WORKSPACE_BUSY;
  return MapStatus(framing::ResetStreamFramingWorkspace(*impl_->state->Artifacts().Plan(),
                                                        *impl_->workspace, impl_->pipeline_index));
}

StreamFramerObservation StreamFramer::Observe() const noexcept {
  StreamFramerObservation result;
  if (impl_ == nullptr) return result;
  if (IsCallbackOwnerActive(this)) {
    result.status = StreamFramerStatus::REENTRANT_CALL;
    return result;
  }
  FacadeLease lease(impl_->facade_in_use);
  if (!lease.Acquired()) {
    result.status = StreamFramerStatus::WORKSPACE_BUSY;
    return result;
  }
  const framing::StreamFramingObservation observed = impl_->workspace->Observe();
  result.status = StreamFramerStatus::OK;
  result.phase = MapPhase(observed.phase);
  result.buffered_bytes = observed.buffered_bytes;
  result.has_internal_work = observed.has_internal_work;
  result.effective_max_submit_bytes = observed.effective_max_submit_bytes;
  result.effective_max_work_units = observed.effective_max_work_units;
  return result;
}

StreamFramingMemoryReport StreamFramer::MemoryReport() const noexcept {
  return impl_ == nullptr ? StreamFramingMemoryReport{} : impl_->memory;
}

StreamFramingCapability QueryStreamFramingCapability(const CompiledProtocol& compiled,
                                                     std::size_t pipeline_index) noexcept {
  StreamFramingCapability result;
  const PipelineFramingQueryResult description =
      QueryPipelineFramingDescription(compiled, pipeline_index);
  if (description.status == PipelineFramingQueryStatus::PIPELINE_OUT_OF_RANGE) {
    result.status = StreamFramerStatus::PIPELINE_OUT_OF_RANGE;
    return result;
  }
  if (description.status != PipelineFramingQueryStatus::OK || !description.value) return result;
  result.status = StreamFramerStatus::OK;
  result.available = description.value->input_kind == PipelineInputKind::STREAM_CHUNK;
  return result;
}

PipelineFramingQueryResult QueryPipelineFramingDescription(const CompiledProtocol& compiled,
                                                           std::size_t pipeline_index) noexcept {
  PipelineFramingQueryResult result;
  internal::CompiledStateRef state = detail::CompiledProtocolAccess::Acquire(compiled);
  if (!state || state->Artifacts().Plan() == nullptr) return result;
  const auto& bundle = *state->Artifacts().Plan();
  if (pipeline_index >= bundle.Pipelines().size()) {
    result.status = PipelineFramingQueryStatus::PIPELINE_OUT_OF_RANGE;
    return result;
  }
  const auto& pipeline = bundle.Pipelines()[pipeline_index];
  if (pipeline.framing_profile_index >= bundle.FramingProfiles().size()) {
    result.status = PipelineFramingQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& profile = bundle.FramingProfiles()[pipeline.framing_profile_index];
  PipelineFramingDescription value;
  value.pipeline_index = pipeline_index;
  switch (profile.input_kind) {
    case plan::InputKind::COMPLETE_RECORD:
      if (profile.strategy != plan::FramingStrategy::COMPLETE_RECORD) {
        result.status = PipelineFramingQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
      value.input_kind = PipelineInputKind::COMPLETE_RECORD;
      value.strategy = PipelineFramingStrategy::COMPLETE_RECORD;
      break;
    case plan::InputKind::STREAM_CHUNK: {
      value.input_kind = PipelineInputKind::STREAM_CHUNK;
      std::uint64_t maximum = 0U;
      switch (profile.strategy) {
        case plan::FramingStrategy::FIXED_LENGTH:
          value.strategy = PipelineFramingStrategy::FIXED_LENGTH;
          maximum = profile.frame_length_bytes;
          break;
        case plan::FramingStrategy::SYNC_FIXED_LENGTH:
          value.strategy = PipelineFramingStrategy::SYNC_FIXED_LENGTH;
          maximum = profile.frame_length_bytes;
          break;
        case plan::FramingStrategy::SYNC_LENGTH_FIELD:
          value.strategy = PipelineFramingStrategy::SYNC_LENGTH_FIELD;
          maximum = profile.maximum_frame_length;
          break;
        case plan::FramingStrategy::ASCII_CRLF:
          value.strategy = PipelineFramingStrategy::ASCII_CRLF;
          maximum = profile.maximum_frame_length;
          break;
        case plan::FramingStrategy::COMPLETE_RECORD:
          result.status = PipelineFramingQueryStatus::INTERNAL_CONTRACT_VIOLATION;
          return result;
      }
      if (maximum == 0U || maximum > plan::kMaxStreamFrameBytesHardLimit ||
          maximum > (std::numeric_limits<std::size_t>::max)()) {
        result.status = PipelineFramingQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
      value.maximum_candidate_frame_bytes = static_cast<std::size_t>(maximum);
      break;
    }
    default:
      result.status = PipelineFramingQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
  }
  result.status = PipelineFramingQueryStatus::OK;
  result.value = value;
  return result;
}

StreamFramerCreateResult CreateStreamFramer(const CompiledProtocol& compiled,
                                            std::size_t pipeline_index,
                                            const StreamFramerOptions& options) noexcept {
  StreamFramerCreateResult result;
  internal::CompiledStateRef state = detail::CompiledProtocolAccess::Acquire(compiled);
  if (!state || state->Artifacts().Plan() == nullptr) {
    result.status = StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }
  const auto& bundle = *state->Artifacts().Plan();
  if (pipeline_index >= bundle.Pipelines().size()) {
    result.status = StreamFramerStatus::PIPELINE_OUT_OF_RANGE;
    return result;
  }
  const auto& pipeline = bundle.Pipelines()[pipeline_index];
  if (pipeline.framing_profile_index >= bundle.FramingProfiles().size()) {
    result.status = StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }
  const auto& profile = bundle.FramingProfiles()[pipeline.framing_profile_index];
  if (profile.input_kind != plan::InputKind::STREAM_CHUNK) {
    result.status = StreamFramerStatus::INPUT_KIND_NOT_STREAM;
    return result;
  }
  if (!OptionsWithinHardLimits(options) ||
      (options.max_sync_bytes != 0U && profile.sync_bytes.size() > options.max_sync_bytes)) {
    result.status = StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED;
    return result;
  }

  const framing::FramingLimitOverrides overrides{
      options.max_submit_bytes, options.max_frames_per_submit, options.max_work_units,
      options.max_sync_bytes, options.max_session_memory_bytes};
  framing::WorkspaceCreateResult created =
      framing::CreateStreamFramingWorkspace(bundle, pipeline_index, overrides);

  StreamFramingMemoryReport memory;
  memory.internal_workspace_bytes = created.accounted_workspace_bytes;
  memory.facade_bytes = sizeof(StreamFramer) + sizeof(StreamFramer::Impl);
  memory.framer_accounted_total_bytes = memory.internal_workspace_bytes;
  memory.retained_compiled_state_facade_bytes = internal::CompiledState::FacadeBytes();
  memory.effective_session_limit_bytes = created.effective_session_limit_bytes;
  // Workspace owner, frame-buffer storage, facade Impl and facade owner. MSVC's checked-iterator
  // vector representation owns one additional proxy allocation in Debug builds.
  memory.allocation_count = 4U;
#if defined(_MSC_VER) && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
  ++memory.allocation_count;
#endif
  if (!AddChecked(memory.facade_bytes, memory.framer_accounted_total_bytes)) {
    result.status = StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED;
    result.memory = memory;
    return result;
  }
  result.memory = memory;
  if (created.api_status != framing::SubmitApiStatus::OK || !created.workspace) {
    // The current internal create path reports INTERNAL_ERROR only when workspace construction
    // throws. Preserve a precise public allocation failure without changing the internal Framer.
    result.status = created.api_status == framing::SubmitApiStatus::INTERNAL_ERROR
                        ? StreamFramerStatus::ALLOCATION_FAILED
                        : MapStatus(created.api_status);
    return result;
  }

  try {
    auto impl = std::make_unique<StreamFramer::Impl>(state, pipeline_index,
                                                     std::move(created.workspace), memory);
    result.framer.reset(new StreamFramer(std::move(impl)));
  } catch (const std::bad_alloc&) {
    result.status = StreamFramerStatus::ALLOCATION_FAILED;
    return result;
  } catch (...) {
    result.status = StreamFramerStatus::INTERNAL_ERROR;
    return result;
  }
  result.status = StreamFramerStatus::OK;
  return result;
}

}  // namespace pae
