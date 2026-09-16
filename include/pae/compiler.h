#pragma once

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

struct CompileDiagnostic {
  CompileStage stage = CompileStage::INTERNAL;
  CompileError code = CompileError::INTERNAL_CONTRACT_VIOLATION;
  std::string json_pointer;
  std::optional<std::size_t> byte_offset;
  std::string detail;
  ResourceKind resource_kind = ResourceKind::NONE;
  std::size_t required_bytes = 0U;
  std::size_t limit_bytes = 0U;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
};

inline constexpr std::size_t kUseDefaultMetadataMemoryLimit = static_cast<std::size_t>(-1);

struct CompileOptions {
  std::size_t metadata_memory_limit_bytes = kUseDefaultMetadataMemoryLimit;
};

// Description values contain borrowed string_views, not owned strings. Destroying or replacing
// their owner invalidates them. Reacquire descriptions after moving the owner. Empty/moved-from
// owners and out-of-range queries return empty results; using an expired view is not checked.
class PAE_API CompiledProtocol final {
 public:
  CompiledProtocol() noexcept;
  CompiledProtocol(const CompiledProtocol&) = delete;
  CompiledProtocol& operator=(const CompiledProtocol&) = delete;
  CompiledProtocol(CompiledProtocol&& other) noexcept;
  CompiledProtocol& operator=(CompiledProtocol&& other) noexcept;
  ~CompiledProtocol();

  [[nodiscard]] bool HasValue() const noexcept;
  [[nodiscard]] std::optional<ProtocolDescription> Protocol() const noexcept;
  [[nodiscard]] std::size_t PipelineCount() const noexcept;
  [[nodiscard]] std::optional<PipelineDescription> Pipeline(std::size_t index) const noexcept;
  [[nodiscard]] std::optional<std::size_t> PipelineMessageIndex(
      std::size_t pipeline_index, std::size_t association_index) const noexcept;
  [[nodiscard]] std::optional<MessageExecutionDescription> PipelineMessageExecution(
      std::size_t pipeline_index, std::size_t message_index) const noexcept;
  [[nodiscard]] std::size_t MessageCount() const noexcept;
  [[nodiscard]] std::optional<MessageDescription> Message(std::size_t index) const noexcept;
  [[nodiscard]] std::size_t FieldCount() const noexcept;
  [[nodiscard]] std::optional<FieldDescription> Field(std::size_t flat_index) const noexcept;
  // ASCII actions and ordered segments are read-only frozen facts, not Pipeline permission or
  // evidence that a frame matched. Literal views borrow this compiled owner.
  [[nodiscard]] AsciiActionQueryResult AsciiAction(std::size_t message_index,
                                                   pae::AsciiAction action) const noexcept;
  [[nodiscard]] AsciiSegmentQueryResult AsciiSegment(std::size_t message_index,
                                                     pae::AsciiAction action,
                                                     std::size_t ordinal) const noexcept;
  [[nodiscard]] AsciiFieldQueryResult AsciiField(std::size_t flat_field_index) const noexcept;
  // Representation is available for Binary and ASCII messages. Physical queries support Binary
  // only and return copied values without reading a frame or invoking Codec/Host execution.
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
  [[nodiscard]] CompileMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit CompiledProtocol(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;

  friend class CompileResult;
  friend class detail::CompiledProtocolAccess;
  friend PAE_API CompileResult CompileProtocolJson(std::string_view, const CompileOptions&);
};

class PAE_API CompileResult final {
 public:
  CompileResult() = delete;
  CompileResult(const CompileResult&) = delete;
  CompileResult& operator=(const CompileResult&) = delete;
  CompileResult(CompileResult&& other) noexcept;
  CompileResult& operator=(CompileResult&& other) noexcept;
  ~CompileResult();

  [[nodiscard]] bool Succeeded() const noexcept;
  [[nodiscard]] const CompiledProtocol* Compiled() const noexcept;
  [[nodiscard]] const CompileDiagnostic* Diagnostic() const noexcept;
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
[[nodiscard]] PAE_API CompileResult CompileProtocolJson(std::string_view json_bytes,
                                                        const CompileOptions& options = {});

}  // namespace pae
