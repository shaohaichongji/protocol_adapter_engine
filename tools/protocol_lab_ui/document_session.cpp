#include "document_session.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>

namespace pae::protocol_lab_ui {
namespace {

bool SelectionEqual(const SelectionKey& left, const SelectionKey& right) noexcept {
  return left.plan_generation == right.plan_generation &&
         left.pipeline_index == right.pipeline_index && left.pipeline_id == right.pipeline_id &&
         left.message_index == right.message_index && left.message_id == right.message_id;
}

std::string InspectFailureDetail(std::string_view detail, std::size_t offset) {
  std::ostringstream output;
  output << detail << " at input_offset=" << offset;
  return output.str();
}

bool DraftMatches(const FieldDescriptor& field, const TypedDraft& value) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field.conversion.has_value()) {
    return std::holds_alternative<protocol_lab::v06::Decimal64>(value);
  }
#endif
  switch (field.value_type) {
    case protocol_plan::ValueType::UINT64:
      return std::holds_alternative<std::uint64_t>(value);
    case protocol_plan::ValueType::INT64:
      return std::holds_alternative<std::int64_t>(value);
    case protocol_plan::ValueType::BYTES:
      if (!std::holds_alternative<std::vector<std::uint8_t>>(value)) return false;
      if (field.byte_length_bounds.has_value()) {
        const std::size_t size = std::get<std::vector<std::uint8_t>>(value).size();
        return size >= field.byte_length_bounds->minimum &&
               size <= field.byte_length_bounds->maximum;
      }
      return std::get<std::vector<std::uint8_t>>(value).size() == field.byte_width;
    case protocol_plan::ValueType::ENUM:
      return std::holds_alternative<EnumSelection>(value);
    case protocol_plan::ValueType::BOOL:
      return std::holds_alternative<bool>(value);
  }
  return false;
}

std::string ValueKind(const FieldDescriptor& field) {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field.conversion.has_value()) return "DECIMAL64";
#endif
  switch (field.value_type) {
    case protocol_plan::ValueType::UINT64:
      return "UINT64";
    case protocol_plan::ValueType::INT64:
      return "INT64";
    case protocol_plan::ValueType::BYTES:
      return "BYTES";
    case protocol_plan::ValueType::ENUM:
      return "ENUM";
    case protocol_plan::ValueType::BOOL:
      return "BOOL";
  }
  return {};
}

}  // namespace

DocumentSession::DocumentSession(DocumentId document_id) : document_id_(document_id) {}

InspectFailure MakeStructuralInspectFailure(std::string status,
                                            std::vector<std::uint8_t> input_frame) {
  InspectFailure failure;
  failure.stage = InspectFailureStage::STRUCTURAL_QUERY;
  failure.status = std::move(status);
  failure.diagnostic_id = "UI_INSPECT_" + failure.status;
  failure.detail = "selected Pipeline structural match failed: " + failure.status;
  failure.input_frame = std::move(input_frame);
  return failure;
}

DocumentSession::~DocumentSession() { Close(); }

Revision DocumentSession::BeginLoad() {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED) return load_revision_;
  ++load_revision_;
  prepared_.reset();
  description_.reset();
  ClearSelectionAndPreview();
  inspect_draft_.clear();
  ++inspect_input_revision_;
  ClearInspectOutcome();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  state_ = DocumentState::LOADING;
  return load_revision_;
}

bool DocumentSession::ApplyCompileCompletion(std::unique_ptr<CompileCompletion> completion) {
  if (completion == nullptr || state_ != DocumentState::LOADING ||
      completion->document_id != document_id_ || completion->load_revision != load_revision_) {
    return false;
  }
  if (completion->artifacts == nullptr || completion->diagnostic.has_value()) {
    if (completion->diagnostic.has_value()) {
      SetDiagnostic("UI_CONFIG_COMPILE_FAILED", completion->diagnostic->detail);
    } else {
      SetDiagnostic("UI_CONFIG_COMPILE_FAILED", "compile result contained no complete artifacts");
    }
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }
  auto& artifacts = *completion->artifacts;
  const auto* plan = artifacts.Plan();
  if (plan == nullptr) {
    SetDiagnostic("UI_INTERNAL_CONTRACT_VIOLATION", "compiled artifacts contain no Plan");
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }
  const std::string schema{plan->SchemaVersion()};
  if (schema != "0.5" && schema != "0.6" && schema != "0.7" && schema != "0.8") {
    SetDiagnostic("UI_SCHEMA_UNSUPPORTED", "the UI supports Schema 0.5 through 0.8 only");
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }

  DocumentDescription neutral_description;
  std::string mapping_error;
  if (!BuildDocumentDescription(*plan, artifacts.Description(), neutral_description,
                                mapping_error)) {
    SetDiagnostic("UI_DESCRIPTION_MAPPING_FAILED", std::move(mapping_error));
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }

  auto sidecar = artifacts.TakeDescription();
  protocol_lab::v06::PreparationFailure failure;
  auto bridge = protocol_lab::v06::ExecutionBridge::AdoptCompiledPlan(
      artifacts.TakePlan(), completion->config_sha256, failure);
  if (bridge == nullptr) {
    SetDiagnostic(failure.diagnostic_id.empty() ? "UI_BRIDGE_ADOPTION_FAILED"
                                                : std::move(failure.diagnostic_id),
                  std::move(failure.detail));
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }

  auto prepared = std::make_unique<PreparedDocument>();
  prepared->description = std::move(sidecar);
  prepared->config_sha256 = std::move(completion->config_sha256);
  prepared->bridge = std::move(bridge);
  prepared_ = std::move(prepared);
  description_ = std::move(neutral_description);
  ++plan_generation_;
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  if (!SetInitialSelection()) {
    prepared_.reset();
    description_.reset();
    SetDiagnostic("UI_EMPTY_PLAN_SELECTION", "Plan contains no selectable pipeline/message pair");
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }
  state_ = DocumentState::READY;
  return true;
}

bool DocumentSession::SelectPipeline(std::size_t pipeline_index) {
  if (prepared_ == nullptr || !description_.has_value() ||
      pipeline_index >= description_->pipelines.size()) {
    return false;
  }
  ++selection_revision_;
  ++pipeline_selection_revision_;
  selected_pipeline_index_ = pipeline_index;
  selection_.reset();
  drafts_.clear();
  invalid_drafts_.clear();
  inspect_draft_.clear();
  ++inspect_input_revision_;
  ClearPreview();
  ClearInspectOutcome();
  ClearEncodeFailure();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  state_ = DocumentState::READY;
  return true;
}

bool DocumentSession::SetMode(OperationMode mode) {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr) {
    return false;
  }
  mode_ = mode;
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  if (mode_ == OperationMode::INSPECT && inspect_failure_.has_value()) {
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
  } else if (mode_ == OperationMode::ENCODE && encode_failure_.has_value()) {
    SetDiagnostic(encode_failure_->id, encode_failure_->detail);
  }
  RefreshDocumentState();
  return true;
}

bool DocumentSession::SelectMessage(std::size_t message_index) {
  if (prepared_ == nullptr || !description_.has_value() || !selected_pipeline_index_.has_value() ||
      message_index >= description_->messages.size()) {
    return false;
  }
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  bool allowed = false;
  for (const std::size_t allowed_index : pipeline.message_indices) {
    if (allowed_index == message_index) {
      allowed = true;
      break;
    }
  }
  if (!allowed) return false;
  ++selection_revision_;
  const auto& message = description_->messages[message_index];
  selection_ = SelectionKey{plan_generation_, pipeline.pipeline_index, pipeline.id,
                            message.message_index, message.id};
  drafts_.clear();
  invalid_drafts_.clear();
  ClearPreview();
  ClearEncodeFailure();
  if (mode_ == OperationMode::ENCODE) {
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
  }
  RefreshDocumentState();
  return true;
}

void DocumentSession::InvalidateInput() {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr) {
    return;
  }
  ++input_revision_;
  ClearPreview();
  ClearEncodeFailure();
  if (mode_ == OperationMode::ENCODE) {
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
  }
  RefreshDocumentState();
}

void DocumentSession::InvalidateDraft(std::size_t field_index) {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr) {
    return;
  }
  ++input_revision_;
  drafts_.erase(field_index);
  invalid_drafts_.erase(field_index);
  ClearPreview();
  ClearEncodeFailure();
  if (mode_ == OperationMode::ENCODE) {
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
  }
  RefreshDocumentState();
}

bool DocumentSession::SetInvalidDraft(std::size_t field_index, std::string text,
                                      std::string validation_error) {
  const MessageDescriptor* message = nullptr;
  if (!CurrentMessage(message) || field_index >= message->fields.size() ||
      message->fields[field_index].encode_source != protocol_plan::EncodeSource::INPUT ||
      validation_error.empty()) {
    return false;
  }
  ++input_revision_;
  drafts_.erase(field_index);
  invalid_drafts_[field_index] = InvalidDraftState{std::move(text), std::move(validation_error)};
  ClearPreview();
  SetEncodeFailure("UI_INPUT_INVALID", "field=" + message->fields[field_index].id + "; reason=" +
                                           invalid_drafts_[field_index].validation_error);
  RefreshDocumentState();
  return true;
}

bool DocumentSession::SetDraft(std::size_t field_index, TypedDraft value) {
  const MessageDescriptor* message = nullptr;
  if (!CurrentMessage(message) || field_index >= message->fields.size()) return false;
  const auto& field = message->fields[field_index];
  if (field.encode_source != protocol_plan::EncodeSource::INPUT || !DraftMatches(field, value)) {
    return false;
  }
  if (std::holds_alternative<EnumSelection>(value)) {
    const auto& selected = std::get<EnumSelection>(value);
    if (selected.entry_index >= field.enum_entries.size() ||
        field.enum_entries[selected.entry_index].id != selected.entry_id) {
      return false;
    }
  }
  InvalidateInput();
  drafts_[field_index] = std::move(value);
  invalid_drafts_.erase(field_index);
  ClearEncodeFailure();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  return true;
}

bool DocumentSession::Encode(protocol_lab::v06::ExecutionObserver* observer,
                             InputMaterializationTimer* input_materialization_timer,
                             std::int64_t* input_materialization_ns) {
  const MessageDescriptor* message = nullptr;
  if (!CurrentMessage(message) || prepared_ == nullptr || prepared_->bridge == nullptr) {
    SetEncodeFailure("UI_ENCODE_NOT_READY", "no complete Plan selection is ready");
    ClearPreview();
    return false;
  }
  if (input_materialization_timer != nullptr) {
    input_materialization_timer->Start();
  }
  protocol_lab::v06::ParsedValues values;
  values.format_version = description_->schema_version == "0.8"
                              ? std::string{protocol_lab::v06::kVariableValuesFormat}
                              : std::string{protocol_lab::v06::kValuesFormat};
  values.pipeline_id = selection_->pipeline_id;
  values.message_id = selection_->message_id;
  for (const auto& field : message->fields) {
    if (field.encode_source != protocol_plan::EncodeSource::INPUT) continue;
    const auto draft = drafts_.find(field.field_index);
    if (draft == drafts_.end() || !DraftMatches(field, draft->second)) {
      const auto invalid = invalid_drafts_.find(field.field_index);
      if (invalid != invalid_drafts_.end()) {
        SetEncodeFailure("UI_INPUT_INVALID",
                         "field=" + field.id + "; reason=" + invalid->second.validation_error);
      } else {
        SetEncodeFailure("UI_INPUT_INCOMPLETE",
                         "field=" + field.id + "; reason=no valid typed draft");
      }
      ClearPreview();
      RefreshDocumentState();
      return false;
    }
    protocol_lab::v06::ParsedValue parsed;
    parsed.id = field.id;
    parsed.kind = ValueKind(field);
    std::visit(
        [&parsed](const auto& item) {
          using T = std::decay_t<decltype(item)>;
          if constexpr (std::is_same_v<T, std::uint64_t>) {
            parsed.uint64_value = item;
          } else if constexpr (std::is_same_v<T, std::int64_t>) {
            parsed.int64_value = item;
          } else if constexpr (std::is_same_v<T, std::vector<std::uint8_t>>) {
            parsed.bytes = item;
          } else if constexpr (std::is_same_v<T, EnumSelection>) {
            parsed.enum_entry_id = item.entry_id;
          } else if constexpr (std::is_same_v<T, bool>) {
            parsed.bool_value = item;
          } else if constexpr (std::is_same_v<T, protocol_lab::v06::Decimal64>) {
            parsed.decimal64_value = protocol_lab::v06::NormalizeDecimal64(item);
          }
        },
        draft->second);
    values.fields.push_back(std::move(parsed));
  }
  if (input_materialization_timer != nullptr && input_materialization_ns != nullptr) {
    *input_materialization_ns = input_materialization_timer->ElapsedNanoseconds();
  }

  const PreviewKey requested_key = MakePreviewKey();
  auto outcome = prepared_->bridge->EncodeParsed(values
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                                 ,
                                                 nullptr
#endif
                                                 ,
                                                 observer);
  if (!PreviewKeyStillCurrent(requested_key) || !outcome.result.has_value() ||
      outcome.materialization_failure != protocol_lab::v06::MaterializationFailure::NONE ||
      outcome.main_codec_status != "OK" || outcome.review_decode_status != "OK" ||
      outcome.encoded_frame.empty()) {
    if (outcome.result.has_value() && outcome.result->diagnostic_id.has_value()) {
      SetEncodeFailure(*outcome.result->diagnostic_id, outcome.result->diagnostic_detail);
    } else if (!outcome.preparation_failure.diagnostic_id.empty()) {
      SetEncodeFailure(outcome.preparation_failure.diagnostic_id,
                       outcome.preparation_failure.detail);
    } else {
      SetEncodeFailure("UI_ENCODE_FAILED", "Core Encode or independent Decode review failed");
    }
    ClearPreview();
    RefreshDocumentState();
    return false;
  }
  PreviewResult published;
  published.key = requested_key;
  published.encoded_frame = std::move(outcome.encoded_frame);
  published.fields = std::move(outcome.result->fields);
  preview_ = std::move(published);
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  ClearEncodeFailure();
  RefreshDocumentState();
  return true;
}

bool DocumentSession::SetInspectDraft(std::string text) {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr ||
      !selected_pipeline_index_.has_value()) {
    return false;
  }
  inspect_draft_ = std::move(text);
  ++inspect_input_revision_;
  ClearInspectOutcome();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  RefreshDocumentState();
  return true;
}

bool DocumentSession::Inspect(protocol_lab::v06::ExecutionObserver* observer) {
  ++inspect_request_revision_;
  ClearInspectOutcome();
  if (prepared_ == nullptr || prepared_->bridge == nullptr || !description_.has_value() ||
      !selected_pipeline_index_.has_value() ||
      *selected_pipeline_index_ >= description_->pipelines.size()) {
    InspectFailure failure;
    failure.stage = InspectFailureStage::INPUT;
    failure.diagnostic_id = "UI_INSPECT_NOT_READY";
    failure.detail = "no compiled Plan and selected Pipeline are ready";
    inspect_failure_ = std::move(failure);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    RefreshDocumentState();
    return false;
  }

  const InspectResultKey requested_key = MakeInspectResultKey();
  const std::size_t budget = InspectFrameBudget();
  auto parsed = ParseInspectHex(inspect_draft_, budget);
  if (!parsed.ok()) {
    InspectFailure failure;
    failure.stage = InspectFailureStage::INPUT;
    failure.diagnostic_id = InspectHexDiagnosticId(parsed.error);
    failure.detail = InspectFailureDetail(InspectHexErrorDetail(parsed.error), parsed.input_offset);
    failure.input_offset = parsed.input_offset;
    inspect_failure_ = std::move(failure);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    RefreshDocumentState();
    return false;
  }

  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  auto outcome = prepared_->bridge->InspectPipeline(parsed.bytes, pipeline.id
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                                    ,
                                                    nullptr
#endif
                                                    ,
                                                    observer);
  if (!InspectKeyStillCurrent(requested_key)) return false;

  if (outcome.stage == protocol_lab::v06::ExecutionStage::STRUCTURAL_QUERY) {
    inspect_failure_ = MakeStructuralInspectFailure(outcome.structural_status, parsed.bytes);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    RefreshDocumentState();
    return false;
  }
  InspectFailure failure;
  failure.input_frame = parsed.bytes;
  if (!outcome.result.has_value()) {
    failure.stage = InspectFailureStage::MATERIALIZATION;
    failure.diagnostic_id = "UI_INSPECT_MATERIALIZATION_FAILED";
    failure.detail = "Inspect result mapping or materialization failed";
    inspect_failure_ = std::move(failure);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    RefreshDocumentState();
    return false;
  }

  std::size_t message_index = 0U;
  std::string message_id;
  if (!ResolveInspectMessage(*outcome.result, message_index, message_id)) {
    failure.stage = InspectFailureStage::MATERIALIZATION;
    failure.diagnostic_id = "UI_INSPECT_MESSAGE_IDENTITY_MISMATCH";
    failure.detail = "matched Message identity does not belong to the selected Pipeline";
    inspect_failure_ = std::move(failure);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    RefreshDocumentState();
    return false;
  }

  if (outcome.main_codec_status != "OK" || outcome.result->operation_status != "OK") {
    failure.stage = InspectFailureStage::CODEC;
    failure.status = outcome.main_codec_status;
    failure.message_index = message_index;
    failure.message_id = message_id;
    failure.failed_field_index = outcome.result->failed_field_index;
    failure.failed_field_id = outcome.result->failed_field_id;
    const bool has_field_index = failure.failed_field_index.has_value();
    const bool has_field_id = failure.failed_field_id.has_value();
    if (has_field_index != has_field_id ||
        (has_field_index &&
         (*failure.failed_field_index >= description_->messages[message_index].fields.size() ||
          description_->messages[message_index].fields[*failure.failed_field_index].id !=
              *failure.failed_field_id))) {
      failure.stage = InspectFailureStage::MATERIALIZATION;
      failure.failed_field_index.reset();
      failure.failed_field_id.reset();
      failure.diagnostic_id = "UI_INSPECT_FIELD_IDENTITY_MISMATCH";
      failure.detail = "failed field id/index does not match the UI description";
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    failure.diagnostic_id = outcome.result->diagnostic_id.value_or("UI_INSPECT_CODEC_FAILED");
    failure.detail = outcome.result->diagnostic_detail + "; status=" + outcome.main_codec_status;
    if (outcome.result->conversion_error.has_value()) {
      failure.detail += "; conversion_error=" + *outcome.result->conversion_error;
    }
    if (failure.failed_field_id.has_value()) {
      failure.detail += "; field=" + *failure.failed_field_id;
    }
    inspect_failure_ = std::move(failure);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    RefreshDocumentState();
    return false;
  }

  InspectResult result;
  result.key = requested_key;
  result.input_frame = std::move(parsed.bytes);
  result.message_index = message_index;
  result.message_id = std::move(message_id);
  result.fields = std::move(outcome.result->fields);
  inspect_result_ = std::move(result);
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  RefreshDocumentState();
  return true;
}

void DocumentSession::Close() {
  if (state_ == DocumentState::CLOSED) return;
  state_ = DocumentState::CLOSING;
  ClearSelectionAndPreview();
  inspect_draft_.clear();
  ClearInspectOutcome();
  description_.reset();
  prepared_.reset();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  state_ = DocumentState::CLOSED;
}

void DocumentSession::ClearSelectionAndPreview() {
  selected_pipeline_index_.reset();
  selection_.reset();
  drafts_.clear();
  invalid_drafts_.clear();
  ClearEncodeFailure();
  ClearPreview();
}

void DocumentSession::ClearPreview() { preview_.reset(); }

void DocumentSession::ClearInspectOutcome() {
  inspect_result_.reset();
  inspect_failure_.reset();
}

bool DocumentSession::SetInitialSelection() {
  if (!description_.has_value()) return false;
  for (const auto& pipeline : description_->pipelines) {
    if (!pipeline.message_indices.empty() && SelectPipeline(pipeline.pipeline_index) &&
        SelectMessage(pipeline.message_indices.front())) {
      return true;
    }
  }
  return false;
}

bool DocumentSession::CurrentMessage(const MessageDescriptor*& message) const noexcept {
  message = nullptr;
  if (!selection_.has_value() || !description_.has_value() ||
      selection_->plan_generation != plan_generation_ ||
      selection_->message_index >= description_->messages.size()) {
    return false;
  }
  const auto& selected = description_->messages[selection_->message_index];
  if (selected.id != selection_->message_id) return false;
  message = &selected;
  return true;
}

PreviewKey DocumentSession::MakePreviewKey() const {
  PreviewKey key;
  key.document_id = document_id_;
  key.load_revision = load_revision_;
  key.plan_generation = plan_generation_;
  key.selection_revision = selection_revision_;
  key.input_revision = input_revision_;
  if (selection_.has_value()) key.selection = *selection_;
  return key;
}

InspectResultKey DocumentSession::MakeInspectResultKey() const {
  InspectResultKey key;
  key.document_id = document_id_;
  key.load_revision = load_revision_;
  key.plan_generation = plan_generation_;
  key.pipeline_selection_revision = pipeline_selection_revision_;
  key.inspect_input_revision = inspect_input_revision_;
  key.inspect_request_revision = inspect_request_revision_;
  if (description_.has_value() && selected_pipeline_index_.has_value() &&
      *selected_pipeline_index_ < description_->pipelines.size()) {
    const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
    key.pipeline_index = pipeline.pipeline_index;
    key.pipeline_id = pipeline.id;
  }
  return key;
}

bool DocumentSession::PreviewKeyStillCurrent(const PreviewKey& key) const noexcept {
  return state_ != DocumentState::CLOSING && state_ != DocumentState::CLOSED &&
         selection_.has_value() && key.document_id == document_id_ &&
         key.load_revision == load_revision_ && key.plan_generation == plan_generation_ &&
         key.selection_revision == selection_revision_ && key.input_revision == input_revision_ &&
         SelectionEqual(key.selection, *selection_);
}

bool DocumentSession::InspectKeyStillCurrent(const InspectResultKey& key) const noexcept {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED ||
      !description_.has_value() || !selected_pipeline_index_.has_value() ||
      *selected_pipeline_index_ >= description_->pipelines.size()) {
    return false;
  }
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  return key.document_id == document_id_ && key.load_revision == load_revision_ &&
         key.plan_generation == plan_generation_ &&
         key.pipeline_selection_revision == pipeline_selection_revision_ &&
         key.inspect_input_revision == inspect_input_revision_ &&
         key.inspect_request_revision == inspect_request_revision_ &&
         key.pipeline_index == pipeline.pipeline_index && key.pipeline_id == pipeline.id;
}

bool DocumentSession::ResolveInspectMessage(const protocol_lab::v06::Result& result,
                                            std::size_t& message_index,
                                            std::string& message_id) const noexcept {
  if (!description_.has_value() || !selected_pipeline_index_.has_value() ||
      !result.message_id.has_value() ||
      *selected_pipeline_index_ >= description_->pipelines.size()) {
    return false;
  }
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  for (const std::size_t candidate : pipeline.message_indices) {
    if (candidate < description_->messages.size() &&
        description_->messages[candidate].id == *result.message_id) {
      message_index = candidate;
      message_id = *result.message_id;
      return true;
    }
  }
  return false;
}

std::size_t DocumentSession::InspectFrameBudget() const noexcept {
  if (!description_.has_value() || !selected_pipeline_index_.has_value() ||
      *selected_pipeline_index_ >= description_->pipelines.size()) {
    return 0U;
  }
  std::size_t pipeline_maximum = 0U;
  for (const std::size_t message_index :
       description_->pipelines[*selected_pipeline_index_].message_indices) {
    if (message_index < description_->messages.size()) {
      pipeline_maximum =
          (std::max)(pipeline_maximum, description_->messages[message_index].frame_size);
    }
  }
  return (std::min)({pipeline_maximum, description_->max_frame_bytes, std::size_t{65536U}});
}

void DocumentSession::RefreshDocumentState() {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED ||
      state_ == DocumentState::LOADING || state_ == DocumentState::CONFIG_ERROR) {
    return;
  }
  const bool valid =
      mode_ == OperationMode::ENCODE ? preview_.has_value() : inspect_result_.has_value();
  state_ = valid ? DocumentState::PREVIEW_VALID : DocumentState::READY;
}

void DocumentSession::SetDiagnostic(std::string id, std::string detail) {
  diagnostic_id_ = std::move(id);
  diagnostic_detail_ = std::move(detail);
}

void DocumentSession::ClearEncodeFailure() { encode_failure_.reset(); }

void DocumentSession::SetEncodeFailure(std::string id, std::string detail) {
  encode_failure_ = OperationDiagnostic{std::move(id), std::move(detail)};
  if (mode_ == OperationMode::ENCODE) {
    SetDiagnostic(encode_failure_->id, encode_failure_->detail);
  }
}

}  // namespace pae::protocol_lab_ui
