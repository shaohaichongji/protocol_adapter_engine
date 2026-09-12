#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "../protocol_lab/v06_execution.h"
#include "ascii_escaped_input.h"
#include "compile_worker.h"
#include "description_mapping.h"
#include "inspect_hex_input.h"
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
#include "../protocol_lab_ascii/ascii_offline_adapter.h"
#endif

namespace pae::protocol_lab_ui {

enum class DocumentState {
  EMPTY,
  LOADING,
  READY,
  PREVIEW_VALID,
  CONFIG_ERROR,
  CLOSING,
  CLOSED,
};

enum class OperationMode {
  ENCODE,
  INSPECT,
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  STREAM_INSPECT,
#endif
};

enum class InspectFailureStage {
  INPUT,
  STRUCTURAL_QUERY,
  CODEC,
  MATERIALIZATION,
};

struct SelectionKey {
  Revision plan_generation = 0U;
  std::size_t pipeline_index = 0U;
  std::string pipeline_id;
  std::size_t message_index = 0U;
  std::string message_id;
};

struct PreviewKey {
  DocumentId document_id = 0U;
  Revision load_revision = 0U;
  Revision plan_generation = 0U;
  Revision selection_revision = 0U;
  Revision input_revision = 0U;
  SelectionKey selection;
};

struct EnumSelection {
  std::size_t entry_index = 0U;
  std::string entry_id;
};

struct InvalidDraftState {
  std::u16string text;
  std::string validation_error;
};

struct OperationDiagnostic {
  std::string id;
  std::string detail;
};

struct UiFieldResult {
  std::size_t field_index = 0U;
  std::string id;
  std::string raw_value;
  std::string logical_value;
  std::optional<ByteRange> actual_range;
};

using TypedDraft = std::variant<std::uint64_t, std::int64_t, std::vector<std::uint8_t>,
                                EnumSelection, bool, protocol_lab::v06::Decimal64>;

struct PreviewResult {
  PreviewKey key;
  std::vector<std::uint8_t> encoded_frame;
  std::vector<UiFieldResult> fields;
  bool zero_field_success = false;
  bool tx_template_review = false;
};

struct InspectResultKey {
  DocumentId document_id = 0U;
  Revision load_revision = 0U;
  Revision plan_generation = 0U;
  Revision pipeline_selection_revision = 0U;
  Revision inspect_input_revision = 0U;
  Revision inspect_request_revision = 0U;
  std::size_t pipeline_index = 0U;
  std::string pipeline_id;
};

struct InspectResult {
  InspectResultKey key;
  std::vector<std::uint8_t> input_frame;
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<UiFieldResult> fields;
  bool zero_field_success = false;
};

struct InspectFailure {
  InspectFailureStage stage = InspectFailureStage::INPUT;
  std::string diagnostic_id;
  std::string detail;
  std::string status;
  std::optional<std::size_t> input_offset;
  bool input_offset_is_utf16 = false;
  std::vector<std::uint8_t> input_frame;
  std::optional<std::size_t> message_index;
  std::optional<std::string> message_id;
  std::optional<std::size_t> failed_field_index;
  std::optional<std::string> failed_field_id;
};

InspectFailure MakeStructuralInspectFailure(std::string status,
                                            std::vector<std::uint8_t> input_frame);

struct PreparedDocument {
  // Declaration order is intentional: destruction is reverse, so the bridge (and its workspaces
  // and Plan) dies before the sidecar storage.
  config_compiler::UiDescriptionSidecar description;
  std::string config_sha256;
  std::unique_ptr<protocol_lab::v06::ExecutionBridge> bridge;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  std::unique_ptr<protocol_lab::ascii::OfflineAdapter> ascii_adapter;
#endif
};

class InputMaterializationTimer {
 public:
  virtual ~InputMaterializationTimer() = default;
  virtual void Start() noexcept = 0;
  virtual std::int64_t ElapsedNanoseconds() const noexcept = 0;
};

class DocumentSession final {
 public:
  explicit DocumentSession(DocumentId document_id);
  DocumentSession(const DocumentSession&) = delete;
  DocumentSession& operator=(const DocumentSession&) = delete;
  ~DocumentSession();

  Revision BeginLoad();
  bool ApplyCompileCompletion(std::unique_ptr<CompileCompletion> completion);
  bool SelectPipeline(std::size_t pipeline_index);
  bool SelectMessage(std::size_t message_index);
  bool SetMode(OperationMode mode);
  bool SetRepresentation(ByteRepresentation representation);
  void InvalidateDraft(std::size_t field_index);
  void InvalidateInput();
  bool SetInvalidDraft(std::size_t field_index, std::string text, std::string validation_error);
  bool SetInvalidDraftUtf16(std::size_t field_index, std::u16string text,
                            std::string validation_error);
  bool SetDraft(std::size_t field_index, TypedDraft value);
  bool Encode(protocol_lab::v06::ExecutionObserver* observer = nullptr,
              InputMaterializationTimer* input_materialization_timer = nullptr,
              std::int64_t* input_materialization_ns = nullptr);
  bool SetInspectDraft(std::string text);
  bool SetInspectDraftUtf16(std::u16string text);
  void RejectInspectCapacity(std::size_t capacity);
  bool Inspect(protocol_lab::v06::ExecutionObserver* observer = nullptr);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  bool SubmitStream();
  bool ContinueStream();
  bool ResetStream();
  bool StreamInspectAvailable() const noexcept;
  bool StreamContinueAvailable() const noexcept;
  bool StreamHasDiscardableState() const noexcept;
  std::size_t StreamChunkBudget() const noexcept;
  const std::optional<protocol_lab::ascii::StreamStepResult>& stream_step() const noexcept {
    return stream_step_;
  }
  std::optional<protocol_lab::ascii::StreamObservation> StreamObservation() const noexcept;
#endif
  void Close();

  DocumentId id() const noexcept { return document_id_; }
  DocumentState state() const noexcept { return state_; }
  Revision load_revision() const noexcept { return load_revision_; }
  Revision plan_generation() const noexcept { return plan_generation_; }
  Revision selection_revision() const noexcept { return selection_revision_; }
  Revision input_revision() const noexcept { return input_revision_; }
  Revision pipeline_selection_revision() const noexcept { return pipeline_selection_revision_; }
  Revision inspect_input_revision() const noexcept { return inspect_input_revision_; }
  Revision inspect_request_revision() const noexcept { return inspect_request_revision_; }
  OperationMode mode() const noexcept { return mode_; }
  ByteRepresentation representation() const noexcept { return representation_; }
  bool IsAsciiDocument() const noexcept {
    return description_.has_value() && description_->layout == DocumentLayout::ASCII_TEXT;
  }
  bool EncodeAvailable() const noexcept;
  bool InspectAvailable() const noexcept;
  const PreparedDocument* prepared() const noexcept { return prepared_.get(); }
  const DocumentDescription* description() const noexcept {
    return description_.has_value() ? &*description_ : nullptr;
  }
  const std::optional<SelectionKey>& selection() const noexcept { return selection_; }
  const std::optional<std::size_t>& selected_pipeline_index() const noexcept {
    return selected_pipeline_index_;
  }
  const std::unordered_map<std::size_t, TypedDraft>& drafts() const noexcept { return drafts_; }
  const std::unordered_map<std::size_t, InvalidDraftState>& invalid_drafts() const noexcept {
    return invalid_drafts_;
  }
  const std::optional<PreviewResult>& preview() const noexcept { return preview_; }
  const std::string& inspect_draft() const noexcept { return inspect_draft_; }
  const std::u16string& inspect_draft_utf16() const noexcept { return inspect_draft_utf16_; }
  const std::optional<InspectResult>& inspect_result() const noexcept { return inspect_result_; }
  const std::optional<InspectFailure>& inspect_failure() const noexcept { return inspect_failure_; }
  std::size_t InspectFrameBudget() const noexcept;
  const std::string& diagnostic_id() const noexcept { return diagnostic_id_; }
  const std::string& diagnostic_detail() const noexcept { return diagnostic_detail_; }

 private:
  void ClearSelectionAndPreview();
  void ClearPreview();
  void ClearInspectOutcome();
  bool SetInitialSelection();
  bool CurrentMessage(const MessageDescriptor*& message) const noexcept;
  PreviewKey MakePreviewKey() const;
  InspectResultKey MakeInspectResultKey() const;
  bool PreviewKeyStillCurrent(const PreviewKey& key) const noexcept;
  bool InspectKeyStillCurrent(const InspectResultKey& key) const noexcept;
  bool ResolveInspectMessage(const protocol_lab::v06::Result& result, std::size_t& message_index,
                             std::string& message_id) const noexcept;
  void RefreshDocumentState();
  void ClearEncodeFailure();
  void SetEncodeFailure(std::string id, std::string detail);
  void SetDiagnostic(std::string id, std::string detail);

  const DocumentId document_id_;
  DocumentState state_ = DocumentState::EMPTY;
  Revision load_revision_ = 0U;
  Revision plan_generation_ = 0U;
  Revision selection_revision_ = 0U;
  Revision input_revision_ = 0U;
  Revision pipeline_selection_revision_ = 0U;
  Revision inspect_input_revision_ = 0U;
  Revision inspect_request_revision_ = 0U;
  OperationMode mode_ = OperationMode::ENCODE;
  ByteRepresentation representation_ = ByteRepresentation::HEX;
  std::unique_ptr<PreparedDocument> prepared_;
  std::optional<DocumentDescription> description_;
  std::optional<std::size_t> selected_pipeline_index_;
  std::optional<SelectionKey> selection_;
  std::unordered_map<std::size_t, TypedDraft> drafts_;
  std::unordered_map<std::size_t, InvalidDraftState> invalid_drafts_;
  std::optional<PreviewResult> preview_;
  std::optional<OperationDiagnostic> encode_failure_;
  std::string inspect_draft_;
  std::u16string inspect_draft_utf16_;
  std::optional<InspectResult> inspect_result_;
  std::optional<InspectFailure> inspect_failure_;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  std::optional<protocol_lab::ascii::StreamStepResult> stream_step_;
  std::optional<Revision> submitted_stream_input_revision_;
#endif
  std::string diagnostic_id_;
  std::string diagnostic_detail_;
};

}  // namespace pae::protocol_lab_ui
