#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#if defined(NDEBUG)
#error binary_public_h2_tests requires active assertions in every configuration
#endif

#include "../../tools/protocol_lab_ui/binary_host_adapter.h"
#include "../../tools/protocol_lab_ui/compile_worker.h"
#include "../../tools/protocol_lab_ui/document_session.h"

namespace ui = pae::protocol_lab_ui;

std::string Read(const wchar_t* path) {
  std::ifstream input(std::filesystem::path(path), std::ios::binary);
  assert(input.good());
  return {std::istreambuf_iterator<char>{input}, {}};
}

void FailBinaryFlowCopy() { throw std::runtime_error("injected Binary Flow copy failure"); }

struct ResultCopyFailureState {
  std::size_t calls = 0U;
  bool fail_next = true;
};

void FailResultCopy(void* context) {
  auto& state = *static_cast<ResultCopyFailureState*>(context);
  ++state.calls;
  if (state.fail_next) {
    state.fail_next = false;
    throw std::runtime_error("injected Binary result copy failure");
  }
}

std::unique_ptr<ui::CompileCompletion> Compile(ui::CompileWorker& worker, ui::DocumentId document,
                                               ui::Revision revision, std::string_view text) {
  assert(worker.Submit(document, revision, text) == ui::SubmitStatus::ACCEPTED);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline) {
    for (auto ticket : worker.DrainReadyTickets()) {
      auto completion = worker.TakeResult(ticket);
      if (completion && completion->document_id == document &&
          completion->load_revision == revision)
        return completion;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  std::cerr << "H2_WORKER_TIMEOUT document=" << document << std::endl;
  std::exit(2);
}

int main() {
  const auto binary_json = Read(PAE_BINARY_UI_CONFIG);
  const auto repository_root = std::filesystem::path(PAE_BINARY_UI_CONFIG)
                                   .parent_path()
                                   .parent_path()
                                   .parent_path()
                                   .parent_path();
  const auto stream_json =
      Read((repository_root / "examples/config/synthetic_stream_framing_slice.pae.json").c_str());
  const auto legacy_json = Read(PAE_LEGACY_UI_CONFIG);
  const auto ascii_json = Read(PAE_ASCII_TEXT_CONFIG);
  ui::CompileWorker worker;
  auto bad_dispatch =
      Compile(worker, 101U, 1U, R"({"schema_version":"0.9","schema_\u0076ersion":"0.5"})");
  assert(bad_dispatch->route == ui::SchemaDispatchStatus::CLASSIFICATION_FAILED);
  assert(!bad_dispatch->classification_error.empty() &&
         bad_dispatch->compiler_attempt_count == 0U && !bad_dispatch->artifacts &&
         !bad_dispatch->public_compiled);
  auto bad_binary = Compile(worker, 102U, 1U, R"({"schema_version":"0.9"})");
  assert(bad_binary->route == ui::SchemaDispatchStatus::BINARY_PUBLIC &&
         bad_binary->compiler_attempt_count == 1U && bad_binary->public_diagnostic &&
         !bad_binary->artifacts && !bad_binary->diagnostic);
  auto bad_legacy = Compile(worker, 103U, 1U, R"({"schema_version":"0.5"})");
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  assert(bad_legacy->route == ui::SchemaDispatchStatus::LEGACY_PUBLIC &&
         bad_legacy->compiler_attempt_count == 1U && bad_legacy->public_diagnostic &&
         !bad_legacy->artifacts && !bad_legacy->diagnostic);
#else
  assert(bad_legacy->route == ui::SchemaDispatchStatus::PRIVATE_LEGACY &&
         bad_legacy->compiler_attempt_count == 1U && bad_legacy->diagnostic &&
         !bad_legacy->public_compiled && !bad_legacy->public_diagnostic);
#endif
  auto legacy = Compile(worker, 104U, 1U, legacy_json);
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  assert(legacy->route == ui::SchemaDispatchStatus::LEGACY_PUBLIC &&
         legacy->compiler_attempt_count == 1U && legacy->public_compiled && !legacy->artifacts);
#else
  assert(legacy->route == ui::SchemaDispatchStatus::PRIVATE_LEGACY &&
         legacy->compiler_attempt_count == 1U && legacy->artifacts && !legacy->public_compiled);
#endif
  auto ascii = Compile(worker, 105U, 1U, ascii_json);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
  assert(ascii->route == ui::SchemaDispatchStatus::ASCII_PUBLIC &&
         ascii->compiler_attempt_count == 1U && ascii->public_compiled && !ascii->artifacts);
#elif defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
  assert(ascii->route == ui::SchemaDispatchStatus::PRIVATE_ASCII &&
         ascii->compiler_attempt_count == 1U && ascii->artifacts && !ascii->public_compiled);
#else
  assert(ascii->route == ui::SchemaDispatchStatus::CLASSIFICATION_FAILED &&
         ascii->compiler_attempt_count == 0U && !ascii->artifacts && !ascii->public_compiled &&
         !ascii->classification_error.empty());
#endif
  ui::DocumentSession session{106U};
  const auto load = session.BeginLoad();
  auto opening = Compile(worker, 106U, load, binary_json);
  const auto config_hash = opening->config_sha256;
  assert(opening->route == ui::SchemaDispatchStatus::BINARY_PUBLIC &&
         opening->compiler_attempt_count == 1U && opening->public_compiled && !opening->artifacts &&
         !opening->diagnostic);
  assert(session.ApplyCompileCompletion(std::move(opening)) && session.IsBinaryHostDocument() &&
         !session.BinaryHostActive() && session.state() == ui::DocumentState::READY &&
         session.description()->messages[0].fields[7].decode_decimal64 &&
         session.description()->messages[0].fields[7].decimal_conversion &&
         !session.description()->pipelines[0].encode_message_indices.empty());

  auto compiled = pae::CompileProtocolJson(binary_json);
  assert(compiled.Succeeded());
  std::string error;
  auto adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U},
       {"other", pae::HostAction::DECODE, "alternate_pipeline", 2U},
       {"tx", pae::HostAction::ENCODE, "ui_pipeline", 1U}},
      {106U, load, 1U, 9U, config_hash}, nullptr, 0U, 0U, error);
  assert(adapter && error.empty() && adapter->FlowCount(0U) == 2U && adapter->FlowCount(1U) == 2U &&
         adapter->FlowCount(2U) == 1U && adapter->IsCompleteEncode(2U, 0U));
  assert(adapter->Description().messages[0].fields[0].physical_bits.size() > 0U);
  assert(adapter->AccountedInstanceBytes() <= 128U * 1024U * 1024U);
  const auto description_upper = adapter->DescriptionCopyUpperBoundBytes();
  assert(description_upper > 0U);
  int description_copy_calls = 0;
  ui::BinaryUiCopyControls copy_reject;
  copy_reject.description_copy_limit = description_upper - 1U;
  copy_reject.before_description_copy = +[](void* context) { ++*static_cast<int*>(context); };
  copy_reject.context = &description_copy_calls;
  auto description_reject_compiled = pae::CompileProtocolJson(binary_json);
  assert(description_reject_compiled.Succeeded());
  std::string description_reject_error;
  auto description_reject =
      ui::BinaryHostAdapter::CreatePublic(std::move(description_reject_compiled).TakeCompiled(),
                                          {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U}},
                                          {107U, 1U, 1U, 1U, "description-reject"}, nullptr, 0U, 0U,
                                          description_reject_error, {}, &copy_reject);
  assert(!description_reject && description_copy_calls == 0 &&
         description_reject_error == "public Binary description copy preflight exceeded");

  auto stream_compiled = pae::CompileProtocolJson(stream_json);
  assert(stream_compiled.Succeeded());
  pae::protocol_lab_binary::public_decode::Limits stream_limits;
  stream_limits.max_stream_chunk_bytes = 32U;
  std::string stream_error;
  auto stream_adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(stream_compiled).TakeCompiled(),
      {{"fixed", pae::HostAction::DECODE, "fixed_rx", 2U},
       {"sync", pae::HostAction::DECODE, "sync_fixed_rx", 2U},
       {"length", pae::HostAction::DECODE, "sync_length_rx", 2U}},
      {108U, 1U, 1U, 1U, "stream-description"}, nullptr, 0U, 0U, stream_error, stream_limits);
  assert(stream_adapter && stream_error.empty() &&
         stream_adapter->Description().pipelines.size() == 3U &&
         stream_adapter->Description().pipelines[0].input_kind ==
             ui::StreamInputKind::STREAM_CHUNK &&
         stream_adapter->Description().pipelines[0].framing_strategy ==
             ui::StreamFramingStrategy::FIXED_LENGTH &&
         stream_adapter->Description().pipelines[0].maximum_candidate_frame_bytes == 3U &&
         stream_adapter->Description().pipelines[1].framing_strategy ==
             ui::StreamFramingStrategy::SYNC_FIXED_LENGTH &&
         stream_adapter->Description().pipelines[1].maximum_candidate_frame_bytes == 4U &&
         stream_adapter->Description().pipelines[2].framing_strategy ==
             ui::StreamFramingStrategy::SYNC_LENGTH_FIELD &&
         stream_adapter->Description().pipelines[2].maximum_candidate_frame_bytes == 8U &&
         stream_adapter->IsStreamDecode(0U, 0U) && !stream_adapter->IsCompleteDecode(0U, 0U));
  const auto initial_stream = stream_adapter->ObserveStream(0U, 0U);
  assert(initial_stream && initial_stream->strategy == ui::StreamFramingStrategy::FIXED_LENGTH &&
         initial_stream->maximum_candidate_frame_bytes == 3U &&
         initial_stream->effective_max_submit_bytes == 32U);
  const auto partial_stream = stream_adapter->SubmitStream(0U, 0U, {0xAAU});
  assert(partial_stream.status == ui::BinaryStreamPresentationStatus::NO_CANDIDATE &&
         partial_stream.host_called && !partial_stream.result && !partial_stream.failure &&
         partial_stream.after.buffered_bytes == 1U);
  const auto before_oversize = *stream_adapter->ObserveStream(0U, 0U);
  const auto oversized_stream =
      stream_adapter->SubmitStream(0U, 0U, std::vector<std::uint8_t>(33U));
  const auto after_oversize = *stream_adapter->ObserveStream(0U, 0U);
  assert(oversized_stream.status == ui::BinaryStreamPresentationStatus::PREFLIGHT_REJECTED &&
         !oversized_stream.host_called &&
         oversized_stream.diagnostic ==
             pae::protocol_lab_binary::public_decode::StreamDiagnostic::INVALID_CHUNK &&
         after_oversize.buffered_bytes == before_oversize.buffered_bytes &&
         after_oversize.generation == before_oversize.generation);
  const std::vector<std::uint8_t> glued_stream{0x01U, 0x02U, 0x00U, 0x00U,
                                               0x00U, 0xAAU, 0x03U, 0x04U};
  const auto stream_success = stream_adapter->SubmitStream(0U, 0U, glued_stream);
  assert(stream_success.status == ui::BinaryStreamPresentationStatus::DECODE_SUCCESS &&
         stream_success.result && stream_success.result->message_id == "fixed_message" &&
         stream_success.result->fields[0].logical_value == "258" &&
         stream_success.public_host.bytes_consumed == 2U &&
         stream_success.after.frozen_cursor == 2U);
  const auto blocked_stream = stream_adapter->SubmitStream(0U, 0U, {0xAAU, 9U, 9U});
  assert(blocked_stream.status == ui::BinaryStreamPresentationStatus::PREFLIGHT_REJECTED &&
         blocked_stream.diagnostic ==
             pae::protocol_lab_binary::public_decode::StreamDiagnostic::CONTINUE_REQUIRED &&
         blocked_stream.after.frozen_cursor == 2U);
  const auto stream_failure = stream_adapter->ContinueStream(0U, 0U);
  assert(stream_failure.status == ui::BinaryStreamPresentationStatus::DECODE_FAILURE &&
         stream_failure.failure && !stream_failure.result &&
         stream_failure.public_host.decode_failures == 1U &&
         !stream_adapter->MapCurrentStream(0U, 0U).result);
  const auto stream_recovered = stream_adapter->ContinueStream(0U, 0U);
  assert(stream_recovered.status == ui::BinaryStreamPresentationStatus::DECODE_SUCCESS &&
         stream_recovered.result && stream_recovered.result->fields[0].logical_value == "772" &&
         stream_recovered.after.frozen_input_bytes == 0U);
  const auto no_stale = stream_adapter->SubmitStream(0U, 0U, {0xAAU});
  assert(no_stale.status == ui::BinaryStreamPresentationStatus::NO_CANDIDATE &&
         !stream_adapter->MapCurrentStream(0U, 0U).result);
  assert(stream_adapter->SubmitStream(0U, 1U, {0xAAU}).status ==
         ui::BinaryStreamPresentationStatus::NO_CANDIDATE);
  const auto flow_one_before_reset = *stream_adapter->ObserveStream(0U, 1U);
  const auto flow_zero_generation = stream_adapter->ObserveStream(0U, 0U)->generation;
  assert(stream_adapter->ResetStream(0U, 0U) == pae::HostStatus::OK &&
         stream_adapter->ObserveStream(0U, 0U)->generation == flow_zero_generation + 1U &&
         stream_adapter->ObserveStream(0U, 1U)->buffered_bytes ==
             flow_one_before_reset.buffered_bytes);
  const auto isolated_flow = stream_adapter->SubmitStream(0U, 1U, {0x12U, 0x34U});
  assert(isolated_flow.status == ui::BinaryStreamPresentationStatus::DECODE_SUCCESS &&
         isolated_flow.result && isolated_flow.result->fields[0].logical_value == "4660");

  ui::BinaryUiCopyControls stream_copy_failure_controls;
  ResultCopyFailureState stream_copy_failure_state;
  stream_copy_failure_controls.before_result_copy = &FailResultCopy;
  stream_copy_failure_controls.context = &stream_copy_failure_state;
  auto stream_copy_failure_compiled = pae::CompileProtocolJson(stream_json);
  assert(stream_copy_failure_compiled.Succeeded());
  std::string stream_copy_failure_error;
  auto stream_copy_failure_adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(stream_copy_failure_compiled).TakeCompiled(),
      {{"fixed", pae::HostAction::DECODE, "fixed_rx", 2U}},
      {109U, 1U, 1U, 1U, "stream-copy-failure"}, nullptr, 0U, 0U, stream_copy_failure_error,
      stream_limits, &stream_copy_failure_controls);
  assert(stream_copy_failure_adapter && stream_copy_failure_error.empty());
  const auto stream_copy_failure =
      stream_copy_failure_adapter->SubmitStream(0U, 0U, {0xAAU, 1U, 2U});
  assert(stream_copy_failure.status ==
             ui::BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE &&
         stream_copy_failure.host_called && stream_copy_failure.public_host.bytes_consumed == 3U &&
         stream_copy_failure.after.reset_required && !stream_copy_failure.result &&
         stream_copy_failure_adapter->ObserveStream(0U, 0U)->reset_required &&
         !stream_copy_failure_adapter->ObserveStream(0U, 1U)->reset_required &&
         stream_copy_failure_state.calls == 1U);
  const auto copy_failure_observation = *stream_copy_failure_adapter->ObserveStream(0U, 0U);
  const auto mapped_copy_failure = stream_copy_failure_adapter->MapCurrentStream(0U, 0U);
  const auto copy_failure_after_map = *stream_copy_failure_adapter->ObserveStream(0U, 0U);
  assert(mapped_copy_failure.status ==
             ui::BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE &&
         mapped_copy_failure.local_status ==
             pae::protocol_lab_binary::public_decode::LocalStatus::MATERIALIZATION_FAILED &&
         mapped_copy_failure.diagnostic ==
             pae::protocol_lab_binary::public_decode::StreamDiagnostic::
                 COPY_FAILED_RESET_REQUIRED &&
         mapped_copy_failure.host_called &&
         mapped_copy_failure.public_host.status == pae::HostStatus::OK &&
         mapped_copy_failure.public_host.bytes_consumed == 3U && mapped_copy_failure.failure &&
         mapped_copy_failure.failure->host_status == pae::HostStatus::OK &&
         !mapped_copy_failure.result && mapped_copy_failure.after.reset_required &&
         stream_copy_failure_state.calls == 1U &&
         copy_failure_after_map.step_sequence == copy_failure_observation.step_sequence &&
         copy_failure_after_map.total_candidates == copy_failure_observation.total_candidates &&
         !stream_copy_failure_adapter->ObserveStream(0U, 1U)->reset_required);
  const auto copy_fault_blocked =
      stream_copy_failure_adapter->SubmitStream(0U, 0U, {0xAAU, 3U, 4U});
  assert(copy_fault_blocked.status == ui::BinaryStreamPresentationStatus::PREFLIGHT_REJECTED &&
         copy_fault_blocked.diagnostic ==
             pae::protocol_lab_binary::public_decode::StreamDiagnostic::RESET_REQUIRED &&
         !copy_fault_blocked.host_called);
  assert(stream_copy_failure_adapter->ResetStream(0U, 0U) == pae::HostStatus::OK &&
         !stream_copy_failure_adapter->ObserveStream(0U, 0U)->reset_required);
  const auto copy_failure_recovered =
      stream_copy_failure_adapter->SubmitStream(0U, 0U, {0xAAU, 3U, 4U});
  assert(copy_failure_recovered.status == ui::BinaryStreamPresentationStatus::DECODE_SUCCESS &&
         copy_failure_recovered.result && stream_copy_failure_state.calls == 2U);

  auto stream_budget_compiled = pae::CompileProtocolJson(stream_json);
  assert(stream_budget_compiled.Succeeded());
  std::string stream_budget_error;
  auto stream_budget_adapter =
      ui::BinaryHostAdapter::CreatePublic(std::move(stream_budget_compiled).TakeCompiled(),
                                          {{"fixed", pae::HostAction::DECODE, "fixed_rx", 1U}},
                                          {110U, 1U, 1U, 1U, "stream-budget-failure"}, nullptr, 0U,
                                          0U, stream_budget_error, stream_limits);
  assert(stream_budget_adapter && stream_budget_error.empty());
  const auto stream_view_budget = stream_budget_adapter->UiViewReserveBytes() / 2U;
  const auto stream_budget_failure =
      stream_budget_adapter->SubmitStream(0U, 0U, {0xAAU, 1U, 2U}, stream_view_budget - 3U);
  assert(stream_budget_failure.status ==
             ui::BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE &&
         stream_budget_failure.public_host.bytes_consumed == 3U &&
         stream_budget_failure.after.reset_required && !stream_budget_failure.result &&
         stream_budget_adapter->ObserveStream(0U, 0U)->reset_required);
  const auto budget_failure_observation = *stream_budget_adapter->ObserveStream(0U, 0U);
  const auto mapped_budget_failure = stream_budget_adapter->MapCurrentStream(0U, 0U);
  const auto budget_failure_after_map = *stream_budget_adapter->ObserveStream(0U, 0U);
  assert(mapped_budget_failure.status ==
             ui::BinaryStreamPresentationStatus::MATERIALIZATION_FAILURE &&
         mapped_budget_failure.local_status ==
             pae::protocol_lab_binary::public_decode::LocalStatus::MATERIALIZATION_FAILED &&
         mapped_budget_failure.public_host.status == pae::HostStatus::OK &&
         mapped_budget_failure.public_host.bytes_consumed == 3U &&
         mapped_budget_failure.failure &&
         mapped_budget_failure.failure->host_status == pae::HostStatus::OK &&
         !mapped_budget_failure.result && mapped_budget_failure.after.reset_required &&
         budget_failure_after_map.step_sequence == budget_failure_observation.step_sequence &&
         budget_failure_after_map.total_candidates == budget_failure_observation.total_candidates);
  assert(stream_budget_adapter->ResetStream(0U, 0U) == pae::HostStatus::OK);

  ui::DocumentSession stream_session{111U};
  const auto stream_load = stream_session.BeginLoad();
  auto stream_opening = Compile(worker, 111U, stream_load, stream_json);
  const auto stream_config_hash = stream_opening->config_sha256;
  assert(stream_opening->route == ui::SchemaDispatchStatus::BINARY_PUBLIC &&
         stream_session.ApplyCompileCompletion(std::move(stream_opening)) &&
         stream_session.IsBinaryHostDocument() && !stream_session.BinaryHostActive());
  auto stream_session_compiled = pae::CompileProtocolJson(stream_json);
  assert(stream_session_compiled.Succeeded());
  std::string stream_session_error;
  auto stream_session_adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(stream_session_compiled).TakeCompiled(),
      {{"fixed", pae::HostAction::DECODE, "fixed_rx", 2U},
       {"sync", pae::HostAction::DECODE, "sync_fixed_rx", 2U},
       {"length", pae::HostAction::DECODE, "sync_length_rx", 2U}},
      {111U, stream_load, 1U, 77U, stream_config_hash}, nullptr, 0U, 0U,
      stream_session_error, stream_limits);
  assert(stream_session_adapter && stream_session_error.empty());
  auto stream_publication =
      stream_session.PrepareBinaryHostPublication(std::move(stream_session_adapter), 77U);
  assert(stream_publication);
  stream_session.PublishBinaryHostPublication(std::move(*stream_publication));
  assert(stream_session.BinaryHostActive() &&
         stream_session.mode() == ui::OperationMode::STREAM_INSPECT &&
         stream_session.StreamInspectAvailable() && stream_session.StreamChunkBudget() == 32U);
  assert(stream_session.SetInspectDraft("AA") && stream_session.SubmitStream() &&
         !stream_session.inspect_result() && !stream_session.inspect_failure() &&
         stream_session.BinaryStreamObservation()->buffered_bytes == 1U &&
         stream_session.StreamHasDiscardableState());
  assert(stream_session.SetInspectDraft("01 02") && stream_session.SubmitStream() &&
         stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "258");
  assert(stream_session.SetInspectDraft("AA 03 04 AA 05 06") &&
         stream_session.SubmitStream() && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "772" &&
         stream_session.StreamContinueAvailable());
  assert(stream_session.SetInspectDraft("FF") && !stream_session.inspect_result() &&
         stream_session.ContinueStream() && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "1286" &&
         stream_session.inspect_draft() == "FF" && !stream_session.StreamContinueAvailable());
  auto select_stream = [&](std::size_t binding, std::size_t flow) {
    auto publication = stream_session.PrepareBinaryHostFlow(binding, flow);
    return publication && stream_session.PublishBinaryHostFlow(std::move(*publication));
  };
  assert(select_stream(0U, 1U) && stream_session.mode() == ui::OperationMode::STREAM_INSPECT &&
         stream_session.SetInspectDraft("AA 07 08") && stream_session.SubmitStream() &&
         stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "1800");
  const auto flow_one_generation = stream_session.BinaryStreamObservation()->generation;
  assert(select_stream(0U, 0U) && stream_session.inspect_draft() == "FF" &&
         stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "1286" &&
         stream_session.ResetStream() && !stream_session.inspect_result() &&
         !stream_session.inspect_failure());
  assert(select_stream(0U, 1U) && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "1800" &&
         stream_session.BinaryStreamObservation()->generation == flow_one_generation);
  assert(select_stream(1U, 0U) && stream_session.SetInspectDraft(
                                         "00 A5 5A 01 02 A5 5A 03 04") &&
         stream_session.SubmitStream() && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "258" &&
         stream_session.BinaryStreamObservation()->total_discarded_bytes == 1U &&
         stream_session.ContinueStream() && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[0].logical_value == "772");
  assert(select_stream(2U, 0U) && stream_session.SetInspectDraft(
                                         "00 C3 3C 06 01 02 55 C3 3C 06 03 04 55") &&
         stream_session.SubmitStream() && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[1].logical_value == "258" &&
         stream_session.ContinueStream() && stream_session.inspect_result() &&
         stream_session.inspect_result()->fields[1].logical_value == "772");
  assert(stream_session.SetInspectDraft("C3") && stream_session.SubmitStream() &&
         !stream_session.inspect_result() && !stream_session.inspect_failure());
  assert(stream_session.SetInspectDraft("C3 3C 06 00 00 00") && stream_session.SubmitStream() &&
         !stream_session.inspect_result() && stream_session.inspect_failure());
  stream_session.Close();

  const std::vector<std::uint8_t> frame{0x80U, 0x0DU, 0x03U, 0x00U, 0x01U,
                                        0x00U, 0xCAU, 0xFEU, 0x05U, 0x5AU};
  auto first = adapter->DecodeComplete(0U, 0U, frame);
  assert(first.ok && first.public_host.codec_attempted && first.result &&
         first.result->fields.size() == 9U && first.result->fields[0].raw_value == "未单独提供" &&
         first.result->fields[1].logical_value == "Active [active]" &&
         first.result->fields[6].raw_value == "CAFE" && first.result->fields[7].raw_value == "5" &&
         first.result->fields[7].logical_value == "5@0");
  using PublicInput = pae::protocol_lab_binary::public_decode::EncodeInput;
  std::vector<PublicInput> public_inputs;
  public_inputs.push_back(PublicInput::Bool(0U, true));
  public_inputs.push_back(PublicInput::Enum(1U, 1U));
  public_inputs.push_back(PublicInput::UInt64(2U, 0xD0U));
  public_inputs.push_back(PublicInput::UInt64(3U, 3U));
  public_inputs.push_back(PublicInput::UInt64(4U, 1U));
  public_inputs.push_back(PublicInput::Int64(5U, 0));
  public_inputs.push_back(PublicInput::Bytes(6U, {0xCAU, 0xFEU}));
  public_inputs.push_back(PublicInput::Decimal(7U, {5, 0}));
  auto encoded = adapter->EncodeComplete(2U, 0U, 0U, public_inputs);
  assert(encoded.ok && encoded.result &&
         encoded.result->frame == std::vector<std::uint8_t>(
                                      {0x8DU, 0x0DU, 0xC3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU}) &&
         encoded.result->fields[7].raw_value == "未观察" &&
         encoded.result->fields[7].logical_value == "5@0" &&
         encoded.result->fields[8].logical_value == "由 PAE 生成");
  auto invalid_inputs = public_inputs;
  invalid_inputs.back() = PublicInput::Decimal(7U, {1000, 0});
  auto encode_failure = adapter->EncodeComplete(2U, 0U, 0U, invalid_inputs);
  assert(!encode_failure.ok && encode_failure.failure &&
         encode_failure.public_host.codec_status == pae::CodecStatus::VALUE_NOT_REPRESENTABLE &&
         !adapter->MapCurrentEncode(2U, 0U).result);
  assert(adapter->Current(0U, 0U) && !adapter->Current(0U, 1U));
  assert(adapter->MapCurrent(0U, 0U).result && !adapter->MapCurrent(0U, 1U).result);
  std::unordered_map<std::size_t, ui::TypedDraft> saved_typed_drafts;
  saved_typed_drafts.emplace(0U, std::uint64_t{7U});
  assert(!adapter->SaveDraftsAndSelect(u"\u4e0d是 ASCII", saved_typed_drafts, 0U, 0U, 1U));
  assert(adapter->Draft(0U, 0U).empty() && adapter->TypedDrafts(0U, 0U).empty());
  assert(adapter->SaveDraftsAndSelect(u"80 0D", saved_typed_drafts, 0U, 0U, 1U));
  assert(adapter->Draft(0U, 0U) == u"80 0D" &&
         std::get<std::uint64_t>(adapter->TypedDrafts(0U, 0U).at(0U)) == 7U);
  assert(adapter->Draft(0U, 1U).empty());
  const std::vector<std::uint8_t> alternate{0xC3U, 0x2AU};
  auto second_binding = adapter->DecodeComplete(1U, 0U, alternate);
  assert(second_binding.ok && second_binding.result &&
         second_binding.result->message_id == "alternate_record" &&
         second_binding.result->fields[0].logical_value == "42");
  assert(adapter->MapCurrent(0U, 0U).result && adapter->MapCurrent(1U, 0U).result);
  const auto active_upper = adapter->CurrentResultCopyUpperBoundBytes(0U, 0U);
  const auto mapped_budget = adapter->UiViewReserveBytes() / 2U;
  assert(active_upper > 0U && active_upper <= mapped_budget);
  assert(adapter->SetPresentationRetainedBytes(mapped_budget - active_upper + 1U));
  bool rejected_mapping = false;
  try {
    (void)adapter->MapCurrent(0U, 0U);
  } catch (const std::exception&) {
    rejected_mapping = true;
  }
  assert(rejected_mapping && adapter->Current(0U, 0U));
  assert(adapter->SetPresentationRetainedBytes(0U));
  assert(adapter->MapCurrent(0U, 0U).result);
  auto invalid_frame = frame;
  invalid_frame.pop_back();
  auto failed = adapter->DecodeComplete(0U, 0U, invalid_frame);
  assert(!failed.ok && failed.failure && !adapter->MapCurrent(0U, 0U).result &&
         adapter->MapCurrent(1U, 0U).result);
  const auto old_instance = adapter->Instance();
  auto replacement_compiled = pae::CompileProtocolJson(binary_json);
  assert(replacement_compiled.Succeeded());
  std::string replacement_error;
  auto replacement = ui::BinaryHostAdapter::CreatePublic(
      std::move(replacement_compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U},
       {"other", pae::HostAction::DECODE, "alternate_pipeline", 2U},
       {"tx", pae::HostAction::ENCODE, "ui_pipeline", 1U}},
      {106U, load, 2U, 10U, config_hash}, adapter.get(), 0U, 0U, replacement_error);
  assert(replacement && replacement_error.empty() && replacement->Instance() != old_instance &&
         adapter->MapCurrent(1U, 0U).result && !replacement->MapCurrent(1U, 0U).result);
  auto session_compiled = pae::CompileProtocolJson(binary_json);
  assert(session_compiled.Succeeded());
  std::string session_error;
  auto session_adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(session_compiled).TakeCompiled(),
      {{"device", pae::HostAction::DECODE, "ui_pipeline", 2U},
       {"other", pae::HostAction::DECODE, "alternate_pipeline", 2U},
       {"tx", pae::HostAction::ENCODE, "ui_pipeline", 1U}},
      {106U, load, 1U, 9U, config_hash}, nullptr, 0U, 0U, session_error);
  assert(session_adapter && session_error.empty());
  auto publication = session.PrepareBinaryHostPublication(std::move(session_adapter), 9U);
  assert(publication && publication->description.messages[0].fields[7].decode_decimal64);
  session.PublishBinaryHostPublication(std::move(*publication));
  assert(session.BinaryHostActive() && session.BinarySessionRevision() == 1U);
  assert(session.SetInspectDraftUtf16(u"80 0D 03 00 01 00 CA FE 05 5A"));
  auto fresh_flow = session.PrepareBinaryHostFlow(0U, 1U);
  if (!fresh_flow || !fresh_flow->inspect_draft_utf16.empty()) {
    std::cerr << "H2_FLOW_PRECONDITION_FAIL new Flow is not empty\n";
    return 2;
  }
  assert(session.PublishBinaryHostFlow(std::move(*fresh_flow)));
  if (session.BinaryHostFlowIndex() != 1U || !session.inspect_draft_utf16().empty() ||
      session.prepared()->binary_host_adapter->Draft(0U, 0U) != u"80 0D 03 00 01 00 CA FE 05 5A" ||
      !session.prepared()->binary_host_adapter->Draft(0U, 1U).empty()) {
    std::cerr << "H2_FLOW_DRAFT_ISOLATION_FAIL first empty Flow inherited source draft\n";
    return 1;
  }
  const auto move_to = [&](std::size_t binding, std::size_t flow) {
    auto prepared = session.PrepareBinaryHostFlow(binding, flow);
    return prepared && session.PublishBinaryHostFlow(std::move(*prepared));
  };
  const auto matches = [&](std::size_t binding, std::size_t flow, std::u16string_view draft,
                           int count, std::uint8_t frame_byte) {
    if (session.BinaryHostBindingIndex() != binding || session.BinaryHostFlowIndex() != flow ||
        session.inspect_draft_utf16() != draft)
      return false;
    if (count < 0) return !session.inspect_result();
    return session.inspect_result() && session.inspect_result()->fields.size() == 9U &&
           session.inspect_result()->fields[4].logical_value == std::to_string(count) &&
           session.inspect_result()->input_frame.size() == 10U &&
           session.inspect_result()->input_frame[4] == frame_byte;
  };
  constexpr auto flow0_frame = u"80 0D 03 00 01 00 CA FE 05 5A";
  constexpr auto flow1_frame = u"80 0D 03 00 02 00 CA FE 05 5A";
  if (!session.SetInspectDraftUtf16(flow1_frame) || !session.Inspect() ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(0U, 0U) ||
      !matches(0U, 0U, flow0_frame, -1, 0U) || !session.Inspect() ||
      !matches(0U, 0U, flow0_frame, 1, 1U) || !move_to(0U, 1U) ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(0U, 0U) ||
      !matches(0U, 0U, flow0_frame, 1, 1U)) {
    std::cerr << "H2_FLOW_ROUNDTRIP_FAIL draft, count, or decoded frame mismatched\n";
    return 3;
  }
  assert(session.BinaryHasDiscardableState() && !session.StreamHasDiscardableState());
  constexpr auto uninspected = u"80 0D 03 00 03 00 CA FE 05 5A";
  if (!session.SetInspectDraftUtf16(uninspected) || session.inspect_result() || !move_to(0U, 1U) ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(0U, 0U) ||
      !matches(0U, 0U, uninspected, 1, 1U) || !move_to(1U, 0U) || !matches(1U, 0U, u"", -1, 0U) ||
      session.prepared()->binary_host_adapter->Draft(0U, 0U) != uninspected) {
    std::cerr << "H2_FLOW_UNINSPECTED_OR_BINDING_FAIL binding=" << session.BinaryHostBindingIndex()
              << " flow=" << session.BinaryHostFlowIndex() << " draft=" << session.inspect_draft()
              << " result=" << session.inspect_result().has_value() << " saved0="
              << (session.prepared()->binary_host_adapter->Draft(0U, 0U) == uninspected) << "\n";
    return 4;
  }
  if (!session.SetInspectDraftUtf16(u"C3 2A") || !session.Inspect() || !session.inspect_result() ||
      session.inspect_result()->message_id != "alternate_record" ||
      session.inspect_result()->input_frame != alternate || !move_to(0U, 1U) ||
      !matches(0U, 1U, flow1_frame, 2, 2U) || !move_to(1U, 0U) ||
      session.inspect_draft_utf16() != u"C3 2A" || !session.inspect_result() ||
      session.inspect_result()->input_frame != alternate) {
    std::cerr << "H2_FLOW_CROSS_BINDING_FAIL alternate draft or result mismatched\n";
    return 5;
  }
  const auto* active = session.prepared()->binary_host_adapter.get();
  const std::u16string saved_other(active->Draft(1U, 0U));
  const std::u16string saved_flow1(active->Draft(0U, 1U));
  if (!session.SetInspectDraftUtf16(std::u16string(active->DraftLimit(1U, 0U) + 1U, u'A'))) {
    std::cerr << "H2_FLOW_FAILURE_PRECONDITION_FAIL oversize editor draft rejected\n";
    return 6;
  }
  auto rejected = session.PrepareBinaryHostFlow(0U, 1U);
  if (!rejected || session.PublishBinaryHostFlow(std::move(*rejected)) ||
      session.BinaryHostBindingIndex() != 1U || session.BinaryHostFlowIndex() != 0U ||
      active->Draft(1U, 0U) != saved_other || active->Draft(0U, 1U) != saved_flow1) {
    std::cerr << "H2_FLOW_SAVE_REJECTION_FAIL flow state partially published\n";
    return 7;
  }
  if (!session.SetInspectDraftUtf16(u"C3 2A")) return 8;
  const auto failed_copy = session.PrepareBinaryHostFlow(0U, 1U, &FailBinaryFlowCopy);
  if (failed_copy || session.BinaryHostBindingIndex() != 1U ||
      session.BinaryHostFlowIndex() != 0U || session.inspect_draft_utf16() != u"C3 2A" ||
      active->Draft(1U, 0U) != saved_other || active->Draft(0U, 1U) != saved_flow1) {
    std::cerr << "H2_FLOW_COPY_REJECTION_FAIL flow state partially published\n";
    return 9;
  }
  if (!move_to(2U, 0U) || session.mode() != ui::OperationMode::ENCODE ||
      !session.EncodeAvailable() || !session.SetDraft(0U, true) ||
      !session.SetDraft(1U, ui::EnumSelection{1U, "active"}) ||
      !session.SetDraft(2U, std::uint64_t{0xD0U}) || !session.SetDraft(3U, std::uint64_t{3U}) ||
      !session.SetDraft(4U, std::uint64_t{1U}) || !session.SetDraft(5U, std::int64_t{0}) ||
      !session.SetDraft(6U, std::vector<std::uint8_t>{0xCAU, 0xFEU}) ||
      !session.SetDraft(7U, ui::Decimal64{5, 0}) || !session.Encode() || !session.preview() ||
      session.preview()->encoded_frame !=
          std::vector<std::uint8_t>({0x8DU, 0x0DU, 0xC3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU}) ||
      session.preview()->fields[7].raw_value != "未观察" ||
      session.preview()->fields[8].logical_value != "由 PAE 生成") {
    std::cerr << "H2_ENCODE_SUCCESS_FAIL typed inputs or bounded presentation mismatched\n";
    return 10;
  }
  if (!session.SetDraft(7U, ui::Decimal64{1000, 0}) || session.Encode() || session.preview() ||
      session.diagnostic_id() != "UI_BINARY_HOST_ENCODE_VALUE_NOT_REPRESENTABLE") {
    std::cerr << "H2_ENCODE_FAILURE_CLEAR_FAIL old successful output survived failure\n";
    return 11;
  }
  if (!session.SetDraft(7U, ui::Decimal64{5, 0}) || !session.Encode() || !session.preview() ||
      !move_to(0U, 1U) || session.mode() != ui::OperationMode::INSPECT || !move_to(2U, 0U) ||
      session.mode() != ui::OperationMode::ENCODE || !session.preview() ||
      session.drafts().size() != 8U ||
      !(std::get<ui::Decimal64>(session.drafts().at(7U)) == ui::Decimal64{5, 0})) {
    std::cerr << "H2_ENCODE_FLOW_ROUNDTRIP_FAIL drafts or preview were not isolated\n";
    return 12;
  }
  if (!session.SetInvalidDraft(7U, "invalid", "not Decimal64") || session.Encode() ||
      session.preview() || !move_to(0U, 1U) || !move_to(2U, 0U) || session.preview() ||
      session.invalid_drafts().count(7U) != 1U || session.diagnostic_id() != "UI_INPUT_INVALID") {
    std::cerr << "H2_ENCODE_LOCAL_FAILURE_ROUNDTRIP_FAIL old success was restored\n";
    return 13;
  }
  if (!session.SetDraft(2U, std::uint64_t{0xD1U})) {
    std::cerr << "H2_ENCODE_LOCAL_CACHE_COPY_PRECONDITION_FAIL source draft did not change\n";
    return 15;
  }
  const auto& source_drafts_before_copy_failure =
      session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U);
  const auto source_draft_count_before_copy_failure = source_drafts_before_copy_failure.size();
  const auto source_field7_before_copy_failure = source_drafts_before_copy_failure.count(7U);
  const auto source_field2_before_copy_failure =
      std::get<std::uint64_t>(source_drafts_before_copy_failure.at(2U));
  const auto diagnostic_before_copy_failure = session.diagnostic_id();
  auto copy_failure_target = session.PrepareBinaryHostFlow(0U, 1U);
  if (!copy_failure_target ||
      session.PublishBinaryHostFlow(std::move(*copy_failure_target), &FailBinaryFlowCopy) ||
      session.BinaryHostBindingIndex() != 2U || session.BinaryHostFlowIndex() != 0U ||
      session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U).size() !=
          source_draft_count_before_copy_failure ||
      session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U).count(7U) !=
          source_field7_before_copy_failure ||
      std::get<std::uint64_t>(session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U).at(
          2U)) != source_field2_before_copy_failure ||
      session.invalid_drafts().count(7U) != 1U ||
      session.diagnostic_id() != diagnostic_before_copy_failure) {
    std::cerr << "H2_ENCODE_LOCAL_CACHE_COPY_ATOMICITY_FAIL partial Flow state was published\n";
    return 18;
  }
  const auto local_cache_budget =
      session.prepared()->binary_host_adapter->UiViewReserveBytes() / 4U;
  if (!session.SetInvalidDraftUtf16(
          7U, std::u16string(local_cache_budget / sizeof(char16_t) + 1U, u'X'),
          "oversized local diagnostic")) {
    std::cerr
        << "H2_ENCODE_LOCAL_CACHE_BUDGET_PRECONDITION_FAIL invalid draft was rejected early\n";
    return 16;
  }
  const auto& source_drafts_before_budget_failure =
      session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U);
  const auto source_draft_count_before_budget_failure = source_drafts_before_budget_failure.size();
  const auto source_field7_before_budget_failure = source_drafts_before_budget_failure.count(7U);
  auto budget_failure_target = session.PrepareBinaryHostFlow(0U, 1U);
  if (!budget_failure_target || session.PublishBinaryHostFlow(std::move(*budget_failure_target)) ||
      session.BinaryHostBindingIndex() != 2U || session.BinaryHostFlowIndex() != 0U ||
      session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U).size() !=
          source_draft_count_before_budget_failure ||
      session.prepared()->binary_host_adapter->TypedDrafts(2U, 0U).count(7U) !=
          source_field7_before_budget_failure ||
      session.invalid_drafts().count(7U) != 1U || session.diagnostic_id() != "UI_INPUT_INVALID") {
    std::cerr << "H2_ENCODE_LOCAL_CACHE_BUDGET_FAIL oversized cache was published\n";
    return 17;
  }

  std::string multi_json = binary_json;
  const std::string one_message = "\"message_ids\": [\"typed_record\"]";
  const auto association = multi_json.find(one_message);
  assert(association != std::string::npos);
  multi_json.replace(association, one_message.size(),
                     "\"message_ids\": [\"typed_record\", \"alternate_record\"]");
  ui::DocumentSession multi_session{107U};
  const auto multi_load = multi_session.BeginLoad();
  auto multi_opening = Compile(worker, 107U, multi_load, multi_json);
  const auto multi_config_hash = multi_opening->config_sha256;
  assert(multi_opening->route == ui::SchemaDispatchStatus::BINARY_PUBLIC &&
         multi_session.ApplyCompileCompletion(std::move(multi_opening)));
  auto multi_compiled = pae::CompileProtocolJson(multi_json);
  assert(multi_compiled.Succeeded());
  std::string multi_error;
  auto multi_adapter = ui::BinaryHostAdapter::CreatePublic(
      std::move(multi_compiled).TakeCompiled(),
      {{"rx", pae::HostAction::DECODE, "ui_pipeline", 1U},
       {"tx", pae::HostAction::ENCODE, "ui_pipeline", 1U}},
      {107U, multi_load, 1U, 1U, multi_config_hash}, nullptr, 0U, 0U, multi_error);
  assert(multi_adapter && multi_error.empty());
  auto multi_publication = multi_session.PrepareBinaryHostPublication(std::move(multi_adapter), 1U);
  assert(multi_publication);
  multi_session.PublishBinaryHostPublication(std::move(*multi_publication));
  const auto multi_move_to = [&](std::size_t binding) {
    auto prepared = multi_session.PrepareBinaryHostFlow(binding, 0U);
    return prepared && multi_session.PublishBinaryHostFlow(std::move(*prepared));
  };
  if (!multi_move_to(1U) || !multi_session.SelectMessage(1U) ||
      !multi_session.SetDraft(0U, std::uint64_t{42U}) || !multi_session.Encode() ||
      !multi_session.preview() || multi_session.preview()->encoded_frame != alternate ||
      !multi_move_to(0U) || !multi_move_to(1U) || !multi_session.selection() ||
      multi_session.selection()->message_index != 1U ||
      multi_session.selection()->message_id != "alternate_record" ||
      multi_session.drafts().size() != 1U ||
      std::get<std::uint64_t>(multi_session.drafts().at(0U)) != 42U || !multi_session.preview() ||
      multi_session.preview()->encoded_frame != alternate ||
      multi_session.preview()->key.selection.message_index != 1U ||
      multi_session.preview()->key.selection.message_id != "alternate_record") {
    std::cerr
        << "H2_ENCODE_MULTI_MESSAGE_ROUNDTRIP_FAIL selection, draft, result, or key crossed\n";
    return 14;
  }
  multi_session.Close();
  session.Close();
  assert(session.state() == ui::DocumentState::CLOSED);
  return 0;
}
