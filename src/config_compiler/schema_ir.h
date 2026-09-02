#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../protocol_plan/plan_types.h"

namespace pae::config_compiler {

using protocol_plan::ByteOrder;
using protocol_plan::EncodeSource;
using protocol_plan::InputKind;
using protocol_plan::MatcherKind;
using protocol_plan::ResourceProfile;
using protocol_plan::ResourceRequirements;
using protocol_plan::UnknownEnumPolicy;
using protocol_plan::ValueType;
using protocol_plan::WireCodec;

struct ConfigOrigin {
  std::string json_pointer;
};

struct FramingProfileIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  InputKind input_kind = InputKind::COMPLETE_RECORD;
  ConfigOrigin origin;
};

struct PipelineIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::string direction_id;
  std::string input_framing_profile_id;
  std::vector<std::string> message_ids;
  ConfigOrigin origin;
};

struct MatcherClauseIr {
  MatcherKind kind = MatcherKind::FRAME_LENGTH_EQUALS;
  std::uint64_t length_bytes = 0U;
  std::uint64_t byte_offset = 0U;
  std::vector<std::uint8_t> bytes;
  ConfigOrigin origin;
};

struct WireIr {
  WireCodec codec = WireCodec::UNSIGNED_INTEGER;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  ConfigOrigin origin;
};

struct EncodeIr {
  EncodeSource source = EncodeSource::INPUT;
  std::optional<std::uint64_t> constant_value;
  ConfigOrigin origin;
};

struct EnumEntryIr {
  std::string id;
  std::string display_name;
  std::uint64_t raw_value = 0U;
  ConfigOrigin origin;
};

struct FieldIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  ValueType value_type = ValueType::UINT64;
  WireIr wire;
  EncodeIr encode;
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  std::vector<EnumEntryIr> enum_entries;
  ConfigOrigin origin;
};

struct MessageIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::string direction_id;
  std::uint64_t frame_length_bytes = 0U;
  std::vector<MatcherClauseIr> matcher_clauses;
  std::vector<FieldIr> fields;
  ConfigOrigin origin;
};

struct SchemaIr {
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  std::string display_name;
  std::string description;
  std::string source_ref;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
  std::vector<FramingProfileIr> framing_profiles;
  std::vector<PipelineIr> pipelines;
  std::vector<MessageIr> messages;
};

struct ResolvedPipelineIr {
  std::size_t framing_profile_index = 0U;
  std::vector<std::size_t> message_indices;
};

struct ValidatedSchemaIr {
  SchemaIr schema;
  std::vector<ResolvedPipelineIr> resolved_pipelines;
  ResourceRequirements requirements;
};

struct BudgetedSchemaIr {
  ValidatedSchemaIr validated;
};

}  // namespace pae::config_compiler
