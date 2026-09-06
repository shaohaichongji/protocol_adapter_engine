#include "plan_builder_test_peer.h"

#include <memory>
#include <utility>

#include "../../src/protocol_plan/plan_draft_internal.h"

namespace pae::protocol_plan::test_only {

class PlanBuilderTestPeer final {
 public:
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
