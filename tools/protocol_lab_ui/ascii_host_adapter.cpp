#include "ascii_host_adapter.h"

#include <utility>

namespace pae::protocol_lab_ui {
namespace {

protocol_framing::StreamFramingPhase ToPrivatePhase(StreamFramingPhase value) noexcept {
  switch (value) {
    case StreamFramingPhase::DELIVERY_PENDING:
      return protocol_framing::StreamFramingPhase::DELIVERY_PENDING;
    case StreamFramingPhase::DISCARDING_UNTIL_CRLF:
      return protocol_framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF;
    default:
      return protocol_framing::StreamFramingPhase::COLLECTING;
  }
}

protocol_lab::ascii::AdapterStatus ToPrivateAdapterStatus(
    protocol_lab_ascii::public_offline::LocalStatus value) noexcept {
  using L = protocol_lab_ascii::public_offline::LocalStatus;
  switch (value) {
    case L::OK: return protocol_lab::ascii::AdapterStatus::OK;
    case L::CODEC_FAILED: return protocol_lab::ascii::AdapterStatus::CORE_FAILED;
    case L::MATERIALIZATION_FAILED:
    case L::ALLOCATION_FAILED: return protocol_lab::ascii::AdapterStatus::MATERIALIZATION_FAILED;
    default: return protocol_lab::ascii::AdapterStatus::INVALID_REQUEST;
  }
}

protocol_framing::SubmitApiStatus ToPrivateStatus(StreamFramerStatus value) noexcept {
  using P = protocol_framing::SubmitApiStatus;
  switch (value) {
    case StreamFramerStatus::OK: return P::OK;
    case StreamFramerStatus::INVALID_ARGUMENT: return P::INVALID_ARGUMENT;
    case StreamFramerStatus::INVALID_COMPILED_PROTOCOL: return P::INVALID_PLAN;
    case StreamFramerStatus::WORKSPACE_BUSY: return P::WORKSPACE_BUSY;
    case StreamFramerStatus::REENTRANT_CALL: return P::REENTRANT_CALL;
    case StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED: return P::LIMIT_EXCEEDED;
    default: return P::INTERNAL_ERROR;
  }
}

protocol_framing::SubmitStopReason ToPrivateStop(StreamFramerStopReason value) noexcept {
  using P = protocol_framing::SubmitStopReason;
  switch (value) {
    case StreamFramerStopReason::NEED_MORE: return P::NEED_MORE;
    case StreamFramerStopReason::WORK_BUDGET_REACHED: return P::WORK_BUDGET_REACHED;
    case StreamFramerStopReason::SINK_STOP: return P::SINK_STOP;
    default: return P::INPUT_EXHAUSTED;
  }
}

protocol_framing::FramingIssue ToPrivateIssue(StreamFramingIssue value) noexcept {
  using P = protocol_framing::FramingIssue;
  switch (value) {
    case StreamFramingIssue::MALFORMED_LENGTH: return P::MALFORMED_LENGTH;
    case StreamFramingIssue::RECORD_TOO_LONG: return P::RECORD_TOO_LONG;
    default: return P::NONE;
  }
}

protocol_framing::SubmitResult ConvertFraming(const StreamSubmitResult& source) noexcept {
  protocol_framing::SubmitResult result;
  result.api_status = ToPrivateStatus(source.status);
  result.stop_reason = ToPrivateStop(source.stop_reason);
  result.bytes_consumed = source.bytes_consumed;
  result.frames_delivered = source.candidates_delivered;
  result.bytes_discarded = source.bytes_discarded;
  result.malformed_candidates = source.malformed_candidates;
  result.last_framing_issue = ToPrivateIssue(source.last_issue);
  result.work_units_used = source.work_units_used;
  return result;
}

std::string StreamDiagnosticText(protocol_lab_ascii::public_offline::StreamDiagnostic value) {
  using D = protocol_lab_ascii::public_offline::StreamDiagnostic;
  switch (value) {
    case D::NONE: return {};
    case D::CHANNEL_NOT_ASCII_STREAM: return "selected Host channel is not an ASCII stream";
    case D::RESET_REQUIRED: return "stream Reset is required";
    case D::INVALID_CHUNK_OR_CONTINUE_REQUIRED: return "invalid chunk or Continue is required";
    case D::CHUNK_ALLOCATION_FAILED: return "stream chunk allocation failed";
    case D::CHUNK_MATERIALIZATION_FAILED: return "stream chunk materialization failed";
    case D::NO_FROZEN_SUFFIX_OR_INTERNAL_WORK: return "no frozen suffix or internal work remains";
    case D::CANDIDATE_COPY_FAILED_RESET_REQUIRED:
      return "candidate copy failed; stream Reset is required";
    case D::STREAM_CONTRACT_VIOLATION_RESET_REQUIRED:
      return "stream contract violation; stream Reset is required";
  }
  return "unknown public stream diagnostic";
}

protocol_lab::ascii::StreamObservation ConvertObservation(
    const protocol_lab_ascii::public_offline::StreamObservation& source) noexcept {
  protocol_lab::ascii::StreamObservation result;
  result.phase = ToPrivatePhase(source.phase);
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
  result.total_observed_candidates = source.total_observer_callbacks;
  result.total_business_outputs = source.total_business_callbacks;
  result.total_discarded_bytes = source.total_discarded_bytes;
  result.total_malformed_candidates = source.total_malformed_candidates;
  return result;
}

protocol_core::CodecStatus ToPrivateStatus(CodecStatus value) noexcept {
  using P = protocol_core::CodecStatus;
  switch (value) {
    case CodecStatus::OK:
      return P::OK;
    case CodecStatus::UNKNOWN_MESSAGE:
      return P::UNKNOWN_MESSAGE;
    case CodecStatus::AMBIGUOUS_MESSAGE:
      return P::AMBIGUOUS_MESSAGE;
    case CodecStatus::OUTPUT_SLOTS_TOO_SMALL:
      return P::OUTPUT_SLOTS_TOO_SMALL;
    case CodecStatus::INTEGRITY_FAILED:
      return P::INTEGRITY_FAILED;
    case CodecStatus::MESSAGE_NOT_ALLOWED:
      return P::MESSAGE_NOT_ALLOWED;
    case CodecStatus::FIELD_REFERENCE_MISMATCH:
      return P::FIELD_REFERENCE_MISMATCH;
    case CodecStatus::DUPLICATE_FIELD:
      return P::DUPLICATE_FIELD;
    case CodecStatus::MISSING_FIELD:
      return P::MISSING_FIELD;
    case CodecStatus::TYPE_MISMATCH:
      return P::TYPE_MISMATCH;
    case CodecStatus::VALUE_NOT_REPRESENTABLE:
      return P::VALUE_NOT_REPRESENTABLE;
    case CodecStatus::BYTES_LENGTH_MISMATCH:
      return P::BYTES_LENGTH_MISMATCH;
    case CodecStatus::UNKNOWN_ENUM_VALUE:
      return P::UNKNOWN_ENUM_VALUE;
    case CodecStatus::ENUM_REFERENCE_MISMATCH:
      return P::ENUM_REFERENCE_MISMATCH;
    case CodecStatus::CONSTANT_FIELD_OVERRIDE:
      return P::CONSTANT_FIELD_OVERRIDE;
    case CodecStatus::INPUT_OUTPUT_OVERLAP:
      return P::INPUT_OUTPUT_OVERLAP;
    case CodecStatus::BUFFER_TOO_SMALL:
      return P::BUFFER_TOO_SMALL;
    case CodecStatus::FINAL_REVIEW_FAILED:
      return P::FINAL_REVIEW_FAILED;
    case CodecStatus::ASCII_CHARACTER_NOT_ALLOWED:
      return P::ASCII_CHARACTER_NOT_ALLOWED;
    case CodecStatus::ASCII_TERMINATOR_CONFLICT:
      return P::ASCII_TERMINATOR_CONFLICT;
    case CodecStatus::OPERATION_NOT_SUPPORTED:
      return P::OPERATION_NOT_SUPPORTED;
    case CodecStatus::COMPUTED_FIELD_OVERRIDE:
      return P::COMPUTED_FIELD_OVERRIDE;
    case CodecStatus::LENGTH_MISMATCH:
      return P::LENGTH_MISMATCH;
    case CodecStatus::WORKSPACE_BUSY:
      return P::WORKSPACE_BUSY;
    default:
      return P::INVALID_ARGUMENT;
  }
}

protocol_lab::ascii::ExecutionResult Convert(
    protocol_lab_ascii::public_offline::OperationResult source,
    protocol_lab::ascii::ExecutionIdentity identity, protocol_lab::ascii::Operation operation) {
  protocol_lab::ascii::ExecutionResult result;
  result.identity = std::move(identity);
  result.operation = operation;
  result.core_called = source.codec_called;
  result.core_status = ToPrivateStatus(source.codec_status);
  result.message_index = source.message_index;
  result.message_id = std::move(source.message_id);
  result.failed_input_index = source.failed_value_index;
  result.failed_field_index = source.failed_field_flat_index;
  result.frame = std::move(source.frame);
  result.diagnostic_input_frame = std::move(source.diagnostic_input_frame);
  result.review_kind = operation == protocol_lab::ascii::Operation::ENCODE
                           ? protocol_lab::ascii::ReviewKind::TX_TEMPLATE
                           : protocol_lab::ascii::ReviewKind::NOT_APPLICABLE;
  switch (source.local_status) {
    case protocol_lab_ascii::public_offline::LocalStatus::OK:
      result.status = protocol_lab::ascii::AdapterStatus::OK;
      break;
    case protocol_lab_ascii::public_offline::LocalStatus::CODEC_FAILED:
      result.status = protocol_lab::ascii::AdapterStatus::CORE_FAILED;
      break;
    case protocol_lab_ascii::public_offline::LocalStatus::MATERIALIZATION_FAILED:
    case protocol_lab_ascii::public_offline::LocalStatus::ALLOCATION_FAILED:
      result.status = protocol_lab::ascii::AdapterStatus::MATERIALIZATION_FAILED;
      break;
    default:
      result.status = protocol_lab::ascii::AdapterStatus::INVALID_REQUEST;
      break;
  }
  result.fields.reserve(source.fields.size());
  for (auto& value : source.fields) {
    protocol_lab::ascii::FieldResult field;
    field.field_index = value.field_index;
    field.field_id = std::move(value.id);
    field.bytes = std::move(value.bytes);
    field.range = {value.range.offset, value.range.length};
    result.fields.push_back(std::move(field));
  }
  return result;
}

}  // namespace

protocol_lab::ascii::ExecutionResult ConvertPublicAsciiResult(
    protocol_lab_ascii::public_offline::OperationResult source,
    protocol_lab::ascii::ExecutionIdentity identity, protocol_lab::ascii::Operation operation) {
  return Convert(std::move(source), std::move(identity), operation);
}

std::unique_ptr<AsciiHostAdapter> AsciiHostAdapter::CreatePublic(
    CompiledProtocol compiled, std::vector<protocol_lab::ascii::HostBinding> bindings,
    std::size_t previous_instance_bytes, std::string& error) {
  try {
    std::vector<protocol_lab_ascii::public_offline::HostBinding> public_bindings;
    public_bindings.reserve(bindings.size());
    for (const auto& binding : bindings)
      public_bindings.push_back({binding.endpoint,
                                 binding.action == host_endpoint::Action::DECODE
                                     ? HostAction::DECODE
                                     : HostAction::ENCODE,
                                 binding.pipeline_index});
    auto prepared = protocol_lab_ascii::public_offline::HostAdapter::Create(
        std::move(compiled), std::move(public_bindings), {}, previous_instance_bytes);
    if (!prepared.adapter) {
      error = "public ASCII Host preparation failed: local=" +
              std::to_string(static_cast<int>(prepared.status)) +
              "; host=" + std::to_string(static_cast<int>(prepared.host_status));
      return nullptr;
    }
    auto result = std::unique_ptr<AsciiHostAdapter>(new AsciiHostAdapter);
    if (!BuildDocumentDescription(prepared.adapter->Description(), result->description_, error))
      return nullptr;
    result->bindings_ = std::move(bindings);
    result->public_ = std::move(prepared.adapter);
    for (std::size_t index = 0U; index < result->bindings_.size(); ++index) {
      const auto observed = result->public_->ObserveStream(index, 0U);
      if (!observed) continue;
      result->public_stream_ = true;
      auto& pipeline = result->description_.pipelines[result->bindings_[index].pipeline_index];
      pipeline.stream_ascii_crlf = true;
      pipeline.maximum_frame_length = observed->maximum_candidate_frame_bytes;
    }
    return result;
  } catch (const std::exception& exception) {
    error = exception.what();
    return nullptr;
  }
}

std::unique_ptr<AsciiHostAdapter> AsciiHostAdapter::CreatePublicDirect(CompiledProtocol compiled,
                                                                       std::string& error) {
  std::vector<protocol_lab::ascii::HostBinding> bindings;
  try {
    for (std::size_t pipeline = 0U; pipeline < compiled.PipelineCount(); ++pipeline) {
      const auto description = compiled.Pipeline(pipeline);
      if (!description) {
        error = "public ASCII Pipeline description is unavailable";
        return nullptr;
      }
      bool decode = false;
      bool encode = false;
      for (std::size_t association = 0U; association < description->message_count; ++association) {
        const auto message = compiled.PipelineMessageIndex(pipeline, association);
        const auto execution = message ? compiled.PipelineMessageExecution(pipeline, *message)
                                       : std::nullopt;
        if (!execution) {
          error = "public ASCII Pipeline execution description is unavailable";
          return nullptr;
        }
        decode = decode || execution->decode_available;
        encode = encode || execution->encode_available;
      }
      const auto endpoint = "direct." + std::to_string(pipeline);
      if (decode) bindings.push_back({endpoint, host_endpoint::Action::DECODE, pipeline});
      if (encode) bindings.push_back({endpoint, host_endpoint::Action::ENCODE, pipeline});
    }
  } catch (const std::exception& exception) {
    error = exception.what();
    return nullptr;
  }
  if (bindings.empty()) {
    error = "public ASCII description has no executable binding";
    return nullptr;
  }
  return CreatePublic(std::move(compiled), std::move(bindings), 0U, error);
}

std::unique_ptr<AsciiHostAdapter> AsciiHostAdapter::CreatePrivate(
    config_compiler::CompiledUiArtifacts artifacts,
    std::vector<protocol_lab::ascii::HostBinding> bindings, std::string& error) {
  auto adapter =
      protocol_lab::ascii::HostObserverAdapter::Create(std::move(artifacts), bindings, error);
  if (!adapter) return nullptr;
  auto result = std::unique_ptr<AsciiHostAdapter>(new AsciiHostAdapter);
  if (!BuildDocumentDescription(adapter->Description(), result->description_, error))
    return nullptr;
  result->bindings_ = std::move(bindings);
  result->private_ = std::move(adapter);
  return result;
}

std::size_t AsciiHostAdapter::FlowCount(std::size_t binding) const noexcept {
  if (binding >= bindings_.size()) return 0U;
  return bindings_[binding].action == host_endpoint::Action::DECODE ? 2U : 1U;
}

std::size_t AsciiHostAdapter::AccountedBytes() const noexcept {
  return public_ ? public_->AccountedBytes() : private_->AccountedBytes();
}

std::optional<std::size_t> AsciiHostAdapter::FindBinding(
    std::size_t pipeline_index, host_endpoint::Action action) const noexcept {
  for (std::size_t index = 0U; index < bindings_.size(); ++index)
    if (bindings_[index].pipeline_index == pipeline_index && bindings_[index].action == action)
      return index;
  return std::nullopt;
}

protocol_lab::ascii::ExecutionResult AsciiHostAdapter::Inspect(
    std::size_t binding, std::size_t stream, protocol_lab::ascii::ExecutionIdentity identity,
    const std::vector<std::uint8_t>& frame) {
  if (private_) return private_->Inspect(binding, stream, std::move(identity), frame);
  auto result = public_->Decode(binding, stream, {frame.data(), frame.size()});
  return Convert(std::move(result), std::move(identity), protocol_lab::ascii::Operation::INSPECT);
}

protocol_lab::ascii::ExecutionResult AsciiHostAdapter::Encode(
    std::size_t binding, protocol_lab::ascii::ExecutionIdentity identity,
    const std::vector<protocol_lab::ascii::InputField>& fields) {
  if (private_) return private_->Encode(binding, std::move(identity), fields);
  std::vector<protocol_lab_ascii::public_offline::InputField> inputs;
  inputs.reserve(fields.size());
  for (const auto& field : fields) inputs.push_back({field.field_index, field.bytes});
  const auto message_index = identity.message_index.value_or(static_cast<std::size_t>(-1));
  auto result = public_->Encode(binding, message_index, inputs);
  return Convert(std::move(result), std::move(identity), protocol_lab::ascii::Operation::ENCODE);
}

protocol_lab::ascii::StreamStepResult AsciiHostAdapter::Submit(
    std::size_t binding, std::size_t stream, protocol_lab::ascii::ExecutionIdentity identity,
    const std::vector<std::uint8_t>& chunk) {
  if (private_) return private_->Submit(binding, stream, std::move(identity), chunk);
  auto source = public_->SubmitStreamChunk(binding, stream, chunk);
  protocol_lab::ascii::StreamStepResult result;
  result.identity = std::move(identity);
  result.status = ToPrivateAdapterStatus(source.status);
  result.push_called = source.host_called;
  result.framing = ConvertFraming(source.host.framing);
  result.before = ConvertObservation(source.before);
  result.after = ConvertObservation(source.after);
  if (source.candidate)
    result.candidate = Convert(std::move(*source.candidate), result.identity,
                               protocol_lab::ascii::Operation::INSPECT);
  result.detail = StreamDiagnosticText(source.diagnostic);
  return result;
}

protocol_lab::ascii::StreamStepResult AsciiHostAdapter::Continue(
    std::size_t binding, std::size_t stream, protocol_lab::ascii::ExecutionIdentity identity) {
  if (private_) return private_->Continue(binding, stream, std::move(identity));
  auto source = public_->ContinueStream(binding, stream);
  protocol_lab::ascii::StreamStepResult result;
  result.identity = std::move(identity);
  result.status = ToPrivateAdapterStatus(source.status);
  result.push_called = source.host_called;
  result.framing = ConvertFraming(source.host.framing);
  result.before = ConvertObservation(source.before);
  result.after = ConvertObservation(source.after);
  if (source.candidate)
    result.candidate = Convert(std::move(*source.candidate), result.identity,
                               protocol_lab::ascii::Operation::INSPECT);
  result.detail = StreamDiagnosticText(source.diagnostic);
  return result;
}

bool AsciiHostAdapter::Reset(std::size_t binding, std::size_t stream) {
  return public_ ? public_->Reset(binding, stream) : private_->Reset(binding, stream);
}

std::optional<protocol_lab::ascii::StreamObservation> AsciiHostAdapter::Observe(
    std::size_t binding, std::size_t stream) const noexcept {
  if (private_) return private_->Observe(binding, stream);
  const auto observed = public_->ObserveStream(binding, stream);
  return observed ? std::optional<protocol_lab::ascii::StreamObservation>{ConvertObservation(*observed)}
                  : std::nullopt;
}

bool AsciiHostAdapter::HasDiscardableState() const noexcept {
  if (private_) return private_->HasDiscardableState();
  for (std::size_t binding = 0U; binding < bindings_.size(); ++binding)
    for (std::size_t flow = 0U; flow < FlowCount(binding); ++flow) {
      const auto observed = public_->ObserveStream(binding, flow);
      if (observed && (observed->buffered_bytes != 0U || observed->frozen_input_bytes != 0U ||
                       observed->has_internal_work || observed->reset_required ||
                       observed->phase != StreamFramingPhase::COLLECTING))
        return true;
    }
  return false;
}

}  // namespace pae::protocol_lab_ui
