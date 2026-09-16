#include "public_binary_decode.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pae::protocol_lab_binary::public_decode {
namespace {
std::size_t Add(std::size_t a, std::size_t b) {
  if (b > std::numeric_limits<std::size_t>::max() - a) throw std::length_error("budget overflow");
  return a + b;
}
std::size_t Multiply(std::size_t a, std::size_t b) {
  if (b && a > std::numeric_limits<std::size_t>::max() / b)
    throw std::length_error("budget overflow");
  return a * b;
}
std::size_t DraftPeakReserve(std::size_t max_frame_bytes) {
  // Two hex characters per byte; allow capacity growth, SSO, and the terminator.
  return (std::max)(std::size_t{16U}, Add(Multiply(4U, max_frame_bytes), 1U));
}
void Require(bool value) {
  if (!value) throw std::runtime_error("public Decode observation mismatch");
}
void CopyFrame(std::vector<std::uint8_t>& dst, ByteView src, const Limits& limits) {
  Require(src.size <= limits.max_frame_bytes && (src.data || src.size == 0U));
  if (src.size) dst.assign(src.data, src.data + src.size);
}
void CheckRange(const ByteRange& range, std::size_t size) {
  Require(range.offset <= size && range.length <= size - range.offset);
}
std::size_t Account(const Candidate& value) {
  auto n = Add(sizeof(Candidate), Add(value.frame.capacity(), value.message_id.capacity() + 1U));
  n = Add(n, Multiply(value.fields.capacity(), sizeof(Field)));
  for (const auto& field : value.fields)
    n = Add(n, Add(field.id.capacity() + 1U, field.bytes.capacity()));
  return n;
}
struct Callback {
  const CompiledProtocol& compiled;
  const std::vector<OwnedMessageName>& names;
  const Limits& limits;
  std::optional<Candidate> pending;
};
HostCallbackAction Observe(const HostCandidateView& view, void* context) {
  auto& call = *static_cast<Callback*>(context);
  if (view.decode_status == CodecStatus::OK) return HostCallbackAction::STOP;
  Candidate candidate;
  candidate.codec_status = view.decode_status;
  candidate.failed_field_flat_index = view.failed_field_flat_index;
  candidate.conversion_error = view.conversion_error;
  Require(Add(sizeof(Candidate), view.frame.size) <= call.limits.max_result_bytes);
  CopyFrame(candidate.frame, view.frame, call.limits);
  // A failed-field index does not prove the selected Message identity.
  candidate.accounted_bytes = Account(candidate);
  Require(candidate.accounted_bytes <= call.limits.max_result_bytes);
  call.pending = std::move(candidate);
  return HostCallbackAction::STOP;
}
HostCallbackAction Output(const HostOutputView& view, void* context) {
  auto& call = *static_cast<Callback*>(context);
  Require(view.action == HostAction::DECODE && view.record.HasValue() &&
          view.message_index == view.record.MessageIndex() &&
          view.record.FieldCount() <= call.limits.max_fields);
  const auto message = call.compiled.Message(view.message_index);
  const auto named = std::find_if(call.names.begin(), call.names.end(), [&](const auto& item) {
    return item.index == view.message_index;
  });
  const auto representation = call.compiled.MessageRepresentation(view.message_index);
  const auto physical = call.compiled.ResolveMessagePhysical(view.message_index, view.frame.size);
  Require(message && named != call.names.end() && named->id == message->id &&
          named->field_ids.size() == view.record.FieldCount() &&
          representation.status == PhysicalQueryStatus::OK &&
          representation.value == RecordRepresentation::BINARY &&
          physical.status == PhysicalQueryStatus::OK && physical.value &&
          message->field_count == view.record.FieldCount());

  auto minimum_copy = Add(sizeof(Candidate), view.frame.size);
  minimum_copy = Add(minimum_copy, message->id.size() + 1U);
  minimum_copy = Add(minimum_copy, Multiply(view.record.FieldCount(), sizeof(Field)));
  for (std::size_t i = 0U; i < view.record.FieldCount(); ++i) {
    const auto field_view = view.record.Field(i);
    Require(field_view && field_view->HasValue());
    const auto meta = call.compiled.Field(field_view->FlatFieldIndex());
    Require(meta && meta->message_index == view.message_index);
    minimum_copy = Add(minimum_copy, meta->id.size() + 1U);
    if (field_view->Kind() == ValueKind::BYTES) {
      const auto bytes = field_view->Bytes();
      Require(bytes && bytes->size <= call.limits.max_field_bytes);
      minimum_copy = Add(minimum_copy, bytes->size);
    }
  }
  Require(minimum_copy <= call.limits.max_result_bytes);

  Candidate candidate;
  candidate.success = true;
  candidate.codec_status = CodecStatus::OK;
  candidate.message_index = view.message_index;
  candidate.message_id = named->id;
  CopyFrame(candidate.frame, view.frame, call.limits);
  candidate.integrity_storage = physical.value->integrity_storage;
  candidate.computed_length_storage = physical.value->computed_length_storage;
  if (candidate.integrity_storage) CheckRange(*candidate.integrity_storage, view.frame.size);
  if (candidate.computed_length_storage)
    CheckRange(*candidate.computed_length_storage, view.frame.size);
  candidate.fields.reserve(view.record.FieldCount());
  for (std::size_t i = 0U; i < view.record.FieldCount(); ++i) {
    const auto field_view = view.record.Field(i);
    Require(field_view && field_view->HasValue());
    const auto flat = field_view->FlatFieldIndex();
    const auto meta = call.compiled.Field(flat);
    const auto layout = call.compiled.ResolveFieldPhysical(flat, view.frame.size);
    Require(meta && named->field_ids[i] == meta->id &&
            layout.status == PhysicalQueryStatus::OK && layout.value &&
            meta->message_index == view.message_index && meta->index_in_message == i &&
            layout.value->message_index == view.message_index &&
            layout.value->field_index == i && meta->value_kind == field_view->Kind());
    Field field;
    field.flat_index = flat;
    field.id = named->field_ids[i];
    field.kind = field_view->Kind();
    field.conversion_raw_kind = field_view->ConversionRawKind();
    field.conversion_raw_uint64 = field_view->ConversionRawUInt64();
    field.conversion_raw_int64 = field_view->ConversionRawInt64();
    Require(!field.conversion_raw_kind ||
            (*field.conversion_raw_kind == RawIntegerKind::UINT64
                 ? field.conversion_raw_uint64.has_value() && !field.conversion_raw_int64
                 : field.conversion_raw_int64.has_value() && !field.conversion_raw_uint64));
    switch (field.kind) {
      case ValueKind::UINT64:
        Require(field_view->UInt64().has_value());
        field.uint64_value = *field_view->UInt64();
        break;
      case ValueKind::INT64:
        Require(field_view->Int64().has_value());
        field.int64_value = *field_view->Int64();
        break;
      case ValueKind::BOOL:
        Require(field_view->Bool().has_value());
        field.bool_value = *field_view->Bool();
        break;
      case ValueKind::BYTES: {
        const auto bytes = field_view->Bytes();
        Require(bytes && (bytes->data || bytes->size == 0U) &&
                bytes->size <= call.limits.max_field_bytes);
        if (bytes->size) field.bytes.assign(bytes->data, bytes->data + bytes->size);
        break;
      }
      case ValueKind::ENUM:
        field.enum_raw = field_view->EnumRawValue();
        field.known_enum_flat_index = field_view->KnownEnumFlatIndex();
        Require(field.enum_raw.has_value());
        if (field.known_enum_flat_index) {
          const auto entry = call.compiled.Enum(*field.known_enum_flat_index);
          Require(entry && entry->field_flat_index == flat && entry->raw_value == *field.enum_raw);
        }
        break;
      case ValueKind::DECIMAL64:
        Require(field_view->Decimal().has_value());
        field.decimal = *field_view->Decimal();
        break;
    }
    if (layout.value->physical_kind == FieldPhysicalKind::BYTE_RANGE) {
      Require(layout.value->byte_range.has_value());
      field.byte_range = layout.value->byte_range;
      CheckRange(*field.byte_range, view.frame.size);
      if (field.kind == ValueKind::BYTES) Require(field.byte_range->length == field.bytes.size());
    } else {
      Require(!layout.value->byte_range && layout.value->bit_mask_count > 0U &&
              layout.value->bit_mask_count <= kMaximumFieldPhysicalBitMasks &&
              field.kind != ValueKind::BYTES);
      field.bit_masks = layout.value->bit_masks;
      field.bit_mask_count = layout.value->bit_mask_count;
      for (std::size_t bit = 0U; bit < field.bit_mask_count; ++bit)
        Require(field.bit_masks[bit].mask != 0U &&
                field.bit_masks[bit].frame_byte_index < view.frame.size);
    }
    candidate.fields.push_back(std::move(field));
    Require(Account(candidate) <= call.limits.max_result_bytes);
  }
  candidate.accounted_bytes = Account(candidate);
  Require(candidate.accounted_bytes <= call.limits.max_result_bytes);
  call.pending = std::move(candidate);
  return HostCallbackAction::STOP;
}
}  // namespace

Adapter::~Adapter() = default;

Preparation Adapter::Create(std::string_view json, std::string_view endpoint,
                            std::string_view pipeline_id, const Limits& limits,
                            std::size_t previous_instance_bytes) {
  Preparation result;
  if (json.empty() || endpoint.empty() || endpoint.size() > 256U || pipeline_id.empty()) {
    result.status = LocalStatus::INVALID_INPUT;
    return result;
  }
  try {
    auto compiled = CompileProtocolJson(json);
    if (!compiled.Succeeded()) {
      if (compiled.Diagnostic()) result.diagnostic = *compiled.Diagnostic();
      return result;
    }
    return AdoptCompiled(std::move(compiled).TakeCompiled(),
                         {{std::string(endpoint), std::string(pipeline_id), 2U}}, limits,
                         previous_instance_bytes);
  } catch (const std::exception&) {
    result.status = LocalStatus::RESOURCE_LIMIT;
  }
  return result;
}

Preparation Adapter::AdoptCompiled(CompiledProtocol compiled, std::vector<Binding> bindings,
                                   const Limits& limits, std::size_t previous_instance_bytes) {
  Preparation result;
  if (!compiled.HasValue() || bindings.empty() || bindings.size() > 64U) {
    result.status = LocalStatus::INVALID_INPUT;
    return result;
  }
  try {
    auto out = std::unique_ptr<Adapter>(new Adapter);
    out->compiled_ = std::move(compiled);
    out->bindings_ = std::move(bindings);
    out->limits_ = limits;
    std::size_t channels = 0U;
    std::size_t description = Multiply(out->bindings_.capacity(), sizeof(Binding));
    std::vector<HostBindingSpec> specs;
    specs.reserve(out->bindings_.size());
    out->flow_begin_.reserve(out->bindings_.size());
    for (const auto& binding : out->bindings_) {
      if (binding.endpoint.empty() || binding.endpoint.size() > 256U ||
          binding.pipeline_id.empty() || binding.flow_count == 0U ||
          binding.flow_count > 64U - channels) {
        result.status = LocalStatus::INVALID_BINDING;
        return result;
      }
      description = Add(description, Add(binding.endpoint.capacity() + 1U,
                                         binding.pipeline_id.capacity() + 1U));
      std::optional<std::size_t> pipeline_index;
      for (std::size_t p = 0U; p < out->compiled_.PipelineCount(); ++p) {
        const auto pipeline = out->compiled_.Pipeline(p);
        if (pipeline && pipeline->id == binding.pipeline_id) pipeline_index = p;
      }
      const auto pipeline = pipeline_index ? out->compiled_.Pipeline(*pipeline_index)
                                           : std::nullopt;
      if (!pipeline || pipeline->message_count == 0U) {
        result.status = LocalStatus::INVALID_BINDING;
        return result;
      }
      for (std::size_t a = 0U; a < pipeline->message_count; ++a) {
        const auto index = out->compiled_.PipelineMessageIndex(*pipeline_index, a);
        const auto execution = index ? out->compiled_.PipelineMessageExecution(*pipeline_index, *index)
                                     : std::nullopt;
        const auto representation = index ? out->compiled_.MessageRepresentation(*index)
                                          : MessageRepresentationQueryResult{};
        const auto message = index ? out->compiled_.Message(*index) : std::nullopt;
        if (!execution || !execution->decode_available ||
            representation.status != PhysicalQueryStatus::OK ||
            representation.value != RecordRepresentation::BINARY || !message) {
          result.status = LocalStatus::INVALID_BINDING;
          return result;
        }
        if (message->field_count > limits.max_fields) {
          result.status = LocalStatus::RESOURCE_LIMIT;
          return result;
        }
        if (std::any_of(out->messages_.begin(), out->messages_.end(),
                        [&](const OwnedMessageName& name) { return name.index == *index; }))
          continue;
        OwnedMessageName named;
        named.index = *index;
        named.id = std::string(message->id);
        named.field_ids.reserve(message->field_count);
        for (std::size_t f = 0U; f < message->field_count; ++f) {
          const auto field = out->compiled_.Field(message->field_begin + f);
          if (!field || field->message_index != *index || field->index_in_message != f) {
            result.status = LocalStatus::INVALID_BINDING;
            return result;
          }
          named.field_ids.emplace_back(field->id);
        }
        out->messages_.push_back(std::move(named));
      }
      out->flow_begin_.push_back(channels);
      channels += binding.flow_count;
      specs.push_back({binding.endpoint, HostAction::DECODE, *pipeline_index,
                       binding.flow_count, {}});
    }
    description = Add(description, Multiply(out->flow_begin_.capacity(), sizeof(std::size_t)));
    description = Add(description, Multiply(out->messages_.capacity(), sizeof(OwnedMessageName)));
    for (const auto& named : out->messages_) {
      description = Add(description, named.id.capacity() + 1U);
      description = Add(description, Multiply(named.field_ids.capacity(), sizeof(std::string)));
      for (const auto& id : named.field_ids) description = Add(description, id.capacity() + 1U);
    }
    if (description > limits.max_description_bytes) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      return result;
    }
    HostLimits host_limits;
    host_limits.max_accounted_bytes = std::min(host_limits.max_accounted_bytes, limits.instance_bytes);
    auto created = CreateHostEndpoint(out->compiled_, specs.data(), specs.size(), host_limits);
    result.host_status = created.status;
    if (created.status != HostStatus::OK || !created.host) return result;
    out->host_ = std::move(created.host);
    out->flows_.resize(channels);
    for (std::size_t b = 0U; b < out->bindings_.size(); ++b) {
      for (std::size_t flow = 0U; flow < out->bindings_[b].flow_count; ++flow) {
        const auto found = out->host_->Find(out->bindings_[b].endpoint, HostAction::DECODE, flow);
        if (found.status != HostStatus::OK || out->host_->Observe(found.handle).stream) return result;
        out->flows_[out->flow_begin_[b] + flow].handle = found.handle;
      }
    }
    const auto memory = out->compiled_.MemoryReport();
    auto total = Add(sizeof(Adapter), memory.plan_accounted_bytes);
    total = Add(total, memory.metadata_accounted_bytes);
    total = Add(total, memory.facade_allocation_bytes);
    total = Add(total, created.memory.host_accounted_total_bytes);
    total = Add(total, description);
    total = Add(total, Multiply(out->flows_.capacity(), sizeof(FlowState)));
    total = Add(total, Multiply(specs.capacity(), sizeof(HostBindingSpec)));
    total = Add(total, Multiply(channels + 1U, limits.max_result_bytes));
    total = Add(total, Add(Multiply(channels + 1U, DraftPeakReserve(limits.max_frame_bytes)),
                           sizeof(std::string)));
    out->instance_bytes_ = total;
    if (total > limits.instance_bytes || Add(total, previous_instance_bytes) > limits.replacement_bytes) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      return result;
    }
    result.status = LocalStatus::OK;
    result.adapter = std::move(out);
  } catch (const std::exception&) {
    result.status = LocalStatus::RESOURCE_LIMIT;
  }
  return result;
}

const Operation& Adapter::Decode(std::size_t flow, ByteView frame) {
  rejected_ = {};
  if (flow >= flows_.size() || (frame.data == nullptr && frame.size != 0U) ||
      frame.size > limits_.max_frame_bytes) {
    rejected_.local_status = LocalStatus::INVALID_INPUT;
    return rejected_;
  }
  auto& state = flows_[flow];
  Callback call{compiled_, messages_, limits_, {}};
  Operation next;
  next.host = host_->Decode(state.handle, frame, {Output, &call}, {Observe, &call});
  if (next.host.status == HostStatus::OK && call.pending && call.pending->success) {
    next.candidate = std::move(call.pending);
  } else if (next.host.codec_attempted && next.host.codec_status != CodecStatus::OK &&
             call.pending && next.host.status == HostStatus::CODEC_FAILED) {
    next.candidate = std::move(call.pending);
  } else if (next.host.status == HostStatus::CALLBACK_FAILED) {
    next.local_status = LocalStatus::MATERIALIZATION_FAILED;
  }
  // A Host action or attempted Decode replaces the old success, including callback/copy failure.
  if (next.host.codec_attempted || next.host.status == HostStatus::CALLBACK_FAILED)
    state.current = std::move(next);
  else
    rejected_ = std::move(next);
  return next.host.codec_attempted || next.host.status == HostStatus::CALLBACK_FAILED
             ? state.current : rejected_;
}

HostStatus Adapter::Reset(std::size_t flow) noexcept {
  if (flow >= flows_.size()) return HostStatus::INVALID_ARGUMENT;
  const auto status = host_->Reset(flows_[flow].handle);
  if (status == HostStatus::OK) {
    flows_[flow].draft.clear();
    flows_[flow].current.candidate.reset();
    flows_[flow].current.host = {};
    flows_[flow].current.local_status = LocalStatus::OK;
    std::size_t binding = 0U;
    while (binding + 1U < flow_begin_.size() && flow_begin_[binding + 1U] <= flow) ++binding;
    auto found = host_->Find(bindings_[binding].endpoint, HostAction::DECODE,
                             flow - flow_begin_[binding]);
    if (found.status != HostStatus::OK) return found.status;
    flows_[flow].handle = found.handle;
  }
  return status;
}

bool Adapter::SetDraft(std::size_t flow, std::string_view text) {
  if (flow >= flows_.size()) return false;
  try {
    const auto ceiling = Multiply(2U, limits_.max_frame_bytes);
    if (text.size() > ceiling) return false;
    std::string pending(text);
    if (Add(pending.capacity(), 1U) > DraftPeakReserve(limits_.max_frame_bytes)) return false;
    flows_[flow].draft = std::move(pending);
    return true;
  } catch (...) { return false; }
}

const FlowState* Adapter::State(std::size_t flow) const noexcept {
  return flow < flows_.size() ? &flows_[flow] : nullptr;
}

std::size_t Adapter::FlowCount(std::size_t binding) const noexcept {
  return binding < bindings_.size() ? bindings_[binding].flow_count : 0U;
}

std::size_t Adapter::FlowIndex(std::size_t binding, std::size_t flow) const noexcept {
  return binding < bindings_.size() && flow < bindings_[binding].flow_count
             ? flow_begin_[binding] + flow : flows_.size();
}
}  // namespace pae::protocol_lab_binary::public_decode
