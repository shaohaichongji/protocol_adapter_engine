#include "canonical_input.h"

#include <limits>

namespace pae::protocol_lab_ui {

bool ParseCanonicalUint64(std::string_view text, std::uint64_t& output) noexcept {
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

bool ParseCanonicalInt64(std::string_view text, std::int64_t& output) noexcept {
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

Decimal64 NormalizeDecimal64(Decimal64 value) noexcept {
  if (value.coefficient == 0) return Decimal64{};
  while (value.scale > 0 && value.coefficient % 10 == 0) {
    value.coefficient /= 10;
    --value.scale;
  }
  return value;
}

}  // namespace pae::protocol_lab_ui
