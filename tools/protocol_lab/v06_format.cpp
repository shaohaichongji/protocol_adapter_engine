#include "v06_format.h"

#include <yyjson.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <utility>

#include "sha256.h"

namespace pae::protocol_lab::v06 {
namespace {

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }
};
using DocumentPtr = std::unique_ptr<yyjson_doc, DocumentDeleter>;

bool ParseUint64(std::string_view text, std::uint64_t& output) noexcept {
  if (text.empty() || (text.size() > 1U && text.front() == '0')) return false;
  std::uint64_t value = 0U;
  for (const char character : text) {
    if (character < '0' || character > '9') return false;
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (value > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10U) return false;
    value = value * 10U + digit;
  }
  output = value;
  return true;
}

bool ParseInt64(std::string_view text, std::int64_t& output) noexcept {
  if (text.empty() || text.front() == '+' || text == "-0") return false;
  const bool negative = text.front() == '-';
  const std::string_view digits = negative ? text.substr(1U) : text;
  if (digits.empty() || (digits.size() > 1U && digits.front() == '0')) return false;
  const std::uint64_t negative_limit = std::uint64_t{1U} << 63U;
  const std::uint64_t limit =
      negative ? negative_limit
               : static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
  std::uint64_t magnitude = 0U;
  for (const char character : digits) {
    if (character < '0' || character > '9') return false;
    const auto digit = static_cast<std::uint64_t>(character - '0');
    if (magnitude > (limit - digit) / 10U) return false;
    magnitude = magnitude * 10U + digit;
  }
  if (!negative) {
    output = static_cast<std::int64_t>(magnitude);
  } else if (magnitude == negative_limit) {
    output = (std::numeric_limits<std::int64_t>::min)();
  } else {
    output = -static_cast<std::int64_t>(magnitude);
  }
  return true;
}

bool ParseUpperHex(std::string_view text, std::vector<std::uint8_t>& output) {
  if (text.empty() || text.size() % 2U != 0U) return false;
  output.clear();
  output.reserve(text.size() / 2U);
  auto nibble = [](char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  for (std::size_t index = 0U; index < text.size(); index += 2U) {
    const int high = nibble(text[index]);
    const int low = nibble(text[index + 1U]);
    if (high < 0 || low < 0) return false;
    output.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
  return true;
}

bool IsObjectWithKeys(yyjson_val* value, const std::set<std::string_view>& allowed,
                      const std::set<std::string_view>& required, std::string_view pointer,
                      std::string& error) {
  if (!yyjson_is_obj(value)) {
    error = std::string{pointer} + " must be an object";
    return false;
  }
  for (const auto key : required) {
    if (yyjson_obj_getn(value, key.data(), key.size()) == nullptr) {
      error = std::string{pointer} + " is missing property " + std::string{key};
      return false;
    }
  }
  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(value, &iterator);
  std::set<std::string> seen;
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string_view name{yyjson_get_str(key), yyjson_get_len(key)};
    if (!seen.emplace(name).second) {
      error = std::string{pointer} + " contains duplicate property " + std::string{name};
      return false;
    }
    if (allowed.find(name) == allowed.end()) {
      error = std::string{pointer} + " contains unknown property " + std::string{name};
      return false;
    }
  }
  return true;
}

bool ReadString(yyjson_val* object, std::string_view key, std::string& output, std::string& error) {
  yyjson_val* value = yyjson_obj_getn(object, key.data(), key.size());
  if (!yyjson_is_str(value)) {
    error = std::string{key} + " must be a string";
    return false;
  }
  output.assign(yyjson_get_str(value), yyjson_get_len(value));
  return true;
}

std::string Quote(std::string_view text) {
  std::ostringstream output;
  output << '"';
  for (const unsigned char value : text) {
    switch (value) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (value < 0x20U) {
          constexpr char kHex[] = "0123456789ABCDEF";
          output << "\\u00" << kHex[value >> 4U] << kHex[value & 0x0FU];
        } else {
          output << static_cast<char>(value);
        }
    }
  }
  output << '"';
  return output.str();
}

std::string EncodeNull() { return "N;"; }
std::string EncodeString(std::string_view value) {
  return "S" + std::to_string(value.size()) + ":" + std::string{value};
}
std::string EncodeInteger(std::string_view canonical) {
  return "I" + std::to_string(canonical.size()) + ":" + std::string{canonical};
}
std::string EncodeBoolean(bool value) { return value ? "B1;" : "B0;"; }
std::string EncodeOptionalString(const std::optional<std::string>& value) {
  return value.has_value() ? EncodeString(*value) : EncodeNull();
}
std::string EncodeOptionalIndex(const std::optional<std::size_t>& value) {
  return value.has_value() ? EncodeInteger(std::to_string(*value)) : EncodeNull();
}

bool IsUpperHexOrEmpty(std::string_view value) noexcept {
  return std::all_of(value.begin(), value.end(),
                     [](char character) {
                       return (character >= '0' && character <= '9') ||
                              (character >= 'A' && character <= 'F');
                     }) &&
         value.size() % 2U == 0U;
}

bool ValidateField(const FieldResult& field, std::string& error) {
  if (field.id.empty()) {
    error = "result field id must not be empty";
    return false;
  }
  if (field.kind == "DECIMAL64") {
    if (!field.decimal64.has_value() || !field.raw_kind.has_value() ||
        !field.logical_value.empty() || field.enum_known) {
      error = "DECIMAL64 result field has a mixed or incomplete shape";
      return false;
    }
    const Decimal64 normalized = NormalizeDecimal64(*field.decimal64);
    if (field.decimal64->scale < 0 || field.decimal64->scale > 18 ||
        normalized.coefficient != field.decimal64->coefficient ||
        normalized.scale != field.decimal64->scale) {
      error = "DECIMAL64 result field is not normalized";
      return false;
    }
    if (*field.raw_kind == "INT64") {
      std::int64_t ignored = 0;
      if (!ParseInt64(field.raw_value, ignored)) {
        error = "DECIMAL64 raw_value is not canonical INT64";
        return false;
      }
    } else if (*field.raw_kind == "UINT64") {
      std::uint64_t ignored = 0U;
      if (!ParseUint64(field.raw_value, ignored)) {
        error = "DECIMAL64 raw_value is not canonical UINT64";
        return false;
      }
    } else {
      error = "DECIMAL64 raw_kind must be INT64 or UINT64";
      return false;
    }
    return true;
  }
  if (field.decimal64.has_value() || field.raw_kind.has_value()) {
    error = "non-DECIMAL64 result field contains Decimal properties";
    return false;
  }
  if (field.kind != "UINT64" && field.kind != "INT64" && field.kind != "BYTES" &&
      field.kind != "ENUM" && field.kind != "BOOL") {
    error = "result field kind is unsupported";
    return false;
  }
  if (field.kind == "UINT64") {
    std::uint64_t ignored = 0U;
    if (!ParseUint64(field.raw_value, ignored) || field.logical_value != field.raw_value ||
        field.enum_known) {
      error = "UINT64 result field has an invalid legacy tuple";
      return false;
    }
  } else if (field.kind == "INT64") {
    std::int64_t ignored = 0;
    if (!ParseInt64(field.raw_value, ignored) || field.logical_value != field.raw_value ||
        field.enum_known) {
      error = "INT64 result field has an invalid legacy tuple";
      return false;
    }
  } else if (field.kind == "BYTES") {
    if (field.raw_value.empty() || !IsUpperHexOrEmpty(field.raw_value) ||
        field.logical_value != field.raw_value || field.enum_known) {
      error = "BYTES result field has an invalid legacy tuple";
      return false;
    }
  } else if (field.kind == "BOOL") {
    if ((field.raw_value != "0" && field.raw_value != "1") ||
        (field.logical_value != "false" && field.logical_value != "true") ||
        ((field.raw_value == "1") != (field.logical_value == "true")) || field.enum_known) {
      error = "BOOL result field has an invalid legacy tuple";
      return false;
    }
  } else {
    std::uint64_t ignored = 0U;
    const bool valid_known_id =
        !field.logical_value.empty() && field.logical_value.size() <= 128U &&
        field.logical_value.front() >= 'a' && field.logical_value.front() <= 'z' &&
        std::all_of(field.logical_value.begin(), field.logical_value.end(), [](char character) {
          return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
                 character == '_';
        });
    if (!ParseUint64(field.raw_value, ignored) ||
        (!field.enum_known && field.logical_value != field.raw_value) ||
        (field.enum_known && !valid_known_id)) {
      error = "ENUM result field has an invalid legacy tuple";
      return false;
    }
  }
  return true;
}

std::string SerializeFields(const std::vector<FieldResult>& fields) {
  std::ostringstream output;
  output << '[';
  for (std::size_t index = 0U; index < fields.size(); ++index) {
    if (index != 0U) output << ',';
    const FieldResult& field = fields[index];
    output << "{\"id\":" << Quote(field.id) << ",\"kind\":" << Quote(field.kind);
    if (field.kind == "DECIMAL64") {
      output << ",\"decimal64\":{\"coefficient\":"
             << Quote(std::to_string(field.decimal64->coefficient))
             << ",\"scale\":" << field.decimal64->scale
             << "},\"raw_kind\":" << Quote(*field.raw_kind)
             << ",\"raw_value\":" << Quote(field.raw_value);
    } else {
      output << ",\"raw_value\":" << Quote(field.raw_value)
             << ",\"logical_value\":" << Quote(field.logical_value)
             << ",\"enum_known\":" << (field.enum_known ? "true" : "false");
    }
    output << '}';
  }
  output << ']';
  return output.str();
}

std::string JsonOptionalString(const std::optional<std::string>& value) {
  return value.has_value() ? Quote(*value) : "null";
}

std::string UppercaseHash(std::string_view payload) {
  std::string hash = HashBytes(payload);
  std::transform(hash.begin(), hash.end(), hash.begin(),
                 [](unsigned char value) { return static_cast<char>(std::toupper(value)); });
  return hash;
}

}  // namespace

Decimal64 NormalizeDecimal64(Decimal64 value) noexcept {
  if (value.coefficient == 0) return Decimal64{};
  while (value.scale > 0 && value.coefficient % 10 == 0) {
    value.coefficient /= 10;
    --value.scale;
  }
  return value;
}

bool ParseValues(std::string& text, ParsedValues& output, std::string& error) {
  output = ParsedValues{};
  yyjson_read_err read_error{};
  DocumentPtr document{
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &read_error)};
  if (!document) {
    error = "Values 0.4 is not valid strict JSON";
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> root_keys{"format_version", "pipeline_id", "message_id",
                                             "fields"};
  if (!IsObjectWithKeys(root, root_keys, root_keys, "values", error)) return false;
  std::string format;
  if (!ReadString(root, "format_version", format, error) || format != kValuesFormat ||
      !ReadString(root, "pipeline_id", output.pipeline_id, error) ||
      !ReadString(root, "message_id", output.message_id, error)) {
    if (error.empty()) error = "unsupported Values format_version";
    return false;
  }
  yyjson_val* fields = yyjson_obj_get(root, "fields");
  if (!yyjson_is_arr(fields)) {
    error = "fields must be an array";
    return false;
  }
  std::set<std::string> ids;
  std::size_t ordinal = 0U;
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(fields, &iterator);
  while (yyjson_val* value = yyjson_arr_iter_next(&iterator)) {
    const std::string pointer = "fields[" + std::to_string(ordinal) + "]";
    const std::set<std::string_view> allowed{"id",  "kind",     "uint64", "int64",
                                             "hex", "entry_id", "bool",   "decimal64"};
    if (!IsObjectWithKeys(value, allowed, {"id", "kind"}, pointer, error)) return false;
    ParsedValue parsed;
    if (!ReadString(value, "id", parsed.id, error) ||
        !ReadString(value, "kind", parsed.kind, error))
      return false;
    if (!ids.insert(parsed.id).second) {
      error = pointer + " duplicates field id " + parsed.id;
      return false;
    }
    std::set<std::string_view> exact{"id", "kind"};
    if (parsed.kind == "UINT64") {
      exact.insert("uint64");
      std::string raw;
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "uint64", raw, error) || !ParseUint64(raw, parsed.uint64_value)) {
        if (error.empty()) error = pointer + ".uint64 is not canonical UINT64";
        return false;
      }
    } else if (parsed.kind == "INT64") {
      exact.insert("int64");
      std::string raw;
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "int64", raw, error) || !ParseInt64(raw, parsed.int64_value)) {
        if (error.empty()) error = pointer + ".int64 is not canonical INT64";
        return false;
      }
    } else if (parsed.kind == "BYTES") {
      exact.insert("hex");
      std::string raw;
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "hex", raw, error) || !ParseUpperHex(raw, parsed.bytes)) {
        if (error.empty()) error = pointer + ".hex is not canonical uppercase bytes";
        return false;
      }
    } else if (parsed.kind == "ENUM") {
      exact.insert("entry_id");
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "entry_id", parsed.enum_entry_id, error))
        return false;
    } else if (parsed.kind == "BOOL") {
      exact.insert("bool");
      yyjson_val* boolean = yyjson_obj_get(value, "bool");
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) || !yyjson_is_bool(boolean)) {
        if (error.empty()) error = pointer + ".bool must be a native JSON boolean";
        return false;
      }
      parsed.bool_value = yyjson_get_bool(boolean);
    } else if (parsed.kind == "DECIMAL64") {
      exact.insert("decimal64");
      yyjson_val* decimal = yyjson_obj_get(value, "decimal64");
      const std::set<std::string_view> decimal_keys{"coefficient", "scale"};
      std::string coefficient;
      yyjson_val* scale = yyjson_is_obj(decimal) ? yyjson_obj_get(decimal, "scale") : nullptr;
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !IsObjectWithKeys(decimal, decimal_keys, decimal_keys, pointer + ".decimal64", error) ||
          !ReadString(decimal, "coefficient", coefficient, error) ||
          !ParseInt64(coefficient, parsed.decimal64_value.coefficient) || !yyjson_is_int(scale) ||
          yyjson_get_sint(scale) < 0 || yyjson_get_sint(scale) > 18) {
        if (error.empty()) error = pointer + ".decimal64 is not a valid Values 0.4 Decimal64";
        return false;
      }
      parsed.decimal64_value.scale = static_cast<std::int32_t>(yyjson_get_sint(scale));
      parsed.decimal64_value = NormalizeDecimal64(parsed.decimal64_value);
    } else {
      error = pointer + ".kind is unsupported";
      return false;
    }
    output.fields.push_back(std::move(parsed));
    ++ordinal;
  }
  error.clear();
  return true;
}

bool ValidateResult(const Result& result, std::string& error) {
  if (result.operation_kind.empty() || result.operation_status.empty() ||
      result.replay_mode.empty() || result.replay_subject.empty() ||
      result.current_execution_status.empty()) {
    error = "Result 0.6 deterministic execution identity is incomplete";
    return false;
  }
  for (const auto* identity : {&result.config_sha256, &result.protocol_id, &result.pipeline_id,
                               &result.message_id, &result.direction_id}) {
    if (identity->has_value() && identity->value().empty()) {
      error = "Result 0.6 known identity must not be empty";
      return false;
    }
  }
  if ((result.diagnostic_id.has_value() && result.diagnostic_id->empty()) ||
      (result.current_execution_diagnostic_id.has_value() &&
       result.current_execution_diagnostic_id->empty())) {
    error = "Result 0.6 known diagnostic identity must not be empty";
    return false;
  }
  const bool has_failure_identity = result.failed_field_id.has_value() ||
                                    result.failed_field_index.has_value() ||
                                    result.failed_value_index.has_value();
  if (result.replay_mode == "NONE") {
    if (result.replay_subject != "NONE" || result.current_execution_status != "NOT_EVALUATED" ||
        result.current_execution_diagnostic_id.has_value() || result.conversion_error.has_value() ||
        has_failure_identity || !result.fields.empty()) {
      error = "Result 0.6 NONE mode must remain unevaluated and empty";
      return false;
    }
  } else if (result.replay_mode == "ENCODE_TX") {
    if (result.replay_subject != "TX" || result.current_execution_status == "NOT_EVALUATED") {
      error = "Result 0.6 ENCODE_TX mode and execution state are inconsistent";
      return false;
    }
  } else if (result.replay_mode == "DECODE_RX") {
    if (result.replay_subject != "RX" || result.current_execution_status == "NOT_EVALUATED") {
      error = "Result 0.6 DECODE_RX mode and execution state are inconsistent";
      return false;
    }
  } else if (result.replay_mode == "NO_CODEC_REEXECUTION") {
    if ((result.replay_subject != "RX" && result.replay_subject != "RX_INCOMPLETE") ||
        result.operation_status == "OK" || result.current_execution_status != "NOT_EVALUATED" ||
        result.current_execution_diagnostic_id.has_value() || result.conversion_error.has_value() ||
        has_failure_identity || !result.fields.empty()) {
      error = "Result 0.6 NO_CODEC_REEXECUTION must remain unevaluated and empty";
      return false;
    }
  } else {
    error = "Result 0.6 replay_mode is unsupported";
    return false;
  }
  for (const auto* frame : {&result.frame_hex, &result.tx_frame_hex, &result.rx_frame_hex}) {
    if (frame->has_value() && !IsUpperHexOrEmpty(**frame)) {
      error = "Result 0.6 frame Hex is not canonical uppercase Hex";
      return false;
    }
  }
  const bool has_conversion_error = result.conversion_error.has_value();
  if (has_conversion_error) {
    const std::string& reason = *result.conversion_error;
    const bool scale_error = reason == "DECIMAL_SCALE_OUT_OF_RANGE";
    const bool numeric_error = reason == "RAW_NOT_INTEGRAL" || reason == "RAW_OUT_OF_RANGE" ||
                               reason == "LOGICAL_OUT_OF_RANGE";
    if ((!scale_error && !numeric_error) ||
        (scale_error && result.current_execution_status != "INVALID_ARGUMENT") ||
        (numeric_error && result.current_execution_status != "VALUE_NOT_REPRESENTABLE")) {
      error = "Result 0.6 status and conversion_error are inconsistent";
      return false;
    }
  }
  const bool succeeded = result.current_execution_status == "OK";
  if (!succeeded && !result.fields.empty()) {
    error = "failed Result 0.6 must not contain partial fields";
    return false;
  }
  if (succeeded &&
      (result.conversion_error.has_value() || result.failed_field_id.has_value() ||
       result.failed_field_index.has_value() || result.failed_value_index.has_value())) {
    error = "successful Result 0.6 must not contain failure details";
    return false;
  }
  if (result.failed_field_id.has_value() != result.failed_field_index.has_value()) {
    error = "Result 0.6 failed field id and index must be present together";
    return false;
  }
  if (result.failed_field_id.has_value() &&
      (result.failed_field_id->empty() || !result.message_id.has_value() ||
       result.message_id->empty())) {
    error = "Result 0.6 failed field identity requires a known non-empty Message and field id";
    return false;
  }
  if (result.failed_value_index.has_value() && result.replay_mode != "ENCODE_TX") {
    error = "Result 0.6 failed_value_index is only valid for Encode execution";
    return false;
  }
  std::set<std::string> ids;
  for (const FieldResult& field : result.fields) {
    if (!ValidateField(field, error)) return false;
    if (!ids.insert(field.id).second) {
      error = "Result 0.6 contains duplicate field ids";
      return false;
    }
  }
  error.clear();
  return true;
}

std::string EncodeFieldsCanonical(const std::vector<FieldResult>& fields, std::string& error) {
  std::string output = "A" + std::to_string(fields.size()) + ":";
  for (const FieldResult& field : fields) {
    if (!ValidateField(field, error)) return {};
    if (field.kind == "DECIMAL64") {
      const std::string coefficient = std::to_string(field.decimal64->coefficient);
      const std::string scale = std::to_string(field.decimal64->scale);
      output += "A6:" + EncodeString(field.id) + EncodeString(field.kind) +
                EncodeInteger(coefficient) + EncodeInteger(scale) + EncodeString(*field.raw_kind) +
                EncodeInteger(field.raw_value);
    } else {
      output += "A5:" + EncodeString(field.id) + EncodeString(field.kind) +
                EncodeString(field.raw_value) + EncodeString(field.logical_value) +
                EncodeBoolean(field.enum_known);
    }
  }
  error.clear();
  return output;
}

std::string EncodeFingerprintPayload(const Result& result, std::string& error) {
  if (!ValidateResult(result, error)) return {};
  const std::string fields = EncodeFieldsCanonical(result.fields, error);
  if (!error.empty()) return {};
  return "A20:" + EncodeString(kFingerprintDomain) + EncodeString("0.5") +
         EncodeString(result.operation_kind) + EncodeString(result.replay_mode) +
         EncodeString(result.replay_subject) + EncodeString(result.current_execution_status) +
         EncodeOptionalString(result.current_execution_diagnostic_id) +
         EncodeOptionalString(result.config_sha256) + EncodeOptionalString(result.protocol_id) +
         EncodeOptionalString(result.pipeline_id) + EncodeOptionalString(result.message_id) +
         EncodeOptionalString(result.direction_id) + EncodeOptionalString(result.frame_hex) +
         EncodeOptionalString(result.tx_frame_hex) + EncodeOptionalString(result.rx_frame_hex) +
         EncodeOptionalString(result.conversion_error) +
         EncodeOptionalString(result.failed_field_id) +
         EncodeOptionalIndex(result.failed_field_index) +
         EncodeOptionalIndex(result.failed_value_index) + fields;
}

std::string FinalizeFingerprint(const Result& result, std::string& error) {
  const std::string payload = EncodeFingerprintPayload(result, error);
  return error.empty() ? UppercaseHash(payload) : std::string{};
}

std::string SerializeResult(const Result& result, std::string& error) {
  if (!ValidateResult(result, error)) return {};
  const std::string fingerprint = FinalizeFingerprint(result, error);
  if (!error.empty()) return {};
  std::ostringstream output;
  output << "{\n"
         << "  \"format_version\":\"pae.lab.result/0.6\",\n"
         << "  \"command\":" << Quote(result.command) << ",\n"
         << "  \"operation_kind\":" << Quote(result.operation_kind) << ",\n"
         << "  \"operation_status\":" << Quote(result.operation_status) << ",\n"
         << "  \"exit_code\":" << result.exit_code << ",\n"
         << "  \"config_sha256\":" << JsonOptionalString(result.config_sha256) << ",\n"
         << "  \"protocol_id\":" << JsonOptionalString(result.protocol_id) << ",\n"
         << "  \"pipeline_id\":" << JsonOptionalString(result.pipeline_id) << ",\n"
         << "  \"message_id\":" << JsonOptionalString(result.message_id) << ",\n"
         << "  \"direction_id\":" << JsonOptionalString(result.direction_id) << ",\n"
         << "  \"frame_hex\":" << JsonOptionalString(result.frame_hex) << ",\n"
         << "  \"tx_frame_hex\":" << JsonOptionalString(result.tx_frame_hex) << ",\n"
         << "  \"rx_frame_hex\":" << JsonOptionalString(result.rx_frame_hex) << ",\n"
         << "  \"fields\":" << SerializeFields(result.fields) << ",\n"
         << "  \"diagnostic\":{\"id\":" << JsonOptionalString(result.diagnostic_id)
         << ",\"detail\":" << Quote(result.diagnostic_detail) << "},\n"
         << "  \"deterministic_fingerprint\":" << Quote(fingerprint) << ",\n"
         << "  \"replay_mode\":" << Quote(result.replay_mode) << ",\n"
         << "  \"replay_subject\":" << Quote(result.replay_subject) << ",\n"
         << "  \"current_execution_status\":" << Quote(result.current_execution_status) << ",\n"
         << "  \"current_execution_diagnostic_id\":"
         << JsonOptionalString(result.current_execution_diagnostic_id) << ",\n"
         << "  \"conversion_error\":" << JsonOptionalString(result.conversion_error) << ",\n"
         << "  \"failed_field_id\":" << JsonOptionalString(result.failed_field_id) << ",\n"
         << "  \"failed_field_index\":";
  if (result.failed_field_index.has_value())
    output << *result.failed_field_index;
  else
    output << "null";
  output << ",\n  \"failed_value_index\":";
  if (result.failed_value_index.has_value())
    output << *result.failed_value_index;
  else
    output << "null";
  output << "\n}\n";
  error.clear();
  return output.str();
}

}  // namespace pae::protocol_lab::v06
