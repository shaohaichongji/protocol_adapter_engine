#include "cli_path_encoding_internal.h"

#include <limits>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

namespace pae::protocol_lab {
namespace {

bool IsUtf8(std::string_view text) {
#if defined(_WIN32)
  if (text.empty()) return true;
  if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
  return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                             static_cast<int>(text.size()), nullptr, 0) > 0;
#else
  (void)text;
  return true;
#endif
}

}  // namespace

std::filesystem::path DecodeCommandLinePath(std::string_view text) {
  if (IsUtf8(text)) return std::filesystem::u8path(text.begin(), text.end());
  return std::filesystem::path{text};
}

std::string EncodePathForCli(const std::filesystem::path& path) { return path.generic_u8string(); }

}  // namespace pae::protocol_lab
