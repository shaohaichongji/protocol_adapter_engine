#include "host_observer_adapter.h"

#include <algorithm>
#include <stdexcept>

namespace pae::protocol_lab::ascii {
namespace host = host_endpoint;
namespace core = protocol_core;
namespace {
bool Add(std::size_t& value, std::size_t increment) {
  if (increment > (std::numeric_limits<std::size_t>::max)() - value) return false;
  value += increment;
  return true;
}
}  // namespace

std::unique_ptr<HostObserverAdapter> HostObserverAdapter::Create(
    config_compiler::CompiledUiArtifacts artifacts, std::vector<HostBinding> bindings,
    std::string& error, const protocol_framing::FramingLimitOverrides& overrides) {
  error.clear();
  try {
    if (bindings.empty() || bindings.size() > 64U) throw std::runtime_error("binding capacity");
    auto result = std::unique_ptr<HostObserverAdapter>(new HostObserverAdapter);
    if (!artifacts.Plan()) throw std::runtime_error("missing Plan");
    result->accounted_bytes_ = artifacts.Plan()->GetPlanMemoryReport().accounted_total_bytes;
    // Admission reserve, not RSS: sidecar, owned/mapped descriptions, view/result copies and
    // transient editor strings coexist. Multiplications below follow explicit small caps.
    const auto description_bytes = artifacts.DescriptionMemory().accounted_total_bytes;
    if (description_bytes > 4U * 1024U * 1024U ||
        !Add(result->accounted_bytes_, description_bytes * 8U + 8U * 1024U * 1024U))
      throw std::runtime_error("description admission capacity");
    if (!OfflineAdapter::Describe(artifacts, result->description_, error)) return nullptr;
    std::size_t field_count = 0U;
    std::size_t field_bytes = 0U;
    for (const auto& message : result->description_.messages) {
      if (!Add(field_count, message.fields.size()))
        throw std::runtime_error("field count overflow");
      for (const auto& field : message.fields)
        if (!Add(field_bytes, field.max_byte_length))
          throw std::runtime_error("field bytes overflow");
    }
    if (result->description_.max_record_bytes > 65536U || field_count > 1024U ||
        field_bytes > 1048576U)
      throw std::runtime_error("host UI record/field admission capacity");
    const auto per_view = result->description_.max_record_bytes * 32U + field_count * 2048U +
                          field_bytes * 32U + 1048576U;
    if (!Add(result->accounted_bytes_, per_view * bindings.size() * 2U) ||
        result->accounted_bytes_ > kMaximumAccountedBytes)
      throw std::runtime_error("host UI preparation admission capacity");
    std::vector<host::BindingSpec> specs;
    for (const auto& binding : bindings) {
      if (binding.pipeline_index >= result->description_.pipelines.size())
        throw std::runtime_error("unknown Pipeline");
      specs.push_back({binding.endpoint, binding.action,
                       result->description_.pipelines[binding.pipeline_index].id,
                       binding.action == host::Action::DECODE ? 2U : 1U, overrides});
    }
    auto created = host::Session::Create(artifacts.TakePlan(), specs.data(), specs.size());
    if (!created.session)
      throw std::runtime_error("host registration rejected: " +
                               std::to_string(static_cast<int>(created.status)));
    result->session_ = std::move(created.session);
    result->bindings_ = std::move(bindings);
    result->flows_.resize(result->bindings_.size());
    if (!Add(result->accounted_bytes_, result->session_->AccountedBytes()))
      throw std::runtime_error("session accounting overflow");
    for (std::size_t b = 0U; b < result->bindings_.size(); ++b) {
      const auto& binding = result->bindings_[b];
      for (std::size_t s = 0U; s < (binding.action == host::Action::DECODE ? 2U : 1U); ++s) {
        auto& flow = result->flows_[b][s];
        flow.handle = result->session_->Find(binding.endpoint, binding.action, s);
        const auto observed = result->session_->Observe(flow.handle);
        if (observed.status != host::Status::OK) throw std::runtime_error("invalid created handle");
        const auto capacity =
            observed.stream
                ? (std::min)(std::size_t{65536}, observed.framing.effective_max_submit_bytes)
                : 0U;
        flow.frozen.reserve(capacity);
        if (!Add(result->accounted_bytes_, capacity + sizeof(Flow)))
          throw std::runtime_error("adapter accounting overflow");
      }
    }
    if (result->accounted_bytes_ > kMaximumAccountedBytes)
      throw std::runtime_error("host UI active admission capacity");
    return result;
  } catch (const std::exception& failure) {
    error = failure.what();
    return nullptr;
  } catch (...) {
    error = "host adapter preparation failed";
    return nullptr;
  }
}

bool HostObserverAdapter::Matches(std::size_t b, std::size_t s, host::Action action,
                                  const ExecutionIdentity& identity) const noexcept {
  return b < bindings_.size() && s < (action == host::Action::DECODE ? 2U : 1U) &&
         bindings_[b].action == action && bindings_[b].pipeline_index == identity.pipeline_index &&
         identity.pipeline_index < description_.pipelines.size() &&
         identity.pipeline_id == description_.pipelines[identity.pipeline_index].id &&
         (action == host::Action::ENCODE ||
          (!identity.message_id.has_value() && !identity.message_index.has_value()));
}
host::Sink HostObserverAdapter::BusinessSink() noexcept {
  return {[](const host::Output&, void*) { return host::SinkAction::CONTINUE; }, nullptr};
}
ExecutionResult HostObserverAdapter::CopyCandidate(const host::Candidate& candidate,
                                                   ExecutionIdentity identity) const {
  if (candidate.frame.size > copy_limit_) throw std::runtime_error("candidate copy capacity");
  ExecutionResult result;
  result.identity = std::move(identity);
  result.identity.stream_generation = candidate.generation;
  result.core_called = true;
  result.core_status = candidate.decoded.status;
  result.status = candidate.decoded.status == core::CodecStatus::OK ? AdapterStatus::OK
                                                                    : AdapterStatus::CORE_FAILED;
  const auto message = candidate.decoded.message_index;
  if (message < description_.messages.size()) {
    result.message_index = message;
    result.message_id = description_.messages[message].id;
    const auto field = candidate.decoded.failed_field_index;
    if (field < description_.messages[message].fields.size()) {
      result.failed_field_index = field;
      result.failed_field_id = description_.messages[message].fields[field].id;
    }
  }
  auto& copied = result.status == AdapterStatus::OK ? result.frame : result.diagnostic_input_frame;
  if (candidate.frame.size)
    copied.assign(candidate.frame.data, candidate.frame.data + candidate.frame.size);
  if (result.status != AdapterStatus::OK) return result;
  if (!result.message_index) throw std::runtime_error("candidate message identity");
  result.fields.reserve(candidate.field_count);
  for (std::size_t i = 0U; i < candidate.field_count; ++i) {
    const auto& source = candidate.fields[i];
    const auto base = reinterpret_cast<std::uintptr_t>(candidate.frame.data);
    const auto address = reinterpret_cast<std::uintptr_t>(source.bytes_value.data);
    if (source.value_kind != core::LogicalValueKind::BYTES ||
        source.field.plan_scope != candidate.plan || source.field.message_index != message ||
        source.field.field_index >= description_.messages[message].fields.size() ||
        address < base || address - base > candidate.frame.size ||
        source.bytes_value.size > candidate.frame.size - (address - base))
      throw std::runtime_error("candidate field range or identity");
    FieldResult field;
    field.field_index = source.field.field_index;
    field.field_id = description_.messages[message].fields[field.field_index].id;
    field.range = {static_cast<std::size_t>(address - base), source.bytes_value.size};
    if (source.bytes_value.size)
      field.bytes.assign(source.bytes_value.data,
                         source.bytes_value.data + source.bytes_value.size);
    result.fields.push_back(std::move(field));
  }
  return result;
}

ExecutionResult HostObserverAdapter::Inspect(std::size_t b, std::size_t s,
                                             ExecutionIdentity identity,
                                             const std::vector<std::uint8_t>& frame) {
  identity.binding_index = b;
  identity.stream_index = s;
  ExecutionResult result;
  result.identity = identity;
  if (!Matches(b, s, host::Action::DECODE, identity)) return result;
  auto& flow = flows_[b][s];
  if (flow.counters.step_sequence == (std::numeric_limits<std::uint64_t>::max)()) return result;
  identity.operation_sequence = ++flow.counters.step_sequence;
  identity.stream_generation = session_->Observe(flow.handle).generation;
  result.identity = identity;
  struct Context {
    HostObserverAdapter* self;
    ExecutionResult* output;
    ExecutionIdentity identity;
  };
  Context context{this, &result, identity};
  const auto executed =
      session_->Decode(flows_[b][s].handle, {frame.data(), frame.size()}, BusinessSink(),
                       {[](const host::Candidate& candidate, void* data) {
                          auto& c = *static_cast<Context*>(data);
                          *c.output = c.self->CopyCandidate(candidate, c.identity);
                          return host::SinkAction::STOP;
                        },
                        &context});
  if (executed.status == host::Status::CALLBACK_FAILED) {
    result = {};
    result.identity = identity;
    result.status = AdapterStatus::MATERIALIZATION_FAILED;
    result.detail = "candidate copy failed; Reset required";
  } else if (!executed.codec_attempted) {
    result.detail = "host Decode rejected: " + std::to_string(static_cast<int>(executed.status));
  }
  return result;
}

ExecutionResult HostObserverAdapter::Encode(std::size_t b, ExecutionIdentity identity,
                                            const std::vector<InputField>& fields) {
  identity.binding_index = b;
  identity.stream_index = 0U;
  ExecutionResult result;
  result.identity = identity;
  result.operation = Operation::ENCODE;
  result.review_kind = ReviewKind::TX_TEMPLATE;
  if (!Matches(b, 0U, host::Action::ENCODE, identity) || !identity.message_index ||
      !identity.message_id || *identity.message_index >= description_.messages.size())
    return result;
  const auto& message = description_.messages[*identity.message_index];
  if (message.id != *identity.message_id || !message.encode) return result;
  auto& flow = flows_[b][0];
  if (flow.counters.step_sequence == (std::numeric_limits<std::uint64_t>::max)()) return result;
  identity.operation_sequence = ++flow.counters.step_sequence;
  identity.stream_generation = session_->Observe(flow.handle).generation;
  result.identity = identity;
  std::vector<host::NamedValue> named;
  if (fields.size() > message.fields.size()) return result;
  for (const auto& field : fields) {
    if (field.field_index >= message.fields.size() ||
        message.fields[field.field_index].id != field.field_id)
      return result;
    host::NamedValue value;
    value.field_id = field.field_id;
    value.value.value_kind = core::LogicalValueKind::BYTES;
    value.value.bytes_value = {field.bytes.data(), field.bytes.size()};
    named.push_back(value);
  }
  struct Context {
    ExecutionResult* result;
    const MessageDescription* message;
    const std::vector<InputField>* fields;
  };
  Context context{&result, &message, &fields};
  const auto executed = session_->Encode(
      flows_[b][0].handle, message.id, named.data(), named.size(),
      {[](const host::Output& output, void* data) {
         auto& c = *static_cast<Context*>(data);
         c.result->frame.assign(output.bytes.data, output.bytes.data + output.bytes.size);
         std::size_t cursor = 0U;
         for (const auto& segment : c.message->encode->segments) {
           if (segment.kind == SegmentKind::LITERAL) {
             if (!Add(cursor, segment.literal.size()))
               throw std::runtime_error("TX range overflow");
             continue;
           }
           const auto found =
               std::find_if(c.fields->begin(), c.fields->end(), [&](const InputField& field) {
                 return segment.field_index && field.field_index == *segment.field_index;
               });
           if (found == c.fields->end() || cursor > output.bytes.size ||
               found->bytes.size() > output.bytes.size - cursor)
             throw std::runtime_error("TX field range");
           c.result->fields.push_back(
               {found->field_index, found->field_id, found->bytes, {cursor, found->bytes.size()}});
           cursor += found->bytes.size();
         }
         if (cursor != output.bytes.size) throw std::runtime_error("TX total range");
         return host::SinkAction::CONTINUE;
       },
       &context});
  result.core_called = executed.codec_attempted;
  result.core_status = executed.codec_status;
  result.message_index = identity.message_index;
  result.message_id = identity.message_id;
  if (executed.status == host::Status::OK)
    result.status = AdapterStatus::OK;
  else {
    result.frame.clear();
    result.fields.clear();
    result.status = executed.status == host::Status::CALLBACK_FAILED
                        ? AdapterStatus::MATERIALIZATION_FAILED
                        : AdapterStatus::CORE_FAILED;
    result.detail = "host Encode failed: " + std::to_string(static_cast<int>(executed.status));
  }
  return result;
}

std::optional<StreamObservation> HostObserverAdapter::Observe(std::size_t b,
                                                              std::size_t s) const noexcept {
  if (b >= bindings_.size() || bindings_[b].action != host::Action::DECODE || s >= 2U)
    return std::nullopt;
  const auto& flow = flows_[b][s];
  const auto state = session_->Observe(flow.handle);
  if (state.status != host::Status::OK || !state.stream) return std::nullopt;
  auto result = flow.counters;
  result.phase = state.framing.phase;
  result.buffered_bytes = state.framing.buffered_bytes;
  result.has_internal_work = state.framing.has_internal_work;
  result.effective_max_submit_bytes = state.framing.effective_max_submit_bytes;
  result.effective_max_work_units = state.framing.effective_max_work_units;
  result.generation = state.generation;
  result.reset_required = flow.faulted || state.reset_required;
  result.frozen_input_bytes = flow.frozen.size();
  result.frozen_cursor = flow.cursor;
  return result;
}
bool HostObserverAdapter::HasDiscardableState() const noexcept {
  for (std::size_t b = 0U; b < bindings_.size(); ++b) {
    for (std::size_t s = 0U; s < 2U; ++s) {
      const auto state = Observe(b, s);
      if (state && (state->buffered_bytes || state->has_internal_work || state->reset_required ||
                    state->frozen_cursor < state->frozen_input_bytes ||
                    state->phase != protocol_framing::StreamFramingPhase::COLLECTING))
        return true;
    }
  }
  return false;
}
bool HostObserverAdapter::Reset(std::size_t b, std::size_t s) {
  if (b >= bindings_.size() || bindings_[b].action != host::Action::DECODE || s >= 2U ||
      session_->Reset(flows_[b][s].handle) != host::Status::OK)
    return false;
  auto& flow = flows_[b][s];
  flow.frozen.clear();
  flow.cursor = 0U;
  flow.counters = {};
  flow.faulted = false;
  flow.no_progress = 0U;
  return true;
}
StreamStepResult HostObserverAdapter::Submit(std::size_t b, std::size_t s,
                                             ExecutionIdentity identity,
                                             const std::vector<std::uint8_t>& chunk) {
  identity.binding_index = b;
  identity.stream_index = s;
  StreamStepResult result;
  result.identity = identity;
  const auto state = Observe(b, s);
  if (!Matches(b, s, host::Action::DECODE, identity) || !state || state->reset_required ||
      state->has_internal_work || state->frozen_cursor < state->frozen_input_bytes ||
      chunk.empty() ||
      chunk.size() > (std::min)(std::size_t{65536}, state->effective_max_submit_bytes)) {
    result.detail = "invalid Submit or Continue required";
    return result;
  }
  flows_[b][s].frozen = chunk;
  flows_[b][s].cursor = 0U;
  return Continue(b, s, std::move(identity));
}
StreamStepResult HostObserverAdapter::Continue(std::size_t b, std::size_t s,
                                               ExecutionIdentity identity) {
  identity.binding_index = b;
  identity.stream_index = s;
  StreamStepResult result;
  result.identity = identity;
  const auto state = Observe(b, s);
  if (!Matches(b, s, host::Action::DECODE, identity) || !state || state->reset_required ||
      (!state->has_internal_work && state->frozen_cursor == state->frozen_input_bytes)) {
    result.detail = "no continuation or Reset required";
    return result;
  }
  result.before = *state;
  auto& flow = flows_[b][s];
  if (state->step_sequence == (std::numeric_limits<std::uint64_t>::max)()) {
    flow.faulted = true;
    result.status = AdapterStatus::MATERIALIZATION_FAILED;
    result.detail = "operation sequence exhausted; Reset required";
    result.after = *Observe(b, s);
    return result;
  }
  identity.operation_sequence = state->step_sequence + 1U;
  identity.stream_generation = state->generation;
  result.identity = identity;
  struct Context {
    HostObserverAdapter* self;
    StreamStepResult* result;
    ExecutionIdentity identity;
  };
  Context context{this, &result, identity};
  const auto size = flow.frozen.size() - flow.cursor;
  const auto executed = session_->Push(
      flow.handle, {size ? flow.frozen.data() + flow.cursor : nullptr, size}, BusinessSink(),
      {[](const host::Candidate& candidate, void* data) {
         auto& c = *static_cast<Context*>(data);
         c.result->candidate = c.self->CopyCandidate(candidate, c.identity);
         return host::SinkAction::STOP;
       },
       &context});
  result.push_called = executed.framing_attempted;
  result.framing = executed.framing;
  if (executed.framing.bytes_consumed > size || executed.framing.frames_delivered > 1U)
    flow.faulted = true;
  else
    flow.cursor += executed.framing.bytes_consumed;
  if (flow.cursor == flow.frozen.size()) {
    flow.frozen.clear();
    flow.cursor = 0U;
  }
  auto& counts = flow.counters;
  if (counts.step_sequence == (std::numeric_limits<std::uint64_t>::max)() ||
      executed.framing.frames_delivered >
          (std::numeric_limits<std::uint64_t>::max)() - counts.total_candidates ||
      executed.decode_successes >
          (std::numeric_limits<std::uint64_t>::max)() - counts.total_decode_successes ||
      executed.observed_candidates >
          (std::numeric_limits<std::uint64_t>::max)() - counts.total_observed_candidates ||
      executed.successful_outputs >
          (std::numeric_limits<std::uint64_t>::max)() - counts.total_business_outputs ||
      !Add(counts.total_discarded_bytes, executed.framing.bytes_discarded) ||
      !Add(counts.total_malformed_candidates, executed.framing.malformed_candidates))
    flow.faulted = true;
  else {
    ++counts.step_sequence;
    counts.total_candidates += executed.framing.frames_delivered;
    counts.total_decode_successes += executed.decode_successes;
    counts.total_observed_candidates += executed.observed_candidates;
    counts.total_business_outputs += executed.successful_outputs;
  }
  if (executed.status != host::Status::OK && executed.status != host::Status::CODEC_FAILED)
    flow.faulted = true;
  const bool progress = executed.framing.bytes_consumed || executed.framing.frames_delivered ||
                        executed.framing.work_units_used || executed.framing.bytes_discarded;
  flow.no_progress = progress ? 0U : flow.no_progress + 1U;
  if (flow.no_progress >= 2U) flow.faulted = true;
  result.status = flow.faulted ? AdapterStatus::MATERIALIZATION_FAILED : AdapterStatus::OK;
  if (flow.faulted) {
    result.candidate.reset();
    result.detail = "host stream failure; consumption retained; Reset required";
  }
  result.after = *Observe(b, s);
  return result;
}
}  // namespace pae::protocol_lab::ascii
