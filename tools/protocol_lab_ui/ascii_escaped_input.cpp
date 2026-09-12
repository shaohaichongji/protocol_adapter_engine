#include "ascii_escaped_input.h"

#include <limits>

namespace pae::protocol_lab_ui {
namespace {

int HexNibble(char16_t value) noexcept {
  if (value >= u'0' && value <= u'9') return value - u'0';
  if (value >= u'A' && value <= u'F') return value - u'A' + 10;
  return -1;
}

char HexDigit(unsigned int value) noexcept {
  return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
}

void Fail(AsciiEscapedParseResult& result, std::size_t offset, std::string detail) {
  result.bytes.clear();
  result.utf16_offset = offset;
  result.detail = std::move(detail);
}

}  // namespace

AsciiEscapedParseResult ParseAsciiEscaped(std::u16string_view input) {
  AsciiEscapedParseResult result;
  result.bytes.reserve(input.size());
  for (std::size_t offset = 0U; offset < input.size(); ++offset) {
    const char16_t value = input[offset];
    if (value >= 0x20U && value <= 0x7EU && value != u'\\') {
      result.bytes.push_back(static_cast<std::uint8_t>(value));
      continue;
    }
    if (value != u'\\') {
      Fail(result, offset,
           value > 0x7FU ? "non-ASCII Unicode is not accepted"
                         : "actual control characters are not accepted; use a visible escape");
      return result;
    }
    if (++offset >= input.size()) {
      Fail(result, offset - 1U, "trailing backslash has no escape code");
      return result;
    }
    const char16_t escape = input[offset];
    switch (escape) {
      case u'\\':
        result.bytes.push_back(0x5CU);
        break;
      case u'r':
        result.bytes.push_back(0x0DU);
        break;
      case u'n':
        result.bytes.push_back(0x0AU);
        break;
      case u't':
        result.bytes.push_back(0x09U);
        break;
      case u'0':
        result.bytes.push_back(0x00U);
        break;
      case u'x': {
        const std::size_t escape_begin = offset - 1U;
        if (offset + 2U >= input.size()) {
          Fail(result, escape_begin, "\\x requires exactly two uppercase Hex digits");
          return result;
        }
        const int high = HexNibble(input[offset + 1U]);
        const int low = HexNibble(input[offset + 2U]);
        if (high < 0 || low < 0) {
          Fail(result, high < 0 ? offset + 1U : offset + 2U,
               "\\x requires exactly two uppercase Hex digits");
          return result;
        }
        const auto byte = static_cast<std::uint8_t>((high << 4U) | low);
        if (byte > 0x7FU) {
          Fail(result, escape_begin, "\\x value must be in the ASCII range 00..7F");
          return result;
        }
        result.bytes.push_back(byte);
        offset += 2U;
        break;
      }
      default:
        Fail(result, offset - 1U, "unknown ASCII escape");
        return result;
    }
  }
  return result;
}

std::optional<std::u16string> FormatAsciiEscaped(const std::vector<std::uint8_t>& bytes,
                                                 std::string& error) {
  std::u16string result;
  if (bytes.size() > (std::numeric_limits<std::size_t>::max)() / 4U) {
    error = "ASCII escaped display length overflow";
    return std::nullopt;
  }
  result.reserve(bytes.size() * 4U);
  for (const std::uint8_t byte : bytes) {
    if (byte > 0x7FU) {
      error = "non-ASCII bytes cannot be represented as editable ASCII escaped text";
      return std::nullopt;
    }
    if (byte >= 0x20U && byte <= 0x7EU && byte != 0x5CU) {
      result.push_back(static_cast<char16_t>(byte));
    } else if (byte == 0x5CU) {
      result += u"\\\\";
    } else if (byte == 0x0DU) {
      result += u"\\r";
    } else if (byte == 0x0AU) {
      result += u"\\n";
    } else if (byte == 0x09U) {
      result += u"\\t";
    } else if (byte == 0x00U) {
      result += u"\\0";
    } else {
      result += u"\\x";
      result.push_back(static_cast<char16_t>(HexDigit(byte >> 4U)));
      result.push_back(static_cast<char16_t>(HexDigit(byte & 0x0FU)));
    }
  }
  error.clear();
  return result;
}

std::string FormatContinuousUpperHex(const std::vector<std::uint8_t>& bytes) {
  std::string result;
  result.reserve(bytes.size() * 2U);
  for (const std::uint8_t byte : bytes) {
    result.push_back(HexDigit(byte >> 4U));
    result.push_back(HexDigit(byte & 0x0FU));
  }
  return result;
}

bool ParseContinuousUpperHex(std::string_view input, std::vector<std::uint8_t>& bytes,
                             std::size_t& error_offset) noexcept {
  bytes.clear();
  if (input.size() % 2U != 0U) {
    error_offset = input.empty() ? 0U : input.size() - 1U;
    return false;
  }
  bytes.reserve(input.size() / 2U);
  for (std::size_t offset = 0U; offset < input.size(); offset += 2U) {
    const int high = HexNibble(static_cast<char16_t>(static_cast<unsigned char>(input[offset])));
    const int low =
        HexNibble(static_cast<char16_t>(static_cast<unsigned char>(input[offset + 1U])));
    if (high < 0 || low < 0) {
      error_offset = high < 0 ? offset : offset + 1U;
      bytes.clear();
      return false;
    }
    bytes.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
  return true;
}

std::optional<std::size_t> ByteEditorCapacity(std::size_t maximum_bytes,
                                              ByteRepresentation representation) noexcept {
  const std::size_t rejected_operation_headroom =
      representation == ByteRepresentation::ASCII_ESCAPED ? 4U : 2U;
  const std::size_t multiplier = representation == ByteRepresentation::ASCII_ESCAPED ? 4U : 2U;
  if (maximum_bytes >
      ((std::numeric_limits<std::size_t>::max)() - rejected_operation_headroom) / multiplier) {
    return std::nullopt;
  }
  return maximum_bytes * multiplier + rejected_operation_headroom;
}

}  // namespace pae::protocol_lab_ui
