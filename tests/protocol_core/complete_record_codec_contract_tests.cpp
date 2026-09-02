#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "complete_record_codec.h"
#include "config_compiler.h"
#include "fixture_reader.h"
#include "plan_builder.h"

namespace {

std::atomic<std::size_t> g_allocation_count = ATOMIC_VAR_INIT(0U);

void* AllocateAligned(std::size_t size, std::size_t requested_alignment) {
  const std::size_t alignment =
      (std::max)(requested_alignment, static_cast<std::size_t>(alignof(void*)));
  if (alignment == 0U || (alignment & (alignment - 1U)) != 0U) {
    throw std::bad_alloc{};
  }

  const std::size_t payload_size = size == 0U ? 1U : size;
  const std::size_t overhead = sizeof(void*) + alignment - 1U;
  if (payload_size > (std::numeric_limits<std::size_t>::max)() - overhead) {
    throw std::bad_alloc{};
  }

  const std::size_t allocation_size = payload_size + overhead;
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

void* operator new[](std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* memory = std::malloc(size == 0U ? 1U : size)) {
    return memory;
  }
  throw std::bad_alloc{};
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return ::operator new(size);
  } catch (...) {
    return nullptr;
  }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return ::operator new[](size);
  } catch (...) {
    return nullptr;
  }
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
  try {
    return ::operator new[](size, alignment);
  } catch (...) {
    return nullptr;
  }
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

using pae::config_compiler::CompileJsonToPlan;
using pae::config_compiler::CompileResult;
using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodeCompleteRecord;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::DecodeResult;
using pae::protocol_core::EncodeCompleteRecord;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::kInvalidIndex;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;
using pae::protocol_core::test_support::DecodeExpectedRow;
using pae::protocol_core::test_support::EncodeInputRow;
using pae::protocol_core::test_support::ManifestEntry;
using pae::protocol_core::test_support::ParseHexBytes;
using pae::protocol_core::test_support::ParseUnsignedDecimal;
using pae::protocol_core::test_support::ReadDecodeExpected;
using pae::protocol_core::test_support::ReadEncodeInput;
using pae::protocol_core::test_support::ReadHexFile;
using pae::protocol_core::test_support::ReadManifest;
using pae::protocol_core::test_support::ReadTextFile;
using pae::protocol_plan::ByteOrder;
using pae::protocol_plan::EncodeSource;
using pae::protocol_plan::EnumEntryPlan;
using pae::protocol_plan::FieldPlan;
using pae::protocol_plan::FramingPlan;
using pae::protocol_plan::InputKind;
using pae::protocol_plan::MatcherKind;
using pae::protocol_plan::MatcherPlan;
using pae::protocol_plan::MessagePlan;
using pae::protocol_plan::PipelinePlan;
using pae::protocol_plan::PlanBuilder;
using pae::protocol_plan::PlanBuildError;
using pae::protocol_plan::PlanBuildResult;
using pae::protocol_plan::PlanBundle;
using pae::protocol_plan::PlanDraft;
using pae::protocol_plan::ResourceProfile;
using pae::protocol_plan::ResourceRequirements;
using pae::protocol_plan::UnknownEnumPolicy;
using pae::protocol_plan::ValueType;
using pae::protocol_plan::WireCodec;

static_assert(!std::is_default_constructible_v<PlanBundle>,
              "a frozen PlanBundle must not be publicly constructible");
static_assert(!std::is_copy_constructible_v<PlanBundle>,
              "a frozen PlanBundle must not be copy constructible");
static_assert(!std::is_move_constructible_v<PlanBundle>,
              "a frozen PlanBundle must not be move constructible");

constexpr std::string_view kRequestVectorId = "synthetic_lab_command_001";
constexpr std::string_view kResponseVectorId = "synthetic_lab_report_001";
constexpr std::string_view kRequestFrameSha256 =
    "492AF67348459F1450FC87287B5CC6907D6FF5E75A295DBA36B489E09909040D";
constexpr std::string_view kResponseFrameSha256 =
    "AB2C014349E0C673FBF38C692DC2B5F1CB70609CF00968D94449A848E3B24E31";

constexpr std::array<std::string_view, 60U> kExpectedCaseIds{
    "fixture_manifest_contract",
    "frozen_execution_descriptors",
    "candidate_group_behavior",
    "enum_lookup_behavior",
    "synthetic_request_decode_exact",
    "synthetic_request_encode_exact",
    "synthetic_response_decode_exact",
    "synthetic_response_encode_exact",
    "encode_repeat_deterministic",
    "encode_input_order_independent",
    "integer_big_width_1_unaligned",
    "integer_big_width_2_unaligned",
    "integer_big_width_3_unaligned",
    "integer_big_width_4_unaligned",
    "integer_big_width_5_unaligned",
    "integer_big_width_6_unaligned",
    "integer_big_width_7_unaligned",
    "integer_big_width_8_unaligned",
    "integer_little_width_1_unaligned",
    "integer_little_width_2_unaligned",
    "integer_little_width_3_unaligned",
    "integer_little_width_4_unaligned",
    "integer_little_width_5_unaligned",
    "integer_little_width_6_unaligned",
    "integer_little_width_7_unaligned",
    "integer_little_width_8_unaligned",
    "replaceable_new_counter_probes",
    "enum_reject_decode",
    "enum_preserve_decode_tainted",
    "decode_unknown_message",
    "freeze_rejects_ambiguous_matcher",
    "decode_truncated_complete_record",
    "decode_extra_complete_record",
    "decode_output_slots_too_small",
    "freeze_rejects_invalid_framing",
    "encode_missing_field",
    "encode_duplicate_field",
    "encode_cross_plan_same_index_field_reference",
    "workspace_plan_mismatch",
    "workspace_reuse",
    "encode_cross_message_field_reference",
    "encode_type_mismatch",
    "encode_uint_width_overflow",
    "encode_bytes_length_short",
    "encode_bytes_length_long",
    "encode_enum_reference_mismatch",
    "encode_cross_plan_enum_reference",
    "encode_constant_override",
    "encode_message_not_allowed",
    "encode_buffer_too_small",
    "encode_descriptor_count_overflow",
    "encode_descriptor_count_exceeds_plan_limit",
    "encode_descriptor_output_overlap",
    "encode_input_output_overlap",
    "decode_input_slot_overlap",
    "freeze_rejects_resource_limit_exceeded",
    "freeze_rejects_incomplete_coverage",
    "encode_final_review_failed",
    "hot_path_decode_success_failure_zero_replaceable_new_allocation",
    "hot_path_encode_success_failure_zero_replaceable_new_allocation",
};

constexpr std::size_t FindExpectedCaseIndex(std::string_view case_id) noexcept {
  for (std::size_t index = 0U; index < kExpectedCaseIds.size(); ++index) {
    if (kExpectedCaseIds[index] == case_id) {
      return index;
    }
  }
  return kExpectedCaseIds.size();
}

constexpr bool ExpectedCaseIdsAreUnique() noexcept {
  for (std::size_t index = 0U; index < kExpectedCaseIds.size(); ++index) {
    for (std::size_t earlier = 0U; earlier < index; ++earlier) {
      if (kExpectedCaseIds[index] == kExpectedCaseIds[earlier]) {
        return false;
      }
    }
  }
  return true;
}

static_assert(ExpectedCaseIdsAreUnique(), "expected test case IDs must be unique");

class TestRunner final {
 public:
  static constexpr std::size_t kExpectedCaseCount = kExpectedCaseIds.size();

  void Record(std::string_view case_id, bool passed, const std::string& detail = std::string{}) {
    const std::size_t expected_index = FindExpectedCaseIndex(case_id);
    if (expected_index == kExpectedCaseIds.size()) {
      ++failed_;
      std::cerr << "FAIL case=" << case_id << " detail=case ID is not in the expected contract";
      if (!detail.empty()) {
        std::cerr << "; " << detail;
      }
      std::cerr << '\n';
      return;
    }
    if (seen_[expected_index]) {
      ++failed_;
      std::cerr << "FAIL case=" << case_id << " detail=duplicate case ID\n";
      return;
    }
    seen_[expected_index] = true;
    if (passed) {
      ++passed_;
      std::cout << "PASS case=" << case_id << '\n';
      return;
    }
    ++failed_;
    std::cerr << "FAIL case=" << case_id;
    if (!detail.empty()) {
      std::cerr << " detail=" << detail;
    }
    std::cerr << '\n';
  }

  int Finish() const {
    const bool all_expected_seen =
        std::all_of(seen_.begin(), seen_.end(), [](bool value) { return value; });
    const bool gate_passed = failed_ == 0U && passed_ == kExpectedCaseCount && all_expected_seen;
    std::cout << "COMPLETE_RECORD_CODEC_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " expected=" << kExpectedCaseCount << " gate=" << (gate_passed ? "PASS" : "FAIL")
              << '\n';
    return gate_passed ? 0 : 1;
  }

 private:
  std::array<bool, kExpectedCaseCount> seen_{};
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

struct LoadedVector {
  ManifestEntry manifest;
  std::vector<std::uint8_t> frame;
  std::vector<DecodeExpectedRow> decode_expected;
  std::vector<EncodeInputRow> encode_input;
};

struct LoadedFixtures {
  std::vector<ManifestEntry> manifest;
  std::vector<LoadedVector> vectors;
};

struct OwnedEncodeValues {
  std::vector<std::vector<std::uint8_t>> byte_storage;
  std::vector<EncodeFieldValue> values;
};

bool TextEquals(const std::string& actual, std::string_view expected) noexcept {
  return actual.size() == expected.size() &&
         std::equal(actual.begin(), actual.end(), expected.begin(), expected.end());
}

bool BytesEqual(ByteView actual, const std::vector<std::uint8_t>& expected) noexcept {
  if (actual.size != expected.size() || (actual.data == nullptr && actual.size != 0U)) {
    return false;
  }
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    if (actual.data[index] != expected[index]) {
      return false;
    }
  }
  return true;
}

bool SlotsUntouched(const DecodedFieldSlot* slots, std::size_t slot_count) noexcept {
  for (std::size_t index = 0U; index < slot_count; ++index) {
    const DecodedFieldSlot& slot = slots[index];
    if (slot.field.plan_scope != nullptr || slot.field.message_index != kInvalidIndex ||
        slot.field.field_index != kInvalidIndex || slot.value_kind != LogicalValueKind::UINT64 ||
        slot.uint64_value != 0U || slot.bytes_value.data != nullptr ||
        slot.bytes_value.size != 0U || slot.enum_value.known || slot.enum_value.raw_value != 0U ||
        slot.enum_value.reference.plan_scope != nullptr ||
        slot.enum_value.reference.message_index != kInvalidIndex ||
        slot.enum_value.reference.field_index != kInvalidIndex ||
        slot.enum_value.reference.entry_index != kInvalidIndex) {
      return false;
    }
  }
  return true;
}

bool SlotsUntouched(const std::vector<DecodedFieldSlot>& slots) noexcept {
  return SlotsUntouched(slots.data(), slots.size());
}

template <std::size_t SlotCount>
bool SlotsUntouched(const std::array<DecodedFieldSlot, SlotCount>& slots) noexcept {
  return SlotsUntouched(slots.data(), slots.size());
}

std::size_t FindPipelineIndex(const PlanBundle& plan, std::string_view id) noexcept {
  const auto& pipelines = plan.Pipelines();
  for (std::size_t index = 0U; index < pipelines.size(); ++index) {
    if (TextEquals(pipelines[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindMessageIndex(const PlanBundle& plan, std::string_view id) noexcept {
  const auto& messages = plan.Messages();
  for (std::size_t index = 0U; index < messages.size(); ++index) {
    if (TextEquals(messages[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindFieldIndex(const MessagePlan& message, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < message.fields.size(); ++index) {
    if (TextEquals(message.fields[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindEnumIndex(const FieldPlan& field, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < field.enum_entries.size(); ++index) {
    if (TextEquals(field.enum_entries[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindValueIndex(const std::vector<EncodeFieldValue>& values, std::size_t message_index,
                           std::size_t field_index) noexcept {
  for (std::size_t index = 0U; index < values.size(); ++index) {
    if (values[index].field.message_index == message_index &&
        values[index].field.field_index == field_index) {
      return index;
    }
  }
  return kInvalidIndex;
}

const LoadedVector* FindLoadedVector(const LoadedFixtures& fixtures,
                                     std::string_view vector_id) noexcept {
  for (const LoadedVector& fixture : fixtures.vectors) {
    if (TextEquals(fixture.manifest.vector_id, vector_id)) {
      return &fixture;
    }
  }
  return nullptr;
}

bool LoadFixtures(const std::filesystem::path& data_root, LoadedFixtures& output,
                  std::string& error) {
  const std::filesystem::path golden_root = data_root / "golden";
  if (!ReadManifest(golden_root / "manifest_v0.1.tsv", output.manifest, error)) {
    return false;
  }

  output.vectors.clear();
  output.vectors.reserve(output.manifest.size());
  for (const ManifestEntry& entry : output.manifest) {
    LoadedVector fixture;
    fixture.manifest = entry;
    if (!ReadHexFile(golden_root / entry.frame_file, fixture.frame, error) ||
        !ReadDecodeExpected(golden_root / entry.decode_expected_file, fixture.decode_expected,
                            error) ||
        !ReadEncodeInput(golden_root / entry.encode_input_file, fixture.encode_input, error)) {
      return false;
    }
    output.vectors.emplace_back(std::move(fixture));
  }
  return true;
}

bool BuildEncodeValues(const PlanBundle& plan, std::size_t message_index,
                       const std::vector<EncodeInputRow>& rows, OwnedEncodeValues& output,
                       std::string& error) {
  if (message_index >= plan.Messages().size()) {
    error = "message index is outside PlanBundle";
    return false;
  }
  const MessagePlan& message = plan.Messages()[message_index];
  output.byte_storage.clear();
  output.values.clear();
  output.byte_storage.reserve(rows.size());
  output.values.reserve(rows.size());

  for (const EncodeInputRow& row : rows) {
    const std::size_t field_index = FindFieldIndex(message, row.field_id);
    if (field_index == kInvalidIndex) {
      error = "encode fixture references unknown field '" + row.field_id + "'";
      return false;
    }
    const FieldPlan& field = message.fields[field_index];
    if (field.encode_source != EncodeSource::INPUT) {
      error = "encode fixture attempts to supply constant field '" + row.field_id + "'";
      return false;
    }

    EncodeFieldValue value;
    value.field = FieldRef{&plan, message_index, field_index};
    if (row.value_kind == "UINT64") {
      value.value_kind = LogicalValueKind::UINT64;
      if (!ParseUnsignedDecimal(row.logical_value, value.uint64_value, error)) {
        error = "field '" + row.field_id + "': " + error;
        return false;
      }
    } else if (row.value_kind == "BYTES") {
      value.value_kind = LogicalValueKind::BYTES;
      output.byte_storage.emplace_back();
      if (!ParseHexBytes(row.logical_value, output.byte_storage.back(), error)) {
        error = "field '" + row.field_id + "': " + error;
        return false;
      }
      const auto& bytes = output.byte_storage.back();
      value.bytes_value = ByteView{bytes.data(), bytes.size()};
    } else if (row.value_kind == "ENUM") {
      value.value_kind = LogicalValueKind::ENUM;
      const std::size_t entry_index = FindEnumIndex(field, row.logical_value);
      if (entry_index == kInvalidIndex) {
        error = "encode fixture references unknown enum entry '" + row.logical_value + "'";
        return false;
      }
      value.enum_value =
          pae::protocol_core::EnumValueRef{&plan, message_index, field_index, entry_index};
    } else {
      error = "unsupported encode fixture value_kind '" + row.value_kind + "'";
      return false;
    }
    output.values.push_back(value);
  }
  return true;
}

bool CheckManifestContract(const LoadedFixtures& fixtures, const PlanBundle& plan,
                           std::string& error) {
  if (fixtures.manifest.size() != 2U || fixtures.vectors.size() != 2U) {
    error = "manifest must contain exactly two independently loaded vectors";
    return false;
  }
  const LoadedVector* request = FindLoadedVector(fixtures, kRequestVectorId);
  const LoadedVector* response = FindLoadedVector(fixtures, kResponseVectorId);
  if (request == nullptr || response == nullptr) {
    error = "required request or response vector is missing";
    return false;
  }

  const auto check_vector = [&](const LoadedVector& fixture, std::string_view pipeline_id,
                                std::string_view message_id, std::string_view direction_id,
                                std::string_view frame_file, std::string_view decode_expected_file,
                                std::string_view encode_input_file,
                                std::string_view frame_sha256) -> bool {
    const ManifestEntry& manifest = fixture.manifest;
    const std::size_t pipeline_index = FindPipelineIndex(plan, pipeline_id);
    const std::size_t message_index = FindMessageIndex(plan, message_id);
    if (manifest.protocol_id != plan.ProtocolId() ||
        manifest.protocol_version != plan.ProtocolVersion() ||
        !TextEquals(manifest.pipeline_id, pipeline_id) ||
        !TextEquals(manifest.message_id, message_id) ||
        !TextEquals(manifest.direction_id, direction_id) ||
        manifest.authority_class != "SYNTHETIC_REVIEWED" ||
        manifest.review_status != "INDEPENDENT_ENGINEERING_REVIEWED" ||
        !TextEquals(manifest.frame_file.generic_string(), frame_file) ||
        !TextEquals(manifest.decode_expected_file.generic_string(), decode_expected_file) ||
        !TextEquals(manifest.encode_input_file.generic_string(), encode_input_file) ||
        !TextEquals(manifest.frame_file_sha256, frame_sha256) || pipeline_index == kInvalidIndex ||
        message_index == kInvalidIndex || fixture.frame.empty() ||
        fixture.decode_expected.empty() || fixture.encode_input.empty()) {
      return false;
    }

    const PipelinePlan& pipeline = plan.Pipelines()[pipeline_index];
    const MessagePlan& message = plan.Messages()[message_index];
    return TextEquals(pipeline.direction_id, direction_id) &&
           TextEquals(message.direction_id, direction_id) &&
           std::find(pipeline.message_indices.begin(), pipeline.message_indices.end(),
                     message_index) != pipeline.message_indices.end();
  };

  const bool request_valid =
      check_vector(*request, "host_to_fixture", "lab_command", "host_to_fixture",
                   "synthetic_lab_exchange/lab_command_001.frame.hex",
                   "synthetic_lab_exchange/lab_command_001.decode_expected.tsv",
                   "synthetic_lab_exchange/lab_command_001.encode_input.tsv", kRequestFrameSha256);
  const bool response_valid =
      check_vector(*response, "fixture_to_host", "lab_report", "fixture_to_host",
                   "synthetic_lab_exchange/lab_report_001.frame.hex",
                   "synthetic_lab_exchange/lab_report_001.decode_expected.tsv",
                   "synthetic_lab_exchange/lab_report_001.encode_input.tsv", kResponseFrameSha256);
  const bool paths_are_unique =
      request->manifest.frame_file != response->manifest.frame_file &&
      request->manifest.decode_expected_file != response->manifest.decode_expected_file &&
      request->manifest.encode_input_file != response->manifest.encode_input_file;
  if (!request_valid || !response_valid || !paths_are_unique) {
    error =
        "manifest vector identity, direction, canonical path, SHA-256, or Plan binding mismatch";
    return false;
  }
  return true;
}

bool CheckSyntheticDecode(const PlanBundle& plan, const LoadedVector& fixture, std::string& error) {
  const std::size_t pipeline_index = FindPipelineIndex(plan, fixture.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, fixture.manifest.message_id);
  if (pipeline_index == kInvalidIndex || message_index == kInvalidIndex) {
    error = "synthetic vector stable ID cannot be resolved";
    return false;
  }
  const MessagePlan& message = plan.Messages()[message_index];
  std::vector<DecodedFieldSlot> slots(message.fields.size());
  ExecutionWorkspace workspace{plan};
  const auto result = DecodeCompleteRecord(plan, workspace, pipeline_index,
                                           ByteView{fixture.frame.data(), fixture.frame.size()},
                                           slots.data(), slots.size());
  if (result.status != CodecStatus::OK || result.message_index != message_index ||
      result.field_count != message.fields.size() ||
      result.required_field_count != message.fields.size() || result.tainted ||
      fixture.decode_expected.size() != message.fields.size()) {
    error = "synthetic Decode result metadata mismatch";
    return false;
  }

  std::vector<bool> seen(message.fields.size(), false);
  for (const DecodeExpectedRow& expected : fixture.decode_expected) {
    const std::size_t field_index = FindFieldIndex(message, expected.field_id);
    if (field_index == kInvalidIndex || seen[field_index]) {
      error = "synthetic Decode expected field set is invalid";
      return false;
    }
    seen[field_index] = true;
    const FieldPlan& field = message.fields[field_index];
    const DecodedFieldSlot& slot = slots[field_index];
    if (slot.field.plan_scope != &plan || slot.field.message_index != message_index ||
        slot.field.field_index != field_index) {
      error = "decoded FieldRef mismatch for " + expected.field_id;
      return false;
    }

    if (expected.value_kind == "UINT64") {
      std::uint64_t logical = 0U;
      std::uint64_t raw = 0U;
      if (field.value_type != ValueType::UINT64 || slot.value_kind != LogicalValueKind::UINT64 ||
          !ParseUnsignedDecimal(expected.logical_value, logical, error) ||
          !ParseUnsignedDecimal(expected.raw_value, raw, error) || logical != raw ||
          slot.uint64_value != logical) {
        error = "decoded UINT64 mismatch for " + expected.field_id + ": " + error;
        return false;
      }
    } else if (expected.value_kind == "BYTES") {
      std::vector<std::uint8_t> logical;
      std::vector<std::uint8_t> raw;
      if (field.value_type != ValueType::BYTES || slot.value_kind != LogicalValueKind::BYTES ||
          !ParseHexBytes(expected.logical_value, logical, error) ||
          !ParseHexBytes(expected.raw_value, raw, error) || logical != raw ||
          !BytesEqual(slot.bytes_value, logical)) {
        error = "decoded BYTES mismatch for " + expected.field_id + ": " + error;
        return false;
      }
    } else if (expected.value_kind == "ENUM") {
      std::uint64_t raw = 0U;
      const std::size_t entry_index = FindEnumIndex(field, expected.logical_value);
      if (field.value_type != ValueType::ENUM || slot.value_kind != LogicalValueKind::ENUM ||
          entry_index == kInvalidIndex || !ParseUnsignedDecimal(expected.raw_value, raw, error) ||
          !slot.enum_value.known || slot.enum_value.raw_value != raw ||
          slot.enum_value.reference.plan_scope != &plan ||
          slot.enum_value.reference.message_index != message_index ||
          slot.enum_value.reference.field_index != field_index ||
          slot.enum_value.reference.entry_index != entry_index ||
          field.enum_entries[entry_index].raw_value != raw) {
        error = "decoded ENUM mismatch for " + expected.field_id + ": " + error;
        return false;
      }
    } else {
      error = "unsupported synthetic Decode value_kind";
      return false;
    }
  }
  return std::all_of(seen.begin(), seen.end(), [](bool value) { return value; });
}

bool CheckSyntheticEncode(const PlanBundle& plan, const LoadedVector& fixture, std::string& error) {
  const std::size_t pipeline_index = FindPipelineIndex(plan, fixture.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, fixture.manifest.message_id);
  OwnedEncodeValues owned;
  if (pipeline_index == kInvalidIndex || message_index == kInvalidIndex ||
      !BuildEncodeValues(plan, message_index, fixture.encode_input, owned, error)) {
    return false;
  }

  std::vector<std::uint8_t> output(fixture.frame.size(), 0xCCU);
  ExecutionWorkspace workspace{plan};
  const auto result =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, owned.values.data(),
                           owned.values.size(), MutableByteBuffer{output.data(), output.size()});
  if (result.status != CodecStatus::OK || result.bytes_written != fixture.frame.size() ||
      result.required_size != fixture.frame.size() || output != fixture.frame) {
    error = "Encode output does not exactly match the independently stored frame";
    return false;
  }
  return true;
}

bool ExpectDecodeFailure(const PlanBundle& plan, std::size_t pipeline_index, ByteView input,
                         std::size_t slot_capacity, CodecStatus expected_status,
                         std::string& error) {
  std::vector<DecodedFieldSlot> slots(slot_capacity);
  ExecutionWorkspace workspace{plan};
  const auto result =
      DecodeCompleteRecord(plan, workspace, pipeline_index, input, slots.data(), slots.size());
  if (result.status != expected_status || result.field_count != 0U || !SlotsUntouched(slots)) {
    error = "Decode failure was not atomic or returned an unexpected status";
    return false;
  }
  return true;
}

bool ExpectEncodeFailure(const PlanBundle& plan, std::size_t pipeline_index,
                         std::size_t message_index, const std::vector<EncodeFieldValue>& values,
                         std::size_t output_capacity, CodecStatus expected_status,
                         std::string& error) {
  std::vector<std::uint8_t> output(output_capacity, 0xCCU);
  ExecutionWorkspace workspace{plan};
  const auto result =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, values.data(),
                           values.size(), MutableByteBuffer{output.data(), output.size()});
  if (result.status != expected_status || result.bytes_written != 0U) {
    error = "Encode failure returned an unexpected status or committed bytes";
    return false;
  }
  return true;
}

ResourceRequirements MakeRequirements(const std::vector<FramingPlan>& framing_profiles,
                                      const std::vector<PipelinePlan>& pipelines,
                                      const std::vector<MessagePlan>& messages) {
  ResourceRequirements requirements;
  requirements.framing_profile_count = framing_profiles.size();
  requirements.pipeline_count = pipelines.size();
  requirements.message_count = messages.size();
  for (const MessagePlan& message : messages) {
    requirements.max_frame_bytes =
        (std::max)(requirements.max_frame_bytes, message.frame_length_bytes);
    requirements.total_field_count += message.fields.size();
    requirements.total_matcher_count += message.matchers.size();
    for (const FieldPlan& field : message.fields) {
      requirements.total_enum_entry_count += field.enum_entries.size();
    }
  }
  return requirements;
}

PlanDraft MakeDraft(std::vector<FramingPlan> framing_profiles, std::vector<PipelinePlan> pipelines,
                    std::vector<MessagePlan> messages) {
  const ResourceRequirements requirements = MakeRequirements(framing_profiles, pipelines, messages);
  PlanDraft draft;
  draft.schema_version = "0.1";
  draft.protocol_id = "synthetic_codec_unit";
  draft.protocol_version = "1";
  draft.resource_profile = ResourceProfile::DESKTOP;
  draft.resource_requirements = requirements;
  draft.framing_profiles = std::move(framing_profiles);
  draft.pipelines = std::move(pipelines);
  draft.messages = std::move(messages);
  return draft;
}

PlanDraft CloneDraft(const PlanBundle& source, std::vector<PipelinePlan> pipelines,
                     std::vector<MessagePlan> messages) {
  PlanDraft draft = MakeDraft(source.FramingProfiles(), std::move(pipelines), std::move(messages));
  draft.schema_version = source.SchemaVersion();
  draft.protocol_id = source.ProtocolId();
  draft.protocol_version = source.ProtocolVersion();
  draft.resource_profile = source.GetResourceProfile();
  return draft;
}

PlanBuildResult FreezeDraft(PlanDraft draft) { return PlanBuilder::Freeze(std::move(draft)); }

FieldPlan MakeUnsignedField(std::string id, std::uint64_t offset, std::uint64_t width,
                            ByteOrder byte_order, EncodeSource source,
                            std::uint64_t constant = 0U) {
  FieldPlan field;
  field.id = std::move(id);
  field.value_type = ValueType::UINT64;
  field.wire_codec = WireCodec::UNSIGNED_INTEGER;
  field.byte_offset = offset;
  field.byte_width = width;
  field.byte_order = byte_order;
  field.encode_source = source;
  if (source == EncodeSource::CONSTANT) {
    field.constant_value = constant;
  }
  return field;
}

PlanBuildResult MakeWidthPlan(std::size_t width, ByteOrder byte_order) {
  MessagePlan message;
  message.id = "width_message";
  message.direction_id = "unit_direction";
  message.frame_length_bytes = width + 1U;
  MatcherPlan length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = message.frame_length_bytes;
  message.matchers.push_back(length_matcher);
  message.fields.push_back(MakeUnsignedField("prefix", 0U, 1U, ByteOrder::NOT_APPLICABLE,
                                             EncodeSource::CONSTANT, 0xA5U));
  message.fields.push_back(
      MakeUnsignedField("unaligned_value", 1U, width, byte_order, EncodeSource::INPUT));

  PipelinePlan pipeline;
  pipeline.id = "unit_pipeline";
  pipeline.direction_id = "unit_direction";
  pipeline.framing_profile_index = 0U;
  pipeline.message_indices.push_back(0U);
  return FreezeDraft(MakeDraft({FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                               {std::move(pipeline)}, {std::move(message)}));
}

std::uint64_t PatternValue(std::size_t width) noexcept {
  constexpr std::array<std::uint8_t, 8U> kPattern{0x11U, 0x22U, 0x33U, 0x44U,
                                                  0x55U, 0x66U, 0x77U, 0x88U};
  std::uint64_t value = 0U;
  for (std::size_t index = 0U; index < width; ++index) {
    value = (value << 8U) | kPattern[index];
  }
  return value;
}

std::vector<std::uint8_t> PatternFrame(std::size_t width, ByteOrder byte_order) {
  constexpr std::array<std::uint8_t, 8U> kPattern{0x11U, 0x22U, 0x33U, 0x44U,
                                                  0x55U, 0x66U, 0x77U, 0x88U};
  std::vector<std::uint8_t> frame;
  frame.reserve(width + 1U);
  frame.push_back(0xA5U);
  if (byte_order == ByteOrder::BIG) {
    frame.insert(frame.end(), kPattern.begin(),
                 kPattern.begin() + static_cast<std::ptrdiff_t>(width));
  } else {
    for (std::size_t index = width; index > 0U; --index) {
      frame.push_back(kPattern[index - 1U]);
    }
  }
  return frame;
}

bool CheckWidthValue(const PlanBundle& plan, std::uint64_t value,
                     const std::vector<std::uint8_t>& expected, std::string_view value_label,
                     std::string& error) {
  EncodeFieldValue input;
  input.field = FieldRef{&plan, 0U, 1U};
  input.value_kind = LogicalValueKind::UINT64;
  input.uint64_value = value;
  std::vector<std::uint8_t> output(expected.size(), 0xCCU);
  ExecutionWorkspace workspace{plan};
  const auto encoded = EncodeCompleteRecord(plan, workspace, 0U, 0U, &input, 1U,
                                            MutableByteBuffer{output.data(), output.size()});
  if (encoded.status != CodecStatus::OK || encoded.bytes_written != expected.size() ||
      encoded.required_size != expected.size() || output != expected) {
    error = "unaligned integer Encode mismatch for " + std::string{value_label};
    return false;
  }

  std::array<DecodedFieldSlot, 2U> slots{};
  const auto decoded = DecodeCompleteRecord(
      plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  if (decoded.status != CodecStatus::OK || decoded.field_count != 2U ||
      slots[0].uint64_value != 0xA5U || slots[1].uint64_value != value ||
      slots[1].field.field_index != 1U) {
    error = "unaligned integer Decode mismatch for " + std::string{value_label};
    return false;
  }
  return true;
}

bool CheckWidthCase(std::size_t width, ByteOrder byte_order, std::string& error) {
  PlanBuildResult frozen = MakeWidthPlan(width, byte_order);
  if (!frozen.Succeeded()) {
    error = "valid width PlanDraft failed to freeze";
    return false;
  }
  const PlanBundle& plan = *frozen.plan;
  const std::uint64_t pattern_value = PatternValue(width);
  const std::vector<std::uint8_t> pattern_frame = PatternFrame(width, byte_order);
  std::vector<std::uint8_t> zero_frame(width + 1U, 0U);
  zero_frame.front() = 0xA5U;
  std::vector<std::uint8_t> maximum_frame(width + 1U, 0xFFU);
  maximum_frame.front() = 0xA5U;
  const std::uint64_t maximum_value =
      width == 8U ? (std::numeric_limits<std::uint64_t>::max)()
                  : (std::uint64_t{1U} << static_cast<unsigned>(width * 8U)) - 1U;

  return CheckWidthValue(plan, pattern_value, pattern_frame, "pattern", error) &&
         CheckWidthValue(plan, 0U, zero_frame, "zero", error) &&
         CheckWidthValue(plan, maximum_value, maximum_frame, "maximum", error);
}

bool CheckReplaceableNewCounterProbes(std::string& error) {
  constexpr std::align_val_t kAlignment{64U};
  const std::size_t before = g_allocation_count.load(std::memory_order_relaxed);
  void* scalar = ::operator new(17U);
  void* array = ::operator new[](19U);
  void* nothrow_scalar = ::operator new(23U, std::nothrow);
  void* aligned_scalar = ::operator new(29U, kAlignment);
  void* aligned_nothrow_array = ::operator new[](31U, kAlignment, std::nothrow);
  const std::size_t after = g_allocation_count.load(std::memory_order_relaxed);

  const bool pointers_valid = scalar != nullptr && array != nullptr && nothrow_scalar != nullptr &&
                              aligned_scalar != nullptr && aligned_nothrow_array != nullptr;
  const bool alignment_valid = aligned_scalar != nullptr && aligned_nothrow_array != nullptr &&
                               reinterpret_cast<std::uintptr_t>(aligned_scalar) % 64U == 0U &&
                               reinterpret_cast<std::uintptr_t>(aligned_nothrow_array) % 64U == 0U;

  ::operator delete(scalar);
  ::operator delete[](array);
  ::operator delete(nothrow_scalar);
  ::operator delete(aligned_scalar, kAlignment);
  ::operator delete[](aligned_nothrow_array, kAlignment);

  if (!pointers_valid || !alignment_valid || after - before != 5U) {
    error = "replaceable new counter did not observe ordinary, array, nothrow, and aligned probes";
    return false;
  }
  return true;
}

PlanBuildResult MakeIncompleteCoveragePlan() {
  MessagePlan message;
  message.id = "incomplete_message";
  message.direction_id = "unit_direction";
  message.frame_length_bytes = 2U;
  MatcherPlan length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = 2U;
  message.matchers.push_back(length_matcher);
  message.fields.push_back(
      MakeUnsignedField("only_first_byte", 0U, 1U, ByteOrder::NOT_APPLICABLE, EncodeSource::INPUT));
  PipelinePlan pipeline;
  pipeline.id = "incomplete_pipeline";
  pipeline.direction_id = "unit_direction";
  pipeline.message_indices.push_back(0U);
  return FreezeDraft(MakeDraft({FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                               {std::move(pipeline)}, {std::move(message)}));
}

PlanDraft MakeOverConstrainedProfileDraft() {
  constexpr std::uint64_t kFrameSize =
      pae::protocol_plan::kConstrainedResourceProfileLimits.max_frame_bytes + 1U;
  MessagePlan message;
  message.id = "over_profile_message";
  message.direction_id = "unit_direction";
  message.frame_length_bytes = kFrameSize;

  MatcherPlan length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = kFrameSize;
  message.matchers.push_back(length_matcher);

  FieldPlan payload;
  payload.id = "payload";
  payload.value_type = ValueType::BYTES;
  payload.wire_codec = WireCodec::BYTES;
  payload.byte_offset = 0U;
  payload.byte_width = kFrameSize;
  payload.byte_order = ByteOrder::NOT_APPLICABLE;
  payload.encode_source = EncodeSource::INPUT;
  message.fields.push_back(std::move(payload));

  PipelinePlan pipeline;
  pipeline.id = "over_profile_pipeline";
  pipeline.direction_id = "unit_direction";
  pipeline.framing_profile_index = 0U;
  pipeline.message_indices.push_back(0U);

  PlanDraft draft = MakeDraft({FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                              {std::move(pipeline)}, {std::move(message)});
  draft.resource_profile = ResourceProfile::CONSTRAINED;
  return draft;
}

PlanBuildResult MakeFinalReviewFailurePlan() {
  MessagePlan message;
  message.id = "final_review_failure_message";
  message.direction_id = "unit_direction";
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

  message.fields.push_back(MakeUnsignedField("matcher_controlled_input", 0U, 1U,
                                             ByteOrder::NOT_APPLICABLE, EncodeSource::INPUT));
  message.fields.push_back(
      MakeUnsignedField("input_value", 1U, 1U, ByteOrder::NOT_APPLICABLE, EncodeSource::INPUT));

  PipelinePlan pipeline;
  pipeline.id = "final_review_failure_pipeline";
  pipeline.direction_id = "unit_direction";
  pipeline.message_indices.push_back(0U);
  return FreezeDraft(MakeDraft({FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                               {std::move(pipeline)}, {std::move(message)}));
}

bool CheckRepeatedEncode(const PlanBundle& plan, const LoadedVector& fixture, std::string& error) {
  const std::size_t pipeline_index = FindPipelineIndex(plan, fixture.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, fixture.manifest.message_id);
  OwnedEncodeValues owned;
  if (!BuildEncodeValues(plan, message_index, fixture.encode_input, owned, error)) {
    return false;
  }
  std::vector<std::uint8_t> first(fixture.frame.size(), 0xCCU);
  std::vector<std::uint8_t> second(fixture.frame.size(), 0x55U);
  ExecutionWorkspace workspace{plan};
  const auto first_result =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, owned.values.data(),
                           owned.values.size(), MutableByteBuffer{first.data(), first.size()});
  const auto second_result =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, owned.values.data(),
                           owned.values.size(), MutableByteBuffer{second.data(), second.size()});
  if (first_result.status != CodecStatus::OK || second_result.status != CodecStatus::OK ||
      first != second || first != fixture.frame) {
    error = "repeated Encode is not deterministic";
    return false;
  }
  return true;
}

bool CheckReorderedEncode(const PlanBundle& plan, const LoadedVector& fixture, std::string& error) {
  const std::size_t pipeline_index = FindPipelineIndex(plan, fixture.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, fixture.manifest.message_id);
  OwnedEncodeValues owned;
  if (!BuildEncodeValues(plan, message_index, fixture.encode_input, owned, error)) {
    return false;
  }
  std::reverse(owned.values.begin(), owned.values.end());
  std::vector<std::uint8_t> output(fixture.frame.size(), 0xCCU);
  ExecutionWorkspace workspace{plan};
  const auto result =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, owned.values.data(),
                           owned.values.size(), MutableByteBuffer{output.data(), output.size()});
  if (result.status != CodecStatus::OK || output != fixture.frame) {
    error = "Encode depends on caller field order";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  TestRunner runner;
  try {
    if (argc != 2) {
      runner.Record("runner_arguments", false, "expected one relative test-data directory");
      return runner.Finish();
    }

    const std::filesystem::path data_root{argv[1]};
    LoadedFixtures fixtures;
    std::string error;
    if (!LoadFixtures(data_root, fixtures, error)) {
      runner.Record("fixture_loading", false, error);
      return runner.Finish();
    }
    std::string config_json;
    if (!ReadTextFile(data_root / "synthetic_lab_exchange_slice.pae.json", config_json, error)) {
      runner.Record("config_loading", false, error);
      return runner.Finish();
    }
    CompileResult compiled = CompileJsonToPlan(config_json);
    if (!compiled.Succeeded()) {
      runner.Record("plan_compilation", false, "synthetic lab exchange Plan did not compile");
      return runner.Finish();
    }
    const PlanBundle& plan = *compiled.plan;
    const LoadedVector* request = FindLoadedVector(fixtures, kRequestVectorId);
    const LoadedVector* response = FindLoadedVector(fixtures, kResponseVectorId);
    if (request == nullptr || response == nullptr) {
      runner.Record("vector_resolution", false, "required Synthetic Engine Vector is missing");
      return runner.Finish();
    }

    error.clear();
    const bool manifest_contract_ok = CheckManifestContract(fixtures, plan, error);
    runner.Record("fixture_manifest_contract", manifest_contract_ok, error);
    if (!manifest_contract_ok) {
      return runner.Finish();
    }
    const std::size_t request_pipeline = FindPipelineIndex(plan, request->manifest.pipeline_id);
    const std::size_t request_message = FindMessageIndex(plan, request->manifest.message_id);
    const std::size_t response_message = FindMessageIndex(plan, response->manifest.message_id);
    if (request_pipeline == kInvalidIndex || request_message == kInvalidIndex ||
        response_message == kInvalidIndex) {
      runner.Record("required_index_resolution", false,
                    "manifest-approved pipeline or message index could not be resolved");
      return runner.Finish();
    }

    const auto& message_execution_plans = plan.MessageExecutionPlans();
    const auto& pipeline_execution_plans = plan.PipelineExecutionPlans();
    const auto& resource_layout = plan.GetExecutionResourceLayout();
    bool descriptors_ok = message_execution_plans.size() == plan.Messages().size() &&
                          pipeline_execution_plans.size() == plan.Pipelines().size() &&
                          resource_layout.max_fields_per_message == 5U &&
                          resource_layout.max_input_fields_per_message == 4U &&
                          resource_layout.encode_value_index_count == 4U &&
                          resource_layout.encode_presence_word_count == 1U &&
                          resource_layout.estimated_workspace_bytes ==
                              4U * sizeof(std::size_t) + sizeof(std::uint64_t);
    for (std::size_t message_index = 0U; descriptors_ok && message_index < plan.Messages().size();
         ++message_index) {
      const MessagePlan& metadata = plan.Messages()[message_index];
      const auto& execution = message_execution_plans[message_index];
      descriptors_ok = execution.frame_size == metadata.frame_length_bytes &&
                       execution.fields.size() == metadata.fields.size();
      for (std::size_t field_index = 0U; descriptors_ok && field_index < metadata.fields.size();
           ++field_index) {
        const FieldPlan& field = metadata.fields[field_index];
        const auto& field_execution = execution.fields[field_index];
        descriptors_ok = field_execution.offset == field.byte_offset &&
                         field_execution.width == field.byte_width &&
                         field_execution.value_type == field.value_type &&
                         field_execution.byte_order == field.byte_order &&
                         field_execution.encode_source == field.encode_source;
      }
    }
    runner.Record("frozen_execution_descriptors", descriptors_ok,
                  "frozen field descriptors or resource layout do not match metadata");

    bool candidate_groups_ok = request_pipeline < pipeline_execution_plans.size();
    if (candidate_groups_ok) {
      const auto& groups = pipeline_execution_plans[request_pipeline].candidate_groups;
      candidate_groups_ok = groups.size() == 1U &&
                            groups.front().frame_size == request->frame.size() &&
                            groups.front().message_indices.size() == 1U &&
                            groups.front().message_indices.front() == request_message;
    }
    runner.Record("candidate_group_behavior", candidate_groups_ok,
                  "request Pipeline was not compiled into its exact frame-length candidate group");

    const MessagePlan& descriptor_request_message = plan.Messages()[request_message];
    const std::size_t descriptor_enum_field =
        FindFieldIndex(descriptor_request_message, "operating_mode");
    bool enum_lookup_ok = descriptor_enum_field != kInvalidIndex;
    if (enum_lookup_ok) {
      const auto& request_execution = message_execution_plans[request_message];
      const auto& enum_field = request_execution.fields[descriptor_enum_field];
      enum_lookup_ok =
          enum_field.enum_lookup_count ==
              descriptor_request_message.fields[descriptor_enum_field].enum_entries.size() &&
          enum_field.enum_values_count == enum_field.enum_lookup_count;
      std::uint64_t previous_raw = 0U;
      for (std::size_t offset = 0U; enum_lookup_ok && offset < enum_field.enum_lookup_count;
           ++offset) {
        const auto& lookup =
            request_execution.enum_lookup_entries[enum_field.enum_lookup_begin + offset];
        enum_lookup_ok =
            lookup.entry_index < enum_field.enum_values_count &&
            (offset == 0U || previous_raw < lookup.raw_value) &&
            request_execution.enum_raw_values[enum_field.enum_values_begin + lookup.entry_index] ==
                lookup.raw_value;
        previous_raw = lookup.raw_value;
      }
    }
    runner.Record("enum_lookup_behavior", enum_lookup_ok,
                  "sorted Enum lookup did not preserve the configured entry index");

    error.clear();
    runner.Record("synthetic_request_decode_exact", CheckSyntheticDecode(plan, *request, error),
                  error);
    error.clear();
    runner.Record("synthetic_request_encode_exact", CheckSyntheticEncode(plan, *request, error),
                  error);
    error.clear();
    runner.Record("synthetic_response_decode_exact", CheckSyntheticDecode(plan, *response, error),
                  error);
    error.clear();
    runner.Record("synthetic_response_encode_exact", CheckSyntheticEncode(plan, *response, error),
                  error);
    error.clear();
    runner.Record("encode_repeat_deterministic", CheckRepeatedEncode(plan, *request, error), error);
    error.clear();
    runner.Record("encode_input_order_independent", CheckReorderedEncode(plan, *request, error),
                  error);

    for (const ByteOrder order : {ByteOrder::BIG, ByteOrder::LITTLE}) {
      for (std::size_t width = 1U; width <= 8U; ++width) {
        const std::string case_id = std::string{"integer_"} +
                                    (order == ByteOrder::BIG ? "big" : "little") + "_width_" +
                                    std::to_string(width) + "_unaligned";
        error.clear();
        runner.Record(case_id, CheckWidthCase(width, order, error), error);
      }
    }
    error.clear();
    runner.Record("replaceable_new_counter_probes", CheckReplaceableNewCounterProbes(error), error);

    OwnedEncodeValues request_values;
    OwnedEncodeValues response_values;
    if (!BuildEncodeValues(plan, request_message, request->encode_input, request_values, error) ||
        !BuildEncodeValues(plan, response_message, response->encode_input, response_values,
                           error)) {
      runner.Record("encode_value_setup", false, error);
      return runner.Finish();
    }

    const MessagePlan& request_message_plan = plan.Messages()[request_message];
    const std::size_t operating_mode_index = FindFieldIndex(request_message_plan, "operating_mode");
    const std::size_t length_field = FindFieldIndex(request_message_plan, "record_length");
    const std::size_t bytes_field = FindFieldIndex(request_message_plan, "payload_tag");
    const std::size_t constant_field = FindFieldIndex(request_message_plan, "operation_code");
    const std::size_t length_value =
        FindValueIndex(request_values.values, request_message, length_field);
    const std::size_t bytes_value =
        FindValueIndex(request_values.values, request_message, bytes_field);
    const std::size_t enum_value =
        FindValueIndex(request_values.values, request_message, operating_mode_index);
    if (operating_mode_index == kInvalidIndex || length_field == kInvalidIndex ||
        bytes_field == kInvalidIndex || constant_field == kInvalidIndex ||
        length_value == kInvalidIndex || bytes_value == kInvalidIndex ||
        enum_value == kInvalidIndex) {
      runner.Record("required_field_resolution", false,
                    "required request field or Encode value index could not be resolved");
      return runner.Finish();
    }

    std::vector<std::uint8_t> unknown_enum_frame = request->frame;
    unknown_enum_frame[unknown_enum_frame.size() - 2U] = 5U;
    unknown_enum_frame[unknown_enum_frame.size() - 1U] = 0U;
    error.clear();
    runner.Record(
        "enum_reject_decode",
        ExpectDecodeFailure(plan, request_pipeline,
                            ByteView{unknown_enum_frame.data(), unknown_enum_frame.size()}, 5U,
                            CodecStatus::UNKNOWN_ENUM_VALUE, error),
        error);

    std::vector<MessagePlan> preserve_messages = plan.Messages();
    preserve_messages[request_message].fields[operating_mode_index].unknown_enum_policy =
        UnknownEnumPolicy::PRESERVE;
    PlanBuildResult preserve_frozen =
        FreezeDraft(CloneDraft(plan, plan.Pipelines(), std::move(preserve_messages)));
    std::array<DecodedFieldSlot, 5U> preserve_slots{};
    bool preserve_ok = preserve_frozen.Succeeded();
    DecodeResult preserve_result;
    if (preserve_ok) {
      ExecutionWorkspace preserve_workspace{*preserve_frozen.plan};
      preserve_result =
          DecodeCompleteRecord(*preserve_frozen.plan, preserve_workspace, request_pipeline,
                               ByteView{unknown_enum_frame.data(), unknown_enum_frame.size()},
                               preserve_slots.data(), preserve_slots.size());
    }
    preserve_ok = preserve_ok && preserve_result.status == CodecStatus::OK &&
                  preserve_result.field_count == 5U && preserve_result.tainted &&
                  !preserve_slots[operating_mode_index].enum_value.known &&
                  preserve_slots[operating_mode_index].enum_value.raw_value == 5U;
    runner.Record("enum_preserve_decode_tainted", preserve_ok,
                  preserve_ok ? "" : "unknown ENUM was not preserved as tainted");

    std::vector<std::uint8_t> unknown_message_frame = request->frame;
    unknown_message_frame[2] = 0x7FU;
    unknown_message_frame[3] = 0xFFU;
    error.clear();
    runner.Record(
        "decode_unknown_message",
        ExpectDecodeFailure(plan, request_pipeline,
                            ByteView{unknown_message_frame.data(), unknown_message_frame.size()},
                            5U, CodecStatus::UNKNOWN_MESSAGE, error),
        error);

    std::vector<MessagePlan> ambiguous_messages = plan.Messages();
    MessagePlan duplicate_message = ambiguous_messages[request_message];
    duplicate_message.id = "lab_command_duplicate";
    ambiguous_messages.push_back(std::move(duplicate_message));
    std::vector<PipelinePlan> ambiguous_pipelines = plan.Pipelines();
    ambiguous_pipelines[request_pipeline].message_indices.push_back(ambiguous_messages.size() - 1U);
    PlanBuildResult ambiguous_frozen = FreezeDraft(
        CloneDraft(plan, std::move(ambiguous_pipelines), std::move(ambiguous_messages)));
    runner.Record("freeze_rejects_ambiguous_matcher",
                  !ambiguous_frozen.Succeeded() && ambiguous_frozen.plan == nullptr &&
                      ambiguous_frozen.diagnostic.has_value() &&
                      ambiguous_frozen.diagnostic->code == PlanBuildError::AMBIGUOUS_MATCHER,
                  "ambiguous PlanDraft was not rejected without a frozen Plan");

    error.clear();
    runner.Record("decode_truncated_complete_record",
                  ExpectDecodeFailure(plan, request_pipeline,
                                      ByteView{request->frame.data(), request->frame.size() - 1U},
                                      5U, CodecStatus::UNKNOWN_MESSAGE, error),
                  error);
    std::vector<std::uint8_t> extra_frame = request->frame;
    extra_frame.push_back(0U);
    error.clear();
    runner.Record("decode_extra_complete_record",
                  ExpectDecodeFailure(plan, request_pipeline,
                                      ByteView{extra_frame.data(), extra_frame.size()}, 5U,
                                      CodecStatus::UNKNOWN_MESSAGE, error),
                  error);
    error.clear();
    runner.Record("decode_output_slots_too_small",
                  ExpectDecodeFailure(plan, request_pipeline,
                                      ByteView{request->frame.data(), request->frame.size()}, 4U,
                                      CodecStatus::OUTPUT_SLOTS_TOO_SMALL, error),
                  error);

    std::vector<PipelinePlan> invalid_pipelines = plan.Pipelines();
    invalid_pipelines[request_pipeline].framing_profile_index = plan.FramingProfiles().size();
    PlanBuildResult invalid_framing_frozen =
        FreezeDraft(CloneDraft(plan, std::move(invalid_pipelines), plan.Messages()));
    runner.Record(
        "freeze_rejects_invalid_framing",
        !invalid_framing_frozen.Succeeded() && invalid_framing_frozen.plan == nullptr &&
            invalid_framing_frozen.diagnostic.has_value() &&
            invalid_framing_frozen.diagnostic->code == PlanBuildError::INVALID_PIPELINE_PLAN,
        "invalid framing reference was not rejected without a frozen Plan");

    std::vector<EncodeFieldValue> missing_values = request_values.values;
    missing_values.pop_back();
    error.clear();
    runner.Record("encode_missing_field",
                  ExpectEncodeFailure(plan, request_pipeline, request_message, missing_values,
                                      request->frame.size(), CodecStatus::MISSING_FIELD, error),
                  error);

    std::vector<EncodeFieldValue> duplicate_values = request_values.values;
    duplicate_values.push_back(duplicate_values.front());
    error.clear();
    runner.Record("encode_duplicate_field",
                  ExpectEncodeFailure(plan, request_pipeline, request_message, duplicate_values,
                                      request->frame.size(), CodecStatus::DUPLICATE_FIELD, error),
                  error);

    CompileResult same_shape_compiled = CompileJsonToPlan(config_json);
    if (!same_shape_compiled.Succeeded()) {
      runner.Record("same_shape_plan_compilation", false,
                    "second independent synthetic lab Plan did not compile");
      return runner.Finish();
    }
    const PlanBundle& same_shape_plan = *same_shape_compiled.plan;
    std::vector<EncodeFieldValue> cross_plan_values = request_values.values;
    cross_plan_values.front().field.plan_scope = &same_shape_plan;
    error.clear();
    runner.Record(
        "encode_cross_plan_same_index_field_reference",
        ExpectEncodeFailure(plan, request_pipeline, request_message, cross_plan_values,
                            request->frame.size(), CodecStatus::FIELD_REFERENCE_MISMATCH, error),
        error);

    ExecutionWorkspace original_workspace{plan};
    std::array<DecodedFieldSlot, 5U> mismatched_slots{};
    const auto mismatched_decode =
        DecodeCompleteRecord(same_shape_plan, original_workspace, request_pipeline,
                             ByteView{request->frame.data(), request->frame.size()},
                             mismatched_slots.data(), mismatched_slots.size());
    ExecutionWorkspace same_shape_workspace{same_shape_plan};
    std::vector<std::uint8_t> mismatched_output(request->frame.size(), 0xCCU);
    const auto mismatched_encode =
        EncodeCompleteRecord(plan, same_shape_workspace, request_pipeline, request_message,
                             request_values.values.data(), request_values.values.size(),
                             MutableByteBuffer{mismatched_output.data(), mismatched_output.size()});
    runner.Record("workspace_plan_mismatch",
                  mismatched_decode.status == CodecStatus::WORKSPACE_PLAN_MISMATCH &&
                      mismatched_decode.field_count == 0U && SlotsUntouched(mismatched_slots) &&
                      mismatched_encode.status == CodecStatus::WORKSPACE_PLAN_MISMATCH &&
                      mismatched_encode.bytes_written == 0U &&
                      std::all_of(mismatched_output.begin(), mismatched_output.end(),
                                  [](std::uint8_t value) { return value == 0xCCU; }),
                  "Workspace accepted a different frozen Plan or committed output");

    ExecutionWorkspace reuse_workspace{plan};
    std::array<DecodedFieldSlot, 5U> reuse_slots{};
    std::vector<std::uint8_t> reuse_output(request->frame.size(), 0xCCU);
    const auto reuse_decode_first =
        DecodeCompleteRecord(plan, reuse_workspace, request_pipeline,
                             ByteView{request->frame.data(), request->frame.size()},
                             reuse_slots.data(), reuse_slots.size());
    const auto reuse_decode_failure =
        DecodeCompleteRecord(plan, reuse_workspace, request_pipeline,
                             ByteView{unknown_message_frame.data(), unknown_message_frame.size()},
                             reuse_slots.data(), reuse_slots.size());
    const auto reuse_decode_second =
        DecodeCompleteRecord(plan, reuse_workspace, request_pipeline,
                             ByteView{request->frame.data(), request->frame.size()},
                             reuse_slots.data(), reuse_slots.size());
    std::fill(reuse_output.begin(), reuse_output.end(), std::uint8_t{0xCCU});
    const auto reuse_encode_failure = EncodeCompleteRecord(
        plan, reuse_workspace, request_pipeline, request_message, missing_values.data(),
        missing_values.size(), MutableByteBuffer{reuse_output.data(), reuse_output.size()});
    const bool failure_output_untouched =
        std::all_of(reuse_output.begin(), reuse_output.end(),
                    [](std::uint8_t value) { return value == 0xCCU; });
    const auto reuse_encode_success = EncodeCompleteRecord(
        plan, reuse_workspace, request_pipeline, request_message, request_values.values.data(),
        request_values.values.size(), MutableByteBuffer{reuse_output.data(), reuse_output.size()});
    runner.Record("workspace_reuse",
                  reuse_decode_first.status == CodecStatus::OK &&
                      reuse_decode_failure.status == CodecStatus::UNKNOWN_MESSAGE &&
                      reuse_decode_failure.field_count == 0U &&
                      reuse_decode_second.status == CodecStatus::OK &&
                      reuse_decode_second.field_count == 5U &&
                      reuse_encode_failure.status == CodecStatus::MISSING_FIELD &&
                      reuse_encode_failure.bytes_written == 0U && failure_output_untouched &&
                      reuse_encode_success.status == CodecStatus::OK &&
                      reuse_output == request->frame,
                  "sequential Workspace reuse retained stale matching or Encode input state");

    std::vector<EncodeFieldValue> cross_message_values = request_values.values;
    cross_message_values.front().field.message_index = response_message;
    error.clear();
    runner.Record(
        "encode_cross_message_field_reference",
        ExpectEncodeFailure(plan, request_pipeline, request_message, cross_message_values,
                            request->frame.size(), CodecStatus::FIELD_REFERENCE_MISMATCH, error),
        error);

    std::vector<EncodeFieldValue> wrong_type_values = request_values.values;
    wrong_type_values.front().value_kind = LogicalValueKind::BYTES;
    error.clear();
    runner.Record("encode_type_mismatch",
                  ExpectEncodeFailure(plan, request_pipeline, request_message, wrong_type_values,
                                      request->frame.size(), CodecStatus::TYPE_MISMATCH, error),
                  error);

    std::vector<EncodeFieldValue> overflow_values = request_values.values;
    overflow_values[length_value].uint64_value = 65536U;
    error.clear();
    runner.Record(
        "encode_uint_width_overflow",
        ExpectEncodeFailure(plan, request_pipeline, request_message, overflow_values,
                            request->frame.size(), CodecStatus::VALUE_NOT_REPRESENTABLE, error),
        error);

    std::vector<std::uint8_t> short_bytes{0xC0U, 0x00U, 0x02U};
    std::vector<EncodeFieldValue> short_bytes_values = request_values.values;
    short_bytes_values[bytes_value].bytes_value = ByteView{short_bytes.data(), short_bytes.size()};
    error.clear();
    runner.Record(
        "encode_bytes_length_short",
        ExpectEncodeFailure(plan, request_pipeline, request_message, short_bytes_values,
                            request->frame.size(), CodecStatus::BYTES_LENGTH_MISMATCH, error),
        error);

    std::vector<std::uint8_t> long_bytes{0xC0U, 0x00U, 0x02U, 0x7BU, 0x00U};
    std::vector<EncodeFieldValue> long_bytes_values = request_values.values;
    long_bytes_values[bytes_value].bytes_value = ByteView{long_bytes.data(), long_bytes.size()};
    error.clear();
    runner.Record(
        "encode_bytes_length_long",
        ExpectEncodeFailure(plan, request_pipeline, request_message, long_bytes_values,
                            request->frame.size(), CodecStatus::BYTES_LENGTH_MISMATCH, error),
        error);

    std::vector<EncodeFieldValue> bad_enum_values = request_values.values;
    bad_enum_values[enum_value].enum_value.entry_index =
        request_message_plan.fields[operating_mode_index].enum_entries.size();
    error.clear();
    runner.Record(
        "encode_enum_reference_mismatch",
        ExpectEncodeFailure(plan, request_pipeline, request_message, bad_enum_values,
                            request->frame.size(), CodecStatus::ENUM_REFERENCE_MISMATCH, error),
        error);

    std::vector<EncodeFieldValue> cross_plan_enum_values = request_values.values;
    cross_plan_enum_values[enum_value].enum_value.plan_scope = &same_shape_plan;
    error.clear();
    runner.Record(
        "encode_cross_plan_enum_reference",
        ExpectEncodeFailure(plan, request_pipeline, request_message, cross_plan_enum_values,
                            request->frame.size(), CodecStatus::ENUM_REFERENCE_MISMATCH, error),
        error);

    std::vector<EncodeFieldValue> constant_override_values = request_values.values;
    EncodeFieldValue constant_override;
    constant_override.field = FieldRef{&plan, request_message, constant_field};
    constant_override.value_kind = LogicalValueKind::UINT64;
    constant_override.uint64_value = 1U;
    constant_override_values.push_back(constant_override);
    error.clear();
    runner.Record(
        "encode_constant_override",
        ExpectEncodeFailure(plan, request_pipeline, request_message, constant_override_values,
                            request->frame.size(), CodecStatus::CONSTANT_FIELD_OVERRIDE, error),
        error);

    error.clear();
    runner.Record(
        "encode_message_not_allowed",
        ExpectEncodeFailure(plan, request_pipeline, response_message, response_values.values,
                            response->frame.size(), CodecStatus::MESSAGE_NOT_ALLOWED, error),
        error);

    error.clear();
    runner.Record(
        "encode_buffer_too_small",
        ExpectEncodeFailure(plan, request_pipeline, request_message, request_values.values,
                            request->frame.size() - 1U, CodecStatus::BUFFER_TOO_SMALL, error),
        error);

    EncodeFieldValue overflow_sentinel;
    std::vector<std::uint8_t> overflow_output(request->frame.size(), 0xCCU);
    ExecutionWorkspace overflow_workspace{plan};
    const std::size_t overflow_value_count =
        (std::numeric_limits<std::size_t>::max)() / sizeof(EncodeFieldValue) + 1U;
    const auto overflow_result = EncodeCompleteRecord(
        plan, overflow_workspace, request_pipeline, request_message, &overflow_sentinel,
        overflow_value_count, MutableByteBuffer{overflow_output.data(), overflow_output.size()});
    runner.Record("encode_descriptor_count_overflow",
                  overflow_result.status == CodecStatus::INVALID_ARGUMENT &&
                      overflow_result.bytes_written == 0U &&
                      std::all_of(overflow_output.begin(), overflow_output.end(),
                                  [](std::uint8_t value) { return value == 0xCCU; }),
                  "overflowing Encode descriptor count was not rejected before reading values");

    std::vector<std::uint8_t> over_limit_output(request->frame.size(), 0xCCU);
    const std::size_t over_limit_value_count = resource_layout.max_fields_per_message + 1U;
    const auto over_limit_result =
        EncodeCompleteRecord(plan, overflow_workspace, request_pipeline, request_message,
                             &overflow_sentinel, over_limit_value_count,
                             MutableByteBuffer{over_limit_output.data(), over_limit_output.size()});
    runner.Record("encode_descriptor_count_exceeds_plan_limit",
                  over_limit_result.status == CodecStatus::INVALID_ARGUMENT &&
                      over_limit_result.bytes_written == 0U &&
                      std::all_of(over_limit_output.begin(), over_limit_output.end(),
                                  [](std::uint8_t value) { return value == 0xCCU; }),
                  "over-limit Encode descriptor count was not rejected before reading values");

    std::vector<EncodeFieldValue> descriptor_alias_values = request_values.values;
    const bool descriptor_span_is_large_enough =
        descriptor_alias_values.size() <=
            (std::numeric_limits<std::size_t>::max)() / sizeof(EncodeFieldValue) &&
        descriptor_alias_values.size() * sizeof(EncodeFieldValue) >= request->frame.size();
    bool descriptor_alias_ok = false;
    if (descriptor_span_is_large_enough) {
      ExecutionWorkspace descriptor_alias_workspace{plan};
      const auto descriptor_alias_result = EncodeCompleteRecord(
          plan, descriptor_alias_workspace, request_pipeline, request_message,
          descriptor_alias_values.data(), descriptor_alias_values.size(),
          MutableByteBuffer{reinterpret_cast<std::uint8_t*>(descriptor_alias_values.data()),
                            request->frame.size()});
      descriptor_alias_ok = descriptor_alias_result.status == CodecStatus::INPUT_OUTPUT_OVERLAP &&
                            descriptor_alias_result.bytes_written == 0U;
    }
    runner.Record("encode_descriptor_output_overlap", descriptor_alias_ok,
                  "overlapping Encode descriptors were not rejected without committed bytes");

    std::vector<std::uint8_t> overlap_output(request->frame.size(), 0xCCU);
    std::vector<EncodeFieldValue> overlap_values = request_values.values;
    overlap_values[bytes_value].bytes_value = ByteView{overlap_output.data() + 1U, 4U};
    ExecutionWorkspace overlap_workspace{plan};
    const auto overlap_result = EncodeCompleteRecord(
        plan, overlap_workspace, request_pipeline, request_message, overlap_values.data(),
        overlap_values.size(), MutableByteBuffer{overlap_output.data(), overlap_output.size()});
    runner.Record("encode_input_output_overlap",
                  overlap_result.status == CodecStatus::INPUT_OUTPUT_OVERLAP &&
                      overlap_result.bytes_written == 0U,
                  "overlapping BYTES input was not rejected without committed bytes");

    PlanBuildResult decode_alias_frozen = MakeWidthPlan(1U, ByteOrder::BIG);
    std::array<DecodedFieldSlot, 2U> decode_alias_slots{};
    DecodeResult decode_alias_result;
    if (decode_alias_frozen.Succeeded()) {
      ExecutionWorkspace decode_alias_workspace{*decode_alias_frozen.plan};
      decode_alias_result = DecodeCompleteRecord(
          *decode_alias_frozen.plan, decode_alias_workspace, 0U,
          ByteView{reinterpret_cast<const std::uint8_t*>(decode_alias_slots.data()), 2U},
          decode_alias_slots.data(), decode_alias_slots.size());
    }
    runner.Record("decode_input_slot_overlap",
                  decode_alias_frozen.Succeeded() &&
                      decode_alias_result.status == CodecStatus::INPUT_OUTPUT_OVERLAP &&
                      decode_alias_result.field_count == 0U &&
                      SlotsUntouched(decode_alias_slots.data(), decode_alias_slots.size()),
                  "overlapping Decode input and slots were not rejected atomically");

    PlanBuildResult over_profile_frozen = PlanBuilder::Freeze(MakeOverConstrainedProfileDraft());
    runner.Record(
        "freeze_rejects_resource_limit_exceeded",
        !over_profile_frozen.Succeeded() && over_profile_frozen.plan == nullptr &&
            over_profile_frozen.diagnostic.has_value() &&
            over_profile_frozen.diagnostic->code == PlanBuildError::RESOURCE_LIMIT_EXCEEDED,
        "over-profile PlanDraft was not rejected without a frozen Plan");

    PlanBuildResult incomplete_frozen = MakeIncompleteCoveragePlan();
    runner.Record("freeze_rejects_incomplete_coverage",
                  !incomplete_frozen.Succeeded() && incomplete_frozen.plan == nullptr &&
                      incomplete_frozen.diagnostic.has_value() &&
                      incomplete_frozen.diagnostic->code == PlanBuildError::FRAME_NOT_FULLY_DEFINED,
                  "incomplete frame coverage was not rejected without a frozen Plan");

    PlanBuildResult final_review_frozen = MakeFinalReviewFailurePlan();
    std::vector<EncodeFieldValue> final_review_values(2U);
    if (final_review_frozen.Succeeded()) {
      final_review_values[0].field = FieldRef{final_review_frozen.plan.get(), 0U, 0U};
      final_review_values[0].value_kind = LogicalValueKind::UINT64;
      final_review_values[0].uint64_value = 0x5AU;
      final_review_values[1].field = FieldRef{final_review_frozen.plan.get(), 0U, 1U};
      final_review_values[1].value_kind = LogicalValueKind::UINT64;
      final_review_values[1].uint64_value = 0x11U;
    }
    error.clear();
    runner.Record("encode_final_review_failed",
                  final_review_frozen.Succeeded() &&
                      ExpectEncodeFailure(*final_review_frozen.plan, 0U, 0U, final_review_values,
                                          2U, CodecStatus::FINAL_REVIEW_FAILED, error),
                  error);

    std::array<DecodedFieldSlot, 5U> hot_decode_slots{};
    std::vector<std::uint8_t> hot_unknown_frame = request->frame;
    hot_unknown_frame[2] = 0x7FU;
    ExecutionWorkspace hot_decode_workspace{plan};
    const std::size_t decode_allocations_before =
        g_allocation_count.load(std::memory_order_relaxed);
    bool hot_decode_ok = true;
    for (std::size_t iteration = 0U; iteration < 256U; ++iteration) {
      const auto success =
          DecodeCompleteRecord(plan, hot_decode_workspace, request_pipeline,
                               ByteView{request->frame.data(), request->frame.size()},
                               hot_decode_slots.data(), hot_decode_slots.size());
      const auto failure =
          DecodeCompleteRecord(plan, hot_decode_workspace, request_pipeline,
                               ByteView{hot_unknown_frame.data(), hot_unknown_frame.size()},
                               hot_decode_slots.data(), hot_decode_slots.size());
      hot_decode_ok = hot_decode_ok && success.status == CodecStatus::OK &&
                      success.field_count == 5U && failure.status == CodecStatus::UNKNOWN_MESSAGE &&
                      failure.field_count == 0U;
    }
    const std::size_t decode_allocations_after = g_allocation_count.load(std::memory_order_relaxed);
    runner.Record("hot_path_decode_success_failure_zero_replaceable_new_allocation",
                  hot_decode_ok && decode_allocations_before == decode_allocations_after,
                  "Decode hot path used replaceable new or returned unexpected metadata");

    std::vector<std::uint8_t> hot_encode_output(request->frame.size(), 0xCCU);
    ExecutionWorkspace hot_encode_workspace{plan};
    const std::size_t encode_allocations_before =
        g_allocation_count.load(std::memory_order_relaxed);
    bool hot_encode_ok = true;
    for (std::size_t iteration = 0U; iteration < 256U; ++iteration) {
      const auto success = EncodeCompleteRecord(
          plan, hot_encode_workspace, request_pipeline, request_message,
          request_values.values.data(), request_values.values.size(),
          MutableByteBuffer{hot_encode_output.data(), hot_encode_output.size()});
      const auto failure = EncodeCompleteRecord(
          plan, hot_encode_workspace, request_pipeline, request_message, missing_values.data(),
          missing_values.size(),
          MutableByteBuffer{hot_encode_output.data(), hot_encode_output.size()});
      hot_encode_ok = hot_encode_ok && success.status == CodecStatus::OK &&
                      success.bytes_written == request->frame.size() &&
                      failure.status == CodecStatus::MISSING_FIELD && failure.bytes_written == 0U;
    }
    const std::size_t encode_allocations_after = g_allocation_count.load(std::memory_order_relaxed);
    runner.Record("hot_path_encode_success_failure_zero_replaceable_new_allocation",
                  hot_encode_ok && encode_allocations_before == encode_allocations_after,
                  "Encode hot path used replaceable new or returned unexpected metadata");
  } catch (const std::exception& exception) {
    runner.Record("unexpected_exception", false, exception.what());
  } catch (...) {
    runner.Record("unexpected_exception", false, "non-standard exception");
  }
  return runner.Finish();
}
