#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../src/config_compiler/config_compiler.h"
#include "../../src/protocol_core/complete_record_codec.h"

namespace pae::protocol_lab::ascii {

enum class AdapterStatus {
  OK,
  INVALID_REQUEST,
  CORE_FAILED,
  MATERIALIZATION_FAILED,
};

enum class Operation {
  ENCODE,
  INSPECT,
};

enum class ReviewKind {
  NOT_APPLICABLE,
  TX_TEMPLATE,
};

enum class SegmentKind {
  LITERAL,
  FIELD,
};

struct ExecutionIdentity {
  std::uint64_t document_id = 0U;
  std::uint64_t load_revision = 0U;
  std::uint64_t plan_generation = 0U;
  std::uint64_t selection_revision = 0U;
  std::uint64_t input_revision = 0U;
  std::size_t pipeline_index = 0U;
  std::string pipeline_id;
  std::optional<std::size_t> message_index;
  std::optional<std::string> message_id;
};

struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};

struct SegmentDescription {
  SegmentKind kind = SegmentKind::LITERAL;
  std::vector<std::uint8_t> literal;
  std::optional<std::size_t> field_index;
  std::optional<std::string> field_id;
};

struct ActionDescription {
  std::size_t min_record_length = 0U;
  std::size_t max_record_length = 0U;
  std::vector<SegmentDescription> segments;
  std::vector<std::size_t> referenced_field_indices;
};

struct FieldDescription {
  std::size_t field_index = 0U;
  std::string id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::size_t min_byte_length = 0U;
  std::size_t max_byte_length = 0U;
  std::vector<std::uint8_t> allowed_control_bytes;
  bool decode_referenced = false;
  bool encode_referenced = false;
};

struct MessageDescription {
  std::size_t message_index = 0U;
  std::string id;
  std::string direction_id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::size_t max_record_length = 0U;
  std::optional<ActionDescription> decode;
  std::optional<ActionDescription> encode;
  std::vector<FieldDescription> fields;
};

struct PipelineDescription {
  std::size_t pipeline_index = 0U;
  std::string id;
  std::string direction_id;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::vector<std::size_t> message_indices;
  std::vector<std::size_t> decode_message_indices;
};

struct DocumentDescription {
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  std::string display_name;
  std::string description;
  std::string source_ref;
  std::size_t max_record_bytes = 0U;
  std::vector<PipelineDescription> pipelines;
  std::vector<MessageDescription> messages;
};

struct InputField {
  std::size_t field_index = 0U;
  std::string field_id;
  std::vector<std::uint8_t> bytes;
};

struct FieldResult {
  std::size_t field_index = 0U;
  std::string field_id;
  std::vector<std::uint8_t> bytes;
  ByteRange range;
};

struct ExecutionResult {
  ExecutionIdentity identity;
  Operation operation = Operation::INSPECT;
  AdapterStatus status = AdapterStatus::INVALID_REQUEST;
  protocol_core::CodecStatus core_status = protocol_core::CodecStatus::INVALID_ARGUMENT;
  bool core_called = false;
  ReviewKind review_kind = ReviewKind::NOT_APPLICABLE;
  std::optional<std::size_t> message_index;
  std::optional<std::string> message_id;
  std::optional<std::size_t> failed_input_index;
  std::optional<std::size_t> failed_field_index;
  std::optional<std::string> failed_field_id;
  std::vector<std::uint8_t> frame;
  std::vector<FieldResult> fields;
  std::vector<std::uint8_t> diagnostic_input_frame;
  std::string detail;
};

// Internal, non-persistent Schema 0.10 adapter. It owns the Plan, Sidecar, and one reusable
// Workspace. Every returned description/result byte and string is copied and remains valid after
// subsequent calls and after this adapter is destroyed.
class OfflineAdapter final {
 public:
  static bool Supports(const config_compiler::CompiledUiArtifacts& artifacts) noexcept;
  static std::unique_ptr<OfflineAdapter> AdoptCompiledArtifacts(
      config_compiler::CompiledUiArtifacts artifacts, std::string& error);

  OfflineAdapter(const OfflineAdapter&) = delete;
  OfflineAdapter& operator=(const OfflineAdapter&) = delete;
  OfflineAdapter(OfflineAdapter&&) = delete;
  OfflineAdapter& operator=(OfflineAdapter&&) = delete;
  ~OfflineAdapter();

  const DocumentDescription& Description() const noexcept { return description_; }

  ExecutionResult Inspect(ExecutionIdentity identity, const std::vector<std::uint8_t>& input_frame);
  ExecutionResult Encode(ExecutionIdentity identity, const std::vector<InputField>& inputs);

 private:
  OfflineAdapter(protocol_plan::PlanOwner plan, config_compiler::UiDescriptionSidecar sidecar,
                 DocumentDescription description);

  // Declaration order is intentional: destruction is reverse, so the borrowing Workspace dies
  // before the copied description, Sidecar storage, and owned Plan.
  protocol_plan::PlanOwner plan_;
  config_compiler::UiDescriptionSidecar sidecar_;
  DocumentDescription description_;
  protocol_core::ExecutionWorkspace workspace_;
};

std::string_view CodecStatusName(protocol_core::CodecStatus status) noexcept;

}  // namespace pae::protocol_lab::ascii
