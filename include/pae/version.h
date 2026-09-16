#pragma once

#include <string_view>

namespace pae {

inline constexpr std::string_view kPublicApiVersion = "0.experimental.1";

// The Stage 1 compiler is built with the repository's complete Schema 0.1-0.11 feature set.
// This query describes that build contract; it does not promise future source or binary ABI
// compatibility for the experimental API.
constexpr bool IsSchemaVersionSupported(std::string_view version) noexcept {
  return version == "0.1" || version == "0.2" || version == "0.3" || version == "0.4" ||
         version == "0.5" || version == "0.6" || version == "0.7" || version == "0.8" ||
         version == "0.9" || version == "0.10" || version == "0.11";
}

}  // namespace pae
