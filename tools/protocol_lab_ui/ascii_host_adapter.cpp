#include "ascii_host_adapter.h"

#include <algorithm>
#include <utility>

#include "../protocol_lab_ascii/public_ascii_host_adapter.h"

namespace pae::protocol_lab_ui {
namespace {

AsciiAdapterStatus ToAdapterStatus(
    protocol_lab_ascii::public_offline::LocalStatus value) noexcept {
  using L = protocol_lab_ascii::public_offline::LocalStatus;
  switch (value) {
    case L::OK: return AsciiAdapterStatus::OK;
    case L::CODEC_FAILED: return AsciiAdapterStatus::CORE_FAILED;
    case L::MATERIALIZATION_FAILED:
    case L::ALLOCATION_FAILED: return AsciiAdapterStatus::MATERIALIZATION_FAILED;
    default: return AsciiAdapterStatus::INVALID_REQUEST;
  }
}

std::string StreamDiagnosticText(protocol_lab_ascii::public_offline::StreamDiagnostic value) {
  using D = protocol_lab_ascii::public_offline::StreamDiagnostic;
  switch (value) {
    case D::NONE: return {};
    case D::CHANNEL_NOT_ASCII_STREAM: return "selected Host channel is not an ASCII stream";
    case D::RESET_REQUIRED: return "stream Reset is required";
    case D::INVALID_CHUNK_OR_CONTINUE_REQUIRED:
      return "invalid chunk or Continue is required";
    case D::CHUNK_ALLOCATION_FAILED: return "stream chunk allocation failed";
    case D::CHUNK_MATERIALIZATION_FAILED: return "stream chunk materialization failed";
    case D::NO_FROZEN_SUFFIX_OR_INTERNAL_WORK:
      return "no frozen suffix or internal work remains";
    case D::CANDIDATE_COPY_FAILED_RESET_REQUIRED:
      return "candidate copy failed; stream Reset is required";
    case D::STREAM_CONTRACT_VIOLATION_RESET_REQUIRED:
      return "stream contract violation; stream Reset is required";
  }
  return "unknown public stream diagnostic";
}

AsciiStreamObservation ConvertObservation(
    const protocol_lab_ascii::public_offline::StreamObservation& source) noexcept {
  AsciiStreamObservation result;
  result.phase = source.phase;
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

bool BuildPublicDescription(const protocol_lab_ascii::public_offline::OwnedDescription& source,
                            DocumentDescription& output, std::string& error) {
  DocumentDescription built;
  built.layout = DocumentLayout::ASCII_TEXT;
  built.schema_version = source.schema_version;
  built.protocol_id = source.protocol_id;
  built.protocol_version = source.protocol_version;
  built.display_name = source.display_name;
  built.description = source.description;
  built.source_ref = source.source_ref;
  built.pipelines.reserve(source.pipelines.size());
  for (const auto& value : source.pipelines) {
    if (value.index != built.pipelines.size()) {
      error = "public ASCII Pipeline indices are not contiguous";
      return false;
    }
    PipelineDescriptor pipeline;
    pipeline.pipeline_index = value.index;
    pipeline.id = value.id;
    pipeline.direction_id = value.direction_id;
    pipeline.display_name = value.display_name;
    pipeline.description = value.description;
    pipeline.source_ref = value.source_ref;
    pipeline.message_indices = value.message_indices;
    pipeline.decode_message_indices = value.decode_message_indices;
    built.pipelines.push_back(std::move(pipeline));
  }
  built.messages.reserve(source.messages.size());
  for (const auto& value : source.messages) {
    if (value.index != built.messages.size()) {
      error = "public ASCII Message indices are not contiguous";
      return false;
    }
    MessageDescriptor message;
    message.message_index = value.index;
    message.id = value.id;
    message.direction_id = value.direction_id;
    message.display_name = value.display_name;
    message.description = value.description;
    message.source_ref = value.source_ref;
    message.encode_available = value.encode.has_value();
    message.decode_available = value.decode.has_value();
    if (value.encode) message.frame_size = value.encode->record_length.maximum;
    if (value.decode)
      message.frame_size = (std::max)(message.frame_size, value.decode->record_length.maximum);
    built.max_frame_bytes = (std::max)(built.max_frame_bytes, message.frame_size);
    message.fields.reserve(value.fields.size());
    for (const auto& field_value : value.fields) {
      if (field_value.field_index != message.fields.size()) {
        error = "public ASCII Field indices are not contiguous";
        return false;
      }
      FieldDescriptor field;
      field.field_index = field_value.field_index;
      field.id = field_value.id;
      field.display_name = field_value.display_name;
      field.description = field_value.description;
      field.source_ref = field_value.source_ref;
      field.value_type = FieldValueType::BYTES;
      field.wire_codec = FieldWireCodec::ASCII_TEXT;
      field.byte_order = FieldByteOrder::NOT_APPLICABLE;
      field.encode_source = field_value.encode_referenced ? FieldEncodeSource::INPUT
                                                          : FieldEncodeSource::CONSTANT;
      field.byte_width = field_value.byte_length.maximum;
      field.byte_length_bounds =
          ByteLengthBounds{field_value.byte_length.minimum, field_value.byte_length.maximum};
      field.ascii_text = true;
      field.decode_referenced = field_value.decode_referenced;
      field.encode_referenced = field_value.encode_referenced;
      if (field_value.allowed_control_bytes)
        field.allowed_control_bytes = *field_value.allowed_control_bytes;
      if (!field.encode_referenced) field.read_only_annotation = "not referenced by Encode action";
      message.fields.push_back(std::move(field));
    }
    built.messages.push_back(std::move(message));
  }
  for (const auto& pipeline : built.pipelines) {
    for (const auto index : pipeline.message_indices)
      if (index >= built.messages.size()) {
        error = "public ASCII Pipeline Message index is outside the description";
        return false;
      }
    for (const auto index : pipeline.decode_message_indices)
      if (index >= built.messages.size() || !built.messages[index].decode_available) {
        error = "public ASCII Pipeline Decode candidate is outside the description";
        return false;
      }
  }
  output = std::move(built);
  error.clear();
  return true;
}

class PublicBackend final : public AsciiHostBackend {
 public:
  explicit PublicBackend(std::unique_ptr<protocol_lab_ascii::public_offline::HostAdapter> adapter)
      : adapter_(std::move(adapter)) {}

  std::size_t AccountedBytes() const noexcept override { return adapter_->AccountedBytes(); }

  AsciiExecutionResult Inspect(std::size_t binding, std::size_t stream,
                               AsciiExecutionIdentity identity,
                               const std::vector<std::uint8_t>& frame) override {
    return ConvertPublicAsciiResult(adapter_->Decode(binding, stream, {frame.data(), frame.size()}),
                                    std::move(identity), AsciiOperation::INSPECT);
  }

  AsciiExecutionResult Encode(std::size_t binding, AsciiExecutionIdentity identity,
                              const std::vector<AsciiInputField>& fields) override {
    std::vector<protocol_lab_ascii::public_offline::InputField> inputs;
    inputs.reserve(fields.size());
    for (const auto& field : fields) inputs.push_back({field.field_index, field.bytes});
    const auto message_index = identity.message_index.value_or(static_cast<std::size_t>(-1));
    return ConvertPublicAsciiResult(adapter_->Encode(binding, message_index, inputs),
                                    std::move(identity), AsciiOperation::ENCODE);
  }

  AsciiStreamStepResult Submit(std::size_t binding, std::size_t stream,
                               AsciiExecutionIdentity identity,
                               const std::vector<std::uint8_t>& chunk) override {
    return ConvertStream(adapter_->SubmitStreamChunk(binding, stream, chunk), std::move(identity));
  }

  AsciiStreamStepResult Continue(std::size_t binding, std::size_t stream,
                                 AsciiExecutionIdentity identity) override {
    return ConvertStream(adapter_->ContinueStream(binding, stream), std::move(identity));
  }

  bool Reset(std::size_t binding, std::size_t stream) override {
    return adapter_->Reset(binding, stream);
  }

  std::optional<AsciiStreamObservation> Observe(std::size_t binding,
                                                std::size_t stream) const noexcept override {
    const auto observed = adapter_->ObserveStream(binding, stream);
    return observed ? std::optional<AsciiStreamObservation>{ConvertObservation(*observed)}
                    : std::nullopt;
  }

  bool HasDiscardableState(const std::vector<AsciiHostBinding>& bindings) const noexcept override {
    for (std::size_t binding = 0U; binding < bindings.size(); ++binding)
      for (std::size_t flow = 0U;
           flow < (bindings[binding].action == AsciiHostAction::DECODE ? 2U : 1U); ++flow) {
        const auto observed = adapter_->ObserveStream(binding, flow);
        if (observed &&
            (observed->buffered_bytes != 0U || observed->frozen_input_bytes != 0U ||
             observed->has_internal_work || observed->reset_required ||
             observed->phase != StreamFramingPhase::COLLECTING))
          return true;
      }
    return false;
  }

 private:
  static AsciiStreamStepResult ConvertStream(
      protocol_lab_ascii::public_offline::StreamStepResult source,
      AsciiExecutionIdentity identity) {
    AsciiStreamStepResult result;
    result.identity = std::move(identity);
    result.status = ToAdapterStatus(source.status);
    result.push_called = source.host_called;
    result.framing = source.host.framing;
    result.before = ConvertObservation(source.before);
    result.after = ConvertObservation(source.after);
    if (source.candidate)
      result.candidate = ConvertPublicAsciiResult(std::move(*source.candidate), result.identity,
                                                  AsciiOperation::INSPECT);
    result.detail = StreamDiagnosticText(source.diagnostic);
    return result;
  }

  std::unique_ptr<protocol_lab_ascii::public_offline::HostAdapter> adapter_;
};

}  // namespace

AsciiExecutionResult ConvertPublicAsciiResult(
    protocol_lab_ascii::public_offline::OperationResult source,
    AsciiExecutionIdentity identity, AsciiOperation operation) {
  AsciiExecutionResult result;
  result.identity = std::move(identity);
  result.operation = operation;
  result.codec_called = source.codec_called;
  result.codec_status = source.codec_status;
  result.message_index = source.message_index;
  result.message_id = std::move(source.message_id);
  result.failed_input_index = source.failed_value_index;
  result.failed_field_index = source.failed_field_flat_index;
  result.frame = std::move(source.frame);
  result.diagnostic_input_frame = std::move(source.diagnostic_input_frame);
  result.review_kind = operation == AsciiOperation::ENCODE ? AsciiReviewKind::TX_TEMPLATE
                                                            : AsciiReviewKind::NOT_APPLICABLE;
  result.status = ToAdapterStatus(source.local_status);
  result.fields.reserve(source.fields.size());
  for (auto& value : source.fields) {
    AsciiFieldResult field;
    field.field_index = value.field_index;
    field.field_id = std::move(value.id);
    field.bytes = std::move(value.bytes);
    field.range = value.range;
    result.fields.push_back(std::move(field));
  }
  return result;
}

AsciiHostAdapter::AsciiHostAdapter(std::unique_ptr<AsciiHostBackend> backend,
                                   DocumentDescription description,
                                   std::vector<AsciiHostBinding> bindings, bool public_complete,
                                   bool public_stream) noexcept
    : backend_(std::move(backend)),
      description_(std::move(description)),
      bindings_(std::move(bindings)),
      public_complete_(public_complete),
      public_stream_(public_stream) {}

std::unique_ptr<AsciiHostAdapter> AsciiHostAdapter::AdoptBackend(
    std::unique_ptr<AsciiHostBackend> backend, DocumentDescription description,
    std::vector<AsciiHostBinding> bindings, bool public_complete, bool public_stream) {
  if (!backend) return nullptr;
  return std::unique_ptr<AsciiHostAdapter>(new AsciiHostAdapter(
      std::move(backend), std::move(description), std::move(bindings), public_complete,
      public_stream));
}

std::unique_ptr<AsciiHostAdapter> AsciiHostAdapter::CreatePublic(
    CompiledProtocol compiled, std::vector<AsciiHostBinding> bindings,
    std::size_t previous_instance_bytes, std::string& error) {
  try {
    std::vector<protocol_lab_ascii::public_offline::HostBinding> public_bindings;
    public_bindings.reserve(bindings.size());
    for (const auto& binding : bindings)
      public_bindings.push_back({binding.endpoint,
                                 binding.action == AsciiHostAction::DECODE ? HostAction::DECODE
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
    DocumentDescription description;
    if (!BuildPublicDescription(prepared.adapter->Description(), description, error)) return nullptr;
    bool public_stream = false;
    for (std::size_t index = 0U; index < bindings.size(); ++index) {
      const auto observed = prepared.adapter->ObserveStream(index, 0U);
      if (!observed) continue;
      public_stream = true;
      auto& pipeline = description.pipelines[bindings[index].pipeline_index];
      pipeline.stream_ascii_crlf = true;
      pipeline.maximum_frame_length = observed->maximum_candidate_frame_bytes;
    }
    return AdoptBackend(std::make_unique<PublicBackend>(std::move(prepared.adapter)),
                        std::move(description), std::move(bindings), !public_stream, public_stream);
  } catch (const std::exception& exception) {
    error = exception.what();
    return nullptr;
  }
}

std::unique_ptr<AsciiHostAdapter> AsciiHostAdapter::CreatePublicDirect(CompiledProtocol compiled,
                                                                       std::string& error) {
  std::vector<AsciiHostBinding> bindings;
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
      if (decode) bindings.push_back({endpoint, AsciiHostAction::DECODE, pipeline});
      if (encode) bindings.push_back({endpoint, AsciiHostAction::ENCODE, pipeline});
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

std::size_t AsciiHostAdapter::FlowCount(std::size_t binding) const noexcept {
  if (binding >= bindings_.size()) return 0U;
  return bindings_[binding].action == AsciiHostAction::DECODE ? 2U : 1U;
}

std::size_t AsciiHostAdapter::AccountedBytes() const noexcept {
  return backend_->AccountedBytes();
}

std::optional<std::size_t> AsciiHostAdapter::FindBinding(
    std::size_t pipeline_index, AsciiHostAction action) const noexcept {
  for (std::size_t index = 0U; index < bindings_.size(); ++index)
    if (bindings_[index].pipeline_index == pipeline_index && bindings_[index].action == action)
      return index;
  return std::nullopt;
}

AsciiExecutionResult AsciiHostAdapter::Inspect(std::size_t binding, std::size_t stream,
                                               AsciiExecutionIdentity identity,
                                               const std::vector<std::uint8_t>& frame) {
  return backend_->Inspect(binding, stream, std::move(identity), frame);
}

AsciiExecutionResult AsciiHostAdapter::Encode(std::size_t binding,
                                              AsciiExecutionIdentity identity,
                                              const std::vector<AsciiInputField>& fields) {
  return backend_->Encode(binding, std::move(identity), fields);
}

AsciiStreamStepResult AsciiHostAdapter::Submit(std::size_t binding, std::size_t stream,
                                               AsciiExecutionIdentity identity,
                                               const std::vector<std::uint8_t>& chunk) {
  return backend_->Submit(binding, stream, std::move(identity), chunk);
}

AsciiStreamStepResult AsciiHostAdapter::Continue(std::size_t binding, std::size_t stream,
                                                 AsciiExecutionIdentity identity) {
  return backend_->Continue(binding, stream, std::move(identity));
}

bool AsciiHostAdapter::Reset(std::size_t binding, std::size_t stream) {
  return backend_->Reset(binding, stream);
}

std::optional<AsciiStreamObservation> AsciiHostAdapter::Observe(std::size_t binding,
                                                                std::size_t stream) const noexcept {
  return backend_->Observe(binding, stream);
}

bool AsciiHostAdapter::HasDiscardableState() const noexcept {
  return backend_->HasDiscardableState(bindings_);
}

}  // namespace pae::protocol_lab_ui
