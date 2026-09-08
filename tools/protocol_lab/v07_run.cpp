#include "v07_run.h"

#include <chrono>
#include <limits>
#include <utility>

#include "sha256.h"

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
  bundle.record.tool_version = request.tool_version;
  bundle.record.operation_kind = request.operation_kind;
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
  bundle.record.result_file = outcome.result.has_value()
                                  ? std::optional<std::string>{"result_summary_v0.6.json"}
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
    execution = bridge->Inspect(*request.frame
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

}  // namespace pae::protocol_lab::v07
