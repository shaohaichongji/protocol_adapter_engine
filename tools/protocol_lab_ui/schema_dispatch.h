#pragma once

#include <string>
#include <string_view>

namespace pae::protocol_lab_ui {

enum class SchemaDispatchStatus {
  BINARY_PUBLIC,
  PRIVATE_LEGACY,
  PRIVATE_ASCII,
  CLASSIFICATION_FAILED,
};

struct SchemaDispatchResult {
  SchemaDispatchStatus status = SchemaDispatchStatus::CLASSIFICATION_FAILED;
  std::string detail;
};

// Dispatch hint only. The selected compiler must still validate the entire configuration.
// This does not interpret field, matcher, layout, integrity, or protocol semantics.
SchemaDispatchResult ClassifySchemaVersion(std::string_view json_bytes);

}  // namespace pae::protocol_lab_ui
