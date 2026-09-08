#include <yyjson.h>

#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "c3_cli.h"

namespace {

bool Expect(bool condition, std::string_view message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

int Select(int execution_exit, std::optional<std::string> main_status,
           std::optional<std::string> expected, std::optional<std::string> comparison,
           bool publication_failed, std::optional<bool>& matched) {
  pae::protocol_lab::c3::ExitPolicyInput input;
  input.execution_exit = execution_exit;
  input.main_codec_status = std::move(main_status);
  input.expected_status = std::move(expected);
  input.comparison_status = std::move(comparison);
  input.publication_failed = publication_failed;
  return pae::protocol_lab::c3::SelectProcessExit(input, matched);
}

bool IsNull(yyjson_val* value) { return value != nullptr && yyjson_is_null(value); }

bool CheckBindingJson(std::string_view json, std::string_view terminal, bool result_expected) {
  yyjson_doc* document = yyjson_read(json.data(), json.size(), 0U);
  if (document == nullptr) return false;
  yyjson_val* root = yyjson_doc_get_root(document);
  yyjson_val* expectation = yyjson_obj_get(root, "expectation");
  yyjson_val* expected_status = yyjson_obj_get(expectation, "status");
  yyjson_val* matched = yyjson_obj_get(expectation, "matched");
  yyjson_val* diagnostic = yyjson_obj_get(root, "diagnostic");
  yyjson_val* diagnostic_id = yyjson_obj_get(diagnostic, "id");
  yyjson_val* terminal_value = yyjson_obj_get(root, "current_terminal_status");
  yyjson_val* result = yyjson_obj_get(root, "result");
  yyjson_val* result_status =
      result_expected ? yyjson_obj_get(result, "current_execution_status") : nullptr;
  const bool result_shape =
      result_expected ? yyjson_is_obj(result) && yyjson_is_str(result_status) &&
                            std::string_view{yyjson_get_str(result_status)} == "INTERNAL_ERROR"
                      : IsNull(result);
  const bool valid =
      yyjson_get_int(yyjson_obj_get(root, "process_exit_code")) == 10 &&
      yyjson_is_str(expected_status) && std::string_view{yyjson_get_str(expected_status)} == "OK" &&
      IsNull(matched) && yyjson_is_str(diagnostic_id) &&
      std::string_view{yyjson_get_str(diagnostic_id)} == "PAE_LAB_C3_INTERNAL_ERROR" &&
      yyjson_is_str(terminal_value) &&
      std::string_view{yyjson_get_str(terminal_value)} == terminal && result_shape;
  yyjson_doc_free(document);
  return valid;
}

pae::protocol_lab::v07::RunPublishOutcome InternalCodecOutcome() {
  pae::protocol_lab::v07::RunPublishOutcome outcome;
  auto& execution = outcome.bundle.record.execution;
  execution.terminal_status = "CODEC_ERROR";
  execution.main_codec =
      pae::protocol_lab::v07::CodecFacts{std::optional<std::string>{"ENCODE"}, "INTERNAL_ERROR"};
  pae::protocol_lab::v06::Result result;
  result.command = "encode";
  result.operation_kind = "encode";
  result.operation_status = "CODEC_ERROR";
  result.exit_code = 10;
  result.replay_mode = "ENCODE_TX";
  result.replay_subject = "TX";
  result.current_execution_status = "INTERNAL_ERROR";
  result.diagnostic_id = "PAE_LAB_CODEC_INTERNAL_ERROR";
  result.current_execution_diagnostic_id = "PAE_LAB_CODEC_INTERNAL_ERROR";
  outcome.bundle.result = std::move(result);
  return outcome;
}

pae::protocol_lab::v07::RunPublishOutcome ReviewFailureOutcome() {
  pae::protocol_lab::v07::RunPublishOutcome outcome;
  auto& execution = outcome.bundle.record.execution;
  execution.terminal_status = "LAB_RESULT_FAILED";
  execution.main_codec =
      pae::protocol_lab::v07::CodecFacts{std::optional<std::string>{"ENCODE"}, "OK"};
  return outcome;
}

}  // namespace

int main() {
  bool passed = true;
  std::optional<bool> matched;
  passed = Expect(Select(0, "OK", std::nullopt, std::nullopt, false, matched) == 0 && !matched,
                  "successful execution exits zero") &&
           passed;
  passed = Expect(Select(5, "INTEGRITY_FAILED", std::nullopt, "EQUAL", false, matched) == 5,
                  "equal ordinary failure remains exit five without expectation") &&
           passed;
  passed = Expect(Select(5, "INTEGRITY_FAILED", "INTEGRITY_FAILED", "EQUAL", false, matched) == 0 &&
                      matched == true,
                  "matching ordinary failure may exit zero") &&
           passed;
  passed =
      Expect(Select(5, "INTEGRITY_FAILED", "INTEGRITY_FAILED", "DIFFERENT", false, matched) == 6 &&
                 matched == true,
             "Replay difference outranks a matching ordinary expectation") &&
      passed;
  passed = Expect(Select(10, "INTERNAL_ERROR", "INTERNAL_ERROR", "EQUAL", false, matched) == 10 &&
                      !matched,
                  "internal error cannot be converted by expectation") &&
           passed;
  passed =
      Expect(Select(4, std::nullopt, "OK", std::nullopt, false, matched) == 4 && matched == false,
             "configuration preparation failure keeps exit four") &&
      passed;
  passed =
      Expect(Select(5, std::nullopt, "OK", std::nullopt, false, matched) == 5 && matched == false,
             "structural rejection cannot satisfy an expectation") &&
      passed;
  passed = Expect(Select(0, "OK", "OK", "EQUAL", true, matched) == 7 && !matched,
                  "publication failure outranks execution and expectation") &&
           passed;
  passed = Expect(Select(10, "INTERNAL_ERROR", "OK", "EQUAL", true, matched) == 7 && !matched,
                  "publication failure outranks internal execution") &&
           passed;

  pae::protocol_lab::Arguments arguments;
  arguments.command = pae::protocol_lab::Command::ENCODE;
  arguments.expect_status = "OK";
  const auto internal =
      pae::protocol_lab::c3::BindExecutionForTesting(InternalCodecOutcome(), arguments);
  passed = Expect(internal.process_exit_code == 10 && !internal.expected_matched.has_value() &&
                      internal.diagnostic_id ==
                          std::optional<std::string>{"PAE_LAB_C3_INTERNAL_ERROR"} &&
                      internal.result.has_value() &&
                      internal.result->current_execution_status == "INTERNAL_ERROR" &&
                      CheckBindingJson(internal.serialized, "CODEC_ERROR", true),
                  "internal Codec binding keeps null expectation and original Result") &&
           passed;

  const auto review =
      pae::protocol_lab::c3::BindExecutionForTesting(ReviewFailureOutcome(), arguments);
  passed =
      Expect(review.process_exit_code == 10 && !review.expected_matched.has_value() &&
                 review.diagnostic_id == std::optional<std::string>{"PAE_LAB_C3_INTERNAL_ERROR"} &&
                 !review.result.has_value() &&
                 CheckBindingJson(review.serialized, "LAB_RESULT_FAILED", false),
             "review failure binding keeps null expectation and null Result") &&
      passed;
  return passed ? 0 : 1;
}
