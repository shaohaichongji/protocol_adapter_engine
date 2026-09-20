#include "ascii_host_adapter_compat.h"

#include <utility>

namespace pae::protocol_lab_ui {
namespace {

bool BuildPrivateDescription(const protocol_lab::ascii::DocumentDescription& source,
                             DocumentDescription& output, std::string& error) {
  DocumentDescription built;
  built.layout = DocumentLayout::ASCII_TEXT;
  built.schema_version = source.schema_version;
  built.protocol_id = source.protocol_id;
  built.protocol_version = source.protocol_version;
  built.display_name = source.display_name;
  built.description = source.description;
  built.source_ref = source.source_ref;
  built.max_frame_bytes = source.max_record_bytes;
  built.pipelines.reserve(source.pipelines.size());
  for (const auto& value : source.pipelines) {
    if (value.pipeline_index != built.pipelines.size()) {
      error = "ASCII Pipeline indices are not contiguous";
      return false;
    }
    PipelineDescriptor pipeline;
    pipeline.pipeline_index = value.pipeline_index;
    pipeline.id = value.id;
    pipeline.direction_id = value.direction_id;
    pipeline.display_name = value.display_name;
    pipeline.description = value.description;
    pipeline.source_ref = value.source_ref;
    pipeline.message_indices = value.message_indices;
    pipeline.decode_message_indices = value.decode_message_indices;
    pipeline.stream_ascii_crlf = value.stream_ascii_crlf;
    pipeline.maximum_frame_length = value.maximum_frame_length;
    built.pipelines.push_back(std::move(pipeline));
  }
  built.messages.reserve(source.messages.size());
  for (const auto& value : source.messages) {
    if (value.message_index != built.messages.size()) {
      error = "ASCII Message indices are not contiguous";
      return false;
    }
    MessageDescriptor message;
    message.message_index = value.message_index;
    message.id = value.id;
    message.direction_id = value.direction_id;
    message.display_name = value.display_name;
    message.description = value.description;
    message.source_ref = value.source_ref;
    message.frame_size = value.max_record_length;
    message.encode_available = value.encode.has_value();
    message.decode_available = value.decode.has_value();
    message.fields.reserve(value.fields.size());
    for (const auto& field_value : value.fields) {
      if (field_value.field_index != message.fields.size()) {
        error = "ASCII Field indices are not contiguous";
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
      field.byte_width = field_value.max_byte_length;
      field.byte_length_bounds =
          ByteLengthBounds{field_value.min_byte_length, field_value.max_byte_length};
      field.ascii_text = true;
      field.decode_referenced = field_value.decode_referenced;
      field.encode_referenced = field_value.encode_referenced;
      field.allowed_control_bytes = field_value.allowed_control_bytes;
      if (!field.encode_referenced) field.read_only_annotation = "not referenced by Encode action";
      message.fields.push_back(std::move(field));
    }
    built.messages.push_back(std::move(message));
  }
  output = std::move(built);
  error.clear();
  return true;
}

class PrivateBackend final : public AsciiHostBackend {
 public:
  explicit PrivateBackend(std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> adapter)
      : adapter_(std::move(adapter)) {}

  std::size_t AccountedBytes() const noexcept override { return adapter_->AccountedBytes(); }

  AsciiExecutionResult Inspect(std::size_t binding, std::size_t stream,
                               AsciiExecutionIdentity identity,
                               const std::vector<std::uint8_t>& frame) override {
    return ConvertPrivateAsciiResult(
        adapter_->Inspect(binding, stream, ToPrivateAsciiIdentity(std::move(identity)), frame));
  }

  AsciiExecutionResult Encode(std::size_t binding, AsciiExecutionIdentity identity,
                              const std::vector<AsciiInputField>& fields) override {
    return ConvertPrivateAsciiResult(adapter_->Encode(
        binding, ToPrivateAsciiIdentity(std::move(identity)), ToPrivateAsciiInputs(fields)));
  }

  AsciiStreamStepResult Submit(std::size_t binding, std::size_t stream,
                               AsciiExecutionIdentity identity,
                               const std::vector<std::uint8_t>& chunk) override {
    return ConvertPrivateAsciiStreamStep(
        adapter_->Submit(binding, stream, ToPrivateAsciiIdentity(std::move(identity)), chunk));
  }

  AsciiStreamStepResult Continue(std::size_t binding, std::size_t stream,
                                 AsciiExecutionIdentity identity) override {
    return ConvertPrivateAsciiStreamStep(
        adapter_->Continue(binding, stream, ToPrivateAsciiIdentity(std::move(identity))));
  }

  bool Reset(std::size_t binding, std::size_t stream) override {
    return adapter_->Reset(binding, stream);
  }

  std::optional<AsciiStreamObservation> Observe(std::size_t binding,
                                                std::size_t stream) const noexcept override {
    const auto observed = adapter_->Observe(binding, stream);
    return observed ? std::optional<AsciiStreamObservation>{ConvertPrivateAsciiObservation(*observed)}
                    : std::nullopt;
  }

  bool HasDiscardableState(const std::vector<AsciiHostBinding>&) const noexcept override {
    return adapter_->HasDiscardableState();
  }

 private:
  std::unique_ptr<protocol_lab::ascii::HostObserverAdapter> adapter_;
};

}  // namespace

std::unique_ptr<AsciiHostAdapter> CreatePrivateAsciiHostAdapter(
    config_compiler::CompiledProtocolArtifacts artifacts, std::vector<AsciiHostBinding> bindings,
    std::string& error) {
  std::vector<protocol_lab::ascii::HostBinding> private_bindings;
  private_bindings.reserve(bindings.size());
  for (const auto& binding : bindings)
    private_bindings.push_back({binding.endpoint,
                                binding.action == AsciiHostAction::DECODE
                                    ? host_endpoint::Action::DECODE
                                    : host_endpoint::Action::ENCODE,
                                binding.pipeline_index});
  auto adapter = protocol_lab::ascii::HostObserverAdapter::Create(
      std::move(artifacts), std::move(private_bindings), error);
  if (!adapter) return nullptr;
  DocumentDescription description;
  if (!BuildPrivateDescription(adapter->Description(), description, error)) return nullptr;
  return AsciiHostAdapter::AdoptBackend(std::make_unique<PrivateBackend>(std::move(adapter)),
                                        std::move(description), std::move(bindings), false, false);
}

}  // namespace pae::protocol_lab_ui
