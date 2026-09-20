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
std::size_t Account(const Encoded& value) {
  auto n = Add(sizeof(Encoded), Add(value.frame.capacity(), value.message_id.capacity() + 1U));
  n = Add(n, Multiply(value.inputs.capacity(), sizeof(EncodeInput)));
  for (const auto& input : value.inputs) n = Add(n, input.bytes.capacity());
  n = Add(n, Multiply(value.fields.capacity(), sizeof(EncodedField)));
  for (const auto& field : value.fields) n = Add(n, field.id.capacity() + 1U);
  return n;
}
struct Callback {
  const CompiledProtocol& compiled;
  const std::vector<OwnedMessageName>& names;
  const Limits& limits;
  std::optional<Candidate> pending;
};
struct EncodeCallback {
  const CompiledProtocol& compiled;
  const std::vector<OwnedMessageName>& names;
  const Limits& limits;
  std::size_t message_index = 0U;
  const std::vector<EncodeInput>& inputs;
  std::optional<Encoded> pending;
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
HostCallbackAction EncodeOutput(const HostOutputView& view, void* context) {
  auto& call = *static_cast<EncodeCallback*>(context);
  Require(view.action == HostAction::ENCODE && view.message_index == call.message_index &&
          (view.bytes.data || view.bytes.size == 0U) &&
          view.bytes.size <= call.limits.max_frame_bytes);
  const auto message = call.compiled.Message(view.message_index);
  const auto named = std::find_if(call.names.begin(), call.names.end(), [&](const auto& item) {
    return item.index == view.message_index;
  });
  const auto physical = call.compiled.ResolveMessagePhysical(view.message_index, view.bytes.size);
  Require(message && named != call.names.end() && named->id == message->id &&
          named->field_ids.size() == message->field_count &&
          physical.status == PhysicalQueryStatus::OK && physical.value);
  Encoded encoded;
  encoded.message_index = view.message_index;
  encoded.message_id = named->id;
  encoded.inputs = call.inputs;
  CopyFrame(encoded.frame, view.bytes, call.limits);
  encoded.integrity_storage = physical.value->integrity_storage;
  encoded.computed_length_storage = physical.value->computed_length_storage;
  if (encoded.integrity_storage) CheckRange(*encoded.integrity_storage, view.bytes.size);
  if (encoded.computed_length_storage)
    CheckRange(*encoded.computed_length_storage, view.bytes.size);
  encoded.fields.reserve(message->field_count);
  for (std::size_t f = 0U; f < message->field_count; ++f) {
    const auto meta = call.compiled.Field(message->field_begin + f);
    const auto layout = call.compiled.ResolveFieldPhysical(message->field_begin + f,
                                                           view.bytes.size);
    Require(meta && meta->message_index == view.message_index && meta->index_in_message == f &&
            layout.status == PhysicalQueryStatus::OK && layout.value &&
            layout.value->message_index == view.message_index &&
            layout.value->field_index == f);
    EncodedField field;
    field.field_index = f;
    field.id = named->field_ids[f];
    if (layout.value->physical_kind == FieldPhysicalKind::BYTE_RANGE) {
      Require(layout.value->byte_range.has_value());
      field.byte_range = layout.value->byte_range;
      CheckRange(*field.byte_range, view.bytes.size);
    } else {
      Require(!layout.value->byte_range && layout.value->bit_mask_count > 0U &&
              layout.value->bit_mask_count <= kMaximumFieldPhysicalBitMasks);
      field.bit_masks = layout.value->bit_masks;
      field.bit_mask_count = layout.value->bit_mask_count;
    }
    encoded.fields.push_back(std::move(field));
  }
  encoded.accounted_bytes = Account(encoded);
  Require(encoded.accounted_bytes <= call.limits.max_result_bytes);
  call.pending = std::move(encoded);
  return HostCallbackAction::STOP;
}
}  // namespace

EncodeInput EncodeInput::UInt64(std::size_t field, std::uint64_t value) {
  EncodeInput out; out.field_index = field; out.kind = ValueKind::UINT64;
  out.uint64_value = value; return out;
}
EncodeInput EncodeInput::Int64(std::size_t field, std::int64_t value) {
  EncodeInput out; out.field_index = field; out.kind = ValueKind::INT64;
  out.int64_value = value; return out;
}
EncodeInput EncodeInput::Bool(std::size_t field, bool value) {
  EncodeInput out; out.field_index = field; out.kind = ValueKind::BOOL;
  out.bool_value = value; return out;
}
EncodeInput EncodeInput::Bytes(std::size_t field, std::vector<std::uint8_t> value) {
  EncodeInput out; out.field_index = field; out.kind = ValueKind::BYTES;
  out.bytes = std::move(value); return out;
}
EncodeInput EncodeInput::Enum(std::size_t field, std::size_t entry) {
  EncodeInput out; out.field_index = field; out.kind = ValueKind::ENUM;
  out.enum_entry_index = entry; return out;
}
EncodeInput EncodeInput::Decimal(std::size_t field, Decimal64 value) {
  EncodeInput out; out.field_index = field; out.kind = ValueKind::DECIMAL64;
  out.decimal = value; return out;
}

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
                         {{std::string(endpoint), HostAction::DECODE,
                           std::string(pipeline_id), 2U}}, limits,
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
      std::size_t available_messages = 0U;
      for (std::size_t a = 0U; a < pipeline->message_count; ++a) {
        const auto index = out->compiled_.PipelineMessageIndex(*pipeline_index, a);
        const auto execution = index ? out->compiled_.PipelineMessageExecution(*pipeline_index, *index)
                                     : std::nullopt;
        const auto representation = index ? out->compiled_.MessageRepresentation(*index)
                                          : MessageRepresentationQueryResult{};
        const auto message = index ? out->compiled_.Message(*index) : std::nullopt;
        if (!execution) {
          result.status = LocalStatus::INVALID_BINDING;
          return result;
        }
        const bool available = binding.action == HostAction::DECODE
                                   ? execution->decode_available
                                   : execution->encode_available;
        if (!available) continue;
        ++available_messages;
        if (representation.status != PhysicalQueryStatus::OK ||
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
      if (available_messages == 0U) {
        result.status = LocalStatus::INVALID_BINDING;
        return result;
      }
      out->flow_begin_.push_back(channels);
      channels += binding.flow_count;
      if (binding.action == HostAction::ENCODE && binding.flow_count != 1U) {
        result.status = LocalStatus::INVALID_BINDING;
        return result;
      }
      StreamFramerOptions framing_options;
      if (binding.action == HostAction::DECODE) {
        const auto framing = QueryPipelineFramingDescription(out->compiled_, *pipeline_index);
        if (framing.status != PipelineFramingQueryStatus::OK || !framing.value) {
          result.status = LocalStatus::PREPARATION_FAILED;
          return result;
        }
        if (framing.value->input_kind == PipelineInputKind::STREAM_CHUNK) {
          const bool binary_strategy =
              framing.value->strategy == PipelineFramingStrategy::FIXED_LENGTH ||
              framing.value->strategy == PipelineFramingStrategy::SYNC_FIXED_LENGTH ||
              framing.value->strategy == PipelineFramingStrategy::SYNC_LENGTH_FIELD;
          if (!binary_strategy || !framing.value->maximum_candidate_frame_bytes ||
              *framing.value->maximum_candidate_frame_bytes > limits.max_frame_bytes ||
              Add(sizeof(Candidate), *framing.value->maximum_candidate_frame_bytes) >
                  limits.max_result_bytes ||
              limits.max_stream_chunk_bytes == 0U) {
            result.status = LocalStatus::RESOURCE_LIMIT;
            return result;
          }
          framing_options.max_submit_bytes = limits.max_stream_chunk_bytes;
          framing_options.max_work_units = limits.max_stream_work_units;
        }
      }
      specs.push_back(
          {binding.endpoint, binding.action, *pipeline_index, binding.flow_count, framing_options});
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
        const auto found =
            out->host_->Find(out->bindings_[b].endpoint, out->bindings_[b].action, flow);
        if (found.status != HostStatus::OK) return result;
        auto& state = out->flows_[out->flow_begin_[b] + flow];
        state.handle = found.handle;
        if (out->bindings_[b].action == HostAction::DECODE) {
          const auto observed = out->host_->Observe(found.handle);
          if (observed.status != HostStatus::OK) return result;
          const auto framing =
              QueryPipelineFramingDescription(out->compiled_, specs[b].pipeline_index);
          if (framing.status != PipelineFramingQueryStatus::OK || !framing.value ||
              observed.stream != (framing.value->input_kind == PipelineInputKind::STREAM_CHUNK))
            return result;
          if (observed.stream) {
            state.stream.available = true;
            state.stream.strategy = framing.value->strategy;
            state.stream.maximum_candidate_frame_bytes =
                *framing.value->maximum_candidate_frame_bytes;
            state.stream.capacity = (std::min)(limits.max_stream_chunk_bytes,
                                               observed.framing.effective_max_submit_bytes);
            if (state.stream.capacity == 0U) return result;
          }
        }
      }
    }
    for (auto& flow : out->flows_)
      if (flow.stream.available) flow.stream.frozen_input.reserve(flow.stream.capacity);
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
    for (const auto& flow : out->flows_)
      if (flow.stream.available) total = Add(total, flow.stream.frozen_input.capacity());
    out->instance_bytes_ = total;
    if (total > limits.instance_bytes ||
        Add(total, previous_instance_bytes) > limits.replacement_bytes) {
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
  std::size_t binding = 0U;
  while (binding + 1U < flow_begin_.size() && flow_begin_[binding + 1U] <= flow) ++binding;
  if (bindings_[binding].action != HostAction::DECODE) {
    rejected_.local_status = LocalStatus::INVALID_BINDING;
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

const Operation& Adapter::Encode(std::size_t binding, std::size_t message_index,
                                 const std::vector<EncodeInput>& inputs) {
  rejected_ = {};
  if (binding >= bindings_.size() || bindings_[binding].action != HostAction::ENCODE ||
      flow_begin_[binding] >= flows_.size()) {
    rejected_.local_status = LocalStatus::INVALID_BINDING;
    return rejected_;
  }
  auto& state = flows_[flow_begin_[binding]];
  const auto reject = [&](LocalStatus status) -> const Operation& {
    state.current = {};
    state.current.local_status = status;
    return state.current;
  };
  if (inputs.size() > limits_.max_fields) return reject(LocalStatus::RESOURCE_LIMIT);
  const auto message = compiled_.Message(message_index);
  const auto pipeline = compiled_.PipelineMessageExecution(
      [&] {
        for (std::size_t p = 0U; p < compiled_.PipelineCount(); ++p) {
          const auto item = compiled_.Pipeline(p);
          if (item && item->id == bindings_[binding].pipeline_id) return p;
        }
        return compiled_.PipelineCount();
      }(), message_index);
  if (!message || !pipeline || !pipeline->encode_available) {
    return reject(LocalStatus::INVALID_INPUT);
  }
  std::vector<EncodeValue> values;
  values.reserve(inputs.size());
  std::vector<bool> seen(message->field_count, false);
  try {
    for (const auto& input : inputs) {
      if (input.field_index >= message->field_count || seen[input.field_index]) {
        return reject(LocalStatus::INVALID_INPUT);
      }
      seen[input.field_index] = true;
      const auto flat = message->field_begin + input.field_index;
      const auto field = compiled_.Field(flat);
      if (!field || field->value_kind != input.kind ||
          field->encode_value_source != EncodeValueSource::CALLER_INPUT) {
        return reject(LocalStatus::INVALID_INPUT);
      }
      const FieldSelector selector{message_index, input.field_index};
      switch (input.kind) {
        case ValueKind::UINT64:
          values.push_back(EncodeValue::UInt64(selector, input.uint64_value)); break;
        case ValueKind::INT64:
          values.push_back(EncodeValue::Int64(selector, input.int64_value)); break;
        case ValueKind::BOOL:
          values.push_back(EncodeValue::Bool(selector, input.bool_value)); break;
        case ValueKind::BYTES:
          if (input.bytes.size() > limits_.max_field_bytes) {
            return reject(LocalStatus::RESOURCE_LIMIT);
          }
          values.push_back(EncodeValue::Bytes(
              selector, {input.bytes.empty() ? nullptr : input.bytes.data(), input.bytes.size()}));
          break;
        case ValueKind::ENUM:
          if (input.enum_entry_index >= field->enum_count) {
            return reject(LocalStatus::INVALID_INPUT);
          }
          values.push_back(EncodeValue::Enum(
              {message_index, input.field_index, input.enum_entry_index}));
          break;
        case ValueKind::DECIMAL64:
          values.push_back(EncodeValue::Decimal(selector, input.decimal)); break;
      }
    }
  } catch (const std::exception&) {
    return reject(LocalStatus::RESOURCE_LIMIT);
  }
  EncodeCallback call{compiled_, messages_, limits_, message_index, inputs, {}};
  Operation next;
  next.host = host_->Encode(state.handle, message_index,
                            values.empty() ? nullptr : values.data(), values.size(),
                            {EncodeOutput, &call});
  if (next.host.status == HostStatus::OK && call.pending)
    next.encoded = std::move(call.pending);
  else if (next.host.status == HostStatus::CALLBACK_FAILED)
    next.local_status = LocalStatus::MATERIALIZATION_FAILED;
  state.current = std::move(next);
  return state.current;
}

std::optional<StreamObservation> Adapter::ObserveStream(std::size_t binding,
                                                        std::size_t flow) const noexcept {
  const auto index = FlowIndex(binding, flow);
  if (index >= flows_.size()) return std::nullopt;
  const auto& state = flows_[index];
  if (!state.stream.available) return std::nullopt;
  const auto observed = host_->Observe(state.handle);
  if (observed.status != HostStatus::OK || !observed.stream ||
      observed.framing.status != StreamFramerStatus::OK)
    return std::nullopt;
  StreamObservation result;
  result.strategy = state.stream.strategy;
  result.phase = observed.framing.phase;
  result.maximum_candidate_frame_bytes = state.stream.maximum_candidate_frame_bytes;
  result.effective_max_submit_bytes = observed.framing.effective_max_submit_bytes;
  result.effective_max_work_units = observed.framing.effective_max_work_units;
  result.buffered_bytes = observed.framing.buffered_bytes;
  result.frozen_input_bytes = state.stream.frozen_input.size();
  result.frozen_cursor = state.stream.cursor;
  result.has_internal_work = observed.framing.has_internal_work;
  result.reset_required = state.stream.faulted || observed.reset_required;
  result.generation = observed.generation;
  result.step_sequence = state.stream.step_sequence;
  result.total_candidates = state.stream.total_candidates;
  result.total_decode_successes = state.stream.total_decode_successes;
  result.total_decode_failures = state.stream.total_decode_failures;
  result.total_observer_callbacks = state.stream.total_observer_callbacks;
  result.total_business_callbacks = state.stream.total_business_callbacks;
  result.total_discarded_bytes = state.stream.total_discarded_bytes;
  result.total_malformed_candidates = state.stream.total_malformed_candidates;
  return result;
}

bool Adapter::StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept {
  const auto observed = ObserveStream(binding, flow);
  return observed && !observed->reset_required &&
         (observed->frozen_cursor < observed->frozen_input_bytes || observed->has_internal_work);
}

const StreamStep& Adapter::SubmitStreamChunk(std::size_t binding, std::size_t flow,
                                             const std::vector<std::uint8_t>& chunk) noexcept {
  rejected_stream_ = {};
  const auto before = ObserveStream(binding, flow);
  if (!before) {
    rejected_stream_.local_status = LocalStatus::INVALID_BINDING;
    rejected_stream_.diagnostic = StreamDiagnostic::NOT_STREAM;
    return rejected_stream_;
  }
  rejected_stream_.before = *before;
  rejected_stream_.after = *before;
  if (before->reset_required) {
    rejected_stream_.diagnostic = StreamDiagnostic::RESET_REQUIRED;
    return rejected_stream_;
  }
  if (before->frozen_cursor < before->frozen_input_bytes || before->has_internal_work) {
    rejected_stream_.diagnostic = StreamDiagnostic::CONTINUE_REQUIRED;
    return rejected_stream_;
  }
  const auto index = FlowIndex(binding, flow);
  auto& stream = flows_[index].stream;
  if (chunk.empty() || chunk.size() > stream.capacity) {
    rejected_stream_.local_status = LocalStatus::INVALID_INPUT;
    rejected_stream_.diagnostic = StreamDiagnostic::INVALID_CHUNK;
    return rejected_stream_;
  }
  try {
    stream.frozen_input.assign(chunk.begin(), chunk.end());
    stream.cursor = 0U;
  } catch (...) {
    rejected_stream_.local_status = LocalStatus::RESOURCE_LIMIT;
    rejected_stream_.diagnostic = StreamDiagnostic::INVALID_CHUNK;
    return rejected_stream_;
  }
  return RunStreamStep(binding, flow);
}

const StreamStep& Adapter::ContinueStream(std::size_t binding, std::size_t flow) noexcept {
  rejected_stream_ = {};
  const auto before = ObserveStream(binding, flow);
  if (!before) {
    rejected_stream_.local_status = LocalStatus::INVALID_BINDING;
    rejected_stream_.diagnostic = StreamDiagnostic::NOT_STREAM;
    return rejected_stream_;
  }
  rejected_stream_.before = *before;
  rejected_stream_.after = *before;
  if (before->reset_required) {
    rejected_stream_.diagnostic = StreamDiagnostic::RESET_REQUIRED;
    return rejected_stream_;
  }
  if (before->frozen_cursor == before->frozen_input_bytes && !before->has_internal_work) {
    rejected_stream_.diagnostic = StreamDiagnostic::NO_WORK;
    return rejected_stream_;
  }
  return RunStreamStep(binding, flow);
}

const StreamStep& Adapter::RunStreamStep(std::size_t binding, std::size_t flow) noexcept {
  StreamStep result;
  const auto before = ObserveStream(binding, flow);
  if (!before) {
    rejected_stream_ = {};
    rejected_stream_.local_status = LocalStatus::INVALID_BINDING;
    rejected_stream_.diagnostic = StreamDiagnostic::NOT_STREAM;
    return rejected_stream_;
  }
  result.before = *before;
  result.after = *before;
  const auto index = FlowIndex(binding, flow);
  auto& state = flows_[index];
  auto& stream = state.stream;
  stream.current = {};
  const bool has_suffix = stream.cursor < stream.frozen_input.size();
  const std::size_t submitted = has_suffix ? stream.frozen_input.size() - stream.cursor : 0U;
  Callback call{compiled_, messages_, limits_, {}};
  result.host_called = true;
  result.host = has_suffix ? host_->Push(state.handle,
                                         {stream.frozen_input.data() + stream.cursor, submitted},
                                         {Output, &call}, {Observe, &call})
                           : host_->Continue(state.handle, {Output, &call}, {Observe, &call});
  const bool consumed_valid = result.host.bytes_consumed <= submitted &&
                              result.host.bytes_consumed == result.host.framing.bytes_consumed;
  if (consumed_valid) {
    stream.cursor += result.host.bytes_consumed;
    if (stream.cursor == stream.frozen_input.size()) {
      stream.frozen_input.clear();
      stream.cursor = 0U;
    }
  }
  const bool callbacks_valid =
      result.host.candidates <= 1U &&
      result.host.candidates == result.host.framing.candidates_delivered &&
      result.host.decode_attempts == result.host.candidates &&
      result.host.decode_successes + result.host.decode_failures == result.host.candidates &&
      result.host.observer_callbacks_returned == result.host.candidates &&
      result.host.business_callbacks_returned == result.host.decode_successes &&
      ((result.host.candidates == 0U && !call.pending) ||
       (result.host.candidates == 1U && call.pending.has_value()));
  const auto fits_u64 = [](std::uint64_t target, std::size_t value) noexcept {
    return value <= (std::numeric_limits<std::uint64_t>::max)() - target;
  };
  const auto fits_size = [](std::size_t target, std::size_t value) noexcept {
    return value <= (std::numeric_limits<std::size_t>::max)() - target;
  };
  const bool counters_valid =
      fits_u64(stream.step_sequence, 1U) &&
      fits_u64(stream.total_candidates, result.host.candidates) &&
      fits_u64(stream.total_decode_successes, result.host.decode_successes) &&
      fits_u64(stream.total_decode_failures, result.host.decode_failures) &&
      fits_u64(stream.total_observer_callbacks, result.host.observer_callbacks_returned) &&
      fits_u64(stream.total_business_callbacks, result.host.business_callbacks_returned) &&
      fits_size(stream.total_discarded_bytes, result.host.framing.bytes_discarded) &&
      fits_size(stream.total_malformed_candidates, result.host.framing.malformed_candidates);
  if (counters_valid) {
    ++stream.step_sequence;
    stream.total_candidates += result.host.candidates;
    stream.total_decode_successes += result.host.decode_successes;
    stream.total_decode_failures += result.host.decode_failures;
    stream.total_observer_callbacks += result.host.observer_callbacks_returned;
    stream.total_business_callbacks += result.host.business_callbacks_returned;
    stream.total_discarded_bytes += result.host.framing.bytes_discarded;
    stream.total_malformed_candidates += result.host.framing.malformed_candidates;
  }
  const bool accepted_host =
      result.host.status == HostStatus::OK || result.host.status == HostStatus::CODEC_FAILED;
  if (call.pending) result.candidate = std::move(call.pending);
  if (!consumed_valid || !callbacks_valid || !counters_valid || !accepted_host ||
      result.host.reset_required) {
    stream.faulted = true;
  }
  if (result.host.status == HostStatus::CALLBACK_FAILED) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    result.diagnostic = StreamDiagnostic::COPY_FAILED_RESET_REQUIRED;
    result.candidate.reset();
  } else if (stream.faulted) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    result.diagnostic = StreamDiagnostic::CONTRACT_VIOLATION_RESET_REQUIRED;
    result.candidate.reset();
  }
  const auto after = ObserveStream(binding, flow);
  if (after) result.after = *after;
  stream.current = std::move(result);
  return stream.current;
}

void Adapter::ClearCurrent(std::size_t flow) noexcept {
  if (flow < flows_.size()) {
    flows_[flow].current = {};
    flows_[flow].stream.current = {};
  }
}

HostStatus Adapter::Reset(std::size_t flow) noexcept {
  if (flow >= flows_.size()) return HostStatus::INVALID_ARGUMENT;
  const auto status = host_->Reset(flows_[flow].handle);
  if (status == HostStatus::OK) {
    std::size_t binding = 0U;
    while (binding + 1U < flow_begin_.size() && flow_begin_[binding + 1U] <= flow) ++binding;
    auto found = host_->Find(bindings_[binding].endpoint, bindings_[binding].action,
                             flow - flow_begin_[binding]);
    if (found.status != HostStatus::OK) {
      flows_[flow].stream.faulted = true;
      return found.status;
    }
    flows_[flow].handle = found.handle;
    flows_[flow].draft.clear();
    if (flows_[flow].stream.available) {
      flows_[flow].current = {};
      const auto observed = host_->Observe(found.handle);
      if (observed.status != HostStatus::OK || !observed.stream) {
        flows_[flow].stream.faulted = true;
        return observed.status;
      }
      const auto available = flows_[flow].stream.available;
      const auto strategy = flows_[flow].stream.strategy;
      const auto maximum = flows_[flow].stream.maximum_candidate_frame_bytes;
      const auto capacity = flows_[flow].stream.capacity;
      auto frozen = std::move(flows_[flow].stream.frozen_input);
      frozen.clear();
      flows_[flow].stream = {};
      flows_[flow].stream.available = available;
      flows_[flow].stream.strategy = strategy;
      flows_[flow].stream.maximum_candidate_frame_bytes = maximum;
      flows_[flow].stream.capacity = capacity;
      flows_[flow].stream.frozen_input = std::move(frozen);
    } else {
      flows_[flow].current.candidate.reset();
      flows_[flow].current.host = {};
      flows_[flow].current.local_status = LocalStatus::OK;
    }
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
