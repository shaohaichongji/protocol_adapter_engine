#include "binary_host_adapter_public.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

#include "description_mapping.h"
#include "public_binary_description.h"
#include "ui_field_result.h"

namespace pae::protocol_lab_ui {
namespace {
std::size_t Add(std::size_t a, std::size_t b) {
  if (b > (std::numeric_limits<std::size_t>::max)() - a)
    throw std::length_error("public Binary UI accounting overflow");
  return a + b;
}
std::size_t Multiply(std::size_t a, std::size_t b) {
  if (a && b > (std::numeric_limits<std::size_t>::max)() / a)
    throw std::length_error("public Binary UI accounting overflow");
  return a * b;
}
void Require(bool value, const char* detail) {
  if (!value) throw std::runtime_error(detail);
}
void ChargeString(const std::string& value, std::size_t& total) {
  total = Add(total, value.capacity() + 1U);
}
std::size_t AccountDescription(const DocumentDescription& description) {
  auto total = sizeof(DocumentDescription);
  ChargeString(description.schema_version, total);
  ChargeString(description.protocol_id, total);
  ChargeString(description.protocol_version, total);
  ChargeString(description.display_name, total);
  ChargeString(description.description, total);
  ChargeString(description.source_ref, total);
  total = Add(total, Multiply(description.pipelines.capacity(), sizeof(PipelineDescriptor)));
  total = Add(total, Multiply(description.messages.capacity(), sizeof(MessageDescriptor)));
  for (const auto& pipeline : description.pipelines) {
    ChargeString(pipeline.id, total);
    ChargeString(pipeline.direction_id, total);
    ChargeString(pipeline.display_name, total);
    ChargeString(pipeline.description, total);
    ChargeString(pipeline.source_ref, total);
    total = Add(total, Multiply(pipeline.message_indices.capacity(), sizeof(std::size_t)));
    total = Add(total, Multiply(pipeline.decode_message_indices.capacity(), sizeof(std::size_t)));
    total = Add(total, Multiply(pipeline.encode_message_indices.capacity(), sizeof(std::size_t)));
  }
  for (const auto& message : description.messages) {
    ChargeString(message.id, total);
    ChargeString(message.direction_id, total);
    ChargeString(message.display_name, total);
    ChargeString(message.description, total);
    ChargeString(message.source_ref, total);
    total = Add(total, Multiply(message.fields.capacity(), sizeof(FieldDescriptor)));
    for (const auto& field : message.fields) {
      ChargeString(field.id, total);
      ChargeString(field.display_name, total);
      ChargeString(field.description, total);
      ChargeString(field.source_ref, total);
      ChargeString(field.read_only_annotation, total);
      total = Add(total, Multiply(field.enum_entries.capacity(), sizeof(EnumDescriptor)));
      total = Add(total, Multiply(field.physical_bits.capacity(), sizeof(PhysicalBitMask)));
      for (const auto& item : field.enum_entries) {
        ChargeString(item.id, total);
        ChargeString(item.display_name, total);
      }
    }
  }
  return total;
}
std::size_t StringCapacityUpper(std::size_t size) {
  const auto small = std::string{}.capacity();
  if (size <= small) return small;
  return (std::max)(size | small, Add(size, size / 2U));
}
void ChargeDestinationString(std::string_view value, std::size_t& total) {
  total = Add(total, StringCapacityUpper(value.size()) + 1U);
}
template <typename Integer>
std::size_t Characters(Integer value) noexcept {
  char buffer[64];
  const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), value);
  return converted.ec == std::errc{} ? static_cast<std::size_t>(converted.ptr - buffer) : 64U;
}
template <typename Integer>
std::string Number(Integer value) {
  char buffer[64];
  const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), value);
  Require(converted.ec == std::errc{}, "public Binary numeric formatting failed");
  return {buffer, converted.ptr};
}
std::string Decimal(std::int64_t coefficient, std::int32_t scale) {
  return Number(coefficient) + '@' + Number(scale);
}
std::string Hex(const std::vector<std::uint8_t>& bytes) {
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string text;
  text.resize(Multiply(bytes.size(), 2U));
  for (std::size_t i = 0U; i < bytes.size(); ++i) {
    text[2U * i] = digits[bytes[i] >> 4U];
    text[2U * i + 1U] = digits[bytes[i] & 0x0FU];
  }
  return text;
}
std::size_t AccountResult(const BinaryUiDecodeResult& result) {
  auto total = Add(sizeof(BinaryUiDecodeResult), result.frame.capacity());
  total = Add(total, Multiply(result.fields.capacity(), sizeof(UiFieldResult)));
  ChargeString(result.message_id, total);
  for (const auto& field : result.fields) {
    ChargeString(field.id, total);
    ChargeString(field.raw_value, total);
    ChargeString(field.logical_value, total);
  }
  return total;
}
std::size_t AccountResult(const BinaryUiEncodeResult& result) {
  auto total = Add(sizeof(BinaryUiEncodeResult), result.frame.capacity());
  total = Add(total, Multiply(result.fields.capacity(), sizeof(UiFieldResult)));
  ChargeString(result.message_id, total);
  for (const auto& field : result.fields) {
    ChargeString(field.id, total);
    ChargeString(field.raw_value, total);
    ChargeString(field.logical_value, total);
  }
  return total;
}

StreamFramingStrategy UiStrategy(pae::PipelineFramingStrategy strategy) noexcept {
  switch (strategy) {
    case pae::PipelineFramingStrategy::COMPLETE_RECORD:
      return StreamFramingStrategy::COMPLETE_RECORD;
    case pae::PipelineFramingStrategy::FIXED_LENGTH:
      return StreamFramingStrategy::FIXED_LENGTH;
    case pae::PipelineFramingStrategy::SYNC_FIXED_LENGTH:
      return StreamFramingStrategy::SYNC_FIXED_LENGTH;
    case pae::PipelineFramingStrategy::SYNC_LENGTH_FIELD:
      return StreamFramingStrategy::SYNC_LENGTH_FIELD;
    case pae::PipelineFramingStrategy::ASCII_CRLF:
      return StreamFramingStrategy::ASCII_CRLF;
  }
  return StreamFramingStrategy::COMPLETE_RECORD;
}

StreamRuntimePhase UiPhase(pae::StreamFramingPhase phase) noexcept {
  switch (phase) {
    case pae::StreamFramingPhase::COLLECTING:
      return StreamRuntimePhase::COLLECTING;
    case pae::StreamFramingPhase::DELIVERY_PENDING:
      return StreamRuntimePhase::DELIVERY_PENDING;
    case pae::StreamFramingPhase::DISCARDING_UNTIL_CRLF:
      return StreamRuntimePhase::DISCARDING_UNTIL_CRLF;
  }
  return StreamRuntimePhase::COLLECTING;
}

StreamPresentationObservation UiObservation(
    const protocol_lab_binary::public_decode::StreamObservation& source) noexcept {
  StreamPresentationObservation result;
  result.strategy = UiStrategy(source.strategy);
  result.phase = UiPhase(source.phase);
  result.maximum_candidate_frame_bytes = source.maximum_candidate_frame_bytes;
  result.effective_max_submit_bytes = source.effective_max_submit_bytes;
  result.effective_max_work_units = source.effective_max_work_units;
  result.buffered_bytes = source.buffered_bytes;
  result.frozen_input_bytes = source.frozen_input_bytes;
  result.frozen_cursor = source.frozen_cursor;
  result.has_internal_work = source.has_internal_work;
  result.reset_required = source.reset_required;
  result.generation = source.generation;
  result.step_sequence = source.step_sequence;
  result.total_candidates = source.total_candidates;
  result.total_decode_successes = source.total_decode_successes;
  result.total_decode_failures = source.total_decode_failures;
  result.total_observer_callbacks = source.total_observer_callbacks;
  result.total_business_callbacks = source.total_business_callbacks;
  result.total_discarded_bytes = source.total_discarded_bytes;
  result.total_malformed_candidates = source.total_malformed_candidates;
  return result;
}
}  // namespace

const char* PublicBinaryCodecStatusName(pae::CodecStatus status) noexcept {
#define PAE_CODEC_CASE(name)   \
  case pae::CodecStatus::name: \
    return #name
  switch (status) {
    PAE_CODEC_CASE(OK);
    PAE_CODEC_CASE(INVALID_ARGUMENT);
    PAE_CODEC_CASE(INVALID_COMPILED_PROTOCOL);
    PAE_CODEC_CASE(RESOURCE_LIMIT_EXCEEDED);
    PAE_CODEC_CASE(ALLOCATION_FAILED);
    PAE_CODEC_CASE(WORKSPACE_BUSY);
    PAE_CODEC_CASE(INPUT_VALUES_TOO_MANY);
    PAE_CODEC_CASE(UNKNOWN_MESSAGE);
    PAE_CODEC_CASE(AMBIGUOUS_MESSAGE);
    PAE_CODEC_CASE(OUTPUT_SLOTS_TOO_SMALL);
    PAE_CODEC_CASE(INTEGRITY_FAILED);
    PAE_CODEC_CASE(MESSAGE_NOT_ALLOWED);
    PAE_CODEC_CASE(FIELD_REFERENCE_MISMATCH);
    PAE_CODEC_CASE(DUPLICATE_FIELD);
    PAE_CODEC_CASE(MISSING_FIELD);
    PAE_CODEC_CASE(TYPE_MISMATCH);
    PAE_CODEC_CASE(VALUE_NOT_REPRESENTABLE);
    PAE_CODEC_CASE(BYTES_LENGTH_MISMATCH);
    PAE_CODEC_CASE(UNKNOWN_ENUM_VALUE);
    PAE_CODEC_CASE(ENUM_REFERENCE_MISMATCH);
    PAE_CODEC_CASE(CONSTANT_FIELD_OVERRIDE);
    PAE_CODEC_CASE(INPUT_OUTPUT_OVERLAP);
    PAE_CODEC_CASE(BUFFER_TOO_SMALL);
    PAE_CODEC_CASE(FINAL_REVIEW_FAILED);
    PAE_CODEC_CASE(ASCII_CHARACTER_NOT_ALLOWED);
    PAE_CODEC_CASE(ASCII_TERMINATOR_CONFLICT);
    PAE_CODEC_CASE(OPERATION_NOT_SUPPORTED);
    PAE_CODEC_CASE(COMPUTED_FIELD_OVERRIDE);
    PAE_CODEC_CASE(LENGTH_MISMATCH);
    PAE_CODEC_CASE(INTERNAL_ERROR);
  }
#undef PAE_CODEC_CASE
  return "UNKNOWN_CODEC_STATUS";
}

BinaryHostAdapter::BinaryHostAdapter() = default;
BinaryHostAdapter::~BinaryHostAdapter() = default;

std::unique_ptr<BinaryHostAdapter> BinaryHostAdapter::CreatePublic(
    pae::CompiledProtocol compiled, std::vector<BinaryHostBinding> bindings,
    BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
    std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
    std::string& error, const protocol_lab_binary::public_decode::Limits& limits,
    const BinaryUiCopyControls* copy_controls) {
  try {
    Require(identity.document && identity.load && identity.session && identity.request &&
                !identity.config_sha256.empty(),
            "public Binary identity is incomplete");
    if (previous)
      Require(previous->identity_.document == identity.document &&
                  previous->identity_.load == identity.load &&
                  previous->identity_.session != (std::numeric_limits<std::uint64_t>::max)() &&
                  previous->identity_.session + 1U == identity.session &&
                  previous->identity_.config_sha256 == identity.config_sha256,
              "public Binary replacement identity is stale");
    Require(!bindings.empty() && bindings.size() <= 64U,
            "public Binary binding count is outside limits");
    const protocol_lab_binary::public_decode::Limits hard;
    Require(limits.instance_bytes <= hard.instance_bytes &&
                limits.replacement_bytes <= hard.replacement_bytes &&
                limits.max_result_bytes <= hard.max_result_bytes &&
                limits.max_frame_bytes <= hard.max_frame_bytes &&
                limits.max_stream_chunk_bytes <= hard.max_stream_chunk_bytes,
            "public Binary limits exceed hard ceilings");
    auto adapter = std::unique_ptr<BinaryHostAdapter>(new BinaryHostAdapter);
    adapter->limits_ = limits;
    adapter->bindings_ = std::move(bindings);
    adapter->identity_ = std::move(identity);
    adapter->externally_retained_bytes_ = externally_retained_bytes;
    adapter->preparation_coexisting_bytes_ = preparation_coexisting_bytes;
    if (copy_controls) adapter->copy_controls_ = *copy_controls;
    adapter->description_ = std::make_unique<DocumentDescription>();
    Require(BuildPublicBinaryDescription(compiled, *adapter->description_, error),
            "public Binary description mapping failed");
    Require(adapter->description_->max_frame_bytes <= limits.max_frame_bytes,
            "public Binary record exceeds H1 frame limit");
    std::vector<protocol_lab_binary::public_decode::Binding> public_bindings;
    public_bindings.reserve(adapter->bindings_.size());
    std::size_t channels = 0U;
    for (const auto& binding : adapter->bindings_) {
      Require(binding.streams && binding.streams <= 64U - channels &&
                  (binding.action == pae::HostAction::DECODE ||
                   (binding.action == pae::HostAction::ENCODE && binding.streams == 1U)),
              "public Binary H2 binding/action/Flow is invalid");
      channels += binding.streams;
      public_bindings.push_back(
          {binding.endpoint, binding.action, binding.pipeline_id, binding.streams});
    }
    adapter->utf16_draft_reserve_bytes_ =
        Multiply(channels + 1U, Add(Multiply(4U, limits.max_frame_bytes), sizeof(std::u16string)));
    const auto mapped_description_bytes = AccountDescription(*adapter->description_);
    Require(mapped_description_bytes <= limits.max_description_bytes,
            "public Binary UI description exceeds bound");
    adapter->description_copy_upper_bound_bytes_ = Multiply(2U, mapped_description_bytes);
    Require(adapter->description_copy_upper_bound_bytes_ <=
                adapter->copy_controls_.description_copy_limit,
            "public Binary description copy preflight exceeded");
    auto before_copy = Add(mapped_description_bytes, adapter->description_copy_upper_bound_bytes_);
    before_copy = Add(before_copy, externally_retained_bytes);
    before_copy = Add(before_copy, preparation_coexisting_bytes);
    before_copy = Add(before_copy, adapter->ui_view_reserve_bytes_);
    before_copy = Add(before_copy, adapter->hex_preview_reserve_bytes_);
    before_copy = Add(before_copy, adapter->utf16_draft_reserve_bytes_);
    if (previous) before_copy = Add(before_copy, previous->AccountedInstanceBytes());
    Require(before_copy <= (previous ? limits.replacement_bytes : limits.instance_bytes),
            "public Binary UI pre-description preparation budget exceeded");
    if (adapter->copy_controls_.before_description_copy)
      adapter->copy_controls_.before_description_copy(adapter->copy_controls_.context);
    adapter->publication_description_ =
        std::make_unique<DocumentDescription>(*adapter->description_);
    adapter->ui_description_bytes_ = Add(AccountDescription(*adapter->description_),
                                         AccountDescription(*adapter->publication_description_));
    adapter->ui_description_bytes_ =
        Add(adapter->ui_description_bytes_,
            Multiply(adapter->bindings_.capacity(), sizeof(BinaryHostBinding)));
    for (const auto& binding : adapter->bindings_) {
      ChargeString(binding.endpoint, adapter->ui_description_bytes_);
      ChargeString(binding.pipeline_id, adapter->ui_description_bytes_);
    }
    const auto previous_bytes = previous ? previous->owner_->InstanceAdmissionBytes() : 0U;
    auto prepared = protocol_lab_binary::public_decode::Adapter::AdoptCompiled(
        std::move(compiled), std::move(public_bindings), limits, previous_bytes);
    Require(
        prepared.status == protocol_lab_binary::public_decode::LocalStatus::OK && prepared.adapter,
        "public Binary H1 adoption or Host binding failed");
    adapter->owner_ = std::move(prepared.adapter);
    adapter->drafts_.resize(channels);
    adapter->typed_drafts_.resize(channels);
    adapter->message_selections_.resize(channels);
    adapter->stream_mapping_faulted_.resize(channels);
    adapter->ui_description_bytes_ =
        Add(adapter->ui_description_bytes_,
            Multiply(adapter->message_selections_.capacity(), sizeof(std::optional<std::size_t>)));
    adapter->ui_description_bytes_ =
        Add(adapter->ui_description_bytes_, adapter->stream_mapping_faulted_.capacity());
    for (std::size_t binding = 0U; binding < adapter->bindings_.size(); ++binding) {
      if (adapter->bindings_[binding].action != pae::HostAction::ENCODE) continue;
      const auto pipeline = std::find_if(
          adapter->description_->pipelines.begin(), adapter->description_->pipelines.end(),
          [&](const auto& value) { return value.id == adapter->bindings_[binding].pipeline_id; });
      Require(pipeline != adapter->description_->pipelines.end() &&
                  !pipeline->encode_message_indices.empty(),
              "public Binary Encode binding has no Message selection");
      for (std::size_t flow = 0U; flow < adapter->owner_->FlowCount(binding); ++flow)
        adapter->message_selections_[adapter->owner_->FlowIndex(binding, flow)] =
            pipeline->encode_message_indices.front();
    }
    static std::atomic<std::uint64_t> next_instance{1U};
    adapter->instance_ = next_instance.fetch_add(1U);
    Require(adapter->instance_, "public Binary instance generation exhausted");
    Require(adapter->AccountedInstanceBytes() <= limits.instance_bytes,
            "public Binary UI instance budget exceeded");
    auto peak = adapter->AccountedPreparationBytes();
    if (previous) peak = Add(peak, previous->AccountedInstanceBytes());
    Require(peak <= (previous ? limits.replacement_bytes : limits.instance_bytes),
            "public Binary UI preparation/replacement budget exceeded");
    error.clear();
    return adapter;
  } catch (const std::exception& exception) {
    if (error.empty()) error = exception.what();
    return nullptr;
  }
}

const DocumentDescription& BinaryHostAdapter::Description() const noexcept { return *description_; }
DocumentDescription BinaryHostAdapter::TakeDescription() {
  Require(publication_description_ != nullptr, "public Binary description already published");
  auto moved = std::move(*publication_description_);
  publication_description_.reset();
  return moved;
}
std::size_t BinaryHostAdapter::AccountDescriptionBytes(const DocumentDescription& description) {
  return AccountDescription(description);
}
std::size_t BinaryHostAdapter::AccountedInstanceBytes() const noexcept {
  if (!owner_) return (std::numeric_limits<std::size_t>::max)();
  try {
    auto total = Add(owner_->InstanceAdmissionBytes(), ui_description_bytes_);
    total = Add(total, externally_retained_bytes_);
    total = Add(total, ui_view_reserve_bytes_);
    total = Add(total, hex_preview_reserve_bytes_);
    return Add(total, utf16_draft_reserve_bytes_);
  } catch (...) {
    return (std::numeric_limits<std::size_t>::max)();
  }
}
std::size_t BinaryHostAdapter::AccountedPreparationBytes() const noexcept {
  try {
    return Add(AccountedInstanceBytes(), preparation_coexisting_bytes_);
  } catch (...) {
    return (std::numeric_limits<std::size_t>::max)();
  }
}
std::size_t BinaryHostAdapter::FlowCount(std::size_t binding) const noexcept {
  return owner_ ? owner_->FlowCount(binding) : 0U;
}
bool BinaryHostAdapter::IsCompleteDecode(std::size_t binding, std::size_t flow) const noexcept {
  if (binding >= bindings_.size() || flow >= FlowCount(binding) ||
      bindings_[binding].action != pae::HostAction::DECODE)
    return false;
  const auto pipeline =
      std::find_if(description_->pipelines.begin(), description_->pipelines.end(),
                   [&](const auto& value) { return value.id == bindings_[binding].pipeline_id; });
  return pipeline != description_->pipelines.end() &&
         pipeline->input_kind == StreamInputKind::COMPLETE_RECORD;
}
bool BinaryHostAdapter::IsStreamDecode(std::size_t binding, std::size_t flow) const noexcept {
  if (binding >= bindings_.size() || flow >= FlowCount(binding) ||
      bindings_[binding].action != pae::HostAction::DECODE)
    return false;
  const auto pipeline =
      std::find_if(description_->pipelines.begin(), description_->pipelines.end(),
                   [&](const auto& value) { return value.id == bindings_[binding].pipeline_id; });
  return pipeline != description_->pipelines.end() &&
         pipeline->input_kind == StreamInputKind::STREAM_CHUNK;
}
bool BinaryHostAdapter::IsCompleteEncode(std::size_t binding, std::size_t flow) const noexcept {
  return binding < bindings_.size() && flow < FlowCount(binding) &&
         bindings_[binding].action == pae::HostAction::ENCODE;
}
bool BinaryHostAdapter::SetPresentationRetainedBytes(std::size_t bytes) noexcept {
  if (bytes > UiViewReserveBytes() / 2U) return false;
  presentation_retained_bytes_ = bytes;
  return true;
}
std::u16string_view BinaryHostAdapter::Draft(std::size_t binding, std::size_t flow) const noexcept {
  const auto flat = owner_->FlowIndex(binding, flow);
  return flat < drafts_.size() ? std::u16string_view(drafts_[flat]) : std::u16string_view{};
}
std::size_t BinaryHostAdapter::DraftLimit(std::size_t binding, std::size_t flow) const noexcept {
  if (IsStreamDecode(binding, flow)) {
    const auto observed = ObserveStream(binding, flow);
    if (!observed || observed->effective_max_submit_bytes >
                         (std::numeric_limits<std::size_t>::max)() / 3U)
      return 0U;
    return observed->effective_max_submit_bytes * 3U;
  }
  return limits_.max_frame_bytes * 2U;
}
void BinaryHostAdapter::SaveAndSelect(std::u16string_view draft, std::size_t binding,
                                      std::size_t flow) {
  const auto target = owner_->FlowIndex(binding, flow);
  const auto source = owner_->FlowIndex(selected_binding_, selected_flow_);
  Require(target < drafts_.size() && source < drafts_.size(),
          "public Binary Flow index is invalid");
  std::u16string pending(draft);  // Finish every throwing Lab copy before publishing to H1.
  std::string ascii;
  ascii.reserve(draft.size());
  for (char16_t unit : draft) {
    Require(unit <= 0x7FU, "public Binary Hex draft is not ASCII");
    ascii.push_back(static_cast<char>(unit));
  }
  if (!IsStreamDecode(selected_binding_, selected_flow_))
    Require(owner_->SetDraft(source, ascii), "public Binary Flow draft exceeds budget");
  drafts_[source] = std::move(pending);
  selected_binding_ = binding;
  selected_flow_ = flow;
}
bool BinaryHostAdapter::SaveDraftsAndSelect(
    std::u16string_view inspect_draft,
    const std::unordered_map<std::size_t, TypedDraft>& typed_drafts,
    std::size_t source_message_index, std::size_t binding, std::size_t flow) {
  const auto target = owner_->FlowIndex(binding, flow);
  const auto source = owner_->FlowIndex(selected_binding_, selected_flow_);
  if (target >= drafts_.size() || source >= drafts_.size() || source >= typed_drafts_.size() ||
      typed_drafts.size() > limits_.max_fields)
    return false;
  try {
    std::u16string pending_inspect(inspect_draft);
    std::string ascii;
    ascii.reserve(inspect_draft.size());
    for (char16_t unit : inspect_draft) {
      if (unit > 0x7FU) return false;
      ascii.push_back(static_cast<char>(unit));
    }
    std::size_t bytes =
        Multiply(typed_drafts.size(), sizeof(std::pair<const std::size_t, TypedDraft>));
    for (const auto& item : typed_drafts) {
      if (const auto* value = std::get_if<std::vector<std::uint8_t>>(&item.second))
        bytes = Add(bytes, value->capacity());
      if (const auto* value = std::get_if<EnumSelection>(&item.second))
        bytes = Add(bytes, value->entry_id.capacity() + 1U);
    }
    if (bytes > UiViewReserveBytes() / 4U) return false;
    auto pending_typed = typed_drafts;

    // All Lab-owned copies and budget checks complete before changing H1 or selection state.
    if (!IsStreamDecode(selected_binding_, selected_flow_) && !owner_->SetDraft(source, ascii))
      return false;
    drafts_[source] = std::move(pending_inspect);
    typed_drafts_[source] = std::move(pending_typed);
    if (bindings_[selected_binding_].action == pae::HostAction::ENCODE)
      message_selections_[source] = source_message_index;
    selected_binding_ = binding;
    selected_flow_ = flow;
    return true;
  } catch (...) {
    return false;
  }
}
bool BinaryHostAdapter::SaveTypedDrafts(std::size_t binding, std::size_t flow,
                                        const std::unordered_map<std::size_t, TypedDraft>& drafts) {
  const auto flat = owner_->FlowIndex(binding, flow);
  if (flat >= typed_drafts_.size() || drafts.size() > limits_.max_fields) return false;
  try {
    std::size_t bytes = Multiply(drafts.size(), sizeof(std::pair<const std::size_t, TypedDraft>));
    for (const auto& item : drafts) {
      if (const auto* value = std::get_if<std::vector<std::uint8_t>>(&item.second))
        bytes = Add(bytes, value->capacity());
      if (const auto* value = std::get_if<EnumSelection>(&item.second))
        bytes = Add(bytes, value->entry_id.capacity() + 1U);
    }
    if (bytes > UiViewReserveBytes() / 4U) return false;
    auto pending = drafts;
    typed_drafts_[flat] = std::move(pending);
    return true;
  } catch (...) {
    return false;
  }
}
const std::unordered_map<std::size_t, TypedDraft>& BinaryHostAdapter::TypedDrafts(
    std::size_t binding, std::size_t flow) const noexcept {
  static const std::unordered_map<std::size_t, TypedDraft> empty;
  const auto flat = owner_->FlowIndex(binding, flow);
  return flat < typed_drafts_.size() ? typed_drafts_[flat] : empty;
}
std::optional<std::size_t> BinaryHostAdapter::MessageSelection(std::size_t binding,
                                                               std::size_t flow) const noexcept {
  const auto flat = owner_->FlowIndex(binding, flow);
  return flat < message_selections_.size() ? message_selections_[flat] : std::nullopt;
}
bool BinaryHostAdapter::SelectEncodeMessage(std::size_t binding, std::size_t flow,
                                            std::size_t message_index) noexcept {
  if (!IsCompleteEncode(binding, flow)) return false;
  const auto pipeline =
      std::find_if(description_->pipelines.begin(), description_->pipelines.end(),
                   [&](const auto& value) { return value.id == bindings_[binding].pipeline_id; });
  if (pipeline == description_->pipelines.end() ||
      std::find(pipeline->encode_message_indices.begin(), pipeline->encode_message_indices.end(),
                message_index) == pipeline->encode_message_indices.end())
    return false;
  const auto flat = owner_->FlowIndex(binding, flow);
  if (flat >= message_selections_.size()) return false;
  if (message_selections_[flat] != message_index) {
    message_selections_[flat] = message_index;
    typed_drafts_[flat].clear();
    owner_->ClearCurrent(flat);
  }
  return true;
}
void BinaryHostAdapter::ClearCurrentEncode(std::size_t binding, std::size_t flow) noexcept {
  if (IsCompleteEncode(binding, flow)) owner_->ClearCurrent(owner_->FlowIndex(binding, flow));
}
const void* BinaryHostAdapter::Current(std::size_t binding, std::size_t flow) const noexcept {
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? static_cast<const void*>(&state->current)
             : nullptr;
}

std::size_t BinaryHostAdapter::ResultCopyUpperBound(
    const protocol_lab_binary::public_decode::Operation& operation) const {
  return ResultCopyUpperBound(operation.host, operation.candidate);
}

std::size_t BinaryHostAdapter::ResultCopyUpperBound(
    const pae::HostOperationResult& host,
    const std::optional<protocol_lab_binary::public_decode::Candidate>& candidate) const {
  if (!candidate || !candidate->success || !candidate->message_index ||
      host.status != pae::HostStatus::OK)
    return Add(sizeof(BinaryUiDecodeFailure), candidate ? candidate->frame.size() : 0U);
  Require(*candidate->message_index < description_->messages.size(),
          "public Binary result Message index mismatch");
  const auto& message = description_->messages[*candidate->message_index];
  Require(candidate->fields.size() == message.fields.size(),
          "public Binary result Field count mismatch");
  auto total = Add(sizeof(BinaryUiDecodeResult), candidate->frame.size());
  total = Add(total, Multiply(candidate->fields.size(), sizeof(UiFieldResult)));
  ChargeDestinationString(candidate->message_id, total);
  for (std::size_t i = 0U; i < candidate->fields.size(); ++i) {
    const auto& field = candidate->fields[i];
    ChargeDestinationString(field.id, total);
    std::size_t raw = 0U;
    std::size_t logical = 0U;
    switch (field.kind) {
      case pae::ValueKind::UINT64:
        raw = logical = Characters(field.uint64_value);
        break;
      case pae::ValueKind::INT64:
        raw = logical = Characters(field.int64_value);
        break;
      case pae::ValueKind::BOOL:
        raw = std::string_view{"未单独提供"}.size();
        logical = field.bool_value ? 4U : 5U;
        break;
      case pae::ValueKind::BYTES:
        raw = logical = Multiply(field.bytes.size(), 2U);
        break;
      case pae::ValueKind::ENUM: {
        Require(field.enum_raw.has_value(), "public Binary Enum result has no raw value");
        raw = Characters(*field.enum_raw);
        logical = 7U;
        if (field.known_enum_flat_index) {
          const auto& entries = message.fields[i].enum_entries;
          const auto found = std::find_if(entries.begin(), entries.end(), [&](const auto& item) {
            return item.raw_value == *field.enum_raw;
          });
          Require(found != entries.end(), "public Binary known Enum mapping mismatch");
          logical =
              Add(Add(found->display_name.empty() ? found->id.size() : found->display_name.size(),
                      found->id.size()),
                  3U);
        }
        break;
      }
      case pae::ValueKind::DECIMAL64:
        Require(field.conversion_raw_kind.has_value(),
                "public Binary Decimal64 has no raw integer");
        raw = *field.conversion_raw_kind == pae::RawIntegerKind::UINT64
                  ? Characters(*field.conversion_raw_uint64)
                  : Characters(*field.conversion_raw_int64);
        logical =
            Add(Add(Characters(field.decimal.coefficient), 1U), Characters(field.decimal.scale));
        break;
    }
    total = Add(total, StringCapacityUpper(raw) + 1U);
    total = Add(total, StringCapacityUpper(logical) + 1U);
  }
  return total;
}

BinaryUiDecodeView BinaryHostAdapter::MapOperation(
    const protocol_lab_binary::public_decode::Operation& operation,
    std::size_t active_view_bytes) const {
  return MapCandidate(operation.host, operation.candidate, active_view_bytes, 0U);
}

BinaryUiDecodeView BinaryHostAdapter::MapCandidate(
    const pae::HostOperationResult& host,
    const std::optional<protocol_lab_binary::public_decode::Candidate>& candidate,
    std::size_t active_view_bytes, std::size_t input_peak_bytes) const {
  const auto upper = ResultCopyUpperBound(host, candidate);
  const auto budget = UiViewReserveBytes() / 2U;
  auto occupied = Add(presentation_retained_bytes_, active_view_bytes);
  occupied = Add(occupied, input_peak_bytes);
  Require(
      occupied <= budget && upper <= budget - occupied && upper <= copy_controls_.result_copy_limit,
      "public Binary UI result copy preflight exceeded");
  if (copy_controls_.before_result_copy) copy_controls_.before_result_copy(copy_controls_.context);
  BinaryUiDecodeView view;
  view.public_host = host;
  if (host.status == pae::HostStatus::OK && candidate && candidate->success &&
      candidate->message_index) {
    const auto& source = *candidate;
    const auto& message = description_->messages[*source.message_index];
    auto& result = view.result.emplace();
    result.frame = source.frame;
    result.message_index = *source.message_index;
    result.message_id = source.message_id;
    result.fields.reserve(source.fields.size());
    for (std::size_t i = 0U; i < source.fields.size(); ++i) {
      const auto& source_field = source.fields[i];
      const auto& described = message.fields[i];
      Require(source_field.id == described.id, "public Binary Field identity mismatch");
      UiFieldResult field;
      field.field_index = i;
      field.id = source_field.id;
      if (source_field.byte_range)
        field.actual_range =
            ByteRange{source_field.byte_range->offset, source_field.byte_range->length};
      switch (source_field.kind) {
        case pae::ValueKind::UINT64:
          field.raw_value = Number(source_field.uint64_value);
          field.logical_value = field.raw_value;
          break;
        case pae::ValueKind::INT64:
          field.raw_value = Number(source_field.int64_value);
          field.logical_value = field.raw_value;
          break;
        case pae::ValueKind::BOOL:
          field.raw_value = "未单独提供";
          field.logical_value = source_field.bool_value ? "true" : "false";
          break;
        case pae::ValueKind::BYTES:
          field.raw_value = field.logical_value = Hex(source_field.bytes);
          break;
        case pae::ValueKind::ENUM: {
          field.raw_value = Number(*source_field.enum_raw);
          field.logical_value = "unknown";
          if (source_field.known_enum_flat_index) {
            const auto found = std::find_if(
                described.enum_entries.begin(), described.enum_entries.end(),
                [&](const auto& item) { return item.raw_value == *source_field.enum_raw; });
            Require(found != described.enum_entries.end(),
                    "public Binary known Enum mapping mismatch");
            const auto& display = found->display_name.empty() ? found->id : found->display_name;
            field.logical_value = display + " [" + found->id + ']';
          }
          break;
        }
        case pae::ValueKind::DECIMAL64:
          Require(source_field.conversion_raw_kind.has_value(),
                  "public Binary Decimal64 result has no raw integer");
          field.raw_value = *source_field.conversion_raw_kind == pae::RawIntegerKind::UINT64
                                ? Number(*source_field.conversion_raw_uint64)
                                : Number(*source_field.conversion_raw_int64);
          field.logical_value =
              Decimal(source_field.decimal.coefficient, source_field.decimal.scale);
          break;
      }
      result.fields.push_back(std::move(field));
    }
    result.accounted_bytes = AccountResult(result);
    Require(result.accounted_bytes <= upper && result.accounted_bytes <= budget - occupied,
            "public Binary active UI view budget exceeded");
    view.ok = true;
    return view;
  }
  auto& failure = view.failure.emplace();
  failure.host_status = host.status;
  failure.codec_status = host.codec_status;
  if (candidate) {
    failure.diagnostic_frame = candidate->frame;
    failure.message_index = candidate->message_index;
    // A failed flat Field index alone does not establish a Message identity.
  }
  failure.accounted_bytes = sizeof(BinaryUiDecodeFailure) + failure.diagnostic_frame.capacity();
  Require(failure.accounted_bytes <= upper && failure.accounted_bytes <= budget - occupied,
          "public Binary failure UI view budget exceeded");
  return view;
}

BinaryUiStreamView BinaryHostAdapter::MapStreamStep(
    const protocol_lab_binary::public_decode::StreamStep& step, std::size_t active_view_bytes,
    std::size_t input_peak_bytes) const {
  BinaryUiStreamView view;
  view.local_status = step.local_status;
  view.diagnostic = step.diagnostic;
  view.host_called = step.host_called;
  view.public_host = step.host;
  view.before = UiObservation(step.before);
  view.after = UiObservation(step.after);
  if (!step.host_called) {
    view.status = BinaryStreamPresentationStatus::PREFLIGHT_REJECTED;
    return view;
  }
  if (!step.candidate && step.host.status == pae::HostStatus::OK) {
    view.status = BinaryStreamPresentationStatus::NO_CANDIDATE;
    return view;
  }
  auto mapped = MapCandidate(step.host, step.candidate, active_view_bytes, input_peak_bytes);
  view.result = std::move(mapped.result);
  view.failure = std::move(mapped.failure);
  view.status =
      mapped.ok
          ? BinaryStreamPresentationStatus::DECODE_SUCCESS
          : (step.local_status ==
                         protocol_lab_binary::public_decode::LocalStatus::MATERIALIZATION_FAILED ||
                     step.diagnostic == protocol_lab_binary::public_decode::StreamDiagnostic::
                                            COPY_FAILED_RESET_REQUIRED
                 ? BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE
                 : BinaryStreamPresentationStatus::DECODE_FAILURE);
  return view;
}

BinaryUiStreamView BinaryHostAdapter::ProjectStreamMappingFailure(
    const protocol_lab_binary::public_decode::StreamStep& step) const noexcept {
  BinaryUiStreamView view;
  view.status = BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE;
  view.local_status = protocol_lab_binary::public_decode::LocalStatus::MATERIALIZATION_FAILED;
  view.diagnostic =
      protocol_lab_binary::public_decode::StreamDiagnostic::COPY_FAILED_RESET_REQUIRED;
  view.host_called = step.host_called;
  view.public_host = step.host;
  view.before = UiObservation(step.before);
  view.after = UiObservation(step.after);
  view.after.reset_required = true;
  auto& failure = view.failure.emplace();
  failure.host_status = step.host.status;
  failure.codec_status = step.host.codec_status;
  failure.accounted_bytes = sizeof(BinaryUiDecodeFailure);
  return view;
}

BinaryUiStreamView BinaryHostAdapter::StreamMappingFailure(
    const protocol_lab_binary::public_decode::StreamStep& step, std::size_t binding,
    std::size_t flow) noexcept {
  const auto flat = owner_->FlowIndex(binding, flow);
  if (flat < stream_mapping_faulted_.size()) stream_mapping_faulted_[flat] = 1U;
  return ProjectStreamMappingFailure(step);
}

BinaryUiDecodeView BinaryHostAdapter::DecodeComplete(std::size_t binding, std::size_t flow,
                                                     const std::vector<std::uint8_t>& frame,
                                                     std::size_t active_view_bytes) {
  if (!IsCompleteDecode(binding, flow)) {
    BinaryUiDecodeView view;
    view.public_host.status = pae::HostStatus::WRONG_INPUT_KIND;
    view.failure.emplace().host_status = view.public_host.status;
    return view;
  }
  const auto& operation = owner_->Decode(owner_->FlowIndex(binding, flow),
                                         {frame.empty() ? nullptr : frame.data(), frame.size()});
  return MapOperation(operation, active_view_bytes);
}
BinaryUiDecodeView BinaryHostAdapter::MapCurrent(std::size_t binding, std::size_t flow,
                                                 std::size_t active_view_bytes) const {
  if (!IsCompleteDecode(binding, flow)) return {};
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? MapOperation(state->current, active_view_bytes)
             : BinaryUiDecodeView{};
}

std::optional<StreamPresentationObservation> BinaryHostAdapter::ObserveStream(
    std::size_t binding, std::size_t flow) const noexcept {
  if (!IsStreamDecode(binding, flow)) return std::nullopt;
  const auto observed = owner_->ObserveStream(binding, flow);
  if (!observed) return std::nullopt;
  auto result = UiObservation(*observed);
  const auto flat = owner_->FlowIndex(binding, flow);
  if (flat < stream_mapping_faulted_.size() && stream_mapping_faulted_[flat])
    result.reset_required = true;
  return result;
}

bool BinaryHostAdapter::StreamContinueAvailable(std::size_t binding,
                                                std::size_t flow) const noexcept {
  if (!IsStreamDecode(binding, flow)) return false;
  const auto flat = owner_->FlowIndex(binding, flow);
  return flat < stream_mapping_faulted_.size() && !stream_mapping_faulted_[flat] &&
         owner_->StreamContinueAvailable(binding, flow);
}

BinaryUiStreamView BinaryHostAdapter::SubmitStream(std::size_t binding, std::size_t flow,
                                                   const std::vector<std::uint8_t>& chunk,
                                                   std::size_t active_view_bytes) {
  BinaryUiStreamView rejected;
  if (!IsStreamDecode(binding, flow)) return rejected;
  const auto flat = owner_->FlowIndex(binding, flow);
  const auto observed = ObserveStream(binding, flow);
  if (observed) rejected.before = rejected.after = *observed;
  if (flat >= stream_mapping_faulted_.size() || stream_mapping_faulted_[flat]) {
    rejected.local_status = protocol_lab_binary::public_decode::LocalStatus::MATERIALIZATION_FAILED;
    rejected.diagnostic = protocol_lab_binary::public_decode::StreamDiagnostic::RESET_REQUIRED;
    rejected.after.reset_required = true;
    return rejected;
  }
  try {
    const auto occupied =
        Add(Add(presentation_retained_bytes_, active_view_bytes), chunk.capacity());
    Require(occupied <= UiViewReserveBytes() / 2U,
            "public Binary stream input peak preflight exceeded");
  } catch (...) {
    rejected.local_status = protocol_lab_binary::public_decode::LocalStatus::RESOURCE_LIMIT;
    rejected.diagnostic = protocol_lab_binary::public_decode::StreamDiagnostic::INVALID_CHUNK;
    return rejected;
  }
  const auto& step = owner_->SubmitStreamChunk(binding, flow, chunk);
  try {
    return MapStreamStep(step, active_view_bytes, chunk.capacity());
  } catch (...) {
    return StreamMappingFailure(step, binding, flow);
  }
}

BinaryUiStreamView BinaryHostAdapter::ContinueStream(std::size_t binding, std::size_t flow,
                                                     std::size_t active_view_bytes) {
  BinaryUiStreamView rejected;
  if (!IsStreamDecode(binding, flow)) return rejected;
  const auto flat = owner_->FlowIndex(binding, flow);
  const auto observed = ObserveStream(binding, flow);
  if (observed) rejected.before = rejected.after = *observed;
  if (flat >= stream_mapping_faulted_.size() || stream_mapping_faulted_[flat]) {
    rejected.local_status = protocol_lab_binary::public_decode::LocalStatus::MATERIALIZATION_FAILED;
    rejected.diagnostic = protocol_lab_binary::public_decode::StreamDiagnostic::RESET_REQUIRED;
    rejected.after.reset_required = true;
    return rejected;
  }
  const auto& step = owner_->ContinueStream(binding, flow);
  try {
    return MapStreamStep(step, active_view_bytes, 0U);
  } catch (...) {
    return StreamMappingFailure(step, binding, flow);
  }
}

BinaryUiStreamView BinaryHostAdapter::MapCurrentStream(std::size_t binding, std::size_t flow,
                                                       std::size_t active_view_bytes) {
  BinaryUiStreamView empty;
  if (!IsStreamDecode(binding, flow)) return empty;
  const auto flat = owner_->FlowIndex(binding, flow);
  const auto* state = owner_->State(flat);
  if (!state || state->stream.step_sequence == 0U || flat >= stream_mapping_faulted_.size()) {
    const auto observed = ObserveStream(binding, flow);
    if (observed) empty.before = empty.after = *observed;
    return empty;
  }
  if (stream_mapping_faulted_[flat])
    return ProjectStreamMappingFailure(state->stream.current);
  try {
    return MapStreamStep(state->stream.current, active_view_bytes, 0U);
  } catch (...) {
    return StreamMappingFailure(state->stream.current, binding, flow);
  }
}

pae::HostStatus BinaryHostAdapter::ResetStream(std::size_t binding, std::size_t flow) noexcept {
  if (!IsStreamDecode(binding, flow)) return pae::HostStatus::WRONG_INPUT_KIND;
  const auto flat = owner_->FlowIndex(binding, flow);
  const auto status = owner_->Reset(flat);
  if (status == pae::HostStatus::OK && flat < stream_mapping_faulted_.size())
    stream_mapping_faulted_[flat] = 0U;
  return status;
}

BinaryUiEncodeView BinaryHostAdapter::MapEncodeOperation(
    const protocol_lab_binary::public_decode::Operation& operation,
    std::size_t active_view_bytes) const {
  const auto budget = UiViewReserveBytes() / 2U;
  Require(presentation_retained_bytes_ <= budget &&
              active_view_bytes <= budget - presentation_retained_bytes_ &&
              limits_.max_result_bytes <= budget - presentation_retained_bytes_ &&
              limits_.max_result_bytes <= copy_controls_.result_copy_limit,
          "public Binary Encode UI result copy preflight exceeded");
  if (copy_controls_.before_result_copy) copy_controls_.before_result_copy(copy_controls_.context);
  BinaryUiEncodeView view;
  view.public_host = operation.host;
  if (operation.host.status == pae::HostStatus::OK && operation.encoded) {
    const auto& source = *operation.encoded;
    Require(source.message_index < description_->messages.size(),
            "public Binary Encode Message identity mismatch");
    const auto& message = description_->messages[source.message_index];
    Require(source.fields.size() == message.fields.size(),
            "public Binary Encode Field count mismatch");
    auto& result = view.result.emplace();
    result.frame = source.frame;
    result.message_index = source.message_index;
    result.message_id = source.message_id;
    result.fields.reserve(source.fields.size());
    for (std::size_t index = 0U; index < source.fields.size(); ++index) {
      const auto& described = message.fields[index];
      const auto& layout = source.fields[index];
      Require(layout.field_index == index && layout.id == described.id,
              "public Binary Encode Field identity mismatch");
      UiFieldResult field;
      field.field_index = index;
      field.id = described.id;
      field.raw_value = "未观察";
      field.logical_value = described.encode_source == FieldEncodeSource::NOT_REFERENCED
                                ? "未参与本次 Encode"
                                : "由 PAE 生成";
      if (layout.byte_range)
        field.actual_range = ByteRange{layout.byte_range->offset, layout.byte_range->length};
      const auto input =
          std::find_if(source.inputs.begin(), source.inputs.end(),
                       [&](const auto& value) { return value.field_index == index; });
      if (input != source.inputs.end()) {
        switch (input->kind) {
          case pae::ValueKind::UINT64:
            field.logical_value = Number(input->uint64_value);
            break;
          case pae::ValueKind::INT64:
            field.logical_value = Number(input->int64_value);
            break;
          case pae::ValueKind::BOOL:
            field.logical_value = input->bool_value ? "true" : "false";
            break;
          case pae::ValueKind::BYTES:
            field.logical_value = Hex(input->bytes);
            break;
          case pae::ValueKind::ENUM: {
            Require(input->enum_entry_index < described.enum_entries.size(),
                    "public Binary Encode Enum identity mismatch");
            const auto& entry = described.enum_entries[input->enum_entry_index];
            const auto& display = entry.display_name.empty() ? entry.id : entry.display_name;
            field.logical_value = display + " [" + entry.id + ']';
            break;
          }
          case pae::ValueKind::DECIMAL64:
            field.logical_value = Decimal(input->decimal.coefficient, input->decimal.scale);
            break;
        }
      }
      result.fields.push_back(std::move(field));
    }
    result.accounted_bytes = AccountResult(result);
    Require(result.accounted_bytes <= limits_.max_result_bytes,
            "public Binary Encode active UI view budget exceeded");
    view.ok = true;
    return view;
  }
  auto& failure = view.failure.emplace();
  failure.host_status = operation.host.status;
  failure.codec_status = operation.host.codec_status;
  failure.accounted_bytes = sizeof(BinaryUiDecodeFailure);
  return view;
}

BinaryUiEncodeView BinaryHostAdapter::EncodeComplete(
    std::size_t binding, std::size_t flow, std::size_t message_index,
    const std::vector<protocol_lab_binary::public_decode::EncodeInput>& inputs,
    std::size_t active_view_bytes) {
  if (!IsCompleteEncode(binding, flow)) {
    BinaryUiEncodeView view;
    view.public_host.status = pae::HostStatus::WRONG_ACTION;
    view.failure.emplace().host_status = view.public_host.status;
    return view;
  }
  const auto& operation = owner_->Encode(binding, message_index, inputs);
  return MapEncodeOperation(operation, active_view_bytes);
}

BinaryUiEncodeView BinaryHostAdapter::MapCurrentEncode(std::size_t binding, std::size_t flow,
                                                       std::size_t active_view_bytes) const {
  if (!IsCompleteEncode(binding, flow)) return {};
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? MapEncodeOperation(state->current, active_view_bytes)
             : BinaryUiEncodeView{};
}
std::size_t BinaryHostAdapter::CurrentResultCopyUpperBoundBytes(std::size_t binding,
                                                                std::size_t flow) const {
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? ResultCopyUpperBound(state->current)
             : 0U;
}
}  // namespace pae::protocol_lab_ui
