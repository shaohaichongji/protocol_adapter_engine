#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pae/codec.h"
#include "pae/stream_framer.h"

namespace pae::protocol_lab_ui {

enum class AsciiHostAction { DECODE, ENCODE };

enum class AsciiAdapterStatus {
  OK,
  INVALID_REQUEST,
  CORE_FAILED,
  MATERIALIZATION_FAILED,
};

enum class AsciiOperation { ENCODE, INSPECT };
enum class AsciiReviewKind { NOT_APPLICABLE, TX_TEMPLATE };

struct AsciiExecutionIdentity {
  std::optional<std::size_t> binding_index;
  std::optional<std::size_t> stream_index;
  std::uint64_t stream_generation = 0U;
  std::uint64_t operation_sequence = 0U;
  std::uint64_t document_id = 0U;
  std::uint64_t load_revision = 0U;
  std::uint64_t plan_generation = 0U;
  std::uint64_t selection_revision = 0U;
  std::uint64_t input_revision = 0U;
  std::size_t pipeline_index = 0U;
  std::string pipeline_id;
  std::optional<std::size_t> message_index;
  std::optional<std::string> message_id;
};

struct AsciiHostBinding {
  std::string endpoint;
  AsciiHostAction action = AsciiHostAction::DECODE;
  std::size_t pipeline_index = 0U;
};

struct AsciiInputField {
  std::size_t field_index = 0U;
  std::string field_id;
  std::vector<std::uint8_t> bytes;
};

struct AsciiFieldResult {
  std::size_t field_index = 0U;
  std::string field_id;
  std::vector<std::uint8_t> bytes;
  ByteRange range;
};

struct AsciiExecutionResult {
  AsciiExecutionIdentity identity;
  AsciiOperation operation = AsciiOperation::INSPECT;
  AsciiAdapterStatus status = AsciiAdapterStatus::INVALID_REQUEST;
  CodecStatus codec_status = CodecStatus::INVALID_ARGUMENT;
  bool codec_called = false;
  AsciiReviewKind review_kind = AsciiReviewKind::NOT_APPLICABLE;
  std::optional<std::size_t> message_index;
  std::optional<std::string> message_id;
  std::optional<std::size_t> failed_input_index;
  std::optional<std::size_t> failed_field_index;
  std::optional<std::string> failed_field_id;
  std::vector<std::uint8_t> frame;
  std::vector<AsciiFieldResult> fields;
  std::vector<std::uint8_t> diagnostic_input_frame;
  std::string detail;
};

struct AsciiStreamObservation {
  StreamFramingPhase phase = StreamFramingPhase::COLLECTING;
  std::size_t buffered_bytes = 0U;
  bool has_internal_work = false;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
  std::size_t frozen_input_bytes = 0U;
  std::size_t frozen_cursor = 0U;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_observed_candidates = 0U;
  std::uint64_t total_business_outputs = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
};

struct AsciiStreamStepResult {
  AsciiExecutionIdentity identity;
  AsciiAdapterStatus status = AsciiAdapterStatus::INVALID_REQUEST;
  bool push_called = false;
  StreamSubmitResult framing;
  AsciiStreamObservation before;
  AsciiStreamObservation after;
  std::optional<AsciiExecutionResult> candidate;
  std::string detail;
};

std::string_view AsciiCodecStatusName(CodecStatus status) noexcept;

}  // namespace pae::protocol_lab_ui
