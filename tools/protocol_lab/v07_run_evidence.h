#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "v06_evidence.h"

namespace pae::protocol_lab::v07 {

inline constexpr std::string_view kRecordFormat = "pae.lab.record/0.7";
inline constexpr std::string_view kEventFormat = "pae.lab.event/0.7";
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
inline constexpr std::string_view kCrcRecordFormat = "pae.lab.record/0.8";
#endif

struct FileDescriptor {
  std::string path;
  std::uint64_t size = 0U;
  std::string sha256;
};

struct PreparationFacts {
  std::string status;
  std::optional<std::string> diagnostic_id;
  std::string detail;
  std::optional<std::uint64_t> value_index;
};

struct StructuralQueryFacts {
  std::string status;
  std::string candidate_class;
};

struct CodecFacts {
  std::optional<std::string> kind;
  std::string status;
};

struct ResultMappingFacts {
  std::string status;
};

struct ExecutionCounts {
  std::uint64_t structural_query_calls = 0U;
  std::uint64_t encode_calls = 0U;
  std::uint64_t decode_calls = 0U;
  std::uint64_t review_decode_calls = 0U;
};

struct ExecutionFacts {
  std::string terminal_stage;
  std::string terminal_status;
  std::string terminal_reason;
  PreparationFacts preparation;
  std::optional<StructuralQueryFacts> structural_query;
  std::optional<CodecFacts> main_codec;
  std::optional<CodecFacts> review_decode;
  std::optional<ResultMappingFacts> result_mapping;
  ExecutionCounts counts;
};

struct ComparisonFacts {
  std::string status;
  std::string reason;
};

struct HistoricalBaseline {
  std::string parent_record_file;
  std::string parent_record_sha256;
  std::string result_file;
  std::string result_sha256;
  std::string fingerprint_domain;
  std::string deterministic_fingerprint;
};

struct StageEvent {
  std::uint64_t event_id = 0U;
  std::uint64_t offset_us = 0U;
  std::string event_kind;
  std::string phase;
  std::optional<std::string> status;
};

struct RunRecord {
  std::string format_version = std::string{kRecordFormat};
  std::string run_id;
  std::string tool_version;
  std::string operation_kind;
  std::string invocation_kind = "RUN";
  std::optional<std::string> parent_run_id;
  std::optional<std::string> result_file;
  std::optional<std::string> frame_file;
  std::optional<std::string> values_file;
  std::optional<std::string> deterministic_fingerprint;
  std::optional<std::string> requested_pipeline_id;
  ExecutionFacts execution;
  std::optional<ComparisonFacts> comparison;
  std::optional<HistoricalBaseline> historical_baseline;
  std::vector<FileDescriptor> recorded_payload_files;
};

struct RunBundleInput {
  RunRecord record;
  std::string config_text;
  std::optional<std::string> values_text;
  std::optional<std::vector<std::uint8_t>> frame;
  std::optional<v06::Result> result;
  std::vector<StageEvent> events;
  std::optional<std::string> parent_record_text;
  std::optional<std::string> historical_result_text;
};

struct StoredRunBundle {
  RunRecord record;
  std::string config_text;
  std::optional<std::string> values_text;
  std::optional<std::vector<std::uint8_t>> frame;
  std::optional<v06::Result> result;
  std::vector<StageEvent> events;
  std::string record_text;
  std::optional<std::string> result_text;
  std::optional<std::string> parent_record_text;
  std::optional<std::string> historical_result_text;
};

bool WriteRunBundle(const std::filesystem::path& record_root, const RunBundleInput& input,
                    v06::EvidenceFileSystem& file_system, std::filesystem::path& published_path,
                    std::string& error);
bool LoadRunBundle(const std::filesystem::path& bundle, StoredRunBundle& output,
                   std::string& error);
bool LoadRunBundleForTest(const std::filesystem::path& bundle, StoredRunBundle& output,
                          v06::EvidenceFileSystem& file_system, std::string& error);

}  // namespace pae::protocol_lab::v07
