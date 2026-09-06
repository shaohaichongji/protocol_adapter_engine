#include "protocol_operations.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <utility>

#include "complete_record_codec.h"
#include "config_compiler.h"
#include "result_format.h"
#include "sha256.h"

namespace pae::protocol_lab {
namespace {

using protocol_core::ByteView;
using protocol_core::CodecStatus;
using protocol_core::DecodeCompleteRecord;
using protocol_core::DecodedFieldSlot;
using protocol_core::EncodeCompleteRecord;
using protocol_core::EncodeFieldValue;
using protocol_core::ExecutionWorkspace;
using protocol_core::kInvalidIndex;
using protocol_core::LogicalValueKind;
using protocol_core::MutableByteBuffer;
using protocol_plan::EncodeSource;
using protocol_plan::PlanBundle;
using protocol_plan::ValueType;

bool IsAsciiWhitespace(char value) noexcept {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' ||
         value == '\v';
}

int HexNibble(char value) noexcept {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  return -1;
}

bool ParseUpperHex(std::string_view text, std::vector<std::uint8_t>& output, std::string& error) {
  if (text.empty() || text.size() % 2U != 0U) {
    error = "BYTES hex must contain a non-empty even number of digits";
    return false;
  }
  output.resize(text.size() / 2U);
  for (std::size_t index = 0U; index < output.size(); ++index) {
    const char high = text[index * 2U];
    const char low = text[index * 2U + 1U];
    if (HexNibble(high) < 0 || HexNibble(low) < 0 || (high >= 'a' && high <= 'f') ||
        (low >= 'a' && low <= 'f')) {
      error = "BYTES hex must use uppercase hexadecimal without separators";
      return false;
    }
    output[index] = static_cast<std::uint8_t>((HexNibble(high) << 4U) | HexNibble(low));
  }
  return true;
}

bool ParseUint64(std::string_view text, std::uint64_t& output) noexcept {
  if (text.empty() || (text.size() > 1U && text.front() == '0')) {
    return false;
  }
  std::uint64_t value = 0U;
  for (const char digit : text) {
    if (digit < '0' || digit > '9') {
      return false;
    }
    const std::uint64_t part = static_cast<std::uint64_t>(digit - '0');
    if (value > (std::numeric_limits<std::uint64_t>::max() - part) / 10U) {
      return false;
    }
    value = value * 10U + part;
  }
  output = value;
  return true;
}

bool TextEquals(std::string_view left, std::string_view right) noexcept {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

std::size_t FindPipeline(const PlanBundle& plan, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < plan.Pipelines().size(); ++index) {
    if (TextEquals(plan.Pipelines()[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindMessage(const PlanBundle& plan, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < plan.Messages().size(); ++index) {
    if (TextEquals(plan.Messages()[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindField(const protocol_plan::FrozenMessagePlan& message,
                      std::string_view id) noexcept {
  for (std::size_t index = 0U; index < message.fields.size(); ++index) {
    if (TextEquals(message.fields[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindEnumEntry(const protocol_plan::FrozenFieldPlan& field,
                          std::string_view id) noexcept {
  for (std::size_t index = 0U; index < field.enum_entries.size(); ++index) {
    if (TextEquals(field.enum_entries[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

void CopyDecodedFields(const PlanBundle& plan, std::size_t message_index,
                       const std::vector<DecodedFieldSlot>& slots, std::size_t count,
                       std::vector<FieldResult>& output) {
  const auto& message = plan.Messages()[message_index];
  output.clear();
  output.reserve(count);
  for (std::size_t index = 0U; index < count; ++index) {
    const auto& field = message.fields[index];
    const auto& slot = slots[index];
    FieldResult item;
    item.id.assign(field.id.data(), field.id.size());
    if (slot.value_kind == LogicalValueKind::UINT64) {
      item.kind = "UINT64";
      item.raw_value = std::to_string(slot.uint64_value);
      item.logical_value = item.raw_value;
    } else if (slot.value_kind == LogicalValueKind::BYTES) {
      item.kind = "BYTES";
      item.raw_value = HexUpper(slot.bytes_value.data, slot.bytes_value.size);
      item.logical_value = item.raw_value;
    } else if (slot.value_kind == LogicalValueKind::BOOL) {
      item.kind = "BOOL";
      item.raw_value = slot.bool_value ? "1" : "0";
      item.logical_value = slot.bool_value ? "true" : "false";
    } else {
      item.kind = "ENUM";
      item.raw_value = std::to_string(slot.enum_value.raw_value);
      item.enum_known = slot.enum_value.known;
      if (slot.enum_value.known) {
        const auto& entry = field.enum_entries[slot.enum_value.reference.entry_index];
        item.logical_value.assign(entry.id.data(), entry.id.size());
      } else {
        item.logical_value = item.raw_value;
      }
    }
    output.push_back(std::move(item));
  }
}

}  // namespace

void DocumentDeleter::operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }

bool ReadFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
              std::string& error) {
  std::error_code file_error;
  const std::uintmax_t size = std::filesystem::file_size(path, file_error);
  if (file_error) {
    error = "cannot read file size: " + file_error.message();
    return false;
  }
  if (size > kMaximumInputBytes || size > std::numeric_limits<std::size_t>::max()) {
    error = "input exceeds the 16 MiB Lab hard limit";
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    error = "cannot open input file";
    return false;
  }
  output.resize(static_cast<std::size_t>(size));
  if (!output.empty()) {
    stream.read(reinterpret_cast<char*>(output.data()),
                static_cast<std::streamsize>(output.size()));
  }
  if (!stream || stream.peek() != std::ifstream::traits_type::eof()) {
    error = "input file read was incomplete or changed while reading";
    return false;
  }
  return true;
}

bool ReadText(const std::filesystem::path& path, std::string& output, std::string& error) {
  std::vector<std::uint8_t> bytes;
  if (!ReadFile(path, bytes, error)) {
    return false;
  }
  output.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  return true;
}

bool ParseFrameHex(std::string_view text, std::vector<std::uint8_t>& output, std::string& error) {
  std::string compact;
  compact.reserve(text.size());
  for (const char value : text) {
    if (IsAsciiWhitespace(value)) {
      continue;
    }
    if (HexNibble(value) < 0) {
      error = "frame Hex permits only hexadecimal digits and ASCII whitespace";
      return false;
    }
    compact.push_back(value);
  }
  if (compact.empty() || compact.size() % 2U != 0U) {
    error = "frame Hex must contain a non-empty even number of digits";
    return false;
  }
  output.resize(compact.size() / 2U);
  for (std::size_t index = 0U; index < output.size(); ++index) {
    output[index] = static_cast<std::uint8_t>((HexNibble(compact[index * 2U]) << 4U) |
                                              HexNibble(compact[index * 2U + 1U]));
  }
  return true;
}

bool ParseDocument(std::string& text, DocumentPtr& output, std::string& error) {
  yyjson_read_err parse_error{};
  output.reset(yyjson_read_opts(text.data(), text.size(), 0U, nullptr, &parse_error));
  if (!output) {
    error = "JSON syntax error at byte " + std::to_string(parse_error.pos);
    if (parse_error.msg != nullptr) {
      error += ": ";
      error += parse_error.msg;
    }
    return false;
  }
  return true;
}

bool ValidateObject(yyjson_val* object, const std::set<std::string_view>& allowed,
                    const std::set<std::string_view>& required, std::string_view pointer,
                    std::string& error) {
  if (!yyjson_is_obj(object)) {
    error = std::string(pointer) + " must be an object";
    return false;
  }
  std::set<std::string> seen;
  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(object, &iterator);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string name{yyjson_get_str(key), yyjson_get_len(key)};
    if (!seen.insert(name).second) {
      error = std::string(pointer) + " contains duplicate property " + name;
      return false;
    }
    if (allowed.find(name) == allowed.end()) {
      error = std::string(pointer) + " contains unknown property " + name;
      return false;
    }
  }
  for (const std::string_view name : required) {
    if (seen.find(std::string{name}) == seen.end()) {
      error = std::string(pointer) + " is missing property " + std::string(name);
      return false;
    }
  }
  return true;
}

bool ReadJsonString(yyjson_val* object, const char* name, std::string& output, std::string& error) {
  yyjson_val* value = yyjson_obj_get(object, name);
  if (!yyjson_is_str(value)) {
    error = std::string(name) + " must be a string";
    return false;
  }
  output.assign(yyjson_get_str(value), yyjson_get_len(value));
  return true;
}

bool ParseValues(std::string& text, ParsedValues& output, std::string& error) {
  DocumentPtr document;
  if (!ParseDocument(text, document, error)) {
    return false;
  }
  yyjson_val* root = yyjson_doc_get_root(document.get());
  const std::set<std::string_view> root_keys{"format_version", "pipeline_id", "message_id",
                                             "fields"};
  if (!ValidateObject(root, root_keys, root_keys, "values", error)) {
    return false;
  }
  std::string format;
  if (!ReadJsonString(root, "format_version", format, error) ||
      (format != kValuesFormat && format != kValuesFormatV2) ||
      !ReadJsonString(root, "pipeline_id", output.pipeline_id, error) ||
      !ReadJsonString(root, "message_id", output.message_id, error)) {
    if (error.empty()) {
      error = "unsupported values format_version";
    }
    return false;
  }
  output.format_version = format;
  yyjson_val* fields = yyjson_obj_get(root, "fields");
  if (!yyjson_is_arr(fields)) {
    error = "fields must be an array";
    return false;
  }
  std::set<std::string> seen_fields;
  std::size_t ordinal = 0U;
  yyjson_arr_iter iterator;
  yyjson_arr_iter_init(fields, &iterator);
  while (yyjson_val* value = yyjson_arr_iter_next(&iterator)) {
    const std::string pointer = "fields[" + std::to_string(ordinal) + "]";
    const std::set<std::string_view> allowed{"id", "kind", "uint64", "hex", "entry_id", "bool"};
    const std::set<std::string_view> required{"id", "kind"};
    if (!ValidateObject(value, allowed, required, pointer, error)) {
      return false;
    }
    ParsedValue parsed;
    if (!ReadJsonString(value, "id", parsed.id, error) ||
        !ReadJsonString(value, "kind", parsed.kind, error)) {
      return false;
    }
    if (!seen_fields.insert(parsed.id).second) {
      error = "fields contains duplicate id " + parsed.id;
      return false;
    }
    if (parsed.kind == "UINT64") {
      const std::set<std::string_view> exact{"id", "kind", "uint64"};
      std::string decimal;
      if (!ValidateObject(value, exact, exact, pointer, error) ||
          !ReadJsonString(value, "uint64", decimal, error) ||
          !ParseUint64(decimal, parsed.uint64_value)) {
        if (error.empty()) {
          error = pointer + ".uint64 is not canonical UINT64 decimal";
        }
        return false;
      }
    } else if (parsed.kind == "BYTES") {
      const std::set<std::string_view> exact{"id", "kind", "hex"};
      std::string hex;
      if (!ValidateObject(value, exact, exact, pointer, error) ||
          !ReadJsonString(value, "hex", hex, error) || !ParseUpperHex(hex, parsed.bytes, error)) {
        return false;
      }
    } else if (parsed.kind == "ENUM") {
      const std::set<std::string_view> exact{"id", "kind", "entry_id"};
      if (!ValidateObject(value, exact, exact, pointer, error) ||
          !ReadJsonString(value, "entry_id", parsed.enum_entry_id, error)) {
        return false;
      }
    } else if (parsed.kind == "BOOL") {
      const std::set<std::string_view> exact{"id", "kind", "bool"};
      yyjson_val* boolean = yyjson_obj_get(value, "bool");
      if (format != kValuesFormatV2 || !ValidateObject(value, exact, exact, pointer, error) ||
          !yyjson_is_bool(boolean)) {
        if (error.empty()) error = pointer + ".bool must be a native JSON boolean in Values 0.2";
        return false;
      }
      parsed.bool_value = yyjson_get_bool(boolean);
    } else {
      error = pointer + ".kind is unsupported";
      return false;
    }
    output.fields.push_back(std::move(parsed));
    ++ordinal;
  }
  return true;
}

OperationResult InspectFrame(const PlanBundle& plan, const std::vector<std::uint8_t>& frame,
                             InspectFrameExecutionCounts* execution_counts) {
  OperationResult result;
  if (execution_counts != nullptr) {
    *execution_counts = InspectFrameExecutionCounts{};
  }
  result.schema_version.assign(plan.SchemaVersion().data(), plan.SchemaVersion().size());
  result.operation_kind = "inspect";
  result.protocol_id.assign(plan.ProtocolId().data(), plan.ProtocolId().size());
  result.frame = frame;
  const bool integrity_generation = plan.SchemaVersion() == "0.3";
  if (integrity_generation) {
    std::size_t selected_pipeline_index = kInvalidIndex;
    std::size_t selected_message_index = kInvalidIndex;
    std::size_t structural_candidate_count = 0U;
    for (std::size_t pipeline_index = 0U; pipeline_index < plan.Pipelines().size();
         ++pipeline_index) {
      if (execution_counts != nullptr) {
        ++execution_counts->structural_query_calls;
      }
      const auto matched = protocol_core::internal::MatchCompleteRecordStructure(
          plan, pipeline_index, ByteView{frame.data(), frame.size()});
      if (matched.status == CodecStatus::AMBIGUOUS_MESSAGE) {
        structural_candidate_count = 2U;
        break;
      }
      if (matched.status != CodecStatus::OK) {
        continue;
      }
      ++structural_candidate_count;
      if (structural_candidate_count == 1U) {
        selected_pipeline_index = pipeline_index;
        selected_message_index = matched.message_index;
      }
    }

    if (structural_candidate_count == 0U) {
      result.status = "UNKNOWN_MESSAGE";
    } else if (structural_candidate_count > 1U) {
      result.status = "AMBIGUOUS_MESSAGE";
    } else {
      const auto& pipeline = plan.Pipelines()[selected_pipeline_index];
      const auto& message = plan.Messages()[selected_message_index];
      result.pipeline_id.assign(pipeline.id.data(), pipeline.id.size());
      result.message_id.assign(message.id.data(), message.id.size());
      result.direction_id.assign(message.direction_id.data(), message.direction_id.size());

      std::vector<DecodedFieldSlot> slots(plan.GetExecutionResourceLayout().max_fields_per_message);
      ExecutionWorkspace workspace{plan};
      if (execution_counts != nullptr) {
        ++execution_counts->decode_calls;
      }
      const auto decoded =
          DecodeCompleteRecord(plan, workspace, selected_pipeline_index,
                               ByteView{frame.data(), frame.size()}, slots.data(), slots.size());
      result.status = CodecStatusName(decoded.status);
      if (decoded.status == CodecStatus::OK) {
        CopyDecodedFields(plan, decoded.message_index, slots, decoded.field_count, result.fields);
      }
    }

    if (result.status != "OK") {
      result.fields.clear();
      result.diagnostic_id = "PAE_LAB_CODEC_" + result.status;
      result.diagnostic_detail = "frame did not produce one unique successful Pipeline match";
    }
    result.replay_mode = "DECODE_RX";
    result.replay_subject = "RX";
    result.current_execution_status = result.status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
    return result;
  }

  std::size_t success_count = 0U;
  CodecStatus selected_failure = CodecStatus::UNKNOWN_MESSAGE;
  for (std::size_t pipeline_index = 0U; pipeline_index < plan.Pipelines().size();
       ++pipeline_index) {
    std::vector<DecodedFieldSlot> slots(plan.GetExecutionResourceLayout().max_fields_per_message);
    ExecutionWorkspace workspace{plan};
    const auto decoded =
        DecodeCompleteRecord(plan, workspace, pipeline_index, ByteView{frame.data(), frame.size()},
                             slots.data(), slots.size());
    if (decoded.status == CodecStatus::OK) {
      ++success_count;
      if (success_count == 1U) {
        result.status = "OK";
        const auto& pipeline = plan.Pipelines()[pipeline_index];
        const auto& message = plan.Messages()[decoded.message_index];
        result.pipeline_id.assign(pipeline.id.data(), pipeline.id.size());
        result.message_id.assign(message.id.data(), message.id.size());
        result.direction_id.assign(message.direction_id.data(), message.direction_id.size());
        CopyDecodedFields(plan, decoded.message_index, slots, decoded.field_count, result.fields);
      }
    } else if (selected_failure == CodecStatus::UNKNOWN_MESSAGE &&
               decoded.status != CodecStatus::UNKNOWN_MESSAGE) {
      selected_failure = decoded.status;
    }
  }
  if (success_count > 1U) {
    result.status = "AMBIGUOUS_MESSAGE";
    result.pipeline_id.clear();
    result.message_id.clear();
    result.direction_id.clear();
    result.fields.clear();
  } else if (success_count == 0U) {
    result.status = CodecStatusName(selected_failure);
  }
  if (result.status != "OK") {
    result.diagnostic_id = "PAE_LAB_CODEC_" + result.status;
    result.diagnostic_detail = "frame did not produce one unique successful Pipeline match";
  }
  if (result.schema_version != "0.1") {
    result.replay_mode = "DECODE_RX";
    result.replay_subject = "RX";
    result.current_execution_status = result.status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
  }
  return result;
}

OperationResult InspectFrameInPipeline(const PlanBundle& plan, std::string_view pipeline_id,
                                       const std::vector<std::uint8_t>& frame) {
  OperationResult result;
  result.schema_version.assign(plan.SchemaVersion().data(), plan.SchemaVersion().size());
  result.operation_kind = "inspect";
  result.protocol_id.assign(plan.ProtocolId().data(), plan.ProtocolId().size());
  result.pipeline_id.assign(pipeline_id.data(), pipeline_id.size());
  result.frame = frame;

  const std::size_t pipeline_index = FindPipeline(plan, pipeline_id);
  if (pipeline_index == kInvalidIndex) {
    result.status = "INVALID_ARGUMENT";
    result.diagnostic_id = "PAE_LAB_UNKNOWN_RECEIVE_PIPELINE";
    result.diagnostic_detail = "receive pipeline id is unknown";
    return result;
  }

  std::vector<DecodedFieldSlot> slots(plan.GetExecutionResourceLayout().max_fields_per_message);
  ExecutionWorkspace workspace{plan};
  const auto decoded =
      DecodeCompleteRecord(plan, workspace, pipeline_index, ByteView{frame.data(), frame.size()},
                           slots.data(), slots.size());
  result.status = CodecStatusName(decoded.status);
  if (decoded.status == CodecStatus::OK) {
    const auto& message = plan.Messages()[decoded.message_index];
    result.message_id.assign(message.id.data(), message.id.size());
    result.direction_id.assign(message.direction_id.data(), message.direction_id.size());
    CopyDecodedFields(plan, decoded.message_index, slots, decoded.field_count, result.fields);
  } else {
    result.diagnostic_id = "PAE_LAB_CODEC_" + result.status;
    result.diagnostic_detail = "frame failed in the selected receive Pipeline";
  }
  if (result.schema_version != "0.1") {
    result.replay_mode = "DECODE_RX";
    result.replay_subject = "RX";
    result.current_execution_status = result.status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
  }
  return result;
}

OperationResult EncodeValues(const PlanBundle& plan, const ParsedValues& parsed,
                             std::string& error) {
  OperationResult result;
  result.schema_version.assign(plan.SchemaVersion().data(), plan.SchemaVersion().size());
  result.operation_kind = "encode";
  result.protocol_id.assign(plan.ProtocolId().data(), plan.ProtocolId().size());
  result.pipeline_id = parsed.pipeline_id;
  result.message_id = parsed.message_id;
  const std::size_t pipeline_index = FindPipeline(plan, parsed.pipeline_id);
  const std::size_t message_index = FindMessage(plan, parsed.message_id);
  if (pipeline_index == kInvalidIndex || message_index == kInvalidIndex) {
    result.status = "VALUES_INVALID";
    result.diagnostic_id = "PAE_LAB_VALUES_UNKNOWN_BINDING";
    result.diagnostic_detail = "values pipeline_id or message_id is unknown";
    return result;
  }
  const auto& message = plan.Messages()[message_index];
  result.direction_id.assign(message.direction_id.data(), message.direction_id.size());
  std::vector<std::vector<std::uint8_t>> byte_storage;
  std::vector<EncodeFieldValue> values;
  byte_storage.reserve(parsed.fields.size());
  values.reserve(parsed.fields.size());
  for (const ParsedValue& input : parsed.fields) {
    const std::size_t field_index = FindField(message, input.id);
    if (field_index == kInvalidIndex) {
      result.status = "VALUES_INVALID";
      result.diagnostic_id = "PAE_LAB_VALUES_UNKNOWN_FIELD";
      result.diagnostic_detail = "unknown field id " + input.id;
      return result;
    }
    const auto& field = message.fields[field_index];
    if (field.encode_source != EncodeSource::INPUT) {
      result.status = "CONSTANT_FIELD_OVERRIDE";
      result.diagnostic_id = "PAE_LAB_VALUES_CONSTANT_OVERRIDE";
      result.diagnostic_detail = "values cannot override constant field " + input.id;
      return result;
    }
    EncodeFieldValue value;
    value.field = protocol_core::FieldRef{&plan, message_index, field_index};
    if (input.kind == "UINT64" && field.value_type == ValueType::UINT64) {
      value.value_kind = LogicalValueKind::UINT64;
      value.uint64_value = input.uint64_value;
    } else if (input.kind == "BYTES" && field.value_type == ValueType::BYTES) {
      byte_storage.push_back(input.bytes);
      value.value_kind = LogicalValueKind::BYTES;
      value.bytes_value = ByteView{byte_storage.back().data(), byte_storage.back().size()};
    } else if (input.kind == "ENUM" && field.value_type == ValueType::ENUM) {
      const std::size_t entry_index = FindEnumEntry(field, input.enum_entry_id);
      if (entry_index == kInvalidIndex) {
        result.status = "VALUES_INVALID";
        result.diagnostic_id = "PAE_LAB_VALUES_UNKNOWN_ENUM_ENTRY";
        result.diagnostic_detail = "unknown enum entry " + input.enum_entry_id;
        return result;
      }
      value.value_kind = LogicalValueKind::ENUM;
      value.enum_value =
          protocol_core::EnumValueRef{&plan, message_index, field_index, entry_index};
    } else if (input.kind == "BOOL" && field.value_type == ValueType::BOOL) {
      value.value_kind = LogicalValueKind::BOOL;
      value.bool_value = input.bool_value;
    } else {
      result.status = "TYPE_MISMATCH";
      result.diagnostic_id = "PAE_LAB_VALUES_TYPE_MISMATCH";
      result.diagnostic_detail = "values kind does not match Plan field " + input.id;
      return result;
    }
    values.push_back(value);
  }

  result.frame.assign(static_cast<std::size_t>(message.frame_length_bytes), 0U);
  ExecutionWorkspace workspace{plan};
  const auto encoded = EncodeCompleteRecord(
      plan, workspace, pipeline_index, message_index, values.data(), values.size(),
      MutableByteBuffer{result.frame.data(), result.frame.size()});
  result.status = CodecStatusName(encoded.status);
  if (encoded.status != CodecStatus::OK) {
    result.frame.clear();
    result.diagnostic_id = "PAE_LAB_CODEC_" + result.status;
    result.diagnostic_detail = "EncodeCompleteRecord rejected the values";
    if (result.schema_version != "0.1") {
      result.replay_mode = "ENCODE_TX";
      result.replay_subject = "TX";
      result.current_execution_status = result.status;
      result.current_execution_diagnostic_id = result.diagnostic_id;
    }
    return result;
  }

  std::vector<DecodedFieldSlot> slots(plan.GetExecutionResourceLayout().max_fields_per_message);
  ExecutionWorkspace review_workspace{plan};
  const auto reviewed = DecodeCompleteRecord(plan, review_workspace, pipeline_index,
                                             ByteView{result.frame.data(), result.frame.size()},
                                             slots.data(), slots.size());
  if (reviewed.status != CodecStatus::OK || reviewed.message_index != message_index) {
    result.status = "FINAL_REVIEW_FAILED";
    result.frame.clear();
    result.diagnostic_id = "PAE_LAB_ENCODE_REVIEW_FAILED";
    result.diagnostic_detail = "encoded bytes did not decode to the selected message";
    return result;
  }
  CopyDecodedFields(plan, message_index, slots, reviewed.field_count, result.fields);
  if (result.schema_version != "0.1") {
    result.replay_mode = "ENCODE_TX";
    result.replay_subject = "TX";
    result.current_execution_status = result.status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
  }
  error.clear();
  return result;
}

bool LoadFrameArgument(const std::filesystem::path& binary, const std::filesystem::path& hex,
                       std::vector<std::uint8_t>& output, std::string& error) {
  if (!binary.empty()) {
    return ReadFile(binary, output, error) && !output.empty();
  }
  std::string text;
  return ReadText(hex, text, error) && ParseFrameHex(text, output, error);
}

bool CompileConfig(const std::filesystem::path& path, std::string& text,
                   protocol_plan::PlanOwner& plan, OperationResult& result, std::string& error) {
  if (!ReadText(path, text, error)) {
    return false;
  }
  result.config_sha256 = HashBytes(text);
  auto compiled = config_compiler::CompileJsonToPlan(text);
  if (!compiled.Succeeded()) {
    const auto* diagnostic = compiled.Diagnostic();
    result.status = "CONFIG_COMPILE_FAILED";
    result.diagnostic_id = "PAE_LAB_CONFIG_COMPILE_FAILED";
    if (diagnostic != nullptr) {
      result.diagnostic_detail = diagnostic->json_pointer + ": " + diagnostic->detail;
    }
    return true;
  }
  plan = std::move(compiled).TakePlan();
  return true;
}

}  // namespace pae::protocol_lab
