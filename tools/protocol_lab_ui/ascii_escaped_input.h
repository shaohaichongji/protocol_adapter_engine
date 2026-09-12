#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pae::protocol_lab_ui {

enum class ByteRepresentation {
  HEX,
  ASCII_ESCAPED,
};

struct AsciiEscapedParseResult {
  std::vector<std::uint8_t> bytes;
  std::string detail;
  std::optional<std::size_t> utf16_offset;

  bool ok() const noexcept { return detail.empty(); }
};

AsciiEscapedParseResult ParseAsciiEscaped(std::u16string_view input);
std::optional<std::u16string> FormatAsciiEscaped(const std::vector<std::uint8_t>& bytes,
                                                 std::string& error);
std::string FormatContinuousUpperHex(const std::vector<std::uint8_t>& bytes);
bool ParseContinuousUpperHex(std::string_view input, std::vector<std::uint8_t>& bytes,
                             std::size_t& error_offset) noexcept;
std::optional<std::size_t> ByteEditorCapacity(std::size_t maximum_bytes,
                                              ByteRepresentation representation) noexcept;

}  // namespace pae::protocol_lab_ui
