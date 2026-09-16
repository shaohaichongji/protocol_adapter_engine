#include <pae/compiler.h>
#include <pae/host_endpoint.h>

#include <array>
#include <atomic>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "codec_test_support.h"

namespace {

std::atomic<std::size_t> g_allocation_count{0U};

class Runner final {
 public:
  void Check(bool condition, std::string_view name) {
    if (condition) {
      ++passed_;
      std::cout << "PASS case=" << name << '\n';
    } else {
      ++failed_;
      std::cerr << "FAIL case=" << name << '\n';
    }
  }

  int Finish() const {
    std::cout << "PUBLIC_PHYSICAL_QUERY_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " gate=" << (failed_ == 0U ? "PASS" : "FAIL") << '\n';
    return failed_ == 0U ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

pae::CompiledProtocol Compile(const char* path, Runner& runner, std::string_view name) {
  auto result = pae::CompileProtocolJson(ReadFile(path));
  runner.Check(result.Succeeded(), name);
  return result.Succeeded() ? std::move(result).TakeCompiled() : pae::CompiledProtocol{};
}

std::optional<pae::FieldDescription> FindField(const pae::CompiledProtocol& compiled,
                                               std::size_t message_index,
                                               std::string_view id) noexcept {
  const auto message = compiled.Message(message_index);
  if (!message.has_value()) return std::nullopt;
  for (std::size_t offset = 0U; offset < message->field_count; ++offset) {
    const auto field = compiled.Field(message->field_begin + offset);
    if (field.has_value() && field->id == id) return field;
  }
  return std::nullopt;
}

bool RangeEquals(const std::optional<pae::ByteRange>& actual, std::size_t offset,
                 std::size_t length) noexcept {
  return actual.has_value() && actual->offset == offset && actual->length == length;
}

bool MaskEquals(const pae::FieldPhysicalDescription& field, std::size_t ordinal,
                std::size_t byte_index, std::uint8_t mask) noexcept {
  return ordinal < field.bit_mask_count &&
         field.bit_masks[ordinal].frame_byte_index == byte_index &&
         field.bit_masks[ordinal].mask == mask;
}

void CheckBitfields(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "bitfield_compile");
  const auto representation = compiled.MessageRepresentation(0U);
  const auto message = compiled.MessagePhysical(0U);
  runner.Check(representation.status == pae::PhysicalQueryStatus::OK &&
                   representation.value.has_value() &&
                   *representation.value == pae::RecordRepresentation::BINARY &&
                   message.status == pae::PhysicalQueryStatus::OK && message.value.has_value() &&
                   message.value->record_length.minimum == 18U &&
                   message.value->record_length.maximum == 18U &&
                   !message.value->maximum_integrity_storage.has_value() &&
                   !message.value->computed_length_storage.has_value(),
               "binary_representation_and_fixed_record");

  const auto enabled = FindField(compiled, 0U, "enabled");
  const auto cross_lsb = FindField(compiled, 0U, "cross_lsb");
  const auto cross_msb = FindField(compiled, 0U, "cross_msb");
  const auto middle = FindField(compiled, 0U, "middle16");
  const auto full = FindField(compiled, 0U, "full64");
  const auto last = FindField(compiled, 0U, "last_bit");
  const auto enabled_layout =
      enabled ? compiled.FieldPhysical(enabled->flat_index) : pae::FieldPhysicalQueryResult{};
  const auto lsb_layout =
      cross_lsb ? compiled.FieldPhysical(cross_lsb->flat_index) : pae::FieldPhysicalQueryResult{};
  const auto msb_layout =
      cross_msb ? compiled.FieldPhysical(cross_msb->flat_index) : pae::FieldPhysicalQueryResult{};
  const auto middle_layout =
      middle ? compiled.FieldPhysical(middle->flat_index) : pae::FieldPhysicalQueryResult{};
  const auto full_layout =
      full ? compiled.FieldPhysical(full->flat_index) : pae::FieldPhysicalQueryResult{};
  const auto last_layout =
      last ? compiled.FieldPhysical(last->flat_index) : pae::FieldPhysicalQueryResult{};

  runner.Check(enabled_layout.value.has_value() &&
                   enabled_layout.value->physical_kind == pae::FieldPhysicalKind::BIT_MASKS &&
                   enabled_layout.value->bit_mask_count == 1U &&
                   MaskEquals(*enabled_layout.value, 0U, 0U, 0x01U),
               "single_byte_lsb0_mask");
  runner.Check(lsb_layout.value.has_value() && lsb_layout.value->bit_mask_count == 2U &&
                   MaskEquals(*lsb_layout.value, 0U, 1U, 0x0FU) &&
                   MaskEquals(*lsb_layout.value, 1U, 2U, 0xF0U),
               "big_endian_lsb0_cross_byte_masks");
  runner.Check(msb_layout.value.has_value() && msb_layout.value->bit_mask_count == 2U &&
                   MaskEquals(*msb_layout.value, 0U, 3U, 0xC0U) &&
                   MaskEquals(*msb_layout.value, 1U, 4U, 0x7FU),
               "little_endian_msb0_cross_byte_masks");
  runner.Check(middle_layout.value.has_value() && middle_layout.value->bit_mask_count == 2U &&
                   MaskEquals(*middle_layout.value, 0U, 6U, 0xFFU) &&
                   MaskEquals(*middle_layout.value, 1U, 7U, 0xFFU),
               "big_endian_msb0_middle_masks");
  bool full_ok = full_layout.value.has_value() && full_layout.value->bit_mask_count == 8U;
  if (full_ok) {
    for (std::size_t index = 0U; index < 8U; ++index) {
      full_ok = full_ok && MaskEquals(*full_layout.value, index, 9U + index, 0xFFU);
    }
  }
  runner.Check(full_ok, "little_endian_full_width_masks");
  runner.Check(last_layout.value.has_value() && last_layout.value->bit_mask_count == 1U &&
                   MaskEquals(*last_layout.value, 0U, 17U, 0x01U),
               "single_byte_msb0_last_bit_mask");

  const auto resolved = cross_lsb ? compiled.ResolveFieldPhysical(cross_lsb->flat_index, 18U)
                                  : pae::ResolvedFieldPhysicalQueryResult{};
  runner.Check(resolved.status == pae::PhysicalQueryStatus::OK && resolved.value.has_value() &&
                   resolved.value->physical_kind == pae::FieldPhysicalKind::BIT_MASKS &&
                   resolved.value->bit_mask_count == 2U && !resolved.value->byte_range.has_value(),
               "resolved_bitfield_has_masks_not_byte_range");
}

void CheckBounded(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "bounded_compile");
  const auto message = compiled.MessagePhysical(0U);
  runner.Check(message.status == pae::PhysicalQueryStatus::OK && message.value.has_value() &&
                   message.value->record_length.minimum == 3U &&
                   message.value->record_length.maximum == 6U &&
                   RangeEquals(message.value->maximum_integrity_storage, 5U, 1U) &&
                   message.value->integrity_storage_offset_depends_on_frame_size &&
                   RangeEquals(message.value->computed_length_storage, 1U, 1U),
               "bounded_message_static_layout");

  const auto payload = FindField(compiled, 0U, "payload");
  const auto layout =
      payload ? compiled.FieldPhysical(payload->flat_index) : pae::FieldPhysicalQueryResult{};
  runner.Check(layout.status == pae::PhysicalQueryStatus::OK && layout.value.has_value() &&
                   layout.value->physical_kind == pae::FieldPhysicalKind::BYTE_RANGE &&
                   RangeEquals(layout.value->maximum_byte_range, 2U, 3U) &&
                   layout.value->byte_value_length.has_value() &&
                   layout.value->byte_value_length->minimum == 0U &&
                   layout.value->byte_value_length->maximum == 3U &&
                   layout.value->byte_range_length_depends_on_frame_size,
               "bounded_payload_static_bounds");

  const auto zero_message = compiled.ResolveMessagePhysical(0U, 3U);
  const auto middle_message = compiled.ResolveMessagePhysical(0U, 5U);
  const auto maximum_message = compiled.ResolveMessagePhysical(0U, 6U);
  const auto zero_field = payload ? compiled.ResolveFieldPhysical(payload->flat_index, 3U)
                                  : pae::ResolvedFieldPhysicalQueryResult{};
  const auto middle_field = payload ? compiled.ResolveFieldPhysical(payload->flat_index, 5U)
                                    : pae::ResolvedFieldPhysicalQueryResult{};
  const auto maximum_field = payload ? compiled.ResolveFieldPhysical(payload->flat_index, 6U)
                                     : pae::ResolvedFieldPhysicalQueryResult{};
  runner.Check(zero_message.value.has_value() &&
                   RangeEquals(zero_message.value->integrity_storage, 2U, 1U) &&
                   RangeEquals(zero_message.value->computed_length_storage, 1U, 1U) &&
                   zero_field.value.has_value() &&
                   RangeEquals(zero_field.value->byte_range, 2U, 0U),
               "bounded_zero_payload_is_present_empty_range");
  runner.Check(middle_message.value.has_value() &&
                   RangeEquals(middle_message.value->integrity_storage, 4U, 1U) &&
                   middle_field.value.has_value() &&
                   RangeEquals(middle_field.value->byte_range, 2U, 2U),
               "bounded_middle_payload_actual_range");
  runner.Check(maximum_message.value.has_value() &&
                   RangeEquals(maximum_message.value->integrity_storage, 5U, 1U) &&
                   maximum_field.value.has_value() &&
                   RangeEquals(maximum_field.value->byte_range, 2U, 3U),
               "bounded_maximum_payload_actual_range");
  const auto too_small = compiled.ResolveMessagePhysical(0U, 2U);
  const auto too_large = compiled.ResolveFieldPhysical(payload ? payload->flat_index : 0U, 7U);
  runner.Check(too_small.status == pae::PhysicalQueryStatus::FRAME_SIZE_MISMATCH &&
                   !too_small.value.has_value() &&
                   too_large.status == pae::PhysicalQueryStatus::FRAME_SIZE_MISMATCH &&
                   !too_large.value.has_value(),
               "bounded_invalid_lengths_fail_without_partial_value");
}

void CheckFixedIntegrity(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "fixed_integrity_compile");
  const auto message = compiled.MessagePhysical(0U);
  runner.Check(message.status == pae::PhysicalQueryStatus::OK && message.value.has_value() &&
                   message.value->record_length.minimum == 25U &&
                   message.value->record_length.maximum == 25U &&
                   RangeEquals(message.value->maximum_integrity_storage, 24U, 1U) &&
                   !message.value->integrity_storage_offset_depends_on_frame_size,
               "fixed_integrity_storage");
  const auto mismatch = compiled.ResolveMessagePhysical(0U, 24U);
  runner.Check(mismatch.status == pae::PhysicalQueryStatus::FRAME_SIZE_MISMATCH &&
                   !mismatch.value.has_value(),
               "fixed_record_requires_exact_length");
}

void CheckAsciiAndErrors(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "ascii_compile");
  const auto representation = compiled.MessageRepresentation(0U);
  const auto message = compiled.MessagePhysical(0U);
  const auto field = compiled.FieldPhysical(0U);
  const auto resolved = compiled.ResolveMessagePhysical(0U, 1U);
  runner.Check(representation.status == pae::PhysicalQueryStatus::OK &&
                   representation.value.has_value() &&
                   *representation.value == pae::RecordRepresentation::ASCII_TEXT,
               "ascii_representation_is_identifiable");
  runner.Check(message.status == pae::PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED &&
                   !message.value.has_value() &&
                   field.status == pae::PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED &&
                   !field.value.has_value() &&
                   resolved.status == pae::PhysicalQueryStatus::REPRESENTATION_NOT_SUPPORTED &&
                   !resolved.value.has_value(),
               "ascii_physical_queries_are_explicitly_unsupported");

  pae::CompiledProtocol empty;
  runner.Check(
      empty.MessageRepresentation(0U).status ==
              pae::PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL &&
          empty.MessagePhysical(0U).status == pae::PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL &&
          empty.FieldPhysical(0U).status == pae::PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL,
      "empty_owner_is_explicit");
  runner.Check(compiled.MessageRepresentation(compiled.MessageCount()).status ==
                       pae::PhysicalQueryStatus::INDEX_OUT_OF_RANGE &&
                   compiled.FieldPhysical(compiled.FieldCount()).status ==
                       pae::PhysicalQueryStatus::INDEX_OUT_OF_RANGE,
               "out_of_range_indices_are_explicit");
}

void DecodeEntered(void* opaque) noexcept { ++*static_cast<std::size_t*>(opaque); }

struct HostContext {
  const pae::CompiledProtocol* compiled = nullptr;
  std::size_t payload_flat_index = 0U;
  std::size_t callbacks = 0U;
  bool layout_ok = false;
};

pae::HostCallbackAction CaptureHost(const pae::HostOutputView& output, void* opaque) {
  auto& context = *static_cast<HostContext*>(opaque);
  ++context.callbacks;
  const auto message =
      context.compiled->ResolveMessagePhysical(output.message_index, output.frame.size);
  const auto field =
      context.compiled->ResolveFieldPhysical(context.payload_flat_index, output.frame.size);
  context.layout_ok = output.action == pae::HostAction::DECODE && output.record.HasValue() &&
                      message.status == pae::PhysicalQueryStatus::OK && message.value.has_value() &&
                      message.value->frame_size == 10U &&
                      field.status == pae::PhysicalQueryStatus::OK && field.value.has_value() &&
                      RangeEquals(field.value->byte_range, 6U, 2U);
  return pae::HostCallbackAction::CONTINUE;
}

void CheckHostAssociationAndNoAllocation(Runner& runner, const char* path) {
  auto compiled = Compile(path, runner, "host_compile");
  const auto payload = FindField(compiled, 0U, "payload");
  const auto payload_layout =
      payload ? compiled.FieldPhysical(payload->flat_index) : pae::FieldPhysicalQueryResult{};
  runner.Check(payload_layout.status == pae::PhysicalQueryStatus::OK &&
                   payload_layout.value.has_value() &&
                   RangeEquals(payload_layout.value->maximum_byte_range, 6U, 2U) &&
                   payload_layout.value->byte_value_length.has_value() &&
                   payload_layout.value->byte_value_length->minimum == 2U &&
                   payload_layout.value->byte_value_length->maximum == 2U &&
                   !payload_layout.value->byte_range_length_depends_on_frame_size,
               "fixed_bytes_value_length_and_range");
  const pae::HostBindingSpec binding{"physical", pae::HostAction::DECODE, 0U, 1U, {}};
  auto created = pae::CreateHostEndpoint(compiled, &binding, 1U);
  runner.Check(payload.has_value() && created.status == pae::HostStatus::OK && created.host,
               "host_create");
  if (!payload.has_value() || !created.host) return;
  const auto handle = created.host->Find("physical", pae::HostAction::DECODE);
  const std::array<std::uint8_t, 10U> frame{
      {0x80U, 0x0FU, 3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU}};
  HostContext context{&compiled, payload->flat_index};
  std::size_t codec_entries = 0U;
  pae::public_api_internal::test_only::SetCodecEnteredHook(DecodeEntered, &codec_entries);
  const auto result =
      created.host->Decode(handle.handle, {frame.data(), frame.size()}, {CaptureHost, &context});
  pae::public_api_internal::test_only::SetCodecEnteredHook(nullptr, nullptr);
  runner.Check(result.status == pae::HostStatus::OK && result.decode_attempts == 1U &&
                   result.decode_successes == 1U && context.callbacks == 1U && context.layout_ok &&
                   codec_entries == 1U,
               "host_callback_query_uses_same_identity_without_second_decode");

  const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  bool queries_ok = true;
  for (std::size_t index = 0U; index < 128U; ++index) {
    queries_ok =
        queries_ok && compiled.MessageRepresentation(0U).status == pae::PhysicalQueryStatus::OK &&
        compiled.MessagePhysical(0U).status == pae::PhysicalQueryStatus::OK &&
        compiled.ResolveMessagePhysical(0U, 10U).status == pae::PhysicalQueryStatus::OK &&
        compiled.FieldPhysical(payload->flat_index).status == pae::PhysicalQueryStatus::OK &&
        compiled.ResolveFieldPhysical(payload->flat_index, 10U).status ==
            pae::PhysicalQueryStatus::OK;
  }
  const std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  runner.Check(queries_ok && before == after, "repeated_physical_queries_do_not_allocate");

  pae::CompiledProtocol moved = std::move(compiled);
  runner.Check(
      compiled.MessagePhysical(0U).status == pae::PhysicalQueryStatus::INVALID_COMPILED_PROTOCOL &&
          moved.MessagePhysical(0U).status == pae::PhysicalQueryStatus::OK,
      "move_invalidates_source_query_owner");
}

}  // namespace

void* operator new(std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* storage = std::malloc(size)) return storage;
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* storage = std::malloc(size)) return storage;
  throw std::bad_alloc{};
}

void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }

int main(int argc, char** argv) {
  Runner runner;
  runner.Check(argc == 6, "arguments");
  if (argc != 6) return runner.Finish();
  CheckBitfields(runner, argv[1]);
  CheckBounded(runner, argv[2]);
  CheckFixedIntegrity(runner, argv[3]);
  CheckAsciiAndErrors(runner, argv[4]);
  CheckHostAssociationAndNoAllocation(runner, argv[5]);
  return runner.Finish();
}
