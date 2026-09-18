#include "ascii_host_adapter.h"

#include <utility>

namespace pae::protocol_lab_ui {
namespace {

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
    return result;
  } catch (const std::exception& exception) {
    error = exception.what();
    return nullptr;
  }
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
  protocol_lab::ascii::StreamStepResult result;
  result.identity = std::move(identity);
  result.detail = "public ASCII 0.10 Host supports complete records only";
  return result;
}

protocol_lab::ascii::StreamStepResult AsciiHostAdapter::Continue(
    std::size_t binding, std::size_t stream, protocol_lab::ascii::ExecutionIdentity identity) {
  if (private_) return private_->Continue(binding, stream, std::move(identity));
  protocol_lab::ascii::StreamStepResult result;
  result.identity = std::move(identity);
  result.detail = "public ASCII 0.10 Host supports complete records only";
  return result;
}

bool AsciiHostAdapter::Reset(std::size_t binding, std::size_t stream) {
  return public_ ? public_->Reset(binding, stream) : private_->Reset(binding, stream);
}

std::optional<protocol_lab::ascii::StreamObservation> AsciiHostAdapter::Observe(
    std::size_t binding, std::size_t stream) const noexcept {
  return private_ ? private_->Observe(binding, stream) : std::nullopt;
}

bool AsciiHostAdapter::HasDiscardableState() const noexcept {
  return private_ && private_->HasDiscardableState();
}

}  // namespace pae::protocol_lab_ui
