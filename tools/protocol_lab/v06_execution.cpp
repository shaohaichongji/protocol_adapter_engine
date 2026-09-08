#include "v06_execution.h"

#include <algorithm>
#include <limits>
#include <type_traits>
#include <utility>

#include "complete_record_codec.h"
#include "config_compiler.h"
#include "sha256.h"
#include "v06_values_compat_internal.h"

namespace pae::protocol_lab::v06 {
namespace {

using protocol_core::ByteView;
using protocol_core::CodecStatus;
using protocol_core::ConversionError;
using protocol_core::DecodedFieldSlot;
using protocol_core::EncodeFieldValue;
using protocol_core::ExecutionWorkspace;
using protocol_core::kInvalidIndex;
using protocol_core::LogicalValueKind;
using protocol_core::MutableByteBuffer;
using protocol_core::RawIntegerKind;
using protocol_core::RawIntegerValue;
using protocol_plan::FrozenFieldPlan;
using protocol_plan::FrozenMessagePlan;
using protocol_plan::PlanBundle;
using protocol_plan::ValueType;

bool TextEquals(std::string_view left, std::string_view right) noexcept {
  return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
}

std::string CopyText(std::string_view value) { return std::string{value.data(), value.size()}; }

std::string HexUpper(const std::uint8_t* data, std::size_t size) {
  constexpr char kDigits[] = "0123456789ABCDEF";
  std::string output(size * 2U, '0');
  for (std::size_t index = 0U; index < size; ++index) {
    output[index * 2U] = kDigits[data[index] >> 4U];
    output[index * 2U + 1U] = kDigits[data[index] & 0x0FU];
  }
  return output;
}

std::string CodecStatusName(CodecStatus status) {
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
    case CodecStatus::INTERNAL_ERROR:
      return "INTERNAL_ERROR";
  }
  return "INTERNAL_ERROR";
}

std::optional<std::string> ConversionErrorName(ConversionError error) {
  switch (error) {
    case ConversionError::NONE:
      return std::nullopt;
    case ConversionError::DECIMAL_SCALE_OUT_OF_RANGE:
      return "DECIMAL_SCALE_OUT_OF_RANGE";
    case ConversionError::RAW_NOT_INTEGRAL:
      return "RAW_NOT_INTEGRAL";
    case ConversionError::RAW_OUT_OF_RANGE:
      return "RAW_OUT_OF_RANGE";
    case ConversionError::LOGICAL_OUT_OF_RANGE:
      return "LOGICAL_OUT_OF_RANGE";
  }
  return std::nullopt;
}

std::size_t FindPipeline(const PlanBundle& plan, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < plan.Pipelines().size(); ++index) {
    if (TextEquals(plan.Pipelines()[index].id, id)) return index;
  }
  return kInvalidIndex;
}

std::size_t FindMessage(const PlanBundle& plan, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < plan.Messages().size(); ++index) {
    if (TextEquals(plan.Messages()[index].id, id)) return index;
  }
  return kInvalidIndex;
}

bool PipelineAllowsMessage(const PlanBundle& plan, std::size_t pipeline_index,
                           std::size_t message_index) noexcept {
  if (pipeline_index >= plan.Pipelines().size()) return false;
  const auto& indices = plan.Pipelines()[pipeline_index].message_indices;
  return std::find(indices.begin(), indices.end(), message_index) != indices.end();
}

std::size_t FindField(const FrozenMessagePlan& message, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < message.fields.size(); ++index) {
    if (TextEquals(message.fields[index].id, id)) return index;
  }
  return kInvalidIndex;
}

std::size_t FindEnumEntry(const FrozenFieldPlan& field, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < field.enum_entries.size(); ++index) {
    if (TextEquals(field.enum_entries[index].id, id)) return index;
  }
  return kInvalidIndex;
}

bool HasConversion(const PlanBundle& plan, std::size_t message_index,
                   std::size_t field_index) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  if (message_index >= plan.MessageExecutionPlans().size() ||
      field_index >= plan.MessageExecutionPlans()[message_index].fields.size())
    return false;
  return plan.MessageExecutionPlans()[message_index].fields[field_index].conversion_index !=
         kInvalidIndex;
#else
  (void)plan;
  (void)message_index;
  (void)field_index;
  return false;
#endif
}

void SetPreparationFailure(ExecutionOutcome& outcome, std::string id, std::string detail,
                           std::optional<std::size_t> value_index = std::nullopt) {
  outcome.stage = ExecutionStage::PREPARATION;
  outcome.preparation_failure.diagnostic_id = std::move(id);
  outcome.preparation_failure.detail = std::move(detail);
  outcome.preparation_failure.value_index = value_index;
}

Result MakeBaseResult(const PlanBundle& plan, std::string_view config_hash,
                      std::string_view operation_kind, std::size_t pipeline_index,
                      std::size_t message_index) {
  Result result;
  result.command = std::string{operation_kind};
  result.operation_kind = std::string{operation_kind};
  result.config_sha256 = std::string{config_hash};
  result.protocol_id = CopyText(plan.ProtocolId());
  if (pipeline_index < plan.Pipelines().size()) {
    result.pipeline_id = CopyText(plan.Pipelines()[pipeline_index].id);
  }
  if (message_index < plan.Messages().size()) {
    const auto& message = plan.Messages()[message_index];
    result.message_id = CopyText(message.id);
    result.direction_id = CopyText(message.direction_id);
  }
  if (operation_kind == "encode") {
    result.replay_mode = "ENCODE_TX";
    result.replay_subject = "TX";
  } else {
    result.replay_mode = "DECODE_RX";
    result.replay_subject = "RX";
  }
  return result;
}

template <typename CoreResult>
bool SetCodecResult(const PlanBundle& plan, std::string_view config_hash,
                    std::string_view operation_kind, std::size_t pipeline_index,
                    std::size_t message_index, const CoreResult& core_result,
                    const std::optional<std::string>& frame_hex, Result& result,
                    std::string& error) {
  result = MakeBaseResult(plan, config_hash, operation_kind, pipeline_index, message_index);
  result.frame_hex = frame_hex;
  const std::string status = CodecStatusName(core_result.status);
  result.current_execution_status = status;
  if (core_result.status == CodecStatus::OK) {
    result.operation_status = "OK";
    result.exit_code = 0;
  } else {
    result.operation_status = "CODEC_ERROR";
    result.exit_code = 5;
    result.diagnostic_id = "PAE_LAB_CODEC_" + status;
    result.current_execution_diagnostic_id = result.diagnostic_id;
    result.diagnostic_detail = std::string{operation_kind} + " Core execution failed";
    result.conversion_error = ConversionErrorName(core_result.conversion_error);
    if (core_result.failed_field_index != kInvalidIndex && message_index < plan.Messages().size() &&
        core_result.failed_field_index < plan.Messages()[message_index].fields.size()) {
      result.failed_field_index = core_result.failed_field_index;
      result.failed_field_id =
          CopyText(plan.Messages()[message_index].fields[core_result.failed_field_index].id);
    }
  }
  if constexpr (std::is_same_v<CoreResult, protocol_core::EncodeResult>) {
    if (core_result.status != CodecStatus::OK && core_result.failed_value_index != kInvalidIndex) {
      result.failed_value_index = core_result.failed_value_index;
    }
  }
  return ValidateResult(result, error);
}

MaterializationFailure MaterializeFields(const PlanBundle& plan, ExecutionWorkspace& workspace,
                                         std::size_t message_index,
                                         const std::vector<DecodedFieldSlot>& slots,
                                         std::size_t field_count, std::vector<FieldResult>& output
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                         ,
                                         const ExecutionTestHooks* hooks
#endif
) {
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  if (hooks != nullptr && hooks->force_materialization_internal_error) {
    return MaterializationFailure::INTERNAL_ERROR;
  }
  if (hooks != nullptr && hooks->force_raw_association_failure) {
    return MaterializationFailure::RAW_ASSOCIATION_FAILED;
  }
#endif
  if (message_index >= plan.Messages().size() || field_count != slots.size() ||
      field_count != plan.Messages()[message_index].fields.size()) {
    return MaterializationFailure::INTERNAL_ERROR;
  }
  const auto& message = plan.Messages()[message_index];
  std::vector<std::optional<RawIntegerValue>> raw_by_field(message.fields.size());
  for (std::size_t raw_index = 0U; raw_index < workspace.LastRawIntegerCount(); ++raw_index) {
    RawIntegerValue raw;
    if (!workspace.GetLastRawInteger(raw_index, raw) || raw.field.plan_scope != &plan ||
        raw.field.message_index != message_index ||
        raw.field.field_index >= message.fields.size() ||
        raw_by_field[raw.field.field_index].has_value()) {
      return MaterializationFailure::RAW_ASSOCIATION_FAILED;
    }
    raw_by_field[raw.field.field_index] = raw;
  }

  std::vector<FieldResult> candidate;
  candidate.reserve(field_count);
  for (std::size_t index = 0U; index < field_count; ++index) {
    const auto& field = message.fields[index];
    const auto& slot = slots[index];
    if (slot.field.plan_scope != &plan || slot.field.message_index != message_index ||
        slot.field.field_index != index) {
      return MaterializationFailure::INTERNAL_ERROR;
    }
    FieldResult item;
    item.id = CopyText(field.id);
    if (HasConversion(plan, message_index, index)) {
      if (slot.value_kind != LogicalValueKind::DECIMAL64 || !raw_by_field[index].has_value()) {
        return MaterializationFailure::RAW_ASSOCIATION_FAILED;
      }
      const RawIntegerValue& raw = *raw_by_field[index];
      item.kind = "DECIMAL64";
      item.decimal64 = NormalizeDecimal64(
          Decimal64{slot.decimal64_value.coefficient, slot.decimal64_value.scale});
      if (field.value_type == ValueType::INT64 && raw.kind == RawIntegerKind::INT64) {
        item.raw_kind = "INT64";
        item.raw_value = std::to_string(raw.int64_value);
      } else if (field.value_type == ValueType::UINT64 && raw.kind == RawIntegerKind::UINT64) {
        item.raw_kind = "UINT64";
        item.raw_value = std::to_string(raw.uint64_value);
      } else {
        return MaterializationFailure::RAW_ASSOCIATION_FAILED;
      }
    } else if (raw_by_field[index].has_value()) {
      return MaterializationFailure::RAW_ASSOCIATION_FAILED;
    } else if (slot.value_kind == LogicalValueKind::UINT64 &&
               field.value_type == ValueType::UINT64) {
      item.kind = "UINT64";
      item.raw_value = std::to_string(slot.uint64_value);
      item.logical_value = item.raw_value;
    } else if (slot.value_kind == LogicalValueKind::INT64 && field.value_type == ValueType::INT64) {
      item.kind = "INT64";
      item.raw_value = std::to_string(slot.int64_value);
      item.logical_value = item.raw_value;
    } else if (slot.value_kind == LogicalValueKind::BYTES && field.value_type == ValueType::BYTES) {
      item.kind = "BYTES";
      item.raw_value = HexUpper(slot.bytes_value.data, slot.bytes_value.size);
      item.logical_value = item.raw_value;
    } else if (slot.value_kind == LogicalValueKind::BOOL && field.value_type == ValueType::BOOL) {
      item.kind = "BOOL";
      item.raw_value = slot.bool_value ? "1" : "0";
      item.logical_value = slot.bool_value ? "true" : "false";
    } else if (slot.value_kind == LogicalValueKind::ENUM && field.value_type == ValueType::ENUM) {
      item.kind = "ENUM";
      item.raw_value = std::to_string(slot.enum_value.raw_value);
      item.enum_known = slot.enum_value.known;
      if (slot.enum_value.known) {
        const auto& reference = slot.enum_value.reference;
        if (reference.plan_scope != &plan || reference.message_index != message_index ||
            reference.field_index != index || reference.entry_index >= field.enum_entries.size()) {
          return MaterializationFailure::INTERNAL_ERROR;
        }
        item.logical_value = CopyText(field.enum_entries[reference.entry_index].id);
      } else {
        item.logical_value = item.raw_value;
      }
    } else {
      return MaterializationFailure::INTERNAL_ERROR;
    }
    candidate.push_back(std::move(item));
  }
  output = std::move(candidate);
  return MaterializationFailure::NONE;
}

}  // namespace

struct ExecutionBridge::Impl {
  Impl(std::string text, protocol_plan::PlanOwner owner)
      : config_text(std::move(text)),
        config_hash(protocol_lab::HashBytes(config_text)),
        plan(std::move(owner)),
        main_workspace(*plan),
        review_workspace(*plan) {}

  std::string config_text;
  std::string config_hash;
  protocol_plan::PlanOwner plan;
  ExecutionWorkspace main_workspace;
  ExecutionWorkspace review_workspace;
};

ExecutionBridge::ExecutionBridge(std::unique_ptr<Impl> implementation) noexcept
    : implementation_(std::move(implementation)) {}

ExecutionBridge::~ExecutionBridge() = default;

std::unique_ptr<ExecutionBridge> ExecutionBridge::Prepare(std::string_view config_text,
                                                          PreparationFailure& failure) {
  failure = PreparationFailure{};
  auto compiled = config_compiler::CompileJsonToPlan(config_text);
  if (!compiled.Succeeded()) {
    failure.diagnostic_id = "PAE_LAB_C1_CONFIG_INVALID";
    const auto* diagnostic = compiled.Diagnostic();
    failure.detail = diagnostic == nullptr ? "configuration compilation failed without a diagnostic"
                                           : diagnostic->json_pointer + ": " + diagnostic->detail;
    return nullptr;
  }
  auto plan = std::move(compiled).TakePlan();
  if (plan->SchemaVersion() != "0.5") {
    failure.diagnostic_id = "PAE_LAB_C1_SCHEMA_UNSUPPORTED";
    failure.detail = "C1 execution accepts only a successfully compiled Schema 0.5 Plan";
    return nullptr;
  }
  auto implementation = std::make_unique<Impl>(std::string{config_text}, std::move(plan));
  return std::unique_ptr<ExecutionBridge>{new ExecutionBridge{std::move(implementation)}};
}

ExecutionOutcome ExecutionBridge::Inspect(const std::vector<std::uint8_t>& frame
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                          ,
                                          const ExecutionTestHooks* hooks
#endif
) {
  ExecutionOutcome outcome;
  const PlanBundle& plan = *implementation_->plan;
  std::size_t selected_pipeline = kInvalidIndex;
  std::size_t selected_message = kInvalidIndex;
  std::size_t candidate_count = 0U;
  for (std::size_t pipeline_index = 0U; pipeline_index < plan.Pipelines().size();
       ++pipeline_index) {
    ++outcome.counts.structural_query_calls;
    const auto match = protocol_core::internal::MatchCompleteRecordStructure(
        plan, pipeline_index, ByteView{frame.data(), frame.size()});
    if (match.status == CodecStatus::UNKNOWN_MESSAGE) continue;
    if (match.status == CodecStatus::AMBIGUOUS_MESSAGE) {
      candidate_count = 2U;
      break;
    }
    if (match.status != CodecStatus::OK) {
      outcome.stage = ExecutionStage::STRUCTURAL_QUERY;
      outcome.structural_status = CodecStatusName(match.status);
      return outcome;
    }
    ++candidate_count;
    if (candidate_count == 1U) {
      selected_pipeline = pipeline_index;
      selected_message = match.message_index;
    }
  }
  if (candidate_count != 1U) {
    outcome.stage = ExecutionStage::STRUCTURAL_QUERY;
    outcome.structural_status = candidate_count == 0U ? "UNKNOWN_MESSAGE" : "AMBIGUOUS_MESSAGE";
    return outcome;
  }

  std::vector<DecodedFieldSlot> slots(plan.Messages()[selected_message].fields.size());
  ++outcome.counts.decode_calls;
  outcome.main_codec_called = true;
  const auto decoded = protocol_core::DecodeCompleteRecord(
      plan, implementation_->main_workspace, selected_pipeline,
      ByteView{frame.data(), frame.size()}, slots.data(), slots.size());
  outcome.main_codec_status = CodecStatusName(decoded.status);
  outcome.stage = ExecutionStage::CODEC;
  Result result;
  std::string validation_error;
  if (!SetCodecResult(plan, implementation_->config_hash, "inspect", selected_pipeline,
                      selected_message, decoded, HexUpper(frame.data(), frame.size()), result,
                      validation_error)) {
    outcome.stage = ExecutionStage::MATERIALIZATION;
    outcome.materialization_failure = MaterializationFailure::INTERNAL_ERROR;
    return outcome;
  }
  if (decoded.status != CodecStatus::OK) {
    outcome.result = std::move(result);
    return outcome;
  }
  const MaterializationFailure materialized =
      MaterializeFields(plan, implementation_->main_workspace, selected_message, slots,
                        decoded.field_count, result.fields
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                        ,
                        hooks
#endif
      );
  if (materialized != MaterializationFailure::NONE || !ValidateResult(result, validation_error)) {
    outcome.stage = ExecutionStage::MATERIALIZATION;
    outcome.materialization_failure = materialized == MaterializationFailure::NONE
                                          ? MaterializationFailure::INTERNAL_ERROR
                                          : materialized;
    return outcome;
  }
  outcome.result = std::move(result);
  return outcome;
}

ExecutionOutcome ExecutionBridge::EncodeValuesText(std::string values_text
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                                   ,
                                                   const ExecutionTestHooks* hooks
#endif
) {
  ParsedValues parsed;
  std::string error;
  if (!internal::ParseCompatibleValues(values_text, parsed, error)) {
    ExecutionOutcome outcome;
    SetPreparationFailure(outcome, "PAE_LAB_C1_VALUES_INVALID", std::move(error));
    return outcome;
  }
  return EncodeParsed(parsed
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                      ,
                      hooks
#endif
  );
}

ExecutionOutcome ExecutionBridge::EncodeParsed(const ParsedValues& values
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                                               ,
                                               const ExecutionTestHooks* hooks
#endif
) {
  ExecutionOutcome outcome;
  const PlanBundle& plan = *implementation_->plan;
  const std::size_t pipeline_index = FindPipeline(plan, values.pipeline_id);
  if (pipeline_index == kInvalidIndex) {
    SetPreparationFailure(outcome, "PAE_LAB_C1_UNKNOWN_PIPELINE",
                          "Values pipeline_id is not present in the Plan");
    return outcome;
  }
  const std::size_t message_index = FindMessage(plan, values.message_id);
  if (message_index == kInvalidIndex) {
    SetPreparationFailure(outcome, "PAE_LAB_C1_UNKNOWN_MESSAGE",
                          "Values message_id is not present in the Plan");
    return outcome;
  }
  if (!PipelineAllowsMessage(plan, pipeline_index, message_index)) {
    SetPreparationFailure(outcome, "PAE_LAB_C1_MESSAGE_NOT_ALLOWED",
                          "Values Message is not allowed by the selected Pipeline");
    return outcome;
  }

  const auto& message = plan.Messages()[message_index];
  std::vector<EncodeFieldValue> core_values;
  core_values.reserve(values.fields.size());
  for (std::size_t value_index = 0U; value_index < values.fields.size(); ++value_index) {
    const ParsedValue& input = values.fields[value_index];
    const std::size_t field_index = FindField(message, input.id);
    if (field_index == kInvalidIndex) {
      SetPreparationFailure(outcome, "PAE_LAB_C1_UNKNOWN_FIELD",
                            "Values field id is not present in the selected Message", value_index);
      return outcome;
    }
    const auto& field = message.fields[field_index];
    EncodeFieldValue value;
    value.field = {&plan, message_index, field_index};
    if (input.kind == "UINT64") {
      value.value_kind = LogicalValueKind::UINT64;
      value.uint64_value = input.uint64_value;
    } else if (input.kind == "INT64") {
      value.value_kind = LogicalValueKind::INT64;
      value.int64_value = input.int64_value;
    } else if (input.kind == "BYTES") {
      value.value_kind = LogicalValueKind::BYTES;
      value.bytes_value = ByteView{input.bytes.data(), input.bytes.size()};
    } else if (input.kind == "BOOL") {
      value.value_kind = LogicalValueKind::BOOL;
      value.bool_value = input.bool_value;
    } else if (input.kind == "DECIMAL64") {
      value.value_kind = LogicalValueKind::DECIMAL64;
      value.decimal64_value = {input.decimal64_value.coefficient, input.decimal64_value.scale};
    } else if (input.kind == "ENUM") {
      value.value_kind = LogicalValueKind::ENUM;
      if (field.value_type == ValueType::ENUM) {
        const std::size_t entry_index = FindEnumEntry(field, input.enum_entry_id);
        if (entry_index == kInvalidIndex) {
          SetPreparationFailure(outcome, "PAE_LAB_C1_UNKNOWN_ENUM_ENTRY",
                                "Values enum entry is not present in the target field",
                                value_index);
          return outcome;
        }
        value.enum_value = {&plan, message_index, field_index, entry_index};
      }
    } else {
      SetPreparationFailure(outcome, "PAE_LAB_C1_UNSUPPORTED_VALUE_KIND",
                            "Parsed Values contains an unsupported kind", value_index);
      return outcome;
    }
    core_values.push_back(value);
  }

  std::vector<std::uint8_t> encoded(static_cast<std::size_t>(message.frame_length_bytes), 0U);
  ++outcome.counts.encode_calls;
  outcome.main_codec_called = true;
  const auto encoded_result = protocol_core::EncodeCompleteRecord(
      plan, implementation_->main_workspace, pipeline_index, message_index, core_values.data(),
      core_values.size(), MutableByteBuffer{encoded.data(), encoded.size()});
  outcome.main_codec_status = CodecStatusName(encoded_result.status);
  outcome.stage = ExecutionStage::CODEC;
  Result result;
  std::string validation_error;
  if (encoded_result.status != CodecStatus::OK) {
    if (!SetCodecResult(plan, implementation_->config_hash, "encode", pipeline_index, message_index,
                        encoded_result, std::nullopt, result, validation_error)) {
      outcome.stage = ExecutionStage::MATERIALIZATION;
      outcome.materialization_failure = MaterializationFailure::INTERNAL_ERROR;
      return outcome;
    }
    outcome.result = std::move(result);
    return outcome;
  }

  std::vector<DecodedFieldSlot> slots(message.fields.size());
  std::size_t review_pipeline = pipeline_index;
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  if (hooks != nullptr && hooks->fail_review_decode) review_pipeline = plan.Pipelines().size();
  if (hooks != nullptr && hooks->fail_review_decimal_conversion_with_internal_error) {
    protocol_core::test_only::FailNextDecimalConversionOnce();
  }
#endif
  ++outcome.counts.review_decode_calls;
  outcome.review_decode_called = true;
  const auto reviewed = protocol_core::DecodeCompleteRecord(
      plan, implementation_->review_workspace, review_pipeline,
      ByteView{encoded.data(), encoded.size()}, slots.data(), slots.size());
  outcome.review_decode_status = CodecStatusName(reviewed.status);
  if (reviewed.status != CodecStatus::OK) {
    outcome.stage = ExecutionStage::MATERIALIZATION;
    outcome.materialization_failure = MaterializationFailure::REVIEW_DECODE_FAILED;
    return outcome;
  }
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
  const bool message_mismatch = reviewed.message_index != message_index ||
                                (hooks != nullptr && hooks->force_review_message_mismatch);
#else
  const bool message_mismatch = reviewed.message_index != message_index;
#endif
  if (message_mismatch) {
    outcome.stage = ExecutionStage::MATERIALIZATION;
    outcome.materialization_failure = MaterializationFailure::REVIEW_MESSAGE_MISMATCH;
    return outcome;
  }
  if (!SetCodecResult(plan, implementation_->config_hash, "encode", pipeline_index, message_index,
                      encoded_result, HexUpper(encoded.data(), encoded.size()), result,
                      validation_error)) {
    outcome.stage = ExecutionStage::MATERIALIZATION;
    outcome.materialization_failure = MaterializationFailure::INTERNAL_ERROR;
    return outcome;
  }
  const MaterializationFailure materialized =
      MaterializeFields(plan, implementation_->review_workspace, message_index, slots,
                        reviewed.field_count, result.fields
#if defined(PAE_ENABLE_OPERATION_COUNTERS)
                        ,
                        hooks
#endif
      );
  if (materialized != MaterializationFailure::NONE || !ValidateResult(result, validation_error)) {
    outcome.stage = ExecutionStage::MATERIALIZATION;
    outcome.materialization_failure = materialized == MaterializationFailure::NONE
                                          ? MaterializationFailure::INTERNAL_ERROR
                                          : materialized;
    return outcome;
  }
  outcome.result = std::move(result);
  outcome.encoded_frame = std::move(encoded);
  return outcome;
}

}  // namespace pae::protocol_lab::v06
