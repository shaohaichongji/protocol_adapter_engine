#include "candidate_materializer.h"

#include <array>
#include <limits>
#include <string_view>

namespace pae::protocol_lab_binary {
namespace {
namespace core = protocol_core;
namespace plan = protocol_plan;
constexpr std::size_t kMaxFields = 1024U;
using Association = std::array<std::size_t, kMaxFields>;
using Presence = std::array<bool, kMaxFields>;
constexpr std::size_t kScratchBytes = sizeof(Association) + sizeof(Presence);

#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
void BeforeCopy(const CopyTestHooks* hooks) {
  if (hooks != nullptr && hooks->before_copy != nullptr) hooks->before_copy(hooks->context);
}
#endif

void Require(bool condition, const char* message) {
  if (!condition) throw MaterializationError(message);
}

std::size_t Add(std::size_t left, std::size_t right) {
  Require(right <= std::numeric_limits<std::size_t>::max() - left,
          "Binary DTO accounting addition overflow");
  return left + right;
}

std::size_t Multiply(std::size_t left, std::size_t right) {
  Require(left == 0U || right <= std::numeric_limits<std::size_t>::max() / left,
          "Binary DTO accounting multiplication overflow");
  return left * right;
}

void ValidateLimits(const Limits& limits) {
  const Limits hard;
  Require(limits.max_frame_bytes <= hard.max_frame_bytes && limits.max_fields <= hard.max_fields &&
              limits.max_field_bytes <= hard.max_field_bytes &&
              limits.max_string_bytes <= hard.max_string_bytes &&
              limits.max_total_bytes <= hard.max_total_bytes &&
              limits.max_identity_bytes <= hard.max_identity_bytes,
          "Binary DTO limits exceed hard ceilings");
}

void ChargeString(std::string_view value, const Limits& limits, std::size_t& bytes) {
  Require(value.size() <= limits.max_identity_bytes, "Binary DTO identity too long");
  // Reserve a conservative string allowance before copying. Actual capacity is checked below.
  bytes = Add(bytes, Add(Multiply(value.size(), 2U), 32U));
  Require(bytes <= limits.max_string_bytes, "Binary DTO string budget exceeded");
}

bool Converted(const plan::FrozenFieldPlan& field) {
  return field.conversion_index != core::kInvalidIndex;
}

core::LogicalValueKind ExpectedKind(const plan::FrozenFieldPlan& field) {
  if (Converted(field)) return core::LogicalValueKind::DECIMAL64;
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
  }
  throw MaterializationError("Unsupported Binary DTO field type");
}

bool SameField(const core::FieldRef& reference, const host_endpoint::Candidate& candidate,
               std::size_t field_index) {
  return reference.plan_scope == candidate.plan &&
         reference.message_index == candidate.decoded.message_index &&
         reference.field_index == field_index;
}

void ValidateByteView(const core::DecodedFieldSlot& slot,
                      const host_endpoint::Candidate& candidate) {
  const auto& execution = candidate.plan->MessageExecutionPlans();
  Require(candidate.decoded.message_index < execution.size(),
          "Binary DTO message execution index out of range");
  const auto& message = execution[candidate.decoded.message_index];
  Require(slot.field.field_index < message.fields.size(),
          "Binary DTO field execution index out of range");
  const auto& field = message.fields[slot.field.field_index];
  const auto frame = candidate.frame;
  const auto view = slot.bytes_value;
  auto expected_size = field.width;
  Require(field.value_type == plan::ValueType::BYTES, "Binary DTO bytes execution type mismatch");
  if (message.bounded_payload.has_value()) {
    const auto& payload = *message.bounded_payload;
    Require(payload.header_length <= frame.size &&
                payload.trailer_length <= frame.size - payload.header_length &&
                frame.size >= payload.min_frame_length && frame.size <= payload.max_frame_length,
            "Binary DTO bounded frame length mismatch");
    const auto payload_size = frame.size - static_cast<std::size_t>(payload.header_length) -
                              static_cast<std::size_t>(payload.trailer_length);
    Require(
        payload_size >= payload.min_payload_length && payload_size <= payload.max_payload_length,
        "Binary DTO bounded payload length mismatch");
    if (slot.field.field_index == payload.payload_field_index) {
      Require(field.offset == payload.header_length,
              "Binary DTO payload execution offset mismatch");
      expected_size = payload_size;
    }
  } else {
    Require(frame.size == message.frame_size, "Binary DTO fixed frame length mismatch");
  }
  Require(field.offset <= frame.size && expected_size <= frame.size - field.offset &&
              view.size == expected_size && frame.data != nullptr && view.data != nullptr,
          "Binary DTO byte view execution range mismatch");
  const auto frame_address = reinterpret_cast<std::uintptr_t>(frame.data);
  const auto view_address = reinterpret_cast<std::uintptr_t>(view.data);
  Require(view_address >= frame_address && view_address - frame_address == field.offset,
          "Binary DTO field bytes do not match execution offset");
}

std::string_view ValidateEnum(const core::DecodedFieldSlot& slot,
                              const plan::FrozenFieldPlan& field,
                              const host_endpoint::Candidate& candidate) {
  const auto& value = slot.enum_value;
  if (value.known) {
    const auto& reference = value.reference;
    Require(reference.plan_scope == candidate.plan &&
                reference.message_index == candidate.decoded.message_index &&
                reference.field_index == slot.field.field_index &&
                reference.entry_index < field.enum_entries.size(),
            "Binary DTO enum reference mismatch");
    const auto& entry = field.enum_entries[reference.entry_index];
    Require(entry.raw_value == value.raw_value, "Binary DTO enum raw mismatch");
    return entry.id.View();
  }
  Require(field.unknown_enum_policy == plan::UnknownEnumPolicy::PRESERVE,
          "Binary DTO unknown enum is not allowed");
  for (const auto& entry : field.enum_entries) {
    Require(entry.raw_value != value.raw_value, "Binary DTO known enum marked unknown");
  }
  return {};
}

void FinalizeBudget(CandidateResult& result, const Limits& limits) {
  auto& budget = result.budget;
  budget.object_bytes = sizeof(CandidateResult);
  budget.frame_bytes = Add(result.frame.capacity(), result.diagnostic_frame.capacity());
  budget.field_slot_bytes = Multiply(result.fields.capacity(), sizeof(FieldValue));
  budget.string_bytes = Add(result.message_id.capacity(), 1U);
  for (const auto& field : result.fields) {
    budget.field_bytes = Add(budget.field_bytes, field.bytes_value.capacity());
    budget.string_bytes = Add(budget.string_bytes, Add(field.id.capacity(), 1U));
    budget.string_bytes = Add(budget.string_bytes, Add(field.enum_value.item_id.capacity(), 1U));
    budget.string_bytes =
        Add(budget.string_bytes, Add(field.enum_value.display_text.capacity(), 1U));
  }
  Require(budget.frame_bytes <= limits.max_frame_bytes &&
              budget.field_bytes <= limits.max_field_bytes &&
              budget.string_bytes <= limits.max_string_bytes &&
              result.fields.capacity() <= limits.max_fields,
          "Binary DTO allocated capacity exceeds budget");
  budget.accounted_total_bytes =
      Add(Add(Add(budget.object_bytes, budget.frame_bytes), budget.field_slot_bytes),
          Add(budget.field_bytes, budget.string_bytes));
  budget.temporary_bytes = kScratchBytes;
  budget.materialization_peak_bytes = Add(budget.accounted_total_bytes, budget.temporary_bytes);
  Require(budget.materialization_peak_bytes <= limits.max_total_bytes,
          "Binary DTO materialization peak exceeds budget");
}
}  // namespace

CandidateResult MaterializeCandidate(const host_endpoint::Candidate& candidate, const Limits& limits
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
                                     ,
                                     const CopyTestHooks* hooks
#endif
) {
  ValidateLimits(limits);
  Require(candidate.plan != nullptr && candidate.plan->SchemaVersion() == "0.9",
          "Binary DTO requires a Schema 0.9 Plan");
  Require(candidate.frame.size <= limits.max_frame_bytes &&
              (candidate.frame.size == 0U || candidate.frame.data != nullptr),
          "Binary DTO candidate frame exceeds bounds");
  const auto& messages = candidate.plan->Messages();
  const bool success = candidate.decoded.status == core::CodecStatus::OK;
  const auto message_index = candidate.decoded.message_index;
  std::string_view message_id;
  if (message_index != core::kInvalidIndex) {
    Require(message_index < messages.size(), "Binary DTO message index out of range");
    message_id = messages[message_index].id.View();
  }
  std::size_t string_bytes = 0U;
  std::size_t field_bytes = 0U;
  ChargeString(message_id, limits, string_bytes);
  Association raw_indices;
  raw_indices.fill(core::kInvalidIndex);
  Presence seen{};
  if (!success) {
    Require(candidate.field_count == 0U && candidate.fields == nullptr &&
                candidate.RawIntegerCount() == 0U,
            "Failed Binary candidate contains success fields");
  } else {
    // Core marks preserved unknown enums as tainted while still returning OK. Keep that fact
    // in decoded instead of rejecting the explicitly supported unknown-enum policy.
    Require(message_index < messages.size(), "Binary DTO success candidate has invalid message");
    const auto& fields = messages[message_index].fields;
    Require(fields.size() <= limits.max_fields && candidate.field_count == fields.size() &&
                candidate.decoded.field_count == candidate.field_count &&
                (candidate.field_count == 0U || candidate.fields != nullptr),
            "Binary DTO field count mismatch or bound exceeded");
    const auto raw_count = candidate.RawIntegerCount();
    Require(raw_count <= candidate.field_count, "Binary DTO raw count exceeds fields");
    for (std::size_t index = 0U; index < raw_count; ++index) {
      core::RawIntegerValue raw;
      Require(candidate.GetRawInteger(index, raw), "Binary DTO raw read failed");
      const auto field_index = raw.field.field_index;
      Require(field_index < fields.size() && SameField(raw.field, candidate, field_index),
              "Binary DTO raw field reference mismatch");
      const auto& field = fields[field_index];
      Require(Converted(field) && field.conversion_index < candidate.plan->Conversions().size() &&
                  raw_indices[field_index] == core::kInvalidIndex,
              "Binary DTO raw association duplicate or unexpected");
      const auto& conversion = candidate.plan->Conversions()[field.conversion_index];
      Require(conversion.raw_value_type == field.value_type &&
                  ((field.value_type == plan::ValueType::UINT64 &&
                    raw.kind == core::RawIntegerKind::UINT64) ||
                   (field.value_type == plan::ValueType::INT64 &&
                    raw.kind == core::RawIntegerKind::INT64)),
              "Binary DTO raw kind mismatch");
      raw_indices[field_index] = index;
    }
    for (std::size_t index = 0U; index < candidate.field_count; ++index) {
      const auto& slot = candidate.fields[index];
      const auto field_index = slot.field.field_index;
      Require(field_index < fields.size() && SameField(slot.field, candidate, field_index) &&
                  !seen[field_index],
              "Binary DTO decoded field reference duplicate or mismatch");
      seen[field_index] = true;
      const auto& field = fields[field_index];
      Require(slot.value_kind == ExpectedKind(field), "Binary DTO logical kind mismatch");
      ChargeString(field.id.View(), limits, string_bytes);
      // Every FieldValue owns these two strings, even when they remain empty.
      std::string_view enum_id;
      if (slot.value_kind == core::LogicalValueKind::ENUM) {
        enum_id = ValidateEnum(slot, field, candidate);
      }
      ChargeString(enum_id, limits, string_bytes);
      ChargeString({}, limits, string_bytes);
      if (Converted(field)) {
        Require(raw_indices[field_index] != core::kInvalidIndex &&
                    field.conversion_index < candidate.plan->Conversions().size(),
                "Binary DTO converted field missing raw association");
        // Core normalizes trailing decimal zeros (including zero itself); the normalized scale
        // may be smaller than the configured precision, but cannot be negative or larger.
        Require(slot.decimal64_value.scale >= 0 &&
                    slot.decimal64_value.scale <=
                        candidate.plan->Conversions()[field.conversion_index].decimal_places,
                "Binary DTO decimal scale mismatch");
      }
      if (slot.value_kind == core::LogicalValueKind::BYTES) {
        ValidateByteView(slot, candidate);
        field_bytes = Add(field_bytes, slot.bytes_value.size);
        Require(field_bytes <= limits.max_field_bytes, "Binary DTO field bytes budget exceeded");
      }
    }
  }
  const auto slots = success ? candidate.field_count : 0U;
  const auto preflight = Add(
      Add(Add(sizeof(CandidateResult), candidate.frame.size), Multiply(slots, sizeof(FieldValue))),
      Add(Add(field_bytes, string_bytes), kScratchBytes));
  Require(preflight <= limits.max_total_bytes, "Binary DTO copy budget exceeded before allocation");

#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  BeforeCopy(hooks);
#endif
  CandidateResult result;
  result.generation = candidate.generation;
  result.decoded = candidate.decoded;
  result.message_index = message_index;
  if (!message_id.empty()) result.message_id.assign(message_id.data(), message_id.size());
  auto& frame = success ? result.frame : result.diagnostic_frame;
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  BeforeCopy(hooks);
#endif
  if (candidate.frame.size != 0U) {
    frame.assign(candidate.frame.data, candidate.frame.data + candidate.frame.size);
  }
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  BeforeCopy(hooks);
#endif
  result.fields.resize(slots);
  for (std::size_t index = 0U; index < slots; ++index) {
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
    BeforeCopy(hooks);
#endif
    const auto& slot = candidate.fields[index];
    const auto& field = messages[message_index].fields[slot.field.field_index];
    auto& output = result.fields[index];
    output.index = slot.field.field_index;
    output.id.assign(field.id.data(), field.id.size());
    output.kind = slot.value_kind;
    switch (slot.value_kind) {
      case core::LogicalValueKind::UINT64:
        output.uint64_value = slot.uint64_value;
        break;
      case core::LogicalValueKind::INT64:
        output.int64_value = slot.int64_value;
        break;
      case core::LogicalValueKind::BOOL:
        output.bool_value = slot.bool_value;
        break;
      case core::LogicalValueKind::BYTES:
        if (slot.bytes_value.size != 0U) {
          output.bytes_value.assign(slot.bytes_value.data,
                                    slot.bytes_value.data + slot.bytes_value.size);
        }
        break;
      case core::LogicalValueKind::ENUM: {
        output.enum_value.raw_value = slot.enum_value.raw_value;
        output.enum_value.known = slot.enum_value.known;
        if (slot.enum_value.known) {
          const auto id = field.enum_entries[slot.enum_value.reference.entry_index].id.View();
          output.enum_value.item_id.assign(id.data(), id.size());
        }
        break;
      }
      case core::LogicalValueKind::DECIMAL64: {
        output.decimal64_value = slot.decimal64_value;
        core::RawIntegerValue raw;
        Require(candidate.GetRawInteger(raw_indices[output.index], raw) &&
                    SameField(raw.field, candidate, output.index),
                "Binary DTO raw reread failed");
        output.raw_integer = RawInteger{raw.kind, raw.uint64_value, raw.int64_value};
        break;
      }
    }
  }
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
  BeforeCopy(hooks);
#endif
  FinalizeBudget(result, limits);
  return result;
}

}  // namespace pae::protocol_lab_binary
