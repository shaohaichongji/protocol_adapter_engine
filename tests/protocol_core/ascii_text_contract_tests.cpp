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
using pae::config_compiler::CompileJsonToPlan;
using pae::config_compiler::MakeDeterministicPlanSnapshot;
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

bool Expect(bool condition, std::string_view label) {
  if (!condition) std::cerr << "FAILED: " << label << '\n';
  return condition;
}

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

ByteView Bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

std::string OneFieldConfig(std::string_view actions, std::string_view wire, bool encode_declared) {
  return std::string{R"JSON({
"schema_version":"0.10","protocol_id":"ascii_test","protocol_version":"1",
"display_name":"ASCII test","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","input_kind":"complete_record"}],
"pipelines":[{"id":"pipe","display_name":"Pipe","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii",)JSON"} +
         std::string{actions} + R"JSON(},
"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","value_type":"BYTES","wire":)JSON" +
         std::string{wire} +
         (encode_declared ? R"JSON(,"encode":{"source":"input"}}]}]})JSON" : R"JSON(}]}]})JSON");
}

std::string LiteralOnlyConfig(std::string_view actions) {
  return std::string{R"JSON({
"schema_version":"0.10","protocol_id":"ascii_literal_only","protocol_version":"1",
"display_name":"ASCII literal only","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_LITERAL_ONLY",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_LITERAL_ONLY","input_kind":"complete_record"}],
"pipelines":[{"id":"pipe","display_name":"Pipe","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_LITERAL_ONLY","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_LITERAL_ONLY","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii",)JSON"} +
         std::string{actions} + R"JSON(},"fields":[]}]})JSON";
}

bool CheckPrimaryVector(const char* fixture_path) {
  auto compiled = CompileJsonToPlan(ReadFile(fixture_path));
  if (!Expect(compiled.Succeeded(), "primary config compiles")) return false;
  auto owner = std::move(compiled).TakePlan();
  const auto& plan = *owner;
  bool ok = Expect(plan.SchemaVersion() == "0.10", "schema version frozen") &&
            Expect(plan.GetResourceRequirements().total_text_segment_count == 10U,
                   "segment count budgeted") &&
            Expect(plan.GetPlanMemoryReport().accounted_total_bytes > 0U, "plan memory accounted");
  const std::string first_snapshot = MakeDeterministicPlanSnapshot(plan);
  const std::string second_snapshot = MakeDeterministicPlanSnapshot(plan);
  ok &= Expect(first_snapshot == second_snapshot, "V10 Plan snapshot deterministic");
  ok &= Expect(
      first_snapshot.find("\"snapshot_format\":\"pae_plan_bundle_v0.10_ascii_text_slice\"") !=
              std::string::npos &&
          first_snapshot.find("\"total_text_segment_count\":10") != std::string::npos &&
          first_snapshot.find("\"text_decode\":{") != std::string::npos &&
          first_snapshot.find("\"wire_codec\":\"ascii_text\"") != std::string::npos,
      "V10 Plan snapshot retains text execution facts");
  ExecutionWorkspace workspace{plan};

  std::string name = "ALICE";
  std::string tag = "Z";
  std::array<EncodeFieldValue, 2> values{};
  values[0].field = FieldRef{&plan, 0U, 0U};
  values[0].value_kind = LogicalValueKind::BYTES;
  values[0].bytes_value = Bytes(name);
  values[1].field = FieldRef{&plan, 0U, 2U};
  values[1].value_kind = LogicalValueKind::BYTES;
  values[1].bytes_value = Bytes(tag);
  std::array<std::uint8_t, 32> output{};
  output.fill(0xA5U);
  auto encoded = EncodeCompleteRecord(plan, workspace, 0U, 0U, values.data(), values.size(),
                                      MutableByteBuffer{output.data(), output.size()});
  const std::string expected = "TX ALICE!Z\r\n";
  ok &= Expect(encoded.status == CodecStatus::OK && encoded.bytes_written == expected.size(),
               "independent encode succeeds");
  ok &= Expect(std::equal(expected.begin(), expected.end(), output.begin()),
               "independent expected bytes");

  const std::string input = "RX ALICE!OK\r\n";
  std::array<DecodedFieldSlot, 2> slots{};
  auto decoded =
      DecodeCompleteRecord(plan, workspace, 0U, Bytes(input), slots.data(), slots.size());
  ok &= Expect(decoded.status == CodecStatus::OK && decoded.field_count == 2U,
               "independent decode succeeds");
  ok &= Expect(
      slots[0].field.field_index == 0U && slots[0].bytes_value.size == 5U &&
          std::string_view{reinterpret_cast<const char*>(slots[0].bytes_value.data), 5U} == "ALICE",
      "variable field exact and borrowed");
  ok &= Expect(
      slots[1].field.field_index == 1U && slots[1].bytes_value.size == 2U &&
          std::string_view{reinterpret_cast<const char*>(slots[1].bytes_value.data), 2U} == "OK",
      "decode-only field exact");
  ok &=
      Expect(slots[0].bytes_value.data == reinterpret_cast<const std::uint8_t*>(input.data() + 3U),
             "decoded bytes borrow input");

  decoded = DecodeCompleteRecord(plan, workspace, 0U, Bytes(input), slots.data(), 1U);
  ok &= Expect(decoded.status == CodecStatus::OUTPUT_SLOTS_TOO_SMALL && decoded.field_count == 0U &&
                   decoded.required_field_count == 2U,
               "capacity failure has zero delivery");
  std::string invalid_input = "RX A";
  invalid_input.push_back(static_cast<char>(0x80U));
  invalid_input += "!OK\r\n";
  decoded =
      DecodeCompleteRecord(plan, workspace, 0U, Bytes(invalid_input), slots.data(), slots.size());
  ok &= Expect(
      decoded.status == CodecStatus::ASCII_CHARACTER_NOT_ALLOWED && decoded.field_count == 0U,
      "non-ASCII Decode rejected without delivery");
  const std::string trailing = "RX ALICE!OK\r\nX";
  decoded = DecodeCompleteRecord(plan, workspace, 0U, Bytes(trailing), slots.data(), slots.size());
  ok &= Expect(decoded.status == CodecStatus::UNKNOWN_MESSAGE && decoded.field_count == 0U,
               "trailing bytes rejected");

  auto missing = EncodeCompleteRecord(plan, workspace, 0U, 0U, values.data(), 1U,
                                      MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(missing.status == CodecStatus::MISSING_FIELD && missing.bytes_written == 0U,
               "missing input rejected");
  std::array<EncodeFieldValue, 3> duplicate{values[0], values[1], values[0]};
  auto duplicate_result =
      EncodeCompleteRecord(plan, workspace, 0U, 0U, duplicate.data(), duplicate.size(),
                           MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(duplicate_result.status == CodecStatus::DUPLICATE_FIELD &&
                   duplicate_result.bytes_written == 0U,
               "duplicate input rejected");
  EncodeFieldValue decode_only = values[0];
  decode_only.field.field_index = 1U;
  auto extra = EncodeCompleteRecord(plan, workspace, 0U, 0U, &decode_only, 1U,
                                    MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(extra.status == CodecStatus::FIELD_REFERENCE_MISMATCH && extra.bytes_written == 0U,
               "Decode-only input rejected");

  std::string non_ascii(1U, static_cast<char>(0x80U));
  values[0].bytes_value = Bytes(non_ascii);
  auto invalid_ascii = EncodeCompleteRecord(plan, workspace, 0U, 0U, values.data(), values.size(),
                                            MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(invalid_ascii.status == CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
                   invalid_ascii.bytes_written == 0U,
               "non-ASCII encode rejected");
  values[0].bytes_value = Bytes(name);
  auto small = EncodeCompleteRecord(plan, workspace, 0U, 0U, values.data(), values.size(),
                                    MutableByteBuffer{output.data(), expected.size() - 1U});
  ok &= Expect(small.status == CodecStatus::BUFFER_TOO_SMALL && small.bytes_written == 0U &&
                   small.required_size == expected.size(),
               "small output reports exact requirement");

  pae::protocol_core::test_only::CorruptAsciiOutputBeforeFinalReviewOnce();
  auto corrupted = EncodeCompleteRecord(plan, workspace, 0U, 0U, values.data(), values.size(),
                                        MutableByteBuffer{output.data(), output.size()});
  ok &=
      Expect(corrupted.status == CodecStatus::FINAL_REVIEW_FAILED && corrupted.bytes_written == 0U,
             "TX second-pass fault rejected");
  auto recovered = EncodeCompleteRecord(plan, workspace, 0U, 0U, values.data(), values.size(),
                                        MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(recovered.status == CodecStatus::OK, "encode recovers after failure");
  return ok;
}

bool CheckBoundariesAndActions() {
  bool ok = true;
  const std::string wire =
      R"JSON({"codec":"ascii_text","min_byte_length":0,"max_byte_length":4})JSON";
  const std::string both =
      R"JSON("decode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"literal","text":"ABA"}]},"encode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"literal","text":"ABA"}]})JSON";
  auto compiled = CompileJsonToPlan(OneFieldConfig(both, wire, true));
  ok &= Expect(compiled.Succeeded(), "terminator config compiles");
  if (!compiled.Succeeded()) return false;
  auto owner = std::move(compiled).TakePlan();
  ExecutionWorkspace workspace{*owner};
  EncodeFieldValue value;
  value.field = FieldRef{owner.get(), 0U, 0U};
  value.value_kind = LogicalValueKind::BYTES;
  std::array<std::uint8_t, 16> output{};
  std::string conflict_value = "AB";
  value.bytes_value = Bytes(conflict_value);
  auto conflict = EncodeCompleteRecord(*owner, workspace, 0U, 0U, &value, 1U,
                                       MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(
      conflict.status == CodecStatus::ASCII_TERMINATOR_CONFLICT && conflict.bytes_written == 0U,
      "ABA cross-boundary conflict rejected");
  const std::string ambiguous_input = "ABABA";
  DecodedFieldSlot slot;
  auto no_backtrack =
      DecodeCompleteRecord(*owner, workspace, 0U, Bytes(ambiguous_input), &slot, 1U);
  ok &=
      Expect(no_backtrack.status == CodecStatus::UNKNOWN_MESSAGE && no_backtrack.field_count == 0U,
             "Decode uses first terminator without backtracking");
  std::string valid_value = "A";
  value.bytes_value = Bytes(valid_value);
  auto valid = EncodeCompleteRecord(*owner, workspace, 0U, 0U, &value, 1U,
                                    MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(valid.status == CodecStatus::OK && valid.bytes_written == 4U &&
                   std::string_view{reinterpret_cast<const char*>(output.data()), 4U} == "AABA",
               "non-conflicting terminator encode succeeds");

  const std::string decode_only =
      R"JSON("decode":{"segments":[{"kind":"literal","text":"<"},{"kind":"field","field_id":"value"},{"kind":"literal","text":">"}]})JSON";
  auto decode_compiled = CompileJsonToPlan(OneFieldConfig(decode_only, wire, false));
  ok &= Expect(decode_compiled.Succeeded(), "Decode-only config compiles");
  if (decode_compiled.Succeeded()) {
    auto decode_owner = std::move(decode_compiled).TakePlan();
    ExecutionWorkspace decode_workspace{*decode_owner};
    auto unsupported = EncodeCompleteRecord(*decode_owner, decode_workspace, 0U, 0U, nullptr, 0U,
                                            MutableByteBuffer{output.data(), output.size()});
    ok &= Expect(unsupported.status == CodecStatus::OPERATION_NOT_SUPPORTED,
                 "missing Encode action rejected");
  }

  const std::string encode_only =
      R"JSON("encode":{"segments":[{"kind":"literal","text":"<"},{"kind":"field","field_id":"value"},{"kind":"literal","text":">"}]})JSON";
  auto encode_compiled = CompileJsonToPlan(OneFieldConfig(encode_only, wire, true));
  if (!encode_compiled.Succeeded() && encode_compiled.Diagnostic() != nullptr) {
    std::cerr << "encode-only diagnostic code="
              << static_cast<int>(encode_compiled.Diagnostic()->code)
              << " pointer=" << encode_compiled.Diagnostic()->json_pointer
              << " detail=" << encode_compiled.Diagnostic()->detail << '\n';
  }
  ok &= Expect(encode_compiled.Succeeded(), "Encode-only config compiles");
  if (encode_compiled.Succeeded()) {
    auto encode_owner = std::move(encode_compiled).TakePlan();
    ExecutionWorkspace encode_workspace{*encode_owner};
    auto unsupported =
        DecodeCompleteRecord(*encode_owner, encode_workspace, 0U, Bytes("<A>"), &slot, 1U);
    ok &= Expect(unsupported.status == CodecStatus::OPERATION_NOT_SUPPORTED,
                 "missing Decode action rejected");
  }

  const std::string trailing_action =
      R"JSON("decode":{"segments":[{"kind":"literal","text":"<"},{"kind":"field","field_id":"value"}]})JSON";
  auto trailing_compiled = CompileJsonToPlan(OneFieldConfig(trailing_action, wire, false));
  ok &= Expect(trailing_compiled.Succeeded(), "trailing remainder config compiles");
  if (trailing_compiled.Succeeded()) {
    auto trailing_owner = std::move(trailing_compiled).TakePlan();
    ExecutionWorkspace trailing_workspace{*trailing_owner};
    DecodedFieldSlot trailing_slot;
    auto trailing_result = DecodeCompleteRecord(*trailing_owner, trailing_workspace, 0U,
                                                Bytes("<ABC"), &trailing_slot, 1U);
    ok &= Expect(trailing_result.status == CodecStatus::OK && trailing_slot.bytes_value.size == 3U,
                 "trailing field consumes record remainder");
  }

  const std::string control_wire =
      R"JSON({"codec":"ascii_text","min_byte_length":1,"max_byte_length":1,"allowed_control_bytes":"09"})JSON";
  auto control_compiled = CompileJsonToPlan(OneFieldConfig(trailing_action, control_wire, false));
  ok &= Expect(control_compiled.Succeeded(), "explicit control whitelist config compiles");
  if (control_compiled.Succeeded()) {
    auto control_owner = std::move(control_compiled).TakePlan();
    ExecutionWorkspace control_workspace{*control_owner};
    const std::string tab_record{"<\t", 2U};
    DecodedFieldSlot control_slot;
    auto control_result = DecodeCompleteRecord(*control_owner, control_workspace, 0U,
                                               Bytes(tab_record), &control_slot, 1U);
    ok &= Expect(control_result.status == CodecStatus::OK && control_slot.bytes_value.size == 1U,
                 "explicit tab control accepted");
  }
  return ok;
}

bool CheckLiteralOnlyActions(const char* fixture_path) {
  auto compiled = CompileJsonToPlan(ReadFile(fixture_path));
  if (!Expect(compiled.Succeeded(), "public literal-only config compiles")) return false;
  auto owner = std::move(compiled).TakePlan();
  const auto& plan = *owner;
  bool ok =
      Expect(plan.Messages()[0].fields.empty(), "literal-only Plan has no placeholder fields") &&
      Expect(plan.GetResourceRequirements().total_field_count == 0U,
             "literal-only field budget is zero") &&
      Expect(plan.GetExecutionResourceLayout().max_fields_per_message == 0U &&
                 plan.GetExecutionResourceLayout().max_input_fields_per_message == 0U,
             "literal-only Workspace field capacities are zero");
  ExecutionWorkspace workspace{plan};
  std::array<std::uint8_t, 16> output{};
  output.fill(0xA5U);
  const auto encoded = EncodeCompleteRecord(plan, workspace, 0U, 0U, nullptr, 0U,
                                            MutableByteBuffer{output.data(), output.size()});
  const std::string expected_tx = "PONG\r\n";
  ok &= Expect(encoded.status == CodecStatus::OK && encoded.bytes_written == expected_tx.size() &&
                   std::equal(expected_tx.begin(), expected_tx.end(), output.begin()),
               "zero-value literal-only Encode matches independent bytes");

  const std::string expected_rx = "PING\r\n";
  const auto decoded = DecodeCompleteRecord(plan, workspace, 0U, Bytes(expected_rx), nullptr, 0U);
  ok &= Expect(decoded.status == CodecStatus::OK && decoded.field_count == 0U &&
                   decoded.required_field_count == 0U,
               "zero-slot literal-only Decode succeeds without field delivery");
  for (const std::string invalid : {"PANG\r\n", "PING\r", "PING\r\nX"}) {
    const auto rejected = DecodeCompleteRecord(plan, workspace, 0U, Bytes(invalid), nullptr, 0U);
    ok &= Expect(rejected.status == CodecStatus::UNKNOWN_MESSAGE && rejected.field_count == 0U,
                 "literal mismatch, truncation, or trailing bytes fail closed");
  }

  EncodeFieldValue extra;
  const auto extra_result = EncodeCompleteRecord(plan, workspace, 0U, 0U, &extra, 1U,
                                                 MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(
      extra_result.status == CodecStatus::INVALID_ARGUMENT && extra_result.bytes_written == 0U,
      "literal-only Encode rejects an extra input descriptor");

  pae::protocol_core::test_only::CorruptAsciiOutputBeforeFinalReviewOnce();
  const auto corrupted = EncodeCompleteRecord(plan, workspace, 0U, 0U, nullptr, 0U,
                                              MutableByteBuffer{output.data(), output.size()});
  ok &=
      Expect(corrupted.status == CodecStatus::FINAL_REVIEW_FAILED && corrupted.bytes_written == 0U,
             "literal-only TX second-pass review rejects corruption");
  const auto recovered = EncodeCompleteRecord(plan, workspace, 0U, 0U, nullptr, 0U,
                                              MutableByteBuffer{output.data(), output.size()});
  ok &= Expect(recovered.status == CodecStatus::OK, "literal-only Encode recovers after failure");

  const std::string decode_only =
      R"JSON("decode":{"segments":[{"kind":"literal","text":"PING\r\n"}]})JSON";
  auto decode_compiled = CompileJsonToPlan(LiteralOnlyConfig(decode_only));
  ok &= Expect(decode_compiled.Succeeded(), "literal-only Decode-only config compiles");
  if (decode_compiled.Succeeded()) {
    auto decode_owner = std::move(decode_compiled).TakePlan();
    ExecutionWorkspace decode_workspace{*decode_owner};
    const auto unsupported =
        EncodeCompleteRecord(*decode_owner, decode_workspace, 0U, 0U, nullptr, 0U,
                             MutableByteBuffer{output.data(), output.size()});
    ok &= Expect(unsupported.status == CodecStatus::OPERATION_NOT_SUPPORTED,
                 "literal-only missing Encode action is unsupported");
  }

  const std::string encode_only =
      R"JSON("encode":{"segments":[{"kind":"literal","text":"PONG\r\n"}]})JSON";
  auto encode_compiled = CompileJsonToPlan(LiteralOnlyConfig(encode_only));
  ok &= Expect(encode_compiled.Succeeded(), "literal-only Encode-only config compiles");
  if (encode_compiled.Succeeded()) {
    auto encode_owner = std::move(encode_compiled).TakePlan();
    ExecutionWorkspace encode_workspace{*encode_owner};
    const auto unsupported =
        DecodeCompleteRecord(*encode_owner, encode_workspace, 0U, Bytes(expected_rx), nullptr, 0U);
    ok &= Expect(unsupported.status == CodecStatus::OPERATION_NOT_SUPPORTED,
                 "literal-only missing Decode action is unsupported");
  }

  const std::string legacy_binary_empty_fields = R"JSON({
"schema_version":"0.1","protocol_id":"binary_empty","protocol_version":"1",
"display_name":"Binary empty","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:BINARY_EMPTY",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:BINARY_EMPTY","input_kind":"complete_record"}],
"pipelines":[{"id":"pipe","display_name":"Pipe","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:BINARY_EMPTY","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:BINARY_EMPTY","direction_id":"test","frame_length_bytes":1,
"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},"fields":[]}]})JSON";
  const auto legacy = CompileJsonToPlan(legacy_binary_empty_fields);
  ok &= Expect(!legacy.Succeeded() && legacy.Diagnostic() != nullptr &&
                   legacy.Diagnostic()->code == CompileError::EMPTY_ARRAY &&
                   legacy.Diagnostic()->json_pointer == "/messages/0/fields",
               "legacy Binary empty fields remain rejected by Compiler");
  return ok;
}

bool CheckRuntimeUniqueness() {
  const std::string config = R"JSON({
"schema_version":"0.10","protocol_id":"ascii_ambiguous","protocol_version":"1",
"display_name":"ASCII ambiguous","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","input_kind":"complete_record"}],
"pipelines":[{"id":"pipe","display_name":"Pipe","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","direction_id":"test","input_framing_profile_id":"record","message_ids":["first","second"]}],
"messages":[
{"id":"first","display_name":"First","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"X"},{"kind":"field","field_id":"value"}]}},"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","value_type":"BYTES","wire":{"codec":"ascii_text","min_byte_length":1,"max_byte_length":1}}]},
{"id":"second","display_name":"Second","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"X"},{"kind":"field","field_id":"value"}]}},"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_TEST","value_type":"BYTES","wire":{"codec":"ascii_text","min_byte_length":1,"max_byte_length":1}}]}
]})JSON";
  auto compiled = CompileJsonToPlan(config);
  if (!Expect(compiled.Succeeded(), "runtime ambiguity config compiles")) return false;
  auto owner = std::move(compiled).TakePlan();
  ExecutionWorkspace workspace{*owner};
  DecodedFieldSlot slot;
  auto ambiguous = DecodeCompleteRecord(*owner, workspace, 0U, Bytes("XA"), &slot, 1U);
  bool ok =
      Expect(ambiguous.status == CodecStatus::AMBIGUOUS_MESSAGE && ambiguous.field_count == 0U,
             "multiple structural text candidates remain ambiguous");
  auto unknown = DecodeCompleteRecord(*owner, workspace, 0U, Bytes("YA"), &slot, 1U);
  ok &= Expect(unknown.status == CodecStatus::UNKNOWN_MESSAGE && unknown.field_count == 0U,
               "zero text candidates report unknown");
  return ok;
}

bool CheckCompilerFailures() {
  bool ok = true;
  const std::string wire =
      R"JSON({"codec":"ascii_text","min_byte_length":0,"max_byte_length":4})JSON";
  const std::string adjacent =
      R"JSON("decode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"field","field_id":"value"}]})JSON";
  auto duplicate = CompileJsonToPlan(OneFieldConfig(adjacent, wire, false));
  ok &= Expect(!duplicate.Succeeded() && duplicate.Diagnostic() != nullptr &&
                   duplicate.Diagnostic()->code == CompileError::ASCII_FIELD_BOUNDARY_AMBIGUOUS,
               "ambiguous adjacent variable field rejected at compile time");

  const std::string action =
      R"JSON("decode":{"segments":[{"kind":"field","field_id":"value"}]})JSON";
  auto bad_length = CompileJsonToPlan(OneFieldConfig(
      action, R"JSON({"codec":"ascii_text","min_byte_length":5,"max_byte_length":4})JSON", false));
  ok &= Expect(!bad_length.Succeeded() && bad_length.Diagnostic() != nullptr &&
                   bad_length.Diagnostic()->code == CompileError::ASCII_FIELD_LENGTH_INVALID,
               "invalid text length rejected");
  auto bad_control = CompileJsonToPlan(OneFieldConfig(
      action,
      R"JSON({"codec":"ascii_text","min_byte_length":0,"max_byte_length":4,"allowed_control_bytes":"0A 09"})JSON",
      false));
  ok &= Expect(!bad_control.Succeeded() && bad_control.Diagnostic() != nullptr &&
                   bad_control.Diagnostic()->code == CompileError::ASCII_CONTROL_BYTE_INVALID,
               "non-canonical control list rejected");
  auto oversized = CompileJsonToPlan(OneFieldConfig(
      action, R"JSON({"codec":"ascii_text","min_byte_length":0,"max_byte_length":70000})JSON",
      false));
  ok &= Expect(!oversized.Succeeded() && oversized.Diagnostic() != nullptr &&
                   oversized.Diagnostic()->code == CompileError::RESOURCE_LIMIT_EXCEEDED,
               "existing frame resource limit retained");
  return ok;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: ascii_text_contract_tests <config> <literal-only-config>\n";
    return 2;
  }
  const bool ok = CheckPrimaryVector(argv[1]) && CheckBoundariesAndActions() &&
                  CheckLiteralOnlyActions(argv[2]) && CheckRuntimeUniqueness() &&
                  CheckCompilerFailures();
  return ok ? 0 : 1;
}
