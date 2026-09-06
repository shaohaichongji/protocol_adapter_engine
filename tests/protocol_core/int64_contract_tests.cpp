#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "complete_record_codec.h"
#include "config_compiler.h"

namespace {

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

std::string MinimalConfig(std::size_t width, std::string_view order,
                          std::string_view encode = R"({"source":"input"})") {
  const std::string byte_order =
      width == 1U ? "" : ",\"byte_order\":\"" + std::string(order) + "\"";
  return "{\"schema_version\":\"0.4\",\"protocol_id\":\"int64_width\","
         "\"protocol_version\":\"1\",\"display_name\":\"INT64\","
         "\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:DEC042A\",\"resource_profile\":\"desktop\","
         "\"framing_profiles\":[{\"id\":\"f\",\"display_name\":\"F\",\"description\":\"\","
         "\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:DEC042A\",\"input_kind\":\"complete_record\"}],"
         "\"pipelines\":[{\"id\":\"p\",\"display_name\":\"P\",\"description\":\"\","
         "\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:DEC042A\",\"direction_id\":\"d\","
         "\"input_framing_profile_id\":\"f\",\"message_ids\":[\"m\"]}],"
         "\"messages\":[{\"id\":\"m\",\"display_name\":\"M\",\"description\":\"\","
         "\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:DEC042A\",\"direction_id\":\"d\","
         "\"frame_length_bytes\":" +
         std::to_string(width) +
         ",\"matcher\":{\"all\":[{\"kind\":\"frame_length_equals\",\"length_bytes\":" +
         std::to_string(width) +
         "}]},\"fields\":[{\"id\":\"value\",\"display_name\":\"Value\",\"description\":\"\","
         "\"source_ref\":\"SYNTHETIC_FROM_SCRATCH:DEC042A\",\"value_type\":\"INT64\","
         "\"wire\":{\"codec\":\"unsigned_integer\",\"byte_offset\":0,\"byte_width\":" +
         std::to_string(width) + byte_order + "},\"encode\":" + std::string(encode) + "}]}]}";
}

std::string WithFixedMatcher(std::string config, std::size_t width, std::string_view bytes) {
  const std::string original =
      "\"matcher\":{\"all\":[{\"kind\":\"frame_length_equals\",\"length_bytes\":" +
      std::to_string(width) + "}]}";
  const std::string replacement =
      "\"matcher\":{\"all\":[{\"kind\":\"frame_length_equals\",\"length_bytes\":" +
      std::to_string(width) + "},{\"kind\":\"fixed_bytes\",\"byte_offset\":0,\"bytes\":\"" +
      std::string(bytes) + "\"}]}";
  config.replace(config.find(original), original.size(), replacement);
  return config;
}

bool RoundTrip(std::size_t width, std::string_view order, std::int64_t value) {
  auto compiled = pae::config_compiler::CompileJsonToPlan(MinimalConfig(width, order));
  if (!compiled.Succeeded()) return false;
  auto plan = std::move(compiled).TakePlan();
  ExecutionWorkspace workspace(*plan);
  EncodeFieldValue input;
  input.field = FieldRef{plan.get(), 0U, 0U};
  input.value_kind = LogicalValueKind::INT64;
  input.int64_value = value;
  std::vector<std::uint8_t> frame(width, 0xA5U);
  const auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &input, 1U, MutableByteBuffer{frame.data(), frame.size()});
  DecodedFieldSlot output;
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *plan, workspace, 0U, ByteView{frame.data(), frame.size()}, &output, 1U);
  return encoded.status == CodecStatus::OK && encoded.bytes_written == width &&
         decoded.status == CodecStatus::OK && decoded.field_count == 1U &&
         output.value_kind == LogicalValueKind::INT64 && output.int64_value == value;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  for (std::size_t width = 1U; width <= 8U; ++width) {
    const unsigned bits = static_cast<unsigned>(width * 8U);
    const std::int64_t minimum = width == 8U
                                     ? (std::numeric_limits<std::int64_t>::min)()
                                     : -static_cast<std::int64_t>(std::uint64_t{1U} << (bits - 1U));
    const std::int64_t maximum =
        width == 8U ? (std::numeric_limits<std::int64_t>::max)()
                    : static_cast<std::int64_t>((std::uint64_t{1U} << (bits - 1U)) - 1U);
    for (const auto value :
         {std::int64_t{0}, std::int64_t{1}, std::int64_t{-1}, minimum, maximum}) {
      if (!Expect(RoundTrip(width, "big_endian", value), "all widths and boundaries round-trip") ||
          !Expect(RoundTrip(width, "little_endian", value), "both byte orders round-trip"))
        return 1;
    }
  }

  auto compiled = pae::config_compiler::CompileJsonToPlan(MinimalConfig(3U, "big_endian"));
  auto plan = std::move(compiled).TakePlan();
  ExecutionWorkspace workspace(*plan);
  EncodeFieldValue input;
  input.field = FieldRef{plan.get(), 0U, 0U};
  input.value_kind = LogicalValueKind::INT64;
  input.int64_value = 8388608;
  std::array<std::uint8_t, 3U> frame{0xCCU, 0xCCU, 0xCCU};
  auto encoded = pae::protocol_core::EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, &input, 1U, MutableByteBuffer{frame.data(), frame.size()});
  if (!Expect(encoded.status == CodecStatus::VALUE_NOT_REPRESENTABLE && encoded.bytes_written == 0U,
              "positive narrow overflow fails without delivery"))
    return 1;
  input.int64_value = -8388609;
  encoded = pae::protocol_core::EncodeCompleteRecord(*plan, workspace, 0U, 0U, &input, 1U,
                                                     MutableByteBuffer{frame.data(), frame.size()});
  if (!Expect(encoded.status == CodecStatus::VALUE_NOT_REPRESENTABLE && encoded.bytes_written == 0U,
              "negative narrow overflow fails without delivery"))
    return 1;
  input.value_kind = LogicalValueKind::UINT64;
  input.uint64_value = 1U;
  encoded = pae::protocol_core::EncodeCompleteRecord(*plan, workspace, 0U, 0U, &input, 1U,
                                                     MutableByteBuffer{frame.data(), frame.size()});
  if (!Expect(encoded.status == CodecStatus::TYPE_MISMATCH, "UINT64 is not implicitly INT64"))
    return 1;

  auto signed_negative_zero = pae::config_compiler::CompileJsonToPlan(
      MinimalConfig(1U, "big_endian", R"({"source":"constant","value":-0})"));
  if (!Expect(signed_negative_zero.Succeeded() &&
                  signed_negative_zero.Plan()->Messages()[0].fields[0].signed_constant_value == 0,
              "configuration signed -0 normalizes to zero"))
    return 1;
  auto narrow_constant = pae::config_compiler::CompileJsonToPlan(
      MinimalConfig(3U, "big_endian", R"({"source":"constant","value":8388608})"));
  if (!Expect(!narrow_constant.Succeeded() && narrow_constant.Diagnostic() != nullptr &&
                  narrow_constant.Diagnostic()->stage ==
                      pae::config_compiler::CompileStage::DOMAIN_VALIDATION &&
                  narrow_constant.Diagnostic()->code ==
                      pae::config_compiler::CompileError::VALUE_NOT_REPRESENTABLE,
              "constant outside signed wire range fails during domain validation"))
    return 1;
  auto matching_constant = pae::config_compiler::CompileJsonToPlan(WithFixedMatcher(
      MinimalConfig(3U, "big_endian", R"({"source":"constant","value":-2})"), 3U, "FF FF FE"));
  if (!Expect(matching_constant.Succeeded(),
              "negative INT64 constant can agree with a fixed-byte Matcher"))
    return 1;
  auto positive_constant = pae::config_compiler::CompileJsonToPlan(
      MinimalConfig(1U, "big_endian", R"({"source":"constant","value":127})"));
  if (!Expect(positive_constant.Succeeded() &&
                  positive_constant.Plan()->Messages()[0].fields[0].signed_constant_value == 127,
              "positive INT64 constant is retained as signed data"))
    return 1;
  auto minimum_constant = pae::config_compiler::CompileJsonToPlan(
      MinimalConfig(8U, "big_endian", R"({"source":"constant","value":-9223372036854775808})"));
  if (!Expect(minimum_constant.Succeeded(), "INT64_MIN constant compiles")) return 1;
  auto minimum_plan = std::move(minimum_constant).TakePlan();
  ExecutionWorkspace minimum_workspace(*minimum_plan);
  std::array<std::uint8_t, 8U> minimum_frame{};
  const auto minimum_encoded = pae::protocol_core::EncodeCompleteRecord(
      *minimum_plan, minimum_workspace, 0U, 0U, nullptr, 0U,
      MutableByteBuffer{minimum_frame.data(), minimum_frame.size()});
  if (!Expect(minimum_encoded.status == CodecStatus::OK && minimum_encoded.bytes_written == 8U &&
                  minimum_frame == std::array<std::uint8_t, 8U>{0x80U, 0U, 0U, 0U, 0U, 0U, 0U, 0U},
              "INT64_MIN constant encodes without signed overflow"))
    return 1;
  auto conflicting_constant = pae::config_compiler::CompileJsonToPlan(WithFixedMatcher(
      MinimalConfig(3U, "big_endian", R"({"source":"constant","value":-2})"), 3U, "FF FF FD"));
  if (!Expect(!conflicting_constant.Succeeded() && conflicting_constant.Diagnostic() != nullptr &&
                  conflicting_constant.Diagnostic()->stage ==
                      pae::config_compiler::CompileStage::DOMAIN_VALIDATION &&
                  conflicting_constant.Diagnostic()->code ==
                      pae::config_compiler::CompileError::MATCHER_CONFLICT &&
                  conflicting_constant.Diagnostic()->json_pointer ==
                      "/messages/0/fields/0/encode/value",
              "signed constant and fixed-byte Matcher conflict fails at the value"))
    return 1;
  auto fractional_constant = pae::config_compiler::CompileJsonToPlan(
      MinimalConfig(3U, "big_endian", R"({"source":"constant","value":1.0})"));
  if (!Expect(!fractional_constant.Succeeded() && fractional_constant.Diagnostic() != nullptr &&
                  fractional_constant.Diagnostic()->code ==
                      pae::config_compiler::CompileError::INTEGER_NOT_EXACT,
              "fractional INT64 constant is rejected structurally"))
    return 1;
  auto exponent_constant = pae::config_compiler::CompileJsonToPlan(
      MinimalConfig(3U, "big_endian", R"({"source":"constant","value":1e0})"));
  if (!Expect(!exponent_constant.Succeeded() && exponent_constant.Diagnostic() != nullptr &&
                  exponent_constant.Diagnostic()->code ==
                      pae::config_compiler::CompileError::INTEGER_NOT_EXACT,
              "exponent INT64 constant is rejected structurally"))
    return 1;
  std::string old_schema = MinimalConfig(3U, "big_endian");
  old_schema.replace(old_schema.find("\"0.4\""), 5U, "\"0.3\"");
  auto old_generation = pae::config_compiler::CompileJsonToPlan(old_schema);
  if (!Expect(
          !old_generation.Succeeded() && old_generation.Diagnostic() != nullptr &&
              old_generation.Diagnostic()->stage == pae::config_compiler::CompileStage::STRUCTURAL,
          "Schema 0.3 does not gain INT64"))
    return 1;

  const std::string root = argv[1];
  const std::string public_config = ReadText(root + "/synthetic_int64_slice.pae.json");
  auto public_compiled = pae::config_compiler::CompileJsonToPlan(public_config);
  if (!Expect(public_compiled.Succeeded(), "public Schema 0.4 vector compiles")) return 1;
  auto public_plan = std::move(public_compiled).TakePlan();
  const auto& requirements = public_plan->GetResourceRequirements();
  const auto& memory = public_plan->GetPlanMemoryReport();
  const std::size_t categorized_memory =
      memory.alignment_bytes + memory.object_bytes + memory.string_bytes + memory.matcher_bytes +
      memory.metadata_container_bytes + memory.execution_descriptor_bytes + memory.index_bytes +
      memory.extension_bytes;
  const std::string snapshot = pae::config_compiler::MakeDeterministicPlanSnapshot(*public_plan);
  if (!Expect(public_plan->SchemaVersion() == "0.4" && requirements.total_field_count == 8U &&
                  requirements.total_bit_container_count == 1U &&
                  requirements.total_integrity_rule_count == 1U &&
                  memory.accounted_total_bytes == categorized_memory &&
                  memory.accounted_total_bytes != 0U &&
                  snapshot.find("\"snapshot_format\":\"pae_plan_bundle_v0.4_int64_slice\"") !=
                      std::string::npos &&
                  snapshot.find("\"total_bit_container_count\":1") != std::string::npos &&
                  snapshot.find("\"total_integrity_rule_count\":1") != std::string::npos,
              "Schema, fields, inherited rules, and exact Plan memory categories are counted"))
    return 1;

  const std::array<std::uint8_t, 3U> negative_two_be{0xFFU, 0xFFU, 0xFEU};
  DecodedFieldSlot decoded_slot;
  auto tiny_compiled = pae::config_compiler::CompileJsonToPlan(MinimalConfig(3U, "big_endian"));
  auto tiny_plan = std::move(tiny_compiled).TakePlan();
  ExecutionWorkspace tiny_workspace(*tiny_plan);
  const auto decoded = pae::protocol_core::DecodeCompleteRecord(
      *tiny_plan, tiny_workspace, 0U, ByteView{negative_two_be.data(), negative_two_be.size()},
      &decoded_slot, 1U);
  if (!Expect(decoded.status == CodecStatus::OK && decoded_slot.int64_value == -2,
              "independent BE24 FF FF FE decodes as -2"))
    return 1;

  std::cout << "INT64_RESOURCE_EVIDENCE field_plan=" << sizeof(pae::protocol_plan::FieldPlan)
            << " frozen_field_plan=" << sizeof(pae::protocol_plan::FrozenFieldPlan)
            << " execution_field_plan=" << sizeof(pae::protocol_plan::FieldExecutionPlan)
            << " encode_value=" << sizeof(EncodeFieldValue)
            << " decoded_slot=" << sizeof(DecodedFieldSlot)
            << " plan_bytes=" << memory.accounted_total_bytes << " workspace_bytes="
            << public_plan->GetExecutionResourceLayout().estimated_workspace_bytes << '\n';
  std::cout << "PAE DEC-042A INT64 contract tests passed\n";
  return 0;
}
