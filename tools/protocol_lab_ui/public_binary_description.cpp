#include "public_binary_description.h"
#include "description_mapping.h"

#include <algorithm>
#include <exception>

namespace pae::protocol_lab_ui {
namespace {
FieldValueType UiType(pae::ValueKind kind) {
  switch (kind) {
    case pae::ValueKind::UINT64: return FieldValueType::UINT64;
    case pae::ValueKind::INT64: return FieldValueType::INT64;
    case pae::ValueKind::BOOL: return FieldValueType::BOOL;
    case pae::ValueKind::BYTES: return FieldValueType::BYTES;
    case pae::ValueKind::ENUM: return FieldValueType::ENUM;
    case pae::ValueKind::DECIMAL64: return FieldValueType::INT64;
  }
  return FieldValueType::UINT64;
}

FieldEncodeSource UiSource(pae::EncodeValueSource source) {
  switch (source) {
    case pae::EncodeValueSource::CALLER_INPUT: return FieldEncodeSource::INPUT;
    case pae::EncodeValueSource::CONSTANT: return FieldEncodeSource::CONSTANT;
    case pae::EncodeValueSource::COMPUTED: return FieldEncodeSource::COMPUTED;
    case pae::EncodeValueSource::NOT_REFERENCED: return FieldEncodeSource::INPUT;
  }
  return FieldEncodeSource::INPUT;
}

template <typename Value>
bool Good(const Value& query) { return query.status == pae::PhysicalQueryStatus::OK &&
                                        query.value.has_value(); }
}  // namespace

bool BuildPublicBinaryDescription(const pae::CompiledProtocol& compiled,
                                  DocumentDescription& output, std::string& error) {
  try {
    const auto protocol = compiled.Protocol();
    if (!protocol || protocol->schema_version != "0.9") {
      error = "public Binary description requires compiled Schema 0.9";
      return false;
    }
    DocumentDescription built;
    built.layout = DocumentLayout::BINARY;
    built.schema_version = protocol->schema_version;
    built.protocol_id = protocol->id;
    built.protocol_version = protocol->version;
    built.display_name = protocol->display_name;
    built.description = protocol->description;
    built.source_ref = protocol->source_ref;
    built.messages.reserve(compiled.MessageCount());
    for (std::size_t m = 0U; m < compiled.MessageCount(); ++m) {
      const auto meta = compiled.Message(m);
      const auto representation = compiled.MessageRepresentation(m);
      const auto physical = compiled.MessagePhysical(m);
      if (!meta || representation.status != pae::PhysicalQueryStatus::OK ||
          representation.value != pae::RecordRepresentation::BINARY || !Good(physical) ||
          meta->index != m || physical.value->message_index != m ||
          physical.value->record_length.maximum == 0U) {
        error = "public Binary Message metadata/physical query mismatch";
        return false;
      }
      MessageDescriptor message;
      message.message_index = m;
      message.id = meta->id;
      message.direction_id = meta->direction_id;
      message.display_name = meta->display_name;
      message.description = meta->description;
      message.source_ref = meta->source_ref;
      message.frame_size = physical.value->record_length.maximum;
      built.max_frame_bytes = (std::max)(built.max_frame_bytes, message.frame_size);
      if (physical.value->maximum_integrity_storage)
        message.integrity_storage = ByteRange{
            physical.value->maximum_integrity_storage->offset,
            physical.value->maximum_integrity_storage->length};
      message.integrity_storage_at_payload_end =
          physical.value->integrity_storage_offset_depends_on_frame_size;
      if (physical.value->computed_length_storage)
        message.computed_length_storage = ByteRange{
            physical.value->computed_length_storage->offset,
            physical.value->computed_length_storage->length};
      message.fields.reserve(meta->field_count);
      for (std::size_t f = 0U; f < meta->field_count; ++f) {
        const auto field_meta = compiled.Field(meta->field_begin + f);
        const auto field_physical = compiled.FieldPhysical(meta->field_begin + f);
        if (!field_meta || !Good(field_physical) || field_meta->flat_index != meta->field_begin + f ||
            field_meta->message_index != m || field_meta->index_in_message != f ||
            field_physical.value->message_index != m || field_physical.value->field_index != f) {
          error = "public Binary Field metadata/physical query mismatch";
          return false;
        }
        FieldDescriptor field;
        field.field_index = f;
        field.id = field_meta->id;
        field.display_name = field_meta->display_name;
        field.description = field_meta->description;
        field.source_ref = field_meta->source_ref;
        field.value_type = UiType(field_meta->value_kind);
        field.decode_decimal64 = field_meta->value_kind == pae::ValueKind::DECIMAL64;
        field.encode_source = UiSource(field_meta->encode_value_source);
        field.decode_referenced = true;
        if (field_meta->encode_value_source != pae::EncodeValueSource::CALLER_INPUT &&
            field_meta->encode_value_source != pae::EncodeValueSource::NOT_REFERENCED)
          field.read_only_annotation = "constant/computed; read-only";
        if (field_physical.value->maximum_byte_range) {
          const auto& range = *field_physical.value->maximum_byte_range;
          field.byte_range = ByteRange{range.offset, range.length};
          field.byte_offset = range.offset;
          field.byte_width = range.length;
        }
        if (field_physical.value->byte_value_length &&
            field_physical.value->byte_range_length_depends_on_frame_size)
          field.byte_length_bounds = ByteLengthBounds{
              field_physical.value->byte_value_length->minimum,
              field_physical.value->byte_value_length->maximum};
        for (std::size_t bit = 0U; bit < field_physical.value->bit_mask_count; ++bit)
          field.physical_bits.push_back({
              field_physical.value->bit_masks[bit].frame_byte_index,
              field_physical.value->bit_masks[bit].mask});
        for (std::size_t e = 0U; e < field_meta->enum_count; ++e) {
          const auto item = compiled.Enum(field_meta->enum_begin + e);
          if (!item || item->field_flat_index != field_meta->flat_index ||
              item->index_in_field != e) {
            error = "public Binary Enum metadata mismatch";
            return false;
          }
          field.enum_entries.push_back({e, std::string(item->id),
                                        std::string(item->display_name), item->raw_value});
        }
        message.fields.push_back(std::move(field));
      }
      built.messages.push_back(std::move(message));
    }
    built.pipelines.reserve(compiled.PipelineCount());
    for (std::size_t p = 0U; p < compiled.PipelineCount(); ++p) {
      const auto meta = compiled.Pipeline(p);
      if (!meta || meta->index != p) {
        error = "public Binary Pipeline metadata mismatch";
        return false;
      }
      PipelineDescriptor pipeline;
      pipeline.pipeline_index = p;
      pipeline.id = meta->id;
      pipeline.direction_id = meta->direction_id;
      pipeline.display_name = meta->display_name;
      pipeline.description = meta->description;
      pipeline.source_ref = meta->source_ref;
      for (std::size_t a = 0U; a < meta->message_count; ++a) {
        const auto index = compiled.PipelineMessageIndex(p, a);
        const auto execution = index ? compiled.PipelineMessageExecution(p, *index) : std::nullopt;
        if (!index || *index >= built.messages.size() || !execution) {
          error = "public Binary Pipeline association mismatch";
          return false;
        }
        pipeline.message_indices.push_back(*index);
        if (execution->decode_available) pipeline.decode_message_indices.push_back(*index);
      }
      built.pipelines.push_back(std::move(pipeline));
    }
    if (built.max_frame_bytes == 0U || built.pipelines.empty() || built.messages.empty()) {
      error = "public Binary description has no selectable record";
      return false;
    }
    output = std::move(built);
    error.clear();
    return true;
  } catch (const std::exception& exception) {
    error = exception.what();
    return false;
  }
}
}  // namespace pae::protocol_lab_ui
