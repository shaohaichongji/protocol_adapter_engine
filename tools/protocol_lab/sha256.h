#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace pae::protocol_lab {

std::string HashBytes(const std::uint8_t* data, std::size_t size);
std::string HashBytes(std::string_view data);
std::string HashBytes(const std::vector<std::uint8_t>& data);

}  // namespace pae::protocol_lab
