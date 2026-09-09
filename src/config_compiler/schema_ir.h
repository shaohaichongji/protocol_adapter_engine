#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../protocol_plan/plan_types.h"

namespace pae::config_compiler {

using protocol_plan::BitNumbering;
using protocol_plan::ByteOrder;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
using protocol_plan::ComputedLengthScope;
#endif
using protocol_plan::EncodeSource;
using protocol_plan::InputKind;
using protocol_plan::IntegrityAlgorithm;
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
  std::string container_id;
  std::uint64_t bit_offset = 0U;
  std::uint64_t bit_width = 0U;
  std::size_t bit_container_index = static_cast<std::size_t>(-1);
  ConfigOrigin origin;
};

struct BitContainerIr {
  std::string id;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  BitNumbering bit_numbering = BitNumbering::LSB0;
  std::uint64_t base_value = 0U;
  ConfigOrigin origin;
};

struct IntegrityIr {
  IntegrityAlgorithm algorithm = IntegrityAlgorithm::SUM8;
  std::uint64_t range_offset = 0U;
  std::uint64_t range_length = 0U;
  std::uint64_t storage_offset = 0U;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  bool range_ends_at_payload = false;
  bool storage_at_payload_end = false;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  std::uint8_t crc_width = 0U;
  std::uint32_t crc_polynomial = 0U;
  std::uint32_t crc_initial_value = 0U;
  std::uint32_t crc_xor_output = 0U;
  bool crc_reflect_input = false;
  bool crc_reflect_output = false;
  ByteOrder storage_byte_order = ByteOrder::NOT_APPLICABLE;
#endif
  ConfigOrigin origin;
  ConfigOrigin range_origin;
  ConfigOrigin storage_origin;
};

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
struct BoundedPayloadIr {
  std::string payload_field_id;
  std::size_t payload_field_index = static_cast<std::size_t>(-1);
  std::uint64_t header_length = 0U;
  std::uint64_t min_payload_length = 0U;
  std::uint64_t max_payload_length = 0U;
  std::uint64_t trailer_length = 0U;
  std::uint64_t min_frame_length = 0U;
  std::uint64_t max_frame_length = 0U;
  ConfigOrigin origin;
};
#endif

struct EncodeIr {
  EncodeSource source = EncodeSource::INPUT;
  std::optional<std::uint64_t> constant_value;
  ConfigOrigin origin;
  std::optional<std::int64_t> signed_constant_value;
};

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
struct ComputedLengthIr {
  ComputedLengthScope scope = ComputedLengthScope::FRAME;
  std::uint64_t range_offset = 0U;
  std::uint64_t range_length = 0U;
  ConfigOrigin origin;
  ConfigOrigin range_origin;
};
#endif

struct EnumEntryIr {
  std::string id;
  std::string display_name;
  std::uint64_t raw_value = 0U;
  ConfigOrigin origin;
};

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
struct RationalIr {
  std::int64_t numerator = 0;
  std::uint64_t denominator = 1U;
  ConfigOrigin origin;
};

struct LinearConversionIr {
  RationalIr scale;
  RationalIr bias;
  protocol_plan::LinearConversionDescriptor derived;
  ConfigOrigin origin;
};
#endif

struct FieldIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  ValueType value_type = ValueType::UINT64;
  WireIr wire;
  EncodeIr encode;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::optional<ComputedLengthIr> computed_length;
#endif
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  std::vector<EnumEntryIr> enum_entries;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::optional<LinearConversionIr> conversion;
#endif
  ConfigOrigin origin;
};

struct MessageIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::string direction_id;
  std::uint64_t frame_length_bytes = 0U;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  std::optional<BoundedPayloadIr> bounded_payload;
#endif
  std::vector<MatcherClauseIr> matcher_clauses;
  std::vector<BitContainerIr> bit_containers;
  std::optional<IntegrityIr> integrity;
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

class DomainValidator;
class ResourceBudgetValidator;
class PlanDraftAssembler;

// Internal capability state: only DomainValidator can create it. This type is deliberately
// move-only so validation authority cannot be copied or synthesized by setting a public flag.
class ValidatedSchemaIr final {
 public:
  ValidatedSchemaIr() = delete;
  ValidatedSchemaIr(const ValidatedSchemaIr&) = delete;
  ValidatedSchemaIr& operator=(const ValidatedSchemaIr&) = delete;
  ValidatedSchemaIr(ValidatedSchemaIr&&) noexcept = default;
  ValidatedSchemaIr& operator=(ValidatedSchemaIr&&) noexcept = default;
  ~ValidatedSchemaIr() = default;

 private:
  struct Payload final {
    Payload(SchemaIr schema, std::vector<ResolvedPipelineIr> resolved_pipelines,
            ResourceRequirements requirements)
        : schema(std::move(schema)),
          resolved_pipelines(std::move(resolved_pipelines)),
          requirements(requirements) {}

    SchemaIr schema;
    std::vector<ResolvedPipelineIr> resolved_pipelines;
    ResourceRequirements requirements;
  };

  ValidatedSchemaIr(SchemaIr schema, std::vector<ResolvedPipelineIr> resolved_pipelines,
                    ResourceRequirements requirements)
      : payload_(std::make_unique<Payload>(std::move(schema), std::move(resolved_pipelines),
                                           requirements)) {}

  friend class DomainValidator;
  friend class ResourceBudgetValidator;
  friend class PlanDraftAssembler;

  std::unique_ptr<Payload> payload_;
};

// Internal capability state: only ResourceBudgetValidator can promote a validated SchemaIr.
class BudgetedSchemaIr final {
 public:
  BudgetedSchemaIr() = delete;
  BudgetedSchemaIr(const BudgetedSchemaIr&) = delete;
  BudgetedSchemaIr& operator=(const BudgetedSchemaIr&) = delete;
  BudgetedSchemaIr(BudgetedSchemaIr&&) noexcept = default;
  BudgetedSchemaIr& operator=(BudgetedSchemaIr&&) noexcept = default;
  ~BudgetedSchemaIr() = default;

 private:
  BudgetedSchemaIr(ValidatedSchemaIr validated, protocol_plan::PlanMemoryReport plan_memory,
                   std::size_t plan_memory_limit_bytes)
      : validated_(std::make_unique<ValidatedSchemaIr>(std::move(validated))),
        plan_memory_(plan_memory),
        plan_memory_limit_bytes_(plan_memory_limit_bytes) {}

  friend class ResourceBudgetValidator;
  friend class PlanDraftAssembler;

  std::unique_ptr<ValidatedSchemaIr> validated_;
  protocol_plan::PlanMemoryReport plan_memory_;
  std::size_t plan_memory_limit_bytes_ = 0U;
};

}  // namespace pae::config_compiler
