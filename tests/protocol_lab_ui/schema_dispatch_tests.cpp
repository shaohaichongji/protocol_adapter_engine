#include <iostream>
#include <string>
#include <string_view>

#include "schema_dispatch.h"

namespace {
int failures = 0;
void Check(std::string_view json, pae::protocol_lab_ui::SchemaDispatchStatus expected,
           std::string_view name) {
  const auto result = pae::protocol_lab_ui::ClassifySchemaVersion(json);
  if (result.status != expected) {
    std::cerr << "SCHEMA_DISPATCH_FAIL case=" << name
              << " actual=" << static_cast<int>(result.status)
              << " expected=" << static_cast<int>(expected) << " detail=" << result.detail << '\n';
    ++failures;
  }
}
}  // namespace

int main() {
  using pae::protocol_lab_ui::SchemaDispatchStatus;
  Check(R"({"schema_version":"0.9"})", SchemaDispatchStatus::BINARY_PUBLIC, "binary");
  Check(R"({"schema_version":"0.5"})", SchemaDispatchStatus::PRIVATE_LEGACY, "legacy");
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  Check(R"({"schema_version":"0.10"})", SchemaDispatchStatus::ASCII_PUBLIC, "ascii");
#else
  Check(R"({"schema_version":"0.10"})", SchemaDispatchStatus::PRIVATE_ASCII, "ascii");
#endif
#else
  Check(R"({"schema_version":"0.10"})", SchemaDispatchStatus::CLASSIFICATION_FAILED, "ascii_off");
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  Check(R"({"schema_version":"0.11"})", SchemaDispatchStatus::PRIVATE_ASCII, "ascii_stream");
#else
  Check(R"({"schema_version":"0.11"})", SchemaDispatchStatus::CLASSIFICATION_FAILED,
        "ascii_stream_off");
#endif
  Check(R"({"schema_\u0076ersion":"0.9"})", SchemaDispatchStatus::BINARY_PUBLIC,
        "escaped_root_key");
  Check(R"({"schema_version":"0.\u0039"})", SchemaDispatchStatus::BINARY_PUBLIC, "escaped_value");
  Check(R"({"nested":{"schema_version":"0.9"},"schema_version":"0.5"})",
        SchemaDispatchStatus::PRIVATE_LEGACY, "nested_decoy");
  Check(R"({"text":"schema_version\\\":\\\"0.9","schema_version":"0.5"})",
        SchemaDispatchStatus::PRIVATE_LEGACY, "string_decoy");
  Check(R"({"schema_version":"0.9","schema_version":"0.5"})",
        SchemaDispatchStatus::CLASSIFICATION_FAILED, "duplicate_plain");
  Check(R"({"schema_version":"0.9","schema_\u0076ersion":"0.5"})",
        SchemaDispatchStatus::CLASSIFICATION_FAILED, "duplicate_escaped");
  Check(R"({"nested":{"schema_version":"0.9"}})", SchemaDispatchStatus::CLASSIFICATION_FAILED,
        "missing_root");
  Check(R"({"schema_version":9})", SchemaDispatchStatus::CLASSIFICATION_FAILED, "wrong_type");
  Check(R"({"schema_version":null})", SchemaDispatchStatus::CLASSIFICATION_FAILED, "null_type");
  Check(R"({"schema_version":"0.12"})", SchemaDispatchStatus::CLASSIFICATION_FAILED,
        "unknown_version");
  Check(R"([{"schema_version":"0.9"}])", SchemaDispatchStatus::CLASSIFICATION_FAILED, "root_array");
  Check(R"({"schema_version":"0.9",})", SchemaDispatchStatus::CLASSIFICATION_FAILED,
        "trailing_comma");
  Check(R"({"schema_version":"0.9"} // comment)", SchemaDispatchStatus::CLASSIFICATION_FAILED,
        "comment");
  Check(std::string(4U * 1024U * 1024U + 1U, 'x'), SchemaDispatchStatus::CLASSIFICATION_FAILED,
        "input_limit");
  std::string deep = R"({"schema_version":"0.9","nested":)";
  for (int i = 0; i < 65; ++i) deep.push_back('[');
  deep.push_back('0');
  for (int i = 0; i < 65; ++i) deep.push_back(']');
  deep.push_back('}');
  Check(deep, SchemaDispatchStatus::CLASSIFICATION_FAILED, "depth_limit");
  std::cout << "SCHEMA_DISPATCH_SUMMARY failed=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}
