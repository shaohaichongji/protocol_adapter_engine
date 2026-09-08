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

}  // namespace pae::protocol_lab::v07
