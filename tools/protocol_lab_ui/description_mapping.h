#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../../src/config_compiler/ui_description.h"
#include "../../src/protocol_plan/plan_bundle.h"
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
#include "../protocol_lab_ascii/ascii_offline_adapter.h"
#endif

namespace pae::protocol_lab_ui {

enum class DocumentLayout {
  BINARY,
  ASCII_TEXT,
};

struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t uint8_mask = 0U;
};

struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};

struct BoundedPayloadDescriptor {
  std::size_t payload_field_index = 0U;
  std::size_t header_length = 0U;
  std::size_t min_payload_length = 0U;
  std::size_t max_payload_length = 0U;
  std::size_t trailer_length = 0U;
  std::size_t min_frame_length = 0U;
  std::size_t max_frame_length = 0U;
};

struct ByteLengthBounds {
  std::size_t minimum = 0U;
  std::size_t maximum = 0U;
};

inline bool operator==(const ByteLengthBounds& left, const ByteLengthBounds& right) noexcept {
  return left.minimum == right.minimum && left.maximum == right.maximum;
}

inline bool operator==(const ByteRange& left, const ByteRange& right) noexcept {
  return left.offset == right.offset && left.length == right.length;
}

struct EnumDescriptor {
  std::size_t entry_index = 0U;
  std::string id;
  std::string display_name;
  std::uint64_t raw_value = 0U;
};

struct FieldDescriptor {
  std::size_t field_index = 0U;
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  protocol_plan::ValueType value_type = protocol_plan::ValueType::UINT64;
  protocol_plan::WireCodec wire_codec = protocol_plan::WireCodec::UNSIGNED_INTEGER;
  protocol_plan::ByteOrder byte_order = protocol_plan::ByteOrder::NOT_APPLICABLE;
  protocol_plan::EncodeSource encode_source = protocol_plan::EncodeSource::INPUT;
  std::size_t byte_offset = 0U;
  std::size_t byte_width = 0U;
  std::vector<EnumDescriptor> enum_entries;
  std::vector<PhysicalBitMask> physical_bits;
  std::optional<ByteRange> byte_range;
  std::optional<ByteLengthBounds> byte_length_bounds;
  std::string read_only_annotation;
  bool ascii_text = false;
  bool decode_referenced = false;
  bool encode_referenced = false;
  std::vector<std::uint8_t> allowed_control_bytes;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::optional<protocol_plan::LinearConversionDescriptor> conversion;
#endif
};

struct MessageDescriptor {
  std::size_t message_index = 0U;
  std::string id;
  std::string direction_id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::size_t frame_size = 0U;
  std::vector<FieldDescriptor> fields;
  std::optional<BoundedPayloadDescriptor> bounded_payload;
  std::optional<ByteRange> integrity_storage;
  bool integrity_range_ends_at_payload = false;
  bool integrity_storage_at_payload_end = false;
  bool encode_available = true;
  bool decode_available = true;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::optional<ByteRange> computed_length_storage;
#endif
};

struct PipelineDescriptor {
  std::size_t pipeline_index = 0U;
  std::string id;
  std::string direction_id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::vector<std::size_t> message_indices;
  std::vector<std::size_t> decode_message_indices;
};

struct DocumentDescription {
  DocumentLayout layout = DocumentLayout::BINARY;
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::size_t max_frame_bytes = 0U;
  std::vector<PipelineDescriptor> pipelines;
  std::vector<MessageDescriptor> messages;
};

bool BuildDocumentDescription(const protocol_plan::PlanBundle& plan,
                              const config_compiler::UiDescriptionSidecar& sidecar,
                              DocumentDescription& output, std::string& error);
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
bool BuildDocumentDescription(const protocol_lab::ascii::DocumentDescription& source,
                              DocumentDescription& output, std::string& error);
#endif

std::string FormatPhysicalLocation(const FieldDescriptor& field);
std::optional<ByteRange> ResolveActualFieldRange(const MessageDescriptor& message,
                                                 const FieldDescriptor& field,
                                                 std::size_t actual_frame_size) noexcept;
std::optional<ByteRange> ResolveActualIntegrityStorage(const MessageDescriptor& message,
                                                       std::size_t actual_frame_size) noexcept;
std::string FormatPhysicalLocation(const MessageDescriptor& message, const FieldDescriptor& field,
                                   std::optional<std::size_t> actual_frame_size);

}  // namespace pae::protocol_lab_ui
