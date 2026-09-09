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

struct Vector {
  std::uint8_t width;
  std::string_view poly;
  std::string_view init;
  bool refin;
  bool refout;
  std::string_view xorout;
  std::uint32_t check;
  std::string_view big_endian_frame;
  std::string_view little_endian_frame;
};

std::uint8_t HexNibble(char value) {
  if (value >= '0' && value <= '9') return static_cast<std::uint8_t>(value - '0');
  return static_cast<std::uint8_t>(value - 'A' + 10);
}

std::vector<std::uint8_t> DecodeHex(std::string_view text) {
  std::vector<std::uint8_t> output;
  output.reserve(text.size() / 2U);
  for (std::size_t index = 0U; index < text.size(); index += 2U) {
    output.push_back(
        static_cast<std::uint8_t>((HexNibble(text[index]) << 4U) | HexNibble(text[index + 1U])));
  }
  return output;
}

bool CheckVectorStorageOrder(const std::string& base, const Vector& vector, bool little_endian) {
  std::string config = base;
  if (!ReplaceOnce(config, "\"width\": 16",
                   std::string{"\"width\": "} + std::to_string(vector.width)) ||
      !ReplaceOnce(config, "\"poly\": \"1021\"",
                   std::string{"\"poly\": \""} + std::string{vector.poly} + "\"") ||
      !ReplaceOnce(config, "\"init\": \"FFFF\"",
                   std::string{"\"init\": \""} + std::string{vector.init} + "\"") ||
      !ReplaceOnce(config, "\"refin\": false",
                   std::string{"\"refin\": "} + (vector.refin ? "true" : "false")) ||
      !ReplaceOnce(config, "\"refout\": false",
                   std::string{"\"refout\": "} + (vector.refout ? "true" : "false")) ||
      !ReplaceOnce(config, "\"xorout\": \"0000\"",
                   std::string{"\"xorout\": \""} + std::string{vector.xorout} + "\"")) {
    return false;
  }
  if (little_endian &&
      !ReplaceOnce(config, "\"byte_order\": \"big_endian\"", "\"byte_order\": \"little_endian\"")) {
    return false;
  }
  std::size_t output_size = 12U;
  if (vector.width == 32U) {
    output_size = 14U;
    if (!ReplaceOnce(config, "\"frame_length_bytes\": 12", "\"frame_length_bytes\": 14") ||
        !ReplaceOnce(config, "\"length_bytes\": 12", "\"length_bytes\": 14") ||
        !ReplaceOnce(config, "\"byte_offset\": 11", "\"byte_offset\": 13")) {
      return false;
    }
  }
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!compiled.Succeeded()) return false;
  auto plan = std::move(compiled).TakePlan();
  ExecutionWorkspace workspace(*plan);
  const std::array<std::uint8_t, 9U> payload{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  EncodeFieldValue value;
  value.field = FieldRef{plan.get(), 0U, 0U};
  value.value_kind = LogicalValueKind::BYTES;
  value.bytes_value = ByteView{payload.data(), payload.size()};
  std::vector<std::uint8_t> output(output_size, 0U);
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &value, 1U, MutableByteBuffer{output.data(), output.size()});
  const std::vector<std::uint8_t> expected =
      DecodeHex(little_endian ? vector.little_endian_frame : vector.big_endian_frame);
  if (encoded.status != CodecStatus::OK || encoded.bytes_written != output.size() ||
      output != expected)
    return false;

  std::array<DecodedFieldSlot, 1U> slots{};
  auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  if (decoded.status != CodecStatus::OK || decoded.field_count != 1U) return false;

  auto damaged = expected;
  damaged[0] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{damaged.data(), damaged.size()}, slots.data(), slots.size());
  if (decoded.status != CodecStatus::INTEGRITY_FAILED || decoded.field_count != 0U) return false;
  const std::size_t storage_width = vector.width / 8U;
  for (std::size_t index = 0U; index < storage_width; ++index) {
    damaged = expected;
    damaged[9U + index] ^= 0x01U;
    decoded = pae::protocol_core::DecodeCompleteRecord(
        *plan, workspace, 0U, ByteView{damaged.data(), damaged.size()}, slots.data(), slots.size());
    if (decoded.status != CodecStatus::INTEGRITY_FAILED || decoded.field_count != 0U) return false;
  }
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  return decoded.status == CodecStatus::OK && decoded.field_count == 1U;
}

bool CheckVector(const std::string& base, const Vector& vector) {
  return CheckVectorStorageOrder(base, vector, false) &&
         CheckVectorStorageOrder(base, vector, true);
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
  if (!Expect(compiled.Succeeded(), "Schema 0.6 public CRC vector compiles")) return 1;
  auto plan = std::move(compiled).TakePlan();
  if (!Expect(plan->SchemaVersion() == "0.6", "Plan preserves Schema 0.6") ||
      !Expect(plan->Messages()[0].integrity.has_value(), "Frozen CRC descriptor exists") ||
      !Expect(plan->Messages()[0].integrity->crc_width == 16U &&
                  plan->Messages()[0].integrity->crc_polynomial == 0x1021U,
              "CRC parameters are frozen exactly") ||
      !Expect(plan->GetResourceRequirements().total_integrity_rule_count == 1U,
              "CRC uses the bounded per-message integrity count"))
    return 1;

  const std::array<Vector, 6U> vectors{{
      {16U, "1021", "FFFF", false, false, "0000", 0x29B1U, "31323334353637383929B1AA",
       "313233343536373839B129AA"},
      {16U, "8005", "FFFF", true, true, "0000", 0x4B37U, "3132333435363738394B37AA",
       "313233343536373839374BAA"},
      {32U, "04C11DB7", "FFFFFFFF", true, true, "FFFFFFFF", 0xCBF43926U,
       "313233343536373839CBF43926AA", "3132333435363738392639F4CBAA"},
      {32U, "04C11DB7", "FFFFFFFF", false, false, "00000000", 0x0376E6E7U,
       "3132333435363738390376E6E7AA", "313233343536373839E7E67603AA"},
      {16U, "1021", "1D0F", false, true, "BEEF", 0x8D48U, "3132333435363738398D48AA",
       "313233343536373839488DAA"},
      {16U, "1021", "1D0F", true, false, "BEEF", 0xFB64U, "313233343536373839FB64AA",
       "31323334353637383964FBAA"},
  }};
  for (const Vector& vector : vectors) {
    if (!Expect(CheckVector(config, vector),
                "CRC fixed full-frame Encode and Decode match in both storage orders"))
      return 1;
  }

  const std::array<std::uint8_t, 9U> payload{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  std::string short_range_config = config;
  if (!Expect(ReplaceOnce(short_range_config, "\"byte_length\": 9", "\"byte_length\": 1"),
              "short CRC range mutation is available"))
    return 1;
  auto short_range_compiled = pae::config_compiler::CompileJsonToPlan(short_range_config);
  if (!Expect(short_range_compiled.Succeeded(), "one-byte CRC range compiles")) return 1;
  auto short_range_plan = std::move(short_range_compiled).TakePlan();
  ExecutionWorkspace short_range_workspace(*short_range_plan);
  EncodeFieldValue short_range_value;
  short_range_value.field = FieldRef{short_range_plan.get(), 0U, 0U};
  short_range_value.value_kind = LogicalValueKind::BYTES;
  short_range_value.bytes_value = ByteView{payload.data(), payload.size()};
  std::array<std::uint8_t, 12U> short_range_frame{};
  auto short_range_encoded = pae::protocol_core::EncodeCompleteRecord(
      *short_range_plan, short_range_workspace, 0U, 0U, &short_range_value, 1U,
      MutableByteBuffer{short_range_frame.data(), short_range_frame.size()});
  if (!Expect(short_range_encoded.status == CodecStatus::OK && short_range_frame[9] == 0xC7U &&
                  short_range_frame[10] == 0x82U,
              "one-byte CRC range matches independent C782 expectation"))
    return 1;
  short_range_frame[1] = 'X';
  std::array<DecodedFieldSlot, 1U> short_range_slots{};
  auto short_range_decoded = pae::protocol_core::DecodeCompleteRecord(
      *short_range_plan, short_range_workspace, 0U,
      ByteView{short_range_frame.data(), short_range_frame.size()}, short_range_slots.data(),
      short_range_slots.size());
  if (!Expect(short_range_decoded.status == CodecStatus::OK &&
                  short_range_decoded.field_count == 1U &&
                  short_range_slots[0].bytes_value.data[1] == 'X',
              "dynamic bytes outside the CRC range remain valid and are delivered"))
    return 1;

  EncodeFieldValue value;
  value.field = FieldRef{plan.get(), 0U, 0U};
  value.value_kind = LogicalValueKind::BYTES;
  value.bytes_value = ByteView{payload.data(), payload.size()};
  ExecutionWorkspace workspace(*plan);
  std::array<std::uint8_t, 12U> output;
  output.fill(0xCCU);
  auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &value, 1U, MutableByteBuffer{output.data(), output.size()});
  const std::array<std::uint8_t, 12U> expected{'1', '2', '3', '4',   '5',   '6',
                                               '7', '8', '9', 0x29U, 0xB1U, 0xAAU};
  const auto encode_counts = workspace.LastOperationCounts();
  if (!Expect(encoded.status == CodecStatus::OK && output == expected,
              "CRC Encode matches the independent published check value") ||
      !Expect(encode_counts.integrity_bytes_accumulated == 9U &&
                  encode_counts.integrity_bytes_verified == 9U,
              "CRC Encode operation accounting is 2N"))
    return 1;

  std::string little_config = config;
  if (!Expect(ReplaceOnce(little_config, "\"byte_order\": \"big_endian\"",
                          "\"byte_order\": \"little_endian\""),
              "little-endian storage mutation is available"))
    return 1;
  auto little_compiled = pae::config_compiler::CompileJsonToPlan(little_config);
  if (!Expect(little_compiled.Succeeded(), "little-endian CRC storage compiles")) return 1;
  auto little_plan = std::move(little_compiled).TakePlan();
  ExecutionWorkspace little_workspace(*little_plan);
  value.field = FieldRef{little_plan.get(), 0U, 0U};
  std::array<std::uint8_t, 12U> little_output{};
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *little_plan, little_workspace, 0U, 0U, &value, 1U,
      MutableByteBuffer{little_output.data(), little_output.size()});
  if (!Expect(encoded.status == CodecStatus::OK && little_output[9] == 0xB1U &&
                  little_output[10] == 0x29U,
              "CRC storage byte order is independent from reflection"))
    return 1;
  value.field = FieldRef{plan.get(), 0U, 0U};

  std::array<DecodedFieldSlot, 1U> slots{};
  auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && decoded.field_count == 1U,
              "published CRC frame decodes") ||
      !Expect(slots[0].bytes_value.size == payload.size(), "decoded payload length is exact"))
    return 1;

  auto damaged = expected;
  damaged[0] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{damaged.data(), damaged.size()}, slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::INTEGRITY_FAILED && decoded.field_count == 0U,
              "covered corruption fails without field delivery"))
    return 1;
  damaged = expected;
  damaged[10] ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{damaged.data(), damaged.size()}, slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::INTEGRITY_FAILED && decoded.field_count == 0U,
              "CRC storage corruption fails without field delivery"))
    return 1;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{expected.data(), expected.size()}, slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::OK && decoded.field_count == 1U,
              "successful Decode after CRC failure has no stale failure state"))
    return 1;

  std::array<std::uint8_t, 12U> review_failure{};
  pae::protocol_core::test_only::CorruptIntegrityStorageBeforeFinalReviewOnce();
  encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &value, 1U,
      MutableByteBuffer{review_failure.data(), review_failure.size()});
  if (!Expect(encoded.status == CodecStatus::FINAL_REVIEW_FAILED && encoded.bytes_written == 0U,
              "CRC final review failure exposes no valid output"))
    return 1;

  if (!Expect(
          CompileMutationFails(config, "\"schema_version\": \"0.6\"", "\"schema_version\": \"0.5\"",
                               CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                               "/messages/0/integrity/parameters"),
          "Schema 0.5 rejects CRC properties") ||
      !Expect(CompileMutationFails(config, "\"width\": 16", "\"width\": 24",
                                   CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                                   "/messages/0/integrity/parameters/width"),
              "unsupported CRC width is rejected") ||
      !Expect(
          CompileMutationFails(config, "\"poly\": \"1021\"", "\"poly\": \"1020\"",
                               CompileStage::DOMAIN_VALIDATION, CompileError::INVALID_ENUM_VALUE,
                               "/messages/0/integrity/parameters/poly"),
          "even CRC polynomial is rejected") ||
      !Expect(CompileMutationFails(config, "\"poly\": \"1021\"", "\"poly\": \"102a\"",
                                   CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                                   "/messages/0/integrity/parameters/poly"),
              "lowercase CRC hexadecimal is rejected") ||
      !Expect(CompileMutationFails(
                  config, "\"storage\": {\"byte_offset\": 9", "\"storage\": {\"byte_offset\": 8",
                  CompileStage::DOMAIN_VALIDATION, CompileError::INTEGRITY_SELF_INCLUDED,
                  "/messages/0/integrity/storage/byte_offset"),
              "full CRC storage interval cannot overlap its range"))
    return 1;

  std::cout << "PAE_CRC_CONTRACT_PASS\n";
  return 0;
}
