#include <string>
#include <vector>

#include "../../tools/protocol_lab_ui/ascii_escaped_input.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
  auto parsed = ParseAsciiEscaped(u"A \\\\ \\r\\n\\t\\0\\x1F4");
  assert(parsed.ok());
  assert(parsed.bytes ==
         std::vector<std::uint8_t>({'A', ' ', '\\', ' ', 0x0D, 0x0A, 0x09, 0x00, 0x1F, '4'}));

  parsed = ParseAsciiEscaped(u"\\x414");
  assert(parsed.ok() && parsed.bytes == std::vector<std::uint8_t>({'A', '4'}));
  parsed = ParseAsciiEscaped(u"A\\");
  assert(!parsed.ok() && parsed.utf16_offset == 1U);
  parsed = ParseAsciiEscaped(u"A\\q");
  assert(!parsed.ok() && parsed.utf16_offset == 1U);
  parsed = ParseAsciiEscaped(u"A\\x8F");
  assert(!parsed.ok() && parsed.utf16_offset == 1U);
  parsed = ParseAsciiEscaped(u"A\nB");
  assert(!parsed.ok() && parsed.utf16_offset == 1U);
  parsed = ParseAsciiEscaped(u"A\U0001F600B");
  assert(!parsed.ok() && parsed.utf16_offset == 1U);

  std::string error;
  const auto formatted = FormatAsciiEscaped(
      std::vector<std::uint8_t>{'A', '\\', 0x0D, 0x0A, 0x09, 0x00, 0x1F, 0x7F}, error);
  assert(formatted.has_value() && *formatted == u"A\\\\\\r\\n\\t\\0\\x1F\\x7F");
  assert(!FormatAsciiEscaped(std::vector<std::uint8_t>{0x80U}, error).has_value());
  assert(ByteEditorCapacity(8U, ByteRepresentation::ASCII_ESCAPED) == 36U);
  assert(ByteEditorCapacity(3U, ByteRepresentation::HEX) == 8U);
  return 0;
}
