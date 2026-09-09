#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

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

std::string ReadText(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool ReplaceOnce(std::string& text, std::string_view from, std::string_view to) {
  const std::size_t position = text.find(from);
  if (position == std::string::npos) return false;
  text.replace(position, from.size(), to);
  return true;
}

bool ExpectCompileFailure(const std::string& source, std::string_view from, std::string_view to,
                          CompileStage stage, CompileError code, std::string_view pointer,
                          std::string_view name) {
  std::string mutated = source;
  if (!ReplaceOnce(mutated, from, to)) return Expect(false, std::string{name} + " mutation exists");
  const auto result = pae::config_compiler::CompileJsonToPlan(mutated);
  const auto* diagnostic = result.Diagnostic();
  if (result.Succeeded() || diagnostic == nullptr || diagnostic->stage != stage ||
      diagnostic->code != code || diagnostic->json_pointer != pointer) {
    if (diagnostic != nullptr) {
      std::cerr << name << " actual stage=" << static_cast<int>(diagnostic->stage)
                << " code=" << static_cast<int>(diagnostic->code)
                << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail
                << '\n';
    }
    return Expect(false, name);
  }
  return true;
}

bool ExpectLateBitContainerRejected(const std::string& source) {
  std::string mutated = source;
  const std::string container =
      "\"bit_containers\": [{\"id\": \"late_bits\", \"container_offset\": 5, "
      "\"container_width\": 1, \"bit_numbering\": \"lsb0\", \"base_value\": 0}],\n"
      "      \"matcher\": {";
  const std::string member =
      "\"fields\": [{\"id\": \"late_flag\", \"display_name\": \"Late flag\", "
      "\"description\": \"Synthetic invalid late bit\", "
      "\"source_ref\": \"SYNTHETIC_FROM_SCRATCH:late_bit\", \"value_type\": \"BOOL\", "
      "\"wire\": {\"codec\": \"bitfield\", \"container_id\": \"late_bits\", "
      "\"bit_offset\": 0, \"bit_width\": 1}, \"encode\": {\"source\": \"input\"}},";
  if (!ReplaceOnce(mutated, "\"matcher\": {", container) ||
      !ReplaceOnce(mutated, "\"fields\": [", member)) {
    return Expect(false, "late bit-container mutation anchors exist");
  }
  const auto result = pae::config_compiler::CompileJsonToPlan(mutated);
  const auto* diagnostic = result.Diagnostic();
  return Expect(!result.Succeeded() && diagnostic != nullptr &&
                    diagnostic->stage == CompileStage::DOMAIN_VALIDATION &&
                    diagnostic->code == CompileError::FIELD_OUT_OF_BOUNDS &&
                    diagnostic->json_pointer == "/messages/0/bit_containers/0",
                "late bit container is rejected by Domain Validator with an exact location");
}

bool ExpectMissingComputedLengthRejected(const std::string& source) {
  std::string mutated = source;
  const std::size_t length_id = mutated.find("\"id\": \"record_length\"");
  const std::size_t payload_id = mutated.find("\"id\": \"payload\"", length_id);
  const std::size_t length_begin = mutated.rfind('{', length_id);
  const std::size_t payload_begin = mutated.rfind('{', payload_id);
  if (length_id == std::string::npos || payload_id == std::string::npos ||
      length_begin == std::string::npos || payload_begin == std::string::npos ||
      length_begin >= payload_begin) {
    return Expect(false, "missing computed-length mutation anchors exist");
  }
  mutated.erase(length_begin, payload_begin - length_begin);
  const auto result = pae::config_compiler::CompileJsonToPlan(mutated);
  const auto* diagnostic = result.Diagnostic();
  return Expect(!result.Succeeded() && diagnostic != nullptr &&
                    diagnostic->stage == CompileStage::DOMAIN_VALIDATION &&
                    diagnostic->code == CompileError::UNSUPPORTED_FEATURE &&
                    diagnostic->json_pointer == "/messages/0/layout",
                "bounded Message without computed length is rejected at its layout");
}

bool ExpectSecondDynamicBytesRejected(const std::string& source) {
  std::string mutated = source;
  const std::string field =
      "\"fields\": [{\"id\": \"header_bytes\", \"display_name\": \"Header bytes\", "
      "\"description\": \"Synthetic invalid zero-width header bytes\", "
      "\"source_ref\": \"SYNTHETIC_FROM_SCRATCH:header_bytes\", \"value_type\": \"BYTES\", "
      "\"wire\": {\"codec\": \"bytes\", \"byte_offset\": 0}, "
      "\"encode\": {\"source\": \"input\"}},";
  if (!ReplaceOnce(mutated, "\"fields\": [", field)) {
    return Expect(false, "second dynamic BYTES mutation anchor exists");
  }
  const auto result = pae::config_compiler::CompileJsonToPlan(mutated);
  const auto* diagnostic = result.Diagnostic();
  return Expect(!result.Succeeded() && diagnostic != nullptr &&
                    diagnostic->stage == CompileStage::DOMAIN_VALIDATION &&
                    diagnostic->code == CompileError::UNSUPPORTED_FEATURE &&
                    diagnostic->json_pointer == "/messages/0/fields/0/wire",
                "only the selected payload BYTES may omit byte_length");
}

bool ExpectUndefinedHeaderRejected(const std::string& source) {
  std::string mutated = source;
  if (!ReplaceOnce(mutated, "\"header_length_bytes\": 2", "\"header_length_bytes\": 3") ||
      !ReplaceOnce(mutated, "\"wire\": {\"codec\": \"bytes\", \"byte_offset\": 2}",
                   "\"wire\": {\"codec\": \"bytes\", \"byte_offset\": 3}")) {
    return Expect(false, "undefined header mutation anchors exist");
  }
  const auto result = pae::config_compiler::CompileJsonToPlan(mutated);
  const auto* diagnostic = result.Diagnostic();
  return Expect(!result.Succeeded() && diagnostic != nullptr &&
                    diagnostic->stage == CompileStage::DOMAIN_VALIDATION &&
                    diagnostic->code == CompileError::FRAME_NOT_FULLY_DEFINED &&
                    diagnostic->json_pointer == "/messages/0",
                "bounded Message with an undefined header byte is rejected by Domain Validator");
}

bool EncodeMutation(std::string config, std::string_view from, std::string_view to,
                    ByteView payload, std::uint64_t expected_length, ByteView expected) {
  if (!ReplaceOnce(config, from, to)) return false;
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!compiled.Succeeded()) return false;
  auto plan = std::move(compiled).TakePlan();
  ExecutionWorkspace workspace(*plan);
  EncodeFieldValue value{};
  value.field = FieldRef{plan.get(), 0U, 1U};
  value.value_kind = LogicalValueKind::BYTES;
  value.bytes_value = payload;
  std::array<std::uint8_t, 10U> output{};
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &value, 1U, MutableByteBuffer{output.data(), output.size()});
  std::array<DecodedFieldSlot, 2U> slots{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{output.data(), encoded.bytes_written}, slots.data(),
      slots.size());
  return encoded.status == CodecStatus::OK && encoded.bytes_written == expected.size &&
         std::equal(expected.data, expected.data + expected.size, output.data()) &&
         decoded.status == CodecStatus::OK && slots[0].uint64_value == expected_length;
}

bool EncodeAndDecode(const pae::protocol_plan::PlanBundle& plan, ExecutionWorkspace& workspace,
                     ByteView payload, ByteView expected) {
  EncodeFieldValue value{};
  value.field = FieldRef{&plan, 0U, 1U};
  value.value_kind = LogicalValueKind::BYTES;
  value.bytes_value = payload;
  std::array<std::uint8_t, 6U> output{};
  output.fill(0xCCU);
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      plan, workspace, 0U, 0U, &value, 1U, MutableByteBuffer{output.data(), expected.size});
  const auto encode_counts = workspace.LastOperationCounts();
  if (!Expect(encoded.status == CodecStatus::OK && encoded.bytes_written == expected.size &&
                  encoded.required_size == expected.size &&
                  std::equal(expected.data, expected.data + expected.size, output.data()),
              "Encode matches independent bounded vector and actual size")) {
    return false;
  }
  std::array<std::uint8_t, 6U> differently_prefilled{};
  differently_prefilled.fill(0x5AU);
  const auto repeated = pae::protocol_core::EncodeCompleteRecord(
      plan, workspace, 0U, 0U, &value, 1U,
      MutableByteBuffer{differently_prefilled.data(), expected.size});
  if (!Expect(repeated.status == CodecStatus::OK && repeated.bytes_written == expected.size &&
                  std::equal(output.begin(), output.begin() + expected.size,
                             differently_prefilled.begin()),
              "valid bounded Encode is independent of output buffer prefill")) {
    return false;
  }
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  if (!Expect(encode_counts.computed_length_fields_generated == 1U &&
                  encode_counts.computed_length_fields_verified == 1U &&
                  encode_counts.integrity_bytes_accumulated == payload.size + 2U &&
                  encode_counts.integrity_bytes_verified == payload.size + 2U,
              "Encode operation counts are bounded by the actual dynamic range")) {
    return false;
  }
#endif

  std::array<DecodedFieldSlot, 2U> slots{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      plan, workspace, 0U, ByteView{output.data(), encoded.bytes_written}, slots.data(),
      slots.size());
  const auto decode_counts = workspace.LastOperationCounts();
  return Expect(
             decoded.status == CodecStatus::OK && decoded.field_count == 2U &&
                 slots[0].uint64_value == expected.size &&
                 slots[1].value_kind == LogicalValueKind::BYTES &&
                 slots[1].bytes_value.size == payload.size &&
                 std::equal(payload.data, payload.data + payload.size, slots[1].bytes_value.data),
             "Decode derives actual payload length and returns exactly the payload bytes") &&
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
         Expect(decode_counts.computed_length_fields_verified == 1U &&
                    decode_counts.integrity_bytes_verified == payload.size + 2U,
                "Decode operation counts are bounded by the actual dynamic range");
#else
         true;
#endif
}

std::string BoundedDecimalConfig(const std::string& source) {
  std::string config = source;
  if (!ReplaceOnce(config, "\"header_length_bytes\": 2", "\"header_length_bytes\": 4") ||
      !ReplaceOnce(config, "\"codec\": \"bytes\", \"byte_offset\": 2",
                   "\"codec\": \"bytes\", \"byte_offset\": 4")) {
    return {};
  }
  const std::size_t payload_id = config.find("\"id\": \"payload\"");
  const std::size_t payload_begin = config.rfind('{', payload_id);
  if (payload_id == std::string::npos || payload_begin == std::string::npos) return {};
  config.insert(payload_begin, R"json({
          "id": "scaled",
          "display_name": "Scaled",
          "description": "Synthetic non-identity Decimal conversion.",
          "source_ref": "SYNTHETIC_FROM_SCRATCH:bounded_decimal",
          "value_type": "UINT64",
          "wire": {"codec": "unsigned_integer", "byte_offset": 2, "byte_width": 2, "byte_order": "big_endian"},
          "encode": {"source": "input"},
          "conversion": {"kind": "linear", "output_type": "DECIMAL64", "scale": {"numerator": 2, "denominator": 1}, "bias": {"numerator": 1, "denominator": 1}}
        },
        )json");
  return config;
}

bool EncodeAndDecodeBoundedDecimal(const std::string& source, ByteView payload, ByteView expected) {
  auto compiled = pae::config_compiler::CompileJsonToPlan(BoundedDecimalConfig(source));
  if (!Expect(compiled.Succeeded(), "bounded Decimal configuration compiles")) return false;
  auto plan = std::move(compiled).TakePlan();
  if (!Expect(plan->Conversions().size() == 1U &&
                  plan->GetExecutionResourceLayout().conversion_value_count == 1U,
              "bounded Decimal descriptor and workspace slot are preserved")) {
    return false;
  }
  const std::string snapshot = pae::config_compiler::MakeDeterministicPlanSnapshot(*plan);
  if (!Expect(
          snapshot.find("\"snapshot_format\":\"pae_plan_bundle_v0.8_bounded_variable_slice\"") !=
                  std::string::npos &&
              snapshot.find("\"total_conversion_count\":1") != std::string::npos &&
              snapshot.find("\"conversion_index\":0") != std::string::npos &&
              snapshot.find("\"bounded_payload\":{") != std::string::npos &&
              snapshot.find("\"scope\":\"frame\"") != std::string::npos &&
              snapshot.find("\"range_ends_at_payload\":true") != std::string::npos,
          "Schema 0.8 snapshot preserves inherited and bounded execution descriptors")) {
    std::cerr << snapshot << '\n';
    return false;
  }
  ExecutionWorkspace workspace(*plan);
  std::array<EncodeFieldValue, 2U> values{};
  values[0].field = FieldRef{plan.get(), 0U, 1U};
  values[0].value_kind = LogicalValueKind::DECIMAL64;
  values[0].decimal64_value = {7, 0};
  values[1].field = FieldRef{plan.get(), 0U, 2U};
  values[1].value_kind = LogicalValueKind::BYTES;
  values[1].bytes_value = payload;
  std::array<std::uint8_t, 9U> output{};
  output.fill(0xCCU);
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(),
      MutableByteBuffer{output.data(), output.size()});
  if (!Expect(encoded.status == CodecStatus::OK && encoded.bytes_written == expected.size &&
                  std::equal(expected.data, expected.data + expected.size, output.data()),
              "bounded Decimal Encode matches an independent non-identity vector")) {
    return false;
  }
  std::array<DecodedFieldSlot, 3U> slots{};
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{output.data(), encoded.bytes_written}, slots.data(),
      slots.size());
  return Expect(
      decoded.status == CodecStatus::OK && decoded.field_count == 3U &&
          slots[0].uint64_value == expected.size &&
          slots[1].value_kind == LogicalValueKind::DECIMAL64 &&
          slots[1].decimal64_value.coefficient == 7 && slots[1].decimal64_value.scale == 0 &&
          slots[2].value_kind == LogicalValueKind::BYTES &&
          slots[2].bytes_value.size == payload.size &&
          std::equal(payload.data, payload.data + payload.size, slots[2].bytes_value.data),
      "bounded Decimal Decode preserves logical value and actual payload");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::string config = ReadText(argv[1]);
  auto compiled = pae::config_compiler::CompileJsonToPlan(config);
  if (!compiled.Succeeded()) {
    const auto* diagnostic = compiled.Diagnostic();
    if (diagnostic != nullptr) {
      std::cerr << "compile failed stage=" << static_cast<int>(diagnostic->stage)
                << " code=" << static_cast<int>(diagnostic->code)
                << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail
                << '\n';
    }
    return 1;
  }
  auto plan = std::move(compiled).TakePlan();
  if (!Expect(plan->SchemaVersion() == "0.8", "Schema 0.8 is preserved") ||
      !Expect(plan->Messages().size() == 1U && plan->Messages()[0].bounded_payload.has_value() &&
                  plan->MessageExecutionPlans()[0].bounded_payload.has_value(),
              "bounded descriptor is frozen in metadata and execution views") ||
      !Expect(plan->GetResourceRequirements().max_frame_bytes == 6U,
              "resource limit uses maximum frame size") ||
      !Expect(plan->PipelineExecutionPlans()[0].candidate_groups.empty() &&
                  plan->PipelineExecutionPlans()[0].variable_message_indices.size() == 1U,
              "bounded messages use the interval candidate index")) {
    return 1;
  }
  if (!ExpectCompileFailure(config, "\"min_payload_bytes\": 0", "\"min_payload_bytes\": 4",
                            CompileStage::DOMAIN_VALIDATION, CompileError::VALUE_NOT_REPRESENTABLE,
                            "/messages/0/layout/min_payload_bytes", "min exceeds max") ||
      !ExpectCompileFailure(config, "\"max_payload_bytes\": 3", "\"max_payload_bytes\": 300",
                            CompileStage::DOMAIN_VALIDATION, CompileError::VALUE_NOT_REPRESENTABLE,
                            "/messages/0/fields/0/computed", "wire cannot represent max frame") ||
      !ExpectCompileFailure(config, "\"payload_field_id\": \"payload\"",
                            "\"payload_field_id\": \"missing\"", CompileStage::DOMAIN_VALIDATION,
                            CompileError::UNKNOWN_REFERENCE, "/messages/0/layout/payload_field_id",
                            "payload reference missing") ||
      !ExpectCompileFailure(config,
                            "{\"kind\": \"fixed_bytes\", \"byte_offset\": 0, \"bytes\": \"A5\"}",
                            "{\"kind\": \"frame_length_equals\", \"length_bytes\": 6}",
                            CompileStage::DOMAIN_VALIDATION, CompileError::MATCHER_CONFLICT,
                            "/messages/0/matcher/all/0", "variable frame matcher forbidden") ||
      !ExpectCompileFailure(
          config, "\"byte_offset\": 0, \"end\": \"payload_end\"",
          "\"byte_offset\": 3, \"end\": \"payload_end\"", CompileStage::DOMAIN_VALIDATION,
          CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS, "/messages/0/integrity/range/byte_offset",
          "dynamic integrity starts after header") ||
      !ExpectLateBitContainerRejected(config)) {
    return 1;
  }
  if (!ExpectMissingComputedLengthRejected(config)) return 1;
  if (!ExpectSecondDynamicBytesRejected(config)) return 1;
  if (!ExpectUndefinedHeaderRejected(config)) return 1;

  ExecutionWorkspace workspace(*plan);
  const std::array<std::uint8_t, 1U> empty_storage{};
  const std::array<std::uint8_t, 3U> empty_expected{0xA5U, 0x03U, 0xA8U};
  if (!EncodeAndDecode(*plan, workspace, ByteView{empty_storage.data(), 0U},
                       ByteView{empty_expected.data(), empty_expected.size()})) {
    return 1;
  }
  const std::array<std::uint8_t, 2U> payload_two{0x10U, 0x20U};
  const std::array<std::uint8_t, 5U> expected_two{0xA5U, 0x05U, 0x10U, 0x20U, 0xDAU};
  if (!EncodeAndDecode(*plan, workspace, ByteView{payload_two.data(), payload_two.size()},
                       ByteView{expected_two.data(), expected_two.size()})) {
    return 1;
  }
  const std::array<std::uint8_t, 3U> payload_three{0x00U, 0xFFU, 0x01U};
  const std::array<std::uint8_t, 6U> expected_three{0xA5U, 0x06U, 0x00U, 0xFFU, 0x01U, 0xABU};
  if (!EncodeAndDecode(*plan, workspace, ByteView{payload_three.data(), payload_three.size()},
                       ByteView{expected_three.data(), expected_three.size()})) {
    return 1;
  }
  const std::array<std::uint8_t, 5U> decimal_empty_expected{0xA5U, 0x05U, 0x00U, 0x03U, 0xADU};
  if (!EncodeAndDecodeBoundedDecimal(
          config, ByteView{empty_storage.data(), 0U},
          ByteView{decimal_empty_expected.data(), decimal_empty_expected.size()})) {
    return 1;
  }
  const std::array<std::uint8_t, 7U> decimal_payload_expected{0xA5U, 0x07U, 0x00U, 0x03U,
                                                              0x10U, 0x20U, 0xDFU};
  if (!EncodeAndDecodeBoundedDecimal(
          config, ByteView{payload_two.data(), payload_two.size()},
          ByteView{decimal_payload_expected.data(), decimal_payload_expected.size()})) {
    return 1;
  }
  const std::array<std::uint8_t, 5U> payload_scope_expected{0xA5U, 0x02U, 0x10U, 0x20U, 0xD7U};
  if (!Expect(
          EncodeMutation(config, "\"scope\": \"frame\"", "\"scope\": \"payload\"",
                         ByteView{payload_two.data(), payload_two.size()}, 2U,
                         ByteView{payload_scope_expected.data(), payload_scope_expected.size()}),
          "payload scope uses actual P and checksum observes the stored length")) {
    return 1;
  }
  std::string no_integrity = config;
  const std::size_t integrity_begin = no_integrity.find("      \"integrity\": {");
  const std::size_t fields_begin = no_integrity.find("      \"fields\": [", integrity_begin);
  if (!Expect(integrity_begin != std::string::npos && fields_begin != std::string::npos,
              "no-integrity mutation anchors exist")) {
    return 1;
  }
  no_integrity.erase(integrity_begin, fields_begin - integrity_begin);
  const std::array<std::uint8_t, 4U> no_integrity_expected{0xA5U, 0x04U, 0x10U, 0x20U};
  if (!Expect(EncodeMutation(no_integrity, "\"scope\": \"frame\"", "\"scope\": \"frame\"",
                             ByteView{payload_two.data(), payload_two.size()}, 4U,
                             ByteView{no_integrity_expected.data(), no_integrity_expected.size()}),
              "bounded message without integrity derives a zero-byte trailer")) {
    return 1;
  }
  std::string crc_config = config;
  const std::string sum8_integrity =
      "\"algorithm\": \"sum8\",\n"
      "        \"range\": {\"byte_offset\": 0, \"end\": \"payload_end\"},\n"
      "        \"storage\": {\"anchor\": \"payload_end\"}";
  const std::string crc_integrity =
      "\"algorithm\": \"crc\",\n"
      "        \"parameters\": {\"width\": 16, \"poly\": \"1021\", \"init\": \"FFFF\", "
      "\"refin\": false, \"refout\": false, \"xorout\": \"0000\"},\n"
      "        \"range\": {\"byte_offset\": 0, \"end\": \"payload_end\"},\n"
      "        \"storage\": {\"anchor\": \"payload_end\", \"byte_order\": \"big_endian\"}";
  const std::array<std::uint8_t, 6U> crc_expected{0xA5U, 0x06U, 0x10U, 0x20U, 0x47U, 0x42U};
  if (!Expect(ReplaceOnce(crc_config, sum8_integrity, crc_integrity),
              "CRC mutation anchors exist") ||
      !Expect(EncodeMutation(crc_config, "\"scope\": \"frame\"", "\"scope\": \"frame\"",
                             ByteView{payload_two.data(), payload_two.size()}, 6U,
                             ByteView{crc_expected.data(), crc_expected.size()}),
              "dynamic CRC-16 trailer matches an independently calculated vector")) {
    return 1;
  }

  std::string fixed_range = config;
  const std::array<std::uint8_t, 5U> fixed_range_expected{0xA5U, 0x05U, 0x10U, 0x20U, 0xAAU};
  if (!Expect(EncodeMutation(fixed_range, "\"byte_offset\": 0, \"end\": \"payload_end\"",
                             "\"byte_offset\": 0, \"byte_length\": 2",
                             ByteView{payload_two.data(), payload_two.size()}, 5U,
                             ByteView{fixed_range_expected.data(), fixed_range_expected.size()}),
              "fixed integrity range remains independent of actual payload length")) {
    return 1;
  }

  std::string empty_range = config;
  const std::array<std::uint8_t, 3U> empty_range_expected{0xA5U, 0x03U, 0x00U};
  if (!Expect(
          EncodeMutation(empty_range, "\"byte_offset\": 0, \"end\": \"payload_end\"",
                         "\"byte_offset\": 2, \"end\": \"payload_end\"", ByteView{nullptr, 0U}, 3U,
                         ByteView{empty_range_expected.data(), empty_range_expected.size()}),
          "empty dynamic SUM8 range produces zero")) {
    return 1;
  }
  std::string empty_crc = crc_config;
  const std::array<std::uint8_t, 4U> empty_crc_expected{0xA5U, 0x04U, 0xFFU, 0xFFU};
  if (!Expect(EncodeMutation(empty_crc, "\"byte_offset\": 0, \"end\": \"payload_end\"",
                             "\"byte_offset\": 2, \"end\": \"payload_end\"", ByteView{nullptr, 0U},
                             4U, ByteView{empty_crc_expected.data(), empty_crc_expected.size()}),
              "empty dynamic CRC range preserves the configured initial state")) {
    return 1;
  }

  const auto check_wide_length = [&](std::uint64_t width, std::string_view order,
                                     ByteView expected) {
    std::string wide = config;
    const std::uint64_t header = width + 1U;
    if (!ReplaceOnce(wide, "\"header_length_bytes\": 2",
                     "\"header_length_bytes\": " + std::to_string(header)) ||
        !ReplaceOnce(wide, "\"byte_offset\": 1, \"byte_width\": 1",
                     "\"byte_offset\": 1, \"byte_width\": " + std::to_string(width) +
                         ", \"byte_order\": \"" + std::string{order} + "\"") ||
        !ReplaceOnce(wide, "\"codec\": \"bytes\", \"byte_offset\": 2",
                     "\"codec\": \"bytes\", \"byte_offset\": " + std::to_string(header))) {
      return false;
    }
    return EncodeMutation(wide, "\"scope\": \"frame\"", "\"scope\": \"frame\"",
                          ByteView{payload_two.data(), payload_two.size()}, expected.size,
                          expected);
  };
  const std::array<std::uint8_t, 6U> length16_be{0xA5U, 0x00U, 0x06U, 0x10U, 0x20U, 0xDBU};
  const std::array<std::uint8_t, 6U> length16_le{0xA5U, 0x06U, 0x00U, 0x10U, 0x20U, 0xDBU};
  const std::array<std::uint8_t, 8U> length32_be{0xA5U, 0x00U, 0x00U, 0x00U,
                                                 0x08U, 0x10U, 0x20U, 0xDDU};
  const std::array<std::uint8_t, 8U> length32_le{0xA5U, 0x08U, 0x00U, 0x00U,
                                                 0x00U, 0x10U, 0x20U, 0xDDU};
  if (!Expect(check_wide_length(2U, "big_endian", ByteView{length16_be.data(), length16_be.size()}),
              "two-byte big-endian actual frame length") ||
      !Expect(
          check_wide_length(2U, "little_endian", ByteView{length16_le.data(), length16_le.size()}),
          "two-byte little-endian actual frame length") ||
      !Expect(check_wide_length(4U, "big_endian", ByteView{length32_be.data(), length32_be.size()}),
              "four-byte big-endian actual frame length") ||
      !Expect(
          check_wide_length(4U, "little_endian", ByteView{length32_le.data(), length32_le.size()}),
          "four-byte little-endian actual frame length")) {
    return 1;
  }

  std::array<DecodedFieldSlot, 2U> slots{};
  auto bad_length = expected_two;
  bad_length[1] = 0x04U;
  auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{bad_length.data(), bad_length.size()}, slots.data(),
      slots.size());
  if (!Expect(decoded.status == CodecStatus::LENGTH_MISMATCH && decoded.field_count == 0U,
              "derived structure precedes computed length validation")) {
    return 1;
  }
  auto bad_integrity = expected_two;
  bad_integrity.back() ^= 0x01U;
  decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{bad_integrity.data(), bad_integrity.size()}, slots.data(),
      slots.size());
  if (!Expect(decoded.status == CodecStatus::INTEGRITY_FAILED && decoded.field_count == 0U,
              "integrity failure does not deliver fields")) {
    return 1;
  }
  const std::array<std::uint8_t, 2U> too_short{0xA5U, 0x02U};
  decoded = pae::protocol_core::DecodeCompleteRecord(*plan, workspace, 0U,
                                                     ByteView{too_short.data(), too_short.size()},
                                                     slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::UNKNOWN_MESSAGE,
              "actual size below the bounded interval is not a structural candidate")) {
    return 1;
  }

  const std::array<std::uint8_t, 4U> oversized_payload{1U, 2U, 3U, 4U};
  EncodeFieldValue oversized{};
  oversized.field = FieldRef{plan.get(), 0U, 1U};
  oversized.value_kind = LogicalValueKind::BYTES;
  oversized.bytes_value = ByteView{oversized_payload.data(), oversized_payload.size()};
  std::array<std::uint8_t, 7U> output{};
  output.fill(0x5AU);
  const auto oversized_result = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &oversized, 1U, MutableByteBuffer{output.data(), output.size()});
  if (!Expect(oversized_result.status == CodecStatus::BYTES_LENGTH_MISMATCH &&
                  oversized_result.bytes_written == 0U && oversized_result.required_size == 0U &&
                  std::all_of(output.begin(), output.end(),
                              [](std::uint8_t value) { return value == 0x5AU; }),
              "payload above max fails before output mutation")) {
    return 1;
  }

  std::cout << "bounded variable record contract checks passed\n";
  return 0;
}
