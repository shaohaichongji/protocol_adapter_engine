#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>
#include <system_error>

#include "spike_common.h"

namespace pae::json_spike {
namespace {

struct NumberLexeme {
  bool negative = false;
  bool real = false;
  bool has_fraction = false;
  bool has_exponent = false;
  bool significand_is_zero = true;
  std::size_t integer_begin = 0U;
  std::size_t integer_end = 0U;
};

constexpr std::int64_t kDecimalOrderLimit = (std::numeric_limits<std::int64_t>::max)() / 4;

static_assert(sizeof(double) == sizeof(std::uint64_t), "REAL64 requires an 8-byte double");
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53 &&
                  std::numeric_limits<double>::max_exponent == 1024,
              "REAL64 requires IEEE 754 binary64 semantics");

bool IsDigit(char character) noexcept { return character >= '0' && character <= '9'; }

bool ParseNumberLexeme(std::string_view token, NumberLexeme& lexeme) noexcept {
  if (token.empty()) {
    return false;
  }

  std::size_t cursor = 0U;
  lexeme.negative = token[cursor] == '-';
  if (lexeme.negative) {
    ++cursor;
    if (cursor == token.size()) {
      return false;
    }
  }

  lexeme.integer_begin = cursor;
  if (token[cursor] == '0') {
    ++cursor;
    if (cursor < token.size() && IsDigit(token[cursor])) {
      return false;
    }
  } else if (token[cursor] >= '1' && token[cursor] <= '9') {
    lexeme.significand_is_zero = false;
    do {
      ++cursor;
    } while (cursor < token.size() && IsDigit(token[cursor]));
  } else {
    return false;
  }
  lexeme.integer_end = cursor;

  lexeme.real = false;
  lexeme.has_fraction = false;
  lexeme.has_exponent = false;
  if (cursor < token.size() && token[cursor] == '.') {
    lexeme.real = true;
    lexeme.has_fraction = true;
    ++cursor;
    const std::size_t fraction_begin = cursor;
    while (cursor < token.size() && IsDigit(token[cursor])) {
      if (token[cursor] != '0') {
        lexeme.significand_is_zero = false;
      }
      ++cursor;
    }
    if (cursor == fraction_begin) {
      return false;
    }
  }

  if (cursor < token.size() && (token[cursor] == 'e' || token[cursor] == 'E')) {
    lexeme.real = true;
    lexeme.has_exponent = true;
    ++cursor;
    if (cursor < token.size() && (token[cursor] == '+' || token[cursor] == '-')) {
      ++cursor;
    }
    const std::size_t exponent_begin = cursor;
    while (cursor < token.size() && IsDigit(token[cursor])) {
      ++cursor;
    }
    if (cursor == exponent_begin) {
      return false;
    }
  }

  return cursor == token.size();
}

bool ParseMagnitude(std::string_view token, const NumberLexeme& lexeme, std::uint64_t maximum,
                    std::uint64_t& magnitude) noexcept {
  std::uint64_t parsed = 0U;
  for (std::size_t cursor = lexeme.integer_begin; cursor < lexeme.integer_end; ++cursor) {
    const std::uint64_t digit = static_cast<std::uint64_t>(token[cursor] - '0');
    if (parsed > (maximum - digit) / 10U) {
      return false;
    }
    parsed = parsed * 10U + digit;
  }
  magnitude = parsed;
  return true;
}

std::int64_t ClampDecimalMagnitude(std::uint64_t magnitude) noexcept {
  return magnitude > static_cast<std::uint64_t>(kDecimalOrderLimit)
             ? kDecimalOrderLimit
             : static_cast<std::int64_t>(magnitude);
}

std::int64_t SaturatingAddDecimalOrder(std::int64_t left, std::int64_t right) noexcept {
  if (right > 0 && left > kDecimalOrderLimit - right) {
    return kDecimalOrderLimit;
  }
  if (right < 0 && left < -kDecimalOrderLimit - right) {
    return -kDecimalOrderLimit;
  }
  return left + right;
}

std::int64_t ParseExplicitDecimalExponent(std::string_view token,
                                          const NumberLexeme& lexeme) noexcept {
  if (!lexeme.has_exponent) {
    return 0;
  }

  std::size_t cursor = token.find_first_of("eE", lexeme.integer_end);
  ++cursor;
  const bool negative = cursor < token.size() && token[cursor] == '-';
  if (cursor < token.size() && (token[cursor] == '+' || token[cursor] == '-')) {
    ++cursor;
  }

  std::uint64_t magnitude = 0U;
  const std::uint64_t magnitude_limit = static_cast<std::uint64_t>(kDecimalOrderLimit);
  while (cursor < token.size()) {
    const std::uint64_t digit = static_cast<std::uint64_t>(token[cursor] - '0');
    if (magnitude > (magnitude_limit - digit) / 10U) {
      magnitude = magnitude_limit;
    } else {
      magnitude = magnitude * 10U + digit;
    }
    ++cursor;
  }
  const std::int64_t clamped = ClampDecimalMagnitude(magnitude);
  return negative ? -clamped : clamped;
}

std::int64_t DecimalScientificOrder(std::string_view token, const NumberLexeme& lexeme) noexcept {
  std::size_t significand_cursor = lexeme.negative ? 1U : 0U;
  std::size_t digit_index = 0U;
  std::size_t digits_before_decimal = 0U;
  std::size_t first_nonzero_digit = 0U;
  bool found_nonzero = false;
  bool before_decimal = true;
  while (significand_cursor < token.size() && token[significand_cursor] != 'e' &&
         token[significand_cursor] != 'E') {
    const char character = token[significand_cursor];
    if (character == '.') {
      before_decimal = false;
    } else {
      if (before_decimal) {
        ++digits_before_decimal;
      }
      if (!found_nonzero && character != '0') {
        first_nonzero_digit = digit_index;
        found_nonzero = true;
      }
      ++digit_index;
    }
    ++significand_cursor;
  }

  std::int64_t significand_order = 0;
  const std::size_t digits_through_first_nonzero = first_nonzero_digit + 1U;
  if (digits_before_decimal >= digits_through_first_nonzero) {
    const std::size_t positive_order = digits_before_decimal - digits_through_first_nonzero;
    significand_order = positive_order > static_cast<std::size_t>(kDecimalOrderLimit)
                            ? kDecimalOrderLimit
                            : static_cast<std::int64_t>(positive_order);
  } else {
    const std::size_t negative_order = digits_through_first_nonzero - digits_before_decimal;
    significand_order = negative_order > static_cast<std::size_t>(kDecimalOrderLimit)
                            ? -kDecimalOrderLimit
                            : -static_cast<std::int64_t>(negative_order);
  }
  return SaturatingAddDecimalOrder(significand_order, ParseExplicitDecimalExponent(token, lexeme));
}

}  // namespace

const char* ToString(ExactIntegerStatus status) noexcept {
  switch (status) {
    case ExactIntegerStatus::kOk:
      return "OK";
    case ExactIntegerStatus::kNotInteger:
      return "NOT_INTEGER";
    case ExactIntegerStatus::kOutOfRange:
      return "OUT_OF_RANGE";
    case ExactIntegerStatus::kInvalidToken:
      return "INVALID_TOKEN";
  }
  return "INVALID_TOKEN";
}

const char* ToString(ErrorReason reason) noexcept {
  switch (reason) {
    case ErrorReason::kNone:
      return "NONE";
    case ErrorReason::kNegativeTokenForUnsigned:
      return "NEGATIVE_TOKEN_FOR_UNSIGNED";
    case ErrorReason::kNotInteger:
      return "NOT_INTEGER";
    case ErrorReason::kIntegerOutOfRange:
      return "INTEGER_OUT_OF_RANGE";
    case ErrorReason::kInvalidToken:
      return "INVALID_TOKEN";
    case ErrorReason::kRealOverflow:
      return "OVERFLOW";
    case ErrorReason::kRealUnderflow:
      return "UNDERFLOW";
  }
  return "INVALID_TOKEN";
}

const char* ToString(RealTokenStatus status) noexcept {
  switch (status) {
    case RealTokenStatus::kOk:
      return "OK";
    case RealTokenStatus::kOverflow:
      return "OVERFLOW";
    case RealTokenStatus::kUnderflow:
      return "UNDERFLOW";
    case RealTokenStatus::kInvalidToken:
      return "INVALID_TOKEN";
  }
  return "INVALID_TOKEN";
}

const char* ToString(NumberTokenKind kind) noexcept {
  switch (kind) {
    case NumberTokenKind::kUnsignedInteger:
      return "UNSIGNED_INTEGER";
    case NumberTokenKind::kSignedInteger:
      return "SIGNED_INTEGER";
    case NumberTokenKind::kReal:
      return "REAL";
    case NumberTokenKind::kInvalid:
      return "INVALID";
  }
  return "INVALID";
}

NumberTokenInfo ClassifyJsonNumberToken(std::string_view token) noexcept {
  NumberLexeme lexeme;
  if (!ParseNumberLexeme(token, lexeme)) {
    return {};
  }

  NumberTokenInfo result;
  result.kind = lexeme.real ? NumberTokenKind::kReal
                            : (lexeme.negative ? NumberTokenKind::kSignedInteger
                                               : NumberTokenKind::kUnsignedInteger);
  result.negative = lexeme.negative;
  result.negative_zero = lexeme.negative && lexeme.significand_is_zero;
  result.has_fraction = lexeme.has_fraction;
  result.has_exponent = lexeme.has_exponent;
  return result;
}

ExactIntegerStatus ParseJsonUint64Token(std::string_view token, std::uint64_t& value) noexcept {
  ErrorReason reason = ErrorReason::kNone;
  return ParseJsonUint64Token(token, value, reason);
}

ExactIntegerStatus ParseJsonUint64Token(std::string_view token, std::uint64_t& value,
                                        ErrorReason& reason) noexcept {
  reason = ErrorReason::kNone;
  NumberLexeme lexeme;
  if (!ParseNumberLexeme(token, lexeme)) {
    reason = ErrorReason::kInvalidToken;
    return ExactIntegerStatus::kInvalidToken;
  }
  if (lexeme.real) {
    reason = ErrorReason::kNotInteger;
    return ExactIntegerStatus::kNotInteger;
  }
  if (lexeme.negative) {
    reason = ErrorReason::kNegativeTokenForUnsigned;
    return ExactIntegerStatus::kOutOfRange;
  }

  std::uint64_t magnitude = 0U;
  if (!ParseMagnitude(token, lexeme, (std::numeric_limits<std::uint64_t>::max)(), magnitude)) {
    reason = ErrorReason::kIntegerOutOfRange;
    return ExactIntegerStatus::kOutOfRange;
  }

  value = magnitude;
  return ExactIntegerStatus::kOk;
}

ExactIntegerStatus ParseJsonInt64Token(std::string_view token, std::int64_t& value) noexcept {
  ErrorReason reason = ErrorReason::kNone;
  return ParseJsonInt64Token(token, value, reason);
}

ExactIntegerStatus ParseJsonInt64Token(std::string_view token, std::int64_t& value,
                                       ErrorReason& reason) noexcept {
  reason = ErrorReason::kNone;
  NumberLexeme lexeme;
  if (!ParseNumberLexeme(token, lexeme)) {
    reason = ErrorReason::kInvalidToken;
    return ExactIntegerStatus::kInvalidToken;
  }
  if (lexeme.real) {
    reason = ErrorReason::kNotInteger;
    return ExactIntegerStatus::kNotInteger;
  }

  const std::uint64_t signed_max =
      static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
  const std::uint64_t magnitude_limit = lexeme.negative ? signed_max + 1U : signed_max;
  std::uint64_t magnitude = 0U;
  if (!ParseMagnitude(token, lexeme, magnitude_limit, magnitude)) {
    reason = ErrorReason::kIntegerOutOfRange;
    return ExactIntegerStatus::kOutOfRange;
  }

  if (lexeme.negative && magnitude == signed_max + 1U) {
    value = (std::numeric_limits<std::int64_t>::min)();
  } else if (lexeme.negative) {
    value = -static_cast<std::int64_t>(magnitude);
  } else {
    value = static_cast<std::int64_t>(magnitude);
  }
  return ExactIntegerStatus::kOk;
}

RealTokenStatus ParseJsonReal64Token(std::string_view token, double& value,
                                     ErrorReason& reason) noexcept {
  reason = ErrorReason::kNone;
  NumberLexeme lexeme;
  if (!ParseNumberLexeme(token, lexeme)) {
    reason = ErrorReason::kInvalidToken;
    return RealTokenStatus::kInvalidToken;
  }

  if (lexeme.significand_is_zero) {
    value = 0.0;
    return RealTokenStatus::kOk;
  }

  double parsed = 0.0;
  const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed,
                                      std::chars_format::general);
  if (result.ec == std::errc::result_out_of_range) {
    if (DecimalScientificOrder(token, lexeme) >= 0) {
      reason = ErrorReason::kRealOverflow;
      return RealTokenStatus::kOverflow;
    }
    reason = ErrorReason::kRealUnderflow;
    return RealTokenStatus::kUnderflow;
  }
  if (result.ec != std::errc{} || result.ptr != token.data() + token.size()) {
    reason = ErrorReason::kInvalidToken;
    return RealTokenStatus::kInvalidToken;
  }
  if (!std::isfinite(parsed)) {
    reason = ErrorReason::kRealOverflow;
    return RealTokenStatus::kOverflow;
  }
  if (parsed == 0.0) {
    reason = ErrorReason::kRealUnderflow;
    return RealTokenStatus::kUnderflow;
  }

  value = parsed;
  return RealTokenStatus::kOk;
}

}  // namespace pae::json_spike
