#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace pae {

enum class ValueKind { UINT64, INT64, BOOL, BYTES, ENUM, DECIMAL64 };

enum class EncodeValueSource { CALLER_INPUT, CONSTANT, COMPUTED, NOT_REFERENCED };

enum class EncodeOutputSizeKind { NOT_AVAILABLE, EXACT, UPPER_BOUND };

enum class RecordRepresentation { BINARY, ASCII_TEXT };

enum class PhysicalQueryStatus {
  OK,
  INVALID_COMPILED_PROTOCOL,
  INDEX_OUT_OF_RANGE,
  FRAME_SIZE_MISMATCH,
  REPRESENTATION_NOT_SUPPORTED,
  INTERNAL_CONTRACT_VIOLATION,
};

enum class FieldPhysicalKind { BYTE_RANGE, BIT_MASKS };

enum class AsciiAction { DECODE, ENCODE };
enum class AsciiSegmentKind { LITERAL, FIELD };
enum class AsciiQueryStatus {
  OK,
  INVALID_COMPILED_PROTOCOL,
  INDEX_OUT_OF_RANGE,
  INVALID_SELECTOR,
  REPRESENTATION_NOT_SUPPORTED,
  ACTION_NOT_AVAILABLE,
  INTERNAL_CONTRACT_VIOLATION,
};

struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};

struct ByteLengthBounds {
  std::size_t minimum = 0U;
  std::size_t maximum = 0U;
};

// Literal bytes borrow the CompiledProtocol's frozen state. Copy them before the owner is moved,
// replaced, or destroyed. The explicit size preserves embedded NUL bytes.
struct AsciiLiteralView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

struct AsciiActionDescription {
  std::size_t message_index = 0U;
  AsciiAction action = AsciiAction::DECODE;
  ByteLengthBounds record_length;
  std::size_t segment_count = 0U;
};

struct AsciiSegmentDescription {
  std::size_t message_index = 0U;
  AsciiAction action = AsciiAction::DECODE;
  std::size_t ordinal = 0U;
  AsciiSegmentKind kind = AsciiSegmentKind::LITERAL;
  std::optional<AsciiLiteralView> literal;
  std::optional<std::size_t> field_index;
  std::optional<std::size_t> flat_field_index;
};

struct AsciiFieldDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  ByteLengthBounds byte_length;
  bool decode_referenced = false;
  bool encode_referenced = false;
};

struct AsciiActionQueryResult {
  AsciiQueryStatus status = AsciiQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<AsciiActionDescription> value;
};

struct AsciiSegmentQueryResult {
  AsciiQueryStatus status = AsciiQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<AsciiSegmentDescription> value;
};

struct AsciiFieldQueryResult {
  AsciiQueryStatus status = AsciiQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<AsciiFieldDescription> value;
};

struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t mask = 0U;
};

inline constexpr std::size_t kMaximumFieldPhysicalBitMasks = 8U;

// Physical descriptions are copied values from the immutable compiled Plan. They do not prove
// that any frame matched, passed integrity checks, or decoded successfully. Indices are meaningful
// only for the CompiledProtocol that produced them.
struct MessagePhysicalDescription {
  std::size_t message_index = 0U;
  ByteLengthBounds record_length;
  std::optional<ByteRange> maximum_integrity_storage;
  bool integrity_storage_offset_depends_on_frame_size = false;
  std::optional<ByteRange> computed_length_storage;
};

struct ResolvedMessagePhysicalDescription {
  std::size_t message_index = 0U;
  std::size_t frame_size = 0U;
  std::optional<ByteRange> integrity_storage;
  std::optional<ByteRange> computed_length_storage;
};

struct FieldPhysicalDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  FieldPhysicalKind physical_kind = FieldPhysicalKind::BYTE_RANGE;
  std::optional<ByteRange> maximum_byte_range;
  std::optional<ByteLengthBounds> byte_value_length;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
  bool byte_range_length_depends_on_frame_size = false;
};

struct ResolvedFieldPhysicalDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  FieldPhysicalKind physical_kind = FieldPhysicalKind::BYTE_RANGE;
  std::optional<ByteRange> byte_range;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
};

struct MessageRepresentationQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<RecordRepresentation> value;
};

struct MessagePhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<MessagePhysicalDescription> value;
};

struct ResolvedMessagePhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<ResolvedMessagePhysicalDescription> value;
};

struct FieldPhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<FieldPhysicalDescription> value;
};

struct ResolvedFieldPhysicalQueryResult {
  PhysicalQueryStatus status = PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<ResolvedFieldPhysicalDescription> value;
};

struct ProtocolDescription {
  std::string_view schema_version;
  std::string_view id;
  std::string_view version;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
};

struct PipelineDescription {
  std::size_t index = 0U;
  std::string_view id;
  std::string_view direction_id;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
  std::size_t message_count = 0U;
};

struct MessageDescription {
  std::size_t index = 0U;
  std::string_view id;
  std::string_view direction_id;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
  std::size_t field_begin = 0U;
  std::size_t field_count = 0U;
};

struct FieldDescription {
  std::size_t flat_index = 0U;
  std::size_t message_index = 0U;
  std::size_t index_in_message = 0U;
  std::string_view id;
  std::string_view display_name;
  std::string_view description;
  std::string_view source_ref;
  ValueKind value_kind = ValueKind::UINT64;
  EncodeValueSource encode_value_source = EncodeValueSource::NOT_REFERENCED;
  std::size_t enum_begin = 0U;
  std::size_t enum_count = 0U;
};

struct MessageExecutionDescription {
  std::size_t pipeline_index = 0U;
  std::size_t message_index = 0U;
  bool decode_available = false;
  bool encode_available = false;
  EncodeOutputSizeKind encode_output_size_kind = EncodeOutputSizeKind::NOT_AVAILABLE;
  std::size_t encode_output_size = 0U;
};

struct EnumDescription {
  std::size_t flat_index = 0U;
  std::size_t field_flat_index = 0U;
  std::size_t index_in_field = 0U;
  std::string_view id;
  std::string_view display_name;
  std::uint64_t raw_value = 0U;
};

struct CompileMemoryReport {
  std::size_t plan_accounted_bytes = 0U;
  std::size_t metadata_accounted_bytes = 0U;
  std::size_t metadata_allocation_count = 0U;
  std::size_t facade_allocation_bytes = 0U;
};

}  // namespace pae
