#include "result_format.h"

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
  output << "operation=" << result.operation_kind << '\n';
  output << "status=" << result.status << '\n';
  output << "config=" << result.config_sha256 << '\n';
  output << "protocol=" << result.protocol_id << '\n';
  output << "pipeline=" << result.pipeline_id << '\n';
  output << "message=" << result.message_id << '\n';
  output << "direction=" << result.direction_id << '\n';
  output << "frame=" << HexUpper(result.frame) << '\n';
  output << "diagnostic=" << result.diagnostic_id << '\n';
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
  const std::string frame_hex = HexUpper(result.frame);
  std::ostringstream output;
  output << "{\n"
         << "  \"format_version\":" << Quoted(kResultFormat) << ",\n"
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
         << "  \"frame_file\":\"frames/000001_frame.bin\",\n"
         << "  \"values_file\":"
         << (result.operation_kind == "encode" ? "\"inputs/values.pae-lab.json\"" : "null") << ",\n"
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
  const std::string engine_gate = result.operation_kind == "compare"
                                      ? "NOT_EVALUATED"
                                      : (result.status == "OK" ? "PASS" : "FAIL");
  output << ",\n"
         << "  \"comparison_categories\":" << SerializeStringArray(result.comparison_categories)
         << ",\n"
         << "  \"evidence_bundle\":" << Quoted(result.evidence_bundle) << ",\n"
         << "  \"send_attempted\":false,\n"
         << "  \"send_succeeded\":false,\n"
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
  if (left.operation_kind != right.operation_kind) {
    categories.emplace_back("OPERATION_KIND");
  }
  if (left.config_sha256 != right.config_sha256) {
    categories.emplace_back("CONFIG");
  }
  if (left.frame_hex != right.frame_hex) {
    categories.emplace_back("WIRE_BYTES");
  }
  if (left.pipeline_id != right.pipeline_id || left.message_id != right.message_id ||
      left.direction_id != right.direction_id) {
    categories.emplace_back("MESSAGE_MATCH");
  }
  if (left.operation_status != right.operation_status) {
    categories.emplace_back("STATUS");
  }
  if (left.fields_canonical != right.fields_canonical) {
    categories.emplace_back("TYPED_FIELDS");
  }
  if (left.diagnostic_id != right.diagnostic_id) {
    categories.emplace_back("STABLE_DIAGNOSTIC");
  }
  if (categories.empty() && left.deterministic_fingerprint != right.deterministic_fingerprint) {
    categories.emplace_back("DETERMINISTIC_FINGERPRINT");
  }
  return categories;
}

StoredRun ToStoredRun(const OperationResult& result) {
  StoredRun stored;
  stored.operation_kind = result.operation_kind;
  stored.operation_status = result.status;
  stored.config_sha256 = result.config_sha256;
  stored.pipeline_id = result.pipeline_id;
  stored.message_id = result.message_id;
  stored.direction_id = result.direction_id;
  stored.frame_hex = HexUpper(result.frame);
  stored.fields_canonical = FieldsCanonical(result);
  stored.diagnostic_id = result.diagnostic_id;
  stored.deterministic_fingerprint = result.deterministic_fingerprint;
  return stored;
}

}  // namespace pae::protocol_lab
