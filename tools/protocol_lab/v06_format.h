#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pae::protocol_lab::v06 {

inline constexpr std::string_view kValuesFormat = "pae.lab.values/0.4";
inline constexpr std::string_view kResultFormat = "pae.lab.result/0.6";
inline constexpr std::string_view kFingerprintDomain = "pae.lab.fingerprint/0.6";

struct Decimal64 {
  std::int64_t coefficient = 0;
  std::int32_t scale = 0;
};

struct ParsedValue {
  std::string id;
  std::string kind;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  std::vector<std::uint8_t> bytes;
  std::string enum_entry_id;
  bool bool_value = false;
  Decimal64 decimal64_value;
};

struct ParsedValues {
  std::string pipeline_id;
  std::string message_id;
  std::vector<ParsedValue> fields;
};

struct FieldResult {
  std::string id;
  std::string kind;
  std::string raw_value;
  std::string logical_value;
  bool enum_known = false;
  std::optional<Decimal64> decimal64;
  std::optional<std::string> raw_kind;
};

struct Result {
  std::string command;
  std::string operation_kind;
  std::string operation_status;
  int exit_code = 0;
  std::optional<std::string> config_sha256;
  std::optional<std::string> protocol_id;
  std::optional<std::string> pipeline_id;
  std::optional<std::string> message_id;
  std::optional<std::string> direction_id;
  std::optional<std::string> frame_hex;
  std::optional<std::string> tx_frame_hex;
  std::optional<std::string> rx_frame_hex;
  // The caller supplies successful fields in frozen configuration order. This isolated module has
  // no Plan dependency and must not infer or reorder them from Values input order.
  std::vector<FieldResult> fields;
  std::optional<std::string> diagnostic_id;
  std::string diagnostic_detail;
  std::string replay_mode = "NONE";
  std::string replay_subject = "NONE";
  std::string current_execution_status = "NOT_EVALUATED";
  std::optional<std::string> current_execution_diagnostic_id;
  std::optional<std::string> conversion_error;
  std::optional<std::string> failed_field_id;
  std::optional<std::size_t> failed_field_index;
  std::optional<std::size_t> failed_value_index;
};

bool ParseValues(std::string& text, ParsedValues& output, std::string& error);
bool ParseResult(std::string& text, Result& output, std::string& error);
Decimal64 NormalizeDecimal64(Decimal64 value) noexcept;
bool ValidateResult(const Result& result, std::string& error);
std::string EncodeFieldsCanonical(const std::vector<FieldResult>& fields, std::string& error);
std::string EncodeFingerprintPayload(const Result& result, std::string& error);
std::string FinalizeFingerprint(const Result& result, std::string& error);
std::string SerializeResult(const Result& result, std::string& error);

}  // namespace pae::protocol_lab::v06
