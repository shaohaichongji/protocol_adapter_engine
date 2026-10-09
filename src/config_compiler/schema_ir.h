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

// 编译期自有 IR：字符串和数组由各节点拥有，不借用 JSON DOM 或调用方输入。
// 默认成员值只用于初始化；可接受的配置仍由 Schema 版本、构建开关和校验共同决定。
// IR 尚不是执行 Plan，运行热路径不解释这里的字符串或可变容器。
using protocol_plan::BitNumbering;
using protocol_plan::ByteOrder;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
using protocol_plan::ComputedLengthScope;
#endif
using protocol_plan::EncodeSource;
using protocol_plan::InputKind;
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
using protocol_plan::FramingStrategy;
#endif
using protocol_plan::IntegrityAlgorithm;
using protocol_plan::MatcherKind;
using protocol_plan::ResourceProfile;
using protocol_plan::ResourceRequirements;
using protocol_plan::UnknownEnumPolicy;
using protocol_plan::ValueType;
using protocol_plan::WireCodec;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
using protocol_plan::TextSegmentKind;
#endif

// 保存配置位置以定位诊断，不保存 yyjson 节点指针；JSON Pointer 与输入字节偏移不同。
struct ConfigOrigin {
  std::string json_pointer;
};

struct FramingProfileIr {
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  InputKind input_kind = InputKind::COMPLETE_RECORD;
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  FramingStrategy strategy = FramingStrategy::COMPLETE_RECORD;
  std::uint64_t frame_length_bytes = 0U;
  std::vector<std::uint8_t> sync_bytes;
  std::uint64_t length_field_offset = 0U;
  std::uint64_t length_field_width = 0U;
  ByteOrder length_field_byte_order = ByteOrder::NOT_APPLICABLE;
  std::uint64_t minimum_frame_length = 0U;
  std::uint64_t maximum_frame_length = 0U;
#endif
  ConfigOrigin origin;
};

// 关联仍使用符号 ID；领域校验解析引用及方向，不表示传输端点或业务路由。
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

// byte_offset/byte_width 以字节计，bit_offset/bit_width 以位计；只解释当前 codec 的成员。
struct WireIr {
  WireCodec codec = WireCodec::UNSIGNED_INTEGER;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  std::string container_id;
  std::uint64_t bit_offset = 0U;
  std::uint64_t bit_width = 0U;
  // 未解析哨兵；成功校验后索引本 Message 的 bit_containers，不是全局容器索引。
  std::size_t bit_container_index = static_cast<std::size_t>(-1);
  ConfigOrigin origin;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::uint64_t text_min_length = 0U;
  std::uint64_t text_max_length = 0U;
  std::uint64_t allowed_ascii_low = 0U;
  std::uint64_t allowed_ascii_high = 0U;
#endif
};

#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
// literal 自有字节；FIELD 段的符号引用在领域校验后成为本 Message 的字段索引。
struct TextSegmentIr {
  TextSegmentKind kind = TextSegmentKind::LITERAL;
  std::vector<std::uint8_t> literal;
  std::string field_id;
  std::size_t field_index = static_cast<std::size_t>(-1);
  ConfigOrigin origin;
};

struct TextActionIr {
  std::vector<TextSegmentIr> segments;
  std::uint64_t min_record_length = 0U;
  std::uint64_t max_record_length = 0U;
};

// 两个方向各自可选；存在一个方向不隐含另一个方向也能执行。
struct AsciiTextLayoutIr {
  std::optional<TextActionIr> decode;
  std::optional<TextActionIr> encode;
  ConfigOrigin origin;
};
#endif

// 容器 ID 与字段 ID 是 Message 内不同引用空间。字节序作用于容器，位编号作用于成员。
// base_value 是 Encode 未覆盖位的基础值，不是 Decode 保留位必须相等的业务约束。
struct BitContainerIr {
  std::string id;
  std::uint64_t byte_offset = 0U;
  std::uint64_t byte_width = 0U;
  ByteOrder byte_order = ByteOrder::NOT_APPLICABLE;
  BitNumbering bit_numbering = BitNumbering::LSB0;
  std::uint64_t base_value = 0U;
  ConfigOrigin origin;
};

// 静态范围以字节计；启用变长切片时，payload 锚点由对应标志区分，不能当固定偏移。
// 算法及参数在编译期检查；默认 SUM8 不代表缺省配置自动创建校验规则。
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
// payload_field_index 引用本 Message 的字段；帧长度上下界由领域校验安全推导。
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

// 有符号与无符号常量独立保存；source 决定 Encode 取值，不扩展常量的接收约束。
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

// scale/bias 保留精确有理数；derived 是领域校验产生的执行描述，不是逐帧表达式。
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

// 消息拥有布局和字段；Binary 与 ASCII 的准入分支不同，不能任意混合这些描述。
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
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::optional<AsciiTextLayoutIr> ascii_text;
#endif
  ConfigOrigin origin;
};

// 包级自有根节点；resource_profile 默认值不能替代 Loader 对必需配置属性的检查。
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

// 索引分别指向 SchemaIr.framing_profiles/messages，消息顺序保留 Pipeline 声明顺序。
struct ResolvedPipelineIr {
  std::size_t framing_profile_index = 0U;
  std::vector<std::size_t> message_indices;
};

class DomainValidator;
class ResourceBudgetValidator;
class PlanDraftAssembler;
class ProtocolMetadataBuilder;

// Internal capability state: only DomainValidator can create it. This type is deliberately
// move-only so validation authority cannot be copied or synthesized by setting a public flag.
// 正常链路由 DomainValidator 在全部领域校验通过后发布；private/friend 限制构造入口。
// Payload 一起拥有 IR、解析后的引用和资源需求；需求统计尚不表示资源预算获批。
// 移动转移唯一所有权，moved-from 对象不再是可复用的阶段输入。
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
  friend class ProtocolMetadataBuilder;

  std::unique_ptr<Payload> payload_;
};

// Internal capability state: only ResourceBudgetValidator can promote a validated SchemaIr.
// 正常链路由预算阶段提升凭证；保留验证产物、不可变 Plan 计费报告和实际批准上限。
// 预算不是 JSON DOM/临时 IR/进程 RSS 的全链峰值约束，也不表示 Plan 已组装或冻结。
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
  friend class ProtocolMetadataBuilder;

  std::unique_ptr<ValidatedSchemaIr> validated_;
  protocol_plan::PlanMemoryReport plan_memory_;
  std::size_t plan_memory_limit_bytes_ = 0U;
};

}  // namespace pae::config_compiler
