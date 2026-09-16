#include "pae/codec.h"

#include <atomic>
#include <limits>
#include <memory>
#include <new>
#include <utility>

#include "../protocol_core/complete_record_codec.h"
#include "compiled_state_internal.h"
#if defined(PAE_ENABLE_PUBLIC_CODEC_TEST_HOOKS)
#include "codec_test_support.h"
#endif

namespace pae {
namespace {

namespace core = protocol_core;
namespace internal = public_api_internal;

constexpr std::size_t kInvalidIndex = (std::numeric_limits<std::size_t>::max)();

bool AddChecked(std::size_t value, std::size_t& total) noexcept {
  if (value > (std::numeric_limits<std::size_t>::max)() - total) return false;
  total += value;
  return true;
}

bool MultiplyChecked(std::size_t count, std::size_t element_size, std::size_t& output) noexcept {
  if (count != 0U && element_size > (std::numeric_limits<std::size_t>::max)() / count) {
    return false;
  }
  output = count * element_size;
  return true;
}

CodecStatus MapStatus(core::CodecStatus status) noexcept {
  switch (status) {
    case core::CodecStatus::OK:
      return CodecStatus::OK;
    case core::CodecStatus::INVALID_ARGUMENT:
      return CodecStatus::INVALID_ARGUMENT;
    case core::CodecStatus::INVALID_PLAN:
    case core::CodecStatus::WORKSPACE_PLAN_MISMATCH:
      return CodecStatus::INVALID_COMPILED_PROTOCOL;
    case core::CodecStatus::WORKSPACE_BUSY:
      return CodecStatus::WORKSPACE_BUSY;
    case core::CodecStatus::UNKNOWN_MESSAGE:
      return CodecStatus::UNKNOWN_MESSAGE;
    case core::CodecStatus::AMBIGUOUS_MESSAGE:
      return CodecStatus::AMBIGUOUS_MESSAGE;
    case core::CodecStatus::OUTPUT_SLOTS_TOO_SMALL:
      return CodecStatus::OUTPUT_SLOTS_TOO_SMALL;
    case core::CodecStatus::INTEGRITY_FAILED:
      return CodecStatus::INTEGRITY_FAILED;
    case core::CodecStatus::MESSAGE_NOT_ALLOWED:
      return CodecStatus::MESSAGE_NOT_ALLOWED;
    case core::CodecStatus::FIELD_REFERENCE_MISMATCH:
      return CodecStatus::FIELD_REFERENCE_MISMATCH;
    case core::CodecStatus::DUPLICATE_FIELD:
      return CodecStatus::DUPLICATE_FIELD;
    case core::CodecStatus::MISSING_FIELD:
      return CodecStatus::MISSING_FIELD;
    case core::CodecStatus::TYPE_MISMATCH:
      return CodecStatus::TYPE_MISMATCH;
    case core::CodecStatus::VALUE_NOT_REPRESENTABLE:
      return CodecStatus::VALUE_NOT_REPRESENTABLE;
    case core::CodecStatus::BYTES_LENGTH_MISMATCH:
      return CodecStatus::BYTES_LENGTH_MISMATCH;
    case core::CodecStatus::UNKNOWN_ENUM_VALUE:
      return CodecStatus::UNKNOWN_ENUM_VALUE;
    case core::CodecStatus::ENUM_REFERENCE_MISMATCH:
      return CodecStatus::ENUM_REFERENCE_MISMATCH;
    case core::CodecStatus::CONSTANT_FIELD_OVERRIDE:
      return CodecStatus::CONSTANT_FIELD_OVERRIDE;
    case core::CodecStatus::INPUT_OUTPUT_OVERLAP:
      return CodecStatus::INPUT_OUTPUT_OVERLAP;
    case core::CodecStatus::BUFFER_TOO_SMALL:
      return CodecStatus::BUFFER_TOO_SMALL;
    case core::CodecStatus::FINAL_REVIEW_FAILED:
      return CodecStatus::FINAL_REVIEW_FAILED;
    case core::CodecStatus::INTERNAL_ERROR:
      return CodecStatus::INTERNAL_ERROR;
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    case core::CodecStatus::ASCII_CHARACTER_NOT_ALLOWED:
      return CodecStatus::ASCII_CHARACTER_NOT_ALLOWED;
    case core::CodecStatus::ASCII_TERMINATOR_CONFLICT:
      return CodecStatus::ASCII_TERMINATOR_CONFLICT;
    case core::CodecStatus::OPERATION_NOT_SUPPORTED:
      return CodecStatus::OPERATION_NOT_SUPPORTED;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    case core::CodecStatus::COMPUTED_FIELD_OVERRIDE:
      return CodecStatus::COMPUTED_FIELD_OVERRIDE;
    case core::CodecStatus::LENGTH_MISMATCH:
      return CodecStatus::LENGTH_MISMATCH;
#endif
  }
  return CodecStatus::INTERNAL_ERROR;
}

ConversionError MapConversionError(core::ConversionError error) noexcept {
  switch (error) {
    case core::ConversionError::NONE:
      return ConversionError::NONE;
    case core::ConversionError::DECIMAL_SCALE_OUT_OF_RANGE:
      return ConversionError::DECIMAL_SCALE_OUT_OF_RANGE;
    case core::ConversionError::RAW_NOT_INTEGRAL:
      return ConversionError::RAW_NOT_INTEGRAL;
    case core::ConversionError::RAW_OUT_OF_RANGE:
      return ConversionError::RAW_OUT_OF_RANGE;
    case core::ConversionError::LOGICAL_OUT_OF_RANGE:
      return ConversionError::LOGICAL_OUT_OF_RANGE;
  }
  return ConversionError::NONE;
}

ValueKind MapValueKind(core::LogicalValueKind kind) noexcept {
  switch (kind) {
    case core::LogicalValueKind::UINT64:
      return ValueKind::UINT64;
    case core::LogicalValueKind::INT64:
      return ValueKind::INT64;
    case core::LogicalValueKind::BOOL:
      return ValueKind::BOOL;
    case core::LogicalValueKind::BYTES:
      return ValueKind::BYTES;
    case core::LogicalValueKind::ENUM:
      return ValueKind::ENUM;
    case core::LogicalValueKind::DECIMAL64:
      return ValueKind::DECIMAL64;
  }
  return ValueKind::UINT64;
}

std::optional<std::size_t> ResolveFlatFieldIndex(const internal::CompiledState& state,
                                                 std::size_t message_index,
                                                 std::size_t field_index) noexcept {
  const auto& metadata = state.Artifacts().Description();
  if (message_index >= metadata.Messages().size()) return std::nullopt;
  const auto& message = metadata.Messages()[message_index];
  if (field_index >= message.field_count || message.field_begin > metadata.Fields().size() ||
      field_index > metadata.Fields().size() - message.field_begin) {
    return std::nullopt;
  }
  const std::size_t flat = message.field_begin + field_index;
  return flat < metadata.Fields().size() ? std::optional<std::size_t>{flat} : std::nullopt;
}

std::optional<std::size_t> FlatEnumIndex(const internal::CompiledState& state,
                                         const core::DecodedFieldSlot& slot) noexcept {
  if (!slot.enum_value.known) return std::nullopt;
  const auto field_flat =
      ResolveFlatFieldIndex(state, slot.field.message_index, slot.field.field_index);
  if (!field_flat.has_value()) return std::nullopt;
  const auto& metadata = state.Artifacts().Description();
  const auto& field = metadata.Fields()[*field_flat];
  const std::size_t entry = slot.enum_value.reference.entry_index;
  if (entry >= field.enum_count || field.enum_begin > metadata.Enums().size() ||
      entry > metadata.Enums().size() - field.enum_begin) {
    return std::nullopt;
  }
  const std::size_t flat = field.enum_begin + entry;
  return flat < metadata.Enums().size() ? std::optional<std::size_t>{flat} : std::nullopt;
}

std::size_t CoreWorkspaceAllocationCount(
    const protocol_plan::ExecutionResourceLayout& layout) noexcept {
  std::size_t count = 0U;
  count += layout.encode_value_index_count != 0U ? 1U : 0U;
  count += layout.encode_presence_word_count != 0U ? 1U : 0U;
  count += layout.bit_container_value_count != 0U ? 1U : 0U;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  count += layout.conversion_value_count != 0U ? 6U : 0U;
#endif
  return count;
}

class FacadeLease final {
 public:
  explicit FacadeLease(std::atomic_flag& in_use) noexcept
      : in_use_(in_use), acquired_(!in_use_.test_and_set(std::memory_order_acquire)) {}
  FacadeLease(const FacadeLease&) = delete;
  FacadeLease& operator=(const FacadeLease&) = delete;
  ~FacadeLease() {
    if (acquired_) in_use_.clear(std::memory_order_release);
  }
  [[nodiscard]] bool Acquired() const noexcept { return acquired_; }

 private:
  std::atomic_flag& in_use_;
  bool acquired_ = false;
};

#if defined(PAE_ENABLE_PUBLIC_CODEC_TEST_HOOKS)
std::atomic<internal::test_only::CodecEnteredHook> g_entered_hook{nullptr};
std::atomic<void*> g_entered_hook_context{nullptr};

void InvokeEnteredHook() noexcept {
  const auto hook = g_entered_hook.load(std::memory_order_acquire);
  if (hook != nullptr) hook(g_entered_hook_context.load(std::memory_order_relaxed));
}
#else
void InvokeEnteredHook() noexcept {}
#endif

}  // namespace

struct CompleteRecordCodec::Impl final {
  Impl(internal::CompiledStateRef shared_state, std::size_t decoded_capacity,
       std::size_t encode_capacity, ExecutionMemoryReport report_value)
      : state(std::move(shared_state)),
        workspace(*state->Artifacts().Plan()),
        decoded_slots(decoded_capacity == 0U
                          ? nullptr
                          : std::make_unique<core::DecodedFieldSlot[]>(decoded_capacity)),
        decoded_slot_capacity(decoded_capacity),
        mapped_values(encode_capacity == 0U
                          ? nullptr
                          : std::make_unique<core::EncodeFieldValue[]>(encode_capacity)),
        mapped_value_capacity(encode_capacity),
        report(report_value) {}

  [[nodiscard]] bool IsPublished(std::uint64_t expected_generation) const noexcept {
    return published && generation == expected_generation;
  }

  void BeginGuardedCall() noexcept {
    ++generation;
    published = false;
    published_message_index = kInvalidIndex;
    published_field_count = 0U;
  }

  internal::CompiledStateRef state;
  core::ExecutionWorkspace workspace;
  std::unique_ptr<core::DecodedFieldSlot[]> decoded_slots;
  std::size_t decoded_slot_capacity = 0U;
  std::unique_ptr<core::EncodeFieldValue[]> mapped_values;
  std::size_t mapped_value_capacity = 0U;
  ExecutionMemoryReport report;
  std::atomic_flag facade_in_use = ATOMIC_FLAG_INIT;
  std::uint64_t generation = 0U;
  bool published = false;
  std::size_t published_message_index = kInvalidIndex;
  std::size_t published_field_count = 0U;
};

EncodeValue EncodeValue::UInt64(FieldSelector field, std::uint64_t value) noexcept {
  EncodeValue result;
  result.field_ = field;
  result.kind_ = ValueKind::UINT64;
  result.uint64_value_ = value;
  return result;
}

EncodeValue EncodeValue::Int64(FieldSelector field, std::int64_t value) noexcept {
  EncodeValue result;
  result.field_ = field;
  result.kind_ = ValueKind::INT64;
  result.int64_value_ = value;
  return result;
}

EncodeValue EncodeValue::Bool(FieldSelector field, bool value) noexcept {
  EncodeValue result;
  result.field_ = field;
  result.kind_ = ValueKind::BOOL;
  result.bool_value_ = value;
  return result;
}

EncodeValue EncodeValue::Bytes(FieldSelector field, ByteView value) noexcept {
  EncodeValue result;
  result.field_ = field;
  result.kind_ = ValueKind::BYTES;
  result.bytes_value_ = value;
  return result;
}

EncodeValue EncodeValue::Enum(EnumSelector value) noexcept {
  EncodeValue result;
  result.field_ = FieldSelector{value.message_index, value.field_index};
  result.enum_ = value;
  result.kind_ = ValueKind::ENUM;
  return result;
}

EncodeValue EncodeValue::Decimal(FieldSelector field, Decimal64 value) noexcept {
  EncodeValue result;
  result.field_ = field;
  result.kind_ = ValueKind::DECIMAL64;
  result.decimal64_value_ = value;
  return result;
}

CompleteRecordCodec::CompleteRecordCodec(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

CompleteRecordCodec::CompleteRecordCodec(CompleteRecordCodec&& other) noexcept
    : impl_(std::move(other.impl_)) {
  other.AdvanceViewEpoch();
}

CompleteRecordCodec& CompleteRecordCodec::operator=(CompleteRecordCodec&& other) noexcept {
  AdvanceViewEpoch();
  if (this != &other) {
    other.AdvanceViewEpoch();
    impl_ = std::move(other.impl_);
  }
  return *this;
}

CompleteRecordCodec::~CompleteRecordCodec() = default;

void CompleteRecordCodec::AdvanceViewEpoch() noexcept {
  ++view_epoch_;
  if (view_epoch_ == 0U) ++view_epoch_;
}

DecodeResult CompleteRecordCodec::Decode(std::size_t pipeline_index, ByteView frame) noexcept {
  DecodeResult result;
  if (impl_ == nullptr) {
    result.status = CodecStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }
  FacadeLease lease(impl_->facade_in_use);
  if (!lease.Acquired()) {
    result.status = CodecStatus::WORKSPACE_BUSY;
    return result;
  }
  InvokeEnteredHook();
  impl_->BeginGuardedCall();

  const auto* plan = impl_->state->Artifacts().Plan();
  const core::DecodeResult decoded = core::DecodeCompleteRecord(
      *plan, impl_->workspace, pipeline_index, core::ByteView{frame.data, frame.size},
      impl_->decoded_slots.get(), impl_->decoded_slot_capacity);
  result.status = MapStatus(decoded.status);
  result.required_field_count = decoded.required_field_count;
  result.conversion_error = MapConversionError(decoded.conversion_error);
  result.output_tainted = decoded.tainted;
  if (decoded.message_index != core::kInvalidIndex) {
    const auto& pipelines = plan->Pipelines();
    const auto& pipeline_execution = plan->PipelineExecutionPlans();
    const auto& messages = plan->MessageExecutionPlans();
    const auto& described_messages = impl_->state->Artifacts().Description().Messages();
    bool associated = false;
    if (pipeline_index < pipelines.size() && decoded.message_index < messages.size() &&
        decoded.message_index < described_messages.size() &&
        pipeline_index < pipeline_execution.size()) {
      for (const std::size_t candidate : pipelines[pipeline_index].message_indices) {
        if (candidate == decoded.message_index) associated = true;
      }
      const std::size_t word = decoded.message_index / 64U;
      const auto& allowed = pipeline_execution[pipeline_index].allowed_message_words;
      associated = associated && word < allowed.size() &&
                   (allowed[word] & (std::uint64_t{1U} << (decoded.message_index % 64U))) != 0U;
    }
    if (!associated) {
      result.status = CodecStatus::INTERNAL_ERROR;
      return result;
    }
    result.matched_message_index = decoded.message_index;
  }
  if (decoded.status == core::CodecStatus::OK && !result.matched_message_index.has_value()) {
    result.status = CodecStatus::INTERNAL_ERROR;
    return result;
  }
  if (decoded.failed_field_index != core::kInvalidIndex) {
    result.failed_field_flat_index =
        ResolveFlatFieldIndex(*impl_->state, decoded.message_index, decoded.failed_field_index);
  }
  if (decoded.status == core::CodecStatus::OK) {
    impl_->published = true;
    impl_->published_message_index = decoded.message_index;
    impl_->published_field_count = decoded.field_count;
    result.record = DecodedRecordView{this, view_epoch_, impl_->generation};
  }
  return result;
}

EncodeResult CompleteRecordCodec::Encode(std::size_t pipeline_index, std::size_t message_index,
                                         const EncodeValue* values, std::size_t value_count,
                                         MutableByteBuffer output) noexcept {
  EncodeResult result;
  if (impl_ == nullptr) {
    result.status = CodecStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }
  FacadeLease lease(impl_->facade_in_use);
  if (!lease.Acquired()) {
    result.status = CodecStatus::WORKSPACE_BUSY;
    return result;
  }
  InvokeEnteredHook();
  impl_->BeginGuardedCall();

  if (value_count > impl_->mapped_value_capacity) {
    result.status = CodecStatus::INPUT_VALUES_TOO_MANY;
    return result;
  }
  if (values == nullptr && value_count != 0U) {
    result.status = CodecStatus::INVALID_ARGUMENT;
    return result;
  }

  const auto* plan = impl_->state->Artifacts().Plan();
  for (std::size_t index = 0U; index < value_count; ++index) {
    const EncodeValue& source = values[index];
    core::EncodeFieldValue& target = impl_->mapped_values[index];
    target = core::EncodeFieldValue{};
    target.field = core::FieldRef{plan, source.field_.message_index, source.field_.field_index};
    switch (source.kind_) {
      case ValueKind::UINT64:
        target.value_kind = core::LogicalValueKind::UINT64;
        target.uint64_value = source.uint64_value_;
        break;
      case ValueKind::INT64:
        target.value_kind = core::LogicalValueKind::INT64;
        target.int64_value = source.int64_value_;
        break;
      case ValueKind::BOOL:
        target.value_kind = core::LogicalValueKind::BOOL;
        target.bool_value = source.bool_value_;
        break;
      case ValueKind::BYTES:
        target.value_kind = core::LogicalValueKind::BYTES;
        target.bytes_value = core::ByteView{source.bytes_value_.data, source.bytes_value_.size};
        break;
      case ValueKind::ENUM:
        target.value_kind = core::LogicalValueKind::ENUM;
        target.enum_value = core::EnumValueRef{plan, source.enum_.message_index,
                                               source.enum_.field_index, source.enum_.entry_index};
        break;
      case ValueKind::DECIMAL64:
        target.value_kind = core::LogicalValueKind::DECIMAL64;
        target.decimal64_value =
            core::Decimal64{source.decimal64_value_.coefficient, source.decimal64_value_.scale};
        break;
    }
  }

  const core::EncodeResult encoded = core::EncodeCompleteRecord(
      *plan, impl_->workspace, pipeline_index, message_index, impl_->mapped_values.get(),
      value_count, core::MutableByteBuffer{output.data, output.capacity});
  result.status = MapStatus(encoded.status);
  result.bytes_written = encoded.bytes_written;
  result.required_size = encoded.required_size;
  result.conversion_error = MapConversionError(encoded.conversion_error);
  if (encoded.failed_value_index != core::kInvalidIndex) {
    result.failed_value_index = encoded.failed_value_index;
  }
  if (encoded.failed_field_index != core::kInvalidIndex) {
    result.failed_field_flat_index =
        ResolveFlatFieldIndex(*impl_->state, message_index, encoded.failed_field_index);
  }
  return result;
}

ExecutionMemoryReport CompleteRecordCodec::MemoryReport() const noexcept {
  return impl_ == nullptr ? ExecutionMemoryReport{} : impl_->report;
}

DecodedRecordView::DecodedRecordView(const CompleteRecordCodec* owner, std::uint64_t owner_epoch,
                                     std::uint64_t generation) noexcept
    : owner_(owner), owner_epoch_(owner_epoch), generation_(generation) {}

bool DecodedRecordView::HasValue() const noexcept {
  return owner_ != nullptr && owner_->view_epoch_ == owner_epoch_ && owner_->impl_ != nullptr &&
         owner_->impl_->IsPublished(generation_);
}

std::size_t DecodedRecordView::MessageIndex() const noexcept {
  return HasValue() ? owner_->impl_->published_message_index : kInvalidIndex;
}

std::size_t DecodedRecordView::FieldCount() const noexcept {
  return HasValue() ? owner_->impl_->published_field_count : 0U;
}

std::optional<DecodedFieldView> DecodedRecordView::Field(std::size_t ordinal) const noexcept {
  if (!HasValue() || ordinal >= owner_->impl_->published_field_count) return std::nullopt;
  return DecodedFieldView{owner_, owner_epoch_, generation_, ordinal};
}

DecodedFieldView::DecodedFieldView(const CompleteRecordCodec* owner, std::uint64_t owner_epoch,
                                   std::uint64_t generation, std::size_t ordinal) noexcept
    : owner_(owner), owner_epoch_(owner_epoch), generation_(generation), ordinal_(ordinal) {}

bool DecodedFieldView::HasValue() const noexcept {
  return owner_ != nullptr && owner_->view_epoch_ == owner_epoch_ && owner_->impl_ != nullptr &&
         owner_->impl_->IsPublished(generation_) && ordinal_ < owner_->impl_->published_field_count;
}

std::size_t DecodedFieldView::FlatFieldIndex() const noexcept {
  if (!HasValue()) return kInvalidIndex;
  const auto& slot = owner_->impl_->decoded_slots[ordinal_];
  return ResolveFlatFieldIndex(*owner_->impl_->state, slot.field.message_index,
                               slot.field.field_index)
      .value_or(kInvalidIndex);
}

FieldSelector DecodedFieldView::Field() const noexcept {
  if (!HasValue()) return {};
  const auto& field = owner_->impl_->decoded_slots[ordinal_].field;
  return FieldSelector{field.message_index, field.field_index};
}

ValueKind DecodedFieldView::Kind() const noexcept {
  return HasValue() ? MapValueKind(owner_->impl_->decoded_slots[ordinal_].value_kind)
                    : ValueKind::UINT64;
}

std::optional<std::uint64_t> DecodedFieldView::UInt64() const noexcept {
  if (!HasValue() || Kind() != ValueKind::UINT64) return std::nullopt;
  return owner_->impl_->decoded_slots[ordinal_].uint64_value;
}

std::optional<std::int64_t> DecodedFieldView::Int64() const noexcept {
  if (!HasValue() || Kind() != ValueKind::INT64) return std::nullopt;
  return owner_->impl_->decoded_slots[ordinal_].int64_value;
}

std::optional<bool> DecodedFieldView::Bool() const noexcept {
  if (!HasValue() || Kind() != ValueKind::BOOL) return std::nullopt;
  return owner_->impl_->decoded_slots[ordinal_].bool_value;
}

std::optional<ByteView> DecodedFieldView::Bytes() const noexcept {
  if (!HasValue() || Kind() != ValueKind::BYTES) return std::nullopt;
  const auto& value = owner_->impl_->decoded_slots[ordinal_].bytes_value;
  return ByteView{value.data, value.size};
}

std::optional<std::uint64_t> DecodedFieldView::EnumRawValue() const noexcept {
  if (!HasValue() || Kind() != ValueKind::ENUM) return std::nullopt;
  return owner_->impl_->decoded_slots[ordinal_].enum_value.raw_value;
}

std::optional<std::size_t> DecodedFieldView::KnownEnumFlatIndex() const noexcept {
  if (!HasValue() || Kind() != ValueKind::ENUM) return std::nullopt;
  return FlatEnumIndex(*owner_->impl_->state, owner_->impl_->decoded_slots[ordinal_]);
}

std::optional<Decimal64> DecodedFieldView::Decimal() const noexcept {
  if (!HasValue() || Kind() != ValueKind::DECIMAL64) return std::nullopt;
  const auto& value = owner_->impl_->decoded_slots[ordinal_].decimal64_value;
  return Decimal64{value.coefficient, value.scale};
}

std::optional<RawIntegerKind> DecodedFieldView::ConversionRawKind() const noexcept {
  if (!HasValue()) return std::nullopt;
  const auto& slot = owner_->impl_->decoded_slots[ordinal_];
  core::RawIntegerValue raw;
  for (std::size_t index = 0U; index < owner_->impl_->workspace.LastRawIntegerCount(); ++index) {
    if (owner_->impl_->workspace.GetLastRawInteger(index, raw) &&
        raw.field.message_index == slot.field.message_index &&
        raw.field.field_index == slot.field.field_index) {
      return raw.kind == core::RawIntegerKind::INT64 ? RawIntegerKind::INT64
                                                     : RawIntegerKind::UINT64;
    }
  }
  return std::nullopt;
}

std::optional<std::uint64_t> DecodedFieldView::ConversionRawUInt64() const noexcept {
  if (!HasValue()) return std::nullopt;
  const auto& slot = owner_->impl_->decoded_slots[ordinal_];
  core::RawIntegerValue raw;
  for (std::size_t index = 0U; index < owner_->impl_->workspace.LastRawIntegerCount(); ++index) {
    if (owner_->impl_->workspace.GetLastRawInteger(index, raw) &&
        raw.field.message_index == slot.field.message_index &&
        raw.field.field_index == slot.field.field_index &&
        raw.kind == core::RawIntegerKind::UINT64) {
      return raw.uint64_value;
    }
  }
  return std::nullopt;
}

std::optional<std::int64_t> DecodedFieldView::ConversionRawInt64() const noexcept {
  if (!HasValue()) return std::nullopt;
  const auto& slot = owner_->impl_->decoded_slots[ordinal_];
  core::RawIntegerValue raw;
  for (std::size_t index = 0U; index < owner_->impl_->workspace.LastRawIntegerCount(); ++index) {
    if (owner_->impl_->workspace.GetLastRawInteger(index, raw) &&
        raw.field.message_index == slot.field.message_index &&
        raw.field.field_index == slot.field.field_index &&
        raw.kind == core::RawIntegerKind::INT64) {
      return raw.int64_value;
    }
  }
  return std::nullopt;
}

CompleteRecordCodecCreateResult CreateCompleteRecordCodec(
    const CompiledProtocol& compiled, const CompleteRecordCodecOptions& options) noexcept {
  CompleteRecordCodecCreateResult result;
  internal::CompiledStateRef state = detail::CompiledProtocolAccess::Acquire(compiled);
  if (!state || state->Artifacts().Plan() == nullptr) {
    result.status = CodecStatus::INVALID_COMPILED_PROTOCOL;
    return result;
  }

  const auto& layout = state->Artifacts().Plan()->GetExecutionResourceLayout();
  const std::size_t decoded_capacity =
      options.decoded_field_capacity == kUsePlanDecodedFieldCapacity
          ? layout.max_fields_per_message
          : options.decoded_field_capacity;
  if (decoded_capacity > layout.max_fields_per_message) {
    result.status = CodecStatus::INVALID_ARGUMENT;
    return result;
  }

  ExecutionMemoryReport report;
  report.core_workspace_bytes = layout.estimated_workspace_bytes;
  report.retained_compiled_state_facade_bytes = internal::CompiledState::FacadeBytes();
  if (!MultiplyChecked(decoded_capacity, sizeof(core::DecodedFieldSlot),
                       report.decoded_slot_bytes) ||
      !MultiplyChecked(layout.max_fields_per_message, sizeof(core::EncodeFieldValue),
                       report.encode_mapping_bytes)) {
    result.status = CodecStatus::RESOURCE_LIMIT_EXCEEDED;
    return result;
  }
  report.facade_bytes = sizeof(CompleteRecordCodec) + sizeof(CompleteRecordCodec::Impl);
  report.codec_accounted_total_bytes = report.core_workspace_bytes;
  if (!AddChecked(report.decoded_slot_bytes, report.codec_accounted_total_bytes) ||
      !AddChecked(report.encode_mapping_bytes, report.codec_accounted_total_bytes) ||
      !AddChecked(report.facade_bytes, report.codec_accounted_total_bytes) ||
      report.codec_accounted_total_bytes > options.execution_memory_limit_bytes) {
    result.status = CodecStatus::RESOURCE_LIMIT_EXCEEDED;
    result.memory = report;
    return result;
  }
  report.allocation_count = 2U + CoreWorkspaceAllocationCount(layout) +
                            (decoded_capacity != 0U ? 1U : 0U) +
                            (layout.max_fields_per_message != 0U ? 1U : 0U);
#if defined(_MSC_VER) && defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL != 0
  // ExecutionWorkspace owns nine vectors in the enabled public capability set. MSVC's checked
  // iterator representation gives each vector one proxy allocation, including empty vectors.
  report.allocation_count += 9U;
#endif

  try {
    auto impl = std::make_unique<CompleteRecordCodec::Impl>(state, decoded_capacity,
                                                            layout.max_fields_per_message, report);
    result.codec.reset(new CompleteRecordCodec(std::move(impl)));
  } catch (const std::bad_alloc&) {
    result.status = CodecStatus::ALLOCATION_FAILED;
    result.memory = report;
    return result;
  } catch (...) {
    result.status = CodecStatus::INTERNAL_ERROR;
    result.memory = report;
    return result;
  }
  result.status = CodecStatus::OK;
  result.memory = report;
  return result;
}

#if defined(PAE_ENABLE_PUBLIC_CODEC_TEST_HOOKS)
namespace public_api_internal::test_only {

void SetCodecEnteredHook(CodecEnteredHook hook, void* context) noexcept {
  g_entered_hook_context.store(context, std::memory_order_relaxed);
  g_entered_hook.store(hook, std::memory_order_release);
}

}  // namespace public_api_internal::test_only
#endif

}  // namespace pae
