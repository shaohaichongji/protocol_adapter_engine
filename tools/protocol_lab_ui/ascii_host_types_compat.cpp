#include "ascii_host_types_compat.h"

#include <utility>

namespace pae::protocol_lab_ui {
namespace {

CodecStatus ToPublicStatus(protocol_core::CodecStatus value) noexcept {
  using P = protocol_core::CodecStatus;
  switch (value) {
    case P::OK: return CodecStatus::OK;
    case P::WORKSPACE_BUSY: return CodecStatus::WORKSPACE_BUSY;
    case P::UNKNOWN_MESSAGE: return CodecStatus::UNKNOWN_MESSAGE;
    case P::AMBIGUOUS_MESSAGE: return CodecStatus::AMBIGUOUS_MESSAGE;
    case P::OUTPUT_SLOTS_TOO_SMALL: return CodecStatus::OUTPUT_SLOTS_TOO_SMALL;
    case P::INTEGRITY_FAILED: return CodecStatus::INTEGRITY_FAILED;
    case P::MESSAGE_NOT_ALLOWED: return CodecStatus::MESSAGE_NOT_ALLOWED;
    case P::FIELD_REFERENCE_MISMATCH: return CodecStatus::FIELD_REFERENCE_MISMATCH;
    case P::DUPLICATE_FIELD: return CodecStatus::DUPLICATE_FIELD;
    case P::MISSING_FIELD: return CodecStatus::MISSING_FIELD;
    case P::TYPE_MISMATCH: return CodecStatus::TYPE_MISMATCH;
    case P::VALUE_NOT_REPRESENTABLE: return CodecStatus::VALUE_NOT_REPRESENTABLE;
    case P::BYTES_LENGTH_MISMATCH: return CodecStatus::BYTES_LENGTH_MISMATCH;
    case P::UNKNOWN_ENUM_VALUE: return CodecStatus::UNKNOWN_ENUM_VALUE;
    case P::ENUM_REFERENCE_MISMATCH: return CodecStatus::ENUM_REFERENCE_MISMATCH;
    case P::CONSTANT_FIELD_OVERRIDE: return CodecStatus::CONSTANT_FIELD_OVERRIDE;
    case P::INPUT_OUTPUT_OVERLAP: return CodecStatus::INPUT_OUTPUT_OVERLAP;
    case P::BUFFER_TOO_SMALL: return CodecStatus::BUFFER_TOO_SMALL;
    case P::FINAL_REVIEW_FAILED: return CodecStatus::FINAL_REVIEW_FAILED;
    case P::ASCII_CHARACTER_NOT_ALLOWED: return CodecStatus::ASCII_CHARACTER_NOT_ALLOWED;
    case P::ASCII_TERMINATOR_CONFLICT: return CodecStatus::ASCII_TERMINATOR_CONFLICT;
    case P::OPERATION_NOT_SUPPORTED: return CodecStatus::OPERATION_NOT_SUPPORTED;
    case P::COMPUTED_FIELD_OVERRIDE: return CodecStatus::COMPUTED_FIELD_OVERRIDE;
    case P::LENGTH_MISMATCH: return CodecStatus::LENGTH_MISMATCH;
    case P::INTERNAL_ERROR: return CodecStatus::INTERNAL_ERROR;
    default: return CodecStatus::INVALID_ARGUMENT;
  }
}

AsciiAdapterStatus ToOwnedStatus(protocol_lab::ascii::AdapterStatus value) noexcept {
  using P = protocol_lab::ascii::AdapterStatus;
  switch (value) {
    case P::OK: return AsciiAdapterStatus::OK;
    case P::CORE_FAILED: return AsciiAdapterStatus::CORE_FAILED;
    case P::MATERIALIZATION_FAILED: return AsciiAdapterStatus::MATERIALIZATION_FAILED;
    default: return AsciiAdapterStatus::INVALID_REQUEST;
  }
}

AsciiExecutionIdentity ConvertIdentity(protocol_lab::ascii::ExecutionIdentity source) {
  AsciiExecutionIdentity result;
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  result.binding_index = source.binding_index;
  result.stream_index = source.stream_index;
  result.stream_generation = source.stream_generation;
  result.operation_sequence = source.operation_sequence;
#endif
  result.document_id = source.document_id;
  result.load_revision = source.load_revision;
  result.plan_generation = source.plan_generation;
  result.selection_revision = source.selection_revision;
  result.input_revision = source.input_revision;
  result.pipeline_index = source.pipeline_index;
  result.pipeline_id = std::move(source.pipeline_id);
  result.message_index = source.message_index;
  result.message_id = std::move(source.message_id);
  return result;
}

StreamSubmitResult ConvertFraming(const protocol_framing::SubmitResult& source) noexcept {
  StreamSubmitResult result;
  switch (source.api_status) {
    case protocol_framing::SubmitApiStatus::OK: result.status = StreamFramerStatus::OK; break;
    case protocol_framing::SubmitApiStatus::INVALID_ARGUMENT:
      result.status = StreamFramerStatus::INVALID_ARGUMENT;
      break;
    case protocol_framing::SubmitApiStatus::INVALID_PLAN:
    case protocol_framing::SubmitApiStatus::WORKSPACE_PLAN_MISMATCH:
      result.status = StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
      break;
    case protocol_framing::SubmitApiStatus::WORKSPACE_BUSY:
      result.status = StreamFramerStatus::WORKSPACE_BUSY;
      break;
    case protocol_framing::SubmitApiStatus::REENTRANT_CALL:
      result.status = StreamFramerStatus::REENTRANT_CALL;
      break;
    case protocol_framing::SubmitApiStatus::LIMIT_EXCEEDED:
      result.status = StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED;
      break;
    default: result.status = StreamFramerStatus::INTERNAL_ERROR; break;
  }
  switch (source.stop_reason) {
    case protocol_framing::SubmitStopReason::NEED_MORE:
      result.stop_reason = StreamFramerStopReason::NEED_MORE;
      break;
    case protocol_framing::SubmitStopReason::WORK_BUDGET_REACHED:
      result.stop_reason = StreamFramerStopReason::WORK_BUDGET_REACHED;
      break;
    case protocol_framing::SubmitStopReason::SINK_STOP:
      result.stop_reason = StreamFramerStopReason::SINK_STOP;
      break;
    default: result.stop_reason = StreamFramerStopReason::INPUT_EXHAUSTED; break;
  }
  result.bytes_consumed = source.bytes_consumed;
  result.candidates_delivered = source.frames_delivered;
  result.bytes_discarded = source.bytes_discarded;
  result.malformed_candidates = source.malformed_candidates;
  result.last_issue = source.last_framing_issue == protocol_framing::FramingIssue::MALFORMED_LENGTH
                          ? StreamFramingIssue::MALFORMED_LENGTH
                          : (source.last_framing_issue == protocol_framing::FramingIssue::RECORD_TOO_LONG
                                 ? StreamFramingIssue::RECORD_TOO_LONG
                                 : StreamFramingIssue::NONE);
  result.work_units_used = source.work_units_used;
  return result;
}

}  // namespace

protocol_lab::ascii::ExecutionIdentity ToPrivateAsciiIdentity(AsciiExecutionIdentity source) {
  protocol_lab::ascii::ExecutionIdentity result;
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  result.binding_index = source.binding_index;
  result.stream_index = source.stream_index;
  result.stream_generation = source.stream_generation;
  result.operation_sequence = source.operation_sequence;
#endif
  result.document_id = source.document_id;
  result.load_revision = source.load_revision;
  result.plan_generation = source.plan_generation;
  result.selection_revision = source.selection_revision;
  result.input_revision = source.input_revision;
  result.pipeline_index = source.pipeline_index;
  result.pipeline_id = std::move(source.pipeline_id);
  result.message_index = source.message_index;
  result.message_id = std::move(source.message_id);
  return result;
}

std::vector<protocol_lab::ascii::InputField> ToPrivateAsciiInputs(
    const std::vector<AsciiInputField>& source) {
  std::vector<protocol_lab::ascii::InputField> result;
  result.reserve(source.size());
  for (const auto& field : source)
    result.push_back({field.field_index, field.field_id, field.bytes});
  return result;
}

AsciiExecutionResult ConvertPrivateAsciiResult(protocol_lab::ascii::ExecutionResult source) {
  AsciiExecutionResult result;
  result.identity = ConvertIdentity(std::move(source.identity));
  result.operation = source.operation == protocol_lab::ascii::Operation::ENCODE
                         ? AsciiOperation::ENCODE
                         : AsciiOperation::INSPECT;
  result.status = ToOwnedStatus(source.status);
  result.codec_status = ToPublicStatus(source.core_status);
  result.codec_called = source.core_called;
  result.review_kind = source.review_kind == protocol_lab::ascii::ReviewKind::TX_TEMPLATE
                           ? AsciiReviewKind::TX_TEMPLATE
                           : AsciiReviewKind::NOT_APPLICABLE;
  result.message_index = source.message_index;
  result.message_id = std::move(source.message_id);
  result.failed_input_index = source.failed_input_index;
  result.failed_field_index = source.failed_field_index;
  result.failed_field_id = std::move(source.failed_field_id);
  result.frame = std::move(source.frame);
  result.diagnostic_input_frame = std::move(source.diagnostic_input_frame);
  result.detail = std::move(source.detail);
  result.fields.reserve(source.fields.size());
  for (auto& value : source.fields) {
    AsciiFieldResult field;
    field.field_index = value.field_index;
    field.field_id = std::move(value.field_id);
    field.bytes = std::move(value.bytes);
    field.range = {value.range.offset, value.range.length};
    result.fields.push_back(std::move(field));
  }
  return result;
}

AsciiStreamObservation ConvertPrivateAsciiObservation(
    const protocol_lab::ascii::StreamObservation& source) noexcept {
  AsciiStreamObservation result;
  switch (source.phase) {
    case protocol_framing::StreamFramingPhase::DELIVERY_PENDING:
      result.phase = StreamFramingPhase::DELIVERY_PENDING;
      break;
    case protocol_framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF:
      result.phase = StreamFramingPhase::DISCARDING_UNTIL_CRLF;
      break;
    default: result.phase = StreamFramingPhase::COLLECTING; break;
  }
  result.buffered_bytes = source.buffered_bytes;
  result.has_internal_work = source.has_internal_work;
  result.effective_max_submit_bytes = source.effective_max_submit_bytes;
  result.effective_max_work_units = source.effective_max_work_units;
  result.frozen_input_bytes = source.frozen_input_bytes;
  result.frozen_cursor = source.frozen_cursor;
  result.reset_required = source.reset_required;
  result.generation = source.generation;
  result.step_sequence = source.step_sequence;
  result.total_candidates = source.total_candidates;
  result.total_decode_successes = source.total_decode_successes;
#if defined(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER)
  result.total_observed_candidates = source.total_observed_candidates;
  result.total_business_outputs = source.total_business_outputs;
#endif
  result.total_discarded_bytes = source.total_discarded_bytes;
  result.total_malformed_candidates = source.total_malformed_candidates;
  return result;
}

AsciiStreamStepResult ConvertPrivateAsciiStreamStep(protocol_lab::ascii::StreamStepResult source) {
  AsciiStreamStepResult result;
  result.identity = ConvertIdentity(std::move(source.identity));
  result.status = ToOwnedStatus(source.status);
  result.push_called = source.push_called;
  result.framing = ConvertFraming(source.framing);
  result.before = ConvertPrivateAsciiObservation(source.before);
  result.after = ConvertPrivateAsciiObservation(source.after);
  if (source.candidate) result.candidate = ConvertPrivateAsciiResult(std::move(*source.candidate));
  result.detail = std::move(source.detail);
  return result;
}

}  // namespace pae::protocol_lab_ui
