#include "pae/host_endpoint.h"

#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "compiled_state_internal.h"
#if defined(PAE_ENABLE_PUBLIC_HOST_TEST_HOOKS)
#include "host_endpoint_test_support.h"
#endif

namespace pae {

namespace detail {
class HostScopeToken final {};
}  // namespace detail

namespace {

namespace internal = public_api_internal;

constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);
constexpr std::size_t kHardMaxBindings = 64U;
constexpr std::size_t kHardMaxChannels = 64U;
constexpr std::size_t kHardMaxIdentityBytes = 256U;
constexpr std::size_t kHardMaxAccountedBytes = 64U * 1024U * 1024U;

bool AddChecked(std::size_t value, std::size_t& total) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)() - total) return false;
  total += value;
  return true;
}

bool MultiplyChecked(std::size_t left, std::size_t right, std::size_t& output) noexcept {
  if (right != 0U && left > (std::numeric_limits<std::size_t>::max)() / right) return false;
  output = left * right;
  return true;
}

bool HasFramingOverrides(const StreamFramerOptions& options) noexcept {
  return options.max_submit_bytes != 0U || options.max_frames_per_submit != 0U ||
         options.max_work_units != 0U || options.max_sync_bytes != 0U ||
         options.max_session_memory_bytes != 0U;
}

struct EncodePipelineCapacity {
  bool valid = false;
  bool available = false;
  std::size_t message_count = 0U;
  std::size_t maximum_output_size = 0U;
};

EncodePipelineCapacity QueryEncodePipelineCapacity(const CompiledProtocol& compiled,
                                                   std::size_t pipeline_index) noexcept {
  EncodePipelineCapacity result;
  const auto pipeline = compiled.Pipeline(pipeline_index);
  if (!pipeline) return result;
  result.valid = true;
  for (std::size_t association = 0U; association < pipeline->message_count; ++association) {
    const auto message_index = compiled.PipelineMessageIndex(pipeline_index, association);
    if (!message_index) {
      result.valid = false;
      return result;
    }
    const auto execution = compiled.PipelineMessageExecution(pipeline_index, *message_index);
    if (!execution) {
      result.valid = false;
      return result;
    }
    if (!execution->encode_available) continue;
    if (execution->encode_output_size_kind != EncodeOutputSizeKind::EXACT &&
        execution->encode_output_size_kind != EncodeOutputSizeKind::UPPER_BOUND) {
      result.valid = false;
      return result;
    }
    if (result.message_count == (std::numeric_limits<std::size_t>::max)()) {
      result.valid = false;
      return result;
    }
    result.available = true;
    ++result.message_count;
    if (execution->encode_output_size > result.maximum_output_size) {
      result.maximum_output_size = execution->encode_output_size;
    }
  }
  return result;
}

HostStatus MapCodecCreateStatus(CodecStatus status) noexcept {
  switch (status) {
    case CodecStatus::OK:
      return HostStatus::OK;
    case CodecStatus::INVALID_COMPILED_PROTOCOL:
      return HostStatus::INVALID_COMPILED_PROTOCOL;
    case CodecStatus::RESOURCE_LIMIT_EXCEEDED:
      return HostStatus::RESOURCE_LIMIT_EXCEEDED;
    case CodecStatus::ALLOCATION_FAILED:
      return HostStatus::ALLOCATION_FAILED;
    default:
      return HostStatus::INTERNAL_ERROR;
  }
}

HostStatus MapFramerCreateStatus(StreamFramerStatus status) noexcept {
  switch (status) {
    case StreamFramerStatus::OK:
      return HostStatus::OK;
    case StreamFramerStatus::INVALID_COMPILED_PROTOCOL:
      return HostStatus::INVALID_COMPILED_PROTOCOL;
    case StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED:
      return HostStatus::RESOURCE_LIMIT_EXCEEDED;
    case StreamFramerStatus::ALLOCATION_FAILED:
      return HostStatus::ALLOCATION_FAILED;
    case StreamFramerStatus::PIPELINE_OUT_OF_RANGE:
    case StreamFramerStatus::INPUT_KIND_NOT_STREAM:
    case StreamFramerStatus::INVALID_ARGUMENT:
      return HostStatus::INVALID_BINDING;
    default:
      return HostStatus::INTERNAL_ERROR;
  }
}

struct CallbackScopeNode {
  const HostEndpoint* owner = nullptr;
  const CallbackScopeNode* previous = nullptr;
};

thread_local const CallbackScopeNode* g_callback_scope = nullptr;

bool IsCallbackOwnerActive(const HostEndpoint* owner) noexcept {
  for (const CallbackScopeNode* scope = g_callback_scope; scope != nullptr;
       scope = scope->previous) {
    if (scope->owner == owner) return true;
  }
  return false;
}

class CallbackScope final {
 public:
  explicit CallbackScope(const HostEndpoint* owner) noexcept : node_{owner, g_callback_scope} {
    g_callback_scope = &node_;
  }
  CallbackScope(const CallbackScope&) = delete;
  CallbackScope& operator=(const CallbackScope&) = delete;
  ~CallbackScope() { g_callback_scope = node_.previous; }

 private:
  CallbackScopeNode node_;
};

class HostLease final {
 public:
  explicit HostLease(std::atomic_flag& in_use) noexcept
      : in_use_(in_use), acquired_(!in_use_.test_and_set(std::memory_order_acquire)) {}
  HostLease(const HostLease&) = delete;
  HostLease& operator=(const HostLease&) = delete;
  ~HostLease() {
    if (acquired_) in_use_.clear(std::memory_order_release);
  }
  [[nodiscard]] bool Acquired() const noexcept { return acquired_; }

 private:
  std::atomic_flag& in_use_;
  bool acquired_ = false;
};

}  // namespace

struct HostEndpoint::Impl final {
  struct Binding {
    std::unique_ptr<char[]> endpoint;
    std::size_t endpoint_size = 0U;
    HostAction action = HostAction::DECODE;
    std::size_t pipeline_index = 0U;
    std::unique_ptr<std::size_t[]> encode_message_indices;
    std::size_t encode_message_count = 0U;
    std::size_t first_channel = 0U;
    std::size_t channel_count = 0U;
    StreamFramerOptions framing_options;
  };

  struct Channel {
    std::size_t binding_index = 0U;
    std::uint64_t generation = 1U;
    bool faulted = false;
    std::unique_ptr<CompleteRecordCodec> codec;
    std::unique_ptr<StreamFramer> framer;
    std::unique_ptr<std::uint8_t[]> encode_buffer;
    std::size_t encode_capacity = 0U;
  };

  internal::CompiledStateRef state;
  std::shared_ptr<const detail::HostScopeToken> scope;
  std::unique_ptr<Binding[]> bindings;
  std::size_t binding_count = 0U;
  std::unique_ptr<Channel[]> channels;
  std::size_t channel_count = 0U;
  HostMemoryReport memory;
  mutable std::atomic_flag in_use = ATOMIC_FLAG_INIT;

  [[nodiscard]] bool EndpointEquals(const Binding& binding, std::string_view value) const noexcept {
    return binding.endpoint_size == value.size() &&
           (value.empty() || std::memcmp(binding.endpoint.get(), value.data(), value.size()) == 0);
  }

  [[nodiscard]] HostStatus Resolve(const HostChannelHandle& handle,
                                   std::size_t& index) const noexcept {
    index = kInvalidIndex;
    if (!handle.assigned_) return HostStatus::INVALID_BINDING;
    const auto locked = handle.scope_.lock();
    if (!locked) return HostStatus::EXPIRED_HANDLE;
    if (locked != scope) return HostStatus::FOREIGN_HANDLE;
    if (handle.channel_index_ >= channel_count) return HostStatus::INVALID_BINDING;
    const Channel& channel = channels[handle.channel_index_];
    if (handle.generation_ != channel.generation) return HostStatus::STALE_HANDLE;
    index = handle.channel_index_;
    return HostStatus::OK;
  }

  bool InvokeObserver(const HostEndpoint* owner, Channel& channel, ByteView frame,
                      const DecodeResult& decoded, HostCandidateObserver observer,
                      HostOperationResult& result, bool& continue_requested) noexcept {
    continue_requested = true;
    if (observer.function == nullptr) return true;
    HostCandidateView view;
    view.generation = channel.generation;
    view.frame = frame;
    view.decode_status = decoded.status;
    view.record = decoded.record;
    view.matched_message_index = decoded.matched_message_index;
    view.required_field_count = decoded.required_field_count;
    view.failed_field_flat_index = decoded.failed_field_flat_index;
    view.conversion_error = decoded.conversion_error;
    view.output_tainted = decoded.output_tainted;
    try {
      CallbackScope callback_scope(owner);
      continue_requested =
          observer.function(view, observer.context) == HostCallbackAction::CONTINUE;
      ++result.observer_callbacks_returned;
      return true;
    } catch (...) {
      channel.faulted = true;
      result.status = HostStatus::CALLBACK_FAILED;
      result.reset_required = true;
      continue_requested = false;
      return false;
    }
  }

  bool InvokeBusiness(const HostEndpoint* owner, Channel& channel, HostOutputView output,
                      HostOutputSink sink, HostOperationResult& result,
                      bool& continue_requested) noexcept {
    continue_requested = true;
    try {
      CallbackScope callback_scope(owner);
      continue_requested = sink.function(output, sink.context) == HostCallbackAction::CONTINUE;
      ++result.business_callbacks_returned;
      return true;
    } catch (...) {
      channel.faulted = true;
      result.status = HostStatus::CALLBACK_FAILED;
      result.reset_required = true;
      continue_requested = false;
      return false;
    }
  }

  bool DecodeCandidate(const HostEndpoint* owner, Channel& channel, ByteView frame,
                       HostOutputSink sink, HostCandidateObserver observer,
                       HostOperationResult& result) noexcept {
    ++result.candidates;
    ++result.decode_attempts;
    result.codec_attempted = true;
    const Binding& binding = bindings[channel.binding_index];
    const DecodeResult decoded = channel.codec->Decode(binding.pipeline_index, frame);
    result.codec_status = decoded.status;
    if (decoded.status == CodecStatus::OK) {
      ++result.decode_successes;
    } else {
      ++result.decode_failures;
      if (result.status == HostStatus::OK) result.status = HostStatus::CODEC_FAILED;
    }

    bool observer_continue = true;
    if (!InvokeObserver(owner, channel, frame, decoded, observer, result, observer_continue)) {
      return false;
    }
    if (decoded.status != CodecStatus::OK) return observer_continue;

    HostOutputView output;
    output.action = HostAction::DECODE;
    output.generation = channel.generation;
    output.message_index = decoded.record.MessageIndex();
    output.record = decoded.record;
    output.frame = frame;
    bool business_continue = true;
    if (!InvokeBusiness(owner, channel, output, sink, result, business_continue)) return false;
    return observer_continue && business_continue;
  }
};

HostEndpoint::HostEndpoint(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

HostEndpoint::~HostEndpoint() = default;

HostFindResult HostEndpoint::Find(std::string_view endpoint_key, HostAction action,
                                  std::size_t decode_stream_index) const noexcept {
  HostFindResult result;
  if (IsCallbackOwnerActive(this)) {
    result.status = HostStatus::REENTRANT_CALL;
    return result;
  }
  HostLease lease(impl_->in_use);
  if (!lease.Acquired()) {
    result.status = HostStatus::WORKSPACE_BUSY;
    return result;
  }
  if (endpoint_key.empty() || endpoint_key.data() == nullptr ||
      (action != HostAction::DECODE && action != HostAction::ENCODE)) {
    return result;
  }
  result.status = HostStatus::INVALID_BINDING;
  for (std::size_t index = 0U; index < impl_->binding_count; ++index) {
    const Impl::Binding& binding = impl_->bindings[index];
    if (!impl_->EndpointEquals(binding, endpoint_key) || binding.action != action) continue;
    if (decode_stream_index >= binding.channel_count) return result;
    const std::size_t channel_index = binding.first_channel + decode_stream_index;
    const Impl::Channel& channel = impl_->channels[channel_index];
    result.handle.scope_ = impl_->scope;
    result.handle.channel_index_ = channel_index;
    result.handle.generation_ = channel.generation;
    result.handle.assigned_ = true;
    result.status = HostStatus::OK;
    return result;
  }
  return result;
}

HostObservation HostEndpoint::Observe(const HostChannelHandle& handle) const noexcept {
  HostObservation result;
  if (IsCallbackOwnerActive(this)) {
    result.status = HostStatus::REENTRANT_CALL;
    return result;
  }
  HostLease lease(impl_->in_use);
  if (!lease.Acquired()) {
    result.status = HostStatus::WORKSPACE_BUSY;
    return result;
  }
  std::size_t index = kInvalidIndex;
  result.status = impl_->Resolve(handle, index);
  if (result.status != HostStatus::OK) return result;
  const Impl::Channel& channel = impl_->channels[index];
  const Impl::Binding& binding = impl_->bindings[channel.binding_index];
  result.action = binding.action;
  result.generation = channel.generation;
  result.reset_required = channel.faulted;
  result.stream = channel.framer != nullptr;
  if (channel.framer) {
    result.framing = channel.framer->Observe();
    if (result.framing.status != StreamFramerStatus::OK) {
      result.status = HostStatus::FRAMING_FAILED;
    }
  }
  return result;
}

HostStatus HostEndpoint::Reset(const HostChannelHandle& handle) noexcept {
  if (IsCallbackOwnerActive(this)) return HostStatus::REENTRANT_CALL;
  HostLease lease(impl_->in_use);
  if (!lease.Acquired()) return HostStatus::WORKSPACE_BUSY;
  std::size_t index = kInvalidIndex;
  const HostStatus resolved = impl_->Resolve(handle, index);
  if (resolved != HostStatus::OK) return resolved;
  Impl::Channel& channel = impl_->channels[index];
  if (channel.generation == (std::numeric_limits<std::uint64_t>::max)()) {
    return HostStatus::GENERATION_EXHAUSTED;
  }
  if (channel.framer) {
    const StreamFramerStatus reset = channel.framer->Reset();
    if (reset != StreamFramerStatus::OK) return HostStatus::FRAMING_FAILED;
  }
  channel.codec->AdvanceViewEpoch();
  ++channel.generation;
  channel.faulted = false;
  return HostStatus::OK;
}

HostOperationResult HostEndpoint::Decode(const HostChannelHandle& handle, ByteView frame,
                                         HostOutputSink sink,
                                         HostCandidateObserver observer) noexcept {
  HostOperationResult result;
  if (IsCallbackOwnerActive(this)) {
    result.status = HostStatus::REENTRANT_CALL;
    return result;
  }
  HostLease lease(impl_->in_use);
  if (!lease.Acquired()) {
    result.status = HostStatus::WORKSPACE_BUSY;
    return result;
  }
  std::size_t index = kInvalidIndex;
  result.status = impl_->Resolve(handle, index);
  if (result.status != HostStatus::OK) return result;
  Impl::Channel& channel = impl_->channels[index];
  const Impl::Binding& binding = impl_->bindings[channel.binding_index];
  result.generation = channel.generation;
  if (binding.action != HostAction::DECODE) {
    result.status = HostStatus::WRONG_ACTION;
    return result;
  }
  if (channel.framer) {
    result.status = HostStatus::WRONG_INPUT_KIND;
    return result;
  }
  if ((frame.data == nullptr && frame.size != 0U) || sink.function == nullptr) {
    result.status = HostStatus::INVALID_ARGUMENT;
    return result;
  }
  if (channel.faulted) {
    result.status = HostStatus::RESET_REQUIRED;
    result.reset_required = true;
    return result;
  }
  result.status = HostStatus::OK;
  impl_->DecodeCandidate(this, channel, frame, sink, observer, result);
  result.bytes_consumed = frame.size;
  result.reset_required = channel.faulted;
  return result;
}

HostOperationResult HostEndpoint::Push(const HostChannelHandle& handle, ByteView input,
                                       HostOutputSink sink,
                                       HostCandidateObserver observer) noexcept {
  HostOperationResult result;
  if (IsCallbackOwnerActive(this)) {
    result.status = HostStatus::REENTRANT_CALL;
    return result;
  }
  HostLease lease(impl_->in_use);
  if (!lease.Acquired()) {
    result.status = HostStatus::WORKSPACE_BUSY;
    return result;
  }
  std::size_t index = kInvalidIndex;
  result.status = impl_->Resolve(handle, index);
  if (result.status != HostStatus::OK) return result;
  Impl::Channel& channel = impl_->channels[index];
  const Impl::Binding& binding = impl_->bindings[channel.binding_index];
  result.generation = channel.generation;
  if (binding.action != HostAction::DECODE) {
    result.status = HostStatus::WRONG_ACTION;
    return result;
  }
  if (!channel.framer) {
    result.status = HostStatus::WRONG_INPUT_KIND;
    return result;
  }
  if ((input.data == nullptr && input.size != 0U) || sink.function == nullptr) {
    result.status = HostStatus::INVALID_ARGUMENT;
    return result;
  }
  if (channel.faulted) {
    result.status = HostStatus::RESET_REQUIRED;
    result.reset_required = true;
    return result;
  }

  struct Context {
    HostEndpoint* owner;
    Impl* impl;
    Impl::Channel* channel;
    HostOutputSink sink;
    HostCandidateObserver observer;
    HostOperationResult* result;
  };
  Context context{this, impl_.get(), &channel, sink, observer, &result};
  result.status = HostStatus::OK;
  result.framing_attempted = true;
  result.framing = channel.framer->Push(
      input, {[](FrameCandidateView candidate, void* opaque) noexcept {
                auto& context_value = *static_cast<Context*>(opaque);
                return context_value.impl->DecodeCandidate(
                           context_value.owner, *context_value.channel, candidate.bytes,
                           context_value.sink, context_value.observer, *context_value.result)
                           ? FrameSinkAction::CONTINUE
                           : FrameSinkAction::STOP;
              },
              &context});
  result.bytes_consumed = result.framing.bytes_consumed;
  if (result.framing.status != StreamFramerStatus::OK &&
      result.status != HostStatus::CALLBACK_FAILED) {
    result.status = HostStatus::FRAMING_FAILED;
  }
  result.reset_required = channel.faulted;
  return result;
}

HostOperationResult HostEndpoint::Continue(const HostChannelHandle& handle, HostOutputSink sink,
                                           HostCandidateObserver observer) noexcept {
  return Push(handle, ByteView{}, sink, observer);
}

HostOperationResult HostEndpoint::Encode(const HostChannelHandle& handle, std::size_t message_index,
                                         const EncodeValue* values, std::size_t value_count,
                                         HostOutputSink sink) noexcept {
  HostOperationResult result;
  if (IsCallbackOwnerActive(this)) {
    result.status = HostStatus::REENTRANT_CALL;
    return result;
  }
  HostLease lease(impl_->in_use);
  if (!lease.Acquired()) {
    result.status = HostStatus::WORKSPACE_BUSY;
    return result;
  }
  std::size_t index = kInvalidIndex;
  result.status = impl_->Resolve(handle, index);
  if (result.status != HostStatus::OK) return result;
  Impl::Channel& channel = impl_->channels[index];
  const Impl::Binding& binding = impl_->bindings[channel.binding_index];
  result.generation = channel.generation;
  if (binding.action != HostAction::ENCODE) {
    result.status = HostStatus::WRONG_ACTION;
    return result;
  }
  if (channel.faulted) {
    result.status = HostStatus::RESET_REQUIRED;
    result.reset_required = true;
    return result;
  }

  bool message_available = false;
  for (std::size_t ordinal = 0U; ordinal < binding.encode_message_count; ++ordinal) {
    if (binding.encode_message_indices[ordinal] == message_index) {
      message_available = true;
      break;
    }
  }
  if (!message_available) {
    result.status = HostStatus::INVALID_ARGUMENT;
    return result;
  }
  if ((values == nullptr && value_count != 0U) || sink.function == nullptr) {
    result.status = HostStatus::INVALID_ARGUMENT;
    return result;
  }

  result.codec_attempted = true;
  const EncodeResult encoded = channel.codec->Encode(
      binding.pipeline_index, message_index, values, value_count,
      MutableByteBuffer{channel.encode_buffer.get(), channel.encode_capacity});
  result.codec_status = encoded.status;
  if (encoded.status != CodecStatus::OK) {
    result.status = HostStatus::CODEC_FAILED;
    return result;
  }
  result.status = HostStatus::OK;
  result.bytes_produced = encoded.bytes_written;
  HostOutputView output;
  output.action = HostAction::ENCODE;
  output.generation = channel.generation;
  output.message_index = message_index;
  output.bytes = ByteView{channel.encode_buffer.get(), encoded.bytes_written};
  bool continue_requested = true;
  impl_->InvokeBusiness(this, channel, output, sink, result, continue_requested);
  result.reset_required = channel.faulted;
  return result;
}

HostMemoryReport HostEndpoint::MemoryReport() const noexcept { return impl_->memory; }

HostEndpointCreateResult CreateHostEndpoint(const CompiledProtocol& compiled,
                                            const HostBindingSpec* bindings,
                                            std::size_t binding_count,
                                            const HostLimits& limits) noexcept {
  HostEndpointCreateResult result;
  internal::CompiledStateRef state = detail::CompiledProtocolAccess::Acquire(compiled);
  if (!state || state->Artifacts().Plan() == nullptr) {
    result.status = HostStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }
  if (bindings == nullptr || binding_count == 0U) return result;
  if (limits.max_bindings == 0U || limits.max_bindings > kHardMaxBindings ||
      limits.max_channels == 0U || limits.max_channels > kHardMaxChannels ||
      limits.max_identity_bytes == 0U || limits.max_identity_bytes > kHardMaxIdentityBytes ||
      limits.max_accounted_bytes == 0U || limits.max_accounted_bytes > kHardMaxAccountedBytes ||
      binding_count > limits.max_bindings) {
    result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
    return result;
  }

  HostMemoryReport report;
  report.facade_bytes = sizeof(HostEndpoint) + sizeof(HostEndpoint::Impl);
  report.scope_payload_bytes = sizeof(detail::HostScopeToken);
  report.retained_compiled_state_facade_bytes = internal::CompiledState::FacadeBytes();
  if (!MultiplyChecked(binding_count, sizeof(HostEndpoint::Impl::Binding),
                       report.binding_storage_bytes)) {
    result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
    return result;
  }
  std::size_t total_channels = 0U;
  std::size_t encode_channel_count = 0U;
  std::size_t encode_binding_count = 0U;
  for (std::size_t index = 0U; index < binding_count; ++index) {
    const HostBindingSpec& spec = bindings[index];
    result.status = HostStatus::INVALID_BINDING;
    if (spec.endpoint_key.empty() || spec.endpoint_key.data() == nullptr ||
        spec.endpoint_key.size() > limits.max_identity_bytes ||
        (spec.action != HostAction::DECODE && spec.action != HostAction::ENCODE) ||
        spec.pipeline_index >= compiled.PipelineCount()) {
      if (spec.endpoint_key.size() > limits.max_identity_bytes) {
        result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
      }
      return result;
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (bindings[prior].action == spec.action &&
          bindings[prior].endpoint_key == spec.endpoint_key) {
        result.status = HostStatus::DUPLICATE_BINDING;
        return result;
      }
    }
    if (!AddChecked(spec.endpoint_key.size() + 1U, report.identity_storage_bytes)) {
      result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
      return result;
    }

    bool action_supported = false;
    const std::size_t association_count = compiled.Pipeline(spec.pipeline_index)->message_count;
    if (spec.action == HostAction::DECODE) {
      if (spec.decode_stream_count == 0U) return result;
      for (std::size_t association = 0U; association < association_count; ++association) {
        const auto message = compiled.PipelineMessageIndex(spec.pipeline_index, association);
        if (message && compiled.PipelineMessageExecution(spec.pipeline_index, *message)
                           .value_or(MessageExecutionDescription{})
                           .decode_available) {
          action_supported = true;
        }
      }
      if (!AddChecked(spec.decode_stream_count, total_channels) ||
          total_channels > limits.max_channels) {
        result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
        return result;
      }
    } else {
      if (spec.decode_stream_count != 1U || HasFramingOverrides(spec.framing_options)) {
        return result;
      }
      const EncodePipelineCapacity capacity =
          QueryEncodePipelineCapacity(compiled, spec.pipeline_index);
      if (!capacity.valid) return result;
      action_supported = capacity.available;
      std::size_t selector_bytes = 0U;
      if (!MultiplyChecked(capacity.message_count, sizeof(std::size_t), selector_bytes) ||
          !AddChecked(selector_bytes, report.binding_storage_bytes) ||
          !AddChecked(capacity.maximum_output_size, report.encode_buffer_bytes)) {
        result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
        return result;
      }
      if (!AddChecked(1U, total_channels) || total_channels > limits.max_channels) {
        result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
        return result;
      }
      if (action_supported) ++encode_binding_count;
      if (action_supported && capacity.maximum_output_size != 0U) ++encode_channel_count;
    }
    if (!action_supported) return result;

    const auto stream = QueryStreamFramingCapability(compiled, spec.pipeline_index);
    if (stream.status != StreamFramerStatus::OK) return result;
    if (spec.action == HostAction::DECODE) {
      if (!stream.available && HasFramingOverrides(spec.framing_options)) return result;
    }
  }
  if (!MultiplyChecked(total_channels, sizeof(HostEndpoint::Impl::Channel),
                       report.channel_storage_bytes)) {
    result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
    return result;
  }
  report.host_accounted_total_bytes = report.facade_bytes;
  if (!AddChecked(report.scope_payload_bytes, report.host_accounted_total_bytes) ||
      !AddChecked(report.binding_storage_bytes, report.host_accounted_total_bytes) ||
      !AddChecked(report.identity_storage_bytes, report.host_accounted_total_bytes) ||
      !AddChecked(report.channel_storage_bytes, report.host_accounted_total_bytes) ||
      !AddChecked(report.encode_buffer_bytes, report.host_accounted_total_bytes) ||
      report.host_accounted_total_bytes > limits.max_accounted_bytes) {
    result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
    result.memory = report;
    return result;
  }
  report.allocation_count = 5U + binding_count + encode_binding_count + encode_channel_count;

  try {
    auto impl = std::make_unique<HostEndpoint::Impl>();
    impl->state = state;
    impl->scope = std::make_shared<const detail::HostScopeToken>();
    impl->bindings = std::make_unique<HostEndpoint::Impl::Binding[]>(binding_count);
    impl->binding_count = binding_count;
    impl->channels = std::make_unique<HostEndpoint::Impl::Channel[]>(total_channels);
    impl->channel_count = total_channels;

    std::size_t next_channel = 0U;
    for (std::size_t binding_index = 0U; binding_index < binding_count; ++binding_index) {
      const HostBindingSpec& spec = bindings[binding_index];
      HostEndpoint::Impl::Binding& binding = impl->bindings[binding_index];
      binding.endpoint = std::make_unique<char[]>(spec.endpoint_key.size() + 1U);
      std::memcpy(binding.endpoint.get(), spec.endpoint_key.data(), spec.endpoint_key.size());
      binding.endpoint[spec.endpoint_key.size()] = '\0';
      binding.endpoint_size = spec.endpoint_key.size();
      binding.action = spec.action;
      binding.pipeline_index = spec.pipeline_index;
      binding.first_channel = next_channel;
      binding.channel_count = spec.action == HostAction::DECODE ? spec.decode_stream_count : 1U;
      binding.framing_options = spec.framing_options;

      EncodePipelineCapacity encode_capacity;
      if (spec.action == HostAction::ENCODE) {
        encode_capacity = QueryEncodePipelineCapacity(compiled, spec.pipeline_index);
        const std::size_t association_count = compiled.Pipeline(spec.pipeline_index)->message_count;
        binding.encode_message_indices =
            std::make_unique<std::size_t[]>(encode_capacity.message_count);
        for (std::size_t association = 0U; association < association_count; ++association) {
          const auto message = compiled.PipelineMessageIndex(spec.pipeline_index, association);
          const auto execution =
              message ? compiled.PipelineMessageExecution(spec.pipeline_index, *message)
                      : std::nullopt;
          if (execution && execution->encode_available) {
            binding.encode_message_indices[binding.encode_message_count++] = *message;
          }
        }
      }

      const auto stream = QueryStreamFramingCapability(compiled, spec.pipeline_index);
      for (std::size_t ordinal = 0U; ordinal < binding.channel_count; ++ordinal) {
        HostEndpoint::Impl::Channel& channel = impl->channels[next_channel++];
        channel.binding_index = binding_index;
        auto codec = CreateCompleteRecordCodec(compiled, limits.codec_options);
        if (codec.status != CodecStatus::OK || !codec.codec) {
          result.status = MapCodecCreateStatus(codec.status);
          result.memory = report;
          return result;
        }
        if (!AddChecked(codec.memory.codec_accounted_total_bytes, report.codec_accounted_bytes) ||
            !AddChecked(codec.memory.codec_accounted_total_bytes,
                        report.host_accounted_total_bytes) ||
            !AddChecked(codec.memory.allocation_count, report.allocation_count)) {
          result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
          result.memory = report;
          return result;
        }
        channel.codec = std::move(codec.codec);

        if (spec.action == HostAction::DECODE && stream.available) {
          auto framer = CreateStreamFramer(compiled, spec.pipeline_index, spec.framing_options);
          if (framer.status != StreamFramerStatus::OK || !framer.framer) {
            result.status = MapFramerCreateStatus(framer.status);
            result.memory = report;
            return result;
          }
          if (!AddChecked(framer.memory.framer_accounted_total_bytes,
                          report.framer_accounted_bytes) ||
              !AddChecked(framer.memory.framer_accounted_total_bytes,
                          report.host_accounted_total_bytes) ||
              !AddChecked(framer.memory.allocation_count, report.allocation_count)) {
            result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
            result.memory = report;
            return result;
          }
          channel.framer = std::move(framer.framer);
        }
        if (spec.action == HostAction::ENCODE) {
          channel.encode_capacity = encode_capacity.maximum_output_size;
          if (channel.encode_capacity != 0U) {
            channel.encode_buffer = std::make_unique<std::uint8_t[]>(channel.encode_capacity);
          }
        }
        if (report.host_accounted_total_bytes > limits.max_accounted_bytes) {
          result.status = HostStatus::RESOURCE_LIMIT_EXCEEDED;
          result.memory = report;
          return result;
        }
      }
    }
    impl->memory = report;
    result.host.reset(new HostEndpoint(std::move(impl)));
  } catch (const std::bad_alloc&) {
    result.status = HostStatus::ALLOCATION_FAILED;
    result.memory = report;
    return result;
  } catch (...) {
    result.status = HostStatus::INTERNAL_ERROR;
    result.memory = report;
    return result;
  }
  result.status = HostStatus::OK;
  result.memory = report;
  return result;
}

#if defined(PAE_ENABLE_PUBLIC_HOST_TEST_HOOKS)
bool detail::HostEndpointTestAccess::SetGeneration(HostEndpoint& host,
                                                   const HostChannelHandle& handle,
                                                   std::uint64_t generation) noexcept {
  if (!host.impl_ || !handle.assigned_) return false;
  const auto locked = handle.scope_.lock();
  if (!locked || locked != host.impl_->scope ||
      handle.channel_index_ >= host.impl_->channel_count) {
    return false;
  }
  host.impl_->channels[handle.channel_index_].generation = generation;
  return true;
}
#endif

}  // namespace pae
