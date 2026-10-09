#pragma once

// 内部试验接入层：拥有 Plan 和不可变 endpoint/action 绑定，同步组合 Core/Framer。
// 不安装为公开 API，也不管理 Socket、线程、路由或自动重试。
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

// 身份是逻辑键而非网络端点；Pipeline 由名字显式绑定，Encode 仍在调用时选择 Message。
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
// 成功业务输出，仅在同步 sink 中借用；generation 是 Reset 代次，不是逐帧编号。
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
// Diagnostic only; all views expire when the callback returns. Failed candidates have no fields.
// 这是一次 Decode 之后的观察材料，不是 Decode 前门禁；raw 留存来自成功 Core 调用。
struct Candidate {
  std::uint64_t generation = 0U;
  const protocol_plan::PlanBundle* plan = nullptr;
  protocol_core::ByteView frame;
  protocol_core::DecodeResult decoded;
  const protocol_core::DecodedFieldSlot* fields = nullptr;
  std::size_t field_count = 0U;

  // Converted integer fields only, not all decoded fields. Callback-scoped, serialized view.
  // Failure candidates expose zero entries. A failed read leaves output unchanged.
  // The copied value's FieldRef still borrows the Plan; do not retain it across callbacks.
  [[nodiscard]] std::size_t RawIntegerCount() const noexcept;
  [[nodiscard]] bool GetRawInteger(std::size_t index,
                                  protocol_core::RawIntegerValue& output) const noexcept;

 private:
  friend class Session;
  const protocol_core::ExecutionWorkspace* raw_workspace_ = nullptr;
};
struct CandidateObserver {
  // Runs before the success Sink. Normal STOP keeps current success delivery; exceptions do not.
  SinkAction (*function)(const Candidate&, void*) = nullptr;
  void* context = nullptr;
};
// 单次操作聚合事实；最后一次 codec_status 不代表所有候选都成功，回调数只计正常返回。
struct Result {
  Status status = Status::INVALID_ARGUMENT;
  std::uint64_t generation = 0U;
  bool codec_attempted = false;
  protocol_core::CodecStatus codec_status = protocol_core::CodecStatus::INVALID_ARGUMENT;
  std::size_t successful_outputs = 0U;
  std::size_t decode_successes = 0U;
  std::size_t observed_candidates = 0U;  // Observer returned normally, including STOP.
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
  // 移交 owner，使 Plan 覆盖全部独立 Channel 工作区寿命；失败不发布部分 Session。
  static CreateResult Create(protocol_plan::PlanOwner plan, const BindingSpec* specs,
                             std::size_t count, const Limits& limits = {}) noexcept;
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Handle Find(std::string_view endpoint, Action action, std::size_t stream = 0U) const noexcept;
  Result Decode(const Handle&, protocol_core::ByteView, Sink, CandidateObserver = {}) noexcept;
  // 内部没有 Continue 方法；仅有内部工作时才接受空 Push，未消费后缀由宿主重提。
  Result Push(const Handle&, protocol_core::ByteView, Sink, CandidateObserver = {}) noexcept;
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
