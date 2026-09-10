#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pae::protocol_lab_ui {

enum class InspectHexError {
  NONE,
  INVALID_BUDGET,
  EMPTY_INPUT,
  TEXT_LIMIT_EXCEEDED,
  FRAME_LIMIT_EXCEEDED,
  INVALID_CHARACTER,
  ODD_NIBBLE_COUNT,
};

struct InspectHexParseResult {
  InspectHexError error = InspectHexError::NONE;
  std::size_t input_offset = 0U;
  std::vector<std::uint8_t> bytes;
  std::string canonical_hex;

  bool ok() const noexcept { return error == InspectHexError::NONE; }
};

InspectHexParseResult ParseInspectHex(std::string_view input, std::size_t maximum_frame_bytes);
const char* InspectHexDiagnosticId(InspectHexError error) noexcept;
const char* InspectHexErrorDetail(InspectHexError error) noexcept;

}  // namespace pae::protocol_lab_ui
