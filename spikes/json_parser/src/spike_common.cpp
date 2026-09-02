#include "spike_common.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "strict_json_corpus_generated.h"

namespace pae::json_spike {
namespace {

struct Fixture {
  std::string name;
  std::string input;
  ErrorCode expected = ErrorCode::kInternalError;
  Limits limits;
  bool verify_json_pointer = false;
  std::string expected_json_pointer;
  bool known_contract_gap = false;
  ErrorCode contract_target = ErrorCode::kOk;
  std::string target_lock_candidate;
  bool require_json_pointer_absent = false;
  bool verify_byte_offset = false;
  bool require_byte_offset_absent = false;
  std::size_t expected_byte_offset = kNoByteOffset;
  ByteOffsetKind expected_byte_offset_kind = ByteOffsetKind::kNone;
  bool verify_byte_offset_kind_only = false;
  bool open_decision = false;
  std::string source = "embedded";
  std::string source_state = "LEGACY_EMBEDDED";
  std::string rule_id = "SPIKE";
  std::string suite = "SPIKE_PROBE";
};

ErrorCode ParseErrorCode(std::string_view code) {
  constexpr ErrorCode kCodes[] = {
      ErrorCode::kOk,
      ErrorCode::kInputLimit,
      ErrorCode::kBomNotAllowed,
      ErrorCode::kInvalidUtf8,
      ErrorCode::kSyntaxError,
      ErrorCode::kDuplicateKey,
      ErrorCode::kDepthLimit,
      ErrorCode::kNodeLimit,
      ErrorCode::kObjectLimit,
      ErrorCode::kStringLimit,
      ErrorCode::kArrayLimit,
      ErrorCode::kNumberTokenLimit,
      ErrorCode::kTotalStringLimit,
      ErrorCode::kIntegerNotExact,
      ErrorCode::kIntegerOutOfRange,
      ErrorCode::kRealOutOfRange,
      ErrorCode::kUnknownField,
      ErrorCode::kMissingField,
      ErrorCode::kTypeMismatch,
      ErrorCode::kReferenceNotFound,
      ErrorCode::kSemanticConstraint,
      ErrorCode::kResourceExhausted,
      ErrorCode::kInternalError,
  };
  for (const ErrorCode candidate : kCodes) {
    if (code == ToString(candidate)) {
      return candidate;
    }
  }
  throw std::logic_error{"unknown generated corpus error code"};
}

ByteOffsetKind ParseByteOffsetKind(std::string_view kind) {
  if (kind == "NONE") {
    return ByteOffsetKind::kNone;
  }
  if (kind == "EXACT_INPUT_BYTE") {
    return ByteOffsetKind::kExactInputByte;
  }
  if (kind == "PARSER_REPORTED_POSITION") {
    return ByteOffsetKind::kParserReportedPosition;
  }
  throw std::logic_error{"unknown generated corpus byte offset kind"};
}

bool IsContinuationByte(unsigned char value) noexcept { return (value & 0xC0U) == 0x80U; }

bool ValidateUtf8(std::string_view input, std::size_t& invalid_offset) noexcept {
  std::size_t index = 0U;
  while (index < input.size()) {
    const auto first = static_cast<unsigned char>(input[index]);
    if (first <= 0x7FU) {
      ++index;
      continue;
    }

    std::size_t continuation_count = 0U;
    std::uint32_t code_point = 0U;
    std::uint32_t minimum_code_point = 0U;
    if ((first & 0xE0U) == 0xC0U) {
      continuation_count = 1U;
      code_point = static_cast<std::uint32_t>(first & 0x1FU);
      minimum_code_point = 0x80U;
    } else if ((first & 0xF0U) == 0xE0U) {
      continuation_count = 2U;
      code_point = static_cast<std::uint32_t>(first & 0x0FU);
      minimum_code_point = 0x800U;
    } else if ((first & 0xF8U) == 0xF0U) {
      continuation_count = 3U;
      code_point = static_cast<std::uint32_t>(first & 0x07U);
      minimum_code_point = 0x10000U;
    } else {
      invalid_offset = index;
      return false;
    }

    if (continuation_count > input.size() - index - 1U) {
      invalid_offset = index;
      return false;
    }

    for (std::size_t offset = 1U; offset <= continuation_count; ++offset) {
      const auto continuation = static_cast<unsigned char>(input[index + offset]);
      if (!IsContinuationByte(continuation)) {
        invalid_offset = index + offset;
        return false;
      }
      code_point = (code_point << 6U) | static_cast<std::uint32_t>(continuation & 0x3FU);
    }

    if (code_point < minimum_code_point || (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
        code_point > 0x10FFFFU) {
      invalid_offset = index;
      return false;
    }

    index += continuation_count + 1U;
  }

  return true;
}

int HexDigitValue(char character) noexcept {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'a' && character <= 'f') {
    return character - 'a' + 10;
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

std::string DecodeCorpusHex(std::string_view hex) {
  if (hex == "-") {
    return {};
  }
  if (hex.size() % 2U != 0U) {
    throw std::logic_error{"generated corpus hex length is not even"};
  }

  std::string decoded;
  decoded.reserve(hex.size() / 2U);
  for (std::size_t index = 0U; index < hex.size(); index += 2U) {
    const int high = HexDigitValue(hex[index]);
    const int low = HexDigitValue(hex[index + 1U]);
    if (high < 0 || low < 0) {
      throw std::logic_error{"generated corpus contains invalid hex"};
    }
    decoded.push_back(static_cast<char>((high << 4) | low));
  }
  return decoded;
}

std::size_t ParseCorpusSize(std::string_view decimal) {
  std::size_t value = 0U;
  for (const char character : decimal) {
    if (character < '0' || character > '9') {
      throw std::logic_error{"generated corpus contains invalid size"};
    }
    const std::size_t digit = static_cast<std::size_t>(character - '0');
    if (value > ((std::numeric_limits<std::size_t>::max)() - digit) / 10U) {
      throw std::logic_error{"generated corpus size is out of range"};
    }
    value = value * 10U + digit;
  }
  return value;
}

Limits FindCorpusLimits(std::string_view limits_id) {
  for (const auto& record : corpus_generated::kLimits) {
    if (limits_id == record.id) {
      return Limits{
          record.max_input_bytes,  record.max_depth,          record.max_nodes,
          record.max_string_bytes, record.max_array_elements, record.max_parser_memory_bytes,
      };
    }
  }
  throw std::logic_error{"generated corpus references unknown limits"};
}

void AppendGeneratedCorpusFixtures(std::vector<Fixture>& fixtures) {
  for (const auto& record : corpus_generated::kFixtures) {
    const auto duplicate =
        std::find_if(fixtures.cbegin(), fixtures.cend(),
                     [&record](const Fixture& fixture) { return fixture.name == record.case_id; });
    if (duplicate != fixtures.cend()) {
      throw std::logic_error{"generated corpus case duplicates embedded fixture"};
    }

    Fixture fixture;
    fixture.name = record.case_id;
    fixture.input = DecodeCorpusHex(record.input_hex);
    if (fixture.input.size() != record.expected_input_size) {
      throw std::logic_error{"generated corpus input size changed"};
    }
    fixture.expected = ParseErrorCode(record.expected_code);
    fixture.limits = FindCorpusLimits(record.limits_id);
    fixture.source = "strict_json_corpus_v0.1.tsv";
    fixture.source_state = "MANIFEST_AUTHORITY";
    fixture.rule_id = record.rule_id;
    fixture.suite = record.suite;

    const std::string_view status{record.status};
    if (status == "CHARACTERIZATION") {
      fixture.known_contract_gap = true;
      fixture.contract_target = ParseErrorCode(record.target_code);
      if (std::string_view{record.target_lock_candidate} != "-") {
        fixture.target_lock_candidate = record.target_lock_candidate;
      }
    } else if (status == "OPEN_DECISION") {
      fixture.open_decision = true;
    }

    const std::string_view pointer_assertion{record.pointer_assertion};
    if (pointer_assertion == "ABSENT") {
      fixture.require_json_pointer_absent = true;
    } else if (pointer_assertion == "ROOT") {
      fixture.verify_json_pointer = true;
    } else if (pointer_assertion == "EXACT") {
      fixture.verify_json_pointer = true;
      fixture.expected_json_pointer = DecodeCorpusHex(record.pointer_utf8_hex);
    }

    const std::string_view offset_assertion{record.offset_assertion};
    fixture.expected_byte_offset_kind = ParseByteOffsetKind(record.offset_kind);
    if (offset_assertion == "ABSENT") {
      fixture.require_byte_offset_absent = true;
    } else if (offset_assertion == "EXACT") {
      fixture.verify_byte_offset = true;
      fixture.expected_byte_offset = ParseCorpusSize(record.offset_value);
    } else if (offset_assertion == "KIND_ONLY") {
      fixture.verify_byte_offset_kind_only = true;
    }

    fixtures.push_back(std::move(fixture));
  }
}

void ValidateFixtureInventory(const std::vector<Fixture>& fixtures) {
  if (fixtures.size() != std::size(corpus_generated::kInventory)) {
    throw std::logic_error{"fixture count differs from strict_json_inventory_v0.1.tsv"};
  }

  for (const auto& expected : corpus_generated::kInventory) {
    const auto first = std::find_if(
        fixtures.cbegin(), fixtures.cend(),
        [&expected](const Fixture& fixture) { return fixture.name == expected.case_id; });
    if (first == fixtures.cend()) {
      throw std::logic_error{"fixture is missing from the executable inventory"};
    }
    const auto duplicate = std::find_if(
        std::next(first), fixtures.cend(),
        [&expected](const Fixture& fixture) { return fixture.name == expected.case_id; });
    if (duplicate != fixtures.cend()) {
      throw std::logic_error{"fixture inventory contains a duplicate case_id"};
    }
    if (first->source_state != expected.source_state) {
      throw std::logic_error{"fixture source state differs from strict_json_inventory_v0.1.tsv"};
    }
  }
}

bool ReadUnicodeEscape(std::string_view input, std::size_t slash_offset,
                       std::uint16_t& code_unit) noexcept {
  if (slash_offset > input.size() || input.size() - slash_offset < 6U ||
      input[slash_offset] != '\\' || input[slash_offset + 1U] != 'u') {
    return false;
  }

  std::uint16_t value = 0U;
  for (std::size_t index = slash_offset + 2U; index < slash_offset + 6U; ++index) {
    const int digit = HexDigitValue(input[index]);
    if (digit < 0) {
      return false;
    }
    value = static_cast<std::uint16_t>((value << 4U) | static_cast<std::uint16_t>(digit));
  }
  code_unit = value;
  return true;
}

bool ValidateUnicodeSurrogateEscapes(std::string_view input, std::size_t& invalid_offset) noexcept {
  bool in_string = false;
  std::size_t index = 0U;
  while (index < input.size()) {
    const char character = input[index];
    if (!in_string) {
      if (character == '"') {
        in_string = true;
      }
      ++index;
      continue;
    }

    if (character == '"') {
      in_string = false;
      ++index;
      continue;
    }
    if (character != '\\') {
      ++index;
      continue;
    }
    if (index + 1U >= input.size()) {
      return true;
    }
    if (input[index + 1U] != 'u') {
      index += 2U;
      continue;
    }

    std::uint16_t first_code_unit = 0U;
    if (!ReadUnicodeEscape(input, index, first_code_unit)) {
      ++index;
      continue;
    }
    if (first_code_unit >= 0xDC00U && first_code_unit <= 0xDFFFU) {
      invalid_offset = index;
      return false;
    }
    if (first_code_unit < 0xD800U || first_code_unit > 0xDBFFU) {
      index += 6U;
      continue;
    }

    const std::size_t second_escape_offset = index + 6U;
    std::uint16_t second_code_unit = 0U;
    if (!ReadUnicodeEscape(input, second_escape_offset, second_code_unit) ||
        second_code_unit < 0xDC00U || second_code_unit > 0xDFFFU) {
      invalid_offset = index;
      return false;
    }
    index += 12U;
  }

  return true;
}

bool ValidateNestingDepth(std::string_view input, std::size_t maximum_depth,
                          std::size_t& invalid_offset) noexcept {
  bool in_string = false;
  bool escaped = false;
  std::size_t depth = 0U;

  for (std::size_t index = 0U; index < input.size(); ++index) {
    const char character = input[index];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == '"') {
        in_string = false;
      }
      continue;
    }

    if (character == '"') {
      in_string = true;
    } else if (character == '{' || character == '[') {
      if (depth == std::numeric_limits<std::size_t>::max()) {
        invalid_offset = index;
        return false;
      }
      ++depth;
      if (depth > maximum_depth) {
        invalid_offset = index;
        return false;
      }
    } else if ((character == '}' || character == ']') && depth > 0U) {
      --depth;
    }
  }

  return true;
}

std::string MakeProbeWithPayload(std::string payload) {
  return std::string{"{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","} +
         "\"byte_offset\":1,\"messages\":[],\"payload\":" + std::move(payload) + "}";
}

std::string MakeNestedArray(std::size_t depth) {
  return std::string(depth, '[') + "0" + std::string(depth, ']');
}

std::string MakeArray(std::size_t element_count) {
  std::string result{"["};
  for (std::size_t index = 0U; index < element_count; ++index) {
    if (index != 0U) {
      result.push_back(',');
    }
    result.push_back('0');
  }
  result.push_back(']');
  return result;
}

std::string MakeObject(std::size_t member_count) {
  std::string result{"{"};
  for (std::size_t index = 0U; index < member_count; ++index) {
    if (index != 0U) {
      result.push_back(',');
    }
    result += "\"k" + std::to_string(index) + "\":null";
  }
  result.push_back('}');
  return result;
}

std::string MakeProtocolSkeleton(std::string_view framing, const std::vector<std::size_t>& fields) {
  std::string result{"{\"framing\":\""};
  result += framing;
  result += "\",\"messages\":[";
  for (std::size_t message_index = 0U; message_index < fields.size(); ++message_index) {
    if (message_index != 0U) {
      result.push_back(',');
    }
    result += "{\"id\":\"message_" + std::to_string(message_index) + "\",\"fields\":[";
    for (std::size_t field_index = 0U; field_index < fields[message_index]; ++field_index) {
      if (field_index != 0U) {
        result.push_back(',');
      }
      result += "{\"id\":\"field_" + std::to_string(field_index) +
                "\",\"byte_offset\":" + std::to_string(field_index) + "}";
    }
    result += "]}";
  }
  result += "]}";
  return result;
}

std::vector<Fixture> BuildFixtures(const Limits& limits) {
  const std::string valid_minimal =
      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
      "\"byte_offset\":1,\"messages\":[]}";

  std::string nonbreaking_space_whitespace;
  nonbreaking_space_whitespace.append("\xC2\xA0", 2U);
  nonbreaking_space_whitespace += valid_minimal;

  std::string raw_control_in_string = "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe";
  raw_control_in_string.push_back(static_cast<char>(0x01));
  raw_control_in_string += "x\",\"byte_offset\":1,\"messages\":[]}";

  std::string raw_nul_in_string = "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe";
  raw_nul_in_string.push_back(static_cast<char>(0x00));
  raw_nul_in_string += "x\",\"byte_offset\":1,\"messages\":[]}";

  std::string unescaped_newline_in_string = "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe";
  unescaped_newline_in_string.push_back('\n');
  unescaped_newline_in_string += "x\",\"byte_offset\":1,\"messages\":[]}";

  const std::size_t exact_node_payload_members = limits.max_nodes - 6U;
  const std::size_t over_node_payload_members = exact_node_payload_members + 1U;
  Limits field_dense_limits = limits;
  field_dense_limits.max_nodes = 20000U;
  field_dense_limits.max_array_elements = 4096U;

  std::vector<Fixture> fixtures;
  fixtures.push_back({"valid_minimal", valid_minimal, ErrorCode::kOk});
  fixtures.push_back(
      {"valid_top_level_whitespace", " \t\r\n" + valid_minimal + "\r\n ", ErrorCode::kOk});
  fixtures.push_back({"valid_escaped_solidus_and_surrogate_pair",
                      "{\"schema_version\":\"0.1\","
                      "\"protocol_id\":\"probe\\/\\uD83D\\uDE00\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kOk});
  fixtures.push_back({"valid_highest_unicode_scalar_escape",
                      "{\"schema_version\":\"0.1\","
                      "\"protocol_id\":\"probe\\uDBFF\\uDFFF\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kOk});
  fixtures.push_back({"valid_literal_backslash_u_low_surrogate",
                      "{\"schema_version\":\"0.1\","
                      "\"protocol_id\":\"probe\\\\uDC00\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kOk});
  fixtures.push_back(
      {"valid_escaped_control", MakeProbeWithPayload("\"\\u0000\""), ErrorCode::kOk});
  fixtures.push_back(
      {"valid_internal_bom_codepoint", MakeProbeWithPayload("\"\\uFEFF\""), ErrorCode::kOk});
  fixtures.push_back({"valid_unicode_distinct_keys",
                      MakeProbeWithPayload("{\"\\u00E9\":1,\"e\\u0301\":2}"), ErrorCode::kOk});
  fixtures.push_back({"valid_fraction_payload", MakeProbeWithPayload("0.5"), ErrorCode::kOk});
  fixtures.push_back({"valid_exponent_payload", MakeProbeWithPayload("1e3"), ErrorCode::kOk});
  fixtures.push_back({"valid_exponent_plus_payload", MakeProbeWithPayload("1e+3"), ErrorCode::kOk});
  fixtures.push_back({"vertical_tab_whitespace", "\v" + valid_minimal, ErrorCode::kSyntaxError});
  fixtures.push_back({"form_feed_whitespace", "\f" + valid_minimal, ErrorCode::kSyntaxError});
  fixtures.push_back({"nonbreaking_space_whitespace", std::move(nonbreaking_space_whitespace),
                      ErrorCode::kSyntaxError});
  fixtures.push_back(
      {"raw_control_in_string", std::move(raw_control_in_string), ErrorCode::kSyntaxError});
  fixtures.push_back({"raw_nul_in_string", std::move(raw_nul_in_string), ErrorCode::kSyntaxError});
  fixtures.push_back({"unescaped_newline_in_string", std::move(unescaped_newline_in_string),
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"comment",
                      "{\"schema_version\":\"0.1\",/*comment*/\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"line_comment",
                      "{\"schema_version\":\"0.1\",//comment\n\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"trailing_comma",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back(
      {"array_trailing_comma", MakeProbeWithPayload("[0,]"), ErrorCode::kSyntaxError});
  fixtures.push_back({"nan",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":NaN,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"infinity",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":Infinity,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"second_document", valid_minimal + " {}", ErrorCode::kSyntaxError});
  fixtures.push_back({"trailing_character", valid_minimal + "x", ErrorCode::kSyntaxError});
  fixtures.push_back({"lone_surrogate",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"\\uD800\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"lone_low_surrogate",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"\\uDC00\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"reversed_surrogate_pair",
                      "{\"schema_version\":\"0.1\","
                      "\"protocol_id\":\"\\uDC00\\uD800\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"invalid_escape_x",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"\\x41\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"invalid_escape_q",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"\\q\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"invalid_unicode_escape_hex",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"\\u12G4\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"single_quoted_string",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":'probe',"
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"unquoted_key",
                      "{\"schema_version\":\"0.1\",protocol_id:\"probe\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"hexadecimal_number",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":0x10,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"leading_plus_number",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":+1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"leading_zero_number",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":01,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"negative_leading_zero_number",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":-01,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"trailing_decimal_point",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1.,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"leading_decimal_point",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":.1,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"incomplete_exponent",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1e,\"messages\":[]}",
                      ErrorCode::kSyntaxError});
  fixtures.push_back({"duplicate_nested",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[{\"id\":\"a\",\"id\":\"b\"}]}",
                      ErrorCode::kDuplicateKey});
  fixtures.push_back({"duplicate_embedded_null_key",
                      MakeProbeWithPayload("{\"a\\u0000b\":1,\"a\\u0000b\":2}"),
                      ErrorCode::kDuplicateKey});
  fixtures.push_back({"reference_not_found",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"probe_known_id\":\"known\","
                      "\"probe_reference_id\":\"missing\"}",
                      ErrorCode::kReferenceNotFound});
  fixtures.push_back({"reference_found",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"probe_known_id\":\"known\","
                      "\"probe_reference_id\":\"known\"}",
                      ErrorCode::kOk});
  fixtures.push_back({"cross_field_semantic_constraint",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"probe_range_start\":10,"
                      "\"probe_range_end\":9}",
                      ErrorCode::kSemanticConstraint});
  fixtures.push_back({"cross_field_semantic_valid",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"probe_range_start\":9,"
                      "\"probe_range_end\":10}",
                      ErrorCode::kOk});
  fixtures.push_back({"unknown_root_field",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"unexpected\":true}",
                      ErrorCode::kUnknownField});
  fixtures.push_back({"unknown_pointer_escape_slash",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"unexpected/name\":true}",
                      ErrorCode::kUnknownField});
  fixtures.push_back({"unknown_pointer_escape_tilde",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[],\"unexpected~name\":true}",
                      ErrorCode::kUnknownField});
  fixtures.push_back({"missing_protocol_id",
                      "{\"schema_version\":\"0.1\",\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kMissingField});
  fixtures.push_back({"wrong_schema_version_type",
                      "{\"schema_version\":1,\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":[]}",
                      ErrorCode::kTypeMismatch});
  fixtures.push_back({"wrong_messages_type",
                      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
                      "\"byte_offset\":1,\"messages\":{}}",
                      ErrorCode::kTypeMismatch});
  fixtures.push_back({"root_array", "[]", ErrorCode::kTypeMismatch});
  fixtures.push_back({"root_string", "\"probe\"", ErrorCode::kTypeMismatch});
  fixtures.push_back({"root_number", "0", ErrorCode::kTypeMismatch});
  fixtures.push_back({"root_boolean", "true", ErrorCode::kTypeMismatch});
  fixtures.push_back({"root_null", "null", ErrorCode::kTypeMismatch});
  fixtures.push_back({"depth_exact", MakeProbeWithPayload(MakeNestedArray(limits.max_depth - 2U)),
                      ErrorCode::kOk});
  fixtures.push_back({"value_depth_over",
                      MakeProbeWithPayload(MakeNestedArray(limits.max_depth - 1U)),
                      ErrorCode::kDepthLimit});
  fixtures.push_back({"container_depth_preflight_over",
                      MakeProbeWithPayload(MakeNestedArray(limits.max_depth)),
                      ErrorCode::kDepthLimit});
  fixtures.push_back({"string_exact",
                      MakeProbeWithPayload("\"" + std::string(limits.max_string_bytes, 'x') + "\""),
                      ErrorCode::kOk});
  fixtures.push_back(
      {"string_over",
       MakeProbeWithPayload("\"" + std::string(limits.max_string_bytes + 1U, 'x') + "\""),
       ErrorCode::kStringLimit});
  fixtures.push_back({"nested_string_over",
                      MakeProbeWithPayload("[{\"a/b~c\":\"" +
                                           std::string(limits.max_string_bytes + 1U, 'x') + "\"}]"),
                      ErrorCode::kStringLimit});
  fixtures.push_back(
      {"array_exact", MakeProbeWithPayload(MakeArray(limits.max_array_elements)), ErrorCode::kOk});
  fixtures.push_back({"array_over", MakeProbeWithPayload(MakeArray(limits.max_array_elements + 1U)),
                      ErrorCode::kArrayLimit});
  fixtures.push_back(
      {"node_exact", MakeProbeWithPayload(MakeObject(exact_node_payload_members)), ErrorCode::kOk});
  fixtures.push_back({"node_over", MakeProbeWithPayload(MakeObject(over_node_payload_members)),
                      ErrorCode::kNodeLimit});
  fixtures.push_back(
      {"input_exact",
       valid_minimal + std::string(limits.max_input_bytes - valid_minimal.size(), ' '),
       ErrorCode::kOk});
  fixtures.push_back(
      {"input_over",
       valid_minimal + std::string(limits.max_input_bytes - valid_minimal.size() + 1U, ' '),
       ErrorCode::kInputLimit});
  fixtures.push_back(
      {"poc_complete_record_skeleton",
       MakeProbeWithPayload(MakeProtocolSkeleton("complete_record", {12U, 12U, 12U, 12U})),
       ErrorCode::kOk});
  fixtures.push_back(
      {"poc_fixed_frame_skeleton",
       MakeProbeWithPayload(MakeProtocolSkeleton("sync_fixed_length", {6U, 6U, 6U, 6U, 6U})),
       ErrorCode::kOk});
  fixtures.push_back(
      {"poc_stream_field_dense_skeleton",
       MakeProbeWithPayload(MakeProtocolSkeleton("sync_length_field", {24U, 1708U, 32U})),
       ErrorCode::kOk, field_dense_limits});

  const auto expect_pointer = [&fixtures](std::string_view fixture_name,
                                          std::string_view json_pointer) {
    const auto iterator = std::find_if(
        fixtures.begin(), fixtures.end(),
        [fixture_name](const Fixture& fixture) { return fixture.name == fixture_name; });
    if (iterator == fixtures.end()) {
      throw std::logic_error{"JSON Pointer fixture is missing"};
    }
    iterator->verify_json_pointer = true;
    iterator->expected_json_pointer.assign(json_pointer);
  };

  expect_pointer("reference_not_found", "/probe_reference_id");
  expect_pointer("cross_field_semantic_constraint", "/probe_range_start");
  expect_pointer("unknown_root_field", "/unexpected");
  expect_pointer("unknown_pointer_escape_slash", "/unexpected~1name");
  expect_pointer("unknown_pointer_escape_tilde", "/unexpected~0name");
  expect_pointer("missing_protocol_id", "/protocol_id");
  expect_pointer("wrong_schema_version_type", "/schema_version");
  expect_pointer("wrong_messages_type", "/messages");
  expect_pointer("root_array", "");
  expect_pointer("string_over", "/payload");
  expect_pointer("nested_string_over", "/payload/0/a~1b~0c");
  expect_pointer("array_over", "/payload");
  AppendGeneratedCorpusFixtures(fixtures);
  return fixtures;
}

std::size_t ParseIterationCount(int argc, char* argv[]) {
  std::size_t iterations = 10000U;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument != "--benchmark-iterations") {
      throw std::invalid_argument("unknown argument: " + std::string(argument));
    }
    if (index + 1 >= argc) {
      throw std::invalid_argument("--benchmark-iterations requires a value");
    }
    const std::string_view value{argv[++index]};
    if (value.empty()) {
      throw std::invalid_argument("benchmark iteration count must contain digits");
    }
    for (const char character : value) {
      if (character < '0' || character > '9') {
        throw std::invalid_argument("benchmark iteration count must contain digits only");
      }
    }
    const unsigned long long parsed = std::stoull(std::string(value));
    constexpr unsigned long long kMaxBenchmarkIterations = 10000000ULL;
    if (parsed == 0ULL || parsed > kMaxBenchmarkIterations ||
        parsed > static_cast<unsigned long long>(std::numeric_limits<std::size_t>::max())) {
      throw std::out_of_range("benchmark iteration count is out of range");
    }
    iterations = static_cast<std::size_t>(parsed);
  }
  return iterations;
}

bool RunRawNumberSelfTest(const char* candidate_name, ParseFunction parse,
                          const Limits& base_limits) {
  Limits limits = base_limits;
  limits.capture_number_lexemes = true;
  const std::string input =
      "{\"schema_version\":\"0.1\",\"protocol_id\":\"raw-number-self-test\","
      "\"byte_offset\":1,\"messages\":[],"
      "\"payload\":[-0,1.0,1e0,1E+007,18446744073709551616,0e999,1e999]}";
  constexpr std::string_view kExpectedTokens[] = {
      "1", "-0", "1.0", "1e0", "1E+007", "18446744073709551616", "0e999", "1e999",
  };
  std::string expected_lexemes;
  for (const std::string_view token : kExpectedTokens) {
    if (!expected_lexemes.empty()) {
      expected_lexemes.push_back('\0');
    }
    expected_lexemes.append(token);
  }

  const ParseResult result = parse(input, limits);
  const bool passed = result.code == ErrorCode::kOk && result.stats.number_lexemes_preserved &&
                      result.stats.number_token_count == std::size(kExpectedTokens) &&
                      result.stats.number_lexemes == expected_lexemes;
  std::cout << "RAW_NUMBER_SELF_TEST candidate=" << candidate_name
            << " expected_tokens=" << std::size(kExpectedTokens)
            << " actual_tokens=" << result.stats.number_token_count
            << " expected_bytes=" << expected_lexemes.size()
            << " actual_bytes=" << result.stats.number_lexemes.size()
            << " parser_code=" << ToString(result.code)
            << " lexemes_preserved=" << (result.stats.number_lexemes_preserved ? "true" : "false")
            << " byte_for_byte="
            << (result.stats.number_lexemes == expected_lexemes ? "true" : "false")
            << " regression_gate_passed=" << (passed ? "true" : "false") << '\n';
  return passed;
}

}  // namespace

const char* ToString(ErrorCode code) noexcept {
  switch (code) {
    case ErrorCode::kOk:
      return "OK";
    case ErrorCode::kInputLimit:
      return "INPUT_LIMIT";
    case ErrorCode::kBomNotAllowed:
      return "BOM_NOT_ALLOWED";
    case ErrorCode::kInvalidUtf8:
      return "INVALID_UTF8";
    case ErrorCode::kSyntaxError:
      return "SYNTAX_ERROR";
    case ErrorCode::kDuplicateKey:
      return "DUPLICATE_KEY";
    case ErrorCode::kDepthLimit:
      return "DEPTH_LIMIT";
    case ErrorCode::kNodeLimit:
      return "NODE_LIMIT";
    case ErrorCode::kObjectLimit:
      return "OBJECT_LIMIT";
    case ErrorCode::kStringLimit:
      return "STRING_LIMIT";
    case ErrorCode::kArrayLimit:
      return "ARRAY_LIMIT";
    case ErrorCode::kNumberTokenLimit:
      return "NUMBER_TOKEN_LIMIT";
    case ErrorCode::kTotalStringLimit:
      return "TOTAL_STRING_LIMIT";
    case ErrorCode::kIntegerNotExact:
      return "INTEGER_NOT_EXACT";
    case ErrorCode::kIntegerOutOfRange:
      return "INTEGER_OUT_OF_RANGE";
    case ErrorCode::kRealOutOfRange:
      return "REAL_OUT_OF_RANGE";
    case ErrorCode::kUnknownField:
      return "UNKNOWN_FIELD";
    case ErrorCode::kMissingField:
      return "MISSING_FIELD";
    case ErrorCode::kTypeMismatch:
      return "TYPE_MISMATCH";
    case ErrorCode::kReferenceNotFound:
      return "REFERENCE_NOT_FOUND";
    case ErrorCode::kSemanticConstraint:
      return "SEMANTIC_CONSTRAINT";
    case ErrorCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case ErrorCode::kInternalError:
      return "INTERNAL_ERROR";
  }
  return "INTERNAL_ERROR";
}

const char* ToString(ByteOffsetKind kind) noexcept {
  switch (kind) {
    case ByteOffsetKind::kNone:
      return "NONE";
    case ByteOffsetKind::kExactInputByte:
      return "EXACT_INPUT_BYTE";
    case ByteOffsetKind::kParserReportedPosition:
      return "PARSER_REPORTED_POSITION";
  }
  return "NONE";
}

ParseResult Accepted(DocumentStats stats) {
  ParseResult result;
  result.code = ErrorCode::kOk;
  result.byte_offset = kNoByteOffset;
  result.byte_offset_kind = ByteOffsetKind::kNone;
  result.stats = stats;
  return result;
}

ParseResult Rejected(ErrorCode code, std::size_t byte_offset, ByteOffsetKind byte_offset_kind,
                     ErrorReason reason) noexcept {
  ParseResult result;
  result.code = code;
  result.reason = reason;
  result.byte_offset = byte_offset;
  result.byte_offset_kind = byte_offset == kNoByteOffset ? ByteOffsetKind::kNone : byte_offset_kind;
  return result;
}

ParseResult RejectedAtPath(ErrorCode code, std::string_view json_pointer, ErrorReason reason) {
  ParseResult result =
      Rejected(code, kNoByteOffset, ByteOffsetKind::kParserReportedPosition, reason);
  result.has_json_pointer = true;
  result.json_pointer.assign(json_pointer);
  return result;
}

std::size_t AppendJsonPointerToken(std::string& json_pointer, std::string_view token) {
  const std::size_t previous_size = json_pointer.size();
  json_pointer.push_back('/');
  for (const char character : token) {
    if (character == '~') {
      json_pointer += "~0";
    } else if (character == '/') {
      json_pointer += "~1";
    } else {
      json_pointer.push_back(character);
    }
  }
  return previous_size;
}

std::size_t AppendJsonPointerIndex(std::string& json_pointer, std::size_t index) {
  return AppendJsonPointerToken(json_pointer, std::to_string(index));
}

void RestoreJsonPointer(std::string& json_pointer, std::size_t previous_size) noexcept {
  json_pointer.resize(previous_size);
}

bool RunCommonPreflight(std::string_view input, const Limits& limits,
                        ParseResult& rejection) noexcept {
  if (input.size() > limits.max_input_bytes) {
    rejection = Rejected(ErrorCode::kInputLimit);
    return false;
  }

  if (input.size() >= 3U && static_cast<unsigned char>(input[0]) == 0xEFU &&
      static_cast<unsigned char>(input[1]) == 0xBBU &&
      static_cast<unsigned char>(input[2]) == 0xBFU) {
    rejection = Rejected(ErrorCode::kBomNotAllowed, 0U, ByteOffsetKind::kExactInputByte);
    return false;
  }

  std::size_t invalid_offset = 0U;
  if (!ValidateUtf8(input, invalid_offset)) {
    rejection = Rejected(ErrorCode::kInvalidUtf8, invalid_offset, ByteOffsetKind::kExactInputByte);
    return false;
  }

  if (!ValidateUnicodeSurrogateEscapes(input, invalid_offset)) {
    rejection = Rejected(ErrorCode::kSyntaxError, invalid_offset, ByteOffsetKind::kExactInputByte);
    return false;
  }

  if (!ValidateNestingDepth(input, limits.max_depth, invalid_offset)) {
    rejection = Rejected(ErrorCode::kDepthLimit, invalid_offset, ByteOffsetKind::kExactInputByte);
    return false;
  }

  return true;
}

int RunCandidate(const char* candidate_name, const char* candidate_version, ParseFunction parse,
                 int argc, char* argv[]) {
  try {
    const Limits limits;
    const std::size_t iterations = ParseIterationCount(argc, argv);
    const std::vector<Fixture> fixtures = BuildFixtures(limits);
    ValidateFixtureInventory(fixtures);

    std::size_t regression_failures = 0U;
    std::size_t conformance_cases = 0U;
    std::size_t conformance_failures = 0U;
    std::size_t characterization_cases = 0U;
    std::size_t characterization_changes = 0U;
    std::size_t ready_to_promote = 0U;
    std::size_t open_contract_gaps = 0U;
    std::size_t target_locked_cases = 0U;
    std::size_t target_lock_failures = 0U;
    std::size_t open_decision_cases = 0U;
    std::size_t open_decision_changes = 0U;
    std::size_t raw_number_self_test_cases = 0U;
    std::size_t raw_number_self_test_failures = 0U;
    if (std::string_view{candidate_name} == "yyjson") {
      ++raw_number_self_test_cases;
      if (!RunRawNumberSelfTest(candidate_name, parse, limits)) {
        ++raw_number_self_test_failures;
        ++regression_failures;
      }
    }
    for (const Fixture& fixture : fixtures) {
      const ParseResult result = parse(fixture.input, fixture.limits);
      const bool target_locked =
          fixture.known_contract_gap && fixture.target_lock_candidate == candidate_name;
      const bool code_matches =
          target_locked ? result.code == fixture.contract_target
                        : (result.code == fixture.expected ||
                           (fixture.known_contract_gap && result.code == fixture.contract_target));
      const bool pointer_matches =
          (!fixture.verify_json_pointer ||
           (result.has_json_pointer && result.json_pointer == fixture.expected_json_pointer)) &&
          (!fixture.require_json_pointer_absent || !result.has_json_pointer);
      const bool offset_matches =
          (!fixture.verify_byte_offset ||
           (result.byte_offset == fixture.expected_byte_offset &&
            result.byte_offset_kind == fixture.expected_byte_offset_kind)) &&
          (!fixture.require_byte_offset_absent ||
           (result.byte_offset == kNoByteOffset &&
            result.byte_offset_kind == ByteOffsetKind::kNone)) &&
          (!fixture.verify_byte_offset_kind_only ||
           (result.byte_offset != kNoByteOffset &&
            result.byte_offset_kind == fixture.expected_byte_offset_kind));
      const bool regression_gate_passed = code_matches && pointer_matches && offset_matches;
      if (!regression_gate_passed) {
        ++regression_failures;
      }
      if (fixture.known_contract_gap) {
        ++characterization_cases;
        if (target_locked) {
          ++target_locked_cases;
          if (!regression_gate_passed) {
            ++target_lock_failures;
          }
        }
        if (result.code == fixture.contract_target && pointer_matches && offset_matches) {
          ++ready_to_promote;
        } else {
          ++open_contract_gaps;
        }
        if (!regression_gate_passed) {
          ++characterization_changes;
        }
      } else if (fixture.open_decision) {
        ++open_decision_cases;
        if (!regression_gate_passed) {
          ++open_decision_changes;
        }
      } else {
        ++conformance_cases;
        if (!regression_gate_passed) {
          ++conformance_failures;
        }
      }

      const char* outcome = "UNEXPECTED_CHANGE";
      if (regression_gate_passed) {
        if (fixture.known_contract_gap) {
          outcome = result.code == fixture.contract_target ? "TARGET_REACHED" : "OBSERVED_STABLE";
        } else if (fixture.open_decision) {
          outcome = "OPEN_STABLE";
        } else {
          outcome = "CONFORMANT";
        }
      }

      std::cout << "CASE candidate=" << candidate_name << " version=" << candidate_version
                << " name=" << fixture.name << " source=" << fixture.source
                << " source_state=" << fixture.source_state << " rule=" << fixture.rule_id
                << " suite=" << fixture.suite << " status="
                << (fixture.known_contract_gap
                        ? "CHARACTERIZATION"
                        : (fixture.open_decision ? "OPEN_DECISION" : "CONFORMANCE"))
                << " expected=" << ToString(fixture.expected) << " actual=" << ToString(result.code)
                << " reason=" << ToString(result.reason) << " offset=";
      if (result.byte_offset == kNoByteOffset) {
        std::cout << "NA";
      } else {
        std::cout << result.byte_offset;
      }
      std::cout << " offset_kind=" << ToString(result.byte_offset_kind) << " pointer=";
      if (!result.has_json_pointer) {
        std::cout << "NA";
      } else if (result.json_pointer.empty()) {
        std::cout << "<root>";
      } else {
        std::cout << result.json_pointer;
      }
      std::cout << " expected_pointer=";
      if (!fixture.verify_json_pointer) {
        std::cout << "NA";
      } else if (fixture.expected_json_pointer.empty()) {
        std::cout << "<root>";
      } else {
        std::cout << fixture.expected_json_pointer;
      }
      std::cout << " expected_offset=";
      if (fixture.require_byte_offset_absent) {
        std::cout << "ABSENT";
      } else if (fixture.verify_byte_offset) {
        std::cout << fixture.expected_byte_offset;
      } else {
        std::cout << "IGNORE";
      }
      std::cout << " expected_offset_kind=";
      if (fixture.require_byte_offset_absent) {
        std::cout << "NONE";
      } else if (fixture.verify_byte_offset || fixture.verify_byte_offset_kind_only) {
        std::cout << ToString(fixture.expected_byte_offset_kind);
      } else {
        std::cout << "IGNORE";
      }
      std::cout << " contract_target=";
      if (fixture.known_contract_gap) {
        std::cout << ToString(fixture.contract_target);
      } else {
        std::cout << "NA";
      }
      std::cout << " target_lock_candidate="
                << (fixture.target_lock_candidate.empty() ? "NA" : fixture.target_lock_candidate)
                << " target_locked=" << (target_locked ? "true" : "false");
      std::cout << " parser_hard_limit="
                << (result.parser_memory.hard_limit_enforced ? "true" : "false")
                << " parser_limit=" << result.parser_memory.limit_bytes
                << " parser_reserved=" << result.parser_memory.reserved_bytes
                << " parser_upper_bound=" << result.parser_memory.required_upper_bound_bytes
                << " parser_attempts=" << result.parser_memory.allocation_attempts
                << " parser_reallocs=" << result.parser_memory.realloc_calls
                << " parser_peak_requested=" << result.parser_memory.peak_live_requested_bytes
                << " parser_live_blocks=" << result.parser_memory.live_blocks
                << " parser_contract_ok="
                << (result.parser_memory.allocator_contract_ok ? "true" : "false")
                << " nodes=" << result.stats.node_count << " depth=" << result.stats.max_depth
                << " max_string=" << result.stats.max_string_bytes
                << " max_array=" << result.stats.max_array_elements
                << " number_tokens=" << result.stats.number_token_count
                << " number_lexemes_preserved="
                << (result.stats.number_lexemes_preserved ? "true" : "false")
                << " outcome=" << outcome
                << " regression_gate_passed=" << (regression_gate_passed ? "true" : "false")
                << '\n';
    }

    const std::string small_benchmark_input =
        "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
        "\"byte_offset\":1,\"messages\":[]}";
    const auto dense_fixture = std::find_if(
        fixtures.cbegin(), fixtures.cend(),
        [](const Fixture& fixture) { return fixture.name == "poc_stream_field_dense_skeleton"; });
    if (dense_fixture == fixtures.cend()) {
      std::cerr << "dense benchmark fixture is missing\n";
      return 2;
    }

    const auto run_benchmark = [&](std::string_view profile, std::string_view input,
                                   const Limits& benchmark_limits,
                                   std::size_t benchmark_iterations) -> bool {
      std::uint64_t checksum = 0U;
      const auto start = std::chrono::steady_clock::now();
      for (std::size_t index = 0U; index < benchmark_iterations; ++index) {
        const ParseResult result = parse(input, benchmark_limits);
        if (result.code != ErrorCode::kOk) {
          std::cerr << "benchmark parse failed: " << ToString(result.code) << '\n';
          return false;
        }
        checksum += static_cast<std::uint64_t>(result.stats.node_count);
      }
      const auto finish = std::chrono::steady_clock::now();
      const auto total_nanoseconds =
          std::chrono::duration_cast<std::chrono::nanoseconds>(finish - start).count();
      const double nanoseconds_per_parse =
          static_cast<double>(total_nanoseconds) / static_cast<double>(benchmark_iterations);
      const double total_mebibytes = static_cast<double>(input.size()) *
                                     static_cast<double>(benchmark_iterations) / (1024.0 * 1024.0);
      const double seconds = static_cast<double>(total_nanoseconds) / 1'000'000'000.0;
      const double mebibytes_per_second = seconds > 0.0 ? total_mebibytes / seconds : 0.0;

      std::cout << std::fixed << std::setprecision(2) << "BENCHMARK candidate=" << candidate_name
                << " version=" << candidate_version << " profile=" << profile
                << " iterations=" << benchmark_iterations << " input_bytes=" << input.size()
                << " total_ns=" << total_nanoseconds << " ns_per_parse=" << nanoseconds_per_parse
                << " mib_per_second=" << mebibytes_per_second << " checksum=" << checksum << '\n';
      return true;
    };

    if (!run_benchmark("small", small_benchmark_input, limits, iterations)) {
      return 2;
    }
    const std::size_t dense_iterations =
        (std::max)(std::size_t{10U}, iterations / std::size_t{5000U});
    if (!run_benchmark("field_dense", dense_fixture->input, dense_fixture->limits,
                       dense_iterations)) {
      return 2;
    }
    const bool contract_closure_ready = regression_failures == 0U && conformance_failures == 0U &&
                                        characterization_cases == 0U && open_decision_cases == 0U;
    std::cout << "SUMMARY candidate=" << candidate_name << " cases=" << fixtures.size()
              << " inventory_cases=" << std::size(corpus_generated::kInventory)
              << " inventory_gate=PASS regression_failures=" << regression_failures
              << " regression_gate=" << (regression_failures == 0U ? "PASS" : "FAIL")
              << " contract_closure_state=" << (contract_closure_ready ? "CLOSED" : "OPEN")
              << " conformance_cases=" << conformance_cases
              << " conformance_failures=" << conformance_failures
              << " characterization_cases=" << characterization_cases
              << " characterization_changes=" << characterization_changes
              << " ready_to_promote=" << ready_to_promote
              << " open_contract_gaps=" << open_contract_gaps
              << " target_locked_cases=" << target_locked_cases
              << " target_lock_failures=" << target_lock_failures
              << " raw_number_self_test_cases=" << raw_number_self_test_cases
              << " raw_number_self_test_failures=" << raw_number_self_test_failures
              << " open_decision_cases=" << open_decision_cases
              << " open_decision_changes=" << open_decision_changes << '\n';
    return regression_failures == 0U ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << "spike runner failed: " << error.what() << '\n';
    return 3;
  }
}

}  // namespace pae::json_spike
