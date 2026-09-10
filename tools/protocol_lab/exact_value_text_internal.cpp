#include "exact_value_text_internal.h"

#include <limits>
#include <utility>

namespace pae::protocol_lab::v06::internal {

bool ParseCanonicalUint64Text(std::string_view text, std::uint64_t& output) noexcept {
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

bool ParseCanonicalInt64Text(std::string_view text, std::int64_t& output) noexcept {
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

bool ParseCanonicalUpperHexText(std::string_view text, std::vector<std::uint8_t>& output) {
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

}  // namespace pae::protocol_lab::v06::internal
