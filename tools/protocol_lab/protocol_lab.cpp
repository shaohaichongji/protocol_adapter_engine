#include "protocol_lab.h"

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli_options.h"
#include "evidence_bundle.h"
#include "lab_types.h"
#include "protocol_operations.h"
#include "result_format.h"
#include "sha256.h"

namespace pae::protocol_lab {
namespace {

int ApplyExpectedStatus(const Arguments& arguments, const OperationResult& result,
                        int natural_exit) {
  if (arguments.expect_status.empty()) {
    return natural_exit;
  }
  return result.status == arguments.expect_status ? 0 : 5;
}

void PrintResult(const OperationResult& result, std::string_view output_kind) {
  if (output_kind == "json") {
    std::cout << SerializeResult(result);
    return;
  }
  std::cout << "COMMAND=" << result.command << " STATUS=" << result.status
            << " EXIT_CODE=" << result.exit_code << '\n';
  if (!result.protocol_id.empty()) {
    std::cout << "PROTOCOL=" << result.protocol_id << " PIPELINE=" << result.pipeline_id
              << " MESSAGE=" << result.message_id << " DIRECTION=" << result.direction_id << '\n';
  }
  if (!result.frame.empty()) {
    std::cout << "FRAME_LENGTH=" << result.frame.size()
              << " FRAME_SHA256=" << HashBytes(result.frame) << '\n'
              << "FRAME_HEX=" << HexUpper(result.frame) << '\n';
  }
  for (const FieldResult& field : result.fields) {
    std::cout << "FIELD id=" << field.id << " kind=" << field.kind << " raw=" << field.raw_value
              << " logical=" << field.logical_value << '\n';
  }
  if (!result.diagnostic_id.empty()) {
    std::cout << "DIAGNOSTIC id=" << result.diagnostic_id << " detail=" << result.diagnostic_detail
              << '\n';
  }
  if (result.comparison_equal.has_value()) {
    std::cout << "COMPARISON_EQUAL=" << (*result.comparison_equal ? "true" : "false") << '\n';
  }
  for (const std::string& category : result.comparison_categories) {
    std::cout << "COMPARISON_CATEGORY=" << category << '\n';
  }
  if (!result.evidence_bundle.empty()) {
    std::cout << "EVIDENCE_BUNDLE=" << result.evidence_bundle << '\n';
  }
}

int RunInspectOrEncode(const Arguments& arguments, OperationResult& result,
                       std::string& config_text, std::optional<std::string>& values_text) {
  protocol_plan::PlanOwner plan;
  std::string error;
  if (!CompileConfig(arguments.config, config_text, plan, result, error)) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_INPUT_ERROR";
    result.diagnostic_detail = error;
    return 3;
  }
  if (!plan) {
    return 4;
  }
  if (arguments.command == Command::INSPECT) {
    std::vector<std::uint8_t> frame;
    if (!LoadFrameArgument(arguments.frame_binary, arguments.frame_hex, frame, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_FRAME_INPUT_ERROR";
      result.diagnostic_detail = error.empty() ? "binary frame must be non-empty" : error;
      return 3;
    }
    if (frame.size() > plan->GetResourceRequirements().max_frame_bytes) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_FRAME_LIMIT_EXCEEDED";
      result.diagnostic_detail = "frame exceeds compiled Plan limit";
      return 3;
    }
    result = InspectFrame(*plan, frame);
    result.command = "inspect";
    result.config_sha256 = HashBytes(config_text);
  } else {
    std::string text;
    ParsedValues parsed;
    if (!ReadText(arguments.values, text, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_VALUES_INPUT_ERROR";
      result.diagnostic_detail = error;
      return 3;
    }
    values_text = text;
    if (!ParseValues(text, parsed, error)) {
      result.status = "VALUES_INVALID";
      result.diagnostic_id = "PAE_LAB_VALUES_INVALID";
      result.diagnostic_detail = error;
      return 5;
    }
    result = EncodeValues(*plan, parsed, error);
    result.command = "encode";
    result.config_sha256 = HashBytes(config_text);
  }
  FinalizeFingerprint(result);
  return result.status == "OK" ? 0 : 5;
}

int RunReplay(const Arguments& arguments, OperationResult& result, std::string& config_text,
              std::optional<std::string>& values_text, std::filesystem::path& record_root) {
  std::string error;
  StoredRun stored;
  if (!LoadStoredRun(arguments.bundle, stored, error)) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_RUN_INPUT_ERROR";
    result.diagnostic_detail = error;
    return 3;
  }
  const std::filesystem::path config_path =
      arguments.config.empty() ? arguments.bundle / "inputs/protocol.pae.json" : arguments.config;
  protocol_plan::PlanOwner plan;
  if (!CompileConfig(config_path, config_text, plan, result, error)) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_INPUT_ERROR";
    result.diagnostic_detail = error;
    return 3;
  }
  if (!plan) {
    return 4;
  }
  if (stored.operation_kind == "inspect") {
    std::vector<std::uint8_t> frame;
    if (!ReadFile(arguments.bundle / stored.frame_file, frame, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_REPLAY_FRAME_ERROR";
      result.diagnostic_detail = error;
      return 3;
    }
    result = InspectFrame(*plan, frame);
  } else if (stored.operation_kind == "encode" && !stored.values_file.empty()) {
    std::string text;
    ParsedValues parsed;
    if (!ReadText(arguments.bundle / stored.values_file, text, error) ||
        !ParseValues(text, parsed, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_REPLAY_VALUES_ERROR";
      result.diagnostic_detail = error;
      return 3;
    }
    values_text = text;
    result = EncodeValues(*plan, parsed, error);
  } else {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_REPLAY_OPERATION_UNSUPPORTED";
    result.diagnostic_detail = "stored operation cannot be replayed by the offline slice";
    return 3;
  }
  result.command = "replay";
  result.config_sha256 = HashBytes(config_text);
  result.cross_config_replay = !arguments.config.empty();
  FinalizeFingerprint(result);
  result.comparison_equal = result.deterministic_fingerprint == stored.deterministic_fingerprint;
  result.comparison_categories = CompareStoredRuns(stored, ToStoredRun(result));
  record_root =
      arguments.record_root.empty() ? arguments.bundle.parent_path() : arguments.record_root;
  if (result.status != "OK") {
    return 5;
  }
  return *result.comparison_equal ? 0 : 6;
}

int RunCompare(const Arguments& arguments, OperationResult& result) {
  result.command = "compare";
  result.operation_kind = "compare";
  std::string error;
  bool equal = false;
  if (!arguments.left_run.empty()) {
    StoredRun left;
    StoredRun right;
    if (!LoadStoredRun(arguments.left_run, left, error) ||
        !LoadStoredRun(arguments.right_run, right, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_COMPARE_RUN_ERROR";
      result.diagnostic_detail = error;
      return 3;
    }
    result.comparison_categories = CompareStoredRuns(left, right);
    equal = left.deterministic_fingerprint == right.deterministic_fingerprint &&
            result.comparison_categories.empty();
  } else {
    std::vector<std::uint8_t> left;
    std::vector<std::uint8_t> right;
    if (!LoadFrameArgument(arguments.left_frame_binary, arguments.left_frame_hex, left, error) ||
        !LoadFrameArgument(arguments.right_frame_binary, arguments.right_frame_hex, right, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_COMPARE_FRAME_ERROR";
      result.diagnostic_detail = error;
      return 3;
    }
    equal = left == right;
    result.frame = left;
    if (!equal) {
      result.comparison_categories.emplace_back("WIRE_BYTES");
    }
  }
  result.comparison_equal = equal;
  result.status = equal ? "EQUAL" : "DIFFERENT";
  if (!equal) {
    result.diagnostic_id = "PAE_LAB_COMPARE_DIFFERENT";
    result.diagnostic_detail = "deterministic protocol content differs";
  }
  FinalizeFingerprint(result);
  return equal ? 0 : 6;
}

}  // namespace

int RunApplicationWithFileSystem(int argc, char** argv, RecordFileSystem& file_system) {
  Arguments arguments;
  bool early_success = false;
  if (!ParseArguments(argc, argv, arguments, early_success)) {
    PrintUsage();
    return 2;
  }
  if (early_success) {
    return 0;
  }

  OperationResult result;
  result.command = CommandName(arguments.command);
  std::string config_text;
  std::optional<std::string> values_text;
  std::filesystem::path record_root = arguments.record_root;
  int exit_code = 10;
  if (arguments.command == Command::INSPECT || arguments.command == Command::ENCODE) {
    exit_code = RunInspectOrEncode(arguments, result, config_text, values_text);
  } else if (arguments.command == Command::REPLAY) {
    exit_code = RunReplay(arguments, result, config_text, values_text, record_root);
  } else {
    exit_code = RunCompare(arguments, result);
  }

  exit_code = ApplyExpectedStatus(arguments, result, exit_code);
  result.exit_code = exit_code;
  if (!record_root.empty() && arguments.command != Command::COMPARE && !config_text.empty()) {
    std::string record_error;
    if (!CreateEvidenceBundle(record_root, config_text, values_text, result, file_system,
                              record_error)) {
      result.status = "RECORD_FAILED";
      result.diagnostic_id = "PAE_LAB_RECORD_FAILED";
      result.diagnostic_detail = record_error;
      result.evidence_bundle.clear();
      result.exit_code = kRecordFailedExitCode;
      FinalizeFingerprint(result);
      PrintResult(result, arguments.output);
      return kRecordFailedExitCode;
    }
  }
  PrintResult(result, arguments.output);
  return result.exit_code;
}

int RunApplication(int argc, char** argv) {
  StandardRecordFileSystem file_system;
  return RunApplicationWithFileSystem(argc, argv, file_system);
}

}  // namespace pae::protocol_lab
