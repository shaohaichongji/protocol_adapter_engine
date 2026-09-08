#include "v06_evidence.h"

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

namespace pae::protocol_lab::v06 {
namespace {

constexpr std::string_view kResultFile = "result_summary_v0.6.json";
constexpr std::string_view kRecordFile = "run_record_v0.6.json";
constexpr std::string_view kEventFile = "events_v0.6.jsonl";
constexpr std::string_view kConfigFile = "inputs/protocol.pae.json";
constexpr std::string_view kValuesFile = "inputs/values.pae-lab.json";
constexpr std::string_view kFrameFile = "frames/000001_frame.bin";
constexpr std::string_view kTxFrameFile = "frames/000001_tx.bin";
constexpr std::string_view kRxFrameFile = "frames/000002_rx.bin";
constexpr std::string_view kRxMetadataFile = "frames/000002_rx.meta.json";

struct FileDescriptor {
  std::string path;
  std::size_t size = 0U;
  std::string sha256;
};

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

bool PreflightBundleTree(const std::filesystem::path& bundle, std::set<std::string>& disk_files,
                         std::string& error) {
  std::error_code file_error;
  if (IsLinkOrReparsePoint(bundle, file_error)) {
    error = "Evidence Bundle path is a symlink or reparse point";
    return false;
  }
  if (file_error || !std::filesystem::is_directory(bundle, file_error) || file_error) {
    error = "Evidence Bundle path is not a regular directory";
    return false;
  }
  for (std::filesystem::recursive_directory_iterator iterator{bundle, file_error}, end;
       iterator != end && !file_error; iterator.increment(file_error)) {
    if (IsLinkOrReparsePoint(iterator->path(), file_error)) {
      error = "Evidence Bundle contains a symlink or reparse point";
      return false;
    }
    if (file_error) break;
    const std::string relative =
        std::filesystem::relative(iterator->path(), bundle, file_error).generic_string();
    if (file_error || !SafeRelativePath(relative)) {
      error = "Evidence Bundle contains an unsafe file path";
      return false;
    }
    const std::filesystem::file_status status = iterator->status(file_error);
    if (file_error) break;
    if (std::filesystem::is_regular_file(status)) {
      disk_files.insert(relative);
    } else if (!std::filesystem::is_directory(status)) {
      error = "Evidence Bundle contains an unsupported filesystem object";
      return false;
    }
  }
  if (file_error) {
    error = "cannot enumerate Evidence Bundle";
    return false;
  }
  return true;
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
    if (!seen.emplace(name).second) {
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
    error = std::string{key} + " must be a string";
    return false;
  }
  output.assign(yyjson_get_str(value), yyjson_get_len(value));
  return true;
}

bool ReadDiskFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                  std::string& error) {
  std::error_code file_error;
  const auto size = std::filesystem::file_size(path, file_error);
  if (file_error || size > static_cast<std::uintmax_t>((std::numeric_limits<std::size_t>::max)())) {
    error = "cannot determine Evidence file size";
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "cannot open Evidence file";
    return false;
  }
  output.resize(static_cast<std::size_t>(size));
  if (!output.empty())
    stream.read(reinterpret_cast<char*>(output.data()),
                static_cast<std::streamsize>(output.size()));
  if (!stream && !output.empty()) {
    error = "cannot read Evidence file";
    return false;
  }
  return true;
}

bool ReadText(EvidenceFileSystem& file_system, const std::filesystem::path& path,
              std::string& output, std::string& error) {
  std::vector<std::uint8_t> bytes;
  if (!file_system.ReadFile(path, bytes, error)) return false;
  output.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return true;
}

bool WriteVerified(EvidenceFileSystem& file_system, const std::filesystem::path& root,
                   std::string_view relative, const std::uint8_t* data, std::size_t size,
                   std::vector<FileDescriptor>& files, std::string& error) {
  if (!SafeRelativePath(relative)) {
    error = "writer received an unsafe relative path";
    return false;
  }
  const std::filesystem::path destination = root / std::filesystem::path{relative};
  if (!file_system.CreateDirectories(destination.parent_path(), error)) return false;
  bool exists = false;
  if (!file_system.Exists(destination, exists, error) || exists) {
    if (error.empty()) error = "record destination already exists";
    return false;
  }
  std::filesystem::path temporary = destination;
  temporary += ".tmp";
  if (!file_system.WriteClosedFile(temporary, data, size, error)) return false;
  std::vector<std::uint8_t> reviewed;
  if (!file_system.ReadFile(temporary, reviewed, error) || reviewed.size() != size ||
      !std::equal(reviewed.begin(), reviewed.end(), data)) {
    if (error.empty()) error = "record file verification mismatch";
    return false;
  }
  if (HashBytes(reviewed) != HashBytes(data, size)) {
    error = "record file hash verification mismatch";
    return false;
  }
  if (!file_system.Rename(temporary, destination, error)) return false;
  files.push_back(FileDescriptor{std::string{relative}, size, HashBytes(data, size)});
  return true;
}

bool WriteVerified(EvidenceFileSystem& file_system, const std::filesystem::path& root,
                   std::string_view relative, std::string_view text,
                   std::vector<FileDescriptor>& files, std::string& error) {
  return WriteVerified(file_system, root, relative,
                       reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), files,
                       error);
}

std::string SerializeEvent(const BundleInput& input, std::string_view fingerprint) {
  const bool has_frame = input.frame.has_value();
  const bool has_any_frame = has_frame || input.tx_frame.has_value() || input.rx_frame.has_value();
  auto nullable_path = [](bool present, std::string_view path) {
    return present ? Quote(path) : std::string{"null"};
  };
  auto nullable_size = [](const std::optional<std::vector<std::uint8_t>>& frame) {
    return frame.has_value() ? std::to_string(frame->size()) : std::string{"null"};
  };
  auto nullable_hash = [](const std::optional<std::vector<std::uint8_t>>& frame) {
    return frame.has_value() ? Quote(HashBytes(*frame)) : std::string{"null"};
  };
  std::ostringstream output;
  output << "{\"format_version\":\"pae.lab.event/0.6\",\"event_id\":1,"
            "\"event_kind\":"
         << Quote(has_any_frame ? "FRAME_SET" : "RESULT")
         << ",\"operation_kind\":" << Quote(input.result.operation_kind)
         << ",\"operation_status\":" << Quote(input.result.operation_status)
         << ",\"frame_file\":" << nullable_path(has_frame, kFrameFile)
         << ",\"frame_length\":" << nullable_size(input.frame)
         << ",\"frame_sha256\":" << nullable_hash(input.frame)
         << ",\"tx_frame_file\":" << nullable_path(input.tx_frame.has_value(), kTxFrameFile)
         << ",\"tx_frame_length\":" << nullable_size(input.tx_frame)
         << ",\"tx_frame_sha256\":" << nullable_hash(input.tx_frame)
         << ",\"rx_frame_file\":" << nullable_path(input.rx_frame.has_value(), kRxFrameFile)
         << ",\"rx_frame_length\":" << nullable_size(input.rx_frame)
         << ",\"rx_frame_sha256\":" << nullable_hash(input.rx_frame) << ",\"rx_metadata_file\":"
         << nullable_path(input.rx_received_from.has_value(), kRxMetadataFile)
         << ",\"frame_origin\":" << (has_any_frame ? Quote("LAB_B_SYNTHETIC") : "null")
         << ",\"result_file\":" << Quote(kResultFile)
         << ",\"deterministic_fingerprint\":" << Quote(fingerprint) << "}\n";
  return output.str();
}

std::string SerializeRecord(std::string_view run_name, const BundleInput& input,
                            std::string_view fingerprint,
                            const std::vector<FileDescriptor>& files) {
  std::ostringstream output;
  output << "{\n  \"format_version\":\"pae.lab.record/0.6\",\n"
         << "  \"run_id\":" << Quote(run_name) << ",\n"
         << "  \"tool_version\":\"0.1.0-dec042b-lab-b\",\n"
         << "  \"result_file\":" << Quote(kResultFile) << ",\n"
         << "  \"event_file\":" << Quote(kEventFile) << ",\n"
         << "  \"config_file\":" << Quote(kConfigFile) << ",\n"
         << "  \"values_file\":" << (input.values_text ? Quote(kValuesFile) : "null") << ",\n"
         << "  \"frame_file\":" << (input.frame ? Quote(kFrameFile) : "null") << ",\n"
         << "  \"tx_frame_file\":" << (input.tx_frame ? Quote(kTxFrameFile) : "null") << ",\n"
         << "  \"rx_frame_file\":" << (input.rx_frame ? Quote(kRxFrameFile) : "null") << ",\n"
         << "  \"rx_metadata_file\":" << (input.rx_received_from ? Quote(kRxMetadataFile) : "null")
         << ",\n"
         << "  \"deterministic_fingerprint\":" << Quote(fingerprint) << ",\n"
         << "  \"hash_manifest\":\"SHA256SUMS\",\n"
         << "  \"hash_manifest_excludes_self\":true,\n"
         << "  \"recorded_payload_files\":[\n";
  for (std::size_t index = 0U; index < files.size(); ++index) {
    const auto& file = files[index];
    output << "    {\"path\":" << Quote(file.path) << ",\"size\":" << file.size
           << ",\"sha256\":" << Quote(file.sha256) << "}"
           << (index + 1U == files.size() ? "\n" : ",\n");
  }
  output << "  ]\n}\n";
  return output.str();
}

std::string SerializeRxMetadata(const BundleInput& input) {
  std::ostringstream output;
  output << "{\n  \"format_version\":\"pae.lab.rx-meta/0.2\",\n"
         << "  \"event_id\":1,\n"
         << "  \"frame_file\":" << Quote(kRxFrameFile) << ",\n"
         << "  \"frame_length\":" << input.rx_frame->size() << ",\n"
         << "  \"frame_sha256\":" << Quote(HashBytes(*input.rx_frame)) << ",\n"
         << "  \"received_from\":" << Quote(*input.rx_received_from) << ",\n"
         << "  \"direction\":\"RX\"\n}\n";
  return output.str();
}

bool VerifyRxMetadata(std::string& text, const std::vector<std::uint8_t>& rx_frame,
                      std::string& received_from, std::string& error) {
  yyjson_read_err read_error{};
  DocumentPtr document{
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &read_error)};
  if (!document) {
    error = "RX Metadata 0.2 is not valid strict JSON";
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> keys{"format_version", "event_id",     "frame_file",
                                        "frame_length",   "frame_sha256", "received_from",
                                        "direction"};
  if (!IsObject(root, keys, keys, "RX Metadata 0.2", error)) return false;
  std::string format, frame_file, frame_sha256, direction;
  yyjson_val* event_id = yyjson_obj_get(root, "event_id");
  yyjson_val* frame_length = yyjson_obj_get(root, "frame_length");
  if (!ReadString(root, "format_version", format, error) || format != "pae.lab.rx-meta/0.2" ||
      !yyjson_is_uint(event_id) || yyjson_get_uint(event_id) != 1U ||
      !ReadString(root, "frame_file", frame_file, error) || frame_file != kRxFrameFile ||
      !yyjson_is_uint(frame_length) || yyjson_get_uint(frame_length) != rx_frame.size() ||
      !ReadString(root, "frame_sha256", frame_sha256, error) ||
      frame_sha256 != HashBytes(rx_frame) ||
      !ReadString(root, "received_from", received_from, error) || received_from.empty() ||
      !ReadString(root, "direction", direction, error) || direction != "RX") {
    if (error.empty()) error = "RX Metadata 0.2 does not match its RX Frame";
    return false;
  }
  return true;
}

bool ParseManifest(const std::filesystem::path& bundle, EvidenceFileSystem& file_system,
                   std::set<std::string>& paths, std::string& error) {
  std::string text;
  if (!ReadText(file_system, bundle / "SHA256SUMS", text, error)) return false;
  std::istringstream input{text};
  std::string line;
  std::string previous;
  while (std::getline(input, line)) {
    if (line.size() < 67U || line.substr(64U, 2U) != "  ") {
      error = "SHA256SUMS line has an invalid shape";
      return false;
    }
    const std::string digest = line.substr(0U, 64U);
    const std::string path = line.substr(66U);
    if (!IsLowerHash(digest) || !SafeRelativePath(path) || path == "SHA256SUMS") {
      error = "SHA256SUMS contains an invalid digest or unsafe path";
      return false;
    }
    if ((!previous.empty() && path <= previous) || !paths.insert(path).second) {
      error = "SHA256SUMS paths are duplicate or not strictly sorted";
      return false;
    }
    previous = path;
    std::vector<std::uint8_t> bytes;
    if (!file_system.ReadFile(bundle / path, bytes, error)) return false;
    if (HashBytes(bytes) != digest) {
      error = "SHA256SUMS digest does not match file " + path;
      return false;
    }
  }
  if (paths.empty()) {
    error = "SHA256SUMS must not be empty";
    return false;
  }
  return true;
}

bool ParseRecord(std::string& text, std::vector<FileDescriptor>& payloads, bool& has_values,
                 bool& has_frame, bool& has_tx_frame, bool& has_rx_frame, bool& has_rx_metadata,
                 std::string& run_id, std::string& fingerprint, std::string& error) {
  yyjson_read_err read_error{};
  DocumentPtr document{
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &read_error)};
  if (!document) {
    error = "Run Record 0.6 is not valid strict JSON";
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> keys{"format_version",
                                        "run_id",
                                        "tool_version",
                                        "result_file",
                                        "event_file",
                                        "config_file",
                                        "values_file",
                                        "frame_file",
                                        "tx_frame_file",
                                        "rx_frame_file",
                                        "rx_metadata_file",
                                        "deterministic_fingerprint",
                                        "hash_manifest",
                                        "hash_manifest_excludes_self",
                                        "recorded_payload_files"};
  if (!IsObject(root, keys, keys, "Run Record 0.6", error)) return false;
  std::string format, tool_version, result_file, event_file, config_file, manifest;
  if (!ReadString(root, "format_version", format, error) || format != kRecordFormat ||
      !ReadString(root, "run_id", run_id, error) || run_id.empty() ||
      !ReadString(root, "tool_version", tool_version, error) ||
      tool_version != "0.1.0-dec042b-lab-b" ||
      !ReadString(root, "result_file", result_file, error) || result_file != kResultFile ||
      !ReadString(root, "event_file", event_file, error) || event_file != kEventFile ||
      !ReadString(root, "config_file", config_file, error) || config_file != kConfigFile ||
      !ReadString(root, "deterministic_fingerprint", fingerprint, error) ||
      !ReadString(root, "hash_manifest", manifest, error) || manifest != "SHA256SUMS") {
    if (error.empty()) error = "Run Record 0.6 has an unsupported version or association";
    return false;
  }
  yyjson_val* excludes = yyjson_obj_get(root, "hash_manifest_excludes_self");
  if (!yyjson_is_true(excludes)) {
    error = "Run Record 0.6 must exclude SHA256SUMS from itself";
    return false;
  }
  auto nullable_path = [&](const char* key, std::string_view expected, bool& present) {
    yyjson_val* value = yyjson_obj_get(root, key);
    if (yyjson_is_null(value)) {
      present = false;
      return true;
    }
    if (!yyjson_is_str(value)) {
      error = std::string{key} + " must be a string or null";
      return false;
    }
    const std::string_view path{yyjson_get_str(value), yyjson_get_len(value)};
    if (!SafeRelativePath(path)) {
      error = std::string{key} + " contains an unsafe path";
      return false;
    }
    if (path != expected) {
      error = std::string{key} + " has an unsupported file association";
      return false;
    }
    present = true;
    return true;
  };
  if (!nullable_path("values_file", kValuesFile, has_values) ||
      !nullable_path("frame_file", kFrameFile, has_frame) ||
      !nullable_path("tx_frame_file", kTxFrameFile, has_tx_frame) ||
      !nullable_path("rx_frame_file", kRxFrameFile, has_rx_frame) ||
      !nullable_path("rx_metadata_file", kRxMetadataFile, has_rx_metadata))
    return false;
  if (has_rx_metadata && !has_rx_frame) {
    error = "Run Record 0.6 RX Metadata requires an RX Frame";
    return false;
  }

  yyjson_val* array = yyjson_obj_get(root, "recorded_payload_files");
  if (!yyjson_is_arr(array) || yyjson_arr_size(array) == 0U) {
    error = "Run Record 0.6 recorded_payload_files must be a nonempty array";
    return false;
  }
  std::set<std::string> seen;
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(array, &iterator);
  while (yyjson_val* node = yyjson_arr_iter_next(&iterator)) {
    const std::set<std::string_view> descriptor_keys{"path", "size", "sha256"};
    if (!IsObject(node, descriptor_keys, descriptor_keys, "recorded payload", error)) return false;
    FileDescriptor descriptor;
    yyjson_val* size = yyjson_obj_get(node, "size");
    if (!ReadString(node, "path", descriptor.path, error) || !SafeRelativePath(descriptor.path)) {
      if (error.empty()) error = "recorded payload contains an unsafe path";
      return false;
    }
    if (!seen.insert(descriptor.path).second) {
      error = "Run Record 0.6 contains a duplicate payload path";
      return false;
    }
    if (!yyjson_is_uint(size) ||
        yyjson_get_uint(size) > (std::numeric_limits<std::size_t>::max)()) {
      error = "recorded payload size must be a non-negative integer";
      return false;
    }
    descriptor.size = static_cast<std::size_t>(yyjson_get_uint(size));
    if (!ReadString(node, "sha256", descriptor.sha256, error) || !IsLowerHash(descriptor.sha256)) {
      if (error.empty()) error = "recorded payload sha256 is invalid";
      return false;
    }
    payloads.push_back(std::move(descriptor));
  }
  return true;
}

bool VerifyEvent(std::string& text, const Result& result,
                 const std::optional<std::vector<std::uint8_t>>& frame,
                 const std::optional<std::vector<std::uint8_t>>& tx_frame,
                 const std::optional<std::vector<std::uint8_t>>& rx_frame, bool has_rx_metadata,
                 std::string_view fingerprint, std::string& error) {
  if (text.empty() || text.back() != '\n' || text.find('\n') != text.size() - 1U) {
    error = "Event 0.6 log must contain exactly one newline-terminated event";
    return false;
  }
  text.pop_back();
  yyjson_read_err read_error{};
  DocumentPtr document{
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &read_error)};
  if (!document) {
    error = "Event 0.6 is not valid strict JSON";
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> keys{
      "format_version",  "event_id",         "event_kind",
      "operation_kind",  "operation_status", "frame_file",
      "frame_length",    "frame_sha256",     "tx_frame_file",
      "tx_frame_length", "tx_frame_sha256",  "rx_frame_file",
      "rx_frame_length", "rx_frame_sha256",  "rx_metadata_file",
      "frame_origin",    "result_file",      "deterministic_fingerprint"};
  if (!IsObject(root, keys, keys, "Event 0.6", error)) return false;
  std::string format, kind, operation_kind, operation_status, result_file, declared_fingerprint;
  const bool has_any_frame = frame.has_value() || tx_frame.has_value() || rx_frame.has_value();
  yyjson_val* event_id = yyjson_obj_get(root, "event_id");
  if (!ReadString(root, "format_version", format, error) || format != kEventFormat ||
      !yyjson_is_uint(event_id) || yyjson_get_uint(event_id) != 1U ||
      !ReadString(root, "event_kind", kind, error) ||
      kind != (has_any_frame ? "FRAME_SET" : "RESULT") ||
      !ReadString(root, "operation_kind", operation_kind, error) ||
      operation_kind != result.operation_kind ||
      !ReadString(root, "operation_status", operation_status, error) ||
      operation_status != result.operation_status ||
      !ReadString(root, "result_file", result_file, error) || result_file != kResultFile ||
      !ReadString(root, "deterministic_fingerprint", declared_fingerprint, error) ||
      declared_fingerprint != fingerprint) {
    if (error.empty()) error = "Event 0.6 does not match Result 0.6";
    return false;
  }
  auto verify_frame = [&](const char* file_key, const char* length_key, const char* sha_key,
                          std::string_view expected_path,
                          const std::optional<std::vector<std::uint8_t>>& bytes) {
    yyjson_val* file = yyjson_obj_get(root, file_key);
    yyjson_val* length = yyjson_obj_get(root, length_key);
    yyjson_val* sha = yyjson_obj_get(root, sha_key);
    if (!bytes.has_value())
      return yyjson_is_null(file) && yyjson_is_null(length) && yyjson_is_null(sha);
    return yyjson_is_str(file) &&
           std::string_view{yyjson_get_str(file), yyjson_get_len(file)} == expected_path &&
           yyjson_is_uint(length) && yyjson_get_uint(length) == bytes->size() &&
           yyjson_is_str(sha) &&
           std::string_view{yyjson_get_str(sha), yyjson_get_len(sha)} == HashBytes(*bytes);
  };
  yyjson_val* origin = yyjson_obj_get(root, "frame_origin");
  yyjson_val* metadata = yyjson_obj_get(root, "rx_metadata_file");
  const bool valid_metadata =
      has_rx_metadata
          ? yyjson_is_str(metadata) && std::string_view{yyjson_get_str(metadata),
                                                        yyjson_get_len(metadata)} == kRxMetadataFile
          : yyjson_is_null(metadata);
  const bool valid_origin =
      has_any_frame
          ? yyjson_is_str(origin) && std::string_view{yyjson_get_str(origin),
                                                      yyjson_get_len(origin)} == "LAB_B_SYNTHETIC"
          : yyjson_is_null(origin);
  if (!verify_frame("frame_file", "frame_length", "frame_sha256", kFrameFile, frame) ||
      !verify_frame("tx_frame_file", "tx_frame_length", "tx_frame_sha256", kTxFrameFile,
                    tx_frame) ||
      !verify_frame("rx_frame_file", "rx_frame_length", "rx_frame_sha256", kRxFrameFile,
                    rx_frame) ||
      !valid_metadata || !valid_origin) {
    error = "Event 0.6 Frame association is inconsistent";
    return false;
  }
  return true;
}

}  // namespace

bool StandardEvidenceFileSystem::EnsureDirectory(const std::filesystem::path& path,
                                                 std::string& error) {
  std::error_code file_error;
  std::filesystem::create_directories(path, file_error);
  if (file_error || !std::filesystem::is_directory(path, file_error)) {
    error = "record root is not a writable directory";
    return false;
  }
  return true;
}

bool StandardEvidenceFileSystem::Exists(const std::filesystem::path& path, bool& exists,
                                        std::string& error) {
  std::error_code file_error;
  exists = std::filesystem::exists(path, file_error);
  if (file_error) {
    error = "cannot inspect Evidence path: " + file_error.message();
    return false;
  }
  return true;
}

bool StandardEvidenceFileSystem::CreateDirectory(const std::filesystem::path& path,
                                                 std::string& error) {
  std::error_code file_error;
  if (!std::filesystem::create_directory(path, file_error) || file_error) {
    error = "cannot create in-progress Run directory: " + file_error.message();
    return false;
  }
  return true;
}

bool StandardEvidenceFileSystem::CreateDirectories(const std::filesystem::path& path,
                                                   std::string& error) {
  if (path.empty()) return true;
  std::error_code file_error;
  std::filesystem::create_directories(path, file_error);
  if (file_error) {
    error = "cannot create Evidence directory: " + file_error.message();
    return false;
  }
  return true;
}

bool StandardEvidenceFileSystem::WriteClosedFile(const std::filesystem::path& path,
                                                 const std::uint8_t* data, std::size_t size,
                                                 std::string& error) {
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    error = "cannot create temporary Evidence file";
    return false;
  }
  if (size != 0U)
    stream.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  stream.close();
  if (!stream) {
    error = "Evidence file write or close failed";
    return false;
  }
  return true;
}

bool StandardEvidenceFileSystem::ReadFile(const std::filesystem::path& path,
                                          std::vector<std::uint8_t>& output, std::string& error) {
  return ReadDiskFile(path, output, error);
}

bool StandardEvidenceFileSystem::Rename(const std::filesystem::path& from,
                                        const std::filesystem::path& to, std::string& error) {
  std::error_code file_error;
  std::filesystem::rename(from, to, file_error);
  if (file_error) {
    error = from.extension() == ".inprogress"
                ? "cannot publish completed Run directory: " + file_error.message()
                : "cannot publish Evidence file: " + file_error.message();
    return false;
  }
  return true;
}

bool WriteEvidenceBundle(const std::filesystem::path& record_root, std::string_view run_name,
                         const BundleInput& input, EvidenceFileSystem& file_system,
                         std::filesystem::path& published_path, std::string& error) {
  published_path.clear();
  if (!SafeRelativePath(run_name) || run_name.find('/') != std::string_view::npos ||
      run_name.rfind("run_", 0U) != 0U) {
    error = "Run name must be one safe run_ directory component";
    return false;
  }
  if (!input.result.config_sha256.has_value() ||
      *input.result.config_sha256 != HashBytes(input.config_text)) {
    error = "Result 0.6 config_sha256 does not bind the original configuration";
    return false;
  }
  if (input.frame.has_value() != input.result.frame_hex.has_value() ||
      (input.frame.has_value() && *input.result.frame_hex != HexUpper(*input.frame))) {
    error = "Result 0.6 frame_hex does not bind the supplied Frame bytes";
    return false;
  }
  if (input.tx_frame.has_value() != input.result.tx_frame_hex.has_value() ||
      (input.tx_frame.has_value() && *input.result.tx_frame_hex != HexUpper(*input.tx_frame)) ||
      input.rx_frame.has_value() != input.result.rx_frame_hex.has_value() ||
      (input.rx_frame.has_value() && *input.result.rx_frame_hex != HexUpper(*input.rx_frame))) {
    error = "Result 0.6 TX/RX Hex does not bind the supplied Frame bytes";
    return false;
  }
  if (input.rx_received_from.has_value() &&
      (!input.rx_frame.has_value() || input.rx_received_from->empty())) {
    error = "RX source Metadata requires an RX Frame and non-empty source";
    return false;
  }
  std::string format_error;
  const std::string result_text = SerializeResult(input.result, format_error);
  if (!format_error.empty()) {
    error = format_error;
    return false;
  }
  const std::string fingerprint = FinalizeFingerprint(input.result, format_error);
  if (!format_error.empty()) {
    error = format_error;
    return false;
  }
  if (!file_system.EnsureDirectory(record_root, error)) return false;
  const std::filesystem::path final_path = record_root / std::filesystem::path{run_name};
  std::filesystem::path progress_path = final_path;
  progress_path += ".inprogress";
  bool final_exists = false;
  bool progress_exists = false;
  if (!file_system.Exists(final_path, final_exists, error) ||
      !file_system.Exists(progress_path, progress_exists, error))
    return false;
  if (final_exists || progress_exists) {
    error = "Evidence Bundle destination already exists";
    return false;
  }
  if (!file_system.CreateDirectory(progress_path, error)) return false;

  std::vector<FileDescriptor> files;
  if (!WriteVerified(file_system, progress_path, kConfigFile, input.config_text, files, error) ||
      (input.values_text.has_value() &&
       !WriteVerified(file_system, progress_path, kValuesFile, *input.values_text, files, error)) ||
      (input.frame.has_value() &&
       !WriteVerified(file_system, progress_path, kFrameFile, input.frame->data(),
                      input.frame->size(), files, error)) ||
      (input.tx_frame.has_value() &&
       !WriteVerified(file_system, progress_path, kTxFrameFile, input.tx_frame->data(),
                      input.tx_frame->size(), files, error)) ||
      (input.rx_frame.has_value() &&
       !WriteVerified(file_system, progress_path, kRxFrameFile, input.rx_frame->data(),
                      input.rx_frame->size(), files, error)) ||
      (input.rx_received_from.has_value() &&
       !WriteVerified(file_system, progress_path, kRxMetadataFile, SerializeRxMetadata(input),
                      files, error)) ||
      !WriteVerified(file_system, progress_path, kResultFile, result_text, files, error)) {
    return false;
  }
  const std::string event_text = SerializeEvent(input, fingerprint);
  if (!WriteVerified(file_system, progress_path, kEventFile, event_text, files, error))
    return false;
  const std::string record_text = SerializeRecord(run_name, input, fingerprint, files);
  if (!WriteVerified(file_system, progress_path, kRecordFile, record_text, files, error) ||
      !WriteVerified(file_system, progress_path, "COMPLETE", std::string_view{}, files, error)) {
    return false;
  }
  std::sort(files.begin(), files.end(),
            [](const FileDescriptor& left, const FileDescriptor& right) {
              return left.path < right.path;
            });
  std::ostringstream manifest;
  for (const auto& file : files) manifest << file.sha256 << "  " << file.path << '\n';
  if (!WriteVerified(file_system, progress_path, "SHA256SUMS", manifest.str(), files, error)) {
    return false;
  }
  StoredBundle reviewed;
  if (!LoadEvidenceBundle(progress_path, reviewed, error)) {
    error = "published Bundle preflight failed: " + error;
    return false;
  }
  if (!file_system.Rename(progress_path, final_path, error)) return false;
  published_path = final_path;
  return true;
}

bool LoadEvidenceBundleForTest(const std::filesystem::path& bundle, StoredBundle& output,
                               EvidenceFileSystem& file_system, std::string& error) {
  output = StoredBundle{};
  error.clear();
  StoredBundle candidate;
  std::set<std::string> disk_files;
  if (!PreflightBundleTree(bundle, disk_files, error)) return false;
  std::set<std::string> manifest_paths;
  if (!ParseManifest(bundle, file_system, manifest_paths, error)) return false;
  const std::set<std::string> mandatory{"COMPLETE", std::string{kConfigFile},
                                        std::string{kEventFile}, std::string{kRecordFile},
                                        std::string{kResultFile}};
  if (!std::includes(manifest_paths.begin(), manifest_paths.end(), mandatory.begin(),
                     mandatory.end())) {
    error = "Evidence Bundle 0.6 manifest omits a required file";
    return false;
  }
  std::set<std::string> expected_disk = manifest_paths;
  expected_disk.insert("SHA256SUMS");
  if (disk_files != expected_disk) {
    error = "Evidence Bundle contains an unlisted, duplicate-generation, or missing file";
    return false;
  }

  std::string record_text;
  std::vector<FileDescriptor> payloads;
  bool has_values = false;
  bool has_frame = false;
  bool has_tx_frame = false;
  bool has_rx_frame = false;
  bool has_rx_metadata = false;
  std::string record_run_id;
  std::string record_fingerprint;
  if (!ReadText(file_system, bundle / kRecordFile, record_text, error) ||
      !ParseRecord(record_text, payloads, has_values, has_frame, has_tx_frame, has_rx_frame,
                   has_rx_metadata, record_run_id, record_fingerprint, error)) {
    return false;
  }
  std::string bundle_name = bundle.filename().generic_string();
  constexpr std::string_view kInProgressSuffix = ".inprogress";
  if (bundle_name.size() > kInProgressSuffix.size() &&
      bundle_name.substr(bundle_name.size() - kInProgressSuffix.size()) == kInProgressSuffix) {
    bundle_name.resize(bundle_name.size() - kInProgressSuffix.size());
  }
  if (record_run_id != bundle_name) {
    error = "Run Record 0.6 run_id does not match the Bundle directory";
    return false;
  }
  std::set<std::string> payload_paths;
  for (const auto& descriptor : payloads) {
    if (!payload_paths.insert(descriptor.path).second) {
      error = "Run Record 0.6 contains duplicate payload paths";
      return false;
    }
    std::vector<std::uint8_t> bytes;
    if (!file_system.ReadFile(bundle / descriptor.path, bytes, error)) return false;
    if (bytes.size() != descriptor.size || HashBytes(bytes) != descriptor.sha256) {
      error = "Run Record 0.6 payload length or hash does not match the recorded file";
      return false;
    }
  }
  std::set<std::string> expected_payloads{std::string{kConfigFile}, std::string{kEventFile},
                                          std::string{kResultFile}};
  if (has_values) expected_payloads.insert(std::string{kValuesFile});
  if (has_frame) expected_payloads.insert(std::string{kFrameFile});
  if (has_tx_frame) expected_payloads.insert(std::string{kTxFrameFile});
  if (has_rx_frame) expected_payloads.insert(std::string{kRxFrameFile});
  if (has_rx_metadata) expected_payloads.insert(std::string{kRxMetadataFile});
  if (payload_paths != expected_payloads) {
    error = "Run Record 0.6 payload set is inconsistent with its file associations";
    return false;
  }
  std::set<std::string> expected_manifest = expected_payloads;
  expected_manifest.insert("COMPLETE");
  expected_manifest.insert(std::string{kRecordFile});
  if (manifest_paths != expected_manifest) {
    error = "Evidence Bundle 0.6 has a mixed generation or unsupported file set";
    return false;
  }

  if (!ReadText(file_system, bundle / kConfigFile, candidate.config_text, error)) return false;
  if (has_values) {
    std::string values;
    if (!ReadText(file_system, bundle / kValuesFile, values, error)) return false;
    candidate.values_text = std::move(values);
  }
  if (has_frame) {
    std::vector<std::uint8_t> frame;
    if (!file_system.ReadFile(bundle / kFrameFile, frame, error)) return false;
    candidate.frame = std::move(frame);
  }
  if (has_tx_frame) {
    std::vector<std::uint8_t> frame;
    if (!file_system.ReadFile(bundle / kTxFrameFile, frame, error)) return false;
    candidate.tx_frame = std::move(frame);
  }
  if (has_rx_frame) {
    std::vector<std::uint8_t> frame;
    if (!file_system.ReadFile(bundle / kRxFrameFile, frame, error)) return false;
    candidate.rx_frame = std::move(frame);
  }
  if (has_rx_metadata) {
    std::string metadata;
    std::string received_from;
    if (!ReadText(file_system, bundle / kRxMetadataFile, metadata, error) ||
        !VerifyRxMetadata(metadata, *candidate.rx_frame, received_from, error))
      return false;
    candidate.rx_received_from = std::move(received_from);
  }
  std::string result_text;
  if (!ReadText(file_system, bundle / kResultFile, result_text, error) ||
      !ParseResult(result_text, candidate.result, error))
    return false;
  candidate.deterministic_fingerprint = FinalizeFingerprint(candidate.result, error);
  if (!error.empty()) return false;
  if (record_fingerprint != candidate.deterministic_fingerprint) {
    error = "Run Record 0.6 fingerprint does not match Result 0.6";
    return false;
  }
  if (!candidate.result.config_sha256.has_value() ||
      *candidate.result.config_sha256 != HashBytes(candidate.config_text)) {
    error = "Result 0.6 config_sha256 does not match the recorded configuration";
    return false;
  }
  if (has_frame != candidate.result.frame_hex.has_value() ||
      (has_frame && *candidate.result.frame_hex != HexUpper(*candidate.frame))) {
    error = "Result 0.6 frame_hex does not match the recorded Frame";
    return false;
  }
  if (has_tx_frame != candidate.result.tx_frame_hex.has_value() ||
      (has_tx_frame && *candidate.result.tx_frame_hex != HexUpper(*candidate.tx_frame)) ||
      has_rx_frame != candidate.result.rx_frame_hex.has_value() ||
      (has_rx_frame && *candidate.result.rx_frame_hex != HexUpper(*candidate.rx_frame))) {
    error = "Result 0.6 TX/RX Hex does not match the recorded Frame bytes";
    return false;
  }
  std::string event_text;
  if (!ReadText(file_system, bundle / kEventFile, event_text, error) ||
      !VerifyEvent(event_text, candidate.result, candidate.frame, candidate.tx_frame,
                   candidate.rx_frame, has_rx_metadata, candidate.deterministic_fingerprint, error))
    return false;
  output = std::move(candidate);
  error.clear();
  return true;
}

bool LoadEvidenceBundle(const std::filesystem::path& bundle, StoredBundle& output,
                        std::string& error) {
  StandardEvidenceFileSystem file_system;
  return LoadEvidenceBundleForTest(bundle, output, file_system, error);
}

}  // namespace pae::protocol_lab::v06
