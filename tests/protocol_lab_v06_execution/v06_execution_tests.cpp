#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include "sha256.h"
#include "v06_execution.h"

namespace {

using pae::protocol_lab::HashBytes;
using pae::protocol_lab::v06::ExecutionBridge;
using pae::protocol_lab::v06::ExecutionOutcome;
using pae::protocol_lab::v06::ExecutionStage;
using pae::protocol_lab::v06::ExecutionTestHooks;
using pae::protocol_lab::v06::MaterializationFailure;
using pae::protocol_lab::v06::ParsedValue;
using pae::protocol_lab::v06::ParsedValues;
using pae::protocol_lab::v06::PreparationFailure;
using pae::protocol_lab::v06::Result;

bool Expect(bool condition, std::string_view message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::unique_ptr<ExecutionBridge> Prepare(std::string_view config) {
  PreparationFailure failure;
  auto bridge = ExecutionBridge::Prepare(config, failure);
  if (!bridge) {
    std::cerr << "prepare failure " << failure.diagnostic_id << ": " << failure.detail << '\n';
  }
  return bridge;
}

ParsedValue Decimal(std::string id, std::int64_t coefficient, std::int32_t scale) {
  ParsedValue value;
  value.id = std::move(id);
  value.kind = "DECIMAL64";
  value.decimal64_value = {coefficient, scale};
  return value;
}

ParsedValue Uint(std::string id, std::uint64_t raw) {
  ParsedValue value;
  value.id = std::move(id);
  value.kind = "UINT64";
  value.uint64_value = raw;
  return value;
}

ParsedValue Int(std::string id, std::int64_t raw) {
  ParsedValue value;
  value.id = std::move(id);
  value.kind = "INT64";
  value.int64_value = raw;
  return value;
}

ParsedValues ValidDecimalValues() {
  ParsedValues values;
  values.pipeline_id = "sample_pipeline";
  values.message_id = "sample";
  values.fields.push_back(Decimal("temperature", 123, 1));
  values.fields.push_back(Decimal("cancelled", (std::numeric_limits<std::int64_t>::max)(), 0));
  values.fields.push_back(Decimal("overflow_probe", 0, 0));
  values.fields.push_back(
      Decimal("signed_identity", (std::numeric_limits<std::int64_t>::max)(), 0));
  return values;
}

const std::array<std::uint8_t, 34>& ExpectedDecimalFrame() {
  static const std::array<std::uint8_t, 34> frame{
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0B, 0xFF, 0xFF, 0xFF, 0xFF,
      0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x2A};
  return frame;
}

bool IsValidResult(const ExecutionOutcome& outcome) {
  if (!outcome.result.has_value()) return false;
  std::string error;
  if (!pae::protocol_lab::v06::ValidateResult(*outcome.result, error)) return false;
  std::string serialized = pae::protocol_lab::v06::SerializeResult(*outcome.result, error);
  Result reparsed;
  return error.empty() && !serialized.empty() &&
         pae::protocol_lab::v06::ParseResult(serialized, reparsed, error);
}

bool IsPreparationFailure(const ExecutionOutcome& outcome, std::string_view diagnostic,
                          std::optional<std::size_t> value_index = std::nullopt) {
  return outcome.stage == ExecutionStage::PREPARATION && !outcome.main_codec_called &&
         !outcome.result.has_value() && outcome.encoded_frame.empty() &&
         outcome.counts.encode_calls == 0U && outcome.counts.decode_calls == 0U &&
         outcome.counts.review_decode_calls == 0U &&
         outcome.preparation_failure.diagnostic_id == diagnostic &&
         outcome.preparation_failure.value_index == value_index;
}

bool TestSuccessAndLifetime(std::string_view config) {
  bool passed = true;
  auto bridge = Prepare(config);
  if (!Expect(bridge != nullptr, "Schema 0.5 C1 bridge prepares")) return false;
  const auto& expected_array = ExpectedDecimalFrame();
  const std::vector<std::uint8_t> expected{expected_array.begin(), expected_array.end()};

  const ExecutionOutcome inspected = bridge->Inspect(expected);
  passed = Expect(inspected.stage == ExecutionStage::CODEC && inspected.main_codec_called &&
                      inspected.main_codec_status == "OK" &&
                      inspected.counts.structural_query_calls == 1U &&
                      inspected.counts.decode_calls == 1U && inspected.counts.encode_calls == 0U &&
                      IsValidResult(inspected),
                  "unique Inspect performs one structural query and one actual Decode") &&
           passed;
  if (!inspected.result.has_value()) return false;
  if (inspected.result.has_value()) {
    const Result& result = *inspected.result;
    passed =
        Expect(result.config_sha256 == HashBytes(config) &&
                   result.protocol_id == "decimal_core_contract" &&
                   result.pipeline_id == "sample_pipeline" && result.message_id == "sample" &&
                   result.operation_status == "OK" && result.exit_code == 0 &&
                   result.replay_mode == "DECODE_RX" && result.replay_subject == "RX" &&
                   result.current_execution_status == "OK" && result.fields.size() == 5U &&
                   result.fields[0].id == "temperature" && result.fields[0].kind == "DECIMAL64" &&
                   result.fields[0].decimal64->coefficient == 123 &&
                   result.fields[0].decimal64->scale == 1 && result.fields[0].raw_kind == "INT64" &&
                   result.fields[0].raw_value == "523" && result.fields[1].raw_kind == "UINT64" &&
                   result.fields[1].raw_value == "18446744073709551615" &&
                   result.fields[3].raw_value == "-9223372036854775808" &&
                   result.fields[4].kind == "UINT64" && result.fields[4].raw_value == "165",
               "Inspect copies normalized Decimal, exact raw values and inherited fields") &&
        passed;
  }

  const ExecutionOutcome encoded = bridge->EncodeParsed(ValidDecimalValues());
  passed =
      Expect(encoded.stage == ExecutionStage::CODEC && encoded.main_codec_called &&
                 encoded.main_codec_status == "OK" && encoded.review_decode_called &&
                 encoded.review_decode_status == "OK" && encoded.counts.encode_calls == 1U &&
                 encoded.counts.review_decode_calls == 1U && encoded.counts.decode_calls == 0U &&
                 encoded.encoded_frame == expected && IsValidResult(encoded),
             "Encode uses one main Encode and one independent display Decode") &&
      passed;
  if (encoded.result.has_value()) {
    passed =
        Expect(encoded.result->operation_status == "OK" &&
                   encoded.result->replay_mode == "ENCODE_TX" &&
                   encoded.result->replay_subject == "TX" && encoded.result->fields.size() == 5U &&
                   encoded.result->fields[0].raw_value == "523",
               "Encode Result is based on reviewed output bytes") &&
        passed;
  }

  ParsedValues reversed = ValidDecimalValues();
  std::reverse(reversed.fields.begin(), reversed.fields.end());
  const ExecutionOutcome reordered = bridge->EncodeParsed(reversed);
  passed = Expect(reordered.result.has_value() && reordered.encoded_frame == expected &&
                      reordered.result->fields[0].id == "temperature",
                  "author input order does not change frozen field result order") &&
           passed;

  ParsedValues equivalent = ValidDecimalValues();
  equivalent.fields[0].decimal64_value = {1230, 2};
  const ExecutionOutcome normalized = bridge->EncodeParsed(equivalent);
  passed = Expect(normalized.encoded_frame == expected && normalized.result.has_value() &&
                      normalized.result->fields.size() == 5U &&
                      normalized.result->fields[0].decimal64.has_value() &&
                      normalized.result->fields[0].decimal64->coefficient == 123 &&
                      normalized.result->fields[0].decimal64->scale == 1 &&
                      normalized.result->fields[0].raw_value == "523",
                  "equivalent Decimal input produces the same bytes and normalized result") &&
           passed;

  const Result retained = *inspected.result;
  bridge.reset();
  passed = Expect(retained.fields[0].id == "temperature" && retained.fields[0].raw_value == "523" &&
                      retained.frame_hex == inspected.result->frame_hex,
                  "Lab Result owns strings and values after Plan and Workspace destruction") &&
           passed;
  return passed;
}

std::string NoConversionConfig() {
  return R"json({
    "schema_version":"0.5","protocol_id":"c1_types","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","input_kind":"complete_record"}],
    "pipelines":[{"id":"p","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx",
      "input_framing_profile_id":"record","message_ids":["m"]}],
    "messages":[{"id":"m","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx",
      "frame_length_bytes":6,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":6}]},
      "bit_containers":[{"id":"flags","container_offset":3,"container_width":1,
        "bit_numbering":"lsb0","base_value":0}],
      "fields":[
        {"id":"mode","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
          "value_type":"ENUM","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},
          "encode":{"source":"input"},"unknown_enum_policy":"reject",
          "enum_entries":[{"id":"active","display_name":"Active","raw_value":2}]},
        {"id":"signed","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
          "value_type":"INT64","wire":{"codec":"unsigned_integer","byte_offset":1,"byte_width":1},
          "encode":{"source":"input"}},
        {"id":"unsigned","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
          "value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":2,"byte_width":1},
          "encode":{"source":"input"}},
        {"id":"enabled","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
          "value_type":"BOOL","wire":{"codec":"bitfield","container_id":"flags","bit_offset":0,"bit_width":1},
          "encode":{"source":"input"}},
        {"id":"payload","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
          "value_type":"BYTES","wire":{"codec":"bytes","byte_offset":4,"byte_length":2},
          "encode":{"source":"input"}}
      ]}]
  })json";
}

std::string LegacyValuesConfig() {
  return R"json({
    "schema_version":"0.5","protocol_id":"c1_legacy_values","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","input_kind":"complete_record"}],
    "pipelines":[{"id":"p","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx",
      "input_framing_profile_id":"record","message_ids":["m"]}],
    "messages":[{"id":"m","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx",
      "frame_length_bytes":4,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":4}]},
      "fields":[
        {"id":"value","display_name":"Synthetic","description":"",
          "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","value_type":"UINT64",
          "wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},
          "encode":{"source":"input"}},
        {"id":"payload","display_name":"Synthetic","description":"",
          "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","value_type":"BYTES",
          "wire":{"codec":"bytes","byte_offset":1,"byte_length":2},
          "encode":{"source":"input"}},
        {"id":"mode","display_name":"Synthetic","description":"",
          "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","value_type":"ENUM",
          "wire":{"codec":"unsigned_integer","byte_offset":3,"byte_width":1},
          "encode":{"source":"input"},"unknown_enum_policy":"reject",
          "enum_entries":[{"id":"active","display_name":"Active","raw_value":2}]}
      ]}]
  })json";
}

std::string LegacyBoolValuesConfig() {
  return R"json({
    "schema_version":"0.5","protocol_id":"c1_legacy_bool","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","input_kind":"complete_record"}],
    "pipelines":[{"id":"p","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx",
      "input_framing_profile_id":"record","message_ids":["m"]}],
    "messages":[{"id":"m","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx",
      "frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},
      "bit_containers":[{"id":"flags","container_offset":0,"container_width":1,
        "bit_numbering":"lsb0","base_value":0}],
      "fields":[{"id":"enabled","display_name":"Synthetic","description":"",
        "source_ref":"SYNTHETIC_FROM_SCRATCH:C1","value_type":"BOOL",
        "wire":{"codec":"bitfield","container_id":"flags","bit_offset":0,"bit_width":1},
        "encode":{"source":"input"}}]}]
  })json";
}

std::string LegacyUintValues(std::string_view format, std::string_view value = "42") {
  return std::string{"{\"format_version\":\""} + std::string{format} +
         "\",\"pipeline_id\":\"p\",\"message_id\":\"m\",\"fields\":[{\"id\":\"value\","
         "\"kind\":\"UINT64\",\"uint64\":\"" +
         std::string{value} + "\"}]}";
}

std::string LegacyCommonValues(std::string_view format) {
  return std::string{"{\"format_version\":\""} + std::string{format} +
         "\",\"pipeline_id\":\"p\",\"message_id\":\"m\",\"fields\":["
         "{\"id\":\"value\",\"kind\":\"UINT64\",\"uint64\":\"42\"},"
         "{\"id\":\"payload\",\"kind\":\"BYTES\",\"hex\":\"AABB\"},"
         "{\"id\":\"mode\",\"kind\":\"ENUM\",\"entry_id\":\"active\"}]}";
}

bool TestLegacyValuesCompatibility() {
  auto bridge = Prepare(LegacyValuesConfig());
  if (!Expect(bridge != nullptr, "legacy Values compatibility fixture prepares")) return false;
  bool passed = true;
  for (const std::string_view format :
       {"pae.lab.values/0.1", "pae.lab.values/0.2", "pae.lab.values/0.3"}) {
    const ExecutionOutcome encoded = bridge->EncodeValuesText(LegacyCommonValues(format));
    passed =
        Expect(
            encoded.result.has_value() &&
                encoded.encoded_frame == std::vector<std::uint8_t>{0x2AU, 0xAAU, 0xBBU, 0x02U} &&
                encoded.counts.encode_calls == 1U && encoded.counts.review_decode_calls == 1U &&
                IsValidResult(encoded),
            std::string{"legacy Values must execute under Schema 0.5: "} + std::string{format}) &&
        passed;
  }

  auto bool_bridge = Prepare(LegacyBoolValuesConfig());
  if (!Expect(bool_bridge != nullptr, "legacy BOOL compatibility fixture prepares")) return false;
  for (const std::string_view format : {"pae.lab.values/0.2", "pae.lab.values/0.3"}) {
    const std::string values = std::string{"{\"format_version\":\""} + std::string{format} +
                               "\",\"pipeline_id\":\"p\",\"message_id\":\"m\","
                               "\"fields\":[{\"id\":\"enabled\",\"kind\":\"BOOL\","
                               "\"bool\":true}]}";
    const ExecutionOutcome encoded = bool_bridge->EncodeValuesText(values);
    passed = Expect(encoded.result.has_value() &&
                        encoded.encoded_frame == std::vector<std::uint8_t>{0x01U} &&
                        encoded.counts.encode_calls == 1U && IsValidResult(encoded),
                    std::string{"legacy BOOL executes in its supported generation: "} +
                        std::string{format}) &&
             passed;
  }

  const auto expect_preparation_detail = [&](std::string text, std::string_view detail,
                                             std::string_view name) {
    const ExecutionOutcome failed = bridge->EncodeValuesText(std::move(text));
    return Expect(IsPreparationFailure(failed, "PAE_LAB_C1_VALUES_INVALID") &&
                      failed.preparation_failure.detail == detail,
                  name);
  };
  passed =
      expect_preparation_detail(
          R"json({"format_version":"pae.lab.values/0.1","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"BOOL","bool":true}]})json",
          "fields[0].bool must be a native JSON boolean in Values 0.2",
          "Values 0.1 rejects the Values 0.2 BOOL type before Core") &&
      passed;
  passed =
      expect_preparation_detail(
          R"json({"format_version":"pae.lab.values/0.2","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"INT64","int64":"1"}]})json",
          "fields[0].int64 is not canonical INT64 decimal in Values 0.3",
          "Values 0.2 rejects the Values 0.3 INT64 type before Core") &&
      passed;
  for (const std::string_view format :
       {"pae.lab.values/0.1", "pae.lab.values/0.2", "pae.lab.values/0.3"}) {
    const std::string decimal =
        std::string{"{\"format_version\":\""} + std::string{format} +
        "\",\"pipeline_id\":\"p\",\"message_id\":\"m\",\"fields\":[{\"id\":\"value\","
        "\"kind\":\"DECIMAL64\",\"decimal64\":{\"coefficient\":\"1\",\"scale\":0}}]}";
    passed =
        expect_preparation_detail(
            decimal, "fields[0] contains unknown property decimal64",
            std::string{"legacy Values rejects DECIMAL64 before Core: "} + std::string{format}) &&
        passed;
  }
  passed = expect_preparation_detail(LegacyUintValues("pae.lab.values/0.9"),
                                     "unsupported Values format_version",
                                     "unknown Values generation fails closed before Core") &&
           passed;
  passed =
      expect_preparation_detail(
          R"json({"format_version":"pae.lab.values/0.1","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"UINT64","uint64":42}]})json",
          "uint64 must be a string", "legacy UINT64 rejects a JSON Number before Core") &&
      passed;
  passed =
      expect_preparation_detail(
          R"json({"format_version":"pae.lab.values/0.1","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"UINT64","uint64":"1"},{"id":"value","kind":"UINT64","uint64":"2"}]})json",
          "fields contains duplicate id value",
          "legacy duplicate field ids fail strict parsing before Core") &&
      passed;
  return passed;
}

bool TestInheritedTypeMapping() {
  auto bridge = Prepare(NoConversionConfig());
  if (!Expect(bridge != nullptr, "Schema 0.5 no-conversion bridge prepares")) return false;
  const std::string values = R"json({"format_version":"pae.lab.values/0.4","pipeline_id":"p",
    "message_id":"m","fields":[
      {"id":"payload","kind":"BYTES","hex":"AABB"},
      {"id":"enabled","kind":"BOOL","bool":true},
      {"id":"unsigned","kind":"UINT64","uint64":"255"},
      {"id":"signed","kind":"INT64","int64":"-128"},
      {"id":"mode","kind":"ENUM","entry_id":"active"}]})json";
  const std::vector<std::uint8_t> expected{0x02U, 0x80U, 0xFFU, 0x01U, 0xAAU, 0xBBU};
  const ExecutionOutcome encoded = bridge->EncodeValuesText(values);
  bool passed = Expect(
      encoded.encoded_frame == expected && encoded.result.has_value() &&
          encoded.result->fields.size() == 5U && encoded.result->fields[0].kind == "ENUM" &&
          encoded.result->fields[0].logical_value == "active" &&
          encoded.result->fields[1].kind == "INT64" &&
          encoded.result->fields[1].raw_value == "-128" &&
          encoded.result->fields[2].kind == "UINT64" &&
          encoded.result->fields[2].raw_value == "255" &&
          encoded.result->fields[3].kind == "BOOL" && encoded.result->fields[4].kind == "BYTES" &&
          encoded.result->fields[4].raw_value == "AABB",
      "all inherited value kinds map without string or numeric coercion");
  std::string legacy_values = values;
  const std::string v4 = "pae.lab.values/0.4";
  const std::string v3 = "pae.lab.values/0.3";
  legacy_values.replace(legacy_values.find(v4), v4.size(), v3);
  const ExecutionOutcome legacy_encoded = bridge->EncodeValuesText(legacy_values);
  passed =
      Expect(legacy_encoded.encoded_frame == expected && legacy_encoded.result.has_value() &&
                 legacy_encoded.result->fields[1].kind == "INT64" &&
                 legacy_encoded.result->fields[3].kind == "BOOL" && IsValidResult(legacy_encoded),
             "Values 0.3 preserves its INT64 and BOOL capability under Schema 0.5") &&
      passed;
  const ExecutionOutcome decoded = bridge->Inspect(expected);
  passed = Expect(decoded.result.has_value() && decoded.result->fields.size() == 5U &&
                      decoded.result->fields[1].raw_value == "-128" &&
                      decoded.result->fields[2].raw_value == "255",
                  "Schema 0.5 without conversion still produces a valid Result 0.6") &&
           passed;
  const std::string unknown_enum = R"json({"format_version":"pae.lab.values/0.4",
    "pipeline_id":"p","message_id":"m","fields":[
      {"id":"mode","kind":"ENUM","entry_id":"missing"}]})json";
  passed = Expect(IsPreparationFailure(bridge->EncodeValuesText(unknown_enum),
                                       "PAE_LAB_C1_UNKNOWN_ENUM_ENTRY", 0U),
                  "unknown Enum entry fails binding before Core") &&
           passed;
  return passed;
}

bool TestPreparationAndCorePriority(std::string_view config) {
  bool passed = true;
  PreparationFailure config_failure;
  auto invalid = ExecutionBridge::Prepare("{}", config_failure);
  passed = Expect(!invalid && config_failure.diagnostic_id == "PAE_LAB_C1_CONFIG_INVALID",
                  "configuration failure is preparation-only") &&
           passed;

  PreparationFailure schema_failure;
  auto old = ExecutionBridge::Prepare(
      R"json({"schema_version":"0.1","protocol_id":"old","protocol_version":"1",
      "display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
      "resource_profile":"desktop","framing_profiles":[{"id":"r","display_name":"S",
      "description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","input_kind":"complete_record"}],
      "pipelines":[{"id":"p","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
      "direction_id":"rx","input_framing_profile_id":"r","message_ids":["m"]}],
      "messages":[{"id":"m","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
      "direction_id":"rx","frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},
      "fields":[{"id":"x","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1",
      "value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},
      "encode":{"source":"input"}}]}]})json",
      schema_failure);
  passed = Expect(!old && schema_failure.diagnostic_id == "PAE_LAB_C1_SCHEMA_UNSUPPORTED",
                  "old Schema is compiled but rejected before C1 execution") &&
           passed;

  auto bridge = Prepare(config);
  if (!Expect(bridge != nullptr, "priority fixture prepares")) return false;
  passed = Expect(IsPreparationFailure(bridge->EncodeValuesText("{}"), "PAE_LAB_C1_VALUES_INVALID"),
                  "Values syntax failure does not call Core") &&
           passed;
  ParsedValues unknown = ValidDecimalValues();
  unknown.fields.back().id = "unknown";
  passed =
      Expect(IsPreparationFailure(bridge->EncodeParsed(unknown), "PAE_LAB_C1_UNKNOWN_FIELD", 3U),
             "unknown Field binding fails before Core with author index") &&
      passed;

  ParsedValues enum_type_mismatch = ValidDecimalValues();
  enum_type_mismatch.fields[0].kind = "ENUM";
  enum_type_mismatch.fields[0].enum_entry_id = "not_applicable";
  const ExecutionOutcome enum_type_failure = bridge->EncodeParsed(enum_type_mismatch);
  passed = Expect(enum_type_failure.main_codec_status == "TYPE_MISMATCH" &&
                      enum_type_failure.counts.encode_calls == 1U &&
                      enum_type_failure.result.has_value() &&
                      enum_type_failure.result->failed_field_id == "temperature" &&
                      enum_type_failure.result->failed_value_index == 0U,
                  "Enum kind for a non-Enum field is deferred to Core type checking") &&
           passed;

  ParsedValues constant = ValidDecimalValues();
  constant.fields.push_back(Uint("legacy_constant", 165U));
  const ExecutionOutcome constant_failure = bridge->EncodeParsed(constant);
  passed = Expect(constant_failure.main_codec_status == "CONSTANT_FIELD_OVERRIDE" &&
                      constant_failure.counts.encode_calls == 1U &&
                      constant_failure.counts.review_decode_calls == 0U &&
                      constant_failure.result.has_value() &&
                      constant_failure.result->failed_value_index == 4U &&
                      constant_failure.result->failed_field_id == "legacy_constant" &&
                      constant_failure.result->fields.empty() && IsValidResult(constant_failure),
                  "constant override is reported by Core, not bridge preparation") &&
           passed;

  ParsedValues duplicate = ValidDecimalValues();
  duplicate.fields[0] = Uint("temperature", 1U);
  duplicate.fields.push_back(duplicate.fields[1]);
  const ExecutionOutcome duplicate_failure = bridge->EncodeParsed(duplicate);
  passed = Expect(duplicate_failure.main_codec_status == "DUPLICATE_FIELD" &&
                      duplicate_failure.result.has_value() &&
                      duplicate_failure.result->failed_value_index == 4U,
                  "later duplicate precedes earlier deferred type mismatch in Core") &&
           passed;

  ParsedValues missing = ValidDecimalValues();
  missing.fields[0] = Uint("temperature", 1U);
  missing.fields.erase(missing.fields.begin() + 1);
  const ExecutionOutcome missing_failure = bridge->EncodeParsed(missing);
  passed = Expect(missing_failure.main_codec_status == "MISSING_FIELD" &&
                      missing_failure.result.has_value() &&
                      missing_failure.result->failed_field_id == "cancelled",
                  "missing required field precedes deferred type mismatch in Core") &&
           passed;

  ParsedValues invalid_scale = ValidDecimalValues();
  invalid_scale.fields[0].decimal64_value = {1, 19};
  const ExecutionOutcome scale_failure = bridge->EncodeParsed(invalid_scale);
  passed =
      Expect(scale_failure.main_codec_status == "INVALID_ARGUMENT" &&
                 scale_failure.result.has_value() &&
                 scale_failure.result->conversion_error == "DECIMAL_SCALE_OUT_OF_RANGE" &&
                 scale_failure.result->failed_field_id == "temperature" &&
                 scale_failure.result->failed_value_index == 0U && IsValidResult(scale_failure),
             "typed invalid scale maps the actual Core status and conversion reason") &&
      passed;
  invalid_scale.fields[0].decimal64_value = {1, -1};
  const ExecutionOutcome negative_scale_failure = bridge->EncodeParsed(invalid_scale);
  passed =
      Expect(negative_scale_failure.main_codec_status == "INVALID_ARGUMENT" &&
                 negative_scale_failure.result.has_value() &&
                 negative_scale_failure.result->conversion_error == "DECIMAL_SCALE_OUT_OF_RANGE" &&
                 negative_scale_failure.result->failed_field_id == "temperature" &&
                 negative_scale_failure.result->failed_value_index == 0U &&
                 IsValidResult(negative_scale_failure),
             "negative Decimal scale maps the actual Core invalid-argument result") &&
      passed;

  ParsedValues raw_overflow = ValidDecimalValues();
  raw_overflow.fields[0].decimal64_value = {(std::numeric_limits<std::int64_t>::max)(), 0};
  const ExecutionOutcome raw_overflow_failure = bridge->EncodeParsed(raw_overflow);
  passed = Expect(raw_overflow_failure.main_codec_status == "VALUE_NOT_REPRESENTABLE" &&
                      raw_overflow_failure.result.has_value() &&
                      raw_overflow_failure.result->conversion_error == "RAW_OUT_OF_RANGE" &&
                      raw_overflow_failure.result->failed_field_id == "temperature" &&
                      raw_overflow_failure.result->failed_value_index == 0U &&
                      IsValidResult(raw_overflow_failure),
                  "inverse result outside Wire range maps the exact Core conversion reason") &&
           passed;
  return passed;
}

std::string AmbiguousConfig(bool reverse) {
  const std::string first = reverse ? "m2" : "m1";
  const std::string second = reverse ? "m1" : "m2";
  return std::string{
             R"json({"schema_version":"0.5","protocol_id":"ambiguous","protocol_version":"1",
    "display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","resource_profile":"desktop",
    "framing_profiles":[{"id":"r","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","input_kind":"complete_record"}],
    "pipelines":[{"id":"p1","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx","input_framing_profile_id":"r","message_ids":[")json"} +
         first +
         R"json("]},{"id":"p2","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx","input_framing_profile_id":"r","message_ids":[")json" +
         second +
         R"json("]}],"messages":[
      {"id":"m1","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx","frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},"fields":[{"id":"a","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},"encode":{"source":"input"}}]},
      {"id":"m2","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","direction_id":"rx","frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},"fields":[{"id":"b","display_name":"S","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C1","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},"encode":{"source":"input"}}]}]})json";
}

bool TestStructuralAndCodecFailures(std::string_view config) {
  bool passed = true;
  auto bridge = Prepare(config);
  if (!Expect(bridge != nullptr, "structural fixture prepares")) return false;
  const ExecutionOutcome zero = bridge->Inspect(std::vector<std::uint8_t>{0U});
  passed = Expect(zero.stage == ExecutionStage::STRUCTURAL_QUERY &&
                      zero.structural_status == "UNKNOWN_MESSAGE" &&
                      zero.counts.structural_query_calls == 1U && zero.counts.decode_calls == 0U &&
                      !zero.main_codec_called && !zero.result.has_value(),
                  "zero global candidates do not execute Decode or fabricate Result 0.6") &&
           passed;

  for (bool reverse : {false, true}) {
    auto ambiguous = Prepare(AmbiguousConfig(reverse));
    if (!Expect(ambiguous != nullptr, "cross-Pipeline ambiguity fixture prepares")) return false;
    const ExecutionOutcome multiple = ambiguous->Inspect(std::vector<std::uint8_t>{0x11U});
    passed =
        Expect(
            multiple.stage == ExecutionStage::STRUCTURAL_QUERY &&
                multiple.structural_status == "AMBIGUOUS_MESSAGE" &&
                multiple.counts.structural_query_calls == 2U &&
                multiple.counts.decode_calls == 0U && !multiple.main_codec_called &&
                !multiple.result.has_value(),
            "cross-Pipeline ambiguity is independent of Pipeline order and performs no Decode") &&
        passed;
  }

  const auto& expected_array = ExpectedDecimalFrame();
  std::vector<std::uint8_t> integrity_and_conversion_error{expected_array.begin(),
                                                           expected_array.end()};
  for (std::size_t index = 16U; index < 24U; ++index) {
    integrity_and_conversion_error[index] = 0xFFU;
  }
  const ExecutionOutcome integrity = bridge->Inspect(integrity_and_conversion_error);
  passed = Expect(integrity.main_codec_status == "INTEGRITY_FAILED" &&
                      integrity.counts.decode_calls == 1U && integrity.result.has_value() &&
                      integrity.result->fields.empty() &&
                      !integrity.result->conversion_error.has_value() && IsValidResult(integrity),
                  "SUM8 failure precedes Decimal conversion and delivers no fields") &&
           passed;

  integrity_and_conversion_error[33] = 0x22U;
  const ExecutionOutcome conversion = bridge->Inspect(integrity_and_conversion_error);
  passed = Expect(conversion.main_codec_status == "VALUE_NOT_REPRESENTABLE" &&
                      conversion.result.has_value() &&
                      conversion.result->conversion_error == "LOGICAL_OUT_OF_RANGE" &&
                      conversion.result->failed_field_id == "overflow_probe" &&
                      conversion.result->failed_field_index == 2U &&
                      conversion.result->fields.empty() && IsValidResult(conversion),
                  "valid SUM8 exposes the actual Decimal logical overflow identity") &&
           passed;
  return passed;
}

bool TestReviewFailuresAndReuse(std::string_view config) {
  bool passed = true;
  auto bridge = Prepare(config);
  if (!Expect(bridge != nullptr, "review fixture prepares")) return false;
  const ParsedValues values = ValidDecimalValues();
  const ExecutionOutcome successful = bridge->EncodeParsed(values);
  if (!Expect(successful.result.has_value(), "review baseline succeeds")) return false;
  const Result retained = *successful.result;

  struct Case {
    ExecutionTestHooks hooks;
    MaterializationFailure expected;
    const char* expected_review_status;
    const char* name;
  };
  std::array<Case, 5> cases{};
  cases[0].hooks.fail_review_decode = true;
  cases[0].expected = MaterializationFailure::REVIEW_DECODE_FAILED;
  cases[0].expected_review_status = "INVALID_ARGUMENT";
  cases[0].name = "actual display Decode failure";
  cases[1].hooks.fail_review_decimal_conversion_with_internal_error = true;
  cases[1].expected = MaterializationFailure::REVIEW_DECODE_FAILED;
  cases[1].expected_review_status = "INTERNAL_ERROR";
  cases[1].name = "display Decode internal arithmetic failure";
  cases[2].hooks.force_review_message_mismatch = true;
  cases[2].expected = MaterializationFailure::REVIEW_MESSAGE_MISMATCH;
  cases[2].expected_review_status = "OK";
  cases[2].name = "display Message mismatch";
  cases[3].hooks.force_raw_association_failure = true;
  cases[3].expected = MaterializationFailure::RAW_ASSOCIATION_FAILED;
  cases[3].expected_review_status = "OK";
  cases[3].name = "raw FieldRef association failure";
  cases[4].hooks.force_materialization_internal_error = true;
  cases[4].expected = MaterializationFailure::INTERNAL_ERROR;
  cases[4].expected_review_status = "OK";
  cases[4].name = "materialization internal failure";
  for (const Case& test_case : cases) {
    const ExecutionOutcome failed = bridge->EncodeParsed(values, &test_case.hooks);
    passed =
        Expect(failed.stage == ExecutionStage::MATERIALIZATION && failed.main_codec_called &&
                   failed.main_codec_status == "OK" && failed.review_decode_called &&
                   failed.review_decode_status == test_case.expected_review_status &&
                   failed.counts.encode_calls == 1U && failed.counts.review_decode_calls == 1U &&
                   failed.materialization_failure == test_case.expected &&
                   !failed.result.has_value() && failed.encoded_frame.empty(),
               test_case.name) &&
        passed;
  }
  const ExecutionOutcome recovered = bridge->EncodeParsed(values);
  passed = Expect(recovered.result.has_value() && recovered.main_codec_status == "OK" &&
                      retained.fields[0].raw_value == "523" &&
                      retained.frame_hex == successful.result->frame_hex,
                  "workspaces remain reusable and prior self-owned Result remains unchanged") &&
           passed;

  ParsedValues non_integral = values;
  non_integral.fields[0].decimal64_value = {1235, 2};
  const ExecutionOutcome main_failure = bridge->EncodeParsed(non_integral);
  passed = Expect(main_failure.main_codec_status == "VALUE_NOT_REPRESENTABLE" &&
                      main_failure.result.has_value() &&
                      main_failure.result->conversion_error == "RAW_NOT_INTEGRAL" &&
                      main_failure.result->failed_field_id == "temperature" &&
                      main_failure.result->failed_value_index == 0U &&
                      main_failure.counts.encode_calls == 1U &&
                      main_failure.counts.review_decode_calls == 0U &&
                      !main_failure.review_decode_called && main_failure.encoded_frame.empty(),
                  "main Encode failure never triggers the display Decode") &&
           passed;
  return passed;
}

}  // namespace

int main(int argc, char** argv) {
  if (!Expect(argc == 2, "decimal Core fixture path is required")) return 1;
  const std::string config = ReadText(argv[1]);
  bool passed = true;
  passed = TestSuccessAndLifetime(config) && passed;
  passed = TestInheritedTypeMapping() && passed;
  passed = TestLegacyValuesCompatibility() && passed;
  passed = TestPreparationAndCorePriority(config) && passed;
  passed = TestStructuralAndCodecFailures(config) && passed;
  passed = TestReviewFailuresAndReuse(config) && passed;
  return passed ? 0 : 1;
}
