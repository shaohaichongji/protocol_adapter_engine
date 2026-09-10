#include "description_mapping.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <string_view>

namespace pae::protocol_lab_ui {
namespace {

constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);

bool ToSize(std::uint64_t value, std::size_t& output) noexcept {
  if (value > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) return false;
  output = static_cast<std::size_t>(value);
  return true;
}

template <typename Span>
std::string CopyResolved(const config_compiler::UiDescriptionSidecar& sidecar, Span span) {
  return std::string{sidecar.Resolve(span)};
}

std::string BitSet(std::size_t byte_index, std::uint8_t mask, bool global) {
  std::ostringstream output;
  output << '{';
  bool first = true;
  for (std::size_t bit = 0U; bit < 8U; ++bit) {
    if ((mask & static_cast<std::uint8_t>(1U << bit)) == 0U) continue;
    if (!first) output << ',';
    output << (global ? byte_index * 8U + bit : bit);
    first = false;
  }
  output << '}';
  return output.str();
}

void MergeBit(std::vector<PhysicalBitMask>& output, std::size_t byte_index, std::uint8_t mask) {
  const auto found = std::lower_bound(
      output.begin(), output.end(), byte_index,
      [](const PhysicalBitMask& item, std::size_t value) { return item.frame_byte_index < value; });
  if (found != output.end() && found->frame_byte_index == byte_index) {
    found->uint8_mask = static_cast<std::uint8_t>(found->uint8_mask | mask);
    return;
  }
  output.insert(found, PhysicalBitMask{byte_index, mask});
}

bool BuildPhysicalMapping(const protocol_plan::FrozenFieldPlan& field,
                          const protocol_plan::FieldExecutionPlan& execution,
                          const protocol_plan::MessageExecutionPlan& message_execution,
                          FieldDescriptor& output, std::string& error) {
  output.byte_offset = execution.offset;
  output.byte_width = execution.width;
  if (execution.bit_container_index == kInvalidIndex) {
    output.byte_range = ByteRange{execution.offset, execution.width};
    return true;
  }
  if (execution.bit_container_index >= message_execution.bit_containers.size()) {
    error = "field bit container index is outside the execution Plan";
    return false;
  }
  const auto& container = message_execution.bit_containers[execution.bit_container_index];
  if (container.width == 0U || container.width > 8U) {
    error = "field bit container width is outside the physical mapper range";
    return false;
  }
  if (container.byte_order != protocol_plan::ByteOrder::LITTLE &&
      container.byte_order != protocol_plan::ByteOrder::BIG) {
    error = "bit container has no physical byte order";
    return false;
  }
  for (std::size_t numeric_bit = 0U; numeric_bit < 64U; ++numeric_bit) {
    const std::uint64_t bit = std::uint64_t{1U} << numeric_bit;
    if ((execution.bit_mask & bit) == 0U) continue;
    const std::size_t byte_in_container = numeric_bit / 8U;
    if (byte_in_container >= container.width) {
      error = "field bit mask exceeds its container width";
      return false;
    }
    const std::size_t physical_byte =
        container.byte_order == protocol_plan::ByteOrder::LITTLE
            ? container.offset + byte_in_container
            : container.offset + (container.width - 1U - byte_in_container);
    if (physical_byte >= message_execution.frame_size) {
      error = "field physical bit mapping exceeds frame size";
      return false;
    }
    MergeBit(output.physical_bits, physical_byte,
             static_cast<std::uint8_t>(1U << (numeric_bit % 8U)));
  }
  if (output.physical_bits.empty() || field.bit_width == 0U) {
    error = "bit field has an empty resolved physical mapping";
    return false;
  }
  return true;
}

}  // namespace

bool BuildDocumentDescription(const protocol_plan::PlanBundle& plan,
                              const config_compiler::UiDescriptionSidecar& sidecar,
                              DocumentDescription& output, std::string& error) {
  error.clear();
  DocumentDescription built;
  const auto pipeline_metadata = sidecar.Pipelines();
  const auto message_metadata = sidecar.Messages();
  const auto field_metadata = sidecar.Fields();
  const auto enum_metadata = sidecar.Enums();
  if (pipeline_metadata.size() != plan.Pipelines().size() ||
      message_metadata.size() != plan.Messages().size()) {
    error = "Plan and UI description top-level counts differ";
    return false;
  }

  built.schema_version = std::string{plan.SchemaVersion()};
  built.protocol_id = std::string{plan.ProtocolId()};
  built.protocol_version = std::string{plan.ProtocolVersion()};
  const auto& protocol_metadata = sidecar.Protocol();
  built.display_name = CopyResolved(sidecar, protocol_metadata.display_name);
  built.description = CopyResolved(sidecar, protocol_metadata.description);
  built.source_ref = CopyResolved(sidecar, protocol_metadata.source_ref);
  if (!ToSize(plan.GetResourceRequirements().max_frame_bytes, built.max_frame_bytes) ||
      built.max_frame_bytes == 0U) {
    error = "Plan has no representable positive max_frame_bytes";
    return false;
  }

  built.pipelines.reserve(plan.Pipelines().size());
  for (std::size_t pipeline_index = 0U; pipeline_index < plan.Pipelines().size();
       ++pipeline_index) {
    const auto& source = plan.Pipelines()[pipeline_index];
    const auto& metadata = pipeline_metadata[pipeline_index];
    PipelineDescriptor item;
    item.pipeline_index = pipeline_index;
    item.id = std::string{source.id.View()};
    item.direction_id = std::string{source.direction_id.View()};
    item.display_name = CopyResolved(sidecar, metadata.display_name);
    item.description = CopyResolved(sidecar, metadata.description);
    item.source_ref = CopyResolved(sidecar, metadata.source_ref);
    item.message_indices.assign(source.message_indices.begin(), source.message_indices.end());
    for (const std::size_t message_index : item.message_indices) {
      if (message_index >= plan.Messages().size()) {
        error = "pipeline message index is outside the Plan";
        return false;
      }
    }
    built.pipelines.push_back(std::move(item));
  }

  built.messages.reserve(plan.Messages().size());
  for (std::size_t message_index = 0U; message_index < plan.Messages().size(); ++message_index) {
    const auto& source = plan.Messages()[message_index];
    const auto& execution = plan.MessageExecutionPlans()[message_index];
    const auto& metadata = message_metadata[message_index];
    if (metadata.field_begin > field_metadata.size() ||
        metadata.field_count > field_metadata.size() - metadata.field_begin ||
        metadata.field_count != source.fields.size() ||
        execution.fields.size() != source.fields.size()) {
      error = "message field range does not match the Plan";
      return false;
    }
    MessageDescriptor item;
    item.message_index = message_index;
    item.id = std::string{source.id.View()};
    item.direction_id = std::string{source.direction_id.View()};
    item.display_name = CopyResolved(sidecar, metadata.display_name);
    item.description = CopyResolved(sidecar, metadata.description);
    item.source_ref = CopyResolved(sidecar, metadata.source_ref);
    item.frame_size = execution.frame_size;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (execution.bounded_payload.has_value()) {
      const auto& bounded = *execution.bounded_payload;
      BoundedPayloadDescriptor copied;
      copied.payload_field_index = bounded.payload_field_index;
      if (!ToSize(bounded.header_length, copied.header_length) ||
          !ToSize(bounded.min_payload_length, copied.min_payload_length) ||
          !ToSize(bounded.max_payload_length, copied.max_payload_length) ||
          !ToSize(bounded.trailer_length, copied.trailer_length) ||
          !ToSize(bounded.min_frame_length, copied.min_frame_length) ||
          !ToSize(bounded.max_frame_length, copied.max_frame_length)) {
        error = "bounded payload metadata is not representable";
        return false;
      }
      item.bounded_payload = copied;
      if (copied.payload_field_index >= source.fields.size() ||
          bounded.min_payload_length > bounded.max_payload_length ||
          bounded.min_frame_length > bounded.max_frame_length ||
          copied.max_frame_length != execution.frame_size) {
        error = "bounded payload metadata does not match the execution Plan";
        return false;
      }
    }
#endif
    if (source.integrity.has_value()) {
      std::size_t offset = 0U;
      if (!ToSize(source.integrity->storage_offset, offset)) {
        error = "integrity storage offset is not representable";
        return false;
      }
      const std::size_t width =
          source.integrity->algorithm == protocol_plan::IntegrityAlgorithm::SUM8
              ? 1U
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
              : static_cast<std::size_t>(source.integrity->crc_width / 8U);
#else
              : 0U;
#endif
      item.integrity_storage = ByteRange{offset, width};
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      item.integrity_range_ends_at_payload = source.integrity->range_ends_at_payload;
      item.integrity_storage_at_payload_end = source.integrity->storage_at_payload_end;
#endif
    }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (source.computed_length.has_value()) {
      std::size_t offset = 0U;
      std::size_t width = 0U;
      if (!ToSize(source.computed_length->storage_offset, offset) ||
          !ToSize(source.computed_length->storage_width, width)) {
        error = "computed length storage range is not representable";
        return false;
      }
      item.computed_length_storage = ByteRange{offset, width};
    }
#endif
    item.fields.reserve(source.fields.size());
    for (std::size_t field_index = 0U; field_index < source.fields.size(); ++field_index) {
      const auto& field = source.fields[field_index];
      const auto& field_execution = execution.fields[field_index];
      const auto& field_meta = field_metadata[metadata.field_begin + field_index];
      if (field_meta.enum_begin > enum_metadata.size() ||
          field_meta.enum_count > enum_metadata.size() - field_meta.enum_begin ||
          field_meta.enum_count != field.enum_entries.size()) {
        error = "field enum range does not match the Plan";
        return false;
      }
      FieldDescriptor field_item;
      field_item.field_index = field_index;
      field_item.id = std::string{field.id.View()};
      field_item.display_name = CopyResolved(sidecar, field_meta.display_name);
      field_item.description = CopyResolved(sidecar, field_meta.description);
      field_item.source_ref = CopyResolved(sidecar, field_meta.source_ref);
      field_item.value_type = field.value_type;
      field_item.wire_codec = field.wire_codec;
      field_item.byte_order = field.byte_order;
      field_item.encode_source = field.encode_source;
      field_item.enum_entries.reserve(field.enum_entries.size());
      for (std::size_t entry_index = 0U; entry_index < field.enum_entries.size(); ++entry_index) {
        const auto& entry = field.enum_entries[entry_index];
        field_item.enum_entries.push_back(EnumDescriptor{
            entry_index, std::string{entry.id.View()},
            CopyResolved(sidecar, enum_metadata[field_meta.enum_begin + entry_index].display_name),
            entry.raw_value});
      }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      if (field.conversion_index != kInvalidIndex) {
        if (field.conversion_index >= plan.Conversions().size()) {
          error = "field conversion index is outside the Plan";
          return false;
        }
        field_item.conversion = plan.Conversions()[field.conversion_index];
      }
#endif
      if (!BuildPhysicalMapping(field, field_execution, execution, field_item, error)) return false;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      if (item.bounded_payload.has_value() &&
          item.bounded_payload->payload_field_index == field_index) {
        field_item.byte_length_bounds = ByteLengthBounds{item.bounded_payload->min_payload_length,
                                                         item.bounded_payload->max_payload_length};
      }
#endif
      if (field.encode_source != protocol_plan::EncodeSource::INPUT) {
        std::ostringstream annotation;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
        if (field.encode_source == protocol_plan::EncodeSource::COMPUTED) {
          annotation << "computed length; read-only";
        } else
#endif
        {
          annotation << "constant; read-only";
        }
        field_item.read_only_annotation = annotation.str();
      }
      item.fields.push_back(std::move(field_item));
    }
    built.messages.push_back(std::move(item));
  }
  output = std::move(built);
  return true;
}

std::string FormatPhysicalLocation(const FieldDescriptor& field) {
  std::vector<PhysicalBitMask> masks = field.physical_bits;
  if (masks.empty() && field.byte_range.has_value()) {
    masks.reserve(field.byte_range->length);
    for (std::size_t index = 0U; index < field.byte_range->length; ++index) {
      masks.push_back(PhysicalBitMask{field.byte_range->offset + index, 0xFFU});
    }
  }
  std::ostringstream output;
  output << std::uppercase << std::hex;
  for (std::size_t index = 0U; index < masks.size(); ++index) {
    if (index != 0U) output << "; ";
    const auto& item = masks[index];
    output << std::dec << "byte[" << item.frame_byte_index << "] mask=0x" << std::hex;
    if (item.uint8_mask < 0x10U) output << '0';
    output << static_cast<unsigned int>(item.uint8_mask) << std::dec
           << " bits=" << BitSet(item.frame_byte_index, item.uint8_mask, false)
           << " global_bits=" << BitSet(item.frame_byte_index, item.uint8_mask, true);
  }
  return output.str();
}

std::optional<ByteRange> ResolveActualFieldRange(const MessageDescriptor& message,
                                                 const FieldDescriptor& field,
                                                 std::size_t actual_frame_size) noexcept {
  if (!message.bounded_payload.has_value() ||
      message.bounded_payload->payload_field_index != field.field_index) {
    return field.byte_range;
  }
  const auto& bounded = *message.bounded_payload;
  if (actual_frame_size < bounded.min_frame_length ||
      actual_frame_size > bounded.max_frame_length ||
      actual_frame_size < bounded.header_length + bounded.trailer_length) {
    return std::nullopt;
  }
  const std::size_t payload_length =
      actual_frame_size - bounded.header_length - bounded.trailer_length;
  if (payload_length < bounded.min_payload_length || payload_length > bounded.max_payload_length) {
    return std::nullopt;
  }
  return ByteRange{bounded.header_length, payload_length};
}

std::optional<ByteRange> ResolveActualIntegrityStorage(const MessageDescriptor& message,
                                                       std::size_t actual_frame_size) noexcept {
  if (!message.integrity_storage.has_value()) return std::nullopt;
  if (!message.integrity_storage_at_payload_end) return message.integrity_storage;
  if (!message.bounded_payload.has_value()) return std::nullopt;
  const auto& bounded = *message.bounded_payload;
  if (actual_frame_size < bounded.min_frame_length ||
      actual_frame_size > bounded.max_frame_length ||
      actual_frame_size < bounded.header_length + bounded.trailer_length) {
    return std::nullopt;
  }
  const std::size_t payload_length =
      actual_frame_size - bounded.header_length - bounded.trailer_length;
  if (payload_length < bounded.min_payload_length || payload_length > bounded.max_payload_length ||
      message.integrity_storage->length > bounded.trailer_length) {
    return std::nullopt;
  }
  return ByteRange{bounded.header_length + payload_length, message.integrity_storage->length};
}

std::string FormatPhysicalLocation(const MessageDescriptor& message, const FieldDescriptor& field,
                                   std::optional<std::size_t> actual_frame_size) {
  if (field.byte_length_bounds.has_value()) {
    if (!actual_frame_size.has_value()) {
      std::ostringstream output;
      output << "dynamic byte[" << field.byte_offset << "..], payload length "
             << field.byte_length_bounds->minimum << ".." << field.byte_length_bounds->maximum
             << " bytes; current range unavailable";
      return output.str();
    }
    const auto actual = ResolveActualFieldRange(message, field, *actual_frame_size);
    if (!actual.has_value()) return "current range unavailable";
    if (actual->length == 0U) {
      std::ostringstream output;
      output << "byte[" << actual->offset << "] length=0 (empty payload)";
      return output.str();
    }
    FieldDescriptor resolved = field;
    resolved.physical_bits.clear();
    resolved.byte_range = actual;
    return FormatPhysicalLocation(resolved);
  }
  return FormatPhysicalLocation(field);
}

}  // namespace pae::protocol_lab_ui
