#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "frozen_storage.h"
#include "plan_memory.h"
#include "plan_types.h"

namespace pae::protocol_plan {

class PlanBuilder;

// Mutable compiler/test transfer objects. Frozen PlanBundle storage uses the Frozen* types below.
// 三层表示：可变传递对象 → 冻结的配置事实 → 热路径执行描述。
// 后两层在同一 Plan 存储中，但用途不同；默认初始化值不代表配置已通过准入。

struct FramingPlan {
  std::string id;
  InputKind input_kind = InputKind::COMPLETE_RECORD;
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  FramingStrategy strategy = FramingStrategy::COMPLETE_RECORD;
  std::uint64_t frame_length_bytes = 0U;
  std::vector<std::uint8_t> sync_bytes;
  std::vector<std::size_t> sync_prefix_table;
  std::uint64_t length_field_offset = 0U;
  std::uint64_t length_field_width = 0U;
  ByteOrder length_field_byte_order = ByteOrder::NOT_APPLICABLE;
  std::uint64_t minimum_frame_length = 0U;
  std::uint64_t maximum_frame_length = 0U;
#endif
};

struct MatcherPlan {
  MatcherKind kind = MatcherKind::FRAME_LENGTH_EQUALS;
  std::uint64_t length_bytes = 0U;
  std::uint64_t byte_offset = 0U;
  std::vector<std::uint8_t> bytes;
};

struct EnumEntryPlan {
  std::string id;
  std::uint64_t raw_value = 0U;
};

// byte_* 以字节计，bit_* 以位计；容器索引属于本 Message，conversion 索引属于包级数组。
struct FieldPlan {
  std::string id;
  ValueType value_type = ValueType::UINT64;
  WireCodec wire_codec = WireCodec::UNSIGNED_INTEGER;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  EncodeSource encode_source = EncodeSource::INPUT;
  std::optional<std::uint64_t> constant_value;
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  std::vector<EnumEntryPlan> enum_entries;
  std::size_t bit_container_index = static_cast<std::size_t>(-1);
  std::uint64_t bit_offset = 0U;
  std::uint64_t bit_width = 0U;
  std::optional<std::int64_t> signed_constant_value;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t conversion_index = static_cast<std::size_t>(-1);
#endif
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::uint64_t text_min_length = 0U;
  std::uint64_t text_max_length = 0U;
  std::uint64_t allowed_ascii_low = 0U;
  std::uint64_t allowed_ascii_high = 0U;
#endif
};

#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
struct TextSegmentPlan {
  TextSegmentKind kind = TextSegmentKind::LITERAL;
  std::vector<std::uint8_t> literal;
  std::vector<std::size_t> prefix_table;
  std::size_t field_index = static_cast<std::size_t>(-1);
};

struct TextActionPlan {
  std::vector<TextSegmentPlan> segments;
  std::uint64_t min_record_length = 0U;
  std::uint64_t max_record_length = 0U;
};

struct AsciiTextPlan {
  std::optional<TextActionPlan> decode;
  std::optional<TextActionPlan> encode;
};
#endif

struct BitContainerPlan {
  std::string id;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  BitNumbering bit_numbering = BitNumbering::LSB0;
  std::uint64_t base_value = 0U;
};

struct IntegrityPlan {
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
};

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
struct BoundedPayloadPlan {
  std::size_t payload_field_index = static_cast<std::size_t>(-1);
  std::uint64_t header_length = 0U;
  std::uint64_t min_payload_length = 0U;
  std::uint64_t max_payload_length = 0U;
  std::uint64_t trailer_length = 0U;
  std::uint64_t min_frame_length = 0U;
  std::uint64_t max_frame_length = 0U;
};
#endif

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
struct ComputedLengthPlan {
  std::size_t field_index = static_cast<std::size_t>(-1);
  std::uint64_t storage_offset = 0U;
  std::uint64_t storage_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  ComputedLengthScope scope = ComputedLengthScope::FRAME;
  std::uint64_t range_offset = 0U;
  std::uint64_t range_length = 0U;
  std::uint64_t expected_value = 0U;
};
#endif

struct MessagePlan {
  std::string id;
  std::string direction_id;
  std::uint64_t frame_length_bytes = 0U;
  std::vector<MatcherPlan> matchers;
  std::vector<BitContainerPlan> bit_containers;
  std::optional<IntegrityPlan> integrity;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  std::optional<BoundedPayloadPlan> bounded_payload;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::optional<ComputedLengthPlan> computed_length;
#endif
  std::vector<FieldPlan> fields;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::optional<AsciiTextPlan> ascii_text;
#endif
};

// 引用已解析为包级索引，不再查找配置字符串；这不是传输注册或运行时业务路由。
struct PipelinePlan {
  std::string id;
  std::string direction_id;
  std::size_t framing_profile_index = 0U;
  std::vector<std::size_t> message_indices;
};

// 冻结的配置事实用于查询和关联；字符串借用 Arena，数组接管元素析构而非内存释放。
struct FrozenFramingPlan {
  FrozenString id;
  InputKind input_kind = InputKind::COMPLETE_RECORD;
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  FramingStrategy strategy = FramingStrategy::COMPLETE_RECORD;
  std::uint64_t frame_length_bytes = 0U;
  FrozenArray<std::uint8_t> sync_bytes;
  FrozenArray<std::size_t> sync_prefix_table;
  std::uint64_t length_field_offset = 0U;
  std::uint64_t length_field_width = 0U;
  ByteOrder length_field_byte_order = ByteOrder::NOT_APPLICABLE;
  std::uint64_t minimum_frame_length = 0U;
  std::uint64_t maximum_frame_length = 0U;
#endif
};

struct FrozenMatcherPlan {
  MatcherKind kind = MatcherKind::FRAME_LENGTH_EQUALS;
  std::uint64_t length_bytes = 0U;
  std::uint64_t byte_offset = 0U;
  FrozenArray<std::uint8_t> bytes;
};

struct FrozenEnumEntryPlan {
  FrozenString id;
  std::uint64_t raw_value = 0U;
};

struct FrozenFieldPlan {
  FrozenString id;
  ValueType value_type = ValueType::UINT64;
  WireCodec wire_codec = WireCodec::UNSIGNED_INTEGER;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  EncodeSource encode_source = EncodeSource::INPUT;
  std::optional<std::uint64_t> constant_value;
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  FrozenArray<FrozenEnumEntryPlan> enum_entries;
  std::size_t bit_container_index = static_cast<std::size_t>(-1);
  std::uint64_t bit_offset = 0U;
  std::uint64_t bit_width = 0U;
  std::optional<std::int64_t> signed_constant_value;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t conversion_index = static_cast<std::size_t>(-1);
#endif
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::uint64_t text_min_length = 0U;
  std::uint64_t text_max_length = 0U;
  std::uint64_t allowed_ascii_low = 0U;
  std::uint64_t allowed_ascii_high = 0U;
#endif
};

struct FrozenBitContainerPlan {
  FrozenString id;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  BitNumbering bit_numbering = BitNumbering::LSB0;
  std::uint64_t base_value = 0U;
};

struct FrozenIntegrityPlan {
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
};

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
using FrozenBoundedPayloadPlan = BoundedPayloadPlan;
#endif

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
using FrozenComputedLengthPlan = ComputedLengthPlan;
#endif

struct FrozenMessagePlan {
  FrozenString id;
  FrozenString direction_id;
  std::uint64_t frame_length_bytes = 0U;
  FrozenArray<FrozenMatcherPlan> matchers;
  FrozenArray<FrozenBitContainerPlan> bit_containers;
  std::optional<FrozenIntegrityPlan> integrity;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  std::optional<FrozenBoundedPayloadPlan> bounded_payload;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::optional<FrozenComputedLengthPlan> computed_length;
#endif
  FrozenArray<FrozenFieldPlan> fields;
};

// 执行描述保存已核验的机器尺寸和基础值；位成员的掩码/移位在 FieldExecutionPlan。
struct BitContainerExecutionPlan {
  std::size_t offset = 0U;
  std::size_t width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  std::uint64_t base_value = 0U;
};

struct FrozenPipelinePlan {
  FrozenString id;
  FrozenString direction_id;
  std::size_t framing_profile_index = 0U;
  FrozenArray<std::size_t> message_indices;
};

struct FixedByteExecutionPlan {
  std::size_t offset = 0U;
  std::uint8_t value = 0U;
};

struct EnumLookupExecutionPlan {
  std::uint64_t raw_value = 0U;
  std::size_t entry_index = 0U;
};

// 热路径读取预计算布局，不解释配置 JSON。普通字段 offset/width 是 Frame 字节范围，
// 位成员指向容器并使用 bit_mask/bit_shift；input_ordinal 只为动态输入分配。
// 枚举 begin/count 索引本 Message 的执行表，不能当全包字段索引。
struct FieldExecutionPlan {
  std::size_t offset = 0U;
  std::size_t width = 0U;
  std::size_t input_ordinal = 0U;
  ValueType value_type = ValueType::UINT64;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  EncodeSource encode_source = EncodeSource::INPUT;
  bool has_constant = false;
  std::uint64_t constant_value = 0U;
  UnknownEnumPolicy unknown_enum_policy = UnknownEnumPolicy::REJECT;
  std::size_t enum_values_begin = 0U;
  std::size_t enum_values_count = 0U;
  std::size_t enum_lookup_begin = 0U;
  std::size_t enum_lookup_count = 0U;
  std::size_t bit_container_index = static_cast<std::size_t>(-1);
  std::uint64_t bit_mask = 0U;
  std::uint8_t bit_shift = 0U;
  std::uint8_t bit_width = 0U;
  std::int64_t signed_constant_value = 0;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t conversion_index = static_cast<std::size_t>(-1);
  std::size_t conversion_slot = static_cast<std::size_t>(-1);
#endif
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::size_t text_min_length = 0U;
  std::size_t text_max_length = 0U;
  std::uint64_t allowed_ascii_low = 0U;
  std::uint64_t allowed_ascii_high = 0U;
  bool text_encode_input = false;
#endif
};

#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
struct TextSegmentExecutionPlan {
  TextSegmentKind kind = TextSegmentKind::LITERAL;
  FrozenArray<std::uint8_t> literal;
  FrozenArray<std::size_t> prefix_table;
  std::size_t field_index = static_cast<std::size_t>(-1);
};

struct TextActionExecutionPlan {
  FrozenArray<TextSegmentExecutionPlan> segments;
  std::size_t min_record_length = 0U;
  std::size_t max_record_length = 0U;
};
#endif

// 描述是冻结的执行材料，不保存单帧业务值；可选文本方向与 Binary 描述按已准入分支使用。
struct MessageExecutionPlan {
  std::size_t frame_size = 0U;
  std::size_t required_input_count = 0U;
  std::optional<FrozenIntegrityPlan> integrity;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  std::optional<FrozenBoundedPayloadPlan> bounded_payload;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::optional<FrozenComputedLengthPlan> computed_length;
#endif
  FrozenArray<FixedByteExecutionPlan> fixed_bytes;
  FrozenArray<BitContainerExecutionPlan> bit_containers;
  FrozenArray<FieldExecutionPlan> fields;
  FrozenArray<std::uint64_t> enum_raw_values;
  FrozenArray<EnumLookupExecutionPlan> enum_lookup_entries;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::optional<TextActionExecutionPlan> text_decode;
  std::optional<TextActionExecutionPlan> text_encode;
  std::size_t text_decode_field_count = 0U;
#endif
};

// 固定长度结构候选组，不表示完整性或字段语义通过，也不能据此合并 Pipeline 身份。
struct CandidateGroupExecutionPlan {
  std::size_t frame_size = 0U;
  FrozenArray<std::size_t> message_indices;
};

// allowed 位图以全包消息索引置位；文本和变长候选另存索引，分组仅用于结构筛选。
struct PipelineExecutionPlan {
  std::size_t framing_profile_index = 0U;
  FrozenArray<std::uint64_t> allowed_message_words;
  FrozenArray<CandidateGroupExecutionPlan> candidate_groups;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  FrozenArray<std::size_t> variable_message_indices;
#endif
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  FrozenArray<std::size_t> text_message_indices;
#endif
};

// 按各消息最大需求决定 Workspace 槽容量；estimated_workspace_bytes 是逻辑计费，非 RSS。
struct ExecutionResourceLayout {
  std::size_t max_fields_per_message = 0U;
  std::size_t max_input_fields_per_message = 0U;
  std::size_t encode_value_index_count = 0U;
  std::size_t encode_presence_word_count = 0U;
  std::size_t bit_container_value_count = 0U;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t conversion_value_count = 0U;
#endif
  std::size_t estimated_workspace_bytes = 0U;
};

// 对象置于 PlanOwner 的存储块内，自身不可复制/移动；转移 owner 不搬动这个对象。
// 成员数组负责析构，存储块由 owner 释放。
// 查询的视图/引用为借用，不增加引用或延长寿命；枚举值返回为独立值。
// Frozen/const 访问不承诺任意共享操作线程安全、稳定 ABI 或进程内存上限。
class PlanBundle final {
 public:
  PlanBundle(const PlanBundle&) = delete;
  PlanBundle& operator=(const PlanBundle&) = delete;
  PlanBundle(PlanBundle&&) noexcept = delete;
  PlanBundle& operator=(PlanBundle&&) noexcept = delete;
  ~PlanBundle() = default;

  std::string_view SchemaVersion() const noexcept;
  std::string_view ProtocolId() const noexcept;
  std::string_view ProtocolVersion() const noexcept;
  ResourceProfile GetResourceProfile() const noexcept;
  const ResourceRequirements& GetResourceRequirements() const noexcept;
  const FrozenArray<FrozenFramingPlan>& FramingProfiles() const noexcept;
  const FrozenArray<FrozenPipelinePlan>& Pipelines() const noexcept;
  const FrozenArray<FrozenMessagePlan>& Messages() const noexcept;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  const FrozenArray<LinearConversionDescriptor>& Conversions() const noexcept;
#endif
  const ExecutionResourceLayout& GetExecutionResourceLayout() const noexcept;
  const FrozenArray<MessageExecutionPlan>& MessageExecutionPlans() const noexcept;
  const FrozenArray<PipelineExecutionPlan>& PipelineExecutionPlans() const noexcept;
  const PlanMemoryReport& GetPlanMemoryReport() const noexcept;

 private:
  friend class PlanBuilder;
  friend class PlanOwner;

  PlanBundle(FrozenString schema_version, FrozenString protocol_id, FrozenString protocol_version,
             ResourceProfile resource_profile, ResourceRequirements resource_requirements,
             FrozenArray<FrozenFramingPlan> framing_profiles,
             FrozenArray<FrozenPipelinePlan> pipelines, FrozenArray<FrozenMessagePlan> messages,
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
             FrozenArray<LinearConversionDescriptor> conversions,
#endif
             ExecutionResourceLayout execution_resource_layout,
             FrozenArray<MessageExecutionPlan> message_execution_plans,
             FrozenArray<PipelineExecutionPlan> pipeline_execution_plans,
             PlanMemoryReport memory_report) noexcept;

  FrozenString schema_version_;
  FrozenString protocol_id_;
  FrozenString protocol_version_;
  ResourceProfile resource_profile_ = ResourceProfile::DESKTOP;
  ResourceRequirements resource_requirements_;
  FrozenArray<FrozenFramingPlan> framing_profiles_;
  FrozenArray<FrozenPipelinePlan> pipelines_;
  FrozenArray<FrozenMessagePlan> messages_;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  FrozenArray<LinearConversionDescriptor> conversions_;
#endif
  ExecutionResourceLayout execution_resource_layout_;
  FrozenArray<MessageExecutionPlan> message_execution_plans_;
  FrozenArray<PipelineExecutionPlan> pipeline_execution_plans_;
  PlanMemoryReport memory_report_;
};

}  // namespace pae::protocol_plan
