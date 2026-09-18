#include "public_ascii_offline_adapter.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>

namespace pae::protocol_lab_ascii::public_offline {
namespace {

constexpr std::size_t kMax = (std::numeric_limits<std::size_t>::max)();

std::size_t Add(std::size_t left, std::size_t right) noexcept {
  return left > kMax - right ? kMax : left + right;
}

std::size_t Multiply(std::size_t left, std::size_t right) noexcept {
  return left != 0U && right > kMax / left ? kMax : left * right;
}

std::size_t StringBytes(std::string_view value) noexcept { return Add(value.size(), 1U); }

template <typename T>
void AddVectorStorage(std::size_t count, std::size_t& total) noexcept {
  total = Add(total, Multiply(count, sizeof(T)));
}

void AddDescriptionStrings(const ProtocolDescription& source, std::size_t& total) noexcept {
  total = Add(total, StringBytes(source.schema_version));
  total = Add(total, StringBytes(source.id));
  total = Add(total, StringBytes(source.version));
  total = Add(total, StringBytes(source.display_name));
  total = Add(total, StringBytes(source.description));
  total = Add(total, StringBytes(source.source_ref));
}

void AddDescriptionStrings(const PipelineDescription& source, std::size_t& total) noexcept {
  total = Add(total, StringBytes(source.id));
  total = Add(total, StringBytes(source.direction_id));
  total = Add(total, StringBytes(source.display_name));
  total = Add(total, StringBytes(source.description));
  total = Add(total, StringBytes(source.source_ref));
}

void AddDescriptionStrings(const MessageDescription& source, std::size_t& total) noexcept {
  total = Add(total, StringBytes(source.id));
  total = Add(total, StringBytes(source.direction_id));
  total = Add(total, StringBytes(source.display_name));
  total = Add(total, StringBytes(source.description));
  total = Add(total, StringBytes(source.source_ref));
}

void AddDescriptionStrings(const FieldDescription& source, std::size_t& total) noexcept {
  total = Add(total, StringBytes(source.id));
  total = Add(total, StringBytes(source.display_name));
  total = Add(total, StringBytes(source.description));
  total = Add(total, StringBytes(source.source_ref));
}

bool PreflightAction(const CompiledProtocol& compiled, std::size_t message_index,
                     AsciiAction action, std::size_t& total) noexcept {
  const auto queried = compiled.AsciiAction(message_index, action);
  if (queried.status == AsciiQueryStatus::ACTION_NOT_AVAILABLE) return true;
  if (queried.status != AsciiQueryStatus::OK || !queried.value) return false;
  total = Add(total, sizeof(OwnedAction));
  AddVectorStorage<OwnedSegment>(queried.value->segment_count, total);
  for (std::size_t ordinal = 0U; ordinal < queried.value->segment_count; ++ordinal) {
    const auto segment = compiled.AsciiSegment(message_index, action, ordinal);
    if (segment.status != AsciiQueryStatus::OK || !segment.value) return false;
    if (segment.value->kind == AsciiSegmentKind::LITERAL) {
      if (!segment.value->literal ||
          (segment.value->literal->data == nullptr && segment.value->literal->size != 0U)) {
        return false;
      }
      total = Add(total, segment.value->literal->size);
    } else if (!segment.value->field_index || !segment.value->flat_field_index) {
      return false;
    }
  }
  return true;
}

bool PreflightDescription(const CompiledProtocol& compiled, const Limits& limits,
                          std::size_t& total) noexcept {
  const auto protocol = compiled.Protocol();
  if (!protocol) return false;
  total = sizeof(OwnedDescription);
  AddDescriptionStrings(*protocol, total);
  AddVectorStorage<OwnedPipelineDescription>(compiled.PipelineCount(), total);
  AddVectorStorage<OwnedMessageDescription>(compiled.MessageCount(), total);
  if (compiled.FieldCount() > limits.max_fields) return false;

  for (std::size_t pipeline_index = 0U; pipeline_index < compiled.PipelineCount();
       ++pipeline_index) {
    const auto pipeline = compiled.Pipeline(pipeline_index);
    if (!pipeline) return false;
    AddDescriptionStrings(*pipeline, total);
    AddVectorStorage<std::size_t>(pipeline->message_count, total);
    for (std::size_t association = 0U; association < pipeline->message_count; ++association) {
      const auto message = compiled.PipelineMessageIndex(pipeline_index, association);
      if (!message) return false;
      const auto execution = compiled.PipelineMessageExecution(pipeline_index, *message);
      if (!execution) return false;
      if (execution->decode_available) total = Add(total, sizeof(std::size_t));
      if (execution->encode_available) total = Add(total, sizeof(std::size_t));
    }
  }
  for (std::size_t message_index = 0U; message_index < compiled.MessageCount(); ++message_index) {
    const auto message = compiled.Message(message_index);
    const auto representation = compiled.MessageRepresentation(message_index);
    if (!message || representation.status != PhysicalQueryStatus::OK ||
        representation.value != RecordRepresentation::ASCII_TEXT) {
      return false;
    }
    AddDescriptionStrings(*message, total);
    AddVectorStorage<OwnedFieldDescription>(message->field_count, total);
    if (!PreflightAction(compiled, message_index, AsciiAction::DECODE, total) ||
        !PreflightAction(compiled, message_index, AsciiAction::ENCODE, total)) {
      return false;
    }
    for (std::size_t field = 0U; field < message->field_count; ++field) {
      const std::size_t flat = Add(message->field_begin, field);
      const auto authored = compiled.Field(flat);
      const auto ascii = compiled.AsciiField(flat);
      if (!authored || authored->value_kind != ValueKind::BYTES ||
          ascii.status != AsciiQueryStatus::OK || !ascii.value ||
          ascii.value->message_index != message_index || ascii.value->field_index != field ||
          ascii.value->byte_length.maximum > limits.max_field_bytes) {
        return false;
      }
      AddDescriptionStrings(*authored, total);
    }
  }
  return total <= limits.max_description_bytes;
}

OwnedAction CopyAction(const CompiledProtocol& compiled, std::size_t message_index,
                       AsciiAction action, const AsciiActionDescription& source) {
  OwnedAction result;
  result.record_length = source.record_length;
  result.segments.reserve(source.segment_count);
  for (std::size_t ordinal = 0U; ordinal < source.segment_count; ++ordinal) {
    const auto queried = compiled.AsciiSegment(message_index, action, ordinal);
    if (queried.status != AsciiQueryStatus::OK || !queried.value) throw std::bad_alloc{};
    OwnedSegment segment;
    segment.kind = queried.value->kind;
    segment.field_index = queried.value->field_index;
    segment.flat_field_index = queried.value->flat_field_index;
    if (queried.value->literal && queried.value->literal->size != 0U) {
      segment.literal.assign(queried.value->literal->data,
                             queried.value->literal->data + queried.value->literal->size);
    }
    result.segments.push_back(std::move(segment));
  }
  return result;
}

bool CopyDescription(const CompiledProtocol& compiled, OwnedDescription& output) {
  const auto protocol = compiled.Protocol();
  if (!protocol) return false;
  output.schema_version.assign(protocol->schema_version);
  output.protocol_id.assign(protocol->id);
  output.protocol_version.assign(protocol->version);
  output.display_name.assign(protocol->display_name);
  output.description.assign(protocol->description);
  output.source_ref.assign(protocol->source_ref);
  output.pipelines.reserve(compiled.PipelineCount());
  for (std::size_t index = 0U; index < compiled.PipelineCount(); ++index) {
    const auto source = compiled.Pipeline(index);
    if (!source) return false;
    OwnedPipelineDescription pipeline;
    pipeline.index = index;
    pipeline.id.assign(source->id);
    pipeline.direction_id.assign(source->direction_id);
    pipeline.display_name.assign(source->display_name);
    pipeline.description.assign(source->description);
    pipeline.source_ref.assign(source->source_ref);
    std::size_t decode_message_count = 0U;
    std::size_t encode_message_count = 0U;
    for (std::size_t association = 0U; association < source->message_count; ++association) {
      const auto message = compiled.PipelineMessageIndex(index, association);
      if (!message) return false;
      const auto execution = compiled.PipelineMessageExecution(index, *message);
      if (!execution) return false;
      if (execution->decode_available) ++decode_message_count;
      if (execution->encode_available) ++encode_message_count;
    }
    pipeline.message_indices.reserve(source->message_count);
    pipeline.decode_message_indices.reserve(decode_message_count);
    pipeline.encode_message_indices.reserve(encode_message_count);
    for (std::size_t association = 0U; association < source->message_count; ++association) {
      const auto message = compiled.PipelineMessageIndex(index, association);
      if (!message) return false;
      pipeline.message_indices.push_back(*message);
      const auto execution = compiled.PipelineMessageExecution(index, *message);
      if (!execution) return false;
      if (execution->decode_available) pipeline.decode_message_indices.push_back(*message);
      if (execution->encode_available) pipeline.encode_message_indices.push_back(*message);
    }
    output.pipelines.push_back(std::move(pipeline));
  }
  output.messages.reserve(compiled.MessageCount());
  for (std::size_t index = 0U; index < compiled.MessageCount(); ++index) {
    const auto source = compiled.Message(index);
    if (!source) return false;
    OwnedMessageDescription message;
    message.index = index;
    message.id.assign(source->id);
    message.direction_id.assign(source->direction_id);
    message.display_name.assign(source->display_name);
    message.description.assign(source->description);
    message.source_ref.assign(source->source_ref);
    for (const auto action : {AsciiAction::DECODE, AsciiAction::ENCODE}) {
      const auto queried = compiled.AsciiAction(index, action);
      if (queried.status == AsciiQueryStatus::OK && queried.value) {
        auto copied = CopyAction(compiled, index, action, *queried.value);
        if (action == AsciiAction::DECODE)
          message.decode = std::move(copied);
        else
          message.encode = std::move(copied);
      } else if (queried.status != AsciiQueryStatus::ACTION_NOT_AVAILABLE) {
        return false;
      }
    }
    message.fields.reserve(source->field_count);
    for (std::size_t field_index = 0U; field_index < source->field_count; ++field_index) {
      const auto flat = source->field_begin + field_index;
      const auto authored = compiled.Field(flat);
      const auto ascii = compiled.AsciiField(flat);
      if (!authored || ascii.status != AsciiQueryStatus::OK || !ascii.value) return false;
      OwnedFieldDescription field;
      field.flat_index = flat;
      field.field_index = field_index;
      field.id.assign(authored->id);
      field.display_name.assign(authored->display_name);
      field.description.assign(authored->description);
      field.source_ref.assign(authored->source_ref);
      field.byte_length = ascii.value->byte_length;
      field.decode_referenced = ascii.value->decode_referenced;
      field.encode_referenced = ascii.value->encode_referenced;
      message.fields.push_back(std::move(field));
    }
    output.messages.push_back(std::move(message));
  }
  return true;
}

bool CheckedRange(ByteView frame, ByteView field, ByteRange& range) noexcept {
  if (frame.data == nullptr || field.data == nullptr) return false;
  const auto begin = reinterpret_cast<std::uintptr_t>(frame.data);
  const auto pointer = reinterpret_cast<std::uintptr_t>(field.data);
  if (frame.size > (std::numeric_limits<std::uintptr_t>::max)() - begin) return false;
  const auto end = begin + frame.size;
  if (pointer < begin || pointer > end || field.size > end - pointer) return false;
  range.offset = static_cast<std::size_t>(pointer - begin);
  range.length = field.size;
  return true;
}

const OwnedFieldDescription* FindField(const OwnedMessageDescription& message,
                                       std::size_t field_index) noexcept {
  return field_index < message.fields.size() ? &message.fields[field_index] : nullptr;
}

const InputField* FindInput(const std::vector<InputField>& inputs,
                            std::size_t field_index) noexcept {
  const auto found = std::find_if(
      inputs.begin(), inputs.end(),
      [field_index](const InputField& value) { return value.field_index == field_index; });
  return found == inputs.end() ? nullptr : &*found;
}

std::size_t BaseResultBytes(const OperationResult& result) noexcept {
  auto total = sizeof(OperationResult);
  total = Add(total, result.frame.size());
  total = Add(total, result.diagnostic_input_frame.size());
  if (result.message_id) total = Add(total, StringBytes(*result.message_id));
  AddVectorStorage<OwnedFieldValue>(result.fields.size(), total);
  for (const auto& field : result.fields) {
    total = Add(total, StringBytes(field.id));
    total = Add(total, field.bytes.size());
  }
  return total;
}

}  // namespace

Adapter::Adapter(CompiledProtocol compiled, std::unique_ptr<CompleteRecordCodec> codec,
                 OwnedDescription description, const Limits& limits,
                 std::size_t description_accounted_bytes, std::size_t instance_bytes) noexcept
    : compiled_(std::move(compiled)),
      codec_(std::move(codec)),
      description_(std::move(description)),
      limits_(limits),
      description_accounted_bytes_(description_accounted_bytes),
      instance_bytes_(instance_bytes) {}

Adapter::~Adapter() = default;

#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
std::optional<Adapter::PipelineCapacityObservation> Adapter::PipelineCapacitiesForTesting(
    std::size_t pipeline_index) const noexcept {
  if (pipeline_index >= description_.pipelines.size()) return std::nullopt;
  const auto& pipeline = description_.pipelines[pipeline_index];
  return PipelineCapacityObservation{pipeline.message_indices.capacity(),
                                     pipeline.decode_message_indices.capacity(),
                                     pipeline.encode_message_indices.capacity()};
}
#endif

PrepareResult Adapter::AdoptCompiled(CompiledProtocol compiled, const Limits& limits,
                                     std::size_t previous_instance_bytes) noexcept {
  PrepareResult result;
  try {
    std::size_t description_bytes = 0U;
    if (!compiled.HasValue() || !PreflightDescription(compiled, limits, description_bytes)) {
      result.status = description_bytes > limits.max_description_bytes
                          ? LocalStatus::RESOURCE_LIMIT
                          : LocalStatus::PREPARATION_FAILED;
      return result;
    }
    auto created = CreateCompleteRecordCodec(compiled);
    result.codec_status = created.status;
    if (created.status != CodecStatus::OK || !created.codec) return result;
    const auto compiled_memory = compiled.MemoryReport();
    auto instance = Add(sizeof(Adapter), description_bytes);
    instance = Add(instance, compiled_memory.plan_accounted_bytes);
    instance = Add(instance, compiled_memory.metadata_accounted_bytes);
    instance = Add(instance, compiled_memory.facade_allocation_bytes);
    instance = Add(instance, created.memory.codec_accounted_total_bytes);
    if (instance > limits.instance_bytes ||
        Add(instance, previous_instance_bytes) > limits.replacement_bytes) {
      result.status = LocalStatus::RESOURCE_LIMIT;
      return result;
    }
    OwnedDescription description;
    if (!CopyDescription(compiled, description)) return result;
    result.adapter.reset(new Adapter(std::move(compiled), std::move(created.codec),
                                     std::move(description), limits, description_bytes, instance));
    result.status = LocalStatus::OK;
  } catch (const std::bad_alloc&) {
    result.status = LocalStatus::ALLOCATION_FAILED;
  } catch (...) {
    result.status = LocalStatus::PREPARATION_FAILED;
  }
  return result;
}

const OwnedMessageDescription* Adapter::Message(std::size_t index) const noexcept {
  return index < description_.messages.size() ? &description_.messages[index] : nullptr;
}

bool Adapter::ConsumeMaterializationFailureHook() noexcept {
  const bool fail = fail_next_materialization_;
  fail_next_materialization_ = false;
  return fail;
}

void Adapter::AllocationCheckpoint() {
  if (fail_next_allocation_) {
    fail_next_allocation_ = false;
    throw std::bad_alloc{};
  }
}

OperationResult Adapter::Decode(std::size_t pipeline_index, ByteView frame) noexcept {
  OperationResult result;
  if ((frame.data == nullptr && frame.size != 0U) || frame.size > limits_.max_frame_bytes) {
    result.local_status = frame.size > limits_.max_frame_bytes ? LocalStatus::RESOURCE_LIMIT
                                                               : LocalStatus::INVALID_INPUT;
    return result;
  }
  const auto decoded = codec_->Decode(pipeline_index, frame);
  result.codec_called = true;
  result.codec_status = decoded.status;
  result.matched_message_index = decoded.matched_message_index;
  result.failed_field_flat_index = decoded.failed_field_flat_index;
  result.conversion_error = decoded.conversion_error;
  result.output_tainted = decoded.output_tainted;
  if (decoded.status != CodecStatus::OK) {
    result.local_status = LocalStatus::CODEC_FAILED;
    try {
      const auto needed = Add(sizeof(OperationResult), frame.size);
      if (needed > limits_.max_result_bytes) {
        result.local_status = LocalStatus::MATERIALIZATION_FAILED;
        return result;
      }
      if (frame.size != 0U)
        result.diagnostic_input_frame.assign(frame.data, frame.data + frame.size);
      result.accounted_bytes = needed;
    } catch (const std::bad_alloc&) {
      result.local_status = LocalStatus::ALLOCATION_FAILED;
      result.diagnostic_input_frame.clear();
    }
    return result;
  }
  if (!decoded.record.HasValue() || !decoded.matched_message_index ||
      decoded.record.MessageIndex() != *decoded.matched_message_index) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    return result;
  }
  const auto* message = Message(decoded.record.MessageIndex());
  if (message == nullptr || decoded.record.FieldCount() > limits_.max_fields ||
      ConsumeMaterializationFailureHook()) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    return result;
  }
  try {
    std::size_t needed = Add(sizeof(OperationResult), frame.size);
    needed = Add(needed, StringBytes(message->id));
    AddVectorStorage<OwnedFieldValue>(decoded.record.FieldCount(), needed);
    for (std::size_t ordinal = 0U; ordinal < decoded.record.FieldCount(); ++ordinal) {
      const auto field = decoded.record.Field(ordinal);
      if (!field || !field->HasValue() || field->Kind() != ValueKind::BYTES ||
          field->Field().message_index != message->index) {
        result.local_status = LocalStatus::MATERIALIZATION_FAILED;
        return result;
      }
      const auto* description = FindField(*message, field->Field().field_index);
      const auto bytes = field->Bytes();
      ByteRange range;
      if (description == nullptr || description->flat_index != field->FlatFieldIndex() || !bytes ||
          bytes->size > limits_.max_field_bytes || !CheckedRange(frame, *bytes, range)) {
        result.local_status = LocalStatus::MATERIALIZATION_FAILED;
        return result;
      }
      needed = Add(needed, StringBytes(description->id));
      needed = Add(needed, bytes->size);
    }
    if (needed > limits_.max_result_bytes) {
      result.local_status = LocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    AllocationCheckpoint();
    OperationResult published = result;
    published.local_status = LocalStatus::OK;
    published.message_index = message->index;
    published.message_id = message->id;
    if (frame.size != 0U) published.frame.assign(frame.data, frame.data + frame.size);
    published.fields.reserve(decoded.record.FieldCount());
    for (std::size_t ordinal = 0U; ordinal < decoded.record.FieldCount(); ++ordinal) {
      const auto field = *decoded.record.Field(ordinal);
      const auto bytes = *field.Bytes();
      const auto* description = FindField(*message, field.Field().field_index);
      OwnedFieldValue value;
      value.flat_index = field.FlatFieldIndex();
      value.field_index = field.Field().field_index;
      value.id = description->id;
      if (bytes.size != 0U) value.bytes.assign(bytes.data, bytes.data + bytes.size);
      if (!CheckedRange(frame, bytes, value.range)) {
        result.local_status = LocalStatus::MATERIALIZATION_FAILED;
        return result;
      }
      published.fields.push_back(std::move(value));
    }
    published.accounted_bytes = needed;
    return published;
  } catch (const std::bad_alloc&) {
    result.local_status = LocalStatus::ALLOCATION_FAILED;
    return result;
  } catch (...) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    return result;
  }
}

OperationResult Adapter::Encode(std::size_t pipeline_index, std::size_t message_index,
                                const std::vector<InputField>& inputs) noexcept {
  OperationResult result;
  if (inputs.size() > limits_.max_fields) {
    result.local_status = LocalStatus::RESOURCE_LIMIT;
    return result;
  }
  try {
    std::vector<EncodeValue> values;
    values.reserve(inputs.size());
    for (const auto& input : inputs) {
      if (input.bytes.size() > limits_.max_field_bytes) {
        result.local_status = LocalStatus::RESOURCE_LIMIT;
        return result;
      }
      values.push_back(EncodeValue::Bytes({message_index, input.field_index},
                                          {input.bytes.data(), input.bytes.size()}));
    }
    std::size_t output_capacity = 0U;
    if (const auto execution = compiled_.PipelineMessageExecution(pipeline_index, message_index)) {
      if (execution->encode_output_size_kind != EncodeOutputSizeKind::NOT_AVAILABLE)
        output_capacity = execution->encode_output_size;
    }
    if (output_capacity > limits_.max_frame_bytes) {
      result.local_status = LocalStatus::RESOURCE_LIMIT;
      return result;
    }
    std::vector<std::uint8_t> output(output_capacity);
    const auto encoded = codec_->Encode(pipeline_index, message_index, values.data(), values.size(),
                                        {output.data(), output.size()});
    result.codec_called = true;
    result.codec_status = encoded.status;
    result.failed_value_index = encoded.failed_value_index;
    result.failed_field_flat_index = encoded.failed_field_flat_index;
    result.conversion_error = encoded.conversion_error;
    if (encoded.status != CodecStatus::OK) {
      result.local_status = LocalStatus::CODEC_FAILED;
      return result;
    }
    const auto* message = Message(message_index);
    if (message == nullptr || !message->encode || encoded.bytes_written > output.size() ||
        ConsumeMaterializationFailureHook()) {
      result.local_status = LocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    std::size_t cursor = 0U;
    std::size_t field_count = 0U;
    std::size_t needed = Add(sizeof(OperationResult), encoded.bytes_written);
    needed = Add(needed, StringBytes(message->id));
    for (const auto& segment : message->encode->segments) {
      if (segment.kind == AsciiSegmentKind::LITERAL) {
        if (segment.literal.size() >
                encoded.bytes_written - (std::min)(cursor, encoded.bytes_written) ||
            cursor > encoded.bytes_written ||
            !std::equal(segment.literal.begin(), segment.literal.end(), output.begin() + cursor)) {
          result.local_status = LocalStatus::MATERIALIZATION_FAILED;
          return result;
        }
        cursor += segment.literal.size();
      } else {
        if (!segment.field_index) {
          result.local_status = LocalStatus::MATERIALIZATION_FAILED;
          return result;
        }
        const auto* input = FindInput(inputs, *segment.field_index);
        const auto* field = FindField(*message, *segment.field_index);
        if (input == nullptr || field == nullptr ||
            input->bytes.size() < field->byte_length.minimum ||
            input->bytes.size() > field->byte_length.maximum || cursor > encoded.bytes_written ||
            input->bytes.size() > encoded.bytes_written - cursor ||
            !std::equal(input->bytes.begin(), input->bytes.end(), output.begin() + cursor)) {
          result.local_status = LocalStatus::MATERIALIZATION_FAILED;
          return result;
        }
        needed = Add(needed, StringBytes(field->id));
        needed = Add(needed, input->bytes.size());
        cursor += input->bytes.size();
        ++field_count;
      }
    }
    AddVectorStorage<OwnedFieldValue>(field_count, needed);
    if (cursor != encoded.bytes_written || needed > limits_.max_result_bytes) {
      result.local_status = LocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    AllocationCheckpoint();
    OperationResult published = result;
    published.local_status = LocalStatus::OK;
    published.message_index = message_index;
    published.message_id = message->id;
    published.frame.assign(output.begin(), output.begin() + encoded.bytes_written);
    published.fields.reserve(field_count);
    cursor = 0U;
    for (const auto& segment : message->encode->segments) {
      if (segment.kind == AsciiSegmentKind::LITERAL) {
        cursor += segment.literal.size();
        continue;
      }
      const auto* input = FindInput(inputs, *segment.field_index);
      const auto* field = FindField(*message, *segment.field_index);
      OwnedFieldValue value;
      value.flat_index = field->flat_index;
      value.field_index = field->field_index;
      value.id = field->id;
      value.bytes = input->bytes;
      value.range = {cursor, input->bytes.size()};
      cursor += input->bytes.size();
      published.fields.push_back(std::move(value));
    }
    published.accounted_bytes = needed;
    return published;
  } catch (const std::bad_alloc&) {
    result.local_status = LocalStatus::ALLOCATION_FAILED;
    return result;
  } catch (...) {
    result.local_status = LocalStatus::MATERIALIZATION_FAILED;
    return result;
  }
}

}  // namespace pae::protocol_lab_ascii::public_offline
