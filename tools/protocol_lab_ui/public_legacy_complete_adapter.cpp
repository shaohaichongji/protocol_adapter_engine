#include "public_legacy_complete_adapter.h"

#include <algorithm>
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
#include <atomic>
#endif
#include <charconv>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pae::protocol_lab_ui {
namespace {

#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
std::atomic<bool> g_fail_description_copy_allocation{false};
std::atomic<bool> g_fail_preparation_diagnostic_allocation{false};
#endif

std::size_t Add(std::size_t left, std::size_t right) {
  if (right > (std::numeric_limits<std::size_t>::max)() - left)
    throw std::length_error("public legacy complete budget overflow");
  return left + right;
}

std::size_t Multiply(std::size_t left, std::size_t right) {
  if (right != 0U && left > (std::numeric_limits<std::size_t>::max)() / right)
    throw std::length_error("public legacy complete budget overflow");
  return left * right;
}

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

bool AddStorage(std::size_t count, std::size_t item_size, std::size_t& total) noexcept {
  std::size_t bytes = 0U;
  return MultiplyChecked(count, item_size, bytes) && AddChecked(bytes, total);
}

bool AddText(std::string_view value, std::size_t& total) noexcept {
  return value.size() != (std::numeric_limits<std::size_t>::max)() &&
         AddChecked(value.size() + 1U, total);
}

std::string Hex(const std::uint8_t* data, std::size_t size) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  result.resize(Multiply(size, 2U));
  for (std::size_t index = 0U; index < size; ++index) {
    result[index * 2U] = digits[data[index] >> 4U];
    result[index * 2U + 1U] = digits[data[index] & 0x0FU];
  }
  return result;
}

template <typename Integer>
std::size_t IntegerTextBytes(Integer value) noexcept {
  char buffer[64];
  const auto converted = std::to_chars(buffer, buffer + sizeof(buffer), value);
  return converted.ec == std::errc{} ? static_cast<std::size_t>(converted.ptr - buffer) + 1U : 0U;
}

FieldValueType UiType(pae::ValueKind value) noexcept {
  switch (value) {
    case pae::ValueKind::UINT64: return FieldValueType::UINT64;
    case pae::ValueKind::INT64: return FieldValueType::INT64;
    case pae::ValueKind::BYTES: return FieldValueType::BYTES;
    case pae::ValueKind::ENUM: return FieldValueType::ENUM;
    case pae::ValueKind::BOOL: return FieldValueType::BOOL;
    case pae::ValueKind::DECIMAL64: return FieldValueType::INT64;
  }
  return FieldValueType::UINT64;
}

FieldEncodeSource UiSource(pae::EncodeValueSource value) noexcept {
  switch (value) {
    case pae::EncodeValueSource::CALLER_INPUT: return FieldEncodeSource::INPUT;
    case pae::EncodeValueSource::CONSTANT: return FieldEncodeSource::CONSTANT;
    case pae::EncodeValueSource::COMPUTED: return FieldEncodeSource::COMPUTED;
    case pae::EncodeValueSource::NOT_REFERENCED: return FieldEncodeSource::NOT_REFERENCED;
  }
  return FieldEncodeSource::INPUT;
}

bool SupportedLegacy(std::string_view version) noexcept {
  return version == "0.5" || version == "0.6" || version == "0.7" || version == "0.8";
}

enum class DescriptionPreflightStatus { OK, INVALID, RESOURCE_LIMIT };

DescriptionPreflightStatus PreflightDescription(const pae::CompiledProtocol& compiled,
                                                 const PublicLegacyCompleteLimits& limits,
                                                 std::size_t& accounted) noexcept {
  const auto protocol = compiled.Protocol();
  if (!protocol || !SupportedLegacy(protocol->schema_version) || compiled.MessageCount() == 0U ||
      compiled.PipelineCount() == 0U)
    return DescriptionPreflightStatus::INVALID;
  std::size_t total = sizeof(DocumentDescription);
  if (!AddStorage(compiled.MessageCount(), sizeof(MessageDescriptor), total) ||
      !AddStorage(compiled.PipelineCount(), sizeof(PipelineDescriptor), total) ||
      !AddText(protocol->schema_version, total) || !AddText(protocol->id, total) ||
      !AddText(protocol->version, total) || !AddText(protocol->display_name, total) ||
      !AddText(protocol->description, total) || !AddText(protocol->source_ref, total))
    return DescriptionPreflightStatus::RESOURCE_LIMIT;
  if (total > limits.max_description_bytes) return DescriptionPreflightStatus::RESOURCE_LIMIT;

  for (std::size_t message_index = 0U; message_index < compiled.MessageCount(); ++message_index) {
    const auto message = compiled.Message(message_index);
    const auto representation = compiled.MessageRepresentation(message_index);
    const auto physical = compiled.MessagePhysical(message_index);
    if (!message || message->index != message_index || message->field_count > limits.max_fields ||
        representation.status != pae::PhysicalQueryStatus::OK ||
        representation.value != pae::RecordRepresentation::BINARY ||
        physical.status != pae::PhysicalQueryStatus::OK || !physical.value ||
        physical.value->message_index != message_index ||
        physical.value->record_length.maximum == 0U ||
        physical.value->record_length.maximum > limits.max_frame_bytes ||
        !AddText(message->id, total) || !AddText(message->direction_id, total) ||
        !AddText(message->display_name, total) || !AddText(message->description, total) ||
        !AddText(message->source_ref, total) ||
        !AddStorage(message->field_count, sizeof(FieldDescriptor), total))
      return DescriptionPreflightStatus::INVALID;
    if (total > limits.max_description_bytes) return DescriptionPreflightStatus::RESOURCE_LIMIT;
    bool bounded_payload_seen = false;
    for (std::size_t field_index = 0U; field_index < message->field_count; ++field_index) {
      const std::size_t flat = message->field_begin + field_index;
      const auto field = compiled.Field(flat);
      const auto field_physical = compiled.FieldPhysical(flat);
      if (!field || field->flat_index != flat || field->message_index != message_index ||
          field->index_in_message != field_index ||
          field_physical.status != pae::PhysicalQueryStatus::OK || !field_physical.value ||
          field_physical.value->message_index != message_index ||
          field_physical.value->field_index != field_index || !AddText(field->id, total) ||
          !AddText(field->display_name, total) || !AddText(field->description, total) ||
          !AddText(field->source_ref, total) ||
          !AddStorage(field->enum_count, sizeof(EnumDescriptor), total))
        return DescriptionPreflightStatus::INVALID;
      if (field->encode_value_source == pae::EncodeValueSource::CONSTANT) {
        if (!AddText("constant; read-only", total)) return DescriptionPreflightStatus::RESOURCE_LIMIT;
      } else if (field->encode_value_source == pae::EncodeValueSource::COMPUTED) {
        if (!AddText("computed length; read-only", total))
          return DescriptionPreflightStatus::RESOURCE_LIMIT;
      } else if (!AddText({}, total)) {
        return DescriptionPreflightStatus::RESOURCE_LIMIT;
      }
      if (field_physical.value->physical_kind == pae::FieldPhysicalKind::BYTE_RANGE) {
        if (!field_physical.value->maximum_byte_range)
          return DescriptionPreflightStatus::INVALID;
        if (field_physical.value->byte_range_length_depends_on_frame_size) {
          if (!field_physical.value->byte_value_length || bounded_payload_seen)
            return DescriptionPreflightStatus::INVALID;
          bounded_payload_seen = true;
        }
      } else if (field_physical.value->bit_mask_count == 0U ||
                 field_physical.value->bit_mask_count > pae::kMaximumFieldPhysicalBitMasks ||
                 !AddStorage(field_physical.value->bit_mask_count, sizeof(PhysicalBitMask), total)) {
        return DescriptionPreflightStatus::INVALID;
      }
      for (std::size_t entry_index = 0U; entry_index < field->enum_count; ++entry_index) {
        const auto entry = compiled.Enum(field->enum_begin + entry_index);
        if (!entry || entry->field_flat_index != flat || entry->index_in_field != entry_index ||
            !AddText(entry->id, total) || !AddText(entry->display_name, total))
          return DescriptionPreflightStatus::INVALID;
      }
      if (total > limits.max_description_bytes)
        return DescriptionPreflightStatus::RESOURCE_LIMIT;
    }
    if (physical.value->record_length.minimum != physical.value->record_length.maximum &&
        !bounded_payload_seen)
      return DescriptionPreflightStatus::INVALID;
  }

  for (std::size_t pipeline_index = 0U; pipeline_index < compiled.PipelineCount();
       ++pipeline_index) {
    const auto pipeline = compiled.Pipeline(pipeline_index);
    if (!pipeline || pipeline->index != pipeline_index || !AddText(pipeline->id, total) ||
        !AddText(pipeline->direction_id, total) || !AddText(pipeline->display_name, total) ||
        !AddText(pipeline->description, total) || !AddText(pipeline->source_ref, total) ||
        !AddStorage(pipeline->message_count, sizeof(std::size_t), total))
      return DescriptionPreflightStatus::INVALID;
    std::size_t decode_count = 0U;
    for (std::size_t association = 0U; association < pipeline->message_count; ++association) {
      const auto message_index = compiled.PipelineMessageIndex(pipeline_index, association);
      const auto execution = message_index
                                 ? compiled.PipelineMessageExecution(pipeline_index, *message_index)
                                 : std::nullopt;
      if (!message_index || *message_index >= compiled.MessageCount() || !execution)
        return DescriptionPreflightStatus::INVALID;
      if (execution->decode_available) ++decode_count;
    }
    if (!AddStorage(decode_count, sizeof(std::size_t), total))
      return DescriptionPreflightStatus::RESOURCE_LIMIT;
    if (total > limits.max_description_bytes) return DescriptionPreflightStatus::RESOURCE_LIMIT;
  }
  accounted = total;
  return DescriptionPreflightStatus::OK;
}

bool BuildDescription(const pae::CompiledProtocol& compiled, DocumentDescription& output,
                      std::size_t expected_accounted, std::size_t& accounted,
                      const PublicLegacyCompleteLimits& limits, std::string& error) {
  const auto protocol = compiled.Protocol();
  if (!protocol || !SupportedLegacy(protocol->schema_version)) {
    error = "public legacy complete requires Schema 0.5-0.8";
    return false;
  }
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
  if (g_fail_description_copy_allocation.exchange(false, std::memory_order_acq_rel))
    throw std::bad_alloc{};
#endif
  DocumentDescription built;
  built.layout = DocumentLayout::BINARY;
  built.schema_version = protocol->schema_version;
  built.protocol_id = protocol->id;
  built.protocol_version = protocol->version;
  built.display_name = protocol->display_name;
  built.description = protocol->description;
  built.source_ref = protocol->source_ref;
  built.messages.reserve(compiled.MessageCount());
  for (std::size_t message_index = 0U; message_index < compiled.MessageCount(); ++message_index) {
    const auto meta = compiled.Message(message_index);
    const auto representation = compiled.MessageRepresentation(message_index);
    const auto physical = compiled.MessagePhysical(message_index);
    if (!meta || meta->index != message_index ||
        representation.status != pae::PhysicalQueryStatus::OK ||
        representation.value != pae::RecordRepresentation::BINARY ||
        physical.status != pae::PhysicalQueryStatus::OK || !physical.value ||
        physical.value->message_index != message_index ||
        physical.value->record_length.maximum == 0U ||
        physical.value->record_length.maximum > limits.max_frame_bytes ||
        meta->field_count > limits.max_fields) {
      error = "public legacy Message metadata/physical query mismatch";
      return false;
    }
    MessageDescriptor message;
    message.encode_available = false;
    message.decode_available = false;
    message.message_index = message_index;
    message.id = meta->id;
    message.direction_id = meta->direction_id;
    message.display_name = meta->display_name;
    message.description = meta->description;
    message.source_ref = meta->source_ref;
    message.frame_size = physical.value->record_length.maximum;
    built.max_frame_bytes = (std::max)(built.max_frame_bytes, message.frame_size);
    if (physical.value->maximum_integrity_storage)
      message.integrity_storage = ByteRange{physical.value->maximum_integrity_storage->offset,
                                            physical.value->maximum_integrity_storage->length};
    message.integrity_storage_at_payload_end =
        physical.value->integrity_storage_offset_depends_on_frame_size;
    message.integrity_range_ends_at_payload =
        physical.value->integrity_storage_offset_depends_on_frame_size;
    if (physical.value->computed_length_storage)
      message.computed_length_storage = ByteRange{physical.value->computed_length_storage->offset,
                                                  physical.value->computed_length_storage->length};
    message.fields.reserve(meta->field_count);
    std::optional<BoundedPayloadDescriptor> bounded;
    for (std::size_t field_index = 0U; field_index < meta->field_count; ++field_index) {
      const std::size_t flat = meta->field_begin + field_index;
      const auto field_meta = compiled.Field(flat);
      const auto field_physical = compiled.FieldPhysical(flat);
      if (!field_meta || field_meta->flat_index != flat ||
          field_meta->message_index != message_index ||
          field_meta->index_in_message != field_index ||
          field_physical.status != pae::PhysicalQueryStatus::OK || !field_physical.value ||
          field_physical.value->message_index != message_index ||
          field_physical.value->field_index != field_index) {
        error = "public legacy Field metadata/physical query mismatch";
        return false;
      }
      FieldDescriptor field;
      field.field_index = field_index;
      field.id = field_meta->id;
      field.display_name = field_meta->display_name;
      field.description = field_meta->description;
      field.source_ref = field_meta->source_ref;
      field.value_type = UiType(field_meta->value_kind);
      field.decimal_conversion = field_meta->value_kind == pae::ValueKind::DECIMAL64;
      field.encode_source = UiSource(field_meta->encode_value_source);
      field.decode_referenced = true;
      field.encode_referenced =
          field_meta->encode_value_source == pae::EncodeValueSource::CALLER_INPUT;
      if (field.encode_source == FieldEncodeSource::CONSTANT)
        field.read_only_annotation = "constant; read-only";
      else if (field.encode_source == FieldEncodeSource::COMPUTED)
        field.read_only_annotation = "computed length; read-only";
      if (field_physical.value->physical_kind == pae::FieldPhysicalKind::BYTE_RANGE) {
        if (!field_physical.value->maximum_byte_range) {
          error = "public legacy byte Field has no maximum range";
          return false;
        }
        const auto range = *field_physical.value->maximum_byte_range;
        field.byte_range = ByteRange{range.offset, range.length};
        field.byte_offset = range.offset;
        field.byte_width = range.length;
        field.wire_codec = field.value_type == FieldValueType::BYTES
                               ? FieldWireCodec::BYTES
                               : FieldWireCodec::UNSIGNED_INTEGER;
        if (field_physical.value->byte_range_length_depends_on_frame_size) {
          if (!field_physical.value->byte_value_length || bounded.has_value() ||
              range.offset > physical.value->record_length.maximum ||
              field_physical.value->byte_value_length->maximum >
                  physical.value->record_length.maximum - range.offset) {
            error = "public legacy bounded payload facts are inconsistent";
            return false;
          }
          const auto bounds = *field_physical.value->byte_value_length;
          field.byte_length_bounds = ByteLengthBounds{bounds.minimum, bounds.maximum};
          const std::size_t trailer =
              physical.value->record_length.maximum - range.offset - bounds.maximum;
          bounded = BoundedPayloadDescriptor{field_index,
                                             range.offset,
                                             bounds.minimum,
                                             bounds.maximum,
                                             trailer,
                                             Add(Add(range.offset, bounds.minimum), trailer),
                                             physical.value->record_length.maximum};
          if (bounded->min_frame_length != physical.value->record_length.minimum) {
            error = "public legacy bounded record minimum is inconsistent";
            return false;
          }
        }
      } else {
        field.wire_codec = FieldWireCodec::BITFIELD;
        if (field_physical.value->bit_mask_count == 0U ||
            field_physical.value->bit_mask_count > pae::kMaximumFieldPhysicalBitMasks) {
          error = "public legacy bit Field mask count is invalid";
          return false;
        }
        for (std::size_t index = 0U; index < field_physical.value->bit_mask_count; ++index)
          field.physical_bits.push_back({field_physical.value->bit_masks[index].frame_byte_index,
                                         field_physical.value->bit_masks[index].mask});
      }
      field.enum_entries.reserve(field_meta->enum_count);
      for (std::size_t entry_index = 0U; entry_index < field_meta->enum_count; ++entry_index) {
        const auto entry = compiled.Enum(field_meta->enum_begin + entry_index);
        if (!entry || entry->field_flat_index != flat || entry->index_in_field != entry_index) {
          error = "public legacy Enum metadata mismatch";
          return false;
        }
        field.enum_entries.push_back(
            {entry_index, std::string(entry->id), std::string(entry->display_name), entry->raw_value});
      }
      message.fields.push_back(std::move(field));
    }
    if (physical.value->record_length.minimum != physical.value->record_length.maximum) {
      if (!bounded) {
        error = "public legacy variable record has no unique bounded payload";
        return false;
      }
      message.bounded_payload = bounded;
    }
    built.messages.push_back(std::move(message));
  }
  built.pipelines.reserve(compiled.PipelineCount());
  for (std::size_t pipeline_index = 0U; pipeline_index < compiled.PipelineCount(); ++pipeline_index) {
    const auto meta = compiled.Pipeline(pipeline_index);
    if (!meta || meta->index != pipeline_index) {
      error = "public legacy Pipeline metadata mismatch";
      return false;
    }
    PipelineDescriptor pipeline;
    pipeline.pipeline_index = pipeline_index;
    pipeline.id = meta->id;
    pipeline.direction_id = meta->direction_id;
    pipeline.display_name = meta->display_name;
    pipeline.description = meta->description;
    pipeline.source_ref = meta->source_ref;
    pipeline.message_indices.reserve(meta->message_count);
    std::size_t decode_count = 0U;
    for (std::size_t association = 0U; association < meta->message_count; ++association) {
      const auto message_index = compiled.PipelineMessageIndex(pipeline_index, association);
      const auto execution = message_index
                                 ? compiled.PipelineMessageExecution(pipeline_index, *message_index)
                                 : std::nullopt;
      if (message_index && execution && execution->decode_available) ++decode_count;
    }
    pipeline.decode_message_indices.reserve(decode_count);
    for (std::size_t association = 0U; association < meta->message_count; ++association) {
      const auto message_index = compiled.PipelineMessageIndex(pipeline_index, association);
      const auto execution = message_index
                                 ? compiled.PipelineMessageExecution(pipeline_index, *message_index)
                                 : std::nullopt;
      if (!message_index || *message_index >= built.messages.size() || !execution) {
        error = "public legacy Pipeline association mismatch";
        return false;
      }
      pipeline.message_indices.push_back(*message_index);
      if (execution->decode_available) pipeline.decode_message_indices.push_back(*message_index);
      built.messages[*message_index].decode_available |= execution->decode_available;
      built.messages[*message_index].encode_available |= execution->encode_available;
    }
    built.pipelines.push_back(std::move(pipeline));
  }
  accounted = expected_accounted;
  output = std::move(built);
  error.clear();
  return true;
}

}  // namespace

const char* PublicLegacyCodecStatusName(pae::CodecStatus status) noexcept {
#define PAE_CODEC_CASE(name) case pae::CodecStatus::name: return #name
  switch (status) {
    PAE_CODEC_CASE(OK); PAE_CODEC_CASE(INVALID_ARGUMENT);
    PAE_CODEC_CASE(INVALID_COMPILED_PROTOCOL); PAE_CODEC_CASE(RESOURCE_LIMIT_EXCEEDED);
    PAE_CODEC_CASE(ALLOCATION_FAILED); PAE_CODEC_CASE(WORKSPACE_BUSY);
    PAE_CODEC_CASE(INPUT_VALUES_TOO_MANY); PAE_CODEC_CASE(UNKNOWN_MESSAGE);
    PAE_CODEC_CASE(AMBIGUOUS_MESSAGE); PAE_CODEC_CASE(OUTPUT_SLOTS_TOO_SMALL);
    PAE_CODEC_CASE(INTEGRITY_FAILED); PAE_CODEC_CASE(MESSAGE_NOT_ALLOWED);
    PAE_CODEC_CASE(FIELD_REFERENCE_MISMATCH); PAE_CODEC_CASE(DUPLICATE_FIELD);
    PAE_CODEC_CASE(MISSING_FIELD); PAE_CODEC_CASE(TYPE_MISMATCH);
    PAE_CODEC_CASE(VALUE_NOT_REPRESENTABLE); PAE_CODEC_CASE(BYTES_LENGTH_MISMATCH);
    PAE_CODEC_CASE(UNKNOWN_ENUM_VALUE); PAE_CODEC_CASE(ENUM_REFERENCE_MISMATCH);
    PAE_CODEC_CASE(CONSTANT_FIELD_OVERRIDE); PAE_CODEC_CASE(INPUT_OUTPUT_OVERLAP);
    PAE_CODEC_CASE(BUFFER_TOO_SMALL); PAE_CODEC_CASE(FINAL_REVIEW_FAILED);
    PAE_CODEC_CASE(ASCII_CHARACTER_NOT_ALLOWED); PAE_CODEC_CASE(ASCII_TERMINATOR_CONFLICT);
    PAE_CODEC_CASE(OPERATION_NOT_SUPPORTED); PAE_CODEC_CASE(COMPUTED_FIELD_OVERRIDE);
    PAE_CODEC_CASE(LENGTH_MISMATCH); PAE_CODEC_CASE(INTERNAL_ERROR);
  }
#undef PAE_CODEC_CASE
  return "UNKNOWN_CODEC_STATUS";
}

const char* PublicLegacyConversionErrorName(pae::ConversionError error) noexcept {
  switch (error) {
    case pae::ConversionError::NONE: return "NONE";
    case pae::ConversionError::DECIMAL_SCALE_OUT_OF_RANGE: return "DECIMAL_SCALE_OUT_OF_RANGE";
    case pae::ConversionError::RAW_NOT_INTEGRAL: return "RAW_NOT_INTEGRAL";
    case pae::ConversionError::RAW_OUT_OF_RANGE: return "RAW_OUT_OF_RANGE";
    case pae::ConversionError::LOGICAL_OUT_OF_RANGE: return "LOGICAL_OUT_OF_RANGE";
  }
  return "UNKNOWN_CONVERSION_ERROR";
}

PublicLegacyCompleteAdapter::~PublicLegacyCompleteAdapter() = default;

#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
void PublicLegacyCompleteAdapter::FailNextDescriptionCopyAllocationForTesting() noexcept {
  g_fail_description_copy_allocation.store(true, std::memory_order_release);
}

void PublicLegacyCompleteAdapter::FailNextPreparationDiagnosticAllocationForTesting() noexcept {
  g_fail_preparation_diagnostic_allocation.store(true, std::memory_order_release);
}
#endif

PublicLegacyCompleteAdapter::Preparation PublicLegacyCompleteAdapter::AdoptCompiled(
    pae::CompiledProtocol compiled, const PublicLegacyCompleteLimits& limits,
    std::size_t previous_instance_bytes) noexcept {
  Preparation result;
  try {
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
    if (g_fail_preparation_diagnostic_allocation.exchange(false, std::memory_order_acq_rel))
      throw std::bad_alloc{};
#endif
    if (!compiled.HasValue()) {
      result.error = PublicLegacyPreparationError::INVALID_COMPILED;
      return result;
    }
    std::size_t description_preflight_bytes = 0U;
    const auto description_preflight =
        PreflightDescription(compiled, limits, description_preflight_bytes);
    if (description_preflight != DescriptionPreflightStatus::OK) {
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
      g_fail_description_copy_allocation.store(false, std::memory_order_release);
#endif
      result.status = description_preflight == DescriptionPreflightStatus::RESOURCE_LIMIT
                          ? PublicLegacyLocalStatus::RESOURCE_LIMIT
                          : PublicLegacyLocalStatus::PREPARATION_FAILED;
      result.error = description_preflight == DescriptionPreflightStatus::RESOURCE_LIMIT
                         ? PublicLegacyPreparationError::DESCRIPTION_LIMIT
                         : PublicLegacyPreparationError::DESCRIPTION_INVALID;
      return result;
    }
    const auto memory = compiled.MemoryReport();
    std::size_t instance = sizeof(PublicLegacyCompleteAdapter);
    if (!AddChecked(description_preflight_bytes, instance) ||
        !AddChecked(memory.plan_accounted_bytes, instance) ||
        !AddChecked(memory.metadata_accounted_bytes, instance) ||
        !AddChecked(memory.facade_allocation_bytes, instance) ||
        instance > limits.instance_bytes || previous_instance_bytes > limits.replacement_bytes ||
        instance > limits.replacement_bytes - previous_instance_bytes) {
      result.status = PublicLegacyLocalStatus::RESOURCE_LIMIT;
      result.error = PublicLegacyPreparationError::INSTANCE_LIMIT;
      return result;
    }
    pae::CompleteRecordCodecOptions probe_options;
    probe_options.execution_memory_limit_bytes = 0U;
    auto probe = pae::CreateCompleteRecordCodec(compiled, probe_options);
    const std::size_t codec_bytes = probe.memory.codec_accounted_total_bytes;
    if (probe.status != pae::CodecStatus::RESOURCE_LIMIT_EXCEEDED || codec_bytes == 0U) {
      result.codec_status = probe.status;
      result.error = PublicLegacyPreparationError::CODEC_PROBE_FAILED;
      return result;
    }
    std::size_t codecs_bytes = 0U;
    if (!MultiplyChecked(codec_bytes, 2U, codecs_bytes) ||
        !AddChecked(codecs_bytes, instance) || instance > limits.instance_bytes ||
        previous_instance_bytes > limits.replacement_bytes ||
        instance > limits.replacement_bytes - previous_instance_bytes) {
      result.status = PublicLegacyLocalStatus::RESOURCE_LIMIT;
      result.error = PublicLegacyPreparationError::INSTANCE_LIMIT;
      return result;
    }
    DocumentDescription description;
    std::size_t description_bytes = 0U;
    if (!BuildDescription(compiled, description, description_preflight_bytes, description_bytes,
                          limits, result.detail)) {
      result.error = PublicLegacyPreparationError::DESCRIPTION_INVALID;
      return result;
    }
    pae::CompleteRecordCodecOptions create_options;
    create_options.execution_memory_limit_bytes = codec_bytes;
    auto main = pae::CreateCompleteRecordCodec(compiled, create_options);
    result.codec_status = main.status;
    if (main.status != pae::CodecStatus::OK || !main.codec) {
      result.error = PublicLegacyPreparationError::CODEC_CREATE_FAILED;
      return result;
    }
    auto review = pae::CreateCompleteRecordCodec(compiled, create_options);
    if (review.status != pae::CodecStatus::OK || !review.codec) {
      result.codec_status = review.status;
      result.error = PublicLegacyPreparationError::CODEC_CREATE_FAILED;
      return result;
    }
    auto adapter = std::unique_ptr<PublicLegacyCompleteAdapter>(new PublicLegacyCompleteAdapter);
    adapter->compiled_ = std::move(compiled);
    adapter->codec_ = std::move(main.codec);
    adapter->review_codec_ = std::move(review.codec);
    adapter->limits_ = limits;
    adapter->instance_bytes_ = instance;
    adapter->description_bytes_ = description_bytes;
    result.description = std::move(description);
    result.adapter = std::move(adapter);
    result.status = PublicLegacyLocalStatus::OK;
    result.error = PublicLegacyPreparationError::NONE;
    result.detail.clear();
  } catch (const std::bad_alloc&) {
    result.status = PublicLegacyLocalStatus::ALLOCATION_FAILED;
    result.error = PublicLegacyPreparationError::ALLOCATION_FAILED;
    result.detail.clear();
  } catch (...) {
    result.status = PublicLegacyLocalStatus::PREPARATION_FAILED;
    result.error = PublicLegacyPreparationError::INTERNAL_EXCEPTION;
    result.detail.clear();
  }
  return result;
}

PublicLegacyOperation PublicLegacyCompleteAdapter::Materialize(
    const pae::DecodeResult& decoded, const std::vector<std::uint8_t>& frame, bool review) {
  PublicLegacyOperation result;
  result.codec_called = !review;
  result.review_decode_called = review;
  result.codec_status = review ? pae::CodecStatus::OK : decoded.status;
  result.review_status = review ? decoded.status : pae::CodecStatus::INVALID_ARGUMENT;
  result.message_index = decoded.matched_message_index;
  result.conversion_error = decoded.conversion_error;
  std::optional<pae::MessageDescription> matched_message;
  if (decoded.matched_message_index) {
    matched_message = compiled_.Message(*decoded.matched_message_index);
  }
  std::optional<pae::FieldDescription> failed_field;
  if (decoded.failed_field_flat_index) {
    failed_field = compiled_.Field(*decoded.failed_field_flat_index);
    if (failed_field) {
      result.failed_field_index = failed_field->index_in_message;
    }
  }
  if (decoded.status != pae::CodecStatus::OK || !decoded.record.HasValue() ||
      !decoded.matched_message_index ||
      decoded.record.MessageIndex() != *decoded.matched_message_index) {
    result.local_status = decoded.status == pae::CodecStatus::OK
                              ? PublicLegacyLocalStatus::MATERIALIZATION_FAILED
                              : (review ? PublicLegacyLocalStatus::REVIEW_FAILED
                                        : PublicLegacyLocalStatus::CODEC_FAILED);
    std::size_t needed = sizeof(PublicLegacyOperation);
    const bool admitted = AddChecked(frame.size(), needed) &&
                          AddChecked(matched_message ? matched_message->id.size() + 1U : 1U,
                                     needed) &&
                          AddChecked(failed_field ? failed_field->id.size() + 1U : 1U, needed) &&
                          needed <= limits_.max_result_bytes;
    if (!admitted) {
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
      fail_next_result_allocation_ = false;
#endif
      result.local_status = PublicLegacyLocalStatus::RESOURCE_LIMIT;
      return result;
    }
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
    if (fail_next_result_allocation_) {
      fail_next_result_allocation_ = false;
      throw std::bad_alloc{};
    }
#endif
    if (matched_message) result.message_id = matched_message->id;
    if (failed_field) result.failed_field_id = failed_field->id;
    result.frame = frame;
    result.accounted_bytes = needed;
    return result;
  }
  const auto message = compiled_.Message(*decoded.matched_message_index);
  if (!message || message->field_count != decoded.record.FieldCount() ||
      message->field_count > limits_.max_fields) {
    result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
    return result;
  }
  std::size_t preflight = sizeof(PublicLegacyOperation);
  bool admitted = AddChecked(frame.size(), preflight) && AddText(message->id, preflight) &&
                  AddText({}, preflight) &&
                  AddStorage(message->field_count, sizeof(UiFieldResult), preflight);
  for (std::size_t ordinal = 0U; ordinal < message->field_count; ++ordinal) {
    const auto view = decoded.record.Field(ordinal);
    const auto meta = view ? compiled_.Field(view->FlatFieldIndex()) : std::nullopt;
    if (!view || !view->HasValue() || !meta || meta->message_index != message->index ||
        meta->index_in_message != ordinal || meta->value_kind != view->Kind()) {
      result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    const auto physical = compiled_.ResolveFieldPhysical(view->FlatFieldIndex(), frame.size());
    if (physical.status != pae::PhysicalQueryStatus::OK || !physical.value) {
      result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
      return result;
    }
    admitted = admitted && AddText(meta->id, preflight);
    switch (view->Kind()) {
      case pae::ValueKind::UINT64: {
        const auto value = view->UInt64();
        const std::size_t text_bytes = value ? IntegerTextBytes(*value) : 0U;
        admitted = admitted && text_bytes != 0U && AddChecked(text_bytes, preflight) &&
                   AddChecked(text_bytes, preflight);
        break;
      }
      case pae::ValueKind::INT64: {
        const auto value = view->Int64();
        const std::size_t text_bytes = value ? IntegerTextBytes(*value) : 0U;
        admitted = admitted && text_bytes != 0U && AddChecked(text_bytes, preflight) &&
                   AddChecked(text_bytes, preflight);
        break;
      }
      case pae::ValueKind::BOOL: {
        const auto value = view->Bool();
        admitted = admitted && value && AddChecked(2U, preflight) &&
                   AddChecked(*value ? 5U : 6U, preflight);
        break;
      }
      case pae::ValueKind::BYTES: {
        const auto bytes = view->Bytes();
        std::size_t hex_size = 0U;
        if (!bytes || (bytes->size != 0U && bytes->data == nullptr)) {
          result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
          return result;
        }
        if (bytes->size > limits_.max_field_bytes) {
          result.local_status = PublicLegacyLocalStatus::RESOURCE_LIMIT;
          return result;
        }
        admitted = admitted && MultiplyChecked(bytes->size, 2U, hex_size) &&
                   hex_size != (std::numeric_limits<std::size_t>::max)() &&
                   AddChecked(hex_size + 1U, preflight) &&
                   AddChecked(hex_size + 1U, preflight);
        break;
      }
      case pae::ValueKind::ENUM: {
        const auto raw = view->EnumRawValue();
        const std::size_t raw_bytes = raw ? IntegerTextBytes(*raw) : 0U;
        admitted = admitted && raw_bytes != 0U && AddChecked(raw_bytes, preflight);
        if (view->KnownEnumFlatIndex()) {
          const auto entry = compiled_.Enum(*view->KnownEnumFlatIndex());
          if (!entry || entry->field_flat_index != meta->flat_index || !raw ||
              entry->raw_value != *raw) {
            result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
            return result;
          }
          admitted = admitted && AddText(entry->id, preflight);
        } else {
          admitted = admitted && AddChecked(raw_bytes, preflight);
        }
        break;
      }
      case pae::ValueKind::DECIMAL64: {
        const auto decimal = view->Decimal();
        const auto raw_kind = view->ConversionRawKind();
        std::size_t raw_bytes = 0U;
        if (!decimal || !raw_kind) {
          result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
          return result;
        }
        if (*raw_kind == pae::RawIntegerKind::UINT64) {
          const auto raw = view->ConversionRawUInt64();
          raw_bytes = raw ? IntegerTextBytes(*raw) : 0U;
        } else {
          const auto raw = view->ConversionRawInt64();
          raw_bytes = raw ? IntegerTextBytes(*raw) : 0U;
        }
        admitted = admitted && raw_bytes != 0U && AddChecked(raw_bytes, preflight) &&
                   AddChecked(1U, preflight);
        break;
      }
    }
  }
  if (!admitted || preflight > limits_.max_result_bytes) {
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
    fail_next_result_allocation_ = false;
#endif
    result.local_status = PublicLegacyLocalStatus::RESOURCE_LIMIT;
    return result;
  }
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
  if (fail_next_result_allocation_) {
    fail_next_result_allocation_ = false;
    throw std::bad_alloc{};
  }
#endif
  result.frame = frame;
  result.message_id = message->id;
  result.fields.reserve(message->field_count);
  const auto fail_materialization = [&result]() {
    result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
    result.frame.clear();
    result.message_id.clear();
    result.failed_field_id.clear();
    result.fields.clear();
    result.accounted_bytes = 0U;
    return result;
  };
  for (std::size_t ordinal = 0U; ordinal < message->field_count; ++ordinal) {
    const auto view = *decoded.record.Field(ordinal);
    const auto meta = *compiled_.Field(view.FlatFieldIndex());
    UiFieldResult field;
    field.field_index = ordinal;
    field.id = meta.id;
    const auto physical = compiled_.ResolveFieldPhysical(view.FlatFieldIndex(), frame.size());
    if (physical.status != pae::PhysicalQueryStatus::OK || !physical.value) {
      result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
      result.fields.clear();
      return result;
    }
    // Preserve the legacy Binary table contract: its physical column derives the current range
    // from the owned description plus frame size instead of consuming an action-result range.
    switch (view.Kind()) {
      case pae::ValueKind::UINT64:
        if (!view.UInt64()) return fail_materialization();
        field.raw_value = field.logical_value = std::to_string(*view.UInt64());
        break;
      case pae::ValueKind::INT64:
        if (!view.Int64()) return fail_materialization();
        field.raw_value = field.logical_value = std::to_string(*view.Int64());
        break;
      case pae::ValueKind::BOOL:
        if (!view.Bool()) return fail_materialization();
        field.raw_value = *view.Bool() ? "1" : "0";
        field.logical_value = *view.Bool() ? "true" : "false";
        break;
      case pae::ValueKind::BYTES: {
        const auto bytes = view.Bytes();
        if (!bytes || (bytes->size != 0U && bytes->data == nullptr))
          return fail_materialization();
        field.raw_value = field.logical_value = Hex(bytes->data, bytes->size);
        break;
      }
      case pae::ValueKind::ENUM: {
        const auto raw = view.EnumRawValue();
        if (!raw) return fail_materialization();
        field.raw_value = std::to_string(*raw);
        field.logical_value = field.raw_value;
        if (view.KnownEnumFlatIndex()) {
          const auto entry = compiled_.Enum(*view.KnownEnumFlatIndex());
          if (!entry || entry->field_flat_index != meta.flat_index || entry->raw_value != *raw)
            return fail_materialization();
          field.logical_value = entry->id;
        }
        break;
      }
      case pae::ValueKind::DECIMAL64: {
        const auto decimal = view.Decimal();
        const auto raw_kind = view.ConversionRawKind();
        if (!decimal || !raw_kind) return fail_materialization();
        if (*raw_kind == pae::RawIntegerKind::UINT64) {
          const auto raw = view.ConversionRawUInt64();
          if (!raw) return fail_materialization();
          field.raw_value = std::to_string(*raw);
        } else {
          const auto raw = view.ConversionRawInt64();
          if (!raw) return fail_materialization();
          field.raw_value = std::to_string(*raw);
        }
        field.logical_value.clear();
        break;
      }
    }
    result.fields.push_back(std::move(field));
  }
  result.local_status = PublicLegacyLocalStatus::OK;
  result.accounted_bytes = preflight;
  return result;
}

PublicLegacyOperation PublicLegacyCompleteAdapter::Decode(
    std::size_t pipeline_index, const std::vector<std::uint8_t>& frame) noexcept {
  PublicLegacyOperation result;
  if (frame.empty() || frame.size() > limits_.max_frame_bytes) {
    result.local_status = frame.size() > limits_.max_frame_bytes
                              ? PublicLegacyLocalStatus::RESOURCE_LIMIT
                              : PublicLegacyLocalStatus::INVALID_INPUT;
    return result;
  }
  try {
    const auto decoded = codec_->Decode(pipeline_index, {frame.data(), frame.size()});
    return Materialize(decoded, frame, false);
  } catch (const std::bad_alloc&) {
    result.local_status = PublicLegacyLocalStatus::ALLOCATION_FAILED;
  } catch (...) {
    result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
  }
  return result;
}

bool PublicLegacyCompleteAdapter::EncodeAvailable(std::size_t pipeline_index,
                                                  std::size_t message_index) const noexcept {
  const auto execution = compiled_.PipelineMessageExecution(pipeline_index, message_index);
  return execution && execution->encode_available &&
         execution->encode_output_size_kind != pae::EncodeOutputSizeKind::NOT_AVAILABLE;
}

bool PublicLegacyCompleteAdapter::DecodeAvailable(std::size_t pipeline_index) const noexcept {
  const auto pipeline = compiled_.Pipeline(pipeline_index);
  if (!pipeline) return false;
  for (std::size_t association = 0U; association < pipeline->message_count; ++association) {
    const auto message = compiled_.PipelineMessageIndex(pipeline_index, association);
    if (message) {
      const auto execution = compiled_.PipelineMessageExecution(pipeline_index, *message);
      if (execution && execution->decode_available) return true;
    }
  }
  return false;
}

PublicLegacyOperation PublicLegacyCompleteAdapter::Encode(
    std::size_t pipeline_index, std::size_t message_index,
    const std::vector<PublicLegacyInput>& inputs) noexcept {
  PublicLegacyOperation result;
  if (inputs.size() > limits_.max_fields) {
    result.local_status = PublicLegacyLocalStatus::RESOURCE_LIMIT;
    return result;
  }
  try {
    std::vector<pae::EncodeValue> values;
    values.reserve(inputs.size());
    for (const auto& input : inputs) {
      const pae::FieldSelector field{message_index, input.field_index};
      std::visit(
          [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::uint64_t>)
              values.push_back(pae::EncodeValue::UInt64(field, value));
            else if constexpr (std::is_same_v<T, std::int64_t>)
              values.push_back(pae::EncodeValue::Int64(field, value));
            else if constexpr (std::is_same_v<T, bool>)
              values.push_back(pae::EncodeValue::Bool(field, value));
            else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>)
              values.push_back(pae::EncodeValue::Bytes(field, {value.data(), value.size()}));
            else if constexpr (std::is_same_v<T, EnumSelection>)
              values.push_back(pae::EncodeValue::Enum(
                  {message_index, input.field_index, value.entry_index}));
            else if constexpr (std::is_same_v<T, Decimal64>)
              values.push_back(pae::EncodeValue::Decimal(
                  field, {value.coefficient, value.scale}));
          },
          input.value);
    }
    const auto execution = compiled_.PipelineMessageExecution(pipeline_index, message_index);
    if (!execution || !execution->encode_available ||
        execution->encode_output_size_kind == pae::EncodeOutputSizeKind::NOT_AVAILABLE ||
        execution->encode_output_size > limits_.max_frame_bytes) {
      result.local_status = PublicLegacyLocalStatus::INVALID_INPUT;
      return result;
    }
    std::vector<std::uint8_t> output(execution->encode_output_size);
    const auto encoded = codec_->Encode(pipeline_index, message_index, values.data(), values.size(),
                                        {output.data(), output.size()});
    result.codec_called = true;
    result.codec_status = encoded.status;
    result.failed_value_index = encoded.failed_value_index;
    result.conversion_error = encoded.conversion_error;
    if (encoded.failed_field_flat_index) {
      const auto field = compiled_.Field(*encoded.failed_field_flat_index);
      if (field) {
        result.failed_field_index = field->index_in_message;
        result.failed_field_id = field->id;
      }
    }
    if (encoded.status != pae::CodecStatus::OK || encoded.bytes_written > output.size()) {
      result.local_status = PublicLegacyLocalStatus::CODEC_FAILED;
      return result;
    }
    output.resize(encoded.bytes_written);
    const auto reviewed = review_codec_->Decode(pipeline_index, {output.data(), output.size()});
    result = Materialize(reviewed, output, true);
    result.codec_called = true;
    result.codec_status = encoded.status;
    result.review_decode_called = true;
    result.review_status = reviewed.status;
    if (result.ok() && result.message_index != message_index) {
      result.local_status = PublicLegacyLocalStatus::REVIEW_FAILED;
      result.fields.clear();
    }
    return result;
  } catch (const std::bad_alloc&) {
    result.local_status = PublicLegacyLocalStatus::ALLOCATION_FAILED;
  } catch (...) {
    result.local_status = PublicLegacyLocalStatus::MATERIALIZATION_FAILED;
  }
  return result;
}

}  // namespace pae::protocol_lab_ui
