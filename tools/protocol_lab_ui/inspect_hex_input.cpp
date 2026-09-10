#include "inspect_hex_input.h"

#include <limits>

namespace pae::protocol_lab_ui {
namespace {

bool IsAcceptedWhitespace(unsigned char value) noexcept {
  return value == 0x20U || value == 0x09U || value == 0x0DU || value == 0x0AU;
}

int Nibble(unsigned char value) noexcept {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  return -1;
}

char UpperHex(unsigned int value) noexcept {
  return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
}

}  // namespace

InspectHexParseResult ParseInspectHex(std::string_view input, std::size_t maximum_frame_bytes) {
  InspectHexParseResult result;
  if (maximum_frame_bytes == 0U ||
      maximum_frame_bytes > std::numeric_limits<std::size_t>::max() / 3U) {
    result.error = InspectHexError::INVALID_BUDGET;
    return result;
  }
  const std::size_t maximum_text_bytes = maximum_frame_bytes * 3U;
  if (input.size() > maximum_text_bytes) {
    result.error = InspectHexError::TEXT_LIMIT_EXCEEDED;
    result.input_offset = maximum_text_bytes;
    return result;
  }

  result.bytes.reserve(maximum_frame_bytes);
  result.canonical_hex.reserve(maximum_frame_bytes * 2U);
  int high_nibble = -1;
  std::size_t high_offset = 0U;
  for (std::size_t offset = 0U; offset < input.size(); ++offset) {
    const auto value = static_cast<unsigned char>(input[offset]);
    if (IsAcceptedWhitespace(value)) continue;
    const int nibble = Nibble(value);
    if (nibble < 0) {
      result.error = InspectHexError::INVALID_CHARACTER;
      result.input_offset = offset;
      result.bytes.clear();
      result.canonical_hex.clear();
      return result;
    }
    if (high_nibble < 0) {
      if (result.bytes.size() >= maximum_frame_bytes) {
        result.error = InspectHexError::FRAME_LIMIT_EXCEEDED;
        result.input_offset = offset;
        result.bytes.clear();
        result.canonical_hex.clear();
        return result;
      }
      high_nibble = nibble;
      high_offset = offset;
      continue;
    }
    result.bytes.push_back(static_cast<std::uint8_t>((high_nibble << 4U) | nibble));
    result.canonical_hex.push_back(UpperHex(static_cast<unsigned int>(high_nibble)));
    result.canonical_hex.push_back(UpperHex(static_cast<unsigned int>(nibble)));
    high_nibble = -1;
  }
  if (high_nibble >= 0) {
    result.error = InspectHexError::ODD_NIBBLE_COUNT;
    result.input_offset = high_offset;
    result.bytes.clear();
    result.canonical_hex.clear();
    return result;
  }
  if (result.bytes.empty()) {
    result.error = InspectHexError::EMPTY_INPUT;
    result.input_offset = 0U;
  }
  return result;
}

const char* InspectHexDiagnosticId(InspectHexError error) noexcept {
  switch (error) {
    case InspectHexError::NONE:
      return "";
    case InspectHexError::INVALID_BUDGET:
      return "UI_INSPECT_INVALID_BUDGET";
    case InspectHexError::EMPTY_INPUT:
      return "UI_INSPECT_HEX_EMPTY";
    case InspectHexError::TEXT_LIMIT_EXCEEDED:
      return "UI_INSPECT_HEX_TEXT_LIMIT_EXCEEDED";
    case InspectHexError::FRAME_LIMIT_EXCEEDED:
      return "UI_INSPECT_FRAME_LIMIT_EXCEEDED";
    case InspectHexError::INVALID_CHARACTER:
      return "UI_INSPECT_HEX_INVALID_CHARACTER";
    case InspectHexError::ODD_NIBBLE_COUNT:
      return "UI_INSPECT_HEX_ODD_NIBBLE_COUNT";
  }
  return "UI_INSPECT_HEX_UNKNOWN_ERROR";
}

const char* InspectHexErrorDetail(InspectHexError error) noexcept {
  switch (error) {
    case InspectHexError::NONE:
      return "";
    case InspectHexError::INVALID_BUDGET:
      return "selected Pipeline has no valid positive Inspect frame budget";
    case InspectHexError::EMPTY_INPUT:
      return "Inspect Hex must contain at least one complete byte";
    case InspectHexError::TEXT_LIMIT_EXCEEDED:
      return "Inspect Hex text exceeds the bounded input budget";
    case InspectHexError::FRAME_LIMIT_EXCEEDED:
      return "decoded Inspect frame exceeds the selected Pipeline budget";
    case InspectHexError::INVALID_CHARACTER:
      return "Inspect Hex contains a character outside hex digits and SP/HT/CR/LF";
    case InspectHexError::ODD_NIBBLE_COUNT:
      return "Inspect Hex ends with an unpaired nibble";
  }
  return "unknown Inspect Hex error";
}

}  // namespace pae::protocol_lab_ui
