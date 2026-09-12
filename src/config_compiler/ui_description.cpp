#include <algorithm>
#include <cstring>
#include <limits>
#include <new>
#include <type_traits>
#include <utility>

#include "ui_description_internal.h"

namespace pae::config_compiler {
namespace {

struct DescriptionHeader {
  std::size_t protocol_offset = 0U;
  std::size_t pipeline_offset = 0U;
  std::size_t pipeline_count = 0U;
  std::size_t message_offset = 0U;
  std::size_t message_count = 0U;
  std::size_t field_offset = 0U;
  std::size_t field_count = 0U;
  std::size_t enum_offset = 0U;
  std::size_t enum_count = 0U;
  std::size_t string_offset = 0U;
  std::size_t string_size = 0U;
};

struct DescriptionLayout {
  DescriptionHeader header;
  DescriptionMemoryReport report;
};

static_assert(std::is_trivially_destructible_v<DescriptionHeader>);
static_assert(std::is_trivially_destructible_v<ProtocolMetadata>);
static_assert(std::is_trivially_destructible_v<PipelineMetadata>);
static_assert(std::is_trivially_destructible_v<MessageMetadata>);
static_assert(std::is_trivially_destructible_v<FieldMetadata>);
static_assert(std::is_trivially_destructible_v<EnumMetadata>);

thread_local UiDescriptionTestProbe* g_test_probe = nullptr;

bool CheckedAdd(std::size_t left, std::size_t right, std::size_t& result) noexcept {
  if (right > (std::numeric_limits<std::size_t>::max)() - left) {
    return false;
  }
  result = left + right;
  return true;
}

bool CheckedMultiply(std::size_t left, std::size_t right, std::size_t& result) noexcept {
  if (left != 0U && right > (std::numeric_limits<std::size_t>::max)() / left) {
    return false;
  }
  result = left * right;
  return true;
}

bool AlignUp(std::size_t value, std::size_t alignment, std::size_t& result) noexcept {
  const std::size_t remainder = value % alignment;
  if (remainder == 0U) {
    result = value;
    return true;
  }
  return CheckedAdd(value, alignment - remainder, result);
}

template <typename T>
bool AddArrayRegion(std::size_t count, std::size_t& cursor, std::size_t& object_bytes,
                    std::size_t& alignment_bytes, std::size_t& offset) noexcept {
  std::size_t aligned = 0U;
  if (!AlignUp(cursor, alignof(T), aligned) ||
      !CheckedAdd(alignment_bytes, aligned - cursor, alignment_bytes)) {
    return false;
  }
  std::size_t bytes = 0U;
  if (!CheckedMultiply(count, sizeof(T), bytes) || !CheckedAdd(object_bytes, bytes, object_bytes) ||
      !CheckedAdd(aligned, bytes, cursor)) {
    return false;
  }
  offset = aligned;
  return true;
}

bool CalculateLayout(const UiDescriptionLayoutTestInput& input,
                     DescriptionLayout& layout) noexcept {
  layout = {};
  std::size_t cursor = 0U;
  if (!AddArrayRegion<DescriptionHeader>(1U, cursor, layout.report.object_bytes,
                                         layout.report.alignment_bytes,
                                         layout.header.protocol_offset)) {
    return false;
  }
  // The header owns the protocol descriptor's location; reuse protocol_offset after placing it.
  if (!AddArrayRegion<ProtocolMetadata>(1U, cursor, layout.report.object_bytes,
                                        layout.report.alignment_bytes,
                                        layout.header.protocol_offset) ||
      !AddArrayRegion<PipelineMetadata>(input.pipeline_count, cursor, layout.report.object_bytes,
                                        layout.report.alignment_bytes,
                                        layout.header.pipeline_offset) ||
      !AddArrayRegion<MessageMetadata>(input.message_count, cursor, layout.report.object_bytes,
                                       layout.report.alignment_bytes,
                                       layout.header.message_offset) ||
      !AddArrayRegion<FieldMetadata>(input.field_count, cursor, layout.report.object_bytes,
                                     layout.report.alignment_bytes, layout.header.field_offset) ||
      !AddArrayRegion<EnumMetadata>(input.enum_count, cursor, layout.report.object_bytes,
                                    layout.report.alignment_bytes, layout.header.enum_offset)) {
    return false;
  }
  layout.header.pipeline_count = input.pipeline_count;
  layout.header.message_count = input.message_count;
  layout.header.field_count = input.field_count;
  layout.header.enum_count = input.enum_count;
  layout.header.string_offset = cursor;
  layout.header.string_size = input.string_bytes;
  layout.report.string_bytes = input.string_bytes;
  layout.report.index_bytes = 0U;
  layout.report.allocation_count = 1U;
  if (!CheckedAdd(cursor, input.string_bytes, layout.report.accounted_total_bytes)) {
    return false;
  }
  return true;
}

bool TouchProbe(std::size_t UiDescriptionTestProbe::* counter) noexcept {
  if (g_test_probe == nullptr) {
    return true;
  }
  ++(g_test_probe->*counter);
  return !g_test_probe->fail_if_touched &&
         !(g_test_probe->fail_storage_allocation &&
           counter == &UiDescriptionTestProbe::storage_allocation_count) &&
         !(g_test_probe->fail_index_audit && counter == &UiDescriptionTestProbe::index_audit_count);
}

CompileDiagnostic InternalDiagnostic(std::string detail) {
  return CompileDiagnostic{CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION, "",
                           std::nullopt, std::move(detail)};
}

bool AddStringSize(const std::string& value, std::size_t& total) noexcept {
  return CheckedAdd(total, value.size(), total);
}

bool CountSchema(const SchemaIr& schema, UiDescriptionLayoutTestInput& input) noexcept {
  input = {};
  input.pipeline_count = schema.pipelines.size();
  input.message_count = schema.messages.size();
  if (!AddStringSize(schema.display_name, input.string_bytes) ||
      !AddStringSize(schema.description, input.string_bytes) ||
      !AddStringSize(schema.source_ref, input.string_bytes)) {
    return false;
  }
  for (const PipelineIr& pipeline : schema.pipelines) {
    if (!AddStringSize(pipeline.display_name, input.string_bytes) ||
        !AddStringSize(pipeline.description, input.string_bytes) ||
        !AddStringSize(pipeline.source_ref, input.string_bytes)) {
      return false;
    }
  }
  for (const MessageIr& message : schema.messages) {
    if (!CheckedAdd(input.field_count, message.fields.size(), input.field_count) ||
        !AddStringSize(message.display_name, input.string_bytes) ||
        !AddStringSize(message.description, input.string_bytes) ||
        !AddStringSize(message.source_ref, input.string_bytes)) {
      return false;
    }
    for (const FieldIr& field : message.fields) {
      if (!CheckedAdd(input.enum_count, field.enum_entries.size(), input.enum_count) ||
          !AddStringSize(field.display_name, input.string_bytes) ||
          !AddStringSize(field.description, input.string_bytes) ||
          !AddStringSize(field.source_ref, input.string_bytes)) {
        return false;
      }
      for (const EnumEntryIr& entry : field.enum_entries) {
        if (!AddStringSize(entry.display_name, input.string_bytes)) {
          return false;
        }
      }
    }
  }
  return true;
}

template <typename T>
T* At(std::byte* storage, std::size_t offset) noexcept {
  return reinterpret_cast<T*>(storage + offset);
}

template <typename T>
const T* At(const std::byte* storage, std::size_t offset) noexcept {
  return reinterpret_cast<const T*>(storage + offset);
}

DescriptionStringSpan CopyString(std::string_view value, std::byte* storage,
                                 std::size_t& cursor) noexcept {
  DescriptionStringSpan span{cursor, value.size()};
  if (!value.empty()) {
    std::memcpy(storage + cursor, value.data(), value.size());
    cursor += value.size();
  }
  return span;
}

bool CheckedRange(std::size_t begin, std::size_t count, std::size_t size) noexcept {
  std::size_t end = 0U;
  return CheckedAdd(begin, count, end) && end <= size;
}

bool SpanWithin(std::size_t string_begin, std::size_t storage_size,
                DescriptionStringSpan span) noexcept {
  std::size_t end = 0U;
  return span.offset >= string_begin && CheckedAdd(span.offset, span.size, end) &&
         end <= storage_size;
}

template <typename Metadata>
bool CommonSpansWithin(std::size_t string_begin, std::size_t storage_size,
                       const Metadata& metadata) noexcept {
  return SpanWithin(string_begin, storage_size, metadata.display_name) &&
         SpanWithin(string_begin, storage_size, metadata.description) &&
         SpanWithin(string_begin, storage_size, metadata.source_ref);
}

bool ReportsEqual(const DescriptionMemoryReport& left,
                  const DescriptionMemoryReport& right) noexcept {
  return left.object_bytes == right.object_bytes && left.string_bytes == right.string_bytes &&
         left.index_bytes == right.index_bytes && left.alignment_bytes == right.alignment_bytes &&
         left.allocation_count == right.allocation_count &&
         left.accounted_total_bytes == right.accounted_total_bytes;
}

}  // namespace

void UiDescriptionSidecar::StorageDeleter::operator()(std::byte* storage) const noexcept {
  delete[] storage;
  if (storage == nullptr || test_probe == nullptr) {
    return;
  }
  if (test_probe->live_storage_count > 0U) {
    --test_probe->live_storage_count;
  }
  if (test_probe->live_accounted_bytes >= accounted_bytes) {
    test_probe->live_accounted_bytes -= accounted_bytes;
  } else {
    test_probe->live_accounted_bytes = 0U;
  }
  ++test_probe->released_storage_count;
  test_probe->released_accounted_bytes += accounted_bytes;
}

const ProtocolMetadata& UiDescriptionSidecar::Protocol() const noexcept {
  const DescriptionHeader* header = reinterpret_cast<const DescriptionHeader*>(storage_.get());
  return *At<ProtocolMetadata>(storage_.get(), header->protocol_offset);
}

DescriptionArrayView<PipelineMetadata> UiDescriptionSidecar::Pipelines() const noexcept {
  const DescriptionHeader* header = reinterpret_cast<const DescriptionHeader*>(storage_.get());
  return {At<PipelineMetadata>(storage_.get(), header->pipeline_offset), header->pipeline_count};
}

DescriptionArrayView<MessageMetadata> UiDescriptionSidecar::Messages() const noexcept {
  const DescriptionHeader* header = reinterpret_cast<const DescriptionHeader*>(storage_.get());
  return {At<MessageMetadata>(storage_.get(), header->message_offset), header->message_count};
}

DescriptionArrayView<FieldMetadata> UiDescriptionSidecar::Fields() const noexcept {
  const DescriptionHeader* header = reinterpret_cast<const DescriptionHeader*>(storage_.get());
  return {At<FieldMetadata>(storage_.get(), header->field_offset), header->field_count};
}

DescriptionArrayView<EnumMetadata> UiDescriptionSidecar::Enums() const noexcept {
  const DescriptionHeader* header = reinterpret_cast<const DescriptionHeader*>(storage_.get());
  return {At<EnumMetadata>(storage_.get(), header->enum_offset), header->enum_count};
}

std::string_view UiDescriptionSidecar::Resolve(DescriptionStringSpan span) const noexcept {
  std::size_t end = 0U;
  if (storage_ == nullptr || !CheckedAdd(span.offset, span.size, end) || end > storage_size_) {
    return {};
  }
  return {reinterpret_cast<const char*>(storage_.get() + span.offset), span.size};
}

std::size_t DerivedUiDescriptionMemoryLimit(ResourceProfile resource_profile) noexcept {
  const protocol_plan::ResourceProfileLimits* limits =
      protocol_plan::GetResourceProfileLimits(resource_profile);
  if (limits == nullptr) {
    return 0U;
  }
  DescriptionLayout layout;
  const UiDescriptionLayoutTestInput maximum{
      limits->max_pipelines, limits->max_messages, limits->max_total_fields,
      limits->max_total_enum_entries, kCompilerDecodedStringHardLimitBytes};
  return CalculateLayout(maximum, layout) ? layout.report.accounted_total_bytes : 0U;
}

ScopedUiDescriptionTestProbe::ScopedUiDescriptionTestProbe(UiDescriptionTestProbe& probe) noexcept
    : previous_(g_test_probe) {
  g_test_probe = &probe;
}

ScopedUiDescriptionTestProbe::~ScopedUiDescriptionTestProbe() { g_test_probe = previous_; }

bool EstimateUiDescriptionLayoutForTest(const UiDescriptionLayoutTestInput& input,
                                        DescriptionMemoryReport& report) noexcept {
  DescriptionLayout layout;
  if (!CalculateLayout(input, layout)) {
    report = {};
    return false;
  }
  report = layout.report;
  return true;
}

bool UiDescriptionPlanFreezeAllowedForTest(CompileDiagnostic& diagnostic) {
  if (g_test_probe == nullptr) {
    return true;
  }
  ++g_test_probe->plan_freeze_count;
  if (!g_test_probe->fail_if_touched && !g_test_probe->fail_plan_freeze) {
    return true;
  }
  diagnostic =
      CompileDiagnostic{CompileStage::INTERNAL, CompileError::COMPILER_ALLOCATION_FAILED, "",
                        std::nullopt, "test probe injected Plan Freeze allocation failure"};
  return false;
}

bool UiDescriptionBuilder::Build(const BudgetedSchemaIr& budgeted, std::size_t memory_limit_bytes,
                                 UiDescriptionSidecar& sidecar, CompileDiagnostic& diagnostic) {
  sidecar = {};
  if (!TouchProbe(&UiDescriptionTestProbe::layout_count)) {
    diagnostic = InternalDiagnostic("test probe rejected UI description layout");
    return false;
  }
  if (budgeted.validated_ == nullptr || budgeted.validated_->payload_ == nullptr) {
    diagnostic = InternalDiagnostic("moved-from budgeted schema used for UI description");
    return false;
  }
  const SchemaIr& schema = budgeted.validated_->payload_->schema;
  UiDescriptionLayoutTestInput input;
  DescriptionLayout layout;
  if (!CountSchema(schema, input) || !CalculateLayout(input, layout)) {
    diagnostic = InternalDiagnostic("UI description layout overflowed");
    return false;
  }
  const std::size_t derived_limit = DerivedUiDescriptionMemoryLimit(schema.resource_profile);
  const std::size_t effective_limit = (std::min)(memory_limit_bytes, derived_limit);
  if (derived_limit == 0U || layout.report.accounted_total_bytes > effective_limit) {
    diagnostic = CompileDiagnostic{CompileStage::RESOURCE_BUDGET,
                                   CompileError::RESOURCE_LIMIT_EXCEEDED,
                                   "",
                                   std::nullopt,
                                   "accounted UI description memory exceeds the selected limit",
                                   ResourceKind::UI_DESCRIPTION_ACCOUNTED_MEMORY,
                                   layout.report.accounted_total_bytes,
                                   effective_limit,
                                   schema.resource_profile};
    return false;
  }
  if (!TouchProbe(&UiDescriptionTestProbe::storage_allocation_count)) {
    diagnostic = CompileDiagnostic{CompileStage::INTERNAL, CompileError::COMPILER_ALLOCATION_FAILED,
                                   "", std::nullopt,
                                   "test probe injected UI description storage allocation failure"};
    return false;
  }
  std::byte* raw_storage = new (std::nothrow) std::byte[layout.report.accounted_total_bytes];
  if (raw_storage == nullptr) {
    diagnostic = CompileDiagnostic{CompileStage::INTERNAL, CompileError::COMPILER_ALLOCATION_FAILED,
                                   "", std::nullopt, "failed to allocate UI description storage"};
    return false;
  }
  UiDescriptionTestProbe* lifetime_probe =
      g_test_probe != nullptr && g_test_probe->track_storage_lifetime ? g_test_probe : nullptr;
  if (lifetime_probe != nullptr) {
    ++lifetime_probe->live_storage_count;
    lifetime_probe->live_accounted_bytes += layout.report.accounted_total_bytes;
  }
  UiDescriptionSidecar::StorageOwner storage{
      raw_storage,
      UiDescriptionSidecar::StorageDeleter{lifetime_probe, layout.report.accounted_total_bytes}};
  DescriptionHeader* header = ::new (storage.get()) DescriptionHeader(layout.header);
  auto* protocol =
      ::new (At<ProtocolMetadata>(storage.get(), header->protocol_offset)) ProtocolMetadata{};
  PipelineMetadata* pipelines = At<PipelineMetadata>(storage.get(), header->pipeline_offset);
  MessageMetadata* messages = At<MessageMetadata>(storage.get(), header->message_offset);
  FieldMetadata* fields = At<FieldMetadata>(storage.get(), header->field_offset);
  EnumMetadata* enums = At<EnumMetadata>(storage.get(), header->enum_offset);
  for (std::size_t index = 0U; index < header->pipeline_count; ++index) {
    ::new (pipelines + index) PipelineMetadata{};
  }
  for (std::size_t index = 0U; index < header->message_count; ++index) {
    ::new (messages + index) MessageMetadata{};
  }
  for (std::size_t index = 0U; index < header->field_count; ++index) {
    ::new (fields + index) FieldMetadata{};
  }
  for (std::size_t index = 0U; index < header->enum_count; ++index) {
    ::new (enums + index) EnumMetadata{};
  }
  if (!TouchProbe(&UiDescriptionTestProbe::metadata_copy_count)) {
    diagnostic = InternalDiagnostic("test probe rejected UI description metadata copy");
    return false;
  }
  std::size_t string_cursor = header->string_offset;
  protocol->display_name = CopyString(schema.display_name, storage.get(), string_cursor);
  protocol->description = CopyString(schema.description, storage.get(), string_cursor);
  protocol->source_ref = CopyString(schema.source_ref, storage.get(), string_cursor);
  for (std::size_t index = 0U; index < schema.pipelines.size(); ++index) {
    const PipelineIr& source = schema.pipelines[index];
    pipelines[index].display_name = CopyString(source.display_name, storage.get(), string_cursor);
    pipelines[index].description = CopyString(source.description, storage.get(), string_cursor);
    pipelines[index].source_ref = CopyString(source.source_ref, storage.get(), string_cursor);
  }
  std::size_t field_cursor = 0U;
  std::size_t enum_cursor = 0U;
  for (std::size_t message_index = 0U; message_index < schema.messages.size(); ++message_index) {
    const MessageIr& source_message = schema.messages[message_index];
    MessageMetadata& target_message = messages[message_index];
    target_message.display_name =
        CopyString(source_message.display_name, storage.get(), string_cursor);
    target_message.description =
        CopyString(source_message.description, storage.get(), string_cursor);
    target_message.source_ref = CopyString(source_message.source_ref, storage.get(), string_cursor);
    target_message.field_begin = field_cursor;
    target_message.field_count = source_message.fields.size();
    for (const FieldIr& source_field : source_message.fields) {
      FieldMetadata& target_field = fields[field_cursor++];
      target_field.display_name =
          CopyString(source_field.display_name, storage.get(), string_cursor);
      target_field.description = CopyString(source_field.description, storage.get(), string_cursor);
      target_field.source_ref = CopyString(source_field.source_ref, storage.get(), string_cursor);
      target_field.enum_begin = enum_cursor;
      target_field.enum_count = source_field.enum_entries.size();
      for (const EnumEntryIr& source_enum : source_field.enum_entries) {
        enums[enum_cursor++].display_name =
            CopyString(source_enum.display_name, storage.get(), string_cursor);
      }
    }
  }
  if (string_cursor != layout.report.accounted_total_bytes || field_cursor != header->field_count ||
      enum_cursor != header->enum_count) {
    diagnostic = InternalDiagnostic("UI description estimate and final write differ");
    return false;
  }
  sidecar =
      UiDescriptionSidecar{std::move(storage), layout.report.accounted_total_bytes, layout.report};
  return true;
}

bool UiDescriptionBuilder::Audit(const protocol_plan::PlanBundle& plan,
                                 const UiDescriptionSidecar& sidecar,
                                 CompileDiagnostic& diagnostic) {
  if (!TouchProbe(&UiDescriptionTestProbe::index_audit_count)) {
    diagnostic = InternalDiagnostic("test probe rejected UI description index audit");
    return false;
  }
  if (sidecar.empty()) {
    diagnostic = InternalDiagnostic("UI description storage is empty during index audit");
    return false;
  }
  const DescriptionHeader* header =
      reinterpret_cast<const DescriptionHeader*>(sidecar.storage_.get());
  DescriptionLayout expected_layout;
  const UiDescriptionLayoutTestInput expected_input{header->pipeline_count, header->message_count,
                                                    header->field_count, header->enum_count,
                                                    header->string_size};
  if (!CalculateLayout(expected_input, expected_layout) ||
      header->protocol_offset != expected_layout.header.protocol_offset ||
      header->pipeline_offset != expected_layout.header.pipeline_offset ||
      header->message_offset != expected_layout.header.message_offset ||
      header->field_offset != expected_layout.header.field_offset ||
      header->enum_offset != expected_layout.header.enum_offset ||
      header->string_offset != expected_layout.header.string_offset ||
      sidecar.storage_size_ != expected_layout.report.accounted_total_bytes ||
      !ReportsEqual(sidecar.memory_report_, expected_layout.report)) {
    diagnostic = InternalDiagnostic("UI description storage layout differs from its report");
    return false;
  }
  const std::size_t string_begin = header->string_offset;
  const std::size_t storage_size = sidecar.storage_size_;
  if (sidecar.Pipelines().size() != plan.Pipelines().size() ||
      sidecar.Messages().size() != plan.Messages().size() ||
      sidecar.Fields().size() != plan.GetResourceRequirements().total_field_count ||
      sidecar.Enums().size() != plan.GetResourceRequirements().total_enum_entry_count ||
      !CommonSpansWithin(string_begin, storage_size, sidecar.Protocol())) {
    diagnostic = InternalDiagnostic("Plan and UI description top-level indexes differ");
    return false;
  }
  for (const PipelineMetadata& pipeline : sidecar.Pipelines()) {
    if (!CommonSpansWithin(string_begin, storage_size, pipeline)) {
      diagnostic = InternalDiagnostic("UI pipeline metadata span is outside storage");
      return false;
    }
  }
  std::size_t expected_field_begin = 0U;
  std::size_t expected_enum_begin = 0U;
  for (std::size_t message_index = 0U; message_index < sidecar.Messages().size(); ++message_index) {
    const MessageMetadata& message = sidecar.Messages()[message_index];
    if (!CommonSpansWithin(string_begin, storage_size, message) ||
        message.field_begin != expected_field_begin ||
        message.field_count != plan.Messages()[message_index].fields.size() ||
        !CheckedRange(message.field_begin, message.field_count, sidecar.Fields().size())) {
      diagnostic = InternalDiagnostic("Plan and UI description message field indexes differ");
      return false;
    }
    for (std::size_t field_offset = 0U; field_offset < message.field_count; ++field_offset) {
      const std::size_t field_index = message.field_begin + field_offset;
      const FieldMetadata& field = sidecar.Fields()[field_index];
      const auto& plan_field = plan.Messages()[message_index].fields[field_offset];
      if (!CommonSpansWithin(string_begin, storage_size, field) ||
          field.enum_begin != expected_enum_begin ||
          field.enum_count != plan_field.enum_entries.size() ||
          !CheckedRange(field.enum_begin, field.enum_count, sidecar.Enums().size())) {
        diagnostic = InternalDiagnostic(
            "Plan and UI description field enum indexes differ at message " +
            std::to_string(message_index) + ", field " + std::to_string(field_offset) +
            ": UI begin/count=" + std::to_string(field.enum_begin) + "/" +
            std::to_string(field.enum_count) +
            ", expected begin=" + std::to_string(expected_enum_begin) +
            ", Plan count=" + std::to_string(plan_field.enum_entries.size()));
        return false;
      }
      for (std::size_t enum_offset = 0U; enum_offset < field.enum_count; ++enum_offset) {
        if (!SpanWithin(string_begin, storage_size,
                        sidecar.Enums()[field.enum_begin + enum_offset].display_name)) {
          diagnostic = InternalDiagnostic("UI enum metadata span is outside storage");
          return false;
        }
      }
      expected_enum_begin += field.enum_count;
    }
    expected_field_begin += message.field_count;
  }
  if (expected_field_begin != sidecar.Fields().size() ||
      expected_enum_begin != sidecar.Enums().size()) {
    diagnostic = InternalDiagnostic("UI description flat ranges are not exhaustive");
    return false;
  }
  return true;
}

}  // namespace pae::config_compiler
