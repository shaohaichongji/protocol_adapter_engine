#pragma once

#include <string>
#include <vector>

#include "plan_bundle.h"

namespace pae::protocol_plan::detail {

// Raw plan material is an internal payload. Production PlanBuilder entry points never accept this
// type directly; it can only be wrapped by the configuration compiler after validation and budget
// admission have both succeeded.
// 编译期自有字符串/数组及已解析索引；正常生产入口必须携带前两阶段的批准结果。
// 测试友元能构造或破坏原始材料，因此 Builder 仍做防御性复核，不重新解释 JSON。
struct PlanDraftData {
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
  ResourceRequirements resource_requirements;
  std::vector<FramingPlan> framing_profiles;
  std::vector<PipelinePlan> pipelines;
  std::vector<MessagePlan> messages;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::vector<LinearConversionDescriptor> conversions;
#endif
  // 批准报告随草稿移交，Freeze 按 Prepared 实际布局复算并逐类别核对，不能只比较总字节数。
  PlanMemoryReport approved_plan_memory;
  std::size_t plan_memory_limit_bytes = 0U;
  std::size_t test_fail_at_allocation = 0U;
  test_only::PlanMemoryTestProbe* test_memory_probe = nullptr;
};

// 冻结前临时自有容器：保存预计算结果，之后复制到 Arena 的 MessageExecutionPlan。
// 这里的 vector/optional 并不是已经发布的 FrozenArray 或每帧 Workspace。
struct PreparedMessageExecutionPlan {
  std::size_t frame_size = 0U;
  std::size_t required_input_count = 0U;
  std::optional<FrozenIntegrityPlan> integrity;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  std::optional<FrozenBoundedPayloadPlan> bounded_payload;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::optional<FrozenComputedLengthPlan> computed_length;
#endif
  std::vector<FixedByteExecutionPlan> fixed_bytes;
  std::vector<BitContainerExecutionPlan> bit_containers;
  std::vector<FieldExecutionPlan> fields;
  std::vector<std::uint64_t> enum_raw_values;
  std::vector<EnumLookupExecutionPlan> enum_lookup_entries;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::optional<TextActionPlan> text_decode;
  std::optional<TextActionPlan> text_encode;
  std::size_t text_decode_field_count = 0U;
#endif
};

// 固定长度候选按字节长度分组，索引指向全包消息；候选存在不表示字段或完整性校验成功。
struct PreparedCandidateGroupExecutionPlan {
  std::size_t frame_size = 0U;
  std::vector<std::size_t> message_indices;
};

// allowed 位图覆盖包级消息空间；变长及文本候选另存索引，不强行按固定长度分组。
struct PreparedPipelineExecutionPlan {
  std::size_t framing_profile_index = 0U;
  std::vector<std::uint64_t> allowed_message_words;
  std::vector<PreparedCandidateGroupExecutionPlan> candidate_groups;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  std::vector<std::size_t> variable_message_indices;
#endif
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::vector<std::size_t> text_message_indices;
#endif
};

}  // namespace pae::protocol_plan::detail
