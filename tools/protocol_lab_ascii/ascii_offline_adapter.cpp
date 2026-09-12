#include "ascii_offline_adapter.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace pae::protocol_lab::ascii {
namespace {

constexpr std::size_t kInvalidIndex = (std::numeric_limits<std::size_t>::max)();

template <typename Span>
std::string CopyResolved(const config_compiler::UiDescriptionSidecar& sidecar, Span span) {
  return std::string{sidecar.Resolve(span)};
}

bool CheckedAdd(std::size_t value, std::size_t& total) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)() - total) return false;
  total += value;
  return true;
}

bool Contains(const std::vector<std::size_t>& values, std::size_t value) noexcept {
  return std::find(values.begin(), values.end(), value) != values.end();
}

bool AllowedAscii(const protocol_plan::FrozenFieldPlan& field, std::uint8_t value) noexcept {
  const std::uint64_t mask = std::uint64_t{1U} << (value % 64U);
  return ((value < 64U ? field.allowed_ascii_low : field.allowed_ascii_high) & mask) != 0U;
}

bool BuildActionDescription(const protocol_plan::TextActionExecutionPlan& action,
                            const protocol_plan::FrozenMessagePlan& message,
                            ActionDescription& output, std::vector<bool>& referenced,
                            std::string& error) {
  ActionDescription built;
  built.min_record_length = action.min_record_length;
  built.max_record_length = action.max_record_length;
  built.segments.reserve(action.segments.size());
  for (const auto& source : action.segments) {
    SegmentDescription segment;
    if (source.kind == protocol_plan::TextSegmentKind::LITERAL) {
      if (source.literal.empty()) {
        error = "ASCII action contains an empty literal";
        return false;
      }
      segment.kind = SegmentKind::LITERAL;
      segment.literal.assign(source.literal.begin(), source.literal.end());
    } else if (source.kind == protocol_plan::TextSegmentKind::FIELD) {
      if (source.field_index >= message.fields.size() || referenced[source.field_index]) {
        error = "ASCII action field reference does not match the Plan";
        return false;
      }
      segment.kind = SegmentKind::FIELD;
      segment.field_index = source.field_index;
      segment.field_id = std::string{message.fields[source.field_index].id.View()};
      referenced[source.field_index] = true;
      built.referenced_field_indices.push_back(source.field_index);
    } else {
      error = "ASCII action contains an unknown segment kind";
      return false;
    }
    built.segments.push_back(std::move(segment));
  }
  if (built.segments.empty() || built.min_record_length > built.max_record_length ||
      built.max_record_length == 0U) {
    error = "ASCII action length bounds are invalid";
    return false;
  }
  output = std::move(built);
  return true;
}

bool BuildDescription(const protocol_plan::PlanBundle& plan,
                      const config_compiler::UiDescriptionSidecar& sidecar,
                      DocumentDescription& output, std::string& error) {
  error.clear();
  if (plan.SchemaVersion() != "0.10" || sidecar.empty()) {
    error = "ASCII adapter requires complete Schema 0.10 UI artifacts";
    return false;
  }
  const auto pipeline_metadata = sidecar.Pipelines();
  const auto message_metadata = sidecar.Messages();
  const auto field_metadata = sidecar.Fields();
  if (pipeline_metadata.size() != plan.Pipelines().size() ||
      message_metadata.size() != plan.Messages().size() ||
      plan.MessageExecutionPlans().size() != plan.Messages().size() ||
      plan.PipelineExecutionPlans().size() != plan.Pipelines().size()) {
    error = "Plan and UI description top-level counts differ";
    return false;
  }

  DocumentDescription built;
  built.schema_version = std::string{plan.SchemaVersion()};
  built.protocol_id = std::string{plan.ProtocolId()};
  built.protocol_version = std::string{plan.ProtocolVersion()};
  const auto& protocol_metadata = sidecar.Protocol();
  built.display_name = CopyResolved(sidecar, protocol_metadata.display_name);
  built.description = CopyResolved(sidecar, protocol_metadata.description);
  built.source_ref = CopyResolved(sidecar, protocol_metadata.source_ref);
  built.max_record_bytes = static_cast<std::size_t>(plan.GetResourceRequirements().max_frame_bytes);
  if (static_cast<std::uint64_t>(built.max_record_bytes) !=
          plan.GetResourceRequirements().max_frame_bytes ||
      built.max_record_bytes == 0U) {
    error = "Plan maximum record length is not representable";
    return false;
  }

  built.messages.reserve(plan.Messages().size());
  for (std::size_t message_index = 0U; message_index < plan.Messages().size(); ++message_index) {
    const auto& source = plan.Messages()[message_index];
    const auto& execution = plan.MessageExecutionPlans()[message_index];
    const auto& metadata = message_metadata[message_index];
    if (metadata.field_begin > field_metadata.size() ||
        metadata.field_count > field_metadata.size() - metadata.field_begin ||
        metadata.field_count != source.fields.size() ||
        execution.fields.size() != source.fields.size() ||
        (!execution.text_decode.has_value() && !execution.text_encode.has_value())) {
      error = "ASCII Message description does not match the Plan";
      return false;
    }

    MessageDescription message;
    message.message_index = message_index;
    message.id = std::string{source.id.View()};
    message.direction_id = std::string{source.direction_id.View()};
    message.display_name = CopyResolved(sidecar, metadata.display_name);
    message.description = CopyResolved(sidecar, metadata.description);
    message.source_ref = CopyResolved(sidecar, metadata.source_ref);
    message.max_record_length = execution.frame_size;
    std::vector<bool> decode_references(source.fields.size(), false);
    std::vector<bool> encode_references(source.fields.size(), false);
    if (execution.text_decode.has_value()) {
      ActionDescription action;
      if (!BuildActionDescription(*execution.text_decode, source, action, decode_references,
                                  error)) {
        return false;
      }
      message.decode = std::move(action);
    }
    if (execution.text_encode.has_value()) {
      ActionDescription action;
      if (!BuildActionDescription(*execution.text_encode, source, action, encode_references,
                                  error)) {
        return false;
      }
      message.encode = std::move(action);
    }

    message.fields.reserve(source.fields.size());
    for (std::size_t field_index = 0U; field_index < source.fields.size(); ++field_index) {
      const auto& field = source.fields[field_index];
      const auto& field_execution = execution.fields[field_index];
      const auto& metadata_field = field_metadata[metadata.field_begin + field_index];
      if (field.value_type != protocol_plan::ValueType::BYTES ||
          field.wire_codec != protocol_plan::WireCodec::ASCII_TEXT ||
          field_execution.value_type != protocol_plan::ValueType::BYTES ||
          field_execution.text_min_length > field_execution.text_max_length ||
          (!decode_references[field_index] && !encode_references[field_index])) {
        error = "ASCII field description does not match the Plan";
        return false;
      }
      FieldDescription copied;
      copied.field_index = field_index;
      copied.id = std::string{field.id.View()};
      copied.display_name = CopyResolved(sidecar, metadata_field.display_name);
      copied.description = CopyResolved(sidecar, metadata_field.description);
      copied.source_ref = CopyResolved(sidecar, metadata_field.source_ref);
      copied.min_byte_length = field_execution.text_min_length;
      copied.max_byte_length = field_execution.text_max_length;
      copied.decode_referenced = decode_references[field_index];
      copied.encode_referenced = encode_references[field_index];
      for (std::uint8_t value = 0U; value <= 0x1FU; ++value) {
        if (AllowedAscii(field, value)) copied.allowed_control_bytes.push_back(value);
      }
      if (AllowedAscii(field, 0x7FU)) copied.allowed_control_bytes.push_back(0x7FU);
      message.fields.push_back(std::move(copied));
    }
    built.messages.push_back(std::move(message));
  }

  built.pipelines.reserve(plan.Pipelines().size());
  for (std::size_t pipeline_index = 0U; pipeline_index < plan.Pipelines().size();
       ++pipeline_index) {
    const auto& source = plan.Pipelines()[pipeline_index];
    const auto& execution = plan.PipelineExecutionPlans()[pipeline_index];
    const auto& metadata = pipeline_metadata[pipeline_index];
    PipelineDescription pipeline;
    pipeline.pipeline_index = pipeline_index;
    pipeline.id = std::string{source.id.View()};
    pipeline.direction_id = std::string{source.direction_id.View()};
    pipeline.display_name = CopyResolved(sidecar, metadata.display_name);
    pipeline.description = CopyResolved(sidecar, metadata.description);
    pipeline.source_ref = CopyResolved(sidecar, metadata.source_ref);
    pipeline.message_indices.assign(source.message_indices.begin(), source.message_indices.end());
    pipeline.decode_message_indices.assign(execution.text_message_indices.begin(),
                                           execution.text_message_indices.end());
    for (const std::size_t message_index : pipeline.message_indices) {
      if (message_index >= built.messages.size()) {
        error = "Pipeline Message index is outside the ASCII description";
        return false;
      }
    }
    for (const std::size_t message_index : pipeline.decode_message_indices) {
      if (!Contains(pipeline.message_indices, message_index) ||
          !built.messages[message_index].decode.has_value()) {
        error = "Pipeline Decode candidate does not match the ASCII description";
        return false;
      }
    }
    built.pipelines.push_back(std::move(pipeline));
  }
  output = std::move(built);
  return true;
}

bool PipelineIdentityMatches(const DocumentDescription& description,
                             const ExecutionIdentity& identity) noexcept {
  return identity.pipeline_index < description.pipelines.size() &&
         description.pipelines[identity.pipeline_index].id == identity.pipeline_id;
}

bool EncodeSelectionMatches(const DocumentDescription& description,
                            const ExecutionIdentity& identity) noexcept {
  if (!PipelineIdentityMatches(description, identity) || !identity.message_index.has_value() ||
      !identity.message_id.has_value() || *identity.message_index >= description.messages.size()) {
    return false;
  }
  const auto& pipeline = description.pipelines[identity.pipeline_index];
  const auto& message = description.messages[*identity.message_index];
  return message.id == *identity.message_id &&
         Contains(pipeline.message_indices, *identity.message_index);
}

bool BorrowedRange(const std::vector<std::uint8_t>& input, protocol_core::ByteView value,
                   ByteRange& output) noexcept {
  if (input.empty()) return value.data == nullptr && value.size == 0U;
  const auto begin = reinterpret_cast<std::uintptr_t>(input.data());
  if (input.size() > (std::numeric_limits<std::uintptr_t>::max)() - begin) return false;
  const auto end = begin + input.size();
  const auto value_begin = reinterpret_cast<std::uintptr_t>(value.data);
  if (value_begin < begin || value_begin > end || value.size > end - value_begin) return false;
  output.offset = static_cast<std::size_t>(value_begin - begin);
  output.length = value.size;
  return true;
}

void MaterializationFailure(ExecutionResult& result, std::string detail) {
  result.status = AdapterStatus::MATERIALIZATION_FAILED;
  result.frame.clear();
  result.fields.clear();
  result.detail = std::move(detail);
}

}  // namespace

OfflineAdapter::OfflineAdapter(protocol_plan::PlanOwner plan,
                               config_compiler::UiDescriptionSidecar sidecar,
                               DocumentDescription description)
    : plan_(std::move(plan)),
      sidecar_(std::move(sidecar)),
      description_(std::move(description)),
      workspace_(*plan_) {}

OfflineAdapter::~OfflineAdapter() = default;

bool OfflineAdapter::Supports(const config_compiler::CompiledUiArtifacts& artifacts) noexcept {
  return artifacts.Plan() != nullptr && artifacts.Plan()->SchemaVersion() == "0.10";
}

std::unique_ptr<OfflineAdapter> OfflineAdapter::AdoptCompiledArtifacts(
    config_compiler::CompiledUiArtifacts artifacts, std::string& error) {
  if (!Supports(artifacts)) {
    error = "ASCII adapter requires Schema 0.10 artifacts";
    return nullptr;
  }
  auto plan = artifacts.TakePlan();
  auto sidecar = artifacts.TakeDescription();
  if (!plan) {
    error = "compiled UI artifacts have no Plan";
    return nullptr;
  }
  DocumentDescription description;
  if (!BuildDescription(*plan, sidecar, description, error)) return nullptr;
  return std::unique_ptr<OfflineAdapter>{
      new OfflineAdapter{std::move(plan), std::move(sidecar), std::move(description)}};
}

ExecutionResult OfflineAdapter::Inspect(ExecutionIdentity identity,
                                        const std::vector<std::uint8_t>& input_frame) {
  ExecutionResult result;
  result.identity = std::move(identity);
  result.operation = Operation::INSPECT;
  result.diagnostic_input_frame = input_frame;
  if (!PipelineIdentityMatches(description_, result.identity) ||
      result.identity.message_index.has_value() || result.identity.message_id.has_value()) {
    result.detail = "Inspect requires an exact Pipeline identity and no Message selection";
    return result;
  }

  const auto& pipeline = description_.pipelines[result.identity.pipeline_index];
  std::size_t slot_capacity = 0U;
  for (const std::size_t message_index : pipeline.decode_message_indices) {
    slot_capacity = (std::max)(slot_capacity, description_.messages[message_index].fields.size());
  }
  std::vector<protocol_core::DecodedFieldSlot> slots(slot_capacity);
  result.core_called = true;
  const auto decoded = protocol_core::DecodeCompleteRecord(
      *plan_, workspace_, result.identity.pipeline_index,
      protocol_core::ByteView{input_frame.data(), input_frame.size()}, slots.data(), slots.size());
  result.core_status = decoded.status;
  if (decoded.status != protocol_core::CodecStatus::OK) {
    result.status = AdapterStatus::CORE_FAILED;
    if (decoded.message_index != kInvalidIndex &&
        decoded.message_index < description_.messages.size()) {
      result.message_index = decoded.message_index;
      result.message_id = description_.messages[decoded.message_index].id;
      if (decoded.failed_field_index != kInvalidIndex &&
          decoded.failed_field_index < description_.messages[decoded.message_index].fields.size()) {
        result.failed_field_index = decoded.failed_field_index;
        result.failed_field_id =
            description_.messages[decoded.message_index].fields[decoded.failed_field_index].id;
      }
    }
    return result;
  }
  if (decoded.message_index >= description_.messages.size() ||
      !Contains(pipeline.decode_message_indices, decoded.message_index)) {
    MaterializationFailure(result, "Core Message identity is outside the selected Pipeline");
    return result;
  }
  const auto& message = description_.messages[decoded.message_index];
  if (!message.decode.has_value() || decoded.field_count != decoded.required_field_count ||
      decoded.field_count > slots.size()) {
    MaterializationFailure(result, "Core Decode field count does not match the ASCII description");
    return result;
  }
  result.message_index = decoded.message_index;
  result.message_id = message.id;
  result.fields.reserve(decoded.field_count);
  for (std::size_t slot_index = 0U; slot_index < decoded.field_count; ++slot_index) {
    const auto& slot = slots[slot_index];
    if (slot.value_kind != protocol_core::LogicalValueKind::BYTES ||
        slot.field.plan_scope != plan_.get() || slot.field.message_index != decoded.message_index ||
        slot.field.field_index >= message.fields.size()) {
      MaterializationFailure(result, "Core field identity does not match the ASCII description");
      return result;
    }
    ByteRange range;
    if (!BorrowedRange(input_frame, slot.bytes_value, range)) {
      MaterializationFailure(result, "Core field bytes are outside the Inspect input");
      return result;
    }
    FieldResult field;
    field.field_index = slot.field.field_index;
    field.field_id = message.fields[field.field_index].id;
    field.range = range;
    if (slot.bytes_value.size != 0U) {
      field.bytes.assign(slot.bytes_value.data, slot.bytes_value.data + slot.bytes_value.size);
    }
    result.fields.push_back(std::move(field));
  }
  result.frame = input_frame;
  result.diagnostic_input_frame.clear();
  result.status = AdapterStatus::OK;
  result.detail.clear();
  return result;
}

ExecutionResult OfflineAdapter::Encode(ExecutionIdentity identity,
                                       const std::vector<InputField>& inputs) {
  ExecutionResult result;
  result.identity = std::move(identity);
  result.operation = Operation::ENCODE;
  if (!EncodeSelectionMatches(description_, result.identity)) {
    result.detail = "Encode requires exact Pipeline and Message identities";
    return result;
  }
  const std::size_t message_index = *result.identity.message_index;
  const auto& message = description_.messages[message_index];
  if (!message.encode.has_value()) {
    result.core_called = true;
    const auto unsupported = protocol_core::EncodeCompleteRecord(
        *plan_, workspace_, result.identity.pipeline_index, message_index, nullptr, 0U, {});
    result.core_status = unsupported.status;
    result.status = unsupported.status == protocol_core::CodecStatus::OK
                        ? AdapterStatus::MATERIALIZATION_FAILED
                        : AdapterStatus::CORE_FAILED;
    return result;
  }
  result.review_kind = ReviewKind::TX_TEMPLATE;

  std::vector<protocol_core::EncodeFieldValue> core_inputs;
  core_inputs.reserve(inputs.size());
  for (const auto& input : inputs) {
    if (input.field_index >= message.fields.size() ||
        message.fields[input.field_index].id != input.field_id) {
      result.detail = "Encode input field identity does not match the ASCII description";
      return result;
    }
    protocol_core::EncodeFieldValue value;
    value.field = protocol_core::FieldRef{plan_.get(), message_index, input.field_index};
    value.value_kind = protocol_core::LogicalValueKind::BYTES;
    value.bytes_value = protocol_core::ByteView{input.bytes.data(), input.bytes.size()};
    core_inputs.push_back(value);
  }

  std::vector<std::uint8_t> output(message.max_record_length);
  result.core_called = true;
  const auto encoded = protocol_core::EncodeCompleteRecord(
      *plan_, workspace_, result.identity.pipeline_index, message_index, core_inputs.data(),
      core_inputs.size(), protocol_core::MutableByteBuffer{output.data(), output.size()});
  result.core_status = encoded.status;
  result.message_index = message_index;
  result.message_id = message.id;
  if (encoded.status != protocol_core::CodecStatus::OK) {
    result.status = AdapterStatus::CORE_FAILED;
    if (encoded.failed_value_index != kInvalidIndex && encoded.failed_value_index < inputs.size()) {
      result.failed_input_index = encoded.failed_value_index;
    }
    if (encoded.failed_field_index != kInvalidIndex &&
        encoded.failed_field_index < message.fields.size()) {
      result.failed_field_index = encoded.failed_field_index;
      result.failed_field_id = message.fields[encoded.failed_field_index].id;
    }
    return result;
  }
  if (encoded.bytes_written > output.size()) {
    MaterializationFailure(result, "Core Encode length exceeds the owned output buffer");
    return result;
  }
  output.resize(encoded.bytes_written);

  std::size_t cursor = 0U;
  for (const auto& segment : message.encode->segments) {
    if (segment.kind == SegmentKind::LITERAL) {
      if (!CheckedAdd(segment.literal.size(), cursor)) {
        MaterializationFailure(result, "Encode display range arithmetic overflowed");
        return result;
      }
      continue;
    }
    if (!segment.field_index.has_value() || !segment.field_id.has_value()) {
      MaterializationFailure(result, "Encode field segment has no identity");
      return result;
    }
    const auto found = std::find_if(inputs.begin(), inputs.end(), [&](const InputField& input) {
      return input.field_index == *segment.field_index && input.field_id == *segment.field_id;
    });
    if (found == inputs.end() || cursor > output.size() ||
        found->bytes.size() > output.size() - cursor) {
      MaterializationFailure(result, "Encode field range does not match the successful output");
      return result;
    }
    FieldResult field;
    field.field_index = found->field_index;
    field.field_id = found->field_id;
    field.bytes = found->bytes;
    field.range = ByteRange{cursor, found->bytes.size()};
    result.fields.push_back(std::move(field));
    if (!CheckedAdd(found->bytes.size(), cursor)) {
      MaterializationFailure(result, "Encode display range arithmetic overflowed");
      return result;
    }
  }
  if (cursor != output.size()) {
    MaterializationFailure(result, "Encode display ranges do not consume the successful output");
    return result;
  }
  result.frame = std::move(output);
  result.status = AdapterStatus::OK;
  result.detail.clear();
  return result;
}

std::string_view CodecStatusName(protocol_core::CodecStatus status) noexcept {
  using protocol_core::CodecStatus;
  switch (status) {
    case CodecStatus::OK:
      return "OK";
    case CodecStatus::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case CodecStatus::INVALID_PLAN:
      return "INVALID_PLAN";
    case CodecStatus::WORKSPACE_PLAN_MISMATCH:
      return "WORKSPACE_PLAN_MISMATCH";
    case CodecStatus::WORKSPACE_BUSY:
      return "WORKSPACE_BUSY";
    case CodecStatus::UNKNOWN_MESSAGE:
      return "UNKNOWN_MESSAGE";
    case CodecStatus::AMBIGUOUS_MESSAGE:
      return "AMBIGUOUS_MESSAGE";
    case CodecStatus::OUTPUT_SLOTS_TOO_SMALL:
      return "OUTPUT_SLOTS_TOO_SMALL";
    case CodecStatus::INTEGRITY_FAILED:
      return "INTEGRITY_FAILED";
    case CodecStatus::MESSAGE_NOT_ALLOWED:
      return "MESSAGE_NOT_ALLOWED";
    case CodecStatus::FIELD_REFERENCE_MISMATCH:
      return "FIELD_REFERENCE_MISMATCH";
    case CodecStatus::DUPLICATE_FIELD:
      return "DUPLICATE_FIELD";
    case CodecStatus::MISSING_FIELD:
      return "MISSING_FIELD";
    case CodecStatus::TYPE_MISMATCH:
      return "TYPE_MISMATCH";
    case CodecStatus::VALUE_NOT_REPRESENTABLE:
      return "VALUE_NOT_REPRESENTABLE";
    case CodecStatus::BYTES_LENGTH_MISMATCH:
      return "BYTES_LENGTH_MISMATCH";
    case CodecStatus::UNKNOWN_ENUM_VALUE:
      return "UNKNOWN_ENUM_VALUE";
    case CodecStatus::ENUM_REFERENCE_MISMATCH:
      return "ENUM_REFERENCE_MISMATCH";
    case CodecStatus::CONSTANT_FIELD_OVERRIDE:
      return "CONSTANT_FIELD_OVERRIDE";
    case CodecStatus::INPUT_OUTPUT_OVERLAP:
      return "INPUT_OUTPUT_OVERLAP";
    case CodecStatus::BUFFER_TOO_SMALL:
      return "BUFFER_TOO_SMALL";
    case CodecStatus::FINAL_REVIEW_FAILED:
      return "FINAL_REVIEW_FAILED";
    case CodecStatus::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
    case CodecStatus::ASCII_CHARACTER_NOT_ALLOWED:
      return "ASCII_CHARACTER_NOT_ALLOWED";
    case CodecStatus::ASCII_TERMINATOR_CONFLICT:
      return "ASCII_TERMINATOR_CONFLICT";
    case CodecStatus::OPERATION_NOT_SUPPORTED:
      return "OPERATION_NOT_SUPPORTED";
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    case CodecStatus::COMPUTED_FIELD_OVERRIDE:
      return "COMPUTED_FIELD_OVERRIDE";
    case CodecStatus::LENGTH_MISMATCH:
      return "LENGTH_MISMATCH";
#endif
  }
  return "UNKNOWN_CODEC_STATUS";
}

}  // namespace pae::protocol_lab::ascii
