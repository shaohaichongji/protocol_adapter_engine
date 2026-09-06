#include "test_plan_factory.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

namespace pae::test_support {
namespace {

using protocol_plan::ByteOrder;
using protocol_plan::EncodeSource;
using protocol_plan::FieldPlan;
using protocol_plan::InputKind;
using protocol_plan::MatcherKind;
using protocol_plan::ResourceProfile;
using protocol_plan::UnknownEnumPolicy;
using protocol_plan::ValueType;
using protocol_plan::WireCodec;

void AppendJsonString(std::ostringstream& output, std::string_view value) {
  output << '"';
  for (const unsigned char character : value) {
    switch (character) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (character < 0x20U) {
          output << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                 << static_cast<unsigned>(character) << std::dec << std::nouppercase;
        } else {
          output << static_cast<char>(character);
        }
        break;
    }
  }
  output << '"';
}

void AppendCommonMetadata(std::ostringstream& output, std::string_view id) {
  output << "\"id\":";
  AppendJsonString(output, id);
  output << ",\"display_name\":\"Synthetic test item\",\"description\":\"\",";
  output << "\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:test_plan_factory\"";
}

void AppendMatcher(std::ostringstream& output, const protocol_plan::MatcherPlan& matcher) {
  output << '{';
  if (matcher.kind == MatcherKind::FRAME_LENGTH_EQUALS) {
    output << "\"kind\":\"frame_length_equals\",\"length_bytes\":" << matcher.length_bytes;
  } else {
    output << "\"kind\":\"fixed_bytes\",\"byte_offset\":" << matcher.byte_offset << ",\"bytes\":\"";
    for (std::size_t index = 0U; index < matcher.bytes.size(); ++index) {
      if (index != 0U) {
        output << ' ';
      }
      output << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
             << static_cast<unsigned>(matcher.bytes[index]);
    }
    output << std::dec << std::nouppercase << '"';
  }
  output << '}';
}

void AppendWire(std::ostringstream& output, const FieldPlan& field) {
  output << "\"wire\":{";
  if (field.wire_codec == WireCodec::BYTES) {
    output << "\"codec\":\"bytes\",\"byte_offset\":" << field.byte_offset
           << ",\"byte_length\":" << field.byte_width;
  } else {
    output << "\"codec\":\"unsigned_integer\",\"byte_offset\":" << field.byte_offset
           << ",\"byte_width\":" << field.byte_width;
    if (field.byte_order == ByteOrder::BIG) {
      output << ",\"byte_order\":\"big_endian\"";
    } else if (field.byte_order == ByteOrder::LITTLE) {
      output << ",\"byte_order\":\"little_endian\"";
    }
  }
  output << '}';
}

void AppendField(std::ostringstream& output, const FieldPlan& field) {
  output << '{';
  AppendCommonMetadata(output, field.id);
  output << ",\"value_type\":\"";
  if (field.value_type == ValueType::UINT64) {
    output << "UINT64";
  } else if (field.value_type == ValueType::INT64) {
    output << "INT64";
  } else if (field.value_type == ValueType::BYTES) {
    output << "BYTES";
  } else {
    output << "ENUM";
  }
  output << "\",";
  AppendWire(output, field);
  output << ",\"encode\":{\"source\":\"";
  if (field.encode_source == EncodeSource::CONSTANT) {
    output << "constant\",\"value\":";
    if (field.value_type == ValueType::INT64) {
      output << field.signed_constant_value.value_or(0);
    } else {
      output << field.constant_value.value_or(0U);
    }
  } else {
    output << "input\"";
  }
  output << '}';
  if (field.value_type == ValueType::ENUM) {
    output << ",\"unknown_enum_policy\":\""
           << (field.unknown_enum_policy == UnknownEnumPolicy::PRESERVE ? "preserve" : "reject")
           << "\",\"enum_entries\":[";
    for (std::size_t index = 0U; index < field.enum_entries.size(); ++index) {
      if (index != 0U) {
        output << ',';
      }
      output << '{';
      AppendJsonString(output, "id");
      output << ':';
      AppendJsonString(output, field.enum_entries[index].id);
      output << ",\"display_name\":\"Synthetic enum\",\"raw_value\":"
             << field.enum_entries[index].raw_value << '}';
    }
    output << ']';
  }
  output << '}';
}

std::string FramingId(const std::vector<protocol_plan::FramingPlan>& framing_profiles,
                      std::size_t index) {
  return index < framing_profiles.size() ? framing_profiles[index].id : "missing_framing_profile";
}

std::string MessageId(const std::vector<protocol_plan::MessagePlan>& messages, std::size_t index) {
  return index < messages.size() ? messages[index].id : "missing_message";
}

std::vector<protocol_plan::FramingPlan> CloneFramingProfiles(
    const protocol_plan::PlanBundle& source) {
  std::vector<protocol_plan::FramingPlan> output;
  output.reserve(source.FramingProfiles().size());
  for (const protocol_plan::FrozenFramingPlan& framing : source.FramingProfiles()) {
    output.push_back(
        protocol_plan::FramingPlan{std::string(framing.id.View()), framing.input_kind});
  }
  return output;
}

}  // namespace

config_compiler::CompileResult CompileTestPlan(
    std::string protocol_id, ResourceProfile resource_profile,
    std::vector<protocol_plan::FramingPlan> framing_profiles,
    std::vector<protocol_plan::PipelinePlan> pipelines,
    std::vector<protocol_plan::MessagePlan> messages) {
  std::ostringstream output;
  output << '{';
  output << "\"schema_version\":\"0.1\",\"protocol_id\":";
  AppendJsonString(output, protocol_id);
  output << ",\"protocol_version\":\"1\",\"display_name\":\"Synthetic test protocol\",";
  output << "\"description\":\"\",\"source_ref\":";
  AppendJsonString(output, "SYNTHETIC_FROM_SCRATCH:test_plan_factory");
  output << ",\"resource_profile\":\""
         << (resource_profile == ResourceProfile::CONSTRAINED ? "constrained" : "desktop")
         << "\",\"framing_profiles\":[";

  for (std::size_t index = 0U; index < framing_profiles.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << '{';
    AppendCommonMetadata(output, framing_profiles[index].id);
    output << ",\"input_kind\":\""
           << (framing_profiles[index].input_kind == InputKind::COMPLETE_RECORD ? "complete_record"
                                                                                : "unsupported")
           << "\"}";
  }
  output << "],\"pipelines\":[";

  for (std::size_t index = 0U; index < pipelines.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    const protocol_plan::PipelinePlan& pipeline = pipelines[index];
    output << '{';
    AppendCommonMetadata(output, pipeline.id);
    output << ",\"direction_id\":";
    AppendJsonString(output, pipeline.direction_id);
    output << ",\"input_framing_profile_id\":";
    AppendJsonString(output, FramingId(framing_profiles, pipeline.framing_profile_index));
    output << ",\"message_ids\":[";
    for (std::size_t reference = 0U; reference < pipeline.message_indices.size(); ++reference) {
      if (reference != 0U) {
        output << ',';
      }
      AppendJsonString(output, MessageId(messages, pipeline.message_indices[reference]));
    }
    output << "]}";
  }
  output << "],\"messages\":[";

  for (std::size_t index = 0U; index < messages.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    const protocol_plan::MessagePlan& message = messages[index];
    output << '{';
    AppendCommonMetadata(output, message.id);
    output << ",\"direction_id\":";
    AppendJsonString(output, message.direction_id);
    output << ",\"frame_length_bytes\":" << message.frame_length_bytes << ",\"matcher\":{\"all\":[";
    for (std::size_t matcher_index = 0U; matcher_index < message.matchers.size(); ++matcher_index) {
      if (matcher_index != 0U) {
        output << ',';
      }
      AppendMatcher(output, message.matchers[matcher_index]);
    }
    output << "]},\"fields\":[";
    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      if (field_index != 0U) {
        output << ',';
      }
      AppendField(output, message.fields[field_index]);
    }
    output << "]}";
  }
  output << "]}";
  return config_compiler::CompileJsonToPlan(output.str());
}

config_compiler::CompileResult CloneTestPlan(const protocol_plan::PlanBundle& source,
                                             std::vector<protocol_plan::PipelinePlan> pipelines,
                                             std::vector<protocol_plan::MessagePlan> messages) {
  return CompileTestPlan(std::string(source.ProtocolId()), source.GetResourceProfile(),
                         CloneFramingProfiles(source), std::move(pipelines), std::move(messages));
}

std::vector<protocol_plan::PipelinePlan> CloneMutablePipelines(
    const protocol_plan::PlanBundle& source) {
  std::vector<protocol_plan::PipelinePlan> output;
  output.reserve(source.Pipelines().size());
  for (const protocol_plan::FrozenPipelinePlan& frozen : source.Pipelines()) {
    protocol_plan::PipelinePlan pipeline;
    pipeline.id = std::string(frozen.id.View());
    pipeline.direction_id = std::string(frozen.direction_id.View());
    pipeline.framing_profile_index = frozen.framing_profile_index;
    pipeline.message_indices.assign(frozen.message_indices.begin(), frozen.message_indices.end());
    output.push_back(std::move(pipeline));
  }
  return output;
}

std::vector<protocol_plan::MessagePlan> CloneMutableMessages(
    const protocol_plan::PlanBundle& source) {
  std::vector<protocol_plan::MessagePlan> output;
  output.reserve(source.Messages().size());
  for (const protocol_plan::FrozenMessagePlan& frozen_message : source.Messages()) {
    protocol_plan::MessagePlan message;
    message.id = std::string(frozen_message.id.View());
    message.direction_id = std::string(frozen_message.direction_id.View());
    message.frame_length_bytes = frozen_message.frame_length_bytes;
    message.matchers.reserve(frozen_message.matchers.size());
    for (const protocol_plan::FrozenMatcherPlan& frozen_matcher : frozen_message.matchers) {
      protocol_plan::MatcherPlan matcher;
      matcher.kind = frozen_matcher.kind;
      matcher.length_bytes = frozen_matcher.length_bytes;
      matcher.byte_offset = frozen_matcher.byte_offset;
      matcher.bytes.assign(frozen_matcher.bytes.begin(), frozen_matcher.bytes.end());
      message.matchers.push_back(std::move(matcher));
    }
    message.fields.reserve(frozen_message.fields.size());
    for (const protocol_plan::FrozenFieldPlan& frozen_field : frozen_message.fields) {
      protocol_plan::FieldPlan field;
      field.id = std::string(frozen_field.id.View());
      field.value_type = frozen_field.value_type;
      field.wire_codec = frozen_field.wire_codec;
      field.byte_offset = frozen_field.byte_offset;
      field.byte_width = frozen_field.byte_width;
      field.byte_order = frozen_field.byte_order;
      field.encode_source = frozen_field.encode_source;
      field.constant_value = frozen_field.constant_value;
      field.signed_constant_value = frozen_field.signed_constant_value;
      field.unknown_enum_policy = frozen_field.unknown_enum_policy;
      field.enum_entries.reserve(frozen_field.enum_entries.size());
      for (const protocol_plan::FrozenEnumEntryPlan& frozen_entry : frozen_field.enum_entries) {
        field.enum_entries.push_back(protocol_plan::EnumEntryPlan{
            std::string(frozen_entry.id.View()), frozen_entry.raw_value});
      }
      message.fields.push_back(std::move(field));
    }
    output.push_back(std::move(message));
  }
  return output;
}

}  // namespace pae::test_support
