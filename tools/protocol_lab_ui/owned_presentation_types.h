#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ui_physical_types.h"

namespace pae::protocol_lab_ui {

enum class DocumentLayout {
  BINARY,
  ASCII_TEXT,
};

enum class FieldValueType {
  UINT64,
  INT64,
  BYTES,
  ENUM,
  BOOL,
};

enum class FieldWireCodec {
  UNSIGNED_INTEGER,
  BYTES,
  BITFIELD,
  ASCII_TEXT,
};

enum class FieldByteOrder {
  NOT_APPLICABLE,
  BIG,
  LITTLE,
};

enum class FieldEncodeSource {
  INPUT,
  CONSTANT,
  COMPUTED,
  NOT_REFERENCED,
};

enum class StreamInputKind {
  COMPLETE_RECORD,
  STREAM_CHUNK,
};

enum class StreamFramingStrategy {
  COMPLETE_RECORD,
  FIXED_LENGTH,
  SYNC_FIXED_LENGTH,
  SYNC_LENGTH_FIELD,
  ASCII_CRLF,
};

enum class StreamRuntimePhase {
  COLLECTING,
  DELIVERY_PENDING,
  DISCARDING_UNTIL_CRLF,
};

struct StreamPresentationObservation {
  StreamFramingStrategy strategy = StreamFramingStrategy::COMPLETE_RECORD;
  StreamRuntimePhase phase = StreamRuntimePhase::COLLECTING;
  std::size_t maximum_candidate_frame_bytes = 0U;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
  std::size_t buffered_bytes = 0U;
  std::size_t frozen_input_bytes = 0U;
  std::size_t frozen_cursor = 0U;
  bool has_internal_work = false;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_decode_failures = 0U;
  std::uint64_t total_observer_callbacks = 0U;
  std::uint64_t total_business_callbacks = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
};

struct Decimal64 {
  std::int64_t coefficient = 0;
  std::int32_t scale = 0;
};

inline bool operator==(const Decimal64& left, const Decimal64& right) noexcept {
  return left.coefficient == right.coefficient && left.scale == right.scale;
}

struct EnumSelection {
  std::size_t entry_index = 0U;
  std::string entry_id;
};

using TypedDraft = std::variant<std::uint64_t, std::int64_t, std::vector<std::uint8_t>,
                                EnumSelection, bool, Decimal64>;

struct BoundedPayloadDescriptor {
  std::size_t payload_field_index = 0U;
  std::size_t header_length = 0U;
  std::size_t min_payload_length = 0U;
  std::size_t max_payload_length = 0U;
  std::size_t trailer_length = 0U;
  std::size_t min_frame_length = 0U;
  std::size_t max_frame_length = 0U;
};

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
  FieldValueType value_type = FieldValueType::UINT64;
  FieldWireCodec wire_codec = FieldWireCodec::UNSIGNED_INTEGER;
  FieldByteOrder byte_order = FieldByteOrder::NOT_APPLICABLE;
  FieldEncodeSource encode_source = FieldEncodeSource::INPUT;
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
  bool decimal_conversion = false;
  bool decode_decimal64 = false;
  std::vector<std::uint8_t> allowed_control_bytes;
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
  std::optional<ByteRange> computed_length_storage;
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
  std::vector<std::size_t> encode_message_indices;
  StreamInputKind input_kind = StreamInputKind::COMPLETE_RECORD;
  StreamFramingStrategy framing_strategy = StreamFramingStrategy::COMPLETE_RECORD;
  std::optional<std::size_t> maximum_candidate_frame_bytes;
  bool stream_ascii_crlf = false;
  std::size_t maximum_frame_length = 0U;
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

}  // namespace pae::protocol_lab_ui
