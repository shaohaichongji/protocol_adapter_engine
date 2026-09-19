#pragma once

#include <string>
#include <string_view>

namespace pae::protocol_lab_ui {

enum class SchemaDispatchStatus {
  BINARY_PUBLIC,
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  LEGACY_PUBLIC,
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  ASCII_PUBLIC,
#endif
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
