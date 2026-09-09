#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "complete_record_codec.h"
#include "config_compiler.h"

namespace {

using pae::config_compiler::CompileError;
using pae::config_compiler::CompileStage;
using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;

bool Expect(bool condition, std::string_view message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

std::string ReadText(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool ReplaceOnce(std::string& text, std::string_view from, std::string_view to) {
  const std::size_t position = text.find(from);
  if (position == std::string::npos) return false;
  text.replace(position, from.size(), to);
  return true;
}

bool CompileMutationFails(std::string text, std::string_view from, std::string_view to,
                          CompileStage stage, CompileError code, std::string_view pointer) {
  if (!ReplaceOnce(text, from, to)) return false;
  const auto result = pae::config_compiler::CompileJsonToPlan(text);
  const auto* diagnostic = result.Diagnostic();
  return !result.Succeeded() && diagnostic != nullptr && diagnostic->stage == stage &&
         diagnostic->code == code && diagnostic->json_pointer == pointer;
}

std::string LengthWithCrcConfig() {
  return R"json({
    "schema_version":"0.7","protocol_id":"length_crc","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:length_crc",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:length_crc","input_kind":"complete_record"}],
    "pipelines":[{"id":"pipe","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:length_crc","direction_id":"rx",
      "input_framing_profile_id":"record","message_ids":["message"]}],
    "messages":[{"id":"message","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:length_crc","direction_id":"rx",
      "frame_length_bytes":8,"matcher":{"all":[
        {"kind":"frame_length_equals","length_bytes":8},
        {"kind":"fixed_bytes","byte_offset":0,"bytes":"AA"},
        {"kind":"fixed_bytes","byte_offset":5,"bytes":"55"}]},
      "integrity":{"algorithm":"crc","parameters":{"width":16,"poly":"1021",
        "init":"FFFF","refin":false,"refout":false,"xorout":"0000"},
        "range":{"byte_offset":0,"byte_length":6},
        "storage":{"byte_offset":6,"byte_order":"big_endian"}},
      "fields":[
        {"id":"record_length","display_name":"Synthetic","description":"",
         "source_ref":"SYNTHETIC_FROM_SCRATCH:length_crc","value_type":"UINT64",
         "wire":{"codec":"unsigned_integer","byte_offset":1,"byte_width":2,
                  "byte_order":"big_endian"},"encode":{"source":"computed"},
         "computed":{"kind":"length","scope":"frame"}},
        {"id":"payload","display_name":"Synthetic","description":"",
         "source_ref":"SYNTHETIC_FROM_SCRATCH:length_crc","value_type":"BYTES",
         "wire":{"codec":"bytes","byte_offset":3,"byte_length":2},
         "encode":{"source":"input"}}]}]})json";
}

std::string OneByteFrameLengthConfig(std::size_t frame_length) {
  const std::string length = std::to_string(frame_length);
  const std::string payload_length = std::to_string(frame_length - 1U);
  return R"json({
    "schema_version":"0.7","protocol_id":"length_boundary","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:length_boundary",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:length_boundary","input_kind":"complete_record"}],
    "pipelines":[{"id":"pipe","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:length_boundary","direction_id":"rx",
      "input_framing_profile_id":"record","message_ids":["message"]}],
    "messages":[{"id":"message","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:length_boundary","direction_id":"rx",
      "frame_length_bytes":)json" +
         length + R"json(,"matcher":{"all":[
        {"kind":"frame_length_equals","length_bytes":)json" +
         length + R"json(}]},"fields":[
        {"id":"record_length","display_name":"Synthetic","description":"",
         "source_ref":"SYNTHETIC_FROM_SCRATCH:length_boundary","value_type":"UINT64",
         "wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},
         "encode":{"source":"computed"},"computed":{"kind":"length","scope":"frame"}},
        {"id":"payload","display_name":"Synthetic","description":"",
         "source_ref":"SYNTHETIC_FROM_SCRATCH:length_boundary","value_type":"BYTES",
         "wire":{"codec":"bytes","byte_offset":1,"byte_length":)json" +
         payload_length + R"json(},"encode":{"source":"input"}}]}]})json";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string config = ReadText(argv[1]);
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!compiled.Succeeded()) {
    const auto* diagnostic = compiled.Diagnostic();
    if (diagnostic != nullptr) {
      std::cerr << "compile diagnostic: stage=" << static_cast<int>(diagnostic->stage)
                << " code=" << static_cast<int>(diagnostic->code)
                << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail
                << '\n';
    }
  }
  if (!Expect(compiled.Succeeded(), "Schema 0.7 public length vector compiles")) return 1;
  auto plan = std::move(compiled).TakePlan();
  if (!Expect(plan->SchemaVersion() == "0.7", "Plan preserves Schema 0.7") ||
      !Expect(plan->GetResourceRequirements().total_computed_length_count == 3U,
              "computed length resource count is exact") ||
      !Expect(plan->Messages()[0].computed_length.has_value() &&
                  plan->MessageExecutionPlans()[0].computed_length.has_value(),
              "computed length descriptor is frozen in both Plan views") ||
      !Expect(plan->Messages()[0].computed_length->expected_value == 6U &&
                  plan->Messages()[1].computed_length->expected_value == 3U &&
                  plan->Messages()[2].computed_length->expected_value == 8U,
              "frame, region, and wide expected values are compiled independently"))
    return 1;

  auto boundary_255_compiled =
      pae::config_compiler::CompileJsonToPlan(OneByteFrameLengthConfig(255U));
  if (!Expect(boundary_255_compiled.Succeeded(), "one-byte computed frame length accepts 255"))
    return 1;
  auto boundary_255_plan = std::move(boundary_255_compiled).TakePlan();
  ExecutionWorkspace boundary_255_workspace(*boundary_255_plan);
  std::vector<std::uint8_t> boundary_payload(254U, 0xA5U);
  EncodeFieldValue boundary_value{};
  boundary_value.field = FieldRef{boundary_255_plan.get(), 0U, 1U};
  boundary_value.value_kind = LogicalValueKind::BYTES;
  boundary_value.bytes_value = ByteView{boundary_payload.data(), boundary_payload.size()};
  std::vector<std::uint8_t> boundary_frame(255U, 0x5AU);
  auto boundary_encoded = pae::protocol_core::EncodeCompleteRecord(
      *boundary_255_plan, boundary_255_workspace, 0U, 0U, &boundary_value, 1U,
      MutableByteBuffer{boundary_frame.data(), boundary_frame.size()});
  if (!Expect(boundary_encoded.status == CodecStatus::OK &&
                  boundary_encoded.bytes_written == 255U && boundary_frame[0] == 0xFFU &&
                  std::equal(boundary_payload.begin(), boundary_payload.end(),
                             boundary_frame.begin() + 1),
              "one-byte computed frame length encodes independent 255 boundary bytes"))
    return 1;
  std::array<DecodedFieldSlot, 2U> boundary_slots{};
  const auto boundary_decoded = pae::protocol_core::DecodeCompleteRecord(
      *boundary_255_plan, boundary_255_workspace, 0U,
      ByteView{boundary_frame.data(), boundary_frame.size()}, boundary_slots.data(),
      boundary_slots.size());
  if (!Expect(boundary_decoded.status == CodecStatus::OK && boundary_slots[0].uint64_value == 255U,
              "one-byte computed frame length decodes the 255 boundary"))
    return 1;

  const auto boundary_256_result =
      pae::config_compiler::CompileJsonToPlan(OneByteFrameLengthConfig(256U));
  if (!Expect(!boundary_256_result.Succeeded() && boundary_256_result.Diagnostic() != nullptr &&
                  boundary_256_result.Diagnostic()->stage == CompileStage::DOMAIN_VALIDATION &&
                  boundary_256_result.Diagnostic()->code == CompileError::VALUE_NOT_REPRESENTABLE &&
                  boundary_256_result.Diagnostic()->json_pointer == "/messages/0/fields/0/computed",
              "one-byte computed frame length rejects 256 at the computed rule"))
    return 1;

  ExecutionWorkspace workspace(*plan);
  const std::array<std::uint8_t, 1U> payload{0x7EU};
  std::array<EncodeFieldValue, 2U> values{};
  values[0].field = FieldRef{plan.get(), 0U, 1U};
  values[0].value_kind = LogicalValueKind::UINT64;
  values[0].uint64_value = 5U;
  values[1].field = FieldRef{plan.get(), 0U, 2U};
  values[1].value_kind = LogicalValueKind::BYTES;
  values[1].bytes_value = ByteView{payload.data(), payload.size()};
  std::array<std::uint8_t, 6U> output{};
  output.fill(0xCCU);
  auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{output.data(), output.size()});
  const std::array<std::uint8_t, 6U> expected{0xAAU, 0x00U, 0x06U, 0x05U, 0x7EU, 0x55U};
  auto counts = workspace.LastOperationCounts();
  if (!Expect(encoded.status == CodecStatus::OK && encoded.bytes_written == expected.size() &&
                  output == expected,
              "frame length Encode matches the independently calculated bytes") ||
      !Expect(counts.computed_length_fields_generated == 1U &&
                  counts.computed_length_fields_verified == 1U,
              "Encode generates and final-verifies the length exactly once"))
    return 1;

  std::reverse(values.begin(), values.end());
  output.fill(0xA5U);
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{output.data(), output.size()});
  if (!Expect(encoded.status == CodecStatus::OK && output == expected,
              "nonzero prefill and Values order do not affect computed output"))
    return 1;

  std::array<DecodedFieldSlot, 3U> slots{};
  auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  counts = workspace.LastOperationCounts();
  if (!Expect(decoded.status == CodecStatus::OK && decoded.field_count == slots.size(),
              "independent frame length bytes Decode") ||
      !Expect(slots[0].uint64_value == 6U && slots[1].uint64_value == 5U &&
                  slots[2].bytes_value.size == 1U && slots[2].bytes_value.data[0] == 0x7EU,
              "Decode returns the computed field and business fields exactly") ||
      !Expect(counts.computed_length_fields_verified == 1U,
              "Decode verifies the length exactly once"))
    return 1;

  auto bad_length = expected;
  bad_length[2] = 0x05U;
  decoded = pae::protocol_core::DecodeCompleteRecord(*plan, workspace, 0U,
                                                     ByteView{bad_length.data(), bad_length.size()},
                                                     slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::LENGTH_MISMATCH && decoded.field_count == 0U &&
                  decoded.failed_field_index == 0U,
              "length mismatch fails before field delivery and identifies the computed field"))
    return 1;

  EncodeFieldValue override_value{};
  override_value.field = FieldRef{plan.get(), 0U, 0U};
  override_value.value_kind = LogicalValueKind::UINT64;
  override_value.uint64_value = 6U;
  encoded =
      pae::protocol_core::EncodeCompleteRecord(*plan, workspace, 0U, 0U, &override_value, 1U,
                                               MutableByteBuffer{output.data(), output.size()});
  if (!Expect(encoded.status == CodecStatus::COMPUTED_FIELD_OVERRIDE &&
                  encoded.bytes_written == 0U && encoded.failed_value_index == 0U &&
                  encoded.failed_field_index == 0U,
              "caller cannot override a computed field even with the correct value"))
    return 1;

  const std::array<std::uint8_t, 3U> region_payload{0x01U, 0x02U, 0x03U};
  EncodeFieldValue region_value{};
  region_value.field = FieldRef{plan.get(), 1U, 1U};
  region_value.value_kind = LogicalValueKind::BYTES;
  region_value.bytes_value = ByteView{region_payload.data(), region_payload.size()};
  std::array<std::uint8_t, 6U> region_output{};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 1U, &region_value, 1U,
      MutableByteBuffer{region_output.data(), region_output.size()});
  const std::array<std::uint8_t, 6U> region_expected{0xABU, 0x03U, 0x01U, 0x02U, 0x03U, 0x55U};
  if (!Expect(encoded.status == CodecStatus::OK && region_output == region_expected,
              "region length Encode matches the independent three-byte expectation"))
    return 1;
  std::array<DecodedFieldSlot, 2U> region_slots{};
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{region_expected.data(), region_expected.size()},
      region_slots.data(), region_slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && region_slots[0].uint64_value == 3U,
              "region length Decode succeeds"))
    return 1;

  std::string self_counting = config;
  if (!Expect(ReplaceOnce(self_counting, "\"range\": {\"byte_offset\": 2, \"byte_length\": 3}",
                          "\"range\": {\"byte_offset\": 1, \"byte_length\": 4}"),
              "self-counting region mutation is available"))
    return 1;
  auto self_counting_compiled = pae::config_compiler::CompileJsonToPlan(self_counting);
  if (!Expect(self_counting_compiled.Succeeded(), "region may include its own storage")) return 1;
  auto self_counting_plan = std::move(self_counting_compiled).TakePlan();
  ExecutionWorkspace self_counting_workspace(*self_counting_plan);
  region_value.field = FieldRef{self_counting_plan.get(), 1U, 1U};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *self_counting_plan, self_counting_workspace, 0U, 1U, &region_value, 1U,
      MutableByteBuffer{region_output.data(), region_output.size()});
  if (!Expect(encoded.status == CodecStatus::OK && region_output[1] == 4U,
              "self-counting region writes the configured fixed byte count"))
    return 1;

  std::array<std::uint8_t, 6U> review_failure{};
  region_value.field = FieldRef{plan.get(), 1U, 1U};
  pae::protocol_core::test_only::CorruptComputedLengthBeforeFinalReviewOnce();
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 1U, &region_value, 1U,
      MutableByteBuffer{review_failure.data(), review_failure.size()});
  if (!Expect(encoded.status == CodecStatus::FINAL_REVIEW_FAILED && encoded.bytes_written == 0U &&
                  encoded.failed_field_index == 0U,
              "computed length final review failure exposes no valid output"))
    return 1;

  review_failure.fill(0x5AU);
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 1U, &region_value, 1U,
      MutableByteBuffer{review_failure.data(), review_failure.size()});
  if (!Expect(encoded.status == CodecStatus::OK && review_failure == region_expected,
              "a final review failure does not poison the next Encode"))
    return 1;

  if (!Expect(CompileMutationFails(config, "\"schema_version\": \"0.7\"",
                                   "\"schema_version\": \"0.6\"", CompileStage::STRUCTURAL,
                                   CompileError::UNKNOWN_PROPERTY, "/messages/0/fields/0/computed"),
              "Schema 0.6 rejects the computed length rule") ||
      !Expect(
          CompileMutationFails(config, "\"byte_length\": 3", "\"byte_length\": 0",
                               CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OUT_OF_BOUNDS,
                               "/messages/1/fields/0/computed/range/byte_length"),
          "empty computed region is rejected") ||
      !Expect(
          CompileMutationFails(config, "\"byte_offset\": 2, \"byte_length\": 3",
                               "\"byte_offset\": 18446744073709551615, \"byte_length\": 3",
                               CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OUT_OF_BOUNDS,
                               "/messages/1/fields/0/computed/range"),
          "computed region integer boundary is rejected without overflow") ||
      !Expect(CompileMutationFails(
                  config, "{\"kind\": \"fixed_bytes\", \"byte_offset\": 0, \"bytes\": \"AA\"}",
                  "{\"kind\": \"fixed_bytes\", \"byte_offset\": 1, \"bytes\": \"AA\"}",
                  CompileStage::DOMAIN_VALIDATION, CompileError::MATCHER_CONFLICT,
                  "/messages/0/fields/0/wire"),
              "computed storage cannot overlap a fixed_bytes Matcher"))
    return 1;

  std::string duplicate_computed = config;
  if (!Expect(ReplaceOnce(duplicate_computed, "\"encode\": {\"source\": \"input\"}",
                          "\"encode\": {\"source\": \"computed\"},\n"
                          "          \"computed\": {\"kind\": \"length\", \"scope\": \"frame\"}"),
              "duplicate computed length mutation is available"))
    return 1;
  const auto duplicate_result = pae::config_compiler::CompileJsonToPlan(duplicate_computed);
  if (!Expect(!duplicate_result.Succeeded() && duplicate_result.Diagnostic() != nullptr &&
                  duplicate_result.Diagnostic()->stage == CompileStage::DOMAIN_VALIDATION &&
                  duplicate_result.Diagnostic()->code == CompileError::UNSUPPORTED_FEATURE &&
                  duplicate_result.Diagnostic()->json_pointer == "/messages/0/fields/1/computed",
              "a second computed length field is rejected at its rule"))
    return 1;

  std::string little_endian = config;
  if (!Expect(ReplaceOnce(little_endian, "\"byte_order\": \"big_endian\"",
                          "\"byte_order\": \"little_endian\""),
              "little-endian mutation is available"))
    return 1;
  auto little_compiled = pae::config_compiler::CompileJsonToPlan(little_endian);
  if (!Expect(little_compiled.Succeeded(), "two-byte little-endian length compiles")) return 1;
  auto little_plan = std::move(little_compiled).TakePlan();
  ExecutionWorkspace little_workspace(*little_plan);
  values[0].field = FieldRef{little_plan.get(), 0U, 2U};
  values[1].field = FieldRef{little_plan.get(), 0U, 1U};
  std::array<std::uint8_t, 6U> little_output{};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *little_plan, little_workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{little_output.data(), little_output.size()});
  const std::array<std::uint8_t, 6U> little_expected{0xAAU, 0x06U, 0x00U, 0x05U, 0x7EU, 0x55U};
  if (!Expect(encoded.status == CodecStatus::OK && little_output == little_expected,
              "two-byte little-endian computed length matches independent bytes"))
    return 1;
  std::array<DecodedFieldSlot, 3U> little_slots{};
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *little_plan, little_workspace, 0U, ByteView{little_expected.data(), little_expected.size()},
      little_slots.data(), little_slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && little_slots[0].uint64_value == 6U &&
                  little_slots[1].uint64_value == 5U && little_slots[2].bytes_value.size == 1U &&
                  little_slots[2].bytes_value.data[0] == 0x7EU,
              "two-byte little-endian independent frame decodes exactly"))
    return 1;

  EncodeFieldValue wide_value{};
  wide_value.field = FieldRef{plan.get(), 2U, 1U};
  wide_value.value_kind = LogicalValueKind::UINT64;
  wide_value.uint64_value = 0x42U;
  std::array<std::uint8_t, 8U> wide_output{};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 2U, &wide_value, 1U,
      MutableByteBuffer{wide_output.data(), wide_output.size()});
  const std::array<std::uint8_t, 8U> wide_expected{0xACU, 0x08U, 0x00U, 0x00U,
                                                   0x00U, 0x42U, 0x55U, 0x66U};
  if (!Expect(encoded.status == CodecStatus::OK && wide_output == wide_expected,
              "four-byte little-endian frame length matches independent bytes"))
    return 1;
  std::array<DecodedFieldSlot, 2U> wide_slots{};
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{wide_expected.data(), wide_expected.size()}, wide_slots.data(),
      wide_slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && wide_slots[0].uint64_value == 8U &&
                  wide_slots[1].uint64_value == 0x42U,
              "four-byte computed length decodes as UINT64"))
    return 1;

  std::string wide_big_endian = config;
  if (!Expect(ReplaceOnce(wide_big_endian, "\"byte_order\": \"little_endian\"",
                          "\"byte_order\": \"big_endian\""),
              "four-byte big-endian mutation is available"))
    return 1;
  auto wide_big_compiled = pae::config_compiler::CompileJsonToPlan(wide_big_endian);
  if (!Expect(wide_big_compiled.Succeeded(), "four-byte big-endian length compiles")) return 1;
  auto wide_big_plan = std::move(wide_big_compiled).TakePlan();
  ExecutionWorkspace wide_big_workspace(*wide_big_plan);
  wide_value.field = FieldRef{wide_big_plan.get(), 2U, 1U};
  std::array<std::uint8_t, 8U> wide_big_output{};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *wide_big_plan, wide_big_workspace, 0U, 2U, &wide_value, 1U,
      MutableByteBuffer{wide_big_output.data(), wide_big_output.size()});
  const std::array<std::uint8_t, 8U> wide_big_expected{0xACU, 0x00U, 0x00U, 0x00U,
                                                       0x08U, 0x42U, 0x55U, 0x66U};
  if (!Expect(encoded.status == CodecStatus::OK && wide_big_output == wide_big_expected,
              "four-byte big-endian frame length matches independent bytes"))
    return 1;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *wide_big_plan, wide_big_workspace, 0U,
      ByteView{wide_big_expected.data(), wide_big_expected.size()}, wide_slots.data(),
      wide_slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && wide_slots[0].uint64_value == 8U &&
                  wide_slots[1].uint64_value == 0x42U,
              "four-byte big-endian independent frame decodes exactly"))
    return 1;

  auto length_crc_compiled = pae::config_compiler::CompileJsonToPlan(LengthWithCrcConfig());
  if (!Expect(length_crc_compiled.Succeeded(), "length plus CRC configuration compiles")) return 1;
  auto length_crc_plan = std::move(length_crc_compiled).TakePlan();
  ExecutionWorkspace length_crc_workspace(*length_crc_plan);
  const std::array<std::uint8_t, 2U> crc_payload{0x7EU, 0x7FU};
  EncodeFieldValue crc_value{};
  crc_value.field = FieldRef{length_crc_plan.get(), 0U, 1U};
  crc_value.value_kind = LogicalValueKind::BYTES;
  crc_value.bytes_value = ByteView{crc_payload.data(), crc_payload.size()};
  std::array<std::uint8_t, 8U> length_crc_output{};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *length_crc_plan, length_crc_workspace, 0U, 0U, &crc_value, 1U,
      MutableByteBuffer{length_crc_output.data(), length_crc_output.size()});
  const std::array<std::uint8_t, 8U> length_crc_expected{0xAAU, 0x00U, 0x08U, 0x7EU,
                                                         0x7FU, 0x55U, 0x3DU, 0xC7U};
  if (!Expect(encoded.status == CodecStatus::OK && length_crc_output == length_crc_expected,
              "length is written before independently calculated CRC"))
    return 1;
  std::array<DecodedFieldSlot, 2U> length_crc_slots{};
  auto bad_length_and_crc = length_crc_expected;
  bad_length_and_crc[2] = 0x07U;
  bad_length_and_crc[7] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *length_crc_plan, length_crc_workspace, 0U,
      ByteView{bad_length_and_crc.data(), bad_length_and_crc.size()}, length_crc_slots.data(),
      length_crc_slots.size());
  if (!Expect(decoded.status == CodecStatus::LENGTH_MISMATCH && decoded.field_count == 0U,
              "length mismatch precedes CRC failure without field delivery"))
    return 1;
  auto bad_crc = length_crc_expected;
  bad_crc[7] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *length_crc_plan, length_crc_workspace, 0U, ByteView{bad_crc.data(), bad_crc.size()},
      length_crc_slots.data(), length_crc_slots.size());
  if (!Expect(decoded.status == CodecStatus::INTEGRITY_FAILED && decoded.field_count == 0U,
              "CRC failure follows a successful length check"))
    return 1;

  std::cout << "PAE_LENGTH_FIELD_CONTRACT_PASS\n";
  return 0;
}
