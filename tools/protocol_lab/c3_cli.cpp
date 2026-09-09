#include "c3_cli.h"

#include <yyjson.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cli_options.h"
#include "cli_path_encoding_internal.h"
#include "protocol_operations.h"
#include "v06_format.h"
#include "v07_run.h"

namespace pae::protocol_lab::c3 {
namespace {

constexpr std::string_view kCliFormat = "pae.lab.cli/0.1";
#if defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
constexpr std::string_view kC3ToolVersion = "0.8.0-length-cli-slice";
#elif defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
constexpr std::string_view kC3ToolVersion = "0.7.0-crc-cli-slice";
#else
constexpr std::string_view kC3ToolVersion = "0.6.0-decimal-cli-slice";
#endif

struct Envelope {
  std::string command;
  int process_exit_code = 10;
  std::optional<std::string> diagnostic_id;
  std::string diagnostic_detail;
  std::optional<std::string> current_terminal_status;
  std::optional<std::string> expected_status;
  std::optional<bool> expected_matched;
  std::optional<v07::ComparisonFacts> comparison;
  std::optional<std::filesystem::path> published_bundle;
  std::optional<v06::Result> result;
};

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

std::string JsonString(const std::optional<std::string>& value) {
  return value.has_value() ? Quote(*value) : "null";
}

std::string JsonBoolean(const std::optional<bool>& value) {
  if (!value.has_value()) return "null";
  return *value ? "true" : "false";
}

std::string Serialize(const Envelope& envelope) {
  std::string error;
  const std::string result =
      envelope.result.has_value() ? v06::SerializeResult(*envelope.result, error) : "null";
  if (!error.empty()) return {};
  const std::optional<std::string> bundle =
      envelope.published_bundle.has_value()
          ? std::optional<std::string>{EncodePathForCli(*envelope.published_bundle)}
          : std::nullopt;
  std::ostringstream output;
  output << "{\n"
         << "  \"format_version\":" << Quote(kCliFormat) << ",\n"
         << "  \"command\":" << Quote(envelope.command) << ",\n"
         << "  \"process_exit_code\":" << envelope.process_exit_code << ",\n"
         << "  \"diagnostic\":{\"id\":" << JsonString(envelope.diagnostic_id)
         << ",\"detail\":" << Quote(envelope.diagnostic_detail) << "},\n"
         << "  \"current_terminal_status\":" << JsonString(envelope.current_terminal_status)
         << ",\n"
         << "  \"expectation\":{\"status\":" << JsonString(envelope.expected_status)
         << ",\"matched\":" << JsonBoolean(envelope.expected_matched) << "},\n"
         << "  \"comparison\":";
  if (envelope.comparison.has_value()) {
    output << "{\"status\":" << Quote(envelope.comparison->status)
           << ",\"reason\":" << Quote(envelope.comparison->reason) << '}';
  } else {
    output << "null";
  }
  output << ",\n  \"published_bundle\":" << JsonString(bundle) << ",\n"
         << "  \"result\":" << result << "\n}\n";
  return output.str();
}

void Print(const Envelope& envelope, std::string_view output_kind) {
  if (output_kind == "json") {
    std::cout << Serialize(envelope);
    return;
  }
  std::cout << "CLI_FORMAT=" << kCliFormat << " COMMAND=" << envelope.command
            << " EXIT_CODE=" << envelope.process_exit_code << '\n';
  std::cout << "CURRENT_TERMINAL_STATUS="
            << (envelope.current_terminal_status.has_value() ? *envelope.current_terminal_status
                                                             : "null")
            << '\n';
  std::cout << "EXPECT_STATUS="
            << (envelope.expected_status.has_value() ? *envelope.expected_status : "null")
            << " EXPECT_MATCHED=";
  if (envelope.expected_matched.has_value())
    std::cout << (*envelope.expected_matched ? "true" : "false");
  else
    std::cout << "null";
  std::cout << '\n';
  if (envelope.comparison.has_value())
    std::cout << "COMPARISON_STATUS=" << envelope.comparison->status
              << " COMPARISON_REASON=" << envelope.comparison->reason << '\n';
  std::cout << "PUBLISHED_BUNDLE="
            << (envelope.published_bundle.has_value() ? EncodePathForCli(*envelope.published_bundle)
                                                      : "null")
            << '\n';
  if (envelope.diagnostic_id.has_value())
    std::cout << "DIAGNOSTIC id=" << *envelope.diagnostic_id
              << " detail=" << envelope.diagnostic_detail << '\n';
  if (envelope.result.has_value()) {
    std::string error;
    std::cout << v06::SerializeResult(*envelope.result, error);
  }
}

std::optional<std::string_view> OptionValue(int argc, char** argv, std::string_view name) {
  for (int index = 2; index + 1 < argc; ++index) {
    if (std::string_view{argv[index]} == name) return std::string_view{argv[index + 1]};
  }
  return std::nullopt;
}

bool IsSchemaV05(const std::filesystem::path& path, bool route_unclassified) {
  std::string text;
  std::string error;
  if (!ReadText(path, text, error)) return route_unclassified;
  yyjson_read_err parse_error{};
  yyjson_doc* document =
      yyjson_read_opts(text.data(), text.size(), YYJSON_READ_NOFLAG, nullptr, &parse_error);
  if (!document) return route_unclassified;
  yyjson_val* root = yyjson_doc_get_root(document);
  yyjson_val* version = yyjson_is_obj(root) ? yyjson_obj_get(root, "schema_version") : nullptr;
  const bool classified = yyjson_is_str(version);
  const std::string_view schema_version =
      classified ? std::string_view{yyjson_get_str(version), yyjson_get_len(version)}
                 : std::string_view{};
  const bool known_legacy = schema_version == "0.1" || schema_version == "0.2" ||
                            schema_version == "0.3" || schema_version == "0.4";
  const bool matches = schema_version == "0.5"
#if defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
                       || schema_version == "0.6"
#endif
#if defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
                       || schema_version == "0.7"
#endif
                       || (route_unclassified && classified && !known_legacy);
  yyjson_doc_free(document);
  return matches || (!classified && route_unclassified);
}

bool IsRunV07(const std::filesystem::path& bundle) {
  return std::filesystem::is_regular_file(bundle / "run_record_v0.7.json") ||
#if defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC)
         std::filesystem::is_regular_file(bundle / "run_record_v0.8.json") ||
#endif
#if defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
         std::filesystem::is_regular_file(bundle / "run_record_v0.9.json") ||
#endif
         std::filesystem::is_regular_file(bundle / "events_v0.7.jsonl");
}

std::string OutputKind(int argc, char** argv) {
  const auto output = OptionValue(argc, argv, "--output");
  return output == "json" ? "json" : "text";
}

std::string NewRunId(const std::filesystem::path& root) {
  const auto tick = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
  const std::string prefix = "run_c3_" + std::to_string(tick);
  for (std::size_t suffix = 0U; suffix < 1000U; ++suffix) {
    const std::string candidate = prefix + "_" + std::to_string(suffix);
    std::error_code final_error;
    std::error_code progress_error;
    const bool final_exists = std::filesystem::exists(root / candidate, final_error);
    const bool progress_exists =
        std::filesystem::exists(root / (candidate + ".inprogress"), progress_error);
    if (!final_exists && !progress_exists && !final_error && !progress_error) return candidate;
  }
  return prefix + "_overflow";
}

const std::set<std::string>& ExpectedStatuses() {
  static const std::set<std::string> statuses{"OK",
                                              "INVALID_ARGUMENT",
                                              "INVALID_PLAN",
                                              "WORKSPACE_PLAN_MISMATCH",
                                              "WORKSPACE_BUSY",
                                              "INPUT_OUTPUT_OVERLAP",
                                              "VALUE_NOT_REPRESENTABLE",
                                              "UNKNOWN_MESSAGE",
                                              "AMBIGUOUS_MESSAGE",
                                              "OUTPUT_SLOTS_TOO_SMALL",
                                              "INTEGRITY_FAILED",
                                              "UNKNOWN_ENUM_VALUE",
                                              "MESSAGE_NOT_ALLOWED",
                                              "FIELD_REFERENCE_MISMATCH",
                                              "DUPLICATE_FIELD",
                                              "MISSING_FIELD",
                                              "TYPE_MISMATCH",
                                              "BYTES_LENGTH_MISMATCH",
                                              "ENUM_REFERENCE_MISMATCH",
                                              "CONSTANT_FIELD_OVERRIDE",
#if defined(PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH)
                                              "COMPUTED_FIELD_OVERRIDE",
                                              "LENGTH_MISMATCH",
#endif
                                              "BUFFER_TOO_SMALL"};
  return statuses;
}

bool ValidateExpectation(const Arguments& arguments, Envelope& envelope) {
  if (arguments.expect_status.empty()) return true;
  envelope.expected_status = arguments.expect_status;
  if (ExpectedStatuses().find(arguments.expect_status) == ExpectedStatuses().end()) {
    envelope.process_exit_code = 2;
    envelope.diagnostic_id = "PAE_LAB_C3_EXPECT_STATUS_INVALID";
    envelope.diagnostic_detail = "--expect-status is unknown or forbidden for the C3 command";
    return false;
  }
  return true;
}

bool IsInternal(const v07::RunRecord& record) {
  if (record.execution.terminal_status == "STRUCTURAL_ERROR" ||
      record.execution.terminal_status == "LAB_RESULT_FAILED")
    return true;
  return record.execution.main_codec.has_value() &&
         (record.execution.main_codec->status == "INTERNAL_ERROR" ||
          record.execution.main_codec->status == "FINAL_REVIEW_FAILED");
}

void BindExecution(const v07::RunPublishOutcome& outcome, const Arguments& arguments,
                   Envelope& envelope) {
  const auto& record = outcome.bundle.record;
  envelope.current_terminal_status = record.execution.terminal_status;
  envelope.published_bundle = outcome.published_path;
  envelope.result = outcome.bundle.result;
  if (record.comparison.has_value()) envelope.comparison = record.comparison;
  int execution_exit = 0;
  if (record.execution.terminal_status == "PREPARATION_FAILED") {
    envelope.diagnostic_id = record.execution.preparation.diagnostic_id;
    envelope.diagnostic_detail = record.execution.preparation.detail;
    execution_exit =
        record.execution.preparation.diagnostic_id == "PAE_LAB_C1_CONFIG_INVALID" ||
                record.execution.preparation.diagnostic_id == "PAE_LAB_C1_SCHEMA_UNSUPPORTED"
            ? 4
            : 5;
  } else if (IsInternal(record)) {
    execution_exit = 10;
    envelope.diagnostic_id = "PAE_LAB_C3_INTERNAL_ERROR";
    envelope.diagnostic_detail = "execution ended in an internal C3 terminal state";
  } else if (record.execution.terminal_status == "STRUCTURAL_REJECTED") {
    execution_exit = 5;
    envelope.diagnostic_id = "PAE_LAB_C3_STRUCTURAL_REJECTED";
    envelope.diagnostic_detail = "no unique structural candidate was selected";
  } else if (record.execution.terminal_status == "CODEC_ERROR") {
    execution_exit = 5;
    if (envelope.result.has_value()) {
      envelope.diagnostic_id = envelope.result->diagnostic_id;
      envelope.diagnostic_detail = envelope.result->diagnostic_detail;
    }
  } else {
    execution_exit = 0;
  }

  ExitPolicyInput policy;
  policy.execution_exit = execution_exit;
  if (record.execution.main_codec.has_value())
    policy.main_codec_status = record.execution.main_codec->status;
  policy.expected_status = envelope.expected_status;
  if (envelope.comparison.has_value()) policy.comparison_status = envelope.comparison->status;
  envelope.process_exit_code = SelectProcessExit(policy, envelope.expected_matched);
  if (envelope.expected_status.has_value() && record.execution.main_codec.has_value() &&
      envelope.expected_matched.has_value()) {
    if (!*envelope.expected_matched) {
      envelope.diagnostic_id = "PAE_LAB_C3_EXPECTATION_MISMATCH";
      envelope.diagnostic_detail = "actual main Codec status does not match --expect-status";
    } else if (execution_exit == 5 && envelope.process_exit_code != 6) {
      envelope.diagnostic_id.reset();
      envelope.diagnostic_detail.clear();
    }
  }
  if (arguments.command == Command::REPLAY && envelope.process_exit_code == 6) {
    envelope.diagnostic_id = "PAE_LAB_C3_REPLAY_DIFFERENT";
    envelope.diagnostic_detail = "current execution differs from the historical fingerprint";
  }
}

int RunExecute(const Arguments& arguments, Envelope& envelope) {
  if (arguments.record_root.empty()) {
    envelope.process_exit_code = 2;
    envelope.diagnostic_id = "PAE_LAB_C3_RECORD_ROOT_REQUIRED";
    envelope.diagnostic_detail = "Schema 0.5 inspect and encode require --record-root";
    return envelope.process_exit_code;
  }
  v07::RunRequest request;
  request.run_id = NewRunId(arguments.record_root);
  request.tool_version = std::string{kC3ToolVersion};
  request.operation_kind = arguments.command == Command::INSPECT ? "inspect" : "encode";
  std::string error;
  if (!ReadText(arguments.config, request.config_text, error)) {
    envelope.process_exit_code = 3;
    envelope.diagnostic_id = "PAE_LAB_C3_CONFIG_READ_FAILED";
    envelope.diagnostic_detail = error;
    return envelope.process_exit_code;
  }
  if (arguments.command == Command::INSPECT) {
    std::vector<std::uint8_t> frame;
    if (!LoadFrameArgument(arguments.frame_binary, arguments.frame_hex, frame, error)) {
      envelope.process_exit_code = 3;
      envelope.diagnostic_id = "PAE_LAB_C3_FRAME_READ_FAILED";
      envelope.diagnostic_detail = error;
      return envelope.process_exit_code;
    }
    request.frame = std::move(frame);
  } else {
    std::string values;
    if (!ReadText(arguments.values, values, error)) {
      envelope.process_exit_code = 3;
      envelope.diagnostic_id = "PAE_LAB_C3_VALUES_READ_FAILED";
      envelope.diagnostic_detail = error;
      return envelope.process_exit_code;
    }
    request.values_text = std::move(values);
  }
  v06::StandardEvidenceFileSystem file_system;
  v07::RunPublishOutcome outcome;
  if (!v07::ExecuteRunAndWrite(arguments.record_root, request, file_system, outcome)) {
    BindExecution(outcome, arguments, envelope);
    envelope.published_bundle.reset();
    envelope.process_exit_code = 7;
    envelope.diagnostic_id = "PAE_LAB_C3_EVIDENCE_PUBLISH_FAILED";
    envelope.diagnostic_detail = outcome.publication_error;
    return envelope.process_exit_code;
  }
  BindExecution(outcome, arguments, envelope);
  return envelope.process_exit_code;
}

int RunReplay(const Arguments& arguments, Envelope& envelope) {
  if (arguments.record_root.empty() || !arguments.config.empty()) {
    envelope.process_exit_code = 2;
    envelope.diagnostic_id = "PAE_LAB_C3_REPLAY_OPTIONS_INVALID";
    envelope.diagnostic_detail =
        "C3 replay requires --record-root and forbids replacement --config or Pipeline";
    return envelope.process_exit_code;
  }
  v06::StandardEvidenceFileSystem file_system;
  v07::RunPublishOutcome outcome;
  v07::EvidenceQualificationStatus qualification = v07::EvidenceQualificationStatus::INVALID_BUNDLE;
  std::string error;
  if (!v07::ReplayRunAndWrite(arguments.record_root, arguments.bundle,
                              NewRunId(arguments.record_root), std::string{kC3ToolVersion},
                              file_system, outcome, qualification, error)) {
    if (qualification == v07::EvidenceQualificationStatus::OK) {
      BindExecution(outcome, arguments, envelope);
      envelope.published_bundle.reset();
    }
    envelope.process_exit_code = qualification == v07::EvidenceQualificationStatus::OK ? 7 : 3;
    envelope.diagnostic_id = qualification == v07::EvidenceQualificationStatus::OK
                                 ? "PAE_LAB_C3_EVIDENCE_PUBLISH_FAILED"
                                 : "PAE_LAB_C3_REPLAY_EVIDENCE_INVALID";
    envelope.diagnostic_detail = error;
    return envelope.process_exit_code;
  }
  BindExecution(outcome, arguments, envelope);
  return envelope.process_exit_code;
}

int RunCompare(const Arguments& arguments, Envelope& envelope) {
  v07::CompareOutcome compared;
  v07::EvidenceQualificationStatus qualification = v07::EvidenceQualificationStatus::INVALID_BUNDLE;
  std::string error;
  if (!v07::CompareRunEvidence(arguments.left_run, arguments.right_run, compared, qualification,
                               error)) {
    envelope.process_exit_code = 3;
    envelope.diagnostic_id = "PAE_LAB_C3_COMPARE_EVIDENCE_INVALID";
    envelope.diagnostic_detail = error;
    return envelope.process_exit_code;
  }
  envelope.comparison = compared.comparison;
  envelope.process_exit_code = compared.comparison.status == "EQUAL" ? 0 : 6;
  if (envelope.process_exit_code == 6) {
    envelope.diagnostic_id = "PAE_LAB_C3_COMPARE_DIFFERENT";
    envelope.diagnostic_detail = "qualified C execution fingerprints differ";
  }
  return envelope.process_exit_code;
}

}  // namespace

int SelectProcessExit(const ExitPolicyInput& input, std::optional<bool>& expected_matched) {
  expected_matched.reset();
  if (input.publication_failed) return 7;
  if (input.execution_exit == 10) return 10;
  int selected = input.execution_exit;
  if (input.expected_status.has_value() && input.main_codec_status.has_value()) {
    expected_matched = *input.expected_status == *input.main_codec_status;
    selected = *expected_matched ? (selected == 5 ? 0 : selected) : 5;
  } else if (input.expected_status.has_value()) {
    expected_matched = false;
  }
  if (selected == 0 && input.comparison_status == std::optional<std::string>{"DIFFERENT"}) return 6;
  return selected;
}

bool IsInvocation(int argc, char** argv) {
  if (argc < 2) return false;
  const std::string_view command{argv[1]};
  if (command == "inspect" || command == "encode") {
    const auto config = OptionValue(argc, argv, "--config");
    const bool has_record_root = OptionValue(argc, argv, "--record-root").has_value();
    return config.has_value() && IsSchemaV05(DecodeCommandLinePath(*config), has_record_root);
  }
  if (command == "replay") {
    const auto bundle = OptionValue(argc, argv, "--bundle");
    return bundle.has_value() && IsRunV07(DecodeCommandLinePath(*bundle));
  }
  if (command == "compare") {
    const auto left = OptionValue(argc, argv, "--left-run");
    const auto right = OptionValue(argc, argv, "--right-run");
    return (left.has_value() && IsRunV07(DecodeCommandLinePath(*left))) ||
           (right.has_value() && IsRunV07(DecodeCommandLinePath(*right)));
  }
  return false;
}

bool IsInvocation(const Arguments& arguments) {
  if (arguments.command == Command::INSPECT || arguments.command == Command::ENCODE)
    return IsSchemaV05(arguments.config, !arguments.record_root.empty());
  if (arguments.command == Command::REPLAY) return IsRunV07(arguments.bundle);
  if (arguments.command == Command::COMPARE)
    return IsRunV07(arguments.left_run) || IsRunV07(arguments.right_run);
  return false;
}

int PrintArgumentError(int argc, char** argv, std::string_view detail) {
  Envelope envelope;
  envelope.command = argc >= 2 ? argv[1] : "unknown";
  envelope.process_exit_code = 2;
  envelope.diagnostic_id = "PAE_LAB_C3_ARGUMENT_ERROR";
  envelope.diagnostic_detail = std::string{detail};
  Print(envelope, OutputKind(argc, argv));
  return envelope.process_exit_code;
}

int Run(const Arguments& arguments) {
  Envelope envelope;
  envelope.command = CommandName(arguments.command);
  if (!ValidateExpectation(arguments, envelope)) {
    Print(envelope, arguments.output);
    return envelope.process_exit_code;
  }
  if (arguments.command == Command::COMPARE && !arguments.expect_status.empty()) {
    envelope.process_exit_code = 2;
    envelope.diagnostic_id = "PAE_LAB_C3_EXPECT_STATUS_NOT_APPLICABLE";
    envelope.diagnostic_detail = "--expect-status is not valid for compare";
  } else if (arguments.command == Command::INSPECT || arguments.command == Command::ENCODE) {
    RunExecute(arguments, envelope);
  } else if (arguments.command == Command::REPLAY) {
    RunReplay(arguments, envelope);
  } else if (arguments.command == Command::COMPARE) {
    RunCompare(arguments, envelope);
  } else {
    envelope.process_exit_code = 2;
    envelope.diagnostic_id = "PAE_LAB_C3_COMMAND_UNSUPPORTED";
    envelope.diagnostic_detail = "the C3 offline chain does not support this command";
  }
  Print(envelope, arguments.output);
  return envelope.process_exit_code;
}

#if defined(PAE_ENABLE_C3_BINDING_TESTS)
BindingTestOutput BindExecutionForTesting(const v07::RunPublishOutcome& outcome,
                                          const Arguments& arguments) {
  Envelope envelope;
  envelope.command = CommandName(arguments.command);
  if (!arguments.expect_status.empty()) envelope.expected_status = arguments.expect_status;
  BindExecution(outcome, arguments, envelope);
  BindingTestOutput output;
  output.process_exit_code = envelope.process_exit_code;
  output.expected_matched = envelope.expected_matched;
  output.diagnostic_id = envelope.diagnostic_id;
  output.result = envelope.result;
  output.serialized = Serialize(envelope);
  return output;
}
#endif

}  // namespace pae::protocol_lab::c3
