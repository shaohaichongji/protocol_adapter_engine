#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pae/compiler.h"
#include "pae/host_endpoint.h"

namespace pae::protocol_lab_binary::public_decode {

struct Limits {
  std::size_t max_frame_bytes = 65536U;
  std::size_t max_stream_chunk_bytes = 65536U;
  std::size_t max_stream_work_units = 0U;
  std::size_t max_fields = 1024U;
  std::size_t max_field_bytes = 1024U * 1024U;
  std::size_t max_result_bytes = 8U * 1024U * 1024U;
  std::size_t max_description_bytes = 4U * 1024U * 1024U;
  std::size_t instance_bytes = 128U * 1024U * 1024U;
  std::size_t replacement_bytes = 256U * 1024U * 1024U;
};

enum class LocalStatus { OK, INVALID_INPUT, INVALID_BINDING, RESOURCE_LIMIT, PREPARATION_FAILED,
                         MATERIALIZATION_FAILED };

enum class StreamDiagnostic {
  NONE,
  NOT_STREAM,
  INVALID_CHUNK,
  CONTINUE_REQUIRED,
  NO_WORK,
  RESET_REQUIRED,
  COPY_FAILED_RESET_REQUIRED,
  CONTRACT_VIOLATION_RESET_REQUIRED,
};

struct Field {
  std::size_t flat_index = 0U;
  std::string id;
  ValueKind kind = ValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  Decimal64 decimal;
  std::vector<std::uint8_t> bytes;
  std::optional<std::uint64_t> enum_raw;
  std::optional<std::size_t> known_enum_flat_index;
  std::optional<RawIntegerKind> conversion_raw_kind;
  std::optional<std::uint64_t> conversion_raw_uint64;
  std::optional<std::int64_t> conversion_raw_int64;
  std::optional<ByteRange> byte_range;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
};

struct Candidate {
  bool success = false;
  std::optional<std::size_t> message_index;
  std::string message_id;
  CodecStatus codec_status = CodecStatus::INVALID_ARGUMENT;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  std::vector<std::uint8_t> frame;
  std::vector<Field> fields;
  std::optional<ByteRange> integrity_storage;
  std::optional<ByteRange> computed_length_storage;
  std::size_t accounted_bytes = 0U;
};

struct EncodeInput {
  std::size_t field_index = 0U;
  ValueKind kind = ValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  std::vector<std::uint8_t> bytes;
  std::size_t enum_entry_index = 0U;
  Decimal64 decimal;

  static EncodeInput UInt64(std::size_t field, std::uint64_t value);
  static EncodeInput Int64(std::size_t field, std::int64_t value);
  static EncodeInput Bool(std::size_t field, bool value);
  static EncodeInput Bytes(std::size_t field, std::vector<std::uint8_t> value);
  static EncodeInput Enum(std::size_t field, std::size_t entry);
  static EncodeInput Decimal(std::size_t field, Decimal64 value);
};

struct EncodedField {
  std::size_t field_index = 0U;
  std::string id;
  std::optional<ByteRange> byte_range;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
};

struct Encoded {
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<EncodeInput> inputs;
  std::vector<std::uint8_t> frame;
  std::vector<EncodedField> fields;
  std::optional<ByteRange> integrity_storage;
  std::optional<ByteRange> computed_length_storage;
  std::size_t accounted_bytes = 0U;
};

struct Operation {
  LocalStatus local_status = LocalStatus::OK;
  HostOperationResult host;
  std::optional<Candidate> candidate;
  std::optional<Encoded> encoded;
};

struct StreamObservation {
  PipelineFramingStrategy strategy = PipelineFramingStrategy::COMPLETE_RECORD;
  StreamFramingPhase phase = StreamFramingPhase::COLLECTING;
  std::size_t maximum_candidate_frame_bytes = 0U;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
  std::size_t buffered_bytes = 0U;
  std::size_t frozen_input_bytes = 0U;
  std::size_t frozen_cursor = 0U;
  bool has_internal_work = false;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_decode_failures = 0U;
  std::uint64_t total_observer_callbacks = 0U;
  std::uint64_t total_business_callbacks = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
};

struct StreamStep {
  LocalStatus local_status = LocalStatus::OK;
  StreamDiagnostic diagnostic = StreamDiagnostic::NONE;
  bool host_called = false;
  HostOperationResult host;
  StreamObservation before;
  StreamObservation after;
  std::optional<Candidate> candidate;
};

struct StreamState {
  bool available = false;
  PipelineFramingStrategy strategy = PipelineFramingStrategy::COMPLETE_RECORD;
  std::size_t maximum_candidate_frame_bytes = 0U;
  std::size_t capacity = 0U;
  std::vector<std::uint8_t> frozen_input;
  std::size_t cursor = 0U;
  bool faulted = false;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_decode_failures = 0U;
  std::uint64_t total_observer_callbacks = 0U;
  std::uint64_t total_business_callbacks = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
  StreamStep current;
};

struct FlowState {
  std::string draft;
  Operation current;
  HostChannelHandle handle;
  StreamState stream;
};

struct OwnedMessageName {
  std::size_t index = 0U;
  std::string id;
  std::vector<std::string> field_ids;
};

struct Preparation {
  LocalStatus status = LocalStatus::PREPARATION_FAILED;
  HostStatus host_status = HostStatus::INVALID_ARGUMENT;
  std::unique_ptr<class Adapter> adapter;
  std::optional<CompileDiagnostic> diagnostic;
};

struct Binding {
  std::string endpoint;
  HostAction action = HostAction::DECODE;
  std::string pipeline_id;
  std::size_t flow_count = 2U;
};

class Adapter final {
 public:
  static Preparation Create(std::string_view json, std::string_view endpoint,
                            std::string_view pipeline_id, const Limits& limits = {},
                            std::size_t previous_instance_bytes = 0U);
  static Preparation AdoptCompiled(CompiledProtocol compiled, std::vector<Binding> bindings,
                                   const Limits& limits = {},
                                   std::size_t previous_instance_bytes = 0U);
  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;
  ~Adapter();

  const Operation& Decode(std::size_t flow, ByteView frame);
  const Operation& Encode(std::size_t binding, std::size_t message_index,
                          const std::vector<EncodeInput>& inputs);
  const StreamStep& SubmitStreamChunk(std::size_t binding, std::size_t flow,
                                      const std::vector<std::uint8_t>& chunk) noexcept;
  const StreamStep& ContinueStream(std::size_t binding, std::size_t flow) noexcept;
  std::optional<StreamObservation> ObserveStream(std::size_t binding,
                                                 std::size_t flow) const noexcept;
  bool StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept;
  HostStatus Reset(std::size_t flow) noexcept;
  void ClearCurrent(std::size_t flow) noexcept;
  bool SetDraft(std::size_t flow, std::string_view text);
  const FlowState* State(std::size_t flow) const noexcept;
  std::size_t FlowCount(std::size_t binding) const noexcept;
  std::size_t FlowIndex(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t InstanceAdmissionBytes() const noexcept { return instance_bytes_; }

 private:
  Adapter() = default;
  const StreamStep& RunStreamStep(std::size_t binding, std::size_t flow) noexcept;
  CompiledProtocol compiled_;
  std::vector<OwnedMessageName> messages_;
  std::unique_ptr<HostEndpoint> host_;
  std::vector<Binding> bindings_;
  std::vector<std::size_t> flow_begin_;
  std::vector<FlowState> flows_;
  Operation rejected_;
  StreamStep rejected_stream_;
  Limits limits_;
  std::size_t instance_bytes_ = 0U;
};

}  // namespace pae::protocol_lab_binary::public_decode
