#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../../src/config_compiler/config_compiler.h"
#include "candidate_materializer.h"

namespace pae::protocol_lab_binary {

struct DescriptionLimits {
  std::size_t max_description_bytes = 4U * 1024U * 1024U;
  std::size_t max_fields = 1024U;
  std::size_t max_frame_bytes = 65536U;
  std::size_t max_identity_bytes = 256U;
};
struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};
struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t uint8_mask = 0U;
};
struct EnumDescriptor {
  std::size_t entry_index = 0U;
  std::string id;
  std::string display_name;
  std::uint64_t raw_value = 0U;
};
struct FieldDescriptor {
  std::size_t field_index = 0U;
  std::string id, display_name, description, source_ref;
  protocol_plan::ValueType value_type = protocol_plan::ValueType::UINT64;
  protocol_plan::WireCodec wire_codec = protocol_plan::WireCodec::UNSIGNED_INTEGER;
  protocol_plan::ByteOrder byte_order = protocol_plan::ByteOrder::NOT_APPLICABLE;
  protocol_plan::EncodeSource encode_source = protocol_plan::EncodeSource::INPUT;
  std::optional<protocol_plan::LinearConversionDescriptor> conversion;
  std::optional<ByteRange> byte_range;
  std::vector<PhysicalBitMask> physical_bits;
  std::vector<EnumDescriptor> enum_entries;
};
struct BoundedPayloadDescriptor {
  std::size_t payload_field_index = 0U;
  std::size_t header_length = 0U, trailer_length = 0U;
  std::size_t min_payload_length = 0U, max_payload_length = 0U;
  std::size_t min_frame_length = 0U, max_frame_length = 0U;
};
struct MessageDescriptor {
  std::size_t message_index = 0U;
  std::string id, direction_id, display_name, description, source_ref;
  std::size_t frame_size = 0U;
  std::vector<FieldDescriptor> fields;
  std::optional<BoundedPayloadDescriptor> bounded_payload;
  std::optional<ByteRange> integrity_storage;
  bool integrity_storage_at_payload_end = false;
  std::optional<ByteRange> computed_length_storage;
};
struct PipelineDescriptor {
  std::size_t pipeline_index = 0U;
  std::string id, direction_id, display_name, description, source_ref;
  std::vector<std::size_t> message_indices;
};
struct OwnedDescription {
  std::string schema_version, protocol_id, protocol_version, display_name, description, source_ref;
  std::size_t max_frame_bytes = 0U;
  std::vector<PipelineDescriptor> pipelines;
  std::vector<MessageDescriptor> messages;
  // DTO object plus vector capacities and string capacity + terminator (including SSO).
  // Excludes input artifacts, allocator bookkeeping and caller-retained copies.
  std::size_t accounted_total_bytes = 0U;
};

// Schema 0.9 only. A single compiler artifact supplies both Plan and sidecar; no borrowed
// data survives this call. Preflight precedes copies; final capacity accounting precedes return.
[[nodiscard]] OwnedDescription BuildOwnedDescription(
    const config_compiler::CompiledProtocolArtifacts& artifacts,
    const DescriptionLimits& limits = {});
// Allocation-free frame-length prerequisite for consuming frozen physical_bits. Does not prove
// Core success or Candidate/description identity; the caller must establish both separately.
[[nodiscard]] bool IsActualFrameValid(const MessageDescriptor& message,
                                      std::size_t frame_size) noexcept;
// Call only for a successful Core result. Invalid frame size or descriptor/range returns nullopt;
// a bit field has no full-byte range. Zero payload resolves to an empty range, not a highlight.
[[nodiscard]] std::optional<ByteRange> ResolveActualFieldRange(const MessageDescriptor& message,
                                                               const FieldDescriptor& field,
                                                               std::size_t frame_size) noexcept;
[[nodiscard]] std::optional<ByteRange> ResolveActualIntegrityStorage(
    const MessageDescriptor& message, std::size_t frame_size) noexcept;

}  // namespace pae::protocol_lab_binary
