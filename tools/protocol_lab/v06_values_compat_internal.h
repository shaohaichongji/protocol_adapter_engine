#pragma once

#include <string>
#include <string_view>

#include "v06_format.h"

namespace pae::protocol_lab::v06::internal {

// C1-only version dispatcher. Values 0.4 remains owned by v06::ParseValues; this function adds
// strict legacy parsing without widening that A-stage parser's acceptance domain.
bool ParseCompatibleValues(std::string& text, ParsedValues& output, std::string& error);
bool ValuesFormatCompatibleWithSchema(std::string_view values_format,
                                      std::string_view schema_version) noexcept;

}  // namespace pae::protocol_lab::v06::internal
