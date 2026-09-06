#include <algorithm>
#include <cctype>
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
using pae::protocol_core::MutableByteBuffer;

std::string ReadText(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<std::uint8_t> ParseHex(std::string text) {
  std::vector<std::uint8_t> bytes;
  for (std::size_t index = 0U; index < text.size();) {
    while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index]))) ++index;
    if (index + 2U > text.size()) break;
    bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(index, 2U), nullptr, 16)));
    index += 2U;
  }
  return bytes;
}

bool Expect(bool condition, std::string_view message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

bool CompileMutationFails(std::string text, std::string_view from, std::string_view to,
                          CompileStage stage, CompileError code, std::string_view pointer) {
  const std::size_t position = text.find(from);
  if (position == std::string::npos) return false;
  text.replace(position, from.size(), to);
  const auto result = pae::config_compiler::CompileJsonToPlan(text);
  const auto* diagnostic = result.Diagnostic();
  return !result.Succeeded() && diagnostic != nullptr && diagnostic->stage == stage &&
         diagnostic->code == code && diagnostic->json_pointer == pointer;
}

std::vector<EncodeFieldValue> Values(const pae::protocol_plan::PlanBundle& plan,
                                     std::uint64_t a = 0xF0U, std::uint64_t b = 0x20U,
                                     std::uint64_t word = 0x0102U) {
  std::vector<EncodeFieldValue> values(3U);
  values[0].field = FieldRef{&plan, 0U, 0U};
  values[0].uint64_value = a;
  values[1].field = FieldRef{&plan, 0U, 1U};
  values[1].uint64_value = b;
  values[2].field = FieldRef{&plan, 0U, 2U};
  values[2].uint64_value = word;
  return values;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string root = argv[1];
  const std::string config = ReadText(root + "/synthetic_sum8_slice.pae.json");
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!Expect(compiled.Succeeded(), "Schema 0.3 public SUM8 vector compiles")) return 1;
  auto plan = std::move(compiled).TakePlan();
  const auto expected = ParseHex(ReadText(root + "/sum8_record_001.frame.hex"));
  if (!Expect(plan->SchemaVersion() == "0.3", "Plan preserves Schema 0.3") ||
      !Expect(plan->Messages()[0].integrity.has_value(), "Frozen Message contains integrity") ||
      !Expect(plan->MessageExecutionPlans()[0].integrity.has_value(),
              "execution descriptor contains integrity") ||
      !Expect(plan->GetResourceRequirements().total_integrity_rule_count == 1U,
              "integrity resource count is exact"))
    return 1;

  auto values = Values(*plan);
  ExecutionWorkspace workspace(*plan);
  std::vector<std::uint8_t> output(expected.size(), 0xCCU);
  auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{output.data(), output.size()});
  const auto encode_counts = workspace.LastOperationCounts();
  if (!Expect(encoded.status == CodecStatus::OK && encoded.bytes_written == expected.size(),
              "SUM8 Encode succeeds") ||
      !Expect(output == expected, "Encode matches independently calculated frame") ||
      !Expect(encode_counts.integrity_bytes_accumulated == 4U &&
                  encode_counts.integrity_bytes_verified == 4U,
              "Encode SUM8 operation bound is 2N"))
    return 1;

  std::reverse(values.begin(), values.end());
  std::vector<std::uint8_t> reordered(expected.size(), 0x7EU);
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{reordered.data(), reordered.size()});
  if (!Expect(encoded.status == CodecStatus::OK && reordered == expected,
              "nonzero prefill and Values order do not affect output"))
    return 1;

  std::vector<std::uint8_t> review_failure(expected.size(), 0xA5U);
  pae::protocol_core::test_only::CorruptIntegrityStorageBeforeFinalReviewOnce();
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{review_failure.data(), review_failure.size()});
  if (!Expect(encoded.status == CodecStatus::FINAL_REVIEW_FAILED && encoded.bytes_written == 0U,
              "SUM8 final review fault injection fails with no valid output"))
    return 1;

  auto edge_values = Values(*plan, 0U, 0xFFU, 0xFFFFU);
  std::vector<std::uint8_t> edge_output(expected.size(), 0U);
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, edge_values.data(), edge_values.size(),
      MutableByteBuffer{edge_output.data(), edge_output.size()});
  const std::vector<std::uint8_t> edge_expected{0x70U, 0x00U, 0xFFU, 0xFFU, 0xFFU, 0xFDU, 0x7EU};
  if (!Expect(encoded.status == CodecStatus::OK && edge_output == edge_expected,
              "zero, maximum bytes, and modulo-256 overflow are exact"))
    return 1;

  std::vector<DecodedFieldSlot> slots(3U);
  auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  const auto decode_counts = workspace.LastOperationCounts();
  if (!Expect(decoded.status == CodecStatus::OK && decoded.field_count == 3U,
              "independent SUM8 frame decodes") ||
      !Expect(slots[0].uint64_value == 0xF0U && slots[1].uint64_value == 0x20U &&
                  slots[2].uint64_value == 0x0102U,
              "decoded business values are exact") ||
      !Expect(decode_counts.integrity_bytes_verified == 4U &&
                  decode_counts.integrity_bytes_accumulated == 0U,
              "Decode SUM8 operation bound is N"))
    return 1;

  auto corrupted = expected;
  corrupted[1] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(*plan, workspace, 0U,
                                                     ByteView{corrupted.data(), corrupted.size()},
                                                     slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::INTEGRITY_FAILED && decoded.field_count == 0U,
              "covered single-bit corruption fails without delivered fields"))
    return 1;
  corrupted = expected;
  corrupted[5] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(*plan, workspace, 0U,
                                                     ByteView{corrupted.data(), corrupted.size()},
                                                     slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::INTEGRITY_FAILED && decoded.field_count == 0U,
              "stored checksum corruption fails"))
    return 1;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{corrupted.data(), corrupted.size()}, nullptr, 0U);
  if (!Expect(decoded.status == CodecStatus::OUTPUT_SLOTS_TOO_SMALL,
              "output capacity check precedes integrity verification"))
    return 1;

  std::string enum_config = config;
  const std::string enum_from =
      "\"id\": \"payload_b\",\n          \"display_name\": \"载荷B\",\n          "
      "\"description\": \"单字节动态值。\",\n          "
      "\"source_ref\": \"SYNTHETIC_FROM_SCRATCH:dec041_public_vector#payload_b\",\n"
      "          \"value_type\": \"UINT64\",\n          "
      "\"wire\": {\"codec\": \"unsigned_integer\", \"byte_offset\": 2, \"byte_width\": 1},\n"
      "          \"encode\": {\"source\": \"input\"}";
  const std::string enum_to =
      "\"id\": \"payload_b\",\n          \"display_name\": \"载荷B\",\n          "
      "\"description\": \"单字节动态值。\",\n          "
      "\"source_ref\": \"SYNTHETIC_FROM_SCRATCH:dec041_public_vector#payload_b\",\n"
      "          \"value_type\": \"ENUM\",\n          "
      "\"wire\": {\"codec\": \"unsigned_integer\", \"byte_offset\": 2, \"byte_width\": 1},\n"
      "          \"encode\": {\"source\": \"input\"},\n          "
      "\"unknown_enum_policy\": \"reject\",\n          "
      "\"enum_entries\": [{\"id\": \"known\", \"display_name\": \"已知值\", "
      "\"raw_value\": 1}]";
  const std::size_t enum_position = enum_config.find(enum_from);
  if (!Expect(enum_position != std::string::npos, "enum semantic test mutation is available"))
    return 1;
  enum_config.replace(enum_position, enum_from.size(), enum_to);
  auto enum_compiled = pae::config_compiler::CompileJsonToPlan(enum_config);
  if (!Expect(enum_compiled.Succeeded(), "SUM8 unknown-enum semantic Plan compiles")) return 1;
  auto enum_plan = std::move(enum_compiled).TakePlan();
  ExecutionWorkspace enum_workspace(*enum_plan);
  decoded = pae::protocol_core::DecodeCompleteRecord(*enum_plan, enum_workspace, 0U,
                                                     ByteView{expected.data(), expected.size()},
                                                     slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::UNKNOWN_ENUM_VALUE && decoded.field_count == 0U,
              "valid SUM8 does not hide later field semantic failure"))
    return 1;

  auto compensated = expected;
  ++compensated[1];
  --compensated[2];
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{compensated.data(), compensated.size()}, slots.data(),
      slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && slots[0].uint64_value == 0xF1U &&
                  slots[1].uint64_value == 0x1FU,
              "compensating byte changes demonstrate SUM8 detection limit"))
    return 1;

  auto outside = expected;
  outside[0] ^= 1U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{outside.data(), outside.size()}, slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::UNKNOWN_MESSAGE,
              "non-covered Matcher byte is governed by structural matching"))
    return 1;

  const std::string integrity =
      "\"integrity\": {\n        \"algorithm\": \"sum8\",\n        \"range\": {\"byte_offset\": 1, "
      "\"byte_length\": 4},\n        \"storage\": {\"byte_offset\": 5}\n      }";
  if (!Expect(CompileMutationFails(config, "\"schema_version\": \"0.3\"",
                                   "\"schema_version\": \"0.2\"", CompileStage::STRUCTURAL,
                                   CompileError::UNKNOWN_PROPERTY, "/messages/0/integrity"),
              "Schema 0.2 rejects integrity") ||
      !Expect(
          CompileMutationFails(config, integrity, "\"integrity\": null", CompileStage::STRUCTURAL,
                               CompileError::TYPE_MISMATCH, "/messages/0/integrity"),
          "integrity null is rejected") ||
      !Expect(CompileMutationFails(config, "\"algorithm\": \"sum8\"", "\"algorithm\": \"crc\"",
                                   CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                                   "/messages/0/integrity/algorithm"),
              "unknown algorithm is rejected") ||
      !Expect(
          CompileMutationFails(config, "\"algorithm\": \"sum8\"",
                               "\"algorithm\": \"sum8\", \"width\": 1", CompileStage::STRUCTURAL,
                               CompileError::UNKNOWN_PROPERTY, "/messages/0/integrity/width"),
          "unknown integrity property is rejected") ||
      !Expect(CompileMutationFails(config, "\"byte_length\": 4", "\"byte_length\": 0",
                                   CompileStage::DOMAIN_VALIDATION,
                                   CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS,
                                   "/messages/0/integrity/range/byte_length"),
              "empty range is rejected") ||
      !Expect(CompileMutationFails(config, "\"byte_offset\": 1, \"byte_length\": 4",
                                   "\"byte_offset\": 18446744073709551615, \"byte_length\": 2",
                                   CompileStage::DOMAIN_VALIDATION,
                                   CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS,
                                   "/messages/0/integrity/range"),
              "range integer boundary is rejected without overflow") ||
      !Expect(CompileMutationFails(
                  config, "\"storage\": {\"byte_offset\": 5}", "\"storage\": {\"byte_offset\": 7}",
                  CompileStage::DOMAIN_VALIDATION, CompileError::INTEGRITY_STORAGE_OUT_OF_BOUNDS,
                  "/messages/0/integrity/storage/byte_offset"),
              "out-of-frame storage is rejected") ||
      !Expect(CompileMutationFails(
                  config, "\"storage\": {\"byte_offset\": 5}", "\"storage\": {\"byte_offset\": 2}",
                  CompileStage::DOMAIN_VALIDATION, CompileError::INTEGRITY_SELF_INCLUDED,
                  "/messages/0/integrity/storage/byte_offset"),
              "self-included storage is rejected") ||
      !Expect(CompileMutationFails(
                  config, "\"storage\": {\"byte_offset\": 5}", "\"storage\": {\"byte_offset\": 6}",
                  CompileStage::DOMAIN_VALIDATION, CompileError::INTEGRITY_STORAGE_CONFLICT,
                  "/messages/0/integrity/storage/byte_offset"),
              "storage and fixed matcher overlap is rejected") ||
      !Expect(CompileMutationFails(config,
                                   "\"range\": {\"byte_offset\": 1, \"byte_length\": 4},\n        "
                                   "\"storage\": {\"byte_offset\": 5}",
                                   "\"range\": {\"byte_offset\": 2, \"byte_length\": 3},\n        "
                                   "\"storage\": {\"byte_offset\": 1}",
                                   CompileStage::DOMAIN_VALIDATION,
                                   CompileError::INTEGRITY_STORAGE_CONFLICT,
                                   "/messages/0/integrity/storage/byte_offset"),
              "storage and ordinary field overlap is rejected") ||
      !Expect(CompileMutationFails(config,
                                   "\"range\": {\"byte_offset\": 1, \"byte_length\": 4},\n        "
                                   "\"storage\": {\"byte_offset\": 5}",
                                   "\"range\": {\"byte_offset\": 1, \"byte_length\": 2},\n        "
                                   "\"storage\": {\"byte_offset\": 3}",
                                   CompileStage::DOMAIN_VALIDATION,
                                   CompileError::INTEGRITY_STORAGE_CONFLICT,
                                   "/messages/0/integrity/storage/byte_offset"),
              "storage and bit container overlap is rejected") ||
      !Expect(CompileMutationFails(
                  config, "\"storage\": {\"byte_offset\": 5}", "\"storage\": {\"byte_offset\": 1}",
                  CompileStage::DOMAIN_VALIDATION, CompileError::INTEGRITY_SELF_INCLUDED,
                  "/messages/0/integrity/storage/byte_offset"),
              "storage inside range takes self-inclusion precedence"))
    return 1;

  std::cout << "PAE_SUM8_CONTRACT_PASS\n";
  return 0;
}
