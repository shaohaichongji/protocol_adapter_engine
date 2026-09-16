#include "host_endpoint.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace pae::host_endpoint {
std::size_t Candidate::RawIntegerCount() const noexcept {
  return raw_workspace_ ? raw_workspace_->LastRawIntegerCount() : 0U;
}

bool Candidate::GetRawInteger(std::size_t index,
                             protocol_core::RawIntegerValue& output) const noexcept {
  return raw_workspace_ && raw_workspace_->GetLastRawInteger(index, output);
}

namespace {
namespace core = protocol_core;
namespace framing = protocol_framing;
namespace plan = protocol_plan;
constexpr auto kInvalid = core::kInvalidIndex;

bool Supports(const plan::PlanBundle& bundle, std::size_t message, Action action) noexcept {
  if (bundle.SchemaVersion() != "0.10" && bundle.SchemaVersion() != "0.11") return true;
  const auto& execution = bundle.MessageExecutionPlans()[message];
  return action == Action::DECODE ? execution.text_decode.has_value()
                                  : execution.text_encode.has_value();
}
bool Charge(std::size_t& used, std::size_t count, std::size_t width, std::size_t limit) noexcept {
  if (used > limit || (width != 0U && count > (limit - used) / width)) return false;
  used += count * width;
  return true;
}
struct Lease {
  bool& busy;
  explicit Lease(bool& flag) : busy(flag) { busy = true; }
  ~Lease() { busy = false; }
};
}  // namespace

struct Session::Impl {
  struct Channel {
    // Explicit size constructors can propagate Debug STL proxy allocation failures.
    // Default vector construction may be noexcept in the validated MSVC library.
    Channel() : fields(0U), values(0U), bytes(0U) {}
    std::size_t binding = 0U;
    std::uint64_t generation = 0U;
    bool faulted = false;
    std::unique_ptr<core::ExecutionWorkspace> core;
    std::unique_ptr<framing::StreamFramingWorkspace> framer;
    std::vector<core::DecodedFieldSlot> fields;
    std::vector<core::EncodeFieldValue> values;
    std::vector<std::uint8_t> bytes;
  };
  struct Binding {
    Binding() : endpoint(0U, '\0'), action(Action::DECODE), pipeline(0U), first(0U), count(0U) {}
    Binding(std::string_view name, Action requested_action, std::size_t pipeline_index,
            std::size_t first_channel, std::size_t channel_count)
        : endpoint(name),
          action(requested_action),
          pipeline(pipeline_index),
          first(first_channel),
          count(channel_count) {}
    std::string endpoint;
    Action action;
    std::size_t pipeline;
    std::size_t first;
    std::size_t count;
  };
  // Declaration order keeps Plan alive until all workspaces have been destroyed.
  plan::PlanOwner owner;
  std::shared_ptr<const int> identity = std::make_shared<const int>(0);
  std::vector<Binding> bindings;
  std::vector<Channel> channels;
  std::size_t accounted = sizeof(Impl) + sizeof(Session) + sizeof(int);
  bool busy = false;
  Impl() : bindings(0U), channels(0U) {}

  bool Deliver(Channel& channel, Output output, Sink sink, Result& result) noexcept {
    output.plan = owner.get();
    output.generation = channel.generation;
    try {
      const auto action = sink.function(output, sink.context);
      ++result.successful_outputs;
      return action == SinkAction::CONTINUE;
    } catch (...) {
      channel.faulted = true;
      result.status = Status::CALLBACK_FAILED;
      return false;
    }
  }
  bool DecodeCandidate(Channel& channel, core::ByteView bytes, Sink sink, Result& result,
                       CandidateObserver observer) noexcept {
    const auto decoded =
        core::DecodeCompleteRecord(*owner, *channel.core, bindings[channel.binding].pipeline, bytes,
                                   channel.fields.data(), channel.fields.size());
    result.codec_attempted = true;
    result.codec_status = decoded.status;
    if (decoded.status != core::CodecStatus::OK) {
      ++result.decode_failures;
      result.status = Status::CODEC_FAILED;
    } else {
      ++result.decode_successes;
    }
    bool continue_observing = true;
    if (observer.function) {
      Candidate candidate;
      candidate.generation = channel.generation;
      candidate.plan = owner.get();
      candidate.frame = bytes;
      candidate.decoded = decoded;
      if (decoded.status == core::CodecStatus::OK) {
        candidate.fields = channel.fields.data();
        candidate.field_count = decoded.field_count;
        candidate.raw_workspace_ = channel.core.get();
      }
      try {
        continue_observing = observer.function(candidate, observer.context) == SinkAction::CONTINUE;
        ++result.observed_candidates;
      } catch (...) {
        channel.faulted = true;
        result.status = Status::CALLBACK_FAILED;
        return false;
      }
    }
    if (decoded.status != core::CodecStatus::OK) return continue_observing;
    Output output;
    output.message_index = decoded.message_index;
    output.fields = channel.fields.data();
    output.field_count = decoded.field_count;
    const bool continue_business = Deliver(channel, output, sink, result);
    return continue_business && continue_observing;
  }
};

Session::Session(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}
Session::~Session() = default;
const plan::PlanBundle& Session::Plan() const noexcept { return *impl_->owner; }
std::size_t Session::AccountedBytes() const noexcept { return impl_->accounted; }

CreateResult Session::Create(plan::PlanOwner owner, const BindingSpec* specs, std::size_t count,
                             const Limits& limits) noexcept {
  CreateResult result;
  if (!owner || !specs || count == 0U) return result;
  // This first slice deliberately accepts only the exercised internal generations.
  const auto version = owner->SchemaVersion();
  if (version != "0.9" && version != "0.10" && version != "0.11") {
    result.status = Status::UNSUPPORTED;
    return result;
  }
  result.status = Status::LIMIT_EXCEEDED;
  if (count > limits.max_bindings || limits.max_bindings > 64U || limits.max_channels > 64U ||
      limits.max_identity_bytes > 256U || limits.max_accounted_bytes > 64U * 1024U * 1024U)
    return result;
  try {
    auto impl = std::make_unique<Impl>();
    impl->owner = std::move(owner);
    const auto& bundle = *impl->owner;
    const auto& layout = bundle.GetExecutionResourceLayout();
    std::size_t total = 0U;
    for (std::size_t i = 0U; i < count; ++i) {
      if (specs[i].streams == 0U || total > limits.max_channels ||
          specs[i].streams > limits.max_channels - total)
        return result;
      total += specs[i].streams;
    }
    if (!Charge(impl->accounted, count, sizeof(Impl::Binding), limits.max_accounted_bytes) ||
        !Charge(impl->accounted, total, sizeof(Impl::Channel), limits.max_accounted_bytes))
      return result;
    impl->bindings.reserve(count);
    impl->channels.reserve(total);
    for (std::size_t i = 0U; i < count; ++i) {
      const auto& spec = specs[i];
      result.status = Status::INVALID_BINDING;
      if (spec.endpoint.empty() || spec.pipeline.empty() ||
          (spec.action != Action::DECODE && spec.action != Action::ENCODE) ||
          (spec.action == Action::ENCODE && spec.streams != 1U))
        return result;
      if (spec.endpoint.size() > limits.max_identity_bytes ||
          spec.pipeline.size() > limits.max_identity_bytes) {
        result.status = Status::LIMIT_EXCEEDED;
        return result;
      }
      for (const auto& existing : impl->bindings) {
        if (existing.endpoint == spec.endpoint && existing.action == spec.action) return result;
      }
      std::size_t pipeline = kInvalid;
      for (std::size_t p = 0U; p < bundle.Pipelines().size(); ++p) {
        if (bundle.Pipelines()[p].id.View() == spec.pipeline) pipeline = p;
      }
      if (pipeline == kInvalid) return result;
      bool supported = false;
      for (auto message : bundle.Pipelines()[pipeline].message_indices) {
        supported = supported || Supports(bundle, message, spec.action);
      }
      if (!supported) {
        result.status = Status::UNSUPPORTED;
        return result;
      }
      result.status = Status::LIMIT_EXCEEDED;
      if (!Charge(impl->accounted, spec.endpoint.size() + 1U, 1U, limits.max_accounted_bytes))
        return result;
      impl->bindings.emplace_back(spec.endpoint, spec.action, pipeline, impl->channels.size(),
                                  spec.streams);
      for (std::size_t s = 0U; s < spec.streams; ++s) {
        impl->channels.emplace_back();
        auto& channel = impl->channels.back();
        channel.binding = i;
        if (!Charge(impl->accounted, 1U, sizeof(core::ExecutionWorkspace),
                    limits.max_accounted_bytes) ||
            !Charge(impl->accounted, 1U, layout.estimated_workspace_bytes,
                    limits.max_accounted_bytes))
          return result;
        if (spec.action == Action::DECODE) {
          if (!Charge(impl->accounted, layout.max_fields_per_message,
                      sizeof(core::DecodedFieldSlot), limits.max_accounted_bytes))
            return result;
          channel.fields.resize(layout.max_fields_per_message);
          const auto& profile =
              bundle.FramingProfiles()[bundle.Pipelines()[pipeline].framing_profile_index];
          if (profile.input_kind == plan::InputKind::STREAM_CHUNK) {
            auto overrides = spec.framing_limits;
            const auto remaining = limits.max_accounted_bytes - impl->accounted;
            if (remaining == 0U) return result;
            if (overrides.max_session_memory_bytes > plan::kMaxStreamSessionMemoryBytesHardLimit)
              return result;
            const auto* resource_profile =
                plan::GetResourceProfileLimits(bundle.GetResourceProfile());
            if (!resource_profile) {
              result.status = Status::UNSUPPORTED;
              return result;
            }
            const auto engine_limit = overrides.max_session_memory_bytes == 0U
                                          ? (std::min)(resource_profile->max_session_memory_bytes,
                                                       plan::kMaxStreamSessionMemoryBytesHardLimit)
                                          : overrides.max_session_memory_bytes;
            overrides.max_session_memory_bytes = (std::min)(engine_limit, remaining);
            auto created = framing::CreateStreamFramingWorkspace(bundle, pipeline, overrides);
            if (created.api_status != framing::SubmitApiStatus::OK || !created.workspace) {
              result.status = created.api_status == framing::SubmitApiStatus::LIMIT_EXCEEDED
                                  ? Status::LIMIT_EXCEEDED
                                  : Status::UNSUPPORTED;
              return result;
            }
            if (!Charge(impl->accounted, 1U, created.accounted_workspace_bytes,
                        limits.max_accounted_bytes))
              return result;
            channel.framer = std::move(created.workspace);
          } else if (profile.input_kind != plan::InputKind::COMPLETE_RECORD) {
            result.status = Status::UNSUPPORTED;
            return result;
          }
        } else {
          const auto max_bytes = bundle.GetResourceRequirements().max_frame_bytes;
          if (max_bytes > limits.max_accounted_bytes ||
              !Charge(impl->accounted, static_cast<std::size_t>(max_bytes), 1U,
                      limits.max_accounted_bytes) ||
              !Charge(impl->accounted, layout.max_fields_per_message,
                      sizeof(core::EncodeFieldValue), limits.max_accounted_bytes))
            return result;
          channel.bytes.resize(static_cast<std::size_t>(max_bytes));
          channel.values.resize(layout.max_fields_per_message);
        }
        channel.core = std::make_unique<core::ExecutionWorkspace>(bundle);
      }
    }
    result.session.reset(new Session(std::move(impl)));
    result.status = Status::OK;
  } catch (...) {
    result.status = Status::ALLOCATION_FAILED;
  }
  return result;
}

Handle Session::Find(std::string_view endpoint, Action action, std::size_t stream) const noexcept {
  Handle handle;
  if (impl_->busy) return handle;
  for (const auto& binding : impl_->bindings) {
    if (binding.endpoint == endpoint && binding.action == action && stream < binding.count) {
      handle.scope_ = impl_->identity;
      handle.channel_ = binding.first + stream;
      break;
    }
  }
  return handle;
}
std::size_t Session::Resolve(const Handle& handle) const noexcept {
  return handle.scope_.lock() == impl_->identity && handle.channel_ < impl_->channels.size()
             ? handle.channel_
             : kInvalid;
}
Observation Session::Observe(const Handle& handle) const noexcept {
  Observation result;
  if (impl_->busy) {
    result.status = Status::BUSY;
    return result;
  }
  const auto index = Resolve(handle);
  if (index == kInvalid) return result;
  const auto& channel = impl_->channels[index];
  result.status = Status::OK;
  result.generation = channel.generation;
  result.reset_required = channel.faulted;
  result.stream = channel.framer != nullptr;
  if (channel.framer) result.framing = channel.framer->Observe();
  return result;
}
Status Session::Reset(const Handle& handle) noexcept {
  if (impl_->busy) return Status::BUSY;
  const auto index = Resolve(handle);
  if (index == kInvalid) return Status::INVALID_BINDING;
  auto& channel = impl_->channels[index];
  if (impl_->bindings[channel.binding].action != Action::DECODE) return Status::INVALID_BINDING;
  if (channel.generation == (std::numeric_limits<std::uint64_t>::max)())
    return Status::LIMIT_EXCEEDED;
  Lease lease(impl_->busy);
  if (channel.framer &&
      framing::ResetStreamFramingWorkspace(*impl_->owner, *channel.framer,
                                           impl_->bindings[channel.binding].pipeline) !=
          framing::SubmitApiStatus::OK)
    return Status::FRAMING_FAILED;
  ++channel.generation;
  channel.faulted = false;
  return Status::OK;
}
Result Session::Decode(const Handle& handle, core::ByteView bytes, Sink sink,
                       CandidateObserver observer) noexcept {
  Result result;
  if (impl_->busy) {
    result.status = Status::BUSY;
    return result;
  }
  const auto index = Resolve(handle);
  result.status = Status::INVALID_BINDING;
  if (index == kInvalid) return result;
  auto& channel = impl_->channels[index];
  result.generation = channel.generation;
  if (impl_->bindings[channel.binding].action != Action::DECODE) return result;
  if (channel.framer) {
    result.status = Status::WRONG_INPUT_KIND;
    return result;
  }
  if (channel.faulted) {
    result.status = Status::RESET_REQUIRED;
    return result;
  }
  if (!sink.function || (!bytes.data && bytes.size)) {
    result.status = Status::INVALID_ARGUMENT;
    return result;
  }
  Lease lease(impl_->busy);
  result.status = Status::OK;
  impl_->DecodeCandidate(channel, bytes, sink, result, observer);
  return result;
}
Result Session::Push(const Handle& handle, core::ByteView bytes, Sink sink,
                     CandidateObserver observer) noexcept {
  Result result;
  if (impl_->busy) {
    result.status = Status::BUSY;
    return result;
  }
  const auto index = Resolve(handle);
  result.status = Status::INVALID_BINDING;
  if (index == kInvalid) return result;
  auto& channel = impl_->channels[index];
  result.generation = channel.generation;
  if (impl_->bindings[channel.binding].action != Action::DECODE) return result;
  if (!channel.framer) {
    result.status = Status::WRONG_INPUT_KIND;
    return result;
  }
  if (channel.faulted) {
    result.status = Status::RESET_REQUIRED;
    return result;
  }
  if (!sink.function || (!bytes.data && bytes.size) ||
      (bytes.size == 0U && !channel.framer->Observe().has_internal_work)) {
    result.status = Status::INVALID_ARGUMENT;
    return result;
  }
  Lease lease(impl_->busy);
  struct Context {
    Impl* impl;
    Impl::Channel* channel;
    Sink sink;
    Result* result;
    CandidateObserver observer;
  };
  Context context{impl_.get(), &channel, sink, &result, observer};
  result.status = Status::OK;
  result.framing_attempted = true;
  result.framing = framing::PushStreamChunk(
      *impl_->owner, *channel.framer, impl_->bindings[channel.binding].pipeline,
      {bytes.data, bytes.size},
      {[](framing::ByteView frame, void* opaque) noexcept {
         auto& ctx = *static_cast<Context*>(opaque);
         return ctx.impl->DecodeCandidate(*ctx.channel, {frame.data, frame.size}, ctx.sink,
                                          *ctx.result, ctx.observer)
                    ? framing::FrameSinkAction::CONTINUE
                    : framing::FrameSinkAction::STOP;
       },
       &context});
  if (result.framing.api_status != framing::SubmitApiStatus::OK) {
    channel.faulted = true;
    result.status = Status::FRAMING_FAILED;
  }
  return result;
}
Result Session::Encode(const Handle& handle, std::string_view message_id, const NamedValue* values,
                       std::size_t count, Sink sink) noexcept {
  Result result;
  if (impl_->busy) {
    result.status = Status::BUSY;
    return result;
  }
  const auto index = Resolve(handle);
  result.status = Status::INVALID_BINDING;
  if (index == kInvalid) return result;
  auto& channel = impl_->channels[index];
  const auto& binding = impl_->bindings[channel.binding];
  if (binding.action != Action::ENCODE) return result;
  if (channel.faulted) {
    result.status = Status::RESET_REQUIRED;
    return result;
  }
  const auto& bundle = *impl_->owner;
  std::size_t message = kInvalid;
  for (auto candidate : bundle.Pipelines()[binding.pipeline].message_indices) {
    if (bundle.Messages()[candidate].id.View() == message_id) message = candidate;
  }
  if (message == kInvalid) return result;
  if (!Supports(bundle, message, Action::ENCODE)) {
    result.status = Status::UNSUPPORTED;
    return result;
  }
  if (!sink.function || (count && !values) || count > channel.values.size()) {
    result.status = Status::INVALID_ARGUMENT;
    return result;
  }
  Lease lease(impl_->busy);
  for (std::size_t v = 0U; v < count; ++v) {
    const auto& reference = values[v].value.field;
    if (reference.plan_scope || reference.message_index != kInvalid ||
        reference.field_index != kInvalid)
      return result;
    std::size_t field = kInvalid;
    for (std::size_t f = 0U; f < bundle.Messages()[message].fields.size(); ++f) {
      if (bundle.Messages()[message].fields[f].id.View() == values[v].field_id) field = f;
    }
    if (field == kInvalid) return result;
    channel.values[v] = values[v].value;
    channel.values[v].field = {impl_->owner.get(), message, field};
  }
  const auto encoded = core::EncodeCompleteRecord(bundle, *channel.core, binding.pipeline, message,
                                                  channel.values.data(), count,
                                                  {channel.bytes.data(), channel.bytes.size()});
  result.codec_attempted = true;
  result.codec_status = encoded.status;
  result.status = encoded.status == core::CodecStatus::OK ? Status::OK : Status::CODEC_FAILED;
  if (encoded.status == core::CodecStatus::OK) {
    Output output;
    output.action = Action::ENCODE;
    output.message_index = message;
    output.bytes = {channel.bytes.data(), encoded.bytes_written};
    impl_->Deliver(channel, output, sink, result);
  }
  return result;
}
}  // namespace pae::host_endpoint
