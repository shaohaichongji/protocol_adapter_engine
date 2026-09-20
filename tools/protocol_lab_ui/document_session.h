#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
#include "../protocol_lab/v06_execution.h"
#endif
#include "ascii_escaped_input.h"
#include "ascii_host_types.h"
#include "compile_worker.h"
#include "description_mapping.h"
#include "inspect_hex_input.h"
#include "lab_execution_observer.h"
#include "ui_field_result.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
#include "public_legacy_complete_adapter.h"
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
#include "binary_host_adapter.h"
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
#include "ascii_host_adapter.h"
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER) && \
    !defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
#include "../protocol_lab_ascii/host_observer_adapter.h"
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
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
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
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

struct InvalidDraftState {
  std::u16string text;
  std::string validation_error;
};

struct OperationDiagnostic {
  std::string id;
  std::string detail;
};

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
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  std::unique_ptr<PublicLegacyCompleteAdapter> public_legacy_adapter;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  std::unique_ptr<BinaryHostAdapter> binary_host_adapter;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  std::unique_ptr<AsciiHostAdapter> host_adapter;
#else
  std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> host_adapter;
#endif
#endif
  // Declaration order is intentional: destruction is reverse, so the bridge (and its workspaces
  // and Plan) dies before the sidecar storage.
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  config_compiler::UiDescriptionSidecar description;
  std::unique_ptr<protocol_lab::v06::ExecutionBridge> bridge;
#endif
  std::string config_sha256;
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
  std::unique_ptr<protocol_lab::ascii::OfflineAdapter> ascii_adapter;
#elif defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  // Source-compatible negative assertion only; no private adapter type enters this build.
  std::nullptr_t ascii_adapter = nullptr;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  std::unique_ptr<protocol_lab_ascii::public_offline::Adapter> public_ascii_adapter;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  std::unique_ptr<AsciiHostAdapter> public_ascii_stream_adapter;
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  struct BinaryPublication {
    std::unique_ptr<BinaryHostAdapter> adapter;
    DocumentDescription description;
    SelectionKey selection;
    std::string inspect_draft;
    std::u16string inspect_draft_utf16;
    std::optional<InspectResult> inspect_result;
    std::optional<InspectFailure> inspect_failure;
    std::size_t mapped_view_bytes = 0U;
    Revision session_revision = 0U;
  };
  struct BinaryFlowPublication {
    std::size_t binding = 0U;
    std::size_t flow = 0U;
    std::size_t pipeline_index = 0U;
    SelectionKey selection;
    std::string inspect_draft;
    std::u16string inspect_draft_utf16;
    std::optional<InspectResult> inspect_result;
    std::optional<InspectFailure> inspect_failure;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    std::optional<BinaryUiStreamView> stream_view;
#endif
    std::size_t mapped_view_bytes = 0U;
    OperationMode mode = OperationMode::INSPECT;
    std::unordered_map<std::size_t, TypedDraft> drafts;
    std::unordered_map<std::size_t, InvalidDraftState> invalid_drafts;
    std::optional<PreviewResult> preview;
    std::optional<OperationDiagnostic> encode_failure;
    std::string diagnostic_id;
    std::string diagnostic_detail;
  };
  using BinaryPreparationHook = void (*)();
  std::optional<BinaryPublication> PrepareBinaryHostPublication(
      std::unique_ptr<BinaryHostAdapter> adapter, Revision expected_request,
      BinaryPreparationHook before_copy = nullptr);
  void PublishBinaryHostPublication(BinaryPublication publication) noexcept;
  std::optional<BinaryFlowPublication> PrepareBinaryHostFlow(
      std::size_t binding, std::size_t flow, BinaryPreparationHook before_copy = nullptr) const;
  bool PublishBinaryHostFlow(BinaryFlowPublication publication,
                             BinaryPreparationHook before_local_cache_copy = nullptr);
  bool BinaryHostActive() const noexcept { return prepared_ && prepared_->binary_host_adapter; }
  bool IsBinaryHostDocument() const noexcept {
    return description_.has_value() && description_->schema_version == "0.9";
  }
  std::size_t BinaryHostBindingIndex() const noexcept { return binary_host_binding_; }
  std::size_t BinaryHostFlowIndex() const noexcept { return binary_host_flow_; }
  Revision BinarySessionRevision() const noexcept { return binary_session_revision_; }
  std::size_t BinaryActiveViewBytes() const noexcept { return binary_active_view_bytes_; }
  bool BinaryHasDiscardableState() const noexcept;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  const std::optional<BinaryUiStreamView>& binary_stream_view() const noexcept {
    return binary_stream_view_;
  }
  std::optional<StreamPresentationObservation> BinaryStreamObservation() const noexcept;
#endif
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  bool ApplyHostAdapter(std::unique_ptr<AsciiHostAdapter> adapter);
#else
  bool ApplyHostAdapter(std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> adapter);
#endif
  bool SelectHostFlow(std::size_t binding, std::size_t stream);
  bool HostActive() const noexcept { return prepared_ && prepared_->host_adapter; }
  std::size_t HostBindingIndex() const noexcept { return host_binding_; }
  std::size_t HostStreamIndex() const noexcept { return host_stream_; }
  bool HostHasDiscardableState() const noexcept;
  void ResetAllHostStreams();
#endif
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
  bool Encode(LabExecutionObserver* observer = nullptr,
              InputMaterializationTimer* input_materialization_timer = nullptr,
              std::int64_t* input_materialization_ns = nullptr);
  bool SetInspectDraft(std::string text);
  bool SetInspectDraftUtf16(std::u16string text);
  void RejectInspectCapacity(std::size_t capacity);
  bool Inspect(LabExecutionObserver* observer = nullptr);
#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
  bool SubmitStream();
  bool ContinueStream();
  bool ResetStream();
  bool StreamInspectAvailable() const noexcept;
  bool StreamContinueAvailable() const noexcept;
  bool StreamHasDiscardableState() const noexcept;
  std::size_t StreamChunkBudget() const noexcept;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  const std::optional<AsciiStreamStepResult>& stream_step() const noexcept {
    return stream_step_;
  }
  std::optional<AsciiStreamObservation> StreamObservation() const noexcept;
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
  bool AsciiBackendReady() const noexcept;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  std::size_t binary_host_binding_ = 0U;
  std::size_t binary_host_flow_ = 0U;
  Revision binary_session_revision_ = 0U;
  std::size_t binary_active_view_bytes_ = 0U;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  std::optional<BinaryUiStreamView> binary_stream_view_;
#endif
  struct BinaryEncodeLocalState {
    std::unordered_map<std::size_t, InvalidDraftState> invalid_drafts;
    std::optional<OperationDiagnostic> encode_failure;
    std::size_t accounted_bytes = 0U;
  };
  std::map<std::pair<std::size_t, std::size_t>, BinaryEncodeLocalState>
      binary_encode_local_states_;
  std::size_t binary_encode_local_state_bytes_ = 0U;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  struct HostView {
    std::string draft;
    std::u16string utf16;
    ByteRepresentation representation = ByteRepresentation::HEX;
    std::optional<InspectResult> result;
    std::optional<InspectFailure> failure;
    std::optional<AsciiStreamStepResult> step;
    std::optional<Revision> submitted;
    Revision input_revision = 0U;
    std::unordered_map<std::size_t, TypedDraft> encode_drafts;
    std::unordered_map<std::size_t, InvalidDraftState> invalid_drafts;
    std::optional<PreviewResult> preview;
    std::optional<OperationDiagnostic> encode_failure;
    std::optional<SelectionKey> selection;
    std::string diagnostic_id, diagnostic_detail;
  };
  void SaveHostView();
  std::vector<HostView> host_views_;
  std::size_t host_binding_ = 0U;
  std::size_t host_stream_ = 0U;
#endif
  void ClearSelectionAndPreview();
  void ClearPreview();
  void ClearInspectOutcome();
  bool SetInitialSelection();
  bool CurrentMessage(const MessageDescriptor*& message) const noexcept;
  PreviewKey MakePreviewKey() const;
  InspectResultKey MakeInspectResultKey() const;
  bool PreviewKeyStillCurrent(const PreviewKey& key) const noexcept;
  bool InspectKeyStillCurrent(const InspectResultKey& key) const noexcept;
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  bool ResolveInspectMessage(const protocol_lab::v06::Result& result, std::size_t& message_index,
                             std::string& message_id) const noexcept;
#endif
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
  std::optional<AsciiStreamStepResult> stream_step_;
  std::optional<Revision> submitted_stream_input_revision_;
#endif
  std::string diagnostic_id_;
  std::string diagnostic_detail_;
};

}  // namespace pae::protocol_lab_ui
