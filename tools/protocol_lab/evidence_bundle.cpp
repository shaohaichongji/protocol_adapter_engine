#include "evidence_bundle.h"

#include <yyjson.h>

#include <algorithm>
#include <chrono>
#include <climits>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <set>
#include <sstream>
#include <system_error>

#include "protocol_operations.h"
#include "result_format.h"
#include "sha256.h"

namespace pae::protocol_lab {
namespace {

std::string UtcTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t time = std::chrono::system_clock::to_time_t(now);
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &time);
#else
  gmtime_r(&time, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y%m%dT%H%M%S") << '.' << std::setfill('0') << std::setw(3)
         << milliseconds.count() << 'Z';
  return output.str();
}

std::string RandomRunId() {
  std::random_device random;
  std::uniform_int_distribution<std::uint32_t> distribution;
  std::ostringstream output;
  output << std::hex << std::setfill('0') << std::setw(8) << distribution(random) << std::setw(8)
         << distribution(random);
  return output.str();
}

std::string GenericPath(const std::filesystem::path& path) { return path.generic_string(); }

std::string_view EventFormatForSchema(std::string_view schema_version) {
  return schema_version == "0.4"   ? kEventFormatV5
         : schema_version == "0.3" ? kEventFormatV4
         : schema_version == "0.2" ? kEventFormatV3
                                   : kEventFormat;
}

std::string EventFileForResult(std::string_view result_format) {
  return result_format == kResultFormatV5   ? "events_v0.5.jsonl"
         : result_format == kResultFormatV4 ? "events_v0.4.jsonl"
         : result_format == kResultFormatV3 ? "events_v0.3.jsonl"
                                            : "events_v0.2.jsonl";
}

bool WriteVerified(RecordFileSystem& file_system, const std::filesystem::path& root,
                   const std::filesystem::path& relative, const std::uint8_t* data,
                   std::size_t size, std::vector<RecordedFile>& files, std::string& error) {
  const std::filesystem::path destination = root / relative;
  if (!file_system.CreateDirectories(destination.parent_path(), error)) {
    return false;
  }
  bool destination_exists = false;
  if (!file_system.Exists(destination, destination_exists, error) || destination_exists) {
    if (error.empty()) {
      error = "record destination already exists";
    }
    return false;
  }
  std::filesystem::path temporary = destination;
  temporary += ".tmp";
  if (!file_system.WriteClosedFile(temporary, data, size, error)) {
    return false;
  }
  std::vector<std::uint8_t> reviewed;
  if (!file_system.ReadFile(temporary, reviewed, error) || reviewed.size() != size ||
      !std::equal(reviewed.begin(), reviewed.end(), data)) {
    if (error.empty()) {
      error = "record file verification mismatch";
    }
    return false;
  }
  if (HashBytes(reviewed) != HashBytes(data, size)) {
    error = "record file hash verification mismatch";
    return false;
  }
  if (!file_system.Rename(temporary, destination, error)) {
    return false;
  }
  files.push_back(RecordedFile{GenericPath(relative), HashBytes(data, size), size});
  return true;
}

bool WriteVerified(RecordFileSystem& file_system, const std::filesystem::path& root,
                   const std::filesystem::path& relative, std::string_view text,
                   std::vector<RecordedFile>& files, std::string& error) {
  return WriteVerified(file_system, root, relative,
                       reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), files,
                       error);
}

std::string SerializeRunRecord(const OperationResult& result, std::string_view run_id,
                               std::string_view timestamp, const std::vector<RecordedFile>& files) {
  const std::string_view record_format =
      result.schema_version == "0.4" ? kRecordFormatV5
      : result.schema_version == "0.3"
          ? kRecordFormatV4
          : (result.schema_version == "0.2"
                 ? kRecordFormatV3
                 : (result.operation_kind == "udp-exchange" ? kRecordFormat : kRecordFormatV1));
  const std::string_view tool_version =
      record_format == kRecordFormatV1
          ? "0.1.0-udp-exchange-slice"
          : (record_format == kRecordFormatV5 ? kToolVersionV5
             : record_format == kRecordFormatV4
                 ? kToolVersionV4
                 : (record_format == kRecordFormatV3 ? kToolVersionV3 : kToolVersion));
  std::ostringstream output;
  output << "{\n"
         << "  \"format_version\":" << Quoted(record_format) << ",\n"
         << "  \"run_id\":" << Quoted(run_id) << ",\n"
         << "  \"tool_version\":" << Quoted(tool_version) << ",\n"
         << "  \"pae_version\":\"0.1.0-internal\",\n"
         << "  \"source_revision\":\"NOT_EMBEDDED\",\n"
         << "  \"command\":" << Quoted(result.command) << ",\n"
         << "  \"started_utc\":" << Quoted(timestamp) << ",\n"
         << "  \"end_status\":" << Quoted(result.status) << ",\n"
         << "  \"transport\":" << Quoted(result.transport) << ",\n"
         << "  \"local_endpoint\":" << Quoted(result.local_endpoint) << ",\n"
         << "  \"remote_endpoint\":" << Quoted(result.remote_endpoint) << ",\n"
         << "  \"received_from\":" << Quoted(result.received_from) << ",\n"
         << "  \"frame_origin\":"
         << Quoted(result.command == "udp-exchange"
                       ? (result.frame_file == "frames/000002_rx.bin" ? "MANUAL_SYNTHETIC"
                                                                      : "PAE_GENERATED")
                       : (result.command == "encode"
                              ? "PAE_GENERATED"
                              : (result.command == "replay" ? "REPLAYED" : "MANUAL_SYNTHETIC")))
         << ",\n"
         << "  \"peer_kind\":"
         << Quoted(result.command == "udp-exchange" ? "LAB_SIMULATED_PEER" : "OFFLINE_FILE")
         << ",\n"
         << "  \"authority_level\":\"OPEN\",\n"
         << "  \"sensitivity\":\"USER_REVIEW_REQUIRED\",\n"
         << "  \"send_attempted\":" << (result.send_attempted ? "true" : "false") << ",\n"
         << "  \"send_succeeded\":" << (result.send_succeeded ? "true" : "false") << ",\n"
         << "  \"response_received\":" << (result.response_received ? "true" : "false") << ",\n"
         << "  \"response_decoded\":" << (result.response_decoded ? "true" : "false");
  if (record_format == kRecordFormat || record_format == kRecordFormatV3 ||
      record_format == kRecordFormatV4 || record_format == kRecordFormatV5) {
    output << ",\n"
           << "  \"receive_pipeline_id\":" << Quoted(result.receive_pipeline_id) << ",\n"
           << "  \"replay_mode\":" << Quoted(result.replay_mode) << ",\n"
           << "  \"replay_subject\":" << Quoted(result.replay_subject) << ",\n"
           << "  \"current_execution_status\":" << Quoted(result.current_execution_status) << ",\n"
           << "  \"comparison_status\":" << Quoted(result.comparison_status);
  }
  output << ",\n"
         << "  \"limits\":{\"max_frame_bytes\":" << result.max_frame_bytes
         << ",\"max_receive_frames\":" << (result.command == "udp-exchange" ? 1 : 0) << ","
         << "\"timeout_ms\":" << result.timeout_ms
         << ",\"max_send_count\":" << (result.command == "udp-exchange" ? 1 : 0)
         << ",\"max_events\":256,"
            "\"max_raw_bytes\":16777216},\n"
         << "  \"hash_manifest\":\"SHA256SUMS\",\n"
         << "  \"hash_manifest_excludes_self\":true,\n"
         << "  \"recorded_payload_files\":[\n";
  for (std::size_t index = 0U; index < files.size(); ++index) {
    const RecordedFile& file = files[index];
    output << "    {\"path\":" << Quoted(file.relative_path) << ",\"size\":" << file.size
           << ",\"sha256\":" << Quoted(file.sha256) << "}"
           << (index + 1U == files.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
  return output.str();
}

std::string SerializeEvent(const LabEvent& event, std::string_view format) {
  std::ostringstream output;
  output << "{\"format_version\":" << Quoted(format) << ",\"event_id\":" << event.event_id
         << ",\"event_kind\":" << Quoted(event.event_kind)
         << ",\"direction\":" << Quoted(event.direction)
         << ",\"frame_file\":" << Quoted(event.frame_file)
         << ",\"frame_length\":" << event.frame_length
         << ",\"frame_sha256\":" << Quoted(event.frame_sha256)
         << ",\"frame_origin\":" << Quoted(event.frame_origin)
         << ",\"peer_kind\":" << Quoted(event.peer_kind)
         << ",\"remote_endpoint\":" << Quoted(event.remote_endpoint)
         << ",\"received_from\":" << Quoted(event.received_from)
         << ",\"succeeded\":" << (event.succeeded ? "true" : "false")
         << ",\"diagnostic_id\":" << Quoted(event.diagnostic_id)
         << ",\"wall_clock_utc\":" << Quoted(event.wall_clock_utc)
         << ",\"monotonic_offset_ns\":" << event.monotonic_offset_ns << "}\n";
  return output.str();
}

std::string SerializeEvents(const OperationResult& result) {
  if (result.operation_kind != "udp-exchange" && result.schema_version == "0.1") {
    const std::string direction =
        result.command == "encode" ? "TX" : (result.command == "replay" ? "REPLAY" : "RX");
    const std::string origin = result.command == "encode"
                                   ? "PAE_GENERATED"
                                   : (result.command == "replay" ? "REPLAYED" : "MANUAL_SYNTHETIC");
    std::ostringstream legacy;
    legacy << "{\"format_version\":\"pae.lab.event/0.1\",\"event_id\":1,"
              "\"event_kind\":\"FRAME\",\"direction\":"
           << Quoted(direction) << ",\"frame_file\":\"frames/000001_frame.bin\","
           << "\"frame_length\":" << result.frame.size()
           << ",\"frame_sha256\":" << Quoted(HashBytes(result.frame))
           << ",\"frame_origin\":" << Quoted(origin)
           << ",\"peer_kind\":\"OFFLINE_FILE\",\"pipeline_id\":" << Quoted(result.pipeline_id)
           << ",\"message_id\":" << Quoted(result.message_id)
           << ",\"direction_id\":" << Quoted(result.direction_id)
           << ",\"status\":" << Quoted(result.status)
           << ",\"fields\":" << SerializeFieldsCompact(result.fields)
           << ",\"diagnostic_id\":" << Quoted(result.diagnostic_id)
           << ",\"wall_clock_utc\":" << Quoted(UtcTimestamp()) << ",\"monotonic_offset_ns\":0}\n";
    return legacy.str();
  }
  std::ostringstream output;
  for (const LabEvent& event : result.events) {
    output << SerializeEvent(event, EventFormatForSchema(result.schema_version));
  }
  return output.str();
}

bool IsSafeRelativePath(const std::filesystem::path& path) {
  if (path.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) {
    return false;
  }
  for (const auto& component : path) {
    if (component == "..") {
      return false;
    }
  }
  return true;
}

bool ReadJsonBool(yyjson_val* object, const char* name, bool& output, std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, name);
  if (!yyjson_is_bool(value)) {
    error = std::string{name} + " must be boolean";
    return false;
  }
  output = yyjson_get_bool(value);
  return true;
}

bool ReadJsonSize(yyjson_val* object, const char* name, std::size_t& output, std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, name);
  if (!yyjson_is_uint(value) || yyjson_get_uint(value) > SIZE_MAX) {
    error = std::string{name} + " must be an in-range unsigned integer";
    return false;
  }
  output = static_cast<std::size_t>(yyjson_get_uint(value));
  return true;
}

bool ReadJsonNonnegativeInt(yyjson_val* object, const char* name, int& output, std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, name);
  if (!yyjson_is_uint(value) || yyjson_get_uint(value) > INT_MAX) {
    error = std::string{name} + " must be a nonnegative in-range integer";
    return false;
  }
  output = static_cast<int>(yyjson_get_uint(value));
  return true;
}

bool VerifyRxMetadata(const std::filesystem::path& bundle, const StoredRun& run, bool /*v3*/,
                      std::string& error) {
  const std::filesystem::path metadata_path = bundle / "frames/000002_rx.meta.json";
  if (!std::filesystem::is_regular_file(metadata_path)) {
    error = "V0.2 UDP Run with a received response has no RX metadata";
    return false;
  }
  std::string metadata_text;
  if (!ReadText(metadata_path, metadata_text, error)) {
    return false;
  }
  DocumentPtr metadata_document;
  if (!ParseDocument(metadata_text, metadata_document, error)) {
    return false;
  }
  yyjson_val* metadata = yyjson_doc_get_root(metadata_document.get());
  const std::set<std::string_view> keys{"format_version", "event_id",     "frame_file",
                                        "frame_length",   "frame_sha256", "received_from",
                                        "direction"};
  if (!ValidateObject(metadata, keys, keys, "rx_metadata", error)) {
    return false;
  }
  std::string format;
  std::string frame_file;
  std::string frame_sha256;
  std::string received_from;
  std::string direction;
  std::size_t frame_length = 0U;
  std::size_t event_id = 0U;
  if (!ReadJsonString(metadata, "format_version", format, error) ||
      !ReadJsonSize(metadata, "event_id", event_id, error) ||
      !ReadJsonString(metadata, "frame_file", frame_file, error) ||
      !ReadJsonSize(metadata, "frame_length", frame_length, error) ||
      !ReadJsonString(metadata, "frame_sha256", frame_sha256, error) ||
      !ReadJsonString(metadata, "received_from", received_from, error) ||
      !ReadJsonString(metadata, "direction", direction, error)) {
    return false;
  }
  std::vector<std::uint8_t> frame;
  if (format != "pae.lab.rx-meta/0.2" || event_id == 0U || frame_file != "frames/000002_rx.bin" ||
      direction != "RX" || received_from != run.received_from ||
      !ReadFile(bundle / frame_file, frame, error) || frame_length != frame.size() ||
      frame_sha256 != HashBytes(frame) || HexUpper(frame) != run.rx_frame_hex) {
    if (error.empty()) {
      error = "RX metadata does not match the recorded frame or source endpoint";
    }
    return false;
  }

  std::string events_text;
  if (!ReadText(bundle / EventFileForResult(run.format_version), events_text, error)) {
    return false;
  }
  std::istringstream event_lines{events_text};
  std::string event_text;
  bool matched = false;
  while (std::getline(event_lines, event_text)) {
    if (event_text.empty()) {
      continue;
    }
    DocumentPtr event_document;
    if (!ParseDocument(event_text, event_document, error)) {
      return false;
    }
    yyjson_val* event = yyjson_doc_get_root(event_document.get());
    yyjson_val* id = yyjson_obj_get(event, "event_id");
    if (!yyjson_is_uint(id) || yyjson_get_uint(id) != event_id) {
      continue;
    }
    std::string kind;
    std::string event_direction;
    std::string event_frame;
    std::string event_frame_sha256;
    std::string event_source;
    std::size_t event_frame_length = 0U;
    if (!ReadJsonString(event, "event_kind", kind, error) ||
        !ReadJsonString(event, "direction", event_direction, error) ||
        !ReadJsonString(event, "frame_file", event_frame, error) ||
        !ReadJsonSize(event, "frame_length", event_frame_length, error) ||
        !ReadJsonString(event, "frame_sha256", event_frame_sha256, error) ||
        !ReadJsonString(event, "received_from", event_source, error)) {
      return false;
    }
    matched = kind == "FRAME" && event_direction == "RX" && event_frame == frame_file &&
              event_frame_length == frame_length && event_frame_sha256 == frame_sha256 &&
              event_source == received_from;
    break;
  }
  if (!matched) {
    error = "RX metadata does not bind to the corresponding RX event";
  }
  return matched;
}

bool ReadEventV2(yyjson_val* event, LabEvent& output, std::string_view expected_format,
                 std::string& error) {
  const std::set<std::string_view> keys{
      "format_version", "event_id",     "event_kind",    "direction",      "frame_file",
      "frame_length",   "frame_sha256", "frame_origin",  "peer_kind",      "remote_endpoint",
      "received_from",  "succeeded",    "diagnostic_id", "wall_clock_utc", "monotonic_offset_ns"};
  if (!ValidateObject(event, keys, keys, "V0.2 event", error)) {
    return false;
  }
  std::string format;
  std::size_t offset = 0U;
  if (!ReadJsonString(event, "format_version", format, error) || format != expected_format ||
      !ReadJsonSize(event, "event_id", output.event_id, error) ||
      !ReadJsonString(event, "event_kind", output.event_kind, error) ||
      !ReadJsonString(event, "direction", output.direction, error) ||
      !ReadJsonString(event, "frame_file", output.frame_file, error) ||
      !ReadJsonSize(event, "frame_length", output.frame_length, error) ||
      !ReadJsonString(event, "frame_sha256", output.frame_sha256, error) ||
      !ReadJsonString(event, "frame_origin", output.frame_origin, error) ||
      !ReadJsonString(event, "peer_kind", output.peer_kind, error) ||
      !ReadJsonString(event, "remote_endpoint", output.remote_endpoint, error) ||
      !ReadJsonString(event, "received_from", output.received_from, error) ||
      !ReadJsonBool(event, "succeeded", output.succeeded, error) ||
      !ReadJsonString(event, "diagnostic_id", output.diagnostic_id, error) ||
      !ReadJsonString(event, "wall_clock_utc", output.wall_clock_utc, error) ||
      !ReadJsonSize(event, "monotonic_offset_ns", offset, error)) {
    if (error.empty()) {
      error = "unsupported V0.2 event format_version";
    }
    return false;
  }
  output.monotonic_offset_ns = static_cast<std::uint64_t>(offset);

  const bool empty_frame = output.frame_file.empty() && output.frame_length == 0U &&
                           output.frame_sha256.empty() && output.frame_origin.empty() &&
                           output.peer_kind.empty() && output.received_from.empty();
  if (output.event_kind == "FRAME") {
    const bool offline_rx = output.direction == "RX" && output.peer_kind == "OFFLINE_FILE";
    const bool received_from_valid =
        output.direction == "RX"
            ? (offline_rx ? output.received_from.empty() : !output.received_from.empty())
            : output.received_from.empty();
    if ((output.direction != "TX" && output.direction != "RX" && output.direction != "REPLAY") ||
        !IsSafeRelativePath(output.frame_file) || output.frame_sha256.size() != 64U ||
        output.frame_origin.empty() || output.peer_kind.empty() || !output.succeeded ||
        (output.direction != "REPLAY" && !output.diagnostic_id.empty()) || !received_from_valid ||
        !output.remote_endpoint.empty()) {
      error = "V0.2 FRAME event fields are inconsistent";
      return false;
    }
  } else if (output.event_kind == "SEND_INTENT") {
    if (output.direction != "TX" || !empty_frame || output.remote_endpoint.empty() ||
        !output.succeeded || !output.diagnostic_id.empty()) {
      error = "V0.2 SEND_INTENT event fields are inconsistent";
      return false;
    }
  } else if (output.event_kind == "SEND_RESULT") {
    if (output.direction != "TX" || !empty_frame || !output.remote_endpoint.empty() ||
        (output.succeeded ? !output.diagnostic_id.empty()
                          : output.diagnostic_id != "UDP_SEND_FAILED")) {
      error = "V0.2 SEND_RESULT event fields are inconsistent";
      return false;
    }
  } else {
    error = "V0.2 event has an unsupported event_kind";
    return false;
  }
  return true;
}

bool VerifyFrameShape(std::string_view label, std::string_view hex, std::size_t length,
                      std::string_view sha256, std::vector<std::uint8_t>& bytes,
                      std::string& error) {
  bytes.clear();
  const bool parsed = hex.empty() ? true : ParseFrameHex(hex, bytes, error);
  if (!parsed || HexUpper(bytes) != hex || bytes.size() != length || HashBytes(bytes) != sha256) {
    if (error.empty()) {
      error = std::string{label} + " summary length, Hash, or Hex is inconsistent";
    }
    return false;
  }
  return true;
}

bool EventBindsFrame(const std::filesystem::path& bundle, const LabEvent& event,
                     std::string_view expected_file, const std::vector<std::uint8_t>& expected,
                     std::string& error) {
  std::vector<std::uint8_t> recorded;
  if (event.frame_file != expected_file || event.frame_length != expected.size() ||
      event.frame_sha256 != HashBytes(expected) ||
      !ReadFile(bundle / event.frame_file, recorded, error) || recorded != expected) {
    if (error.empty()) {
      error = "V0.2 FRAME event does not bind to its summary and byte file";
    }
    return false;
  }
  return true;
}

bool VerifyEventLogV2(const std::filesystem::path& bundle, const StoredRun& run, bool v3,
                      std::string& error) {
  std::string events_text;
  const std::string events_file = EventFileForResult(run.format_version);
  const std::string_view expected_format = run.format_version == kResultFormatV5 ? kEventFormatV5
                                           : run.format_version == kResultFormatV4
                                               ? kEventFormatV4
                                               : (v3 ? kEventFormatV3 : kEventFormat);
  if (!ReadText(bundle / events_file, events_text, error)) {
    return false;
  }
  std::istringstream lines{events_text};
  std::string line;
  std::size_t expected_id = 1U;
  std::uint64_t previous_offset = 0U;
  std::vector<LabEvent> events;
  while (std::getline(lines, line)) {
    if (line.empty()) {
      error = "V0.2 event log contains an empty line";
      return false;
    }
    DocumentPtr document;
    if (!ParseDocument(line, document, error)) {
      return false;
    }
    yyjson_val* event = yyjson_doc_get_root(document.get());
    LabEvent parsed;
    if (!ReadEventV2(event, parsed, expected_format, error) || parsed.event_id != expected_id ||
        (expected_id > 1U && parsed.monotonic_offset_ns < previous_offset)) {
      if (error.empty()) {
        error = "V0.2 event ids, version, or monotonic offsets are invalid";
      }
      return false;
    }
    previous_offset = parsed.monotonic_offset_ns;
    events.push_back(std::move(parsed));
    ++expected_id;
    if (expected_id > 257U) {
      error = "V0.2 event log exceeds the configured event limit";
      return false;
    }
  }
  if (expected_id == 1U) {
    error = "V0.2 event log is empty";
    return false;
  }

  std::vector<std::uint8_t> frame;
  std::vector<std::uint8_t> tx;
  std::vector<std::uint8_t> rx;
  if (!VerifyFrameShape("frame", run.frame_hex, run.frame_length, run.frame_sha256, frame, error) ||
      !VerifyFrameShape("TX frame", run.tx_frame_hex, run.tx_frame_length, run.tx_frame_sha256, tx,
                        error) ||
      !VerifyFrameShape("RX frame", run.rx_frame_hex, run.rx_frame_length, run.rx_frame_sha256, rx,
                        error)) {
    return false;
  }

  std::size_t tx_frames = 0U;
  std::size_t rx_frames = 0U;
  std::size_t replay_frames = 0U;
  std::size_t intents = 0U;
  std::size_t send_results = 0U;
  std::size_t intent_index = 0U;
  std::size_t send_result_index = 0U;
  bool send_result_succeeded = false;
  for (std::size_t index = 0U; index < events.size(); ++index) {
    const LabEvent& event = events[index];
    if (event.event_kind == "SEND_INTENT") {
      ++intents;
      intent_index = index;
    } else if (event.event_kind == "SEND_RESULT") {
      ++send_results;
      send_result_index = index;
      send_result_succeeded = event.succeeded;
    } else if (event.direction == "TX") {
      ++tx_frames;
      if (!EventBindsFrame(bundle, event,
                           run.command == "udp-exchange" ? run.tx_frame_file : run.frame_file,
                           run.command == "udp-exchange" ? tx : frame, error)) {
        return false;
      }
    } else if (event.direction == "RX") {
      ++rx_frames;
      if (!EventBindsFrame(bundle, event,
                           run.command == "udp-exchange" ? run.rx_frame_file : run.frame_file,
                           run.command == "udp-exchange" ? rx : frame, error)) {
        return false;
      }
    } else {
      ++replay_frames;
      if (!EventBindsFrame(bundle, event, run.frame_file, frame, error)) {
        return false;
      }
    }
  }

  if (run.command == "udp-exchange") {
    const bool main_frame_bound =
        (run.frame_file == run.tx_frame_file && frame == tx) ||
        (!run.rx_frame_file.empty() && run.frame_file == run.rx_frame_file && frame == rx);
    if (tx_frames != 1U || replay_frames != 0U || run.tx_frame_file != "frames/000001_tx.bin" ||
        !main_frame_bound || run.send_succeeded > run.send_attempted ||
        (run.send_attempted
             ? (intents != 1U || send_results != 1U || intent_index >= send_result_index ||
                send_result_succeeded != run.send_succeeded)
             : (intents != 0U || send_results != 0U)) ||
        (run.response_received ? (rx_frames != 1U || run.rx_frame_file.empty())
                               : (rx_frames != 0U || !run.rx_frame_file.empty())) ||
        (run.response_decoded && (!run.response_received || run.replay_mode != "DECODE_RX" ||
                                  run.current_execution_status != "OK"))) {
      error = "V0.2 UDP source event sequence conflicts with the result summary";
      return false;
    }
  } else if (run.command == "replay") {
    const bool rx_subject = run.replay_subject == "RX" || run.replay_subject == "RX_INCOMPLETE";
    if (replay_frames != 1U || tx_frames != 0U || rx_frames != 0U || intents != 0U ||
        send_results != 0U || run.send_attempted || run.send_succeeded || run.response_received ||
        !run.tx_frame_file.empty() || !run.rx_frame_file.empty() ||
        (run.replay_mode == "ENCODE_TX" ? run.replay_subject != "TX" : !rx_subject) ||
        (run.operation_kind == "udp-exchange" && run.replay_mode != "ENCODE_TX" && frame != rx) ||
        !EventBindsFrame(bundle, events.front(), run.frame_file, frame, error)) {
      if (error.empty()) {
        error = "V0.2 Replay event sequence conflicts with the result summary";
      }
      return false;
    }
  } else if ((v3 || run.format_version == kResultFormatV4 ||
              run.format_version == kResultFormatV5) &&
             run.command == "encode") {
    if (tx_frames != 1U || rx_frames != 0U || replay_frames != 0U || intents != 0U ||
        send_results != 0U ||
        !EventBindsFrame(bundle, events.front(), run.frame_file, frame, error)) {
      if (error.empty()) error = "V0.3 Encode event does not bind its result frame";
      return false;
    }
  } else if ((v3 || run.format_version == kResultFormatV4 ||
              run.format_version == kResultFormatV5) &&
             run.command == "inspect") {
    if (rx_frames != 1U || tx_frames != 0U || replay_frames != 0U || intents != 0U ||
        send_results != 0U ||
        !EventBindsFrame(bundle, events.front(), run.frame_file, frame, error)) {
      if (error.empty()) error = "V0.3 Inspect event does not bind its input frame";
      return false;
    }
  } else {
    error = "V0.2 UDP evidence has an unsupported command";
    return false;
  }
  return true;
}

bool VerifyRunRecord(const std::filesystem::path& bundle, const StoredRun& run, int generation,
                     std::string& error) {
  const bool modern = generation >= 2;
  const std::filesystem::path record_path =
      bundle / (generation == 5   ? "run_record_v0.5.json"
                : generation == 4 ? "run_record_v0.4.json"
                                  : (generation == 3 ? "run_record_v0.3.json"
                                                     : (generation == 2 ? "run_record_v0.2.json"
                                                                        : "run_record_v0.1.json")));
  std::string text;
  if (!ReadText(record_path, text, error)) {
    return false;
  }
  DocumentPtr document;
  if (!ParseDocument(text, document, error)) {
    error = "Run Record is not valid JSON: " + error;
    return false;
  }
  yyjson_val* record = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> base_keys{"format_version",
                                             "run_id",
                                             "tool_version",
                                             "pae_version",
                                             "source_revision",
                                             "command",
                                             "started_utc",
                                             "end_status",
                                             "transport",
                                             "local_endpoint",
                                             "remote_endpoint",
                                             "received_from",
                                             "frame_origin",
                                             "peer_kind",
                                             "authority_level",
                                             "sensitivity",
                                             "send_attempted",
                                             "send_succeeded",
                                             "response_received",
                                             "response_decoded",
                                             "limits",
                                             "hash_manifest",
                                             "hash_manifest_excludes_self",
                                             "recorded_payload_files"};
  std::set<std::string_view> allowed = base_keys;
  if (modern) {
    allowed.insert("receive_pipeline_id");
    allowed.insert("replay_mode");
    allowed.insert("replay_subject");
    allowed.insert("current_execution_status");
    allowed.insert("comparison_status");
  }
  const std::set<std::string_view> required_v1{"format_version",
                                               "run_id",
                                               "tool_version",
                                               "pae_version",
                                               "source_revision",
                                               "command",
                                               "started_utc",
                                               "end_status",
                                               "transport",
                                               "frame_origin",
                                               "peer_kind",
                                               "authority_level",
                                               "sensitivity",
                                               "send_attempted",
                                               "limits",
                                               "hash_manifest",
                                               "hash_manifest_excludes_self",
                                               "recorded_payload_files"};
  if (!ValidateObject(record, allowed, modern ? allowed : required_v1, "Run Record", error)) {
    return false;
  }

  std::string format;
  std::string run_id;
  std::string ignored;
  std::string command;
  std::string end_status;
  std::string transport;
  std::string local_endpoint;
  std::string remote_endpoint;
  std::string received_from;
  std::string hash_manifest;
  bool send_attempted = false;
  bool send_succeeded = false;
  bool response_received = false;
  bool response_decoded = false;
  bool manifest_excludes_self = false;
  if (!ReadJsonString(record, "format_version", format, error) ||
      !ReadJsonString(record, "run_id", run_id, error) || run_id.empty() ||
      !ReadJsonString(record, "tool_version", ignored, error) ||
      !ReadJsonString(record, "pae_version", ignored, error) ||
      !ReadJsonString(record, "source_revision", ignored, error) ||
      !ReadJsonString(record, "command", command, error) ||
      !ReadJsonString(record, "started_utc", ignored, error) ||
      !ReadJsonString(record, "end_status", end_status, error) ||
      !ReadJsonString(record, "transport", transport, error) ||
      !ReadJsonString(record, "frame_origin", ignored, error) ||
      !ReadJsonString(record, "peer_kind", ignored, error) ||
      !ReadJsonString(record, "authority_level", ignored, error) ||
      !ReadJsonString(record, "sensitivity", ignored, error) ||
      !ReadJsonBool(record, "send_attempted", send_attempted, error) ||
      !ReadJsonString(record, "hash_manifest", hash_manifest, error) ||
      !ReadJsonBool(record, "hash_manifest_excludes_self", manifest_excludes_self, error)) {
    return false;
  }
  auto read_optional_string = [&](const char* name, std::string& value, bool& present) {
    present = yyjson_obj_get(record, name) != nullptr;
    return !present || ReadJsonString(record, name, value, error);
  };
  auto read_optional_bool = [&](const char* name, bool& value, bool& present) {
    present = yyjson_obj_get(record, name) != nullptr;
    return !present || ReadJsonBool(record, name, value, error);
  };
  bool has_local_endpoint = false;
  bool has_remote_endpoint = false;
  bool has_received_from = false;
  bool has_send_succeeded = false;
  bool has_response_received = false;
  bool has_response_decoded = false;
  if (!read_optional_string("local_endpoint", local_endpoint, has_local_endpoint) ||
      !read_optional_string("remote_endpoint", remote_endpoint, has_remote_endpoint) ||
      !read_optional_string("received_from", received_from, has_received_from) ||
      !read_optional_bool("send_succeeded", send_succeeded, has_send_succeeded) ||
      !read_optional_bool("response_received", response_received, has_response_received) ||
      !read_optional_bool("response_decoded", response_decoded, has_response_decoded)) {
    return false;
  }
  const std::string expected_format = generation == 5   ? std::string{kRecordFormatV5}
                                      : generation == 4 ? std::string{kRecordFormatV4}
                                      : generation == 3 ? std::string{kRecordFormatV3}
                                      : generation == 2 ? std::string{kRecordFormat}
                                                        : std::string{kRecordFormatV1};
  if (format != expected_format || hash_manifest != "SHA256SUMS" || !manifest_excludes_self) {
    error = "Run Record version or manifest contract is unsupported";
    return false;
  }

  std::string receive_pipeline;
  std::string replay_mode;
  std::string replay_subject;
  std::string current_status;
  std::string comparison_status;
  if (modern && (!ReadJsonString(record, "receive_pipeline_id", receive_pipeline, error) ||
                 !ReadJsonString(record, "replay_mode", replay_mode, error) ||
                 !ReadJsonString(record, "replay_subject", replay_subject, error) ||
                 !ReadJsonString(record, "current_execution_status", current_status, error) ||
                 !ReadJsonString(record, "comparison_status", comparison_status, error))) {
    return false;
  }
  if (command != run.command || end_status != run.operation_status ||
      ((modern || !run.transport.empty()) && transport != run.transport) ||
      send_attempted != run.send_attempted ||
      (has_local_endpoint && local_endpoint != run.local_endpoint) ||
      (has_remote_endpoint && remote_endpoint != run.remote_endpoint) ||
      (has_received_from && received_from != run.received_from) ||
      (has_send_succeeded && send_succeeded != run.send_succeeded) ||
      (has_response_received && response_received != run.response_received) ||
      (has_response_decoded && response_decoded != run.response_decoded) ||
      (modern &&
       (receive_pipeline != run.receive_pipeline_id || replay_mode != run.replay_mode ||
        replay_subject != run.replay_subject || current_status != run.current_execution_status ||
        comparison_status != run.comparison_status))) {
    error = "Run Record conflicts with the result summary";
    return false;
  }

  yyjson_val* limits = yyjson_obj_get(record, "limits");
  const std::set<std::string_view> limit_keys{"max_frame_bytes", "max_receive_frames",
                                              "timeout_ms",      "max_send_count",
                                              "max_events",      "max_raw_bytes"};
  if (!ValidateObject(limits, limit_keys, limit_keys, "Run Record limits", error)) {
    return false;
  }
  std::size_t ignored_limit = 0U;
  for (const char* name :
       {"max_frame_bytes", "max_receive_frames", "max_send_count", "max_events", "max_raw_bytes"}) {
    if (!ReadJsonSize(limits, name, ignored_limit, error)) {
      return false;
    }
  }
  std::size_t timeout_ms = 0U;
  if (!ReadJsonSize(limits, "timeout_ms", timeout_ms, error) || timeout_ms != run.timeout_ms) {
    if (error.empty()) {
      error = "Run Record timeout conflicts with the result summary";
    }
    return false;
  }

  yyjson_val* payloads = yyjson_obj_get(record, "recorded_payload_files");
  if (!yyjson_is_arr(payloads) || yyjson_arr_size(payloads) == 0U) {
    error = "Run Record recorded_payload_files must be a nonempty array";
    return false;
  }
  std::set<std::string> payload_paths;
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(payloads, &iterator);
  while (yyjson_val* payload = yyjson_arr_iter_next(&iterator)) {
    const std::set<std::string_view> payload_keys{"path", "size", "sha256"};
    if (!ValidateObject(payload, payload_keys, payload_keys, "Run Record payload", error)) {
      return false;
    }
    std::string path;
    std::string sha256;
    std::size_t size = 0U;
    if (!ReadJsonString(payload, "path", path, error) ||
        !ReadJsonSize(payload, "size", size, error) ||
        !ReadJsonString(payload, "sha256", sha256, error) || !IsSafeRelativePath(path) ||
        !payload_paths.insert(path).second) {
      if (error.empty()) {
        error = "Run Record payload path is unsafe or duplicated";
      }
      return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!ReadFile(bundle / path, bytes, error) || bytes.size() != size ||
        HashBytes(bytes) != sha256) {
      if (error.empty()) {
        error = "Run Record payload does not match the recorded file";
      }
      return false;
    }
  }
  const std::string result_name = generation == 5   ? "result_summary_v0.5.json"
                                  : generation == 4 ? "result_summary_v0.4.json"
                                  : generation == 3 ? "result_summary_v0.3.json"
                                  : generation == 2 ? "result_summary_v0.2.json"
                                                    : "result_summary_v0.1.json";
  const std::string events_name = generation == 5   ? "events_v0.5.jsonl"
                                  : generation == 4 ? "events_v0.4.jsonl"
                                  : generation == 3 ? "events_v0.3.jsonl"
                                  : generation == 2 ? "events_v0.2.jsonl"
                                                    : "events_v0.1.jsonl";
  auto requires_payload = [&](const std::string& path) {
    return path.empty() || payload_paths.find(path) != payload_paths.end();
  };
  auto hex_path = [](const std::string& binary) {
    std::filesystem::path path{binary};
    path.replace_extension(".hex");
    return path.generic_string();
  };
  if (payload_paths.find("inputs/protocol.pae.json") == payload_paths.end() ||
      payload_paths.find(result_name) == payload_paths.end() ||
      payload_paths.find(events_name) == payload_paths.end() ||
      !requires_payload(run.values_file) || !requires_payload(run.frame_file) ||
      !requires_payload(hex_path(run.frame_file)) || !requires_payload(run.tx_frame_file) ||
      (!run.tx_frame_file.empty() && !requires_payload(hex_path(run.tx_frame_file))) ||
      !requires_payload(run.rx_frame_file) ||
      (!run.rx_frame_file.empty() &&
       (!requires_payload(hex_path(run.rx_frame_file)) ||
        payload_paths.find("frames/000002_rx.meta.json") == payload_paths.end()))) {
    error = "Run Record omits a required recorded payload";
    return false;
  }
  std::error_code traversal_error;
  for (std::filesystem::recursive_directory_iterator file_iterator{bundle, traversal_error}, end;
       file_iterator != end && !traversal_error; file_iterator.increment(traversal_error)) {
    if (!file_iterator->is_regular_file(traversal_error)) {
      continue;
    }
    const std::string relative =
        std::filesystem::relative(file_iterator->path(), bundle, traversal_error).generic_string();
    if (relative != "SHA256SUMS" && relative != "COMPLETE" &&
        relative != record_path.filename().generic_string() &&
        payload_paths.find(relative) == payload_paths.end()) {
      error = "Run Record omits recorded file " + relative;
      return false;
    }
  }
  if (traversal_error) {
    error = "Run Record payload traversal failed: " + traversal_error.message();
    return false;
  }
  return true;
}

bool VerifyBundleHashes(const std::filesystem::path& bundle, std::string& error) {
  std::string sums;
  if (!ReadText(bundle / "SHA256SUMS", sums, error)) {
    return false;
  }
  std::istringstream lines{sums};
  std::string line;
  std::string previous_path;
  bool saw_complete = false;
  bool saw_result_v1 = false;
  bool saw_result_v2 = false;
  bool saw_result_v3 = false;
  bool saw_result_v4 = false;
  bool saw_result_v5 = false;
  bool saw_record_v1 = false;
  bool saw_record_v2 = false;
  bool saw_record_v3 = false;
  bool saw_record_v4 = false;
  bool saw_record_v5 = false;
  bool saw_events_v1 = false;
  bool saw_events_v2 = false;
  bool saw_events_v3 = false;
  bool saw_events_v4 = false;
  bool saw_events_v5 = false;
  bool saw_config = false;
  bool saw_frame = false;
  std::size_t entry_count = 0U;
  std::set<std::string> listed_paths;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      error = "SHA256SUMS contains an empty line";
      return false;
    }
    const std::size_t separator = line.find("  ");
    if (separator != 64U || line.size() <= separator + 2U) {
      error = "SHA256SUMS line has an invalid shape";
      return false;
    }
    const std::string expected = line.substr(0U, separator);
    if (!std::all_of(expected.begin(), expected.end(), [](char value) {
          return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
        })) {
      error = "SHA256SUMS digest must be lowercase hexadecimal";
      return false;
    }
    const std::string relative_text = line.substr(separator + 2U);
    const std::filesystem::path relative{relative_text};
    if (!IsSafeRelativePath(relative) || relative_text == "SHA256SUMS") {
      error = "SHA256SUMS contains an unsafe or self-referential path";
      return false;
    }
    if (!previous_path.empty() && previous_path >= relative_text) {
      error = "SHA256SUMS paths are not strictly sorted";
      return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!ReadFile(bundle / relative, bytes, error) || HashBytes(bytes) != expected) {
      if (error.empty()) {
        error = "SHA-256 mismatch for " + relative_text;
      }
      return false;
    }
    previous_path = relative_text;
    listed_paths.insert(relative_text);
    saw_complete = saw_complete || relative_text == "COMPLETE";
    saw_result_v1 = saw_result_v1 || relative_text == "result_summary_v0.1.json";
    saw_result_v2 = saw_result_v2 || relative_text == "result_summary_v0.2.json";
    saw_result_v3 = saw_result_v3 || relative_text == "result_summary_v0.3.json";
    saw_result_v4 = saw_result_v4 || relative_text == "result_summary_v0.4.json";
    saw_result_v5 = saw_result_v5 || relative_text == "result_summary_v0.5.json";
    saw_record_v1 = saw_record_v1 || relative_text == "run_record_v0.1.json";
    saw_record_v2 = saw_record_v2 || relative_text == "run_record_v0.2.json";
    saw_record_v3 = saw_record_v3 || relative_text == "run_record_v0.3.json";
    saw_record_v4 = saw_record_v4 || relative_text == "run_record_v0.4.json";
    saw_record_v5 = saw_record_v5 || relative_text == "run_record_v0.5.json";
    saw_events_v1 = saw_events_v1 || relative_text == "events_v0.1.jsonl";
    saw_events_v2 = saw_events_v2 || relative_text == "events_v0.2.jsonl";
    saw_events_v3 = saw_events_v3 || relative_text == "events_v0.3.jsonl";
    saw_events_v4 = saw_events_v4 || relative_text == "events_v0.4.jsonl";
    saw_events_v5 = saw_events_v5 || relative_text == "events_v0.5.jsonl";
    saw_config = saw_config || relative_text == "inputs/protocol.pae.json";
    saw_frame = saw_frame || relative_text == "frames/000001_frame.bin" ||
                relative_text == "frames/000001_tx.bin" || relative_text == "frames/000002_rx.bin";
    ++entry_count;
  }
  const bool complete_v1 = saw_result_v1 && saw_record_v1 && saw_events_v1 && !saw_result_v2 &&
                           !saw_record_v2 && !saw_events_v2 && !saw_result_v3 && !saw_record_v3 &&
                           !saw_events_v3 && !saw_result_v4 && !saw_record_v4 && !saw_events_v4 &&
                           !saw_result_v5 && !saw_record_v5 && !saw_events_v5;
  const bool complete_v2 = saw_result_v2 && saw_record_v2 && saw_events_v2 && !saw_result_v1 &&
                           !saw_record_v1 && !saw_events_v1 && !saw_result_v3 && !saw_record_v3 &&
                           !saw_events_v3 && !saw_result_v4 && !saw_record_v4 && !saw_events_v4 &&
                           !saw_result_v5 && !saw_record_v5 && !saw_events_v5;
  const bool complete_v3 = saw_result_v3 && saw_record_v3 && saw_events_v3 && !saw_result_v1 &&
                           !saw_record_v1 && !saw_events_v1 && !saw_result_v2 && !saw_record_v2 &&
                           !saw_events_v2 && !saw_result_v4 && !saw_record_v4 && !saw_events_v4 &&
                           !saw_result_v5 && !saw_record_v5 && !saw_events_v5;
  const bool complete_v4 = saw_result_v4 && saw_record_v4 && saw_events_v4 && !saw_result_v1 &&
                           !saw_record_v1 && !saw_events_v1 && !saw_result_v2 && !saw_record_v2 &&
                           !saw_events_v2 && !saw_result_v3 && !saw_record_v3 && !saw_events_v3 &&
                           !saw_result_v5 && !saw_record_v5 && !saw_events_v5;
  const bool complete_v5 = saw_result_v5 && saw_record_v5 && saw_events_v5 && !saw_result_v1 &&
                           !saw_record_v1 && !saw_events_v1 && !saw_result_v2 && !saw_record_v2 &&
                           !saw_events_v2 && !saw_result_v3 && !saw_record_v3 && !saw_events_v3 &&
                           !saw_result_v4 && !saw_record_v4 && !saw_events_v4;
  if (entry_count == 0U || !saw_complete || !saw_config || !saw_frame ||
      (!complete_v1 && !complete_v2 && !complete_v3 && !complete_v4 && !complete_v5)) {
    error = "SHA256SUMS omits a required Evidence Bundle file";
    return false;
  }
  std::error_code traversal_error;
  for (std::filesystem::recursive_directory_iterator iterator{bundle, traversal_error}, end;
       iterator != end && !traversal_error; iterator.increment(traversal_error)) {
    const auto status = iterator->symlink_status(traversal_error);
    if (traversal_error) {
      break;
    }
    if (std::filesystem::is_symlink(status)) {
      error = "Evidence Bundle must not contain symbolic links";
      return false;
    }
    if (!std::filesystem::is_regular_file(status)) {
      continue;
    }
    const std::filesystem::path relative =
        std::filesystem::relative(iterator->path(), bundle, traversal_error);
    if (traversal_error) {
      break;
    }
    const std::string relative_text = relative.generic_string();
    if (relative_text != "SHA256SUMS" && listed_paths.find(relative_text) == listed_paths.end()) {
      error = "Evidence Bundle contains an unlisted file: " + relative_text;
      return false;
    }
  }
  if (traversal_error) {
    error = "Evidence Bundle traversal failed: " + traversal_error.message();
    return false;
  }
  return true;
}

}  // namespace

bool StandardRecordFileSystem::EnsureDirectory(const std::filesystem::path& path,
                                               std::string& error) {
  std::error_code file_error;
  std::filesystem::create_directories(path, file_error);
  if (file_error || !std::filesystem::is_directory(path, file_error)) {
    error = "record root is not a writable directory";
    return false;
  }
  return true;
}

bool StandardRecordFileSystem::Exists(const std::filesystem::path& path, bool& exists,
                                      std::string& error) {
  std::error_code file_error;
  exists = std::filesystem::exists(path, file_error);
  if (file_error) {
    error = "cannot inspect record path: " + file_error.message();
    return false;
  }
  return true;
}

bool StandardRecordFileSystem::CreateDirectory(const std::filesystem::path& path,
                                               std::string& error) {
  std::error_code file_error;
  if (!std::filesystem::create_directory(path, file_error) || file_error) {
    error = "cannot create in-progress Run directory: " + file_error.message();
    return false;
  }
  return true;
}

bool StandardRecordFileSystem::CreateDirectories(const std::filesystem::path& path,
                                                 std::string& error) {
  if (path.empty()) {
    return true;
  }
  std::error_code file_error;
  std::filesystem::create_directories(path, file_error);
  if (file_error) {
    error = "cannot create record directory: " + file_error.message();
    return false;
  }
  return true;
}

bool StandardRecordFileSystem::WriteClosedFile(const std::filesystem::path& path,
                                               const std::uint8_t* data, std::size_t size,
                                               std::string& error) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "cannot create temporary record file";
    return false;
  }
  if (size != 0U) {
    stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  }
  stream.close();
  if (!stream) {
    error = "record file write or close failed";
    return false;
  }
  return true;
}

bool StandardRecordFileSystem::ReadFile(const std::filesystem::path& path,
                                        std::vector<std::uint8_t>& output, std::string& error) {
  return pae::protocol_lab::ReadFile(path, output, error);
}

bool StandardRecordFileSystem::Rename(const std::filesystem::path& from,
                                      const std::filesystem::path& to, std::string& error) {
  std::error_code file_error;
  std::filesystem::rename(from, to, file_error);
  if (file_error) {
    error = from.extension() == ".inprogress"
                ? "cannot publish completed Run directory: " + file_error.message()
                : "cannot publish record file: " + file_error.message();
    return false;
  }
  return true;
}

EvidenceBundleTransaction::EvidenceBundleTransaction(RecordFileSystem& file_system)
    : file_system_(file_system) {}

bool EvidenceBundleTransaction::Begin(const std::filesystem::path& record_root,
                                      std::string_view config,
                                      const std::optional<std::string>& values,
                                      OperationResult& result, std::string& error) {
  if (begun_ || completed_) {
    error = "Evidence Bundle transaction has already been used";
    return false;
  }
  if (!file_system_.EnsureDirectory(record_root, error)) {
    return false;
  }
  timestamp_ = UtcTimestamp();
  monotonic_start_ = std::chrono::steady_clock::now();
  for (std::size_t attempt = 0U; attempt < 32U; ++attempt) {
    run_id_ = RandomRunId();
    const std::string base = "run_" + timestamp_ + "_" + run_id_;
    final_ = record_root / base;
    in_progress_ = record_root / (base + ".inprogress");
    bool final_exists = false;
    bool in_progress_exists = false;
    if (!file_system_.Exists(final_, final_exists, error) ||
        !file_system_.Exists(in_progress_, in_progress_exists, error)) {
      return false;
    }
    if (!final_exists && !in_progress_exists) {
      break;
    }
    final_.clear();
    in_progress_.clear();
  }
  if (final_.empty()) {
    error = "cannot allocate a unique Run directory";
    return false;
  }
  if (!file_system_.CreateDirectory(in_progress_, error)) {
    return false;
  }

  result.evidence_bundle = final_.generic_u8string();
  begun_ = true;
  if (!WriteVerified(file_system_, in_progress_, "inputs/protocol.pae.json", config, files_,
                     error) ||
      (values.has_value() &&
       !WriteVerified(file_system_, in_progress_, "inputs/values.pae-lab.json", *values, files_,
                      error))) {
    return false;
  }
  return true;
}

bool EvidenceBundleTransaction::RecordFrame(const std::filesystem::path& relative_stem,
                                            const std::vector<std::uint8_t>& frame,
                                            std::string& error) {
  if (!begun_ || completed_ || relative_stem.empty() || relative_stem.extension() != "") {
    error = "invalid Evidence Bundle frame transaction state";
    return false;
  }
  std::filesystem::path binary = relative_stem;
  binary += ".bin";
  std::filesystem::path hex = relative_stem;
  hex += ".hex";
  const std::string frame_hex = HexUpper(frame) + "\n";
  return WriteVerified(file_system_, in_progress_, binary, frame.data(), frame.size(), files_,
                       error) &&
         WriteVerified(file_system_, in_progress_, hex, frame_hex, files_, error);
}

LabEvent EvidenceBundleTransaction::NewEvent(std::string_view event_kind,
                                             std::string_view direction) {
  LabEvent event;
  event.event_id = events_.size() + 1U;
  event.event_kind.assign(event_kind.data(), event_kind.size());
  event.direction.assign(direction.data(), direction.size());
  event.wall_clock_utc = UtcTimestamp();
  const auto elapsed = std::chrono::steady_clock::now() - monotonic_start_;
  event.monotonic_offset_ns = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
  return event;
}

void EvidenceBundleTransaction::CaptureTxFrame(const std::vector<std::uint8_t>& frame) {
  LabEvent event = NewEvent("FRAME", "TX");
  event.frame_file = "frames/000001_tx.bin";
  event.frame_length = frame.size();
  event.frame_sha256 = HashBytes(frame);
  event.frame_origin = "PAE_GENERATED";
  event.peer_kind = "LAB_SIMULATED_PEER";
  event.succeeded = true;
  events_.push_back(std::move(event));
}

void EvidenceBundleTransaction::CaptureOfflineFrame(const OperationResult& result) {
  const std::string direction =
      result.command == "encode" ? "TX" : (result.command == "replay" ? "REPLAY" : "RX");
  LabEvent event = NewEvent("FRAME", direction);
  event.frame_file = "frames/000001_frame.bin";
  event.frame_length = result.frame.size();
  event.frame_sha256 = HashBytes(result.frame);
  event.frame_origin = result.command == "encode"
                           ? "PAE_GENERATED"
                           : (result.command == "replay" ? "REPLAYED" : "MANUAL_SYNTHETIC");
  event.peer_kind = "OFFLINE_FILE";
  event.succeeded = true;
  if (result.command == "replay") {
    event.diagnostic_id = result.diagnostic_id;
  }
  events_.push_back(std::move(event));
}

void EvidenceBundleTransaction::CaptureSendIntent(std::string_view remote_endpoint) {
  LabEvent event = NewEvent("SEND_INTENT", "TX");
  event.remote_endpoint.assign(remote_endpoint.data(), remote_endpoint.size());
  event.succeeded = true;
  events_.push_back(std::move(event));
}

void EvidenceBundleTransaction::CaptureSendResult(bool succeeded, std::string_view diagnostic_id) {
  LabEvent event = NewEvent("SEND_RESULT", "TX");
  event.succeeded = succeeded;
  event.diagnostic_id.assign(diagnostic_id.data(), diagnostic_id.size());
  events_.push_back(std::move(event));
}

bool EvidenceBundleTransaction::RecordReceivedFrame(const std::filesystem::path& relative_stem,
                                                    const std::vector<std::uint8_t>& frame,
                                                    std::string_view received_from,
                                                    std::string& error) {
  if (!RecordFrame(relative_stem, frame, error)) {
    return false;
  }
  LabEvent event = NewEvent("FRAME", "RX");
  event.frame_file = (relative_stem.generic_string() + ".bin");
  event.frame_length = frame.size();
  event.frame_sha256 = HashBytes(frame);
  event.frame_origin = "MANUAL_SYNTHETIC";
  event.peer_kind = "LAB_SIMULATED_PEER";
  event.received_from.assign(received_from.data(), received_from.size());
  event.succeeded = true;

  std::ostringstream metadata;
  metadata << "{\n"
           << "  \"format_version\":\"pae.lab.rx-meta/0.2\",\n"
           << "  \"event_id\":" << event.event_id << ",\n"
           << "  \"frame_file\":" << Quoted(event.frame_file) << ",\n"
           << "  \"frame_length\":" << event.frame_length << ",\n"
           << "  \"frame_sha256\":" << Quoted(event.frame_sha256) << ",\n"
           << "  \"received_from\":" << Quoted(event.received_from) << ",\n"
           << "  \"direction\":\"RX\"\n"
           << "}\n";
  std::filesystem::path metadata_path = relative_stem;
  metadata_path += ".meta.json";
  if (!WriteVerified(file_system_, in_progress_, metadata_path, metadata.str(), files_, error)) {
    return false;
  }
  events_.push_back(std::move(event));
  return true;
}

bool EvidenceBundleTransaction::Complete(OperationResult& result, std::string& error) {
  if (!begun_ || completed_) {
    error = "invalid Evidence Bundle completion state";
    return false;
  }
  if (events_.empty() || events_.size() > 256U) {
    error = "Evidence Bundle event count is outside the configured limit";
    return false;
  }
  result.events = events_;
  const std::string result_json = SerializeResult(result);
  const bool v5 = result.schema_version == "0.4";
  const bool v4 = result.schema_version == "0.3";
  const bool v3 = result.schema_version == "0.2";
  const bool udp_v2 = !v5 && !v4 && !v3 && result.operation_kind == "udp-exchange";
  const std::filesystem::path result_name =
      v5   ? "result_summary_v0.5.json"
      : v4 ? "result_summary_v0.4.json"
           : (v3 ? "result_summary_v0.3.json"
                 : (udp_v2 ? "result_summary_v0.2.json" : "result_summary_v0.1.json"));
  const std::filesystem::path events_name =
      v5   ? "events_v0.5.jsonl"
      : v4 ? "events_v0.4.jsonl"
           : (v3 ? "events_v0.3.jsonl" : (udp_v2 ? "events_v0.2.jsonl" : "events_v0.1.jsonl"));
  const std::filesystem::path record_name =
      v5   ? "run_record_v0.5.json"
      : v4 ? "run_record_v0.4.json"
           : (v3 ? "run_record_v0.3.json"
                 : (udp_v2 ? "run_record_v0.2.json" : "run_record_v0.1.json"));
  if (!WriteVerified(file_system_, in_progress_, result_name, result_json, files_, error) ||
      !WriteVerified(file_system_, in_progress_, events_name, SerializeEvents(result), files_,
                     error)) {
    return false;
  }
  const std::string run_record = SerializeRunRecord(result, run_id_, timestamp_, files_);
  if (!WriteVerified(file_system_, in_progress_, record_name, run_record, files_, error) ||
      !WriteVerified(file_system_, in_progress_, "COMPLETE", std::string_view{}, files_, error)) {
    return false;
  }
  std::sort(files_.begin(), files_.end(), [](const RecordedFile& left, const RecordedFile& right) {
    return left.relative_path < right.relative_path;
  });
  std::ostringstream hashes;
  for (const RecordedFile& file : files_) {
    hashes << file.sha256 << "  " << file.relative_path << '\n';
  }
  if (!WriteVerified(file_system_, in_progress_, "SHA256SUMS", hashes.str(), files_, error)) {
    return false;
  }
  if (!file_system_.Rename(in_progress_, final_, error)) {
    return false;
  }
  completed_ = true;
  return true;
}

bool CreateEvidenceBundle(const std::filesystem::path& record_root, std::string_view config,
                          const std::optional<std::string>& values, OperationResult& result,
                          RecordFileSystem& file_system, std::string& error) {
  EvidenceBundleTransaction transaction{file_system};
  if (!transaction.Begin(record_root, config, values, result, error)) {
    return false;
  }
  result.frame_file = "frames/000001_frame.bin";
  return transaction.RecordFrame("frames/000001_frame", result.frame, error) &&
         (transaction.CaptureOfflineFrame(result), true) && transaction.Complete(result, error);
}

bool LoadStoredRun(const std::filesystem::path& bundle, StoredRun& output, std::string& error) {
  output = StoredRun{};
  if (!std::filesystem::is_regular_file(bundle / "COMPLETE")) {
    error = "Run has no COMPLETE marker";
    return false;
  }
  if (!VerifyBundleHashes(bundle, error)) {
    return false;
  }
  const bool has_v2 = std::filesystem::is_regular_file(bundle / "result_summary_v0.2.json");
  const bool has_v1 = std::filesystem::is_regular_file(bundle / "result_summary_v0.1.json");
  const bool has_v3 = std::filesystem::is_regular_file(bundle / "result_summary_v0.3.json");
  const bool has_v4 = std::filesystem::is_regular_file(bundle / "result_summary_v0.4.json");
  const bool has_v5 = std::filesystem::is_regular_file(bundle / "result_summary_v0.5.json");
  if (static_cast<unsigned>(has_v1) + static_cast<unsigned>(has_v2) +
          static_cast<unsigned>(has_v3) + static_cast<unsigned>(has_v4) +
          static_cast<unsigned>(has_v5) !=
      1U) {
    error = "Run must contain exactly one supported result summary";
    return false;
  }
  const std::filesystem::path result_path =
      bundle /
      (has_v5   ? "result_summary_v0.5.json"
       : has_v4 ? "result_summary_v0.4.json"
                : (has_v3 ? "result_summary_v0.3.json"
                          : (has_v2 ? "result_summary_v0.2.json" : "result_summary_v0.1.json")));
  const bool modern = has_v2 || has_v3 || has_v4 || has_v5;
  std::string text;
  if (!ReadText(result_path, text, error)) {
    return false;
  }
  DocumentPtr document;
  if (!ParseDocument(text, document, error)) {
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  std::string format;
  if (!ReadJsonString(root, "format_version", format, error) ||
      (format != kResultFormatV1 && format != kResultFormat && format != kResultFormatV3 &&
       format != kResultFormatV4 && format != kResultFormatV5)) {
    if (error.empty()) {
      error = "unsupported result format_version";
    }
    return false;
  }
  if ((format == kResultFormat) != has_v2 || (format == kResultFormatV3) != has_v3 ||
      (format == kResultFormatV4) != has_v4 || (format == kResultFormatV5) != has_v5) {
    error = "result filename and format_version do not agree";
    return false;
  }
  output.format_version = format;

  const std::set<std::string_view> allowed_v1{"format_version",
                                              "command",
                                              "operation_kind",
                                              "operation_status",
                                              "exit_code",
                                              "config_sha256",
                                              "protocol_id",
                                              "pipeline_id",
                                              "message_id",
                                              "direction_id",
                                              "frame_length",
                                              "frame_sha256",
                                              "frame_hex",
                                              "frame_file",
                                              "tx_frame_length",
                                              "tx_frame_sha256",
                                              "tx_frame_hex",
                                              "tx_frame_file",
                                              "rx_frame_length",
                                              "rx_frame_sha256",
                                              "rx_frame_hex",
                                              "rx_frame_file",
                                              "values_file",
                                              "fields",
                                              "diagnostic",
                                              "deterministic_fingerprint",
                                              "cross_config_replay",
                                              "comparison_equal",
                                              "comparison_categories",
                                              "evidence_bundle",
                                              "transport",
                                              "local_endpoint",
                                              "remote_endpoint",
                                              "received_from",
                                              "timeout_ms",
                                              "send_attempted",
                                              "send_succeeded",
                                              "response_received",
                                              "response_decoded",
                                              "gates"};
  std::set<std::string_view> allowed = allowed_v1;
  if (modern) {
    allowed.insert("replay_mode");
    allowed.insert("replay_subject");
    allowed.insert("current_execution_status");
    allowed.insert("current_execution_diagnostic_id");
    allowed.insert("comparison_status");
    allowed.insert("comparison_reason");
    allowed.insert("historical_transport");
    allowed.insert("receive_pipeline_id");
  }
  std::set<std::string_view> required{"format_version",
                                      "command",
                                      "operation_kind",
                                      "operation_status",
                                      "exit_code",
                                      "config_sha256",
                                      "protocol_id",
                                      "pipeline_id",
                                      "message_id",
                                      "direction_id",
                                      "frame_length",
                                      "frame_sha256",
                                      "frame_hex",
                                      "frame_file",
                                      "values_file",
                                      "fields",
                                      "diagnostic",
                                      "deterministic_fingerprint",
                                      "cross_config_replay",
                                      "comparison_equal",
                                      "comparison_categories",
                                      "evidence_bundle",
                                      "send_attempted",
                                      "send_succeeded",
                                      "gates"};
  if (modern) {
    required.insert("tx_frame_length");
    required.insert("tx_frame_sha256");
    required.insert("tx_frame_hex");
    required.insert("tx_frame_file");
    required.insert("rx_frame_length");
    required.insert("rx_frame_sha256");
    required.insert("rx_frame_hex");
    required.insert("rx_frame_file");
    required.insert("transport");
    required.insert("local_endpoint");
    required.insert("remote_endpoint");
    required.insert("received_from");
    required.insert("response_received");
    required.insert("response_decoded");
    required.insert("replay_mode");
    required.insert("replay_subject");
    required.insert("current_execution_status");
    required.insert("current_execution_diagnostic_id");
    required.insert("comparison_status");
    required.insert("comparison_reason");
    required.insert("historical_transport");
    required.insert("receive_pipeline_id");
  }
  if (!ValidateObject(root, allowed, required, "result_summary", error)) {
    return false;
  }
  if (!ReadJsonString(root, "command", output.command, error) ||
      !ReadJsonString(root, "operation_kind", output.operation_kind, error) ||
      !ReadJsonString(root, "operation_status", output.operation_status, error) ||
      !ReadJsonNonnegativeInt(root, "exit_code", output.exit_code, error) ||
      !ReadJsonString(root, "config_sha256", output.config_sha256, error) ||
      !ReadJsonString(root, "pipeline_id", output.pipeline_id, error) ||
      !ReadJsonString(root, "message_id", output.message_id, error) ||
      !ReadJsonString(root, "direction_id", output.direction_id, error) ||
      !ReadJsonSize(root, "frame_length", output.frame_length, error) ||
      !ReadJsonString(root, "frame_sha256", output.frame_sha256, error) ||
      !ReadJsonString(root, "frame_hex", output.frame_hex, error) ||
      !ReadJsonString(root, "frame_file", output.frame_file, error) ||
      !ReadJsonString(root, "deterministic_fingerprint", output.deterministic_fingerprint, error)) {
    if (error.empty()) {
      error = "unsupported result format_version";
    }
    return false;
  }
  auto read_nullable_string = [&](const char* name, std::string& destination) {
    yyjson_val* value = yyjson_obj_get(root, name);
    if (yyjson_is_null(value)) {
      destination.clear();
      return true;
    }
    return ReadJsonString(root, name, destination, error);
  };
  yyjson_val* tx_frame_hex = yyjson_obj_get(root, "tx_frame_hex");
  if (tx_frame_hex != nullptr) {
    if (!yyjson_is_str(tx_frame_hex)) {
      error = "tx_frame_hex must be a string";
      return false;
    }
    output.tx_frame_hex.assign(yyjson_get_str(tx_frame_hex), yyjson_get_len(tx_frame_hex));
  }
  if (modern && (!ReadJsonSize(root, "tx_frame_length", output.tx_frame_length, error) ||
                 !ReadJsonString(root, "tx_frame_sha256", output.tx_frame_sha256, error) ||
                 !read_nullable_string("tx_frame_file", output.tx_frame_file))) {
    return false;
  }
  yyjson_val* rx_frame_hex = yyjson_obj_get(root, "rx_frame_hex");
  if (rx_frame_hex != nullptr) {
    if (!yyjson_is_str(rx_frame_hex)) {
      error = "rx_frame_hex must be a string";
      return false;
    }
    output.rx_frame_hex.assign(yyjson_get_str(rx_frame_hex), yyjson_get_len(rx_frame_hex));
  }
  if (modern && (!ReadJsonSize(root, "rx_frame_length", output.rx_frame_length, error) ||
                 !ReadJsonString(root, "rx_frame_sha256", output.rx_frame_sha256, error) ||
                 !read_nullable_string("rx_frame_file", output.rx_frame_file))) {
    return false;
  }
  yyjson_val* fields = yyjson_obj_get(root, "fields");
  if (!yyjson_is_arr(fields)) {
    error = "fields must be an array";
    return false;
  }
  std::set<std::string> field_ids;
  yyjson_arr_iter field_iterator;
  yyjson_arr_iter_init(fields, &field_iterator);
  while (yyjson_val* field = yyjson_arr_iter_next(&field_iterator)) {
    const std::set<std::string_view> field_keys{"id", "kind", "raw_value", "logical_value",
                                                "enum_known"};
    if (!ValidateObject(field, field_keys, field_keys, "stored field", error)) {
      return false;
    }
    std::string id;
    std::string kind;
    std::string raw;
    std::string logical;
    if (!ReadJsonString(field, "id", id, error) || !ReadJsonString(field, "kind", kind, error) ||
        !ReadJsonString(field, "raw_value", raw, error) ||
        !ReadJsonString(field, "logical_value", logical, error)) {
      return false;
    }
    const bool stable_id = !id.empty() && id.size() <= 128U && id.front() >= 'a' &&
                           id.front() <= 'z' &&
                           std::all_of(id.begin(), id.end(), [](char character) {
                             return (character >= 'a' && character <= 'z') ||
                                    (character >= '0' && character <= '9') || character == '_';
                           });
    if (!stable_id || !field_ids.emplace(id).second) {
      error = "stored field id is invalid or duplicated";
      return false;
    }
    yyjson_val* known = yyjson_obj_get(field, "enum_known");
    if (!yyjson_is_bool(known)) {
      error = "stored field enum_known must be boolean";
      return false;
    }
    const bool enum_known = yyjson_get_bool(known);
    auto is_canonical_uint64 = [](std::string_view value) {
      if (value.empty() || (value.size() > 1U && value.front() == '0')) {
        return false;
      }
      std::uint64_t parsed = 0U;
      for (const char character : value) {
        if (character < '0' || character > '9') {
          return false;
        }
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (parsed > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10U) {
          return false;
        }
        parsed = parsed * 10U + digit;
      }
      return true;
    };
    auto is_upper_hex = [](std::string_view value) {
      return !value.empty() && value.size() % 2U == 0U &&
             std::all_of(value.begin(), value.end(), [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'A' && character <= 'F');
             });
    };
    auto is_canonical_int64 = [](std::string_view value) {
      if (value.empty() || value.front() == '+' || value == "-0") return false;
      const bool negative = value.front() == '-';
      const std::string_view digits = negative ? value.substr(1U) : value;
      if (digits.empty() || (digits.size() > 1U && digits.front() == '0')) return false;
      const std::uint64_t limit =
          negative ? std::uint64_t{1U} << 63U
                   : static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
      std::uint64_t magnitude = 0U;
      for (const char character : digits) {
        if (character < '0' || character > '9') return false;
        const std::uint64_t digit = static_cast<std::uint64_t>(character - '0');
        if (magnitude > (limit - digit) / 10U) return false;
        magnitude = magnitude * 10U + digit;
      }
      return true;
    };
    if (kind != "UINT64" && kind != "INT64" && kind != "BYTES" && kind != "ENUM" &&
        kind != "BOOL") {
      error = "stored field kind is unsupported";
      return false;
    }
    if (kind == "BOOL" && !has_v3 && !has_v4 && !has_v5) {
      error = "BOOL fields require result format 0.3, 0.4, or 0.5";
      return false;
    }
    if (kind == "INT64" && !has_v5) {
      error = "INT64 fields require result format 0.5";
      return false;
    }
    if (kind == "BOOL" &&
        ((raw != "0" && raw != "1") || (logical != "false" && logical != "true") ||
         ((raw == "1") != (logical == "true")) || enum_known)) {
      error = "stored BOOL field has an invalid raw/logical/enum_known tuple";
      return false;
    }
    if (kind == "UINT64" && (!is_canonical_uint64(raw) || logical != raw || enum_known)) {
      error = "stored UINT64 field has an invalid raw/logical/enum_known tuple";
      return false;
    }
    if (kind == "INT64" && (!is_canonical_int64(raw) || logical != raw || enum_known)) {
      error = "stored INT64 field has an invalid raw/logical/enum_known tuple";
      return false;
    }
    if (kind == "BYTES" && (!is_upper_hex(raw) || logical != raw || enum_known)) {
      error = "stored BYTES field has an invalid raw/logical/enum_known tuple";
      return false;
    }
    if (kind == "ENUM" &&
        (!is_canonical_uint64(raw) || (!enum_known && logical != raw) ||
         (enum_known && (logical.empty() || logical.size() > 128U || logical.front() < 'a' ||
                         logical.front() > 'z' ||
                         !std::all_of(logical.begin(), logical.end(), [](char character) {
                           return (character >= 'a' && character <= 'z') ||
                                  (character >= '0' && character <= '9') || character == '_';
                         }))))) {
      error = "stored ENUM field has an invalid raw/logical/enum_known tuple";
      return false;
    }
    output.fields_canonical += id + "|" + kind + "|" + raw + "|" + logical + "|" +
                               (yyjson_get_bool(known) ? "true" : "false") + "\n";
  }
  yyjson_val* diagnostic = yyjson_obj_get(root, "diagnostic");
  const std::set<std::string_view> diagnostic_keys{"id", "detail"};
  std::string ignored_detail;
  if (!ValidateObject(diagnostic, diagnostic_keys, diagnostic_keys, "diagnostic", error) ||
      !ReadJsonString(diagnostic, "id", output.diagnostic_id, error) ||
      !ReadJsonString(diagnostic, "detail", ignored_detail, error)) {
    return false;
  }
  yyjson_val* values = yyjson_obj_get(root, "values_file");
  if (yyjson_is_str(values)) {
    output.values_file.assign(yyjson_get_str(values), yyjson_get_len(values));
  } else if (!yyjson_is_null(values)) {
    error = "values_file must be string or null";
    return false;
  }

  auto read_optional_string = [&](const char* name, std::string& destination) {
    yyjson_val* value = yyjson_obj_get(root, name);
    if (value == nullptr) {
      return true;
    }
    return ReadJsonString(root, name, destination, error);
  };
  auto read_optional_bool = [&](const char* name, bool& destination) {
    yyjson_val* value = yyjson_obj_get(root, name);
    if (value == nullptr) {
      return true;
    }
    return ReadJsonBool(root, name, destination, error);
  };
  auto read_optional_u32 = [&](const char* name, std::uint32_t& destination) {
    yyjson_val* value = yyjson_obj_get(root, name);
    if (value == nullptr) {
      return true;
    }
    std::size_t parsed = 0U;
    if (!ReadJsonSize(root, name, parsed, error) || parsed > UINT32_MAX) {
      if (error.empty()) {
        error = std::string{name} + " exceeds uint32 range";
      }
      return false;
    }
    destination = static_cast<std::uint32_t>(parsed);
    return true;
  };
  if (!read_optional_string("transport", output.transport) ||
      !read_optional_u32("timeout_ms", output.timeout_ms) ||
      !read_optional_string("local_endpoint", output.local_endpoint) ||
      !read_optional_string("remote_endpoint", output.remote_endpoint) ||
      !read_optional_string("received_from", output.received_from) ||
      !read_optional_string("receive_pipeline_id", output.receive_pipeline_id) ||
      !read_optional_bool("cross_config_replay", output.cross_config_replay) ||
      !read_optional_bool("send_attempted", output.send_attempted) ||
      !read_optional_bool("send_succeeded", output.send_succeeded) ||
      !read_optional_bool("response_received", output.response_received) ||
      !read_optional_bool("response_decoded", output.response_decoded)) {
    return false;
  }
  if (modern &&
      (!ReadJsonString(root, "replay_mode", output.replay_mode, error) ||
       !ReadJsonString(root, "replay_subject", output.replay_subject, error) ||
       !ReadJsonString(root, "current_execution_status", output.current_execution_status, error) ||
       !ReadJsonString(root, "current_execution_diagnostic_id",
                       output.current_execution_diagnostic_id, error) ||
       !ReadJsonString(root, "comparison_status", output.comparison_status, error) ||
       !ReadJsonString(root, "comparison_reason", output.comparison_reason, error))) {
    return false;
  }
  yyjson_val* comparison_equal = yyjson_obj_get(root, "comparison_equal");
  if (yyjson_is_bool(comparison_equal)) {
    output.comparison_equal = yyjson_get_bool(comparison_equal);
  } else if (!yyjson_is_null(comparison_equal)) {
    error = "comparison_equal must be boolean or null";
    return false;
  }

  yyjson_val* historical = yyjson_obj_get(root, "historical_transport");
  if (modern && yyjson_is_obj(historical)) {
    const std::set<std::string_view> historical_keys{
        "transport",      "timeout_ms",        "status",          "diagnostic_id",
        "local_endpoint", "remote_endpoint",   "received_from",   "send_attempted",
        "send_succeeded", "response_received", "response_decoded"};
    if (!ValidateObject(historical, historical_keys, historical_keys, "historical_transport",
                        error)) {
      return false;
    }
    output.historical_transport.present = true;
    std::size_t historical_timeout_ms = 0U;
    if (!ReadJsonString(historical, "transport", output.historical_transport.transport, error) ||
        !ReadJsonSize(historical, "timeout_ms", historical_timeout_ms, error) ||
        historical_timeout_ms > UINT32_MAX ||
        !ReadJsonString(historical, "status", output.historical_transport.status, error) ||
        !ReadJsonString(historical, "diagnostic_id", output.historical_transport.diagnostic_id,
                        error) ||
        !ReadJsonString(historical, "local_endpoint", output.historical_transport.local_endpoint,
                        error) ||
        !ReadJsonString(historical, "remote_endpoint", output.historical_transport.remote_endpoint,
                        error) ||
        !ReadJsonString(historical, "received_from", output.historical_transport.received_from,
                        error) ||
        !ReadJsonBool(historical, "send_attempted", output.historical_transport.send_attempted,
                      error) ||
        !ReadJsonBool(historical, "send_succeeded", output.historical_transport.send_succeeded,
                      error) ||
        !ReadJsonBool(historical, "response_received",
                      output.historical_transport.response_received, error) ||
        !ReadJsonBool(historical, "response_decoded", output.historical_transport.response_decoded,
                      error)) {
      return false;
    }
    output.historical_transport.timeout_ms = static_cast<std::uint32_t>(historical_timeout_ms);
  } else if (modern && !yyjson_is_null(historical)) {
    error = "historical_transport must be an object or null";
    return false;
  }
  if (modern && output.operation_kind == "udp-exchange") {
    if (!output.historical_transport.present ||
        (output.historical_transport.send_succeeded &&
         !output.historical_transport.send_attempted) ||
        (output.historical_transport.response_decoded &&
         !output.historical_transport.response_received)) {
      error = "V0.2 UDP historical_transport is missing or internally inconsistent";
      return false;
    }
    if ((output.replay_mode == "ENCODE_TX" && output.replay_subject != "TX") ||
        (output.replay_mode == "DECODE_RX" && output.replay_subject != "RX") ||
        (output.replay_mode == "NO_CODEC_REEXECUTION" && output.replay_subject != "RX" &&
         output.replay_subject != "RX_INCOMPLETE")) {
      error = "V0.2 UDP replay mode and subject are inconsistent";
      return false;
    }
    if (output.command == "replay" &&
        (output.send_attempted || output.send_succeeded || output.response_received ||
         (output.response_decoded !=
          (output.replay_mode == "DECODE_RX" && output.current_execution_status == "OK")) ||
         (output.replay_mode == "NO_CODEC_REEXECUTION" &&
          (output.current_execution_status != "NOT_EVALUATED" ||
           output.comparison_status != "NOT_EVALUATED" || output.comparison_equal.has_value())))) {
      error = "V0.2 Replay current execution state is inconsistent";
      return false;
    }
  }
  if (!IsSafeRelativePath(output.frame_file) ||
      (!output.values_file.empty() && !IsSafeRelativePath(output.values_file)) ||
      (!output.tx_frame_file.empty() && !IsSafeRelativePath(output.tx_frame_file)) ||
      (!output.rx_frame_file.empty() && !IsSafeRelativePath(output.rx_frame_file))) {
    error = "stored Run contains an unsafe relative path";
    return false;
  }
  if (modern && output.command == "udp-exchange" && output.response_received &&
      !std::filesystem::is_regular_file(bundle / "frames/000002_rx.meta.json")) {
    error = "V0.2 UDP Run with a received response has no RX metadata";
    return false;
  }
  const int generation = has_v5 ? 5 : (has_v4 ? 4 : (has_v3 ? 3 : (has_v2 ? 2 : 1)));
  if (!VerifyRunRecord(bundle, output, generation, error)) {
    return false;
  }
  if (modern && output.command == "udp-exchange") {
    if (output.replay_mode != "ENCODE_TX" && output.replay_mode != "DECODE_RX" &&
        output.replay_mode != "NO_CODEC_REEXECUTION") {
      error = "V0.2 UDP Run has an invalid replay_mode";
      return false;
    }
    if (!VerifyEventLogV2(bundle, output, has_v3, error) ||
        (output.response_received && !VerifyRxMetadata(bundle, output, has_v3, error))) {
      return false;
    }
  } else if (modern && !VerifyEventLogV2(bundle, output, has_v3, error)) {
    return false;
  }
  return true;
}

}  // namespace pae::protocol_lab
