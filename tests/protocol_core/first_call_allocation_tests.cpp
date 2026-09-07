#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

#include "complete_record_codec.h"
#include "test_plan_factory.h"

namespace {

std::atomic<std::size_t> g_allocation_count = ATOMIC_VAR_INIT(0U);

void* AllocateAligned(std::size_t size, std::size_t requested_alignment) {
  const std::size_t alignment =
      (std::max)(requested_alignment, static_cast<std::size_t>(alignof(void*)));
  const std::size_t payload_size = size == 0U ? 1U : size;
  if (alignment == 0U || (alignment & (alignment - 1U)) != 0U ||
      payload_size > (std::numeric_limits<std::size_t>::max)() - sizeof(void*) - alignment + 1U) {
    throw std::bad_alloc{};
  }
  const std::size_t allocation_size = payload_size + sizeof(void*) + alignment - 1U;
  void* raw_memory = std::malloc(allocation_size);
  if (raw_memory == nullptr) {
    throw std::bad_alloc{};
  }
  void* aligned_memory = static_cast<unsigned char*>(raw_memory) + sizeof(void*);
  std::size_t remaining_space = allocation_size - sizeof(void*);
  if (std::align(alignment, payload_size, aligned_memory, remaining_space) == nullptr) {
    std::free(raw_memory);
    throw std::bad_alloc{};
  }
  reinterpret_cast<void**>(aligned_memory)[-1] = raw_memory;
  return aligned_memory;
}

void FreeAligned(void* memory) noexcept {
  if (memory != nullptr) {
    std::free(reinterpret_cast<void**>(memory)[-1]);
  }
}

}  // namespace

void* operator new(std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* memory = std::malloc(size == 0U ? 1U : size)) {
    return memory;
  }
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) { return ::operator new(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return ::operator new(size);
  } catch (...) {
    return nullptr;
  }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  return ::operator new(size, std::nothrow);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  return AllocateAligned(size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
  return ::operator new(size, alignment);
}

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  try {
    return ::operator new(size, alignment);
  } catch (...) {
    return nullptr;
  }
}

void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
  return ::operator new(size, alignment, std::nothrow);
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete(void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete[](void* memory, const std::nothrow_t&) noexcept { std::free(memory); }
void operator delete(void* memory, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete[](void* memory, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete(void* memory, std::size_t, std::align_val_t) noexcept { FreeAligned(memory); }
void operator delete[](void* memory, std::size_t, std::align_val_t) noexcept {
  FreeAligned(memory);
}
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept {
  FreeAligned(memory);
}
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept {
  FreeAligned(memory);
}

namespace {

using pae::config_compiler::CompileResult;
using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodeCompleteRecord;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::EncodeCompleteRecord;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;
using pae::protocol_plan::ByteOrder;
using pae::protocol_plan::EncodeSource;
using pae::protocol_plan::FieldPlan;
using pae::protocol_plan::FramingPlan;
using pae::protocol_plan::InputKind;
using pae::protocol_plan::MatcherKind;
using pae::protocol_plan::MatcherPlan;
using pae::protocol_plan::MessagePlan;
using pae::protocol_plan::PipelinePlan;
using pae::protocol_plan::ResourceProfile;
using pae::protocol_plan::ValueType;
using pae::protocol_plan::WireCodec;
using pae::test_support::CompileTestPlan;

CompileResult BuildPlan() {
  MessagePlan message;
  message.id = "first_call_message";
  message.direction_id = "first_call_direction";
  message.frame_length_bytes = 2U;

  MatcherPlan length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = 2U;
  message.matchers.push_back(length_matcher);
  MatcherPlan fixed_matcher;
  fixed_matcher.kind = MatcherKind::FIXED_BYTES;
  fixed_matcher.byte_offset = 0U;
  fixed_matcher.bytes.push_back(0xA5U);
  message.matchers.push_back(std::move(fixed_matcher));

  FieldPlan constant;
  constant.id = "prefix";
  constant.value_type = ValueType::UINT64;
  constant.wire_codec = WireCodec::UNSIGNED_INTEGER;
  constant.byte_offset = 0U;
  constant.byte_width = 1U;
  constant.byte_order = ByteOrder::NOT_APPLICABLE;
  constant.encode_source = EncodeSource::CONSTANT;
  constant.constant_value = 0xA5U;
  message.fields.push_back(std::move(constant));

  FieldPlan input;
  input.id = "value";
  input.value_type = ValueType::UINT64;
  input.wire_codec = WireCodec::UNSIGNED_INTEGER;
  input.byte_offset = 1U;
  input.byte_width = 1U;
  input.byte_order = ByteOrder::NOT_APPLICABLE;
  input.encode_source = EncodeSource::INPUT;
  message.fields.push_back(std::move(input));

  PipelinePlan pipeline;
  pipeline.id = "first_call_pipeline";
  pipeline.direction_id = "first_call_direction";
  pipeline.framing_profile_index = 0U;
  pipeline.message_indices.push_back(0U);

  return CompileTestPlan("first_call_protocol", ResourceProfile::DESKTOP,
                         {FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                         {std::move(pipeline)}, {std::move(message)});
}

CompileResult BuildBitfieldPlan() {
  return pae::config_compiler::CompileJsonToPlan(R"json({
    "schema_version":"0.2","protocol_id":"allocation_bitfield","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:allocation",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"complete_record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","input_kind":"complete_record"}],
    "pipelines":[{"id":"bit_pipe","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"bit_direction",
      "input_framing_profile_id":"complete_record","message_ids":["bit_message"]}],
    "messages":[{"id":"bit_message","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"bit_direction",
      "frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},
      "bit_containers":[{"id":"bits","container_offset":0,"container_width":1,
        "bit_numbering":"lsb0","base_value":240}],
      "fields":[
        {"id":"enabled","display_name":"Synthetic","description":"",
         "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","value_type":"BOOL",
         "wire":{"codec":"bitfield","container_id":"bits","bit_offset":0,"bit_width":1},
         "encode":{"source":"input"}},
        {"id":"value","display_name":"Synthetic","description":"",
         "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","value_type":"UINT64",
         "wire":{"codec":"bitfield","container_id":"bits","bit_offset":1,"bit_width":3},
         "encode":{"source":"input"}}]}]})json");
}

CompileResult BuildSum8Plan() {
  return pae::config_compiler::CompileJsonToPlan(R"json({
    "schema_version":"0.3","protocol_id":"allocation_sum8","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:allocation",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"complete_record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","input_kind":"complete_record"}],
    "pipelines":[{"id":"sum_pipe","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"sum_direction",
      "input_framing_profile_id":"complete_record","message_ids":["sum_message"]}],
    "messages":[{"id":"sum_message","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"sum_direction",
      "frame_length_bytes":2,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":2}]},
      "integrity":{"algorithm":"sum8","range":{"byte_offset":0,"byte_length":1},
        "storage":{"byte_offset":1}},
      "fields":[{"id":"value","display_name":"Synthetic","description":"",
        "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","value_type":"UINT64",
        "wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},
        "encode":{"source":"input"}}]}]})json");
}

CompileResult BuildInt64Plan() {
  return pae::config_compiler::CompileJsonToPlan(R"json({
    "schema_version":"0.4","protocol_id":"allocation_int64","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:allocation",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"complete_record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","input_kind":"complete_record"}],
    "pipelines":[{"id":"signed_pipe","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"signed_direction",
      "input_framing_profile_id":"complete_record","message_ids":["signed_message"]}],
    "messages":[{"id":"signed_message","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"signed_direction",
      "frame_length_bytes":3,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":3}]},
      "fields":[{"id":"value","display_name":"Synthetic","description":"",
        "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","value_type":"INT64",
        "wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":3,"byte_order":"big_endian"},
        "encode":{"source":"input"}}]}]})json");
}

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
CompileResult BuildDecimalPlan() {
  return pae::config_compiler::CompileJsonToPlan(R"json({
    "schema_version":"0.5","protocol_id":"allocation_decimal","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:allocation",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"complete_record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","input_kind":"complete_record"}],
    "pipelines":[{"id":"decimal_pipe","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"decimal_direction",
      "input_framing_profile_id":"complete_record","message_ids":["decimal_message"]}],
    "messages":[{"id":"decimal_message","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","direction_id":"decimal_direction",
      "frame_length_bytes":8,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":8}]},
      "fields":[{"id":"value","display_name":"Synthetic","description":"",
        "source_ref":"SYNTHETIC_FROM_SCRATCH:allocation","value_type":"INT64",
        "wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":8,"byte_order":"big_endian"},
        "encode":{"source":"input"},"conversion":{"kind":"linear","output_type":"DECIMAL64",
        "scale":{"numerator":1,"denominator":10},"bias":{"numerator":-40,"denominator":1}}}]}]})json");
}
#endif

bool CounterProbe() {
  const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  void* memory = ::operator new(17U);
  const std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  ::operator delete(memory);
  return after == before + 1U;
}

bool RunFirstDecode() {
  CompileResult frozen = BuildPlan();
  if (!frozen.Succeeded()) {
    return false;
  }
  ExecutionWorkspace workspace{*frozen.Plan()};
  constexpr std::array<std::uint8_t, 2U> frame{0xA5U, 0x11U};
  std::array<DecodedFieldSlot, 2U> slots{};
  const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto result =
      DecodeCompleteRecord(*frozen.Plan(), workspace, 0U, ByteView{frame.data(), frame.size()},
                           slots.data(), slots.size());
  const std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  return result.status == CodecStatus::OK && result.field_count == 2U &&
         slots[0].uint64_value == 0xA5U && slots[1].uint64_value == 0x11U && before == after;
}

bool RunFirstEncode() {
  CompileResult frozen = BuildPlan();
  if (!frozen.Succeeded()) {
    return false;
  }
  ExecutionWorkspace workspace{*frozen.Plan()};
  EncodeFieldValue input;
  input.field = FieldRef{frozen.Plan(), 0U, 1U};
  input.value_kind = LogicalValueKind::UINT64;
  input.uint64_value = 0x11U;
  std::array<std::uint8_t, 2U> output{0xCCU, 0xCCU};
  const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto result = EncodeCompleteRecord(*frozen.Plan(), workspace, 0U, 0U, &input, 1U,
                                           MutableByteBuffer{output.data(), output.size()});
  const std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  return result.status == CodecStatus::OK && result.bytes_written == output.size() &&
         output == std::array<std::uint8_t, 2U>{0xA5U, 0x11U} && before == after;
}

bool RunFirstBitfieldCalls() {
  CompileResult frozen = BuildBitfieldPlan();
  if (!frozen.Succeeded()) return false;
  ExecutionWorkspace workspace{*frozen.Plan()};
  std::array<EncodeFieldValue, 2U> values{};
  values[0].field = FieldRef{frozen.Plan(), 0U, 0U};
  values[0].value_kind = LogicalValueKind::BOOL;
  values[0].bool_value = true;
  values[1].field = FieldRef{frozen.Plan(), 0U, 1U};
  values[1].uint64_value = 5U;
  std::array<std::uint8_t, 1U> output{0xCCU};
  std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto encoded = EncodeCompleteRecord(*frozen.Plan(), workspace, 0U, 0U, values.data(),
                                            values.size(), {output.data(), output.size()});
  std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  if (encoded.status != CodecStatus::OK || output[0] != 0xFBU || before != after) return false;
  std::array<DecodedFieldSlot, 2U> slots{};
  before = g_allocation_count.load(std::memory_order_relaxed);
  const auto decoded = DecodeCompleteRecord(
      *frozen.Plan(), workspace, 0U, {output.data(), output.size()}, slots.data(), slots.size());
  after = g_allocation_count.load(std::memory_order_relaxed);
  return decoded.status == CodecStatus::OK && slots[0].bool_value && slots[1].uint64_value == 5U &&
         before == after;
}

bool RunFirstSum8Calls() {
  CompileResult frozen = BuildSum8Plan();
  if (!frozen.Succeeded()) return false;
  ExecutionWorkspace workspace{*frozen.Plan()};
  EncodeFieldValue value;
  value.field = FieldRef{frozen.Plan(), 0U, 0U};
  value.uint64_value = 0x11U;
  std::array<std::uint8_t, 2U> output{0xCCU, 0xCCU};
  std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto encoded = EncodeCompleteRecord(*frozen.Plan(), workspace, 0U, 0U, &value, 1U,
                                            {output.data(), output.size()});
  std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  if (encoded.status != CodecStatus::OK || output != std::array<std::uint8_t, 2U>{0x11U, 0x11U} ||
      before != after)
    return false;
  std::array<DecodedFieldSlot, 1U> slots{};
  before = g_allocation_count.load(std::memory_order_relaxed);
  const auto decoded = DecodeCompleteRecord(
      *frozen.Plan(), workspace, 0U, {output.data(), output.size()}, slots.data(), slots.size());
  after = g_allocation_count.load(std::memory_order_relaxed);
  return decoded.status == CodecStatus::OK && decoded.field_count == 1U &&
         slots[0].uint64_value == 0x11U && before == after;
}

bool RunFirstInt64Calls() {
  CompileResult frozen = BuildInt64Plan();
  if (!frozen.Succeeded()) return false;
  ExecutionWorkspace workspace{*frozen.Plan()};
  EncodeFieldValue value;
  value.field = FieldRef{frozen.Plan(), 0U, 0U};
  value.value_kind = LogicalValueKind::INT64;
  value.int64_value = -2;
  std::array<std::uint8_t, 3U> output{0xCCU, 0xCCU, 0xCCU};
  std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto encoded = EncodeCompleteRecord(*frozen.Plan(), workspace, 0U, 0U, &value, 1U,
                                            {output.data(), output.size()});
  std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  if (encoded.status != CodecStatus::OK ||
      output != std::array<std::uint8_t, 3U>{0xFFU, 0xFFU, 0xFEU} || before != after)
    return false;
  DecodedFieldSlot slot;
  before = g_allocation_count.load(std::memory_order_relaxed);
  const auto decoded = DecodeCompleteRecord(*frozen.Plan(), workspace, 0U,
                                            {output.data(), output.size()}, &slot, 1U);
  after = g_allocation_count.load(std::memory_order_relaxed);
  return decoded.status == CodecStatus::OK && decoded.field_count == 1U &&
         slot.value_kind == LogicalValueKind::INT64 && slot.int64_value == -2 && before == after;
}

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
bool RunFirstDecimalCalls() {
  CompileResult frozen = BuildDecimalPlan();
  if (!frozen.Succeeded()) return false;
  ExecutionWorkspace workspace{*frozen.Plan()};
  EncodeFieldValue value;
  value.field = FieldRef{frozen.Plan(), 0U, 0U};
  value.value_kind = LogicalValueKind::DECIMAL64;
  value.decimal64_value = {123, 1};
  std::array<std::uint8_t, 8U> output{};
  std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  const auto encoded = EncodeCompleteRecord(*frozen.Plan(), workspace, 0U, 0U, &value, 1U,
                                            {output.data(), output.size()});
  std::size_t after = g_allocation_count.load(std::memory_order_relaxed);
  if (encoded.status != CodecStatus::OK || output[6] != 0x02U || output[7] != 0x0BU ||
      before != after)
    return false;
  DecodedFieldSlot slot;
  before = g_allocation_count.load(std::memory_order_relaxed);
  const auto decoded = DecodeCompleteRecord(*frozen.Plan(), workspace, 0U,
                                            {output.data(), output.size()}, &slot, 1U);
  after = g_allocation_count.load(std::memory_order_relaxed);
  return decoded.status == CodecStatus::OK && slot.value_kind == LogicalValueKind::DECIMAL64 &&
         slot.decimal64_value.coefficient == 123 && slot.decimal64_value.scale == 1 &&
         before == after;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2 || !CounterProbe()) {
    std::cerr << "FIRST_CALL_ALLOCATION_TEST_SUMMARY passed=0 failed=1 expected=1 gate=FAIL\n";
    return 1;
  }
  const std::string_view mode{argv[1]};
  const bool passed = mode == "--first-decode"     ? RunFirstDecode()
                      : mode == "--first-encode"   ? RunFirstEncode()
                      : mode == "--first-bitfield" ? RunFirstBitfieldCalls()
                      : mode == "--first-sum8"     ? RunFirstSum8Calls()
                      : mode == "--first-int64"    ? RunFirstInt64Calls()
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
                      : mode == "--first-decimal" ? RunFirstDecimalCalls()
#endif
                                                  : false;
  const std::string_view case_id =
      mode == "--first-decode"
          ? "first_decode_zero_replaceable_new_allocation"
          : (mode == "--first-encode"
                 ? "first_encode_zero_replaceable_new_allocation"
                 : (mode == "--first-bitfield"
                        ? "first_bitfield_calls_zero_replaceable_new_allocation"
                        : (mode == "--first-sum8"
                               ? "first_sum8_calls_zero_replaceable_new_allocation"
                               : (mode == "--first-int64"
                                      ? "first_int64_calls_zero_replaceable_new_allocation"
                                      : "first_decimal_calls_zero_replaceable_new_allocation"))));
  std::cout << (passed ? "PASS" : "FAIL") << " case=" << case_id << '\n';
  std::cout << "FIRST_CALL_ALLOCATION_TEST_SUMMARY passed=" << (passed ? 1 : 0)
            << " failed=" << (passed ? 0 : 1) << " expected=1 gate=" << (passed ? "PASS" : "FAIL")
            << '\n';
  return passed ? 0 : 1;
}
