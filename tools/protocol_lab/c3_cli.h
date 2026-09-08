#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "lab_types.h"
#if defined(PAE_ENABLE_C3_BINDING_TESTS)
#include "v07_run.h"
#endif

namespace pae::protocol_lab::c3 {

struct ExitPolicyInput {
  int execution_exit = 10;
  bool publication_failed = false;
  std::optional<std::string> main_codec_status;
  std::optional<std::string> expected_status;
  std::optional<std::string> comparison_status;
};

int SelectProcessExit(const ExitPolicyInput& input, std::optional<bool>& expected_matched);

bool IsInvocation(int argc, char** argv);
bool IsInvocation(const Arguments& arguments);
int PrintArgumentError(int argc, char** argv, std::string_view detail);
int Run(const Arguments& arguments);

#if defined(PAE_ENABLE_C3_BINDING_TESTS)
struct BindingTestOutput {
  int process_exit_code = 10;
  std::optional<bool> expected_matched;
  std::optional<std::string> diagnostic_id;
  std::optional<v06::Result> result;
  std::string serialized;
};

BindingTestOutput BindExecutionForTesting(const v07::RunPublishOutcome& outcome,
                                          const Arguments& arguments);
#endif

}  // namespace pae::protocol_lab::c3
