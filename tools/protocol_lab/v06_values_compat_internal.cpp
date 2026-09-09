#include "v06_values_compat_internal.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string_view>
#include <utility>

#include "yyjson.h"

namespace pae::protocol_lab::v06::internal {
namespace {

constexpr std::string_view kValuesFormatV1 = "pae.lab.values/0.1";
constexpr std::string_view kValuesFormatV2 = "pae.lab.values/0.2";
constexpr std::string_view kValuesFormatV3 = "pae.lab.values/0.3";

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }
};
using DocumentPtr = std::unique_ptr<yyjson_doc, DocumentDeleter>;

bool IsObjectWithKeys(yyjson_val* value, const std::set<std::string_view>& allowed,
                      const std::set<std::string_view>& required, std::string_view pointer,
                      std::string& error) {
  if (!yyjson_is_obj(value)) {
    error = std::string{pointer} + " must be an object";
    return false;
  }
  std::set<std::string> seen;
  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(value, &iterator);
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
  for (const std::string_view name : required) {
    if (seen.find(std::string{name}) == seen.end()) {
      error = std::string{pointer} + " is missing property " + std::string{name};
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
  auto nibble = [](char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
  };
  std::vector<std::uint8_t> candidate;
  candidate.reserve(text.size() / 2U);
  for (std::size_t index = 0U; index < text.size(); index += 2U) {
    const int high = nibble(text[index]);
    const int low = nibble(text[index + 1U]);
    if (high < 0 || low < 0) return false;
    candidate.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
  output = std::move(candidate);
  return true;
}

bool IsLegacyFormat(std::string_view format) noexcept {
  return format == kValuesFormatV1 || format == kValuesFormatV2 || format == kValuesFormatV3;
}

}  // namespace

bool ParseCompatibleValues(std::string& text, ParsedValues& output, std::string& error) {
  output = ParsedValues{};
  yyjson_read_err read_error{};
  DocumentPtr document{
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &read_error)};
  if (!document) {
    error = "Values is not valid strict JSON at byte " + std::to_string(read_error.pos);
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> root_keys{"format_version", "pipeline_id", "message_id",
                                             "fields"};
  if (!IsObjectWithKeys(root, root_keys, root_keys, "values", error)) return false;
  std::string format;
  if (!ReadString(root, "format_version", format, error)) return false;
  if (format == kValuesFormat
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      || format == kVariableValuesFormat
#endif
  ) {
    return ParseValues(text, output, error);
  }
  if (!IsLegacyFormat(format)) {
    error = "unsupported Values format_version";
    return false;
  }

  ParsedValues candidate;
  candidate.format_version = format;
  if (!ReadString(root, "pipeline_id", candidate.pipeline_id, error) ||
      !ReadString(root, "message_id", candidate.message_id, error))
    return false;
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
                                             "hex", "entry_id", "bool"};
    if (!IsObjectWithKeys(value, allowed, {"id", "kind"}, pointer, error)) return false;
    ParsedValue parsed;
    if (!ReadString(value, "id", parsed.id, error) ||
        !ReadString(value, "kind", parsed.kind, error))
      return false;
    if (!ids.insert(parsed.id).second) {
      error = "fields contains duplicate id " + parsed.id;
      return false;
    }
    if (parsed.kind == "UINT64") {
      const std::set<std::string_view> exact{"id", "kind", "uint64"};
      std::string raw;
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "uint64", raw, error) || !ParseUint64(raw, parsed.uint64_value)) {
        if (error.empty()) error = pointer + ".uint64 is not canonical UINT64 decimal";
        return false;
      }
    } else if (parsed.kind == "INT64") {
      const std::set<std::string_view> exact{"id", "kind", "int64"};
      std::string raw;
      if (format != kValuesFormatV3 || !IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "int64", raw, error) || !ParseInt64(raw, parsed.int64_value)) {
        if (error.empty()) {
          error = pointer + ".int64 is not canonical INT64 decimal in Values 0.3";
        }
        return false;
      }
    } else if (parsed.kind == "BYTES") {
      const std::set<std::string_view> exact{"id", "kind", "hex"};
      std::string raw;
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "hex", raw, error) || !ParseUpperHex(raw, parsed.bytes)) {
        if (error.empty()) error = pointer + ".hex is not canonical uppercase bytes";
        return false;
      }
    } else if (parsed.kind == "ENUM") {
      const std::set<std::string_view> exact{"id", "kind", "entry_id"};
      if (!IsObjectWithKeys(value, exact, exact, pointer, error) ||
          !ReadString(value, "entry_id", parsed.enum_entry_id, error))
        return false;
    } else if (parsed.kind == "BOOL") {
      const std::set<std::string_view> exact{"id", "kind", "bool"};
      yyjson_val* boolean = yyjson_obj_get(value, "bool");
      if ((format != kValuesFormatV2 && format != kValuesFormatV3) ||
          !IsObjectWithKeys(value, exact, exact, pointer, error) || !yyjson_is_bool(boolean)) {
        if (error.empty()) {
          error = pointer + ".bool must be a native JSON boolean in Values 0.2";
        }
        return false;
      }
      parsed.bool_value = yyjson_get_bool(boolean);
    } else {
      error = pointer + ".kind is unsupported";
      return false;
    }
    candidate.fields.push_back(std::move(parsed));
    ++ordinal;
  }
  output = std::move(candidate);
  error.clear();
  return true;
}

bool ValuesFormatCompatibleWithSchema(std::string_view values_format,
                                      std::string_view schema_version) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (values_format == kVariableValuesFormat) return schema_version == "0.8";
#else
  static_cast<void>(values_format);
  static_cast<void>(schema_version);
#endif
  return true;
}

}  // namespace pae::protocol_lab::v06::internal
