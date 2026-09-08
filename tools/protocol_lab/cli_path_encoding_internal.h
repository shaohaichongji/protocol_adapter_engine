#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace pae::protocol_lab {

std::filesystem::path DecodeCommandLinePath(std::string_view text);
std::string EncodePathForCli(const std::filesystem::path& path);

}  // namespace pae::protocol_lab
