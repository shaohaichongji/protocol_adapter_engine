#pragma once

// 严格 JSON 配置编译入口：配置字节经检查形成冻结 Plan 和只读描述，供 Codec/Framer/Host
// 创建执行对象；本层不负责文件读取、网络收发或设备生命周期。

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "pae/export.h"
#include "pae/protocol_description.h"

namespace pae {

namespace detail {
class CompiledProtocolAccess;
}

// 失败发生的检查阶段；结合 CompileError 和位置定位，不能只把所有错误视作 JSON 语法错误。
enum class CompileStage {
  INPUT_PROFILE,
  JSON_SYNTAX,
  JSON_RESOURCE,
  STRUCTURAL,
  DOMAIN_VALIDATION,
  RESOURCE_BUDGET,
  PLAN_BUILD,
  INTERNAL,
};

enum class CompileError {
  NONE,
  EMPTY_INPUT,
  INPUT_LIMIT_EXCEEDED,
  UTF8_BOM_NOT_ALLOWED,
  INVALID_UTF8,
  INVALID_UNICODE_ESCAPE,
  NESTING_DEPTH_LIMIT_EXCEEDED,
  JSON_SYNTAX_ERROR,
  JSON_ALLOCATION_FAILED,
  JSON_PARSER_MEMORY_LIMIT_EXCEEDED,
  JSON_NODE_LIMIT_EXCEEDED,
  OBJECT_MEMBER_LIMIT_EXCEEDED,
  ARRAY_ELEMENT_LIMIT_EXCEEDED,
  STRING_LIMIT_EXCEEDED,
  DECODED_STRING_BUDGET_EXCEEDED,
  NUMBER_TOKEN_LIMIT_EXCEEDED,
  DUPLICATE_KEY,
  ROOT_MUST_BE_OBJECT,
  MISSING_PROPERTY,
  UNKNOWN_PROPERTY,
  TYPE_MISMATCH,
  INVALID_STRING_LENGTH,
  INVALID_ID,
  INVALID_ENUM_VALUE,
  INTEGER_NOT_EXACT,
  INTEGER_OUT_OF_RANGE,
  INVALID_HEX_BYTES,
  EMPTY_ARRAY,
  UNSUPPORTED_FEATURE,
  DUPLICATE_ID,
  DUPLICATE_REFERENCE,
  UNKNOWN_REFERENCE,
  DIRECTION_MISMATCH,
  FIELD_OUT_OF_BOUNDS,
  FIELD_OVERLAP,
  FRAME_NOT_FULLY_DEFINED,
  VALUE_NOT_REPRESENTABLE,
  MATCHER_OUT_OF_BOUNDS,
  MATCHER_CONFLICT,
  INTEGRITY_RANGE_OUT_OF_BOUNDS,
  INTEGRITY_STORAGE_OUT_OF_BOUNDS,
  INTEGRITY_SELF_INCLUDED,
  INTEGRITY_STORAGE_CONFLICT,
  AMBIGUOUS_MATCHER,
  RESOURCE_LIMIT_EXCEEDED,
  COMPILER_ALLOCATION_FAILED,
  INTERNAL_CONTRACT_VIOLATION,
  ASCII_LITERAL_INVALID,
  ASCII_CONTROL_BYTE_INVALID,
  ASCII_FIELD_LENGTH_INVALID,
  ASCII_FIELD_REFERENCE_INVALID,
  ASCII_FIELD_BOUNDARY_AMBIGUOUS,
  ASCII_TEMPLATE_EMPTY,
  ASCII_STREAM_TERMINATOR_INVALID,
  ASCII_STREAM_BOUNDARY_UNPROVEN,
  ASCII_STREAM_PROFILE_MISMATCH,
};

enum class ResourceKind {
  NONE,
  PLAN_ACCOUNTED_MEMORY,
  METADATA_ACCOUNTED_MEMORY,
};

enum class ResourceProfile {
  DESKTOP,
  CONSTRAINED,
};

// 自有诊断数据，可复制保存；位置与资源信息按失败种类提供，不保证每种错误都有字节偏移。
struct CompileDiagnostic {
  CompileStage stage = CompileStage::INTERNAL;
  CompileError code = CompileError::INTERNAL_CONTRACT_VIOLATION;
  std::string json_pointer;  // 配置中的 JSON Pointer；空串可表示根或没有更具体的位置。
  std::optional<std::size_t> byte_offset;  // 原始输入中的字节偏移，不是字符序号。
  std::string detail;
  ResourceKind resource_kind = ResourceKind::NONE;
  std::size_t required_bytes = 0U;  // 与 resource_kind 对应的逻辑计费，不是进程 RSS。
  std::size_t limit_bytes = 0U;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
};

inline constexpr std::size_t kUseDefaultMetadataMemoryLimit = static_cast<std::size_t>(-1);

// 控制描述元数据预算，单位为字节；默认哨兵请求编译器采用默认预算，不是显式的无限预算。
struct CompileOptions {
  std::size_t metadata_memory_limit_bytes = kUseDefaultMetadataMemoryLimit;
};

// Description values contain borrowed string_views, not owned strings. Destroying or replacing
// their owner invalidates them. Reacquire descriptions after moving the owner. Empty/moved-from
// owners and out-of-range queries return empty results; using an expired view is not checked.
// 可移动、不可复制的外部编译结果持有者。Description 中的 string_view/字面量指针借用本
// owner，移动后应重新查询，销毁后不得再访问；执行对象另外保留冻结状态，并不依赖本对象
// 的地址或寿命。保留执行对象不意味着可以继续使用已失效的外部 metadata 视图。
class PAE_API CompiledProtocol final {
 public:
  CompiledProtocol() noexcept;
  CompiledProtocol(const CompiledProtocol&) = delete;
  CompiledProtocol& operator=(const CompiledProtocol&) = delete;
  CompiledProtocol(CompiledProtocol&& other) noexcept;
  CompiledProtocol& operator=(CompiledProtocol&& other) noexcept;
  ~CompiledProtocol();

  [[nodiscard]] bool HasValue() const noexcept;
  // 只查询冻结事实，不读取报文或触发执行。索引均为零基，且仅对产生它们的配置有效。
  [[nodiscard]] std::optional<ProtocolDescription> Protocol() const noexcept;
  [[nodiscard]] std::size_t PipelineCount() const noexcept;
  [[nodiscard]] std::optional<PipelineDescription> Pipeline(std::size_t index) const noexcept;
  // association_index 是该 Pipeline 的关联序号；返回全局 Message 索引。
  [[nodiscard]] std::optional<std::size_t> PipelineMessageIndex(
      std::size_t pipeline_index, std::size_t association_index) const noexcept;
  // message_index 是全局索引，不是关联序号；查询指定 Pipeline 下实际可用的执行方向与尺寸。
  [[nodiscard]] std::optional<MessageExecutionDescription> PipelineMessageExecution(
      std::size_t pipeline_index, std::size_t message_index) const noexcept;
  [[nodiscard]] std::size_t MessageCount() const noexcept;
  [[nodiscard]] std::optional<MessageDescription> Message(std::size_t index) const noexcept;
  [[nodiscard]] std::size_t FieldCount() const noexcept;
  // flat_index 是全局字段索引；Encode 的 FieldSelector 则使用消息内字段索引。
  [[nodiscard]] std::optional<FieldDescription> Field(std::size_t flat_index) const noexcept;
  // ASCII actions and ordered segments are read-only frozen facts, not Pipeline permission or
  // evidence that a frame matched. Literal views borrow this compiled owner.
  // ASCII 查询描述指定方向的模板；存在模板不等于 Pipeline 授权或收到的报文已匹配成功。
  [[nodiscard]] AsciiActionQueryResult AsciiAction(std::size_t message_index,
                                                   pae::AsciiAction action) const noexcept;
  [[nodiscard]] AsciiSegmentQueryResult AsciiSegment(std::size_t message_index,
                                                     pae::AsciiAction action,
                                                     std::size_t ordinal) const noexcept;
  [[nodiscard]] AsciiFieldQueryResult AsciiField(std::size_t flat_field_index) const noexcept;
  // Representation is available for Binary and ASCII messages. Physical queries support Binary
  // only and return copied values without reading a frame or invoking Codec/Host execution.
  // 物理描述返回自有数值。Resolve 系列中的 frame_size 单位为字节，用于解析可变布局，
  // 不验证实际 Frame 内容、完整性或业务字段。
  [[nodiscard]] MessageRepresentationQueryResult MessageRepresentation(
      std::size_t message_index) const noexcept;
  [[nodiscard]] MessagePhysicalQueryResult MessagePhysical(
      std::size_t message_index) const noexcept;
  [[nodiscard]] ResolvedMessagePhysicalQueryResult ResolveMessagePhysical(
      std::size_t message_index, std::size_t frame_size) const noexcept;
  [[nodiscard]] FieldPhysicalQueryResult FieldPhysical(std::size_t flat_index) const noexcept;
  [[nodiscard]] ResolvedFieldPhysicalQueryResult ResolveFieldPhysical(
      std::size_t flat_index, std::size_t frame_size) const noexcept;
  [[nodiscard]] std::size_t EnumCount() const noexcept;
  [[nodiscard]] std::optional<EnumDescription> Enum(std::size_t flat_index) const noexcept;
  // 编译产物的逻辑内存计费快照；不含调用方输入、运行时执行对象或分配器全部额外开销。
  [[nodiscard]] CompileMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit CompiledProtocol(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;

  friend class CompileResult;
  friend class detail::CompiledProtocolAccess;
  friend PAE_API CompileResult CompileProtocolJson(std::string_view, const CompileOptions&);
};

// 编译成功或失败的自有结果；移动或提取后不再表示原来的成功产物。
class PAE_API CompileResult final {
 public:
  CompileResult() = delete;
  CompileResult(const CompileResult&) = delete;
  CompileResult& operator=(const CompileResult&) = delete;
  CompileResult(CompileResult&& other) noexcept;
  CompileResult& operator=(CompileResult&& other) noexcept;
  ~CompileResult();

  [[nodiscard]] bool Succeeded() const noexcept;
  // 借用本结果；失败时为空。
  [[nodiscard]] const CompiledProtocol* Compiled() const noexcept;
  // 借用本结果的诊断。
  [[nodiscard]] const CompileDiagnostic* Diagnostic() const noexcept;
  // 先检查 Succeeded()，再以右值转移所有权；提取后本结果为空，失败提取得到空 owner。
  [[nodiscard]] CompiledProtocol TakeCompiled() && noexcept;

 private:
  explicit CompileResult(CompiledProtocol compiled) noexcept;
  explicit CompileResult(CompileDiagnostic diagnostic) noexcept;

  bool succeeded_ = false;
  CompiledProtocol compiled_;
  std::optional<CompileDiagnostic> diagnostic_;

  friend PAE_API CompileResult CompileProtocolJson(std::string_view, const CompileOptions&);
};

// Configuration failures, including compiler-side allocation failures, are returned as diagnostics.
// The function itself is not noexcept: constructing the public facade or diagnostic strings can
// still throw an allocation exception in the calling C++ runtime.
// json_bytes 只在同步调用期间借用，可含明确长度；成功返回后不需保留原配置字符串。
// 配置/预算失败通过 Diagnostic 返回，但 facade 或诊断字符串分配仍可能抛出 C++ 异常。
[[nodiscard]] PAE_API CompileResult CompileProtocolJson(std::string_view json_bytes,
                                                        const CompileOptions& options = {});

}  // namespace pae
