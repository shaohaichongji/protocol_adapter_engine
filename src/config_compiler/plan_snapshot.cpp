#include <array>
#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>

#include "config_compiler.h"

namespace pae::config_compiler {
namespace {

void AppendQuoted(std::string_view value, std::string& output) {
  constexpr char kHex[] = "0123456789ABCDEF";
  output.push_back('"');
  for (const unsigned char byte : value) {
    switch (byte) {
      case '"':
        output.append("\\\"");
        break;
      case '\\':
        output.append("\\\\");
        break;
      case '\b':
        output.append("\\b");
        break;
      case '\f':
        output.append("\\f");
        break;
      case '\n':
        output.append("\\n");
        break;
      case '\r':
        output.append("\\r");
        break;
      case '\t':
        output.append("\\t");
        break;
      default:
        if (byte < 0x20U) {
          output.append("\\u00");
          output.push_back(kHex[(byte >> 4U) & 0x0FU]);
          output.push_back(kHex[byte & 0x0FU]);
        } else {
          output.push_back(static_cast<char>(byte));
        }
        break;
    }
  }
  output.push_back('"');
}

template <typename Integer>
void AppendInteger(Integer value, std::string& output) {
  std::array<char, 32U> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (result.ec == std::errc{}) {
    output.append(buffer.data(), result.ptr);
  }
}

std::string_view ToString(ResourceProfile value) noexcept {
  switch (value) {
    case ResourceProfile::DESKTOP:
      return "desktop";
    case ResourceProfile::CONSTRAINED:
      return "constrained";
  }
  return "invalid";
}

std::string_view ToString(InputKind value) noexcept {
  switch (value) {
    case InputKind::COMPLETE_RECORD:
      return "complete_record";
  }
  return "invalid";
}

std::string_view ToString(MatcherKind value) noexcept {
  switch (value) {
    case MatcherKind::FRAME_LENGTH_EQUALS:
      return "frame_length_equals";
    case MatcherKind::FIXED_BYTES:
      return "fixed_bytes";
  }
  return "invalid";
}

std::string_view ToString(ValueType value) noexcept {
  switch (value) {
    case ValueType::UINT64:
      return "UINT64";
    case ValueType::INT64:
      return "INT64";
    case ValueType::BYTES:
      return "BYTES";
    case ValueType::ENUM:
      return "ENUM";
    case ValueType::BOOL:
      return "BOOL";
  }
  return "invalid";
}

std::string_view ToString(WireCodec value) noexcept {
  switch (value) {
    case WireCodec::UNSIGNED_INTEGER:
      return "unsigned_integer";
    case WireCodec::BYTES:
      return "bytes";
    case WireCodec::BITFIELD:
      return "bitfield";
  }
  return "invalid";
}

std::string_view ToString(BitNumbering value) noexcept {
  return value == BitNumbering::LSB0 ? "lsb0" : "msb0";
}

std::string_view ToString(ByteOrder value) noexcept {
  switch (value) {
    case ByteOrder::NOT_APPLICABLE:
      return "not_applicable";
    case ByteOrder::BIG:
      return "big_endian";
    case ByteOrder::LITTLE:
      return "little_endian";
  }
  return "invalid";
}

std::string_view ToString(EncodeSource value) noexcept {
  switch (value) {
    case EncodeSource::INPUT:
      return "input";
    case EncodeSource::CONSTANT:
      return "constant";
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    case EncodeSource::COMPUTED:
      return "computed";
#endif
  }
  return "invalid";
}

std::string_view ToString(UnknownEnumPolicy value) noexcept {
  switch (value) {
    case UnknownEnumPolicy::REJECT:
      return "reject";
    case UnknownEnumPolicy::PRESERVE:
      return "preserve";
  }
  return "invalid";
}

void AppendStringProperty(std::string_view name, std::string_view value, std::string& output) {
  AppendQuoted(name, output);
  output.push_back(':');
  AppendQuoted(value, output);
}

template <typename Integer>
void AppendIntegerProperty(std::string_view name, Integer value, std::string& output) {
  AppendQuoted(name, output);
  output.push_back(':');
  AppendInteger(value, output);
}

template <typename ByteSequence>
void AppendHexBytes(const ByteSequence& bytes, std::string& output) {
  constexpr char kHex[] = "0123456789ABCDEF";
  output.push_back('"');
  for (std::size_t index = 0U; index < bytes.size(); ++index) {
    if (index != 0U) {
      output.push_back(' ');
    }
    const std::uint8_t byte = bytes[index];
    output.push_back(kHex[(byte >> 4U) & 0x0FU]);
    output.push_back(kHex[byte & 0x0FU]);
  }
  output.push_back('"');
}

}  // namespace

std::string MakeDeterministicPlanSnapshot(const PlanBundle& plan) {
  std::string output;
  output.reserve(1024U);
  output.push_back('{');
  const bool v02 = plan.SchemaVersion() == "0.2";
  const bool v03 = plan.SchemaVersion() == "0.3";
  const bool v04 = plan.SchemaVersion() == "0.4";
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  const bool v05 = plan.SchemaVersion() == "0.5";
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  const bool v06 = plan.SchemaVersion() == "0.6";
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  const bool v07 = plan.SchemaVersion() == "0.7";
#else
  const bool v07 = false;
#endif
#else
  const bool v06 = false;
  const bool v07 = false;
#endif
#else
  const bool v05 = false;
  const bool v06 = false;
  const bool v07 = false;
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  const bool v08 = plan.SchemaVersion() == "0.8";
#else
  const bool v08 = false;
#endif
  AppendStringProperty("snapshot_format",
                       v08   ? "pae_plan_bundle_v0.8_bounded_variable_slice"
                       : v07 ? "pae_plan_bundle_v0.7_length_slice"
                       : v06 ? "pae_plan_bundle_v0.6_crc_slice"
                       : v05 ? "pae_plan_bundle_v0.5_decimal_compiler_slice"
                       : v04 ? "pae_plan_bundle_v0.4_int64_slice"
                             : (v03 ? "pae_plan_bundle_v0.3_sum8_slice"
                                    : (v02 ? "pae_plan_bundle_v0.2_bitfield_slice"
                                           : "pae_plan_bundle_v0.1_draft_slice")),
                       output);
  output.push_back(',');
  AppendStringProperty("schema_version", plan.SchemaVersion(), output);
  output.push_back(',');
  AppendStringProperty("protocol_id", plan.ProtocolId(), output);
  output.push_back(',');
  AppendStringProperty("protocol_version", plan.ProtocolVersion(), output);
  output.push_back(',');
  AppendStringProperty("resource_profile", ToString(plan.GetResourceProfile()), output);

  const ResourceRequirements& requirements = plan.GetResourceRequirements();
  output.append(",\"resource_requirements\":{");
  AppendIntegerProperty("max_frame_bytes", requirements.max_frame_bytes, output);
  output.push_back(',');
  AppendIntegerProperty("framing_profile_count", requirements.framing_profile_count, output);
  output.push_back(',');
  AppendIntegerProperty("pipeline_count", requirements.pipeline_count, output);
  output.push_back(',');
  AppendIntegerProperty("message_count", requirements.message_count, output);
  output.push_back(',');
  AppendIntegerProperty("total_field_count", requirements.total_field_count, output);
  output.push_back(',');
  AppendIntegerProperty("total_matcher_count", requirements.total_matcher_count, output);
  output.push_back(',');
  AppendIntegerProperty("total_enum_entry_count", requirements.total_enum_entry_count, output);
  if (v02 || v03 || v04 || v05 || v06 || v07 || v08) {
    output.push_back(',');
    AppendIntegerProperty("total_bit_container_count", requirements.total_bit_container_count,
                          output);
  }
  if (v03 || v04 || v05 || v06 || v07 || v08) {
    output.push_back(',');
    AppendIntegerProperty("total_integrity_rule_count", requirements.total_integrity_rule_count,
                          output);
  }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (v05 || v06 || v07 || v08) {
    output.push_back(',');
    AppendIntegerProperty("total_conversion_count", requirements.total_conversion_count, output);
  }
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (v07 || v08) {
    output.push_back(',');
    AppendIntegerProperty("total_computed_length_count", requirements.total_computed_length_count,
                          output);
  }
#endif
  output.push_back('}');

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (v05 || v06 || v07 || v08) {
    output.append(",\"conversions\":[");
    const auto& conversions = plan.Conversions();
    for (std::size_t index = 0U; index < conversions.size(); ++index) {
      if (index != 0U) output.push_back(',');
      const auto& conversion = conversions[index];
      output.push_back('{');
      AppendStringProperty("raw_value_type", ToString(conversion.raw_value_type), output);
      output.push_back(',');
      AppendIntegerProperty("scale_numerator", conversion.scale_numerator, output);
      output.push_back(',');
      AppendIntegerProperty("scale_denominator", conversion.scale_denominator, output);
      output.push_back(',');
      AppendIntegerProperty("bias_numerator", conversion.bias_numerator, output);
      output.push_back(',');
      AppendIntegerProperty("bias_denominator", conversion.bias_denominator, output);
      output.push_back(',');
      AppendIntegerProperty("decimal_places", conversion.decimal_places, output);
      const auto append_coefficient = [&output](std::string_view name,
                                                const protocol_plan::SignedCoefficient256& value) {
        output.push_back(',');
        AppendQuoted(name, output);
        output.append(":{\"negative\":");
        output.append(value.negative ? "true" : "false");
        output.append(",\"words\":[");
        for (std::size_t word = 0U; word < value.words.size(); ++word) {
          if (word != 0U) output.push_back(',');
          AppendInteger(value.words[word], output);
        }
        output.append("]}");
      };
      append_coefficient("scale_coefficient", conversion.scale_coefficient);
      append_coefficient("bias_coefficient", conversion.bias_coefficient);
      output.push_back('}');
    }
    output.push_back(']');
  }
#endif

  output.append(",\"framing_profiles\":[");
  const auto& framing_profiles = plan.FramingProfiles();
  for (std::size_t framing_index = 0U; framing_index < framing_profiles.size(); ++framing_index) {
    if (framing_index != 0U) {
      output.push_back(',');
    }
    const protocol_plan::FrozenFramingPlan& framing = framing_profiles[framing_index];
    output.push_back('{');
    AppendStringProperty("id", framing.id, output);
    output.push_back(',');
    AppendStringProperty("input_kind", ToString(framing.input_kind), output);
    output.push_back('}');
  }
  output.push_back(']');

  const auto& messages = plan.Messages();
  output.append(",\"pipelines\":[");
  const auto& pipelines = plan.Pipelines();
  for (std::size_t pipeline_index = 0U; pipeline_index < pipelines.size(); ++pipeline_index) {
    if (pipeline_index != 0U) {
      output.push_back(',');
    }
    const protocol_plan::FrozenPipelinePlan& pipeline = pipelines[pipeline_index];
    output.push_back('{');
    AppendStringProperty("id", pipeline.id, output);
    output.push_back(',');
    AppendStringProperty("direction_id", pipeline.direction_id, output);
    output.push_back(',');
    AppendStringProperty("framing_profile_id", framing_profiles[pipeline.framing_profile_index].id,
                         output);
    output.append(",\"message_ids\":[");
    for (std::size_t index = 0U; index < pipeline.message_indices.size(); ++index) {
      if (index != 0U) {
        output.push_back(',');
      }
      AppendQuoted(messages[pipeline.message_indices[index]].id, output);
    }
    output.append("]}");
  }
  output.push_back(']');

  output.append(",\"messages\":[");
  for (std::size_t message_index = 0U; message_index < messages.size(); ++message_index) {
    if (message_index != 0U) {
      output.push_back(',');
    }
    const protocol_plan::FrozenMessagePlan& message = messages[message_index];
    output.push_back('{');
    AppendStringProperty("id", message.id, output);
    output.push_back(',');
    AppendStringProperty("direction_id", message.direction_id, output);
    output.push_back(',');
    AppendIntegerProperty("frame_length_bytes", message.frame_length_bytes, output);
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (v08) {
      output.append(",\"bounded_payload\":");
      if (!message.bounded_payload.has_value()) {
        output.append("null");
      } else {
        const auto& bounded = *message.bounded_payload;
        output.push_back('{');
        AppendIntegerProperty("header_length", bounded.header_length, output);
        output.push_back(',');
        AppendIntegerProperty("payload_field_index", bounded.payload_field_index, output);
        output.push_back(',');
        AppendIntegerProperty("min_payload_length", bounded.min_payload_length, output);
        output.push_back(',');
        AppendIntegerProperty("max_payload_length", bounded.max_payload_length, output);
        output.push_back(',');
        AppendIntegerProperty("trailer_length", bounded.trailer_length, output);
        output.push_back(',');
        AppendIntegerProperty("min_frame_length", bounded.min_frame_length, output);
        output.push_back(',');
        AppendIntegerProperty("max_frame_length", bounded.max_frame_length, output);
        output.push_back('}');
      }
    }
#endif

    output.append(",\"matchers\":[");
    for (std::size_t matcher_index = 0U; matcher_index < message.matchers.size(); ++matcher_index) {
      if (matcher_index != 0U) {
        output.push_back(',');
      }
      const protocol_plan::FrozenMatcherPlan& matcher = message.matchers[matcher_index];
      output.push_back('{');
      AppendStringProperty("kind", ToString(matcher.kind), output);
      if (matcher.kind == MatcherKind::FRAME_LENGTH_EQUALS) {
        output.push_back(',');
        AppendIntegerProperty("length_bytes", matcher.length_bytes, output);
      } else {
        output.push_back(',');
        AppendIntegerProperty("byte_offset", matcher.byte_offset, output);
        output.append(",\"bytes\":");
        AppendHexBytes(matcher.bytes, output);
      }
      output.push_back('}');
    }
    output.push_back(']');

    if (v02 || v03 || v04 || v05 || v06 || v07 || v08) {
      output.append(",\"bit_containers\":[");
      for (std::size_t index = 0U; index < message.bit_containers.size(); ++index) {
        if (index != 0U) output.push_back(',');
        const auto& container = message.bit_containers[index];
        output.push_back('{');
        AppendStringProperty("id", container.id, output);
        output.push_back(',');
        AppendIntegerProperty("byte_offset", container.byte_offset, output);
        output.push_back(',');
        AppendIntegerProperty("byte_width", container.byte_width, output);
        output.push_back(',');
        AppendStringProperty("byte_order", ToString(container.byte_order), output);
        output.push_back(',');
        AppendStringProperty("bit_numbering", ToString(container.bit_numbering), output);
        output.push_back(',');
        AppendIntegerProperty("base_value", container.base_value, output);
        output.push_back('}');
      }
      output.push_back(']');
    }

    if (v03 || v04 || v05 || v06 || v07 || v08) {
      output.append(",\"integrity\":");
      if (!message.integrity.has_value()) {
        output.append("null");
      } else {
        output.push_back('{');
        AppendStringProperty(
            "algorithm",
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
            message.integrity->algorithm == protocol_plan::IntegrityAlgorithm::CRC ? "crc" : "sum8",
#else
            "sum8",
#endif
            output);
        output.push_back(',');
        AppendIntegerProperty("range_offset", message.integrity->range_offset, output);
        output.push_back(',');
        AppendIntegerProperty("range_length", message.integrity->range_length, output);
        output.push_back(',');
        AppendIntegerProperty("storage_offset", message.integrity->storage_offset, output);
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
        if (v08) {
          output.append(",\"range_ends_at_payload\":");
          output.append(message.integrity->range_ends_at_payload ? "true" : "false");
          output.append(",\"storage_at_payload_end\":");
          output.append(message.integrity->storage_at_payload_end ? "true" : "false");
        }
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
        if (message.integrity->algorithm == protocol_plan::IntegrityAlgorithm::CRC) {
          output.push_back(',');
          AppendIntegerProperty("crc_width", message.integrity->crc_width, output);
          output.push_back(',');
          AppendIntegerProperty("crc_polynomial", message.integrity->crc_polynomial, output);
          output.push_back(',');
          AppendIntegerProperty("crc_initial_value", message.integrity->crc_initial_value, output);
          output.push_back(',');
          AppendIntegerProperty("crc_xor_output", message.integrity->crc_xor_output, output);
          output.append(",\"crc_reflect_input\":");
          output.append(message.integrity->crc_reflect_input ? "true" : "false");
          output.append(",\"crc_reflect_output\":");
          output.append(message.integrity->crc_reflect_output ? "true" : "false");
          output.push_back(',');
          AppendStringProperty("storage_byte_order",
                               ToString(message.integrity->storage_byte_order), output);
        }
#endif
        output.push_back('}');
      }
    }

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (v07 || v08) {
      output.append(",\"computed_length\":");
      if (!message.computed_length.has_value()) {
        output.append("null");
      } else {
        const auto& computed = *message.computed_length;
        output.push_back('{');
        AppendIntegerProperty("field_index", computed.field_index, output);
        output.push_back(',');
        AppendIntegerProperty("storage_offset", computed.storage_offset, output);
        output.push_back(',');
        AppendIntegerProperty("storage_width", computed.storage_width, output);
        output.push_back(',');
        AppendStringProperty("byte_order", ToString(computed.byte_order), output);
        output.push_back(',');
        std::string_view scope =
            computed.scope == protocol_plan::ComputedLengthScope::FRAME ? "frame" : "region";
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
        if (computed.scope == protocol_plan::ComputedLengthScope::PAYLOAD) scope = "payload";
#endif
        AppendStringProperty("scope", scope, output);
        output.push_back(',');
        AppendIntegerProperty("range_offset", computed.range_offset, output);
        output.push_back(',');
        AppendIntegerProperty("range_length", computed.range_length, output);
        output.push_back(',');
        AppendIntegerProperty("expected_value", computed.expected_value, output);
        output.push_back('}');
      }
    }
#endif

    output.append(",\"fields\":[");
    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      if (field_index != 0U) {
        output.push_back(',');
      }
      const protocol_plan::FrozenFieldPlan& field = message.fields[field_index];
      output.push_back('{');
      AppendStringProperty("id", field.id, output);
      output.push_back(',');
      AppendStringProperty("value_type", ToString(field.value_type), output);
      output.push_back(',');
      AppendStringProperty("wire_codec", ToString(field.wire_codec), output);
      output.push_back(',');
      if (field.wire_codec == WireCodec::BITFIELD) {
        AppendIntegerProperty("bit_container_index", field.bit_container_index, output);
        output.push_back(',');
        AppendIntegerProperty("bit_offset", field.bit_offset, output);
        output.push_back(',');
        AppendIntegerProperty("bit_width", field.bit_width, output);
      } else {
        AppendIntegerProperty("byte_offset", field.byte_offset, output);
        output.push_back(',');
        AppendIntegerProperty("byte_width", field.byte_width, output);
        output.push_back(',');
        AppendStringProperty("byte_order", ToString(field.byte_order), output);
      }
      output.push_back(',');
      AppendStringProperty("encode_source", ToString(field.encode_source), output);
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      if ((v05 || v06 || v07 || v08) &&
          field.conversion_index != (std::numeric_limits<std::size_t>::max)()) {
        output.push_back(',');
        AppendIntegerProperty("conversion_index", field.conversion_index, output);
      }
#endif
      if (field.constant_value.has_value()) {
        output.push_back(',');
        AppendIntegerProperty("constant_value", *field.constant_value, output);
      }
      if (field.signed_constant_value.has_value()) {
        output.push_back(',');
        AppendIntegerProperty("constant_value", *field.signed_constant_value, output);
      }
      if (field.value_type == ValueType::ENUM) {
        output.push_back(',');
        AppendStringProperty("unknown_enum_policy", ToString(field.unknown_enum_policy), output);
        output.append(",\"enum_entries\":[");
        for (std::size_t enum_index = 0U; enum_index < field.enum_entries.size(); ++enum_index) {
          if (enum_index != 0U) {
            output.push_back(',');
          }
          const protocol_plan::FrozenEnumEntryPlan& entry = field.enum_entries[enum_index];
          output.push_back('{');
          AppendStringProperty("id", entry.id, output);
          output.push_back(',');
          AppendIntegerProperty("raw_value", entry.raw_value, output);
          output.push_back('}');
        }
        output.push_back(']');
      }
      output.push_back('}');
    }
    output.append("]}");
  }
  output.append("]}");
  return output;
}

}  // namespace pae::config_compiler
