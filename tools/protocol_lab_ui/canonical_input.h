#pragma once

#include <cstdint>
#include <string_view>

#include "owned_presentation_types.h"

namespace pae::protocol_lab_ui {

bool ParseCanonicalUint64(std::string_view text, std::uint64_t& output) noexcept;
bool ParseCanonicalInt64(std::string_view text, std::int64_t& output) noexcept;
Decimal64 NormalizeDecimal64(Decimal64 value) noexcept;

}  // namespace pae::protocol_lab_ui
