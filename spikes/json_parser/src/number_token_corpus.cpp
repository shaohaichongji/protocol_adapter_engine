#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>

#include "number_token_corpus_generated.h"
#include "spike_common.h"

namespace pae::json_spike {
namespace {

int HexDigit(char character) noexcept {
  if (character >= '0' && character <= '9') {
    return character - '0';
  }
  if (character >= 'A' && character <= 'F') {
    return character - 'A' + 10;
  }
  return -1;
}

std::string DecodeHex(std::string_view hex) {
  if (hex == "-") {
    return {};
  }
  std::string decoded;
  decoded.reserve(hex.size() / 2U);
  for (std::size_t index = 0U; index < hex.size(); index += 2U) {
    const int high = HexDigit(hex[index]);
    const int low = HexDigit(hex[index + 1U]);
    decoded.push_back(static_cast<char>((high << 4) | low));
  }
  return decoded;
}

template <typename Integer>
std::string FormatInteger(Integer value) {
  std::array<char, 32U> buffer{};
  const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
  if (result.ec != std::errc{}) {
    return "<format-error>";
  }
  return std::string(buffer.data(), result.ptr);
}

std::string FormatDoubleBits(double value) {
  static constexpr char kHexDigits[] = "0123456789ABCDEF";
  static_assert(sizeof(double) == sizeof(std::uint64_t),
                "REAL64 bit assertions require an 8-byte double");
  std::uint64_t bits = 0U;
  std::memcpy(&bits, &value, sizeof(bits));
  std::string result(16U, '0');
  for (std::size_t index = 0U; index < result.size(); ++index) {
    const std::size_t shift = (result.size() - index - 1U) * 4U;
    const std::size_t digit = static_cast<std::size_t>((bits >> shift) & 0x0FU);
    result[index] = kHexDigits[digit];
  }
  return result;
}

}  // namespace

int RunNumberTokenCorpus() {
  std::size_t conformance_cases = 0U;
  std::size_t open_decision_cases = 0U;
  std::size_t regression_failures = 0U;

  for (const auto& record : number_corpus_generated::kRecords) {
    const std::string token = DecodeHex(record.token_hex);
    const NumberTokenInfo info = ClassifyJsonNumberToken(token);
    std::string_view actual_status = "INVALID_TOKEN";
    ErrorReason actual_reason = ErrorReason::kInvalidToken;
    std::string actual_value = "-";
    std::string actual_bits_hex = "-";

    if (std::string_view{record.target_type} == "UINT64") {
      std::uint64_t value = 0U;
      const ExactIntegerStatus actual = ParseJsonUint64Token(token, value, actual_reason);
      actual_status = ToString(actual);
      if (actual == ExactIntegerStatus::kOk) {
        actual_value = FormatInteger(value);
      }
    } else if (std::string_view{record.target_type} == "INT64") {
      std::int64_t value = 0;
      const ExactIntegerStatus actual = ParseJsonInt64Token(token, value, actual_reason);
      actual_status = ToString(actual);
      if (actual == ExactIntegerStatus::kOk) {
        actual_value = FormatInteger(value);
      }
    } else {
      double value = 0.0;
      const RealTokenStatus actual = ParseJsonReal64Token(token, value, actual_reason);
      actual_status = ToString(actual);
      if (actual == RealTokenStatus::kOk) {
        actual_bits_hex = FormatDoubleBits(value);
      }
    }

    const bool kind_matches = std::string_view{ToString(info.kind)} == record.expected_kind;
    const bool negative_zero_matches =
        info.negative_zero == (std::string_view{record.expected_negative_zero} == "true");
    const bool expected_token_is_valid = std::string_view{record.expected_kind} != "INVALID";
    const bool expected_negative =
        expected_token_is_valid && !token.empty() && token.front() == '-';
    const bool expected_has_fraction =
        expected_token_is_valid && token.find('.') != std::string::npos;
    const bool expected_has_exponent =
        expected_token_is_valid &&
        (token.find('e') != std::string::npos || token.find('E') != std::string::npos);
    const bool metadata_matches = info.negative == expected_negative &&
                                  info.has_fraction == expected_has_fraction &&
                                  info.has_exponent == expected_has_exponent;
    const bool status_matches = actual_status == record.expected_status;
    const bool reason_matches = std::string_view{ToString(actual_reason)} == record.expected_reason;
    const bool value_matches = actual_value == record.expected_value;
    const bool bits_match = actual_bits_hex == record.expected_bits_hex;
    const bool regression_gate_passed = kind_matches && negative_zero_matches && metadata_matches &&
                                        status_matches && reason_matches && value_matches &&
                                        bits_match;
    if (!regression_gate_passed) {
      ++regression_failures;
    }
    if (std::string_view{record.status} == "OPEN_DECISION") {
      ++open_decision_cases;
    } else {
      ++conformance_cases;
    }

    const char* outcome = "UNEXPECTED_CHANGE";
    if (regression_gate_passed) {
      outcome = std::string_view{record.status} == "OPEN_DECISION" ? "OPEN_STABLE" : "CONFORMANT";
    }

    std::cout << "NUMBER_TOKEN_CASE name=" << record.case_id << " target=" << record.target_type
              << " status=" << record.status << " expected=" << record.expected_status
              << " actual=" << actual_status << " expected_reason=" << record.expected_reason
              << " actual_reason=" << ToString(actual_reason)
              << " expected_kind=" << record.expected_kind << " actual_kind=" << ToString(info.kind)
              << " expected_negative_zero=" << record.expected_negative_zero
              << " actual_negative_zero=" << (info.negative_zero ? "true" : "false")
              << " expected_negative=" << (expected_negative ? "true" : "false")
              << " actual_negative=" << (info.negative ? "true" : "false")
              << " expected_fraction=" << (expected_has_fraction ? "true" : "false")
              << " actual_fraction=" << (info.has_fraction ? "true" : "false")
              << " expected_exponent=" << (expected_has_exponent ? "true" : "false")
              << " actual_exponent=" << (info.has_exponent ? "true" : "false")
              << " expected_value=" << record.expected_value << " actual_value=" << actual_value
              << " expected_bits_hex=" << record.expected_bits_hex
              << " actual_bits_hex=" << actual_bits_hex << " outcome=" << outcome
              << " regression_gate_passed=" << (regression_gate_passed ? "true" : "false") << '\n';
  }

  const bool regression_gate_passed = regression_failures == 0U;
  const bool closure_gate_passed = regression_gate_passed && open_decision_cases == 0U;
  std::cout << "NUMBER_TOKEN_SUMMARY cases=" << std::size(number_corpus_generated::kRecords)
            << " conformance_cases=" << conformance_cases
            << " open_decision_cases=" << open_decision_cases
            << " regression_failures=" << regression_failures
            << " regression_gate=" << (regression_gate_passed ? "PASS" : "FAIL")
            << " closure_gate=" << (closure_gate_passed ? "PASS" : "FAIL")
            << " contract_closure_state=" << (closure_gate_passed ? "CLOSED" : "OPEN") << '\n';
  return closure_gate_passed ? 0 : 1;
}

}  // namespace pae::json_spike

int main() { return pae::json_spike::RunNumberTokenCorpus(); }
