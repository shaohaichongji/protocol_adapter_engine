#include "binary_host_adapter.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <limits>
#include <sstream>
#include <stdexcept>
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
#include "public_binary_description.h"
#endif

namespace pae::protocol_lab_ui {
namespace {

using protocol_lab_binary::MaterializationError;

FieldValueType UiValueType(protocol_plan::ValueType value) noexcept {
  switch (value) {
    case protocol_plan::ValueType::UINT64: return FieldValueType::UINT64;
    case protocol_plan::ValueType::INT64: return FieldValueType::INT64;
    case protocol_plan::ValueType::BYTES: return FieldValueType::BYTES;
    case protocol_plan::ValueType::ENUM: return FieldValueType::ENUM;
    case protocol_plan::ValueType::BOOL: return FieldValueType::BOOL;
  }
  return FieldValueType::UINT64;
}

FieldWireCodec UiWireCodec(protocol_plan::WireCodec value) noexcept {
  switch (value) {
    case protocol_plan::WireCodec::UNSIGNED_INTEGER: return FieldWireCodec::UNSIGNED_INTEGER;
    case protocol_plan::WireCodec::BYTES: return FieldWireCodec::BYTES;
    case protocol_plan::WireCodec::BITFIELD: return FieldWireCodec::BITFIELD;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    case protocol_plan::WireCodec::ASCII_TEXT: return FieldWireCodec::ASCII_TEXT;
#endif
  }
  return FieldWireCodec::UNSIGNED_INTEGER;
}

FieldByteOrder UiByteOrder(protocol_plan::ByteOrder value) noexcept {
  switch (value) {
    case protocol_plan::ByteOrder::NOT_APPLICABLE: return FieldByteOrder::NOT_APPLICABLE;
    case protocol_plan::ByteOrder::BIG: return FieldByteOrder::BIG;
    case protocol_plan::ByteOrder::LITTLE: return FieldByteOrder::LITTLE;
  }
  return FieldByteOrder::NOT_APPLICABLE;
}

FieldEncodeSource UiEncodeSource(protocol_plan::EncodeSource value) noexcept {
  switch (value) {
    case protocol_plan::EncodeSource::INPUT: return FieldEncodeSource::INPUT;
    case protocol_plan::EncodeSource::CONSTANT: return FieldEncodeSource::CONSTANT;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    case protocol_plan::EncodeSource::COMPUTED: return FieldEncodeSource::COMPUTED;
#endif
  }
  return FieldEncodeSource::INPUT;
}

void Require(bool condition, const char* detail) {
  if (!condition) throw MaterializationError(detail);
}

std::size_t Add(std::size_t left, std::size_t right) {
  Require(right <= (std::numeric_limits<std::size_t>::max)() - left,
          "Binary UI accounting overflow");
  return left + right;
}

std::size_t Multiply(std::size_t left, std::size_t right) {
  Require(left == 0U || right <= (std::numeric_limits<std::size_t>::max)() / left,
          "Binary UI accounting overflow");
  return left * right;
}

void ChargeString(const std::string& value, std::size_t& total) {
  total = Add(total, Add(value.capacity(), 1U));
}

std::size_t StringCapacityUpperBound(std::size_t size) {
  const auto small_capacity = std::string{}.capacity();
#if defined(_MSC_VER)
  Require(small_capacity == 15U, "unsupported MSVC std::string layout");
#endif
  if (size <= small_capacity) return small_capacity;
  Require(size <= (std::numeric_limits<std::size_t>::max)() - small_capacity,
          "Binary UI string upper-bound overflow");
  // MSVC 14.29 rounds a first allocation from the 15-byte SSO buffer with `size | 15`.
  // The wider fallback also remains defensive on other standard-library implementations.
  return (std::max)(size | small_capacity, Add(size, size / 2U));
}

void ChargeDestinationStringUpper(std::string_view value, std::size_t& total) {
  total = Add(total, Add(StringCapacityUpperBound(value.size()), 1U));
}

std::size_t DescriptionCopyUpperBound(const protocol_lab_binary::OwnedDescription& source) {
  std::size_t total = sizeof(DocumentDescription);
  ChargeDestinationStringUpper(source.schema_version, total);
  ChargeDestinationStringUpper(source.protocol_id, total);
  ChargeDestinationStringUpper(source.protocol_version, total);
  ChargeDestinationStringUpper(source.display_name, total);
  ChargeDestinationStringUpper(source.description, total);
  ChargeDestinationStringUpper(source.source_ref, total);
  total = Add(total, Multiply(source.pipelines.size(), sizeof(PipelineDescriptor)));
  total = Add(total, Multiply(source.messages.size(), sizeof(MessageDescriptor)));
  for (const auto& pipeline : source.pipelines) {
    ChargeDestinationStringUpper(pipeline.id, total);
    ChargeDestinationStringUpper(pipeline.direction_id, total);
    ChargeDestinationStringUpper(pipeline.display_name, total);
    ChargeDestinationStringUpper(pipeline.description, total);
    ChargeDestinationStringUpper(pipeline.source_ref, total);
    total = Add(total, Multiply(pipeline.message_indices.size(), sizeof(std::size_t)));
    total = Add(total, Multiply(pipeline.message_indices.size(), sizeof(std::size_t)));
  }
  for (const auto& message : source.messages) {
    ChargeDestinationStringUpper(message.id, total);
    ChargeDestinationStringUpper(message.direction_id, total);
    ChargeDestinationStringUpper(message.display_name, total);
    ChargeDestinationStringUpper(message.description, total);
    ChargeDestinationStringUpper(message.source_ref, total);
    total = Add(total, Multiply(message.fields.size(), sizeof(FieldDescriptor)));
    for (const auto& field : message.fields) {
      ChargeDestinationStringUpper(field.id, total);
      ChargeDestinationStringUpper(field.display_name, total);
      ChargeDestinationStringUpper(field.description, total);
      ChargeDestinationStringUpper(field.source_ref, total);
      if (field.encode_source != protocol_plan::EncodeSource::INPUT)
        ChargeDestinationStringUpper("constant/computed; read-only", total);
      else
        ChargeDestinationStringUpper({}, total);
      total = Add(total, Multiply(field.physical_bits.size(), sizeof(PhysicalBitMask)));
      total = Add(total, Multiply(field.enum_entries.size(), sizeof(EnumDescriptor)));
      for (const auto& entry : field.enum_entries) {
        ChargeDestinationStringUpper(entry.id, total);
        ChargeDestinationStringUpper(entry.display_name, total);
      }
    }
  }
  return total;
}

std::size_t AccountDescription(const DocumentDescription& value) {
  std::size_t total = sizeof(DocumentDescription);
  ChargeString(value.schema_version, total);
  ChargeString(value.protocol_id, total);
  ChargeString(value.protocol_version, total);
  ChargeString(value.display_name, total);
  ChargeString(value.description, total);
  ChargeString(value.source_ref, total);
  total = Add(total, Multiply(value.pipelines.capacity(), sizeof(PipelineDescriptor)));
  total = Add(total, Multiply(value.messages.capacity(), sizeof(MessageDescriptor)));
  for (const auto& pipeline : value.pipelines) {
    ChargeString(pipeline.id, total);
    ChargeString(pipeline.direction_id, total);
    ChargeString(pipeline.display_name, total);
    ChargeString(pipeline.description, total);
    ChargeString(pipeline.source_ref, total);
    total = Add(total, Multiply(pipeline.message_indices.capacity(), sizeof(std::size_t)));
    total = Add(total, Multiply(pipeline.decode_message_indices.capacity(), sizeof(std::size_t)));
  }
  for (const auto& message : value.messages) {
    ChargeString(message.id, total);
    ChargeString(message.direction_id, total);
    ChargeString(message.display_name, total);
    ChargeString(message.description, total);
    ChargeString(message.source_ref, total);
    total = Add(total, Multiply(message.fields.capacity(), sizeof(FieldDescriptor)));
    for (const auto& field : message.fields) {
      ChargeString(field.id, total);
      ChargeString(field.display_name, total);
      ChargeString(field.description, total);
      ChargeString(field.source_ref, total);
      ChargeString(field.read_only_annotation, total);
      total = Add(total, Multiply(field.physical_bits.capacity(), sizeof(PhysicalBitMask)));
      total = Add(total, Multiply(field.enum_entries.capacity(), sizeof(EnumDescriptor)));
      for (const auto& entry : field.enum_entries) {
        ChargeString(entry.id, total);
        ChargeString(entry.display_name, total);
      }
    }
  }
  return total;
}

DocumentDescription MapDescription(const protocol_lab_binary::OwnedDescription& source,
                                   std::size_t copy_limit, const BinaryUiCopyControls& controls,
                                   std::size_t& upper_bound, std::size_t& accounted) {
  upper_bound = DescriptionCopyUpperBound(source);
  Require(upper_bound <= copy_limit && upper_bound <= controls.description_copy_limit,
          "Binary UI description copy preflight exceeded");
  if (controls.before_description_copy != nullptr)
    controls.before_description_copy(controls.context);
  DocumentDescription output;
  output.layout = DocumentLayout::BINARY;
  output.schema_version = source.schema_version;
  output.protocol_id = source.protocol_id;
  output.protocol_version = source.protocol_version;
  output.display_name = source.display_name;
  output.description = source.description;
  output.source_ref = source.source_ref;
  output.max_frame_bytes = source.max_frame_bytes;
  output.pipelines.reserve(source.pipelines.size());
  for (const auto& item : source.pipelines) {
    PipelineDescriptor pipeline;
    pipeline.pipeline_index = item.pipeline_index;
    pipeline.id = item.id;
    pipeline.direction_id = item.direction_id;
    pipeline.display_name = item.display_name;
    pipeline.description = item.description;
    pipeline.source_ref = item.source_ref;
    pipeline.message_indices.reserve(item.message_indices.size());
    pipeline.message_indices.insert(pipeline.message_indices.end(), item.message_indices.begin(),
                                    item.message_indices.end());
    pipeline.decode_message_indices.reserve(item.message_indices.size());
    pipeline.decode_message_indices.insert(pipeline.decode_message_indices.end(),
                                           item.message_indices.begin(),
                                           item.message_indices.end());
    output.pipelines.push_back(std::move(pipeline));
  }
  output.messages.reserve(source.messages.size());
  for (const auto& item : source.messages) {
    MessageDescriptor message;
    message.message_index = item.message_index;
    message.id = item.id;
    message.direction_id = item.direction_id;
    message.display_name = item.display_name;
    message.description = item.description;
    message.source_ref = item.source_ref;
    message.frame_size = item.frame_size;
    if (item.bounded_payload) {
      const auto& value = *item.bounded_payload;
      message.bounded_payload = BoundedPayloadDescriptor{
          value.payload_field_index, value.header_length,  value.min_payload_length,
          value.max_payload_length,  value.trailer_length, value.min_frame_length,
          value.max_frame_length};
    }
    if (item.integrity_storage)
      message.integrity_storage =
          ByteRange{item.integrity_storage->offset, item.integrity_storage->length};
    message.integrity_storage_at_payload_end = item.integrity_storage_at_payload_end;
    if (item.computed_length_storage)
      message.computed_length_storage =
          ByteRange{item.computed_length_storage->offset, item.computed_length_storage->length};
    message.fields.reserve(item.fields.size());
    for (const auto& value : item.fields) {
      FieldDescriptor field;
      field.field_index = value.field_index;
      field.id = value.id;
      field.display_name = value.display_name;
      field.description = value.description;
      field.source_ref = value.source_ref;
      field.value_type = UiValueType(value.value_type);
      field.wire_codec = UiWireCodec(value.wire_codec);
      field.byte_order = UiByteOrder(value.byte_order);
      field.encode_source = UiEncodeSource(value.encode_source);
      field.decimal_conversion = value.conversion.has_value();
      if (value.byte_range) {
        field.byte_range = ByteRange{value.byte_range->offset, value.byte_range->length};
        field.byte_offset = value.byte_range->offset;
        field.byte_width = value.byte_range->length;
      }
      field.physical_bits.reserve(value.physical_bits.size());
      for (const auto& bit : value.physical_bits)
        field.physical_bits.push_back({bit.frame_byte_index, bit.uint8_mask});
      field.enum_entries.reserve(value.enum_entries.size());
      for (const auto& entry : value.enum_entries)
        field.enum_entries.push_back(
            {entry.entry_index, entry.id, entry.display_name, entry.raw_value});
      if (message.bounded_payload &&
          message.bounded_payload->payload_field_index == field.field_index) {
        field.byte_length_bounds = ByteLengthBounds{message.bounded_payload->min_payload_length,
                                                    message.bounded_payload->max_payload_length};
      }
      if (field.encode_source != FieldEncodeSource::INPUT)
        field.read_only_annotation = "constant/computed; read-only";
      message.fields.push_back(std::move(field));
    }
    output.messages.push_back(std::move(message));
  }

  accounted = AccountDescription(output);
  return output;
}

std::string Hex(const std::vector<std::uint8_t>& bytes) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string output;
  output.resize(Multiply(bytes.size(), 2U));
  for (std::size_t index = 0U; index < bytes.size(); ++index) {
    output[index * 2U] = digits[bytes[index] >> 4U];
    output[index * 2U + 1U] = digits[bytes[index] & 0x0FU];
  }
  return output;
}

std::string CopyText(std::string_view value) {
  std::string output;
  output.reserve(value.size());
  output.append(value.data(), value.size());
  return output;
}

template <typename Integer>
std::string Number(Integer value) {
  char buffer[64];
  const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), value);
  Require(converted.ec == std::errc{}, "Binary UI numeric formatting failed");
  return CopyText({buffer, static_cast<std::size_t>(converted.ptr - buffer)});
}

std::string Decimal(std::int64_t coefficient, std::int32_t scale) {
  char left[64];
  char right[64];
  const auto left_result = std::to_chars(std::begin(left), std::end(left), coefficient);
  const auto right_result = std::to_chars(std::begin(right), std::end(right), scale);
  Require(left_result.ec == std::errc{} && right_result.ec == std::errc{},
          "Binary UI numeric formatting failed");
  const auto left_size = static_cast<std::size_t>(left_result.ptr - left);
  const auto right_size = static_cast<std::size_t>(right_result.ptr - right);
  std::string output;
  output.reserve(Add(Add(left_size, 1U), right_size));
  output.append(left, left_size);
  output.push_back('@');
  output.append(right, right_size);
  return output;
}

std::string KnownEnum(std::string_view display, std::string_view id) {
  std::string output;
  output.reserve(Add(Add(display.size(), id.size()), 3U));
  output.append(display.data(), display.size());
  output.append(" [");
  output.append(id.data(), id.size());
  output.push_back(']');
  return output;
}

template <typename Integer>
std::size_t DecimalCharacters(Integer value) noexcept {
  char buffer[64];
  const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), value);
  return converted.ec == std::errc{} ? static_cast<std::size_t>(converted.ptr - buffer) : 64U;
}

std::size_t ResultCopyUpperBound(const protocol_lab_binary::ObservedOperation& operation) {
  if (operation.candidate &&
      operation.candidate->value.decoded.status == protocol_core::CodecStatus::OK) {
    const auto& candidate = *operation.candidate;
    Require(candidate.value.fields.size() == candidate.presentation.size(),
            "Binary UI result presentation count mismatch");
    std::size_t total = sizeof(BinaryUiDecodeResult);
    total = Add(total, candidate.value.frame.size());
    total = Add(total, Multiply(candidate.value.fields.size(), sizeof(UiFieldResult)));
    ChargeDestinationStringUpper(candidate.value.message_id, total);
    for (std::size_t index = 0U; index < candidate.value.fields.size(); ++index) {
      const auto& value = candidate.value.fields[index];
      const auto& presentation = candidate.presentation[index];
      ChargeDestinationStringUpper(value.id, total);
      std::size_t raw = 0U;
      std::size_t logical = 0U;
      switch (value.kind) {
        case protocol_core::LogicalValueKind::UINT64:
          raw = logical = DecimalCharacters(value.uint64_value);
          break;
        case protocol_core::LogicalValueKind::INT64:
          raw = logical = DecimalCharacters(value.int64_value);
          break;
        case protocol_core::LogicalValueKind::BOOL:
          raw = std::string_view{"未单独提供"}.size();
          logical = value.bool_value ? 4U : 5U;
          break;
        case protocol_core::LogicalValueKind::ENUM: {
          raw = DecimalCharacters(value.enum_value.raw_value);
          if (value.enum_value.known) {
            const auto display = presentation.enum_display_text.empty()
                                     ? std::string_view{value.enum_value.item_id}
                                     : std::string_view{presentation.enum_display_text};
            logical = Add(Add(display.size(), value.enum_value.item_id.size()), 3U);
          } else {
            logical = 7U;
          }
          break;
        }
        case protocol_core::LogicalValueKind::BYTES:
          raw = logical = Multiply(value.bytes_value.size(), 2U);
          break;
        case protocol_core::LogicalValueKind::DECIMAL64:
          Require(value.raw_integer.has_value(), "Binary UI converted field has no observed raw");
          raw = value.raw_integer->kind == protocol_core::RawIntegerKind::UINT64
                    ? DecimalCharacters(value.raw_integer->uint64_value)
                    : DecimalCharacters(value.raw_integer->int64_value);
          logical = Add(Add(DecimalCharacters(value.decimal64_value.coefficient), 1U),
                        DecimalCharacters(value.decimal64_value.scale));
          break;
      }
      total = Add(total, Add(StringCapacityUpperBound(raw), 1U));
      total = Add(total, Add(StringCapacityUpperBound(logical), 1U));
    }
    return total;
  }
  std::size_t total = sizeof(BinaryUiDecodeFailure);
  if (operation.candidate) total = Add(total, operation.candidate->value.diagnostic_frame.size());
  return total;
}

UiFieldResult MapField(const protocol_lab_binary::FieldValue& value,
                       const protocol_lab_binary::FieldPresentation& presentation) {
  Require(value.index == presentation.field_index, "Binary UI field presentation mismatch");
  UiFieldResult output;
  output.field_index = value.index;
  output.id = value.id;
  if (presentation.byte_range)
    output.actual_range =
        ByteRange{presentation.byte_range->offset, presentation.byte_range->length};
  switch (value.kind) {
    case protocol_core::LogicalValueKind::UINT64:
      output.raw_value = Number(value.uint64_value);
      output.logical_value = output.raw_value;
      break;
    case protocol_core::LogicalValueKind::INT64:
      output.raw_value = Number(value.int64_value);
      output.logical_value = output.raw_value;
      break;
    case protocol_core::LogicalValueKind::BOOL:
      output.raw_value = "未单独提供";
      output.logical_value = value.bool_value ? "true" : "false";
      break;
    case protocol_core::LogicalValueKind::ENUM:
      output.raw_value = Number(value.enum_value.raw_value);
      output.logical_value = value.enum_value.known
                                 ? KnownEnum(presentation.enum_display_text.empty()
                                                 ? std::string_view{value.enum_value.item_id}
                                                 : std::string_view{presentation.enum_display_text},
                                             value.enum_value.item_id)
                                 : CopyText("unknown");
      break;
    case protocol_core::LogicalValueKind::BYTES:
      output.raw_value = output.logical_value = Hex(value.bytes_value);
      break;
    case protocol_core::LogicalValueKind::DECIMAL64:
      Require(value.raw_integer.has_value(), "Binary UI converted field has no observed raw");
      output.raw_value = value.raw_integer->kind == protocol_core::RawIntegerKind::UINT64
                             ? Number(value.raw_integer->uint64_value)
                             : Number(value.raw_integer->int64_value);
      output.logical_value =
          Decimal(value.decimal64_value.coefficient, value.decimal64_value.scale);
      break;
  }
  return output;
}

std::size_t Account(BinaryUiDecodeResult& value) {
  std::size_t total = sizeof(BinaryUiDecodeResult);
  total = Add(total, value.frame.capacity());
  total = Add(total, Multiply(value.fields.capacity(), sizeof(UiFieldResult)));
  ChargeString(value.message_id, total);
  for (const auto& field : value.fields) {
    ChargeString(field.id, total);
    ChargeString(field.raw_value, total);
    ChargeString(field.logical_value, total);
  }
  return total;
}

}  // namespace

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
const char* PublicBinaryCodecStatusName(pae::CodecStatus status) noexcept {
#define PAE_CODEC_CASE(name) case pae::CodecStatus::name: return #name
  switch (status) {
    PAE_CODEC_CASE(OK);
    PAE_CODEC_CASE(INVALID_ARGUMENT);
    PAE_CODEC_CASE(INVALID_COMPILED_PROTOCOL);
    PAE_CODEC_CASE(RESOURCE_LIMIT_EXCEEDED);
    PAE_CODEC_CASE(ALLOCATION_FAILED);
    PAE_CODEC_CASE(WORKSPACE_BUSY);
    PAE_CODEC_CASE(INPUT_VALUES_TOO_MANY);
    PAE_CODEC_CASE(UNKNOWN_MESSAGE);
    PAE_CODEC_CASE(AMBIGUOUS_MESSAGE);
    PAE_CODEC_CASE(OUTPUT_SLOTS_TOO_SMALL);
    PAE_CODEC_CASE(INTEGRITY_FAILED);
    PAE_CODEC_CASE(MESSAGE_NOT_ALLOWED);
    PAE_CODEC_CASE(FIELD_REFERENCE_MISMATCH);
    PAE_CODEC_CASE(DUPLICATE_FIELD);
    PAE_CODEC_CASE(MISSING_FIELD);
    PAE_CODEC_CASE(TYPE_MISMATCH);
    PAE_CODEC_CASE(VALUE_NOT_REPRESENTABLE);
    PAE_CODEC_CASE(BYTES_LENGTH_MISMATCH);
    PAE_CODEC_CASE(UNKNOWN_ENUM_VALUE);
    PAE_CODEC_CASE(ENUM_REFERENCE_MISMATCH);
    PAE_CODEC_CASE(CONSTANT_FIELD_OVERRIDE);
    PAE_CODEC_CASE(INPUT_OUTPUT_OVERLAP);
    PAE_CODEC_CASE(BUFFER_TOO_SMALL);
    PAE_CODEC_CASE(FINAL_REVIEW_FAILED);
    PAE_CODEC_CASE(ASCII_CHARACTER_NOT_ALLOWED);
    PAE_CODEC_CASE(ASCII_TERMINATOR_CONFLICT);
    PAE_CODEC_CASE(OPERATION_NOT_SUPPORTED);
    PAE_CODEC_CASE(COMPUTED_FIELD_OVERRIDE);
    PAE_CODEC_CASE(LENGTH_MISMATCH);
    PAE_CODEC_CASE(INTERNAL_ERROR);
  }
#undef PAE_CODEC_CASE
  return "UNKNOWN_CODEC_STATUS";
}

std::unique_ptr<BinaryHostAdapter> BinaryHostAdapter::CreatePublic(
    pae::CompiledProtocol compiled, std::vector<BinaryHostBinding> bindings,
    BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
    std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
    std::string& error, const protocol_lab_binary::ResourceLimits& resource_limits) {
  try {
    Require(identity.document && identity.load && identity.session && identity.request &&
                !identity.config_sha256.empty(), "public Binary identity is incomplete");
    if (previous)
      Require(previous->identity_.document == identity.document &&
                  previous->identity_.load == identity.load &&
                  previous->identity_.session + 1U == identity.session &&
                  previous->identity_.config_sha256 == identity.config_sha256,
              "public Binary replacement identity is stale");
    Require(!bindings.empty() && bindings.size() <= 64U,
            "public Binary binding table is outside limits");
    auto adapter = std::unique_ptr<BinaryHostAdapter>(new BinaryHostAdapter);
    adapter->identity_ = std::move(identity);
    adapter->bindings_ = std::move(bindings);
    adapter->externally_retained_bytes_ = externally_retained_bytes;
    adapter->public_limits_.instance_bytes = resource_limits.instance_bytes;
    adapter->public_limits_.replacement_bytes = resource_limits.replacement_bytes;
    Require(BuildPublicBinaryDescription(compiled, adapter->description_, error),
            "public Binary description mapping failed");
    Require(adapter->description_.max_frame_bytes <= adapter->public_limits_.max_frame_bytes,
            "public Binary record exceeds H1 frame limit");
    std::vector<protocol_lab_binary::public_decode::Binding> public_bindings;
    public_bindings.reserve(adapter->bindings_.size());
    std::size_t channels = 0U;
    for (const auto& binding : adapter->bindings_) {
      Require(binding.action == host_endpoint::Action::DECODE &&
                  binding.streams > 0U && binding.streams <= 64U - channels,
              "public Binary H2 supports complete Decode bindings only");
      channels += binding.streams;
      public_bindings.push_back({binding.endpoint, binding.pipeline_id, binding.streams});
    }
    adapter->ui_description_bytes_ = Multiply(2U, AccountDescription(adapter->description_));
    adapter->ui_description_bytes_ = Add(
        adapter->ui_description_bytes_,
        Multiply(adapter->bindings_.capacity(), sizeof(BinaryHostBinding)));
    for (const auto& binding : adapter->bindings_) {
      ChargeString(binding.endpoint, adapter->ui_description_bytes_);
      ChargeString(binding.pipeline_id, adapter->ui_description_bytes_);
    }
    adapter->ui_view_reserve_bytes_ = 32U * 1024U * 1024U;
    adapter->hex_preview_reserve_bytes_ = 4U * 1024U * 1024U;
    const auto previous_public_bytes = previous && previous->public_owner_
                                           ? previous->public_owner_->InstanceAdmissionBytes()
                                           : 0U;
    auto prepared = protocol_lab_binary::public_decode::Adapter::AdoptCompiled(
        std::move(compiled), std::move(public_bindings), adapter->public_limits_,
        previous_public_bytes);
    Require(prepared.status == protocol_lab_binary::public_decode::LocalStatus::OK &&
                prepared.adapter != nullptr,
            "public Binary H1 adoption or Host binding failed");
    adapter->public_owner_ = std::move(prepared.adapter);
    adapter->public_drafts_.resize(channels);
    static std::atomic<std::uint64_t> next_instance{1U};
    adapter->public_instance_ = next_instance.fetch_add(1U);
    Require(adapter->public_instance_ != 0U, "public Binary instance generation exhausted");
    auto instance = adapter->AccountedInstanceBytes();
    Require(instance <= resource_limits.instance_bytes,
            "public Binary UI instance budget exceeded");
    auto peak = Add(instance, preparation_coexisting_bytes);
    if (previous) peak = Add(peak, previous->AccountedInstanceBytes());
    Require(peak <= (previous ? resource_limits.replacement_bytes : resource_limits.instance_bytes),
            "public Binary UI preparation/replacement budget exceeded");
    adapter->preparation_overhead_bytes_ = preparation_coexisting_bytes;
    error.clear();
    return adapter;
  } catch (const std::exception& exception) {
    if (error.empty()) error = exception.what();
    return nullptr;
  }
}
#endif

std::unique_ptr<BinaryHostAdapter> BinaryHostAdapter::Create(
    config_compiler::CompiledProtocolArtifacts artifacts, std::vector<BinaryHostBinding> bindings,
    BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
    std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
    std::string& error, const protocol_lab_binary::ResourceLimits& resource_limits,
    const BinaryUiCopyControls* copy_controls) {
  try {
    Require(identity.document != 0U && identity.load != 0U && identity.session != 0U &&
                identity.request != 0U && !identity.config_sha256.empty(),
            "Binary UI preparation identity is incomplete");
    if (previous) {
      Require(previous->identity_.session != (std::numeric_limits<std::uint64_t>::max)() &&
                  identity.document == previous->identity_.document &&
                  identity.load == previous->identity_.load &&
                  identity.session == previous->identity_.session + 1U &&
                  identity.config_sha256 == previous->identity_.config_sha256,
              "Binary UI replacement identity is stale or mismatched");
    }
    Require(!bindings.empty() && bindings.size() <= 64U,
            "Binary UI binding table is empty or too large");
    std::vector<host_endpoint::BindingSpec> specs;
    specs.reserve(bindings.size());
    for (const auto& binding : bindings) {
      Require(!binding.endpoint.empty() && !binding.pipeline_id.empty() && binding.streams > 0U,
              "Binary UI binding is incomplete");
      specs.push_back({binding.endpoint, binding.action, binding.pipeline_id, binding.streams});
    }
    auto adapter = std::unique_ptr<BinaryHostAdapter>(new BinaryHostAdapter);
    if (copy_controls != nullptr) adapter->copy_controls_ = *copy_controls;
    adapter->preparation_overhead_bytes_ =
        Multiply(specs.capacity(), sizeof(host_endpoint::BindingSpec));
    adapter->bindings_ = std::move(bindings);
    adapter->ui_description_bytes_ =
        Add(adapter->ui_description_bytes_,
            Multiply(adapter->bindings_.capacity(), sizeof(BinaryHostBinding)));
    for (const auto& binding : adapter->bindings_) {
      ChargeString(binding.endpoint, adapter->ui_description_bytes_);
      ChargeString(binding.pipeline_id, adapter->ui_description_bytes_);
    }
    adapter->identity_ = std::move(identity);
    adapter->externally_retained_bytes_ = externally_retained_bytes;
    const protocol_lab_binary::Revisions revisions{
        adapter->identity_.document, adapter->identity_.load, adapter->identity_.session};
    adapter->owner_ = protocol_lab_binary::PreparedBinary::Create(
        std::move(artifacts), specs.data(), specs.size(), revisions, {}, {}, resource_limits,
        previous ? previous->owner_.get() : nullptr);
    const auto& resources = adapter->owner_->Resources();
    std::size_t before_description = resources.replacement_peak_bytes;
    before_description = Add(before_description, adapter->ui_description_bytes_);
    before_description = Add(before_description, adapter->externally_retained_bytes_);
    before_description = Add(before_description, adapter->preparation_overhead_bytes_);
    before_description = Add(before_description, preparation_coexisting_bytes);
    if (previous) {
      before_description = Add(before_description, previous->ui_description_bytes_);
      before_description = Add(before_description, previous->externally_retained_bytes_);
    }
    const auto preparation_limit =
        previous ? resource_limits.replacement_bytes : resource_limits.instance_bytes;
    Require(before_description <= preparation_limit,
            "Binary UI pre-description preparation budget exceeded");
    std::size_t description_bytes = 0U;
    adapter->description_ = MapDescription(
        adapter->owner_->Description(), preparation_limit - before_description,
        adapter->copy_controls_, adapter->description_copy_upper_bound_bytes_, description_bytes);
    Require(description_bytes <= adapter->description_copy_upper_bound_bytes_,
            "Binary UI description capacity exceeded preflight upper bound");
    adapter->ui_description_bytes_ = Add(adapter->ui_description_bytes_, description_bytes);

    const protocol_lab_binary::ResourceLimits hard;
    Require(resource_limits.instance_bytes <= hard.instance_bytes &&
                resource_limits.replacement_bytes <= hard.replacement_bytes,
            "Binary UI resource limits exceed hard ceilings");
    std::size_t combined_peak = resources.replacement_peak_bytes;
    combined_peak = Add(combined_peak, adapter->ui_description_bytes_);
    combined_peak = Add(combined_peak, adapter->externally_retained_bytes_);
    combined_peak = Add(combined_peak, adapter->preparation_overhead_bytes_);
    combined_peak = Add(combined_peak, preparation_coexisting_bytes);
    if (previous) {
      combined_peak = Add(combined_peak, previous->ui_description_bytes_);
      combined_peak = Add(combined_peak, previous->externally_retained_bytes_);
    }
    Require(combined_peak <=
                (previous ? resource_limits.replacement_bytes : resource_limits.instance_bytes),
            "Binary UI preparation/replacement budget exceeded");
    Require(adapter->AccountedInstanceBytes() <= resource_limits.instance_bytes,
            "Binary UI instance budget exceeded");
    error.clear();
    return adapter;
  } catch (const std::exception& exception) {
    error = exception.what();
    return nullptr;
  }
}

std::size_t BinaryHostAdapter::AccountedInstanceBytes() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) {
    const auto owner_bytes = public_owner_->InstanceAdmissionBytes();
    if (ui_description_bytes_ > (std::numeric_limits<std::size_t>::max)() - owner_bytes)
      return (std::numeric_limits<std::size_t>::max)();
    const auto with_ui = owner_bytes + ui_description_bytes_;
    if (externally_retained_bytes_ > (std::numeric_limits<std::size_t>::max)() - with_ui)
      return (std::numeric_limits<std::size_t>::max)();
    const auto with_external = with_ui + externally_retained_bytes_;
    if (ui_view_reserve_bytes_ > (std::numeric_limits<std::size_t>::max)() - with_external ||
        hex_preview_reserve_bytes_ > (std::numeric_limits<std::size_t>::max)() -
                                         with_external - ui_view_reserve_bytes_)
      return (std::numeric_limits<std::size_t>::max)();
    return with_external + ui_view_reserve_bytes_ + hex_preview_reserve_bytes_;
  }
#endif
  const auto owner_bytes = owner_->Resources().instance_admission_bytes;
  if (ui_description_bytes_ > (std::numeric_limits<std::size_t>::max)() - owner_bytes)
    return (std::numeric_limits<std::size_t>::max)();
  const auto with_ui = owner_bytes + ui_description_bytes_;
  if (externally_retained_bytes_ > (std::numeric_limits<std::size_t>::max)() - with_ui)
    return (std::numeric_limits<std::size_t>::max)();
  return with_ui + externally_retained_bytes_;
}

std::size_t BinaryHostAdapter::AccountDescriptionBytes(const DocumentDescription& description) {
  return AccountDescription(description);
}

std::size_t BinaryHostAdapter::AccountedPreparationBytes() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) {
    const auto instance = AccountedInstanceBytes();
    return preparation_overhead_bytes_ > (std::numeric_limits<std::size_t>::max)() - instance
               ? (std::numeric_limits<std::size_t>::max)()
               : instance + preparation_overhead_bytes_;
  }
#endif
  const auto owner_bytes = owner_->Resources().preparation_peak_bytes;
  if (ui_description_bytes_ > (std::numeric_limits<std::size_t>::max)() - owner_bytes)
    return (std::numeric_limits<std::size_t>::max)();
  const auto with_ui = owner_bytes + ui_description_bytes_;
  if (externally_retained_bytes_ > (std::numeric_limits<std::size_t>::max)() - with_ui)
    return (std::numeric_limits<std::size_t>::max)();
  const auto with_external = with_ui + externally_retained_bytes_;
  if (preparation_overhead_bytes_ > (std::numeric_limits<std::size_t>::max)() - with_external)
    return (std::numeric_limits<std::size_t>::max)();
  return with_external + preparation_overhead_bytes_;
}

std::size_t BinaryHostAdapter::FlowCount(std::size_t binding) const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) return public_owner_->FlowCount(binding);
#endif
  return binding < bindings_.size() ? bindings_[binding].streams : 0U;
}

bool BinaryHostAdapter::IsCompleteDecode(std::size_t binding, std::size_t flow) const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) return binding < bindings_.size() &&
                            flow < public_owner_->FlowCount(binding) &&
                            bindings_[binding].action == host_endpoint::Action::DECODE;
#endif
  return binding < bindings_.size() && flow < bindings_[binding].streams &&
         bindings_[binding].action == host_endpoint::Action::DECODE &&
         !owner_->Observe(binding, flow).stream;
}

BinaryUiDecodeView BinaryHostAdapter::DecodeComplete(std::size_t binding, std::size_t flow,
                                                     const std::vector<std::uint8_t>& frame,
                                                     std::size_t active_view_bytes) {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) {
    if (!IsCompleteDecode(binding, flow)) {
      BinaryUiDecodeView view;
      view.public_host.status = pae::HostStatus::WRONG_INPUT_KIND;
      view.failure.emplace();
      return view;
    }
    const auto flat = public_owner_->FlowIndex(binding, flow);
    const auto& operation = public_owner_->Decode(
        flat, {frame.empty() ? nullptr : frame.data(), frame.size()});
    return MapPublic(operation, active_view_bytes);
  }
#endif
  if (!IsCompleteDecode(binding, flow)) {
    BinaryUiDecodeView view;
    view.host.status = host_endpoint::Status::WRONG_INPUT_KIND;
    view.failure.emplace().host_status = view.host.status;
    return view;
  }
  const auto& operation =
      owner_->Decode(binding, flow, {frame.empty() ? nullptr : frame.data(), frame.size()});
  return MapObserved(operation, binding, flow, active_view_bytes);
}

BinaryUiDecodeView BinaryHostAdapter::MapCurrent(std::size_t binding, std::size_t flow,
                                                 std::size_t active_view_bytes) const {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) {
    if (!IsCompleteDecode(binding, flow)) return {};
    const auto* state = public_owner_->State(public_owner_->FlowIndex(binding, flow));
    return state && (state->current.host.codec_attempted ||
                     state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
               ? MapPublic(state->current, active_view_bytes) : BinaryUiDecodeView{};
  }
#endif
  if (!IsCompleteDecode(binding, flow)) return {};
  const auto* operation = owner_->Current(binding, flow);
  return operation == nullptr ? BinaryUiDecodeView{}
                              : MapObserved(*operation, binding, flow, active_view_bytes);
}

BinaryUiDecodeView BinaryHostAdapter::MapObserved(
    const protocol_lab_binary::ObservedOperation& operation, std::size_t binding, std::size_t flow,
    std::size_t active_view_bytes) const {
  const auto upper_bound = ResultCopyUpperBound(operation);
  const auto mapped_view_budget = owner_->Resources().ui_view_reserve / 2U;
  Require(presentation_retained_bytes_ <= mapped_view_budget &&
              active_view_bytes <= mapped_view_budget - presentation_retained_bytes_ &&
              upper_bound <= mapped_view_budget - presentation_retained_bytes_ &&
              upper_bound <= copy_controls_.result_copy_limit,
          "Binary UI result copy preflight exceeded");
  if (copy_controls_.before_result_copy != nullptr)
    copy_controls_.before_result_copy(copy_controls_.context);
  BinaryUiDecodeView view;
  view.host = operation.host;
  if (operation.candidate &&
      operation.candidate->value.decoded.status == protocol_core::CodecStatus::OK) {
    const auto& candidate = *operation.candidate;
    Require(candidate.identity.revisions.tab == identity_.document &&
                candidate.identity.revisions.load == identity_.load &&
                candidate.identity.revisions.session == identity_.session &&
                candidate.identity.instance == owner_->Instance() &&
                candidate.identity.operation != 0U && candidate.identity.binding == binding &&
                candidate.identity.flow == flow &&
                candidate.identity.generation == operation.host.generation,
            "Binary UI result identity mismatch");
    auto& result = view.result.emplace();
    result.identity = candidate.identity;
    result.frame.reserve(candidate.value.frame.size());
    result.frame.insert(result.frame.end(), candidate.value.frame.begin(),
                        candidate.value.frame.end());
    result.message_index = candidate.value.message_index;
    result.message_id = candidate.value.message_id;
    Require(candidate.value.fields.size() == candidate.presentation.size(),
            "Binary UI result presentation count mismatch");
    result.fields.reserve(candidate.value.fields.size());
    for (std::size_t index = 0U; index < candidate.value.fields.size(); ++index)
      result.fields.push_back(
          MapField(candidate.value.fields[index], candidate.presentation[index]));
    result.accounted_bytes = Account(result);
    Require(result.accounted_bytes <= upper_bound &&
                result.accounted_bytes <= mapped_view_budget - presentation_retained_bytes_,
            "Binary UI active view budget exceeded");
    view.ok = operation.host.status == host_endpoint::Status::OK;
    return view;
  }
  auto& failure = view.failure.emplace();
  failure.host_status = operation.host.status;
  failure.codec_status = operation.host.codec_status;
  if (operation.candidate) {
    const auto& value = operation.candidate->value;
    failure.diagnostic_frame.reserve(value.diagnostic_frame.size());
    failure.diagnostic_frame.insert(failure.diagnostic_frame.end(), value.diagnostic_frame.begin(),
                                    value.diagnostic_frame.end());
    if (value.message_index != protocol_core::kInvalidIndex)
      failure.message_index = value.message_index;
    if (value.decoded.failed_field_index != protocol_core::kInvalidIndex)
      failure.failed_field_index = value.decoded.failed_field_index;
  }
  failure.accounted_bytes = sizeof(BinaryUiDecodeFailure) + failure.diagnostic_frame.capacity();
  Require(failure.accounted_bytes <= upper_bound &&
              failure.accounted_bytes <= mapped_view_budget - presentation_retained_bytes_,
          "Binary UI failure view budget exceeded");
  return view;
}

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
std::size_t BinaryHostAdapter::PublicResultCopyUpperBound(
    const protocol_lab_binary::public_decode::Operation& operation) const {
  const auto& candidate = operation.candidate;
  if (!candidate || !candidate->success || !candidate->message_index ||
      operation.host.status != pae::HostStatus::OK)
    return Add(sizeof(BinaryUiDecodeFailure),
               candidate ? candidate->frame.size() : 0U);
  Require(*candidate->message_index < description_.messages.size(),
          "public Binary result Message index mismatch");
  const auto& message = description_.messages[*candidate->message_index];
  Require(candidate->fields.size() == message.fields.size(),
          "public Binary result Field count mismatch");
  auto total = Add(sizeof(BinaryUiDecodeResult), candidate->frame.size());
  total = Add(total, Multiply(candidate->fields.size(), sizeof(UiFieldResult)));
  ChargeDestinationStringUpper(candidate->message_id, total);
  for (std::size_t i = 0U; i < candidate->fields.size(); ++i) {
    const auto& field = candidate->fields[i];
    ChargeDestinationStringUpper(field.id, total);
    std::size_t raw = 0U;
    std::size_t logical = 0U;
    switch (field.kind) {
      case pae::ValueKind::UINT64:
        raw = logical = DecimalCharacters(field.uint64_value); break;
      case pae::ValueKind::INT64:
        raw = logical = DecimalCharacters(field.int64_value); break;
      case pae::ValueKind::BOOL:
        raw = std::string_view{"未单独提供"}.size();
        logical = field.bool_value ? 4U : 5U; break;
      case pae::ValueKind::BYTES:
        raw = logical = Multiply(field.bytes.size(), 2U); break;
      case pae::ValueKind::ENUM: {
        raw = DecimalCharacters(*field.enum_raw);
        logical = 7U;  // unknown
        if (field.known_enum_flat_index) {
          const auto& entries = message.fields[i].enum_entries;
          const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& item) {
            return item.raw_value == *field.enum_raw;
          });
          Require(found != entries.end(), "public Binary known Enum mapping mismatch");
          logical = Add(Add(found->display_name.empty() ? found->id.size()
                                                      : found->display_name.size(),
                            found->id.size()), 3U);
        }
        break;
      }
      case pae::ValueKind::DECIMAL64:
        Require(field.conversion_raw_kind.has_value(),
                "public Binary Decimal64 result has no raw integer");
        raw = *field.conversion_raw_kind == pae::RawIntegerKind::UINT64
                  ? DecimalCharacters(*field.conversion_raw_uint64)
                  : DecimalCharacters(*field.conversion_raw_int64);
        logical = Add(Add(DecimalCharacters(field.decimal.coefficient), 1U),
                      DecimalCharacters(field.decimal.scale));
        break;
    }
    total = Add(total, Add(StringCapacityUpperBound(raw), 1U));
    total = Add(total, Add(StringCapacityUpperBound(logical), 1U));
  }
  return total;
}

BinaryUiDecodeView BinaryHostAdapter::MapPublic(
    const protocol_lab_binary::public_decode::Operation& operation,
    std::size_t active_view_bytes) const {
  const auto upper = PublicResultCopyUpperBound(operation);
  const auto budget = UiViewReserveBytes() / 2U;
  Require(presentation_retained_bytes_ <= budget &&
              active_view_bytes <= budget - presentation_retained_bytes_ &&
              upper <= budget - presentation_retained_bytes_ &&
              upper <= copy_controls_.result_copy_limit,
          "public Binary UI result copy preflight exceeded");
  if (copy_controls_.before_result_copy)
    copy_controls_.before_result_copy(copy_controls_.context);
  BinaryUiDecodeView view;
  view.public_host = operation.host;
  if (operation.host.status == pae::HostStatus::OK && operation.candidate &&
      operation.candidate->success && operation.candidate->message_index) {
    const auto& source = *operation.candidate;
    const auto& message = description_.messages[*source.message_index];
    auto& result = view.result.emplace();
    result.frame = source.frame;
    result.message_index = *source.message_index;
    result.message_id = source.message_id;
    result.fields.reserve(source.fields.size());
    for (std::size_t i = 0U; i < source.fields.size(); ++i) {
      const auto& source_field = source.fields[i];
      const auto& described = message.fields[i];
      Require(source_field.id == described.id, "public Binary Field identity mismatch");
      UiFieldResult field;
      field.field_index = i;
      field.id = source_field.id;
      if (source_field.byte_range)
        field.actual_range = ByteRange{source_field.byte_range->offset,
                                       source_field.byte_range->length};
      switch (source_field.kind) {
        case pae::ValueKind::UINT64:
          field.raw_value = Number(source_field.uint64_value);
          field.logical_value = field.raw_value; break;
        case pae::ValueKind::INT64:
          field.raw_value = Number(source_field.int64_value);
          field.logical_value = field.raw_value; break;
        case pae::ValueKind::BOOL:
          field.raw_value = "未单独提供";
          field.logical_value = source_field.bool_value ? "true" : "false"; break;
        case pae::ValueKind::BYTES:
          field.raw_value = field.logical_value = Hex(source_field.bytes); break;
        case pae::ValueKind::ENUM: {
          field.raw_value = Number(*source_field.enum_raw);
          field.logical_value = "unknown";
          if (source_field.known_enum_flat_index) {
            const auto found = std::find_if(described.enum_entries.begin(),
                                            described.enum_entries.end(),
                                            [&](const auto& item) {
                                              return item.raw_value == *source_field.enum_raw;
                                            });
            Require(found != described.enum_entries.end(),
                    "public Binary known Enum mapping mismatch");
            field.logical_value = KnownEnum(found->display_name.empty() ? found->id
                                                                     : found->display_name,
                                           found->id);
          }
          break;
        }
        case pae::ValueKind::DECIMAL64:
          Require(source_field.conversion_raw_kind.has_value(),
                  "public Binary Decimal64 result has no raw integer");
          field.raw_value = *source_field.conversion_raw_kind == pae::RawIntegerKind::UINT64
                                ? Number(*source_field.conversion_raw_uint64)
                                : Number(*source_field.conversion_raw_int64);
          field.logical_value = Decimal(source_field.decimal.coefficient,
                                        source_field.decimal.scale);
          break;
      }
      result.fields.push_back(std::move(field));
    }
    result.accounted_bytes = Account(result);
    Require(result.accounted_bytes <= upper &&
                result.accounted_bytes <= budget - presentation_retained_bytes_,
            "public Binary active UI view budget exceeded");
    view.ok = true;
    return view;
  }
  auto& failure = view.failure.emplace();
  if (operation.candidate) {
    failure.diagnostic_frame = operation.candidate->frame;
    failure.message_index = operation.candidate->message_index;
    // A failed flat Field index alone does not establish a Message identity.
  }
  failure.accounted_bytes = sizeof(BinaryUiDecodeFailure) + failure.diagnostic_frame.capacity();
  Require(failure.accounted_bytes <= upper &&
              failure.accounted_bytes <= budget - presentation_retained_bytes_,
          "public Binary failure UI view budget exceeded");
  return view;
}
#endif

std::size_t BinaryHostAdapter::CurrentResultCopyUpperBoundBytes(std::size_t binding,
                                                                std::size_t flow) const {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) {
    const auto* state = public_owner_->State(public_owner_->FlowIndex(binding, flow));
    return state && (state->current.host.codec_attempted ||
                     state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
               ? PublicResultCopyUpperBound(state->current) : 0U;
  }
#endif
  const auto* operation = owner_->Current(binding, flow);
  return operation == nullptr ? 0U : ResultCopyUpperBound(*operation);
}

bool BinaryHostAdapter::SetPresentationRetainedBytes(std::size_t bytes) noexcept {
  if (bytes > UiViewReserveBytes() / 2U) return false;
  presentation_retained_bytes_ = bytes;
  return true;
}

const void* BinaryHostAdapter::Current(std::size_t binding, std::size_t flow) const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (public_owner_) {
    const auto* state = public_owner_->State(public_owner_->FlowIndex(binding, flow));
    return state && (state->current.host.codec_attempted ||
                     state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
               ? static_cast<const void*>(&state->current) : nullptr;
  }
#endif
  return owner_->Current(binding, flow);
}

}  // namespace pae::protocol_lab_ui
