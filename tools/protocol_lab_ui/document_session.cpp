#include "document_session.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace pae::protocol_lab_ui {
namespace {

bool SelectionEqual(const SelectionKey& left, const SelectionKey& right) noexcept {
  return left.plan_generation == right.plan_generation &&
         left.pipeline_index == right.pipeline_index && left.pipeline_id == right.pipeline_id &&
         left.message_index == right.message_index && left.message_id == right.message_id;
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
      return std::holds_alternative<std::vector<std::uint8_t>>(value) &&
             std::get<std::vector<std::uint8_t>>(value).size() == field.byte_width;
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

DocumentSession::~DocumentSession() { Close(); }

Revision DocumentSession::BeginLoad() {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED) return load_revision_;
  ++load_revision_;
  prepared_.reset();
  description_.reset();
  ClearSelectionAndPreview();
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
  if (schema != "0.5" && schema != "0.6" && schema != "0.7") {
    SetDiagnostic("UI_SCHEMA_UNSUPPORTED", "the UI supports Schema 0.5, 0.6 and 0.7 only");
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
  selected_pipeline_index_ = pipeline_index;
  selection_.reset();
  drafts_.clear();
  ClearPreview();
  state_ = DocumentState::READY;
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
  ClearPreview();
  state_ = DocumentState::READY;
  return true;
}

void DocumentSession::InvalidateInput() {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr) {
    return;
  }
  ++input_revision_;
  ClearPreview();
  state_ = DocumentState::READY;
}

void DocumentSession::InvalidateDraft(std::size_t field_index) {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr) {
    return;
  }
  ++input_revision_;
  drafts_.erase(field_index);
  ClearPreview();
  state_ = DocumentState::READY;
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
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  return true;
}

bool DocumentSession::Encode(protocol_lab::v06::ExecutionObserver* observer,
                             InputMaterializationTimer* input_materialization_timer,
                             std::int64_t* input_materialization_ns) {
  const MessageDescriptor* message = nullptr;
  if (!CurrentMessage(message) || prepared_ == nullptr || prepared_->bridge == nullptr) {
    SetDiagnostic("UI_ENCODE_NOT_READY", "no complete Plan selection is ready");
    ClearPreview();
    return false;
  }
  if (input_materialization_timer != nullptr) {
    input_materialization_timer->Start();
  }
  protocol_lab::v06::ParsedValues values;
  values.format_version = std::string{protocol_lab::v06::kValuesFormat};
  values.pipeline_id = selection_->pipeline_id;
  values.message_id = selection_->message_id;
  for (const auto& field : message->fields) {
    if (field.encode_source != protocol_plan::EncodeSource::INPUT) continue;
    const auto draft = drafts_.find(field.field_index);
    if (draft == drafts_.end() || !DraftMatches(field, draft->second)) {
      SetDiagnostic("UI_INPUT_INCOMPLETE", "an input field has no valid typed draft");
      ClearPreview();
      state_ = DocumentState::READY;
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
      SetDiagnostic(*outcome.result->diagnostic_id, outcome.result->diagnostic_detail);
    } else if (!outcome.preparation_failure.diagnostic_id.empty()) {
      SetDiagnostic(outcome.preparation_failure.diagnostic_id, outcome.preparation_failure.detail);
    } else {
      SetDiagnostic("UI_ENCODE_FAILED", "Core Encode or independent Decode review failed");
    }
    ClearPreview();
    state_ = DocumentState::READY;
    return false;
  }
  PreviewResult published;
  published.key = requested_key;
  published.encoded_frame = std::move(outcome.encoded_frame);
  published.fields = std::move(outcome.result->fields);
  preview_ = std::move(published);
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  state_ = DocumentState::PREVIEW_VALID;
  return true;
}

void DocumentSession::Close() {
  if (state_ == DocumentState::CLOSED) return;
  state_ = DocumentState::CLOSING;
  ClearSelectionAndPreview();
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
  ClearPreview();
}

void DocumentSession::ClearPreview() { preview_.reset(); }

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

bool DocumentSession::PreviewKeyStillCurrent(const PreviewKey& key) const noexcept {
  return state_ != DocumentState::CLOSING && state_ != DocumentState::CLOSED &&
         selection_.has_value() && key.document_id == document_id_ &&
         key.load_revision == load_revision_ && key.plan_generation == plan_generation_ &&
         key.selection_revision == selection_revision_ && key.input_revision == input_revision_ &&
         SelectionEqual(key.selection, *selection_);
}

void DocumentSession::SetDiagnostic(std::string id, std::string detail) {
  diagnostic_id_ = std::move(id);
  diagnostic_detail_ = std::move(detail);
}

}  // namespace pae::protocol_lab_ui
