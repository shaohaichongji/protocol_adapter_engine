#include "config_compiler.h"

#include <yyjson.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../protocol_plan/decimal_conversion_internal.h"
#include "../protocol_plan/plan_builder.h"
#include "../protocol_plan/plan_bundle.h"
#include "../protocol_plan/plan_draft_internal.h"
#include "../protocol_plan/plan_memory.h"
#include "schema_ir.h"
#include "ui_description_internal.h"
#include "validation_pipeline_internal.h"

namespace pae::config_compiler {

namespace {

constexpr yyjson_read_flag kReadFlags = YYJSON_READ_NUMBER_AS_RAW;

struct JsonAuditLimits {
  std::size_t max_input_bytes = 4U * 1024U * 1024U;
  std::size_t max_parser_memory_bytes = 64U * 1024U * 1024U;
  std::size_t max_depth = 64U;
  std::size_t max_nodes = 524288U;
  std::size_t max_object_members = 4096U;
  std::size_t max_array_elements = 16384U;
  std::size_t max_string_bytes = 64U * 1024U;
  std::size_t max_number_token_bytes = 128U;
  std::size_t max_total_decoded_string_bytes = kCompilerDecodedStringHardLimitBytes;
};

struct JsonAuditStats {
  std::size_t node_count = 0U;
  std::size_t max_depth = 0U;
  std::size_t total_decoded_string_bytes = 0U;
};

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }
};

using DocumentPtr = std::unique_ptr<yyjson_doc, DocumentDeleter>;

CompileResult Reject(CompileStage stage, CompileError code, std::string json_pointer,
                     std::string detail, std::optional<std::size_t> byte_offset = std::nullopt) {
  return CompileResult::Failure(
      CompileDiagnostic{stage, code, std::move(json_pointer), byte_offset, std::move(detail)});
}

bool SetDiagnostic(CompileDiagnostic& diagnostic, CompileStage stage, CompileError code,
                   std::string json_pointer, std::string detail,
                   std::optional<std::size_t> byte_offset = std::nullopt) {
  diagnostic =
      CompileDiagnostic{stage, code, std::move(json_pointer), byte_offset, std::move(detail)};
  return false;
}

void AppendJsonPointerToken(std::string_view token, std::string& pointer) {
  pointer.push_back('/');
  for (const char character : token) {
    if (character == '~') {
      pointer.append("~0");
    } else if (character == '/') {
      pointer.append("~1");
    } else {
      pointer.push_back(character);
    }
  }
}

std::string ChildPointer(std::string_view parent, std::string_view token) {
  std::string pointer{parent};
  AppendJsonPointerToken(token, pointer);
  return pointer;
}

std::string IndexPointer(std::string_view parent, std::size_t index) {
  return ChildPointer(parent, std::to_string(index));
}

bool IsContinuationByte(std::uint8_t byte) noexcept { return (byte & 0xC0U) == 0x80U; }

bool ValidateUtf8(std::string_view input, std::size_t& error_offset) noexcept {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(input.data());
  std::size_t index = 0U;
  while (index < input.size()) {
    const std::uint8_t first = bytes[index];
    if (first <= 0x7FU) {
      ++index;
      continue;
    }

    std::size_t sequence_length = 0U;
    std::uint32_t code_point = 0U;
    std::uint32_t minimum = 0U;
    if (first >= 0xC2U && first <= 0xDFU) {
      sequence_length = 2U;
      code_point = first & 0x1FU;
      minimum = 0x80U;
    } else if (first >= 0xE0U && first <= 0xEFU) {
      sequence_length = 3U;
      code_point = first & 0x0FU;
      minimum = 0x800U;
    } else if (first >= 0xF0U && first <= 0xF4U) {
      sequence_length = 4U;
      code_point = first & 0x07U;
      minimum = 0x10000U;
    } else {
      error_offset = index;
      return false;
    }

    if (sequence_length > input.size() - index) {
      error_offset = index;
      return false;
    }
    for (std::size_t continuation_index = 1U; continuation_index < sequence_length;
         ++continuation_index) {
      const std::uint8_t continuation = bytes[index + continuation_index];
      if (!IsContinuationByte(continuation)) {
        error_offset = index + continuation_index;
        return false;
      }
      code_point = (code_point << 6U) | (continuation & 0x3FU);
    }
    if (code_point < minimum || code_point > 0x10FFFFU ||
        (code_point >= 0xD800U && code_point <= 0xDFFFU)) {
      error_offset = index;
      return false;
    }
    index += sequence_length;
  }
  return true;
}

std::size_t CountUtf8ScalarValues(std::string_view input) noexcept {
  return static_cast<std::size_t>(
      std::count_if(input.begin(), input.end(), [](const char character) {
        return !IsContinuationByte(static_cast<std::uint8_t>(character));
      }));
}

bool IsHexDigit(char character) noexcept {
  return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
         (character >= 'A' && character <= 'F');
}

std::uint16_t ParseHexQuad(std::string_view input, std::size_t offset) noexcept {
  std::uint16_t result = 0U;
  for (std::size_t index = 0U; index < 4U; ++index) {
    const char character = input[offset + index];
    result = static_cast<std::uint16_t>(result << 4U);
    if (character >= '0' && character <= '9') {
      result = static_cast<std::uint16_t>(result + static_cast<std::uint16_t>(character - '0'));
    } else if (character >= 'a' && character <= 'f') {
      result =
          static_cast<std::uint16_t>(result + static_cast<std::uint16_t>(character - 'a' + 10));
    } else {
      result =
          static_cast<std::uint16_t>(result + static_cast<std::uint16_t>(character - 'A' + 10));
    }
  }
  return result;
}

bool RunStrictPrecheck(std::string_view input, const JsonAuditLimits& limits,
                       CompileDiagnostic& diagnostic) {
  if (input.empty()) {
    return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE, CompileError::EMPTY_INPUT, "",
                         "configuration input is empty");
  }
  if (input.size() > limits.max_input_bytes) {
    return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE,
                         CompileError::INPUT_LIMIT_EXCEEDED, "",
                         "configuration input exceeds the draft hard limit");
  }
  if (input.size() >= 3U && static_cast<unsigned char>(input[0]) == 0xEFU &&
      static_cast<unsigned char>(input[1]) == 0xBBU &&
      static_cast<unsigned char>(input[2]) == 0xBFU) {
    return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE,
                         CompileError::UTF8_BOM_NOT_ALLOWED, "", "UTF-8 BOM is not allowed", 0U);
  }

  std::size_t utf8_error = 0U;
  if (!ValidateUtf8(input, utf8_error)) {
    return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE, CompileError::INVALID_UTF8, "",
                         "input is not strict UTF-8", utf8_error);
  }

  bool in_string = false;
  std::size_t depth = 0U;
  for (std::size_t index = 0U; index < input.size(); ++index) {
    const char character = input[index];
    if (!in_string) {
      if (character == '"') {
        in_string = true;
      } else if (character == '{' || character == '[') {
        ++depth;
        if (depth > limits.max_depth) {
          return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE,
                               CompileError::NESTING_DEPTH_LIMIT_EXCEEDED, "",
                               "JSON nesting exceeds the draft hard limit", index);
        }
      } else if ((character == '}' || character == ']') && depth != 0U) {
        --depth;
      }
      continue;
    }

    if (character == '"') {
      in_string = false;
      continue;
    }
    if (character != '\\') {
      continue;
    }
    if (index + 1U >= input.size()) {
      break;
    }
    const char escape = input[index + 1U];
    if (escape != 'u') {
      ++index;
      continue;
    }
    if (index + 5U >= input.size() || !IsHexDigit(input[index + 2U]) ||
        !IsHexDigit(input[index + 3U]) || !IsHexDigit(input[index + 4U]) ||
        !IsHexDigit(input[index + 5U])) {
      continue;
    }
    const std::uint16_t first = ParseHexQuad(input, index + 2U);
    if (first >= 0xD800U && first <= 0xDBFFU) {
      if (index + 11U >= input.size() || input[index + 6U] != '\\' || input[index + 7U] != 'u' ||
          !IsHexDigit(input[index + 8U]) || !IsHexDigit(input[index + 9U]) ||
          !IsHexDigit(input[index + 10U]) || !IsHexDigit(input[index + 11U])) {
        return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE,
                             CompileError::INVALID_UNICODE_ESCAPE, "",
                             "high surrogate must be followed by a low surrogate", index);
      }
      const std::uint16_t second = ParseHexQuad(input, index + 8U);
      if (second < 0xDC00U || second > 0xDFFFU) {
        return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE,
                             CompileError::INVALID_UNICODE_ESCAPE, "",
                             "high surrogate must be followed by a low surrogate", index);
      }
      index += 11U;
    } else if (first >= 0xDC00U && first <= 0xDFFFU) {
      return SetDiagnostic(diagnostic, CompileStage::INPUT_PROFILE,
                           CompileError::INVALID_UNICODE_ESCAPE, "",
                           "isolated low surrogate is not allowed", index);
    } else {
      index += 5U;
    }
  }
  return true;
}

bool AddStringBudget(std::size_t length, const JsonAuditLimits& limits, JsonAuditStats& stats,
                     std::string_view pointer, CompileDiagnostic& diagnostic) {
  if (length > limits.max_string_bytes) {
    return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                         CompileError::STRING_LIMIT_EXCEEDED, std::string(pointer),
                         "decoded string exceeds the draft hard limit");
  }
  if (length >
      limits.max_total_decoded_string_bytes -
          (std::min)(stats.total_decoded_string_bytes, limits.max_total_decoded_string_bytes)) {
    return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                         CompileError::DECODED_STRING_BUDGET_EXCEEDED, std::string(pointer),
                         "total decoded string bytes exceed the draft hard limit");
  }
  stats.total_decoded_string_bytes += length;
  return true;
}

bool AuditJsonValue(yyjson_val* value, std::size_t depth, std::string& pointer,
                    const JsonAuditLimits& limits, JsonAuditStats& stats,
                    CompileDiagnostic& diagnostic) {
  if (depth > limits.max_depth) {
    return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                         CompileError::NESTING_DEPTH_LIMIT_EXCEEDED, pointer,
                         "parsed JSON nesting exceeds the draft hard limit");
  }
  if (stats.node_count >= limits.max_nodes) {
    return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                         CompileError::JSON_NODE_LIMIT_EXCEEDED, pointer,
                         "parsed JSON node count exceeds the draft hard limit");
  }
  ++stats.node_count;
  stats.max_depth = (std::max)(stats.max_depth, depth);

  if (yyjson_is_raw(value)) {
    if (yyjson_get_len(value) > limits.max_number_token_bytes) {
      return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                           CompileError::NUMBER_TOKEN_LIMIT_EXCEEDED, pointer,
                           "number token exceeds the draft hard limit");
    }
    return true;
  }
  if (yyjson_is_str(value)) {
    return AddStringBudget(yyjson_get_len(value), limits, stats, pointer, diagnostic);
  }
  if (yyjson_is_arr(value)) {
    const std::size_t count = yyjson_arr_size(value);
    if (count > limits.max_array_elements) {
      return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                           CompileError::ARRAY_ELEMENT_LIMIT_EXCEEDED, pointer,
                           "array element count exceeds the draft hard limit");
    }
    yyjson_arr_iter iterator;
    yyjson_arr_iter_init(value, &iterator);
    std::size_t index = 0U;
    while (yyjson_val* element = yyjson_arr_iter_next(&iterator)) {
      const std::size_t old_size = pointer.size();
      AppendJsonPointerToken(std::to_string(index), pointer);
      if (!AuditJsonValue(element, depth + 1U, pointer, limits, stats, diagnostic)) {
        return false;
      }
      pointer.resize(old_size);
      ++index;
    }
    return true;
  }
  if (yyjson_is_obj(value)) {
    const std::size_t count = yyjson_obj_size(value);
    if (count > limits.max_object_members) {
      return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE,
                           CompileError::OBJECT_MEMBER_LIMIT_EXCEEDED, pointer,
                           "object member count exceeds the draft hard limit");
    }

    std::unordered_set<std::string> keys;
    keys.reserve(count);
    yyjson_obj_iter key_iterator;
    yyjson_obj_iter_init(value, &key_iterator);
    while (yyjson_val* key = yyjson_obj_iter_next(&key_iterator)) {
      const std::string_view key_view{yyjson_get_str(key), yyjson_get_len(key)};
      const std::size_t old_size = pointer.size();
      AppendJsonPointerToken(key_view, pointer);
      if (!AddStringBudget(key_view.size(), limits, stats, pointer, diagnostic)) {
        return false;
      }
      const bool inserted = keys.emplace(key_view).second;
      if (!inserted) {
        return SetDiagnostic(diagnostic, CompileStage::JSON_RESOURCE, CompileError::DUPLICATE_KEY,
                             pointer, "duplicate object key is not allowed");
      }
      pointer.resize(old_size);
    }

    yyjson_obj_iter value_iterator;
    yyjson_obj_iter_init(value, &value_iterator);
    while (yyjson_val* key = yyjson_obj_iter_next(&value_iterator)) {
      yyjson_val* child = yyjson_obj_iter_get_val(key);
      const std::size_t old_size = pointer.size();
      AppendJsonPointerToken(std::string_view{yyjson_get_str(key), yyjson_get_len(key)}, pointer);
      if (!AuditJsonValue(child, depth + 1U, pointer, limits, stats, diagnostic)) {
        return false;
      }
      pointer.resize(old_size);
    }
  }
  return true;
}

bool IsAllowedProperty(std::string_view property,
                       std::initializer_list<std::string_view> allowed) noexcept {
  return std::find(allowed.begin(), allowed.end(), property) != allowed.end();
}

bool ValidateObjectProperties(yyjson_val* object, std::string_view pointer,
                              std::initializer_list<std::string_view> allowed,
                              CompileDiagnostic& diagnostic) {
  std::optional<std::string> first_unknown;
  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(object, &iterator);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string_view key_view{yyjson_get_str(key), yyjson_get_len(key)};
    if (!IsAllowedProperty(key_view, allowed) &&
        (!first_unknown.has_value() || key_view < std::string_view{*first_unknown})) {
      first_unknown = std::string{key_view};
    }
  }
  if (first_unknown.has_value()) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                         ChildPointer(pointer, *first_unknown),
                         "unknown property is not allowed by the V0.1 draft slice");
  }
  return true;
}

yyjson_val* RequiredProperty(yyjson_val* object, std::string_view key, std::string_view pointer,
                             CompileDiagnostic& diagnostic) {
  yyjson_val* value = yyjson_obj_getn(object, key.data(), key.size());
  if (value == nullptr) {
    SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::MISSING_PROPERTY,
                  ChildPointer(pointer, key), "required property is missing");
  }
  return value;
}

bool ReadString(yyjson_val* value, std::string_view pointer, std::size_t min_length,
                std::size_t max_length, std::string& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_str(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "expected a JSON string");
  }
  const std::string_view decoded{yyjson_get_str(value), yyjson_get_len(value)};
  std::size_t invalid_offset = 0U;
  if (!ValidateUtf8(decoded, invalid_offset)) {
    return SetDiagnostic(diagnostic, CompileStage::INTERNAL,
                         CompileError::INTERNAL_CONTRACT_VIOLATION, std::string(pointer),
                         "yyjson returned a decoded string that is not valid UTF-8");
  }
  const std::size_t scalar_count = CountUtf8ScalarValues(decoded);
  if (scalar_count < min_length || scalar_count > max_length) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_STRING_LENGTH,
                         std::string(pointer),
                         "decoded Unicode scalar count is outside the draft Schema range");
  }
  output.assign(decoded.data(), decoded.size());
  return true;
}

bool IsStableId(std::string_view value) noexcept {
  if (value.empty() || value.front() < 'a' || value.front() > 'z') {
    return false;
  }
  return std::all_of(value.begin() + 1, value.end(), [](char character) {
    return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
           character == '_';
  });
}

bool ReadStableId(yyjson_val* value, std::string_view pointer, std::string& output,
                  CompileDiagnostic& diagnostic) {
  if (!ReadString(value, pointer, 1U, 128U, output, diagnostic)) {
    return false;
  }
  if (!IsStableId(output)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ID,
                         std::string(pointer), "stable ID must match ^[a-z][a-z0-9_]*$");
  }
  return true;
}

bool ReadExactUint64(yyjson_val* value, std::string_view pointer, std::uint64_t& output,
                     CompileDiagnostic& diagnostic) {
  if (!yyjson_is_raw(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "expected an unsigned integer JSON token");
  }
  const std::string_view token{yyjson_get_raw(value), yyjson_get_len(value)};
  if (token.empty()) {
    return SetDiagnostic(diagnostic, CompileStage::INTERNAL,
                         CompileError::INTERNAL_CONTRACT_VIOLATION, std::string(pointer),
                         "yyjson returned an empty raw number token");
  }
  if (token.front() == '-') {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         std::string(pointer),
                         "negative token is not valid for an unsigned property");
  }
  if (!std::all_of(token.begin(), token.end(),
                   [](char character) { return character >= '0' && character <= '9'; })) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_NOT_EXACT,
                         std::string(pointer),
                         "fraction and exponent forms are not accepted for integer properties");
  }

  std::uint64_t parsed = 0U;
  for (const char character : token) {
    const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
    if (parsed > (std::numeric_limits<std::uint64_t>::max() - digit) / 10U) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                           std::string(pointer), "unsigned integer token exceeds UINT64_MAX");
    }
    parsed = parsed * 10U + digit;
  }
  output = parsed;
  return true;
}

bool ReadExactInt64(yyjson_val* value, std::string_view pointer, std::int64_t& output,
                    CompileDiagnostic& diagnostic) {
  if (!yyjson_is_raw(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "expected a signed integer JSON token");
  }
  const std::string_view token{yyjson_get_raw(value), yyjson_get_len(value)};
  if (token.empty()) {
    return SetDiagnostic(diagnostic, CompileStage::INTERNAL,
                         CompileError::INTERNAL_CONTRACT_VIOLATION, std::string(pointer),
                         "yyjson returned an empty raw number token");
  }
  const bool negative = token.front() == '-';
  const std::string_view digits = negative ? token.substr(1U) : token;
  if (digits.empty() || !std::all_of(digits.begin(), digits.end(), [](char character) {
        return character >= '0' && character <= '9';
      })) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_NOT_EXACT,
                         std::string(pointer),
                         "fraction and exponent forms are not accepted for integer properties");
  }
  const std::uint64_t limit =
      negative ? std::uint64_t{1U} << 63U
               : static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
  std::uint64_t magnitude = 0U;
  for (const char character : digits) {
    const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
    if (magnitude > (limit - digit) / 10U) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                           std::string(pointer), "signed integer token is outside INT64 range");
    }
    magnitude = magnitude * 10U + digit;
  }
  if (!negative || magnitude == 0U) {
    output = static_cast<std::int64_t>(magnitude);
  } else if (magnitude == (std::uint64_t{1U} << 63U)) {
    output = (std::numeric_limits<std::int64_t>::min)();
  } else {
    output = -static_cast<std::int64_t>(magnitude);
  }
  return true;
}

bool ReadRequiredString(yyjson_val* object, std::string_view key, std::string_view pointer,
                        std::size_t min_length, std::size_t max_length, std::string& output,
                        CompileDiagnostic& diagnostic) {
  yyjson_val* value = RequiredProperty(object, key, pointer, diagnostic);
  return value != nullptr &&
         ReadString(value, ChildPointer(pointer, key), min_length, max_length, output, diagnostic);
}

bool ReadRequiredId(yyjson_val* object, std::string_view key, std::string_view pointer,
                    std::string& output, CompileDiagnostic& diagnostic) {
  yyjson_val* value = RequiredProperty(object, key, pointer, diagnostic);
  return value != nullptr && ReadStableId(value, ChildPointer(pointer, key), output, diagnostic);
}

bool ReadRequiredUint64(yyjson_val* object, std::string_view key, std::string_view pointer,
                        std::uint64_t& output, CompileDiagnostic& diagnostic) {
  yyjson_val* value = RequiredProperty(object, key, pointer, diagnostic);
  return value != nullptr && ReadExactUint64(value, ChildPointer(pointer, key), output, diagnostic);
}

#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
bool ReadRequiredBool(yyjson_val* object, std::string_view key, std::string_view pointer,
                      bool& output, CompileDiagnostic& diagnostic) {
  yyjson_val* value = RequiredProperty(object, key, pointer, diagnostic);
  if (value == nullptr) {
    return false;
  }
  if (!yyjson_is_bool(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         ChildPointer(pointer, key), "expected a JSON boolean");
  }
  output = yyjson_get_bool(value);
  return true;
}

bool ReadRequiredCrcHex(yyjson_val* object, std::string_view key, std::string_view pointer,
                        std::size_t digits, std::uint32_t& output, CompileDiagnostic& diagnostic) {
  std::string token;
  if (!ReadRequiredString(object, key, pointer, digits, digits, token, diagnostic)) {
    return false;
  }
  std::uint32_t parsed = 0U;
  for (const char character : token) {
    std::uint32_t nibble = 0U;
    if (character >= '0' && character <= '9') {
      nibble = static_cast<std::uint32_t>(character - '0');
    } else if (character >= 'A' && character <= 'F') {
      nibble = static_cast<std::uint32_t>(character - 'A' + 10);
    } else {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                           ChildPointer(pointer, key),
                           "CRC hexadecimal value must use fixed-width uppercase hexadecimal");
    }
    parsed = static_cast<std::uint32_t>((parsed << 4U) | nibble);
  }
  output = parsed;
  return true;
}
#endif

bool ParseHexBytes(std::string_view input, std::vector<std::uint8_t>& output) {
  if (input.size() < 2U || (input.size() + 1U) % 3U != 0U) {
    return false;
  }
  output.clear();
  output.reserve((input.size() + 1U) / 3U);
  for (std::size_t index = 0U; index < input.size(); index += 3U) {
    const char high = input[index];
    const char low = input[index + 1U];
    const auto HexValue = [](char character) -> std::optional<std::uint8_t> {
      if (character >= '0' && character <= '9') {
        return static_cast<std::uint8_t>(character - '0');
      }
      if (character >= 'A' && character <= 'F') {
        return static_cast<std::uint8_t>(character - 'A' + 10);
      }
      return std::nullopt;
    };
    const auto high_value = HexValue(high);
    const auto low_value = HexValue(low);
    if (!high_value.has_value() || !low_value.has_value()) {
      return false;
    }
    output.push_back(static_cast<std::uint8_t>((*high_value << 4U) | *low_value));
    if (index + 2U < input.size() && input[index + 2U] != ' ') {
      return false;
    }
  }
  return true;
}

bool ReadEnumToken(yyjson_val* value, std::string_view pointer,
                   std::initializer_list<std::string_view> allowed, std::string& output,
                   CompileDiagnostic& diagnostic) {
  if (!ReadString(value, pointer, 1U, 64U, output, diagnostic)) {
    return false;
  }
  if (!IsAllowedProperty(output, allowed)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                         std::string(pointer),
                         "string is not one of the allowed draft Schema enum values");
  }
  return true;
}

bool ParseFramingProfile(yyjson_val* value, std::string_view pointer, bool supports_stream,
                         FramingProfileIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "framing profile must be an object");
  }
  if (!ValidateObjectProperties(
          value, pointer,
          supports_stream
              ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                        "source_ref", "input_kind", "strategy",
                                                        "frame_length_bytes", "sync_bytes",
                                                        "length_field", "minimum_frame_length",
                                                        "maximum_frame_length"}
              : std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                        "source_ref", "input_kind"},
          diagnostic)) {
    return false;
  }
  if (!ReadRequiredId(value, "id", pointer, output.id, diagnostic) ||
      !ReadRequiredString(value, "display_name", pointer, 1U, 256U, output.display_name,
                          diagnostic) ||
      !ReadRequiredString(value, "description", pointer, 0U, 1024U, output.description,
                          diagnostic) ||
      !ReadRequiredString(value, "source_ref", pointer, 1U, 512U, output.source_ref, diagnostic)) {
    return false;
  }
  yyjson_val* input_kind = RequiredProperty(value, "input_kind", pointer, diagnostic);
  std::string input_kind_token;
  if (input_kind == nullptr ||
      !ReadEnumToken(input_kind, ChildPointer(pointer, "input_kind"),
                     supports_stream ? std::initializer_list<std::string_view>{"complete_record",
                                                                               "stream_chunk"}
                                     : std::initializer_list<std::string_view>{"complete_record"},
                     input_kind_token, diagnostic)) {
    return false;
  }
  output.input_kind = InputKind::COMPLETE_RECORD;
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  if (input_kind_token == "stream_chunk") {
    output.input_kind = InputKind::STREAM_CHUNK;
    yyjson_val* strategy_value = RequiredProperty(value, "strategy", pointer, diagnostic);
    std::string strategy;
    if (strategy_value == nullptr ||
        !ReadEnumToken(strategy_value, ChildPointer(pointer, "strategy"),
                       {"fixed_length", "sync_fixed_length", "sync_length_field"}, strategy,
                       diagnostic)) {
      return false;
    }
    if (strategy == "fixed_length" || strategy == "sync_fixed_length") {
      output.strategy = strategy == "fixed_length" ? FramingStrategy::FIXED_LENGTH
                                                   : FramingStrategy::SYNC_FIXED_LENGTH;
      if (!ReadRequiredUint64(value, "frame_length_bytes", pointer, output.frame_length_bytes,
                              diagnostic)) {
        return false;
      }
      const auto reject_foreign_member = [&](std::string_view name) {
        return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                             ChildPointer(pointer, name),
                             "framing strategy contains a member from another strategy");
      };
      if (yyjson_obj_get(value, "length_field") != nullptr)
        return reject_foreign_member("length_field");
      if (yyjson_obj_get(value, "minimum_frame_length") != nullptr)
        return reject_foreign_member("minimum_frame_length");
      if (yyjson_obj_get(value, "maximum_frame_length") != nullptr)
        return reject_foreign_member("maximum_frame_length");
      if (strategy == "fixed_length" && yyjson_obj_get(value, "sync_bytes") != nullptr)
        return reject_foreign_member("sync_bytes");
    } else {
      output.strategy = FramingStrategy::SYNC_LENGTH_FIELD;
      if (yyjson_obj_get(value, "frame_length_bytes") != nullptr) {
        return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                             ChildPointer(pointer, "frame_length_bytes"),
                             "framing strategy contains a member from another strategy");
      }
      if (!ReadRequiredUint64(value, "minimum_frame_length", pointer, output.minimum_frame_length,
                              diagnostic) ||
          !ReadRequiredUint64(value, "maximum_frame_length", pointer, output.maximum_frame_length,
                              diagnostic)) {
        return false;
      }
      yyjson_val* length_field = RequiredProperty(value, "length_field", pointer, diagnostic);
      const std::string length_pointer = ChildPointer(pointer, "length_field");
      if (length_field == nullptr || !yyjson_is_obj(length_field)) {
        return length_field == nullptr ? false
                                       : SetDiagnostic(diagnostic, CompileStage::STRUCTURAL,
                                                       CompileError::TYPE_MISMATCH, length_pointer,
                                                       "length_field must be an object");
      }
      if (!ValidateObjectProperties(length_field, length_pointer,
                                    {"byte_offset", "byte_width", "byte_order"}, diagnostic) ||
          !ReadRequiredUint64(length_field, "byte_offset", length_pointer,
                              output.length_field_offset, diagnostic) ||
          !ReadRequiredUint64(length_field, "byte_width", length_pointer, output.length_field_width,
                              diagnostic)) {
        return false;
      }
      if (output.length_field_width != 1U && output.length_field_width != 2U &&
          output.length_field_width != 4U) {
        return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                             ChildPointer(length_pointer, "byte_width"),
                             "stream length_field byte_width must be 1, 2, or 4");
      }
      yyjson_val* byte_order = yyjson_obj_get(length_field, "byte_order");
      if (output.length_field_width == 1U) {
        if (byte_order != nullptr) {
          return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                               ChildPointer(length_pointer, "byte_order"),
                               "one-byte length_field must omit byte_order");
        }
      } else {
        std::string order;
        if (byte_order == nullptr ||
            !ReadEnumToken(byte_order, ChildPointer(length_pointer, "byte_order"),
                           {"big_endian", "little_endian"}, order, diagnostic)) {
          return byte_order == nullptr
                     ? SetDiagnostic(diagnostic, CompileStage::STRUCTURAL,
                                     CompileError::MISSING_PROPERTY,
                                     ChildPointer(length_pointer, "byte_order"),
                                     "multi-byte length_field requires byte_order")
                     : false;
        }
        output.length_field_byte_order = order == "big_endian" ? ByteOrder::BIG : ByteOrder::LITTLE;
      }
    }
    if (strategy != "fixed_length") {
      yyjson_val* sync = RequiredProperty(value, "sync_bytes", pointer, diagnostic);
      std::string sync_text;
      if (sync == nullptr || !ReadString(sync, ChildPointer(pointer, "sync_bytes"), 1U,
                                         3U * 64U - 1U, sync_text, diagnostic)) {
        return false;
      }
      if (!ParseHexBytes(sync_text, output.sync_bytes) || output.sync_bytes.empty()) {
        return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_HEX_BYTES,
                             ChildPointer(pointer, "sync_bytes"),
                             "sync_bytes must be a non-empty canonical hex byte string");
      }
    }
  } else {
    for (const std::string_view name :
         {"strategy", "frame_length_bytes", "sync_bytes", "length_field", "minimum_frame_length",
          "maximum_frame_length"}) {
      if (yyjson_obj_get(value, std::string{name}.c_str()) != nullptr) {
        return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                             ChildPointer(pointer, name),
                             "complete_record framing profile cannot contain stream members");
      }
    }
  }
#else
  static_cast<void>(supports_stream);
#endif
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseStringIdArray(yyjson_val* value, std::string_view pointer,
                        std::vector<std::string>& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_arr(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "expected an array of stable IDs");
  }
  if (yyjson_arr_size(value) == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::EMPTY_ARRAY,
                         std::string(pointer), "array must contain at least one item");
  }
  output.clear();
  output.reserve(yyjson_arr_size(value));
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(value, &iterator);
  std::size_t index = 0U;
  while (yyjson_val* element = yyjson_arr_iter_next(&iterator)) {
    std::string id;
    if (!ReadStableId(element, IndexPointer(pointer, index), id, diagnostic)) {
      return false;
    }
    output.push_back(std::move(id));
    ++index;
  }
  return true;
}

bool ParsePipeline(yyjson_val* value, std::string_view pointer, PipelineIr& output,
                   CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "pipeline must be an object");
  }
  if (!ValidateObjectProperties(value, pointer,
                                {"id", "display_name", "description", "source_ref", "direction_id",
                                 "input_framing_profile_id", "message_ids"},
                                diagnostic)) {
    return false;
  }
  if (!ReadRequiredId(value, "id", pointer, output.id, diagnostic) ||
      !ReadRequiredString(value, "display_name", pointer, 1U, 256U, output.display_name,
                          diagnostic) ||
      !ReadRequiredString(value, "description", pointer, 0U, 1024U, output.description,
                          diagnostic) ||
      !ReadRequiredString(value, "source_ref", pointer, 1U, 512U, output.source_ref, diagnostic) ||
      !ReadRequiredId(value, "direction_id", pointer, output.direction_id, diagnostic) ||
      !ReadRequiredId(value, "input_framing_profile_id", pointer, output.input_framing_profile_id,
                      diagnostic)) {
    return false;
  }
  yyjson_val* message_ids = RequiredProperty(value, "message_ids", pointer, diagnostic);
  if (message_ids == nullptr ||
      !ParseStringIdArray(message_ids, ChildPointer(pointer, "message_ids"), output.message_ids,
                          diagnostic)) {
    return false;
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseMatcherClause(yyjson_val* value, std::string_view pointer, MatcherClauseIr& output,
                        CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "matcher clause must be an object");
  }
  yyjson_val* kind_value = RequiredProperty(value, "kind", pointer, diagnostic);
  std::string kind;
  if (kind_value == nullptr ||
      !ReadEnumToken(kind_value, ChildPointer(pointer, "kind"),
                     {"frame_length_equals", "fixed_bytes"}, kind, diagnostic)) {
    return false;
  }

  if (kind == "frame_length_equals") {
    if (!ValidateObjectProperties(value, pointer, {"kind", "length_bytes"}, diagnostic) ||
        !ReadRequiredUint64(value, "length_bytes", pointer, output.length_bytes, diagnostic)) {
      return false;
    }
    if (output.length_bytes == 0U || output.length_bytes > 1024U * 1024U) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                           ChildPointer(pointer, "length_bytes"),
                           "matcher length must be in the range 1..1048576");
    }
    output.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  } else {
    if (!ValidateObjectProperties(value, pointer, {"kind", "byte_offset", "bytes"}, diagnostic) ||
        !ReadRequiredUint64(value, "byte_offset", pointer, output.byte_offset, diagnostic)) {
      return false;
    }
    if (output.byte_offset >= 1024U * 1024U) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                           ChildPointer(pointer, "byte_offset"),
                           "matcher byte offset must be less than 1048576");
    }
    std::string bytes;
    if (!ReadRequiredString(value, "bytes", pointer, 2U, 3U * 1024U * 1024U, bytes, diagnostic)) {
      return false;
    }
    if (!ParseHexBytes(bytes, output.bytes)) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_HEX_BYTES,
                           ChildPointer(pointer, "bytes"),
                           "fixed byte pattern must be uppercase 'AA BB' form");
    }
    output.kind = MatcherKind::FIXED_BYTES;
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseMatcher(yyjson_val* value, std::string_view pointer, std::vector<MatcherClauseIr>& output,
                  CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "matcher must be an object");
  }
  if (!ValidateObjectProperties(value, pointer, {"all"}, diagnostic)) {
    return false;
  }
  yyjson_val* all = RequiredProperty(value, "all", pointer, diagnostic);
  const std::string all_pointer = ChildPointer(pointer, "all");
  if (all == nullptr) {
    return false;
  }
  if (!yyjson_is_arr(all)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         all_pointer, "matcher all must be an array");
  }
  if (yyjson_arr_size(all) == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::EMPTY_ARRAY,
                         all_pointer, "matcher all must contain at least one clause");
  }
  output.clear();
  output.reserve(yyjson_arr_size(all));
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(all, &iterator);
  std::size_t index = 0U;
  while (yyjson_val* clause_value = yyjson_arr_iter_next(&iterator)) {
    MatcherClauseIr clause;
    if (!ParseMatcherClause(clause_value, IndexPointer(all_pointer, index), clause, diagnostic)) {
      return false;
    }
    output.push_back(std::move(clause));
    ++index;
  }
  return true;
}

bool ParseUnsignedWire(yyjson_val* value, std::string_view pointer, WireIr& output,
                       CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "wire must be an object");
  }
  if (!ValidateObjectProperties(value, pointer,
                                {"codec", "byte_offset", "byte_width", "byte_order"}, diagnostic)) {
    return false;
  }
  yyjson_val* codec = RequiredProperty(value, "codec", pointer, diagnostic);
  std::string codec_token;
  if (codec == nullptr ||
      !ReadEnumToken(codec, ChildPointer(pointer, "codec"), {"unsigned_integer"}, codec_token,
                     diagnostic) ||
      !ReadRequiredUint64(value, "byte_offset", pointer, output.byte_offset, diagnostic) ||
      !ReadRequiredUint64(value, "byte_width", pointer, output.byte_width, diagnostic)) {
    return false;
  }
  if (output.byte_offset >= 1024U * 1024U || output.byte_width == 0U || output.byte_width > 8U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         output.byte_width == 0U || output.byte_width > 8U
                             ? ChildPointer(pointer, "byte_width")
                             : ChildPointer(pointer, "byte_offset"),
                         "unsigned integer wire offset or width is outside the draft Schema range");
  }

  yyjson_val* byte_order = yyjson_obj_get(value, "byte_order");
  if (output.byte_width > 1U && byte_order == nullptr) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::MISSING_PROPERTY,
                         ChildPointer(pointer, "byte_order"),
                         "multi-byte integer requires an explicit byte order");
  }
  if (byte_order != nullptr) {
    std::string byte_order_token;
    if (!ReadEnumToken(byte_order, ChildPointer(pointer, "byte_order"),
                       {"big_endian", "little_endian"}, byte_order_token, diagnostic)) {
      return false;
    }
    output.byte_order = byte_order_token == "big_endian" ? ByteOrder::BIG : ByteOrder::LITTLE;
  } else {
    output.byte_order = ByteOrder::NOT_APPLICABLE;
  }
  output.codec = WireCodec::UNSIGNED_INTEGER;
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseBytesWire(yyjson_val* value, std::string_view pointer, bool allow_omitted_length,
                    WireIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "wire must be an object");
  }
  if (!ValidateObjectProperties(value, pointer, {"codec", "byte_offset", "byte_length"},
                                diagnostic)) {
    return false;
  }
  yyjson_val* codec = RequiredProperty(value, "codec", pointer, diagnostic);
  std::string codec_token;
  if (codec == nullptr ||
      !ReadEnumToken(codec, ChildPointer(pointer, "codec"), {"bytes"}, codec_token, diagnostic) ||
      !ReadRequiredUint64(value, "byte_offset", pointer, output.byte_offset, diagnostic)) {
    return false;
  }
  yyjson_val* byte_length = yyjson_obj_get(value, "byte_length");
  if (byte_length == nullptr) {
    if (!allow_omitted_length) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::MISSING_PROPERTY,
                           ChildPointer(pointer, "byte_length"),
                           "fixed BYTES wire requires byte_length");
    }
    output.byte_width = 0U;
  } else if (!ReadExactUint64(byte_length, ChildPointer(pointer, "byte_length"), output.byte_width,
                              diagnostic)) {
    return false;
  }
  if (output.byte_offset >= 1024U * 1024U || (!allow_omitted_length && output.byte_width == 0U) ||
      output.byte_width > 1024U * 1024U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         output.byte_width == 0U || output.byte_width > 1024U * 1024U
                             ? ChildPointer(pointer, "byte_length")
                             : ChildPointer(pointer, "byte_offset"),
                         "byte wire offset or length is outside the draft Schema range");
  }
  output.codec = WireCodec::BYTES;
  output.byte_order = ByteOrder::NOT_APPLICABLE;
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseBitfieldWire(yyjson_val* value, std::string_view pointer, WireIr& output,
                       CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "wire must be an object");
  }
  if (!ValidateObjectProperties(value, pointer,
                                {"codec", "container_id", "bit_offset", "bit_width"}, diagnostic)) {
    return false;
  }
  yyjson_val* codec = RequiredProperty(value, "codec", pointer, diagnostic);
  std::string codec_token;
  if (codec == nullptr ||
      !ReadEnumToken(codec, ChildPointer(pointer, "codec"), {"bitfield"}, codec_token,
                     diagnostic) ||
      !ReadRequiredId(value, "container_id", pointer, output.container_id, diagnostic) ||
      !ReadRequiredUint64(value, "bit_offset", pointer, output.bit_offset, diagnostic) ||
      !ReadRequiredUint64(value, "bit_width", pointer, output.bit_width, diagnostic)) {
    return false;
  }
  if (output.bit_offset >= 64U || output.bit_width == 0U || output.bit_width > 64U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         output.bit_width == 0U || output.bit_width > 64U
                             ? ChildPointer(pointer, "bit_width")
                             : ChildPointer(pointer, "bit_offset"),
                         "bitfield offset and width must be within a 64-bit container");
  }
  output.codec = WireCodec::BITFIELD;
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseBitContainer(yyjson_val* value, std::string_view pointer, BitContainerIr& output,
                       CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "bit container must be an object");
  }
  if (!ValidateObjectProperties(value, pointer,
                                {"id", "container_offset", "container_width", "byte_order",
                                 "bit_numbering", "base_value"},
                                diagnostic) ||
      !ReadRequiredId(value, "id", pointer, output.id, diagnostic) ||
      !ReadRequiredUint64(value, "container_offset", pointer, output.byte_offset, diagnostic) ||
      !ReadRequiredUint64(value, "container_width", pointer, output.byte_width, diagnostic) ||
      !ReadRequiredUint64(value, "base_value", pointer, output.base_value, diagnostic)) {
    return false;
  }
  if (output.byte_width != 1U && output.byte_width != 2U && output.byte_width != 4U &&
      output.byte_width != 8U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                         ChildPointer(pointer, "container_width"),
                         "bit container width must be 1, 2, 4, or 8 bytes");
  }
  yyjson_val* order = yyjson_obj_get(value, "byte_order");
  if (output.byte_width > 1U && order == nullptr) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::MISSING_PROPERTY,
                         ChildPointer(pointer, "byte_order"),
                         "multi-byte bit container requires an explicit byte order");
  }
  if (output.byte_width == 1U && order != nullptr) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                         ChildPointer(pointer, "byte_order"),
                         "single-byte bit container must omit byte_order");
  }
  if (order != nullptr) {
    std::string token;
    if (!ReadEnumToken(order, ChildPointer(pointer, "byte_order"), {"big_endian", "little_endian"},
                       token, diagnostic)) {
      return false;
    }
    output.byte_order = token == "big_endian" ? ByteOrder::BIG : ByteOrder::LITTLE;
  }
  yyjson_val* numbering = RequiredProperty(value, "bit_numbering", pointer, diagnostic);
  std::string token;
  if (numbering == nullptr || !ReadEnumToken(numbering, ChildPointer(pointer, "bit_numbering"),
                                             {"lsb0", "msb0"}, token, diagnostic)) {
    return false;
  }
  output.bit_numbering = token == "lsb0" ? BitNumbering::LSB0 : BitNumbering::MSB0;
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseEncode(yyjson_val* value, std::string_view pointer, ValueType value_type,
                 bool supports_computed_length, EncodeIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "encode must be an object");
  }
  yyjson_val* source = RequiredProperty(value, "source", pointer, diagnostic);
  std::string source_token;
  if (source == nullptr ||
      !ReadString(source, ChildPointer(pointer, "source"), 1U, 32U, source_token, diagnostic)) {
    return false;
  }
  if (source_token == "computed") {
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (supports_computed_length && value_type == ValueType::UINT64) {
      if (!ValidateObjectProperties(value, pointer, {"source"}, diagnostic)) return false;
      output.source = EncodeSource::COMPUTED;
      output.origin.json_pointer = std::string{pointer};
      return true;
    }
#else
    static_cast<void>(supports_computed_length);
#endif
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNSUPPORTED_FEATURE,
                         ChildPointer(pointer, "source"),
                         "computed length requires a Schema 0.7 UINT64 field");
  }
  if (source_token == "input") {
    if (!ValidateObjectProperties(value, pointer, {"source"}, diagnostic)) {
      return false;
    }
    output.source = EncodeSource::INPUT;
    output.origin.json_pointer = std::string{pointer};
    return true;
  }
  if (source_token != "constant") {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                         ChildPointer(pointer, "source"),
                         "encode source must be input or constant in this draft slice");
  }
  if (value_type != ValueType::UINT64 && value_type != ValueType::INT64) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNSUPPORTED_FEATURE,
                         ChildPointer(pointer, "source"),
                         "constant encode source is supported only for UINT64 and INT64");
  }
  if (!ValidateObjectProperties(value, pointer, {"source", "value"}, diagnostic)) {
    return false;
  }
  output.source = EncodeSource::CONSTANT;
  yyjson_val* constant = RequiredProperty(value, "value", pointer, diagnostic);
  if (constant == nullptr) {
    return false;
  }
  if (value_type == ValueType::INT64) {
    std::int64_t parsed = 0;
    if (!ReadExactInt64(constant, ChildPointer(pointer, "value"), parsed, diagnostic)) {
      return false;
    }
    output.signed_constant_value = parsed;
  } else {
    std::uint64_t parsed = 0U;
    if (!ReadExactUint64(constant, ChildPointer(pointer, "value"), parsed, diagnostic)) {
      return false;
    }
    output.constant_value = parsed;
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
bool ParseComputedLength(yyjson_val* value, std::string_view pointer, bool supports_payload_scope,
                         ComputedLengthIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string{pointer}, "computed length must be an object");
  }
  yyjson_val* kind = RequiredProperty(value, "kind", pointer, diagnostic);
  yyjson_val* scope = RequiredProperty(value, "scope", pointer, diagnostic);
  std::string kind_token;
  std::string scope_token;
  if (kind == nullptr || scope == nullptr ||
      !ReadEnumToken(kind, ChildPointer(pointer, "kind"), {"length"}, kind_token, diagnostic) ||
      !ReadEnumToken(scope, ChildPointer(pointer, "scope"),
                     supports_payload_scope
                         ? std::initializer_list<std::string_view>{"frame", "region", "payload"}
                         : std::initializer_list<std::string_view>{"frame", "region"},
                     scope_token, diagnostic)) {
    return false;
  }
  output.origin.json_pointer = std::string{pointer};
  if (scope_token == "frame") {
    if (!ValidateObjectProperties(value, pointer, {"kind", "scope"}, diagnostic)) return false;
    output.scope = ComputedLengthScope::FRAME;
    return true;
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (scope_token == "payload") {
    if (!ValidateObjectProperties(value, pointer, {"kind", "scope"}, diagnostic)) return false;
    output.scope = ComputedLengthScope::PAYLOAD;
    return true;
  }
#else
  static_cast<void>(supports_payload_scope);
#endif
  if (!ValidateObjectProperties(value, pointer, {"kind", "scope", "range"}, diagnostic))
    return false;
  yyjson_val* range = RequiredProperty(value, "range", pointer, diagnostic);
  const std::string range_pointer = ChildPointer(pointer, "range");
  if (range == nullptr || !yyjson_is_obj(range)) {
    if (range != nullptr) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                           range_pointer, "computed length range must be an object");
    }
    return false;
  }
  if (!ValidateObjectProperties(range, range_pointer, {"byte_offset", "byte_length"}, diagnostic) ||
      !ReadRequiredUint64(range, "byte_offset", range_pointer, output.range_offset, diagnostic) ||
      !ReadRequiredUint64(range, "byte_length", range_pointer, output.range_length, diagnostic)) {
    return false;
  }
  output.scope = ComputedLengthScope::REGION;
  output.range_origin.json_pointer = range_pointer;
  return true;
}
#endif

bool ParseEnumEntries(yyjson_val* value, std::string_view pointer, std::vector<EnumEntryIr>& output,
                      CompileDiagnostic& diagnostic) {
  if (!yyjson_is_arr(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "enum_entries must be an array");
  }
  if (yyjson_arr_size(value) == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::EMPTY_ARRAY,
                         std::string(pointer), "enum_entries must not be empty");
  }
  output.clear();
  output.reserve(yyjson_arr_size(value));
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(value, &iterator);
  std::size_t index = 0U;
  while (yyjson_val* entry_value = yyjson_arr_iter_next(&iterator)) {
    const std::string entry_pointer = IndexPointer(pointer, index);
    if (!yyjson_is_obj(entry_value)) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                           entry_pointer, "enum entry must be an object");
    }
    if (!ValidateObjectProperties(entry_value, entry_pointer, {"id", "display_name", "raw_value"},
                                  diagnostic)) {
      return false;
    }
    EnumEntryIr entry;
    if (!ReadRequiredId(entry_value, "id", entry_pointer, entry.id, diagnostic) ||
        !ReadRequiredString(entry_value, "display_name", entry_pointer, 1U, 256U,
                            entry.display_name, diagnostic) ||
        !ReadRequiredUint64(entry_value, "raw_value", entry_pointer, entry.raw_value, diagnostic)) {
      return false;
    }
    entry.origin.json_pointer = entry_pointer;
    output.push_back(std::move(entry));
    ++index;
  }
  return true;
}

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
bool ParseRational(yyjson_val* value, std::string_view pointer, RationalIr& output,
                   CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "rational parameter must be an object");
  }
  if (!ValidateObjectProperties(value, pointer, {"numerator", "denominator"}, diagnostic)) {
    return false;
  }
  yyjson_val* numerator = RequiredProperty(value, "numerator", pointer, diagnostic);
  yyjson_val* denominator = RequiredProperty(value, "denominator", pointer, diagnostic);
  if (numerator == nullptr || denominator == nullptr ||
      !ReadExactInt64(numerator, ChildPointer(pointer, "numerator"), output.numerator,
                      diagnostic) ||
      !ReadExactUint64(denominator, ChildPointer(pointer, "denominator"), output.denominator,
                       diagnostic)) {
    return false;
  }
  if (output.denominator == 0U || output.denominator > 1000000000000000000ULL) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         ChildPointer(pointer, "denominator"),
                         "denominator must be in the author range 1..1000000000000000000");
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseConversion(yyjson_val* value, std::string_view pointer, LinearConversionIr& output,
                     CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "conversion must be an object");
  }
  if (!ValidateObjectProperties(value, pointer, {"kind", "output_type", "scale", "bias"},
                                diagnostic)) {
    return false;
  }
  yyjson_val* kind = RequiredProperty(value, "kind", pointer, diagnostic);
  yyjson_val* output_type = RequiredProperty(value, "output_type", pointer, diagnostic);
  std::string kind_token;
  std::string output_token;
  if (kind == nullptr || output_type == nullptr ||
      !ReadEnumToken(kind, ChildPointer(pointer, "kind"), {"linear"}, kind_token, diagnostic) ||
      !ReadEnumToken(output_type, ChildPointer(pointer, "output_type"), {"DECIMAL64"}, output_token,
                     diagnostic)) {
    return false;
  }
  yyjson_val* scale = RequiredProperty(value, "scale", pointer, diagnostic);
  yyjson_val* bias = RequiredProperty(value, "bias", pointer, diagnostic);
  if (scale == nullptr || bias == nullptr ||
      !ParseRational(scale, ChildPointer(pointer, "scale"), output.scale, diagnostic) ||
      !ParseRational(bias, ChildPointer(pointer, "bias"), output.bias, diagnostic)) {
    return false;
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}
#endif

bool ParseField(yyjson_val* value, std::string_view pointer, bool supports_bitfields,
                bool supports_int64, bool supports_conversion, bool supports_computed_length,
                bool variable_layout, FieldIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "field must be an object");
  }
  yyjson_val* value_type_value = RequiredProperty(value, "value_type", pointer, diagnostic);
  std::string value_type;
  if (value_type_value == nullptr ||
      !ReadEnumToken(
          value_type_value, ChildPointer(pointer, "value_type"),
          supports_int64
              ? std::initializer_list<std::string_view>{"UINT64", "INT64", "BYTES", "ENUM", "BOOL"}
          : supports_bitfields
              ? std::initializer_list<std::string_view>{"UINT64", "BYTES", "ENUM", "BOOL"}
              : std::initializer_list<std::string_view>{"UINT64", "BYTES", "ENUM"},
          value_type, diagnostic)) {
    return false;
  }
  if (value_type == "UINT64") {
    output.value_type = ValueType::UINT64;
    if (!ValidateObjectProperties(
            value, pointer,
            supports_computed_length
                ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                          "source_ref", "value_type", "wire",
                                                          "encode", "computed", "conversion"}
            : supports_conversion
                ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                          "source_ref", "value_type", "wire",
                                                          "encode", "conversion"}
                : std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                          "source_ref", "value_type", "wire",
                                                          "encode"},
            diagnostic)) {
      return false;
    }
  } else if (value_type == "INT64") {
    output.value_type = ValueType::INT64;
    if (!ValidateObjectProperties(
            value, pointer,
            supports_conversion
                ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                          "source_ref", "value_type", "wire",
                                                          "encode", "conversion"}
                : std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                          "source_ref", "value_type", "wire",
                                                          "encode"},
            diagnostic)) {
      return false;
    }
  } else if (value_type == "BYTES") {
    output.value_type = ValueType::BYTES;
    if (!ValidateObjectProperties(
            value, pointer,
            {"id", "display_name", "description", "source_ref", "value_type", "wire", "encode"},
            diagnostic)) {
      return false;
    }
  } else if (value_type == "ENUM") {
    output.value_type = ValueType::ENUM;
    if (!ValidateObjectProperties(value, pointer,
                                  {"id", "display_name", "description", "source_ref", "value_type",
                                   "wire", "encode", "unknown_enum_policy", "enum_entries"},
                                  diagnostic)) {
      return false;
    }
  } else {
    output.value_type = ValueType::BOOL;
    if (!ValidateObjectProperties(
            value, pointer,
            {"id", "display_name", "description", "source_ref", "value_type", "wire", "encode"},
            diagnostic)) {
      return false;
    }
  }

  if (!ReadRequiredId(value, "id", pointer, output.id, diagnostic) ||
      !ReadRequiredString(value, "display_name", pointer, 1U, 256U, output.display_name,
                          diagnostic) ||
      !ReadRequiredString(value, "description", pointer, 0U, 1024U, output.description,
                          diagnostic) ||
      !ReadRequiredString(value, "source_ref", pointer, 1U, 512U, output.source_ref, diagnostic)) {
    return false;
  }

  yyjson_val* wire = RequiredProperty(value, "wire", pointer, diagnostic);
  if (wire == nullptr) {
    return false;
  }
  const std::string wire_pointer = ChildPointer(pointer, "wire");
  yyjson_val* codec_value = yyjson_obj_get(wire, "codec");
  const bool is_bitfield = codec_value != nullptr && yyjson_is_str(codec_value) &&
                           std::string_view{yyjson_get_str(codec_value)} == "bitfield";
  if (is_bitfield) {
    if (!supports_bitfields || output.value_type == ValueType::BYTES ||
        output.value_type == ValueType::INT64 ||
        !ParseBitfieldWire(wire, wire_pointer, output.wire, diagnostic)) {
      if (!supports_bitfields || output.value_type == ValueType::BYTES ||
          output.value_type == ValueType::INT64) {
        return SetDiagnostic(
            diagnostic, CompileStage::STRUCTURAL, CompileError::UNSUPPORTED_FEATURE, wire_pointer,
            "bitfield wire is available only for Schema 0.2 BOOL, UINT64, and ENUM fields");
      }
      return false;
    }
  } else if (output.value_type == ValueType::BOOL) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNSUPPORTED_FEATURE,
                         wire_pointer, "BOOL requires bitfield wire");
  } else if (output.value_type == ValueType::BYTES) {
    if (!ParseBytesWire(wire, wire_pointer, variable_layout, output.wire, diagnostic)) {
      return false;
    }
  } else if (!ParseUnsignedWire(wire, wire_pointer, output.wire, diagnostic)) {
    return false;
  }

  yyjson_val* encode = RequiredProperty(value, "encode", pointer, diagnostic);
  if (encode == nullptr || !ParseEncode(encode, ChildPointer(pointer, "encode"), output.value_type,
                                        supports_computed_length, output.encode, diagnostic)) {
    return false;
  }

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  yyjson_val* computed = yyjson_obj_get(value, "computed");
  if (output.encode.source == EncodeSource::COMPUTED) {
    if (computed == nullptr) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::MISSING_PROPERTY,
                           ChildPointer(pointer, "computed"),
                           "computed encode source requires a computed rule");
    }
    ComputedLengthIr parsed;
    if (!ParseComputedLength(computed, ChildPointer(pointer, "computed"), variable_layout, parsed,
                             diagnostic))
      return false;
    output.computed_length = std::move(parsed);
  } else if (computed != nullptr) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                         ChildPointer(pointer, "computed"),
                         "computed rule requires encode.source computed");
  }
#else
  static_cast<void>(supports_computed_length);
#endif

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (yyjson_val* conversion = yyjson_obj_get(value, "conversion")) {
    if (!supports_conversion) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                           ChildPointer(pointer, "conversion"),
                           "conversion is available only in Schema 0.5");
    }
    LinearConversionIr parsed;
    if (!ParseConversion(conversion, ChildPointer(pointer, "conversion"), parsed, diagnostic)) {
      return false;
    }
    output.conversion = std::move(parsed);
  }
#else
  static_cast<void>(supports_conversion);
#endif

  if (output.value_type == ValueType::ENUM) {
    yyjson_val* policy = RequiredProperty(value, "unknown_enum_policy", pointer, diagnostic);
    std::string policy_token;
    if (policy == nullptr || !ReadEnumToken(policy, ChildPointer(pointer, "unknown_enum_policy"),
                                            {"reject", "preserve"}, policy_token, diagnostic)) {
      return false;
    }
    output.unknown_enum_policy =
        policy_token == "reject" ? UnknownEnumPolicy::REJECT : UnknownEnumPolicy::PRESERVE;
    yyjson_val* entries = RequiredProperty(value, "enum_entries", pointer, diagnostic);
    if (entries == nullptr || !ParseEnumEntries(entries, ChildPointer(pointer, "enum_entries"),
                                                output.enum_entries, diagnostic)) {
      return false;
    }
  }

  output.origin.json_pointer = std::string{pointer};
  return true;
}

bool ParseFields(yyjson_val* value, std::string_view pointer, bool supports_bitfields,
                 bool supports_int64, bool supports_conversion, bool supports_computed_length,
                 bool variable_layout, std::vector<FieldIr>& output,
                 CompileDiagnostic& diagnostic) {
  if (!yyjson_is_arr(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "fields must be an array");
  }
  if (yyjson_arr_size(value) == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::EMPTY_ARRAY,
                         std::string(pointer), "fields must contain at least one field");
  }
  output.clear();
  output.reserve(yyjson_arr_size(value));
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(value, &iterator);
  std::size_t index = 0U;
  while (yyjson_val* field_value = yyjson_arr_iter_next(&iterator)) {
    FieldIr field;
    if (!ParseField(field_value, IndexPointer(pointer, index), supports_bitfields, supports_int64,
                    supports_conversion, supports_computed_length, variable_layout, field,
                    diagnostic)) {
      return false;
    }
    output.push_back(std::move(field));
    ++index;
  }
  return true;
}

bool ParseBitContainers(yyjson_val* value, std::string_view pointer,
                        std::vector<BitContainerIr>& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_arr(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "bit_containers must be an array");
  }
  if (yyjson_arr_size(value) == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::EMPTY_ARRAY,
                         std::string(pointer), "bit_containers must not be empty when present");
  }
  output.clear();
  output.reserve(yyjson_arr_size(value));
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(value, &iterator);
  std::size_t index = 0U;
  while (yyjson_val* item = yyjson_arr_iter_next(&iterator)) {
    BitContainerIr container;
    if (!ParseBitContainer(item, IndexPointer(pointer, index), container, diagnostic)) {
      return false;
    }
    output.push_back(std::move(container));
    ++index;
  }
  return true;
}

bool ParseIntegrity(yyjson_val* value, std::string_view pointer, bool supports_crc,
                    bool variable_layout, IntegrityIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "integrity must be an object");
  }
  if (!ValidateObjectProperties(
          value, pointer,
          supports_crc ? std::initializer_list<std::string_view>{"algorithm", "parameters", "range",
                                                                 "storage"}
                       : std::initializer_list<std::string_view>{"algorithm", "range", "storage"},
          diagnostic)) {
    return false;
  }
  yyjson_val* algorithm = RequiredProperty(value, "algorithm", pointer, diagnostic);
  std::string algorithm_token;
  if (algorithm == nullptr ||
      !ReadEnumToken(algorithm, ChildPointer(pointer, "algorithm"),
                     supports_crc ? std::initializer_list<std::string_view>{"sum8", "crc"}
                                  : std::initializer_list<std::string_view>{"sum8"},
                     algorithm_token, diagnostic)) {
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (algorithm_token == "crc") {
    yyjson_val* parameters = RequiredProperty(value, "parameters", pointer, diagnostic);
    const std::string parameters_pointer = ChildPointer(pointer, "parameters");
    if (parameters == nullptr) {
      return false;
    }
    if (!yyjson_is_obj(parameters)) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                           parameters_pointer, "CRC parameters must be an object");
    }
    if (!ValidateObjectProperties(parameters, parameters_pointer,
                                  {"width", "poly", "init", "refin", "refout", "xorout"},
                                  diagnostic)) {
      return false;
    }
    std::uint64_t width = 0U;
    if (!ReadRequiredUint64(parameters, "width", parameters_pointer, width, diagnostic)) {
      return false;
    }
    if (width != 16U && width != 32U) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                           ChildPointer(parameters_pointer, "width"), "CRC width must be 16 or 32");
    }
    output.crc_width = static_cast<std::uint8_t>(width);
    const std::size_t digits = static_cast<std::size_t>(width / 4U);
    if (!ReadRequiredCrcHex(parameters, "poly", parameters_pointer, digits, output.crc_polynomial,
                            diagnostic) ||
        !ReadRequiredCrcHex(parameters, "init", parameters_pointer, digits,
                            output.crc_initial_value, diagnostic) ||
        !ReadRequiredBool(parameters, "refin", parameters_pointer, output.crc_reflect_input,
                          diagnostic) ||
        !ReadRequiredBool(parameters, "refout", parameters_pointer, output.crc_reflect_output,
                          diagnostic) ||
        !ReadRequiredCrcHex(parameters, "xorout", parameters_pointer, digits, output.crc_xor_output,
                            diagnostic)) {
      return false;
    }
    if (output.crc_polynomial == 0U || (output.crc_polynomial & 1U) == 0U) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::INVALID_ENUM_VALUE,
                           ChildPointer(parameters_pointer, "poly"),
                           "CRC polynomial must be nonzero with its low bit set");
    }
    output.algorithm = IntegrityAlgorithm::CRC;
  } else
#endif
  {
    if (yyjson_obj_get(value, "parameters") != nullptr) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                           ChildPointer(pointer, "parameters"),
                           "SUM8 does not accept CRC parameters");
    }
    output.algorithm = IntegrityAlgorithm::SUM8;
  }
  yyjson_val* range = RequiredProperty(value, "range", pointer, diagnostic);
  const std::string range_pointer = ChildPointer(pointer, "range");
  if (range == nullptr) {
    return false;
  }
  if (!yyjson_is_obj(range)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         range_pointer, "integrity range must be an object");
  }
  yyjson_val* dynamic_end = yyjson_obj_get(range, "end");
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (variable_layout && dynamic_end != nullptr) {
    std::string end_token;
    if (!ValidateObjectProperties(range, range_pointer, {"byte_offset", "end"}, diagnostic) ||
        !ReadRequiredUint64(range, "byte_offset", range_pointer, output.range_offset, diagnostic) ||
        !ReadEnumToken(dynamic_end, ChildPointer(range_pointer, "end"), {"payload_end"}, end_token,
                       diagnostic)) {
      return false;
    }
    output.range_ends_at_payload = true;
  } else
#else
  static_cast<void>(variable_layout);
  static_cast<void>(dynamic_end);
#endif
  {
    if (!ValidateObjectProperties(range, range_pointer, {"byte_offset", "byte_length"},
                                  diagnostic) ||
        !ReadRequiredUint64(range, "byte_offset", range_pointer, output.range_offset, diagnostic) ||
        !ReadRequiredUint64(range, "byte_length", range_pointer, output.range_length, diagnostic)) {
      return false;
    }
  }
  yyjson_val* storage = RequiredProperty(value, "storage", pointer, diagnostic);
  const std::string storage_pointer = ChildPointer(pointer, "storage");
  if (storage == nullptr) {
    return false;
  }
  if (!yyjson_is_obj(storage)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         storage_pointer, "integrity storage must be an object");
  }
  const bool crc_storage = algorithm_token == "crc";
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (variable_layout) {
    if (!ValidateObjectProperties(
            storage, storage_pointer,
            crc_storage ? std::initializer_list<std::string_view>{"anchor", "byte_order"}
                        : std::initializer_list<std::string_view>{"anchor"},
            diagnostic)) {
      return false;
    }
    yyjson_val* anchor = RequiredProperty(storage, "anchor", storage_pointer, diagnostic);
    std::string anchor_token;
    if (anchor == nullptr || !ReadEnumToken(anchor, ChildPointer(storage_pointer, "anchor"),
                                            {"payload_end"}, anchor_token, diagnostic)) {
      return false;
    }
    output.storage_at_payload_end = true;
  } else
#endif
  {
    if (!ValidateObjectProperties(
            storage, storage_pointer,
            crc_storage ? std::initializer_list<std::string_view>{"byte_offset", "byte_order"}
                        : std::initializer_list<std::string_view>{"byte_offset"},
            diagnostic) ||
        !ReadRequiredUint64(storage, "byte_offset", storage_pointer, output.storage_offset,
                            diagnostic)) {
      return false;
    }
  }
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (crc_storage) {
    yyjson_val* byte_order = RequiredProperty(storage, "byte_order", storage_pointer, diagnostic);
    std::string byte_order_token;
    if (byte_order == nullptr ||
        !ReadEnumToken(byte_order, ChildPointer(storage_pointer, "byte_order"),
                       {"big_endian", "little_endian"}, byte_order_token, diagnostic)) {
      return false;
    }
    output.storage_byte_order =
        byte_order_token == "big_endian" ? ByteOrder::BIG : ByteOrder::LITTLE;
  }
#endif
  output.origin.json_pointer = std::string{pointer};
  output.range_origin.json_pointer = range_pointer;
  output.storage_origin.json_pointer = storage_pointer;
  return true;
}

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
bool ParseBoundedPayloadLayout(yyjson_val* value, std::string_view pointer,
                               BoundedPayloadIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string{pointer}, "layout must be an object");
  }
  if (!ValidateObjectProperties(value, pointer,
                                {"kind", "header_length_bytes", "payload_field_id",
                                 "min_payload_bytes", "max_payload_bytes"},
                                diagnostic)) {
    return false;
  }
  yyjson_val* kind = RequiredProperty(value, "kind", pointer, diagnostic);
  std::string kind_token;
  if (kind == nullptr ||
      !ReadEnumToken(kind, ChildPointer(pointer, "kind"), {"bounded_payload"}, kind_token,
                     diagnostic) ||
      !ReadRequiredUint64(value, "header_length_bytes", pointer, output.header_length,
                          diagnostic) ||
      !ReadRequiredId(value, "payload_field_id", pointer, output.payload_field_id, diagnostic) ||
      !ReadRequiredUint64(value, "min_payload_bytes", pointer, output.min_payload_length,
                          diagnostic) ||
      !ReadRequiredUint64(value, "max_payload_bytes", pointer, output.max_payload_length,
                          diagnostic)) {
    return false;
  }
  if (output.header_length == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         ChildPointer(pointer, "header_length_bytes"),
                         "bounded payload header must contain at least one byte");
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}
#endif

bool ParseMessage(yyjson_val* value, std::string_view pointer, bool supports_bitfields,
                  bool supports_integrity, bool supports_crc, bool supports_int64,
                  bool supports_conversion, bool supports_computed_length,
                  bool supports_variable_layout, MessageIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "message must be an object");
  }
  if (!ValidateObjectProperties(
          value, pointer,
          supports_variable_layout
              ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                        "source_ref", "direction_id",
                                                        "frame_length_bytes", "layout", "matcher",
                                                        "bit_containers", "integrity", "fields"}
          : supports_integrity
              ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                        "source_ref", "direction_id",
                                                        "frame_length_bytes", "matcher",
                                                        "bit_containers", "integrity", "fields"}
          : supports_bitfields
              ? std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                        "source_ref", "direction_id",
                                                        "frame_length_bytes", "matcher",
                                                        "bit_containers", "fields"}
              : std::initializer_list<std::string_view>{"id", "display_name", "description",
                                                        "source_ref", "direction_id",
                                                        "frame_length_bytes", "matcher", "fields"},
          diagnostic)) {
    return false;
  }
  if (!ReadRequiredId(value, "id", pointer, output.id, diagnostic) ||
      !ReadRequiredString(value, "display_name", pointer, 1U, 256U, output.display_name,
                          diagnostic) ||
      !ReadRequiredString(value, "description", pointer, 0U, 1024U, output.description,
                          diagnostic) ||
      !ReadRequiredString(value, "source_ref", pointer, 1U, 512U, output.source_ref, diagnostic) ||
      !ReadRequiredId(value, "direction_id", pointer, output.direction_id, diagnostic)) {
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  yyjson_val* layout = yyjson_obj_get(value, "layout");
  const bool variable_layout = supports_variable_layout && layout != nullptr;
  if (variable_layout) {
    if (yyjson_obj_get(value, "frame_length_bytes") != nullptr) {
      return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                           ChildPointer(pointer, "frame_length_bytes"),
                           "bounded payload Message must omit frame_length_bytes");
    }
    BoundedPayloadIr parsed;
    if (!ParseBoundedPayloadLayout(layout, ChildPointer(pointer, "layout"), parsed, diagnostic)) {
      return false;
    }
    output.bounded_payload = std::move(parsed);
  } else
#else
  const bool variable_layout = false;
  static_cast<void>(supports_variable_layout);
#endif
  {
    if (!ReadRequiredUint64(value, "frame_length_bytes", pointer, output.frame_length_bytes,
                            diagnostic)) {
      return false;
    }
  }
  if (!variable_layout &&
      (output.frame_length_bytes == 0U || output.frame_length_bytes > 1024U * 1024U)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
                         ChildPointer(pointer, "frame_length_bytes"),
                         "frame length must be in the range 1..1048576");
  }
  yyjson_val* matcher = RequiredProperty(value, "matcher", pointer, diagnostic);
  yyjson_val* fields = RequiredProperty(value, "fields", pointer, diagnostic);
  if (matcher == nullptr || fields == nullptr ||
      !ParseMatcher(matcher, ChildPointer(pointer, "matcher"), output.matcher_clauses,
                    diagnostic) ||
      !ParseFields(fields, ChildPointer(pointer, "fields"), supports_bitfields, supports_int64,
                   supports_conversion, supports_computed_length, variable_layout, output.fields,
                   diagnostic)) {
    return false;
  }
  if (supports_bitfields) {
    if (yyjson_val* containers = yyjson_obj_get(value, "bit_containers")) {
      if (!ParseBitContainers(containers, ChildPointer(pointer, "bit_containers"),
                              output.bit_containers, diagnostic)) {
        return false;
      }
    }
  }
  if (supports_integrity) {
    if (yyjson_val* integrity = yyjson_obj_get(value, "integrity")) {
      IntegrityIr parsed;
      if (!ParseIntegrity(integrity, ChildPointer(pointer, "integrity"), supports_crc,
                          variable_layout, parsed, diagnostic)) {
        return false;
      }
      output.integrity = std::move(parsed);
    }
  }
  output.origin.json_pointer = std::string{pointer};
  return true;
}

template <typename Item, typename ParseItem>
bool ParseObjectArray(yyjson_val* value, std::string_view pointer, std::vector<Item>& output,
                      ParseItem parse_item, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_arr(value)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                         std::string(pointer), "expected an array");
  }
  if (yyjson_arr_size(value) == 0U) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::EMPTY_ARRAY,
                         std::string(pointer), "array must contain at least one item");
  }
  output.clear();
  output.reserve(yyjson_arr_size(value));
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(value, &iterator);
  std::size_t index = 0U;
  while (yyjson_val* item_value = yyjson_arr_iter_next(&iterator)) {
    Item item;
    if (!parse_item(item_value, IndexPointer(pointer, index), item, diagnostic)) {
      return false;
    }
    output.push_back(std::move(item));
    ++index;
  }
  return true;
}

bool BuildSchemaIr(yyjson_val* root, SchemaIr& output, CompileDiagnostic& diagnostic) {
  if (!yyjson_is_obj(root)) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::ROOT_MUST_BE_OBJECT,
                         "", "PAE protocol package root must be an object");
  }
  if (!ValidateObjectProperties(
          root, "",
          {"schema_version", "protocol_id", "protocol_version", "display_name", "description",
           "source_ref", "resource_profile", "framing_profiles", "pipelines", "messages"},
          diagnostic)) {
    return false;
  }

  if (!ReadRequiredString(root, "schema_version", "", 1U, 16U, output.schema_version, diagnostic) ||
      !ReadRequiredId(root, "protocol_id", "", output.protocol_id, diagnostic) ||
      !ReadRequiredString(root, "protocol_version", "", 1U, 64U, output.protocol_version,
                          diagnostic) ||
      !ReadRequiredString(root, "display_name", "", 1U, 256U, output.display_name, diagnostic) ||
      !ReadRequiredString(root, "source_ref", "", 1U, 512U, output.source_ref, diagnostic)) {
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  const bool supported_schema = output.schema_version == "0.1" || output.schema_version == "0.2" ||
                                output.schema_version == "0.3" || output.schema_version == "0.4" ||
                                output.schema_version == "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
                                || output.schema_version == "0.6"
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                                || output.schema_version == "0.7"
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
                                || output.schema_version == "0.8"
#endif
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
                                || output.schema_version == "0.9"
#endif
      ;
#else
  const bool supported_schema = output.schema_version == "0.1" || output.schema_version == "0.2" ||
                                output.schema_version == "0.3" || output.schema_version == "0.4";
#endif
  if (!supported_schema) {
    return SetDiagnostic(diagnostic, CompileStage::STRUCTURAL, CompileError::INVALID_ENUM_VALUE,
                         "/schema_version",
                         "schema_version is not supported by this compiler build");
  }

  if (yyjson_val* description = yyjson_obj_get(root, "description")) {
    if (!ReadString(description, "/description", 0U, 1024U, output.description, diagnostic)) {
      return false;
    }
  }

  yyjson_val* profile = RequiredProperty(root, "resource_profile", "", diagnostic);
  std::string profile_token;
  if (profile == nullptr || !ReadEnumToken(profile, "/resource_profile", {"desktop", "constrained"},
                                           profile_token, diagnostic)) {
    return false;
  }
  output.resource_profile =
      profile_token == "desktop" ? ResourceProfile::DESKTOP : ResourceProfile::CONSTRAINED;

  yyjson_val* framing_profiles = RequiredProperty(root, "framing_profiles", "", diagnostic);
  yyjson_val* pipelines = RequiredProperty(root, "pipelines", "", diagnostic);
  yyjson_val* messages = RequiredProperty(root, "messages", "", diagnostic);
  if (framing_profiles == nullptr || pipelines == nullptr || messages == nullptr) {
    return false;
  }
  return ParseObjectArray(
             framing_profiles, "/framing_profiles", output.framing_profiles,
             [&output](yyjson_val* value, std::string_view pointer, FramingProfileIr& framing,
                       CompileDiagnostic& item_diagnostic) {
               return ParseFramingProfile(value, pointer, output.schema_version == "0.9", framing,
                                          item_diagnostic);
             },
             diagnostic) &&
         ParseObjectArray(pipelines, "/pipelines", output.pipelines, ParsePipeline, diagnostic) &&
         ParseObjectArray(
             messages, "/messages", output.messages,
             [&output](yyjson_val* value, std::string_view pointer, MessageIr& message,
                       CompileDiagnostic& item_diagnostic) {
               return ParseMessage(
                   value, pointer, output.schema_version != "0.1",
                   output.schema_version == "0.3" || output.schema_version == "0.4" ||
                       output.schema_version == "0.5" || output.schema_version == "0.6" ||
                       output.schema_version == "0.7" || output.schema_version == "0.8" ||
                       output.schema_version == "0.9",
                   output.schema_version == "0.6" || output.schema_version == "0.7" ||
                       output.schema_version == "0.8" || output.schema_version == "0.9",
                   output.schema_version == "0.4" || output.schema_version == "0.5" ||
                       output.schema_version == "0.6" || output.schema_version == "0.7" ||
                       output.schema_version == "0.8" || output.schema_version == "0.9",
                   output.schema_version == "0.5" || output.schema_version == "0.6" ||
                       output.schema_version == "0.7" || output.schema_version == "0.8" ||
                       output.schema_version == "0.9",
                   output.schema_version == "0.7" || output.schema_version == "0.8" ||
                       output.schema_version == "0.9",
                   output.schema_version == "0.8" || output.schema_version == "0.9", message,
                   item_diagnostic);
             },
             diagnostic);
}

template <typename Item, typename OriginAccessor>
bool BuildUniqueIdIndex(const std::vector<Item>& items,
                        std::unordered_map<std::string, std::size_t>& index,
                        OriginAccessor origin_accessor, CompileDiagnostic& diagnostic) {
  index.clear();
  index.reserve(items.size());
  for (std::size_t item_index = 0U; item_index < items.size(); ++item_index) {
    const auto [unused, inserted] = index.emplace(items[item_index].id, item_index);
    static_cast<void>(unused);
    if (!inserted) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::DUPLICATE_ID,
                           ChildPointer(origin_accessor(items[item_index]), "id"),
                           "stable ID is duplicated in its scope");
    }
  }
  return true;
}

bool AddSizeChecked(std::size_t value, std::size_t& total) noexcept {
  if (value > std::numeric_limits<std::size_t>::max() - total) {
    return false;
  }
  total += value;
  return true;
}

#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
std::pair<std::uint64_t, std::uint64_t> MessageFrameBounds(const MessageIr& message) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (message.bounded_payload.has_value()) {
    return {message.bounded_payload->min_frame_length, message.bounded_payload->max_frame_length};
  }
#endif
  return {message.frame_length_bytes, message.frame_length_bytes};
}

bool MessageHasSyncMatcher(const MessageIr& message,
                           const std::vector<std::uint8_t>& sync) noexcept {
  for (std::size_t offset = 0U; offset < sync.size(); ++offset) {
    bool matched = false;
    for (const MatcherClauseIr& matcher : message.matcher_clauses) {
      if (matcher.kind != MatcherKind::FIXED_BYTES || offset < matcher.byte_offset ||
          offset - matcher.byte_offset >= matcher.bytes.size()) {
        continue;
      }
      matched = matcher.bytes[offset - matcher.byte_offset] == sync[offset];
      break;
    }
    if (!matched) {
      return false;
    }
  }
  return true;
}

bool MessageHasLengthField(const MessageIr& message, const FramingProfileIr& framing) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  for (const FieldIr& field : message.fields) {
    if (field.value_type == ValueType::UINT64 && field.computed_length.has_value() &&
        field.computed_length->scope == ComputedLengthScope::FRAME &&
        field.wire.codec == WireCodec::UNSIGNED_INTEGER &&
        field.wire.byte_offset == framing.length_field_offset &&
        field.wire.byte_width == framing.length_field_width &&
        field.wire.byte_order == framing.length_field_byte_order) {
      return true;
    }
  }
#endif
  return false;
}

bool ValidateStreamFramingLocal(const FramingProfileIr& framing, CompileDiagnostic& diagnostic) {
  if (framing.input_kind == InputKind::COMPLETE_RECORD) {
    return framing.strategy == FramingStrategy::COMPLETE_RECORD;
  }
  if (framing.strategy == FramingStrategy::FIXED_LENGTH) {
    if (framing.frame_length_bytes == 0U) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::INTEGER_OUT_OF_RANGE,
                           ChildPointer(framing.origin.json_pointer, "frame_length_bytes"),
                           "fixed stream frame length must be greater than zero");
    }
    return true;
  }
  if (framing.strategy == FramingStrategy::SYNC_FIXED_LENGTH) {
    if (framing.frame_length_bytes == 0U || framing.sync_bytes.empty() ||
        framing.sync_bytes.size() > framing.frame_length_bytes) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::FIELD_OUT_OF_BOUNDS, framing.origin.json_pointer,
                           "sync header must fit inside the fixed stream frame");
    }
    return true;
  }
  if (framing.strategy != FramingStrategy::SYNC_LENGTH_FIELD || framing.sync_bytes.empty() ||
      (framing.length_field_width != 1U && framing.length_field_width != 2U &&
       framing.length_field_width != 4U) ||
      (framing.length_field_width == 1U &&
       framing.length_field_byte_order != ByteOrder::NOT_APPLICABLE) ||
      (framing.length_field_width != 1U && framing.length_field_byte_order != ByteOrder::BIG &&
       framing.length_field_byte_order != ByteOrder::LITTLE) ||
      framing.minimum_frame_length == 0U ||
      framing.minimum_frame_length > framing.maximum_frame_length ||
      framing.length_field_offset >
          (std::numeric_limits<std::uint64_t>::max)() - framing.length_field_width ||
      framing.length_field_offset + framing.length_field_width > framing.minimum_frame_length) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                         CompileError::FIELD_OUT_OF_BOUNDS, framing.origin.json_pointer,
                         "stream length field and frame bounds are inconsistent");
  }
  const std::uint64_t length_end = framing.length_field_offset + framing.length_field_width;
  if (framing.length_field_offset < framing.sync_bytes.size() && length_end > 0U) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OVERLAP,
                         ChildPointer(framing.origin.json_pointer, "length_field"),
                         "stream length field overlaps the offset-zero sync header");
  }
  const std::uint64_t representable =
      framing.length_field_width == 1U
          ? 0xFFU
          : (framing.length_field_width == 2U ? 0xFFFFU : 0xFFFFFFFFULL);
  if (framing.maximum_frame_length > representable) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                         CompileError::VALUE_NOT_REPRESENTABLE,
                         ChildPointer(framing.origin.json_pointer, "maximum_frame_length"),
                         "maximum stream frame length does not fit the length field");
  }
  return true;
}
#endif

bool FitsUnsignedWidth(std::uint64_t value, std::uint64_t byte_width) noexcept {
  if (byte_width >= 8U) {
    return true;
  }
  const unsigned bit_count = static_cast<unsigned>(byte_width * 8U);
  return value < (std::uint64_t{1U} << bit_count);
}

bool FitsUnsignedBits(std::uint64_t value, std::uint64_t bit_width) noexcept {
  return bit_width == 64U || (bit_width != 0U && bit_width < 64U &&
                              value < (std::uint64_t{1U} << static_cast<unsigned>(bit_width)));
}

bool FitsSignedWidth(std::int64_t value, std::uint64_t byte_width) noexcept {
  if (byte_width == 0U || byte_width > 8U) {
    return false;
  }
  if (byte_width == 8U) {
    return true;
  }
  const unsigned magnitude_bits = static_cast<unsigned>(byte_width * 8U - 1U);
  const std::int64_t minimum = -static_cast<std::int64_t>(std::uint64_t{1U} << magnitude_bits);
  const std::int64_t maximum =
      static_cast<std::int64_t>((std::uint64_t{1U} << magnitude_bits) - 1U);
  return value >= minimum && value <= maximum;
}

std::uint8_t EncodeUnsignedConstantByte(std::uint64_t value, std::uint64_t byte_width,
                                        ByteOrder byte_order, std::uint64_t byte_index) noexcept {
  const std::uint64_t encoded_index =
      byte_order == ByteOrder::BIG ? byte_width - 1U - byte_index : byte_index;
  const unsigned shift = static_cast<unsigned>(encoded_index * 8U);
  return static_cast<std::uint8_t>((value >> shift) & 0xFFU);
}

std::uint64_t BitWidthMask(std::uint64_t byte_width) noexcept {
  return byte_width == 8U ? (std::numeric_limits<std::uint64_t>::max)()
                          : (std::uint64_t{1U} << static_cast<unsigned>(byte_width * 8U)) - 1U;
}

bool FixedMatchersCanIntersect(const MessageIr& left, const MessageIr& right) {
  const std::uint64_t left_min =
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      left.bounded_payload.has_value() ? left.bounded_payload->min_frame_length :
#endif
                                       left.frame_length_bytes;
  const std::uint64_t left_max = left.frame_length_bytes;
  const std::uint64_t right_min =
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      right.bounded_payload.has_value() ? right.bounded_payload->min_frame_length :
#endif
                                        right.frame_length_bytes;
  const std::uint64_t right_max = right.frame_length_bytes;
  if (left_max < right_min || right_max < left_min) {
    return false;
  }
  std::unordered_map<std::uint64_t, std::uint8_t> left_bytes;
  for (const MatcherClauseIr& matcher : left.matcher_clauses) {
    if (matcher.kind != MatcherKind::FIXED_BYTES) {
      continue;
    }
    for (std::size_t index = 0U; index < matcher.bytes.size(); ++index) {
      left_bytes.emplace(matcher.byte_offset + index, matcher.bytes[index]);
    }
  }
  for (const MatcherClauseIr& matcher : right.matcher_clauses) {
    if (matcher.kind != MatcherKind::FIXED_BYTES) {
      continue;
    }
    for (std::size_t index = 0U; index < matcher.bytes.size(); ++index) {
      const auto left_byte = left_bytes.find(matcher.byte_offset + index);
      if (left_byte != left_bytes.end() && left_byte->second != matcher.bytes[index]) {
        return false;
      }
    }
  }
  return true;
}

bool ValidateMessageDomain(MessageIr& message, ResourceRequirements& requirements,
                           CompileDiagnostic& diagnostic) {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (message.bounded_payload.has_value()) {
    BoundedPayloadIr& layout = *message.bounded_payload;
    if (layout.min_payload_length > layout.max_payload_length) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::VALUE_NOT_REPRESENTABLE,
                           ChildPointer(layout.origin.json_pointer, "min_payload_bytes"),
                           "min_payload_bytes must not exceed max_payload_bytes");
    }
    layout.trailer_length = 0U;
    if (message.integrity.has_value()) {
      layout.trailer_length = message.integrity->algorithm == IntegrityAlgorithm::SUM8
                                  ? 1U
                                  : static_cast<std::uint64_t>(message.integrity->crc_width / 8U);
    }
    if (layout.min_payload_length >
            std::numeric_limits<std::uint64_t>::max() - layout.header_length ||
        layout.max_payload_length >
            std::numeric_limits<std::uint64_t>::max() - layout.header_length ||
        layout.trailer_length > std::numeric_limits<std::uint64_t>::max() - layout.header_length -
                                    layout.min_payload_length ||
        layout.trailer_length > std::numeric_limits<std::uint64_t>::max() - layout.header_length -
                                    layout.max_payload_length) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::FIELD_OUT_OF_BOUNDS, layout.origin.json_pointer,
                           "bounded payload frame length arithmetic overflows");
    }
    layout.min_frame_length =
        layout.header_length + layout.min_payload_length + layout.trailer_length;
    layout.max_frame_length =
        layout.header_length + layout.max_payload_length + layout.trailer_length;
    message.frame_length_bytes = layout.max_frame_length;
    std::size_t payload_matches = 0U;
    for (std::size_t index = 0U; index < message.fields.size(); ++index) {
      FieldIr& field = message.fields[index];
      if (field.id != layout.payload_field_id) continue;
      ++payload_matches;
      layout.payload_field_index = index;
      if (field.value_type != ValueType::BYTES || field.wire.codec != WireCodec::BYTES ||
          field.wire.byte_offset != layout.header_length || field.wire.byte_width != 0U ||
          field.encode.source != EncodeSource::INPUT || field.conversion.has_value()) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::UNSUPPORTED_FEATURE, field.wire.origin.json_pointer,
                             "bounded payload field must be input BYTES at header_length_bytes and "
                             "omit byte_length");
      }
      field.wire.byte_width = layout.max_payload_length;
    }
    if (payload_matches != 1U) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::UNKNOWN_REFERENCE,
                           ChildPointer(layout.origin.json_pointer, "payload_field_id"),
                           "payload_field_id must resolve exactly once in its Message");
    }
  }
#endif
  if (!AddSizeChecked(message.fields.size(), requirements.total_field_count) ||
      !AddSizeChecked(message.matcher_clauses.size(), requirements.total_matcher_count) ||
      !AddSizeChecked(message.bit_containers.size(), requirements.total_bit_container_count) ||
      (message.integrity.has_value() &&
       !AddSizeChecked(1U, requirements.total_integrity_rule_count))
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      || (std::any_of(message.fields.begin(), message.fields.end(),
                      [](const FieldIr& field) { return field.computed_length.has_value(); }) &&
          !AddSizeChecked(1U, requirements.total_computed_length_count))
#endif
  ) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                         CompileError::INTERNAL_CONTRACT_VIOLATION, message.origin.json_pointer,
                         "resource count overflow");
  }
  requirements.max_frame_bytes =
      (std::max)(requirements.max_frame_bytes, message.frame_length_bytes);

  std::unordered_set<std::string> field_ids;
  field_ids.reserve(message.fields.size());
  struct FieldSpan {
    std::uint64_t begin = 0U;
    std::uint64_t end = 0U;
    const FieldIr* field = nullptr;
  };
  std::vector<FieldSpan> spans;
  spans.reserve(message.fields.size() + message.bit_containers.size());
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  std::size_t computed_length_count = 0U;
#endif

  std::unordered_map<std::string, std::size_t> container_ids;
  std::vector<std::uint64_t> member_masks(message.bit_containers.size(), 0U);
  std::vector<std::size_t> member_counts(message.bit_containers.size(), 0U);
  for (std::size_t index = 0U; index < message.bit_containers.size(); ++index) {
    BitContainerIr& container = message.bit_containers[index];
    if (!container_ids.emplace(container.id, index).second) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::DUPLICATE_ID,
                           ChildPointer(container.origin.json_pointer, "id"),
                           "bit container ID is duplicated within the message");
    }
    if (container.byte_width > std::numeric_limits<std::uint64_t>::max() - container.byte_offset ||
        container.byte_offset + container.byte_width > message.frame_length_bytes) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::FIELD_OUT_OF_BOUNDS, container.origin.json_pointer,
                           "bit container byte range exceeds message frame_length_bytes");
    }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (message.bounded_payload.has_value() &&
        (container.byte_offset > message.bounded_payload->header_length ||
         container.byte_width > message.bounded_payload->header_length - container.byte_offset)) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::FIELD_OUT_OF_BOUNDS, container.origin.json_pointer,
                           "bit container must be fully inside the bounded payload header");
    }
#endif
    if (!FitsUnsignedWidth(container.base_value, container.byte_width)) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::VALUE_NOT_REPRESENTABLE,
                           ChildPointer(container.origin.json_pointer, "base_value"),
                           "bit container base_value does not fit its width");
    }
    spans.push_back(
        FieldSpan{container.byte_offset, container.byte_offset + container.byte_width, nullptr});
  }

  for (FieldIr& field : message.fields) {
    if (!field_ids.emplace(field.id).second) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::DUPLICATE_ID,
                           ChildPointer(field.origin.json_pointer, "id"),
                           "field ID is duplicated within the message");
    }
    if (field.wire.codec == WireCodec::BITFIELD) {
      const auto container = container_ids.find(field.wire.container_id);
      if (container == container_ids.end()) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::UNKNOWN_REFERENCE,
                             ChildPointer(field.wire.origin.json_pointer, "container_id"),
                             "bitfield references an unknown bit container");
      }
      field.wire.bit_container_index = container->second;
      const BitContainerIr& container_ir = message.bit_containers[container->second];
      const std::uint64_t container_bits = container_ir.byte_width * 8U;
      if (field.wire.bit_width > container_bits ||
          field.wire.bit_offset > container_bits - field.wire.bit_width) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::FIELD_OUT_OF_BOUNDS, field.wire.origin.json_pointer,
                             "bitfield range exceeds its bit container");
      }
      if (field.value_type == ValueType::BOOL && field.wire.bit_width != 1U) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::VALUE_NOT_REPRESENTABLE,
                             ChildPointer(field.wire.origin.json_pointer, "bit_width"),
                             "BOOL bitfield must have bit_width 1");
      }
      const std::uint64_t shift =
          container_ir.bit_numbering == BitNumbering::LSB0
              ? field.wire.bit_offset
              : container_bits - field.wire.bit_offset - field.wire.bit_width;
      const std::uint64_t mask =
          field.wire.bit_width == 64U
              ? std::numeric_limits<std::uint64_t>::max()
              : ((std::uint64_t{1U} << static_cast<unsigned>(field.wire.bit_width)) - 1U)
                    << static_cast<unsigned>(shift);
      if ((member_masks[container->second] & mask) != 0U) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::FIELD_OVERLAP, field.origin.json_pointer,
                             "bitfield members overlap within their container");
      }
      member_masks[container->second] |= mask;
      ++member_counts[container->second];
    } else if (field.wire.byte_width >
                   std::numeric_limits<std::uint64_t>::max() - field.wire.byte_offset ||
               field.wire.byte_offset + field.wire.byte_width > message.frame_length_bytes) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::FIELD_OUT_OF_BOUNDS, field.wire.origin.json_pointer,
                           "field byte range exceeds message frame_length_bytes");
    }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (message.bounded_payload.has_value() &&
        &field != &message.fields[message.bounded_payload->payload_field_index]) {
      if (field.wire.codec == WireCodec::BYTES && field.wire.byte_width == 0U) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::UNSUPPORTED_FEATURE, field.wire.origin.json_pointer,
                             "only the bounded payload field may omit byte_length");
      }
      if (field.wire.codec != WireCodec::BITFIELD &&
          field.wire.byte_offset + field.wire.byte_width > message.bounded_payload->header_length) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::FIELD_OUT_OF_BOUNDS, field.wire.origin.json_pointer,
                             "non-payload field must be fully inside the bounded payload header");
      }
    }
#endif
    if (field.wire.codec != WireCodec::BITFIELD) {
      spans.push_back(FieldSpan{field.wire.byte_offset,
                                field.wire.byte_offset + field.wire.byte_width, &field});
    }

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    if (field.computed_length.has_value()) {
      ++computed_length_count;
      const ComputedLengthIr& computed = *field.computed_length;
      if (computed_length_count > 1U) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::UNSUPPORTED_FEATURE, computed.origin.json_pointer,
                             "a message may contain at most one computed length field");
      }
      if (field.value_type != ValueType::UINT64 ||
          field.wire.codec != WireCodec::UNSIGNED_INTEGER ||
          field.encode.source != EncodeSource::COMPUTED ||
          field.encode.constant_value.has_value() ||
          field.encode.signed_constant_value.has_value() || field.conversion.has_value() ||
          !field.enum_entries.empty() ||
          (field.wire.byte_width != 1U && field.wire.byte_width != 2U &&
           field.wire.byte_width != 4U) ||
          (field.wire.byte_width == 1U && field.wire.byte_order != ByteOrder::NOT_APPLICABLE) ||
          (field.wire.byte_width != 1U && field.wire.byte_order != ByteOrder::BIG &&
           field.wire.byte_order != ByteOrder::LITTLE)) {
        return SetDiagnostic(
            diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::UNSUPPORTED_FEATURE,
            computed.origin.json_pointer,
            "computed length requires a byte-aligned UINT64 field of width 1, 2, or 4");
      }
      std::uint64_t expected = message.frame_length_bytes;
      if (computed.scope == ComputedLengthScope::REGION) {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
        if (message.bounded_payload.has_value()) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::UNSUPPORTED_FEATURE, computed.origin.json_pointer,
                               "bounded payload length supports only frame or payload scope");
        }
#endif
        if (computed.range_length == 0U) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::FIELD_OUT_OF_BOUNDS,
                               ChildPointer(computed.range_origin.json_pointer, "byte_length"),
                               "computed length range must contain at least one byte");
        }
        if (computed.range_offset > message.frame_length_bytes ||
            computed.range_length > message.frame_length_bytes - computed.range_offset) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::FIELD_OUT_OF_BOUNDS,
                               computed.range_origin.json_pointer,
                               "computed length range exceeds message frame_length_bytes");
        }
        expected = computed.range_length;
      }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      if (computed.scope == ComputedLengthScope::PAYLOAD) {
        if (!message.bounded_payload.has_value()) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::UNSUPPORTED_FEATURE, computed.origin.json_pointer,
                               "payload length scope requires a bounded payload Message");
        }
        expected = message.bounded_payload->max_payload_length;
      }
      if (message.bounded_payload.has_value() &&
          field.wire.byte_offset + field.wire.byte_width > message.bounded_payload->header_length) {
        return SetDiagnostic(
            diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OUT_OF_BOUNDS,
            field.wire.origin.json_pointer,
            "computed length storage must be fully inside the bounded payload header");
      }
#endif
      if (!FitsUnsignedWidth(expected, field.wire.byte_width)) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::VALUE_NOT_REPRESENTABLE, computed.origin.json_pointer,
                             "computed length does not fit the configured wire width");
      }
    } else if (field.encode.source == EncodeSource::COMPUTED) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::INTERNAL_CONTRACT_VIOLATION,
                           field.encode.origin.json_pointer,
                           "computed encode source is missing its length descriptor");
    }
#endif

    if (field.encode.constant_value.has_value() &&
        !(field.wire.codec == WireCodec::BITFIELD
              ? FitsUnsignedBits(*field.encode.constant_value, field.wire.bit_width)
              : FitsUnsignedWidth(*field.encode.constant_value, field.wire.byte_width))) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::VALUE_NOT_REPRESENTABLE,
                           ChildPointer(field.encode.origin.json_pointer, "value"),
                           "constant value does not fit the configured wire width");
    }
    if (field.encode.signed_constant_value.has_value() &&
        !FitsSignedWidth(*field.encode.signed_constant_value, field.wire.byte_width)) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::VALUE_NOT_REPRESENTABLE,
                           ChildPointer(field.encode.origin.json_pointer, "value"),
                           "signed constant value does not fit the configured wire width");
    }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
    if (field.conversion.has_value()) {
      if (!AddSizeChecked(1U, requirements.total_conversion_count)) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTERNAL_CONTRACT_VIOLATION,
                             field.conversion->origin.json_pointer, "conversion count overflow");
      }
      if ((field.value_type != ValueType::UINT64 && field.value_type != ValueType::INT64) ||
          field.wire.codec != WireCodec::UNSIGNED_INTEGER ||
          field.encode.source != EncodeSource::INPUT) {
        const std::string pointer = field.encode.source != EncodeSource::INPUT
                                        ? ChildPointer(field.encode.origin.json_pointer, "source")
                                        : field.conversion->origin.json_pointer;
        return SetDiagnostic(
            diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::UNSUPPORTED_FEATURE, pointer,
            "linear conversion requires an input byte-aligned UINT64 or INT64 field");
      }
      const auto derived = protocol_plan::detail::DeriveLinearConversion(
          field.value_type, field.conversion->scale.numerator, field.conversion->scale.denominator,
          field.conversion->bias.numerator, field.conversion->bias.denominator,
          field.conversion->derived);
      if (derived != protocol_plan::detail::DecimalDescriptorError::NONE) {
        const bool zero_scale =
            derived == protocol_plan::detail::DecimalDescriptorError::ZERO_SCALE;
        const bool invalid_scale =
            derived == protocol_plan::detail::DecimalDescriptorError::NON_TERMINATING_SCALE;
        const bool invalid_bias =
            derived == protocol_plan::detail::DecimalDescriptorError::NON_TERMINATING_BIAS;
        const std::string pointer =
            zero_scale ? ChildPointer(field.conversion->scale.origin.json_pointer, "numerator")
            : invalid_scale
                ? ChildPointer(field.conversion->scale.origin.json_pointer, "denominator")
            : invalid_bias ? ChildPointer(field.conversion->bias.origin.json_pointer, "denominator")
                           : field.conversion->origin.json_pointer;
        return SetDiagnostic(
            diagnostic, CompileStage::DOMAIN_VALIDATION,
            derived == protocol_plan::detail::DecimalDescriptorError::ARITHMETIC_OVERFLOW
                ? CompileError::INTERNAL_CONTRACT_VIOLATION
                : CompileError::VALUE_NOT_REPRESENTABLE,
            pointer,
            zero_scale ? "conversion scale numerator must not be zero"
                       : "conversion rational must be an exact decimal with at most 18 places");
      }
    }
#endif

    if (field.value_type == ValueType::ENUM) {
      if (!AddSizeChecked(field.enum_entries.size(), requirements.total_enum_entry_count)) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTERNAL_CONTRACT_VIOLATION, field.origin.json_pointer,
                             "enum entry count overflow");
      }
      std::unordered_set<std::string> enum_ids;
      std::unordered_set<std::uint64_t> enum_values;
      enum_ids.reserve(field.enum_entries.size());
      enum_values.reserve(field.enum_entries.size());
      for (const EnumEntryIr& entry : field.enum_entries) {
        if (!enum_ids.emplace(entry.id).second || !enum_values.emplace(entry.raw_value).second) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::DUPLICATE_ID, entry.origin.json_pointer,
                               "enum entry ID and raw value must both be unique");
        }
        if (!(field.wire.codec == WireCodec::BITFIELD
                  ? FitsUnsignedBits(entry.raw_value, field.wire.bit_width)
                  : FitsUnsignedWidth(entry.raw_value, field.wire.byte_width))) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::VALUE_NOT_REPRESENTABLE,
                               ChildPointer(entry.origin.json_pointer, "raw_value"),
                               "enum raw value does not fit the configured wire width");
        }
      }
    }
  }

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER) && \
    defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (message.bounded_payload.has_value() && computed_length_count != 1U) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                         CompileError::UNSUPPORTED_FEATURE,
                         message.bounded_payload->origin.json_pointer,
                         "bounded payload Message requires exactly one computed length field");
  }
#endif

  for (std::size_t index = 0U; index < member_counts.size(); ++index) {
    if (member_counts[index] == 0U) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::EMPTY_ARRAY,
                           message.bit_containers[index].origin.json_pointer,
                           "bit container must have at least one member field");
    }
  }

  std::sort(spans.begin(), spans.end(), [](const FieldSpan& left, const FieldSpan& right) {
    if (left.begin != right.begin) {
      return left.begin < right.begin;
    }
    return left.end < right.end;
  });
  for (std::size_t index = 1U; index < spans.size(); ++index) {
    if (spans[index].begin < spans[index - 1U].end) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OVERLAP,
                           spans[index].field != nullptr ? spans[index].field->origin.json_pointer
                                                         : message.origin.json_pointer,
                           "ordinary fields and bit container byte ranges must not overlap");
    }
  }

  if (message.integrity.has_value()) {
    const IntegrityIr& integrity = *message.integrity;
    std::uint64_t storage_width = 1U;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    if (integrity.algorithm == IntegrityAlgorithm::CRC) {
      storage_width = static_cast<std::uint64_t>(integrity.crc_width / 8U);
      const std::uint32_t width_mask = integrity.crc_width == 16U ? 0xFFFFU : 0xFFFFFFFFU;
      if ((integrity.crc_width != 16U && integrity.crc_width != 32U) ||
          integrity.crc_polynomial == 0U || (integrity.crc_polynomial & 1U) == 0U ||
          (integrity.crc_polynomial & ~width_mask) != 0U ||
          (integrity.crc_initial_value & ~width_mask) != 0U ||
          (integrity.crc_xor_output & ~width_mask) != 0U ||
          (integrity.storage_byte_order != ByteOrder::BIG &&
           integrity.storage_byte_order != ByteOrder::LITTLE)) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INVALID_ENUM_VALUE, integrity.origin.json_pointer,
                             "CRC parameters are outside the frozen CRC-16/CRC-32 domain");
      }
    }
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (message.bounded_payload.has_value()) {
      const BoundedPayloadIr& layout = *message.bounded_payload;
      if (!integrity.storage_at_payload_end) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTEGRITY_STORAGE_OUT_OF_BOUNDS,
                             integrity.storage_origin.json_pointer,
                             "bounded payload integrity storage must anchor at payload_end");
      }
      if (integrity.range_ends_at_payload) {
        if (integrity.range_offset > layout.header_length) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS,
                               ChildPointer(integrity.range_origin.json_pointer, "byte_offset"),
                               "dynamic integrity range must start at or before the payload");
        }
      } else if (integrity.range_length == 0U ||
                 integrity.range_offset > layout.min_frame_length - layout.trailer_length ||
                 integrity.range_length >
                     layout.min_frame_length - layout.trailer_length - integrity.range_offset) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS,
                             integrity.range_origin.json_pointer,
                             "fixed integrity range must fit before every possible payload_end");
      }
    } else
#endif
    {
      if (integrity.range_length == 0U) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS,
                             ChildPointer(integrity.range_origin.json_pointer, "byte_length"),
                             "integrity range must contain at least one byte");
      }
      if (integrity.range_offset > message.frame_length_bytes ||
          integrity.range_length > message.frame_length_bytes - integrity.range_offset) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTEGRITY_RANGE_OUT_OF_BOUNDS,
                             integrity.range_origin.json_pointer,
                             "integrity range exceeds message frame_length_bytes");
      }
      if (integrity.storage_offset > message.frame_length_bytes ||
          storage_width > message.frame_length_bytes - integrity.storage_offset) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTEGRITY_STORAGE_OUT_OF_BOUNDS,
                             ChildPointer(integrity.storage_origin.json_pointer, "byte_offset"),
                             "integrity storage exceeds message frame_length_bytes");
      }
      const std::uint64_t range_end = integrity.range_offset + integrity.range_length;
      const std::uint64_t storage_end = integrity.storage_offset + storage_width;
      if (integrity.storage_offset < range_end && integrity.range_offset < storage_end) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::INTEGRITY_SELF_INCLUDED,
                             ChildPointer(integrity.storage_origin.json_pointer, "byte_offset"),
                             "integrity storage must be outside its covered range");
      }
      for (const FieldSpan& span : spans) {
        if (integrity.storage_offset < span.end && span.begin < storage_end) {
          return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                               CompileError::INTEGRITY_STORAGE_CONFLICT,
                               ChildPointer(integrity.storage_origin.json_pointer, "byte_offset"),
                               "integrity storage overlaps a field or bit container");
        }
      }
    }
  }

  std::unordered_map<std::uint64_t, std::uint8_t> fixed_matcher_bytes;
  for (const MatcherClauseIr& matcher : message.matcher_clauses) {
    if (matcher.kind == MatcherKind::FRAME_LENGTH_EQUALS) {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      if (message.bounded_payload.has_value()) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::MATCHER_CONFLICT, matcher.origin.json_pointer,
                             "bounded payload Message must not declare frame_length_equals");
      }
#endif
      if (matcher.length_bytes != message.frame_length_bytes) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::MATCHER_CONFLICT,
                             ChildPointer(matcher.origin.json_pointer, "length_bytes"),
                             "frame length matcher conflicts with frame_length_bytes");
      }
      continue;
    }
    if (matcher.bytes.size() > std::numeric_limits<std::uint64_t>::max() - matcher.byte_offset ||
        matcher.byte_offset + matcher.bytes.size() >
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
            (message.bounded_payload.has_value() ? message.bounded_payload->header_length :
#endif
                                                 message.frame_length_bytes
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
             )
#endif
    ) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::MATCHER_OUT_OF_BOUNDS, matcher.origin.json_pointer,
                           "fixed byte matcher exceeds message frame_length_bytes");
    }
    for (std::size_t byte_index = 0U; byte_index < matcher.bytes.size(); ++byte_index) {
      const std::uint64_t absolute_offset = matcher.byte_offset + byte_index;
      const auto [iterator, inserted] =
          fixed_matcher_bytes.emplace(absolute_offset, matcher.bytes[byte_index]);
      if (!inserted && iterator->second != matcher.bytes[byte_index]) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::MATCHER_CONFLICT, matcher.origin.json_pointer,
                             "fixed byte matcher clauses contradict each other");
      }
    }
  }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (message.bounded_payload.has_value() && fixed_matcher_bytes.empty()) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                         CompileError::MATCHER_CONFLICT, message.origin.json_pointer,
                         "bounded payload Message requires at least one fixed_bytes matcher");
  }
#endif

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  for (const FieldIr& field : message.fields) {
    if (!field.computed_length.has_value()) {
      continue;
    }
    for (std::uint64_t index = 0U; index < field.wire.byte_width; ++index) {
      if (fixed_matcher_bytes.find(field.wire.byte_offset + index) != fixed_matcher_bytes.end()) {
        return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                             CompileError::MATCHER_CONFLICT, field.wire.origin.json_pointer,
                             "computed length storage overlaps a fixed_bytes matcher");
      }
    }
  }
#endif

  if (message.integrity.has_value()) {
    std::uint64_t storage_width = 1U;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    if (message.integrity->algorithm == IntegrityAlgorithm::CRC) {
      storage_width = static_cast<std::uint64_t>(message.integrity->crc_width / 8U);
    }
#endif
    for (std::uint64_t index = 0U;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
         !message.bounded_payload.has_value() &&
#endif
         index < storage_width;
         ++index) {
      if (fixed_matcher_bytes.find(message.integrity->storage_offset + index) ==
          fixed_matcher_bytes.end()) {
        continue;
      }
      return SetDiagnostic(
          diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::INTEGRITY_STORAGE_CONFLICT,
          ChildPointer(message.integrity->storage_origin.json_pointer, "byte_offset"),
          "integrity storage overlaps a fixed_bytes matcher");
    }
  }

  for (const FieldIr& field : message.fields) {
    if (field.wire.codec == WireCodec::BITFIELD ||
        (field.value_type != ValueType::UINT64 && field.value_type != ValueType::INT64) ||
        (!field.encode.constant_value.has_value() &&
         !field.encode.signed_constant_value.has_value())) {
      continue;
    }
    const std::uint64_t encoded =
        field.value_type == ValueType::INT64
            ? static_cast<std::uint64_t>(*field.encode.signed_constant_value)
            : *field.encode.constant_value;
    for (std::uint64_t byte_index = 0U; byte_index < field.wire.byte_width; ++byte_index) {
      const auto matcher_byte = fixed_matcher_bytes.find(field.wire.byte_offset + byte_index);
      if (matcher_byte == fixed_matcher_bytes.end()) {
        continue;
      }
      const std::uint8_t encoded_byte = EncodeUnsignedConstantByte(
          encoded, field.wire.byte_width, field.wire.byte_order, byte_index);
      if (matcher_byte->second != encoded_byte) {
        return SetDiagnostic(
            diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::MATCHER_CONFLICT,
            ChildPointer(field.encode.origin.json_pointer, "value"),
            "fixed byte matcher conflicts with the encoded integer constant field bytes");
      }
    }
  }

  std::vector<std::uint64_t> determined_masks(message.bit_containers.size(), 0U);
  std::vector<std::uint64_t> determined_values(message.bit_containers.size(), 0U);
  std::vector<std::uint64_t> constant_masks(message.bit_containers.size(), 0U);
  for (std::size_t index = 0U; index < message.bit_containers.size(); ++index) {
    determined_masks[index] = BitWidthMask(message.bit_containers[index].byte_width);
    determined_values[index] = message.bit_containers[index].base_value;
  }
  for (const FieldIr& field : message.fields) {
    if (field.wire.codec != WireCodec::BITFIELD) {
      continue;
    }
    const BitContainerIr& container = message.bit_containers[field.wire.bit_container_index];
    const std::uint64_t container_bits = container.byte_width * 8U;
    const std::uint64_t shift = container.bit_numbering == BitNumbering::LSB0
                                    ? field.wire.bit_offset
                                    : container_bits - field.wire.bit_offset - field.wire.bit_width;
    const std::uint64_t low_mask = field.wire.bit_width == 64U
                                       ? (std::numeric_limits<std::uint64_t>::max)()
                                       : (std::uint64_t{1U} << field.wire.bit_width) - 1U;
    const std::uint64_t member_mask = low_mask << shift;
    if (field.encode.source == EncodeSource::INPUT) {
      determined_masks[field.wire.bit_container_index] &= ~member_mask;
    } else {
      determined_values[field.wire.bit_container_index] =
          (determined_values[field.wire.bit_container_index] & ~member_mask) |
          ((*field.encode.constant_value << shift) & member_mask);
      constant_masks[field.wire.bit_container_index] |= member_mask;
    }
  }
  for (std::size_t container_index = 0U; container_index < message.bit_containers.size();
       ++container_index) {
    const BitContainerIr& container = message.bit_containers[container_index];
    for (std::uint64_t byte_index = 0U; byte_index < container.byte_width; ++byte_index) {
      const auto matcher = fixed_matcher_bytes.find(container.byte_offset + byte_index);
      if (matcher == fixed_matcher_bytes.end()) {
        continue;
      }
      const std::uint64_t logical_byte = container.byte_order == ByteOrder::BIG
                                             ? container.byte_width - 1U - byte_index
                                             : byte_index;
      const unsigned shift = static_cast<unsigned>(logical_byte * 8U);
      const auto mask = static_cast<std::uint8_t>(determined_masks[container_index] >> shift);
      const auto value = static_cast<std::uint8_t>(determined_values[container_index] >> shift);
      const auto mismatch = static_cast<std::uint8_t>((matcher->second ^ value) & mask);
      if (mismatch == 0U) {
        continue;
      }
      std::string pointer = ChildPointer(container.origin.json_pointer, "base_value");
      const std::uint64_t mismatch_mask = static_cast<std::uint64_t>(mismatch) << shift;
      if ((constant_masks[container_index] & mismatch_mask) != 0U) {
        for (const FieldIr& field : message.fields) {
          if (field.wire.codec != WireCodec::BITFIELD ||
              field.wire.bit_container_index != container_index ||
              !field.encode.constant_value.has_value()) {
            continue;
          }
          const std::uint64_t field_shift =
              container.bit_numbering == BitNumbering::LSB0
                  ? field.wire.bit_offset
                  : container.byte_width * 8U - field.wire.bit_offset - field.wire.bit_width;
          const std::uint64_t field_low_mask =
              field.wire.bit_width == 64U ? (std::numeric_limits<std::uint64_t>::max)()
                                          : (std::uint64_t{1U} << field.wire.bit_width) - 1U;
          if (((field_low_mask << field_shift) & mismatch_mask) != 0U) {
            pointer = ChildPointer(field.encode.origin.json_pointer, "value");
            break;
          }
        }
      }
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::MATCHER_CONFLICT, pointer,
                           "fixed byte matcher conflicts with bit container determined bits");
    }
  }

  struct CoverageSpan {
    std::uint64_t begin = 0U;
    std::uint64_t end = 0U;
  };
  std::vector<CoverageSpan> coverage_spans;
  coverage_spans.reserve(spans.size() + fixed_matcher_bytes.size() +
                         (message.integrity.has_value() ? 1U : 0U));
  std::uint64_t required_coverage = message.frame_length_bytes;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  const bool bounded_payload = message.bounded_payload.has_value();
  if (bounded_payload) required_coverage = message.bounded_payload->header_length;
#else
  constexpr bool bounded_payload = false;
#endif
  for (const FieldSpan& span : spans) {
    if (span.begin < required_coverage) {
      coverage_spans.push_back(CoverageSpan{span.begin, span.end});
    }
  }
  for (const auto& [offset, unused_value] : fixed_matcher_bytes) {
    static_cast<void>(unused_value);
    coverage_spans.push_back(CoverageSpan{offset, offset + 1U});
  }
  if (message.integrity.has_value() && !bounded_payload) {
    std::uint64_t storage_width = 1U;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
    if (message.integrity->algorithm == IntegrityAlgorithm::CRC) {
      storage_width = static_cast<std::uint64_t>(message.integrity->crc_width / 8U);
    }
#endif
    coverage_spans.push_back(CoverageSpan{message.integrity->storage_offset,
                                          message.integrity->storage_offset + storage_width});
  }
  std::sort(coverage_spans.begin(), coverage_spans.end(),
            [](const CoverageSpan& left, const CoverageSpan& right) {
              if (left.begin != right.begin) {
                return left.begin < right.begin;
              }
              return left.end < right.end;
            });
  std::uint64_t covered_end = 0U;
  for (const CoverageSpan& span : coverage_spans) {
    if (span.begin > covered_end) {
      return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                           CompileError::FRAME_NOT_FULLY_DEFINED, message.origin.json_pointer,
                           "field and fixed matcher ranges do not define every frame byte");
    }
    covered_end = (std::max)(covered_end, span.end);
  }
  if (covered_end != required_coverage) {
    return SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                         CompileError::FRAME_NOT_FULLY_DEFINED, message.origin.json_pointer,
                         bounded_payload
                             ? "field and fixed matcher ranges do not define every header byte"
                             : "field and fixed matcher ranges do not define every frame byte");
  }
  return true;
}

}  // namespace

DomainValidationResult DomainValidator::Validate(SchemaIr schema) {
  CompileDiagnostic diagnostic;
  std::unordered_map<std::string, std::size_t> framing_index;
  std::unordered_map<std::string, std::size_t> pipeline_index;
  std::unordered_map<std::string, std::size_t> message_index;
  if (!BuildUniqueIdIndex(
          schema.framing_profiles, framing_index,
          [](const FramingProfileIr& item) -> const std::string& {
            return item.origin.json_pointer;
          },
          diagnostic) ||
      !BuildUniqueIdIndex(
          schema.pipelines, pipeline_index,
          [](const PipelineIr& item) -> const std::string& { return item.origin.json_pointer; },
          diagnostic) ||
      !BuildUniqueIdIndex(
          schema.messages, message_index,
          [](const MessageIr& item) -> const std::string& { return item.origin.json_pointer; },
          diagnostic)) {
    return DomainValidationResult::Failure(std::move(diagnostic));
  }

  ResourceRequirements requirements;
  requirements.framing_profile_count = schema.framing_profiles.size();
  requirements.pipeline_count = schema.pipelines.size();
  requirements.message_count = schema.messages.size();
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  for (const FramingProfileIr& framing : schema.framing_profiles) {
    if (!ValidateStreamFramingLocal(framing, diagnostic)) {
      return DomainValidationResult::Failure(std::move(diagnostic));
    }
    if (framing.input_kind == InputKind::STREAM_CHUNK) {
      const std::uint64_t frame_limit = framing.strategy == FramingStrategy::SYNC_LENGTH_FIELD
                                            ? framing.maximum_frame_length
                                            : framing.frame_length_bytes;
      if (frame_limit > (std::numeric_limits<std::size_t>::max)()) {
        SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                      CompileError::INTEGER_OUT_OF_RANGE, framing.origin.json_pointer,
                      "stream frame limit cannot be represented by this build");
        return DomainValidationResult::Failure(std::move(diagnostic));
      }
      requirements.max_stream_frame_bytes =
          (std::max)(requirements.max_stream_frame_bytes, static_cast<std::size_t>(frame_limit));
      requirements.max_sync_bytes =
          (std::max)(requirements.max_sync_bytes, framing.sync_bytes.size());
    }
  }
  requirements.max_framing_buffer_bytes = requirements.max_stream_frame_bytes;
#endif
  for (MessageIr& message : schema.messages) {
    if (!ValidateMessageDomain(message, requirements, diagnostic)) {
      return DomainValidationResult::Failure(std::move(diagnostic));
    }
  }

  std::vector<ResolvedPipelineIr> resolved_pipelines;
  resolved_pipelines.reserve(schema.pipelines.size());
  for (const PipelineIr& pipeline : schema.pipelines) {
    const auto framing = framing_index.find(pipeline.input_framing_profile_id);
    if (framing == framing_index.end()) {
      SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::UNKNOWN_REFERENCE,
                    ChildPointer(pipeline.origin.json_pointer, "input_framing_profile_id"),
                    "pipeline references an unknown framing profile");
      return DomainValidationResult::Failure(std::move(diagnostic));
    }
    ResolvedPipelineIr resolved;
    resolved.framing_profile_index = framing->second;
    resolved.message_indices.reserve(pipeline.message_ids.size());
    std::unordered_set<std::string> seen_message_ids;
    for (std::size_t reference_index = 0U; reference_index < pipeline.message_ids.size();
         ++reference_index) {
      const std::string& message_id = pipeline.message_ids[reference_index];
      if (!seen_message_ids.emplace(message_id).second) {
        SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                      CompileError::DUPLICATE_REFERENCE,
                      IndexPointer(ChildPointer(pipeline.origin.json_pointer, "message_ids"),
                                   reference_index),
                      "pipeline message reference is duplicated");
        return DomainValidationResult::Failure(std::move(diagnostic));
      }
      const auto message = message_index.find(message_id);
      if (message == message_index.end()) {
        SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::UNKNOWN_REFERENCE,
                      IndexPointer(ChildPointer(pipeline.origin.json_pointer, "message_ids"),
                                   reference_index),
                      "pipeline references an unknown message");
        return DomainValidationResult::Failure(std::move(diagnostic));
      }
      if (schema.messages[message->second].direction_id != pipeline.direction_id) {
        SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::DIRECTION_MISMATCH,
                      IndexPointer(ChildPointer(pipeline.origin.json_pointer, "message_ids"),
                                   reference_index),
                      "pipeline and referenced message direction_id values differ");
        return DomainValidationResult::Failure(std::move(diagnostic));
      }
      for (const std::size_t earlier_message_index : resolved.message_indices) {
        if (FixedMatchersCanIntersect(schema.messages[earlier_message_index],
                                      schema.messages[message->second])) {
          SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                        CompileError::AMBIGUOUS_MATCHER,
                        IndexPointer(ChildPointer(pipeline.origin.json_pointer, "message_ids"),
                                     reference_index),
                        "referenced message matcher intersects an earlier message matcher in this "
                        "pipeline");
          return DomainValidationResult::Failure(std::move(diagnostic));
        }
      }
      resolved.message_indices.push_back(message->second);
    }
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
    const FramingProfileIr& framing_profile = schema.framing_profiles[framing->second];
    if (framing_profile.input_kind == InputKind::STREAM_CHUNK) {
      for (const std::size_t resolved_message_index : resolved.message_indices) {
        const MessageIr& message = schema.messages[resolved_message_index];
        const auto bounds = MessageFrameBounds(message);
        if ((framing_profile.strategy == FramingStrategy::FIXED_LENGTH ||
             framing_profile.strategy == FramingStrategy::SYNC_FIXED_LENGTH) &&
            (bounds.first != framing_profile.frame_length_bytes ||
             bounds.second != framing_profile.frame_length_bytes)) {
          SetDiagnostic(
              diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OUT_OF_BOUNDS,
              ChildPointer(pipeline.origin.json_pointer, "message_ids"),
              "fixed stream profile requires every message to have the same exact length");
          return DomainValidationResult::Failure(std::move(diagnostic));
        }
        if (framing_profile.strategy != FramingStrategy::FIXED_LENGTH &&
            !MessageHasSyncMatcher(message, framing_profile.sync_bytes)) {
          SetDiagnostic(
              diagnostic, CompileStage::DOMAIN_VALIDATION, CompileError::MATCHER_CONFLICT,
              ChildPointer(pipeline.origin.json_pointer, "message_ids"),
              "stream message matcher does not contain the complete offset-zero sync header");
          return DomainValidationResult::Failure(std::move(diagnostic));
        }
        if (framing_profile.strategy == FramingStrategy::SYNC_LENGTH_FIELD &&
            (bounds.first < framing_profile.minimum_frame_length ||
             bounds.second > framing_profile.maximum_frame_length ||
             !MessageHasLengthField(message, framing_profile))) {
          SetDiagnostic(diagnostic, CompileStage::DOMAIN_VALIDATION,
                        CompileError::FIELD_OUT_OF_BOUNDS,
                        ChildPointer(pipeline.origin.json_pointer, "message_ids"),
                        "length-field stream profile and referenced message bounds or computed "
                        "length differ");
          return DomainValidationResult::Failure(std::move(diagnostic));
        }
      }
    }
#endif
    resolved_pipelines.push_back(std::move(resolved));
  }

  return DomainValidationResult::Success(
      ValidatedSchemaIr{std::move(schema), std::move(resolved_pipelines), requirements});
}

namespace {

bool CheckResourceCount(std::size_t value, std::size_t limit, std::string pointer,
                        std::string detail, CompileDiagnostic& diagnostic) {
  if (value <= limit) {
    return true;
  }
  return SetDiagnostic(diagnostic, CompileStage::RESOURCE_BUDGET,
                       CompileError::RESOURCE_LIMIT_EXCEEDED, std::move(pointer),
                       std::move(detail));
}

bool AddStringLayout(protocol_plan::PlanMemoryLayout& layout, std::string_view value) noexcept {
  return value.empty() ||
         layout.AddArray<char>(value.size(), protocol_plan::PlanMemoryCategory::STRING);
}

bool EstimateSchemaPlanMemory(const SchemaIr& schema,
                              const std::vector<ResolvedPipelineIr>& resolved_pipelines,
                              std::size_t limit_bytes, protocol_plan::PlanMemoryReport& output) {
  using namespace protocol_plan;
  PlanMemoryLayout layout{limit_bytes};
  if (!layout.AddArray<PlanBundle>(1U, PlanMemoryCategory::OBJECT) ||
      !AddStringLayout(layout, schema.schema_version) ||
      !AddStringLayout(layout, schema.protocol_id) ||
      !AddStringLayout(layout, schema.protocol_version) ||
      !layout.AddArray<FrozenFramingPlan>(schema.framing_profiles.size(),
                                          PlanMemoryCategory::METADATA_CONTAINER)) {
    return false;
  }
  for (const FramingProfileIr& framing : schema.framing_profiles) {
    if (!AddStringLayout(layout, framing.id)
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
        ||
        !layout.AddArray<std::uint8_t>(framing.sync_bytes.size(), PlanMemoryCategory::EXTENSION) ||
        !layout.AddArray<std::size_t>(framing.sync_bytes.size(), PlanMemoryCategory::EXTENSION)
#endif
    ) {
      return false;
    }
  }
  if (!layout.AddArray<FrozenPipelinePlan>(schema.pipelines.size(),
                                           PlanMemoryCategory::METADATA_CONTAINER)) {
    return false;
  }
  for (std::size_t index = 0U; index < schema.pipelines.size(); ++index) {
    const PipelineIr& pipeline = schema.pipelines[index];
    if (!AddStringLayout(layout, pipeline.id) || !AddStringLayout(layout, pipeline.direction_id) ||
        !layout.AddArray<std::size_t>(resolved_pipelines[index].message_indices.size(),
                                      PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
  }
  if (!layout.AddArray<FrozenMessagePlan>(schema.messages.size(),
                                          PlanMemoryCategory::METADATA_CONTAINER)) {
    return false;
  }
  for (const MessageIr& message : schema.messages) {
    if (!AddStringLayout(layout, message.id) || !AddStringLayout(layout, message.direction_id) ||
        !layout.AddArray<FrozenMatcherPlan>(message.matcher_clauses.size(),
                                            PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
    for (const MatcherClauseIr& matcher : message.matcher_clauses) {
      if (!layout.AddArray<std::uint8_t>(matcher.bytes.size(), PlanMemoryCategory::MATCHER)) {
        return false;
      }
    }
    if (!layout.AddArray<FrozenBitContainerPlan>(message.bit_containers.size(),
                                                 PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
    for (const BitContainerIr& container : message.bit_containers) {
      if (!AddStringLayout(layout, container.id)) {
        return false;
      }
    }
    if (!layout.AddArray<FrozenFieldPlan>(message.fields.size(),
                                          PlanMemoryCategory::METADATA_CONTAINER)) {
      return false;
    }
    for (const FieldIr& field : message.fields) {
      if (!AddStringLayout(layout, field.id) ||
          !layout.AddArray<FrozenEnumEntryPlan>(field.enum_entries.size(),
                                                PlanMemoryCategory::METADATA_CONTAINER)) {
        return false;
      }
      for (const EnumEntryIr& entry : field.enum_entries) {
        if (!AddStringLayout(layout, entry.id)) {
          return false;
        }
      }
    }
  }

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::size_t conversion_count = 0U;
  for (const MessageIr& message : schema.messages) {
    for (const FieldIr& field : message.fields) {
      if (field.conversion.has_value() && !AddSizeChecked(1U, conversion_count)) {
        return false;
      }
    }
  }
  if (!layout.AddArray<LinearConversionDescriptor>(conversion_count,
                                                   PlanMemoryCategory::EXTENSION)) {
    return false;
  }
#endif

  if (!layout.AddArray<MessageExecutionPlan>(schema.messages.size(),
                                             PlanMemoryCategory::EXECUTION_DESCRIPTOR)) {
    return false;
  }
  for (const MessageIr& message : schema.messages) {
    std::unordered_set<std::size_t> fixed_byte_offsets;
    for (const MatcherClauseIr& matcher : message.matcher_clauses) {
      if (matcher.kind != MatcherKind::FIXED_BYTES) {
        continue;
      }
      const std::size_t offset = static_cast<std::size_t>(matcher.byte_offset);
      for (std::size_t byte_index = 0U; byte_index < matcher.bytes.size(); ++byte_index) {
        fixed_byte_offsets.emplace(offset + byte_index);
      }
    }
    std::size_t enum_entry_count = 0U;
    for (const FieldIr& field : message.fields) {
      enum_entry_count += field.enum_entries.size();
    }
    if (!layout.AddArray<FixedByteExecutionPlan>(fixed_byte_offsets.size(),
                                                 PlanMemoryCategory::MATCHER) ||
        !layout.AddArray<BitContainerExecutionPlan>(message.bit_containers.size(),
                                                    PlanMemoryCategory::EXECUTION_DESCRIPTOR) ||
        !layout.AddArray<FieldExecutionPlan>(message.fields.size(),
                                             PlanMemoryCategory::EXECUTION_DESCRIPTOR) ||
        !layout.AddArray<std::uint64_t>(enum_entry_count, PlanMemoryCategory::INDEX) ||
        !layout.AddArray<EnumLookupExecutionPlan>(enum_entry_count, PlanMemoryCategory::INDEX)) {
      return false;
    }
  }

  if (!layout.AddArray<PipelineExecutionPlan>(schema.pipelines.size(),
                                              PlanMemoryCategory::EXECUTION_DESCRIPTOR)) {
    return false;
  }
  const std::size_t allowed_word_count =
      schema.messages.size() / 64U + (schema.messages.size() % 64U == 0U ? 0U : 1U);
  for (const ResolvedPipelineIr& pipeline : resolved_pipelines) {
    std::map<std::uint64_t, std::size_t> candidate_groups;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    std::size_t variable_count = 0U;
#endif
    for (const std::size_t message_index : pipeline.message_indices) {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      if (schema.messages[message_index].bounded_payload.has_value()) {
        ++variable_count;
        continue;
      }
#endif
      ++candidate_groups[schema.messages[message_index].frame_length_bytes];
    }
    if (!layout.AddArray<std::uint64_t>(allowed_word_count, PlanMemoryCategory::INDEX) ||
        !layout.AddArray<CandidateGroupExecutionPlan>(candidate_groups.size(),
                                                      PlanMemoryCategory::EXECUTION_DESCRIPTOR)
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
        || !layout.AddArray<std::size_t>(variable_count, PlanMemoryCategory::INDEX)
#endif
    ) {
      return false;
    }
    for (const auto& [frame_length, message_count] : candidate_groups) {
      static_cast<void>(frame_length);
      if (!layout.AddArray<std::size_t>(message_count, PlanMemoryCategory::INDEX)) {
        return false;
      }
    }
  }
  output = layout.Report();
  return true;
}

void SetPlanMemoryDiagnostic(CompileDiagnostic& diagnostic, std::size_t required_bytes,
                             std::size_t limit_bytes, ResourceProfile profile) {
  SetDiagnostic(diagnostic, CompileStage::RESOURCE_BUDGET, CompileError::RESOURCE_LIMIT_EXCEEDED,
                "", "accounted immutable plan memory exceeds the selected resource profile");
  diagnostic.resource_kind = ResourceKind::PLAN_ACCOUNTED_MEMORY;
  diagnostic.required_bytes = required_bytes;
  diagnostic.limit_bytes = limit_bytes;
  diagnostic.resource_profile = profile;
}

}  // namespace

ResourceBudgetResult ResourceBudgetValidator::Validate(ValidatedSchemaIr validated) {
  return ValidateImpl(std::move(validated), std::nullopt);
}

ResourceBudgetResult ResourceBudgetValidator::ValidateForTest(ValidatedSchemaIr validated,
                                                              std::size_t plan_memory_limit_bytes) {
  return ValidateImpl(std::move(validated), plan_memory_limit_bytes);
}

ResourceBudgetResult ResourceBudgetValidator::ValidateImpl(
    ValidatedSchemaIr validated, std::optional<std::size_t> test_plan_memory_limit_bytes) {
  CompileDiagnostic diagnostic;
  if (validated.payload_ == nullptr) {
    SetDiagnostic(diagnostic, CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION, "",
                  "moved-from validated schema capability was reused");
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
  const protocol_plan::ResourceProfileLimits* budget =
      protocol_plan::GetResourceProfileLimits(validated.payload_->schema.resource_profile);
  if (budget == nullptr) {
    SetDiagnostic(diagnostic, CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION,
                  "/resource_profile", "validated resource profile has no frozen limits");
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
  const ResourceRequirements& requirements = validated.payload_->requirements;
  if (requirements.max_frame_bytes > budget->max_frame_bytes) {
    SetDiagnostic(diagnostic, CompileStage::RESOURCE_BUDGET, CompileError::RESOURCE_LIMIT_EXCEEDED,
                  "/messages", "maximum frame length exceeds the selected resource profile");
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
  if (requirements.max_stream_frame_bytes > budget->max_frame_bytes ||
      requirements.max_stream_frame_bytes > protocol_plan::kMaxStreamFrameBytesHardLimit) {
    SetDiagnostic(diagnostic, CompileStage::RESOURCE_BUDGET, CompileError::RESOURCE_LIMIT_EXCEEDED,
                  "/framing_profiles", "stream frame length exceeds the selected resource profile");
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
  if (requirements.max_sync_bytes > budget->max_sync_bytes ||
      requirements.max_sync_bytes > protocol_plan::kMaxStreamSyncBytesHardLimit) {
    SetDiagnostic(diagnostic, CompileStage::RESOURCE_BUDGET, CompileError::RESOURCE_LIMIT_EXCEEDED,
                  "/framing_profiles", "sync header length exceeds the selected resource profile");
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
  if (requirements.max_framing_buffer_bytes > budget->max_session_memory_bytes ||
      requirements.max_framing_buffer_bytes >
          protocol_plan::kMaxStreamSessionMemoryBytesHardLimit) {
    SetDiagnostic(diagnostic, CompileStage::RESOURCE_BUDGET, CompileError::RESOURCE_LIMIT_EXCEEDED,
                  "/framing_profiles", "framing buffer bytes exceed the per-stream memory limit");
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
#endif
  if (!CheckResourceCount(
          requirements.framing_profile_count, budget->max_framing_profiles, "/framing_profiles",
          "framing profile count exceeds the selected resource profile", diagnostic) ||
      !CheckResourceCount(requirements.pipeline_count, budget->max_pipelines, "/pipelines",
                          "pipeline count exceeds the selected resource profile", diagnostic) ||
      !CheckResourceCount(requirements.message_count, budget->max_messages, "/messages",
                          "message count exceeds the selected resource profile", diagnostic) ||
      !CheckResourceCount(requirements.total_field_count, budget->max_total_fields, "/messages",
                          "total field count exceeds the selected resource profile", diagnostic) ||
      !CheckResourceCount(requirements.total_matcher_count, budget->max_total_matchers, "/messages",
                          "matcher count exceeds the selected resource profile", diagnostic) ||
      !CheckResourceCount(requirements.total_enum_entry_count, budget->max_total_enum_entries,
                          "/messages", "enum entry count exceeds the selected resource profile",
                          diagnostic) ||
      !CheckResourceCount(requirements.total_bit_container_count, budget->max_total_fields,
                          "/messages", "bit container count exceeds the selected resource profile",
                          diagnostic) ||
      !CheckResourceCount(requirements.total_integrity_rule_count, budget->max_messages,
                          "/messages", "integrity rule count exceeds the message count limit",
                          diagnostic)) {
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (!CheckResourceCount(requirements.total_computed_length_count, budget->max_messages,
                          "/messages", "computed length count exceeds the message count limit",
                          diagnostic)) {
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
#endif
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (!CheckResourceCount(requirements.total_conversion_count, budget->max_total_fields,
                          "/messages", "conversion count exceeds the total field limit",
                          diagnostic)) {
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
#endif
  for (const MessageIr& message : validated.payload_->schema.messages) {
    if (!CheckResourceCount(message.fields.size(), budget->max_fields_per_message,
                            ChildPointer(message.origin.json_pointer, "fields"),
                            "field count exceeds the selected resource profile", diagnostic) ||
        !CheckResourceCount(message.matcher_clauses.size(), budget->max_matchers_per_message,
                            ChildPointer(message.origin.json_pointer, "matcher"),
                            "matcher count exceeds the selected resource profile", diagnostic) ||
        !CheckResourceCount(message.bit_containers.size(), budget->max_fields_per_message,
                            ChildPointer(message.origin.json_pointer, "bit_containers"),
                            "bit container count exceeds the selected resource profile",
                            diagnostic)) {
      return ResourceBudgetResult::Failure(std::move(diagnostic));
    }
    for (const FieldIr& field : message.fields) {
      if (!CheckResourceCount(field.enum_entries.size(), budget->max_enum_entries_per_field,
                              ChildPointer(field.origin.json_pointer, "enum_entries"),
                              "enum entry count exceeds the selected resource profile",
                              diagnostic)) {
        return ResourceBudgetResult::Failure(std::move(diagnostic));
      }
    }
  }
  const std::size_t plan_memory_limit =
      (std::min)(test_plan_memory_limit_bytes.value_or(budget->max_plan_memory_bytes),
                 protocol_plan::kV01MaxPlanMemoryHardLimit);
  protocol_plan::PlanMemoryReport plan_memory;
  if (!EstimateSchemaPlanMemory(validated.payload_->schema, validated.payload_->resolved_pipelines,
                                (std::numeric_limits<std::size_t>::max)(), plan_memory)) {
    SetPlanMemoryDiagnostic(diagnostic, (std::numeric_limits<std::size_t>::max)(),
                            plan_memory_limit, validated.payload_->schema.resource_profile);
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
  if (plan_memory.accounted_total_bytes > plan_memory_limit) {
    SetPlanMemoryDiagnostic(diagnostic, plan_memory.accounted_total_bytes, plan_memory_limit,
                            validated.payload_->schema.resource_profile);
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }
  return ResourceBudgetResult::Success(
      BudgetedSchemaIr{std::move(validated), plan_memory, plan_memory_limit});
}

PlanDraftAssemblyResult PlanDraftAssembler::Assemble(BudgetedSchemaIr budgeted) {
  if (budgeted.validated_ == nullptr || budgeted.validated_->payload_ == nullptr) {
    return PlanDraftAssemblyResult::Failure(
        CompileDiagnostic{CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION, "",
                          std::nullopt, "moved-from budgeted schema capability was reused"});
  }
  SchemaIr& schema = budgeted.validated_->payload_->schema;
  std::vector<FramingPlan> framing_plans;
  framing_plans.reserve(schema.framing_profiles.size());
  for (const FramingProfileIr& framing : schema.framing_profiles) {
    FramingPlan plan;
    plan.id = framing.id;
    plan.input_kind = framing.input_kind;
#if defined(PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING)
    plan.strategy = framing.strategy;
    plan.frame_length_bytes = framing.frame_length_bytes;
    plan.sync_bytes = framing.sync_bytes;
    plan.sync_prefix_table.resize(plan.sync_bytes.size(), 0U);
    for (std::size_t index = 1U, prefix = 0U; index < plan.sync_bytes.size(); ++index) {
      while (prefix != 0U && plan.sync_bytes[index] != plan.sync_bytes[prefix]) {
        prefix = plan.sync_prefix_table[prefix - 1U];
      }
      if (plan.sync_bytes[index] == plan.sync_bytes[prefix]) {
        ++prefix;
      }
      plan.sync_prefix_table[index] = prefix;
    }
    plan.length_field_offset = framing.length_field_offset;
    plan.length_field_width = framing.length_field_width;
    plan.length_field_byte_order = framing.length_field_byte_order;
    plan.minimum_frame_length = framing.minimum_frame_length;
    plan.maximum_frame_length = framing.maximum_frame_length;
#endif
    framing_plans.push_back(std::move(plan));
  }

  std::vector<PipelinePlan> pipeline_plans;
  pipeline_plans.reserve(schema.pipelines.size());
  for (std::size_t index = 0U; index < schema.pipelines.size(); ++index) {
    const PipelineIr& pipeline = schema.pipelines[index];
    ResolvedPipelineIr& resolved = budgeted.validated_->payload_->resolved_pipelines[index];
    pipeline_plans.push_back(PipelinePlan{pipeline.id, pipeline.direction_id,
                                          resolved.framing_profile_index,
                                          std::move(resolved.message_indices)});
  }

  std::vector<MessagePlan> message_plans;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  std::vector<protocol_plan::LinearConversionDescriptor> conversion_plans;
  conversion_plans.reserve(budgeted.validated_->payload_->requirements.total_conversion_count);
#endif
  message_plans.reserve(schema.messages.size());
  for (MessageIr& message : schema.messages) {
    MessagePlan message_plan;
    message_plan.id = std::move(message.id);
    message_plan.direction_id = std::move(message.direction_id);
    message_plan.frame_length_bytes = message.frame_length_bytes;
    message_plan.matchers.reserve(message.matcher_clauses.size());
    for (MatcherClauseIr& matcher : message.matcher_clauses) {
      message_plan.matchers.push_back(MatcherPlan{matcher.kind, matcher.length_bytes,
                                                  matcher.byte_offset, std::move(matcher.bytes)});
    }
    message_plan.bit_containers.reserve(message.bit_containers.size());
    for (BitContainerIr& container : message.bit_containers) {
      message_plan.bit_containers.push_back(protocol_plan::BitContainerPlan{
          std::move(container.id), container.byte_offset, container.byte_width,
          container.byte_order, container.bit_numbering, container.base_value});
    }
    if (message.integrity.has_value()) {
      protocol_plan::IntegrityPlan integrity;
      integrity.algorithm = message.integrity->algorithm;
      integrity.range_offset = message.integrity->range_offset;
      integrity.range_length = message.integrity->range_length;
      integrity.storage_offset = message.integrity->storage_offset;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      integrity.range_ends_at_payload = message.integrity->range_ends_at_payload;
      integrity.storage_at_payload_end = message.integrity->storage_at_payload_end;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      integrity.crc_width = message.integrity->crc_width;
      integrity.crc_polynomial = message.integrity->crc_polynomial;
      integrity.crc_initial_value = message.integrity->crc_initial_value;
      integrity.crc_xor_output = message.integrity->crc_xor_output;
      integrity.crc_reflect_input = message.integrity->crc_reflect_input;
      integrity.crc_reflect_output = message.integrity->crc_reflect_output;
      integrity.storage_byte_order = message.integrity->storage_byte_order;
#endif
      message_plan.integrity = integrity;
    }
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
    if (message.bounded_payload.has_value()) {
      message_plan.bounded_payload = protocol_plan::BoundedPayloadPlan{
          message.bounded_payload->payload_field_index, message.bounded_payload->header_length,
          message.bounded_payload->min_payload_length,  message.bounded_payload->max_payload_length,
          message.bounded_payload->trailer_length,      message.bounded_payload->min_frame_length,
          message.bounded_payload->max_frame_length};
    }
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
    for (std::size_t field_index = 0U; field_index < message.fields.size(); ++field_index) {
      const FieldIr& field = message.fields[field_index];
      if (!field.computed_length.has_value()) {
        continue;
      }
      const ComputedLengthIr& computed = *field.computed_length;
      protocol_plan::ComputedLengthPlan descriptor;
      descriptor.field_index = field_index;
      descriptor.storage_offset = field.wire.byte_offset;
      descriptor.storage_width = field.wire.byte_width;
      descriptor.byte_order = field.wire.byte_order;
      descriptor.scope = computed.scope;
      descriptor.range_offset = computed.range_offset;
      descriptor.range_length = computed.range_length;
      descriptor.expected_value = computed.scope == ComputedLengthScope::FRAME
                                      ? message.frame_length_bytes
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
                                  : computed.scope == ComputedLengthScope::PAYLOAD
                                      ? message.bounded_payload->max_payload_length
#endif
                                      : computed.range_length;
      message_plan.computed_length = descriptor;
      break;
    }
#endif
    message_plan.fields.reserve(message.fields.size());
    for (FieldIr& field : message.fields) {
      FieldPlan field_plan;
      field_plan.id = std::move(field.id);
      field_plan.value_type = field.value_type;
      field_plan.wire_codec = field.wire.codec;
      field_plan.byte_offset = field.wire.byte_offset;
      field_plan.byte_width = field.wire.byte_width;
      field_plan.byte_order = field.wire.byte_order;
      field_plan.encode_source = field.encode.source;
      field_plan.constant_value = field.encode.constant_value;
      field_plan.signed_constant_value = field.encode.signed_constant_value;
      field_plan.unknown_enum_policy = field.unknown_enum_policy;
      field_plan.enum_entries.reserve(field.enum_entries.size());
      field_plan.bit_container_index = field.wire.bit_container_index;
      field_plan.bit_offset = field.wire.bit_offset;
      field_plan.bit_width = field.wire.bit_width;
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      if (field.conversion.has_value()) {
        field_plan.conversion_index = conversion_plans.size();
        conversion_plans.push_back(field.conversion->derived);
      }
#endif
      for (EnumEntryIr& entry : field.enum_entries) {
        field_plan.enum_entries.push_back(EnumEntryPlan{std::move(entry.id), entry.raw_value});
      }
      message_plan.fields.push_back(std::move(field_plan));
    }
    message_plans.push_back(std::move(message_plan));
  }

  auto draft = std::make_unique<protocol_plan::detail::PlanDraftData>();
  draft->schema_version = std::move(schema.schema_version);
  draft->protocol_id = std::move(schema.protocol_id);
  draft->protocol_version = std::move(schema.protocol_version);
  draft->resource_profile = schema.resource_profile;
  draft->resource_requirements = budgeted.validated_->payload_->requirements;
  draft->framing_profiles = std::move(framing_plans);
  draft->pipelines = std::move(pipeline_plans);
  draft->messages = std::move(message_plans);
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  draft->conversions = std::move(conversion_plans);
#endif
  draft->approved_plan_memory = budgeted.plan_memory_;
  draft->plan_memory_limit_bytes = budgeted.plan_memory_limit_bytes_;
  return PlanDraftAssemblyResult::Success(protocol_plan::BudgetedPlanDraft{std::move(draft)});
}

CompileResult FreezeBudgetedPlanDraft(protocol_plan::BudgetedPlanDraft draft) {
  protocol_plan::PlanBuildResult frozen = protocol_plan::PlanBuilder::Freeze(std::move(draft));
  if (frozen.Succeeded()) {
    return CompileResult::Success(std::move(frozen).TakePlan());
  }
  if (frozen.Diagnostic() != nullptr &&
      frozen.Diagnostic()->code == protocol_plan::PlanBuildError::ALLOCATION_FAILED) {
    return CompileResult::Failure(
        CompileDiagnostic{CompileStage::INTERNAL, CompileError::COMPILER_ALLOCATION_FAILED, "",
                          std::nullopt, "memory allocation failed while freezing the plan"});
  }
  if (frozen.Diagnostic() != nullptr &&
      frozen.Diagnostic()->code == protocol_plan::PlanBuildError::PLAN_MEMORY_LIMIT_EXCEEDED) {
    CompileDiagnostic diagnostic;
    SetPlanMemoryDiagnostic(diagnostic, frozen.Diagnostic()->required_bytes,
                            frozen.Diagnostic()->limit_bytes,
                            frozen.Diagnostic()->resource_profile);
    return CompileResult::Failure(std::move(diagnostic));
  }
  if (frozen.Diagnostic() != nullptr &&
      frozen.Diagnostic()->code == protocol_plan::PlanBuildError::PLAN_MEMORY_ESTIMATE_MISMATCH) {
    return CompileResult::Failure(
        CompileDiagnostic{CompileStage::PLAN_BUILD, CompileError::INTERNAL_CONTRACT_VIOLATION, "",
                          std::nullopt, "approved and constructed plan memory reports differ"});
  }
  return CompileResult::Failure(CompileDiagnostic{
      CompileStage::PLAN_BUILD, CompileError::INTERNAL_CONTRACT_VIOLATION, "", std::nullopt,
      "validated configuration violated the frozen plan construction contract"});
}

static ResourceBudgetResult CompileJsonToBudgetedSchema(
    std::string_view json_bytes, std::optional<std::size_t> plan_memory_limit) {
  const JsonAuditLimits limits;
  CompileDiagnostic diagnostic;
  if (!RunStrictPrecheck(json_bytes, limits, diagnostic)) {
    return ResourceBudgetResult::Failure(std::move(diagnostic));
  }

  SchemaIr schema;
  {
    const std::size_t parser_memory_bytes =
        yyjson_read_max_memory_usage(json_bytes.size(), kReadFlags);
    if (parser_memory_bytes == 0U || parser_memory_bytes > limits.max_parser_memory_bytes) {
      return ResourceBudgetResult::Failure(CompileDiagnostic{
          CompileStage::JSON_RESOURCE, CompileError::JSON_PARSER_MEMORY_LIMIT_EXCEEDED, "",
          std::nullopt, "yyjson candidate memory upper bound exceeds the 64 MiB hard limit"});
    }
    std::unique_ptr<unsigned char[]> parser_pool{
        new (std::nothrow) unsigned char[parser_memory_bytes]};
    if (parser_pool == nullptr) {
      return ResourceBudgetResult::Failure(CompileDiagnostic{
          CompileStage::JSON_RESOURCE, CompileError::JSON_ALLOCATION_FAILED, "", std::nullopt,
          "failed to reserve the bounded yyjson candidate memory pool"});
    }
    yyjson_alc parser_allocator{};
    if (!yyjson_alc_pool_init(&parser_allocator, parser_pool.get(), parser_memory_bytes)) {
      return ResourceBudgetResult::Failure(CompileDiagnostic{
          CompileStage::JSON_RESOURCE, CompileError::JSON_ALLOCATION_FAILED, "", std::nullopt,
          "failed to initialize the bounded yyjson candidate memory pool"});
    }

    yyjson_read_err error{};
    DocumentPtr document{yyjson_read_opts(const_cast<char*>(json_bytes.data()), json_bytes.size(),
                                          kReadFlags, &parser_allocator, &error)};
    if (document == nullptr) {
      const bool allocation_failure = error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION;
      return ResourceBudgetResult::Failure(CompileDiagnostic{
          allocation_failure ? CompileStage::JSON_RESOURCE : CompileStage::JSON_SYNTAX,
          allocation_failure ? CompileError::JSON_ALLOCATION_FAILED
                             : CompileError::JSON_SYNTAX_ERROR,
          "", allocation_failure ? std::nullopt : std::optional<std::size_t>{error.pos},
          allocation_failure ? "yyjson candidate allocation failed"
                             : "yyjson candidate rejected JSON syntax"});
    }
    yyjson_val* root = yyjson_doc_get_root(document.get());
    if (root == nullptr) {
      return ResourceBudgetResult::Failure(
          CompileDiagnostic{CompileStage::JSON_SYNTAX, CompileError::JSON_SYNTAX_ERROR, "",
                            std::nullopt, "yyjson candidate produced no root value"});
    }

    JsonAuditStats stats;
    std::string pointer;
    if (!AuditJsonValue(root, 1U, pointer, limits, stats, diagnostic) ||
        !BuildSchemaIr(root, schema, diagnostic)) {
      return ResourceBudgetResult::Failure(std::move(diagnostic));
    }
  }

  DomainValidationResult validated = DomainValidator::Validate(std::move(schema));
  if (!validated.Succeeded()) {
    return ResourceBudgetResult::Failure(std::move(validated).TakeDiagnostic());
  }

  return plan_memory_limit.has_value()
             ? ResourceBudgetValidator::ValidateForTest(std::move(validated).TakeCapability(),
                                                        *plan_memory_limit)
             : ResourceBudgetValidator::Validate(std::move(validated).TakeCapability());
}

CompileResult CompileJsonToPlanImpl(std::string_view json_bytes,
                                    std::optional<std::size_t> plan_memory_limit) {
  try {
    ResourceBudgetResult budgeted = CompileJsonToBudgetedSchema(json_bytes, plan_memory_limit);
    if (!budgeted.Succeeded()) {
      return CompileResult::Failure(std::move(budgeted).TakeDiagnostic());
    }

    PlanDraftAssemblyResult assembled =
        PlanDraftAssembler::Assemble(std::move(budgeted).TakeCapability());
    if (!assembled.Succeeded()) {
      return CompileResult::Failure(std::move(assembled).TakeDiagnostic());
    }
    return FreezeBudgetedPlanDraft(std::move(assembled).TakeCapability());
  } catch (const std::bad_alloc&) {
    return Reject(CompileStage::INTERNAL, CompileError::COMPILER_ALLOCATION_FAILED, "",
                  "memory allocation failed during configuration compilation");
  } catch (...) {
    return Reject(CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION, "",
                  "unexpected exception escaped an internal compiler stage");
  }
}

CompileResult CompileJsonToPlan(std::string_view json_bytes) {
  return CompileJsonToPlanImpl(json_bytes, std::nullopt);
}

std::size_t JsonParserPoolUpperBoundForTest(std::size_t input_size) noexcept {
  return yyjson_read_max_memory_usage(input_size, kReadFlags);
}

CompileUiArtifactsResult CompileJsonToPlanWithUiDescription(
    std::string_view json_bytes, std::size_t description_memory_limit_bytes) {
  try {
    ResourceBudgetResult budgeted = CompileJsonToBudgetedSchema(json_bytes, std::nullopt);
    if (!budgeted.Succeeded()) {
      return CompileUiArtifactsResult::Failure(std::move(budgeted).TakeDiagnostic());
    }
    BudgetedSchemaIr budgeted_capability = std::move(budgeted).TakeCapability();
    UiDescriptionSidecar sidecar;
    CompileDiagnostic diagnostic;
    if (!UiDescriptionBuilder::Build(budgeted_capability, description_memory_limit_bytes, sidecar,
                                     diagnostic)) {
      return CompileUiArtifactsResult::Failure(std::move(diagnostic));
    }
    PlanDraftAssemblyResult assembled =
        PlanDraftAssembler::Assemble(std::move(budgeted_capability));
    if (!assembled.Succeeded()) {
      return CompileUiArtifactsResult::Failure(std::move(assembled).TakeDiagnostic());
    }
    if (!UiDescriptionPlanFreezeAllowedForTest(diagnostic)) {
      return CompileUiArtifactsResult::Failure(std::move(diagnostic));
    }
    CompileResult frozen = FreezeBudgetedPlanDraft(std::move(assembled).TakeCapability());
    if (!frozen.Succeeded()) {
      return CompileUiArtifactsResult::Failure(*frozen.Diagnostic());
    }
    protocol_plan::PlanOwner plan = std::move(frozen).TakePlan();
    if (!UiDescriptionBuilder::Audit(*plan, sidecar, diagnostic)) {
      return CompileUiArtifactsResult::Failure(std::move(diagnostic));
    }
    return CompileUiArtifactsResult::Success(std::move(plan), std::move(sidecar));
  } catch (const std::bad_alloc&) {
    return CompileUiArtifactsResult::Failure(CompileDiagnostic{
        CompileStage::INTERNAL, CompileError::COMPILER_ALLOCATION_FAILED, "", std::nullopt,
        "memory allocation failed during UI configuration compilation"});
  } catch (...) {
    return CompileUiArtifactsResult::Failure(CompileDiagnostic{
        CompileStage::INTERNAL, CompileError::INTERNAL_CONTRACT_VIOLATION, "", std::nullopt,
        "unexpected exception escaped an internal UI compiler stage"});
  }
}

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
CompileResult CompileJsonToPlanWithPlanMemoryLimitForTest(std::string_view json_bytes,
                                                          std::size_t plan_memory_limit_bytes) {
  return CompileJsonToPlanImpl(json_bytes, plan_memory_limit_bytes);
}
#endif

}  // namespace pae::config_compiler
