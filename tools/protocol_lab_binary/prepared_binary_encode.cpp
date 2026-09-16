#include <algorithm>
#include <limits>
#include <new>

#include "prepared_binary.h"

namespace pae::protocol_lab_binary {
namespace {
namespace host = host_endpoint;
namespace core = protocol_core;
namespace plan = protocol_plan;
void Require(bool ok, const char* detail) {
  if (!ok) throw MaterializationError(detail);
}
std::size_t Add(std::size_t a, std::size_t b) {
  Require(b <= std::numeric_limits<std::size_t>::max() - a, "Encode budget overflow");
  return a + b;
}
std::size_t Mul(std::size_t a, std::size_t b) {
  Require(!a || b <= std::numeric_limits<std::size_t>::max() / a, "Encode budget overflow");
  return a * b;
}
std::size_t AllowString(std::size_t n) { return Add(Mul(n, 2U), 32U); }
core::LogicalValueKind Kind(const FieldDescriptor& field) {
  if (field.conversion) return core::LogicalValueKind::DECIMAL64;
  switch (field.value_type) {
    case plan::ValueType::UINT64:
      return core::LogicalValueKind::UINT64;
    case plan::ValueType::INT64:
      return core::LogicalValueKind::INT64;
    case plan::ValueType::BOOL:
      return core::LogicalValueKind::BOOL;
    case plan::ValueType::ENUM:
      return core::LogicalValueKind::ENUM;
    case plan::ValueType::BYTES:
      return core::LogicalValueKind::BYTES;
    default:
      throw MaterializationError("unsupported admitted Encode type");
  }
}
const FieldDescriptor* FindField(const MessageDescriptor& message, std::string_view id) {
  for (const auto& field : message.fields)
    if (field.id == id) return &field;
  return nullptr;
}
const EnumDescriptor* FindEnum(const FieldDescriptor& field, std::string_view id) {
  for (const auto& item : field.enum_entries)
    if (item.id == id) return &item;
  return nullptr;
}
void Account(ObservedEncode& value, std::size_t scratch_bytes, const Limits& limits) {
  auto total = Add(sizeof(ObservedEncode), Mul(value.inputs.capacity(), sizeof(FieldValue)));
  total = Add(total, Add(value.frame.capacity(),
                         Mul(value.presentation.capacity(), sizeof(FieldPresentation))));
  std::size_t strings = 0U, bytes = 0U;
  const auto text = [&](const std::string& s) { strings = Add(strings, Add(s.capacity(), 1U)); };
  text(value.message_id);
  for (const auto& field : value.inputs) {
    text(field.id);
    text(field.enum_value.item_id);
    text(field.enum_value.display_text);
    bytes = Add(bytes, field.bytes_value.capacity());
  }
  for (const auto& p : value.presentation) {
    text(p.enum_display_text);
    total = Add(total, Mul(p.physical_bits.capacity(), sizeof(PhysicalBitMask)));
  }
  value.accounted_total_bytes = Add(total, Add(strings, bytes));
  value.work_peak_bytes =
      std::max(value.work_peak_bytes, Add(value.accounted_total_bytes, scratch_bytes));
  Require(bytes <= limits.max_field_bytes && strings <= limits.max_string_bytes &&
              value.frame.size() <= limits.max_frame_bytes &&
              value.work_peak_bytes <= limits.max_total_bytes,
          "Encode actual capacity budget exceeded");
}
}  // namespace

const ObservedOperation& PreparedBinary::Encode(std::size_t binding, std::string_view message_id,
                                                const EncodeInput* inputs, std::size_t count) {
  pending_->Clear();
  if (!Valid(binding, 0U) || bindings_[binding].action != host::Action::ENCODE) {
    pending_->host.status = host::Status::INVALID_BINDING;
    return *pending_;
  }
  try {
    EncodeOnce(binding, message_id, inputs, count, *pending_);
  } catch (const MaterializationError&) {
    pending_->encoded.reset();
    pending_->host.status = host::Status::LIMIT_EXCEEDED;
    pending_->encode_issue = EncodeIssue::BUDGET_EXCEEDED;
  } catch (const std::bad_alloc&) {
    pending_->encoded.reset();
    pending_->host.status = host::Status::ALLOCATION_FAILED;
    pending_->encode_issue = EncodeIssue::ALLOCATION_FAILED;
  }
  return Publish(binding, 0U);
}

void PreparedBinary::EncodeOnce(std::size_t binding, std::string_view message_id,
                                const EncodeInput* inputs, std::size_t count,
                                ObservedOperation& out) {
  out.host.status = host::Status::INVALID_BINDING;
  if (!Valid(binding, 0U) || bindings_[binding].action != host::Action::ENCODE) return;
  const auto observation = Observe(binding, 0U);
  out.host.generation = observation.generation;
  if (observation.status != host::Status::OK || observation.reset_required) {
    out.host.status =
        observation.reset_required ? host::Status::RESET_REQUIRED : observation.status;
    return;
  }
  out.host.status = host::Status::INVALID_ARGUMENT;
  const auto& bound = bindings_[binding];
  const MessageDescriptor* message = nullptr;
  if (message_id.size() <= limits_.max_identity_bytes)
    for (auto index : description_.pipelines[bound.pipeline].message_indices)
      if (description_.messages[index].id == message_id) message = &description_.messages[index];
  if (!message) {
    out.encode_issue = EncodeIssue::MESSAGE_NOT_BOUND;
    return;
  }
  if ((count && !inputs) || count > limits_.max_fields || count > message->fields.size()) {
    out.encode_issue = EncodeIssue::INVALID_FIELD;
    return;
  }
  std::size_t required = 0U;
  for (const auto& field : message->fields)
    if (field.encode_source == plan::EncodeSource::INPUT) ++required;
  for (std::size_t i = 0U; i < count; ++i) {
    const auto& input = inputs[i];
    const auto* field = input.field_id.size() <= limits_.max_identity_bytes
                            ? FindField(*message, input.field_id)
                            : nullptr;
    if (!field || field->encode_source != plan::EncodeSource::INPUT) {
      out.encode_issue = EncodeIssue::INVALID_FIELD;
      return;
    }
    for (std::size_t j = 0U; j < i; ++j)
      if (inputs[j].field_id == input.field_id) {
        out.encode_issue = EncodeIssue::DUPLICATE_FIELD;
        return;
      }
    if (input.kind != Kind(*field)) {
      out.encode_issue = EncodeIssue::TYPE_MISMATCH;
      return;
    }
    if (input.kind == core::LogicalValueKind::ENUM &&
        (input.enum_item_id.size() > limits_.max_identity_bytes ||
         !FindEnum(*field, input.enum_item_id))) {
      out.encode_issue = EncodeIssue::UNKNOWN_ENUM;
      return;
    }
    if (input.kind == core::LogicalValueKind::BYTES && input.bytes_value.size &&
        !input.bytes_value.data) {
      out.encode_issue = EncodeIssue::INVALID_BYTES;
      return;
    }
  }
  if (count != required) {
    out.encode_issue = EncodeIssue::MISSING_FIELD;
    return;
  }
  // Preflight input, resolved Host argument slots, maximum TX bytes and all field mappings.
  const auto frame_bound =
      message->bounded_payload ? message->bounded_payload->max_frame_length : message->frame_size;
  auto preflight =
      Add(sizeof(ObservedEncode), Mul(count, sizeof(FieldValue) + sizeof(host::NamedValue)));
  preflight =
      Add(preflight, Add(frame_bound, Mul(message->fields.size(), sizeof(FieldPresentation))));
  auto strings = AllowString(message_id.size());
  std::size_t bytes = 0U;
  for (std::size_t i = 0U; i < count; ++i) {
    const auto& input = inputs[i];
    strings = Add(strings, AllowString(input.field_id.size()));
    const auto* entry = input.kind == core::LogicalValueKind::ENUM
                            ? FindEnum(*FindField(*message, input.field_id), input.enum_item_id)
                            : nullptr;
    strings = Add(strings, AllowString(entry ? entry->id.size() : 0U));
    strings = Add(strings, AllowString(entry ? entry->display_name.size() : 0U));
    if (input.kind == core::LogicalValueKind::BYTES) bytes = Add(bytes, input.bytes_value.size);
  }
  for (const auto& field : message->fields) {
    preflight = Add(preflight, Mul(field.physical_bits.size(), sizeof(PhysicalBitMask)));
    strings = Add(strings, AllowString(0U));
  }
  preflight = Add(preflight, Add(strings, bytes));
  Require(frame_bound <= limits_.max_frame_bytes && strings <= limits_.max_string_bytes &&
              bytes <= limits_.max_field_bytes && preflight <= limits_.max_total_bytes,
          "Encode preflight budget exceeded");
  Require(operation_ != std::numeric_limits<std::uint64_t>::max(), "operation identity exhausted");
  auto& encoded = out.encoded.emplace();
  encoded.identity = {revisions_, instance_, ++operation_,  observation.generation,
                      binding,    0U,        bound.pipeline};
  encoded.message_index = message->message_index;
  encoded.message_id = message->id;
  encoded.work_peak_bytes = preflight;
  encoded.inputs.resize(count);
  std::vector<host::NamedValue> resolved(count);
  for (std::size_t i = 0U; i < count; ++i) {
    const auto& input = inputs[i];
    const auto& field = *FindField(*message, input.field_id);
    auto& saved = encoded.inputs[i];
    saved.index = field.field_index;
    saved.id = field.id;
    saved.kind = input.kind;
    saved.uint64_value = input.uint64_value;
    saved.int64_value = input.int64_value;
    saved.bool_value = input.bool_value;
    saved.decimal64_value = input.decimal64_value;
    auto& value = resolved[i].value;
    resolved[i].field_id = saved.id;
    value.value_kind = input.kind;
    value.uint64_value = input.uint64_value;
    value.int64_value = input.int64_value;
    value.bool_value = input.bool_value;
    value.decimal64_value = input.decimal64_value;
    if (input.kind == core::LogicalValueKind::BYTES) {
      if (input.bytes_value.size)
        saved.bytes_value.assign(input.bytes_value.data,
                                 input.bytes_value.data + input.bytes_value.size);
      value.bytes_value = {saved.bytes_value.data(), saved.bytes_value.size()};
    }
    if (input.kind == core::LogicalValueKind::ENUM) {
      const auto& entry = *FindEnum(field, input.enum_item_id);
      saved.enum_value = {entry.raw_value, true, entry.id, entry.display_name};
      value.enum_value = {&session_->Plan(), message->message_index, field.field_index,
                          entry.entry_index};
    }
  }
  const auto scratch = Mul(resolved.capacity(), sizeof(host::NamedValue));
  Account(encoded, scratch, limits_);
  struct Context {
    PreparedBinary& owner;
    ObservedEncode& encoded;
    const MessageDescriptor& message;
    std::size_t scratch;
  } context{*this, encoded, *message, scratch};
  out.host = session_->Encode(
      session_->Find(bound.endpoint, bound.action, 0U), message_id, resolved.data(),
      resolved.size(),
      {[](const host::Output& output, void* pointer) {
         auto& call = *static_cast<Context*>(pointer);
         auto& tx = call.encoded;
         Require(output.action == host::Action::ENCODE &&
                     output.plan == &call.owner.session_->Plan() &&
                     output.generation == tx.identity.generation &&
                     output.message_index == tx.message_index && output.bytes.data &&
                     IsActualFrameValid(call.message, output.bytes.size),
                 "Encode output identity/frame mismatch");
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
         Require(!call.owner.fail_encode_copy_, "injected Encode output copy failure");
#endif
         tx.frame.assign(output.bytes.data, output.bytes.data + output.bytes.size);
         tx.presentation.resize(call.message.fields.size());
         for (const auto& field : call.message.fields) {
           auto& p = tx.presentation[field.field_index];
           p.field_index = field.field_index;
           p.byte_range = ResolveActualFieldRange(call.message, field, output.bytes.size);
           p.physical_bits = field.physical_bits;
           Require(p.byte_range || !p.physical_bits.empty(), "Encode physical mapping missing");
           for (const auto& input : tx.inputs)
             if (input.index == field.field_index && input.kind == core::LogicalValueKind::BYTES)
               Require(p.byte_range && p.byte_range->length == input.bytes_value.size(),
                       "Encode BYTES mapping mismatch");
         }
         tx.integrity_storage = ResolveActualIntegrityStorage(call.message, output.bytes.size);
         Require(!call.message.integrity_storage || tx.integrity_storage,
                 "Encode integrity mapping missing");
         Account(tx, call.scratch, call.owner.limits_);
         return host::SinkAction::STOP;
       },
       &context});
  if (out.host.status != host::Status::OK) {
    out.encoded.reset();
    if (out.host.status == host::Status::CALLBACK_FAILED)
      out.encode_issue = EncodeIssue::OUTPUT_COPY_FAILED;
  }
}
}  // namespace pae::protocol_lab_binary
