#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "ui_physical_types.h"

namespace pae::protocol_lab_ui {

struct UiFieldResult {
  std::size_t field_index = 0U;
  std::string id;
  std::string raw_value;
  std::string logical_value;
  std::optional<ByteRange> actual_range;
};

}  // namespace pae::protocol_lab_ui
