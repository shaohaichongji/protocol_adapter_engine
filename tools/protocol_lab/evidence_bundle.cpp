#include "evidence_bundle.h"

#include <yyjson.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
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
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &time);
#else
  gmtime_r(&time, &utc);
#endif
  std::ostringstream output;
  output << std::put_time(&utc, "%Y%m%dT%H%M%SZ");
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
  std::ostringstream output;
  output << "{\n"
         << "  \"format_version\":" << Quoted(kRecordFormat) << ",\n"
         << "  \"run_id\":" << Quoted(run_id) << ",\n"
         << "  \"tool_version\":" << Quoted(kToolVersion) << ",\n"
         << "  \"pae_version\":\"0.1.0-internal\",\n"
         << "  \"source_revision\":\"NOT_EMBEDDED\",\n"
         << "  \"command\":" << Quoted(result.command) << ",\n"
         << "  \"started_utc\":" << Quoted(timestamp) << ",\n"
         << "  \"end_status\":" << Quoted(result.status) << ",\n"
         << "  \"transport\":\"OFFLINE\",\n"
         << "  \"frame_origin\":"
         << Quoted(result.command == "encode"
                       ? "PAE_GENERATED"
                       : (result.command == "replay" ? "REPLAYED" : "MANUAL_SYNTHETIC"))
         << ",\n"
         << "  \"peer_kind\":\"OFFLINE_FILE\",\n"
         << "  \"authority_level\":\"OPEN\",\n"
         << "  \"sensitivity\":\"USER_REVIEW_REQUIRED\",\n"
         << "  \"send_attempted\":false,\n"
         << "  \"limits\":{\"max_frame_bytes\":16777216,\"max_receive_frames\":0,"
            "\"timeout_ms\":0,\"max_send_count\":0,\"max_events\":256,"
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

std::string SerializeEvent(const OperationResult& result, std::string_view timestamp) {
  std::ostringstream output;
  output << "{\"format_version\":\"pae.lab.event/0.1\",\"event_id\":1,\"direction\":"
         << Quoted(result.command == "encode" ? "TX"
                                              : (result.command == "replay" ? "REPLAY" : "RX"))
         << ",\"frame_file\":\"frames/000001_frame.bin\",\"frame_length\":" << result.frame.size()
         << ",\"frame_sha256\":" << Quoted(HashBytes(result.frame)) << ",\"frame_origin\":"
         << Quoted(result.command == "encode"
                       ? "PAE_GENERATED"
                       : (result.command == "replay" ? "REPLAYED" : "MANUAL_SYNTHETIC"))
         << ",\"peer_kind\":\"OFFLINE_FILE\",\"pipeline_id\":" << Quoted(result.pipeline_id)
         << ",\"message_id\":" << Quoted(result.message_id)
         << ",\"direction_id\":" << Quoted(result.direction_id)
         << ",\"status\":" << Quoted(result.status)
         << ",\"fields\":" << SerializeFieldsCompact(result.fields)
         << ",\"diagnostic_id\":" << Quoted(result.diagnostic_id)
         << ",\"wall_clock_utc\":" << Quoted(timestamp) << ",\"monotonic_offset_ns\":0}\n";
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

bool VerifyBundleHashes(const std::filesystem::path& bundle, std::string& error) {
  std::string sums;
  if (!ReadText(bundle / "SHA256SUMS", sums, error)) {
    return false;
  }
  std::istringstream lines{sums};
  std::string line;
  std::string previous_path;
  bool saw_complete = false;
  bool saw_result = false;
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
    saw_result = saw_result || relative_text == "result_summary_v0.1.json";
    saw_config = saw_config || relative_text == "inputs/protocol.pae.json";
    saw_frame = saw_frame || relative_text == "frames/000001_frame.bin";
    ++entry_count;
  }
  if (entry_count == 0U || !saw_complete || !saw_result || !saw_config || !saw_frame) {
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

bool CreateEvidenceBundle(const std::filesystem::path& record_root, std::string_view config,
                          const std::optional<std::string>& values, OperationResult& result,
                          RecordFileSystem& file_system, std::string& error) {
  if (!file_system.EnsureDirectory(record_root, error)) {
    return false;
  }
  const std::string timestamp = UtcTimestamp();
  std::string run_id;
  std::filesystem::path in_progress;
  std::filesystem::path final;
  for (std::size_t attempt = 0U; attempt < 32U; ++attempt) {
    run_id = RandomRunId();
    const std::string base = "run_" + timestamp + "_" + run_id;
    final = record_root / base;
    in_progress = record_root / (base + ".inprogress");
    bool final_exists = false;
    bool in_progress_exists = false;
    if (!file_system.Exists(final, final_exists, error) ||
        !file_system.Exists(in_progress, in_progress_exists, error)) {
      return false;
    }
    if (!final_exists && !in_progress_exists) {
      break;
    }
    final.clear();
  }
  if (final.empty()) {
    error = "cannot allocate a unique Run directory";
    return false;
  }
  if (!file_system.CreateDirectory(in_progress, error)) {
    return false;
  }

  result.evidence_bundle = final.generic_u8string();
  std::vector<RecordedFile> files;
  const std::string frame_hex = HexUpper(result.frame) + "\n";
  const std::string result_json = SerializeResult(result);
  if (!WriteVerified(file_system, in_progress, "inputs/protocol.pae.json", config, files, error) ||
      (values.has_value() && !WriteVerified(file_system, in_progress, "inputs/values.pae-lab.json",
                                            *values, files, error)) ||
      !WriteVerified(file_system, in_progress, "frames/000001_frame.bin", result.frame.data(),
                     result.frame.size(), files, error) ||
      !WriteVerified(file_system, in_progress, "frames/000001_frame.hex", frame_hex, files,
                     error) ||
      !WriteVerified(file_system, in_progress, "result_summary_v0.1.json", result_json, files,
                     error) ||
      !WriteVerified(file_system, in_progress, "events_v0.1.jsonl",
                     SerializeEvent(result, timestamp), files, error)) {
    return false;
  }
  const std::string run_record = SerializeRunRecord(result, run_id, timestamp, files);
  if (!WriteVerified(file_system, in_progress, "run_record_v0.1.json", run_record, files, error) ||
      !WriteVerified(file_system, in_progress, "COMPLETE", std::string_view{}, files, error)) {
    return false;
  }
  std::sort(files.begin(), files.end(), [](const RecordedFile& left, const RecordedFile& right) {
    return left.relative_path < right.relative_path;
  });
  std::ostringstream hashes;
  for (const RecordedFile& file : files) {
    hashes << file.sha256 << "  " << file.relative_path << '\n';
  }
  if (!WriteVerified(file_system, in_progress, "SHA256SUMS", hashes.str(), files, error)) {
    return false;
  }
  if (!file_system.Rename(in_progress, final, error)) {
    return false;
  }
  return true;
}

bool LoadStoredRun(const std::filesystem::path& bundle, StoredRun& output, std::string& error) {
  if (!std::filesystem::is_regular_file(bundle / "COMPLETE")) {
    error = "Run has no COMPLETE marker";
    return false;
  }
  if (!VerifyBundleHashes(bundle, error)) {
    return false;
  }
  std::string text;
  if (!ReadText(bundle / "result_summary_v0.1.json", text, error)) {
    return false;
  }
  DocumentPtr document;
  if (!ParseDocument(text, document, error)) {
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> allowed{"format_version",
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
  const std::set<std::string_view> required = allowed;
  if (!ValidateObject(root, allowed, required, "result_summary", error)) {
    return false;
  }
  std::string format;
  if (!ReadJsonString(root, "format_version", format, error) || format != kResultFormat ||
      !ReadJsonString(root, "operation_kind", output.operation_kind, error) ||
      !ReadJsonString(root, "operation_status", output.operation_status, error) ||
      !ReadJsonString(root, "config_sha256", output.config_sha256, error) ||
      !ReadJsonString(root, "pipeline_id", output.pipeline_id, error) ||
      !ReadJsonString(root, "message_id", output.message_id, error) ||
      !ReadJsonString(root, "direction_id", output.direction_id, error) ||
      !ReadJsonString(root, "frame_hex", output.frame_hex, error) ||
      !ReadJsonString(root, "frame_file", output.frame_file, error) ||
      !ReadJsonString(root, "deterministic_fingerprint", output.deterministic_fingerprint, error)) {
    if (error.empty()) {
      error = "unsupported result format_version";
    }
    return false;
  }
  yyjson_val* fields = yyjson_obj_get(root, "fields");
  if (!yyjson_is_arr(fields)) {
    error = "fields must be an array";
    return false;
  }
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
    yyjson_val* known = yyjson_obj_get(field, "enum_known");
    if (!yyjson_is_bool(known)) {
      error = "stored field enum_known must be boolean";
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
  if (!IsSafeRelativePath(output.frame_file) ||
      (!output.values_file.empty() && !IsSafeRelativePath(output.values_file))) {
    error = "stored Run contains an unsafe relative path";
    return false;
  }
  return true;
}

}  // namespace pae::protocol_lab
