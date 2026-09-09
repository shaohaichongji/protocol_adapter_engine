#include "v07_run.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>

#include "complete_record_codec.h"
#include "config_compiler.h"
#include "plan_bundle.h"
#include "sha256.h"
#include "v06_values_compat_internal.h"

namespace pae::protocol_lab::v07 {
namespace {

std::string PhaseName(v06::ExecutionPhase phase) {
  switch (phase) {
    case v06::ExecutionPhase::PREPARATION:
      return "PREPARATION";
    case v06::ExecutionPhase::STRUCTURAL_QUERY:
      return "STRUCTURAL_QUERY";
    case v06::ExecutionPhase::MAIN_CODEC:
      return "MAIN_CODEC";
    case v06::ExecutionPhase::REVIEW_DECODE:
      return "REVIEW_DECODE";
    case v06::ExecutionPhase::RESULT_MAPPING:
      return "RESULT_MAPPING";
  }
  return "";
}

class EventCollector final : public v06::ExecutionObserver {
 public:
  explicit EventCollector(std::string run_id)
      : run_id_(std::move(run_id)), origin_(std::chrono::steady_clock::now()) {
    events_.push_back({1U, 0U, "RUN_STARTED", "NONE", std::nullopt});
  }

  void PhaseStarted(v06::ExecutionPhase phase) override {
    events_.push_back({NextId(), Offset(), "PHASE_STARTED", PhaseName(phase), std::nullopt});
  }

  void PhaseFinished(v06::ExecutionPhase phase, std::string_view status) override {
    events_.push_back(
        {NextId(), Offset(), "PHASE_FINISHED", PhaseName(phase), std::string{status}});
  }

  void Finish(std::string_view phase, std::string_view status) {
    events_.push_back(
        {NextId(), Offset(), "RUN_FINISHED", std::string{phase}, std::string{status}});
  }

  const std::vector<StageEvent>& Events() const noexcept { return events_; }

 private:
  std::uint64_t NextId() const { return static_cast<std::uint64_t>(events_.size() + 1U); }

  std::uint64_t Offset() const {
    const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::steady_clock::now() - origin_)
                             .count();
    return elapsed < 0 ? 0U : static_cast<std::uint64_t>(elapsed);
  }

  std::string run_id_;
  std::chrono::steady_clock::time_point origin_;
  std::vector<StageEvent> events_;
};

std::optional<ResultMappingFacts> FindResultMapping(const std::vector<StageEvent>& events) {
  std::optional<ResultMappingFacts> result;
  for (const auto& event : events) {
    if (event.event_kind == "PHASE_FINISHED" && event.phase == "RESULT_MAPPING" &&
        event.status.has_value())
      result = ResultMappingFacts{*event.status};
  }
  return result;
}

std::string CandidateClass(std::string_view status) {
  if (status == "OK") return "ONE";
  if (status == "UNKNOWN_MESSAGE") return "ZERO";
  if (status == "AMBIGUOUS_MESSAGE") return "MULTIPLE";
  return "UNDETERMINED";
}

void SetTerminal(const v06::ExecutionOutcome& outcome, ExecutionFacts& execution) {
  if (!outcome.preparation_failure.diagnostic_id.empty()) {
    execution.terminal_stage = "PREPARATION";
    execution.terminal_status = "PREPARATION_FAILED";
    execution.terminal_reason = "PREPARATION_REJECTED";
    return;
  }
  if (!outcome.structural_status.empty() && !outcome.main_codec_called) {
    execution.terminal_stage = "STRUCTURAL_QUERY";
    const bool rejected = outcome.structural_status == "UNKNOWN_MESSAGE" ||
                          outcome.structural_status == "AMBIGUOUS_MESSAGE";
    execution.terminal_status = rejected ? "STRUCTURAL_REJECTED" : "STRUCTURAL_ERROR";
    execution.terminal_reason = rejected ? "STRUCTURAL_REJECTED" : "STRUCTURAL_QUERY_ERROR";
    return;
  }
  if (outcome.materialization_failure != v06::MaterializationFailure::NONE) {
    execution.terminal_status = "LAB_RESULT_FAILED";
    switch (outcome.materialization_failure) {
      case v06::MaterializationFailure::REVIEW_DECODE_FAILED:
        execution.terminal_stage = "REVIEW_DECODE";
        execution.terminal_reason = "REVIEW_DECODE_FAILED";
        return;
      case v06::MaterializationFailure::REVIEW_MESSAGE_MISMATCH:
        execution.terminal_stage = "REVIEW_DECODE";
        execution.terminal_reason = "REVIEW_MESSAGE_MISMATCH";
        return;
      case v06::MaterializationFailure::RAW_ASSOCIATION_FAILED:
        execution.terminal_stage = "RESULT_MAPPING";
        execution.terminal_reason = "RAW_ASSOCIATION_FAILED";
        return;
      case v06::MaterializationFailure::INTERNAL_ERROR:
        execution.terminal_stage = "RESULT_MAPPING";
        execution.terminal_reason = "INTERNAL_ERROR";
        return;
      case v06::MaterializationFailure::NONE:
        break;
    }
  }
  execution.terminal_stage = "RESULT_MAPPING";
  execution.terminal_reason = "NONE";
  execution.terminal_status = outcome.main_codec_status == "OK" ? "OK" : "CODEC_ERROR";
}

RunBundleInput BuildBundle(const RunRequest& request, const v06::ExecutionOutcome& outcome,
                           const EventCollector& collector) {
  RunBundleInput bundle;
  bundle.config_text = request.config_text;
  bundle.values_text = request.values_text;
  bundle.result = outcome.result;
  bundle.record.run_id = request.run_id;
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
  const bool crc_generation =
      outcome.schema_version == "0.6" ||
      (outcome.result.has_value() && outcome.result->format_version == v06::kCrcResultFormat);
  if (crc_generation) bundle.record.format_version = std::string{kCrcRecordFormat};
#else
  const bool crc_generation = false;
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  const bool length_generation =
      outcome.schema_version == "0.7" ||
      (outcome.result.has_value() && outcome.result->format_version == v06::kLengthResultFormat);
  if (length_generation) bundle.record.format_version = std::string{kLengthRecordFormat};
#else
  const bool length_generation = false;
#endif
  bundle.record.tool_version = request.tool_version;
  bundle.record.operation_kind = request.operation_kind;
  bundle.record.invocation_kind = request.invocation_kind;
  bundle.record.parent_run_id = request.parent_run_id;
  bundle.record.requested_pipeline_id = request.requested_pipeline_id;
  bundle.record.values_file = request.values_text.has_value()
                                  ? std::optional<std::string>{"inputs/values.pae-lab.json"}
                                  : std::nullopt;
  if (request.operation_kind == "inspect") {
    bundle.frame = request.frame;
  } else if (outcome.result.has_value() && outcome.main_codec_status == "OK" &&
             outcome.materialization_failure == v06::MaterializationFailure::NONE) {
    bundle.frame = outcome.encoded_frame;
  }
  bundle.record.frame_file = bundle.frame.has_value()
                                 ? std::optional<std::string>{"frames/000001_frame.bin"}
                                 : std::nullopt;
  bundle.record.result_file =
      outcome.result.has_value()
          ? std::optional<std::string>{length_generation ? "result_summary_v0.8.json"
                                       : crc_generation  ? "result_summary_v0.7.json"
                                                         : "result_summary_v0.6.json"}
          : std::nullopt;
  if (outcome.result.has_value()) {
    std::string error;
    bundle.record.deterministic_fingerprint = v06::FinalizeFingerprint(*outcome.result, error);
  }

  auto& execution = bundle.record.execution;
  execution.preparation.status =
      outcome.preparation_failure.diagnostic_id.empty() ? "OK" : "FAILED";
  if (!outcome.preparation_failure.diagnostic_id.empty()) {
    execution.preparation.diagnostic_id = outcome.preparation_failure.diagnostic_id;
    execution.preparation.detail = outcome.preparation_failure.detail;
    if (outcome.preparation_failure.value_index.has_value())
      execution.preparation.value_index =
          static_cast<std::uint64_t>(*outcome.preparation_failure.value_index);
  }
  if (!outcome.structural_status.empty() ||
      (request.operation_kind == "inspect" && outcome.main_codec_called)) {
    const std::string structural_status =
        outcome.structural_status.empty() ? "OK" : outcome.structural_status;
    execution.structural_query =
        StructuralQueryFacts{structural_status, CandidateClass(structural_status)};
  }
  if (outcome.main_codec_called) {
    execution.main_codec =
        CodecFacts{request.operation_kind == "encode" ? std::optional<std::string>{"ENCODE"}
                                                      : std::optional<std::string>{"DECODE"},
                   outcome.main_codec_status};
  }
  if (outcome.review_decode_called)
    execution.review_decode = CodecFacts{std::nullopt, outcome.review_decode_status};
  execution.result_mapping = FindResultMapping(collector.Events());
  execution.counts.structural_query_calls = outcome.counts.structural_query_calls;
  execution.counts.encode_calls = outcome.counts.encode_calls;
  execution.counts.decode_calls = outcome.counts.decode_calls;
  execution.counts.review_decode_calls = outcome.counts.review_decode_calls;
  SetTerminal(outcome, execution);
  if (request.invocation_kind == "REPLAY") {
    bundle.parent_record_text = request.parent_record_text;
    bundle.historical_result_text = request.historical_result_text;
    if (request.parent_record_text.has_value() && request.historical_result_text.has_value() &&
        request.historical_fingerprint.has_value()) {
      bundle.record.historical_baseline =
          HistoricalBaseline{length_generation ? "history/parent_record_v0.9.json"
                             : crc_generation  ? "history/parent_record_v0.8.json"
                                               : "history/parent_record_v0.7.json",
                             HashBytes(*request.parent_record_text),
                             length_generation ? "history/result_summary_v0.8.json"
                             : crc_generation  ? "history/result_summary_v0.7.json"
                                               : "history/result_summary_v0.6.json",
                             HashBytes(*request.historical_result_text),
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                             std::string{length_generation ? v06::kLengthFingerprintDomain
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
                                         : crc_generation ? v06::kCrcFingerprintDomain
                                                          : v06::kFingerprintDomain},
#else
                                                           : v06::kFingerprintDomain},
#endif
#else
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
                             std::string{crc_generation ? v06::kCrcFingerprintDomain
                                                        : v06::kFingerprintDomain},
#else
                             std::string{v06::kFingerprintDomain},
#endif
#endif
                             *request.historical_fingerprint};
    }
    if (bundle.record.deterministic_fingerprint.has_value() &&
        request.historical_fingerprint.has_value()) {
      const bool equal =
          *bundle.record.deterministic_fingerprint == *request.historical_fingerprint;
      bundle.record.comparison = ComparisonFacts{
          equal ? "EQUAL" : "DIFFERENT", equal ? "FINGERPRINT_EQUAL" : "FINGERPRINT_DIFFERENT"};
    } else {
      bundle.record.comparison = ComparisonFacts{"NOT_EVALUATED", "CURRENT_RESULT_UNAVAILABLE"};
    }
  }
  bundle.events = collector.Events();
  return bundle;
}

}  // namespace

bool ExecuteRunAndWrite(const std::filesystem::path& record_root, const RunRequest& request,
                        v06::EvidenceFileSystem& file_system, RunPublishOutcome& output) {
  output = RunPublishOutcome{};
  EventCollector collector{request.run_id};
  collector.PhaseStarted(v06::ExecutionPhase::PREPARATION);

  v06::PreparationFailure preparation_failure;
  std::unique_ptr<v06::ExecutionBridge> bridge =
      v06::ExecutionBridge::Prepare(request.config_text, preparation_failure);
  v06::ExecutionOutcome execution;
  if (!bridge) {
    execution.preparation_failure = std::move(preparation_failure);
    collector.PhaseFinished(v06::ExecutionPhase::PREPARATION, "FAILED");
  } else if (request.operation_kind == "inspect" && request.frame.has_value() &&
             !request.values_text.has_value()) {
    collector.PhaseFinished(v06::ExecutionPhase::PREPARATION, "OK");
    execution = request.requested_pipeline_id.has_value()
                    ? bridge->InspectPipeline(*request.frame, *request.requested_pipeline_id
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                              ,
                                              request.test_hooks
#endif
                                              ,
                                              &collector)
                    : bridge->Inspect(*request.frame
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                      ,
                                      request.test_hooks
#endif
                                      ,
                                      &collector);
  } else if (request.operation_kind == "encode" && request.values_text.has_value() &&
             !request.frame.has_value()) {
    execution = bridge->EncodeValuesText(*request.values_text
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                         ,
                                         request.test_hooks
#endif
                                         ,
                                         &collector);
  } else {
    execution.preparation_failure.diagnostic_id = "PAE_LAB_C1_VALUES_INVALID";
    execution.preparation_failure.detail = "RUN input roles do not match operation_kind";
    collector.PhaseFinished(v06::ExecutionPhase::PREPARATION, "FAILED");
  }

  output.bundle = BuildBundle(request, execution, collector);
  collector.Finish(output.bundle.record.execution.terminal_stage,
                   output.bundle.record.execution.terminal_status);
  output.bundle.events = collector.Events();
  if (!WriteRunBundle(record_root, output.bundle, file_system, output.published_path,
                      output.publication_error)) {
    output.published_path.clear();
    return false;
  }
  return true;
}

namespace {

bool TextEquals(std::string_view left, std::string_view right) {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

std::size_t FindPipeline(const protocol_plan::PlanBundle& plan, std::string_view id) {
  for (std::size_t index = 0U; index < plan.Pipelines().size(); ++index) {
    if (TextEquals(plan.Pipelines()[index].id, id)) return index;
  }
  return protocol_core::kInvalidIndex;
}

std::size_t FindMessage(const protocol_plan::PlanBundle& plan, std::string_view id) {
  for (std::size_t index = 0U; index < plan.Messages().size(); ++index) {
    if (TextEquals(plan.Messages()[index].id, id)) return index;
  }
  return protocol_core::kInvalidIndex;
}

bool PipelineAllows(const protocol_plan::PlanBundle& plan, std::size_t pipeline,
                    std::size_t message) {
  if (pipeline >= plan.Pipelines().size()) return false;
  const auto& indices = plan.Pipelines()[pipeline].message_indices;
  return std::find(indices.begin(), indices.end(), message) != indices.end();
}

std::string ValueTypeName(protocol_plan::ValueType type) {
  switch (type) {
    case protocol_plan::ValueType::UINT64:
      return "UINT64";
    case protocol_plan::ValueType::INT64:
      return "INT64";
    case protocol_plan::ValueType::BYTES:
      return "BYTES";
    case protocol_plan::ValueType::ENUM:
      return "ENUM";
    case protocol_plan::ValueType::BOOL:
      return "BOOL";
  }
  return {};
}

bool HasConversion(const protocol_plan::PlanBundle& plan, std::size_t message, std::size_t field) {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  return message < plan.MessageExecutionPlans().size() &&
         field < plan.MessageExecutionPlans()[message].fields.size() &&
         plan.MessageExecutionPlans()[message].fields[field].conversion_index !=
             protocol_core::kInvalidIndex;
#else
  (void)plan;
  (void)message;
  (void)field;
  return false;
#endif
}

}  // namespace

bool QualifyRunEvidence(const StoredRunBundle& bundle, EvidenceQualificationStatus& status,
                        std::string& error) {
  status = EvidenceQualificationStatus::PLAN_INVALID;
  error.clear();
  if (!bundle.result.has_value()) {
    status = EvidenceQualificationStatus::RESULT_UNAVAILABLE;
    error = "Run Evidence has no Result and is not eligible for execution comparison";
    return false;
  }
  auto compiled = config_compiler::CompileJsonToPlan(bundle.config_text);
  if (!compiled.Succeeded()) {
    error = "Run Evidence configuration cannot be compiled for Plan qualification";
    return false;
  }
  auto owner = std::move(compiled).TakePlan();
  const auto& plan = *owner;
  if (plan.SchemaVersion() != "0.5"
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      && plan.SchemaVersion() != "0.6"
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      && plan.SchemaVersion() != "0.7"
#endif
  ) {
    error = "Run Evidence Plan is outside the enabled C execution generations";
    return false;
  }
  status = EvidenceQualificationStatus::PLAN_MISMATCH;
  const auto& result = *bundle.result;
  const bool generation_matches =
      (plan.SchemaVersion() == "0.5" && result.format_version == v06::kResultFormat &&
       bundle.record.format_version == kRecordFormat)
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      || (plan.SchemaVersion() == "0.6" && result.format_version == v06::kCrcResultFormat &&
          bundle.record.format_version == kCrcRecordFormat)
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      || (plan.SchemaVersion() == "0.7" && result.format_version == v06::kLengthResultFormat &&
          bundle.record.format_version == kLengthRecordFormat)
#endif
      ;
  if (!generation_matches) {
    error = "Result and Run Record generations do not bind the compiled Plan";
    return false;
  }
  if (!result.protocol_id.has_value() || !TextEquals(plan.ProtocolId(), *result.protocol_id) ||
      !result.pipeline_id.has_value() || !result.message_id.has_value()) {
    error = "Result identity does not bind the compiled Plan";
    return false;
  }
  const std::size_t pipeline = FindPipeline(plan, *result.pipeline_id);
  const std::size_t message = FindMessage(plan, *result.message_id);
  if (pipeline == protocol_core::kInvalidIndex || message == protocol_core::kInvalidIndex ||
      !PipelineAllows(plan, pipeline, message) || !result.direction_id.has_value() ||
      !TextEquals(plan.Messages()[message].direction_id, *result.direction_id)) {
    error = "Result Pipeline, Message, or direction does not bind the compiled Plan";
    return false;
  }
  if (bundle.record.invocation_kind == "REPLAY" && bundle.record.operation_kind == "inspect" &&
      bundle.record.requested_pipeline_id != result.pipeline_id) {
    error = "Replay requested Pipeline does not bind the current Result";
    return false;
  }
  if (result.operation_status == "OK") {
    if (result.fields.size() != plan.Messages()[message].fields.size()) {
      error = "successful Result field count does not bind the compiled Plan";
      return false;
    }
    for (std::size_t index = 0U; index < result.fields.size(); ++index) {
      const auto& actual = result.fields[index];
      const auto& expected = plan.Messages()[message].fields[index];
      const std::string wire_kind = ValueTypeName(expected.value_type);
      if (!TextEquals(expected.id, actual.id) ||
          (HasConversion(plan, message, index)
               ? (actual.kind != "DECIMAL64" || actual.raw_kind != wire_kind)
               : (actual.kind != wire_kind || actual.raw_kind.has_value()))) {
        error = "successful Result field order, type, or raw tag does not bind the Plan";
        return false;
      }
    }
  }
  if (result.failed_field_index.has_value()) {
    const std::size_t index = *result.failed_field_index;
    if (index >= plan.Messages()[message].fields.size() || !result.failed_field_id.has_value() ||
        !TextEquals(plan.Messages()[message].fields[index].id, *result.failed_field_id)) {
      error = "failed field identity does not bind the compiled Plan";
      return false;
    }
    if (result.conversion_error.has_value() &&
        !HasConversion(plan, message, *result.failed_field_index)) {
      error = "conversion failure identity does not bind a converted Plan field";
      return false;
    }
  }
  if (bundle.record.operation_kind == "inspect" &&
      result.current_execution_status == "INTEGRITY_FAILED" &&
      !plan.Messages()[message].integrity.has_value()) {
    error = "integrity failure does not bind a Message integrity rule";
    return false;
  }
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  if (result.current_execution_status == "LENGTH_MISMATCH" &&
      (!result.failed_field_index.has_value() ||
       !plan.Messages()[message].computed_length.has_value() ||
       plan.Messages()[message].computed_length->field_index != *result.failed_field_index)) {
    error = "length mismatch does not bind the Message computed length field";
    return false;
  }
  if (result.current_execution_status == "COMPUTED_FIELD_OVERRIDE" &&
      (!result.failed_field_index.has_value() || !result.failed_value_index.has_value() ||
       !plan.Messages()[message].computed_length.has_value() ||
       plan.Messages()[message].computed_length->field_index != *result.failed_field_index)) {
    error =
        "computed field override does not bind the Message computed length field and Values input";
    return false;
  }
#endif
  if (bundle.record.operation_kind == "inspect" &&
      result.current_execution_status == "UNKNOWN_ENUM_VALUE" &&
      (!result.failed_field_index.has_value() ||
       plan.Messages()[message].fields[*result.failed_field_index].value_type !=
           protocol_plan::ValueType::ENUM)) {
    error = "unknown enum failure does not bind an ENUM Plan field";
    return false;
  }
  if (bundle.record.operation_kind == "encode") {
    if (!bundle.values_text.has_value()) {
      error = "Encode Run has no Values material";
      return false;
    }
    v06::ParsedValues values;
    std::string values_text = *bundle.values_text;
    if (!v06::internal::ParseCompatibleValues(values_text, values, error)) {
      error = "Encode Run Values cannot be parsed during Plan qualification: " + error;
      return false;
    }
    if (values.pipeline_id != *result.pipeline_id || values.message_id != *result.message_id) {
      error = "Encode Values identity does not bind the Result and Plan";
      return false;
    }
    if (result.failed_value_index.has_value()) {
      const std::size_t index = *result.failed_value_index;
      if (index >= values.fields.size()) {
        error = "failed Values index is outside the recorded author order";
        return false;
      }
      if (result.failed_field_id.has_value() &&
          values.fields[index].id != *result.failed_field_id) {
        error = "failed Values index does not bind the failed field identity";
        return false;
      }
      if (result.failed_field_index.has_value()) {
        const std::string expected_kind =
            HasConversion(plan, message, *result.failed_field_index)
                ? "DECIMAL64"
                : ValueTypeName(
                      plan.Messages()[message].fields[*result.failed_field_index].value_type);
        const bool type_matches = values.fields[index].kind == expected_kind;
        if (result.current_execution_status == "BYTES_LENGTH_MISMATCH" &&
            (plan.Messages()[message].fields[*result.failed_field_index].value_type !=
                 protocol_plan::ValueType::BYTES ||
             !type_matches)) {
          error = "BYTES_LENGTH_MISMATCH requires a BYTES Plan field and BYTES Values input";
          return false;
        }
        if ((result.current_execution_status == "TYPE_MISMATCH" && type_matches) ||
            (result.current_execution_status == "VALUE_NOT_REPRESENTABLE" && !type_matches)) {
          error = "Codec failure status is not applicable to the recorded Values type";
          return false;
        }
      }
    }
  }
  if (bundle.record.operation_kind == "inspect") {
    const std::uint64_t expected_queries =
        bundle.record.invocation_kind == "REPLAY" ? 1U : plan.Pipelines().size();
    if (bundle.record.execution.structural_query.has_value() &&
        bundle.record.execution.counts.structural_query_calls > expected_queries) {
      error = "structural query count exceeds the qualified Plan bound";
      return false;
    }
  }
  status = EvidenceQualificationStatus::OK;
  return true;
}

bool ReplayRunAndWrite(const std::filesystem::path& record_root,
                       const std::filesystem::path& parent_bundle, std::string run_id,
                       std::string tool_version, v06::EvidenceFileSystem& file_system,
                       RunPublishOutcome& output, EvidenceQualificationStatus& status,
                       std::string& error
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                       ,
                       const v06::ExecutionTestHooks* test_hooks
#endif
) {
  output = RunPublishOutcome{};
  StoredRunBundle parent;
  if (!LoadRunBundleForTest(parent_bundle, parent, file_system, error)) {
    status = EvidenceQualificationStatus::INVALID_BUNDLE;
    return false;
  }
  if (!QualifyRunEvidence(parent, status, error)) return false;
  RunRequest request;
  request.run_id = std::move(run_id);
  request.tool_version = std::move(tool_version);
  request.operation_kind = parent.record.operation_kind;
  request.config_text = parent.config_text;
  request.values_text = parent.values_text;
  if (request.operation_kind == "inspect") request.frame = parent.frame;
  request.invocation_kind = "REPLAY";
  request.parent_run_id = parent.record.run_id;
  if (request.operation_kind == "inspect")
    request.requested_pipeline_id = parent.result->pipeline_id;
  request.parent_record_text = parent.record_text;
  request.historical_result_text = parent.result_text;
  request.historical_fingerprint = parent.record.deterministic_fingerprint;
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  request.test_hooks = test_hooks;
#endif
  if (!ExecuteRunAndWrite(record_root, request, file_system, output)) {
    error = output.publication_error;
    return false;
  }
  status = EvidenceQualificationStatus::OK;
  return true;
}

bool CompareRunEvidence(const std::filesystem::path& left_bundle,
                        const std::filesystem::path& right_bundle, CompareOutcome& output,
                        EvidenceQualificationStatus& status, std::string& error) {
  output = CompareOutcome{};
  StoredRunBundle left;
  StoredRunBundle right;
  if (!LoadRunBundle(left_bundle, left, error) || !LoadRunBundle(right_bundle, right, error)) {
    status = EvidenceQualificationStatus::INVALID_BUNDLE;
    return false;
  }
  if (!QualifyRunEvidence(left, status, error) || !QualifyRunEvidence(right, status, error))
    return false;
  if (left.record.operation_kind != right.record.operation_kind) {
    status = EvidenceQualificationStatus::PLAN_MISMATCH;
    error = "independent Compare requires matching execution operations";
    return false;
  }
  if (left.record.format_version != right.record.format_version ||
      left.result->format_version != right.result->format_version) {
    status = EvidenceQualificationStatus::PLAN_MISMATCH;
    error = "independent Compare rejects different deterministic fingerprint domains";
    return false;
  }
  const bool equal =
      left.record.deterministic_fingerprint == right.record.deterministic_fingerprint;
  output.comparison = ComparisonFacts{equal ? "EQUAL" : "DIFFERENT",
                                      equal ? "FINGERPRINT_EQUAL" : "FINGERPRINT_DIFFERENT"};
  status = EvidenceQualificationStatus::OK;
  return true;
}

}  // namespace pae::protocol_lab::v07
