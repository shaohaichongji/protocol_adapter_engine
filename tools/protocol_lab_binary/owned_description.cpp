#include "owned_description.h"

#include <algorithm>
#include <limits>
#include <string_view>

namespace pae::protocol_lab_binary {
namespace {
namespace plan = protocol_plan;
constexpr std::size_t kInvalid = static_cast<std::size_t>(-1);
void Require(bool ok, const char* message) {
  if (!ok) throw MaterializationError(message);
}
std::size_t Add(std::size_t a, std::size_t b) {
  Require(b <= std::numeric_limits<std::size_t>::max() - a, "description size overflow");
  return a + b;
}
std::size_t Mul(std::size_t a, std::size_t b) {
  Require(a == 0U || b <= std::numeric_limits<std::size_t>::max() / a, "description size overflow");
  return a * b;
}
std::size_t Size(std::uint64_t value) {
  Require(value <= std::numeric_limits<std::size_t>::max(), "description integer overflow");
  return static_cast<std::size_t>(value);
}
bool Fits(ByteRange range, std::size_t size) noexcept {
  return range.offset <= size && range.length <= size - range.offset;
}
bool FrameValid(const MessageDescriptor& message, std::size_t size) noexcept {
  if (!message.bounded_payload) return size == message.frame_size;
  const auto& b = *message.bounded_payload;
  if (size < b.min_frame_length || size > b.max_frame_length || b.header_length > size ||
      b.trailer_length > size - b.header_length)
    return false;
  const auto length = size - b.header_length - b.trailer_length;
  return length >= b.min_payload_length && length <= b.max_payload_length;
}
struct Accounting {
  std::size_t bytes = sizeof(OwnedDescription);
  std::size_t limit;
  void Charge(std::size_t n) {
    bytes = Add(bytes, n);
    Require(bytes <= limit, "description budget exceeded");
  }
  void String(std::string_view value, std::string* target, std::size_t id_limit = kInvalid) {
    Require(value.size() <= id_limit, "description identity too long");
    Charge(std::max(Add(Mul(value.size(), 2U), 32U), Add(std::string{}.capacity(), 1U)));
    if (target) target->assign(value.data(), value.size());
  }
};
template <class Meta, class Descriptor>
void Metadata(const config_compiler::UiDescriptionSidecar& sidecar, const Meta& metadata,
              Descriptor* item, Accounting& a) {
  const auto copy = [&](auto span, std::string* target) {
    const auto value = sidecar.Resolve(span);
    Require(value.size() == span.size, "description string span invalid");
    a.String(value, target);
  };
  copy(metadata.display_name, item ? &item->display_name : nullptr);
  copy(metadata.description, item ? &item->description : nullptr);
  copy(metadata.source_ref, item ? &item->source_ref : nullptr);
}

void Walk(const plan::PlanBundle& p, const config_compiler::UiDescriptionSidecar& s,
          const DescriptionLimits& limits, OwnedDescription* out) {
  Accounting a{sizeof(OwnedDescription), limits.max_description_bytes};
  a.Charge(0U);
  Require(p.SchemaVersion() == "0.9" && !s.empty(), "description requires Schema 0.9 artifacts");
  Require(p.Messages().size() == s.Messages().size() &&
              p.Pipelines().size() == s.Pipelines().size() &&
              p.MessageExecutionPlans().size() == p.Messages().size(),
          "description top-level mismatch");
  const auto max_frame = Size(p.GetResourceRequirements().max_frame_bytes);
  Require(max_frame > 0U && max_frame <= limits.max_frame_bytes,
          "description frame limit exceeded");
  a.String(p.SchemaVersion(), out ? &out->schema_version : nullptr, limits.max_identity_bytes);
  a.String(p.ProtocolId(), out ? &out->protocol_id : nullptr, limits.max_identity_bytes);
  a.String(p.ProtocolVersion(), out ? &out->protocol_version : nullptr, limits.max_identity_bytes);
  Metadata(s, s.Protocol(), out, a);
  a.Charge(Mul(p.Pipelines().size(), sizeof(PipelineDescriptor)));
  a.Charge(Mul(p.Messages().size(), sizeof(MessageDescriptor)));
  if (out) {
    out->max_frame_bytes = max_frame;
    out->pipelines.resize(p.Pipelines().size());
    out->messages.resize(p.Messages().size());
  }
  for (std::size_t i = 0; i < p.Pipelines().size(); ++i) {
    const auto& source = p.Pipelines()[i];
    auto* item = out ? &out->pipelines[i] : nullptr;
    a.String(source.id.View(), item ? &item->id : nullptr, limits.max_identity_bytes);
    a.String(source.direction_id.View(), item ? &item->direction_id : nullptr,
             limits.max_identity_bytes);
    Metadata(s, s.Pipelines()[i], item, a);
    a.Charge(Mul(source.message_indices.size(), sizeof(std::size_t)));
    for (auto index : source.message_indices)
      Require(index < p.Messages().size(), "description pipeline index invalid");
    if (item) {
      item->pipeline_index = i;
      if (!source.message_indices.empty())
        item->message_indices.assign(source.message_indices.begin(), source.message_indices.end());
    }
  }
  std::size_t field_count = 0U, enum_count = 0U;
  for (std::size_t i = 0; i < p.Messages().size(); ++i) {
    const auto& source = p.Messages()[i];
    const auto& execution = p.MessageExecutionPlans()[i];
    const auto& meta = s.Messages()[i];
    MessageDescriptor local;
    auto* item = out ? &out->messages[i] : nullptr;
    auto& mapping = item ? *item : local;
    Require(meta.field_begin == field_count && meta.field_begin <= s.Fields().size() &&
                meta.field_count <= s.Fields().size() - meta.field_begin &&
                meta.field_count == source.fields.size() &&
                execution.fields.size() == source.fields.size(),
            "description fields mismatch");
    field_count = Add(field_count, source.fields.size());
    Require(field_count <= limits.max_fields, "description field limit exceeded");
    a.String(source.id.View(), item ? &item->id : nullptr, limits.max_identity_bytes);
    a.String(source.direction_id.View(), item ? &item->direction_id : nullptr,
             limits.max_identity_bytes);
    Metadata(s, meta, item, a);
    mapping.message_index = i;
    mapping.frame_size = execution.frame_size;
    Require(mapping.frame_size > 0U && mapping.frame_size <= max_frame,
            "description frame invalid");
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (execution.bounded_payload) {
      const auto& b = *execution.bounded_payload;
      mapping.bounded_payload = BoundedPayloadDescriptor{
          b.payload_field_index,      Size(b.header_length),      Size(b.trailer_length),
          Size(b.min_payload_length), Size(b.max_payload_length), Size(b.min_frame_length),
          Size(b.max_frame_length)};
      const auto& copied = *mapping.bounded_payload;
      Require(copied.payload_field_index < source.fields.size() &&
                  copied.min_payload_length <= copied.max_payload_length &&
                  Add(Add(copied.header_length, copied.min_payload_length),
                      copied.trailer_length) == copied.min_frame_length &&
                  Add(Add(copied.header_length, copied.max_payload_length),
                      copied.trailer_length) == copied.max_frame_length &&
                  copied.max_frame_length == mapping.frame_size,
              "description bounded payload invalid");
    }
#endif
    if (execution.integrity) {
      const auto& integrity = *execution.integrity;
      std::size_t width = 1U;
      if (integrity.algorithm != plan::IntegrityAlgorithm::SUM8) {
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
        Require(integrity.algorithm == plan::IntegrityAlgorithm::CRC,
                "description integrity algorithm invalid");
        Require(integrity.crc_width == 8U || integrity.crc_width == 16U ||
                    integrity.crc_width == 32U || integrity.crc_width == 64U,
                "description CRC width invalid");
        width = integrity.crc_width / 8U;
#else
        Require(false, "description CRC unsupported");
#endif
      }
      mapping.integrity_storage = ByteRange{Size(integrity.storage_offset), width};
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      mapping.integrity_storage_at_payload_end = integrity.storage_at_payload_end;
#endif
      Require(ResolveActualIntegrityStorage(mapping, mapping.bounded_payload
                                                         ? mapping.bounded_payload->min_frame_length
                                                         : mapping.frame_size)
                  .has_value(),
              "description integrity range invalid");
    }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (execution.computed_length) {
      mapping.computed_length_storage = ByteRange{Size(execution.computed_length->storage_offset),
                                                  Size(execution.computed_length->storage_width)};
      Require(Fits(*mapping.computed_length_storage, mapping.bounded_payload
                                                         ? mapping.bounded_payload->min_frame_length
                                                         : mapping.frame_size),
              "description length range invalid");
    }
#endif
    a.Charge(Mul(source.fields.size(), sizeof(FieldDescriptor)));
    if (item) item->fields.resize(source.fields.size());
    for (std::size_t j = 0; j < source.fields.size(); ++j) {
      const auto& f = source.fields[j];
      const auto& e = execution.fields[j];
      const auto& fm = s.Fields()[meta.field_begin + j];
      auto* field = item ? &item->fields[j] : nullptr;
      if (f.conversion_index != kInvalid) {
        Require(f.conversion_index < p.Conversions().size(), "description conversion invalid");
        const auto& conversion = p.Conversions()[f.conversion_index];
        Require(conversion.raw_value_type == f.value_type, "description conversion type mismatch");
        if (field) field->conversion = conversion;
      }
      a.String(f.id.View(), field ? &field->id : nullptr, limits.max_identity_bytes);
      Metadata(s, fm, field, a);
      Require(fm.enum_begin == enum_count && fm.enum_begin <= s.Enums().size() &&
                  fm.enum_count <= s.Enums().size() - fm.enum_begin &&
                  fm.enum_count == f.enum_entries.size(),
              "description enums mismatch");
      enum_count = Add(enum_count, fm.enum_count);
      a.Charge(Mul(f.enum_entries.size(), sizeof(EnumDescriptor)));
      if (field) {
        field->field_index = j;
        field->value_type = f.value_type;
        field->wire_codec = f.wire_codec;
        field->byte_order = f.byte_order;
        field->encode_source = f.encode_source;
        field->enum_entries.resize(f.enum_entries.size());
      }
      for (std::size_t k = 0; k < f.enum_entries.size(); ++k) {
        auto* entry = field ? &field->enum_entries[k] : nullptr;
        a.String(f.enum_entries[k].id.View(), entry ? &entry->id : nullptr,
                 limits.max_identity_bytes);
        auto span = s.Enums()[fm.enum_begin + k].display_name;
        auto name = s.Resolve(span);
        Require(name.size() == span.size, "description enum span invalid");
        a.String(name, entry ? &entry->display_name : nullptr);
        if (entry) {
          entry->entry_index = k;
          entry->raw_value = f.enum_entries[k].raw_value;
        }
      }
      const auto minimum_frame =
          mapping.bounded_payload ? mapping.bounded_payload->min_frame_length : mapping.frame_size;
      if (e.bit_container_index == kInvalid) {
        const bool payload =
            mapping.bounded_payload && mapping.bounded_payload->payload_field_index == j;
        Require(Fits(ByteRange{e.offset, e.width}, payload ? mapping.frame_size : minimum_frame),
                "description field range invalid");
        if (payload)
          Require(f.value_type == plan::ValueType::BYTES &&
                      e.offset == mapping.bounded_payload->header_length &&
                      e.width == mapping.bounded_payload->max_payload_length,
                  "description payload field mismatch");
        if (field) field->byte_range = ByteRange{e.offset, e.width};
      } else {
        Require(e.bit_container_index < execution.bit_containers.size(),
                "description bit container invalid");
        const auto& c = execution.bit_containers[e.bit_container_index];
        Require(
            c.width > 0U && c.width <= 8U && Fits(ByteRange{c.offset, c.width}, minimum_frame) &&
                (c.byte_order == plan::ByteOrder::LITTLE || c.byte_order == plan::ByteOrder::BIG) &&
                e.bit_width > 0U && e.bit_width <= 64U && e.bit_shift < 64U &&
                e.bit_width <= 64U - e.bit_shift && e.bit_width == f.bit_width,
            "description bit layout invalid");
        const auto expected = e.bit_width == 64U
                                  ? std::numeric_limits<std::uint64_t>::max()
                                  : ((std::uint64_t{1} << e.bit_width) - 1U) << e.bit_shift;
        Require(e.bit_mask == expected && (c.width == 8U || (e.bit_mask >> (c.width * 8U)) == 0U),
                "description bit mask invalid");
        std::size_t mask_count = 0U;
        for (std::size_t b = 0; b < c.width; ++b) {
          const auto numeric_byte = c.byte_order == plan::ByteOrder::LITTLE ? b : c.width - 1U - b;
          if (static_cast<std::uint8_t>(e.bit_mask >> (numeric_byte * 8U)) != 0U) ++mask_count;
        }
        a.Charge(Mul(mask_count, sizeof(PhysicalBitMask)));
        if (field) {
          field->physical_bits.reserve(mask_count);
          for (std::size_t b = 0; b < c.width; ++b) {
            const auto numeric_byte =
                c.byte_order == plan::ByteOrder::LITTLE ? b : c.width - 1U - b;
            const auto mask = static_cast<std::uint8_t>(e.bit_mask >> (numeric_byte * 8U));
            if (mask != 0U) field->physical_bits.push_back(PhysicalBitMask{c.offset + b, mask});
          }
        }
      }
    }
  }
  Require(field_count == s.Fields().size() && enum_count == s.Enums().size(),
          "description unused metadata");
}
std::size_t ActualBytes(const OwnedDescription& out, std::size_t limit) {
  Accounting a{sizeof(OwnedDescription), limit};
  a.Charge(0U);
  const auto str = [&](const std::string& s) { a.Charge(Add(s.capacity(), 1U)); };
  const auto meta = [&](const auto& item) {
    str(item.display_name);
    str(item.description);
    str(item.source_ref);
  };
  str(out.schema_version);
  str(out.protocol_id);
  str(out.protocol_version);
  meta(out);
  a.Charge(Mul(out.pipelines.capacity(), sizeof(PipelineDescriptor)));
  a.Charge(Mul(out.messages.capacity(), sizeof(MessageDescriptor)));
  for (const auto& p : out.pipelines) {
    str(p.id);
    str(p.direction_id);
    meta(p);
    a.Charge(Mul(p.message_indices.capacity(), sizeof(std::size_t)));
  }
  for (const auto& m : out.messages) {
    str(m.id);
    str(m.direction_id);
    meta(m);
    a.Charge(Mul(m.fields.capacity(), sizeof(FieldDescriptor)));
    for (const auto& f : m.fields) {
      str(f.id);
      meta(f);
      a.Charge(Mul(f.physical_bits.capacity(), sizeof(PhysicalBitMask)));
      a.Charge(Mul(f.enum_entries.capacity(), sizeof(EnumDescriptor)));
      for (const auto& e : f.enum_entries) {
        str(e.id);
        str(e.display_name);
      }
    }
  }
  return a.bytes;
}
}  // namespace

OwnedDescription BuildOwnedDescription(const config_compiler::CompiledUiArtifacts& artifacts,
                                       const DescriptionLimits& limits) {
  Require(limits.max_description_bytes <= 4U * 1024U * 1024U && limits.max_fields <= 1024U &&
              limits.max_frame_bytes <= 65536U && limits.max_identity_bytes <= 256U,
          "description limits cannot enlarge hard bounds");
  Require(artifacts.Plan() != nullptr, "description artifacts have no Plan");
  Walk(*artifacts.Plan(), artifacts.Description(), limits, nullptr);
  OwnedDescription out;
  Walk(*artifacts.Plan(), artifacts.Description(), limits, &out);
  out.accounted_total_bytes = ActualBytes(out, limits.max_description_bytes);
  return out;
}

bool IsActualFrameValid(const MessageDescriptor& message, std::size_t size) noexcept {
  return FrameValid(message, size);
}

std::optional<ByteRange> ResolveActualFieldRange(const MessageDescriptor& message,
                                                 const FieldDescriptor& field,
                                                 std::size_t size) noexcept {
  if (!FrameValid(message, size) || field.field_index >= message.fields.size() ||
      &message.fields[field.field_index] != &field)
    return std::nullopt;
  if (message.bounded_payload &&
      message.bounded_payload->payload_field_index == field.field_index) {
    const auto& b = *message.bounded_payload;
    return ByteRange{b.header_length, size - b.header_length - b.trailer_length};
  }
  if (!field.byte_range || !Fits(*field.byte_range, size)) return std::nullopt;
  return field.byte_range;
}
std::optional<ByteRange> ResolveActualIntegrityStorage(const MessageDescriptor& message,
                                                       std::size_t size) noexcept {
  if (!FrameValid(message, size) || !message.integrity_storage) return std::nullopt;
  auto range = *message.integrity_storage;
  if (message.integrity_storage_at_payload_end) {
    if (!message.bounded_payload || range.length > message.bounded_payload->trailer_length)
      return std::nullopt;
    range.offset = size - message.bounded_payload->trailer_length;
  }
  if (!Fits(range, size)) return std::nullopt;
  return range;
}
}  // namespace pae::protocol_lab_binary
