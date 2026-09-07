#include "decimal_conversion_internal.h"

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)

#include <algorithm>
#include <limits>

namespace pae::protocol_plan::detail {
namespace {

constexpr std::uint64_t kMaximumAuthorDenominator = 1000000000000000000ULL;

std::uint64_t Magnitude(std::int64_t value) noexcept {
  const std::uint64_t bits = static_cast<std::uint64_t>(value);
  return value < 0 ? std::uint64_t{0U} - bits : bits;
}

std::uint64_t GreatestCommonDivisor(std::uint64_t left, std::uint64_t right) noexcept {
  while (right != 0U) {
    const std::uint64_t remainder = left % right;
    left = right;
    right = remainder;
  }
  return left;
}

bool DecimalPlaces(std::uint64_t denominator, std::uint8_t& places) noexcept {
  std::uint8_t twos = 0U;
  std::uint8_t fives = 0U;
  while (denominator % 2U == 0U) {
    denominator /= 2U;
    ++twos;
  }
  while (denominator % 5U == 0U) {
    denominator /= 5U;
    ++fives;
  }
  places = (std::max)(twos, fives);
  return denominator == 1U && places <= 18U;
}

std::uint64_t Power10(std::uint8_t exponent) noexcept {
  std::uint64_t value = 1U;
  for (std::uint8_t index = 0U; index < exponent; ++index) {
    value *= 10U;
  }
  return value;
}

bool Multiply(SignedCoefficient256& value, std::uint64_t multiplier) noexcept {
  const std::uint32_t multiplier_words[2]{static_cast<std::uint32_t>(multiplier),
                                          static_cast<std::uint32_t>(multiplier >> 32U)};
  std::array<std::uint32_t, kDecimalCoefficientWordCount> result{};
  for (std::size_t left = 0U; left < value.words.size(); ++left) {
    std::uint64_t carry = 0U;
    for (std::size_t right = 0U; right < 2U; ++right) {
      const std::size_t target = left + right;
      if (target >= result.size()) {
        if (value.words[left] != 0U && multiplier_words[right] != 0U) {
          return false;
        }
        continue;
      }
      const std::uint64_t product =
          static_cast<std::uint64_t>(value.words[left]) * multiplier_words[right] + result[target] +
          carry;
      result[target] = static_cast<std::uint32_t>(product);
      carry = product >> 32U;
    }
    std::size_t target = left + 2U;
    while (carry != 0U && target < result.size()) {
      const std::uint64_t sum = static_cast<std::uint64_t>(result[target]) + carry;
      result[target] = static_cast<std::uint32_t>(sum);
      carry = sum >> 32U;
      ++target;
    }
    if (carry != 0U) {
      return false;
    }
  }
  value.words = result;
  return true;
}

bool MakeCoefficient(std::int64_t numerator, std::uint64_t denominator, std::uint8_t own_places,
                     std::uint8_t common_places, SignedCoefficient256& output) noexcept {
  output = {};
  output.negative = numerator < 0;
  const std::uint64_t magnitude = Magnitude(numerator);
  output.words[0] = static_cast<std::uint32_t>(magnitude);
  output.words[1] = static_cast<std::uint32_t>(magnitude >> 32U);
  const std::uint64_t exact_factor = Power10(own_places) / denominator;
  if (!Multiply(output, exact_factor) || !Multiply(output, Power10(common_places - own_places))) {
    return false;
  }
  if (magnitude == 0U) {
    output.negative = false;
  }
  return true;
}

void Reduce(std::int64_t& numerator, std::uint64_t& denominator) noexcept {
  if (numerator == 0) {
    denominator = 1U;
    return;
  }
  const std::uint64_t divisor = GreatestCommonDivisor(Magnitude(numerator), denominator);
  denominator /= divisor;
  if (divisor != 1U) {
    const bool negative = numerator < 0;
    const std::uint64_t reduced_magnitude = Magnitude(numerator) / divisor;
    if (negative && reduced_magnitude == (std::uint64_t{1U} << 63U)) {
      numerator = (std::numeric_limits<std::int64_t>::min)();
    } else {
      const std::int64_t signed_magnitude = static_cast<std::int64_t>(reduced_magnitude);
      numerator = negative ? -signed_magnitude : signed_magnitude;
    }
  }
}

}  // namespace

DecimalDescriptorError DeriveLinearConversion(ValueType raw_value_type,
                                              std::int64_t scale_numerator,
                                              std::uint64_t scale_denominator,
                                              std::int64_t bias_numerator,
                                              std::uint64_t bias_denominator,
                                              LinearConversionDescriptor& output) noexcept {
  if (scale_numerator == 0) {
    return DecimalDescriptorError::ZERO_SCALE;
  }
  if (scale_denominator == 0U || bias_denominator == 0U ||
      scale_denominator > kMaximumAuthorDenominator ||
      bias_denominator > kMaximumAuthorDenominator) {
    return DecimalDescriptorError::DENOMINATOR_OUT_OF_RANGE;
  }
  Reduce(scale_numerator, scale_denominator);
  Reduce(bias_numerator, bias_denominator);
  std::uint8_t scale_places = 0U;
  std::uint8_t bias_places = 0U;
  if (!DecimalPlaces(scale_denominator, scale_places)) {
    return DecimalDescriptorError::NON_TERMINATING_SCALE;
  }
  if (!DecimalPlaces(bias_denominator, bias_places)) {
    return DecimalDescriptorError::NON_TERMINATING_BIAS;
  }
  LinearConversionDescriptor candidate;
  candidate.raw_value_type = raw_value_type;
  candidate.scale_numerator = scale_numerator;
  candidate.scale_denominator = scale_denominator;
  candidate.bias_numerator = bias_numerator;
  candidate.bias_denominator = bias_denominator;
  candidate.decimal_places = (std::max)(scale_places, bias_places);
  if (!MakeCoefficient(scale_numerator, scale_denominator, scale_places, candidate.decimal_places,
                       candidate.scale_coefficient) ||
      !MakeCoefficient(bias_numerator, bias_denominator, bias_places, candidate.decimal_places,
                       candidate.bias_coefficient)) {
    return DecimalDescriptorError::ARITHMETIC_OVERFLOW;
  }
  output = candidate;
  return DecimalDescriptorError::NONE;
}

bool LinearConversionEqual(const LinearConversionDescriptor& left,
                           const LinearConversionDescriptor& right) noexcept {
  return left.raw_value_type == right.raw_value_type &&
         left.scale_numerator == right.scale_numerator &&
         left.scale_denominator == right.scale_denominator &&
         left.bias_numerator == right.bias_numerator &&
         left.bias_denominator == right.bias_denominator &&
         left.decimal_places == right.decimal_places &&
         left.scale_coefficient.words == right.scale_coefficient.words &&
         left.scale_coefficient.negative == right.scale_coefficient.negative &&
         left.bias_coefficient.words == right.bias_coefficient.words &&
         left.bias_coefficient.negative == right.bias_coefficient.negative;
}

}  // namespace pae::protocol_plan::detail

#endif
