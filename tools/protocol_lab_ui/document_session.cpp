#include "document_session.h"

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) && \
    defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
#include "ascii_host_adapter_compat.h"
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
#include "ascii_host_types_compat.h"
#endif
#include "canonical_input.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
#include "public_binary_description.h"
#endif

#include <algorithm>
#include <limits>
#include <sstream>
#include <type_traits>
#include <utility>

namespace pae::protocol_lab_ui {
namespace {

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
protocol_lab::v06::ExecutionPhase ToLegacyPhase(LabExecutionPhase phase) {
  using L = LabExecutionPhase;
  using P = protocol_lab::v06::ExecutionPhase;
  switch (phase) {
    case L::PREPARATION: return P::PREPARATION;
    case L::STRUCTURAL_QUERY: return P::STRUCTURAL_QUERY;
    case L::MAIN_CODEC: return P::MAIN_CODEC;
    case L::REVIEW_DECODE: return P::REVIEW_DECODE;
    case L::RESULT_MAPPING: return P::RESULT_MAPPING;
  }
  return P::PREPARATION;
}

class LegacyExecutionObserverAdapter final : public protocol_lab::v06::ExecutionObserver {
 public:
  explicit LegacyExecutionObserverAdapter(LabExecutionObserver* observer) : observer_(observer) {}

  void PhaseStarted(protocol_lab::v06::ExecutionPhase phase) override {
    if (observer_) observer_->PhaseStarted(ToLabPhase(phase));
  }
  void PhaseFinished(protocol_lab::v06::ExecutionPhase phase, std::string_view status) override {
    if (observer_) observer_->PhaseFinished(ToLabPhase(phase), status);
  }

 private:
  static LabExecutionPhase ToLabPhase(protocol_lab::v06::ExecutionPhase phase) {
    using L = LabExecutionPhase;
    using P = protocol_lab::v06::ExecutionPhase;
    switch (phase) {
      case P::PREPARATION: return L::PREPARATION;
      case P::STRUCTURAL_QUERY: return L::STRUCTURAL_QUERY;
      case P::MAIN_CODEC: return L::MAIN_CODEC;
      case P::REVIEW_DECODE: return L::REVIEW_DECODE;
      case P::RESULT_MAPPING: return L::RESULT_MAPPING;
    }
    return L::PREPARATION;
  }
  LabExecutionObserver* observer_ = nullptr;
};
#endif

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

std::string AsciiInputFailureDetail(std::string_view detail, std::size_t utf16_offset) {
  std::ostringstream output;
  output << detail << " at input_utf16_code_unit_offset=" << utf16_offset << " (zero-based)";
  return output.str();
}

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
struct BinaryStreamProjection {
  BinaryUiStreamView view;
  std::optional<InspectResult> result;
  std::optional<InspectFailure> failure;
  std::size_t accounted_bytes = 0U;
  std::string diagnostic_id;
  std::string diagnostic_detail;
};

BinaryStreamProjection ProjectBinaryStreamView(BinaryUiStreamView view,
                                               InspectResultKey key) {
  BinaryStreamProjection projected;
  if (view.result) {
    projected.accounted_bytes = view.result->accounted_bytes;
    InspectResult result;
    result.key = std::move(key);
    result.input_frame = std::move(view.result->frame);
    result.message_index = view.result->message_index;
    result.message_id = std::move(view.result->message_id);
    result.fields = std::move(view.result->fields);
    result.zero_field_success = result.fields.empty();
    projected.result = std::move(result);
    view.result.reset();
  } else if (view.failure) {
    projected.accounted_bytes = view.failure->accounted_bytes;
    InspectFailure failure;
    failure.stage = view.status == BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE
                        ? InspectFailureStage::MATERIALIZATION
                        : InspectFailureStage::CODEC;
    failure.status = PublicBinaryCodecStatusName(view.public_host.codec_status);
    failure.diagnostic_id =
        view.status == BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE
            ? "UI_BINARY_STREAM_MATERIALIZATION_FAILED"
            : "UI_BINARY_STREAM_DECODE_FAILED";
    failure.detail =
        view.status == BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE
            ? "Binary stream result materialization failed; Reset is required"
            : "Binary stream candidate Decode failed";
    failure.input_frame = std::move(view.failure->diagnostic_frame);
    failure.message_index = view.failure->message_index;
    failure.failed_field_index = view.failure->failed_field_index;
    projected.diagnostic_id = failure.diagnostic_id;
    projected.diagnostic_detail = failure.detail;
    projected.failure = std::move(failure);
    view.failure.reset();
  } else if (view.status == BinaryStreamPresentationStatus::PREFLIGHT_REJECTED &&
             view.diagnostic != protocol_lab_binary::public_decode::StreamDiagnostic::NO_WORK) {
    InspectFailure failure;
    failure.stage = InspectFailureStage::INPUT;
    failure.status = "STREAM_PREFLIGHT_REJECTED";
    failure.diagnostic_id = "UI_BINARY_STREAM_PREFLIGHT_REJECTED";
    failure.detail = "Binary stream input was rejected before Host execution";
    projected.diagnostic_id = failure.diagnostic_id;
    projected.diagnostic_detail = failure.detail;
    projected.failure = std::move(failure);
  }
  projected.view = std::move(view);
  return projected;
}
#endif

bool DraftMatches(const FieldDescriptor& field, const TypedDraft& value) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field.decimal_conversion) {
    return std::holds_alternative<Decimal64>(value);
  }
#endif
  switch (field.value_type) {
    case FieldValueType::UINT64:
      return std::holds_alternative<std::uint64_t>(value);
    case FieldValueType::INT64:
      return std::holds_alternative<std::int64_t>(value);
    case FieldValueType::BYTES:
      if (!std::holds_alternative<std::vector<std::uint8_t>>(value)) return false;
      if (field.byte_length_bounds.has_value()) {
        const std::size_t size = std::get<std::vector<std::uint8_t>>(value).size();
        return size >= field.byte_length_bounds->minimum &&
               size <= field.byte_length_bounds->maximum;
      }
      return std::get<std::vector<std::uint8_t>>(value).size() == field.byte_width;
    case FieldValueType::ENUM:
      return std::holds_alternative<EnumSelection>(value);
    case FieldValueType::BOOL:
      return std::holds_alternative<bool>(value);
  }
  return false;
}

std::string ValueKind(const FieldDescriptor& field) {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field.decimal_conversion) return "DECIMAL64";
#endif
  switch (field.value_type) {
    case FieldValueType::UINT64:
      return "UINT64";
    case FieldValueType::INT64:
      return "INT64";
    case FieldValueType::BYTES:
      return "BYTES";
    case FieldValueType::ENUM:
      return "ENUM";
    case FieldValueType::BOOL:
      return "BOOL";
  }
  return {};
}

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
std::vector<UiFieldResult> CopyLegacyFields(
    const MessageDescriptor& message, const std::vector<protocol_lab::v06::FieldResult>& fields) {
  std::vector<UiFieldResult> copied;
  copied.reserve(fields.size());
  for (const auto& source : fields) {
    const auto found = std::find_if(message.fields.begin(), message.fields.end(),
                                    [&](const auto& field) { return field.id == source.id; });
    if (found == message.fields.end()) continue;
    UiFieldResult field;
    field.field_index = found->field_index;
    field.id = source.id;
    field.raw_value = source.raw_value;
    field.logical_value = source.logical_value;
    copied.push_back(std::move(field));
  }
  return copied;
}
#endif

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER) || \
    defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
bool IsAsciiDecodeAction(
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    AsciiHostAction action
#else
    host_endpoint::Action action
#endif
) noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  return action == AsciiHostAction::DECODE;
#else
  return action == host_endpoint::Action::DECODE;
#endif
}

std::vector<UiFieldResult> CopyAsciiFields(
    const std::vector<AsciiFieldResult>& fields) {
  std::vector<UiFieldResult> copied;
  copied.reserve(fields.size());
  for (const auto& source : fields) {
    UiFieldResult field;
    field.field_index = source.field_index;
    field.id = source.field_id;
    field.raw_value = FormatContinuousUpperHex(source.bytes);
    std::string error;
    const auto escaped = FormatAsciiEscaped(source.bytes, error);
    if (escaped.has_value()) {
      field.logical_value.reserve(escaped->size());
      for (const char16_t value : *escaped) field.logical_value.push_back(static_cast<char>(value));
    } else {
      field.logical_value = field.raw_value;
    }
    field.actual_range = ByteRange{source.range.offset, source.range.length};
    copied.push_back(std::move(field));
  }
  return copied;
}
#endif

}  // namespace

DocumentSession::DocumentSession(DocumentId document_id) : document_id_(document_id) {}

bool DocumentSession::AsciiBackendReady() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER) || \
    defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (!prepared_) return false;
  bool ready = false;
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  ready = ready || static_cast<bool>(prepared_->ascii_adapter);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  ready = ready || static_cast<bool>(prepared_->public_ascii_adapter);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  ready = ready || static_cast<bool>(prepared_->public_ascii_stream_adapter);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  ready = ready || static_cast<bool>(prepared_->host_adapter);
#endif
  return ready;
#else
  return false;
#endif
}

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
std::optional<DocumentSession::BinaryPublication> DocumentSession::PrepareBinaryHostPublication(
    std::unique_ptr<BinaryHostAdapter> adapter, Revision expected_request,
    BinaryPreparationHook before_copy) {
  if (!IsBinaryHostDocument() || !adapter || adapter->Bindings().empty() || !prepared_)
    return std::nullopt;
  if (binary_session_revision_ == (std::numeric_limits<Revision>::max)() ||
      plan_generation_ == (std::numeric_limits<Revision>::max)())
    return std::nullopt;
  const auto& identity = adapter->Identity();
  if (identity.document != document_id_ || identity.load != load_revision_ ||
      identity.session != binary_session_revision_ + 1U || identity.request != expected_request ||
      identity.config_sha256 != prepared_->config_sha256 ||
      adapter->Description().schema_version != "0.9")
    return std::nullopt;
  const auto& first = adapter->Bindings().front();
  if (first.action !=
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
          pae::HostAction::DECODE
#else
          host_endpoint::Action::DECODE
#endif
      || adapter->FlowCount(0U) == 0U)
    return std::nullopt;
  const auto found =
      std::find_if(adapter->Description().pipelines.begin(), adapter->Description().pipelines.end(),
                   [&](const auto& value) { return value.id == first.pipeline_id; });
  if (found == adapter->Description().pipelines.end() || found->message_indices.empty())
    return std::nullopt;
  const std::size_t initial_pipeline_index = found->pipeline_index;
  const std::size_t initial_message_index = found->message_indices.front();
  if (initial_message_index >= adapter->Description().messages.size()) return std::nullopt;
  try {
    if (before_copy != nullptr) before_copy();
    BinaryPublication publication;
    publication.session_revision = identity.session;
    publication.description = adapter->TakeDescription();
    const auto& pipeline = publication.description.pipelines[initial_pipeline_index];
    const auto& message = publication.description.messages[initial_message_index];
    publication.selection = SelectionKey{plan_generation_ + 1U, initial_pipeline_index, pipeline.id,
                                         initial_message_index, message.id};
    publication.adapter = std::move(adapter);
    return publication;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

void DocumentSession::PublishBinaryHostPublication(BinaryPublication publication) noexcept {
  static_assert(std::is_nothrow_move_assignable_v<DocumentDescription>);
  static_assert(std::is_nothrow_move_assignable_v<SelectionKey>);
  description_ = std::move(publication.description);
  prepared_->binary_host_adapter = std::move(publication.adapter);
  binary_encode_local_states_.clear();
  binary_encode_local_state_bytes_ = 0U;
  binary_host_binding_ = 0U;
  binary_host_flow_ = 0U;
  binary_session_revision_ = publication.session_revision;
  ++plan_generation_;
  ClearSelectionAndPreview();
  ClearInspectOutcome();
  inspect_draft_ = std::move(publication.inspect_draft);
  inspect_draft_utf16_ = std::move(publication.inspect_draft_utf16);
  inspect_result_ = std::move(publication.inspect_result);
  inspect_failure_ = std::move(publication.inspect_failure);
  selected_pipeline_index_ = publication.selection.pipeline_index;
  selection_ = std::move(publication.selection);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  mode_ = prepared_->binary_host_adapter->IsStreamDecode(0U, 0U)
              ? OperationMode::STREAM_INSPECT
              : OperationMode::INSPECT;
  binary_stream_view_.reset();
#else
  mode_ = OperationMode::INSPECT;
#endif
  representation_ = ByteRepresentation::HEX;
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  state_ = DocumentState::READY;
}

std::optional<DocumentSession::BinaryFlowPublication> DocumentSession::PrepareBinaryHostFlow(
    std::size_t binding, std::size_t flow, BinaryPreparationHook before_copy) const {
  if (!BinaryHostActive() || binding >= prepared_->binary_host_adapter->Bindings().size() ||
      flow >= prepared_->binary_host_adapter->FlowCount(binding))
    return std::nullopt;
  if (binding == binary_host_binding_ && flow == binary_host_flow_) {
    BinaryFlowPublication unchanged;
    unchanged.binding = binding;
    unchanged.flow = flow;
    unchanged.pipeline_index = *selected_pipeline_index_;
    return unchanged;
  }
  const auto& target = prepared_->binary_host_adapter->Bindings()[binding];
  const auto found =
      std::find_if(description_->pipelines.begin(), description_->pipelines.end(),
                   [&](const auto& value) { return value.id == target.pipeline_id; });
  if (found == description_->pipelines.end()) return std::nullopt;
  const bool encode = target.action == pae::HostAction::ENCODE;
  const auto& allowed = encode ? found->encode_message_indices : found->decode_message_indices;
  if (allowed.empty()) return std::nullopt;
  const auto saved_message = encode
      ? prepared_->binary_host_adapter->MessageSelection(binding, flow) : std::nullopt;
  const auto message_index = saved_message.value_or(allowed.front());
  if (std::find(allowed.begin(), allowed.end(), message_index) == allowed.end())
    return std::nullopt;
  if (message_index >= description_->messages.size()) return std::nullopt;
  try {
    if (before_copy != nullptr) before_copy();
    BinaryFlowPublication publication;
    publication.binding = binding;
    publication.flow = flow;
    publication.pipeline_index = found->pipeline_index;
    publication.mode = encode ? OperationMode::ENCODE : OperationMode::INSPECT;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (!encode && prepared_->binary_host_adapter->IsStreamDecode(binding, flow))
      publication.mode = OperationMode::STREAM_INSPECT;
#endif
    publication.selection = SelectionKey{plan_generation_, found->pipeline_index, found->id,
                                         message_index, description_->messages[message_index].id};
    if (encode) {
      publication.drafts = prepared_->binary_host_adapter->TypedDrafts(binding, flow);
      const auto local = binary_encode_local_states_.find({binding, flow});
      if (local != binary_encode_local_states_.end()) {
        publication.invalid_drafts = local->second.invalid_drafts;
        publication.encode_failure = local->second.encode_failure;
      }
      auto view = prepared_->binary_host_adapter->MapCurrentEncode(
          binding, flow, binary_active_view_bytes_);
      if (view.result) {
        if (view.result->message_index != message_index) return std::nullopt;
        publication.mapped_view_bytes = view.result->accounted_bytes;
        PreviewResult result;
        result.key = PreviewKey{document_id_, load_revision_, plan_generation_,
                                selection_revision_ + 1U, input_revision_ + 1U,
                                publication.selection};
        result.encoded_frame = std::move(view.result->frame);
        result.fields = std::move(view.result->fields);
        result.zero_field_success = result.fields.empty();
        publication.preview = std::move(result);
        publication.encode_failure.reset();
      } else if (view.failure) {
        publication.encode_failure = OperationDiagnostic{
            "UI_BINARY_HOST_ENCODE_FAILED",
            std::string{"Binary Host complete-record Encode failed: "} +
                PublicBinaryCodecStatusName(view.public_host.codec_status)};
      }
      if (publication.encode_failure) {
        publication.diagnostic_id = publication.encode_failure->id;
        publication.diagnostic_detail = publication.encode_failure->detail;
      }
      return publication;
    }
    const auto draft = prepared_->binary_host_adapter->Draft(binding, flow);
    publication.inspect_draft_utf16.assign(draft.begin(), draft.end());
    publication.inspect_draft.reserve(draft.size());
    for (const auto value : draft)
      publication.inspect_draft.push_back(value <= 0xFFU ? static_cast<char>(value) : '?');
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (prepared_->binary_host_adapter->IsStreamDecode(binding, flow)) {
      auto projected = ProjectBinaryStreamView(
          prepared_->binary_host_adapter->MapCurrentStream(binding, flow,
                                                            binary_active_view_bytes_),
          InspectResultKey{document_id_, load_revision_, plan_generation_,
                           pipeline_selection_revision_ + 1U, inspect_input_revision_ + 1U,
                           inspect_request_revision_, found->pipeline_index, found->id});
      publication.mapped_view_bytes = projected.accounted_bytes;
      publication.inspect_result = std::move(projected.result);
      publication.inspect_failure = std::move(projected.failure);
      publication.stream_view = std::move(projected.view);
      publication.diagnostic_id = std::move(projected.diagnostic_id);
      publication.diagnostic_detail = std::move(projected.diagnostic_detail);
      return publication;
    }
#endif
    auto view = prepared_->binary_host_adapter->MapCurrent(binding, flow,
                                                           binary_active_view_bytes_);
    InspectResultKey key;
    key.document_id = document_id_;
    key.load_revision = load_revision_;
    key.plan_generation = plan_generation_;
    key.pipeline_selection_revision = pipeline_selection_revision_ + 1U;
    key.inspect_input_revision = inspect_input_revision_ + 1U;
    key.inspect_request_revision = inspect_request_revision_;
    key.pipeline_index = found->pipeline_index;
    key.pipeline_id = found->id;
    if (view.result) {
      publication.mapped_view_bytes = view.result->accounted_bytes;
      InspectResult result;
      result.key = std::move(key);
      result.input_frame = std::move(view.result->frame);
      result.message_index = view.result->message_index;
      result.message_id = std::move(view.result->message_id);
      result.fields = std::move(view.result->fields);
      result.zero_field_success = result.fields.empty();
      publication.inspect_result = std::move(result);
    } else if (view.failure) {
      publication.mapped_view_bytes = view.failure->accounted_bytes;
      InspectFailure failure;
      failure.stage = InspectFailureStage::CODEC;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
      failure.status = PublicBinaryCodecStatusName(view.public_host.codec_status);
#else
      failure.status = std::string{protocol_lab::ascii::CodecStatusName(view.host.codec_status)};
#endif
      failure.diagnostic_id = "UI_BINARY_HOST_DECODE_FAILED";
      failure.detail = "Binary Host complete-record Decode failed";
      failure.input_frame = std::move(view.failure->diagnostic_frame);
      failure.message_index = view.failure->message_index;
      failure.failed_field_index = view.failure->failed_field_index;
      publication.inspect_failure = std::move(failure);
    }
    return publication;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

bool DocumentSession::PublishBinaryHostFlow(BinaryFlowPublication publication,
                                            BinaryPreparationHook before_local_cache_copy) {
  if (!BinaryHostActive() ||
      publication.binding >= prepared_->binary_host_adapter->Bindings().size() ||
      publication.flow >= prepared_->binary_host_adapter->FlowCount(publication.binding))
    return false;
  if (publication.binding == binary_host_binding_ && publication.flow == binary_host_flow_)
    return true;
  const bool save_local_encode =
      prepared_->binary_host_adapter->IsCompleteEncode(binary_host_binding_, binary_host_flow_);
  BinaryEncodeLocalState pending_local;
  std::map<std::pair<std::size_t, std::size_t>, BinaryEncodeLocalState>::iterator local_slot;
  bool inserted_local_slot = false;
  std::size_t retained_local_bytes = binary_encode_local_state_bytes_;
  try {
    if (save_local_encode) {
      if (before_local_cache_copy != nullptr) before_local_cache_copy();
      pending_local.invalid_drafts = invalid_drafts_;
      pending_local.encode_failure = encode_failure_;

      if (pending_local.invalid_drafts.size() >
          (std::numeric_limits<std::size_t>::max)() /
              sizeof(std::pair<const std::size_t, InvalidDraftState>))
        return false;
      std::size_t bytes = sizeof(std::pair<
          const std::pair<std::size_t, std::size_t>, BinaryEncodeLocalState>);
      const auto add = [&](std::size_t value) {
        if (value > (std::numeric_limits<std::size_t>::max)() - bytes) return false;
        bytes += value;
        return true;
      };
      if (!add(pending_local.invalid_drafts.size() *
               sizeof(std::pair<const std::size_t, InvalidDraftState>)))
        return false;
      for (const auto& item : pending_local.invalid_drafts) {
        if (item.second.text.capacity() >
                (std::numeric_limits<std::size_t>::max)() / sizeof(char16_t) ||
            !add(item.second.text.capacity() * sizeof(char16_t)) ||
            item.second.validation_error.capacity() ==
                (std::numeric_limits<std::size_t>::max)() ||
            !add(item.second.validation_error.capacity() + 1U))
          return false;
      }
      if (pending_local.encode_failure &&
          (!add(sizeof(OperationDiagnostic)) ||
           pending_local.encode_failure->id.capacity() ==
               (std::numeric_limits<std::size_t>::max)() ||
           !add(pending_local.encode_failure->id.capacity() + 1U) ||
           pending_local.encode_failure->detail.capacity() ==
               (std::numeric_limits<std::size_t>::max)() ||
           !add(pending_local.encode_failure->detail.capacity() + 1U)))
        return false;
      const auto key = std::make_pair(binary_host_binding_, binary_host_flow_);
      const auto existing = binary_encode_local_states_.find(key);
      if (existing != binary_encode_local_states_.end()) {
        if (existing->second.accounted_bytes > retained_local_bytes) return false;
        retained_local_bytes -= existing->second.accounted_bytes;
      }
      const auto budget = prepared_->binary_host_adapter->UiViewReserveBytes() / 4U;
      if (retained_local_bytes > budget || bytes > budget - retained_local_bytes) return false;
      pending_local.accounted_bytes = bytes;

      const auto inserted = binary_encode_local_states_.try_emplace(key);
      local_slot = inserted.first;
      inserted_local_slot = inserted.second;
    }
    if (!prepared_->binary_host_adapter->SaveDraftsAndSelect(
            inspect_draft_utf16_, drafts_, selection_ ? selection_->message_index : 0U,
            publication.binding, publication.flow)) {
      if (inserted_local_slot) binary_encode_local_states_.erase(local_slot);
      return false;
    }
  } catch (const std::exception&) {
    if (inserted_local_slot) binary_encode_local_states_.erase(local_slot);
    return false;
  }
  if (save_local_encode) {
    static_assert(std::is_nothrow_move_assignable_v<BinaryEncodeLocalState>);
    local_slot->second = std::move(pending_local);
    binary_encode_local_state_bytes_ =
        retained_local_bytes + local_slot->second.accounted_bytes;
  }
  binary_host_binding_ = publication.binding;
  binary_host_flow_ = publication.flow;
  ++selection_revision_;
  ++pipeline_selection_revision_;
  ++inspect_input_revision_;
  selected_pipeline_index_ = publication.pipeline_index;
  selection_ = std::move(publication.selection);
  drafts_ = std::move(publication.drafts);
  invalid_drafts_ = std::move(publication.invalid_drafts);
  preview_ = std::move(publication.preview);
  ClearInspectOutcome();
  inspect_draft_ = std::move(publication.inspect_draft);
  inspect_draft_utf16_ = std::move(publication.inspect_draft_utf16);
  inspect_result_ = std::move(publication.inspect_result);
  inspect_failure_ = std::move(publication.inspect_failure);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  binary_stream_view_ = std::move(publication.stream_view);
#endif
  binary_active_view_bytes_ = publication.mapped_view_bytes;
  mode_ = publication.mode;
  representation_ = ByteRepresentation::HEX;
  encode_failure_ = std::move(publication.encode_failure);
  diagnostic_id_ = std::move(publication.diagnostic_id);
  diagnostic_detail_ = std::move(publication.diagnostic_detail);
  RefreshDocumentState();
  return true;
}

bool DocumentSession::BinaryHasDiscardableState() const noexcept {
  if (!BinaryHostActive()) return false;
  if (!inspect_draft_.empty() || inspect_result_ || inspect_failure_) return true;
  const auto& adapter = *prepared_->binary_host_adapter;
  for (std::size_t binding = 0U; binding < adapter.Bindings().size(); ++binding)
    for (std::size_t flow = 0U; flow < adapter.FlowCount(binding); ++flow)
      if (!adapter.Draft(binding, flow).empty() || adapter.Current(binding, flow) != nullptr
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
          || (adapter.ObserveStream(binding, flow).has_value() &&
              (adapter.ObserveStream(binding, flow)->generation != 0U ||
               adapter.ObserveStream(binding, flow)->buffered_bytes != 0U ||
               adapter.ObserveStream(binding, flow)->frozen_input_bytes != 0U ||
               adapter.ObserveStream(binding, flow)->reset_required))
#endif
      )
        return true;
  return false;
}

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
std::optional<StreamPresentationObservation> DocumentSession::BinaryStreamObservation()
    const noexcept {
  if (!BinaryHostActive()) return std::nullopt;
  return prepared_->binary_host_adapter->ObserveStream(binary_host_binding_, binary_host_flow_);
}
#endif
#endif

#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
bool DocumentSession::ApplyHostAdapter(
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    std::unique_ptr<AsciiHostAdapter> adapter) {
#else
    std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> adapter) {
#endif
  if (!IsAsciiDocument() || !adapter || adapter->Bindings().empty()) return false;
  DocumentDescription mapped;
  std::string error;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  mapped = adapter->Description();
#else
  if (!BuildDocumentDescription(adapter->Description(), mapped, error)) return false;
#endif
  std::vector<HostView> views(adapter->Bindings().size() * 2U);
  if (plan_generation_ == (std::numeric_limits<Revision>::max)()) return false;
  const auto& first_binding = adapter->Bindings().front();
  const auto& first_pipeline = mapped.pipelines[first_binding.pipeline_index];
  if (first_pipeline.message_indices.empty()) return false;
  const auto& first_message = mapped.messages[first_pipeline.message_indices.front()];
  SelectionKey initial{plan_generation_ + 1U, first_binding.pipeline_index, first_pipeline.id,
                       first_message.message_index, first_message.id};
  const auto initial_mode = !IsAsciiDecodeAction(first_binding.action)
                                ? OperationMode::ENCODE
                                : (first_pipeline.stream_ascii_crlf ? OperationMode::STREAM_INSPECT
                                                                    : OperationMode::INSPECT);
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  prepared_->ascii_adapter.reset();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  prepared_->public_ascii_adapter.reset();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  prepared_->public_ascii_stream_adapter.reset();
#endif
  prepared_->host_adapter = std::move(adapter);
  description_ = std::move(mapped);
  ++plan_generation_;
  host_views_ = std::move(views);
  host_binding_ = 0U;
  host_stream_ = 0U;
  ClearSelectionAndPreview();
  ClearInspectOutcome();
  inspect_draft_.clear();
  inspect_draft_utf16_.clear();
  submitted_stream_input_revision_.reset();
  selected_pipeline_index_ = initial.pipeline_index;
  selection_ = std::move(initial);
  mode_ = initial_mode;
  representation_ = ByteRepresentation::HEX;
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  state_ = DocumentState::READY;
  return true;
}
void DocumentSession::SaveHostView() {
  if (!HostActive() || !selected_pipeline_index_) return;
  auto& saved = host_views_[host_binding_ * 2U + host_stream_];
  saved.draft = inspect_draft_;
  saved.utf16 = inspect_draft_utf16_;
  saved.representation = representation_;
  saved.result = inspect_result_;
  saved.failure = inspect_failure_;
  saved.step = stream_step_;
  saved.submitted = submitted_stream_input_revision_;
  saved.input_revision = inspect_input_revision_;
  saved.encode_drafts = drafts_;
  saved.invalid_drafts = invalid_drafts_;
  saved.preview = preview_;
  saved.encode_failure = encode_failure_;
  saved.selection = selection_;
  saved.diagnostic_id = diagnostic_id_;
  saved.diagnostic_detail = diagnostic_detail_;
}
bool DocumentSession::SelectHostFlow(std::size_t b, std::size_t s) {
  if (!HostActive() || b >= prepared_->host_adapter->Bindings().size()) return false;
  const auto& binding = prepared_->host_adapter->Bindings()[b];
  if (s >= (IsAsciiDecodeAction(binding.action) ? 2U : 1U)) return false;
  if (b == host_binding_ && s == host_stream_ && selected_pipeline_index_) return true;
  SaveHostView();
  host_binding_ = b;
  host_stream_ = s;
  if (!SelectPipeline(binding.pipeline_index)) return false;
  mode_ = !IsAsciiDecodeAction(binding.action)
              ? OperationMode::ENCODE
              : (description_->pipelines[binding.pipeline_index].stream_ascii_crlf
                     ? OperationMode::STREAM_INSPECT
                     : OperationMode::INSPECT);
  const auto& messages = description_->pipelines[binding.pipeline_index].message_indices;
  if (!messages.empty()) SelectMessage(messages.front());
  const auto& saved = host_views_[b * 2U + s];
  inspect_draft_ = saved.draft;
  inspect_draft_utf16_ = saved.utf16;
  representation_ = saved.representation;
  inspect_input_revision_ = saved.input_revision;
  inspect_result_ = saved.result;
  inspect_failure_ = saved.failure;
  stream_step_ = saved.step;
  submitted_stream_input_revision_ = saved.submitted;
  drafts_ = saved.encode_drafts;
  invalid_drafts_ = saved.invalid_drafts;
  preview_ = saved.preview;
  encode_failure_ = saved.encode_failure;
  if (saved.selection) {
    selection_ = saved.selection;
    selection_->plan_generation = plan_generation_;
  }
  if (inspect_result_) inspect_result_->key = MakeInspectResultKey();
  if (preview_) preview_->key = MakePreviewKey();
  diagnostic_id_ = saved.diagnostic_id;
  diagnostic_detail_ = saved.diagnostic_detail;
  RefreshDocumentState();
  return true;
}
void DocumentSession::ResetAllHostStreams() {
  if (!HostActive()) return;
  for (std::size_t b = 0U; b < prepared_->host_adapter->Bindings().size(); ++b)
    for (std::size_t s = 0U; s < 2U; ++s)
      if (prepared_->host_adapter->Reset(b, s)) host_views_[b * 2U + s] = {};
  inspect_draft_.clear();
  inspect_draft_utf16_.clear();
  submitted_stream_input_revision_.reset();
  ClearInspectOutcome();
}
bool DocumentSession::HostHasDiscardableState() const noexcept {
  if (!HostActive()) return false;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  if (!prepared_->host_adapter->IsPublicCompleteRecord())
    return prepared_->host_adapter->HasDiscardableState();
#endif
  if (!inspect_draft_.empty() || inspect_result_ || inspect_failure_ || !drafts_.empty() ||
      !invalid_drafts_.empty() || preview_ || encode_failure_)
    return true;
  for (const auto& view : host_views_)
    if (!view.draft.empty() || view.result || view.failure || !view.encode_drafts.empty() ||
        !view.invalid_drafts.empty() || view.preview || view.encode_failure)
      return true;
  return false;
}
#endif

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
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  host_views_.clear();
#endif
  prepared_.reset();
  description_.reset();
  ClearSelectionAndPreview();
  inspect_draft_.clear();
  inspect_draft_utf16_.clear();
  representation_ = ByteRepresentation::HEX;
  ++inspect_input_revision_;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  submitted_stream_input_revision_.reset();
#endif
  ClearInspectOutcome();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  compile_diagnostic_.reset();
  state_ = DocumentState::LOADING;
  return load_revision_;
}

bool DocumentSession::ApplyCompileCompletion(std::unique_ptr<CompileCompletion> completion) {
  if (completion == nullptr || state_ != DocumentState::LOADING ||
      completion->document_id != document_id_ || completion->load_revision != load_revision_) {
    return false;
  }
  compile_diagnostic_.reset();
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  if (completion->route == SchemaDispatchStatus::CLASSIFICATION_FAILED) {
    SetDiagnostic("UI_SCHEMA_CLASSIFICATION_FAILED", completion->classification_error);
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  if (completion->route == SchemaDispatchStatus::LEGACY_PUBLIC) {
    if (!completion->public_compiled || completion->public_diagnostic) {
      SetCompileDiagnostic("UI_PUBLIC_LEGACY_COMPILE_FAILED",
                           completion->structured_compile_diagnostic,
                           completion->public_diagnostic ? completion->public_diagnostic->detail
                                                         : "public compiler returned no owner");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    auto adopted = PublicLegacyCompleteAdapter::AdoptCompiled(
        std::move(*completion->public_compiled));
    if (!adopted.adapter || adopted.status != PublicLegacyLocalStatus::OK) {
      SetDiagnostic("UI_PUBLIC_LEGACY_PREPARATION_FAILED",
                    adopted.detail.empty() ? "public complete-record adapter rejected owner"
                                           : std::move(adopted.detail));
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    auto prepared = std::make_unique<PreparedDocument>();
    prepared->config_sha256 = std::move(completion->config_sha256);
    prepared->public_legacy_adapter = std::move(adopted.adapter);
    prepared_ = std::move(prepared);
    description_ = std::move(adopted.description);
    ++plan_generation_;
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    if (!SetInitialSelection()) {
      prepared_.reset();
      description_.reset();
      SetDiagnostic("UI_EMPTY_PLAN_SELECTION", "public legacy description has no selection");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    state_ = DocumentState::READY;
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (completion->route == SchemaDispatchStatus::BINARY_PUBLIC) {
    if (!completion->public_compiled || completion->public_diagnostic) {
      SetCompileDiagnostic("UI_PUBLIC_BINARY_COMPILE_FAILED",
                           completion->structured_compile_diagnostic,
                           completion->public_diagnostic ? completion->public_diagnostic->detail
                                                         : "public compiler returned no owner");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    DocumentDescription neutral;
    std::string mapping_error;
    if (!BuildPublicBinaryDescription(*completion->public_compiled, neutral, mapping_error)) {
      SetDiagnostic("UI_PUBLIC_BINARY_MAPPING_FAILED", std::move(mapping_error));
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    prepared_ = std::make_unique<PreparedDocument>();
    prepared_->config_sha256 = std::move(completion->config_sha256);
    description_ = std::move(neutral);
    ++plan_generation_;
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    if (!SetInitialSelection()) {
      prepared_.reset();
      description_.reset();
      SetDiagnostic("UI_EMPTY_PLAN_SELECTION", "public Binary description has no selection");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    mode_ = OperationMode::INSPECT;
    state_ = DocumentState::READY;
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  if (completion->route == SchemaDispatchStatus::ASCII_PUBLIC) {
    if (!completion->public_compiled || completion->public_diagnostic) {
      SetCompileDiagnostic("UI_PUBLIC_ASCII_COMPILE_FAILED",
                           completion->structured_compile_diagnostic,
                           completion->public_diagnostic ? completion->public_diagnostic->detail
                                                         : "public compiler returned no owner");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    DocumentDescription neutral;
    std::string mapping_error;
    const auto protocol = completion->public_compiled->Protocol();
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
    if (protocol && protocol->schema_version == "0.11") {
      auto stream = AsciiHostAdapter::CreatePublicDirect(std::move(*completion->public_compiled),
                                                         mapping_error);
      if (!stream) {
        SetDiagnostic("UI_PUBLIC_ASCII_PREPARATION_FAILED", std::move(mapping_error));
        state_ = DocumentState::CONFIG_ERROR;
        return false;
      }
      neutral = stream->Description();
      auto prepared = std::make_unique<PreparedDocument>();
      prepared->config_sha256 = std::move(completion->config_sha256);
      prepared->public_ascii_stream_adapter = std::move(stream);
      prepared_ = std::move(prepared);
      description_ = std::move(neutral);
      ++plan_generation_;
      diagnostic_id_.clear();
      diagnostic_detail_.clear();
      if (!SetInitialSelection()) {
        prepared_.reset();
        description_.reset();
        SetDiagnostic("UI_EMPTY_PLAN_SELECTION", "public ASCII description has no selection");
        state_ = DocumentState::CONFIG_ERROR;
        return false;
      }
      state_ = DocumentState::READY;
      return true;
    }
#endif
    auto adopted = protocol_lab_ascii::public_offline::Adapter::AdoptCompiled(
        std::move(*completion->public_compiled));
    if (!adopted.adapter ||
        !BuildDocumentDescription(adopted.adapter->Description(), neutral, mapping_error)) {
      SetDiagnostic("UI_PUBLIC_ASCII_PREPARATION_FAILED",
                    mapping_error.empty() ? "public ASCII adapter rejected compiled owner"
                                          : std::move(mapping_error));
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    auto prepared = std::make_unique<PreparedDocument>();
    prepared->config_sha256 = std::move(completion->config_sha256);
    prepared->public_ascii_adapter = std::move(adopted.adapter);
    prepared_ = std::move(prepared);
    description_ = std::move(neutral);
    ++plan_generation_;
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    if (!SetInitialSelection()) {
      prepared_.reset();
      description_.reset();
      SetDiagnostic("UI_EMPTY_PLAN_SELECTION", "public ASCII description has no selection");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    state_ = DocumentState::READY;
    return true;
  }
#endif
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (completion->artifacts == nullptr || completion->diagnostic.has_value()) {
    if (completion->diagnostic.has_value()) {
      SetCompileDiagnostic("UI_CONFIG_COMPILE_FAILED", completion->structured_compile_diagnostic,
                           completion->diagnostic->detail);
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
  const bool legacy_schema =
      schema == "0.5" || schema == "0.6" || schema == "0.7" || schema == "0.8";
  const bool ascii_schema = schema == "0.10"
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
                            || schema == "0.11"
#endif
      ;
  const bool binary_host_schema = schema == "0.9"
#if !defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
                                  && false
#endif
      ;
  if (!legacy_schema && !ascii_schema && !binary_host_schema) {
    SetDiagnostic("UI_SCHEMA_UNSUPPORTED", "the UI supports its explicitly compiled schemas");
    state_ = DocumentState::CONFIG_ERROR;
    return false;
  }

  DocumentDescription neutral_description;
  std::string mapping_error;
  auto prepared = std::make_unique<PreparedDocument>();
  prepared->config_sha256 = std::move(completion->config_sha256);
  if (legacy_schema || binary_host_schema) {
    if (!BuildDocumentDescription(*plan, artifacts.Description(), neutral_description,
                                  mapping_error)) {
      SetDiagnostic("UI_DESCRIPTION_MAPPING_FAILED", std::move(mapping_error));
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    if (binary_host_schema) {
      // Schema 0.9 starts deliberately unbound. Keep only the neutral UI description and
      // config identity; an explicit Apply compiles a fresh uniquely-owned candidate.
    } else {
      prepared->description = artifacts.TakeDescription();
      protocol_lab::v06::PreparationFailure failure;
      prepared->bridge = protocol_lab::v06::ExecutionBridge::AdoptCompiledPlan(
          artifacts.TakePlan(), prepared->config_sha256, failure);
      if (prepared->bridge == nullptr) {
        SetDiagnostic(failure.diagnostic_id.empty() ? "UI_BRIDGE_ADOPTION_FAILED"
                                                    : std::move(failure.diagnostic_id),
                      std::move(failure.detail));
        state_ = DocumentState::CONFIG_ERROR;
        return false;
      }
    }
  } else {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER) || \
    defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    if (!protocol_lab::ascii::OfflineAdapter::Supports(artifacts)) {
      SetDiagnostic("UI_ASCII_ADAPTER_UNSUPPORTED", "ASCII adapter rejected the compiled schema");
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
    prepared->ascii_adapter = protocol_lab::ascii::OfflineAdapter::AdoptCompiledArtifacts(
        std::move(artifacts), mapping_error);
    if (prepared->ascii_adapter == nullptr ||
        !BuildDocumentDescription(prepared->ascii_adapter->Description(), neutral_description,
                                  mapping_error)) {
      SetDiagnostic("UI_ASCII_ADAPTER_PREPARATION_FAILED", std::move(mapping_error));
      state_ = DocumentState::CONFIG_ERROR;
      return false;
    }
#else
    SetDiagnostic("UI_SCHEMA_UNSUPPORTED", "Schema 0.10 UI support is not compiled in");
    state_ = DocumentState::CONFIG_ERROR;
    return false;
#endif
  }
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (binary_host_schema) mode_ = OperationMode::INSPECT;
#endif
  state_ = DocumentState::READY;
  return true;
#else
  SetDiagnostic("UI_SCHEMA_UNSUPPORTED",
                completion->diagnostic ? completion->diagnostic->detail
                                       : "schema has no installed-SDK public route");
  state_ = DocumentState::CONFIG_ERROR;
  return false;
#endif
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
  inspect_draft_utf16_.clear();
  ++inspect_input_revision_;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  submitted_stream_input_revision_.reset();
#endif
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

bool DocumentSession::SetRepresentation(ByteRepresentation representation) {
  if (!IsAsciiDocument()) return representation == ByteRepresentation::HEX;
  if (representation_ == representation) return true;

  const auto reject_conversion = [&](std::string reason) {
    const char* current = representation_ == ByteRepresentation::HEX ? "Hex" : "ASCII (escaped)";
    const char* requested = representation == ByteRepresentation::HEX ? "Hex" : "ASCII (escaped)";
    SetDiagnostic("UI_ASCII_INPUT_INVALID",
                  std::string("Representation switch rejected (") + current + " -> " + requested +
                      "). Current format remains " + current +
                      "; draft and stream state preserved. " + "Reason: " + reason +
                      ". Correct the draft in the current format, or copy/clear the draft, "
                      "switch format, then enter new input.");
    return false;
  };

  std::unordered_map<std::size_t, InvalidDraftState> converted_invalid;
  for (const auto& item : invalid_drafts_) {
    std::vector<std::uint8_t> bytes;
    if (representation_ == ByteRepresentation::ASCII_ESCAPED) {
      const auto parsed = ParseAsciiEscaped(item.second.text);
      if (!parsed.ok()) {
        return reject_conversion(parsed.detail);
      }
      bytes = parsed.bytes;
    } else {
      std::string source;
      source.reserve(item.second.text.size());
      for (const char16_t value : item.second.text) {
        if (value > 0x7FU) {
          return reject_conversion("Hex draft contains non-ASCII Unicode");
        }
        source.push_back(static_cast<char>(value));
      }
      std::size_t error_offset = 0U;
      if (!ParseContinuousUpperHex(source, bytes, error_offset)) {
        return reject_conversion("Hex draft is not continuous uppercase Hex");
      }
    }
    InvalidDraftState converted;
    converted.validation_error = item.second.validation_error;
    if (representation == ByteRepresentation::ASCII_ESCAPED) {
      std::string error;
      const auto formatted = FormatAsciiEscaped(bytes, error);
      if (!formatted.has_value()) {
        return reject_conversion(std::move(error));
      }
      converted.text = *formatted;
    } else {
      const auto formatted = FormatContinuousUpperHex(bytes);
      converted.text.assign(formatted.begin(), formatted.end());
    }
    converted_invalid.emplace(item.first, std::move(converted));
  }

  if (!inspect_draft_utf16_.empty()) {
    std::vector<std::uint8_t> bytes;
    if (representation_ == ByteRepresentation::ASCII_ESCAPED) {
      const auto parsed = ParseAsciiEscaped(inspect_draft_utf16_);
      if (!parsed.ok()) {
        return reject_conversion(parsed.detail);
      }
      bytes = parsed.bytes;
    } else {
      std::string source;
      source.reserve(inspect_draft_utf16_.size());
      for (const char16_t value : inspect_draft_utf16_) {
        if (value > 0x7FU) {
          return reject_conversion("Inspect Hex contains non-ASCII Unicode");
        }
        source.push_back(static_cast<char>(value));
      }
      const std::size_t conversion_budget =
          description_->max_frame_bytes == (std::numeric_limits<std::size_t>::max)()
              ? description_->max_frame_bytes
              : description_->max_frame_bytes + 1U;
      const auto parsed = ParseInspectHex(source, conversion_budget);
      if (!parsed.ok()) {
        return reject_conversion(InspectHexErrorDetail(parsed.error));
      }
      bytes = parsed.bytes;
    }
    if (representation == ByteRepresentation::ASCII_ESCAPED) {
      std::string error;
      const auto formatted = FormatAsciiEscaped(bytes, error);
      if (!formatted.has_value()) {
        return reject_conversion(std::move(error));
      }
      inspect_draft_utf16_ = *formatted;
    } else {
      const auto formatted = FormatContinuousUpperHex(bytes);
      inspect_draft_utf16_.assign(formatted.begin(), formatted.end());
    }
  }

  representation_ = representation;
  invalid_drafts_ = std::move(converted_invalid);
  inspect_draft_.clear();
  inspect_draft_.reserve(inspect_draft_utf16_.size());
  for (const char16_t value : inspect_draft_utf16_) {
    inspect_draft_.push_back(value <= 0xFFU ? static_cast<char>(value) : '?');
  }
  ++input_revision_;
  const bool stream_draft_was_submitted =
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
      submitted_stream_input_revision_.has_value() &&
      *submitted_stream_input_revision_ == inspect_input_revision_;
#else
      false;
#endif
  ++inspect_input_revision_;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (stream_draft_was_submitted) submitted_stream_input_revision_ = inspect_input_revision_;
#endif
  ClearPreview();
  ClearInspectOutcome();
  ClearEncodeFailure();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  RefreshDocumentState();
  return true;
}

bool DocumentSession::EncodeAvailable() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (IsBinaryHostDocument())
    return BinaryHostActive() && prepared_->binary_host_adapter->IsCompleteEncode(
                                     binary_host_binding_, binary_host_flow_) &&
           selection_.has_value() && description_ &&
           selection_->message_index < description_->messages.size() &&
           description_->messages[selection_->message_index].encode_available;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (HostActive() &&
      IsAsciiDecodeAction(prepared_->host_adapter->Bindings()[host_binding_].action))
    return false;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  if (prepared_ && prepared_->public_legacy_adapter && selection_)
    return prepared_->public_legacy_adapter->EncodeAvailable(selection_->pipeline_index,
                                                              selection_->message_index);
#endif
  const MessageDescriptor* message = nullptr;
  return CurrentMessage(message) && message->encode_available;
}

bool DocumentSession::InspectAvailable() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (IsBinaryHostDocument())
    return BinaryHostActive() && prepared_->binary_host_adapter->IsCompleteDecode(
                                     binary_host_binding_, binary_host_flow_);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (HostActive() &&
      !IsAsciiDecodeAction(prepared_->host_adapter->Bindings()[host_binding_].action))
    return false;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  if (prepared_ && prepared_->public_legacy_adapter && selected_pipeline_index_)
    return prepared_->public_legacy_adapter->DecodeAvailable(*selected_pipeline_index_);
#endif
  return description_.has_value() && selected_pipeline_index_.has_value() &&
         *selected_pipeline_index_ < description_->pipelines.size() &&
         !description_->pipelines[*selected_pipeline_index_].decode_message_indices.empty()
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
         && !description_->pipelines[*selected_pipeline_index_].stream_ascii_crlf
#endif
      ;
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (IsBinaryHostDocument() && BinaryHostActive() &&
      prepared_->binary_host_adapter->IsCompleteEncode(binary_host_binding_, binary_host_flow_) &&
      !prepared_->binary_host_adapter->SelectEncodeMessage(
          binary_host_binding_, binary_host_flow_, message_index)) return false;
  if (IsBinaryHostDocument() && BinaryHostActive()) {
    const auto cached =
        binary_encode_local_states_.find({binary_host_binding_, binary_host_flow_});
    if (cached != binary_encode_local_states_.end()) {
      binary_encode_local_state_bytes_ =
          cached->second.accounted_bytes <= binary_encode_local_state_bytes_
              ? binary_encode_local_state_bytes_ - cached->second.accounted_bytes
              : 0U;
      binary_encode_local_states_.erase(cached);
    }
  }
#endif
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (IsBinaryHostDocument() && BinaryHostActive()) {
    prepared_->binary_host_adapter->ClearCurrentEncode(binary_host_binding_, binary_host_flow_);
    binary_active_view_bytes_ = 0U;
  }
#endif
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (IsBinaryHostDocument() && BinaryHostActive()) {
    prepared_->binary_host_adapter->ClearCurrentEncode(binary_host_binding_, binary_host_flow_);
    binary_active_view_bytes_ = 0U;
  }
#endif
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
  return SetInvalidDraftUtf16(field_index, std::u16string{text.begin(), text.end()},
                              std::move(validation_error));
}

bool DocumentSession::SetInvalidDraftUtf16(std::size_t field_index, std::u16string text,
                                           std::string validation_error) {
  const MessageDescriptor* message = nullptr;
  if (!CurrentMessage(message) || field_index >= message->fields.size() ||
      message->fields[field_index].encode_source != FieldEncodeSource::INPUT ||
      validation_error.empty()) {
    return false;
  }
  ++input_revision_;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (IsBinaryHostDocument() && BinaryHostActive()) {
    prepared_->binary_host_adapter->ClearCurrentEncode(binary_host_binding_, binary_host_flow_);
    binary_active_view_bytes_ = 0U;
  }
#endif
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
  if (field.encode_source != FieldEncodeSource::INPUT || !DraftMatches(field, value)) {
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

bool DocumentSession::Encode(LabExecutionObserver* observer,
                             InputMaterializationTimer* input_materialization_timer,
                             std::int64_t* input_materialization_ns) {
  const MessageDescriptor* message = nullptr;
  if (!CurrentMessage(message) || prepared_ == nullptr) {
    SetEncodeFailure("UI_ENCODE_NOT_READY", "no complete Plan selection is ready");
    ClearPreview();
    return false;
  }
  if (input_materialization_timer != nullptr) {
    input_materialization_timer->Start();
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (IsBinaryHostDocument()) {
    if (!BinaryHostActive() || !prepared_->binary_host_adapter->IsCompleteEncode(
                                   binary_host_binding_, binary_host_flow_)) {
      SetEncodeFailure("UI_ENCODE_NOT_READY", "Binary public Host Encode is not ready");
      ClearPreview();
      return false;
    }
    std::vector<protocol_lab_binary::public_decode::EncodeInput> inputs;
    inputs.reserve(message->fields.size());
    for (const auto& field : message->fields) {
      if (field.encode_source != FieldEncodeSource::INPUT) continue;
      const auto draft = drafts_.find(field.field_index);
      if (draft == drafts_.end() || !DraftMatches(field, draft->second)) {
        const auto invalid = invalid_drafts_.find(field.field_index);
        SetEncodeFailure(invalid == invalid_drafts_.end() ? "UI_INPUT_INCOMPLETE"
                                                          : "UI_INPUT_INVALID",
                         "field=" + field.id + "; reason=" +
                             (invalid == invalid_drafts_.end()
                                  ? std::string{"no valid typed draft"}
                                  : invalid->second.validation_error));
        ClearPreview();
        RefreshDocumentState();
        return false;
      }
      const auto& value = draft->second;
      if (const auto* uint_value = std::get_if<std::uint64_t>(&value))
        inputs.push_back(protocol_lab_binary::public_decode::EncodeInput::UInt64(
            field.field_index, *uint_value));
      else if (const auto* int_value = std::get_if<std::int64_t>(&value))
        inputs.push_back(protocol_lab_binary::public_decode::EncodeInput::Int64(
            field.field_index, *int_value));
      else if (const auto* bool_value = std::get_if<bool>(&value))
        inputs.push_back(protocol_lab_binary::public_decode::EncodeInput::Bool(
            field.field_index, *bool_value));
      else if (const auto* bytes_value = std::get_if<std::vector<std::uint8_t>>(&value))
        inputs.push_back(protocol_lab_binary::public_decode::EncodeInput::Bytes(
            field.field_index, *bytes_value));
      else if (const auto* enum_value = std::get_if<EnumSelection>(&value))
        inputs.push_back(protocol_lab_binary::public_decode::EncodeInput::Enum(
            field.field_index, enum_value->entry_index));
      else if (const auto* decimal_value = std::get_if<Decimal64>(&value))
        inputs.push_back(protocol_lab_binary::public_decode::EncodeInput::Decimal(
            field.field_index, {decimal_value->coefficient, decimal_value->scale}));
      else {
        SetEncodeFailure("UI_INPUT_INVALID", "field=" + field.id + "; reason=typed draft mismatch");
        ClearPreview();
        return false;
      }
    }
    if (input_materialization_timer != nullptr && input_materialization_ns != nullptr)
      *input_materialization_ns = input_materialization_timer->ElapsedNanoseconds();
    const PreviewKey requested_key = MakePreviewKey();
    if (observer) observer->PhaseStarted(LabExecutionPhase::MAIN_CODEC);
    auto view = prepared_->binary_host_adapter->EncodeComplete(
        binary_host_binding_, binary_host_flow_, selection_->message_index, inputs,
        binary_active_view_bytes_);
    if (observer)
      observer->PhaseFinished(LabExecutionPhase::MAIN_CODEC,
                              PublicBinaryCodecStatusName(view.public_host.codec_status));
    if (!PreviewKeyStillCurrent(requested_key) || !view.ok || !view.result) {
      const std::string status = PublicBinaryCodecStatusName(view.public_host.codec_status);
      SetEncodeFailure("UI_BINARY_HOST_ENCODE_" + status,
                       "Binary Host complete-record Encode failed: " + status);
      ClearPreview();
      RefreshDocumentState();
      return false;
    }
    PreviewResult published;
    published.key = requested_key;
    published.encoded_frame = std::move(view.result->frame);
    published.fields = std::move(view.result->fields);
    published.zero_field_success = published.fields.empty();
    preview_ = std::move(published);
    binary_active_view_bytes_ = view.result->accounted_bytes;
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    ClearEncodeFailure();
    RefreshDocumentState();
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER) || \
    defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (IsAsciiDocument()) {
    if (!AsciiBackendReady()) {
      SetEncodeFailure("UI_ENCODE_NOT_READY", "ASCII adapter is not ready");
      ClearPreview();
      return false;
    }
    std::vector<AsciiInputField> inputs;
    if (message->encode_available) {
      for (const auto& field : message->fields) {
        if (!field.encode_referenced) continue;
        const auto draft = drafts_.find(field.field_index);
        if (draft == drafts_.end() || !DraftMatches(field, draft->second)) {
          const auto invalid = invalid_drafts_.find(field.field_index);
          SetEncodeFailure(
              invalid == invalid_drafts_.end() ? "UI_INPUT_INCOMPLETE" : "UI_INPUT_INVALID",
              "field=" + field.id + "; reason=" +
                  (invalid == invalid_drafts_.end() ? std::string{"no valid byte draft"}
                                                    : invalid->second.validation_error));
          ClearPreview();
          RefreshDocumentState();
          return false;
        }
        AsciiInputField input;
        input.field_index = field.field_index;
        input.field_id = field.id;
        input.bytes = std::get<std::vector<std::uint8_t>>(draft->second);
        inputs.push_back(std::move(input));
      }
    }
    if (input_materialization_timer != nullptr && input_materialization_ns != nullptr) {
      *input_materialization_ns = input_materialization_timer->ElapsedNanoseconds();
    }
    AsciiExecutionIdentity identity;
    identity.document_id = document_id_;
    identity.load_revision = load_revision_;
    identity.plan_generation = plan_generation_;
    identity.selection_revision = selection_revision_;
    identity.input_revision = input_revision_;
    identity.pipeline_index = selection_->pipeline_index;
    identity.pipeline_id = selection_->pipeline_id;
    identity.message_index = selection_->message_index;
    identity.message_id = selection_->message_id;
    const PreviewKey requested_key = MakePreviewKey();
    AsciiExecutionResult outcome;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    if (prepared_->public_ascii_adapter) {
      std::vector<protocol_lab_ascii::public_offline::InputField> public_inputs;
      public_inputs.reserve(inputs.size());
      for (const auto& input : inputs) public_inputs.push_back({input.field_index, input.bytes});
      outcome = ConvertPublicAsciiResult(
          prepared_->public_ascii_adapter->Encode(identity.pipeline_index, *identity.message_index,
                                                  public_inputs),
          identity, AsciiOperation::ENCODE);
    } else
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
        if (prepared_->public_ascii_stream_adapter) {
      const auto binding = prepared_->public_ascii_stream_adapter->FindBinding(
          identity.pipeline_index, AsciiHostAction::ENCODE);
      outcome = binding ? prepared_->public_ascii_stream_adapter->Encode(*binding, identity, inputs)
                        : AsciiExecutionResult{};
    } else
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
        if (HostActive()) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
      outcome = prepared_->host_adapter->Encode(host_binding_, identity, inputs);
#else
      outcome = ConvertPrivateAsciiResult(prepared_->host_adapter->Encode(
          host_binding_, ToPrivateAsciiIdentity(identity), ToPrivateAsciiInputs(inputs)));
#endif
    } else
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
      outcome = AsciiExecutionResult{};
#else
      outcome = ConvertPrivateAsciiResult(
          prepared_->ascii_adapter->Encode(ToPrivateAsciiIdentity(std::move(identity)),
                                           ToPrivateAsciiInputs(inputs)));
#endif
    if (!PreviewKeyStillCurrent(requested_key) ||
        outcome.status != AsciiAdapterStatus::OK) {
      const std::string status =
          outcome.codec_called
              ? std::string{AsciiCodecStatusName(outcome.codec_status)}
              : std::string{"INVALID_REQUEST"};
      SetEncodeFailure(status, outcome.detail.empty() ? "ASCII Encode failed: " + status
                                                      : std::move(outcome.detail));
      ClearPreview();
      RefreshDocumentState();
      return false;
    }
    PreviewResult published;
    published.key = requested_key;
    published.encoded_frame = std::move(outcome.frame);
    published.fields = CopyAsciiFields(outcome.fields);
    published.zero_field_success = published.fields.empty();
    published.tx_template_review =
        outcome.review_kind == AsciiReviewKind::TX_TEMPLATE;
    preview_ = std::move(published);
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    ClearEncodeFailure();
    RefreshDocumentState();
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  if (prepared_->public_legacy_adapter) {
    if (observer) observer->PhaseStarted(LabExecutionPhase::PREPARATION);
    std::vector<PublicLegacyInput> inputs;
    inputs.reserve(message->fields.size());
    for (const auto& field : message->fields) {
      if (field.encode_source != FieldEncodeSource::INPUT) continue;
      const auto draft = drafts_.find(field.field_index);
      if (draft == drafts_.end() || !DraftMatches(field, draft->second)) {
        if (observer)
          observer->PhaseFinished(LabExecutionPhase::PREPARATION,
                                  "INVALID_INPUT");
        const auto invalid = invalid_drafts_.find(field.field_index);
        SetEncodeFailure(invalid == invalid_drafts_.end() ? "UI_INPUT_INCOMPLETE"
                                                          : "UI_INPUT_INVALID",
                         "field=" + field.id + "; reason=" +
                             (invalid == invalid_drafts_.end()
                                  ? std::string{"no valid typed draft"}
                                  : invalid->second.validation_error));
        ClearPreview();
        RefreshDocumentState();
        return false;
      }
      inputs.push_back({field.field_index, draft->second});
    }
    if (input_materialization_timer != nullptr && input_materialization_ns != nullptr)
      *input_materialization_ns = input_materialization_timer->ElapsedNanoseconds();
    if (observer)
      observer->PhaseFinished(LabExecutionPhase::PREPARATION, "OK");
    const PreviewKey requested_key = MakePreviewKey();
    if (observer) observer->PhaseStarted(LabExecutionPhase::MAIN_CODEC);
    auto outcome = prepared_->public_legacy_adapter->Encode(
        selection_->pipeline_index, selection_->message_index, inputs);
    const std::string status = PublicLegacyCodecStatusName(outcome.codec_status);
    if (observer)
      observer->PhaseFinished(LabExecutionPhase::MAIN_CODEC, status);
    if (outcome.review_decode_called && observer) {
      observer->PhaseStarted(LabExecutionPhase::REVIEW_DECODE);
      observer->PhaseFinished(LabExecutionPhase::REVIEW_DECODE,
                              PublicLegacyCodecStatusName(outcome.review_status));
    }
    if (!PreviewKeyStillCurrent(requested_key) || !outcome.ok()) {
      std::string detail = "public complete-record Encode failed: " + status;
      if (outcome.conversion_error != pae::ConversionError::NONE)
        detail += "; conversion_error=" +
                  std::string{PublicLegacyConversionErrorName(outcome.conversion_error)};
      if (!outcome.failed_field_id.empty()) detail += "; field=" + outcome.failed_field_id;
      SetEncodeFailure("UI_ENCODE_" + status, std::move(detail));
      ClearPreview();
      RefreshDocumentState();
      return false;
    }
    if (observer) observer->PhaseStarted(LabExecutionPhase::RESULT_MAPPING);
    PreviewResult published;
    published.key = requested_key;
    published.encoded_frame = std::move(outcome.frame);
    published.fields = std::move(outcome.fields);
    published.zero_field_success = published.fields.empty();
    preview_ = std::move(published);
    if (observer)
      observer->PhaseFinished(LabExecutionPhase::RESULT_MAPPING, "OK");
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    ClearEncodeFailure();
    RefreshDocumentState();
    return true;
  }
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (prepared_->bridge == nullptr) {
    SetEncodeFailure("UI_ENCODE_NOT_READY", "Binary execution bridge is not ready");
    ClearPreview();
    return false;
  }
  protocol_lab::v06::ParsedValues values;
  values.format_version = description_->schema_version == "0.8"
                              ? std::string{protocol_lab::v06::kVariableValuesFormat}
                              : std::string{protocol_lab::v06::kValuesFormat};
  values.pipeline_id = selection_->pipeline_id;
  values.message_id = selection_->message_id;
  for (const auto& field : message->fields) {
    if (field.encode_source != FieldEncodeSource::INPUT) continue;
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
          } else if constexpr (std::is_same_v<T, Decimal64>) {
            const auto normalized = NormalizeDecimal64(item);
            parsed.decimal64_value =
                protocol_lab::v06::Decimal64{normalized.coefficient, normalized.scale};
          }
        },
        draft->second);
    values.fields.push_back(std::move(parsed));
  }
  if (input_materialization_timer != nullptr && input_materialization_ns != nullptr) {
    *input_materialization_ns = input_materialization_timer->ElapsedNanoseconds();
  }

  const PreviewKey requested_key = MakePreviewKey();
  LegacyExecutionObserverAdapter legacy_observer(observer);
  auto outcome = prepared_->bridge->EncodeParsed(values
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                                 ,
                                                 nullptr
#endif
                                                 ,
                                                 observer ? &legacy_observer : nullptr);
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
  published.fields = CopyLegacyFields(*message, outcome.result->fields);
  preview_ = std::move(published);
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  ClearEncodeFailure();
  RefreshDocumentState();
  return true;
#else
  SetEncodeFailure("UI_ENCODE_NOT_READY", "no public complete-record adapter is ready");
  ClearPreview();
  return false;
#endif
}

bool DocumentSession::SetInspectDraft(std::string text) {
  try {
    std::u16string converted;
    converted.reserve(text.size());
    for (const unsigned char value : text) converted.push_back(static_cast<char16_t>(value));
    return SetInspectDraftUtf16(std::move(converted));
  } catch (const std::exception&) {
    return false;
  }
}

bool DocumentSession::SetInspectDraftUtf16(std::u16string text) {
  if (state_ == DocumentState::CLOSING || state_ == DocumentState::CLOSED || prepared_ == nullptr ||
      !selected_pipeline_index_.has_value()) {
    return false;
  }
  if (inspect_draft_utf16_ == text) return true;
  std::string narrowed;
  try {
    narrowed.reserve(text.size());
    for (const char16_t value : text)
      narrowed.push_back(value <= 0xFFU ? static_cast<char>(value) : '?');
  } catch (const std::exception&) {
    return false;
  }
  inspect_draft_utf16_ = std::move(text);
  inspect_draft_ = std::move(narrowed);
  ++inspect_input_revision_;
  ClearInspectOutcome();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  RefreshDocumentState();
  return true;
}

void DocumentSession::RejectInspectCapacity(std::size_t capacity) {
  const bool stream_draft_was_submitted =
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
      submitted_stream_input_revision_.has_value() &&
      *submitted_stream_input_revision_ == inspect_input_revision_;
#else
      false;
#endif
  ++inspect_input_revision_;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (stream_draft_was_submitted) submitted_stream_input_revision_ = inspect_input_revision_;
#endif
  ClearInspectOutcome();
  SetDiagnostic("UI_ASCII_INPUT_CAPACITY_EXCEEDED",
                "Inspect edit exceeds bounded capacity=" + std::to_string(capacity) +
                    "; entire edit rejected");
  RefreshDocumentState();
}

bool DocumentSession::Inspect(LabExecutionObserver* observer) {
  ++inspect_request_revision_;
  ClearInspectOutcome();
  if (prepared_ == nullptr || !description_.has_value() || !selected_pipeline_index_.has_value() ||
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  if (IsBinaryHostDocument()) {
    if (!BinaryHostActive() || !prepared_->binary_host_adapter->IsCompleteDecode(
                                   binary_host_binding_, binary_host_flow_)) {
      inspect_failure_ = InspectFailure{
          InspectFailureStage::INPUT, "UI_INSPECT_OPERATION_NOT_SUPPORTED",
          "Schema 0.9 requires an active complete-record Decode binding", "WRONG_INPUT_KIND"};
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    auto parsed = ParseInspectHex(inspect_draft_, budget);
    if (!parsed.ok()) {
      InspectFailure failure;
      failure.stage = InspectFailureStage::INPUT;
      failure.diagnostic_id = InspectHexDiagnosticId(parsed.error);
      failure.detail =
          InspectFailureDetail(InspectHexErrorDetail(parsed.error), parsed.input_offset);
      failure.input_offset = parsed.input_offset;
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    try {
      auto view = prepared_->binary_host_adapter->DecodeComplete(
          binary_host_binding_, binary_host_flow_, parsed.bytes, binary_active_view_bytes_);
      if (!InspectKeyStillCurrent(requested_key)) return false;
      if (!view.ok || !view.result) {
        InspectFailure failure;
        failure.stage = InspectFailureStage::CODEC;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
        failure.status = PublicBinaryCodecStatusName(view.public_host.codec_status);
#else
        failure.status = std::string{protocol_lab::ascii::CodecStatusName(view.host.codec_status)};
#endif
        failure.diagnostic_id = "UI_BINARY_HOST_DECODE_FAILED";
        failure.detail = "Binary Host complete-record Decode failed";
        if (view.failure) {
          binary_active_view_bytes_ = view.failure->accounted_bytes;
          failure.input_frame = std::move(view.failure->diagnostic_frame);
          failure.message_index = view.failure->message_index;
          failure.failed_field_index = view.failure->failed_field_index;
        }
        inspect_failure_ = std::move(failure);
        SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
        RefreshDocumentState();
        return false;
      }
      InspectResult result;
      binary_active_view_bytes_ = view.result->accounted_bytes;
      result.key = requested_key;
      result.input_frame = std::move(view.result->frame);
      result.message_index = view.result->message_index;
      result.message_id = std::move(view.result->message_id);
      result.fields = std::move(view.result->fields);
      result.zero_field_success = result.fields.empty();
      inspect_result_ = std::move(result);
      diagnostic_id_.clear();
      diagnostic_detail_.clear();
      RefreshDocumentState();
      return true;
    } catch (const std::exception& exception) {
      inspect_failure_ = InspectFailure{InspectFailureStage::MATERIALIZATION,
                                        "UI_BINARY_VIEW_MATERIALIZATION_FAILED", exception.what(),
                                        "MATERIALIZATION_FAILED"};
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER) || \
    defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (IsAsciiDocument()) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    if (description_->pipelines[*selected_pipeline_index_].stream_ascii_crlf) {
      InspectFailure failure;
      failure.stage = InspectFailureStage::INPUT;
      failure.status = "OPERATION_NOT_SUPPORTED";
      failure.diagnostic_id = "UI_INSPECT_OPERATION_NOT_SUPPORTED";
      failure.detail = "stream_chunk + ascii_crlf requires Stream Inspect Submit/Continue";
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
#endif
    if (!AsciiBackendReady()) {
      InspectFailure failure;
      failure.diagnostic_id = "UI_INSPECT_NOT_READY";
      failure.detail = "ASCII adapter is not ready";
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      return false;
    }
    std::vector<std::uint8_t> input;
    if (representation_ == ByteRepresentation::ASCII_ESCAPED) {
      const auto parsed = ParseAsciiEscaped(inspect_draft_utf16_);
      if (!parsed.ok() || parsed.bytes.empty()) {
        InspectFailure failure;
        failure.stage = InspectFailureStage::INPUT;
        failure.diagnostic_id = "UI_ASCII_INPUT_INVALID";
        if (parsed.ok()) {
          failure.detail = "Inspect ASCII escaped input is empty";
        } else {
          failure.input_offset = parsed.utf16_offset;
          failure.input_offset_is_utf16 = true;
          failure.detail = AsciiInputFailureDetail(parsed.detail, *parsed.utf16_offset);
        }
        inspect_failure_ = std::move(failure);
        SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
        RefreshDocumentState();
        return false;
      }
      input = parsed.bytes;
    } else {
      const auto parsed = ParseInspectHex(inspect_draft_, budget);
      if (!parsed.ok()) {
        InspectFailure failure;
        failure.stage = InspectFailureStage::INPUT;
        failure.diagnostic_id = InspectHexDiagnosticId(parsed.error);
        failure.detail =
            InspectFailureDetail(InspectHexErrorDetail(parsed.error), parsed.input_offset);
        failure.input_offset = parsed.input_offset;
        inspect_failure_ = std::move(failure);
        SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
        RefreshDocumentState();
        return false;
      }
      input = parsed.bytes;
    }
    if (budget == 0U || input.size() > budget) {
      InspectFailure failure;
      failure.stage = InspectFailureStage::INPUT;
      failure.diagnostic_id = "UI_INSPECT_FRAME_LIMIT_EXCEEDED";
      failure.detail = "decoded Inspect frame exceeds the selected Pipeline budget";
      failure.input_frame = std::move(input);
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
    AsciiExecutionIdentity identity;
    identity.document_id = document_id_;
    identity.load_revision = load_revision_;
    identity.plan_generation = plan_generation_;
    identity.selection_revision = pipeline_selection_revision_;
    identity.input_revision = inspect_input_revision_;
    identity.pipeline_index = pipeline.pipeline_index;
    identity.pipeline_id = pipeline.id;
    AsciiExecutionResult outcome;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    if (prepared_->public_ascii_adapter) {
      outcome = ConvertPublicAsciiResult(prepared_->public_ascii_adapter->Decode(
                                             pipeline.pipeline_index, {input.data(), input.size()}),
                                         identity, AsciiOperation::INSPECT);
    } else
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
        if (prepared_->public_ascii_stream_adapter) {
      const auto binding = prepared_->public_ascii_stream_adapter->FindBinding(
          pipeline.pipeline_index, AsciiHostAction::DECODE);
      outcome = binding ? prepared_->public_ascii_stream_adapter->Inspect(*binding, 0U, identity,
                                                                          input)
                        : AsciiExecutionResult{};
    } else
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
        if (HostActive()) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
      outcome = prepared_->host_adapter->Inspect(host_binding_, host_stream_, identity, input);
#else
      outcome = ConvertPrivateAsciiResult(prepared_->host_adapter->Inspect(
          host_binding_, host_stream_, ToPrivateAsciiIdentity(identity), input));
#endif
    } else
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
      outcome = AsciiExecutionResult{};
#else
      outcome = ConvertPrivateAsciiResult(
          prepared_->ascii_adapter->Inspect(ToPrivateAsciiIdentity(std::move(identity)), input));
#endif
    if (!InspectKeyStillCurrent(requested_key)) return false;
    if (outcome.status != AsciiAdapterStatus::OK) {
      InspectFailure failure;
      const std::string status =
          outcome.codec_called
              ? std::string{AsciiCodecStatusName(outcome.codec_status)}
              : std::string{"INVALID_REQUEST"};
      failure.stage = outcome.status == AsciiAdapterStatus::MATERIALIZATION_FAILED
                          ? InspectFailureStage::MATERIALIZATION
                          : (status == "UNKNOWN_MESSAGE" || status == "AMBIGUOUS_MESSAGE"
                                 ? InspectFailureStage::STRUCTURAL_QUERY
                                 : InspectFailureStage::CODEC);
      failure.status = status;
      failure.diagnostic_id = "UI_INSPECT_" + status;
      failure.detail =
          outcome.detail.empty() ? "ASCII Inspect failed: " + status : std::move(outcome.detail);
      failure.input_frame = std::move(outcome.diagnostic_input_frame);
      failure.message_index = outcome.message_index;
      failure.message_id = outcome.message_id;
      failure.failed_field_index = outcome.failed_field_index;
      failure.failed_field_id = outcome.failed_field_id;
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    if (!outcome.message_index.has_value() || !outcome.message_id.has_value()) {
      InspectFailure failure;
      failure.stage = InspectFailureStage::MATERIALIZATION;
      failure.diagnostic_id = "UI_INSPECT_MESSAGE_IDENTITY_MISMATCH";
      failure.detail = "ASCII adapter success has no matched Message identity";
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      return false;
    }
    InspectResult result;
    result.key = requested_key;
    result.input_frame = std::move(outcome.frame);
    result.message_index = *outcome.message_index;
    result.message_id = *outcome.message_id;
    result.fields = CopyAsciiFields(outcome.fields);
    result.zero_field_success = result.fields.empty();
    inspect_result_ = std::move(result);
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    RefreshDocumentState();
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  if (prepared_->public_legacy_adapter) {
    const auto parsed = ParseInspectHex(inspect_draft_, budget);
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
    if (observer) observer->PhaseStarted(LabExecutionPhase::STRUCTURAL_QUERY);
    auto outcome = prepared_->public_legacy_adapter->Decode(*selected_pipeline_index_, parsed.bytes);
    const std::string status = PublicLegacyCodecStatusName(outcome.codec_status);
    if (observer)
      observer->PhaseFinished(LabExecutionPhase::STRUCTURAL_QUERY, status);
    if (!InspectKeyStillCurrent(requested_key)) return false;
    if (!outcome.ok()) {
      InspectFailure failure;
      failure.stage = outcome.local_status == PublicLegacyLocalStatus::MATERIALIZATION_FAILED
                          ? InspectFailureStage::MATERIALIZATION
                          : (status == "UNKNOWN_MESSAGE" || status == "AMBIGUOUS_MESSAGE"
                                 ? InspectFailureStage::STRUCTURAL_QUERY
                                 : InspectFailureStage::CODEC);
      failure.status = status;
      failure.diagnostic_id = "UI_INSPECT_" + status;
      failure.detail = "public complete-record Inspect failed: " + status;
      failure.input_frame = std::move(outcome.frame);
      failure.message_index = outcome.message_index;
      if (!outcome.message_id.empty()) failure.message_id = outcome.message_id;
      failure.failed_field_index = outcome.failed_field_index;
      if (!outcome.failed_field_id.empty()) failure.failed_field_id = outcome.failed_field_id;
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    if (!outcome.message_index.has_value() || outcome.message_id.empty()) {
      InspectFailure failure;
      failure.stage = InspectFailureStage::MATERIALIZATION;
      failure.diagnostic_id = "UI_INSPECT_MESSAGE_IDENTITY_MISMATCH";
      failure.detail = "public adapter success has no matched Message identity";
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      RefreshDocumentState();
      return false;
    }
    InspectResult result;
    result.key = requested_key;
    result.input_frame = std::move(outcome.frame);
    result.message_index = *outcome.message_index;
    result.message_id = std::move(outcome.message_id);
    result.fields = std::move(outcome.fields);
    result.zero_field_success = result.fields.empty();
    inspect_result_ = std::move(result);
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    RefreshDocumentState();
    return true;
  }
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  if (prepared_->bridge == nullptr) {
    InspectFailure failure;
    failure.diagnostic_id = "UI_INSPECT_NOT_READY";
    failure.detail = "Binary execution bridge is not ready";
    inspect_failure_ = std::move(failure);
    SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    return false;
  }
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
  LegacyExecutionObserverAdapter legacy_observer(observer);
  auto outcome = prepared_->bridge->InspectPipeline(parsed.bytes, pipeline.id
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                                    ,
                                                    nullptr
#endif
                                                    ,
                                                    observer ? &legacy_observer : nullptr);
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
  result.fields = CopyLegacyFields(description_->messages[message_index], outcome.result->fields);
  inspect_result_ = std::move(result);
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  RefreshDocumentState();
  return true;
#else
  InspectFailure failure;
  failure.diagnostic_id = "UI_INSPECT_NOT_READY";
  failure.detail = "no public complete-record adapter is ready";
  inspect_failure_ = std::move(failure);
  SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
  return false;
#endif
}

#if defined(PAE_BUILD_PROTOCOL_LAB_STREAM_UI)
bool DocumentSession::StreamInspectAvailable() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive())
    return prepared_->binary_host_adapter->IsStreamDecode(binary_host_binding_,
                                                           binary_host_flow_);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (HostActive())
    return prepared_->host_adapter->Observe(host_binding_, host_stream_).has_value();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  if (prepared_ && prepared_->public_ascii_stream_adapter && selected_pipeline_index_) {
    const auto binding = prepared_->public_ascii_stream_adapter->FindBinding(
        *selected_pipeline_index_, AsciiHostAction::DECODE);
    return binding && prepared_->public_ascii_stream_adapter->Observe(*binding, 0U).has_value();
  }
#endif
  return AsciiBackendReady() && description_.has_value() && selected_pipeline_index_.has_value() &&
         *selected_pipeline_index_ < description_->pipelines.size() &&
         description_->pipelines[*selected_pipeline_index_].stream_ascii_crlf;
#else
  return false;
#endif
}

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
std::optional<AsciiStreamObservation> DocumentSession::StreamObservation()
    const noexcept {
  if (!StreamInspectAvailable()) return std::nullopt;
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (HostActive()) {
    const auto observed = prepared_->host_adapter->Observe(host_binding_, host_stream_);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    return observed;
#else
    return observed
               ? std::optional<AsciiStreamObservation>{ConvertPrivateAsciiObservation(*observed)}
               : std::nullopt;
#endif
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  if (prepared_->public_ascii_stream_adapter) {
    const auto binding = prepared_->public_ascii_stream_adapter->FindBinding(
        *selected_pipeline_index_, AsciiHostAction::DECODE);
    return binding ? prepared_->public_ascii_stream_adapter->Observe(*binding, 0U) : std::nullopt;
  }
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  return std::nullopt;
#else
  const auto observed = prepared_->ascii_adapter->ObserveStream(*selected_pipeline_index_);
  return observed ? std::optional<AsciiStreamObservation>{ConvertPrivateAsciiObservation(*observed)}
                  : std::nullopt;
#endif
}
#endif

bool DocumentSession::StreamContinueAvailable() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive())
    return prepared_->binary_host_adapter->StreamContinueAvailable(binary_host_binding_,
                                                                    binary_host_flow_);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  const auto observation = StreamObservation();
  return observation.has_value() && !observation->reset_required &&
         (observation->frozen_cursor < observation->frozen_input_bytes ||
          observation->has_internal_work);
#else
  return false;
#endif
}

bool DocumentSession::StreamHasDiscardableState() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive()) {
    const auto& adapter = *prepared_->binary_host_adapter;
    for (std::size_t binding = 0U; binding < adapter.Bindings().size(); ++binding) {
      for (std::size_t flow = 0U; flow < adapter.FlowCount(binding); ++flow) {
        if (!adapter.IsStreamDecode(binding, flow)) continue;
        const auto observation = adapter.ObserveStream(binding, flow);
        if (!adapter.Draft(binding, flow).empty() ||
            (observation &&
             (observation->buffered_bytes != 0U || observation->has_internal_work ||
              observation->frozen_input_bytes != 0U || observation->step_sequence != 0U ||
              observation->reset_required)))
          return true;
      }
    }
    return false;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (HostActive()) return prepared_->host_adapter->HasDiscardableState();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  if (prepared_ && prepared_->public_ascii_stream_adapter)
    return prepared_->public_ascii_stream_adapter->HasDiscardableState();
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  return false;
#else
  return StreamInspectAvailable() &&
         prepared_->ascii_adapter->StreamHasDiscardableState(*selected_pipeline_index_);
#endif
#else
  return false;
#endif
}

std::size_t DocumentSession::StreamChunkBudget() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive()) {
    const auto state = BinaryStreamObservation();
    return state ? state->effective_max_submit_bytes : 0U;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  if (HostActive()) {
    const auto state = StreamObservation();
    return state ? (std::min)(std::size_t{65536}, state->effective_max_submit_bytes) : 0U;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  if (prepared_ && prepared_->public_ascii_stream_adapter) {
    const auto state = StreamObservation();
    return state ? (std::min)(std::size_t{65536}, state->effective_max_submit_bytes) : 0U;
  }
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  return 0U;
#else
  return StreamInspectAvailable()
             ? prepared_->ascii_adapter->StreamChunkCapacity(*selected_pipeline_index_)
             : 0U;
#endif
#else
  return 0U;
#endif
}

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
namespace {
InspectFailure StreamFailureFromAdapter(const AsciiExecutionResult& outcome) {
  InspectFailure failure;
  const std::string status =
      outcome.codec_called ? std::string{AsciiCodecStatusName(outcome.codec_status)}
                           : std::string{"INVALID_REQUEST"};
  failure.stage = outcome.status == AsciiAdapterStatus::MATERIALIZATION_FAILED
                      ? InspectFailureStage::MATERIALIZATION
                      : (status == "UNKNOWN_MESSAGE" || status == "AMBIGUOUS_MESSAGE"
                             ? InspectFailureStage::STRUCTURAL_QUERY
                             : InspectFailureStage::CODEC);
  failure.status = status;
  failure.diagnostic_id = "UI_STREAM_INSPECT_" + status;
  failure.detail =
      outcome.detail.empty() ? "ASCII stream candidate Decode failed: " + status : outcome.detail;
  failure.input_frame = outcome.diagnostic_input_frame;
  failure.message_index = outcome.message_index;
  failure.message_id = outcome.message_id;
  failure.failed_field_index = outcome.failed_field_index;
  failure.failed_field_id = outcome.failed_field_id;
  return failure;
}
}  // namespace
#endif

bool DocumentSession::SubmitStream() {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive()) {
    if (!StreamInspectAvailable()) {
      SetDiagnostic("UI_BINARY_STREAM_NOT_AVAILABLE",
                    "selected Binary binding is not a stream Decode input");
      return false;
    }
    const auto parsed = ParseInspectHex(inspect_draft_, StreamChunkBudget());
    if (!parsed.ok() || parsed.bytes.empty()) {
      SetDiagnostic(parsed.ok() ? "UI_BINARY_STREAM_CHUNK_EMPTY"
                                : InspectHexDiagnosticId(parsed.error),
                    parsed.ok() ? "Binary stream chunk is empty"
                                : InspectFailureDetail(InspectHexErrorDetail(parsed.error),
                                                       parsed.input_offset));
      return false;
    }
    ++inspect_request_revision_;
    auto view = prepared_->binary_host_adapter->SubmitStream(
        binary_host_binding_, binary_host_flow_, parsed.bytes, binary_active_view_bytes_);
    if (!view.host_called && view.status == BinaryStreamPresentationStatus::PREFLIGHT_REJECTED) {
      binary_stream_view_ = std::move(view);
      SetDiagnostic("UI_BINARY_STREAM_PREFLIGHT_REJECTED",
                    "Binary stream input was rejected before Host execution");
      return false;
    }
    auto projected = ProjectBinaryStreamView(std::move(view), MakeInspectResultKey());
    ClearInspectOutcome();
    inspect_result_ = std::move(projected.result);
    inspect_failure_ = std::move(projected.failure);
    binary_active_view_bytes_ = projected.accounted_bytes;
    binary_stream_view_ = std::move(projected.view);
    diagnostic_id_ = std::move(projected.diagnostic_id);
    diagnostic_detail_ = std::move(projected.diagnostic_detail);
    RefreshDocumentState();
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (!StreamInspectAvailable()) {
    SetDiagnostic("UI_STREAM_INSPECT_NOT_AVAILABLE", "selected Pipeline is not ASCII CRLF stream");
    return false;
  }
  if (submitted_stream_input_revision_.has_value() &&
      *submitted_stream_input_revision_ == inspect_input_revision_) {
    SetDiagnostic("UI_STREAM_CHUNK_ALREADY_SUBMITTED",
                  "edit the stream chunk before submitting new data");
    return false;
  }
  std::vector<std::uint8_t> input;
  if (representation_ == ByteRepresentation::ASCII_ESCAPED) {
    const auto parsed = ParseAsciiEscaped(inspect_draft_utf16_);
    if (!parsed.ok() || parsed.bytes.empty()) {
      InspectFailure failure;
      failure.stage = InspectFailureStage::INPUT;
      failure.diagnostic_id = "UI_ASCII_INPUT_INVALID";
      if (parsed.ok()) {
        failure.detail = "Stream chunk is empty";
      } else {
        failure.input_offset = parsed.utf16_offset;
        failure.input_offset_is_utf16 = true;
        failure.detail = AsciiInputFailureDetail(parsed.detail, *parsed.utf16_offset);
      }
      inspect_failure_ = std::move(failure);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
      return false;
    }
    input = parsed.bytes;
  } else {
    const auto parsed = ParseInspectHex(inspect_draft_, StreamChunkBudget());
    if (!parsed.ok()) {
      SetDiagnostic(InspectHexDiagnosticId(parsed.error),
                    InspectFailureDetail(InspectHexErrorDetail(parsed.error), parsed.input_offset));
      return false;
    }
    input = parsed.bytes;
  }
  if (input.empty() || input.size() > StreamChunkBudget()) {
    SetDiagnostic("UI_STREAM_CHUNK_CAPACITY_EXCEEDED",
                  "stream chunk exceeds effective Submit capacity");
    return false;
  }
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  AsciiExecutionIdentity identity;
  identity.document_id = document_id_;
  identity.load_revision = load_revision_;
  identity.plan_generation = plan_generation_;
  identity.selection_revision = pipeline_selection_revision_;
  identity.input_revision = inspect_input_revision_;
  identity.pipeline_index = pipeline.pipeline_index;
  identity.pipeline_id = pipeline.id;
  ++inspect_request_revision_;
  stream_step_ = [&]() -> AsciiStreamStepResult {
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    if (HostActive()) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
      return prepared_->host_adapter->Submit(host_binding_, host_stream_, identity, input);
#else
      return ConvertPrivateAsciiStreamStep(prepared_->host_adapter->Submit(
          host_binding_, host_stream_, ToPrivateAsciiIdentity(identity), input));
#endif
    }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
    if (prepared_->public_ascii_stream_adapter)
      return prepared_->public_ascii_stream_adapter->Submit(
          *prepared_->public_ascii_stream_adapter->FindBinding(
              pipeline.pipeline_index, AsciiHostAction::DECODE),
          0U, identity, input);
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    return AsciiStreamStepResult{};
#else
    return ConvertPrivateAsciiStreamStep(prepared_->ascii_adapter->SubmitStreamChunk(
        ToPrivateAsciiIdentity(std::move(identity)), input));
#endif
  }();
  if (stream_step_->push_called) submitted_stream_input_revision_ = inspect_input_revision_;
  inspect_result_.reset();
  inspect_failure_.reset();
  if (stream_step_->status != AsciiAdapterStatus::OK) {
    SetDiagnostic("UI_STREAM_STEP_FAILED", stream_step_->detail);
    return false;
  }
  if (stream_step_->candidate.has_value()) {
    const auto& candidate = *stream_step_->candidate;
    if (candidate.status == AsciiAdapterStatus::OK &&
        candidate.message_index.has_value() && candidate.message_id.has_value()) {
      InspectResult mapped;
      mapped.key = MakeInspectResultKey();
      mapped.input_frame = candidate.frame;
      mapped.message_index = *candidate.message_index;
      mapped.message_id = *candidate.message_id;
      mapped.fields = CopyAsciiFields(candidate.fields);
      mapped.zero_field_success = mapped.fields.empty();
      inspect_result_ = std::move(mapped);
      diagnostic_id_.clear();
      diagnostic_detail_.clear();
    } else {
      inspect_failure_ = StreamFailureFromAdapter(candidate);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    }
  } else {
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
  }
  RefreshDocumentState();
  return true;
#else
  SetDiagnostic("UI_STREAM_INSPECT_NOT_AVAILABLE", "no stream adapter is ready");
  return false;
#endif
}

bool DocumentSession::ContinueStream() {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive()) {
    if (!StreamContinueAvailable()) {
      SetDiagnostic("UI_BINARY_STREAM_CONTINUE_NOT_AVAILABLE",
                    "no frozen suffix or internal Binary work remains");
      return false;
    }
    ++inspect_request_revision_;
    auto view = prepared_->binary_host_adapter->ContinueStream(
        binary_host_binding_, binary_host_flow_, binary_active_view_bytes_);
    if (!view.host_called && view.status == BinaryStreamPresentationStatus::PREFLIGHT_REJECTED) {
      binary_stream_view_ = std::move(view);
      SetDiagnostic("UI_BINARY_STREAM_CONTINUE_REJECTED",
                    "Binary stream Continue was rejected before Host execution");
      return false;
    }
    auto projected = ProjectBinaryStreamView(std::move(view), MakeInspectResultKey());
    ClearInspectOutcome();
    inspect_result_ = std::move(projected.result);
    inspect_failure_ = std::move(projected.failure);
    binary_active_view_bytes_ = projected.accounted_bytes;
    binary_stream_view_ = std::move(projected.view);
    diagnostic_id_ = std::move(projected.diagnostic_id);
    diagnostic_detail_ = std::move(projected.diagnostic_detail);
    RefreshDocumentState();
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (!StreamContinueAvailable()) {
    SetDiagnostic("UI_STREAM_CONTINUE_NOT_AVAILABLE", "no frozen suffix or internal work remains");
    return false;
  }
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  AsciiExecutionIdentity identity;
  identity.document_id = document_id_;
  identity.load_revision = load_revision_;
  identity.plan_generation = plan_generation_;
  identity.selection_revision = pipeline_selection_revision_;
  identity.input_revision = inspect_input_revision_;
  identity.pipeline_index = pipeline.pipeline_index;
  identity.pipeline_id = pipeline.id;
  ++inspect_request_revision_;
  stream_step_ = [&]() -> AsciiStreamStepResult {
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
    if (HostActive()) {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
      return prepared_->host_adapter->Continue(host_binding_, host_stream_, identity);
#else
      return ConvertPrivateAsciiStreamStep(prepared_->host_adapter->Continue(
          host_binding_, host_stream_, ToPrivateAsciiIdentity(identity)));
#endif
    }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
    if (prepared_->public_ascii_stream_adapter)
      return prepared_->public_ascii_stream_adapter->Continue(
          *prepared_->public_ascii_stream_adapter->FindBinding(
              pipeline.pipeline_index, AsciiHostAction::DECODE),
          0U, identity);
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    return AsciiStreamStepResult{};
#else
    return ConvertPrivateAsciiStreamStep(
        prepared_->ascii_adapter->ContinueStream(ToPrivateAsciiIdentity(std::move(identity))));
#endif
  }();
  inspect_result_.reset();
  inspect_failure_.reset();
  if (stream_step_->status != AsciiAdapterStatus::OK) {
    SetDiagnostic("UI_STREAM_STEP_FAILED", stream_step_->detail);
    return false;
  }
  if (stream_step_->candidate.has_value()) {
    const auto& candidate = *stream_step_->candidate;
    if (candidate.status == AsciiAdapterStatus::OK &&
        candidate.message_index.has_value() && candidate.message_id.has_value()) {
      InspectResult mapped;
      mapped.key = MakeInspectResultKey();
      mapped.input_frame = candidate.frame;
      mapped.message_index = *candidate.message_index;
      mapped.message_id = *candidate.message_id;
      mapped.fields = CopyAsciiFields(candidate.fields);
      mapped.zero_field_success = mapped.fields.empty();
      inspect_result_ = std::move(mapped);
      diagnostic_id_.clear();
      diagnostic_detail_.clear();
    } else {
      inspect_failure_ = StreamFailureFromAdapter(candidate);
      SetDiagnostic(inspect_failure_->diagnostic_id, inspect_failure_->detail);
    }
  }
  RefreshDocumentState();
  return true;
#else
  SetDiagnostic("UI_STREAM_CONTINUE_NOT_AVAILABLE", "no stream adapter is ready");
  return false;
#endif
}

bool DocumentSession::ResetStream() {
  if (!StreamInspectAvailable()) return false;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  if (BinaryHostActive()) {
    const auto status = prepared_->binary_host_adapter->ResetStream(binary_host_binding_,
                                                                    binary_host_flow_);
    if (status != pae::HostStatus::OK) {
      SetDiagnostic("UI_BINARY_STREAM_RESET_FAILED", "Binary stream Reset failed");
      return false;
    }
    inspect_draft_.clear();
    inspect_draft_utf16_.clear();
    ++inspect_input_revision_;
    ClearInspectOutcome();
    binary_stream_view_.reset();
    diagnostic_id_.clear();
    diagnostic_detail_.clear();
    RefreshDocumentState();
    return true;
  }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  std::string error;
  const bool reset =
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
      HostActive()
          ? prepared_->host_adapter->Reset(host_binding_, host_stream_)
          :
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
      prepared_->public_ascii_stream_adapter
          ? prepared_->public_ascii_stream_adapter->Reset(
                *prepared_->public_ascii_stream_adapter->FindBinding(
                    pipeline.pipeline_index, AsciiHostAction::DECODE),
                0U)
          :
#endif
#if defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
          false;
#else
          prepared_->ascii_adapter->ResetStream(pipeline.pipeline_index, pipeline.id, error);
#endif
  if (!reset) {
    SetDiagnostic("UI_STREAM_RESET_FAILED", std::move(error));
    return false;
  }
  inspect_draft_.clear();
  inspect_draft_utf16_.clear();
  ++inspect_input_revision_;
  submitted_stream_input_revision_.reset();
  ClearInspectOutcome();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  RefreshDocumentState();
  return true;
#else
  return false;
#endif
}
#endif

void DocumentSession::Close() {
  if (state_ == DocumentState::CLOSED) return;
  state_ = DocumentState::CLOSING;
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  host_views_.clear();
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  binary_encode_local_states_.clear();
  binary_encode_local_state_bytes_ = 0U;
#endif
  ClearSelectionAndPreview();
  inspect_draft_.clear();
  inspect_draft_utf16_.clear();
  ClearInspectOutcome();
  description_.reset();
  prepared_.reset();
  diagnostic_id_.clear();
  diagnostic_detail_.clear();
  compile_diagnostic_.reset();
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
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_UI)
  binary_active_view_bytes_ = 0U;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  stream_step_.reset();
#endif
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

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
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
#endif

std::size_t DocumentSession::InspectFrameBudget() const noexcept {
  if (!description_.has_value() || !selected_pipeline_index_.has_value() ||
      *selected_pipeline_index_ >= description_->pipelines.size()) {
    return 0U;
  }
  std::size_t pipeline_maximum = 0U;
  const auto& pipeline = description_->pipelines[*selected_pipeline_index_];
  const auto& candidates = description_->layout == DocumentLayout::ASCII_TEXT
                               ? pipeline.decode_message_indices
                               : pipeline.message_indices;
  if (description_->layout == DocumentLayout::ASCII_TEXT && candidates.empty()) {
    return (std::min)(description_->max_frame_bytes, std::size_t{65536U});
  }
  for (const std::size_t message_index : candidates) {
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
  compile_diagnostic_.reset();
  diagnostic_id_ = std::move(id);
  diagnostic_detail_ = std::move(detail);
}

void DocumentSession::SetCompileDiagnostic(
    std::string id, const std::optional<CompileDiagnosticView>& diagnostic,
    std::string fallback_detail) {
  SetDiagnostic(std::move(id), diagnostic ? diagnostic->detail : std::move(fallback_detail));
  compile_diagnostic_ = diagnostic;
}

void DocumentSession::ClearEncodeFailure() { encode_failure_.reset(); }

void DocumentSession::SetEncodeFailure(std::string id, std::string detail) {
  encode_failure_ = OperationDiagnostic{std::move(id), std::move(detail)};
  if (mode_ == OperationMode::ENCODE) {
    SetDiagnostic(encode_failure_->id, encode_failure_->detail);
  }
}

}  // namespace pae::protocol_lab_ui
