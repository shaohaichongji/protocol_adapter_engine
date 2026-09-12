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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
#include "../../src/protocol_framing/stream_framer.h"
#endif

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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  bool stream_ascii_crlf = false;
  std::size_t maximum_frame_length = 0U;
#endif
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

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
struct StreamObservation {
  protocol_framing::StreamFramingPhase phase = protocol_framing::StreamFramingPhase::COLLECTING;
  std::size_t buffered_bytes = 0U;
  bool has_internal_work = false;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
  std::size_t frozen_input_bytes = 0U;
  std::size_t frozen_cursor = 0U;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
};

struct StreamStepResult {
  ExecutionIdentity identity;
  AdapterStatus status = AdapterStatus::INVALID_REQUEST;
  bool push_called = false;
  protocol_framing::SubmitResult framing;
  StreamObservation before;
  StreamObservation after;
  std::optional<ExecutionResult> candidate;
  std::string detail;
};
#endif

// Internal, non-persistent ASCII adapter. It owns the Plan, Sidecar, reusable Core Workspace and,
// when explicitly enabled, bounded Schema 0.11 stream workspaces. Every returned
// description/result byte and string is copied and remains valid after subsequent calls and after
// this adapter is destroyed.
class OfflineAdapter final {
 public:
  static bool Supports(const config_compiler::CompiledUiArtifacts& artifacts) noexcept;
  static std::unique_ptr<OfflineAdapter> AdoptCompiledArtifacts(
      config_compiler::CompiledUiArtifacts artifacts, std::string& error
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
      ,
      const protocol_framing::FramingLimitOverrides& stream_overrides = {}
#endif
  );

  OfflineAdapter(const OfflineAdapter&) = delete;
  OfflineAdapter& operator=(const OfflineAdapter&) = delete;
  OfflineAdapter(OfflineAdapter&&) = delete;
  OfflineAdapter& operator=(OfflineAdapter&&) = delete;
  ~OfflineAdapter();

  const DocumentDescription& Description() const noexcept { return description_; }

  ExecutionResult Inspect(ExecutionIdentity identity, const std::vector<std::uint8_t>& input_frame);
  ExecutionResult Encode(ExecutionIdentity identity, const std::vector<InputField>& inputs);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  StreamStepResult SubmitStreamChunk(ExecutionIdentity identity,
                                     const std::vector<std::uint8_t>& input_chunk);
  StreamStepResult ContinueStream(ExecutionIdentity identity);
  bool ResetStream(std::size_t pipeline_index, std::string_view pipeline_id,
                   std::string& error) noexcept;
  std::optional<StreamObservation> ObserveStream(std::size_t pipeline_index) const noexcept;
  std::size_t StreamChunkCapacity(std::size_t pipeline_index) const noexcept;
  bool StreamHasDiscardableState(std::size_t pipeline_index) const noexcept;
#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
  void SetCandidateCopyLimitForTesting(std::size_t pipeline_index,
                                       std::size_t maximum_bytes) noexcept;
#endif
#endif

 private:
  OfflineAdapter(protocol_plan::PlanOwner plan, config_compiler::UiDescriptionSidecar sidecar,
                 DocumentDescription description);

  // Declaration order is intentional: destruction is reverse, so the borrowing Workspace dies
  // before the copied description, Sidecar storage, and owned Plan.
  protocol_plan::PlanOwner plan_;
  config_compiler::UiDescriptionSidecar sidecar_;
  DocumentDescription description_;
  protocol_core::ExecutionWorkspace workspace_;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  struct StreamContext;
  std::vector<std::unique_ptr<StreamContext>> streams_;
#endif
};

std::string_view CodecStatusName(protocol_core::CodecStatus status) noexcept;

}  // namespace pae::protocol_lab::ascii
