#pragma once

#include <memory>
#include <string_view>

#include "../protocol_core/complete_record_codec.h"
#include "../protocol_framing/stream_framer.h"

namespace pae::host_endpoint {

enum class Action { DECODE, ENCODE };
enum class Status {
  OK,
  INVALID_ARGUMENT,
  INVALID_BINDING,
  UNSUPPORTED,
  LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  BUSY,
  WRONG_INPUT_KIND,
  CODEC_FAILED,
  FRAMING_FAILED,
  CALLBACK_FAILED,
  RESET_REQUIRED
};

struct BindingSpec {
  std::string_view endpoint;
  Action action = Action::DECODE;
  std::string_view pipeline;
  std::size_t streams = 1U;  // Decode logical streams; Encode must be one.
  protocol_framing::FramingLimitOverrides framing_limits;
};
struct Limits {
  std::size_t max_bindings = 64U;
  std::size_t max_channels = 64U;
  std::size_t max_identity_bytes = 256U;
  std::size_t max_accounted_bytes = 64U * 1024U * 1024U;
};

class Session;
// Opaque, instance-scoped and non-owning. Expired handles cannot alias a new session.
class Handle {
 public:
  Handle() = default;

 private:
  friend class Session;
  std::weak_ptr<const int> scope_;
  std::size_t channel_ = protocol_core::kInvalidIndex;
};

struct NamedValue {
  std::string_view field_id;
  // field must be empty: the session resolves identity in its own Plan.
  // ENUM references, if used, must refer to the same live Plan.
  protocol_core::EncodeFieldValue value;
};
enum class SinkAction { CONTINUE, STOP };
struct Output {
  Action action = Action::DECODE;
  std::uint64_t generation = 0U;
  const protocol_plan::PlanBundle* plan = nullptr;
  std::size_t message_index = protocol_core::kInvalidIndex;
  const protocol_core::DecodedFieldSlot* fields = nullptr;
  std::size_t field_count = 0U;
  protocol_core::ByteView bytes;  // Encode only; Decode fields may borrow candidate bytes.
};
// Only successful results are delivered. Views are valid only inside this callback.
// Callback exceptions are caught; do not destroy this session from its callback.
struct Sink {
  SinkAction (*function)(const Output&, void*) = nullptr;
  void* context = nullptr;
};
struct Result {
  Status status = Status::INVALID_ARGUMENT;
  std::uint64_t generation = 0U;
  bool codec_attempted = false;
  protocol_core::CodecStatus codec_status = protocol_core::CodecStatus::INVALID_ARGUMENT;
  std::size_t successful_outputs = 0U;
  std::size_t decode_failures =
      0U;  // Per-call aggregate; codec_status is the last candidate status.
  bool framing_attempted = false;
  protocol_framing::SubmitResult framing;
};
struct Observation {
  Status status = Status::INVALID_BINDING;
  std::uint64_t generation = 0U;
  bool reset_required = false;
  bool stream = false;
  protocol_framing::StreamFramingObservation framing;
};
struct CreateResult;

// Internal, non-installed API. All operations on one session must be serialized.
// No transport, automatic drive loop, hot registration, or implicit direction inference.
class Session final {
 public:
  static CreateResult Create(protocol_plan::PlanOwner plan, const BindingSpec* specs,
                             std::size_t count, const Limits& limits = {}) noexcept;
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Handle Find(std::string_view endpoint, Action action, std::size_t stream = 0U) const noexcept;
  Result Decode(const Handle&, protocol_core::ByteView, Sink) noexcept;
  Result Push(const Handle&, protocol_core::ByteView, Sink) noexcept;
  Result Encode(const Handle&, std::string_view message, const NamedValue*, std::size_t,
                Sink) noexcept;
  Status Reset(const Handle&) noexcept;
  Observation Observe(const Handle&) const noexcept;
  const protocol_plan::PlanBundle& Plan() const noexcept;
  std::size_t AccountedBytes() const noexcept;

 private:
  struct Impl;
  explicit Session(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
  std::size_t Resolve(const Handle&) const noexcept;
};
struct CreateResult {
  Status status = Status::INVALID_ARGUMENT;
  std::unique_ptr<Session> session;
};
}  // namespace pae::host_endpoint
