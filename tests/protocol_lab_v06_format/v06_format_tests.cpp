#include <yyjson.h>

#include <iostream>
#include <limits>
#include <string>
#include <string_view>

#include "sha256.h"
#include "v06_format.h"

namespace {

using pae::protocol_lab::v06::Decimal64;
using pae::protocol_lab::v06::FieldResult;
using pae::protocol_lab::v06::ParsedValues;
using pae::protocol_lab::v06::Result;

bool Expect(bool condition, std::string_view message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

bool ExpectParse(std::string text, bool expected, std::string_view label) {
  ParsedValues values;
  std::string error;
  const bool parsed = pae::protocol_lab::v06::ParseValues(text, values, error);
  return Expect(parsed == expected, label) &&
         Expect(expected || !error.empty(), "rejected Values must provide a diagnostic");
}

std::string ValuesWithDecimal(std::string_view coefficient, std::string_view scale) {
  return "{\"format_version\":\"pae.lab.values/0.4\",\"pipeline_id\":\"p\","
         "\"message_id\":\"m\",\"fields\":[{\"id\":\"d\",\"kind\":\"DECIMAL64\","
         "\"decimal64\":{\"coefficient\":\"" +
         std::string{coefficient} + "\",\"scale\":" + std::string{scale} + "}}]}";
}

FieldResult DecimalField(std::int64_t coefficient, std::int32_t scale, std::string raw_kind,
                         std::string raw_value) {
  FieldResult field;
  field.id = u8"温\n|:";
  field.kind = "DECIMAL64";
  field.decimal64 = Decimal64{coefficient, scale};
  field.raw_kind = std::move(raw_kind);
  field.raw_value = std::move(raw_value);
  return field;
}

Result SuccessResult() {
  Result result;
  result.command = "encode";
  result.operation_kind = "encode";
  result.operation_status = "OK";
  result.config_sha256 = "ABC";
  result.protocol_id = "p|x";
  result.pipeline_id = "pipe";
  result.message_id = "m\n";
  result.direction_id = "TX";
  result.frame_hex = "";
  result.tx_frame_hex = std::nullopt;
  result.rx_frame_hex = "00FF";
  result.replay_mode = "ENCODE_TX";
  result.replay_subject = "TX";
  result.current_execution_status = "OK";
  result.fields.push_back(DecimalField(123, 1, "INT64", "-9223372036854775808"));
  FieldResult integer;
  integer.id = "u";
  integer.kind = "UINT64";
  integer.raw_value = "18446744073709551615";
  integer.logical_value = "18446744073709551615";
  result.fields.push_back(std::move(integer));
  return result;
}

bool TestValuesV04() {
  ParsedValues first;
  ParsedValues second;
  std::string error;
  std::string first_text = ValuesWithDecimal("123", "1");
  std::string second_text = ValuesWithDecimal("1230", "2");
  if (!Expect(pae::protocol_lab::v06::ParseValues(first_text, first, error),
              "canonical Decimal Values must parse") ||
      !Expect(pae::protocol_lab::v06::ParseValues(second_text, second, error),
              "equivalent Decimal Values must parse"))
    return false;
  if (!Expect(first.fields[0].decimal64_value.coefficient == 123 &&
                  first.fields[0].decimal64_value.scale == 1,
              "canonical Decimal value") ||
      !Expect(second.fields[0].decimal64_value.coefficient == 123 &&
                  second.fields[0].decimal64_value.scale == 1,
              "equivalent Decimal input normalizes without rewriting source text") ||
      !Expect(second_text.find("1230") != std::string::npos,
              "original Values text remains unchanged"))
    return false;
  Result first_result = SuccessResult();
  Result second_result = SuccessResult();
  first_result.fields[0].decimal64 = first.fields[0].decimal64_value;
  second_result.fields[0].decimal64 = second.fields[0].decimal64_value;
  if (!Expect(pae::protocol_lab::v06::FinalizeFingerprint(first_result, error) ==
                  pae::protocol_lab::v06::FinalizeFingerprint(second_result, error),
              "equivalent Values representations produce the same execution fingerprint") ||
      !Expect(pae::protocol_lab::HashBytes(first_text) != pae::protocol_lab::HashBytes(second_text),
              "equivalent Values retain different original file hashes"))
    return false;

  ParsedValues zero;
  std::string zero_text = ValuesWithDecimal("0", "18");
  if (!Expect(pae::protocol_lab::v06::ParseValues(zero_text, zero, error) &&
                  zero.fields[0].decimal64_value.coefficient == 0 &&
                  zero.fields[0].decimal64_value.scale == 0,
              "Decimal zero normalizes to coefficient 0 and scale 0"))
    return false;

  const std::string mixed =
      "{\"format_version\":\"pae.lab.values/0.4\",\"pipeline_id\":\"p\","
      "\"message_id\":\"m\",\"fields\":["
      "{\"id\":\"u\",\"kind\":\"UINT64\",\"uint64\":\"18446744073709551615\"},"
      "{\"id\":\"i\",\"kind\":\"INT64\",\"int64\":\"-9223372036854775808\"},"
      "{\"id\":\"b\",\"kind\":\"BOOL\",\"bool\":true},"
      "{\"id\":\"x\",\"kind\":\"BYTES\",\"hex\":\"00FF\"}]}";
  if (!ExpectParse(mixed, true, "Values 0.4 keeps old supported field types")) return false;

  for (const std::string invalid :
       {ValuesWithDecimal("-0", "0"), ValuesWithDecimal("+1", "0"), ValuesWithDecimal("01", "0"),
        ValuesWithDecimal("1.0", "0"), ValuesWithDecimal("1e1", "0"),
        ValuesWithDecimal("9223372036854775808", "0"), ValuesWithDecimal("1", "-1"),
        ValuesWithDecimal("1", "19"), ValuesWithDecimal("1", "1.0"),
        ValuesWithDecimal("1", "1e0")}) {
    if (!ExpectParse(invalid, false, "invalid Decimal64 lexical/range form")) return false;
  }
  if (!ExpectParse("{\"format_version\":\"pae.lab.values/0.4\",\"pipeline_id\":\"p\","
                   "\"message_id\":\"m\",\"fields\":[{\"id\":\"d\",\"kind\":\"DECIMAL64\","
                   "\"decimal64\":null}]}",
                   false, "Decimal64 rejects null") ||
      !ExpectParse("{\"format_version\":\"pae.lab.values/0.4\",\"pipeline_id\":\"p\","
                   "\"message_id\":\"m\",\"fields\":[{\"id\":\"d\",\"kind\":\"DECIMAL64\","
                   "\"decimal64\":{\"coefficient\":\"1\",\"scale\":0,\"extra\":0}}]}",
                   false, "Decimal64 rejects unknown properties"))
    return false;
  if (!ExpectParse("{\"format_version\":\"pae.lab.values/0.4\",\"pipeline_id\":\"p\","
                   "\"message_id\":\"m\",\"fields\":[{\"id\":\"d\",\"id\":\"d2\","
                   "\"kind\":\"DECIMAL64\","
                   "\"decimal64\":{\"coefficient\":\"1\",\"scale\":0}}]}",
                   false, "Values 0.4 rejects duplicate properties"))
    return false;
  return ExpectParse(
      "{\"format_version\":\"pae.lab.values/0.3\",\"pipeline_id\":\"p\","
      "\"message_id\":\"m\",\"fields\":[]}",
      false, "isolated Values parser accepts only 0.4");
}

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
bool TestValuesV05AndVariableEmptyResult() {
  const std::string prefix =
      "{\"pipeline_id\":\"p\",\"message_id\":\"m\",\"fields\":[{\"id\":\"payload\",";
  const std::string suffix = "}]}";
  const std::string empty_field = "\"kind\":\"BYTES\",\"hex\":\"\"";
  ParsedValues parsed_values;
  std::string error;
  std::string values =
      "{\"format_version\":\"pae.lab.values/0.5\"," + prefix.substr(1U) + empty_field + suffix;
  if (!Expect(pae::protocol_lab::v06::ParseValues(values, parsed_values, error) &&
                  parsed_values.format_version == "pae.lab.values/0.5" &&
                  parsed_values.fields.size() == 1U && parsed_values.fields[0].bytes.empty(),
              "Values 0.5 strictly represents explicit empty BYTES")) {
    return false;
  }
  std::string legacy_empty = values;
  legacy_empty.replace(legacy_empty.find("0.5"), 3U, "0.4");
  std::string null_value = values;
  null_value.replace(null_value.find("\"\"", null_value.find("\"hex\"")), 2U, "null");
  std::string missing_value = values;
  missing_value.erase(missing_value.find(",\"hex\":\"\""), 9U);
  if (!ExpectParse(legacy_empty, false, "Values 0.4 keeps rejecting empty BYTES") ||
      !ExpectParse(null_value, false, "Values 0.5 distinguishes null from empty BYTES") ||
      !ExpectParse(missing_value, false, "Values 0.5 distinguishes missing from empty BYTES")) {
    return false;
  }

  Result result = SuccessResult();
  result.format_version = std::string{pae::protocol_lab::v06::kVariableResultFormat};
  FieldResult payload;
  payload.id = "payload";
  payload.kind = "BYTES";
  payload.raw_value = "";
  payload.logical_value = "";
  result.fields.push_back(payload);
  const std::string empty_fingerprint = pae::protocol_lab::v06::FinalizeFingerprint(result, error);
  const std::string serialized = pae::protocol_lab::v06::SerializeResult(result, error);
  Result parsed_result;
  std::string mutable_serialized = serialized;
  if (!Expect(!empty_fingerprint.empty() && !serialized.empty() &&
                  pae::protocol_lab::v06::ParseResult(mutable_serialized, parsed_result, error) &&
                  parsed_result.fields.back().kind == "BYTES" &&
                  parsed_result.fields.back().raw_value.empty() &&
                  parsed_result.fields.back().logical_value.empty(),
              "Result 0.9 Writer, Reader, and fingerprint share empty BYTES semantics")) {
    return false;
  }
  Result nonempty = result;
  nonempty.fields.back().raw_value = "00";
  nonempty.fields.back().logical_value = "00";
  if (!Expect(empty_fingerprint != pae::protocol_lab::v06::FinalizeFingerprint(nonempty, error),
              "Result 0.9 fingerprint distinguishes empty and non-empty BYTES")) {
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  Result old_generation = result;
  old_generation.format_version = std::string{pae::protocol_lab::v06::kLengthResultFormat};
  return Expect(!pae::protocol_lab::v06::ValidateResult(old_generation, error),
                "Result 0.8 keeps rejecting empty BYTES");
#else
  return true;
#endif
}
#endif

bool TestCanonicalEncoding() {
  Result result = SuccessResult();
  std::string error;
  const std::string fields =
      pae::protocol_lab::v06::EncodeFieldsCanonical(result.fields, result.format_version, error);
  const std::string expected_fields =
      u8"A2:A6:S6:温\n|:S9:DECIMAL64I3:123I1:1S5:INT64I20:-9223372036854775808"
      "A5:S1:uS6:UINT64S20:18446744073709551615S20:18446744073709551615B0;";
  if (!Expect(error.empty() && fields == expected_fields,
              "field canonical bytes use fixed arrays and UTF-8 byte lengths"))
    return false;

  const std::string payload = pae::protocol_lab::v06::EncodeFingerprintPayload(result, error);
  const std::string expected_payload =
      u8"A20:S23:pae.lab.fingerprint/0.6S3:0.5S6:encodeS9:ENCODE_TXS2:TXS2:OKN;"
      "S3:ABCS3:p|xS4:pipeS2:m\nS2:TXS0:N;S4:00FFN;N;N;N;" +
      expected_fields;
  if (!Expect(error.empty() && payload == expected_payload,
              "top-level canonical payload matches independently written fixed bytes"))
    return false;

  const std::string fingerprint = pae::protocol_lab::v06::FinalizeFingerprint(result, error);
  return Expect(fingerprint == "C7082F1B9672FFC5E51E20FB46FB3D97F5985B3DDA1C04AFD12F0D799328BDB6",
                "fingerprint is uppercase SHA-256 of the fixed canonical bytes");
}

bool TestResultSerializationAndValidation() {
  Result result = SuccessResult();
  std::string error;
  const std::string json = pae::protocol_lab::v06::SerializeResult(result, error);
  yyjson_doc* document = yyjson_read(json.data(), json.size(), YYJSON_READ_NOFLAG);
  if (!Expect(error.empty() && document != nullptr, "Result 0.6 serialization is valid JSON"))
    return false;
  yyjson_val* root = yyjson_doc_get_root(document);
  yyjson_val* fields = yyjson_obj_get(root, "fields");
  yyjson_val* decimal = yyjson_arr_get(fields, 0U);
  const bool shape = yyjson_obj_get(decimal, "decimal64") != nullptr &&
                     yyjson_obj_get(decimal, "raw_kind") != nullptr &&
                     yyjson_obj_get(decimal, "logical_value") == nullptr &&
                     yyjson_obj_get(decimal, "enum_known") == nullptr &&
                     yyjson_is_null(yyjson_obj_get(root, "conversion_error"));
  yyjson_doc_free(document);
  if (!Expect(shape, "Decimal Result uses C1 shape and null conversion_error")) return false;

  Result equivalent = result;
  equivalent.fields[0].decimal64 = Decimal64{1230, 2};
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(equivalent, error),
              "Result rejects non-normalized Decimal output"))
    return false;

  Result failed;
  failed.command = "encode";
  failed.operation_kind = "encode";
  failed.operation_status = "VALUE_NOT_REPRESENTABLE";
  failed.replay_mode = "ENCODE_TX";
  failed.replay_subject = "TX";
  failed.current_execution_status = "VALUE_NOT_REPRESENTABLE";
  failed.current_execution_diagnostic_id = "PAE_LAB_CODEC_VALUE_NOT_REPRESENTABLE";
  failed.conversion_error = "RAW_NOT_INTEGRAL";
  failed.message_id = "m";
  failed.failed_field_id = "temperature";
  failed.failed_field_index = 2U;
  failed.failed_value_index = 0U;
  if (!Expect(pae::protocol_lab::v06::ValidateResult(failed, error),
              "conversion failure preserves reason and field/value identity"))
    return false;

  Result wrong_status = failed;
  wrong_status.current_execution_status = "INVALID_ARGUMENT";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(wrong_status, error),
              "conversion reason is checked jointly with status"))
    return false;
  Result partial = failed;
  partial.fields.push_back(DecimalField(1, 0, "INT64", "1"));
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(partial, error),
              "failed result rejects partial fields"))
    return false;
  Result internal = failed;
  internal.current_execution_status = "INTERNAL_ERROR";
  internal.operation_status = "INTERNAL_ERROR";
  internal.conversion_error.reset();
  if (!Expect(pae::protocol_lab::v06::ValidateResult(internal, error),
              "internal error has null conversion reason"))
    return false;
  Result non_conversion = failed;
  non_conversion.conversion_error.reset();
  if (!Expect(pae::protocol_lab::v06::ValidateResult(non_conversion, error),
              "non-conversion VALUE_NOT_REPRESENTABLE may have null conversion reason"))
    return false;
  Result unknown_reason = failed;
  unknown_reason.conversion_error = "UNKNOWN";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(unknown_reason, error),
              "unknown conversion reason is rejected"))
    return false;
  Result no_codec = internal;
  no_codec.operation_kind = "udp-exchange";
  no_codec.operation_status = "EVIDENCE_VERIFIED";
  no_codec.replay_mode = "NO_CODEC_REEXECUTION";
  no_codec.replay_subject = "RX";
  no_codec.current_execution_status = "NOT_EVALUATED";
  no_codec.current_execution_diagnostic_id.reset();
  no_codec.failed_field_id.reset();
  no_codec.failed_field_index.reset();
  no_codec.failed_value_index.reset();
  if (!Expect(pae::protocol_lab::v06::ValidateResult(no_codec, error),
              "NO_CODEC execution keeps null conversion reason and no failure identity"))
    return false;
  Result final_review = internal;
  final_review.current_execution_status = "FINAL_REVIEW_FAILED";
  final_review.operation_status = "FINAL_REVIEW_FAILED";
  return Expect(pae::protocol_lab::v06::ValidateResult(final_review, error),
                "final review failure has null conversion reason");
}

bool TestReplayModeAndFailureIdentityValidation() {
  std::string error;

  Result no_codec;
  no_codec.command = "replay";
  no_codec.operation_kind = "udp-exchange";
  no_codec.operation_status = "EVIDENCE_VERIFIED";
  no_codec.replay_mode = "NO_CODEC_REEXECUTION";
  no_codec.replay_subject = "RX";
  no_codec.current_execution_status = "NOT_EVALUATED";
  if (!Expect(pae::protocol_lab::v06::ValidateResult(no_codec, error),
              "true NO_CODEC RX result is valid"))
    return false;
  no_codec.replay_subject = "RX_INCOMPLETE";
  if (!Expect(pae::protocol_lab::v06::ValidateResult(no_codec, error),
              "true NO_CODEC RX_INCOMPLETE result is valid"))
    return false;

  Result false_success = SuccessResult();
  false_success.operation_kind = "udp-exchange";
  false_success.operation_status = "EVIDENCE_VERIFIED";
  false_success.replay_mode = "NO_CODEC_REEXECUTION";
  false_success.replay_subject = "RX";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(false_success, error),
              "NO_CODEC must reject a false OK result with delivered fields"))
    return false;
  Result false_operation_success = no_codec;
  false_operation_success.operation_status = "OK";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(false_operation_success, error),
              "NO_CODEC rejects a false successful operation status"))
    return false;

  Result wrong_subject = no_codec;
  wrong_subject.replay_subject = "TX";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(wrong_subject, error),
              "NO_CODEC rejects an unsupported subject"))
    return false;
  Result stale_encode_mode = no_codec;
  stale_encode_mode.replay_mode = "ENCODE_TX";
  stale_encode_mode.replay_subject = "TX";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(stale_encode_mode, error),
              "executing replay modes reject NOT_EVALUATED"))
    return false;
  Result wrong_encode_subject = SuccessResult();
  wrong_encode_subject.replay_subject = "RX";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(wrong_encode_subject, error),
              "ENCODE_TX rejects a non-TX subject"))
    return false;
  Result wrong_decode_subject = no_codec;
  wrong_decode_subject.replay_mode = "DECODE_RX";
  wrong_decode_subject.replay_subject = "TX";
  wrong_decode_subject.current_execution_status = "UNKNOWN_MESSAGE";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(wrong_decode_subject, error),
              "DECODE_RX rejects a non-RX subject"))
    return false;
  Result unknown_mode = no_codec;
  unknown_mode.replay_mode = "UNKNOWN";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(unknown_mode, error),
              "unknown replay mode is rejected"))
    return false;
  Result no_codec_diagnostic = no_codec;
  no_codec_diagnostic.current_execution_diagnostic_id = "PAE_LAB_CODEC_OK";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(no_codec_diagnostic, error),
              "NO_CODEC rejects a fabricated current diagnostic"))
    return false;
  Result no_codec_failure = no_codec;
  no_codec_failure.failed_value_index = 0U;
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(no_codec_failure, error),
              "NO_CODEC rejects failure identity"))
    return false;

  Result default_state;
  default_state.command = "pending";
  default_state.operation_kind = "pending";
  default_state.operation_status = "NOT_EVALUATED";
  default_state.replay_mode = "NONE";
  default_state.replay_subject = "NONE";
  default_state.current_execution_status = "NOT_EVALUATED";
  if (!Expect(pae::protocol_lab::v06::ValidateResult(default_state, error),
              "default unexecuted NONE/NONE state remains valid"))
    return false;

  Result decode_failure;
  decode_failure.command = "inspect";
  decode_failure.operation_kind = "inspect";
  decode_failure.operation_status = "VALUE_NOT_REPRESENTABLE";
  decode_failure.replay_mode = "DECODE_RX";
  decode_failure.replay_subject = "RX";
  decode_failure.current_execution_status = "VALUE_NOT_REPRESENTABLE";
  decode_failure.current_execution_diagnostic_id = "PAE_LAB_CODEC_VALUE_NOT_REPRESENTABLE";
  decode_failure.message_id = "m";
  decode_failure.failed_field_id = "temperature";
  decode_failure.failed_field_index = 2U;
  if (!Expect(pae::protocol_lab::v06::ValidateResult(decode_failure, error),
              "Decode failure may carry a known field identity"))
    return false;
  Result decode_value_index = decode_failure;
  decode_value_index.failed_value_index = 0U;
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(decode_value_index, error),
              "Decode failure rejects encode-only failed_value_index"))
    return false;
  Result missing_message = decode_failure;
  missing_message.message_id.reset();
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(missing_message, error),
              "field identity requires a known Message"))
    return false;
  Result field_id_only = decode_failure;
  field_id_only.failed_field_index.reset();
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(field_id_only, error),
              "failed field id without index is rejected"))
    return false;
  Result field_index_only = decode_failure;
  field_index_only.failed_field_id.reset();
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(field_index_only, error),
              "failed field index without id is rejected"))
    return false;
  Result empty_field_id = decode_failure;
  empty_field_id.failed_field_id = "";
  if (!Expect(!pae::protocol_lab::v06::ValidateResult(empty_field_id, error),
              "failed_field_id must not be empty"))
    return false;

  Result encode_input_failure;
  encode_input_failure.command = "encode";
  encode_input_failure.operation_kind = "encode";
  encode_input_failure.operation_status = "VALUES_INVALID";
  encode_input_failure.replay_mode = "ENCODE_TX";
  encode_input_failure.replay_subject = "TX";
  encode_input_failure.current_execution_status = "VALUES_INVALID";
  encode_input_failure.current_execution_diagnostic_id = "PAE_LAB_VALUES_UNKNOWN_FIELD";
  encode_input_failure.failed_value_index = 3U;
  return Expect(pae::protocol_lab::v06::ValidateResult(encode_input_failure, error),
                "Encode input error may retain only its input index when no field is known");
}

bool TestFingerprintDifferences() {
  Result base = SuccessResult();
  std::string error;
  const std::string fingerprint = pae::protocol_lab::v06::FinalizeFingerprint(base, error);
  Result changed_math = base;
  changed_math.fields[0].decimal64 = Decimal64{124, 1};
  Result changed_raw = base;
  changed_raw.fields[0].raw_value = "-9223372036854775807";
  Result changed_kind = base;
  changed_kind.fields[0].raw_kind = "UINT64";
  changed_kind.fields[0].raw_value = "0";
  if (!Expect(fingerprint != pae::protocol_lab::v06::FinalizeFingerprint(changed_math, error),
              "different Decimal mathematical value changes fingerprint") ||
      !Expect(fingerprint != pae::protocol_lab::v06::FinalizeFingerprint(changed_raw, error),
              "different raw value changes fingerprint") ||
      !Expect(fingerprint != pae::protocol_lab::v06::FinalizeFingerprint(changed_kind, error),
              "different raw kind changes fingerprint"))
    return false;

  Result failed;
  failed.command = "encode";
  failed.operation_kind = "encode";
  failed.operation_status = "VALUE_NOT_REPRESENTABLE";
  failed.replay_mode = "ENCODE_TX";
  failed.replay_subject = "TX";
  failed.current_execution_status = "VALUE_NOT_REPRESENTABLE";
  failed.current_execution_diagnostic_id = "PAE_LAB_CODEC_VALUE_NOT_REPRESENTABLE";
  failed.conversion_error = "RAW_NOT_INTEGRAL";
  failed.message_id = "m";
  failed.failed_field_id = "d";
  failed.failed_field_index = 0U;
  failed.failed_value_index = 1U;
  const std::string failed_fingerprint = pae::protocol_lab::v06::FinalizeFingerprint(failed, error);
  Result changed_reason = failed;
  changed_reason.conversion_error = "RAW_OUT_OF_RANGE";
  Result changed_field = failed;
  changed_field.failed_field_id = "other";
  Result changed_value_index = failed;
  changed_value_index.failed_value_index = 2U;
  return Expect(failed_fingerprint !=
                    pae::protocol_lab::v06::FinalizeFingerprint(changed_reason, error),
                "different conversion reason changes failure fingerprint") &&
         Expect(failed_fingerprint !=
                    pae::protocol_lab::v06::FinalizeFingerprint(changed_field, error),
                "different failed field identity changes failure fingerprint") &&
         Expect(failed_fingerprint !=
                    pae::protocol_lab::v06::FinalizeFingerprint(changed_value_index, error),
                "different failed value identity changes failure fingerprint");
}

}  // namespace

int main() {
  if (!TestValuesV04() || !TestCanonicalEncoding() || !TestResultSerializationAndValidation() ||
      !TestReplayModeAndFailureIdentityValidation() || !TestFingerprintDifferences()
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      || !TestValuesV05AndVariableEmptyResult()
#endif
  )
    return 1;
  std::cout << "Protocol Lab V0.6 isolated format tests passed\n";
  return 0;
}
