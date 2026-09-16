#include "prepared_binary.h"

#include <algorithm>
#include <atomic>
#include <limits>

#include "binary_admission.h"

namespace pae::protocol_lab_binary {
namespace {
namespace host = host_endpoint;
namespace core = protocol_core;
void Require(bool condition, const char* message) {
  if (!condition) throw MaterializationError(message);
}
std::size_t Add(std::size_t left, std::size_t right) {
  Require(right <= std::numeric_limits<std::size_t>::max() - left, "association budget overflow");
  return left + right;
}
std::size_t Multiply(std::size_t count, std::size_t width) {
  Require(width == 0U || count <= std::numeric_limits<std::size_t>::max() / width,
          "association budget overflow");
  return count * width;
}
std::uint64_t NewInstance() {
  static std::atomic<std::uint64_t> next{1U};
  auto current = next.load();
  for (;;) {
    Require(current != std::numeric_limits<std::uint64_t>::max(), "instance identity exhausted");
    if (next.compare_exchange_weak(current, current + 1U)) return current;
  }
}
}  // namespace

std::unique_ptr<PreparedBinary> PreparedBinary::Create(
    config_compiler::CompiledUiArtifacts artifacts, const host::BindingSpec* bindings,
    std::size_t count, Revisions revisions, const Limits& limits,
    const DescriptionLimits& description_limits, const ResourceLimits& resource_limits,
    const PreparedBinary* previous) {
  Require(artifacts.Plan() != nullptr && bindings != nullptr && count > 0U && count <= 64U,
          "invalid Binary preparation input");
  ValidateBinaryAdmission(*artifacts.Plan(), limits);
  Require(previous == nullptr || previous->revisions_.tab == revisions.tab,
          "replacement budget requires the same Tab");
  const auto old_bytes = previous ? previous->Resources().instance_admission_bytes : 0U;
  // No external state has changed. All displayed messages are scanned, including unbound ones.
  auto out = std::unique_ptr<PreparedBinary>(new PreparedBinary);
  out->description_ = BuildOwnedDescription(artifacts, description_limits);
  out->limits_ = limits;
  out->revisions_ = revisions;
  out->bindings_.reserve(count);
  std::size_t channels = 0U;
  for (std::size_t i = 0U; i < count; ++i) {
    const auto& spec = bindings[i];
    Require(!spec.endpoint.empty() && spec.endpoint.size() <= limits.max_identity_bytes &&
                !spec.pipeline.empty() && spec.pipeline.size() <= limits.max_identity_bytes &&
                (spec.action == host::Action::DECODE || spec.action == host::Action::ENCODE) &&
                spec.streams > 0U && spec.streams <= 64U - channels &&
                (spec.action != host::Action::ENCODE || spec.streams == 1U),
            "invalid Binary binding");
    channels += spec.streams;
    for (const auto& existing : out->bindings_)
      Require(existing.endpoint != spec.endpoint || existing.action != spec.action,
              "duplicate Binary binding");
    const auto& pipelines = out->description_.pipelines;
    const auto pipeline = std::find_if(pipelines.begin(), pipelines.end(),
                                       [&](const auto& p) { return p.id == spec.pipeline; });
    Require(pipeline != pipelines.end() && !pipeline->message_indices.empty(),
            "Binary binding pipeline absent");
    out->bindings_.push_back({std::string(spec.endpoint), spec.action, pipeline->pipeline_index,
                              spec.streams, channels - spec.streams});
  }
  out->flows_.resize(channels);
  ResourceInputs memory;
  memory.owner_bytes = Add(sizeof(PreparedBinary) - sizeof(OwnedDescription),
                           Multiply(out->flows_.capacity(), sizeof(Flow)));
  // Result envelopes are allocated up front; candidate dynamic storage consumes N*B + B.
  // Inline candidate storage is conservatively also covered by its existing DTO accounting.
  memory.owner_bytes = Add(memory.owner_bytes, Multiply(channels + 1U, sizeof(ObservedOperation)));
  memory.plan_bytes = artifacts.Plan()->GetPlanMemoryReport().accounted_total_bytes;
  memory.description_bytes = out->description_.accounted_total_bytes;
  memory.source_sidecar_bytes = artifacts.Description().MemoryReport().accounted_total_bytes;
  memory.binding_bytes = Multiply(out->bindings_.capacity(), sizeof(Binding));
  memory.channels = channels;
  memory.candidate_limit = limits.max_total_bytes;
  for (const auto& binding : out->bindings_) {
    memory.binding_bytes = Add(memory.binding_bytes, Add(binding.endpoint.capacity(), 1U));
    if (binding.action == host::Action::DECODE)
      memory.decode_channels += binding.streams;
    else
      memory.encode_channels += binding.streams;
  }
  // Reject impossible preparations before allocating Host workspaces. The exact Host cost is
  // available only after Create; its own limit is constrained to the remaining admitted budget.
  const auto base = CalculateResourceReport(memory, resource_limits, old_bytes);
  const auto remaining_instance = resource_limits.instance_bytes - base.preparation_peak_bytes;
  const auto remaining_replacement =
      resource_limits.replacement_bytes - base.replacement_peak_bytes;
  host::Limits host_limits;
  host_limits.max_accounted_bytes = std::min(host_limits.max_accounted_bytes,
                                             std::min(remaining_instance, remaining_replacement));
  auto created = host::Session::Create(artifacts.TakePlan(), bindings, count, host_limits);
  Require(created.status == host::Status::OK && created.session != nullptr,
          "Binary Host registration rejected");
  out->session_ = std::move(created.session);
  memory.session_bytes = out->session_->AccountedBytes();
  out->resources_ = CalculateResourceReport(memory, resource_limits, old_bytes);
  out->pending_ = std::make_unique<ObservedOperation>();
  std::size_t draft_bytes = Multiply(4U * 65536U + 1U, sizeof(char16_t));
  Require(draft_bytes <= out->resources_.utf16_draft_reserve, "draft scratch reserve exceeded");
  out->draft_scratch_ = std::make_unique<char16_t[]>(4U * 65536U + 1U);
  // Data allocation consumes the already admitted frozen reserve, not a second resident charge.
  for (std::size_t i = 0U; i < count; ++i) {
    const auto& b = out->bindings_[i];
    for (std::size_t f = 0U; f < b.streams; ++f) {
      const auto observed = out->Observe(i, f);
      auto& state = out->flows_[b.first_flow + f];
      if (observed.stream && b.action == host::Action::DECODE) {
        state.capacity = std::min<std::size_t>(65536U, observed.framing.effective_max_submit_bytes);
        Require(state.capacity > 0U, "invalid stream chunk capacity");
        state.frozen = std::make_unique<std::uint8_t[]>(state.capacity);
      }
      state.draft_limit = 4U * (state.capacity ? state.capacity : 65536U);
      draft_bytes = Add(draft_bytes, Multiply(state.draft_limit + 1U, sizeof(char16_t)));
      Require(draft_bytes <= out->resources_.utf16_draft_reserve, "saved draft reserve exceeded");
      state.draft = std::make_unique<char16_t[]>(state.draft_limit + 1U);
      state.current = std::make_unique<ObservedOperation>();
    }
  }
  out->instance_ = NewInstance();
  return out;
}

bool PreparedBinary::Valid(std::size_t binding, std::size_t flow) const noexcept {
  return binding < bindings_.size() && flow < bindings_[binding].streams;
}
host::Observation PreparedBinary::Observe(std::size_t binding, std::size_t flow) const noexcept {
  if (!Valid(binding, flow)) return {};
  const auto& b = bindings_[binding];
  return session_->Observe(session_->Find(b.endpoint, b.action, flow));
}
host::Status PreparedBinary::Reset(std::size_t binding, std::size_t flow) noexcept {
  if (!Valid(binding, flow)) return host::Status::INVALID_BINDING;
  const auto& b = bindings_[binding];
  const auto status = session_->Reset(session_->Find(b.endpoint, b.action, flow));
  if (status == host::Status::OK) {
    auto& state = flows_[b.first_flow + flow];
    state.size = state.consumed = 0U;
    state.draft_size = 0U;
    state.draft[0] = u'\0';
    state.current->Clear();
    state.has_current = false;
  }
  return status;
}

const ObservedOperation* PreparedBinary::Current(std::size_t binding,
                                                 std::size_t flow) const noexcept {
  if (!Valid(binding, flow)) return nullptr;
  const auto& state = flows_[bindings_[binding].first_flow + flow];
  return state.has_current ? state.current.get() : nullptr;
}

std::u16string_view PreparedBinary::Draft(std::size_t binding, std::size_t flow) const noexcept {
  if (!Valid(binding, flow)) return {};
  const auto& state = flows_[bindings_[binding].first_flow + flow];
  return {state.draft.get(), state.draft_size};
}

std::size_t PreparedBinary::DraftLimit(std::size_t binding, std::size_t flow) const noexcept {
  if (!Valid(binding, flow)) return 0U;
  return flows_[bindings_[binding].first_flow + flow].draft_limit;
}

void PreparedBinary::SaveAndSelect(std::u16string_view draft, std::size_t binding,
                                   std::size_t flow) {
  Require(Valid(binding, flow), "invalid selection destination");
  auto& source = flows_[bindings_[selected_.binding].first_flow + selected_.flow];
  Require(draft.size() <= source.draft_limit && (draft.data() || draft.empty()),
          "Binary draft capacity exceeded");
  // Stage into pre-admitted scratch so even a borrowed/aliased Draft view is safe.
  if (!draft.empty()) std::copy_n(draft.data(), draft.size(), draft_scratch_.get());
  draft_scratch_[draft.size()] = u'\0';
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  Require(!fail_save_, "injected draft save failure before publication");
#endif
  // No allocation or callback after this point. Publish draft and selection together.
  std::copy_n(draft_scratch_.get(), draft.size() + 1U, source.draft.get());
  source.draft_size = draft.size();
  selected_ = {binding, flow};
}

const ObservedOperation& PreparedBinary::Publish(std::size_t binding, std::size_t flow) noexcept {
  if (!Valid(binding, flow)) return *pending_;
  auto& state = flows_[bindings_[binding].first_flow + flow];
  state.current.swap(pending_);
  state.has_current = true;
  // Drop the replaced result; no history and no copy of a candidate at a view switch.
  pending_->Clear();
  return *state.current;
}

const ObservedOperation& PreparedBinary::Submit(std::size_t binding, std::size_t flow,
                                                core::ByteView chunk) {
  pending_->Clear();
  if (!Valid(binding, flow) || bindings_[binding].action != host::Action::DECODE) {
    pending_->host.status = host::Status::INVALID_BINDING;
    return *pending_;
  }
  SubmitOnce(binding, flow, chunk, *pending_);
  return Publish(binding, flow);
}

const ObservedOperation& PreparedBinary::Continue(std::size_t binding, std::size_t flow) {
  pending_->Clear();
  if (!Valid(binding, flow) || bindings_[binding].action != host::Action::DECODE) {
    pending_->host.status = host::Status::INVALID_BINDING;
    return *pending_;
  }
  ContinueOnce(binding, flow, *pending_);
  return Publish(binding, flow);
}

const ObservedOperation& PreparedBinary::Decode(std::size_t binding, std::size_t flow,
                                                core::ByteView frame) {
  pending_->Clear();
  if (!Valid(binding, flow) || bindings_[binding].action != host::Action::DECODE) {
    pending_->host.status = host::Status::INVALID_BINDING;
    return *pending_;
  }
  DecodeOnce(binding, flow, frame, *pending_);
  return Publish(binding, flow);
}

StreamState PreparedBinary::Stream(std::size_t binding, std::size_t flow) const noexcept {
  StreamState out;
  out.host = Observe(binding, flow);
  if (!Valid(binding, flow)) return out;
  const auto& state = flows_[bindings_[binding].first_flow + flow];
  out.capacity = state.capacity;
  out.frozen_size = state.size;
  out.consumed = state.consumed;
  out.can_continue = out.host.status == host::Status::OK && !out.host.reset_required &&
                     out.host.stream &&
                     (state.consumed < state.size || out.host.framing.has_internal_work);
  return out;
}

void PreparedBinary::SubmitOnce(std::size_t binding, std::size_t flow, core::ByteView chunk,
                                ObservedOperation& out) {
  const auto state = Stream(binding, flow);
  out.host.status = state.host.status;
  out.host.generation = state.host.generation;
  if (state.host.status != host::Status::OK) return;
  if (state.host.reset_required)
    out.host.status = host::Status::RESET_REQUIRED;
  else if (!state.host.stream)
    out.host.status = host::Status::WRONG_INPUT_KIND;
  else if (state.can_continue)
    out.host.status = host::Status::BUSY;
  else if (!chunk.data || !chunk.size)
    out.host.status = host::Status::INVALID_ARGUMENT;
  else if (chunk.size > state.capacity)
    out.host.status = host::Status::LIMIT_EXCEEDED;
  else {
    Require(operation_ != std::numeric_limits<std::uint64_t>::max(),
            "operation identity exhausted");
    auto& target = flows_[bindings_[binding].first_flow + flow];
    std::copy_n(chunk.data, chunk.size, target.frozen.get());
    target.size = chunk.size;
    target.consumed = 0U;
    PushFrozen(binding, flow, out);
  }
}

void PreparedBinary::ContinueOnce(std::size_t binding, std::size_t flow, ObservedOperation& out) {
  const auto state = Stream(binding, flow);
  if (state.can_continue) return PushFrozen(binding, flow, out);
  out.host.generation = state.host.generation;
  out.host.status = state.host.status != host::Status::OK ? state.host.status
                    : state.host.reset_required           ? host::Status::RESET_REQUIRED
                    : !state.host.stream                  ? host::Status::WRONG_INPUT_KIND
                                                          : host::Status::INVALID_ARGUMENT;
}

void PreparedBinary::PushFrozen(std::size_t binding, std::size_t flow, ObservedOperation& out) {
  Require(operation_ != std::numeric_limits<std::uint64_t>::max(), "operation identity exhausted");
  const auto& b = bindings_[binding];
  auto& state = flows_[b.first_flow + flow];
  struct Context {
    const PreparedBinary& owner;
    ObservationIdentity identity;
    std::optional<ObservedCandidate>& destination;
  } context{*this,
            {revisions_, instance_, ++operation_, Observe(binding, flow).generation, binding, flow,
             b.pipeline},
            out.candidate};
  const auto remaining = state.size - state.consumed;
  out.host =
      session_->Push(session_->Find(b.endpoint, b.action, flow),
                     {remaining ? state.frozen.get() + state.consumed : nullptr, remaining},
                     {[](const host::Output&, void*) { return host::SinkAction::STOP; }, nullptr},
                     {[](const host::Candidate& candidate, void* pointer) {
                        auto& call = *static_cast<Context*>(pointer);
                        call.destination = call.owner.Associate(candidate, call.identity);
                        return host::SinkAction::STOP;
                      },
                      &context});
  // Even callback failure consumes the current candidate. Never retry those bytes.
  Require(out.host.framing.bytes_consumed <= remaining, "Host over-consumed frozen input");
  state.consumed += out.host.framing.bytes_consumed;
  if (state.consumed == state.size) state.size = state.consumed = 0U;
}

void PreparedBinary::DecodeOnce(std::size_t binding, std::size_t flow, core::ByteView frame,
                                ObservedOperation& out) {
  if (!Valid(binding, flow) || bindings_[binding].action != host::Action::DECODE) {
    out.host.status = host::Status::INVALID_BINDING;
    return;
  }
  const auto observation = Observe(binding, flow);
  out.host.generation = observation.generation;
  if (observation.status != host::Status::OK || observation.reset_required) {
    out.host.status =
        observation.reset_required ? host::Status::RESET_REQUIRED : observation.status;
    return;
  }
  if (observation.stream) {
    out.host.status = host::Status::WRONG_INPUT_KIND;
    return;
  }
  Require(operation_ != std::numeric_limits<std::uint64_t>::max(), "operation identity exhausted");
  const auto& b = bindings_[binding];
  struct Context {
    const PreparedBinary& owner;
    ObservationIdentity identity;
    std::optional<ObservedCandidate>& destination;
  } context{
      *this,
      {revisions_, instance_, ++operation_, observation.generation, binding, flow, b.pipeline},
      out.candidate};
  out.host =
      session_->Decode(session_->Find(b.endpoint, b.action, flow), frame,
                       {[](const host::Output&, void*) { return host::SinkAction::STOP; }, nullptr},
                       {[](const host::Candidate& candidate, void* pointer) {
                          auto& call = *static_cast<Context*>(pointer);
                          call.destination = call.owner.Associate(candidate, call.identity);
                          return host::SinkAction::STOP;
                        },
                        &context});
}

ObservedCandidate PreparedBinary::Associate(const host::Candidate& candidate,
                                            const ObservationIdentity& identity) const {
  Require(candidate.plan == &session_->Plan(), "candidate belongs to a different live Plan");
  Require(Valid(identity.binding, identity.flow) && identity.instance == instance_ &&
              bindings_[identity.binding].action == host::Action::DECODE &&
              bindings_[identity.binding].pipeline == identity.pipeline &&
              candidate.generation == identity.generation,
          "candidate observation context mismatch");
  const auto index = candidate.decoded.message_index;
  if (index != core::kInvalidIndex) {
    const auto& members = description_.pipelines[identity.pipeline].message_indices;
    Require(std::find(members.begin(), members.end(), index) != members.end(),
            "candidate Message is outside the bound Pipeline");
  }
  ObservedCandidate out;
  out.identity = identity;
  out.value = MaterializeCandidate(candidate, limits_);
  std::size_t extra = sizeof(ObservedCandidate) - sizeof(CandidateResult);
  if (candidate.decoded.status == core::CodecStatus::OK) {
    Require(index < description_.messages.size(), "candidate Message absent from description");
    const auto& message = description_.messages[index];
    Require(IsActualFrameValid(message, candidate.frame.size) &&
                out.value.message_id == message.id &&
                out.value.fields.size() == message.fields.size(),
            "candidate description or frame mismatch");
    auto preflight = Add(extra, Multiply(out.value.fields.size(), sizeof(FieldPresentation)));
    auto string_preflight = out.value.budget.string_bytes;
    for (const auto& value : out.value.fields) {
      Require(value.index < message.fields.size() && message.fields[value.index].id == value.id,
              "candidate field description mismatch");
      const auto& field = message.fields[value.index];
      preflight = Add(preflight, Multiply(field.physical_bits.size(), sizeof(PhysicalBitMask)));
      // Every presentation owns an empty string; known enum text uses a conservative allowance.
      std::size_t text_size = 0U;
      if (value.kind == core::LogicalValueKind::ENUM && value.enum_value.known) {
        const auto entry =
            std::find_if(field.enum_entries.begin(), field.enum_entries.end(), [&](const auto& e) {
              return e.id == value.enum_value.item_id && e.raw_value == value.enum_value.raw_value;
            });
        Require(entry != field.enum_entries.end(), "candidate enum description mismatch");
        text_size = entry->display_name.size();
      }
      const auto text_allowance = Add(Multiply(text_size, 2U), 32U);
      preflight = Add(preflight, text_allowance);
      string_preflight = Add(string_preflight, text_allowance);
    }
    Require(
        string_preflight <= limits_.max_string_bytes &&
            Add(out.value.budget.materialization_peak_bytes, preflight) <= limits_.max_total_bytes,
        "associated candidate preflight budget exceeded");
    out.presentation.resize(out.value.fields.size());
    for (std::size_t i = 0U; i < out.value.fields.size(); ++i) {
      const auto& value = out.value.fields[i];
      const auto& field = message.fields[value.index];
      auto& presentation = out.presentation[i];
      presentation.field_index = value.index;
      presentation.byte_range = ResolveActualFieldRange(message, field, candidate.frame.size);
      Require(presentation.byte_range.has_value() || !field.physical_bits.empty(),
              "successful field has no physical mapping");
      if (value.kind == core::LogicalValueKind::BYTES)
        Require(
            presentation.byte_range && presentation.byte_range->length == value.bytes_value.size(),
            "candidate BYTES length differs from physical range");
      presentation.physical_bits = field.physical_bits;
      if (value.kind == core::LogicalValueKind::ENUM && value.enum_value.known)
        for (const auto& entry : field.enum_entries)
          if (entry.id == value.enum_value.item_id && entry.raw_value == value.enum_value.raw_value)
            presentation.enum_display_text = entry.display_name;
    }
    out.integrity_storage = ResolveActualIntegrityStorage(message, candidate.frame.size);
    Require(!message.integrity_storage || out.integrity_storage.has_value(),
            "integrity mapping absent");
  }
  extra = Add(extra, Multiply(out.presentation.capacity(), sizeof(FieldPresentation)));
  std::size_t strings = out.value.budget.string_bytes;
  for (const auto& p : out.presentation) {
    extra = Add(extra, Multiply(p.physical_bits.capacity(), sizeof(PhysicalBitMask)));
    extra = Add(extra, Add(p.enum_display_text.capacity(), 1U));
    strings = Add(strings, Add(p.enum_display_text.capacity(), 1U));
  }
  out.accounted_total_bytes = Add(out.value.budget.accounted_total_bytes, extra);
  out.copy_peak_bytes = Add(out.accounted_total_bytes, out.value.budget.temporary_bytes);
  Require(strings <= limits_.max_string_bytes && out.copy_peak_bytes <= limits_.max_total_bytes,
          "associated candidate capacity budget exceeded");
  return out;
}
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
ObservedCandidate PreparedBinary::TestAssociate(const host::Candidate& candidate,
                                                std::size_t binding, std::size_t flow,
                                                std::uint64_t generation) const {
  Require(Valid(binding, flow), "invalid association probe binding");
  return Associate(candidate, {revisions_, instance_, 0U, generation, binding, flow,
                               bindings_[binding].pipeline});
}
#endif
}  // namespace pae::protocol_lab_binary
