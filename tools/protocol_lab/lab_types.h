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
inline constexpr std::uint32_t kDefaultUdpTimeoutMs = 2000U;
inline constexpr std::uint32_t kMaximumUdpTimeoutMs = 60000U;
inline constexpr std::string_view kResultFormatV1 = "pae.lab.result/0.1";
inline constexpr std::string_view kResultFormat = "pae.lab.result/0.2";
inline constexpr std::string_view kResultFormatV3 = "pae.lab.result/0.3";
inline constexpr std::string_view kValuesFormat = "pae.lab.values/0.1";
inline constexpr std::string_view kValuesFormatV2 = "pae.lab.values/0.2";
inline constexpr std::string_view kRecordFormatV1 = "pae.lab.record/0.1";
inline constexpr std::string_view kRecordFormat = "pae.lab.record/0.2";
inline constexpr std::string_view kRecordFormatV3 = "pae.lab.record/0.3";
inline constexpr std::string_view kEventFormat = "pae.lab.event/0.2";
inline constexpr std::string_view kEventFormatV3 = "pae.lab.event/0.3";
inline constexpr std::string_view kToolVersion = "0.2.0-udp-exchange-evidence-slice";
inline constexpr std::string_view kToolVersionV3 = "0.3.0-bitfield-slice";
inline constexpr int kRecordFailedExitCode = 7;

enum class Command { INSPECT, ENCODE, REPLAY, COMPARE, UDP_EXCHANGE };

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
  std::string local_endpoint = "127.0.0.1:0";
  std::string remote_endpoint;
  std::string receive_pipeline;
  std::uint32_t timeout_ms = kDefaultUdpTimeoutMs;
  bool send = false;
  bool allow_non_loopback = false;
};

struct FieldResult {
  std::string id;
  std::string kind;
  std::string raw_value;
  std::string logical_value;
  bool enum_known = false;
};

struct LabEvent {
  std::size_t event_id = 0U;
  std::string event_kind;
  std::string direction;
  std::string frame_file;
  std::size_t frame_length = 0U;
  std::string frame_sha256;
  std::string frame_origin;
  std::string peer_kind;
  std::string remote_endpoint;
  std::string received_from;
  bool succeeded = false;
  std::string diagnostic_id;
  std::string wall_clock_utc;
  std::uint64_t monotonic_offset_ns = 0U;
};

struct HistoricalTransportFacts {
  bool present = false;
  std::string transport;
  std::uint32_t timeout_ms = 0U;
  std::string status;
  std::string diagnostic_id;
  std::string local_endpoint;
  std::string remote_endpoint;
  std::string received_from;
  bool send_attempted = false;
  bool send_succeeded = false;
  bool response_received = false;
  bool response_decoded = false;
};

struct OperationResult {
  std::string schema_version = "0.1";
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
  std::string frame_file;
  std::string transport = "OFFLINE";
  std::string local_endpoint;
  std::string remote_endpoint;
  std::string received_from;
  std::string receive_pipeline_id;
  std::uint32_t timeout_ms = 0U;
  std::uint64_t max_frame_bytes = kMaximumInputBytes;
  std::vector<std::uint8_t> tx_frame;
  std::vector<std::uint8_t> rx_frame;
  bool send_attempted = false;
  bool send_succeeded = false;
  bool response_received = false;
  bool response_decoded = false;
  std::string replay_mode = "NONE";
  std::string replay_subject = "NONE";
  std::string current_execution_status = "NOT_EVALUATED";
  std::string current_execution_diagnostic_id;
  std::string comparison_status = "NOT_APPLICABLE";
  std::string comparison_reason;
  HistoricalTransportFacts historical_transport;
  std::vector<LabEvent> events;
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
  bool bool_value = false;
};

struct ParsedValues {
  std::string format_version;
  std::string pipeline_id;
  std::string message_id;
  std::vector<ParsedValue> fields;
};

struct StoredRun {
  std::string format_version;
  std::string command;
  std::string operation_kind;
  std::string operation_status;
  int exit_code = 0;
  std::string config_sha256;
  std::string pipeline_id;
  std::string message_id;
  std::string direction_id;
  std::string frame_hex;
  std::size_t frame_length = 0U;
  std::string frame_sha256;
  std::string tx_frame_hex;
  std::size_t tx_frame_length = 0U;
  std::string tx_frame_sha256;
  std::string tx_frame_file;
  std::string rx_frame_hex;
  std::size_t rx_frame_length = 0U;
  std::string rx_frame_sha256;
  std::string rx_frame_file;
  std::string fields_canonical;
  std::string diagnostic_id;
  std::string frame_file;
  std::string values_file;
  std::string deterministic_fingerprint;
  std::string replay_mode;
  std::string replay_subject;
  std::string current_execution_status;
  std::string current_execution_diagnostic_id;
  std::string comparison_status;
  std::string comparison_reason;
  std::optional<bool> comparison_equal;
  bool cross_config_replay = false;
  bool send_attempted = false;
  bool send_succeeded = false;
  bool response_received = false;
  bool response_decoded = false;
  std::string local_endpoint;
  std::string remote_endpoint;
  std::string received_from;
  std::string receive_pipeline_id;
  std::string transport;
  std::uint32_t timeout_ms = 0U;
  HistoricalTransportFacts historical_transport;
};

struct RecordedFile {
  std::string relative_path;
  std::string sha256;
  std::size_t size = 0U;
};

}  // namespace pae::protocol_lab
