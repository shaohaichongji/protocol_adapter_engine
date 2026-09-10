#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../../src/config_compiler/ui_description.h"
#include "../../src/protocol_plan/plan_bundle.h"

namespace pae::protocol_lab_ui {

struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t uint8_mask = 0U;
};

struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};

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
  std::string read_only_annotation;
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
  std::optional<ByteRange> integrity_storage;
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
};

struct DocumentDescription {
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

std::string FormatPhysicalLocation(const FieldDescriptor& field);

}  // namespace pae::protocol_lab_ui
