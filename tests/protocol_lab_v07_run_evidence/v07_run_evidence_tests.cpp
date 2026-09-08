#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "sha256.h"
#include "v07_run.h"

namespace {

using pae::protocol_lab::HashBytes;
using pae::protocol_lab::v06::BundleInput;
using pae::protocol_lab::v06::EvidenceFileSystem;
using pae::protocol_lab::v06::ExecutionTestHooks;
using pae::protocol_lab::v06::Result;
using pae::protocol_lab::v06::StandardEvidenceFileSystem;
using pae::protocol_lab::v07::RunPublishOutcome;
using pae::protocol_lab::v07::RunRequest;
using pae::protocol_lab::v07::StoredRunBundle;

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

bool ReplaceAll(std::string& text, std::string_view before, std::string_view after) {
  std::size_t position = 0U;
  std::size_t count = 0U;
  while ((position = text.find(before, position)) != std::string::npos) {
    text.replace(position, before.size(), after);
    position += after.size();
    ++count;
  }
  return count != 0U;
}

bool RefreshDescriptorText(std::string& record, std::string_view relative, std::string_view payload,
                           std::string& error) {
  const std::string marker = "{\"path\":\"" + std::string{relative} + "\",\"size\":";
  const std::size_t begin = record.find(marker);
  const std::size_t end = record.find('}', begin);
  if (begin == std::string::npos || end == std::string::npos) {
    error = "test helper could not locate payload descriptor " + std::string{relative};
    return false;
  }
  const std::string replacement =
      marker + std::to_string(payload.size()) + ",\"sha256\":\"" + HashBytes(payload) + "\"}";
  record.replace(begin, end - begin + 1U, replacement);
  return true;
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

bool ManifestHashesMatch(const std::filesystem::path& bundle) {
  std::istringstream input{ReadText(bundle / "SHA256SUMS")};
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.size() < 67U || line[64] != ' ' || line[65] != ' ') return false;
    const std::string hash = line.substr(0U, 64U);
    const std::string relative = line.substr(66U);
    if (HashBytes(ReadText(bundle / relative)) != hash) return false;
  }
  return true;
}

bool RecordDescriptorMatches(const std::filesystem::path& record_path,
                             const std::filesystem::path& bundle, std::string_view relative) {
  const std::string payload = ReadText(bundle / std::filesystem::path{relative});
  const std::string expected = "{\"path\":\"" + std::string{relative} +
                               "\",\"size\":" + std::to_string(payload.size()) + ",\"sha256\":\"" +
                               HashBytes(payload) + "\"}";
  return ReadText(record_path).find(expected) != std::string::npos;
}

bool ReplayHistoryLinksMatch(const std::filesystem::path& bundle) {
  const std::string parent = ReadText(bundle / "history/parent_record_v0.7.json");
  const std::string result = ReadText(bundle / "history/result_summary_v0.6.json");
  const std::string child = ReadText(bundle / "run_record_v0.7.json");
  return ManifestHashesMatch(bundle) &&
         RecordDescriptorMatches(bundle / "run_record_v0.7.json", bundle,
                                 "history/parent_record_v0.7.json") &&
         RecordDescriptorMatches(bundle / "run_record_v0.7.json", bundle,
                                 "history/result_summary_v0.6.json") &&
         child.find("\"parent_record_sha256\":\"" + HashBytes(parent) + "\"") !=
             std::string::npos &&
         child.find("\"result_sha256\":\"" + HashBytes(result) + "\"") != std::string::npos;
}

void RefreshPayloadDescriptor(const std::filesystem::path& bundle, std::string_view relative) {
  const std::string payload = ReadText(bundle / std::filesystem::path{relative});
  std::string record = ReadText(bundle / "run_record_v0.7.json");
  std::string error;
  if (!RefreshDescriptorText(record, relative, payload, error)) {
    std::cerr << "FAILED: " << error << '\n';
    return;
  }
  WriteText(bundle / "run_record_v0.7.json", record);
}

bool RewriteParentRecordAndRefresh(const std::filesystem::path& bundle,
                                   const std::function<void(std::string&)>& mutate,
                                   std::string& error) {
  const std::filesystem::path path = bundle / "history/parent_record_v0.7.json";
  const std::string original = ReadText(path);
  std::string changed = original;
  mutate(changed);
  if (changed == original) {
    error = "test helper parent Record mutation made no change";
    return false;
  }
  WriteText(path, changed);
  std::string child = ReadText(bundle / "run_record_v0.7.json");
  const std::string before = "\"parent_record_sha256\":\"" + HashBytes(original) + "\"";
  const std::string after = "\"parent_record_sha256\":\"" + HashBytes(changed) + "\"";
  if (!ReplaceAll(child, before, after) ||
      !RefreshDescriptorText(child, "history/parent_record_v0.7.json", changed, error)) {
    if (error.empty()) error = "test helper could not refresh parent Record references";
    return false;
  }
  WriteText(bundle / "run_record_v0.7.json", child);
  RebuildManifest(bundle);
  return true;
}

bool RewriteHistoricalResultAndRefresh(const std::filesystem::path& bundle,
                                       const std::function<void(Result&)>& mutate,
                                       std::string& error) {
  const std::filesystem::path result_path = bundle / "history/result_summary_v0.6.json";
  const std::filesystem::path parent_path = bundle / "history/parent_record_v0.7.json";
  const std::string original_result = ReadText(result_path);
  const std::string original_parent = ReadText(parent_path);
  Result result;
  std::string parse_text = original_result;
  if (!pae::protocol_lab::v06::ParseResult(parse_text, result, error)) return false;
  const std::string original_fingerprint =
      pae::protocol_lab::v06::FinalizeFingerprint(result, error);
  if (!error.empty()) return false;
  mutate(result);
  const std::string changed_fingerprint =
      pae::protocol_lab::v06::FinalizeFingerprint(result, error);
  const std::string changed_result = pae::protocol_lab::v06::SerializeResult(result, error);
  if (!error.empty()) return false;

  std::string changed_parent = original_parent;
  const std::string old_fingerprint =
      "\"deterministic_fingerprint\":\"" + original_fingerprint + "\"";
  const std::string new_fingerprint =
      "\"deterministic_fingerprint\":\"" + changed_fingerprint + "\"";
  if (!ReplaceAll(changed_parent, old_fingerprint, new_fingerprint) ||
      !RefreshDescriptorText(changed_parent, "result_summary_v0.6.json", changed_result, error)) {
    if (error.empty()) error = "test helper could not refresh historical parent Result binding";
    return false;
  }
  WriteText(result_path, changed_result);
  WriteText(parent_path, changed_parent);

  std::string child = ReadText(bundle / "run_record_v0.7.json");
  const std::string old_result_hash = "\"result_sha256\":\"" + HashBytes(original_result) + "\"";
  const std::string new_result_hash = "\"result_sha256\":\"" + HashBytes(changed_result) + "\"";
  const std::string old_parent_hash =
      "\"parent_record_sha256\":\"" + HashBytes(original_parent) + "\"";
  const std::string new_parent_hash =
      "\"parent_record_sha256\":\"" + HashBytes(changed_parent) + "\"";
  const std::string old_history_fingerprint =
      "\"fingerprint_domain\":\"pae.lab.fingerprint/0.6\",\"deterministic_fingerprint\":\"" +
      original_fingerprint + "\"";
  const std::string new_history_fingerprint =
      "\"fingerprint_domain\":\"pae.lab.fingerprint/0.6\",\"deterministic_fingerprint\":\"" +
      changed_fingerprint + "\"";
  if (!ReplaceAll(child, old_result_hash, new_result_hash) ||
      !ReplaceAll(child, old_parent_hash, new_parent_hash) ||
      !ReplaceAll(child, old_history_fingerprint, new_history_fingerprint) ||
      !ReplaceAll(child, "\"status\":\"EQUAL\",\"reason\":\"FINGERPRINT_EQUAL\"",
                  "\"status\":\"DIFFERENT\",\"reason\":\"FINGERPRINT_DIFFERENT\"") ||
      !RefreshDescriptorText(child, "history/parent_record_v0.7.json", changed_parent, error) ||
      !RefreshDescriptorText(child, "history/result_summary_v0.6.json", changed_result, error)) {
    if (error.empty()) error = "test helper could not refresh child historical references";
    return false;
  }
  WriteText(bundle / "run_record_v0.7.json", child);
  RebuildManifest(bundle);
  return true;
}

bool RewriteResultAndRefresh(const std::filesystem::path& bundle,
                             const std::function<void(Result&)>& mutate, std::string& error) {
  std::string original = ReadText(bundle / "result_summary_v0.6.json");
  Result result;
  if (!pae::protocol_lab::v06::ParseResult(original, result, error)) return false;
  mutate(result);
  const std::string fingerprint = pae::protocol_lab::v06::FinalizeFingerprint(result, error);
  if (!error.empty()) return false;
  const std::string serialized = pae::protocol_lab::v06::SerializeResult(result, error);
  if (!error.empty()) return false;
  WriteText(bundle / "result_summary_v0.6.json", serialized);
  RefreshPayloadDescriptor(bundle, "result_summary_v0.6.json");
  std::string record = ReadText(bundle / "run_record_v0.7.json");
  constexpr std::string_view marker = "\"deterministic_fingerprint\":\"";
  const std::size_t begin = record.find(marker);
  const std::size_t value_begin = begin == std::string::npos ? begin : begin + marker.size();
  const std::size_t value_end =
      value_begin == std::string::npos ? value_begin : record.find('"', value_begin);
  if (begin == std::string::npos || value_end == std::string::npos) {
    error = "test helper could not locate the Record fingerprint";
    return false;
  }
  record.replace(value_begin, value_end - value_begin, fingerprint);
  WriteText(bundle / "run_record_v0.7.json", record);
  RebuildManifest(bundle);
  return true;
}

bool RemoveFirstResultMappingPair(const std::filesystem::path& bundle, std::string& error) {
  std::istringstream input{ReadText(bundle / "events_v0.7.jsonl")};
  std::vector<std::string> lines;
  std::string line;
  bool removing = false;
  bool removed = false;
  while (std::getline(input, line)) {
    if (!removed && !removing &&
        line.find("\"event_kind\":\"PHASE_STARTED\"") != std::string::npos &&
        line.find("\"phase\":\"RESULT_MAPPING\"") != std::string::npos) {
      removing = true;
      continue;
    }
    if (removing) {
      if (line.find("\"event_kind\":\"PHASE_FINISHED\"") == std::string::npos ||
          line.find("\"phase\":\"RESULT_MAPPING\"") == std::string::npos) {
        error = "test helper found a non-adjacent Result mapping event";
        return false;
      }
      removing = false;
      removed = true;
      continue;
    }
    lines.push_back(std::move(line));
  }
  if (!removed || removing) {
    error = "test helper could not remove a Result mapping pair";
    return false;
  }
  std::ostringstream output;
  for (std::size_t index = 0U; index < lines.size(); ++index) {
    const std::size_t id_begin = lines[index].find("\"event_id\":");
    const std::size_t value_begin = id_begin + std::string_view{"\"event_id\":"}.size();
    const std::size_t value_end = lines[index].find(',', value_begin);
    if (id_begin == std::string::npos || value_end == std::string::npos) {
      error = "test helper could not renumber an Event";
      return false;
    }
    lines[index].replace(value_begin, value_end - value_begin, std::to_string(index + 1U));
    output << lines[index] << '\n';
  }
  WriteText(bundle / "events_v0.7.jsonl", output.str());
  RefreshPayloadDescriptor(bundle, "events_v0.7.jsonl");
  RebuildManifest(bundle);
  return true;
}

std::string ValidValues() {
  return R"json({"format_version":"pae.lab.values/0.4","pipeline_id":"sample_pipeline","message_id":"sample","fields":[{"id":"temperature","kind":"DECIMAL64","decimal64":{"coefficient":"123","scale":1}},{"id":"cancelled","kind":"DECIMAL64","decimal64":{"coefficient":"9223372036854775807","scale":0}},{"id":"overflow_probe","kind":"DECIMAL64","decimal64":{"coefficient":"0","scale":0}},{"id":"signed_identity","kind":"DECIMAL64","decimal64":{"coefficient":"9223372036854775807","scale":0}}]})json";
}

std::string FailingValues() {
  std::string values = ValidValues();
  return ReplaceOne(values, "\"123\",\"scale\":1", "\"1235\",\"scale\":2");
}

const std::vector<std::uint8_t>& ValidFrame() {
  static const std::vector<std::uint8_t> frame{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0B, 0xFF,
                                               0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
                                               0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x2A};
  return frame;
}

std::string NoConversionConfig() {
  return R"json({"schema_version":"0.5","protocol_id":"c2_no_conversion","protocol_version":"1","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","resource_profile":"desktop","framing_profiles":[{"id":"record","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","input_kind":"complete_record"}],"pipelines":[{"id":"p","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","input_framing_profile_id":"record","message_ids":["m"]}],"messages":[{"id":"m","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},"fields":[{"id":"value","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},"encode":{"source":"input"}}]}]})json";
}

std::string LegacyUintValues() {
  return R"json({"format_version":"pae.lab.values/0.1","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"UINT64","uint64":"42"}]})json";
}

std::string MismatchedIntValues() {
  return R"json({"format_version":"pae.lab.values/0.3","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"INT64","int64":"1"}]})json";
}

std::string SingleFieldConfig(std::string_view value_type, std::string_view wire) {
  return std::string{
             R"json({"schema_version":"0.5","protocol_id":"c2_failure_applicability","protocol_version":"1","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","resource_profile":"desktop","framing_profiles":[{"id":"record","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","input_kind":"complete_record"}],"pipelines":[{"id":"p","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","input_framing_profile_id":"record","message_ids":["m"]}],"messages":[{"id":"m","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","frame_length_bytes":2,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":2}]},"fields":[{"id":"value","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","value_type":")json"} +
         std::string{value_type} + R"json(","wire":)json" + std::string{wire} +
         R"json(,"encode":{"source":"input"}}]}]})json";
}

std::string BytesConfig() {
  return SingleFieldConfig("BYTES", R"json({"codec":"bytes","byte_offset":0,"byte_length":2})json");
}

std::string Int64Config() {
  return SingleFieldConfig(
      "INT64",
      R"json({"codec":"unsigned_integer","byte_offset":0,"byte_width":2,"byte_order":"big_endian"})json");
}

std::string WrongLengthBytesValues() {
  return R"json({"format_version":"pae.lab.values/0.4","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"BYTES","hex":"AA"}]})json";
}

std::string MatchingInt64Values() {
  return R"json({"format_version":"pae.lab.values/0.3","pipeline_id":"p","message_id":"m","fields":[{"id":"value","kind":"INT64","int64":"1"}]})json";
}

std::string AmbiguousConfig() {
  return R"json({"schema_version":"0.5","protocol_id":"c2_ambiguous","protocol_version":"1","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","resource_profile":"desktop","framing_profiles":[{"id":"record","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","input_kind":"complete_record"}],"pipelines":[{"id":"p1","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","input_framing_profile_id":"record","message_ids":["m1"]},{"id":"p2","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","input_framing_profile_id":"record","message_ids":["m2"]}],"messages":[{"id":"m1","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},"fields":[{"id":"a","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},"encode":{"source":"input"}}]},{"id":"m2","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","direction_id":"rx","frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},"fields":[{"id":"b","display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:C2","value_type":"UINT64","wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},"encode":{"source":"input"}}]}]})json";
}

RunRequest EncodeRequest(std::string run_id, std::string_view config,
                         std::string values = ValidValues()) {
  RunRequest request;
  request.run_id = std::move(run_id);
  request.tool_version = "0.1.0-test";
  request.operation_kind = "encode";
  request.config_text = std::string{config};
  request.values_text = std::move(values);
  return request;
}

RunRequest InspectRequest(std::string run_id, std::string_view config,
                          std::vector<std::uint8_t> frame = ValidFrame()) {
  RunRequest request;
  request.run_id = std::move(run_id);
  request.tool_version = "0.1.0-test";
  request.operation_kind = "inspect";
  request.config_text = std::string{config};
  request.frame = std::move(frame);
  return request;
}

bool IsPhaseSequence(const StoredRunBundle& stored, const std::vector<std::string_view>& phases) {
  std::vector<std::string_view> actual;
  for (const auto& event : stored.events) {
    if (event.event_kind == "PHASE_STARTED") actual.push_back(event.phase);
  }
  return actual == phases;
}

bool Execute(const std::filesystem::path& root, const RunRequest& request,
             RunPublishOutcome& outcome) {
  StandardEvidenceFileSystem file_system;
  const bool success =
      pae::protocol_lab::v07::ExecuteRunAndWrite(root, request, file_system, outcome);
  if (!success)
    std::cerr << "RUN publication failure for " << request.run_id << ": "
              << outcome.publication_error << '\n';
  return success;
}

bool TestSuccessfulRuns(const std::filesystem::path& root, std::string_view config) {
  bool passed = true;
  RunPublishOutcome encoded;
  passed = Expect(Execute(root, EncodeRequest("run_encode_ok", config), encoded),
                  "successful Encode RUN publishes: " + encoded.publication_error) &&
           passed;
  StoredRunBundle stored;
  std::string error;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(encoded.published_path, stored, error),
                  "successful Encode RUN strictly reloads: " + error) &&
           passed;
  passed = Expect(stored.record.execution.terminal_status == "OK" && stored.result.has_value() &&
                      stored.config_text == config && stored.values_text == ValidValues() &&
                      stored.frame == ValidFrame() &&
                      stored.record.deterministic_fingerprint.has_value() &&
                      stored.record.execution.counts.encode_calls == 1U &&
                      stored.record.execution.counts.review_decode_calls == 1U &&
                      IsPhaseSequence(stored, {"PREPARATION", "MAIN_CODEC", "REVIEW_DECODE",
                                               "RESULT_MAPPING", "RESULT_MAPPING"}),
                  "Encode stores real phase order, counts, Result and fingerprint") &&
           passed;

  RunPublishOutcome inspected;
  passed = Expect(Execute(root, InspectRequest("run_inspect_ok", config), inspected),
                  "successful Inspect RUN publishes: " + inspected.publication_error) &&
           passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(inspected.published_path, stored, error) &&
                      stored.config_text == config && stored.frame == ValidFrame() &&
                      stored.record.execution.terminal_status == "OK" &&
                      stored.record.execution.counts.structural_query_calls == 1U &&
                      stored.record.execution.counts.decode_calls == 1U &&
                      IsPhaseSequence(stored, {"PREPARATION", "STRUCTURAL_QUERY", "MAIN_CODEC",
                                               "RESULT_MAPPING", "RESULT_MAPPING"}),
                  "Inspect stores global structure, one Decode and two real mappings") &&
           passed;

  RunPublishOutcome inherited;
  passed =
      Expect(Execute(root,
                     EncodeRequest("run_no_conversion", NoConversionConfig(), LegacyUintValues()),
                     inherited) &&
                 pae::protocol_lab::v07::LoadRunBundle(inherited.published_path, stored, error) &&
                 stored.result.has_value() && stored.frame == std::vector<std::uint8_t>{42U},
             "Schema 0.5 without conversion and legacy Values still use C RUN 0.7/Result 0.6") &&
      passed;
  return passed;
}

bool TestFailureRuns(const std::filesystem::path& root, std::string_view config) {
  bool passed = true;
  StoredRunBundle stored;
  std::string error;
  RunPublishOutcome outcome;

  passed =
      Expect(Execute(root, EncodeRequest("run_prepare_failure", config, "{}"), outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "PREPARATION_FAILED" &&
                 !stored.result.has_value() &&
                 !stored.record.deterministic_fingerprint.has_value() &&
                 stored.record.execution.counts.encode_calls == 0U &&
                 IsPhaseSequence(stored, {"PREPARATION"}),
             "preparation failure has no Codec, Result or fingerprint") &&
      passed;

  passed =
      Expect(Execute(root, EncodeRequest("run_config_failure", "{}"), outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "PREPARATION_FAILED" &&
                 stored.record.execution.preparation.diagnostic_id == "PAE_LAB_C1_CONFIG_INVALID" &&
                 stored.config_text == "{}" && stored.values_text == ValidValues() &&
                 stored.record.execution.counts.encode_calls == 0U && !stored.result,
             "invalid configuration bytes are retained without claiming independent replay") &&
      passed;

  passed =
      Expect(Execute(root, EncodeRequest("run_codec_failure", config, FailingValues()), outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "CODEC_ERROR" &&
                 stored.record.execution.main_codec->status == "VALUE_NOT_REPRESENTABLE" &&
                 stored.result.has_value() &&
                 stored.record.execution.counts.review_decode_calls == 0U &&
                 IsPhaseSequence(stored, {"PREPARATION", "MAIN_CODEC", "RESULT_MAPPING"}),
             "main Codec failure retains its Result and skips Review") &&
      passed;

  passed =
      Expect(Execute(root, InspectRequest("run_zero_candidate", config, {0U}), outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "STRUCTURAL_REJECTED" &&
                 stored.record.execution.structural_query->candidate_class == "ZERO" &&
                 stored.record.execution.counts.decode_calls == 0U && !stored.result,
             "zero structural candidates publish a diagnostic-only RUN") &&
      passed;

  std::vector<std::uint8_t> integrity_failure = ValidFrame();
  integrity_failure[0] ^= 0x01U;
  passed =
      Expect(
          Execute(root, InspectRequest("run_inspect_integrity_failure", config, integrity_failure),
                  outcome) &&
              pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
              stored.record.execution.terminal_status == "CODEC_ERROR" &&
              stored.record.execution.main_codec->status == "INTEGRITY_FAILED" &&
              stored.record.execution.counts.decode_calls == 1U && stored.result.has_value() &&
              stored.result->fields.empty(),
          "Inspect integrity failure retains its failed Result after one Decode") &&
      passed;

  passed =
      Expect(Execute(root, InspectRequest("run_multiple_candidates", AmbiguousConfig(), {0x11U}),
                     outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "STRUCTURAL_REJECTED" &&
                 stored.record.execution.structural_query->candidate_class == "MULTIPLE" &&
                 stored.record.execution.counts.structural_query_calls == 2U &&
                 stored.record.execution.counts.decode_calls == 0U,
             "multiple structural candidates never execute main Decode") &&
      passed;

  ExecutionTestHooks hooks;
  hooks.fail_structural_query = true;
  RunRequest structural = InspectRequest("run_structural_error", config);
  structural.test_hooks = &hooks;
  passed =
      Expect(Execute(root, structural, outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "STRUCTURAL_ERROR" &&
                 stored.record.execution.structural_query->candidate_class == "UNDETERMINED" &&
                 stored.record.execution.counts.decode_calls == 0U,
             "actual structural helper error is not counted as a candidate") &&
      passed;

  hooks = ExecutionTestHooks{};
  hooks.force_base_result_mapping_failure = true;
  RunRequest mapping = EncodeRequest("run_codec_and_mapping_failure", config, FailingValues());
  mapping.test_hooks = &hooks;
  passed =
      Expect(Execute(root, mapping, outcome) &&
                 pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                 stored.record.execution.terminal_status == "LAB_RESULT_FAILED" &&
                 stored.record.execution.main_codec->status == "VALUE_NOT_REPRESENTABLE" &&
                 stored.record.execution.result_mapping->status == "FAILED" && !stored.result,
             "main Codec error survives a defensive failure-Result mapping failure") &&
      passed;

  struct ReviewCase {
    const char* id;
    ExecutionTestHooks hooks;
    const char* reason;
    const char* terminal_stage;
    const char* review_status;
  };
  std::array<ReviewCase, 4> cases{};
  cases[0].id = "run_review_failure";
  cases[0].hooks.fail_review_decimal_conversion_with_internal_error = true;
  cases[0].reason = "REVIEW_DECODE_FAILED";
  cases[0].terminal_stage = "REVIEW_DECODE";
  cases[0].review_status = "INTERNAL_ERROR";
  cases[1].id = "run_review_mismatch";
  cases[1].hooks.force_review_message_mismatch = true;
  cases[1].reason = "REVIEW_MESSAGE_MISMATCH";
  cases[1].terminal_stage = "REVIEW_DECODE";
  cases[1].review_status = "OK";
  cases[2].id = "run_raw_failure";
  cases[2].hooks.force_raw_association_failure = true;
  cases[2].reason = "RAW_ASSOCIATION_FAILED";
  cases[2].terminal_stage = "RESULT_MAPPING";
  cases[2].review_status = "OK";
  cases[3].id = "run_mapping_internal";
  cases[3].hooks.force_materialization_internal_error = true;
  cases[3].reason = "INTERNAL_ERROR";
  cases[3].terminal_stage = "RESULT_MAPPING";
  cases[3].review_status = "OK";
  for (auto& test_case : cases) {
    RunRequest request = EncodeRequest(test_case.id, config);
    request.test_hooks = &test_case.hooks;
    passed =
        Expect(
            Execute(root, request, outcome) &&
                pae::protocol_lab::v07::LoadRunBundle(outcome.published_path, stored, error) &&
                stored.record.execution.terminal_status == "LAB_RESULT_FAILED" &&
                stored.record.execution.terminal_reason == test_case.reason &&
                stored.record.execution.terminal_stage == test_case.terminal_stage &&
                stored.record.execution.main_codec->status == "OK" &&
                stored.record.execution.review_decode->status == test_case.review_status &&
                !stored.result && !stored.frame,
            std::string{"Review/materialization failure is evidence-complete: "} + test_case.id) &&
        passed;
  }
  return passed;
}

enum class FailurePoint { WRITE_CLOSE, REREAD, FINAL_RENAME };

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
    return standard_.CreateDirectory(path, error);
  }
  bool CreateDirectories(const std::filesystem::path& path, std::string& error) override {
    return standard_.CreateDirectories(path, error);
  }
  bool WriteClosedFile(const std::filesystem::path& path, const std::uint8_t* data,
                       std::size_t size, std::string& error) override {
    const bool written = standard_.WriteClosedFile(path, data, size, error);
    if (written && point_ == FailurePoint::WRITE_CLOSE && ++write_count_ == 1U) {
      error = "injected write-close failure";
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

bool TestPublicationFailures(const std::filesystem::path& root, std::string_view config) {
  bool passed = true;
  std::size_t index = 0U;
  for (FailurePoint point :
       {FailurePoint::WRITE_CLOSE, FailurePoint::REREAD, FailurePoint::FINAL_RENAME}) {
    FailingFileSystem file_system{point};
    RunPublishOutcome outcome;
    RunRequest request = EncodeRequest("run_publish_failure_" + std::to_string(index++), config);
    const bool published =
        pae::protocol_lab::v07::ExecuteRunAndWrite(root, request, file_system, outcome);
    passed = Expect(!published && outcome.published_path.empty() &&
                        outcome.bundle.record.execution.counts.encode_calls == 1U &&
                        outcome.bundle.record.execution.counts.review_decode_calls == 1U,
                    "Writer failure keeps execution facts but returns no formal path") &&
             passed;
  }
  StandardEvidenceFileSystem standard;
  RunPublishOutcome first;
  passed = Expect(pae::protocol_lab::v07::ExecuteRunAndWrite(
                      root, EncodeRequest("run_no_overwrite", config), standard, first),
                  "no-overwrite baseline publishes") &&
           passed;
  RunPublishOutcome second;
  passed = Expect(!pae::protocol_lab::v07::ExecuteRunAndWrite(
                      root, EncodeRequest("run_no_overwrite", config), standard, second) &&
                      second.published_path.empty() &&
                      second.bundle.record.execution.counts.encode_calls == 1U,
                  "existing destination refuses overwrite after one real execution") &&
           passed;
  std::filesystem::create_directory(root / "run_inprogress_block.inprogress");
  RunPublishOutcome inprogress;
  passed = Expect(!pae::protocol_lab::v07::ExecuteRunAndWrite(
                      root, EncodeRequest("run_inprogress_block", config), standard, inprogress) &&
                      inprogress.published_path.empty() &&
                      inprogress.bundle.record.execution.counts.encode_calls == 1U,
                  "existing in-progress destination refuses overwrite after one real execution") &&
           passed;
  return passed;
}

std::filesystem::path CopyBundle(const std::filesystem::path& source,
                                 const std::filesystem::path& root, std::string_view name) {
  const std::filesystem::path copy = root / name;
  std::filesystem::copy(source, copy, std::filesystem::copy_options::recursive);
  std::string record = ReadText(copy / "run_record_v0.7.json");
  record = ReplaceOne(record, source.filename().generic_string(), name);
  WriteText(copy / "run_record_v0.7.json", record);
  std::string events = ReadText(copy / "events_v0.7.jsonl");
  const std::string old_id = source.filename().generic_string();
  std::size_t position = 0U;
  while ((position = events.find(old_id, position)) != std::string::npos) {
    events.replace(position, old_id.size(), name);
    position += name.size();
  }
  WriteText(copy / "events_v0.7.jsonl", events);
  RefreshPayloadDescriptor(copy, "events_v0.7.jsonl");
  RebuildManifest(copy);
  return copy;
}

bool TestStrictReader(const std::filesystem::path& root, std::string_view config) {
  RunPublishOutcome baseline;
  if (!Expect(Execute(root, EncodeRequest("run_reader_baseline", config), baseline),
              "strict Reader baseline publishes"))
    return false;
  bool passed = true;
  StoredRunBundle stored;
  std::string error;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(baseline.published_path, stored, error),
                  "strict Reader baseline loads") &&
           passed;

  auto expect_semantic_rejection = [&](const std::filesystem::path& bundle,
                                       std::string_view diagnostic, std::string_view description) {
    const bool loaded = pae::protocol_lab::v07::LoadRunBundle(bundle, stored, error);
    return Expect(
        !loaded && error.find(diagnostic) != std::string::npos && stored.record.run_id.empty(),
        std::string{description} + "; actual=" + error);
  };

  auto mutate_record = [&](std::string_view name, std::string_view before, std::string_view after,
                           std::string_view diagnostic) {
    const auto copy = CopyBundle(baseline.published_path, root, name);
    std::string record = ReplaceOne(ReadText(copy / "run_record_v0.7.json"), before, after);
    WriteText(copy / "run_record_v0.7.json", record);
    RebuildManifest(copy);
    const bool loaded = pae::protocol_lab::v07::LoadRunBundle(copy, stored, error);
    return Expect(!loaded && error.find(diagnostic) != std::string::npos,
                  std::string{"strict Reader rejects semantic mutation: "} +
                      std::string{diagnostic} + "; actual=" + error);
  };
  passed =
      mutate_record("run_reader_unknown",
                    "\"tool_version\":", "\"unknown\":1,\"tool_version\":", "unknown property") &&
      passed;
  passed = mutate_record("run_reader_duplicate", "\"tool_version\":",
                         "\"format_version\":\"pae.lab.record/0.7\",\"tool_version\":",
                         "duplicate property") &&
           passed;
  passed = mutate_record("run_reader_replay", "\"invocation_kind\":\"RUN\"",
                         "\"invocation_kind\":\"REPLAY\"", "invocation") &&
           passed;
  passed = mutate_record("run_reader_history", "\"comparison\":null", "\"comparison\":{}",
                         "missing property reason") &&
           passed;
  passed = mutate_record("run_reader_parent", "\"parent_run_id\":null",
                         "\"parent_run_id\":\"run_parent\"", "invocation") &&
           passed;
  passed = mutate_record("run_reader_baseline_field", "\"historical_baseline\":null",
                         "\"historical_baseline\":{}", "missing property") &&
           passed;
  passed = mutate_record("run_reader_terminal", "\"terminal_status\":\"OK\"",
                         "\"terminal_status\":\"CODEC_ERROR\"", "inconsistent with Run Record") &&
           passed;
  passed = mutate_record("run_reader_main_count", "\"encode_calls\":1", "\"encode_calls\":0",
                         "execution counts are inconsistent") &&
           passed;

  RunPublishOutcome inspect_failure;
  passed = Expect(Execute(root, InspectRequest("run_reader_inspect_prepare", "{}", {0U}),
                          inspect_failure),
                  "Inspect preparation-failure baseline publishes") &&
           passed;
  const auto inspect_diagnostic =
      CopyBundle(inspect_failure.published_path, root, "run_reader_inspect_diagnostic");
  std::string inspect_record = ReadText(inspect_diagnostic / "run_record_v0.7.json");
  inspect_record =
      ReplaceOne(inspect_record, "PAE_LAB_C1_CONFIG_INVALID", "PAE_LAB_C1_VALUES_INVALID");
  WriteText(inspect_diagnostic / "run_record_v0.7.json", inspect_record);
  RebuildManifest(inspect_diagnostic);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(inspect_diagnostic, stored, error) &&
                      error.find("not valid for Inspect") != std::string::npos,
                  "Inspect rejects an Encode/Values-only preparation diagnostic: " + error) &&
           passed;

  RunPublishOutcome inspect_success;
  passed =
      Expect(Execute(root, InspectRequest("run_reader_inspect_success", config), inspect_success),
             "successful Inspect semantic-mutation baseline publishes") &&
      passed;
  for (const auto& mutation : std::array<std::pair<std::string_view, std::string_view>, 3>{{
           {"UNKNOWN_MESSAGE", "ZERO"},
           {"AMBIGUOUS_MESSAGE", "MULTIPLE"},
           {"INVALID_ARGUMENT", "UNDETERMINED"},
       }}) {
    const std::string name = "run_reader_inspect_structure_" + std::string{mutation.second};
    const auto copy = CopyBundle(inspect_success.published_path, root, name);
    std::string record = ReadText(copy / "run_record_v0.7.json");
    record =
        ReplaceOne(record, "\"structural_query\":{\"status\":\"OK\",\"candidate_class\":\"ONE\"}",
                   "\"structural_query\":{\"status\":\"" + std::string{mutation.first} +
                       "\",\"candidate_class\":\"" + std::string{mutation.second} + "\"}");
    WriteText(copy / "run_record_v0.7.json", record);
    std::string events = ReadText(copy / "events_v0.7.jsonl");
    events = ReplaceOne(
        events,
        "\"event_kind\":\"PHASE_FINISHED\",\"phase\":\"STRUCTURAL_QUERY\",\"status\":\"OK\"",
        "\"event_kind\":\"PHASE_FINISHED\",\"phase\":\"STRUCTURAL_QUERY\",\"status\":\"" +
            std::string{mutation.first} + "\"");
    WriteText(copy / "events_v0.7.jsonl", events);
    RefreshPayloadDescriptor(copy, "events_v0.7.jsonl");
    RebuildManifest(copy);
    passed = expect_semantic_rejection(copy, "successful Inspect requires structural ONE/OK",
                                       "successful Inspect rejects a non-unique structure fact") &&
             passed;
  }

  ExecutionTestHooks raw_hooks;
  raw_hooks.force_raw_association_failure = true;
  RunRequest raw_request = EncodeRequest("run_reader_raw_baseline", config);
  raw_request.test_hooks = &raw_hooks;
  RunPublishOutcome raw_failure;
  passed = Expect(Execute(root, raw_request, raw_failure),
                  "RAW association failure semantic-mutation baseline publishes") &&
           passed;
  const auto raw_single_mapping =
      CopyBundle(raw_failure.published_path, root, "run_reader_raw_single_mapping");
  passed =
      Expect(RemoveFirstResultMappingPair(raw_single_mapping, error),
             "self-consistent RAW mutation removes and rebinds the first mapping pair: " + error) &&
      passed;
  passed = expect_semantic_rejection(raw_single_mapping,
                                     "RAW association failure requires OK then FAILED mappings",
                                     "RAW association failure rejects a single FAILED mapping") &&
           passed;

  auto mutate_result = [&](std::string_view name, const RunPublishOutcome& source,
                           const std::function<void(Result&)>& mutate, std::string_view diagnostic,
                           std::string_view description) {
    const auto copy = CopyBundle(source.published_path, root, name);
    if (!Expect(
            RewriteResultAndRefresh(copy, mutate, error),
            std::string{description} + " prepares an A-valid self-consistent mutation: " + error))
      return false;
    return expect_semantic_rejection(copy, diagnostic, description);
  };
  passed = mutate_result(
               "run_reader_encode_decode_mode", baseline,
               [](Result& result) {
                 result.replay_mode = "DECODE_RX";
                 result.replay_subject = "RX";
               },
               "Result 0.6 mode does not match Run operation",
               "Encode RUN rejects an A-valid Decode mode") &&
           passed;
  passed = mutate_result(
               "run_reader_inspect_encode_mode", inspect_success,
               [](Result& result) {
                 result.replay_mode = "ENCODE_TX";
                 result.replay_subject = "TX";
               },
               "Result 0.6 mode does not match Run operation",
               "Inspect RUN rejects an A-valid Encode mode") &&
           passed;
  passed = mutate_result(
               "run_reader_success_diagnostic", baseline,
               [](Result& result) {
                 result.diagnostic_id = "PAE_LAB_CODEC_OK";
                 result.diagnostic_detail = "injected self-consistent diagnostic";
                 result.current_execution_diagnostic_id = "PAE_LAB_CODEC_OK";
               },
               "successful Result must not contain Codec diagnostics",
               "successful RUN rejects A-valid failure diagnostics") &&
           passed;

  RunPublishOutcome codec_failure;
  passed = Expect(Execute(root, EncodeRequest("run_reader_codec_failure", config, FailingValues()),
                          codec_failure),
                  "Codec failure semantic-mutation baseline publishes") &&
           passed;
  passed = mutate_result(
               "run_reader_failure_outer_diagnostic", codec_failure,
               [](Result& result) { result.diagnostic_id = "PAE_LAB_CODEC_INTERNAL_ERROR"; },
               "Codec diagnostic does not match main status",
               "failed RUN rejects a mismatched outer diagnostic") &&
           passed;
  passed = mutate_result(
               "run_reader_failure_missing_diagnostics", codec_failure,
               [](Result& result) {
                 result.diagnostic_id.reset();
                 result.diagnostic_detail.clear();
                 result.current_execution_diagnostic_id.reset();
               },
               "Codec diagnostic does not match main status",
               "failed RUN rejects missing Codec diagnostics") &&
           passed;

  const auto event_copy = CopyBundle(baseline.published_path, root, "run_reader_event");
  std::string event = ReadText(event_copy / "events_v0.7.jsonl");
  event = ReplaceOne(event, "\"phase\":\"MAIN_CODEC\"", "\"phase\":\"REVIEW_DECODE\"");
  WriteText(event_copy / "events_v0.7.jsonl", event);
  RefreshPayloadDescriptor(event_copy, "events_v0.7.jsonl");
  RebuildManifest(event_copy);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(event_copy, stored, error) &&
                      error.find("phase") != std::string::npos,
                  "self-consistent Event mutation reaches phase semantic rejection: " + error) &&
           passed;

  const auto event_unknown = CopyBundle(baseline.published_path, root, "run_reader_event_unknown");
  event = ReadText(event_unknown / "events_v0.7.jsonl");
  event = ReplaceOne(event, "\"event_id\":1", "\"unknown\":1,\"event_id\":1");
  WriteText(event_unknown / "events_v0.7.jsonl", event);
  RefreshPayloadDescriptor(event_unknown, "events_v0.7.jsonl");
  RebuildManifest(event_unknown);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(event_unknown, stored, error) &&
                      error.find("unknown property unknown") != std::string::npos,
                  "self-consistent Event unknown property reaches strict semantic rejection: " +
                      error) &&
           passed;

  const auto missing = CopyBundle(baseline.published_path, root, "run_reader_missing");
  std::filesystem::remove(missing / "events_v0.7.jsonl");
  RebuildManifest(missing);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(missing, stored, error) &&
                      error.find("omits a required file") != std::string::npos,
                  "missing required Event fails before semantic delivery: " + error) &&
           passed;

  const auto extra = CopyBundle(baseline.published_path, root, "run_reader_extra");
  WriteText(extra / "extra.json", "{}\n");
  RebuildManifest(extra);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(extra, stored, error) &&
                      error.find("unsupported file set") != std::string::npos,
                  "listed extra file is rejected as an unsupported file set: " + error) &&
           passed;

  const auto hash_copy = CopyBundle(baseline.published_path, root, "run_reader_hash");
  WriteText(hash_copy / "inputs/protocol.pae.json", "{}");
  passed =
      Expect(!pae::protocol_lab::v07::LoadRunBundle(hash_copy, stored, error) &&
                 error.find("hash mismatch") != std::string::npos && stored.record.run_id.empty(),
             "Reader Hash failure clears a previously successful output before delivery") &&
      passed;

  BundleInput legacy;
  legacy.config_text = baseline.bundle.config_text;
  legacy.values_text = baseline.bundle.values_text;
  legacy.frame = baseline.bundle.frame;
  legacy.result = *baseline.bundle.result;
  StandardEvidenceFileSystem file_system;
  std::filesystem::path legacy_path;
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_b_legacy", legacy,
                                                              file_system, legacy_path, error),
                  "B synthetic 0.6 Bundle writes unchanged: " + error) &&
           passed;
  pae::protocol_lab::v06::StoredBundle legacy_stored;
  passed = Expect(pae::protocol_lab::v06::LoadEvidenceBundle(legacy_path, legacy_stored, error) &&
                      !pae::protocol_lab::v07::LoadRunBundle(legacy_path, stored, error),
                  "B 0.6 remains readable by B and is not guessed as C 0.7") &&
           passed;

  const std::filesystem::path link = root / "run_reader_link";
  std::error_code link_error;
  std::filesystem::create_directory_symlink(baseline.published_path, link, link_error);
  if (!link_error) {
    passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(link, stored, error) &&
                        error.find("symlink or reparse point") != std::string::npos,
                    "Reader rejects a Bundle directory link before content parsing") &&
             passed;
  } else {
    std::cout << "SKIP: directory symlink creation unavailable: " << link_error.message() << '\n';
  }
  return passed;
}

bool TestPlanReplayAndCompare(const std::filesystem::path& root, std::string_view config) {
  using pae::protocol_lab::v07::CompareOutcome;
  using pae::protocol_lab::v07::EvidenceQualificationStatus;
  bool passed = true;
  std::string error;
  StandardEvidenceFileSystem file_system;
  RunPublishOutcome encoded;
  RunPublishOutcome inspected;
  passed = Expect(Execute(root, EncodeRequest("run_c2_encode", config), encoded) &&
                      Execute(root, InspectRequest("run_c2_inspect", config), inspected),
                  "C2 qualification baselines publish") &&
           passed;
  const std::string inspect_parent_record =
      ReadText(inspected.published_path / "run_record_v0.7.json");

  StoredRunBundle stored;
  EvidenceQualificationStatus qualification = EvidenceQualificationStatus::INVALID_BUNDLE;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(encoded.published_path, stored, error) &&
                      pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
                      qualification == EvidenceQualificationStatus::OK,
                  "valid Encode RUN passes Plan qualification: " + error) &&
           passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(inspected.published_path, stored, error) &&
                      pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error),
                  "valid Inspect RUN passes Plan qualification: " + error) &&
           passed;
  RunPublishOutcome type_failure;
  passed =
      Expect(
          Execute(root,
                  EncodeRequest("run_c2_type_failure", NoConversionConfig(), MismatchedIntValues()),
                  type_failure) &&
              pae::protocol_lab::v07::LoadRunBundle(type_failure.published_path, stored, error) &&
              stored.result->current_execution_status == "TYPE_MISMATCH" &&
              pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error),
          "genuine type-mismatch failure identity passes Plan qualification: " + error) &&
      passed;

  RunPublishOutcome replay_a;
  RunPublishOutcome replay_b;
  passed =
      Expect(pae::protocol_lab::v07::ReplayRunAndWrite(
                 root, inspected.published_path, "run_c2_inspect_replay_a", "0.1.0-test",
                 file_system, replay_a, qualification, error) &&
                 pae::protocol_lab::v07::LoadRunBundle(replay_a.published_path, stored, error) &&
                 stored.record.invocation_kind == "REPLAY" &&
                 stored.record.parent_run_id == "run_c2_inspect" &&
                 stored.record.requested_pipeline_id == stored.result->pipeline_id &&
                 stored.record.execution.counts.structural_query_calls == 1U &&
                 stored.record.execution.counts.decode_calls == 1U &&
                 stored.record.comparison->status == "EQUAL" &&
                 stored.record.historical_baseline.has_value() &&
                 pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error),
             "Inspect replay uses exactly one explicit Pipeline query and reloads: " + error) &&
      passed;
  passed =
      Expect(pae::protocol_lab::v07::ReplayRunAndWrite(
                 root, replay_a.published_path, "run_c2_inspect_replay_b", "0.1.0-test",
                 file_system, replay_b, qualification, error) &&
                 pae::protocol_lab::v07::LoadRunBundle(replay_b.published_path, stored, error) &&
                 stored.record.parent_run_id == "run_c2_inspect_replay_a" &&
                 stored.record.comparison->reason == "FINGERPRINT_EQUAL",
             "Run to Replay A to Replay B remains readable and equal: " + error) &&
      passed;
  passed =
      Expect(ReadText(inspected.published_path / "run_record_v0.7.json") == inspect_parent_record,
             "Replay chain does not rewrite the original parent Record") &&
      passed;

  RunPublishOutcome encode_replay;
  passed = Expect(pae::protocol_lab::v07::ReplayRunAndWrite(
                      root, encoded.published_path, "run_c2_encode_replay", "0.1.0-test",
                      file_system, encode_replay, qualification, error) &&
                      pae::protocol_lab::v07::LoadRunBundle(encode_replay.published_path, stored,
                                                            error) &&
                      !stored.record.requested_pipeline_id.has_value() &&
                      stored.record.execution.counts.encode_calls == 1U &&
                      stored.record.comparison->status == "EQUAL",
                  "Encode replay reuses Values identity without structural query: " + error) &&
           passed;

  const auto bad_history_mode =
      CopyBundle(encode_replay.published_path, root, "run_c2_bad_history_mode");
  passed =
      Expect(RewriteHistoricalResultAndRefresh(
                 bad_history_mode,
                 [](Result& result) {
                   result.replay_mode = "DECODE_RX";
                   result.replay_subject = "RX";
                 },
                 error),
             "A-valid historical mode mutation refreshes all hashes and fingerprints: " + error) &&
      passed;
  passed = Expect(ReplayHistoryLinksMatch(bad_history_mode),
                  "historical mode mutation is length and Hash self-consistent before reading") &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_history_mode, stored, error) &&
                      error.find("historical Result 0.6 mode") != std::string::npos,
                  "fully self-consistent historical Result mode mismatch reaches semantic "
                  "rejection: " +
                      error) &&
           passed;

  const auto bad_history_success_diagnostic =
      CopyBundle(encode_replay.published_path, root, "run_c2_bad_history_success_diagnostic");
  passed = Expect(RewriteHistoricalResultAndRefresh(
                      bad_history_success_diagnostic,
                      [](Result& result) {
                        result.diagnostic_id = "PAE_LAB_CODEC_INTERNAL_ERROR";
                        result.diagnostic_detail = "synthetic historical diagnostic";
                      },
                      error),
                  "A-valid successful historical diagnostic mutation prepares: " + error) &&
           passed;
  passed = Expect(ReplayHistoryLinksMatch(bad_history_success_diagnostic),
                  "successful diagnostic mutation is Hash self-consistent before reading") &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_history_success_diagnostic, stored,
                                                         error) &&
                      error.find("successful historical Result") != std::string::npos,
                  "successful historical Result cannot carry a Codec diagnostic: " + error) &&
           passed;

  const auto bad_parent_role =
      CopyBundle(encode_replay.published_path, root, "run_c2_bad_parent_role");
  passed = Expect(RewriteParentRecordAndRefresh(
                      bad_parent_role,
                      [](std::string& parent) {
                        parent =
                            ReplaceOne(parent, "\"values_file\":\"inputs/values.pae-lab.json\"",
                                       "\"values_file\":null");
                      },
                      error),
                  "parent role mutation refreshes its child Hash bindings: " + error) &&
           passed;
  passed = Expect(ReplayHistoryLinksMatch(bad_parent_role),
                  "parent role mutation is length and Hash self-consistent before reading") &&
           passed;
  passed =
      Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_parent_role, stored, error) &&
                 error.find("payload roles") != std::string::npos,
             "historical parent values role is rejected by terminal role validation: " + error) &&
      passed;

  const auto bad_parent_payload_set =
      CopyBundle(encode_replay.published_path, root, "run_c2_bad_parent_payload_set");
  passed = Expect(RewriteParentRecordAndRefresh(
                      bad_parent_payload_set,
                      [](std::string& parent) {
                        parent = ReplaceOne(parent, "\"path\":\"events_v0.7.jsonl\"",
                                            "\"path\":\"unexpected.jsonl\"");
                      },
                      error),
                  "parent payload-role mutation refreshes its child Hash bindings: " + error) &&
           passed;
  passed = Expect(ReplayHistoryLinksMatch(bad_parent_payload_set),
                  "parent descriptor mutation is length and Hash self-consistent before reading") &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_parent_payload_set, stored, error) &&
                      error.find("historical parent payload set") != std::string::npos,
                  "historical parent descriptor role set reaches semantic rejection: " + error) &&
           passed;

  CompareOutcome compared;
  passed = Expect(pae::protocol_lab::v07::CompareRunEvidence(encoded.published_path,
                                                             encode_replay.published_path, compared,
                                                             qualification, error) &&
                      compared.comparison.status == "EQUAL",
                  "independent Compare is equal for matching executions: " + error) &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::CompareRunEvidence(encoded.published_path,
                                                              inspected.published_path, compared,
                                                              qualification, error) &&
                      qualification == EvidenceQualificationStatus::PLAN_MISMATCH &&
                      error.find("matching execution operations") != std::string::npos,
                  "independent Compare rejects unlike operation kinds before classifying a diff") &&
           passed;

  const std::string equivalent_values =
      ReplaceOne(ValidValues(), "\"123\",\"scale\":1", "\"1230\",\"scale\":2");
  RunPublishOutcome equivalent;
  passed = Expect(Execute(root, EncodeRequest("run_c2_equivalent", config, equivalent_values),
                          equivalent) &&
                      pae::protocol_lab::v07::CompareRunEvidence(encoded.published_path,
                                                                 equivalent.published_path,
                                                                 compared, qualification, error) &&
                      compared.comparison.status == "EQUAL",
                  "independent Compare accepts mathematically equivalent Decimal Values text: " +
                      error) &&
           passed;

  const std::string different_values =
      ReplaceOne(ValidValues(), "\"123\",\"scale\":1", "\"124\",\"scale\":1");
  RunPublishOutcome different;
  passed = Expect(Execute(root, EncodeRequest("run_c2_different", config, different_values),
                          different) &&
                      pae::protocol_lab::v07::CompareRunEvidence(encoded.published_path,
                                                                 different.published_path, compared,
                                                                 qualification, error) &&
                      compared.comparison.status == "DIFFERENT",
                  "independent Compare reports a legitimate execution difference: " + error) &&
           passed;

  RunPublishOutcome failed;
  RunPublishOutcome failed_replay;
  passed = Expect(Execute(root, EncodeRequest("run_c2_failed", config, FailingValues()), failed) &&
                      pae::protocol_lab::v07::ReplayRunAndWrite(
                          root, failed.published_path, "run_c2_failed_replay", "0.1.0-test",
                          file_system, failed_replay, qualification, error) &&
                      pae::protocol_lab::v07::LoadRunBundle(failed_replay.published_path, stored,
                                                            error) &&
                      stored.record.execution.terminal_status == "CODEC_ERROR" &&
                      stored.record.comparison->status == "EQUAL" &&
                      stored.result->operation_status == "CODEC_ERROR",
                  "matching failed replay is EQUAL without becoming execution success: " + error) &&
           passed;
  const auto bad_history_failure_diagnostic =
      CopyBundle(failed_replay.published_path, root, "run_c2_bad_history_failure_diagnostic");
  passed = Expect(RewriteHistoricalResultAndRefresh(
                      bad_history_failure_diagnostic,
                      [](Result& result) {
                        result.diagnostic_id = "PAE_LAB_CODEC_TYPE_MISMATCH";
                        result.current_execution_diagnostic_id = "PAE_LAB_CODEC_TYPE_MISMATCH";
                      },
                      error),
                  "A-valid failed historical diagnostic mutation prepares: " + error) &&
           passed;
  passed = Expect(ReplayHistoryLinksMatch(bad_history_failure_diagnostic),
                  "failed diagnostic mutation is Hash self-consistent before reading") &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_history_failure_diagnostic, stored,
                                                         error) &&
                      error.find("historical Codec diagnostic") != std::string::npos,
                  "historical failure diagnostic must match the parent main status: " + error) &&
           passed;

  RunPublishOutcome no_result;
  passed = Expect(Execute(root, EncodeRequest("run_c2_no_result", config, "{}"), no_result) &&
                      !pae::protocol_lab::v07::ReplayRunAndWrite(
                          root, no_result.published_path, "run_c2_no_result_replay", "0.1.0-test",
                          file_system, replay_a, qualification, error) &&
                      qualification == EvidenceQualificationStatus::RESULT_UNAVAILABLE &&
                      !std::filesystem::exists(root / "run_c2_no_result_replay"),
                  "no-Result RUN remains readable but cannot become a Replay baseline") &&
           passed;

  ExecutionTestHooks structural_hook;
  structural_hook.force_pipeline_structural_unknown = true;
  RunPublishOutcome zero_replay;
  passed =
      Expect(pae::protocol_lab::v07::ReplayRunAndWrite(
                 root, inspected.published_path, "run_c2_zero_replay", "0.1.0-test", file_system,
                 zero_replay, qualification, error, &structural_hook) &&
                 pae::protocol_lab::v07::LoadRunBundle(zero_replay.published_path, stored, error) &&
                 stored.record.execution.structural_query->candidate_class == "ZERO" &&
                 stored.record.execution.counts.structural_query_calls == 1U &&
                 stored.record.execution.counts.decode_calls == 0U && !stored.result &&
                 stored.record.comparison->status == "NOT_EVALUATED" &&
                 !pae::protocol_lab::v07::ReplayRunAndWrite(
                     root, zero_replay.published_path, "run_c2_zero_replay_again", "0.1.0-test",
                     file_system, replay_a, qualification, error),
             "explicit Pipeline ZERO executes no Decode and cannot be replayed again") &&
      passed;
  structural_hook = ExecutionTestHooks{};
  structural_hook.force_pipeline_structural_ambiguous = true;
  RunPublishOutcome multiple_replay;
  passed = Expect(pae::protocol_lab::v07::ReplayRunAndWrite(
                      root, inspected.published_path, "run_c2_multiple_replay", "0.1.0-test",
                      file_system, multiple_replay, qualification, error, &structural_hook) &&
                      pae::protocol_lab::v07::LoadRunBundle(multiple_replay.published_path, stored,
                                                            error) &&
                      stored.record.execution.structural_query->candidate_class == "MULTIPLE" &&
                      stored.record.execution.counts.decode_calls == 0U && !stored.result,
                  "explicit Pipeline MULTIPLE executes no Decode and remains readable") &&
           passed;
  structural_hook = ExecutionTestHooks{};
  structural_hook.fail_structural_query = true;
  RunPublishOutcome query_error_replay;
  passed = Expect(pae::protocol_lab::v07::ReplayRunAndWrite(
                      root, inspected.published_path, "run_c2_query_error_replay", "0.1.0-test",
                      file_system, query_error_replay, qualification, error, &structural_hook) &&
                      pae::protocol_lab::v07::LoadRunBundle(query_error_replay.published_path,
                                                            stored, error) &&
                      stored.record.execution.structural_query->candidate_class == "UNDETERMINED" &&
                      stored.record.execution.counts.decode_calls == 0U && !stored.result,
                  "explicit Pipeline query error executes no Decode and remains readable") &&
           passed;

  const auto bad_pipeline = CopyBundle(encoded.published_path, root, "run_c2_bad_pipeline");
  passed = Expect(RewriteResultAndRefresh(
                      bad_pipeline, [](Result& result) { result.pipeline_id = "missing_pipeline"; },
                      error),
                  "self-consistent invalid Pipeline mutation prepares: " + error) &&
           passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(bad_pipeline, stored, error) &&
                      !pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
                      qualification == EvidenceQualificationStatus::PLAN_MISMATCH,
                  "self-consistent invalid Pipeline is rejected at Plan qualification") &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::ReplayRunAndWrite(
                      root, bad_pipeline, "run_c2_bad_pipeline_child", "0.1.0-test", file_system,
                      replay_a, qualification, error) &&
                      replay_a.published_path.empty() &&
                      !std::filesystem::exists(root / "run_c2_bad_pipeline_child"),
                  "Plan qualification rejection publishes no child Run") &&
           passed;

  const auto bad_fields = CopyBundle(encoded.published_path, root, "run_c2_bad_field_order");
  passed = Expect(RewriteResultAndRefresh(
                      bad_fields,
                      [](Result& result) { std::swap(result.fields[0], result.fields[1]); }, error),
                  "self-consistent field-order mutation prepares: " + error) &&
           passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(bad_fields, stored, error) &&
                      !pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
                      error.find("field order") != std::string::npos,
                  "self-consistent field order is rejected by Plan association: " + error) &&
           passed;

  const auto bad_raw_kind = CopyBundle(encoded.published_path, root, "run_c2_bad_raw_kind");
  passed =
      Expect(RewriteResultAndRefresh(
                 bad_raw_kind, [](Result& result) { result.fields[0].raw_kind = "UINT64"; }, error),
             "self-consistent raw-kind mutation prepares: " + error) &&
      passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(bad_raw_kind, stored, error) &&
                      !pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
                      error.find("raw tag") != std::string::npos,
                  "A-valid raw-kind mismatch is rejected by Plan association: " + error) &&
           passed;

  const auto bad_message = CopyBundle(encoded.published_path, root, "run_c2_bad_message");
  passed =
      Expect(RewriteResultAndRefresh(
                 bad_message, [](Result& result) { result.message_id = "missing_message"; }, error),
             "self-consistent invalid Message mutation prepares: " + error) &&
      passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(bad_message, stored, error) &&
                      !pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
                      error.find("Pipeline, Message") != std::string::npos,
                  "self-consistent invalid Message is rejected by Plan association: " + error) &&
           passed;

  const auto bad_value_index = CopyBundle(failed.published_path, root, "run_c2_bad_value_index");
  passed =
      Expect(RewriteResultAndRefresh(
                 bad_value_index, [](Result& result) { result.failed_value_index = 1U; }, error),
             "self-consistent failed Values index mutation prepares: " + error) &&
      passed;
  passed = Expect(pae::protocol_lab::v07::LoadRunBundle(bad_value_index, stored, error) &&
                      !pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
                      error.find("failed Values index") != std::string::npos,
                  "failed Values author index must bind the failed field: " + error) &&
           passed;

  RunPublishOutcome bytes_failure;
  passed =
      Expect(
          Execute(root,
                  EncodeRequest("run_c2_bytes_failure", BytesConfig(), WrongLengthBytesValues()),
                  bytes_failure) &&
              pae::protocol_lab::v07::LoadRunBundle(bytes_failure.published_path, stored, error) &&
              stored.result->current_execution_status == "BYTES_LENGTH_MISMATCH" &&
              pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error),
          "genuine BYTES length failure passes Plan qualification: " + error) &&
      passed;
  const auto bad_bytes_applicability =
      CopyBundle(bytes_failure.published_path, root, "run_c2_bad_bytes_applicability");
  WriteText(bad_bytes_applicability / "inputs/protocol.pae.json", Int64Config());
  RefreshPayloadDescriptor(bad_bytes_applicability, "inputs/protocol.pae.json");
  WriteText(bad_bytes_applicability / "inputs/values.pae-lab.json", MatchingInt64Values());
  RefreshPayloadDescriptor(bad_bytes_applicability, "inputs/values.pae-lab.json");
  passed =
      Expect(RewriteResultAndRefresh(
                 bad_bytes_applicability,
                 [&](Result& result) { result.config_sha256 = HashBytes(Int64Config()); }, error),
             "self-consistent INT64/BYTES_LENGTH_MISMATCH mutation prepares: " + error) &&
      passed;
  passed =
      Expect(ManifestHashesMatch(bad_bytes_applicability) &&
                 RecordDescriptorMatches(bad_bytes_applicability / "run_record_v0.7.json",
                                         bad_bytes_applicability, "inputs/protocol.pae.json") &&
                 RecordDescriptorMatches(bad_bytes_applicability / "run_record_v0.7.json",
                                         bad_bytes_applicability, "inputs/values.pae-lab.json") &&
                 RecordDescriptorMatches(bad_bytes_applicability / "run_record_v0.7.json",
                                         bad_bytes_applicability, "result_summary_v0.6.json"),
             "INT64 failure mutation is length and Hash self-consistent before qualification") &&
      passed;
  passed =
      Expect(
          pae::protocol_lab::v07::LoadRunBundle(bad_bytes_applicability, stored, error) &&
              !pae::protocol_lab::v07::QualifyRunEvidence(stored, qualification, error) &&
              error.find("BYTES_LENGTH_MISMATCH requires a BYTES Plan field") != std::string::npos,
          "BYTES length failure cannot be attributed to a same-kind INT64 field: " + error) &&
      passed;

  const auto child_input = CopyBundle(encode_replay.published_path, root, "run_c2_child_input");
  WriteText(child_input / "inputs/values.pae-lab.json", equivalent_values);
  RefreshPayloadDescriptor(child_input, "inputs/values.pae-lab.json");
  RebuildManifest(child_input);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(child_input, stored, error) &&
                      error.find("parent material") != std::string::npos,
                  "Replay rejects mathematically equivalent but byte-different child material") &&
           passed;

  const auto bad_comparison =
      CopyBundle(encode_replay.published_path, root, "run_c2_bad_comparison");
  std::string record = ReadText(bad_comparison / "run_record_v0.7.json");
  record = ReplaceOne(record, "\"status\":\"EQUAL\",\"reason\":\"FINGERPRINT_EQUAL\"",
                      "\"status\":\"DIFFERENT\",\"reason\":\"FINGERPRINT_DIFFERENT\"");
  WriteText(bad_comparison / "run_record_v0.7.json", record);
  RebuildManifest(bad_comparison);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_comparison, stored, error) &&
                      error.find("comparison") != std::string::npos,
                  "self-consistent false comparison declaration is rejected: " + error) &&
           passed;

  const auto bad_parent_identity =
      CopyBundle(encode_replay.published_path, root, "run_c2_bad_parent_identity");
  const std::filesystem::path parent_snapshot =
      bad_parent_identity / "history/parent_record_v0.7.json";
  const std::string original_parent = ReadText(parent_snapshot);
  const std::string mutated_parent = ReplaceOne(original_parent, "\"run_id\":\"run_c2_encode\"",
                                                "\"run_id\":\"run_c2_other_parent\"");
  WriteText(parent_snapshot, mutated_parent);
  RefreshPayloadDescriptor(bad_parent_identity, "history/parent_record_v0.7.json");
  record = ReadText(bad_parent_identity / "run_record_v0.7.json");
  record = ReplaceOne(record, HashBytes(original_parent), HashBytes(mutated_parent));
  WriteText(bad_parent_identity / "run_record_v0.7.json", record);
  RebuildManifest(bad_parent_identity);
  passed = Expect(!pae::protocol_lab::v07::LoadRunBundle(bad_parent_identity, stored, error) &&
                      error.find("parent execution") != std::string::npos,
                  "fully rehashed parent snapshot identity mutation reaches semantic rejection: " +
                      error) &&
           passed;

  BundleInput synthetic;
  synthetic.config_text = encoded.bundle.config_text;
  synthetic.values_text = encoded.bundle.values_text;
  synthetic.frame = encoded.bundle.frame;
  synthetic.result = *encoded.bundle.result;
  std::filesystem::path synthetic_path;
  passed = Expect(pae::protocol_lab::v06::WriteEvidenceBundle(root, "run_c2_b_synthetic", synthetic,
                                                              file_system, synthetic_path, error),
                  "B synthetic comparison input prepares: " + error) &&
           passed;
  passed = Expect(!pae::protocol_lab::v07::CompareRunEvidence(
                      synthetic_path, encoded.published_path, compared, qualification, error) &&
                      qualification == EvidenceQualificationStatus::INVALID_BUNDLE,
                  "B synthetic Evidence is not guessed as C execution comparison input") &&
           passed;
  return passed;
}

}  // namespace

int main(int argc, char** argv) {
  if (!Expect(argc == 2, "decimal Core fixture path is required")) return 1;
  const std::string config = ReadText(argv[1]);
  const std::filesystem::path root =
      std::filesystem::current_path() / "v07_run_evidence_test_output";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root, error);
  bool passed = Expect(!error, "isolated test root prepares");
  passed = TestSuccessfulRuns(root, config) && passed;
  passed = TestFailureRuns(root, config) && passed;
  passed = TestPublicationFailures(root, config) && passed;
  passed = TestStrictReader(root, config) && passed;
  passed = TestPlanReplayAndCompare(root, config) && passed;
  return passed ? 0 : 1;
}
