#include "result_format.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "sha256.h"

namespace pae::protocol_lab {

std::string JsonEscape(std::string_view input) {
  std::ostringstream output;
  for (const unsigned char value : input) {
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
          output << "\\u" << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                 << static_cast<unsigned int>(value) << std::dec;
        } else {
          output << static_cast<char>(value);
        }
    }
  }
  return output.str();
}

std::string Quoted(std::string_view value) { return "\"" + JsonEscape(value) + "\""; }

std::string HexUpper(const std::uint8_t* data, std::size_t size) {
  static constexpr char kDigits[] = "0123456789ABCDEF";
  std::string output(size * 2U, '0');
  for (std::size_t index = 0U; index < size; ++index) {
    output[index * 2U] = kDigits[data[index] >> 4U];
    output[index * 2U + 1U] = kDigits[data[index] & 0x0FU];
  }
  return output;
}

std::string HexUpper(const std::vector<std::uint8_t>& data) {
  return HexUpper(data.data(), data.size());
}

std::string CodecStatusName(protocol_core::CodecStatus status) {
  using protocol_core::CodecStatus;
  switch (status) {
    case CodecStatus::OK:
      return "OK";
    case CodecStatus::INVALID_ARGUMENT:
      return "INVALID_ARGUMENT";
    case CodecStatus::INVALID_PLAN:
      return "INVALID_PLAN";
    case CodecStatus::WORKSPACE_PLAN_MISMATCH:
      return "WORKSPACE_PLAN_MISMATCH";
    case CodecStatus::WORKSPACE_BUSY:
      return "WORKSPACE_BUSY";
    case CodecStatus::UNKNOWN_MESSAGE:
      return "UNKNOWN_MESSAGE";
    case CodecStatus::AMBIGUOUS_MESSAGE:
      return "AMBIGUOUS_MESSAGE";
    case CodecStatus::OUTPUT_SLOTS_TOO_SMALL:
      return "OUTPUT_SLOTS_TOO_SMALL";
    case CodecStatus::INTEGRITY_FAILED:
      return "INTEGRITY_FAILED";
    case CodecStatus::MESSAGE_NOT_ALLOWED:
      return "MESSAGE_NOT_ALLOWED";
    case CodecStatus::FIELD_REFERENCE_MISMATCH:
      return "FIELD_REFERENCE_MISMATCH";
    case CodecStatus::DUPLICATE_FIELD:
      return "DUPLICATE_FIELD";
    case CodecStatus::MISSING_FIELD:
      return "MISSING_FIELD";
    case CodecStatus::TYPE_MISMATCH:
      return "TYPE_MISMATCH";
    case CodecStatus::VALUE_NOT_REPRESENTABLE:
      return "VALUE_NOT_REPRESENTABLE";
    case CodecStatus::BYTES_LENGTH_MISMATCH:
      return "BYTES_LENGTH_MISMATCH";
    case CodecStatus::UNKNOWN_ENUM_VALUE:
      return "UNKNOWN_ENUM_VALUE";
    case CodecStatus::ENUM_REFERENCE_MISMATCH:
      return "ENUM_REFERENCE_MISMATCH";
    case CodecStatus::CONSTANT_FIELD_OVERRIDE:
      return "CONSTANT_FIELD_OVERRIDE";
    case CodecStatus::INPUT_OUTPUT_OVERLAP:
      return "INPUT_OUTPUT_OVERLAP";
    case CodecStatus::BUFFER_TOO_SMALL:
      return "BUFFER_TOO_SMALL";
    case CodecStatus::FINAL_REVIEW_FAILED:
      return "FINAL_REVIEW_FAILED";
  }
  return "UNKNOWN_CODEC_STATUS";
}

namespace {

std::string DeterministicPayload(const OperationResult& result) {
  std::ostringstream output;
  if (result.schema_version == "0.4") {
    output << "fingerprint_domain=pae.lab.fingerprint/0.5\n";
  } else if (result.schema_version == "0.3") {
    output << "fingerprint_domain=pae.lab.fingerprint/0.4\n";
  } else if (result.schema_version == "0.2") {
    output << "fingerprint_domain=pae.lab.fingerprint/0.3\n";
  }
  output << "operation=" << result.operation_kind << '\n';
  if (result.operation_kind == "udp-exchange") {
    output << "replay_mode=" << result.replay_mode << '\n';
    output << "replay_subject=" << result.replay_subject << '\n';
    output << "current_execution_status=" << result.current_execution_status << '\n';
    output << "current_execution_diagnostic=" << result.current_execution_diagnostic_id << '\n';
  } else {
    output << "status=" << result.status << '\n';
  }
  output << "config=" << result.config_sha256 << '\n';
  output << "protocol=" << result.protocol_id << '\n';
  output << "pipeline=" << result.pipeline_id << '\n';
  output << "message=" << result.message_id << '\n';
  output << "direction=" << result.direction_id << '\n';
  output << "frame=" << HexUpper(result.frame) << '\n';
  if (result.operation_kind == "udp-exchange") {
    output << "tx_frame=" << HexUpper(result.tx_frame) << '\n';
    output << "rx_frame=" << HexUpper(result.rx_frame) << '\n';
  }
  if (result.operation_kind != "udp-exchange") {
    output << "diagnostic=" << result.diagnostic_id << '\n';
  }
  for (const FieldResult& field : result.fields) {
    output << "field=" << field.id << '|' << field.kind << '|' << field.raw_value << '|'
           << field.logical_value << '|' << (field.enum_known ? "known" : "not-enum-or-unknown")
           << '\n';
  }
  return output.str();
}

std::string SerializeStringArray(const std::vector<std::string>& values) {
  std::ostringstream output;
  output << '[';
  for (std::size_t index = 0U; index < values.size(); ++index) {
    if (index != 0U) {
      output << ',';
    }
    output << Quoted(values[index]);
  }
  output << ']';
  return output.str();
}

}  // namespace

void FinalizeFingerprint(OperationResult& result) {
  result.deterministic_fingerprint = HashBytes(DeterministicPayload(result));
}

std::string SerializeFields(const std::vector<FieldResult>& fields, int indent) {
  const std::string prefix(static_cast<std::size_t>(indent), ' ');
  const std::string inner(static_cast<std::size_t>(indent + 2), ' ');
  std::ostringstream output;
  output << "[";
  if (!fields.empty()) {
    output << '\n';
  }
  for (std::size_t index = 0U; index < fields.size(); ++index) {
    const FieldResult& field = fields[index];
    output << inner << "{\"id\":" << Quoted(field.id) << ",\"kind\":" << Quoted(field.kind)
           << ",\"raw_value\":" << Quoted(field.raw_value)
           << ",\"logical_value\":" << Quoted(field.logical_value)
           << ",\"enum_known\":" << (field.enum_known ? "true" : "false") << "}";
    output << (index + 1U == fields.size() ? "\n" : ",\n");
  }
  if (!fields.empty()) {
    output << prefix;
  }
  output << "]";
  return output.str();
}

std::string SerializeFieldsCompact(const std::vector<FieldResult>& fields) {
  std::ostringstream output;
  output << '[';
  for (std::size_t index = 0U; index < fields.size(); ++index) {
    const FieldResult& field = fields[index];
    if (index != 0U) {
      output << ',';
    }
    output << "{\"id\":" << Quoted(field.id) << ",\"kind\":" << Quoted(field.kind)
           << ",\"raw_value\":" << Quoted(field.raw_value)
           << ",\"logical_value\":" << Quoted(field.logical_value)
           << ",\"enum_known\":" << (field.enum_known ? "true" : "false") << '}';
  }
  output << ']';
  return output.str();
}

std::string SerializeResult(const OperationResult& result) {
  const std::string_view result_format =
      result.schema_version == "0.4" ? kResultFormatV5
      : result.schema_version == "0.3"
          ? kResultFormatV4
          : (result.schema_version == "0.2"
                 ? kResultFormatV3
                 : (result.operation_kind == "udp-exchange" ? kResultFormat : kResultFormatV1));
  const std::string frame_hex = HexUpper(result.frame);
  const std::string frame_file =
      result.frame_file.empty() ? "frames/000001_frame.bin" : result.frame_file;
  std::ostringstream output;
  output << "{\n"
         << "  \"format_version\":" << Quoted(result_format) << ",\n"
         << "  \"command\":" << Quoted(result.command) << ",\n"
         << "  \"operation_kind\":" << Quoted(result.operation_kind) << ",\n"
         << "  \"operation_status\":" << Quoted(result.status) << ",\n"
         << "  \"exit_code\":" << result.exit_code << ",\n"
         << "  \"config_sha256\":" << Quoted(result.config_sha256) << ",\n"
         << "  \"protocol_id\":" << Quoted(result.protocol_id) << ",\n"
         << "  \"pipeline_id\":" << Quoted(result.pipeline_id) << ",\n"
         << "  \"message_id\":" << Quoted(result.message_id) << ",\n"
         << "  \"direction_id\":" << Quoted(result.direction_id) << ",\n"
         << "  \"frame_length\":" << result.frame.size() << ",\n"
         << "  \"frame_sha256\":" << Quoted(HashBytes(result.frame)) << ",\n"
         << "  \"frame_hex\":" << Quoted(frame_hex) << ",\n"
         << "  \"frame_file\":" << Quoted(frame_file) << ",\n"
         << "  \"tx_frame_length\":" << result.tx_frame.size() << ",\n"
         << "  \"tx_frame_sha256\":" << Quoted(HashBytes(result.tx_frame)) << ",\n"
         << "  \"tx_frame_hex\":" << Quoted(HexUpper(result.tx_frame)) << ",\n"
         << "  \"tx_frame_file\":"
         << (result.command == "udp-exchange" && !result.tx_frame.empty()
                 ? "\"frames/000001_tx.bin\""
                 : "null")
         << ",\n"
         << "  \"rx_frame_length\":" << result.rx_frame.size() << ",\n"
         << "  \"rx_frame_sha256\":" << Quoted(HashBytes(result.rx_frame)) << ",\n"
         << "  \"rx_frame_hex\":" << Quoted(HexUpper(result.rx_frame)) << ",\n"
         << "  \"rx_frame_file\":"
         << (result.command == "udp-exchange" && result.response_received
                 ? "\"frames/000002_rx.bin\""
                 : "null")
         << ",\n"
         << "  \"values_file\":"
         << (result.operation_kind == "encode" || result.operation_kind == "udp-exchange"
                 ? "\"inputs/values.pae-lab.json\""
                 : "null")
         << ",\n"
         << "  \"fields\":" << SerializeFields(result.fields, 2) << ",\n"
         << "  \"diagnostic\":{\"id\":" << Quoted(result.diagnostic_id)
         << ",\"detail\":" << Quoted(result.diagnostic_detail) << "},\n"
         << "  \"deterministic_fingerprint\":" << Quoted(result.deterministic_fingerprint) << ",\n"
         << "  \"cross_config_replay\":" << (result.cross_config_replay ? "true" : "false") << ",\n"
         << "  \"comparison_equal\":";
  if (result.comparison_equal.has_value()) {
    output << (*result.comparison_equal ? "true" : "false");
  } else {
    output << "null";
  }
  const std::string engine_gate =
      result.operation_kind == "compare" || result.replay_mode == "NO_CODEC_REEXECUTION"
          ? "NOT_EVALUATED"
          : (result.status == "OK" ? "PASS" : "FAIL");
  output << ",\n"
         << "  \"comparison_categories\":" << SerializeStringArray(result.comparison_categories)
         << ",\n"
         << "  \"evidence_bundle\":" << Quoted(result.evidence_bundle) << ",\n"
         << "  \"transport\":" << Quoted(result.transport) << ",\n"
         << "  \"local_endpoint\":" << Quoted(result.local_endpoint) << ",\n"
         << "  \"remote_endpoint\":" << Quoted(result.remote_endpoint) << ",\n"
         << "  \"received_from\":" << Quoted(result.received_from) << ",\n"
         << "  \"timeout_ms\":" << result.timeout_ms << ",\n"
         << "  \"send_attempted\":" << (result.send_attempted ? "true" : "false") << ",\n"
         << "  \"send_succeeded\":" << (result.send_succeeded ? "true" : "false") << ",\n"
         << "  \"response_received\":" << (result.response_received ? "true" : "false") << ",\n"
         << "  \"response_decoded\":" << (result.response_decoded ? "true" : "false");
  if (result_format == kResultFormat || result_format == kResultFormatV3 ||
      result_format == kResultFormatV4 || result_format == kResultFormatV5) {
    output << ",\n"
           << "  \"receive_pipeline_id\":" << Quoted(result.receive_pipeline_id) << ",\n"
           << "  \"replay_mode\":" << Quoted(result.replay_mode) << ",\n"
           << "  \"replay_subject\":" << Quoted(result.replay_subject) << ",\n"
           << "  \"current_execution_status\":" << Quoted(result.current_execution_status) << ",\n"
           << "  \"current_execution_diagnostic_id\":"
           << Quoted(result.current_execution_diagnostic_id) << ",\n"
           << "  \"comparison_status\":" << Quoted(result.comparison_status) << ",\n"
           << "  \"comparison_reason\":" << Quoted(result.comparison_reason) << ",\n"
           << "  \"historical_transport\":";
    const bool source_udp = result.command == "udp-exchange";
    if (!source_udp && !result.historical_transport.present) {
      output << "null";
    } else {
      const HistoricalTransportFacts& historical = result.historical_transport;
      output << "{\"transport\":" << Quoted(source_udp ? result.transport : historical.transport)
             << ",\"timeout_ms\":" << (source_udp ? result.timeout_ms : historical.timeout_ms)
             << ",\"status\":" << Quoted(source_udp ? result.status : historical.status)
             << ",\"diagnostic_id\":"
             << Quoted(source_udp ? result.diagnostic_id : historical.diagnostic_id)
             << ",\"local_endpoint\":"
             << Quoted(source_udp ? result.local_endpoint : historical.local_endpoint)
             << ",\"remote_endpoint\":"
             << Quoted(source_udp ? result.remote_endpoint : historical.remote_endpoint)
             << ",\"received_from\":"
             << Quoted(source_udp ? result.received_from : historical.received_from)
             << ",\"send_attempted\":"
             << ((source_udp ? result.send_attempted : historical.send_attempted) ? "true"
                                                                                  : "false")
             << ",\"send_succeeded\":"
             << ((source_udp ? result.send_succeeded : historical.send_succeeded) ? "true"
                                                                                  : "false")
             << ",\"response_received\":"
             << ((source_udp ? result.response_received : historical.response_received) ? "true"
                                                                                        : "false")
             << ",\"response_decoded\":"
             << ((source_udp ? result.response_decoded : historical.response_decoded) ? "true"
                                                                                      : "false")
             << '}';
    }
  }
  output << ",\n"
         << "  \"gates\":{\"ENGINE_POC_PASS\":" << Quoted(engine_gate)
         << ",\"LAB_EXCHANGE_PASS\":\"NOT_EVALUATED\","
            "\"PROTOCOL_GOLDEN_PASS\":\"NOT_EVALUATED\"}\n"
         << "}\n";
  return output.str();
}

std::string FieldsCanonical(const OperationResult& result) {
  std::ostringstream output;
  for (const FieldResult& field : result.fields) {
    output << field.id << '|' << field.kind << '|' << field.raw_value << '|' << field.logical_value
           << '|' << (field.enum_known ? "true" : "false") << '\n';
  }
  return output.str();
}

std::vector<std::string> CompareStoredRuns(const StoredRun& left, const StoredRun& right) {
  std::vector<std::string> categories;
  const bool modern =
      (left.format_version == kResultFormatV5 && right.format_version == kResultFormatV5) ||
      (left.format_version == kResultFormatV4 && right.format_version == kResultFormatV4) ||
      (left.format_version == kResultFormatV3 && right.format_version == kResultFormatV3) ||
      (left.operation_kind == "udp-exchange" && right.operation_kind == "udp-exchange" &&
       left.format_version == kResultFormat && right.format_version == kResultFormat);
  if (left.operation_kind != right.operation_kind) {
    categories.emplace_back("OPERATION_KIND");
  }
  if (left.config_sha256 != right.config_sha256) {
    categories.emplace_back("CONFIG");
  }
  if (left.frame_hex != right.frame_hex) {
    categories.emplace_back("WIRE_BYTES");
  }
  if (left.tx_frame_hex != right.tx_frame_hex || left.rx_frame_hex != right.rx_frame_hex) {
    if (std::find(categories.begin(), categories.end(), "WIRE_BYTES") == categories.end()) {
      categories.emplace_back("WIRE_BYTES");
    }
  }
  if (left.pipeline_id != right.pipeline_id || left.message_id != right.message_id ||
      left.direction_id != right.direction_id) {
    categories.emplace_back("MESSAGE_MATCH");
  }
  if ((modern ? left.current_execution_status != right.current_execution_status
              : left.operation_status != right.operation_status)) {
    categories.emplace_back("STATUS");
  }
  if (left.fields_canonical != right.fields_canonical) {
    categories.emplace_back("TYPED_FIELDS");
  }
  if ((modern ? left.current_execution_diagnostic_id != right.current_execution_diagnostic_id
              : left.diagnostic_id != right.diagnostic_id)) {
    categories.emplace_back("STABLE_DIAGNOSTIC");
  }
  if (categories.empty() && left.deterministic_fingerprint != right.deterministic_fingerprint) {
    categories.emplace_back("DETERMINISTIC_FINGERPRINT");
  }
  return categories;
}

StoredRun ToStoredRun(const OperationResult& result) {
  StoredRun stored;
  stored.format_version =
      result.schema_version == "0.4"   ? std::string{kResultFormatV5}
      : result.schema_version == "0.3" ? std::string{kResultFormatV4}
      : result.schema_version == "0.2"
          ? std::string{kResultFormatV3}
          : (result.operation_kind == "udp-exchange" ? std::string{kResultFormat}
                                                     : std::string{kResultFormatV1});
  stored.command = result.command;
  stored.operation_kind = result.operation_kind;
  stored.operation_status = result.status;
  stored.exit_code = result.exit_code;
  stored.config_sha256 = result.config_sha256;
  stored.pipeline_id = result.pipeline_id;
  stored.message_id = result.message_id;
  stored.direction_id = result.direction_id;
  stored.frame_hex = HexUpper(result.frame);
  stored.frame_length = result.frame.size();
  stored.frame_sha256 = HashBytes(result.frame);
  stored.tx_frame_hex = HexUpper(result.tx_frame);
  stored.tx_frame_length = result.tx_frame.size();
  stored.tx_frame_sha256 = HashBytes(result.tx_frame);
  stored.rx_frame_hex = HexUpper(result.rx_frame);
  stored.rx_frame_length = result.rx_frame.size();
  stored.rx_frame_sha256 = HashBytes(result.rx_frame);
  stored.fields_canonical = FieldsCanonical(result);
  stored.diagnostic_id = result.diagnostic_id;
  stored.deterministic_fingerprint = result.deterministic_fingerprint;
  stored.replay_mode = result.replay_mode;
  stored.replay_subject = result.replay_subject;
  stored.current_execution_status = result.current_execution_status;
  stored.current_execution_diagnostic_id = result.current_execution_diagnostic_id;
  stored.comparison_status = result.comparison_status;
  stored.comparison_reason = result.comparison_reason;
  stored.comparison_equal = result.comparison_equal;
  stored.cross_config_replay = result.cross_config_replay;
  stored.send_attempted = result.send_attempted;
  stored.send_succeeded = result.send_succeeded;
  stored.response_received = result.response_received;
  stored.response_decoded = result.response_decoded;
  stored.local_endpoint = result.local_endpoint;
  stored.remote_endpoint = result.remote_endpoint;
  stored.received_from = result.received_from;
  stored.receive_pipeline_id = result.receive_pipeline_id;
  return stored;
}

}  // namespace pae::protocol_lab
