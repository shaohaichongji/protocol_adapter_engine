#include <array>
#include <charconv>
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
    case ValueType::BYTES:
      return "BYTES";
    case ValueType::ENUM:
      return "ENUM";
  }
  return "invalid";
}

std::string_view ToString(WireCodec value) noexcept {
  switch (value) {
    case WireCodec::UNSIGNED_INTEGER:
      return "unsigned_integer";
    case WireCodec::BYTES:
      return "bytes";
  }
  return "invalid";
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
  AppendStringProperty("snapshot_format", "pae_plan_bundle_v0.1_draft_slice", output);
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
  output.push_back('}');

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
      AppendIntegerProperty("byte_offset", field.byte_offset, output);
      output.push_back(',');
      AppendIntegerProperty("byte_width", field.byte_width, output);
      output.push_back(',');
      AppendStringProperty("byte_order", ToString(field.byte_order), output);
      output.push_back(',');
      AppendStringProperty("encode_source", ToString(field.encode_source), output);
      if (field.constant_value.has_value()) {
        output.push_back(',');
        AppendIntegerProperty("constant_value", *field.constant_value, output);
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
