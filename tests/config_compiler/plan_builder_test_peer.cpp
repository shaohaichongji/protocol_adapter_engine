#include "plan_builder_test_peer.h"

#include <memory>
#include <utility>

#include "../../src/protocol_plan/plan_draft_internal.h"

namespace pae::protocol_plan::test_only {

class PlanBuilderTestPeer final {
 public:
  static BudgetedPlanDraft SetDraftSchemaVersion(BudgetedPlanDraft draft, const char* version) {
    draft.draft_->schema_version = version;
    return draft;
  }
  static BudgetedPlanDraft SetFramingLengthOffset(BudgetedPlanDraft draft, std::uint64_t offset) {
    draft.draft_->framing_profiles[0].length_field_offset = offset;
    return draft;
  }
  static BudgetedPlanDraft SetFramingLengthByteOrder(BudgetedPlanDraft draft,
                                                     ByteOrder byte_order) {
    draft.draft_->framing_profiles[0].length_field_byte_order = byte_order;
    return draft;
  }
  static BudgetedPlanDraft InjectSignedConstant(BudgetedPlanDraft draft) {
    draft.draft_->messages[0].fields[1].signed_constant_value = -1;
    return draft;
  }
  static BudgetedPlanDraft MakeCorruptedBudgetedPlanDraft() {
    auto draft = std::make_unique<detail::PlanDraftData>();
    draft->schema_version = "0.1";
    draft->protocol_id = "corrupted_test_draft";
    draft->protocol_version = "1";
    draft->resource_profile = ResourceProfile::DESKTOP;
    return BudgetedPlanDraft{std::move(draft)};
  }

  static BudgetedPlanDraft MakeBitfieldDraft(ByteOrder byte_order, BitNumbering bit_numbering,
                                             bool matcher_conflict = false) {
    auto draft = std::make_unique<detail::PlanDraftData>();
    draft->schema_version = "0.2";
    draft->protocol_id = "bitfield_defense_test";
    draft->protocol_version = "1";
    draft->resource_profile = ResourceProfile::DESKTOP;
    draft->resource_requirements.max_frame_bytes = 1U;
    draft->resource_requirements.framing_profile_count = 1U;
    draft->resource_requirements.pipeline_count = 1U;
    draft->resource_requirements.message_count = 1U;
    draft->resource_requirements.total_field_count = 1U;
    draft->resource_requirements.total_matcher_count = matcher_conflict ? 2U : 1U;
    draft->resource_requirements.total_bit_container_count = 1U;
    draft->framing_profiles.push_back(FramingPlan{"record", InputKind::COMPLETE_RECORD});
    PipelinePlan pipeline;
    pipeline.id = "pipeline";
    pipeline.direction_id = "rx";
    pipeline.message_indices.push_back(0U);
    draft->pipelines.push_back(std::move(pipeline));
    MessagePlan message;
    message.id = "message";
    message.direction_id = "rx";
    message.frame_length_bytes = 1U;
    message.matchers.push_back(MatcherPlan{MatcherKind::FRAME_LENGTH_EQUALS, 1U});
    if (matcher_conflict) {
      MatcherPlan fixed;
      fixed.kind = MatcherKind::FIXED_BYTES;
      fixed.bytes.push_back(0x02U);
      message.matchers.push_back(std::move(fixed));
    }
    message.bit_containers.push_back(
        BitContainerPlan{"container", 0U, 1U, byte_order, bit_numbering, 0U});
    FieldPlan field;
    field.id = "value";
    field.value_type = ValueType::UINT64;
    field.wire_codec = WireCodec::BITFIELD;
    field.bit_container_index = 0U;
    field.bit_width = 1U;
    message.fields.push_back(std::move(field));
    draft->messages.push_back(std::move(message));
    return BudgetedPlanDraft{std::move(draft)};
  }

  static BudgetedPlanDraft MakeBitfieldDraftWithTooManyMessageContainers() {
    auto draft = MakeBitfieldDraft(ByteOrder::NOT_APPLICABLE, BitNumbering::LSB0);
    draft.draft_->resource_profile = ResourceProfile::CONSTRAINED;
    draft.draft_->messages[0].bit_containers.resize(
        kConstrainedResourceProfileLimits.max_fields_per_message + 1U);
    draft.draft_->resource_requirements.total_bit_container_count =
        draft.draft_->messages[0].bit_containers.size();
    return draft;
  }

  static BudgetedPlanDraft MakeBitfieldDraftWithTooManyTotalContainers() {
    auto draft = MakeBitfieldDraft(ByteOrder::NOT_APPLICABLE, BitNumbering::LSB0);
    draft.draft_->resource_profile = ResourceProfile::CONSTRAINED;
    auto message = draft.draft_->messages.front();
    message.bit_containers.resize(kConstrainedResourceProfileLimits.max_fields_per_message);
    draft.draft_->messages.assign(5U, message);
    draft.draft_->resource_requirements.message_count = draft.draft_->messages.size();
    draft.draft_->resource_requirements.total_field_count = draft.draft_->messages.size();
    draft.draft_->resource_requirements.total_matcher_count = draft.draft_->messages.size();
    draft.draft_->resource_requirements.total_bit_container_count =
        draft.draft_->messages.size() * message.bit_containers.size();
    return draft;
  }

  static BudgetedPlanDraft MakeIntegrityDraft() {
    auto draft = std::make_unique<detail::PlanDraftData>();
    draft->schema_version = "0.3";
    draft->protocol_id = "integrity_defense_test";
    draft->protocol_version = "1";
    draft->resource_profile = ResourceProfile::DESKTOP;
    draft->resource_requirements.max_frame_bytes = 2U;
    draft->resource_requirements.framing_profile_count = 1U;
    draft->resource_requirements.pipeline_count = 1U;
    draft->resource_requirements.message_count = 1U;
    draft->resource_requirements.total_field_count = 1U;
    draft->resource_requirements.total_matcher_count = 1U;
    draft->resource_requirements.total_integrity_rule_count = 1U;
    draft->framing_profiles.push_back(FramingPlan{"record", InputKind::COMPLETE_RECORD});
    PipelinePlan pipeline;
    pipeline.id = "pipeline";
    pipeline.direction_id = "rx";
    pipeline.message_indices.push_back(0U);
    draft->pipelines.push_back(std::move(pipeline));
    MessagePlan message;
    message.id = "message";
    message.direction_id = "rx";
    message.frame_length_bytes = 2U;
    message.matchers.push_back(MatcherPlan{MatcherKind::FRAME_LENGTH_EQUALS, 2U});
    FieldPlan field;
    field.id = "value";
    field.value_type = ValueType::UINT64;
    field.wire_codec = WireCodec::UNSIGNED_INTEGER;
    field.byte_offset = 0U;
    field.byte_width = 1U;
    message.fields.push_back(std::move(field));
    message.integrity = IntegrityPlan{IntegrityAlgorithm::SUM8, 0U, 1U, 1U};
    draft->messages.push_back(std::move(message));
    return BudgetedPlanDraft{std::move(draft)};
  }

  static BudgetedPlanDraft MakeIntegrityDraftWithUnknownAlgorithm() {
    auto draft = MakeIntegrityDraft();
    draft.draft_->messages[0].integrity->algorithm = static_cast<IntegrityAlgorithm>(255);
    return draft;
  }

  static BudgetedPlanDraft MakeIntegrityDraftWithSelfIncludedStorage() {
    auto draft = MakeIntegrityDraft();
    draft.draft_->messages[0].integrity->range_length = 2U;
    return draft;
  }

  static BudgetedPlanDraft MakeIntegrityDraftWithFieldStorageConflict() {
    auto draft = MakeIntegrityDraft();
    draft.draft_->messages[0].integrity->range_offset = 1U;
    draft.draft_->messages[0].integrity->storage_offset = 0U;
    return draft;
  }

#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  static BudgetedPlanDraft MakeCrcDraft() {
    auto draft = MakeIntegrityDraft();
    draft.draft_->schema_version = "0.6";
    draft.draft_->messages[0].frame_length_bytes = 3U;
    draft.draft_->messages[0].matchers[0].length_bytes = 3U;
    draft.draft_->resource_requirements.max_frame_bytes = 3U;
    auto& integrity = *draft.draft_->messages[0].integrity;
    integrity.algorithm = IntegrityAlgorithm::CRC;
    integrity.crc_width = 16U;
    integrity.crc_polynomial = 0x1021U;
    integrity.crc_initial_value = 0xFFFFU;
    integrity.crc_xor_output = 0U;
    integrity.storage_byte_order = ByteOrder::BIG;
    return draft;
  }

  static BudgetedPlanDraft MakeCrcDraftWithInvalidWidth() {
    auto draft = MakeCrcDraft();
    draft.draft_->messages[0].integrity->crc_width = 24U;
    return draft;
  }

  static BudgetedPlanDraft MakeCrcDraftWithEvenPolynomial() {
    auto draft = MakeCrcDraft();
    draft.draft_->messages[0].integrity->crc_polynomial = 0x1020U;
    return draft;
  }

  static BudgetedPlanDraft MakeCrcDraftWithInvalidStorageOrder() {
    auto draft = MakeCrcDraft();
    draft.draft_->messages[0].integrity->storage_byte_order = ByteOrder::NOT_APPLICABLE;
    return draft;
  }

  static BudgetedPlanDraft MakeCrcDraftWithOldSchema() {
    auto draft = MakeCrcDraft();
    draft.draft_->schema_version = "0.5";
    return draft;
  }
#endif

#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  static BudgetedPlanDraft MakeCorruptedStreamDraft(
      pae::test_support::StreamDraftMutation mutation) {
    auto draft = MakeVariableDraft();
    draft.draft_->schema_version = "0.9";
    auto& framing = draft.draft_->framing_profiles[0];
    framing.input_kind = InputKind::STREAM_CHUNK;
    framing.strategy = FramingStrategy::SYNC_LENGTH_FIELD;
    framing.sync_bytes = {0xA5U};
    framing.sync_prefix_table = {0U};
    framing.length_field_offset = 1U;
    framing.length_field_width = 1U;
    framing.length_field_byte_order = ByteOrder::NOT_APPLICABLE;
    framing.minimum_frame_length = 3U;
    framing.maximum_frame_length = 6U;
    draft.draft_->resource_requirements.max_stream_frame_bytes = 6U;
    draft.draft_->resource_requirements.max_sync_bytes = 1U;
    draft.draft_->resource_requirements.max_framing_buffer_bytes = 6U;
    switch (mutation) {
      case pae::test_support::StreamDraftMutation::INVALID_STRATEGY_UNION:
        framing.frame_length_bytes = 6U;
        break;
      case pae::test_support::StreamDraftMutation::CORRUPTED_PREFIX_TABLE:
        framing.sync_prefix_table[0] = 1U;
        break;
      case pae::test_support::StreamDraftMutation::MESSAGE_LENGTH_MISMATCH:
        framing.length_field_offset = 2U;
        break;
    }
    return draft;
  }
#endif

#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  static BudgetedPlanDraft MutateAsciiTextDraft(
      BudgetedPlanDraft draft, pae::test_support::AsciiTextDraftMutation mutation) {
    auto& message = draft.draft_->messages[0];
    switch (mutation) {
      case pae::test_support::AsciiTextDraftMutation::CORRUPTED_PREFIX_TABLE:
        message.ascii_text->decode->segments[2].prefix_table[0] = 1U;
        break;
      case pae::test_support::AsciiTextDraftMutation::RESOURCE_COUNT_MISMATCH:
        --draft.draft_->resource_requirements.total_text_segment_count;
        break;
      case pae::test_support::AsciiTextDraftMutation::OLD_SCHEMA_RESIDUE:
        draft.draft_->schema_version = "0.9";
        break;
      case pae::test_support::AsciiTextDraftMutation::FIELD_TYPE_MISMATCH:
        message.fields[0].value_type = ValueType::UINT64;
        break;
    }
    return draft;
  }
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
  static BudgetedPlanDraft MutateAsciiStreamDraft(
      BudgetedPlanDraft draft, pae::test_support::AsciiStreamDraftMutation mutation) {
    auto& framing = draft.draft_->framing_profiles[0];
    auto& message = draft.draft_->messages[0];
    switch (mutation) {
      case pae::test_support::AsciiStreamDraftMutation::CORRUPTED_TERMINATOR:
        framing.sync_bytes[1] = 0x0BU;
        break;
      case pae::test_support::AsciiStreamDraftMutation::PROFILE_TOO_SHORT:
        framing.maximum_frame_length = 2U;
        draft.draft_->resource_requirements.max_stream_frame_bytes = 2U;
        draft.draft_->resource_requirements.max_framing_buffer_bytes = 2U;
        break;
      case pae::test_support::AsciiStreamDraftMutation::BOUNDARY_UNPROVEN:
        message.ascii_text->decode->segments.back().literal = {'!', 'X', 'X'};
        message.ascii_text->decode->segments.back().prefix_table = {0U, 0U, 0U};
        break;
      case pae::test_support::AsciiStreamDraftMutation::NO_DECODE_CANDIDATE:
        draft.draft_->resource_requirements.total_text_segment_count -=
            message.ascii_text->decode->segments.size();
        message.ascii_text->decode.reset();
        break;
    }
    return draft;
  }
#endif
#endif

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  static BudgetedPlanDraft MakeLengthDraft() {
    auto draft = std::make_unique<detail::PlanDraftData>();
    draft->schema_version = "0.7";
    draft->protocol_id = "length_defense_test";
    draft->protocol_version = "1";
    draft->resource_profile = ResourceProfile::DESKTOP;
    draft->resource_requirements.max_frame_bytes = 2U;
    draft->resource_requirements.framing_profile_count = 1U;
    draft->resource_requirements.pipeline_count = 1U;
    draft->resource_requirements.message_count = 1U;
    draft->resource_requirements.total_field_count = 2U;
    draft->resource_requirements.total_matcher_count = 1U;
    draft->resource_requirements.total_computed_length_count = 1U;
    draft->framing_profiles.push_back(FramingPlan{"record", InputKind::COMPLETE_RECORD});
    PipelinePlan pipeline;
    pipeline.id = "pipeline";
    pipeline.direction_id = "rx";
    pipeline.message_indices.push_back(0U);
    draft->pipelines.push_back(std::move(pipeline));
    MessagePlan message;
    message.id = "message";
    message.direction_id = "rx";
    message.frame_length_bytes = 2U;
    message.matchers.push_back(MatcherPlan{MatcherKind::FRAME_LENGTH_EQUALS, 2U});
    FieldPlan length;
    length.id = "record_length";
    length.value_type = ValueType::UINT64;
    length.wire_codec = WireCodec::UNSIGNED_INTEGER;
    length.byte_offset = 0U;
    length.byte_width = 1U;
    length.byte_order = ByteOrder::NOT_APPLICABLE;
    length.encode_source = EncodeSource::COMPUTED;
    message.fields.push_back(std::move(length));
    FieldPlan value;
    value.id = "value";
    value.value_type = ValueType::UINT64;
    value.wire_codec = WireCodec::UNSIGNED_INTEGER;
    value.byte_offset = 1U;
    value.byte_width = 1U;
    value.byte_order = ByteOrder::NOT_APPLICABLE;
    value.encode_source = EncodeSource::INPUT;
    message.fields.push_back(std::move(value));
    message.computed_length = ComputedLengthPlan{
        0U, 0U, 1U, ByteOrder::NOT_APPLICABLE, ComputedLengthScope::FRAME, 0U, 0U, 2U};
    draft->messages.push_back(std::move(message));
    return BudgetedPlanDraft{std::move(draft)};
  }

  static BudgetedPlanDraft MakeLengthDraftWithIncorrectExpectedValue() {
    auto draft = MakeLengthDraft();
    draft.draft_->messages[0].computed_length->expected_value = 1U;
    return draft;
  }

  static BudgetedPlanDraft MakeLengthDraftWithInvalidScope() {
    auto draft = MakeLengthDraft();
    draft.draft_->messages[0].computed_length->scope = static_cast<ComputedLengthScope>(255);
    return draft;
  }

  static BudgetedPlanDraft MakeLengthDraftWithOldSchema() {
    auto draft = MakeLengthDraft();
    draft.draft_->schema_version = "0.6";
    return draft;
  }

  static BudgetedPlanDraft MakeLengthDraftWithResourceCountMismatch() {
    auto draft = MakeLengthDraft();
    draft.draft_->resource_requirements.total_computed_length_count = 0U;
    return draft;
  }
#endif

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  static BudgetedPlanDraft MakeVariableDraft() {
    auto draft = std::make_unique<detail::PlanDraftData>();
    draft->schema_version = "0.8";
    draft->protocol_id = "variable_defense_test";
    draft->protocol_version = "1";
    draft->resource_profile = ResourceProfile::DESKTOP;
    draft->resource_requirements.max_frame_bytes = 6U;
    draft->resource_requirements.framing_profile_count = 1U;
    draft->resource_requirements.pipeline_count = 1U;
    draft->resource_requirements.message_count = 1U;
    draft->resource_requirements.total_field_count = 2U;
    draft->resource_requirements.total_matcher_count = 1U;
    draft->resource_requirements.total_integrity_rule_count = 1U;
    draft->resource_requirements.total_computed_length_count = 1U;
    draft->framing_profiles.push_back(FramingPlan{"record", InputKind::COMPLETE_RECORD});
    PipelinePlan pipeline;
    pipeline.id = "pipeline";
    pipeline.direction_id = "rx";
    pipeline.message_indices.push_back(0U);
    draft->pipelines.push_back(std::move(pipeline));
    MessagePlan message;
    message.id = "message";
    message.direction_id = "rx";
    message.frame_length_bytes = 6U;
    MatcherPlan matcher;
    matcher.kind = MatcherKind::FIXED_BYTES;
    matcher.byte_offset = 0U;
    matcher.bytes.push_back(0xA5U);
    message.matchers.push_back(std::move(matcher));
    FieldPlan length;
    length.id = "record_length";
    length.value_type = ValueType::UINT64;
    length.wire_codec = WireCodec::UNSIGNED_INTEGER;
    length.byte_offset = 1U;
    length.byte_width = 1U;
    length.byte_order = ByteOrder::NOT_APPLICABLE;
    length.encode_source = EncodeSource::COMPUTED;
    message.fields.push_back(std::move(length));
    FieldPlan payload;
    payload.id = "payload";
    payload.value_type = ValueType::BYTES;
    payload.wire_codec = WireCodec::BYTES;
    payload.byte_offset = 2U;
    payload.byte_width = 3U;
    payload.encode_source = EncodeSource::INPUT;
    message.fields.push_back(std::move(payload));
    message.bounded_payload = BoundedPayloadPlan{1U, 2U, 0U, 3U, 1U, 3U, 6U};
    message.computed_length = ComputedLengthPlan{
        0U, 1U, 1U, ByteOrder::NOT_APPLICABLE, ComputedLengthScope::FRAME, 0U, 0U, 6U};
    IntegrityPlan integrity{IntegrityAlgorithm::SUM8, 0U, 0U, 0U};
    integrity.range_ends_at_payload = true;
    integrity.storage_at_payload_end = true;
    message.integrity = integrity;
    draft->messages.push_back(std::move(message));
    return BudgetedPlanDraft{std::move(draft)};
  }

  static BudgetedPlanDraft MakeCorruptedVariableDraft(
      pae::test_support::VariableDraftMutation mutation) {
    auto draft = MakeVariableDraft();
    auto& message = draft.draft_->messages[0];
    switch (mutation) {
      case pae::test_support::VariableDraftMutation::INVALID_PAYLOAD_INDEX:
        message.bounded_payload->payload_field_index = message.fields.size();
        break;
      case pae::test_support::VariableDraftMutation::REVERSED_PAYLOAD_RANGE:
        message.bounded_payload->min_payload_length = 4U;
        break;
      case pae::test_support::VariableDraftMutation::INCORRECT_MAX_FRAME:
        --message.bounded_payload->max_frame_length;
        break;
      case pae::test_support::VariableDraftMutation::INVALID_DYNAMIC_INTEGRITY:
        message.integrity->storage_at_payload_end = false;
        break;
      case pae::test_support::VariableDraftMutation::LATE_BIT_CONTAINER:
        message.bit_containers.push_back(BitContainerPlan{
            "late_bits", 5U, 1U, ByteOrder::NOT_APPLICABLE, BitNumbering::LSB0, 0U});
        draft.draft_->resource_requirements.total_bit_container_count = 1U;
        break;
      case pae::test_support::VariableDraftMutation::MISSING_COMPUTED_LENGTH:
        message.fields.erase(message.fields.begin());
        message.bounded_payload->payload_field_index = 0U;
        message.computed_length.reset();
        draft.draft_->resource_requirements.total_field_count = 1U;
        draft.draft_->resource_requirements.total_computed_length_count = 0U;
        break;
      case pae::test_support::VariableDraftMutation::HEADER_GAP:
        message.fields[1].byte_offset = 3U;
        message.bounded_payload->header_length = 3U;
        message.bounded_payload->min_frame_length = 4U;
        message.bounded_payload->max_frame_length = 7U;
        message.frame_length_bytes = 7U;
        message.computed_length->expected_value = 7U;
        draft.draft_->resource_requirements.max_frame_bytes = 7U;
        break;
      case pae::test_support::VariableDraftMutation::REGION_COMPUTED_LENGTH:
        message.computed_length->scope = ComputedLengthScope::REGION;
        message.computed_length->range_offset = 0U;
        message.computed_length->range_length = 6U;
        break;
      case pae::test_support::VariableDraftMutation::TRAILER_LENGTH_MISMATCH:
        message.bounded_payload->trailer_length = 2U;
        message.bounded_payload->min_frame_length = 4U;
        message.bounded_payload->max_frame_length = 7U;
        message.frame_length_bytes = 7U;
        message.computed_length->expected_value = 7U;
        draft.draft_->resource_requirements.max_frame_bytes = 7U;
        break;
      case pae::test_support::VariableDraftMutation::TRAILER_WITHOUT_INTEGRITY:
        message.integrity.reset();
        draft.draft_->resource_requirements.total_integrity_rule_count = 0U;
        break;
      case pae::test_support::VariableDraftMutation::CRC16_TRAILER_MISMATCH:
        message.integrity->algorithm = IntegrityAlgorithm::CRC;
        message.integrity->crc_width = 16U;
        message.integrity->crc_polynomial = 0x1021U;
        message.integrity->crc_initial_value = 0xFFFFU;
        message.integrity->storage_byte_order = ByteOrder::BIG;
        break;
    }
    return draft;
  }
#endif

  static BudgetedPlanDraft MakeInt64DraftWithOutOfRangeConstant() {
    auto draft = MakeIntegrityDraft();
    draft.draft_->schema_version = "0.4";
    auto& field = draft.draft_->messages[0].fields[0];
    field.value_type = ValueType::INT64;
    field.encode_source = EncodeSource::CONSTANT;
    field.signed_constant_value = 128;
    draft.draft_->messages[0].integrity.reset();
    draft.draft_->resource_requirements.total_integrity_rule_count = 0U;
    return draft;
  }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  static BudgetedPlanDraft MutateConversionDraft(
      BudgetedPlanDraft draft, pae::test_support::ConversionDraftMutation mutation) {
    auto& data = *draft.draft_;
    switch (mutation) {
      case pae::test_support::ConversionDraftMutation::CORRUPTED_COEFFICIENT:
        ++data.conversions[0].scale_coefficient.words[0];
        break;
      case pae::test_support::ConversionDraftMutation::INVALID_INDEX:
        data.messages[0].fields[1].conversion_index = data.conversions.size();
        break;
      case pae::test_support::ConversionDraftMutation::RAW_TYPE_MISMATCH:
        data.conversions[0].raw_value_type = ValueType::INT64;
        break;
      case pae::test_support::ConversionDraftMutation::UNREFERENCED_DESCRIPTOR:
        data.messages[0].fields[1].conversion_index = kInvalidPlanBuildIndex;
        break;
      case pae::test_support::ConversionDraftMutation::DUPLICATE_REFERENCE:
        data.messages[0].fields[2].conversion_index = 0U;
        break;
      case pae::test_support::ConversionDraftMutation::OLD_SCHEMA_RESIDUE:
        data.schema_version = "0.4";
        break;
      case pae::test_support::ConversionDraftMutation::BITFIELD_REFERENCE:
        data.messages[0].fields[2].conversion_index = 0U;
        break;
      case pae::test_support::ConversionDraftMutation::CONSTANT_REFERENCE:
        data.messages[0].fields[1].encode_source = EncodeSource::CONSTANT;
        data.messages[0].fields[1].constant_value = 1U;
        break;
      case pae::test_support::ConversionDraftMutation::RESOURCE_COUNT_MISMATCH:
        ++data.resource_requirements.total_conversion_count;
        break;
    }
    return draft;
  }
#endif

  static BudgetedPlanDraft ConfigurePlanMemoryFailure(BudgetedPlanDraft draft,
                                                      std::size_t fail_at_allocation,
                                                      test_only::PlanMemoryTestProbe* probe) {
    if (draft.draft_ != nullptr) {
      draft.draft_->test_fail_at_allocation = fail_at_allocation;
      draft.draft_->test_memory_probe = probe;
    }
    return draft;
  }

  static BudgetedPlanDraft CorruptApprovedPlanMemory(BudgetedPlanDraft draft) {
    if (draft.draft_ != nullptr) {
      ++draft.draft_->approved_plan_memory.string_bytes;
    }
    return draft;
  }
};

}  // namespace pae::protocol_plan::test_only

namespace pae::test_support {

protocol_plan::BudgetedPlanDraft SetDraftSchemaVersion(protocol_plan::BudgetedPlanDraft draft,
                                                       const char* version) {
  return protocol_plan::test_only::PlanBuilderTestPeer::SetDraftSchemaVersion(std::move(draft),
                                                                              version);
}
protocol_plan::BudgetedPlanDraft SetFramingLengthOffset(protocol_plan::BudgetedPlanDraft draft,
                                                        std::uint64_t offset) {
  return protocol_plan::test_only::PlanBuilderTestPeer::SetFramingLengthOffset(std::move(draft),
                                                                               offset);
}
protocol_plan::BudgetedPlanDraft SetFramingLengthByteOrder(protocol_plan::BudgetedPlanDraft draft,
                                                           protocol_plan::ByteOrder byte_order) {
  return protocol_plan::test_only::PlanBuilderTestPeer::SetFramingLengthByteOrder(std::move(draft),
                                                                                  byte_order);
}
protocol_plan::BudgetedPlanDraft InjectSignedConstant(protocol_plan::BudgetedPlanDraft draft) {
  return protocol_plan::test_only::PlanBuilderTestPeer::InjectSignedConstant(std::move(draft));
}

protocol_plan::BudgetedPlanDraft MakeCorruptedBudgetedPlanDraft() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCorruptedBudgetedPlanDraft();
}

protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithInvalidByteOrder() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeBitfieldDraft(
      protocol_plan::ByteOrder::BIG, protocol_plan::BitNumbering::LSB0);
}

protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithInvalidBitNumbering() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeBitfieldDraft(
      protocol_plan::ByteOrder::NOT_APPLICABLE, static_cast<protocol_plan::BitNumbering>(255));
}

protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithMatcherConflict() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeBitfieldDraft(
      protocol_plan::ByteOrder::NOT_APPLICABLE, protocol_plan::BitNumbering::LSB0, true);
}

protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithTooManyMessageContainers() {
  return protocol_plan::test_only::PlanBuilderTestPeer::
      MakeBitfieldDraftWithTooManyMessageContainers();
}

protocol_plan::BudgetedPlanDraft MakeBitfieldDraftWithTooManyTotalContainers() {
  return protocol_plan::test_only::PlanBuilderTestPeer::
      MakeBitfieldDraftWithTooManyTotalContainers();
}

protocol_plan::BudgetedPlanDraft MakeIntegrityDraftWithUnknownAlgorithm() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeIntegrityDraftWithUnknownAlgorithm();
}

protocol_plan::BudgetedPlanDraft MakeIntegrityDraftWithSelfIncludedStorage() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeIntegrityDraftWithSelfIncludedStorage();
}

protocol_plan::BudgetedPlanDraft MakeIntegrityDraftWithFieldStorageConflict() {
  return protocol_plan::test_only::PlanBuilderTestPeer::
      MakeIntegrityDraftWithFieldStorageConflict();
}

#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
protocol_plan::BudgetedPlanDraft MakeCrcDraftWithInvalidWidth() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCrcDraftWithInvalidWidth();
}

protocol_plan::BudgetedPlanDraft MakeCrcDraftWithEvenPolynomial() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCrcDraftWithEvenPolynomial();
}

protocol_plan::BudgetedPlanDraft MakeCrcDraftWithInvalidStorageOrder() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCrcDraftWithInvalidStorageOrder();
}

protocol_plan::BudgetedPlanDraft MakeCrcDraftWithOldSchema() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCrcDraftWithOldSchema();
}
#endif

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
protocol_plan::BudgetedPlanDraft MakeLengthDraftWithIncorrectExpectedValue() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeLengthDraftWithIncorrectExpectedValue();
}

protocol_plan::BudgetedPlanDraft MakeLengthDraftWithInvalidScope() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeLengthDraftWithInvalidScope();
}

protocol_plan::BudgetedPlanDraft MakeLengthDraftWithOldSchema() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeLengthDraftWithOldSchema();
}

protocol_plan::BudgetedPlanDraft MakeLengthDraftWithResourceCountMismatch() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeLengthDraftWithResourceCountMismatch();
}
#endif

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
protocol_plan::BudgetedPlanDraft MakeCorruptedVariableDraft(VariableDraftMutation mutation) {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCorruptedVariableDraft(mutation);
}
#endif

protocol_plan::BudgetedPlanDraft MakeInt64DraftWithOutOfRangeConstant() {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeInt64DraftWithOutOfRangeConstant();
}

#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
protocol_plan::BudgetedPlanDraft MakeCorruptedStreamDraft(StreamDraftMutation mutation) {
  return protocol_plan::test_only::PlanBuilderTestPeer::MakeCorruptedStreamDraft(mutation);
}
#endif

#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
protocol_plan::BudgetedPlanDraft MutateAsciiTextDraft(protocol_plan::BudgetedPlanDraft draft,
                                                      AsciiTextDraftMutation mutation) {
  return protocol_plan::test_only::PlanBuilderTestPeer::MutateAsciiTextDraft(std::move(draft),
                                                                             mutation);
}
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
protocol_plan::BudgetedPlanDraft MutateAsciiStreamDraft(protocol_plan::BudgetedPlanDraft draft,
                                                        AsciiStreamDraftMutation mutation) {
  return protocol_plan::test_only::PlanBuilderTestPeer::MutateAsciiStreamDraft(std::move(draft),
                                                                               mutation);
}
#endif
#endif

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
protocol_plan::BudgetedPlanDraft MutateConversionDraft(protocol_plan::BudgetedPlanDraft draft,
                                                       ConversionDraftMutation mutation) {
  return protocol_plan::test_only::PlanBuilderTestPeer::MutateConversionDraft(std::move(draft),
                                                                              mutation);
}
#endif

protocol_plan::BudgetedPlanDraft ConfigurePlanMemoryFailure(
    protocol_plan::BudgetedPlanDraft draft, std::size_t fail_at_allocation,
    protocol_plan::test_only::PlanMemoryTestProbe* probe) {
  return protocol_plan::test_only::PlanBuilderTestPeer::ConfigurePlanMemoryFailure(
      std::move(draft), fail_at_allocation, probe);
}

protocol_plan::BudgetedPlanDraft CorruptApprovedPlanMemory(protocol_plan::BudgetedPlanDraft draft) {
  return protocol_plan::test_only::PlanBuilderTestPeer::CorruptApprovedPlanMemory(std::move(draft));
}

}  // namespace pae::test_support
