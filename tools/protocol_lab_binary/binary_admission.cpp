#include "binary_admission.h"

#include <string_view>

namespace pae::protocol_lab_binary {
namespace {
namespace plan = protocol_plan;
constexpr auto kInvalid = protocol_core::kInvalidIndex;
void Require(bool value, const char* message) {
  if (!value) throw MaterializationError(message);
}
void Charge(std::size_t amount, std::size_t limit, std::size_t& total) {
  Require(total <= limit && amount <= limit - total, "Binary admission budget exceeded");
  total += amount;
}
bool IntegerOrder(plan::ByteOrder order) {
  return order == plan::ByteOrder::BIG || order == plan::ByteOrder::LITTLE;
}
}  // namespace

void ValidateBinaryAdmission(const plan::PlanBundle& bundle, const Limits& limits) {
  const Limits hard;
  Require(limits.max_frame_bytes <= hard.max_frame_bytes && limits.max_fields <= hard.max_fields &&
              limits.max_field_bytes <= hard.max_field_bytes &&
              limits.max_string_bytes <= hard.max_string_bytes &&
              limits.max_total_bytes <= hard.max_total_bytes &&
              limits.max_identity_bytes <= hard.max_identity_bytes,
          "Binary admission limits exceed hard ceilings");
  Require(bundle.SchemaVersion() == "0.9", "Binary admission requires Schema 0.9");
  std::size_t identities = 0U, fields = 0U, bytes = 0U;
  const auto identity = [&](std::string_view value) {
    Require(value.size() <= limits.max_identity_bytes, "Binary admission identity too long");
    Charge(value.size(), limits.max_string_bytes, identities);
    Charge(1U, limits.max_string_bytes, identities);
  };
  identity(bundle.ProtocolId());
  identity(bundle.ProtocolVersion());
  Require(bundle.GetResourceRequirements().max_frame_bytes > 0U &&
              bundle.GetResourceRequirements().max_frame_bytes <= limits.max_frame_bytes,
          "Binary admission frame bound exceeded");
  for (const auto& profile : bundle.FramingProfiles()) {
    identity(profile.id.View());
    if (profile.input_kind == plan::InputKind::COMPLETE_RECORD) {
      Require(profile.strategy == plan::FramingStrategy::COMPLETE_RECORD,
              "Binary admission complete-record strategy mismatch");
    } else {
      Require(profile.input_kind == plan::InputKind::STREAM_CHUNK,
              "Binary admission unsupported input kind");
      switch (profile.strategy) {
        case plan::FramingStrategy::FIXED_LENGTH:
        case plan::FramingStrategy::SYNC_FIXED_LENGTH:
          Require(profile.frame_length_bytes > 0U &&
                      profile.frame_length_bytes <= limits.max_frame_bytes,
                  "Binary admission fixed stream frame bound exceeded");
          break;
        case plan::FramingStrategy::SYNC_LENGTH_FIELD:
          Require(profile.minimum_frame_length > 0U &&
                      profile.minimum_frame_length <= profile.maximum_frame_length &&
                      profile.maximum_frame_length <= limits.max_frame_bytes,
                  "Binary admission variable stream frame bound exceeded");
          break;
        default:
          throw MaterializationError("Binary admission unsupported framing strategy");
      }
    }
  }
  Require(bundle.MessageExecutionPlans().size() == bundle.Messages().size(),
          "Binary admission message execution mismatch");
  for (std::size_t i = 0U; i < bundle.Messages().size(); ++i) {
    const auto& message = bundle.Messages()[i];
    const auto& execution = bundle.MessageExecutionPlans()[i];
    identity(message.id.View());
    identity(message.direction_id.View());
    Charge(message.fields.size(), limits.max_fields, fields);
    Require(execution.frame_size > 0U && execution.frame_size <= limits.max_frame_bytes &&
                execution.fields.size() == message.fields.size(),
            "Binary admission message bounds mismatch");
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    Require(!execution.text_decode && !execution.text_encode,
            "Binary admission does not accept text actions");
#endif
    if (message.integrity) {
      Require(message.integrity->algorithm == plan::IntegrityAlgorithm::SUM8 ||
                  (message.integrity->algorithm == plan::IntegrityAlgorithm::CRC &&
                   (message.integrity->crc_width == 8U || message.integrity->crc_width == 16U ||
                    message.integrity->crc_width == 32U)),
              "Binary admission unsupported integrity");
    }
    for (const auto& container : message.bit_containers) identity(container.id.View());
    for (std::size_t j = 0U; j < message.fields.size(); ++j) {
      const auto& field = message.fields[j];
      const auto& resolved = execution.fields[j];
      identity(field.id.View());
      Require(
          field.value_type == resolved.value_type && field.encode_source == resolved.encode_source,
          "Binary admission field execution mismatch");
      switch (field.value_type) {
        case plan::ValueType::UINT64:
        case plan::ValueType::INT64:
        case plan::ValueType::BOOL:
        case plan::ValueType::ENUM:
          Require(
              (field.wire_codec == plan::WireCodec::UNSIGNED_INTEGER && resolved.width > 0U &&
               resolved.width <= 8U &&
               (IntegerOrder(field.byte_order) ||
                (resolved.width == 1U && field.byte_order == plan::ByteOrder::NOT_APPLICABLE))) ||
                  (field.wire_codec == plan::WireCodec::BITFIELD &&
                   field.value_type != plan::ValueType::INT64 &&
                   resolved.bit_container_index < execution.bit_containers.size()),
              "Binary admission unsupported scalar codec");
          break;
        case plan::ValueType::BYTES:
          Require(field.wire_codec == plan::WireCodec::BYTES &&
                      field.byte_order == plan::ByteOrder::NOT_APPLICABLE &&
                      field.encode_source == plan::EncodeSource::INPUT,
                  "Binary admission unsupported bytes codec");
          // The frozen execution width is the maximum for a bounded payload.
          Charge(resolved.width, limits.max_field_bytes, bytes);
          break;
        default:
          throw MaterializationError("Binary admission unsupported value type");
      }
      Require(field.encode_source == plan::EncodeSource::INPUT ||
                  field.encode_source == plan::EncodeSource::CONSTANT ||
                  (field.encode_source == plan::EncodeSource::COMPUTED &&
                   execution.computed_length.has_value() &&
                   field.value_type == plan::ValueType::UINT64 &&
                   field.wire_codec == plan::WireCodec::UNSIGNED_INTEGER),
              "Binary admission unsupported encode source");
      Require(field.conversion_index == resolved.conversion_index,
              "Binary admission conversion execution mismatch");
      if (field.conversion_index != kInvalid) {
        Require(field.conversion_index < bundle.Conversions().size() &&
                    (field.value_type == plan::ValueType::UINT64 ||
                     field.value_type == plan::ValueType::INT64) &&
                    field.wire_codec == plan::WireCodec::UNSIGNED_INTEGER &&
                    field.encode_source == plan::EncodeSource::INPUT,
                "Binary admission unsupported conversion");
        Require(bundle.Conversions()[field.conversion_index].raw_value_type == field.value_type,
                "Binary admission conversion raw type mismatch");
      }
      Require(field.unknown_enum_policy == plan::UnknownEnumPolicy::REJECT ||
                  field.unknown_enum_policy == plan::UnknownEnumPolicy::PRESERVE,
              "Binary admission unsupported enum policy");
      for (const auto& entry : field.enum_entries) identity(entry.id.View());
    }
  }
  for (const auto& pipeline : bundle.Pipelines()) {
    identity(pipeline.id.View());
    identity(pipeline.direction_id.View());
    Require(pipeline.framing_profile_index < bundle.FramingProfiles().size() &&
                !pipeline.message_indices.empty(),
            "Binary admission invalid pipeline");
    for (auto index : pipeline.message_indices)
      Require(index < bundle.Messages().size(), "Binary admission invalid pipeline message");
  }
}

}  // namespace pae::protocol_lab_binary
