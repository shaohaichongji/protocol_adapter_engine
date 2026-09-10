#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace pae::protocol_lab::v06::internal {

// Canonical lexical parsers shared by Values 0.4 and the offline typed editor.
// Output is replaced only on success.
bool ParseCanonicalUint64Text(std::string_view text, std::uint64_t& output) noexcept;
bool ParseCanonicalInt64Text(std::string_view text, std::int64_t& output) noexcept;
bool ParseCanonicalUpperHexText(std::string_view text, std::vector<std::uint8_t>& output);

}  // namespace pae::protocol_lab::v06::internal
