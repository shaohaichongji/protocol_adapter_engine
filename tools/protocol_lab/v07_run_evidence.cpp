#include "v07_run_evidence.h"

#include <yyjson.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <set>
#include <sstream>
#include <system_error>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef CreateDirectory
#endif

#include "sha256.h"

namespace pae::protocol_lab::v07 {
namespace {

constexpr std::string_view kResultFile = "result_summary_v0.6.json";
constexpr std::string_view kRecordFile = "run_record_v0.7.json";
constexpr std::string_view kEventFile = "events_v0.7.jsonl";
constexpr std::string_view kConfigFile = "inputs/protocol.pae.json";
constexpr std::string_view kValuesFile = "inputs/values.pae-lab.json";
constexpr std::string_view kFrameFile = "frames/000001_frame.bin";
constexpr std::string_view kParentRecordFile = "history/parent_record_v0.7.json";
constexpr std::string_view kHistoricalResultFile = "history/result_summary_v0.6.json";
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
constexpr std::string_view kCrcResultFile = "result_summary_v0.7.json";
constexpr std::string_view kCrcRecordFile = "run_record_v0.8.json";
constexpr std::string_view kCrcParentRecordFile = "history/parent_record_v0.8.json";
constexpr std::string_view kCrcHistoricalResultFile = "history/result_summary_v0.7.json";
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
constexpr std::string_view kLengthResultFile = "result_summary_v0.8.json";
constexpr std::string_view kLengthRecordFile = "run_record_v0.9.json";
constexpr std::string_view kLengthParentRecordFile = "history/parent_record_v0.9.json";
constexpr std::string_view kLengthHistoricalResultFile = "history/result_summary_v0.8.json";
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
constexpr std::string_view kVariableResultFile = "result_summary_v0.9.json";
constexpr std::string_view kVariableRecordFile = "run_record_v0.10.json";
constexpr std::string_view kVariableParentRecordFile = "history/parent_record_v0.10.json";
constexpr std::string_view kVariableHistoricalResultFile = "history/result_summary_v0.9.json";
#endif

bool IsCrcGeneration(const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  return record.format_version == kCrcRecordFormat;
#else
  static_cast<void>(record);
  return false;
#endif
}

bool IsLengthGeneration(const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  return record.format_version == kLengthRecordFormat;
#else
  static_cast<void>(record);
  return false;
#endif
}

bool IsVariableGeneration(const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  return record.format_version == kVariableRecordFormat;
#else
  static_cast<void>(record);
  return false;
#endif
}

std::string_view ResultFileFor([[maybe_unused]] const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (IsVariableGeneration(record)) return kVariableResultFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (IsLengthGeneration(record)) return kLengthResultFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (IsCrcGeneration(record)) return kCrcResultFile;
#endif
  return kResultFile;
}

std::string_view RecordFileFor([[maybe_unused]] const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (IsVariableGeneration(record)) return kVariableRecordFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (IsLengthGeneration(record)) return kLengthRecordFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (IsCrcGeneration(record)) return kCrcRecordFile;
#endif
  return kRecordFile;
}

std::string_view ParentRecordFileFor([[maybe_unused]] const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (IsVariableGeneration(record)) return kVariableParentRecordFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (IsLengthGeneration(record)) return kLengthParentRecordFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (IsCrcGeneration(record)) return kCrcParentRecordFile;
#endif
  return kParentRecordFile;
}

std::string_view HistoricalResultFileFor([[maybe_unused]] const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (IsVariableGeneration(record)) return kVariableHistoricalResultFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (IsLengthGeneration(record)) return kLengthHistoricalResultFile;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (IsCrcGeneration(record)) return kCrcHistoricalResultFile;
#endif
  return kHistoricalResultFile;
}

std::string_view FingerprintDomainFor([[maybe_unused]] const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (IsVariableGeneration(record)) return v06::kVariableFingerprintDomain;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (IsLengthGeneration(record)) return v06::kLengthFingerprintDomain;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (IsCrcGeneration(record)) return v06::kCrcFingerprintDomain;
#endif
  return v06::kFingerprintDomain;
}

std::string_view ResultFormatFor([[maybe_unused]] const RunRecord& record) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  if (IsVariableGeneration(record)) return v06::kVariableResultFormat;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (IsLengthGeneration(record)) return v06::kLengthResultFormat;
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  if (IsCrcGeneration(record)) return v06::kCrcResultFormat;
#endif
  return v06::kResultFormat;
}

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }
};
using DocumentPtr = std::unique_ptr<yyjson_doc, DocumentDeleter>;

std::string Quote(std::string_view text) {
  std::ostringstream output;
  output << '"';
  for (const unsigned char value : text) {
    switch (value) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (value < 0x20U) {
          constexpr char kHex[] = "0123456789ABCDEF";
          output << "\\u00" << kHex[value >> 4U] << kHex[value & 0x0FU];
        } else {
          output << static_cast<char>(value);
        }
    }
  }
  output << '"';
  return output.str();
}

std::string Nullable(const std::optional<std::string>& value) {
  return value.has_value() ? Quote(*value) : "null";
}

std::string HexUpper(const std::vector<std::uint8_t>& bytes) {
  constexpr char kDigits[] = "0123456789ABCDEF";
  std::string output(bytes.size() * 2U, '0');
  for (std::size_t index = 0U; index < bytes.size(); ++index) {
    output[index * 2U] = kDigits[bytes[index] >> 4U];
    output[index * 2U + 1U] = kDigits[bytes[index] & 0x0FU];
  }
  return output;
}

bool IsLowerHash(std::string_view value) {
  return value.size() == 64U && std::all_of(value.begin(), value.end(), [](char character) {
           return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
         });
}

bool SafeRelativePath(std::string_view text) {
  if (text.empty() || text.find('\\') != std::string_view::npos ||
      text.find(':') != std::string_view::npos)
    return false;
  const std::filesystem::path path{text};
  if (path.is_absolute() || path.has_root_name() || path.has_root_directory() ||
      path.generic_string() != text)
    return false;
  for (const auto& component : path) {
    if (component == "." || component == ".." || component.empty()) return false;
  }
  return true;
}

bool IsLinkOrReparsePoint(const std::filesystem::path& path, std::error_code& error) {
  const std::filesystem::file_status status = std::filesystem::symlink_status(path, error);
  if (error || std::filesystem::is_symlink(status)) return !error;
#if defined(_WIN32)
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    error = std::error_code{static_cast<int>(GetLastError()), std::system_category()};
    return false;
  }
  return (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0U;
#else
  return false;
#endif
}

bool PreflightBundleTree(const std::filesystem::path& bundle, std::set<std::string>& files,
                         std::string& error) {
  std::error_code file_error;
  if (IsLinkOrReparsePoint(bundle, file_error)) {
    error = "Run Bundle path is a symlink or reparse point";
    return false;
  }
  if (file_error || !std::filesystem::is_directory(bundle, file_error) || file_error) {
    error = "Run Bundle path is not a regular directory";
    return false;
  }
  for (std::filesystem::recursive_directory_iterator iterator{bundle, file_error}, end;
       iterator != end && !file_error; iterator.increment(file_error)) {
    if (IsLinkOrReparsePoint(iterator->path(), file_error)) {
      error = "Run Bundle contains a symlink or reparse point";
      return false;
    }
    if (file_error) break;
    const std::string relative =
        std::filesystem::relative(iterator->path(), bundle, file_error).generic_string();
    if (file_error || !SafeRelativePath(relative)) {
      error = "Run Bundle contains an unsafe file path";
      return false;
    }
    const auto status = iterator->status(file_error);
    if (file_error) break;
    if (std::filesystem::is_regular_file(status)) {
      files.insert(relative);
    } else if (!std::filesystem::is_directory(status)) {
      error = "Run Bundle contains an unsupported filesystem object";
      return false;
    }
  }
  if (file_error) {
    error = "cannot enumerate Run Bundle";
    return false;
  }
  return true;
}

bool ReadText(v06::EvidenceFileSystem& file_system, const std::filesystem::path& path,
              std::string& output, std::string& error) {
  std::vector<std::uint8_t> bytes;
  if (!file_system.ReadFile(path, bytes, error)) return false;
  output.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return true;
}

bool WriteVerified(v06::EvidenceFileSystem& file_system, const std::filesystem::path& root,
                   std::string_view relative, const std::uint8_t* data, std::size_t size,
                   std::vector<FileDescriptor>& files, std::string& error) {
  const std::filesystem::path target = root / std::filesystem::path{relative};
  if (!file_system.CreateDirectories(target.parent_path(), error) ||
      !file_system.WriteClosedFile(target, data, size, error))
    return false;
  std::vector<std::uint8_t> checked;
  if (!file_system.ReadFile(target, checked, error)) return false;
  if (checked.size() != size || !std::equal(checked.begin(), checked.end(), data)) {
    error = "closed Run Evidence file reread does not match written bytes";
    return false;
  }
  files.push_back({std::string{relative}, static_cast<std::uint64_t>(size), HashBytes(data, size)});
  return true;
}

bool WriteVerified(v06::EvidenceFileSystem& file_system, const std::filesystem::path& root,
                   std::string_view relative, std::string_view text,
                   std::vector<FileDescriptor>& files, std::string& error) {
  return WriteVerified(file_system, root, relative,
                       reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), files,
                       error);
}

std::string SerializeEvents(std::string_view run_id, const std::vector<StageEvent>& events) {
  std::ostringstream output;
  for (const auto& event : events) {
    output << "{\"format_version\":" << Quote(kEventFormat) << ",\"run_id\":" << Quote(run_id)
           << ",\"event_id\":" << event.event_id << ",\"offset_us\":" << event.offset_us
           << ",\"event_kind\":" << Quote(event.event_kind) << ",\"phase\":" << Quote(event.phase)
           << ",\"status\":" << Nullable(event.status) << "}\n";
  }
  return output.str();
}

std::string SerializePreparation(const PreparationFacts& facts) {
  std::ostringstream output;
  output << "{\"status\":" << Quote(facts.status)
         << ",\"diagnostic_id\":" << Nullable(facts.diagnostic_id)
         << ",\"detail\":" << Quote(facts.detail) << ",\"value_index\":";
  if (facts.value_index.has_value())
    output << *facts.value_index;
  else
    output << "null";
  output << '}';
  return output.str();
}

std::string SerializeExecution(const ExecutionFacts& facts) {
  std::ostringstream output;
  output << "{\"terminal_stage\":" << Quote(facts.terminal_stage)
         << ",\"terminal_status\":" << Quote(facts.terminal_status)
         << ",\"terminal_reason\":" << Quote(facts.terminal_reason)
         << ",\"preparation\":" << SerializePreparation(facts.preparation)
         << ",\"structural_query\":";
  if (facts.structural_query.has_value()) {
    output << "{\"status\":" << Quote(facts.structural_query->status)
           << ",\"candidate_class\":" << Quote(facts.structural_query->candidate_class) << '}';
  } else {
    output << "null";
  }
  output << ",\"main_codec\":";
  if (facts.main_codec.has_value()) {
    output << "{\"status\":" << Quote(facts.main_codec->status)
           << ",\"kind\":" << Nullable(facts.main_codec->kind) << '}';
  } else {
    output << "null";
  }
  output << ",\"review_decode\":";
  if (facts.review_decode.has_value()) {
    output << "{\"status\":" << Quote(facts.review_decode->status) << ",\"kind\":null}";
  } else {
    output << "null";
  }
  output << ",\"result_mapping\":";
  if (facts.result_mapping.has_value())
    output << "{\"status\":" << Quote(facts.result_mapping->status) << '}';
  else
    output << "null";
  output << ",\"counts\":{\"structural_query_calls\":" << facts.counts.structural_query_calls
         << ",\"encode_calls\":" << facts.counts.encode_calls
         << ",\"decode_calls\":" << facts.counts.decode_calls
         << ",\"review_decode_calls\":" << facts.counts.review_decode_calls << "}}";
  return output.str();
}

std::string SerializeRecord(const RunRecord& record) {
  std::ostringstream output;
  output << "{\"format_version\":" << Quote(record.format_version)
         << ",\"run_id\":" << Quote(record.run_id)
         << ",\"tool_version\":" << Quote(record.tool_version)
         << ",\"evidence_origin\":\"LAB_C_EXECUTION\",\"operation_kind\":"
         << Quote(record.operation_kind) << ",\"invocation_kind\":" << Quote(record.invocation_kind)
         << ",\"parent_run_id\":" << Nullable(record.parent_run_id)
         << ",\"config_file\":" << Quote(kConfigFile)
         << ",\"values_file\":" << Nullable(record.values_file)
         << ",\"frame_file\":" << Nullable(record.frame_file)
         << ",\"tx_frame_file\":null,\"rx_frame_file\":null,\"result_file\":"
         << Nullable(record.result_file) << ",\"event_file\":" << Quote(kEventFile)
         << ",\"deterministic_fingerprint\":" << Nullable(record.deterministic_fingerprint)
         << ",\"requested_pipeline_id\":" << Nullable(record.requested_pipeline_id)
         << ",\"execution\":" << SerializeExecution(record.execution) << ",\"comparison\":";
  if (record.comparison.has_value()) {
    output << "{\"status\":" << Quote(record.comparison->status)
           << ",\"reason\":" << Quote(record.comparison->reason) << '}';
  } else {
    output << "null";
  }
  output << ",\"historical_baseline\":";
  if (record.historical_baseline.has_value()) {
    const auto& history = *record.historical_baseline;
    output << "{\"parent_record_file\":" << Quote(history.parent_record_file)
           << ",\"parent_record_sha256\":" << Quote(history.parent_record_sha256)
           << ",\"result_file\":" << Quote(history.result_file)
           << ",\"result_sha256\":" << Quote(history.result_sha256)
           << ",\"fingerprint_domain\":" << Quote(history.fingerprint_domain)
           << ",\"deterministic_fingerprint\":" << Quote(history.deterministic_fingerprint) << '}';
  } else {
    output << "null";
  }
  output << ",\"hash_manifest\":"
            "\"SHA256SUMS\",\"hash_manifest_excludes_self\":true,"
            "\"recorded_payload_files\":[";
  for (std::size_t index = 0U; index < record.recorded_payload_files.size(); ++index) {
    if (index != 0U) output << ',';
    const auto& file = record.recorded_payload_files[index];
    output << "{\"path\":" << Quote(file.path) << ",\"size\":" << file.size
           << ",\"sha256\":" << Quote(file.sha256) << '}';
  }
  output << "]}\n";
  return output.str();
}

bool IsSafeRunName(std::string_view name) {
  return SafeRelativePath(name) && name.find('/') == std::string_view::npos &&
         name.rfind("run_", 0U) == 0U;
}

bool ValidateInputAssociations(const RunBundleInput& input, std::string& error) {
  const bool encode = input.record.operation_kind == "encode";
  if (!encode && input.record.operation_kind != "inspect") {
    error = "Run Record operation_kind is unsupported";
    return false;
  }
  if (encode != input.values_text.has_value() || (!encode && !input.frame.has_value())) {
    error = "Run input roles do not match operation_kind";
    return false;
  }
  if (input.record.values_file != (input.values_text.has_value()
                                       ? std::optional<std::string>{std::string{kValuesFile}}
                                       : std::nullopt) ||
      input.record.frame_file != (input.frame.has_value()
                                      ? std::optional<std::string>{std::string{kFrameFile}}
                                      : std::nullopt) ||
      input.record.result_file !=
          (input.result.has_value()
               ? std::optional<std::string>{std::string{ResultFileFor(input.record)}}
               : std::nullopt)) {
    error = "Run Record file roles do not match supplied payloads";
    return false;
  }
  if (input.result.has_value()) {
    if (input.result->format_version != ResultFormatFor(input.record)) {
      error = "Run Record and Result generations do not match";
      return false;
    }
    std::string format_error;
    const std::string fingerprint = v06::FinalizeFingerprint(*input.result, format_error);
    if (!format_error.empty() || input.record.deterministic_fingerprint != fingerprint) {
      error = "Run Record fingerprint does not bind Result 0.6";
      return false;
    }
    if (!input.result->config_sha256.has_value() ||
        *input.result->config_sha256 != HashBytes(input.config_text) ||
        input.result->operation_kind != input.record.operation_kind) {
      error = "Result 0.6 does not bind the Run operation and configuration";
      return false;
    }
    const std::optional<std::string> expected_frame =
        input.frame.has_value() ? std::optional<std::string>{HexUpper(*input.frame)} : std::nullopt;
    if (input.result->frame_hex != expected_frame) {
      error = "Result 0.6 frame_hex does not bind the main Frame";
      return false;
    }
  } else if (input.record.deterministic_fingerprint.has_value()) {
    error = "Run without Result must not have an execution fingerprint";
    return false;
  }
  const bool replay = input.record.invocation_kind == "REPLAY";
  if ((!replay && input.record.invocation_kind != "RUN") ||
      replay != input.record.parent_run_id.has_value() ||
      replay != input.record.comparison.has_value() ||
      replay != input.record.historical_baseline.has_value() ||
      replay != input.parent_record_text.has_value() ||
      replay != input.historical_result_text.has_value() ||
      (encode && input.record.requested_pipeline_id.has_value()) ||
      (replay && !encode && !input.record.requested_pipeline_id.has_value())) {
    error = "Run invocation and history roles are inconsistent";
    return false;
  }
  if (replay) {
    const auto& history = *input.record.historical_baseline;
    if (history.parent_record_file != ParentRecordFileFor(input.record) ||
        history.result_file != HistoricalResultFileFor(input.record) ||
        history.fingerprint_domain != FingerprintDomainFor(input.record) ||
        history.parent_record_sha256 != HashBytes(*input.parent_record_text) ||
        history.result_sha256 != HashBytes(*input.historical_result_text)) {
      error = "Replay historical baseline does not bind supplied snapshots";
      return false;
    }
  }
  return true;
}

}  // namespace

bool WriteRunBundle(const std::filesystem::path& record_root, const RunBundleInput& input,
                    v06::EvidenceFileSystem& file_system, std::filesystem::path& published_path,
                    std::string& error) {
  published_path.clear();
  error.clear();
  if (!IsSafeRunName(input.record.run_id) || !ValidateInputAssociations(input, error)) return false;
  if (!file_system.EnsureDirectory(record_root, error)) return false;
  const std::filesystem::path final_path = record_root / input.record.run_id;
  std::filesystem::path progress_path = final_path;
  progress_path += ".inprogress";
  bool final_exists = false;
  bool progress_exists = false;
  if (!file_system.Exists(final_path, final_exists, error) ||
      !file_system.Exists(progress_path, progress_exists, error))
    return false;
  if (final_exists || progress_exists) {
    error = "Run Bundle destination already exists";
    return false;
  }
  if (!file_system.CreateDirectory(progress_path, error)) return false;

  RunRecord record = input.record;
  const std::string_view result_file = ResultFileFor(record);
  const std::string_view record_file = RecordFileFor(record);
  const std::string_view parent_record_file = ParentRecordFileFor(record);
  const std::string_view historical_result_file = HistoricalResultFileFor(record);
  record.recorded_payload_files.clear();
  if (!WriteVerified(file_system, progress_path, kConfigFile, input.config_text,
                     record.recorded_payload_files, error) ||
      (input.values_text.has_value() &&
       !WriteVerified(file_system, progress_path, kValuesFile, *input.values_text,
                      record.recorded_payload_files, error)) ||
      (input.frame.has_value() &&
       !WriteVerified(file_system, progress_path, kFrameFile, input.frame->data(),
                      input.frame->size(), record.recorded_payload_files, error)))
    return false;

  if (input.result.has_value()) {
    std::string result_error;
    const std::string result_text = v06::SerializeResult(*input.result, result_error);
    if (!result_error.empty() ||
        !WriteVerified(file_system, progress_path, result_file, result_text,
                       record.recorded_payload_files, error)) {
      if (error.empty()) error = result_error;
      return false;
    }
  }
  if (input.parent_record_text.has_value() &&
      !WriteVerified(file_system, progress_path, parent_record_file, *input.parent_record_text,
                     record.recorded_payload_files, error))
    return false;
  if (input.historical_result_text.has_value() &&
      !WriteVerified(file_system, progress_path, historical_result_file,
                     *input.historical_result_text, record.recorded_payload_files, error))
    return false;
  const std::string events = SerializeEvents(record.run_id, input.events);
  if (!WriteVerified(file_system, progress_path, kEventFile, events, record.recorded_payload_files,
                     error))
    return false;
  std::sort(record.recorded_payload_files.begin(), record.recorded_payload_files.end(),
            [](const FileDescriptor& left, const FileDescriptor& right) {
              return left.path < right.path;
            });
  std::vector<FileDescriptor> manifest_files = record.recorded_payload_files;
  const std::string record_text = SerializeRecord(record);
  if (!WriteVerified(file_system, progress_path, record_file, record_text, manifest_files, error) ||
      !WriteVerified(file_system, progress_path, "COMPLETE", std::string_view{}, manifest_files,
                     error))
    return false;
  std::sort(manifest_files.begin(), manifest_files.end(),
            [](const FileDescriptor& left, const FileDescriptor& right) {
              return left.path < right.path;
            });
  std::ostringstream manifest;
  for (const auto& file : manifest_files) manifest << file.sha256 << "  " << file.path << '\n';
  std::vector<FileDescriptor> ignored;
  if (!WriteVerified(file_system, progress_path, "SHA256SUMS", manifest.str(), ignored, error))
    return false;

  StoredRunBundle reviewed;
  if (!LoadRunBundleForTest(progress_path, reviewed, file_system, error)) {
    error = "completed Run Bundle preflight failed: " + error;
    return false;
  }
  if (!file_system.Rename(progress_path, final_path, error)) return false;
  published_path = final_path;
  return true;
}

namespace {

bool IsObject(yyjson_val* value, const std::set<std::string_view>& allowed,
              const std::set<std::string_view>& required, std::string_view where,
              std::string& error) {
  if (!yyjson_is_obj(value)) {
    error = std::string{where} + " must be an object";
    return false;
  }
  for (const auto key : required) {
    if (yyjson_obj_getn(value, key.data(), key.size()) == nullptr) {
      error = std::string{where} + " is missing property " + std::string{key};
      return false;
    }
  }
  std::set<std::string> seen;
  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(value, &iterator);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string_view name{yyjson_get_str(key), yyjson_get_len(key)};
    if (!seen.insert(std::string{name}).second) {
      error = std::string{where} + " contains duplicate property " + std::string{name};
      return false;
    }
    if (allowed.find(name) == allowed.end()) {
      error = std::string{where} + " contains unknown property " + std::string{name};
      return false;
    }
  }
  return true;
}

bool ReadString(yyjson_val* object, const char* key, std::string& output, std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (!yyjson_is_str(value)) {
    error = std::string{"Run Record property "} + key + " must be a string";
    return false;
  }
  output.assign(yyjson_get_str(value), yyjson_get_len(value));
  return true;
}

bool ReadNullableString(yyjson_val* object, const char* key, std::optional<std::string>& output,
                        std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (yyjson_is_null(value)) {
    output.reset();
    return true;
  }
  if (!yyjson_is_str(value)) {
    error = std::string{"Run Record property "} + key + " must be a string or null";
    return false;
  }
  output = std::string{yyjson_get_str(value), yyjson_get_len(value)};
  return true;
}

bool ReadUint(yyjson_val* object, const char* key, std::uint64_t& output, std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, key);
  if (!yyjson_is_uint(value)) {
    error =
        std::string{"Run Record property "} + key + " must be an unsigned integer without exponent";
    return false;
  }
  output = yyjson_get_uint(value);
  return true;
}

bool ParsePreparation(yyjson_val* value, PreparationFacts& output, std::string& error) {
  const std::set<std::string_view> properties{"status", "diagnostic_id", "detail", "value_index"};
  if (!IsObject(value, properties, properties, "Run Record preparation", error) ||
      !ReadString(value, "status", output.status, error) ||
      !ReadNullableString(value, "diagnostic_id", output.diagnostic_id, error) ||
      !ReadString(value, "detail", output.detail, error))
    return false;
  yyjson_val* index = yyjson_obj_get(value, "value_index");
  if (yyjson_is_null(index)) {
    output.value_index.reset();
  } else if (yyjson_is_uint(index)) {
    output.value_index = yyjson_get_uint(index);
  } else {
    error = "Run Record preparation.value_index must be an unsigned integer or null";
    return false;
  }
  static const std::set<std::string> diagnostics{
      "PAE_LAB_C1_CONFIG_INVALID",        "PAE_LAB_C1_SCHEMA_UNSUPPORTED",
      "PAE_LAB_C1_VALUES_INVALID",        "PAE_LAB_C1_UNKNOWN_PIPELINE",
      "PAE_LAB_C1_UNKNOWN_MESSAGE",       "PAE_LAB_C1_MESSAGE_NOT_ALLOWED",
      "PAE_LAB_C1_UNKNOWN_FIELD",         "PAE_LAB_C1_UNKNOWN_ENUM_ENTRY",
      "PAE_LAB_C1_UNSUPPORTED_VALUE_KIND"};
  if (output.status == "OK") {
    if (output.diagnostic_id.has_value() || !output.detail.empty() ||
        output.value_index.has_value()) {
      error = "successful preparation must not contain a diagnostic";
      return false;
    }
  } else if (output.status == "FAILED") {
    if (!output.diagnostic_id.has_value() ||
        diagnostics.find(*output.diagnostic_id) == diagnostics.end() || output.detail.empty()) {
      error = "failed preparation has an unsupported or incomplete diagnostic";
      return false;
    }
    const bool indexed = *output.diagnostic_id == "PAE_LAB_C1_UNKNOWN_FIELD" ||
                         *output.diagnostic_id == "PAE_LAB_C1_UNKNOWN_ENUM_ENTRY" ||
                         *output.diagnostic_id == "PAE_LAB_C1_UNSUPPORTED_VALUE_KIND";
    if (indexed != output.value_index.has_value()) {
      error = "preparation diagnostic value_index association is invalid";
      return false;
    }
  } else {
    error = "Run Record preparation status is unsupported";
    return false;
  }
  return true;
}

bool ParseStructural(yyjson_val* value, std::optional<StructuralQueryFacts>& output,
                     std::string& error) {
  if (yyjson_is_null(value)) {
    output.reset();
    return true;
  }
  const std::set<std::string_view> properties{"status", "candidate_class"};
  StructuralQueryFacts facts;
  if (!IsObject(value, properties, properties, "Run Record structural_query", error) ||
      !ReadString(value, "status", facts.status, error) ||
      !ReadString(value, "candidate_class", facts.candidate_class, error))
    return false;
  const bool valid = (facts.status == "OK" && facts.candidate_class == "ONE") ||
                     (facts.status == "UNKNOWN_MESSAGE" && facts.candidate_class == "ZERO") ||
                     (facts.status == "AMBIGUOUS_MESSAGE" && facts.candidate_class == "MULTIPLE") ||
                     ((facts.status == "INVALID_ARGUMENT" || facts.status == "INVALID_PLAN") &&
                      facts.candidate_class == "UNDETERMINED");
  if (!valid) {
    error = "Run Record structural status and candidate_class are inconsistent";
    return false;
  }
  output = std::move(facts);
  return true;
}

const std::set<std::string>& CommonCodecStatuses() {
  static const std::set<std::string> statuses{"OK",
                                              "INVALID_ARGUMENT",
                                              "INVALID_PLAN",
                                              "WORKSPACE_PLAN_MISMATCH",
                                              "WORKSPACE_BUSY",
                                              "INPUT_OUTPUT_OVERLAP",
                                              "VALUE_NOT_REPRESENTABLE",
                                              "INTERNAL_ERROR"};
  return statuses;
}

bool IsCodecStatus(std::string_view status, bool encode) {
  if (CommonCodecStatuses().find(std::string{status}) != CommonCodecStatuses().end()) return true;
  static const std::set<std::string> decode{"UNKNOWN_MESSAGE",
                                            "AMBIGUOUS_MESSAGE",
                                            "OUTPUT_SLOTS_TOO_SMALL",
                                            "INTEGRITY_FAILED",
                                            "UNKNOWN_ENUM_VALUE"
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                                            ,
                                            "LENGTH_MISMATCH"
#endif
  };
  static const std::set<std::string> encode_only{"MESSAGE_NOT_ALLOWED",
                                                 "FIELD_REFERENCE_MISMATCH",
                                                 "DUPLICATE_FIELD",
                                                 "MISSING_FIELD",
                                                 "TYPE_MISMATCH",
                                                 "BYTES_LENGTH_MISMATCH",
                                                 "ENUM_REFERENCE_MISMATCH",
                                                 "CONSTANT_FIELD_OVERRIDE",
                                                 "BUFFER_TOO_SMALL",
                                                 "FINAL_REVIEW_FAILED"
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                                                 ,
                                                 "COMPUTED_FIELD_OVERRIDE"
#endif
  };
  const auto& set = encode ? encode_only : decode;
  return set.find(std::string{status}) != set.end();
}

bool ParseCodec(yyjson_val* value, bool main, std::string_view operation,
                std::optional<CodecFacts>& output, std::string& error) {
  if (yyjson_is_null(value)) {
    output.reset();
    return true;
  }
  const std::set<std::string_view> properties{"status", "kind"};
  CodecFacts facts;
  if (!IsObject(value, properties, properties,
                main ? "Run Record main_codec" : "Run Record review_decode", error) ||
      !ReadString(value, "status", facts.status, error) ||
      !ReadNullableString(value, "kind", facts.kind, error))
    return false;
  const bool encode = main && operation == "encode";
  if ((main && facts.kind != (encode ? std::optional<std::string>{"ENCODE"}
                                     : std::optional<std::string>{"DECODE"})) ||
      (!main && facts.kind.has_value()) || !IsCodecStatus(facts.status, encode)) {
    error = "Run Record Codec kind or status is unsupported";
    return false;
  }
  output = std::move(facts);
  return true;
}

bool ParseExecution(yyjson_val* value, std::string_view operation, ExecutionFacts& output,
                    std::string& error) {
  const std::set<std::string_view> properties{
      "terminal_stage", "terminal_status", "terminal_reason", "preparation", "structural_query",
      "main_codec",     "review_decode",   "result_mapping",  "counts"};
  if (!IsObject(value, properties, properties, "Run Record execution", error) ||
      !ReadString(value, "terminal_stage", output.terminal_stage, error) ||
      !ReadString(value, "terminal_status", output.terminal_status, error) ||
      !ReadString(value, "terminal_reason", output.terminal_reason, error) ||
      !ParsePreparation(yyjson_obj_get(value, "preparation"), output.preparation, error) ||
      !ParseStructural(yyjson_obj_get(value, "structural_query"), output.structural_query, error) ||
      !ParseCodec(yyjson_obj_get(value, "main_codec"), true, operation, output.main_codec, error) ||
      !ParseCodec(yyjson_obj_get(value, "review_decode"), false, operation, output.review_decode,
                  error))
    return false;
  yyjson_val* mapping = yyjson_obj_get(value, "result_mapping");
  if (yyjson_is_null(mapping)) {
    output.result_mapping.reset();
  } else {
    const std::set<std::string_view> mapping_properties{"status"};
    ResultMappingFacts facts;
    if (!IsObject(mapping, mapping_properties, mapping_properties, "Run Record result_mapping",
                  error) ||
        !ReadString(mapping, "status", facts.status, error) ||
        (facts.status != "OK" && facts.status != "FAILED")) {
      if (error.empty()) error = "Run Record result_mapping status is unsupported";
      return false;
    }
    output.result_mapping = std::move(facts);
  }
  yyjson_val* counts = yyjson_obj_get(value, "counts");
  const std::set<std::string_view> count_properties{"structural_query_calls", "encode_calls",
                                                    "decode_calls", "review_decode_calls"};
  if (!IsObject(counts, count_properties, count_properties, "Run Record counts", error) ||
      !ReadUint(counts, "structural_query_calls", output.counts.structural_query_calls, error) ||
      !ReadUint(counts, "encode_calls", output.counts.encode_calls, error) ||
      !ReadUint(counts, "decode_calls", output.counts.decode_calls, error) ||
      !ReadUint(counts, "review_decode_calls", output.counts.review_decode_calls, error))
    return false;
  if (output.counts.encode_calls > 1U || output.counts.decode_calls > 1U ||
      output.counts.review_decode_calls > 1U) {
    error = "Run Record Codec counts exceed the C1 bound";
    return false;
  }
  return true;
}

bool ReadFixedString(yyjson_val* root, const char* key, std::string_view expected,
                     std::string& error) {
  std::string value;
  if (!ReadString(root, key, value, error)) return false;
  if (value != expected) {
    error = std::string{"Run Record property "} + key + " has an unsupported value";
    return false;
  }
  return true;
}

bool ReadRequiredNull(yyjson_val* root, const char* key, std::string& error) {
  if (!yyjson_is_null(yyjson_obj_get(root, key))) {
    error = std::string{"RUN Record property "} + key + " must be null";
    return false;
  }
  return true;
}

bool ParseComparison(yyjson_val* value, std::optional<ComparisonFacts>& output,
                     std::string& error) {
  if (yyjson_is_null(value)) {
    output.reset();
    return true;
  }
  const std::set<std::string_view> properties{"status", "reason"};
  ComparisonFacts facts;
  if (!IsObject(value, properties, properties, "Run Record comparison", error) ||
      !ReadString(value, "status", facts.status, error) ||
      !ReadString(value, "reason", facts.reason, error))
    return false;
  const bool valid =
      (facts.status == "EQUAL" && facts.reason == "FINGERPRINT_EQUAL") ||
      (facts.status == "DIFFERENT" && facts.reason == "FINGERPRINT_DIFFERENT") ||
      (facts.status == "NOT_EVALUATED" && facts.reason == "CURRENT_RESULT_UNAVAILABLE");
  if (!valid) {
    error = "Run Record comparison status and reason are inconsistent";
    return false;
  }
  output = std::move(facts);
  return true;
}

bool ParseHistoricalBaseline(yyjson_val* value, std::optional<HistoricalBaseline>& output,
                             std::string& error) {
  if (yyjson_is_null(value)) {
    output.reset();
    return true;
  }
  const std::set<std::string_view> properties{"parent_record_file", "parent_record_sha256",
                                              "result_file",        "result_sha256",
                                              "fingerprint_domain", "deterministic_fingerprint"};
  HistoricalBaseline history;
  if (!IsObject(value, properties, properties, "Run Record historical_baseline", error) ||
      !ReadString(value, "parent_record_file", history.parent_record_file, error) ||
      !ReadString(value, "parent_record_sha256", history.parent_record_sha256, error) ||
      !ReadString(value, "result_file", history.result_file, error) ||
      !ReadString(value, "result_sha256", history.result_sha256, error) ||
      !ReadString(value, "fingerprint_domain", history.fingerprint_domain, error) ||
      !ReadString(value, "deterministic_fingerprint", history.deterministic_fingerprint, error) ||
      !((history.parent_record_file == kParentRecordFile &&
         history.result_file == kHistoricalResultFile &&
         history.fingerprint_domain == v06::kFingerprintDomain)
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
        || (history.parent_record_file == kCrcParentRecordFile &&
            history.result_file == kCrcHistoricalResultFile &&
            history.fingerprint_domain == v06::kCrcFingerprintDomain)
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
        || (history.parent_record_file == kLengthParentRecordFile &&
            history.result_file == kLengthHistoricalResultFile &&
            history.fingerprint_domain == v06::kLengthFingerprintDomain)
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
        || (history.parent_record_file == kVariableParentRecordFile &&
            history.result_file == kVariableHistoricalResultFile &&
            history.fingerprint_domain == v06::kVariableFingerprintDomain)
#endif
            ) ||
      !IsLowerHash(history.parent_record_sha256) || !IsLowerHash(history.result_sha256) ||
      history.deterministic_fingerprint.size() != 64U) {
    if (error.empty()) error = "Run Record historical_baseline is invalid";
    return false;
  }
  output = std::move(history);
  return true;
}

bool ParseRecord(std::string& text, RunRecord& output, std::string& error) {
  yyjson_read_err parse_error{};
  DocumentPtr document{
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &parse_error)};
  if (!document) {
    error = "Run Record 0.7 is not valid strict JSON";
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> properties{"format_version",
                                              "run_id",
                                              "tool_version",
                                              "evidence_origin",
                                              "operation_kind",
                                              "invocation_kind",
                                              "parent_run_id",
                                              "config_file",
                                              "values_file",
                                              "frame_file",
                                              "tx_frame_file",
                                              "rx_frame_file",
                                              "result_file",
                                              "event_file",
                                              "deterministic_fingerprint",
                                              "requested_pipeline_id",
                                              "execution",
                                              "comparison",
                                              "historical_baseline",
                                              "hash_manifest",
                                              "hash_manifest_excludes_self",
                                              "recorded_payload_files"};
  if (!IsObject(root, properties, properties, "Run Record", error) ||
      !ReadString(root, "format_version", output.format_version, error) ||
      (output.format_version != kRecordFormat
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
       && output.format_version != kCrcRecordFormat
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
       && output.format_version != kLengthRecordFormat
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
       && output.format_version != kVariableRecordFormat
#endif
       ) ||
      !ReadString(root, "run_id", output.run_id, error) || output.run_id.empty() ||
      !ReadString(root, "tool_version", output.tool_version, error) ||
      output.tool_version.empty() ||
      !ReadFixedString(root, "evidence_origin", "LAB_C_EXECUTION", error) ||
      !ReadString(root, "operation_kind", output.operation_kind, error) ||
      (output.operation_kind != "encode" && output.operation_kind != "inspect") ||
      !ReadString(root, "invocation_kind", output.invocation_kind, error) ||
      (output.invocation_kind != "RUN" && output.invocation_kind != "REPLAY") ||
      !ReadNullableString(root, "parent_run_id", output.parent_run_id, error) ||
      !ReadFixedString(root, "config_file", kConfigFile, error) ||
      !ReadNullableString(root, "values_file", output.values_file, error) ||
      !ReadNullableString(root, "frame_file", output.frame_file, error) ||
      !ReadRequiredNull(root, "tx_frame_file", error) ||
      !ReadRequiredNull(root, "rx_frame_file", error) ||
      !ReadNullableString(root, "result_file", output.result_file, error) ||
      !ReadFixedString(root, "event_file", kEventFile, error) ||
      !ReadNullableString(root, "deterministic_fingerprint", output.deterministic_fingerprint,
                          error) ||
      !ReadNullableString(root, "requested_pipeline_id", output.requested_pipeline_id, error) ||
      !ParseExecution(yyjson_obj_get(root, "execution"), output.operation_kind, output.execution,
                      error) ||
      !ParseComparison(yyjson_obj_get(root, "comparison"), output.comparison, error) ||
      !ParseHistoricalBaseline(yyjson_obj_get(root, "historical_baseline"),
                               output.historical_baseline, error) ||
      !ReadFixedString(root, "hash_manifest", "SHA256SUMS", error)) {
    if (error.empty()) error = "Run Record 0.7 contains an unsupported value";
    return false;
  }
  const bool replay = output.invocation_kind == "REPLAY";
  const bool inspect = output.operation_kind == "inspect";
  if (replay != output.parent_run_id.has_value() || replay != output.comparison.has_value() ||
      replay != output.historical_baseline.has_value() ||
      (!replay && output.requested_pipeline_id.has_value()) ||
      (replay && inspect != output.requested_pipeline_id.has_value())) {
    error = "Run Record invocation, history, and requested Pipeline are inconsistent";
    return false;
  }
  if (replay && (output.historical_baseline->parent_record_file != ParentRecordFileFor(output) ||
                 output.historical_baseline->result_file != HistoricalResultFileFor(output) ||
                 output.historical_baseline->fingerprint_domain != FingerprintDomainFor(output))) {
    error = "Run Record history generation does not match its format_version";
    return false;
  }
  yyjson_val* excludes = yyjson_obj_get(root, "hash_manifest_excludes_self");
  if (!yyjson_is_bool(excludes) || !yyjson_get_bool(excludes)) {
    error = "Run Record hash_manifest_excludes_self must be true";
    return false;
  }
  yyjson_val* payloads = yyjson_obj_get(root, "recorded_payload_files");
  if (!yyjson_is_arr(payloads)) {
    error = "Run Record recorded_payload_files must be an array";
    return false;
  }
  output.recorded_payload_files.clear();
  std::set<std::string> seen;
  std::size_t index = 0U;
  std::size_t maximum = 0U;
  yyjson_val* item = nullptr;
  yyjson_arr_foreach(payloads, index, maximum, item) {
    const std::set<std::string_view> descriptor_properties{"path", "size", "sha256"};
    FileDescriptor descriptor;
    if (!IsObject(item, descriptor_properties, descriptor_properties,
                  "Run Record payload descriptor", error) ||
        !ReadString(item, "path", descriptor.path, error) ||
        !ReadUint(item, "size", descriptor.size, error) ||
        !ReadString(item, "sha256", descriptor.sha256, error) ||
        !SafeRelativePath(descriptor.path) || !IsLowerHash(descriptor.sha256) ||
        !seen.insert(descriptor.path).second) {
      if (error.empty()) error = "Run Record payload descriptor is invalid or duplicated";
      return false;
    }
    output.recorded_payload_files.push_back(std::move(descriptor));
  }
  return true;
}

bool ParseManifest(const std::filesystem::path& bundle, v06::EvidenceFileSystem& file_system,
                   std::set<std::string>& paths, std::string& error) {
  std::string text;
  if (!ReadText(file_system, bundle / "SHA256SUMS", text, error)) return false;
  std::istringstream input{text};
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() < 67U || line[64] != ' ' || line[65] != ' ') {
      error = "Run Bundle manifest line is malformed";
      return false;
    }
    const std::string hash = line.substr(0U, 64U);
    const std::string path = line.substr(66U);
    if (!IsLowerHash(hash) || !SafeRelativePath(path) || path == "SHA256SUMS" ||
        !paths.insert(path).second) {
      error = "Run Bundle manifest entry is invalid or duplicated";
      return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!file_system.ReadFile(bundle / path, bytes, error)) return false;
    if (HashBytes(bytes) != hash) {
      error = "Run Bundle manifest hash mismatch";
      return false;
    }
  }
  return true;
}

bool ParseEvents(std::string& text, std::string_view run_id, std::vector<StageEvent>& events,
                 std::string& error) {
  events.clear();
  std::istringstream lines{text};
  std::string line;
  std::uint64_t previous_offset = 0U;
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) {
      error = "Event 0.7 contains an empty line";
      return false;
    }
    DocumentPtr document{yyjson_read(line.data(), line.size(), YYJSON_READ_NOFLAG)};
    yyjson_val* root = document ? yyjson_doc_get_root(document.get()) : nullptr;
    const std::set<std::string_view> properties{"format_version", "run_id", "event_id", "offset_us",
                                                "event_kind",     "phase",  "status"};
    StageEvent event;
    std::string event_run;
    if (!document || !IsObject(root, properties, properties, "Event 0.7", error) ||
        !ReadFixedString(root, "format_version", kEventFormat, error) ||
        !ReadString(root, "run_id", event_run, error) || event_run != run_id ||
        !ReadUint(root, "event_id", event.event_id, error) ||
        !ReadUint(root, "offset_us", event.offset_us, error) ||
        !ReadString(root, "event_kind", event.event_kind, error) ||
        !ReadString(root, "phase", event.phase, error) ||
        !ReadNullableString(root, "status", event.status, error)) {
      if (error.empty()) error = "Event 0.7 is invalid or has the wrong run_id";
      return false;
    }
    if (event.event_id != events.size() + 1U || event.event_id > 256U ||
        (!events.empty() && event.offset_us < previous_offset)) {
      error = "Event 0.7 id or offset sequence is invalid";
      return false;
    }
    previous_offset = event.offset_us;
    events.push_back(std::move(event));
  }
  if (events.size() < 4U || events.front().event_kind != "RUN_STARTED" ||
      events.front().phase != "NONE" || events.front().status.has_value() ||
      events.front().offset_us != 0U || events.back().event_kind != "RUN_FINISHED" ||
      !events.back().status.has_value()) {
    error = "Event 0.7 is missing required Run boundary events";
    return false;
  }
  return true;
}

bool ValidateEvents(const RunRecord& record, const std::vector<StageEvent>& events,
                    std::string& error) {
  std::vector<std::pair<std::string, std::string>> phases;
  for (std::size_t index = 1U; index + 1U < events.size();) {
    if (index + 1U >= events.size() - 1U || events[index].event_kind != "PHASE_STARTED" ||
        events[index].status.has_value() || events[index + 1U].event_kind != "PHASE_FINISHED" ||
        events[index + 1U].phase != events[index].phase || !events[index + 1U].status.has_value()) {
      error = "Event 0.7 phase events are not adjacent start/finish pairs";
      return false;
    }
    phases.emplace_back(events[index].phase, *events[index + 1U].status);
    index += 2U;
  }
  if (phases.empty() || phases.front().first != "PREPARATION" ||
      phases.front().second != record.execution.preparation.status ||
      events.back().phase != record.execution.terminal_stage ||
      *events.back().status != record.execution.terminal_status) {
    error = "Event 0.7 does not bind the Run Record terminal or preparation state";
    return false;
  }
  std::size_t cursor = 1U;
  auto expect_optional = [&](std::string_view phase, const std::optional<std::string>& status) {
    if (!status.has_value()) return true;
    if (cursor >= phases.size() || phases[cursor].first != phase ||
        phases[cursor].second != *status)
      return false;
    ++cursor;
    return true;
  };
  const std::optional<std::string> structural =
      record.execution.structural_query.has_value()
          ? std::optional<std::string>{record.execution.structural_query->status}
          : std::nullopt;
  const std::optional<std::string> main =
      record.execution.main_codec.has_value()
          ? std::optional<std::string>{record.execution.main_codec->status}
          : std::nullopt;
  const std::optional<std::string> review =
      record.execution.review_decode.has_value()
          ? std::optional<std::string>{record.execution.review_decode->status}
          : std::nullopt;
  if (!expect_optional("STRUCTURAL_QUERY", structural) || !expect_optional("MAIN_CODEC", main) ||
      !expect_optional("REVIEW_DECODE", review)) {
    error = "Event 0.7 phase order or status is inconsistent with the Run Record";
    return false;
  }
  std::size_t mapping_count = 0U;
  std::string mapping_status;
  bool mapping_failed = false;
  std::vector<std::string> mapping_statuses;
  while (cursor < phases.size() && phases[cursor].first == "RESULT_MAPPING") {
    mapping_status = phases[cursor].second;
    if (mapping_status != "OK" && mapping_status != "FAILED") {
      error = "Event 0.7 Result mapping status is unsupported";
      return false;
    }
    if (mapping_failed) {
      error = "Event 0.7 contains a phase after failed Result mapping";
      return false;
    }
    mapping_failed = mapping_status == "FAILED";
    mapping_statuses.push_back(mapping_status);
    ++mapping_count;
    ++cursor;
  }
  if (cursor != phases.size() || mapping_count > 2U ||
      record.execution.result_mapping.has_value() != (mapping_count != 0U) ||
      (mapping_count != 0U && record.execution.result_mapping->status != mapping_status)) {
    error = "Event 0.7 Result mapping events do not bind the Run Record";
    return false;
  }
  const auto& execution = record.execution;
  if ((execution.terminal_status == "OK" &&
       (mapping_count != 2U || mapping_statuses[0] != "OK" || mapping_statuses[1] != "OK")) ||
      (execution.terminal_status == "CODEC_ERROR" &&
       (mapping_count != 1U || mapping_statuses[0] != "OK")) ||
      (execution.terminal_status == "LAB_RESULT_FAILED" &&
       execution.terminal_stage == "RESULT_MAPPING" &&
       (mapping_count == 0U || mapping_statuses.back() != "FAILED")) ||
      ((execution.terminal_status == "PREPARATION_FAILED" ||
        execution.terminal_status == "STRUCTURAL_REJECTED" ||
        execution.terminal_status == "STRUCTURAL_ERROR" ||
        execution.terminal_stage == "REVIEW_DECODE") &&
       mapping_count != 0U)) {
    error = "Event 0.7 Result mapping count is inconsistent with the terminal state";
    return false;
  }
  if (execution.terminal_reason == "RAW_ASSOCIATION_FAILED" &&
      (mapping_count != 2U || mapping_statuses[0] != "OK" || mapping_statuses[1] != "FAILED")) {
    error = "RAW association failure requires OK then FAILED mappings";
    return false;
  }
  return true;
}

bool ValidateTerminal(const RunRecord& record, bool has_result, std::string& error) {
  const auto& execution = record.execution;
  if (record.operation_kind == "inspect" && execution.preparation.diagnostic_id.has_value() &&
      *execution.preparation.diagnostic_id != "PAE_LAB_C1_CONFIG_INVALID" &&
      *execution.preparation.diagnostic_id != "PAE_LAB_C1_SCHEMA_UNSUPPORTED") {
    error = "Run Record preparation diagnostic is not valid for Inspect";
    return false;
  }
  if (execution.review_decode.has_value() &&
      (record.operation_kind != "encode" || !execution.main_codec.has_value() ||
       execution.main_codec->status != "OK")) {
    error = "Run Record Review Decode requires a successful main Encode";
    return false;
  }
  if (record.operation_kind == "inspect" && execution.main_codec.has_value() &&
      (!execution.structural_query.has_value() || execution.structural_query->status != "OK" ||
       execution.structural_query->candidate_class != "ONE")) {
    error = "successful Inspect requires structural ONE/OK before main Decode";
    return false;
  }
  bool valid = false;
  if (execution.terminal_status == "PREPARATION_FAILED") {
    valid = execution.terminal_stage == "PREPARATION" &&
            execution.terminal_reason == "PREPARATION_REJECTED" &&
            execution.preparation.status == "FAILED" && !execution.structural_query &&
            !execution.main_codec && !execution.review_decode && !execution.result_mapping &&
            !has_result;
  } else if (execution.terminal_status == "STRUCTURAL_REJECTED") {
    valid = execution.terminal_stage == "STRUCTURAL_QUERY" &&
            execution.terminal_reason == "STRUCTURAL_REJECTED" &&
            execution.structural_query.has_value() &&
            (execution.structural_query->status == "UNKNOWN_MESSAGE" ||
             execution.structural_query->status == "AMBIGUOUS_MESSAGE") &&
            !execution.main_codec && !execution.review_decode && !execution.result_mapping &&
            !has_result;
  } else if (execution.terminal_status == "STRUCTURAL_ERROR") {
    valid = execution.terminal_stage == "STRUCTURAL_QUERY" &&
            execution.terminal_reason == "STRUCTURAL_QUERY_ERROR" &&
            execution.structural_query.has_value() &&
            (execution.structural_query->status == "INVALID_ARGUMENT" ||
             execution.structural_query->status == "INVALID_PLAN") &&
            !execution.main_codec && !execution.review_decode && !execution.result_mapping &&
            !has_result;
  } else if (execution.terminal_status == "CODEC_ERROR") {
    valid = execution.terminal_stage == "RESULT_MAPPING" && execution.terminal_reason == "NONE" &&
            execution.main_codec.has_value() && execution.main_codec->status != "OK" &&
            !execution.review_decode && execution.result_mapping.has_value() &&
            execution.result_mapping->status == "OK" && has_result;
  } else if (execution.terminal_status == "OK") {
    valid = execution.terminal_stage == "RESULT_MAPPING" && execution.terminal_reason == "NONE" &&
            execution.main_codec.has_value() && execution.main_codec->status == "OK" &&
            execution.result_mapping.has_value() && execution.result_mapping->status == "OK" &&
            has_result &&
            (record.operation_kind == "inspect" ||
             (execution.review_decode.has_value() && execution.review_decode->status == "OK"));
  } else if (execution.terminal_status == "LAB_RESULT_FAILED") {
    valid = !has_result && execution.main_codec.has_value();
    if (execution.terminal_stage == "REVIEW_DECODE") {
      valid = valid && record.operation_kind == "encode" && execution.review_decode.has_value() &&
              !execution.result_mapping &&
              ((execution.terminal_reason == "REVIEW_DECODE_FAILED" &&
                execution.review_decode->status != "OK") ||
               (execution.terminal_reason == "REVIEW_MESSAGE_MISMATCH" &&
                execution.review_decode->status == "OK"));
    } else if (execution.terminal_stage == "RESULT_MAPPING") {
      valid = valid && execution.result_mapping.has_value() &&
              execution.result_mapping->status == "FAILED" &&
              (execution.terminal_reason == "INTERNAL_ERROR" ||
               execution.terminal_reason == "RAW_ASSOCIATION_FAILED");
      if (valid && execution.terminal_reason == "RAW_ASSOCIATION_FAILED") {
        valid = execution.main_codec->status == "OK";
      }
      if (valid && record.operation_kind == "encode" && execution.main_codec->status == "OK") {
        valid = execution.review_decode.has_value() && execution.review_decode->status == "OK";
      }
    } else {
      valid = false;
    }
  }
  if (!valid) {
    error = "Run Record terminal state combination is inconsistent: status=" +
            execution.terminal_status + ", stage=" + execution.terminal_stage +
            ", reason=" + execution.terminal_reason +
            ", has_result=" + (has_result ? "true" : "false");
    return false;
  }
  const bool inspect = record.operation_kind == "inspect";
  const bool has_values = record.values_file.has_value();
  const bool has_frame = record.frame_file.has_value();
  if ((inspect && (has_values || !has_frame)) || (!inspect && !has_values) ||
      (record.result_file.has_value() != has_result) ||
      (record.deterministic_fingerprint.has_value() != has_result) ||
      (record.values_file.has_value() && *record.values_file != kValuesFile) ||
      (record.frame_file.has_value() && *record.frame_file != kFrameFile) ||
      (record.result_file.has_value() && *record.result_file != ResultFileFor(record)) ||
      (!inspect && (has_frame != (execution.terminal_status == "OK" && has_result)))) {
    error = "Run Record payload roles are inconsistent with its operation and terminal state";
    return false;
  }
  const bool main_count_matches =
      inspect ? (execution.main_codec.has_value() == (execution.counts.decode_calls == 1U))
              : (execution.main_codec.has_value() == (execution.counts.encode_calls == 1U));
  if ((inspect &&
       ((execution.preparation.status == "OK" && !execution.structural_query) ||
        execution.counts.encode_calls != 0U || execution.counts.review_decode_calls != 0U)) ||
      (!inspect && (execution.structural_query || execution.counts.decode_calls != 0U)) ||
      !main_count_matches ||
      (execution.review_decode.has_value() != (execution.counts.review_decode_calls == 1U))) {
    error = "Run Record execution counts are inconsistent with executed stages";
    return false;
  }
  if ((!execution.structural_query.has_value() && execution.counts.structural_query_calls != 0U) ||
      (execution.structural_query.has_value() && execution.counts.structural_query_calls == 0U) ||
      (execution.preparation.status == "FAILED" &&
       (execution.counts.structural_query_calls != 0U || execution.counts.encode_calls != 0U ||
        execution.counts.decode_calls != 0U || execution.counts.review_decode_calls != 0U))) {
    error = "Run Record preparation or structural counts are inconsistent";
    return false;
  }
  return true;
}

std::set<std::string> ExpectedPayloadPaths(const RunRecord& record) {
  std::set<std::string> expected{std::string{kConfigFile}, std::string{kEventFile}};
  if (record.values_file.has_value()) expected.insert(std::string{kValuesFile});
  if (record.frame_file.has_value()) expected.insert(std::string{kFrameFile});
  if (record.result_file.has_value()) expected.insert(std::string{ResultFileFor(record)});
  if (record.invocation_kind == "REPLAY") {
    expected.insert(std::string{ParentRecordFileFor(record)});
    expected.insert(std::string{HistoricalResultFileFor(record)});
  }
  return expected;
}

bool ValidatePayloadRoleSet(const RunRecord& record, std::string_view context, std::string& error) {
  std::set<std::string> actual;
  for (const auto& descriptor : record.recorded_payload_files) actual.insert(descriptor.path);
  if (actual != ExpectedPayloadPaths(record)) {
    error = std::string{context} + " payload set is inconsistent with its file roles";
    return false;
  }
  return true;
}

bool ValidateResultExecutionSemantics(const RunRecord& record, const v06::Result& result,
                                      bool historical, std::string& error) {
  const std::string prefix = historical ? "historical Result 0.6 " : "Result 0.6 ";
  const std::string expected_mode = record.operation_kind == "encode" ? "ENCODE_TX" : "DECODE_RX";
  const std::string expected_subject = record.operation_kind == "encode" ? "TX" : "RX";
  if (result.replay_mode != expected_mode || result.replay_subject != expected_subject) {
    error = prefix + "mode does not match Run operation";
    return false;
  }
  const bool terminal_ok = record.execution.terminal_status == "OK";
  const bool terminal_codec_error = record.execution.terminal_status == "CODEC_ERROR";
  if ((!terminal_ok && !terminal_codec_error) ||
      (terminal_ok && (result.operation_status != "OK" || result.exit_code != 0 ||
                       result.current_execution_status != "OK")) ||
      (terminal_codec_error && (result.operation_status != "CODEC_ERROR" || result.exit_code != 5 ||
                                result.current_execution_status == "OK")) ||
      result.tx_frame_hex.has_value() || result.rx_frame_hex.has_value()) {
    error = prefix + "status or Frame roles are inconsistent with Run Record 0.7";
    return false;
  }
  if (terminal_ok && (result.diagnostic_id.has_value() || !result.diagnostic_detail.empty() ||
                      result.current_execution_diagnostic_id.has_value())) {
    error = historical ? "successful historical Result must not contain Codec diagnostics"
                       : "successful Result must not contain Codec diagnostics";
    return false;
  }
  if (terminal_codec_error) {
    const std::string expected_diagnostic = "PAE_LAB_CODEC_" + record.execution.main_codec->status;
    if (result.diagnostic_id != expected_diagnostic ||
        result.current_execution_diagnostic_id != expected_diagnostic) {
      error = historical ? "historical Codec diagnostic does not match main status"
                         : "Codec diagnostic does not match main status";
      return false;
    }
  }
  return true;
}

const FileDescriptor* DescriptorFor(const RunRecord& record, std::string_view path) {
  const auto found =
      std::find_if(record.recorded_payload_files.begin(), record.recorded_payload_files.end(),
                   [&](const auto& item) { return item.path == path; });
  return found == record.recorded_payload_files.end() ? nullptr : &*found;
}

bool ValidateHistoricalFrameDescriptor(const RunRecord& record, const v06::Result& result,
                                       std::string& error) {
  const FileDescriptor* descriptor = DescriptorFor(record, kFrameFile);
  if (result.frame_hex.has_value() != (descriptor != nullptr)) {
    error = "historical Result 0.6 frame_hex role does not match parent Frame descriptor";
    return false;
  }
  if (!descriptor) return true;
  const std::string& hex = *result.frame_hex;
  if ((hex.size() % 2U) != 0U) {
    error = "historical Result 0.6 frame_hex has an invalid byte length";
    return false;
  }
  std::vector<std::uint8_t> bytes(hex.size() / 2U);
  const auto digit = [](char value) -> std::uint8_t {
    return value <= '9' ? static_cast<std::uint8_t>(value - '0')
                        : static_cast<std::uint8_t>(value - 'A' + 10);
  };
  for (std::size_t index = 0U; index < bytes.size(); ++index)
    bytes[index] =
        static_cast<std::uint8_t>((digit(hex[index * 2U]) << 4U) | digit(hex[index * 2U + 1U]));
  if (bytes.size() != descriptor->size || HashBytes(bytes) != descriptor->sha256) {
    error = "historical Result 0.6 frame_hex does not bind parent Frame descriptor";
    return false;
  }
  return true;
}

}  // namespace

bool LoadRunBundleForTest(const std::filesystem::path& bundle, StoredRunBundle& output,
                          v06::EvidenceFileSystem& file_system, std::string& error) {
  output = StoredRunBundle{};
  error.clear();
  StoredRunBundle candidate;
  std::set<std::string> disk_files;
  if (!PreflightBundleTree(bundle, disk_files, error)) return false;
  std::set<std::string> manifest_paths;
  if (!ParseManifest(bundle, file_system, manifest_paths, error)) return false;
  const bool has_old_record = manifest_paths.find(std::string{kRecordFile}) != manifest_paths.end();
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  const bool has_crc_record =
      manifest_paths.find(std::string{kCrcRecordFile}) != manifest_paths.end();
#else
  const bool has_crc_record = false;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  const bool has_length_record =
      manifest_paths.find(std::string{kLengthRecordFile}) != manifest_paths.end();
#else
  const bool has_length_record = false;
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  const bool has_variable_record =
      manifest_paths.find(std::string{kVariableRecordFile}) != manifest_paths.end();
#else
  const bool has_variable_record = false;
#endif
  const unsigned record_generation_count =
      static_cast<unsigned>(has_old_record) + static_cast<unsigned>(has_crc_record) +
      static_cast<unsigned>(has_length_record) + static_cast<unsigned>(has_variable_record);
  if (record_generation_count != 1U) {
    error = "Run Bundle must contain exactly one supported Run Record generation";
    return false;
  }
  const std::string_view record_file = has_variable_record
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
                                           ? kVariableRecordFile
#else
                                           ? kRecordFile
#endif
                                           : has_length_record
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                                                 ? kLengthRecordFile
#else
                                                 ? kRecordFile
#endif
                                                 : has_crc_record
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
                                                       ? kCrcRecordFile
#else
                                                       ? kRecordFile
#endif
                                                       : kRecordFile;
  const std::set<std::string> mandatory{"COMPLETE", std::string{kConfigFile},
                                        std::string{kEventFile}, std::string{record_file}};
  if (!std::includes(manifest_paths.begin(), manifest_paths.end(), mandatory.begin(),
                     mandatory.end())) {
    error = "Run Bundle 0.7 manifest omits a required file";
    return false;
  }
  std::set<std::string> expected_disk = manifest_paths;
  expected_disk.insert("SHA256SUMS");
  if (disk_files != expected_disk) {
    error = "Run Bundle contains an unlisted or missing file";
    return false;
  }

  if (!ReadText(file_system, bundle / record_file, candidate.record_text, error) ||
      !ParseRecord(candidate.record_text, candidate.record, error))
    return false;
  if (record_file != RecordFileFor(candidate.record)) {
    error = "Run Record filename does not match its format_version";
    return false;
  }
  std::string bundle_name = bundle.filename().generic_string();
  constexpr std::string_view suffix = ".inprogress";
  if (bundle_name.size() > suffix.size() &&
      bundle_name.substr(bundle_name.size() - suffix.size()) == suffix)
    bundle_name.resize(bundle_name.size() - suffix.size());
  if (candidate.record.run_id != bundle_name) {
    error = "Run Record 0.7 run_id does not match the Bundle directory";
    return false;
  }

  std::set<std::string> payload_paths;
  for (const auto& descriptor : candidate.record.recorded_payload_files) {
    if (!payload_paths.insert(descriptor.path).second) {
      error = "Run Record 0.7 contains duplicate payload paths";
      return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!file_system.ReadFile(bundle / descriptor.path, bytes, error)) return false;
    if (bytes.size() != descriptor.size || HashBytes(bytes) != descriptor.sha256) {
      error = "Run Record 0.7 payload length or hash does not match the recorded file";
      return false;
    }
  }
  const std::set<std::string> expected_payloads = ExpectedPayloadPaths(candidate.record);
  if (!ValidatePayloadRoleSet(candidate.record, "Run Record 0.7", error)) return false;
  std::set<std::string> expected_manifest = expected_payloads;
  expected_manifest.insert("COMPLETE");
  expected_manifest.insert(std::string{record_file});
  if (manifest_paths != expected_manifest) {
    error = "Run Bundle 0.7 has a mixed generation or unsupported file set";
    return false;
  }

  if (!ReadText(file_system, bundle / kConfigFile, candidate.config_text, error)) return false;
  if (candidate.record.values_file.has_value()) {
    std::string values;
    if (*candidate.record.values_file != kValuesFile ||
        !ReadText(file_system, bundle / kValuesFile, values, error))
      return false;
    candidate.values_text = std::move(values);
  }
  if (candidate.record.frame_file.has_value()) {
    std::vector<std::uint8_t> frame;
    if (*candidate.record.frame_file != kFrameFile ||
        !file_system.ReadFile(bundle / kFrameFile, frame, error))
      return false;
    candidate.frame = std::move(frame);
  }
  if (candidate.record.result_file.has_value()) {
    std::string result_text;
    const std::string_view result_file = ResultFileFor(candidate.record);
    if (*candidate.record.result_file != result_file ||
        !ReadText(file_system, bundle / result_file, result_text, error) ||
        !v06::ParseResult(result_text, candidate.result.emplace(), error))
      return false;
    if (candidate.result->format_version != ResultFormatFor(candidate.record)) {
      error = "Result format_version does not match the Run Record generation";
      return false;
    }
    candidate.result_text = result_text;
    const std::string fingerprint = v06::FinalizeFingerprint(*candidate.result, error);
    if (!error.empty() || !candidate.record.execution.main_codec.has_value() ||
        candidate.record.deterministic_fingerprint != fingerprint ||
        !candidate.result->config_sha256.has_value() ||
        *candidate.result->config_sha256 != HashBytes(candidate.config_text) ||
        candidate.result->operation_kind != candidate.record.operation_kind ||
        candidate.result->current_execution_status !=
            candidate.record.execution.main_codec->status) {
      if (error.empty()) error = "Result 0.6 does not bind the Run Record 0.7 execution";
      return false;
    }
    const std::optional<std::string> expected_frame =
        candidate.frame.has_value() ? std::optional<std::string>{HexUpper(*candidate.frame)}
                                    : std::nullopt;
    if (candidate.result->frame_hex != expected_frame) {
      error = "Result 0.6 frame_hex does not bind the Run main Frame";
      return false;
    }
    if (!ValidateResultExecutionSemantics(candidate.record, *candidate.result, false, error))
      return false;
  } else if (candidate.record.deterministic_fingerprint.has_value()) {
    error = "Run without Result has a deterministic fingerprint";
    return false;
  }
  if (candidate.record.invocation_kind == "REPLAY") {
    std::string parent_text;
    std::string historical_result_text;
    const std::string_view parent_record_file = ParentRecordFileFor(candidate.record);
    const std::string_view historical_result_file = HistoricalResultFileFor(candidate.record);
    if (!ReadText(file_system, bundle / parent_record_file, parent_text, error) ||
        !ReadText(file_system, bundle / historical_result_file, historical_result_text, error))
      return false;
    const auto& history = *candidate.record.historical_baseline;
    if (HashBytes(parent_text) != history.parent_record_sha256 ||
        HashBytes(historical_result_text) != history.result_sha256) {
      error = "Replay historical snapshot hash does not match its declaration";
      return false;
    }
    RunRecord parent_record;
    std::string parent_parse_text = parent_text;
    v06::Result historical_result;
    std::string historical_parse_text = historical_result_text;
    if (!ParseRecord(parent_parse_text, parent_record, error) ||
        !v06::ParseResult(historical_parse_text, historical_result, error))
      return false;
    if (historical_result.format_version != ResultFormatFor(parent_record) ||
        history.fingerprint_domain != FingerprintDomainFor(parent_record)) {
      error = "Replay historical Result generation does not match the parent Run Record";
      return false;
    }
    const FileDescriptor* parent_result =
        DescriptorFor(parent_record, ResultFileFor(parent_record));
    const FileDescriptor* parent_config = DescriptorFor(parent_record, kConfigFile);
    const FileDescriptor* child_config = DescriptorFor(candidate.record, kConfigFile);
    const std::string historical_fingerprint = v06::FinalizeFingerprint(historical_result, error);
    if (!error.empty() || !parent_result || !parent_config || !child_config ||
        parent_record.run_id != candidate.record.parent_run_id ||
        parent_record.format_version != candidate.record.format_version ||
        parent_record.result_file !=
            std::optional<std::string>{std::string{ResultFileFor(parent_record)}} ||
        parent_record.deterministic_fingerprint != historical_fingerprint ||
        parent_result->size != historical_result_text.size() ||
        parent_result->sha256 != HashBytes(historical_result_text) ||
        history.deterministic_fingerprint != historical_fingerprint ||
        parent_config->size != child_config->size ||
        parent_config->sha256 != child_config->sha256 ||
        parent_record.operation_kind != candidate.record.operation_kind ||
        historical_result.operation_kind != parent_record.operation_kind ||
        !historical_result.config_sha256.has_value() ||
        *historical_result.config_sha256 != parent_config->sha256 ||
        !parent_record.execution.main_codec.has_value() ||
        historical_result.current_execution_status != parent_record.execution.main_codec->status ||
        !ValidateTerminal(parent_record, true, error) ||
        !ValidatePayloadRoleSet(parent_record, "historical parent", error) ||
        !ValidateResultExecutionSemantics(parent_record, historical_result, true, error) ||
        !ValidateHistoricalFrameDescriptor(parent_record, historical_result, error)) {
      if (error.empty()) error = "Replay historical baseline does not bind the parent execution";
      return false;
    }
    const std::string input_path = candidate.record.operation_kind == "encode"
                                       ? std::string{kValuesFile}
                                       : std::string{kFrameFile};
    const FileDescriptor* parent_input = DescriptorFor(parent_record, input_path);
    const FileDescriptor* child_input = DescriptorFor(candidate.record, input_path);
    if (!parent_input || !child_input || parent_input->size != child_input->size ||
        parent_input->sha256 != child_input->sha256 ||
        (candidate.record.operation_kind == "inspect" &&
         (!historical_result.pipeline_id.has_value() ||
          candidate.record.requested_pipeline_id != historical_result.pipeline_id))) {
      error = "Replay child input does not bind the parent material and Pipeline";
      return false;
    }
    const ComparisonFacts expected =
        candidate.record.deterministic_fingerprint.has_value()
            ? ComparisonFacts{*candidate.record.deterministic_fingerprint == historical_fingerprint
                                  ? "EQUAL"
                                  : "DIFFERENT",
                              *candidate.record.deterministic_fingerprint == historical_fingerprint
                                  ? "FINGERPRINT_EQUAL"
                                  : "FINGERPRINT_DIFFERENT"}
            : ComparisonFacts{"NOT_EVALUATED", "CURRENT_RESULT_UNAVAILABLE"};
    if (!candidate.record.comparison.has_value() ||
        candidate.record.comparison->status != expected.status ||
        candidate.record.comparison->reason != expected.reason) {
      error = "Replay comparison does not match the current and historical fingerprints";
      return false;
    }
    candidate.parent_record_text = std::move(parent_text);
    candidate.historical_result_text = std::move(historical_result_text);
  }
  if (!ValidateTerminal(candidate.record, candidate.result.has_value(), error)) return false;

  std::string event_text;
  if (!ReadText(file_system, bundle / kEventFile, event_text, error) ||
      !ParseEvents(event_text, candidate.record.run_id, candidate.events, error) ||
      !ValidateEvents(candidate.record, candidate.events, error))
    return false;
  output = std::move(candidate);
  error.clear();
  return true;
}

bool LoadRunBundle(const std::filesystem::path& bundle, StoredRunBundle& output,
                   std::string& error) {
  v06::StandardEvidenceFileSystem file_system;
  return LoadRunBundleForTest(bundle, output, file_system, error);
}

}  // namespace pae::protocol_lab::v07
