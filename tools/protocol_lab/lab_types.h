#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pae::protocol_lab {

inline constexpr std::size_t kMaximumInputBytes = 16U * 1024U * 1024U;
inline constexpr std::string_view kResultFormat = "pae.lab.result/0.1";
inline constexpr std::string_view kValuesFormat = "pae.lab.values/0.1";
inline constexpr std::string_view kRecordFormat = "pae.lab.record/0.1";
inline constexpr std::string_view kToolVersion = "0.1.0-offline-slice";
inline constexpr int kRecordFailedExitCode = 7;

enum class Command { INSPECT, ENCODE, REPLAY, COMPARE };

struct Arguments {
  Command command = Command::INSPECT;
  std::filesystem::path config;
  std::filesystem::path frame_binary;
  std::filesystem::path frame_hex;
  std::filesystem::path values;
  std::filesystem::path bundle;
  std::filesystem::path record_root;
  std::filesystem::path left_run;
  std::filesystem::path right_run;
  std::filesystem::path left_frame_binary;
  std::filesystem::path right_frame_binary;
  std::filesystem::path left_frame_hex;
  std::filesystem::path right_frame_hex;
  std::string output = "text";
  std::string expect_status;
};

struct FieldResult {
  std::string id;
  std::string kind;
  std::string raw_value;
  std::string logical_value;
  bool enum_known = false;
};

struct OperationResult {
  std::string command;
  std::string operation_kind;
  std::string status = "INTERNAL_ERROR";
  int exit_code = 10;
  std::string config_sha256;
  std::string protocol_id;
  std::string pipeline_id;
  std::string message_id;
  std::string direction_id;
  std::vector<std::uint8_t> frame;
  std::vector<FieldResult> fields;
  std::string diagnostic_id;
  std::string diagnostic_detail;
  std::string deterministic_fingerprint;
  std::string evidence_bundle;
  std::optional<bool> comparison_equal;
  std::vector<std::string> comparison_categories;
  bool cross_config_replay = false;
};

struct ParsedValue {
  std::string id;
  std::string kind;
  std::uint64_t uint64_value = 0U;
  std::vector<std::uint8_t> bytes;
  std::string enum_entry_id;
};

struct ParsedValues {
  std::string pipeline_id;
  std::string message_id;
  std::vector<ParsedValue> fields;
};

struct StoredRun {
  std::string operation_kind;
  std::string operation_status;
  std::string config_sha256;
  std::string pipeline_id;
  std::string message_id;
  std::string direction_id;
  std::string frame_hex;
  std::string fields_canonical;
  std::string diagnostic_id;
  std::string frame_file;
  std::string values_file;
  std::string deterministic_fingerprint;
};

struct RecordedFile {
  std::string relative_path;
  std::string sha256;
  std::size_t size = 0U;
};

}  // namespace pae::protocol_lab
