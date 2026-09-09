#include "business_adapter.h"

#include <algorithm>
#include <utility>

#include "config_compiler.h"

namespace pae::examples::business_embedding {
namespace {

using protocol_core::kInvalidIndex;
using protocol_plan::PlanBundle;

std::size_t FindPipeline(const PlanBundle& plan, std::string_view id) noexcept {
  const auto& pipelines = plan.Pipelines();
  for (std::size_t index = 0U; index < pipelines.size(); ++index) {
    if (pipelines[index].id.View() == id) return index;
  }
  return kInvalidIndex;
}

std::size_t FindMessage(const PlanBundle& plan, std::string_view id) noexcept {
  const auto& messages = plan.Messages();
  for (std::size_t index = 0U; index < messages.size(); ++index) {
    if (messages[index].id.View() == id) return index;
  }
  return kInvalidIndex;
}

std::size_t FindField(const protocol_plan::FrozenMessagePlan& message,
                      std::string_view id) noexcept {
  for (std::size_t index = 0U; index < message.fields.size(); ++index) {
    if (message.fields[index].id.View() == id) return index;
  }
  return kInvalidIndex;
}

bool PipelineContains(const protocol_plan::FrozenPipelinePlan& pipeline,
                      std::size_t message_index) noexcept {
  return std::find(pipeline.message_indices.begin(), pipeline.message_indices.end(),
                   message_index) != pipeline.message_indices.end();
}

bool IsConvertedField(const PlanBundle& plan, std::size_t message_index,
                      std::size_t field_index) noexcept {
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
  return message_index < plan.MessageExecutionPlans().size() &&
         field_index < plan.MessageExecutionPlans()[message_index].fields.size() &&
         plan.MessageExecutionPlans()[message_index].fields[field_index].conversion_index !=
             kInvalidIndex;
#else
  (void)plan;
  (void)message_index;
  (void)field_index;
  return false;
#endif
}

}  // namespace

std::unique_ptr<BusinessAdapter> BusinessAdapter::Initialize(std::string_view config_text,
                                                             Callbacks callbacks,
                                                             std::string& error_detail) {
  error_detail.clear();
  if (!callbacks.on_measurement || !callbacks.on_bytes_ready) {
    error_detail = "both synchronous business callbacks are required";
    return nullptr;
  }
  auto compiled = config_compiler::CompileJsonToPlan(config_text);
  if (!compiled.Succeeded()) {
    const auto* diagnostic = compiled.Diagnostic();
    error_detail = diagnostic != nullptr ? diagnostic->detail : "configuration compilation failed";
    return nullptr;
  }

  auto plan = std::move(compiled).TakePlan();
  if (!plan || (plan->SchemaVersion() != "0.5" && plan->SchemaVersion() != "0.7")) {
    error_detail = "business embedding requires a supported Schema 0.5 or 0.7 plan";
    return nullptr;
  }

  const std::size_t rx_pipeline = FindPipeline(*plan, "measurement_rx");
  const std::size_t tx_pipeline = FindPipeline(*plan, "command_tx");
  const std::size_t rx_message = FindMessage(*plan, "measurement_record");
  const std::size_t tx_message = FindMessage(*plan, "command_record");
  if (rx_pipeline == kInvalidIndex || tx_pipeline == kInvalidIndex || rx_message == kInvalidIndex ||
      tx_message == kInvalidIndex) {
    error_detail = "required business pipeline or message id is missing";
    return nullptr;
  }
  if (!PipelineContains(plan->Pipelines()[rx_pipeline], rx_message) ||
      !PipelineContains(plan->Pipelines()[tx_pipeline], tx_message) ||
      plan->Messages()[rx_message].direction_id.View() != "rx" ||
      plan->Messages()[tx_message].direction_id.View() != "tx") {
    error_detail = "business pipeline and message directions do not match";
    return nullptr;
  }

  const auto& rx = plan->Messages()[rx_message];
  const auto& tx = plan->Messages()[tx_message];
  const std::size_t temperature = FindField(rx, "temperature");
  const std::size_t alarm = FindField(rx, "alarm");
  const std::size_t target = FindField(tx, "target");
  if (temperature == kInvalidIndex || alarm == kInvalidIndex || target == kInvalidIndex) {
    error_detail = "required business field id is missing";
    return nullptr;
  }
  if (!IsConvertedField(*plan, rx_message, temperature) ||
      rx.fields[alarm].value_type != protocol_plan::ValueType::BOOL ||
      rx.fields[alarm].wire_codec != protocol_plan::WireCodec::BITFIELD ||
      !IsConvertedField(*plan, tx_message, target)) {
    error_detail = "required business field type does not match";
    return nullptr;
  }

  return std::unique_ptr<BusinessAdapter>(
      new BusinessAdapter(std::move(plan), std::move(callbacks), rx_pipeline, rx_message,
                          temperature, alarm, tx_pipeline, tx_message, target));
}

BusinessAdapter::BusinessAdapter(protocol_plan::PlanOwner plan, Callbacks callbacks,
                                 std::size_t rx_pipeline, std::size_t rx_message,
                                 std::size_t rx_temperature, std::size_t rx_alarm,
                                 std::size_t tx_pipeline, std::size_t tx_message,
                                 std::size_t tx_target)
    : plan_(std::move(plan)),
      callbacks_(std::move(callbacks)),
      rx_pipeline_(rx_pipeline),
      rx_message_(rx_message),
      rx_temperature_{plan_.get(), rx_message, rx_temperature},
      rx_alarm_{plan_.get(), rx_message, rx_alarm},
      tx_pipeline_(tx_pipeline),
      tx_message_(tx_message),
      tx_target_{plan_.get(), tx_message, tx_target},
      rx_workspace_(std::make_unique<protocol_core::ExecutionWorkspace>(*plan_)),
      tx_workspace_(std::make_unique<protocol_core::ExecutionWorkspace>(*plan_)),
      rx_slots_(plan_->GetExecutionResourceLayout().max_fields_per_message),
      tx_buffer_(static_cast<std::size_t>(plan_->Messages()[tx_message].frame_length_bytes)) {}

HostResult BusinessAdapter::OnReceivedRecord(protocol_core::ByteView bytes) {
  const auto decoded = protocol_core::DecodeCompleteRecord(
      *plan_, *rx_workspace_, rx_pipeline_, bytes, rx_slots_.data(), rx_slots_.size());
  if (decoded.status != protocol_core::CodecStatus::OK) {
    return {HostStatus::CODEC_ERROR, decoded.status, decoded.conversion_error};
  }
  if (decoded.message_index != rx_message_ || decoded.field_count <= rx_alarm_.field_index ||
      decoded.field_count <= rx_temperature_.field_index) {
    return {HostStatus::CODEC_ERROR, protocol_core::CodecStatus::INTERNAL_ERROR,
            protocol_core::ConversionError::NONE};
  }

  const auto& temperature = rx_slots_[rx_temperature_.field_index];
  const auto& alarm = rx_slots_[rx_alarm_.field_index];
  if (temperature.field.plan_scope != plan_.get() ||
      temperature.field.message_index != rx_message_ ||
      temperature.field.field_index != rx_temperature_.field_index ||
      temperature.value_kind != protocol_core::LogicalValueKind::DECIMAL64 ||
      alarm.field.plan_scope != plan_.get() || alarm.field.message_index != rx_message_ ||
      alarm.field.field_index != rx_alarm_.field_index ||
      alarm.value_kind != protocol_core::LogicalValueKind::BOOL) {
    return {HostStatus::CODEC_ERROR, protocol_core::CodecStatus::INTERNAL_ERROR,
            protocol_core::ConversionError::NONE};
  }

  const Measurement measurement{temperature.decimal64_value, alarm.bool_value};
  if (callbacks_.on_measurement) callbacks_.on_measurement(measurement);
  return {HostStatus::OK, protocol_core::CodecStatus::OK, protocol_core::ConversionError::NONE};
}

HostResult BusinessAdapter::SendCommand(const Command& command) {
  protocol_core::EncodeFieldValue value;
  value.field = tx_target_;
  value.value_kind = protocol_core::LogicalValueKind::DECIMAL64;
  value.decimal64_value = command.target;
  const auto encoded =
      protocol_core::EncodeCompleteRecord(*plan_, *tx_workspace_, tx_pipeline_, tx_message_, &value,
                                          1U, {tx_buffer_.data(), tx_buffer_.size()});
  if (encoded.status != protocol_core::CodecStatus::OK) {
    return {HostStatus::CODEC_ERROR, encoded.status, encoded.conversion_error};
  }
  if (encoded.bytes_written != tx_buffer_.size()) {
    return {HostStatus::CODEC_ERROR, protocol_core::CodecStatus::INTERNAL_ERROR,
            protocol_core::ConversionError::NONE};
  }
  if (callbacks_.on_bytes_ready) {
    callbacks_.on_bytes_ready({tx_buffer_.data(), tx_buffer_.size()});
  }
  return {HostStatus::OK, protocol_core::CodecStatus::OK, protocol_core::ConversionError::NONE};
}

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
std::unique_ptr<BoundedRecordAdapter> BoundedRecordAdapter::Initialize(
    std::string_view config_text, BoundedRecordCallbacks callbacks, std::string& error_detail) {
  error_detail.clear();
  if (!callbacks.on_payload || !callbacks.on_bytes_ready) {
    error_detail = "both synchronous bounded-record callbacks are required";
    return nullptr;
  }
  auto compiled = config_compiler::CompileJsonToPlan(config_text);
  if (!compiled.Succeeded()) {
    const auto* diagnostic = compiled.Diagnostic();
    error_detail = diagnostic != nullptr ? diagnostic->detail : "configuration compilation failed";
    return nullptr;
  }
  auto plan = std::move(compiled).TakePlan();
  if (!plan || plan->SchemaVersion() != "0.8") {
    error_detail = "bounded-record embedding requires a Schema 0.8 plan";
    return nullptr;
  }
  const std::size_t pipeline = FindPipeline(*plan, "synthetic_rx");
  const std::size_t message = FindMessage(*plan, "bounded_record");
  if (pipeline == kInvalidIndex || message == kInvalidIndex ||
      !PipelineContains(plan->Pipelines()[pipeline], message)) {
    error_detail = "required bounded-record pipeline or message id is missing";
    return nullptr;
  }
  const auto& message_plan = plan->Messages()[message];
  const std::size_t payload = FindField(message_plan, "payload");
  if (!message_plan.bounded_payload.has_value() || payload == kInvalidIndex ||
      message_plan.bounded_payload->payload_field_index != payload ||
      message_plan.fields[payload].value_type != protocol_plan::ValueType::BYTES) {
    error_detail = "required bounded payload field does not match";
    return nullptr;
  }
  return std::unique_ptr<BoundedRecordAdapter>(
      new BoundedRecordAdapter(std::move(plan), std::move(callbacks), pipeline, message, payload));
}

BoundedRecordAdapter::BoundedRecordAdapter(protocol_plan::PlanOwner plan,
                                           BoundedRecordCallbacks callbacks, std::size_t pipeline,
                                           std::size_t message, std::size_t payload_field)
    : plan_(std::move(plan)),
      callbacks_(std::move(callbacks)),
      pipeline_(pipeline),
      message_(message),
      payload_field_{plan_.get(), message, payload_field},
      workspace_(std::make_unique<protocol_core::ExecutionWorkspace>(*plan_)),
      slots_(plan_->GetExecutionResourceLayout().max_fields_per_message),
      frame_buffer_(
          static_cast<std::size_t>(plan_->Messages()[message].bounded_payload->max_frame_length)) {}

HostResult BoundedRecordAdapter::OnReceivedRecord(protocol_core::ByteView bytes) {
  const auto decoded = protocol_core::DecodeCompleteRecord(*plan_, *workspace_, pipeline_, bytes,
                                                           slots_.data(), slots_.size());
  if (decoded.status != protocol_core::CodecStatus::OK) {
    return {HostStatus::CODEC_ERROR, decoded.status, decoded.conversion_error};
  }
  if (decoded.message_index != message_ || decoded.field_count <= payload_field_.field_index) {
    return {HostStatus::CODEC_ERROR, protocol_core::CodecStatus::INTERNAL_ERROR,
            protocol_core::ConversionError::NONE};
  }
  const auto& payload = slots_[payload_field_.field_index];
  if (payload.field.plan_scope != plan_.get() || payload.field.message_index != message_ ||
      payload.field.field_index != payload_field_.field_index ||
      payload.value_kind != protocol_core::LogicalValueKind::BYTES) {
    return {HostStatus::CODEC_ERROR, protocol_core::CodecStatus::INTERNAL_ERROR,
            protocol_core::ConversionError::NONE};
  }
  callbacks_.on_payload(payload.bytes_value);
  return {HostStatus::OK, protocol_core::CodecStatus::OK, protocol_core::ConversionError::NONE};
}

HostResult BoundedRecordAdapter::EncodePayload(protocol_core::ByteView payload) {
  protocol_core::EncodeFieldValue value;
  value.field = payload_field_;
  value.value_kind = protocol_core::LogicalValueKind::BYTES;
  value.bytes_value = payload;
  const auto encoded =
      protocol_core::EncodeCompleteRecord(*plan_, *workspace_, pipeline_, message_, &value, 1U,
                                          {frame_buffer_.data(), frame_buffer_.size()});
  if (encoded.status != protocol_core::CodecStatus::OK) {
    return {HostStatus::CODEC_ERROR, encoded.status, encoded.conversion_error};
  }
  callbacks_.on_bytes_ready({frame_buffer_.data(), encoded.bytes_written});
  return {HostStatus::OK, protocol_core::CodecStatus::OK, protocol_core::ConversionError::NONE};
}
#endif

}  // namespace pae::examples::business_embedding
