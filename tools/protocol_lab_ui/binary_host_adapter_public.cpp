#include "description_mapping.h"
#include "ui_field_result.h"
#include "binary_host_adapter_public.h"
#include "public_binary_description.h"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <utility>

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
template <typename Integer> std::size_t Characters(Integer value) noexcept {
  char buffer[64];
  const auto converted = std::to_chars(std::begin(buffer), std::end(buffer), value);
  return converted.ec == std::errc{} ? static_cast<std::size_t>(converted.ptr - buffer) : 64U;
}
template <typename Integer> std::string Number(Integer value) {
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
}  // namespace

const char* PublicBinaryCodecStatusName(pae::CodecStatus status) noexcept {
#define PAE_CODEC_CASE(name) case pae::CodecStatus::name: return #name
  switch (status) {
    PAE_CODEC_CASE(OK); PAE_CODEC_CASE(INVALID_ARGUMENT);
    PAE_CODEC_CASE(INVALID_COMPILED_PROTOCOL); PAE_CODEC_CASE(RESOURCE_LIMIT_EXCEEDED);
    PAE_CODEC_CASE(ALLOCATION_FAILED); PAE_CODEC_CASE(WORKSPACE_BUSY);
    PAE_CODEC_CASE(INPUT_VALUES_TOO_MANY); PAE_CODEC_CASE(UNKNOWN_MESSAGE);
    PAE_CODEC_CASE(AMBIGUOUS_MESSAGE); PAE_CODEC_CASE(OUTPUT_SLOTS_TOO_SMALL);
    PAE_CODEC_CASE(INTEGRITY_FAILED); PAE_CODEC_CASE(MESSAGE_NOT_ALLOWED);
    PAE_CODEC_CASE(FIELD_REFERENCE_MISMATCH); PAE_CODEC_CASE(DUPLICATE_FIELD);
    PAE_CODEC_CASE(MISSING_FIELD); PAE_CODEC_CASE(TYPE_MISMATCH);
    PAE_CODEC_CASE(VALUE_NOT_REPRESENTABLE); PAE_CODEC_CASE(BYTES_LENGTH_MISMATCH);
    PAE_CODEC_CASE(UNKNOWN_ENUM_VALUE); PAE_CODEC_CASE(ENUM_REFERENCE_MISMATCH);
    PAE_CODEC_CASE(CONSTANT_FIELD_OVERRIDE); PAE_CODEC_CASE(INPUT_OUTPUT_OVERLAP);
    PAE_CODEC_CASE(BUFFER_TOO_SMALL); PAE_CODEC_CASE(FINAL_REVIEW_FAILED);
    PAE_CODEC_CASE(ASCII_CHARACTER_NOT_ALLOWED); PAE_CODEC_CASE(ASCII_TERMINATOR_CONFLICT);
    PAE_CODEC_CASE(OPERATION_NOT_SUPPORTED); PAE_CODEC_CASE(COMPUTED_FIELD_OVERRIDE);
    PAE_CODEC_CASE(LENGTH_MISMATCH); PAE_CODEC_CASE(INTERNAL_ERROR);
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
                !identity.config_sha256.empty(), "public Binary identity is incomplete");
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
                limits.max_frame_bytes <= hard.max_frame_bytes,
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
      Require(binding.action == pae::HostAction::DECODE && binding.streams &&
                  binding.streams <= 64U - channels,
              "public Binary H2 supports bounded complete Decode bindings only");
      channels += binding.streams;
      public_bindings.push_back({binding.endpoint, binding.pipeline_id, binding.streams});
    }
    adapter->utf16_draft_reserve_bytes_ = Multiply(
        channels + 1U, Add(Multiply(4U, limits.max_frame_bytes),
                           sizeof(std::u16string)));
    const auto mapped_description_bytes = AccountDescription(*adapter->description_);
    Require(mapped_description_bytes <= limits.max_description_bytes,
            "public Binary UI description exceeds bound");
    adapter->description_copy_upper_bound_bytes_ =
        Multiply(2U, mapped_description_bytes);
    Require(adapter->description_copy_upper_bound_bytes_ <=
                adapter->copy_controls_.description_copy_limit,
            "public Binary description copy preflight exceeded");
    auto before_copy = Add(mapped_description_bytes,
                           adapter->description_copy_upper_bound_bytes_);
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
    adapter->ui_description_bytes_ = Add(adapter->ui_description_bytes_,
        Multiply(adapter->bindings_.capacity(), sizeof(BinaryHostBinding)));
    for (const auto& binding : adapter->bindings_) {
      ChargeString(binding.endpoint, adapter->ui_description_bytes_);
      ChargeString(binding.pipeline_id, adapter->ui_description_bytes_);
    }
    const auto previous_bytes = previous ? previous->owner_->InstanceAdmissionBytes() : 0U;
    auto prepared = protocol_lab_binary::public_decode::Adapter::AdoptCompiled(
        std::move(compiled), std::move(public_bindings), limits, previous_bytes);
    Require(prepared.status == protocol_lab_binary::public_decode::LocalStatus::OK &&
                prepared.adapter, "public Binary H1 adoption or Host binding failed");
    adapter->owner_ = std::move(prepared.adapter);
    adapter->drafts_.resize(channels);
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

const DocumentDescription& BinaryHostAdapter::Description() const noexcept {
  return *description_;
}
DocumentDescription BinaryHostAdapter::TakeDescription() {
  Require(publication_description_ != nullptr, "public Binary description already published");
  auto moved = std::move(*publication_description_);
  publication_description_.reset();
  return moved;
}
std::size_t BinaryHostAdapter::AccountDescriptionBytes(
    const DocumentDescription& description) { return AccountDescription(description); }
std::size_t BinaryHostAdapter::AccountedInstanceBytes() const noexcept {
  if (!owner_) return (std::numeric_limits<std::size_t>::max)();
  try {
    auto total = Add(owner_->InstanceAdmissionBytes(), ui_description_bytes_);
    total = Add(total, externally_retained_bytes_);
    total = Add(total, ui_view_reserve_bytes_);
    total = Add(total, hex_preview_reserve_bytes_);
    return Add(total, utf16_draft_reserve_bytes_);
  } catch (...) { return (std::numeric_limits<std::size_t>::max)(); }
}
std::size_t BinaryHostAdapter::AccountedPreparationBytes() const noexcept {
  try { return Add(AccountedInstanceBytes(), preparation_coexisting_bytes_); }
  catch (...) { return (std::numeric_limits<std::size_t>::max)(); }
}
std::size_t BinaryHostAdapter::FlowCount(std::size_t binding) const noexcept {
  return owner_ ? owner_->FlowCount(binding) : 0U;
}
bool BinaryHostAdapter::IsCompleteDecode(std::size_t binding, std::size_t flow) const noexcept {
  return binding < bindings_.size() && flow < FlowCount(binding) &&
         bindings_[binding].action == pae::HostAction::DECODE;
}
bool BinaryHostAdapter::SetPresentationRetainedBytes(std::size_t bytes) noexcept {
  if (bytes > UiViewReserveBytes() / 2U) return false;
  presentation_retained_bytes_ = bytes;
  return true;
}
std::u16string_view BinaryHostAdapter::Draft(std::size_t binding,
                                             std::size_t flow) const noexcept {
  const auto flat = owner_->FlowIndex(binding, flow);
  return flat < drafts_.size() ? std::u16string_view(drafts_[flat]) : std::u16string_view{};
}
std::size_t BinaryHostAdapter::DraftLimit(std::size_t,
                                          std::size_t) const noexcept {
  return limits_.max_frame_bytes * 2U;
}
void BinaryHostAdapter::SaveAndSelect(std::u16string_view draft,
                                      std::size_t binding, std::size_t flow) {
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
  Require(owner_->SetDraft(source, ascii), "public Binary Flow draft exceeds budget");
  drafts_[source] = std::move(pending);
  selected_binding_ = binding;
  selected_flow_ = flow;
}
const void* BinaryHostAdapter::Current(std::size_t binding,
                                       std::size_t flow) const noexcept {
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? static_cast<const void*>(&state->current) : nullptr;
}

std::size_t BinaryHostAdapter::ResultCopyUpperBound(
    const protocol_lab_binary::public_decode::Operation& operation) const {
  const auto& candidate = operation.candidate;
  if (!candidate || !candidate->success || !candidate->message_index ||
      operation.host.status != pae::HostStatus::OK)
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
      case pae::ValueKind::UINT64: raw = logical = Characters(field.uint64_value); break;
      case pae::ValueKind::INT64: raw = logical = Characters(field.int64_value); break;
      case pae::ValueKind::BOOL:
        raw = std::string_view{"未单独提供"}.size();
        logical = field.bool_value ? 4U : 5U; break;
      case pae::ValueKind::BYTES: raw = logical = Multiply(field.bytes.size(), 2U); break;
      case pae::ValueKind::ENUM: {
        Require(field.enum_raw.has_value(), "public Binary Enum result has no raw value");
        raw = Characters(*field.enum_raw);
        logical = 7U;
        if (field.known_enum_flat_index) {
          const auto& entries = message.fields[i].enum_entries;
          const auto found = std::find_if(entries.begin(), entries.end(),
              [&](const auto& item) { return item.raw_value == *field.enum_raw; });
          Require(found != entries.end(), "public Binary known Enum mapping mismatch");
          logical = Add(Add(found->display_name.empty() ? found->id.size()
                                                    : found->display_name.size(),
                            found->id.size()), 3U);
        }
        break;
      }
      case pae::ValueKind::DECIMAL64:
        Require(field.conversion_raw_kind.has_value(), "public Binary Decimal64 has no raw integer");
        raw = *field.conversion_raw_kind == pae::RawIntegerKind::UINT64
                  ? Characters(*field.conversion_raw_uint64)
                  : Characters(*field.conversion_raw_int64);
        logical = Add(Add(Characters(field.decimal.coefficient), 1U),
                      Characters(field.decimal.scale)); break;
    }
    total = Add(total, StringCapacityUpper(raw) + 1U);
    total = Add(total, StringCapacityUpper(logical) + 1U);
  }
  return total;
}

BinaryUiDecodeView BinaryHostAdapter::MapOperation(
    const protocol_lab_binary::public_decode::Operation& operation,
    std::size_t active_view_bytes) const {
  const auto upper = ResultCopyUpperBound(operation);
  const auto budget = UiViewReserveBytes() / 2U;
  Require(presentation_retained_bytes_ <= budget &&
              active_view_bytes <= budget - presentation_retained_bytes_ &&
              upper <= budget - presentation_retained_bytes_ &&
              upper <= copy_controls_.result_copy_limit,
          "public Binary UI result copy preflight exceeded");
  if (copy_controls_.before_result_copy)
    copy_controls_.before_result_copy(copy_controls_.context);
  BinaryUiDecodeView view;
  view.public_host = operation.host;
  if (operation.host.status == pae::HostStatus::OK && operation.candidate &&
      operation.candidate->success && operation.candidate->message_index) {
    const auto& source = *operation.candidate;
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
        field.actual_range = ByteRange{source_field.byte_range->offset,
                                       source_field.byte_range->length};
      switch (source_field.kind) {
        case pae::ValueKind::UINT64:
          field.raw_value = Number(source_field.uint64_value);
          field.logical_value = field.raw_value; break;
        case pae::ValueKind::INT64:
          field.raw_value = Number(source_field.int64_value);
          field.logical_value = field.raw_value; break;
        case pae::ValueKind::BOOL:
          field.raw_value = "未单独提供";
          field.logical_value = source_field.bool_value ? "true" : "false"; break;
        case pae::ValueKind::BYTES:
          field.raw_value = field.logical_value = Hex(source_field.bytes); break;
        case pae::ValueKind::ENUM: {
          field.raw_value = Number(*source_field.enum_raw);
          field.logical_value = "unknown";
          if (source_field.known_enum_flat_index) {
            const auto found = std::find_if(described.enum_entries.begin(),
                                            described.enum_entries.end(),
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
          field.logical_value = Decimal(source_field.decimal.coefficient,
                                        source_field.decimal.scale); break;
      }
      result.fields.push_back(std::move(field));
    }
    result.accounted_bytes = AccountResult(result);
    Require(result.accounted_bytes <= upper &&
                result.accounted_bytes <= budget - presentation_retained_bytes_,
            "public Binary active UI view budget exceeded");
    view.ok = true;
    return view;
  }
  auto& failure = view.failure.emplace();
  failure.host_status = operation.host.status;
  failure.codec_status = operation.host.codec_status;
  if (operation.candidate) {
    failure.diagnostic_frame = operation.candidate->frame;
    failure.message_index = operation.candidate->message_index;
    // A failed flat Field index alone does not establish a Message identity.
  }
  failure.accounted_bytes = sizeof(BinaryUiDecodeFailure) + failure.diagnostic_frame.capacity();
  Require(failure.accounted_bytes <= upper &&
              failure.accounted_bytes <= budget - presentation_retained_bytes_,
          "public Binary failure UI view budget exceeded");
  return view;
}

BinaryUiDecodeView BinaryHostAdapter::DecodeComplete(
    std::size_t binding, std::size_t flow, const std::vector<std::uint8_t>& frame,
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
BinaryUiDecodeView BinaryHostAdapter::MapCurrent(
    std::size_t binding, std::size_t flow, std::size_t active_view_bytes) const {
  if (!IsCompleteDecode(binding, flow)) return {};
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? MapOperation(state->current, active_view_bytes) : BinaryUiDecodeView{};
}
std::size_t BinaryHostAdapter::CurrentResultCopyUpperBoundBytes(
    std::size_t binding, std::size_t flow) const {
  const auto* state = owner_->State(owner_->FlowIndex(binding, flow));
  return state && (state->current.host.codec_attempted ||
                   state->current.host.status == pae::HostStatus::CALLBACK_FAILED)
             ? ResultCopyUpperBound(state->current) : 0U;
}
}  // namespace pae::protocol_lab_ui
