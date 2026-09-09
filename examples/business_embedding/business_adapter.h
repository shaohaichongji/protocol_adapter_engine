#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "complete_record_codec.h"

namespace pae::examples::business_embedding {

struct Measurement {
  protocol_core::Decimal64 temperature;
  bool alarm = false;
};

struct Command {
  protocol_core::Decimal64 target;
};

struct Callbacks {
  std::function<void(const Measurement&)> on_measurement;
  std::function<void(protocol_core::ByteView)> on_bytes_ready;
};

enum class HostStatus {
  OK,
  CONFIGURATION_ERROR,
  CODEC_ERROR,
};

struct HostResult {
  HostStatus status = HostStatus::CONFIGURATION_ERROR;
  protocol_core::CodecStatus codec_status = protocol_core::CodecStatus::INVALID_ARGUMENT;
  protocol_core::ConversionError conversion_error = protocol_core::ConversionError::NONE;

  [[nodiscard]] bool Succeeded() const noexcept { return status == HostStatus::OK; }
};

// This example-only facade is synchronous and serial. It is not an installed or stable public API.
class BusinessAdapter final {
 public:
  BusinessAdapter(const BusinessAdapter&) = delete;
  BusinessAdapter& operator=(const BusinessAdapter&) = delete;
  BusinessAdapter(BusinessAdapter&&) = delete;
  BusinessAdapter& operator=(BusinessAdapter&&) = delete;
  ~BusinessAdapter() = default;

  static std::unique_ptr<BusinessAdapter> Initialize(std::string_view config_text,
                                                     Callbacks callbacks,
                                                     std::string& error_detail);

  [[nodiscard]] HostResult OnReceivedRecord(protocol_core::ByteView bytes);
  [[nodiscard]] HostResult SendCommand(const Command& command);

 private:
  BusinessAdapter(protocol_plan::PlanOwner plan, Callbacks callbacks, std::size_t rx_pipeline,
                  std::size_t rx_message, std::size_t rx_temperature, std::size_t rx_alarm,
                  std::size_t tx_pipeline, std::size_t tx_message, std::size_t tx_target);

  protocol_plan::PlanOwner plan_;
  Callbacks callbacks_;
  std::size_t rx_pipeline_ = protocol_core::kInvalidIndex;
  std::size_t rx_message_ = protocol_core::kInvalidIndex;
  protocol_core::FieldRef rx_temperature_;
  protocol_core::FieldRef rx_alarm_;
  std::size_t tx_pipeline_ = protocol_core::kInvalidIndex;
  std::size_t tx_message_ = protocol_core::kInvalidIndex;
  protocol_core::FieldRef tx_target_;
  std::unique_ptr<protocol_core::ExecutionWorkspace> rx_workspace_;
  std::unique_ptr<protocol_core::ExecutionWorkspace> tx_workspace_;
  std::vector<protocol_core::DecodedFieldSlot> rx_slots_;
  std::vector<std::uint8_t> tx_buffer_;
};

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
struct BoundedRecordCallbacks {
  std::function<void(protocol_core::ByteView)> on_payload;
  std::function<void(protocol_core::ByteView)> on_bytes_ready;
};

// Example-only synchronous host for the public bounded-record sample. Received payload bytes are
// borrowed only for the callback; a host that retains them must make its own copy.
class BoundedRecordAdapter final {
 public:
  BoundedRecordAdapter(const BoundedRecordAdapter&) = delete;
  BoundedRecordAdapter& operator=(const BoundedRecordAdapter&) = delete;
  BoundedRecordAdapter(BoundedRecordAdapter&&) = delete;
  BoundedRecordAdapter& operator=(BoundedRecordAdapter&&) = delete;
  ~BoundedRecordAdapter() = default;

  static std::unique_ptr<BoundedRecordAdapter> Initialize(std::string_view config_text,
                                                          BoundedRecordCallbacks callbacks,
                                                          std::string& error_detail);

  [[nodiscard]] HostResult OnReceivedRecord(protocol_core::ByteView bytes);
  [[nodiscard]] HostResult EncodePayload(protocol_core::ByteView payload);

 private:
  BoundedRecordAdapter(protocol_plan::PlanOwner plan, BoundedRecordCallbacks callbacks,
                       std::size_t pipeline, std::size_t message, std::size_t payload_field);

  protocol_plan::PlanOwner plan_;
  BoundedRecordCallbacks callbacks_;
  std::size_t pipeline_ = protocol_core::kInvalidIndex;
  std::size_t message_ = protocol_core::kInvalidIndex;
  protocol_core::FieldRef payload_field_;
  std::unique_ptr<protocol_core::ExecutionWorkspace> workspace_;
  std::vector<protocol_core::DecodedFieldSlot> slots_;
  std::vector<std::uint8_t> frame_buffer_;
};
#endif

}  // namespace pae::examples::business_embedding
