#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "v06_execution.h"
#include "v07_run_evidence.h"

namespace pae::protocol_lab::v07 {

struct RunRequest {
  std::string run_id;
  std::string tool_version;
  std::string operation_kind;
  std::string config_text;
  std::optional<std::string> values_text;
  std::optional<std::vector<std::uint8_t>> frame;
  std::string invocation_kind = "RUN";
  std::optional<std::string> parent_run_id;
  std::optional<std::string> requested_pipeline_id;
  std::optional<std::string> parent_record_text;
  std::optional<std::string> historical_result_text;
  std::optional<std::string> historical_fingerprint;
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  const v06::ExecutionTestHooks* test_hooks = nullptr;
#endif
};

struct RunPublishOutcome {
  RunBundleInput bundle;
  std::filesystem::path published_path;
  std::string publication_error;
};

bool ExecuteRunAndWrite(const std::filesystem::path& record_root, const RunRequest& request,
                        v06::EvidenceFileSystem& file_system, RunPublishOutcome& output);

enum class EvidenceQualificationStatus {
  OK,
  INVALID_BUNDLE,
  PLAN_INVALID,
  PLAN_MISMATCH,
  RESULT_UNAVAILABLE,
};

struct CompareOutcome {
  ComparisonFacts comparison;
};

bool QualifyRunEvidence(const StoredRunBundle& bundle, EvidenceQualificationStatus& status,
                        std::string& error);
bool ReplayRunAndWrite(const std::filesystem::path& record_root,
                       const std::filesystem::path& parent_bundle, std::string run_id,
                       std::string tool_version, v06::EvidenceFileSystem& file_system,
                       RunPublishOutcome& output, EvidenceQualificationStatus& status,
                       std::string& error
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                       ,
                       const v06::ExecutionTestHooks* test_hooks = nullptr
#endif
);
bool CompareRunEvidence(const std::filesystem::path& left_bundle,
                        const std::filesystem::path& right_bundle, CompareOutcome& output,
                        EvidenceQualificationStatus& status, std::string& error);

}  // namespace pae::protocol_lab::v07
