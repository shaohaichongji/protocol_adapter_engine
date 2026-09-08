#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "sha256.h"
#include "v06_evidence.h"

namespace {

using pae::protocol_lab::HashBytes;
using pae::protocol_lab::v06::BundleInput;
using pae::protocol_lab::v06::EvidenceFileSystem;
using pae::protocol_lab::v06::Result;
using pae::protocol_lab::v06::StandardEvidenceFileSystem;
using pae::protocol_lab::v06::StoredBundle;

bool Expect(bool condition, std::string_view message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

std::string ReadText(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

void WriteText(const std::filesystem::path& path, std::string_view text) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

std::string ReplaceOne(std::string text, std::string_view before, std::string_view after) {
  const std::size_t position = text.find(before);
  if (position == std::string::npos) return {};
  text.replace(position, before.size(), after);
  return text;
}

void RefreshPayloadDescriptor(const std::filesystem::path& bundle, std::string_view relative) {
  const std::filesystem::path payload = bundle / std::filesystem::path{relative};
  const std::string payload_text = ReadText(payload);
  std::string record = ReadText(bundle / "run_record_v0.6.json");
  const std::string marker = "{\"path\":\"" + std::string{relative} + "\",\"size\":";
  const std::size_t begin = record.find(marker);
  const std::size_t end = record.find('}', begin);
  const std::string replacement = marker + std::to_string(payload_text.size()) + ",\"sha256\":\"" +
                                  HashBytes(payload_text) + "\"}";
  record.replace(begin, end - begin + 1U, replacement);
  WriteText(bundle / "run_record_v0.6.json", record);
}

void RebuildManifest(const std::filesystem::path& bundle) {
  std::vector<std::string> paths;
  for (std::filesystem::recursive_directory_iterator iterator{bundle}, end; iterator != end;
       ++iterator) {
    if (!iterator->is_regular_file()) continue;
    const std::string relative =
        std::filesystem::relative(iterator->path(), bundle).generic_string();
    if (relative != "SHA256SUMS") paths.push_back(relative);
  }
  std::sort(paths.begin(), paths.end());
  std::ostringstream manifest;
  for (const auto& path : paths)
    manifest << HashBytes(ReadText(bundle / path)) << "  " << path << '\n';
  WriteText(bundle / "SHA256SUMS", manifest.str());
}

Result SuccessResult(std::string_view config, bool frame_present = true) {
  Result result;
  result.command = "inspect";
  result.operation_kind = "inspect";
  result.operation_status = "OK";
  result.exit_code = 0;
  result.config_sha256 = HashBytes(config);
  result.protocol_id = "synthetic_decimal";
  result.pipeline_id = "receive";
  result.message_id = "measurement";
  result.direction_id = "rx";
  if (frame_present) result.frame_hex = "0102";
  result.replay_mode = "DECODE_RX";
  result.replay_subject = "RX";
  result.current_execution_status = "OK";
  pae::protocol_lab::v06::FieldResult field;
  field.id = "temperature";
  field.kind = "DECIMAL64";
  field.raw_value = "1230";
  field.decimal64 = pae::protocol_lab::v06::Decimal64{123, 1};
  field.raw_kind = "INT64";
  result.fields.push_back(std::move(field));
  return result;
}

BundleInput SuccessInput(std::string values =
                             "{\"format_version\":\"pae.lab.values/0.4\",\"pipeline_id\":"
                             "\"receive\",\"message_id\":\"measurement\",\"fields\":[]}") {
  BundleInput input;
  input.config_text = "{\"schema_version\":\"0.5\",\"id\":\"synthetic_decimal\"}\n";
  input.values_text = std::move(values);
  input.frame = std::vector<std::uint8_t>{0x01U, 0x02U};
  input.tx_frame = std::vector<std::uint8_t>{0xA1U};
  input.rx_frame = std::vector<std::uint8_t>{0xB2U, 0x00U};
  input.rx_received_from = "127.0.0.1:41000";
  input.result = SuccessResult(input.config_text);
  input.result.tx_frame_hex = "A1";
  input.result.rx_frame_hex = "B200";
  return input;
}

enum class FailurePoint { CREATE, WRITE_CLOSE, REREAD, FINAL_RENAME };

class FailingFileSystem final : public EvidenceFileSystem {
 public:
  explicit FailingFileSystem(FailurePoint point) : point_(point) {}
  bool EnsureDirectory(const std::filesystem::path& path, std::string& error) override {
    return standard_.EnsureDirectory(path, error);
  }
  bool Exists(const std::filesystem::path& path, bool& exists, std::string& error) override {
    return standard_.Exists(path, exists, error);
  }
  bool CreateDirectory(const std::filesystem::path& path, std::string& error) override {
    if (point_ == FailurePoint::CREATE) {
      error = "injected create failure";
      return false;
    }
    return standard_.CreateDirectory(path, error);
  }
  bool CreateDirectories(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectories(path, error);
  }
  bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                       std::size_t size, std::string& error) override {
    const bool written = standard_.WriteClosedFile(path, data, size, error);
    if (written && point_ == FailurePoint::WRITE_CLOSE && ++write_count_ == 1U) {
      error = "injected write-close acknowledgement failure";
      return false;
    }
    return written;
  }
  bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                std::string& error) override {
    if (point_ == FailurePoint::REREAD && ++read_count_ == 1U) {
      error = "injected reread failure";
      return false;
    }
    return standard_.ReadFile(path, output, error);
  }
  bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
              std::string& error) override {
    if (point_ == FailurePoint::FINAL_RENAME && from.extension() == ".inprogress") {
      error = "injected final rename failure";
      return false;
    }
    return standard_.Rename(from, to, error);
  }

 private:
  FailurePoint point_;
  std::size_t write_count_ = 0U;
  std::size_t read_count_ = 0U;
  StandardEvidenceFileSystem standard_;
};

class RecordingFileSystem final : public EvidenceFileSystem {
 public:
  bool EnsureDirectory(const std::filesystem::path& path, std::string& error) override {
    return standard_.EnsureDirectory(path, error);
  }
  bool Exists(const std::filesystem::path& path, bool& exists, std::string& error) override {
    return standard_.Exists(path, exists, error);
  }
  bool CreateDirectory(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectory(path, error);
  }
  bool CreateDirectories(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectories(path, error);
  }
  bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                       std::size_t size, std::string& error) override {
    return standard_.WriteClosedFile(path, data, size, error);
  }
  bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                std::string& error) override {
    read_paths.push_back(path);
    return standard_.ReadFile(path, output, error);
  }
  bool Rename(const std::filesystem::path& from, const std::filesystem::path& to,
              std::string& error) override {
    return standard_.Rename(from, to, error);
  }

  std::vector<std::filesystem::path> read_paths;

 private:
  StandardEvidenceFileSystem standard_;
};

bool TestRoundTrips(const std::filesystem::path& root) {
  bool passed = true;
  StandardEvidenceFileSystem file_system;
  std::string error;
  std::filesystem::path published;
  BundleInput success = SuccessInput();
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_success", success,
                                                              file_system, published, error),
                  "success Bundle writes: " + error) &&
           passed;
  StoredBundle stored;
  passed = Expect(pae::protocol_lab::v06::LoadEvidenceBundle(published, stored, error),
                  "success Bundle loads: " + error) &&
           passed;
  passed = Expect(stored.result.fields.size() == 1U && stored.frame == success.frame &&
                      stored.tx_frame == success.tx_frame && stored.rx_frame == success.rx_frame &&
                      stored.rx_received_from == success.rx_received_from,
                  "success Bundle and RX Metadata are delivered whole") &&
           passed;

  BundleInput failure = SuccessInput();
  failure.result.operation_status = "CODEC_ERROR";
  failure.result.exit_code = 5;
  failure.result.fields.clear();
  failure.result.current_execution_status = "VALUE_NOT_REPRESENTABLE";
  failure.result.current_execution_diagnostic_id = "PAE_LAB_CODEC_VALUE_NOT_REPRESENTABLE";
  failure.result.diagnostic_id = "PAE_LAB_CODEC_VALUE_NOT_REPRESENTABLE";
  failure.result.diagnostic_detail = "synthetic failure";
  failure.result.conversion_error = "RAW_OUT_OF_RANGE";
  failure.result.failed_field_id = "temperature";
  failure.result.failed_field_index = 0U;
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_failure", failure,
                                                              file_system, published, error) &&
                      pae::protocol_lab::v06::LoadEvidenceBundle(published, stored, error) &&
                      stored.result.current_execution_status == "VALUE_NOT_REPRESENTABLE" &&
                      stored.result.fields.empty(),
                  "failure Bundle round-trips without partial fields: " + error) &&
           passed;

  BundleInput no_codec = SuccessInput();
  no_codec.result.operation_status = "INPUT_ERROR";
  no_codec.result.exit_code = 3;
  no_codec.result.fields.clear();
  no_codec.result.replay_mode = "NO_CODEC_REEXECUTION";
  no_codec.result.replay_subject = "RX";
  no_codec.result.current_execution_status = "NOT_EVALUATED";
  no_codec.result.diagnostic_id = "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT";
  no_codec.result.diagnostic_detail = "synthetic no-codec case";
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_no_codec", no_codec,
                                                              file_system, published, error) &&
                      pae::protocol_lab::v06::LoadEvidenceBundle(published, stored, error) &&
                      stored.result.current_execution_status == "NOT_EVALUATED",
                  "NO_CODEC Bundle round-trips without invented execution: " + error) &&
           passed;
  return passed;
}

bool TestRepresentationAndFrameDistinction(const std::filesystem::path& root) {
  bool passed = true;
  StandardEvidenceFileSystem file_system;
  std::string error;
  std::filesystem::path first_path, second_path;
  BundleInput first = SuccessInput("{\"decimal64\":{\"coefficient\":\"1230\",\"scale\":2}}\n");
  BundleInput second = SuccessInput("{\"decimal64\":{\"coefficient\":\"123\",\"scale\":1}}\n");
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_decimal_a", first,
                                                              file_system, first_path, error) &&
                      pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_decimal_b", second,
                                                                  file_system, second_path, error),
                  "equivalent Decimal source Bundles write: " + error) &&
           passed;
  StoredBundle first_stored, second_stored;
  passed =
      Expect(pae::protocol_lab::v06::LoadEvidenceBundle(first_path, first_stored, error) &&
                 pae::protocol_lab::v06::LoadEvidenceBundle(second_path, second_stored, error) &&
                 HashBytes(*first_stored.values_text) != HashBytes(*second_stored.values_text) &&
                 first_stored.deterministic_fingerprint == second_stored.deterministic_fingerprint,
             "different raw Values hashes retain one normalized execution fingerprint") &&
      passed;

  BundleInput absent = SuccessInput();
  absent.frame.reset();
  absent.result = SuccessResult(absent.config_text, false);
  absent.result.tx_frame_hex = "A1";
  absent.result.rx_frame_hex = "B200";
  BundleInput empty = absent;
  empty.frame = std::vector<std::uint8_t>{};
  empty.result.frame_hex = "";
  std::filesystem::path absent_path, empty_path;
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_no_frame", absent,
                                                              file_system, absent_path, error) &&
                      pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_empty_frame", empty,
                                                                  file_system, empty_path, error),
                  "absent and zero-length Frame Bundles write: " + error) &&
           passed;
  StoredBundle absent_stored, empty_stored;
  passed =
      Expect(pae::protocol_lab::v06::LoadEvidenceBundle(absent_path, absent_stored, error) &&
                 pae::protocol_lab::v06::LoadEvidenceBundle(empty_path, empty_stored, error) &&
                 !absent_stored.frame.has_value() && empty_stored.frame.has_value() &&
                 empty_stored.frame->empty() &&
                 absent_stored.deterministic_fingerprint != empty_stored.deterministic_fingerprint,
             "null Frame and present zero-length Frame remain distinct") &&
      passed;
  return passed;
}

bool TestTransactionFailures(const std::filesystem::path& root) {
  bool passed = true;
  std::size_t ordinal = 0U;
  for (FailurePoint point : {FailurePoint::CREATE, FailurePoint::WRITE_CLOSE, FailurePoint::REREAD,
                             FailurePoint::FINAL_RENAME}) {
    const std::string name = "run_fault_" + std::to_string(ordinal++);
    FailingFileSystem file_system{point};
    std::filesystem::path published;
    std::string error;
    const bool wrote = pae::protocol_lab::v06::WriteEvidenceBundle(root, name, SuccessInput(),
                                                                   file_system, published, error);
    passed = Expect(!wrote && published.empty() && !std::filesystem::exists(root / name),
                    "transaction failure does not publish a completed Bundle") &&
             passed;
  }
  StandardEvidenceFileSystem file_system;
  std::filesystem::path published;
  std::string error;
  BundleInput input = SuccessInput();
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_no_overwrite", input,
                                                              file_system, published, error),
                  "overwrite fixture writes") &&
           passed;
  std::filesystem::path ignored;
  passed = Expect(!pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_no_overwrite", input,
                                                               file_system, ignored, error) &&
                      error == "Evidence Bundle destination already exists",
                  "writer refuses to overwrite a published Bundle") &&
           passed;
  return passed;
}

std::filesystem::path CopyBundle(const std::filesystem::path& source,
                                 const std::filesystem::path& destination) {
  std::filesystem::copy(source, destination, std::filesystem::copy_options::recursive);
  std::string record = ReadText(destination / "run_record_v0.6.json");
  record = ReplaceOne(record, "\"run_id\":\"" + source.filename().generic_string() + "\"",
                      "\"run_id\":\"" + destination.filename().generic_string() + "\"");
  WriteText(destination / "run_record_v0.6.json", record);
  RebuildManifest(destination);
  return destination;
}

bool IsDefaultResult(const Result& result) {
  return result.command.empty() && result.operation_kind.empty() &&
         result.operation_status.empty() && result.exit_code == 0 &&
         !result.config_sha256.has_value() && !result.protocol_id.has_value() &&
         !result.pipeline_id.has_value() && !result.message_id.has_value() &&
         !result.direction_id.has_value() && !result.frame_hex.has_value() &&
         !result.tx_frame_hex.has_value() && !result.rx_frame_hex.has_value() &&
         result.fields.empty() && !result.diagnostic_id.has_value() &&
         result.diagnostic_detail.empty() && result.replay_mode == "NONE" &&
         result.replay_subject == "NONE" && result.current_execution_status == "NOT_EVALUATED" &&
         !result.current_execution_diagnostic_id.has_value() &&
         !result.conversion_error.has_value() && !result.failed_field_id.has_value() &&
         !result.failed_field_index.has_value() && !result.failed_value_index.has_value();
}

bool IsDefaultStoredBundle(const StoredBundle& stored) {
  return stored.config_text.empty() && !stored.values_text.has_value() &&
         !stored.frame.has_value() && !stored.tx_frame.has_value() &&
         !stored.rx_frame.has_value() && !stored.rx_received_from.has_value() &&
         IsDefaultResult(stored.result) && stored.deterministic_fingerprint.empty();
}

bool Rejects(const std::filesystem::path& bundle, std::string_view expected) {
  StoredBundle stored;
  stored.config_text = "stale config";
  stored.values_text = "stale values";
  stored.frame = std::vector<std::uint8_t>{0xFFU};
  stored.tx_frame = std::vector<std::uint8_t>{0xFEU};
  stored.rx_frame = std::vector<std::uint8_t>{0xFDU};
  stored.rx_received_from = "stale source";
  stored.result = SuccessResult("stale config");
  stored.deterministic_fingerprint = "stale fingerprint";
  std::string error;
  const bool loaded = pae::protocol_lab::v06::LoadEvidenceBundle(bundle, stored, error);
  return Expect(
      !loaded && error.find(expected) != std::string::npos && IsDefaultStoredBundle(stored),
      "expected whole-delivery rejection containing '" + std::string{expected} +
          "', got: " + error);
}

bool TestReaderNegatives(const std::filesystem::path& root) {
  bool passed = true;
  StandardEvidenceFileSystem file_system;
  std::filesystem::path source;
  std::string error;
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(
                      root, "run_negative_source", SuccessInput(), file_system, source, error),
                  "negative source Bundle writes: " + error) &&
           passed;

  auto missing = CopyBundle(source, root / "negative_missing");
  std::filesystem::remove(missing / "events_v0.6.jsonl");
  passed = Rejects(missing, "cannot determine Evidence file size") && passed;

  auto hash_error = CopyBundle(source, root / "negative_hash");
  WriteText(hash_error / "inputs/protocol.pae.json", "changed");
  passed = Rejects(hash_error, "digest does not match") && passed;

  auto duplicate_manifest = CopyBundle(source, root / "negative_manifest_duplicate");
  std::string sums = ReadText(duplicate_manifest / "SHA256SUMS");
  const std::size_t first_line_end = sums.find('\n') + 1U;
  sums.insert(first_line_end, sums.substr(0U, first_line_end));
  WriteText(duplicate_manifest / "SHA256SUMS", sums);
  passed = Rejects(duplicate_manifest, "duplicate or not strictly sorted") && passed;

  auto mixed = CopyBundle(source, root / "negative_mixed");
  WriteText(mixed / "result_summary_v0.5.json", "{}\n");
  RebuildManifest(mixed);
  passed = Rejects(mixed, "mixed generation") && passed;

  auto traversal = CopyBundle(source, root / "negative_traversal");
  std::string record = ReadText(traversal / "run_record_v0.6.json");
  record = ReplaceOne(record, "\"values_file\":\"inputs/values.pae-lab.json\"",
                      "\"values_file\":\"../values.pae-lab.json\"");
  WriteText(traversal / "run_record_v0.6.json", record);
  RebuildManifest(traversal);
  passed = Rejects(traversal, "unsafe path") && passed;

  auto duplicate = CopyBundle(source, root / "negative_duplicate");
  record = ReadText(duplicate / "run_record_v0.6.json");
  const std::string descriptor_prefix = "    {\"path\":\"inputs/protocol.pae.json\"";
  const std::size_t descriptor_begin = record.find(descriptor_prefix);
  const std::size_t descriptor_end = record.find('}', descriptor_begin) + 1U;
  record.insert(descriptor_end,
                ",\n" + record.substr(descriptor_begin, descriptor_end - descriptor_begin));
  WriteText(duplicate / "run_record_v0.6.json", record);
  RebuildManifest(duplicate);
  passed = Rejects(duplicate, "duplicate payload path") && passed;

  auto wrong_run_id = CopyBundle(source, root / "negative_run_id");
  record = ReadText(wrong_run_id / "run_record_v0.6.json");
  record = ReplaceOne(record, "\"run_id\":\"negative_run_id\"", "\"run_id\":\"run_different\"");
  WriteText(wrong_run_id / "run_record_v0.6.json", record);
  RebuildManifest(wrong_run_id);
  passed = Rejects(wrong_run_id, "run_id does not match") && passed;

  auto semantic = CopyBundle(source, root / "negative_semantic");
  std::string result = ReadText(semantic / "result_summary_v0.6.json");
  result = ReplaceOne(result, "\n}\n", ",\n  \"unknown_property\":true\n}\n");
  WriteText(semantic / "result_summary_v0.6.json", result);
  RefreshPayloadDescriptor(semantic, "result_summary_v0.6.json");
  RebuildManifest(semantic);
  const std::string manifest = ReadText(semantic / "SHA256SUMS");
  const std::string semantic_record = ReadText(semantic / "run_record_v0.6.json");
  passed =
      Expect(manifest.find(HashBytes(result) + "  result_summary_v0.6.json") != std::string::npos &&
                 manifest.find(HashBytes(semantic_record) + "  run_record_v0.6.json") !=
                     std::string::npos,
             "self-consistent semantic fixture has refreshed payload and Record hashes") &&
      passed;
  passed = Rejects(semantic, "unknown property unknown_property") && passed;

  auto event_semantic = CopyBundle(source, root / "negative_event_semantic");
  std::string event = ReadText(event_semantic / "events_v0.6.jsonl");
  event = ReplaceOne(event, "}\n", ",\"unknown_event_property\":true}\n");
  WriteText(event_semantic / "events_v0.6.jsonl", event);
  RefreshPayloadDescriptor(event_semantic, "events_v0.6.jsonl");
  RebuildManifest(event_semantic);
  const std::string event_record = ReadText(event_semantic / "run_record_v0.6.json");
  passed =
      Expect(
          ReadText(event_semantic / "SHA256SUMS").find(HashBytes(event) + "  events_v0.6.jsonl") !=
                  std::string::npos &&
              ReadText(event_semantic / "SHA256SUMS")
                      .find(HashBytes(event_record) + "  run_record_v0.6.json") !=
                  std::string::npos,
          "self-consistent Event fixture refreshes length and all associated hashes") &&
      passed;
  passed = Rejects(event_semantic, "unknown property unknown_event_property") && passed;

  StoredBundle reused;
  passed = Expect(pae::protocol_lab::v06::LoadEvidenceBundle(source, reused, error) &&
                      !IsDefaultStoredBundle(reused),
                  "valid Bundle populates reusable output before rejection check: " + error) &&
           passed;
  const bool reused_loaded =
      pae::protocol_lab::v06::LoadEvidenceBundle(event_semantic, reused, error);
  passed = Expect(!reused_loaded &&
                      error.find("unknown property unknown_event_property") != std::string::npos &&
                      IsDefaultStoredBundle(reused),
                  "failed reload clears every observable field from prior successful output") &&
           passed;
  return passed;
}

bool TestPathGatePrecedesContentReads(const std::filesystem::path& root) {
  bool passed = true;
  StandardEvidenceFileSystem file_system;
  std::filesystem::path source;
  std::string error;
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(
                      root, "run_path_source", SuccessInput(), file_system, source, error),
                  "path-gate source Bundle writes: " + error) &&
           passed;

  auto reject_without_reads = [&](const std::filesystem::path& bundle,
                                  std::string_view fixture_name) {
    RecordingFileSystem recording;
    StoredBundle stored;
    std::string load_error;
    const bool loaded =
        pae::protocol_lab::v06::LoadEvidenceBundleForTest(bundle, stored, recording, load_error);
    return Expect(!loaded && load_error == "Evidence Bundle contains a symlink or reparse point" &&
                      recording.read_paths.empty() && IsDefaultStoredBundle(stored),
                  std::string{fixture_name} +
                      " is rejected by the path gate before any content read; got: " + load_error);
  };

  auto manifest_link = CopyBundle(source, root / "negative_manifest_link");
  const auto external_manifest = root / "controlled_external_manifest.txt";
  WriteText(external_manifest, ReadText(manifest_link / "SHA256SUMS"));
  std::filesystem::remove(manifest_link / "SHA256SUMS");
  std::error_code link_error;
  std::filesystem::create_symlink(external_manifest, manifest_link / "SHA256SUMS", link_error);
  passed = Expect(!link_error,
                  "test environment creates the manifest file symlink: " + link_error.message()) &&
           passed;
  if (!link_error) passed = reject_without_reads(manifest_link, "manifest symlink") && passed;

  auto payload_link = CopyBundle(source, root / "negative_payload_link");
  const auto external_config = root / "controlled_external_config.json";
  WriteText(external_config, ReadText(payload_link / "inputs/protocol.pae.json"));
  std::filesystem::remove(payload_link / "inputs/protocol.pae.json");
  link_error.clear();
  std::filesystem::create_symlink(external_config, payload_link / "inputs/protocol.pae.json",
                                  link_error);
  passed = Expect(!link_error,
                  "test environment creates the payload file symlink: " + link_error.message()) &&
           passed;
  if (!link_error) passed = reject_without_reads(payload_link, "payload symlink") && passed;

  auto directory_link = CopyBundle(source, root / "negative_directory_link");
  const auto external_inputs = root / "controlled_external_inputs";
  std::filesystem::copy(directory_link / "inputs", external_inputs,
                        std::filesystem::copy_options::recursive);
  std::filesystem::remove_all(directory_link / "inputs");
  link_error.clear();
  std::filesystem::create_directory_symlink(external_inputs, directory_link / "inputs", link_error);
  passed = Expect(!link_error, "test environment creates the intermediate directory symlink: " +
                                   link_error.message()) &&
           passed;
  if (!link_error)
    passed = reject_without_reads(directory_link, "intermediate directory symlink") && passed;
  return passed;
}

}  // namespace

int main() {
  const std::filesystem::path root = "v06-evidence-runs";
  std::error_code cleanup_error;
  std::filesystem::remove_all(root, cleanup_error);
  std::filesystem::create_directories(root);
  bool passed = true;
  passed = TestRoundTrips(root) && passed;
  passed = TestRepresentationAndFrameDistinction(root) && passed;
  passed = TestTransactionFailures(root) && passed;
  passed = TestReaderNegatives(root) && passed;
  passed = TestPathGatePrecedesContentReads(root) && passed;
  return passed ? 0 : 1;
}
