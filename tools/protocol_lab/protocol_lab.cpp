#include "protocol_lab.h"

#include <filesystem>
#include <iostream>
#include <memory>
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
#include "udp_exchange.h"

namespace pae::protocol_lab {
namespace {

int ApplyExpectedStatus(const Arguments& arguments, const OperationResult& result,
                        int natural_exit) {
  if (arguments.expect_status.empty()) {
    return natural_exit;
  }
  return result.status == arguments.expect_status ? 0 : 5;
}

void BindCompiledPlan(const protocol_plan::PlanBundle& plan, std::string_view operation_kind,
                      OperationResult& result) {
  result.schema_version.assign(plan.SchemaVersion().data(), plan.SchemaVersion().size());
  result.operation_kind.assign(operation_kind.data(), operation_kind.size());
  result.protocol_id.assign(plan.ProtocolId().data(), plan.ProtocolId().size());
}

void FinalizePostPlanFailure(OperationResult& result, std::string_view replay_mode,
                             std::string_view replay_subject) {
  if (result.schema_version == "0.1") {
    return;
  }
  result.replay_mode.assign(replay_mode.data(), replay_mode.size());
  result.replay_subject.assign(replay_subject.data(), replay_subject.size());
  if (result.replay_mode == "NO_CODEC_REEXECUTION") {
    result.current_execution_status = "NOT_EVALUATED";
    result.current_execution_diagnostic_id.clear();
  } else {
    result.current_execution_status = result.status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
  }
  if (result.command == "replay") {
    result.comparison_equal.reset();
    result.comparison_status = "NOT_EVALUATED";
    result.comparison_reason = "Replay preparation failed before Codec execution";
  }
  FinalizeFingerprint(result);
}

void PreserveHistoricalTransport(const StoredRun& stored, OperationResult& result) {
  if (stored.operation_kind != "udp-exchange") {
    return;
  }
  result.historical_transport = stored.historical_transport;
  if (result.historical_transport.present) {
    return;
  }
  result.historical_transport.present = true;
  result.historical_transport.transport = stored.transport;
  result.historical_transport.timeout_ms = stored.timeout_ms;
  result.historical_transport.status = stored.operation_status;
  result.historical_transport.diagnostic_id = stored.diagnostic_id;
  result.historical_transport.local_endpoint = stored.local_endpoint;
  result.historical_transport.remote_endpoint = stored.remote_endpoint;
  result.historical_transport.received_from = stored.received_from;
  result.historical_transport.send_attempted = stored.send_attempted;
  result.historical_transport.send_succeeded = stored.send_succeeded;
  result.historical_transport.response_received = stored.response_received;
  result.historical_transport.response_decoded = stored.response_decoded;
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
  if (result.transport != "OFFLINE") {
    std::cout << "TRANSPORT=" << result.transport << " LOCAL_ENDPOINT=" << result.local_endpoint
              << " REMOTE_ENDPOINT=" << result.remote_endpoint
              << " RECEIVED_FROM=" << result.received_from << '\n'
              << "SEND_ATTEMPTED=" << (result.send_attempted ? "true" : "false")
              << " SEND_SUCCEEDED=" << (result.send_succeeded ? "true" : "false")
              << " RESPONSE_RECEIVED=" << (result.response_received ? "true" : "false")
              << " RESPONSE_DECODED=" << (result.response_decoded ? "true" : "false") << '\n';
    if (!result.tx_frame.empty()) {
      std::cout << "TX_FRAME_HEX=" << HexUpper(result.tx_frame) << '\n';
    }
    if (!result.rx_frame.empty()) {
      std::cout << "RX_FRAME_HEX=" << HexUpper(result.rx_frame) << '\n';
    }
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

bool InspectWithinPlanLimit(const protocol_plan::PlanBundle& plan,
                            const std::vector<std::uint8_t>& frame, OperationResult& result,
                            IProtocolLabExecutionObserver* execution_observer) {
  result.frame = frame;
  if (frame.size() > plan.GetResourceRequirements().max_frame_bytes) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_FRAME_LIMIT_EXCEEDED";
    result.diagnostic_detail = "frame exceeds compiled Plan limit";
    FinalizePostPlanFailure(result, "DECODE_RX", "RX");
    return false;
  }
  if (execution_observer != nullptr) {
    execution_observer->OnProtocolOperation("DECODE_RX");
  }
  result = InspectFrame(plan, frame);
  return true;
}

int RunInspectOrEncode(const Arguments& arguments, OperationResult& result,
                       std::string& config_text, std::optional<std::string>& values_text,
                       IProtocolLabExecutionObserver* execution_observer) {
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
  const bool inspect = arguments.command == Command::INSPECT;
  BindCompiledPlan(*plan, inspect ? "inspect" : "encode", result);
  if (arguments.command == Command::INSPECT) {
    std::vector<std::uint8_t> frame;
    if (!LoadFrameArgument(arguments.frame_binary, arguments.frame_hex, frame, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_FRAME_INPUT_ERROR";
      result.diagnostic_detail = error.empty() ? "binary frame must be non-empty" : error;
      FinalizePostPlanFailure(result, "DECODE_RX", "RX");
      return 3;
    }
    if (!InspectWithinPlanLimit(*plan, frame, result, execution_observer)) {
      return 3;
    }
    result.command = "inspect";
    result.config_sha256 = HashBytes(config_text);
  } else {
    std::string text;
    ParsedValues parsed;
    if (!ReadText(arguments.values, text, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_VALUES_INPUT_ERROR";
      result.diagnostic_detail = error;
      FinalizePostPlanFailure(result, "ENCODE_TX", "TX");
      return 3;
    }
    values_text = text;
    if (!ParseValues(text, parsed, error)) {
      result.status = "VALUES_INVALID";
      result.diagnostic_id = "PAE_LAB_VALUES_INVALID";
      result.diagnostic_detail = error;
      FinalizePostPlanFailure(result, "ENCODE_TX", "TX");
      return 5;
    }
    if (execution_observer != nullptr) {
      execution_observer->OnProtocolOperation("ENCODE_TX");
    }
    result = EncodeValues(*plan, parsed, error);
    if (plan->SchemaVersion() != "0.1") {
      result.replay_mode = "ENCODE_TX";
      result.replay_subject = "TX";
      result.current_execution_status = result.status;
      result.current_execution_diagnostic_id = result.diagnostic_id;
    }
    result.command = "encode";
    result.config_sha256 = HashBytes(config_text);
  }
  FinalizeFingerprint(result);
  return result.status == "OK" ? 0 : 5;
}

int FailRecord(OperationResult& result, std::string_view detail) {
  result.status = "RECORD_FAILED";
  result.diagnostic_id = "PAE_LAB_RECORD_FAILED";
  result.diagnostic_detail.assign(detail.data(), detail.size());
  result.evidence_bundle.clear();
  result.exit_code = kRecordFailedExitCode;
  FinalizeFingerprint(result);
  return kRecordFailedExitCode;
}

bool HasPipeline(const protocol_plan::PlanBundle& plan, std::string_view pipeline_id) {
  for (const auto& pipeline : plan.Pipelines()) {
    if (pipeline.id == pipeline_id) {
      return true;
    }
  }
  return false;
}

class TransactionUdpEventObserver final : public IUdpExchangeEventObserver {
 public:
  TransactionUdpEventObserver(EvidenceBundleTransaction& transaction,
                              std::string_view remote_endpoint)
      : transaction_(transaction), remote_endpoint_(remote_endpoint) {}

  void OnSendIntent() override { transaction_.CaptureSendIntent(remote_endpoint_); }

  void OnSendResult(bool succeeded, std::string_view diagnostic_id) override {
    transaction_.CaptureSendResult(succeeded, diagnostic_id);
  }

 private:
  EvidenceBundleTransaction& transaction_;
  std::string remote_endpoint_;
};

int CompleteUdpTransaction(const Arguments& arguments, EvidenceBundleTransaction& transaction,
                           OperationResult& result, int natural_exit) {
  const int exit_code = ApplyExpectedStatus(arguments, result, natural_exit);
  result.exit_code = exit_code;
  FinalizeFingerprint(result);
  std::string error;
  if (!transaction.Complete(result, error)) {
    return FailRecord(result, error);
  }
  return exit_code;
}

int RunUdpExchange(const Arguments& arguments, OperationResult& result, std::string& config_text,
                   std::optional<std::string>& values_text, RecordFileSystem& file_system,
                   IUdpExchangeAdapter& udp_adapter,
                   IProtocolLabExecutionObserver* execution_observer) {
  UdpEndpoint local;
  UdpEndpoint remote;
  std::string error;
  if (!ParseUdpEndpoint(arguments.local_endpoint, true, local, error) ||
      !ParseUdpEndpoint(arguments.remote_endpoint, false, remote, error)) {
    result.status = "CLI_ERROR";
    result.diagnostic_id = "PAE_LAB_UDP_ENDPOINT_INVALID";
    result.diagnostic_detail = error;
    FinalizeFingerprint(result);
    return 2;
  }
  if (remote.IsUnspecified() || remote.IsMulticast() || remote.IsBroadcast()) {
    result.status = "CLI_ERROR";
    result.diagnostic_id = "PAE_LAB_UDP_REMOTE_FORBIDDEN";
    result.diagnostic_detail = "remote endpoint must be one explicit unicast IPv4 address";
    FinalizeFingerprint(result);
    return 2;
  }
  if (local.IsMulticast() || local.IsBroadcast()) {
    result.status = "CLI_ERROR";
    result.diagnostic_id = "PAE_LAB_UDP_LOCAL_FORBIDDEN";
    result.diagnostic_detail = "local endpoint cannot be multicast or broadcast";
    FinalizeFingerprint(result);
    return 2;
  }
  if ((!local.IsLoopback() || !remote.IsLoopback()) &&
      (!arguments.send || !arguments.allow_non_loopback)) {
    result.status = "CLI_ERROR";
    result.diagnostic_id = "PAE_LAB_NON_LOOPBACK_CONFIRMATION_REQUIRED";
    result.diagnostic_detail = "non-Loopback UDP requires --send --allow-non-loopback";
    FinalizeFingerprint(result);
    return 2;
  }

  protocol_plan::PlanOwner plan;
  if (!CompileConfig(arguments.config, config_text, plan, result, error)) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_INPUT_ERROR";
    result.diagnostic_detail = error;
    FinalizeFingerprint(result);
    return 3;
  }
  if (!plan) {
    FinalizeFingerprint(result);
    return 4;
  }
  BindCompiledPlan(*plan, "udp-exchange", result);
  result.config_sha256 = HashBytes(config_text);
  result.transport = "UDP";
  result.local_endpoint = FormatUdpEndpoint(local);
  result.remote_endpoint = FormatUdpEndpoint(remote);
  result.timeout_ms = arguments.timeout_ms;
  result.receive_pipeline_id = arguments.receive_pipeline;
  const std::uint64_t plan_frame_limit = plan->GetResourceRequirements().max_frame_bytes;
  result.max_frame_bytes = plan_frame_limit < kMaximumIpv4UdpPayloadBytes
                               ? plan_frame_limit
                               : kMaximumIpv4UdpPayloadBytes;
  if (!HasPipeline(*plan, arguments.receive_pipeline)) {
    result.status = "CLI_ERROR";
    result.diagnostic_id = "PAE_LAB_UNKNOWN_RECEIVE_PIPELINE";
    result.diagnostic_detail = "receive pipeline id is unknown";
    FinalizePostPlanFailure(result, "ENCODE_TX", "TX");
    return 2;
  }

  std::string values;
  ParsedValues parsed;
  if (!ReadText(arguments.values, values, error)) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_VALUES_INPUT_ERROR";
    result.diagnostic_detail = error;
    FinalizePostPlanFailure(result, "ENCODE_TX", "TX");
    return 3;
  }
  values_text = values;
  if (!ParseValues(values, parsed, error)) {
    result.status = "VALUES_INVALID";
    result.diagnostic_id = "PAE_LAB_VALUES_INVALID";
    result.diagnostic_detail = error;
    FinalizePostPlanFailure(result, "ENCODE_TX", "TX");
    return 5;
  }

  if (execution_observer != nullptr) {
    execution_observer->OnProtocolOperation("ENCODE_TX");
  }
  result = EncodeValues(*plan, parsed, error);
  if (plan->SchemaVersion() != "0.1") {
    result.replay_mode = "ENCODE_TX";
    result.replay_subject = "TX";
    result.current_execution_status = result.status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
  }
  result.command = "udp-exchange";
  result.operation_kind = "udp-exchange";
  result.config_sha256 = HashBytes(config_text);
  result.transport = "UDP";
  result.local_endpoint = FormatUdpEndpoint(local);
  result.remote_endpoint = FormatUdpEndpoint(remote);
  result.timeout_ms = arguments.timeout_ms;
  result.receive_pipeline_id = arguments.receive_pipeline;
  result.max_frame_bytes = plan_frame_limit < kMaximumIpv4UdpPayloadBytes
                               ? plan_frame_limit
                               : kMaximumIpv4UdpPayloadBytes;
  if (result.status != "OK") {
    FinalizeFingerprint(result);
    return 5;
  }
  if (result.frame.empty() || result.frame.size() > kMaximumIpv4UdpPayloadBytes) {
    result.status = "TRANSPORT_FAILED";
    result.diagnostic_id = "UDP_DATAGRAM_TOO_LARGE";
    result.diagnostic_detail = "encoded request does not fit one IPv4 UDP datagram";
    result.frame.clear();
    FinalizeFingerprint(result);
    return 8;
  }
  result.tx_frame = result.frame;
  result.frame_file = "frames/000001_tx.bin";
  result.replay_mode = "ENCODE_TX";
  result.replay_subject = "TX";
  result.current_execution_status = result.status;
  result.current_execution_diagnostic_id = result.diagnostic_id;

  EvidenceBundleTransaction transaction{file_system};
  if (!transaction.Begin(arguments.record_root, config_text, values_text, result, error) ||
      !transaction.RecordFrame("frames/000001_tx", result.tx_frame, error)) {
    return FailRecord(result, error);
  }
  transaction.CaptureTxFrame(result.tx_frame);

  if (!arguments.send) {
    return CompleteUdpTransaction(arguments, transaction, result, 0);
  }

  UdpExchangeRequest request;
  request.local = local;
  request.remote = remote;
  request.timeout_ms = arguments.timeout_ms;
  request.payload = result.tx_frame;
  TransactionUdpEventObserver event_observer{transaction, result.remote_endpoint};
  request.event_observer = &event_observer;
  UdpExchangeResponse response = udp_adapter.Exchange(request);
  result.send_attempted = response.send_attempted;
  result.send_succeeded = response.send_succeeded;
  result.response_received = response.response_received;
  if (response.local_bound) {
    result.local_endpoint = FormatUdpEndpoint(response.bound_local);
  }
  if (response.response_received) {
    result.rx_frame = response.payload;
    result.received_from = FormatUdpEndpoint(response.peer);
    if (!transaction.RecordReceivedFrame("frames/000002_rx", result.rx_frame, result.received_from,
                                         error)) {
      return FailRecord(result, error);
    }
  }

  int natural_exit = 0;
  if (response.status != UdpExchangeStatus::OK || !response.response_received) {
    const UdpExchangeStatus status = response.status == UdpExchangeStatus::OK
                                         ? UdpExchangeStatus::RECEIVE_FAILED
                                         : response.status;
    result.status = status == UdpExchangeStatus::TIMEOUT ? "TIMEOUT" : "TRANSPORT_FAILED";
    result.diagnostic_id = UdpDiagnosticId(status);
    result.diagnostic_detail = response.detail.empty() ? "UDP Exchange failed" : response.detail;
    natural_exit = status == UdpExchangeStatus::TIMEOUT ? 9 : 8;
    if (status == UdpExchangeStatus::DATAGRAM_TRUNCATED) {
      result.replay_mode = "NO_CODEC_REEXECUTION";
      result.replay_subject = "RX_INCOMPLETE";
      result.current_execution_status = "NOT_EVALUATED";
      result.current_execution_diagnostic_id.clear();
    }
  } else if (!SameUdpEndpoint(response.peer, remote)) {
    result.status = "TRANSPORT_FAILED";
    result.diagnostic_id = "UDP_PEER_MISMATCH";
    result.diagnostic_detail = "response source does not match the configured remote endpoint";
    natural_exit = 8;
    result.replay_mode = "NO_CODEC_REEXECUTION";
    result.replay_subject = "RX";
    result.current_execution_status = "NOT_EVALUATED";
    result.current_execution_diagnostic_id.clear();
  } else if (result.rx_frame.size() > kMaximumIpv4UdpPayloadBytes ||
             result.rx_frame.size() > plan->GetResourceRequirements().max_frame_bytes) {
    result.status = "TRANSPORT_FAILED";
    result.diagnostic_id = "UDP_DATAGRAM_TOO_LARGE";
    result.diagnostic_detail = "response exceeds the UDP Adapter or compiled Plan limit";
    natural_exit = 8;
    result.replay_mode = "NO_CODEC_REEXECUTION";
    result.replay_subject = "RX";
    result.current_execution_status = "NOT_EVALUATED";
    result.current_execution_diagnostic_id.clear();
  } else {
    if (execution_observer != nullptr) {
      execution_observer->OnProtocolOperation("DECODE_RX");
    }
    OperationResult decoded =
        InspectFrameInPipeline(*plan, arguments.receive_pipeline, result.rx_frame);
    decoded.command = "udp-exchange";
    decoded.operation_kind = "udp-exchange";
    decoded.config_sha256 = result.config_sha256;
    decoded.frame_file = "frames/000002_rx.bin";
    decoded.transport = result.transport;
    decoded.local_endpoint = result.local_endpoint;
    decoded.remote_endpoint = result.remote_endpoint;
    decoded.received_from = result.received_from;
    decoded.receive_pipeline_id = result.receive_pipeline_id;
    decoded.timeout_ms = result.timeout_ms;
    decoded.max_frame_bytes = result.max_frame_bytes;
    decoded.tx_frame = std::move(result.tx_frame);
    decoded.rx_frame = std::move(result.rx_frame);
    decoded.send_attempted = result.send_attempted;
    decoded.send_succeeded = result.send_succeeded;
    decoded.response_received = true;
    decoded.response_decoded = decoded.status == "OK";
    decoded.replay_mode = "DECODE_RX";
    decoded.replay_subject = "RX";
    decoded.current_execution_status = decoded.status;
    decoded.current_execution_diagnostic_id = decoded.diagnostic_id;
    decoded.evidence_bundle = result.evidence_bundle;
    result = std::move(decoded);
    natural_exit = result.status == "OK" ? 0 : 5;
  }
  return CompleteUdpTransaction(arguments, transaction, result, natural_exit);
}

int RunReplay(const Arguments& arguments, OperationResult& result, std::string& config_text,
              std::optional<std::string>& values_text, std::filesystem::path& record_root,
              IProtocolLabExecutionObserver* execution_observer) {
  std::string error;
  StoredRun stored;
  if (!LoadStoredRun(arguments.bundle, stored, error)) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_RUN_INPUT_ERROR";
    result.diagnostic_detail = error;
    return 3;
  }
  if (stored.operation_kind == "udp-exchange" && stored.format_version == kResultFormatV1) {
    result.status = "EVIDENCE_INSUFFICIENT";
    result.diagnostic_id = "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT";
    result.diagnostic_detail =
        "legacy UDP V0.1 Run has no explicit replay mode or RX metadata binding";
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
  BindCompiledPlan(*plan, stored.operation_kind, result);
  result.command = "replay";
  result.config_sha256 = HashBytes(config_text);
  result.cross_config_replay = !arguments.config.empty();
  result.receive_pipeline_id = stored.receive_pipeline_id;
  if (stored.operation_kind == "inspect") {
    result.replay_mode = "DECODE_RX";
    result.replay_subject = "RX";
  } else if (stored.operation_kind == "encode") {
    result.replay_mode = "ENCODE_TX";
    result.replay_subject = "TX";
  } else {
    result.replay_mode = stored.replay_mode;
    result.replay_subject = stored.replay_subject;
  }
  PreserveHistoricalTransport(stored, result);
  const unsigned stored_generation = stored.format_version == kResultFormatV4
                                         ? 4U
                                         : (stored.format_version == kResultFormatV3 ? 3U : 2U);
  const unsigned plan_generation =
      plan->SchemaVersion() == "0.3" ? 4U : (plan->SchemaVersion() == "0.2" ? 3U : 2U);
  if (stored_generation != plan_generation) {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_CROSS_SCHEMA_REPLAY_UNSUPPORTED";
    result.diagnostic_detail =
        "Replay configuration and stored Run belong to different Schema generations";
    FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
    return 3;
  }
  if (stored.operation_kind == "inspect") {
    std::vector<std::uint8_t> frame;
    if (!ReadFile(arguments.bundle / stored.frame_file, frame, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_REPLAY_FRAME_ERROR";
      result.diagnostic_detail = error;
      FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
      return 3;
    }
    if (!InspectWithinPlanLimit(*plan, frame, result, execution_observer)) {
      result.command = "replay";
    }
  } else if (stored.operation_kind == "encode" && !stored.values_file.empty()) {
    std::string text;
    ParsedValues parsed;
    if (!ReadText(arguments.bundle / stored.values_file, text, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_REPLAY_VALUES_ERROR";
      result.diagnostic_detail = error;
      FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
      return 3;
    }
    values_text = text;
    if (!ParseValues(text, parsed, error)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_REPLAY_VALUES_ERROR";
      result.diagnostic_detail = error;
      FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
      return 3;
    }
    if (execution_observer != nullptr) {
      execution_observer->OnProtocolOperation("ENCODE_TX");
    }
    result = EncodeValues(*plan, parsed, error);
    if (stored_generation >= 3U) {
      result.replay_mode = "ENCODE_TX";
      result.replay_subject = "TX";
      result.current_execution_status = result.status;
      result.current_execution_diagnostic_id = result.diagnostic_id;
    }
  } else if (stored.operation_kind == "udp-exchange" &&
             (stored.format_version == kResultFormat || stored.format_version == kResultFormatV3 ||
              stored.format_version == kResultFormatV4)) {
    std::vector<std::uint8_t> tx_frame;
    std::vector<std::uint8_t> rx_frame;
    const bool replay_uses_rx =
        stored.replay_mode == "DECODE_RX" ||
        (stored.replay_mode == "NO_CODEC_REEXECUTION" &&
         (stored.replay_subject == "RX" || stored.replay_subject == "RX_INCOMPLETE"));
    if (!ParseFrameHex(stored.tx_frame_hex, tx_frame, error) ||
        (replay_uses_rx && !stored.rx_frame_hex.empty() &&
         !ParseFrameHex(stored.rx_frame_hex, rx_frame, error)) ||
        (!replay_uses_rx && !stored.rx_frame_hex.empty())) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_REPLAY_FRAME_ERROR";
      result.diagnostic_detail = "stored UDP frame Hex is invalid: " + error;
      FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
      return 3;
    }

    std::string values;
    ParsedValues parsed;
    if (stored.replay_mode == "ENCODE_TX") {
      if (stored.values_file.empty() ||
          !ReadText(arguments.bundle / stored.values_file, values, error)) {
        result.status = "EVIDENCE_INSUFFICIENT";
        result.diagnostic_id = "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT";
        result.diagnostic_detail =
            error.empty() ? "ENCODE_TX Replay requires valid stored Values" : error;
        FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
        return 3;
      }
      values_text = values;
      if (!ParseValues(values, parsed, error)) {
        result.status = "EVIDENCE_INSUFFICIENT";
        result.diagnostic_id = "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT";
        result.diagnostic_detail = error;
        FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
        return 3;
      }
      if (execution_observer != nullptr) {
        execution_observer->OnProtocolOperation("ENCODE_TX");
      }
      result = EncodeValues(*plan, parsed, error);
      result.replay_mode = "ENCODE_TX";
      result.replay_subject = "TX";
      result.current_execution_status = result.status;
      result.current_execution_diagnostic_id = result.diagnostic_id;
      result.frame_file = "frames/000001_frame.bin";
    } else if (stored.replay_mode == "DECODE_RX") {
      if (!replay_uses_rx || stored.receive_pipeline_id.empty()) {
        result.status = "EVIDENCE_INSUFFICIENT";
        result.diagnostic_id = "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT";
        result.diagnostic_detail = "DECODE_RX Replay requires a recorded response and Pipeline";
        FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
        return 3;
      }
      if (!stored.values_file.empty() &&
          !ReadText(arguments.bundle / stored.values_file, values, error)) {
        result.status = "INPUT_ERROR";
        result.diagnostic_id = "PAE_LAB_REPLAY_VALUES_ERROR";
        result.diagnostic_detail = error;
        FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
        return 3;
      }
      if (!values.empty()) {
        values_text = values;
      }
      if (execution_observer != nullptr) {
        execution_observer->OnProtocolOperation("DECODE_RX");
      }
      result = InspectFrameInPipeline(*plan, stored.receive_pipeline_id, rx_frame);
      result.replay_mode = "DECODE_RX";
      result.replay_subject = "RX";
      result.current_execution_status = result.status;
      result.current_execution_diagnostic_id = result.diagnostic_id;
      result.frame_file = "frames/000001_frame.bin";
      result.response_decoded = result.status == "OK";
    } else if (stored.replay_mode == "NO_CODEC_REEXECUTION") {
      result.operation_kind = "udp-exchange";
      result.status = "EVIDENCE_VERIFIED";
      result.replay_mode = "NO_CODEC_REEXECUTION";
      result.replay_subject = stored.replay_subject;
      result.current_execution_status = "NOT_EVALUATED";
      result.comparison_status = "NOT_EVALUATED";
      result.comparison_reason = "historical receive gate prevented Codec execution";
      result.frame = replay_uses_rx ? rx_frame : tx_frame;
      result.frame_file = "frames/000001_frame.bin";
      if (!stored.values_file.empty() &&
          !ReadText(arguments.bundle / stored.values_file, values, error)) {
        result.status = "INPUT_ERROR";
        result.diagnostic_id = "PAE_LAB_REPLAY_VALUES_ERROR";
        result.diagnostic_detail = error;
        FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
        return 3;
      }
      if (!values.empty()) {
        values_text = values;
      }
    } else {
      result.status = "EVIDENCE_INSUFFICIENT";
      result.diagnostic_id = "PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT";
      result.diagnostic_detail = "UDP Run has no supported explicit replay mode";
      FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
      return 3;
    }

    result.operation_kind = "udp-exchange";
    result.tx_frame = std::move(tx_frame);
    result.rx_frame = std::move(rx_frame);
    result.receive_pipeline_id = stored.receive_pipeline_id;
  } else {
    result.status = "INPUT_ERROR";
    result.diagnostic_id = "PAE_LAB_REPLAY_OPERATION_UNSUPPORTED";
    result.diagnostic_detail = "stored operation cannot be replayed by the offline slice";
    FinalizePostPlanFailure(result, result.replay_mode, result.replay_subject);
    return 3;
  }
  result.command = "replay";
  result.schema_version.assign(plan->SchemaVersion().data(), plan->SchemaVersion().size());
  result.config_sha256 = HashBytes(config_text);
  result.cross_config_replay = !arguments.config.empty();
  PreserveHistoricalTransport(stored, result);
  FinalizeFingerprint(result);
  if (result.replay_mode != "NO_CODEC_REEXECUTION") {
    result.comparison_categories = CompareStoredRuns(stored, ToStoredRun(result));
    result.comparison_equal =
        result.deterministic_fingerprint == stored.deterministic_fingerprint &&
        result.comparison_categories.empty();
    result.comparison_status = *result.comparison_equal ? "EQUAL" : "DIFFERENT";
    result.comparison_reason = *result.comparison_equal
                                   ? "deterministic protocol re-execution matches history"
                                   : "deterministic protocol re-execution differs from history";
  }
  record_root =
      arguments.record_root.empty() ? arguments.bundle.parent_path() : arguments.record_root;
  if (result.replay_mode == "NO_CODEC_REEXECUTION") {
    return 0;
  }
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
    const auto generation = [](std::string_view format) {
      return format == kResultFormatV4 ? 4U : (format == kResultFormatV3 ? 3U : 2U);
    };
    if (generation(left.format_version) != generation(right.format_version)) {
      result.status = "INPUT_ERROR";
      result.diagnostic_id = "PAE_LAB_CROSS_FORMAT_COMPARE_UNSUPPORTED";
      result.diagnostic_detail =
          "Run comparison across legacy and 0.3 fingerprint domains is unsupported";
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

int RunApplicationWithDependencies(int argc, char** argv, RecordFileSystem& file_system,
                                   IUdpExchangeAdapter& udp_adapter,
                                   IProtocolLabExecutionObserver* execution_observer) {
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
  if (arguments.command == Command::UDP_EXCHANGE) {
    exit_code = RunUdpExchange(arguments, result, config_text, values_text, file_system,
                               udp_adapter, execution_observer);
    result.exit_code = exit_code;
    PrintResult(result, arguments.output);
    return exit_code;
  }
  if (arguments.command == Command::INSPECT || arguments.command == Command::ENCODE) {
    exit_code = RunInspectOrEncode(arguments, result, config_text, values_text, execution_observer);
  } else if (arguments.command == Command::REPLAY) {
    exit_code =
        RunReplay(arguments, result, config_text, values_text, record_root, execution_observer);
  } else {
    exit_code = RunCompare(arguments, result);
  }

  exit_code = ApplyExpectedStatus(arguments, result, exit_code);
  result.exit_code = exit_code;
  const bool required_inputs_available =
      !((result.operation_kind == "encode" || result.operation_kind == "udp-exchange") &&
        !values_text.has_value()) &&
      !(result.operation_kind == "inspect" && result.diagnostic_id == "PAE_LAB_FRAME_INPUT_ERROR");
  if (!record_root.empty() && arguments.command != Command::COMPARE && !config_text.empty() &&
      required_inputs_available) {
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

int RunApplicationWithFileSystem(int argc, char** argv, RecordFileSystem& file_system) {
  std::unique_ptr<IUdpExchangeAdapter> udp_adapter = CreatePlatformUdpExchangeAdapter();
  return RunApplicationWithDependencies(argc, argv, file_system, *udp_adapter);
}

int RunApplication(int argc, char** argv) {
  StandardRecordFileSystem file_system;
  return RunApplicationWithFileSystem(argc, argv, file_system);
}

}  // namespace pae::protocol_lab
