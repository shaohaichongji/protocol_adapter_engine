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
  const bool supported_schema = plan.SchemaVersion() == "0.10"
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
                                || plan.SchemaVersion() == "0.11"
#endif
      ;
  if (!supported_schema || sidecar.empty()) {
    error = "ASCII adapter requires complete supported ASCII UI artifacts";
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
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    if (source.framing_profile_index >= plan.FramingProfiles().size()) {
      error = "Pipeline framing profile index is outside the Plan";
      return false;
    }
    const auto& framing = plan.FramingProfiles()[source.framing_profile_index];
    pipeline.stream_ascii_crlf = framing.input_kind == protocol_plan::InputKind::STREAM_CHUNK &&
                                 framing.strategy == protocol_plan::FramingStrategy::ASCII_CRLF;
    if (pipeline.stream_ascii_crlf) {
      pipeline.maximum_frame_length = static_cast<std::size_t>(framing.maximum_frame_length);
      if (static_cast<std::uint64_t>(pipeline.maximum_frame_length) !=
              framing.maximum_frame_length ||
          pipeline.maximum_frame_length == 0U) {
        error = "ASCII stream maximum frame length is not representable";
        return false;
      }
    }
#endif
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

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
struct OfflineAdapter::StreamContext {
  std::unique_ptr<protocol_framing::StreamFramingWorkspace> workspace;
  std::vector<std::uint8_t> frozen_input;
  std::size_t cursor = 0U;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::size_t discarded_baseline = 0U;
  std::size_t malformed_baseline = 0U;
  unsigned int consecutive_no_progress = 0U;
#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
  std::size_t candidate_copy_limit = (std::numeric_limits<std::size_t>::max)();
#endif
};
#endif

OfflineAdapter::OfflineAdapter(protocol_plan::PlanOwner plan,
                               config_compiler::UiDescriptionSidecar sidecar,
                               DocumentDescription description)
    : plan_(std::move(plan)),
      sidecar_(std::move(sidecar)),
      description_(std::move(description)),
      workspace_(*plan_)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
      ,
      streams_(description_.pipelines.size())
#endif
{
}

OfflineAdapter::~OfflineAdapter() = default;

bool OfflineAdapter::Supports(const config_compiler::CompiledUiArtifacts& artifacts) noexcept {
  if (artifacts.Plan() == nullptr) return false;
  if (artifacts.Plan()->SchemaVersion() == "0.10") return true;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  return artifacts.Plan()->SchemaVersion() == "0.11";
#else
  return false;
#endif
}

std::unique_ptr<OfflineAdapter> OfflineAdapter::AdoptCompiledArtifacts(
    config_compiler::CompiledUiArtifacts artifacts, std::string& error
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
    ,
    const protocol_framing::FramingLimitOverrides& stream_overrides
#endif
) {
  if (!Supports(artifacts)) {
    error = "ASCII adapter requires supported ASCII artifacts";
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
  auto adapter = std::unique_ptr<OfflineAdapter>{
      new OfflineAdapter{std::move(plan), std::move(sidecar), std::move(description)}};
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  for (std::size_t index = 0U; index < adapter->description_.pipelines.size(); ++index) {
    if (!adapter->description_.pipelines[index].stream_ascii_crlf) continue;
    auto created =
        protocol_framing::CreateStreamFramingWorkspace(*adapter->plan_, index, stream_overrides);
    if (created.api_status != protocol_framing::SubmitApiStatus::OK || !created.workspace) {
      error = "failed to create ASCII stream framing workspace; api_status=" +
              std::to_string(static_cast<int>(created.api_status)) +
              "; accounted=" + std::to_string(created.accounted_workspace_bytes) +
              "; session_limit=" + std::to_string(created.effective_session_limit_bytes);
      return nullptr;
    }
    auto context = std::make_unique<StreamContext>();
    context->workspace = std::move(created.workspace);
    adapter->streams_[index] = std::move(context);
  }
#endif
  return adapter;
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

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
std::optional<StreamObservation> OfflineAdapter::ObserveStream(
    std::size_t pipeline_index) const noexcept {
  if (pipeline_index >= streams_.size() || !streams_[pipeline_index] ||
      !streams_[pipeline_index]->workspace) {
    return std::nullopt;
  }
  const auto& context = *streams_[pipeline_index];
  const auto observed = context.workspace->Observe();
  StreamObservation result;
  result.phase = observed.phase;
  result.buffered_bytes = observed.buffered_bytes;
  result.has_internal_work = observed.has_internal_work;
  result.effective_max_submit_bytes = observed.effective_max_submit_bytes;
  result.effective_max_work_units = observed.effective_max_work_units;
  result.frozen_input_bytes = context.frozen_input.size();
  result.frozen_cursor = context.cursor;
  result.reset_required = context.reset_required;
  result.generation = context.generation;
  result.step_sequence = context.step_sequence;
  result.total_candidates = context.total_candidates;
  result.total_decode_successes = context.total_decode_successes;
  result.total_discarded_bytes =
      context.workspace->TotalDiscardedBytes() - context.discarded_baseline;
  result.total_malformed_candidates =
      context.workspace->TotalMalformedCandidates() - context.malformed_baseline;
  return result;
}

std::size_t OfflineAdapter::StreamChunkCapacity(std::size_t pipeline_index) const noexcept {
  const auto observation = ObserveStream(pipeline_index);
  if (!observation.has_value()) return 0U;
  return (std::min)(std::size_t{64U * 1024U}, observation->effective_max_submit_bytes);
}

bool OfflineAdapter::StreamHasDiscardableState(std::size_t pipeline_index) const noexcept {
  const auto observation = ObserveStream(pipeline_index);
  if (!observation.has_value()) return false;
  return observation->buffered_bytes != 0U || observation->has_internal_work ||
         observation->frozen_cursor < observation->frozen_input_bytes ||
         observation->phase != protocol_framing::StreamFramingPhase::COLLECTING ||
         observation->reset_required;
}

#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
void OfflineAdapter::SetCandidateCopyLimitForTesting(std::size_t pipeline_index,
                                                     std::size_t maximum_bytes) noexcept {
  if (pipeline_index < streams_.size() && streams_[pipeline_index]) {
    streams_[pipeline_index]->candidate_copy_limit = maximum_bytes;
  }
}
#endif

namespace {
bool CheckedIncrement(std::uint64_t& value) noexcept {
  if (value == (std::numeric_limits<std::uint64_t>::max)()) return false;
  ++value;
  return true;
}
}  // namespace

StreamStepResult OfflineAdapter::SubmitStreamChunk(ExecutionIdentity identity,
                                                   const std::vector<std::uint8_t>& input_chunk) {
  StreamStepResult invalid;
  invalid.identity = identity;
  if (!PipelineIdentityMatches(description_, identity) || identity.message_index.has_value() ||
      identity.message_id.has_value() || identity.pipeline_index >= streams_.size() ||
      !streams_[identity.pipeline_index]) {
    invalid.detail = "Stream Submit requires an exact ASCII CRLF Pipeline identity";
    return invalid;
  }
  auto& context = *streams_[identity.pipeline_index];
  invalid.before = *ObserveStream(identity.pipeline_index);
  invalid.after = invalid.before;
  if (context.reset_required) {
    invalid.detail = "Stream workspace requires Reset";
    return invalid;
  }
  if (context.cursor < context.frozen_input.size() || invalid.before.has_internal_work) {
    invalid.detail = "Continue is required before a new stream chunk";
    return invalid;
  }
  const std::size_t capacity = StreamChunkCapacity(identity.pipeline_index);
  if (input_chunk.empty() || input_chunk.size() > capacity) {
    invalid.detail = "Stream chunk is empty or exceeds the effective Submit capacity";
    return invalid;
  }
  context.frozen_input = input_chunk;
  context.cursor = 0U;
  return ContinueStream(std::move(identity));
}

StreamStepResult OfflineAdapter::ContinueStream(ExecutionIdentity identity) {
  StreamStepResult result;
  result.identity = identity;
  if (!PipelineIdentityMatches(description_, identity) || identity.message_index.has_value() ||
      identity.message_id.has_value() || identity.pipeline_index >= streams_.size() ||
      !streams_[identity.pipeline_index]) {
    result.detail = "Stream Continue requires an exact ASCII CRLF Pipeline identity";
    return result;
  }
  auto& context = *streams_[identity.pipeline_index];
  result.before = *ObserveStream(identity.pipeline_index);
  result.after = result.before;
  if (context.reset_required) {
    result.detail = "Stream workspace requires Reset";
    return result;
  }
  const bool has_suffix = context.cursor < context.frozen_input.size();
  if (!has_suffix && !result.before.has_internal_work) {
    result.detail = "Stream Continue has no frozen suffix or internal work";
    return result;
  }

  struct CandidateCapture {
    std::vector<std::uint8_t> bytes;
    bool copy_failed = false;
    std::size_t calls = 0U;
#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
    std::size_t copy_limit = (std::numeric_limits<std::size_t>::max)();
#endif
  } capture;
#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
  capture.copy_limit = context.candidate_copy_limit;
#endif
  const auto sink = [](protocol_framing::ByteView frame,
                       void* user_data) noexcept -> protocol_framing::FrameSinkAction {
    auto& state = *static_cast<CandidateCapture*>(user_data);
    ++state.calls;
#if defined(PAE_PROTOCOL_LAB_ASCII_TEST_HOOKS)
    if (frame.size > state.copy_limit) {
      state.copy_failed = true;
      return protocol_framing::FrameSinkAction::STOP;
    }
#endif
    try {
      state.bytes.assign(frame.data, frame.data + frame.size);
    } catch (...) {
      state.copy_failed = true;
    }
    return protocol_framing::FrameSinkAction::STOP;
  };
  const std::uint8_t* input = has_suffix ? context.frozen_input.data() + context.cursor : nullptr;
  const std::size_t input_size = has_suffix ? context.frozen_input.size() - context.cursor : 0U;
  result.push_called = true;
  result.framing = protocol_framing::PushStreamChunk(
      *plan_, *context.workspace, identity.pipeline_index,
      protocol_framing::ByteView{input, input_size}, protocol_framing::FrameSink{sink, &capture});
  if (!CheckedIncrement(context.step_sequence)) {
    context.reset_required = true;
    result.status = AdapterStatus::MATERIALIZATION_FAILED;
    result.detail = "Stream step counter overflowed";
    result.after = *ObserveStream(identity.pipeline_index);
    return result;
  }
  if (result.framing.bytes_consumed > input_size || result.framing.frames_delivered > 1U ||
      capture.calls > 1U || capture.calls != result.framing.frames_delivered) {
    context.reset_required = true;
    result.status = AdapterStatus::MATERIALIZATION_FAILED;
    result.detail = "Framing step facts violate the observer contract";
    result.after = *ObserveStream(identity.pipeline_index);
    return result;
  }
  context.cursor += result.framing.bytes_consumed;
  if (context.cursor == context.frozen_input.size()) {
    context.frozen_input.clear();
    context.cursor = 0U;
  }
  if (result.framing.api_status != protocol_framing::SubmitApiStatus::OK) {
    context.reset_required = true;
    result.status = AdapterStatus::CORE_FAILED;
    result.detail = "Stream framing API failed";
  } else if (capture.copy_failed) {
    if (!CheckedIncrement(context.total_candidates)) {
      result.detail = "Stream candidate counter overflowed after copy failure";
    } else {
      result.detail = "Candidate materialization failed; Reset is required";
    }
    context.reset_required = true;
    result.status = AdapterStatus::MATERIALIZATION_FAILED;
  } else {
    if (capture.calls == 1U) {
      if (!CheckedIncrement(context.total_candidates)) {
        context.reset_required = true;
        result.status = AdapterStatus::MATERIALIZATION_FAILED;
        result.detail = "Stream candidate counter overflowed";
      } else {
        auto candidate_identity = identity;
        result.candidate = Inspect(std::move(candidate_identity), capture.bytes);
        if (result.candidate->status == AdapterStatus::OK &&
            !CheckedIncrement(context.total_decode_successes)) {
          context.reset_required = true;
          result.status = AdapterStatus::MATERIALIZATION_FAILED;
          result.detail = "Stream Decode success counter overflowed";
        } else {
          result.status = AdapterStatus::OK;
        }
      }
    } else {
      result.status = AdapterStatus::OK;
    }
  }

  const auto raw_after = context.workspace->Observe();
  const bool progressed =
      result.framing.bytes_consumed != 0U || result.framing.frames_delivered != 0U ||
      result.framing.bytes_discarded != 0U || result.framing.malformed_candidates != 0U ||
      result.framing.work_units_used != 0U || raw_after.phase != result.before.phase ||
      raw_after.buffered_bytes != result.before.buffered_bytes ||
      raw_after.has_internal_work != result.before.has_internal_work;
  context.consecutive_no_progress = progressed ? 0U : context.consecutive_no_progress + 1U;
  if (context.consecutive_no_progress >= 2U) {
    context.reset_required = true;
    result.status = AdapterStatus::MATERIALIZATION_FAILED;
    result.detail = "Persistent no-progress stream state; Reset is required";
  }
  result.after = *ObserveStream(identity.pipeline_index);
  return result;
}

bool OfflineAdapter::ResetStream(std::size_t pipeline_index, std::string_view pipeline_id,
                                 std::string& error) noexcept {
  error.clear();
  if (pipeline_index >= streams_.size() || !streams_[pipeline_index] ||
      description_.pipelines[pipeline_index].id != pipeline_id) {
    error = "Reset requires an exact ASCII CRLF Pipeline identity";
    return false;
  }
  auto& context = *streams_[pipeline_index];
  const auto status =
      protocol_framing::ResetStreamFramingWorkspace(*plan_, *context.workspace, pipeline_index);
  if (status != protocol_framing::SubmitApiStatus::OK) {
    error = "Stream framing Reset failed";
    return false;
  }
  context.frozen_input.clear();
  context.cursor = 0U;
  context.reset_required = false;
  context.step_sequence = 0U;
  context.total_candidates = 0U;
  context.total_decode_successes = 0U;
  context.discarded_baseline = context.workspace->TotalDiscardedBytes();
  context.malformed_baseline = context.workspace->TotalMalformedCandidates();
  context.consecutive_no_progress = 0U;
  if (!CheckedIncrement(context.generation)) {
    context.reset_required = true;
    error = "Stream generation counter overflowed";
    return false;
  }
  return true;
}
#endif

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
