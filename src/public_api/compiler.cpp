#include "pae/compiler.h"

#include <array>
#include <limits>
#include <utility>

#include "../config_compiler/config_compiler.h"
#include "compiled_state_internal.h"

namespace pae {
namespace {

namespace internal = config_compiler;
namespace plan = protocol_plan;

CompileStage MapStage(internal::CompileStage stage) noexcept {
  switch (stage) {
    case internal::CompileStage::INPUT_PROFILE:
      return CompileStage::INPUT_PROFILE;
    case internal::CompileStage::JSON_SYNTAX:
      return CompileStage::JSON_SYNTAX;
    case internal::CompileStage::JSON_RESOURCE:
      return CompileStage::JSON_RESOURCE;
    case internal::CompileStage::STRUCTURAL:
      return CompileStage::STRUCTURAL;
    case internal::CompileStage::DOMAIN_VALIDATION:
      return CompileStage::DOMAIN_VALIDATION;
    case internal::CompileStage::RESOURCE_BUDGET:
      return CompileStage::RESOURCE_BUDGET;
    case internal::CompileStage::PLAN_BUILD:
      return CompileStage::PLAN_BUILD;
    case internal::CompileStage::INTERNAL:
      return CompileStage::INTERNAL;
  }
  return CompileStage::INTERNAL;
}

CompileError MapError(internal::CompileError error) noexcept {
#define PAE_MAP_COMPILE_ERROR(name)  \
  case internal::CompileError::name: \
    return CompileError::name
  switch (error) {
    PAE_MAP_COMPILE_ERROR(NONE);
    PAE_MAP_COMPILE_ERROR(EMPTY_INPUT);
    PAE_MAP_COMPILE_ERROR(INPUT_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(UTF8_BOM_NOT_ALLOWED);
    PAE_MAP_COMPILE_ERROR(INVALID_UTF8);
    PAE_MAP_COMPILE_ERROR(INVALID_UNICODE_ESCAPE);
    PAE_MAP_COMPILE_ERROR(NESTING_DEPTH_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(JSON_SYNTAX_ERROR);
    PAE_MAP_COMPILE_ERROR(JSON_ALLOCATION_FAILED);
    PAE_MAP_COMPILE_ERROR(JSON_PARSER_MEMORY_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(JSON_NODE_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(OBJECT_MEMBER_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(ARRAY_ELEMENT_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(STRING_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(DECODED_STRING_BUDGET_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(NUMBER_TOKEN_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(DUPLICATE_KEY);
    PAE_MAP_COMPILE_ERROR(ROOT_MUST_BE_OBJECT);
    PAE_MAP_COMPILE_ERROR(MISSING_PROPERTY);
    PAE_MAP_COMPILE_ERROR(UNKNOWN_PROPERTY);
    PAE_MAP_COMPILE_ERROR(TYPE_MISMATCH);
    PAE_MAP_COMPILE_ERROR(INVALID_STRING_LENGTH);
    PAE_MAP_COMPILE_ERROR(INVALID_ID);
    PAE_MAP_COMPILE_ERROR(INVALID_ENUM_VALUE);
    PAE_MAP_COMPILE_ERROR(INTEGER_NOT_EXACT);
    PAE_MAP_COMPILE_ERROR(INTEGER_OUT_OF_RANGE);
    PAE_MAP_COMPILE_ERROR(INVALID_HEX_BYTES);
    PAE_MAP_COMPILE_ERROR(EMPTY_ARRAY);
    PAE_MAP_COMPILE_ERROR(UNSUPPORTED_FEATURE);
    PAE_MAP_COMPILE_ERROR(DUPLICATE_ID);
    PAE_MAP_COMPILE_ERROR(DUPLICATE_REFERENCE);
    PAE_MAP_COMPILE_ERROR(UNKNOWN_REFERENCE);
    PAE_MAP_COMPILE_ERROR(DIRECTION_MISMATCH);
    PAE_MAP_COMPILE_ERROR(FIELD_OUT_OF_BOUNDS);
    PAE_MAP_COMPILE_ERROR(FIELD_OVERLAP);
    PAE_MAP_COMPILE_ERROR(FRAME_NOT_FULLY_DEFINED);
    PAE_MAP_COMPILE_ERROR(VALUE_NOT_REPRESENTABLE);
    PAE_MAP_COMPILE_ERROR(MATCHER_OUT_OF_BOUNDS);
    PAE_MAP_COMPILE_ERROR(MATCHER_CONFLICT);
    PAE_MAP_COMPILE_ERROR(INTEGRITY_RANGE_OUT_OF_BOUNDS);
    PAE_MAP_COMPILE_ERROR(INTEGRITY_STORAGE_OUT_OF_BOUNDS);
    PAE_MAP_COMPILE_ERROR(INTEGRITY_SELF_INCLUDED);
    PAE_MAP_COMPILE_ERROR(INTEGRITY_STORAGE_CONFLICT);
    PAE_MAP_COMPILE_ERROR(AMBIGUOUS_MATCHER);
    PAE_MAP_COMPILE_ERROR(RESOURCE_LIMIT_EXCEEDED);
    PAE_MAP_COMPILE_ERROR(COMPILER_ALLOCATION_FAILED);
    PAE_MAP_COMPILE_ERROR(INTERNAL_CONTRACT_VIOLATION);
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    PAE_MAP_COMPILE_ERROR(ASCII_LITERAL_INVALID);
    PAE_MAP_COMPILE_ERROR(ASCII_CONTROL_BYTE_INVALID);
    PAE_MAP_COMPILE_ERROR(ASCII_FIELD_LENGTH_INVALID);
    PAE_MAP_COMPILE_ERROR(ASCII_FIELD_REFERENCE_INVALID);
    PAE_MAP_COMPILE_ERROR(ASCII_FIELD_BOUNDARY_AMBIGUOUS);
    PAE_MAP_COMPILE_ERROR(ASCII_TEMPLATE_EMPTY);
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    PAE_MAP_COMPILE_ERROR(ASCII_STREAM_TERMINATOR_INVALID);
    PAE_MAP_COMPILE_ERROR(ASCII_STREAM_BOUNDARY_UNPROVEN);
    PAE_MAP_COMPILE_ERROR(ASCII_STREAM_PROFILE_MISMATCH);
#endif
#endif
  }
#undef PAE_MAP_COMPILE_ERROR
  return CompileError::INTERNAL_CONTRACT_VIOLATION;
}

ResourceKind MapResourceKind(internal::ResourceKind kind) noexcept {
  switch (kind) {
    case internal::ResourceKind::NONE:
      return ResourceKind::NONE;
    case internal::ResourceKind::PLAN_ACCOUNTED_MEMORY:
      return ResourceKind::PLAN_ACCOUNTED_MEMORY;
    case internal::ResourceKind::UI_DESCRIPTION_ACCOUNTED_MEMORY:
      return ResourceKind::METADATA_ACCOUNTED_MEMORY;
  }
  return ResourceKind::NONE;
}

ResourceProfile MapResourceProfile(plan::ResourceProfile profile) noexcept {
  switch (profile) {
    case plan::ResourceProfile::DESKTOP:
      return ResourceProfile::DESKTOP;
    case plan::ResourceProfile::CONSTRAINED:
      return ResourceProfile::CONSTRAINED;
  }
  return ResourceProfile::DESKTOP;
}

CompileDiagnostic MapDiagnostic(const internal::CompileDiagnostic& source) {
  CompileDiagnostic output;
  output.stage = MapStage(source.stage);
  output.code = MapError(source.code);
  output.json_pointer = source.json_pointer;
  output.byte_offset = source.byte_offset;
  output.detail = source.detail;
  output.resource_kind = MapResourceKind(source.resource_kind);
  output.required_bytes = source.required_bytes;
  output.limit_bytes = source.limit_bytes;
  output.resource_profile = MapResourceProfile(source.resource_profile);
  return output;
}

std::string_view Resolve(const internal::UiDescriptionSidecar& metadata,
                         internal::DescriptionStringSpan span) noexcept {
  return metadata.Resolve(span);
}

std::optional<ValueKind> MapValueKind(const plan::FieldExecutionPlan& field) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (field.conversion_index != static_cast<std::size_t>(-1)) {
    return ValueKind::DECIMAL64;
  }
#endif
  switch (field.value_type) {
    case plan::ValueType::UINT64:
      return ValueKind::UINT64;
    case plan::ValueType::INT64:
      return ValueKind::INT64;
    case plan::ValueType::BYTES:
      return ValueKind::BYTES;
    case plan::ValueType::ENUM:
      return ValueKind::ENUM;
    case plan::ValueType::BOOL:
      return ValueKind::BOOL;
  }
  return std::nullopt;
}

std::optional<EncodeValueSource> MapEncodeValueSource(
    const plan::MessageExecutionPlan& message, const plan::FieldExecutionPlan& field) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  if (message.text_decode.has_value() || message.text_encode.has_value()) {
    return message.text_encode.has_value() && field.text_encode_input
               ? EncodeValueSource::CALLER_INPUT
               : EncodeValueSource::NOT_REFERENCED;
  }
#endif
  switch (field.encode_source) {
    case plan::EncodeSource::INPUT:
      return EncodeValueSource::CALLER_INPUT;
    case plan::EncodeSource::CONSTANT:
      return EncodeValueSource::CONSTANT;
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    case plan::EncodeSource::COMPUTED:
      return EncodeValueSource::COMPUTED;
#endif
  }
  return std::nullopt;
}

bool ContainsMessage(const plan::FrozenArray<std::size_t>& indices,
                     std::size_t message_index) noexcept {
  for (const std::size_t candidate : indices) {
    if (candidate == message_index) return true;
  }
  return false;
}

bool IsDecodeCandidate(const plan::PipelineExecutionPlan& pipeline,
                       std::size_t message_index) noexcept {
  for (const auto& group : pipeline.candidate_groups) {
    if (ContainsMessage(group.message_indices, message_index)) return true;
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (ContainsMessage(pipeline.variable_message_indices, message_index)) return true;
#endif
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  if (ContainsMessage(pipeline.text_message_indices, message_index)) return true;
#endif
  return false;
}

bool IsAllowedMessage(const plan::PipelineExecutionPlan& pipeline,
                      std::size_t message_index) noexcept {
  constexpr std::size_t kBitsPerWord = sizeof(std::uint64_t) * 8U;
  const std::size_t word_index = message_index / kBitsPerWord;
  if (word_index >= pipeline.allowed_message_words.size()) return false;
  const auto mask = std::uint64_t{1U} << (message_index % kBitsPerWord);
  return (pipeline.allowed_message_words[word_index] & mask) != 0U;
}

bool Fits(ByteRange range, std::size_t size) noexcept {
  return range.offset <= size && range.length <= size - range.offset;
}

bool AddChecked(std::size_t left, std::size_t right, std::size_t& output) noexcept {
  if (right > std::numeric_limits<std::size_t>::max() - left) return false;
  output = left + right;
  return true;
}

bool ToSize(std::uint64_t value, std::size_t& output) noexcept {
  if (value > std::numeric_limits<std::size_t>::max()) return false;
  output = static_cast<std::size_t>(value);
  return true;
}

bool IsAsciiMessage(const plan::MessageExecutionPlan& message) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  return message.text_decode.has_value() || message.text_encode.has_value();
#else
  static_cast<void>(message);
  return false;
#endif
}

#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
const plan::TextActionExecutionPlan* SelectAsciiAction(const plan::MessageExecutionPlan& message,
                                                       pae::AsciiAction action) noexcept {
  switch (action) {
    case pae::AsciiAction::DECODE:
      return message.text_decode ? &*message.text_decode : nullptr;
    case pae::AsciiAction::ENCODE:
      return message.text_encode ? &*message.text_encode : nullptr;
  }
  return nullptr;
}

bool ReferencesAsciiField(const plan::TextActionExecutionPlan* action,
                          std::size_t field_index) noexcept {
  if (action == nullptr) return false;
  for (const auto& segment : action->segments) {
    if (segment.kind == plan::TextSegmentKind::FIELD && segment.field_index == field_index) {
      return true;
    }
  }
  return false;
}

bool ValidAsciiAction(const plan::TextActionExecutionPlan& action,
                      const plan::MessageExecutionPlan& message) noexcept {
  if (action.segments.empty() || action.min_record_length > action.max_record_length ||
      action.segments.data() == nullptr ||
      message.fields.size() > plan::kDesktopResourceProfileLimits.max_fields_per_message) {
    return false;
  }
  constexpr std::size_t kBitsPerWord = 64U;
  constexpr std::size_t kWordCount =
      (plan::kDesktopResourceProfileLimits.max_fields_per_message + kBitsPerWord - 1U) /
      kBitsPerWord;
  std::array<std::uint64_t, kWordCount> referenced_fields{};
  for (std::size_t ordinal = 0U; ordinal < action.segments.size(); ++ordinal) {
    const auto& segment = action.segments[ordinal];
    if (segment.kind == plan::TextSegmentKind::LITERAL) {
      if (segment.literal.empty() || segment.literal.data() == nullptr ||
          segment.field_index != static_cast<std::size_t>(-1)) {
        return false;
      }
    } else if (segment.kind == plan::TextSegmentKind::FIELD) {
      if (segment.field_index >= message.fields.size() ||
          message.fields[segment.field_index].value_type != plan::ValueType::BYTES ||
          message.fields[segment.field_index].text_min_length >
              message.fields[segment.field_index].text_max_length ||
          !segment.literal.empty()) {
        return false;
      }
      const std::size_t word = segment.field_index / kBitsPerWord;
      const auto mask = std::uint64_t{1U} << (segment.field_index % kBitsPerWord);
      if ((referenced_fields[word] & mask) != 0U) return false;
      referenced_fields[word] |= mask;
    } else {
      return false;
    }
  }
  return true;
}
#endif

bool RecordLengthBounds(const plan::MessageExecutionPlan& message,
                        ByteLengthBounds& output) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (message.bounded_payload.has_value()) {
    const auto& bounded = *message.bounded_payload;
    std::size_t header = 0U;
    std::size_t trailer = 0U;
    std::size_t minimum_payload = 0U;
    std::size_t maximum_payload = 0U;
    std::size_t minimum_frame = 0U;
    std::size_t maximum_frame = 0U;
    std::size_t minimum_sum = 0U;
    std::size_t maximum_sum = 0U;
    return bounded.payload_field_index < message.fields.size() &&
           message.fields[bounded.payload_field_index].value_type == plan::ValueType::BYTES &&
           ToSize(bounded.header_length, header) && ToSize(bounded.trailer_length, trailer) &&
           ToSize(bounded.min_payload_length, minimum_payload) &&
           ToSize(bounded.max_payload_length, maximum_payload) &&
           ToSize(bounded.min_frame_length, minimum_frame) &&
           ToSize(bounded.max_frame_length, maximum_frame) && minimum_payload <= maximum_payload &&
           AddChecked(header, minimum_payload, minimum_sum) &&
           AddChecked(minimum_sum, trailer, minimum_sum) &&
           AddChecked(header, maximum_payload, maximum_sum) &&
           AddChecked(maximum_sum, trailer, maximum_sum) && minimum_sum == minimum_frame &&
           maximum_sum == maximum_frame && maximum_frame == message.frame_size &&
           (output = ByteLengthBounds{minimum_frame, maximum_frame}, true);
  }
#endif
  if (message.frame_size == 0U) return false;
  output = ByteLengthBounds{message.frame_size, message.frame_size};
  return true;
}

bool FrameSizeMatches(const plan::MessageExecutionPlan& message, std::size_t frame_size) noexcept {
  ByteLengthBounds bounds;
  if (!RecordLengthBounds(message, bounds) || frame_size < bounds.minimum ||
      frame_size > bounds.maximum) {
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (message.bounded_payload.has_value()) {
    const auto& bounded = *message.bounded_payload;
    std::size_t header = 0U;
    std::size_t trailer = 0U;
    std::size_t minimum_payload = 0U;
    std::size_t maximum_payload = 0U;
    if (!ToSize(bounded.header_length, header) || !ToSize(bounded.trailer_length, trailer) ||
        !ToSize(bounded.min_payload_length, minimum_payload) ||
        !ToSize(bounded.max_payload_length, maximum_payload) || header > frame_size ||
        trailer > frame_size - header) {
      return false;
    }
    const std::size_t payload = frame_size - header - trailer;
    return payload >= minimum_payload && payload <= maximum_payload;
  }
#endif
  return frame_size == message.frame_size;
}

bool IntegrityWidth(const plan::FrozenIntegrityPlan& integrity, std::size_t& width) noexcept {
  if (integrity.algorithm == plan::IntegrityAlgorithm::SUM8) {
    width = 1U;
    return true;
  }
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (integrity.algorithm == plan::IntegrityAlgorithm::CRC &&
      (integrity.crc_width == 8U || integrity.crc_width == 16U || integrity.crc_width == 32U ||
       integrity.crc_width == 64U)) {
    width = integrity.crc_width / 8U;
    return true;
  }
#endif
  return false;
}

bool ResolveIntegrityStorage(const plan::MessageExecutionPlan& message, std::size_t frame_size,
                             std::optional<ByteRange>& output) noexcept {
  output.reset();
  if (!message.integrity.has_value()) return true;
  std::size_t width = 0U;
  std::size_t offset = 0U;
  if (!IntegrityWidth(*message.integrity, width) ||
      !ToSize(message.integrity->storage_offset, offset)) {
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (message.integrity->storage_at_payload_end) {
    if (!message.bounded_payload.has_value()) return false;
    std::size_t trailer = 0U;
    if (!ToSize(message.bounded_payload->trailer_length, trailer) || width > trailer ||
        trailer > frame_size) {
      return false;
    }
    offset = frame_size - trailer;
  }
#endif
  const ByteRange range{offset, width};
  if (!Fits(range, frame_size)) return false;
  output = range;
  return true;
}

bool ResolveComputedLengthStorage(const plan::MessageExecutionPlan& message, std::size_t frame_size,
                                  std::optional<ByteRange>& output) noexcept {
  output.reset();
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (!message.computed_length.has_value()) return true;
  std::size_t offset = 0U;
  std::size_t width = 0U;
  if (!ToSize(message.computed_length->storage_offset, offset) ||
      !ToSize(message.computed_length->storage_width, width) || width == 0U ||
      message.computed_length->field_index >= message.fields.size()) {
    return false;
  }
  const auto& field = message.fields[message.computed_length->field_index];
  if (field.offset != offset || field.width != width ||
      field.byte_order != message.computed_length->byte_order ||
      field.encode_source != plan::EncodeSource::COMPUTED) {
    return false;
  }
  const ByteRange range{offset, width};
  if (!Fits(range, frame_size)) return false;
  output = range;
#else
  static_cast<void>(message);
  static_cast<void>(frame_size);
#endif
  return true;
}

}  // namespace

CompiledProtocol::CompiledProtocol() noexcept = default;

CompiledProtocol::CompiledProtocol(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

CompiledProtocol::CompiledProtocol(CompiledProtocol&& other) noexcept = default;

CompiledProtocol& CompiledProtocol::operator=(CompiledProtocol&& other) noexcept = default;

CompiledProtocol::~CompiledProtocol() = default;

bool CompiledProtocol::HasValue() const noexcept {
  return impl_ != nullptr && impl_->state && impl_->state->Artifacts().Plan() != nullptr &&
         !impl_->state->Artifacts().Description().empty();
}

std::optional<ProtocolDescription> CompiledProtocol::Protocol() const noexcept {
  if (!HasValue()) {
    return std::nullopt;
  }
  const auto& artifacts = impl_->state->Artifacts();
  const auto& plan_value = *artifacts.Plan();
  const auto& metadata = artifacts.Description();
  const auto& item = metadata.Protocol();
  return ProtocolDescription{
      plan_value.SchemaVersion(),          plan_value.ProtocolId(),
      plan_value.ProtocolVersion(),        Resolve(metadata, item.display_name),
      Resolve(metadata, item.description), Resolve(metadata, item.source_ref)};
}

std::size_t CompiledProtocol::PipelineCount() const noexcept {
  return HasValue() ? impl_->state->Artifacts().Plan()->Pipelines().size() : 0U;
}

std::optional<PipelineDescription> CompiledProtocol::Pipeline(std::size_t index) const noexcept {
  if (!HasValue()) {
    return std::nullopt;
  }
  const auto& artifacts = impl_->state->Artifacts();
  const auto& plan_value = *artifacts.Plan();
  const auto& metadata = artifacts.Description();
  if (index >= plan_value.Pipelines().size() || index >= metadata.Pipelines().size()) {
    return std::nullopt;
  }
  const auto& source = plan_value.Pipelines()[index];
  const auto& item = metadata.Pipelines()[index];
  return PipelineDescription{index,
                             source.id.View(),
                             source.direction_id.View(),
                             Resolve(metadata, item.display_name),
                             Resolve(metadata, item.description),
                             Resolve(metadata, item.source_ref),
                             source.message_indices.size()};
}

std::optional<std::size_t> CompiledProtocol::PipelineMessageIndex(
    std::size_t pipeline_index, std::size_t association_index) const noexcept {
  if (!HasValue()) {
    return std::nullopt;
  }
  const auto& pipelines = impl_->state->Artifacts().Plan()->Pipelines();
  if (pipeline_index >= pipelines.size() ||
      association_index >= pipelines[pipeline_index].message_indices.size()) {
    return std::nullopt;
  }
  const std::size_t message_index = pipelines[pipeline_index].message_indices[association_index];
  if (message_index >= MessageCount()) {
    return std::nullopt;
  }
  return message_index;
}

std::optional<MessageExecutionDescription> CompiledProtocol::PipelineMessageExecution(
    std::size_t pipeline_index, std::size_t message_index) const noexcept {
  if (!HasValue()) return std::nullopt;
  const auto& plan_value = *impl_->state->Artifacts().Plan();
  const auto& pipelines = plan_value.Pipelines();
  const auto& pipeline_execution = plan_value.PipelineExecutionPlans();
  const auto& messages = plan_value.MessageExecutionPlans();
  if (pipeline_index >= pipelines.size() || pipeline_index >= pipeline_execution.size() ||
      message_index >= messages.size() ||
      !ContainsMessage(pipelines[pipeline_index].message_indices, message_index) ||
      !IsAllowedMessage(pipeline_execution[pipeline_index], message_index)) {
    return std::nullopt;
  }

  const auto& message = messages[message_index];
  MessageExecutionDescription output;
  output.pipeline_index = pipeline_index;
  output.message_index = message_index;
  output.decode_available = IsDecodeCandidate(pipeline_execution[pipeline_index], message_index);
  output.encode_available = true;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  if (message.text_decode.has_value() || message.text_encode.has_value()) {
    output.encode_available = message.text_encode.has_value();
    if (output.encode_available) {
      output.encode_output_size = message.text_encode->max_record_length;
      output.encode_output_size_kind =
          message.text_encode->min_record_length == message.text_encode->max_record_length
              ? EncodeOutputSizeKind::EXACT
              : EncodeOutputSizeKind::UPPER_BOUND;
    }
    return output;
  }
#endif
  if (output.encode_available) {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (message.bounded_payload.has_value()) {
      output.encode_output_size =
          static_cast<std::size_t>(message.bounded_payload->max_frame_length);
      output.encode_output_size_kind =
          message.bounded_payload->min_frame_length == message.bounded_payload->max_frame_length
              ? EncodeOutputSizeKind::EXACT
              : EncodeOutputSizeKind::UPPER_BOUND;
      return output;
    }
#endif
    output.encode_output_size = message.frame_size;
    output.encode_output_size_kind = EncodeOutputSizeKind::EXACT;
  }
  return output;
}

std::size_t CompiledProtocol::MessageCount() const noexcept {
  return HasValue() ? impl_->state->Artifacts().Plan()->Messages().size() : 0U;
}

std::optional<MessageDescription> CompiledProtocol::Message(std::size_t index) const noexcept {
  if (!HasValue()) {
    return std::nullopt;
  }
  const auto& artifacts = impl_->state->Artifacts();
  const auto& plan_value = *artifacts.Plan();
  const auto& metadata = artifacts.Description();
  if (index >= plan_value.Messages().size() || index >= metadata.Messages().size()) {
    return std::nullopt;
  }
  const auto& source = plan_value.Messages()[index];
  const auto& item = metadata.Messages()[index];
  if (item.field_begin > metadata.Fields().size() ||
      item.field_count > metadata.Fields().size() - item.field_begin ||
      item.field_count != source.fields.size()) {
    return std::nullopt;
  }
  return MessageDescription{index,
                            source.id.View(),
                            source.direction_id.View(),
                            Resolve(metadata, item.display_name),
                            Resolve(metadata, item.description),
                            Resolve(metadata, item.source_ref),
                            item.field_begin,
                            item.field_count};
}

std::size_t CompiledProtocol::FieldCount() const noexcept {
  return HasValue() ? impl_->state->Artifacts().Description().Fields().size() : 0U;
}

std::optional<FieldDescription> CompiledProtocol::Field(std::size_t flat_index) const noexcept {
  if (!HasValue()) {
    return std::nullopt;
  }
  const auto& artifacts = impl_->state->Artifacts();
  const auto& plan_value = *artifacts.Plan();
  const auto& metadata = artifacts.Description();
  if (flat_index >= metadata.Fields().size()) {
    return std::nullopt;
  }
  for (std::size_t message_index = 0U; message_index < metadata.Messages().size();
       ++message_index) {
    const auto& message_metadata = metadata.Messages()[message_index];
    if (message_metadata.field_begin > flat_index ||
        flat_index - message_metadata.field_begin >= message_metadata.field_count) {
      continue;
    }
    const std::size_t field_index = flat_index - message_metadata.field_begin;
    if (message_index >= plan_value.Messages().size() ||
        field_index >= plan_value.Messages()[message_index].fields.size()) {
      return std::nullopt;
    }
    const auto& source = plan_value.Messages()[message_index].fields[field_index];
    const auto& execution_messages = plan_value.MessageExecutionPlans();
    const auto& item = metadata.Fields()[flat_index];
    if (message_index >= execution_messages.size() ||
        field_index >= execution_messages[message_index].fields.size() ||
        item.enum_begin > metadata.Enums().size() ||
        item.enum_count > metadata.Enums().size() - item.enum_begin ||
        item.enum_count != source.enum_entries.size()) {
      return std::nullopt;
    }
    const auto& execution_field = execution_messages[message_index].fields[field_index];
    const auto value_kind = MapValueKind(execution_field);
    const auto encode_source =
        MapEncodeValueSource(execution_messages[message_index], execution_field);
    if (!value_kind.has_value() || !encode_source.has_value()) return std::nullopt;
    return FieldDescription{flat_index,
                            message_index,
                            field_index,
                            source.id.View(),
                            Resolve(metadata, item.display_name),
                            Resolve(metadata, item.description),
                            Resolve(metadata, item.source_ref),
                            *value_kind,
                            *encode_source,
                            item.enum_begin,
                            item.enum_count};
  }
  return std::nullopt;
}

AsciiActionQueryResult CompiledProtocol::AsciiAction(std::size_t message_index,
                                                     pae::AsciiAction action) const noexcept {
  AsciiActionQueryResult result;
  if (!HasValue()) return result;
  const auto& messages = impl_->state->Artifacts().Plan()->MessageExecutionPlans();
  if (message_index >= messages.size()) {
    result.status = AsciiQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
  if (action != pae::AsciiAction::DECODE && action != pae::AsciiAction::ENCODE) {
    result.status = AsciiQueryStatus::INVALID_SELECTOR;
    return result;
  }
  if (!Message(message_index).has_value()) {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  if (!IsAsciiMessage(messages[message_index])) {
    result.status = AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED;
    return result;
  }
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  const auto* selected = SelectAsciiAction(messages[message_index], action);
  if (selected == nullptr) {
    result.status = AsciiQueryStatus::ACTION_NOT_AVAILABLE;
    return result;
  }
  if (!ValidAsciiAction(*selected, messages[message_index])) {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  result.status = AsciiQueryStatus::OK;
  result.value = AsciiActionDescription{message_index,
                                        action,
                                        {selected->min_record_length, selected->max_record_length},
                                        selected->segments.size()};
#else
  result.status = AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED;
#endif
  return result;
}

AsciiSegmentQueryResult CompiledProtocol::AsciiSegment(std::size_t message_index,
                                                       pae::AsciiAction action,
                                                       std::size_t ordinal) const noexcept {
  AsciiSegmentQueryResult result;
  const auto selected = AsciiAction(message_index, action);
  result.status = selected.status;
  if (selected.status != AsciiQueryStatus::OK || !selected.value) return result;
  if (ordinal >= selected.value->segment_count) {
    result.status = AsciiQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  const auto& message = impl_->state->Artifacts().Plan()->MessageExecutionPlans()[message_index];
  const auto& segment = SelectAsciiAction(message, action)->segments[ordinal];
  AsciiSegmentDescription value;
  value.message_index = message_index;
  value.action = action;
  value.ordinal = ordinal;
  if (segment.kind == plan::TextSegmentKind::LITERAL) {
    value.kind = AsciiSegmentKind::LITERAL;
    value.literal = AsciiLiteralView{segment.literal.data(), segment.literal.size()};
  } else if (segment.kind == plan::TextSegmentKind::FIELD) {
    const auto metadata = Message(message_index);
    if (!metadata || segment.field_index >= metadata->field_count ||
        segment.field_index > std::numeric_limits<std::size_t>::max() - metadata->field_begin) {
      result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    value.kind = AsciiSegmentKind::FIELD;
    value.field_index = segment.field_index;
    value.flat_field_index = metadata->field_begin + segment.field_index;
    if (!Field(*value.flat_field_index).has_value()) {
      result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
  } else {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  result.status = AsciiQueryStatus::OK;
  result.value = value;
#endif
  return result;
}

AsciiFieldQueryResult CompiledProtocol::AsciiField(std::size_t flat_field_index) const noexcept {
  AsciiFieldQueryResult result;
  if (!HasValue()) return result;
  if (flat_field_index >= FieldCount()) {
    result.status = AsciiQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
  const auto metadata = Field(flat_field_index);
  if (!metadata || metadata->message_index >= MessageCount()) {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& message =
      impl_->state->Artifacts().Plan()->MessageExecutionPlans()[metadata->message_index];
  if (!IsAsciiMessage(message)) {
    result.status = AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED;
    return result;
  }
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  if (metadata->index_in_message >= message.fields.size() ||
      message.fields[metadata->index_in_message].value_type != plan::ValueType::BYTES) {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& field = message.fields[metadata->index_in_message];
  if (field.text_min_length > field.text_max_length ||
      (message.text_decode && !ValidAsciiAction(*message.text_decode, message)) ||
      (message.text_encode && !ValidAsciiAction(*message.text_encode, message))) {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const bool decode_reference = ReferencesAsciiField(
      message.text_decode ? &*message.text_decode : nullptr, metadata->index_in_message);
  const bool encode_reference = ReferencesAsciiField(
      message.text_encode ? &*message.text_encode : nullptr, metadata->index_in_message);
  if ((!decode_reference && !encode_reference) || encode_reference != field.text_encode_input) {
    result.status = AsciiQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  result.status = AsciiQueryStatus::OK;
  result.value = AsciiFieldDescription{
      flat_field_index,           metadata->message_index,
      metadata->index_in_message, {field.text_min_length, field.text_max_length},
      decode_reference,           encode_reference};
#else
  result.status = AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED;
#endif
  return result;
}

MessageRepresentationQueryResult CompiledProtocol::MessageRepresentation(
    std::size_t message_index) const noexcept {
  MessageRepresentationQueryResult result;
  if (!HasValue()) return result;
  const auto& plan_value = *impl_->state->Artifacts().Plan();
  if (message_index >= plan_value.Messages().size()) {
    result.status = PhysicalQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
  const auto& messages = plan_value.MessageExecutionPlans();
  if (message_index >= messages.size() || !Message(message_index).has_value()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  result.status = PhysicalQueryStatus::OK;
  result.value = IsAsciiMessage(messages[message_index]) ? RecordRepresentation::ASCII_TEXT
                                                         : RecordRepresentation::BINARY;
  return result;
}

MessagePhysicalQueryResult CompiledProtocol::MessagePhysical(
    std::size_t message_index) const noexcept {
  MessagePhysicalQueryResult result;
  if (!HasValue()) return result;
  const auto& plan_value = *impl_->state->Artifacts().Plan();
  const auto& messages = plan_value.MessageExecutionPlans();
  if (message_index >= plan_value.Messages().size()) {
    result.status = PhysicalQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
  if (message_index >= messages.size() || !Message(message_index).has_value()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& message = messages[message_index];
  if (IsAsciiMessage(message)) {
    result.status = PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED;
    return result;
  }
  MessagePhysicalDescription output;
  output.message_index = message_index;
  std::optional<ByteRange> minimum_integrity_storage;
  if (!RecordLengthBounds(message, output.record_length) ||
      !ResolveIntegrityStorage(message, output.record_length.minimum, minimum_integrity_storage) ||
      !ResolveIntegrityStorage(message, output.record_length.maximum,
                               output.maximum_integrity_storage) ||
      !ResolveComputedLengthStorage(message, output.record_length.minimum,
                                    output.computed_length_storage)) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  output.integrity_storage_offset_depends_on_frame_size =
      message.integrity.has_value() && message.integrity->storage_at_payload_end;
#endif
  if (!output.integrity_storage_offset_depends_on_frame_size &&
      (minimum_integrity_storage.has_value() != output.maximum_integrity_storage.has_value() ||
       (minimum_integrity_storage.has_value() &&
        (minimum_integrity_storage->offset != output.maximum_integrity_storage->offset ||
         minimum_integrity_storage->length != output.maximum_integrity_storage->length)))) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  result.status = PhysicalQueryStatus::OK;
  result.value = output;
  return result;
}

ResolvedMessagePhysicalQueryResult CompiledProtocol::ResolveMessagePhysical(
    std::size_t message_index, std::size_t frame_size) const noexcept {
  ResolvedMessagePhysicalQueryResult result;
  if (!HasValue()) return result;
  const auto& plan_value = *impl_->state->Artifacts().Plan();
  const auto& messages = plan_value.MessageExecutionPlans();
  if (message_index >= plan_value.Messages().size()) {
    result.status = PhysicalQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
  if (message_index >= messages.size() || !Message(message_index).has_value()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& message = messages[message_index];
  if (IsAsciiMessage(message)) {
    result.status = PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED;
    return result;
  }
  ByteLengthBounds bounds;
  if (!RecordLengthBounds(message, bounds)) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  if (!FrameSizeMatches(message, frame_size)) {
    result.status = PhysicalQueryStatus::FRAME_SIZE_MISMATCH;
    return result;
  }
  ResolvedMessagePhysicalDescription output;
  output.message_index = message_index;
  output.frame_size = frame_size;
  if (!ResolveIntegrityStorage(message, frame_size, output.integrity_storage) ||
      !ResolveComputedLengthStorage(message, frame_size, output.computed_length_storage)) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  result.status = PhysicalQueryStatus::OK;
  result.value = output;
  return result;
}

FieldPhysicalQueryResult CompiledProtocol::FieldPhysical(std::size_t flat_index) const noexcept {
  FieldPhysicalQueryResult result;
  if (!HasValue()) return result;
  if (flat_index >= FieldCount()) {
    result.status = PhysicalQueryStatus::INDEX_OUT_OF_RANGE;
    return result;
  }
  const auto field_description = Field(flat_index);
  if (!field_description.has_value()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& messages = impl_->state->Artifacts().Plan()->MessageExecutionPlans();
  if (field_description->message_index >= messages.size()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& message = messages[field_description->message_index];
  if (IsAsciiMessage(message)) {
    result.status = PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED;
    return result;
  }
  if (field_description->index_in_message >= message.fields.size()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  ByteLengthBounds record_bounds;
  if (!RecordLengthBounds(message, record_bounds)) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }

  const auto& field = message.fields[field_description->index_in_message];
  FieldPhysicalDescription output;
  output.flat_field_index = flat_index;
  output.message_index = field_description->message_index;
  output.field_index = field_description->index_in_message;
  constexpr std::size_t kInvalidIndex = static_cast<std::size_t>(-1);
  if (field.bit_container_index == kInvalidIndex) {
    const ByteRange range{field.offset, field.width};
    bool bounded_payload = false;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (message.bounded_payload.has_value() &&
        message.bounded_payload->payload_field_index == output.field_index) {
      std::size_t header = 0U;
      std::size_t minimum_payload = 0U;
      std::size_t maximum_payload = 0U;
      if (!ToSize(message.bounded_payload->header_length, header) ||
          !ToSize(message.bounded_payload->min_payload_length, minimum_payload) ||
          !ToSize(message.bounded_payload->max_payload_length, maximum_payload) ||
          field.value_type != plan::ValueType::BYTES || range.offset != header ||
          range.length != maximum_payload) {
        result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
      output.byte_value_length = ByteLengthBounds{minimum_payload, maximum_payload};
      output.byte_range_length_depends_on_frame_size = true;
      bounded_payload = true;
    }
#endif
    if (!Fits(range, bounded_payload ? record_bounds.maximum : record_bounds.minimum)) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    if (!bounded_payload && field.value_type == plan::ValueType::BYTES) {
      output.byte_value_length = ByteLengthBounds{field.width, field.width};
    }
    output.physical_kind = FieldPhysicalKind::BYTE_RANGE;
    output.maximum_byte_range = range;
  } else {
    if (field.bit_container_index >= message.bit_containers.size()) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    const auto& container = message.bit_containers[field.bit_container_index];
    const ByteRange container_range{container.offset, container.width};
    const bool byte_order_valid = container.width == 1U
                                      ? (container.byte_order == plan::ByteOrder::NOT_APPLICABLE ||
                                         container.byte_order == plan::ByteOrder::LITTLE ||
                                         container.byte_order == plan::ByteOrder::BIG)
                                      : (container.byte_order == plan::ByteOrder::LITTLE ||
                                         container.byte_order == plan::ByteOrder::BIG);
    if (container.width == 0U || container.width > kMaximumFieldPhysicalBitMasks ||
        !Fits(container_range, record_bounds.minimum) || field.bit_width == 0U ||
        field.bit_width > 64U || field.bit_shift >= 64U ||
        field.bit_width > 64U - field.bit_shift || !byte_order_valid) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    const std::uint64_t expected =
        field.bit_width == 64U ? std::numeric_limits<std::uint64_t>::max()
                               : ((std::uint64_t{1U} << field.bit_width) - 1U) << field.bit_shift;
    if (field.bit_mask != expected ||
        (container.width != 8U && (field.bit_mask >> (container.width * 8U)) != 0U)) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    output.physical_kind = FieldPhysicalKind::BIT_MASKS;
    for (std::size_t byte = 0U; byte < container.width; ++byte) {
      const std::size_t numeric_byte =
          container.byte_order == plan::ByteOrder::LITTLE ? byte : container.width - 1U - byte;
      const auto mask = static_cast<std::uint8_t>(field.bit_mask >> (numeric_byte * 8U));
      if (mask == 0U) continue;
      if (output.bit_mask_count >= output.bit_masks.size()) {
        result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
      output.bit_masks[output.bit_mask_count++] = PhysicalBitMask{container.offset + byte, mask};
    }
    if (output.bit_mask_count == 0U) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
  }
  result.status = PhysicalQueryStatus::OK;
  result.value = output;
  return result;
}

ResolvedFieldPhysicalQueryResult CompiledProtocol::ResolveFieldPhysical(
    std::size_t flat_index, std::size_t frame_size) const noexcept {
  ResolvedFieldPhysicalQueryResult result;
  const auto description = FieldPhysical(flat_index);
  if (description.status != PhysicalQueryStatus::OK || !description.value.has_value()) {
    result.status = description.status;
    return result;
  }
  const auto& source = *description.value;
  const auto& messages = impl_->state->Artifacts().Plan()->MessageExecutionPlans();
  if (source.message_index >= messages.size()) {
    result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
    return result;
  }
  const auto& message = messages[source.message_index];
  if (!FrameSizeMatches(message, frame_size)) {
    result.status = PhysicalQueryStatus::FRAME_SIZE_MISMATCH;
    return result;
  }
  ResolvedFieldPhysicalDescription output;
  output.flat_field_index = source.flat_field_index;
  output.message_index = source.message_index;
  output.field_index = source.field_index;
  output.physical_kind = source.physical_kind;
  if (source.physical_kind == FieldPhysicalKind::BIT_MASKS) {
    output.bit_masks = source.bit_masks;
    output.bit_mask_count = source.bit_mask_count;
    for (std::size_t index = 0U; index < output.bit_mask_count; ++index) {
      if (output.bit_masks[index].frame_byte_index >= frame_size) {
        result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
    }
  } else {
    if (!source.maximum_byte_range.has_value()) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    ByteRange range = *source.maximum_byte_range;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (source.byte_range_length_depends_on_frame_size) {
      if (!message.bounded_payload.has_value()) {
        result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
      std::size_t header = 0U;
      std::size_t trailer = 0U;
      if (!ToSize(message.bounded_payload->header_length, header) ||
          !ToSize(message.bounded_payload->trailer_length, trailer) || header > frame_size ||
          trailer > frame_size - header || range.offset != header) {
        result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
        return result;
      }
      range.length = frame_size - header - trailer;
    }
#endif
    if (!Fits(range, frame_size)) {
      result.status = PhysicalQueryStatus::INTERNAL_CONTRACT_VIOLATION;
      return result;
    }
    output.byte_range = range;
  }
  result.status = PhysicalQueryStatus::OK;
  result.value = output;
  return result;
}

std::size_t CompiledProtocol::EnumCount() const noexcept {
  return HasValue() ? impl_->state->Artifacts().Description().Enums().size() : 0U;
}

std::optional<EnumDescription> CompiledProtocol::Enum(std::size_t flat_index) const noexcept {
  if (!HasValue()) {
    return std::nullopt;
  }
  const auto& artifacts = impl_->state->Artifacts();
  const auto& plan_value = *artifacts.Plan();
  const auto& metadata = artifacts.Description();
  if (flat_index >= metadata.Enums().size()) {
    return std::nullopt;
  }
  for (std::size_t field_flat_index = 0U; field_flat_index < metadata.Fields().size();
       ++field_flat_index) {
    const auto& field_metadata = metadata.Fields()[field_flat_index];
    if (field_metadata.enum_begin > flat_index ||
        flat_index - field_metadata.enum_begin >= field_metadata.enum_count) {
      continue;
    }
    const auto field = Field(field_flat_index);
    if (!field.has_value()) {
      return std::nullopt;
    }
    const std::size_t enum_index = flat_index - field_metadata.enum_begin;
    const auto& entries =
        plan_value.Messages()[field->message_index].fields[field->index_in_message].enum_entries;
    if (enum_index >= entries.size()) {
      return std::nullopt;
    }
    const auto& source = entries[enum_index];
    return EnumDescription{flat_index,
                           field_flat_index,
                           enum_index,
                           source.id.View(),
                           Resolve(metadata, metadata.Enums()[flat_index].display_name),
                           source.raw_value};
  }
  return std::nullopt;
}

CompileMemoryReport CompiledProtocol::MemoryReport() const noexcept {
  if (!HasValue()) {
    return {};
  }
  const auto& artifacts = impl_->state->Artifacts();
  const auto& plan_memory = artifacts.Plan()->GetPlanMemoryReport();
  const auto& metadata_memory = artifacts.DescriptionMemory();
  return CompileMemoryReport{plan_memory.accounted_total_bytes,
                             metadata_memory.accounted_total_bytes,
                             metadata_memory.allocation_count,
                             sizeof(Impl) + public_api_internal::CompiledState::FacadeBytes()};
}

CompileResult::CompileResult(CompiledProtocol compiled) noexcept
    : succeeded_(compiled.HasValue()), compiled_(std::move(compiled)) {}

CompileResult::CompileResult(CompileDiagnostic diagnostic) noexcept
    : diagnostic_(std::move(diagnostic)) {}

CompileResult::CompileResult(CompileResult&& other) noexcept
    : succeeded_(other.succeeded_),
      compiled_(std::move(other.compiled_)),
      diagnostic_(std::move(other.diagnostic_)) {
  other.succeeded_ = false;
  other.diagnostic_.reset();
}

CompileResult& CompileResult::operator=(CompileResult&& other) noexcept {
  if (this != &other) {
    succeeded_ = other.succeeded_;
    compiled_ = std::move(other.compiled_);
    diagnostic_ = std::move(other.diagnostic_);
    other.succeeded_ = false;
    other.diagnostic_.reset();
  }
  return *this;
}

CompileResult::~CompileResult() = default;

bool CompileResult::Succeeded() const noexcept {
  return succeeded_ && compiled_.HasValue() && !diagnostic_.has_value();
}

const CompiledProtocol* CompileResult::Compiled() const noexcept {
  return Succeeded() ? &compiled_ : nullptr;
}

const CompileDiagnostic* CompileResult::Diagnostic() const noexcept {
  return diagnostic_.has_value() ? &*diagnostic_ : nullptr;
}

CompiledProtocol CompileResult::TakeCompiled() && noexcept {
  succeeded_ = false;
  diagnostic_.reset();
  return std::move(compiled_);
}

CompileResult CompileProtocolJson(std::string_view json_bytes, const CompileOptions& options) {
  const std::size_t metadata_limit =
      options.metadata_memory_limit_bytes == kUseDefaultMetadataMemoryLimit
          ? internal::DerivedUiDescriptionMemoryLimit(plan::ResourceProfile::DESKTOP)
          : options.metadata_memory_limit_bytes;
  internal::CompileUiArtifactsResult result =
      internal::CompileJsonToPlanWithUiDescription(json_bytes, metadata_limit);
  if (!result.Succeeded()) {
    const internal::CompileDiagnostic* diagnostic = result.Diagnostic();
    if (diagnostic == nullptr) {
      return CompileResult{
          CompileDiagnostic{CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION, "",
                            std::nullopt, "internal compiler failure has no diagnostic"}};
    }
    return CompileResult{MapDiagnostic(*diagnostic)};
  }
  auto artifacts = std::move(result).TakeArtifacts();
  auto impl = std::make_unique<CompiledProtocol::Impl>(std::move(artifacts));
  return CompileResult{CompiledProtocol{std::move(impl)}};
}

}  // namespace pae
