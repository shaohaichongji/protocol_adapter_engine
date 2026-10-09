#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

#include "pae/codec.h"
#include "pae/export.h"
#include "pae/stream_framer.h"

namespace pae {

class CompiledProtocol;
class HostEndpoint;

namespace detail {
class HostScopeToken;
class HostEndpointTestAccess;
}  // namespace detail

enum class HostAction { DECODE, ENCODE };

// Host 层状态与 Codec/Framer 状态分开；OK 可表示仅缓存了半帧，而非已交付业务结果。
enum class HostStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_COMPILED_PROTOCOL,
  INVALID_BINDING,
  DUPLICATE_BINDING,
  RESOURCE_LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  WORKSPACE_BUSY,
  REENTRANT_CALL,
  EXPIRED_HANDLE,
  FOREIGN_HANDLE,
  STALE_HANDLE,
  WRONG_ACTION,
  WRONG_INPUT_KIND,
  RESET_REQUIRED,
  GENERATION_EXHAUSTED,
  CODEC_FAILED,
  FRAMING_FAILED,
  CALLBACK_FAILED,
  INTERNAL_ERROR,
};

inline constexpr std::size_t kNoHostMessageIndex = static_cast<std::size_t>(-1);

// 创建时复制 endpoint_key；按长度和字节精确匹配，区分大小写，可包含 NUL。
// 唯一键为 (endpoint_key, action)，不是网络地址；pipeline_index 为全局 Pipeline 索引。
// DECODE 的各 stream 拥有独立状态（完整记录输入也可多 channel）；ENCODE 数量必须为 1。
// framing_options 仅用于 STREAM_CHUNK DECODE，其余绑定不得设置覆盖值。
struct HostBindingSpec {
  std::string_view endpoint_key;
  HostAction action = HostAction::DECODE;
  std::size_t pipeline_index = 0U;
  std::size_t decode_stream_count = 1U;
  StreamFramerOptions framing_options;
};

// 正数限制仍受内部硬上限约束；channels 包含 Encode channel，identity 按单个 key 字节计。
// max_accounted_bytes 是 Host 自有存储的逻辑预算，不是进程 RSS 限制。
struct HostLimits {
  std::size_t max_bindings = 64U;
  std::size_t max_channels = 64U;
  std::size_t max_identity_bytes = 256U;
  std::size_t max_accounted_bytes = 64U * 1024U * 1024U;
  CompleteRecordCodecOptions codec_options;
};

// 汇总创建时各类自有存储；共享 compiled 状态另列，allocation_count 不表示逐帧分配。
struct HostMemoryReport {
  std::size_t facade_bytes = 0U;
  std::size_t scope_payload_bytes = 0U;
  std::size_t binding_storage_bytes = 0U;
  std::size_t identity_storage_bytes = 0U;
  std::size_t channel_storage_bytes = 0U;
  std::size_t codec_accounted_bytes = 0U;
  std::size_t framer_accounted_bytes = 0U;
  std::size_t encode_buffer_bytes = 0U;
  std::size_t host_accounted_total_bytes = 0U;
  std::size_t retained_compiled_state_facade_bytes = 0U;
  std::size_t allocation_count = 0U;
};

// 可复制的非拥有句柄，不能延长 Host 寿命。默认句柄无效，其他 Host 的句柄为 FOREIGN。
// 原 Host 已销毁则为 EXPIRED；成功 Reset 后该 channel 的旧句柄为 STALE，需重新 Find。
// 这些检查不允许通过已销毁的 Host 对象调用方法。
class PAE_API HostChannelHandle final {
 public:
  HostChannelHandle() noexcept = default;

 private:
  std::weak_ptr<const detail::HostScopeToken> scope_;
  std::size_t channel_index_ = static_cast<std::size_t>(-1);
  std::uint64_t generation_ = 0U;
  bool assigned_ = false;

  friend class HostEndpoint;
  friend class detail::HostEndpointTestAccess;
};

struct HostFindResult {
  HostStatus status = HostStatus::INVALID_ARGUMENT;
  HostChannelHandle handle;
};

enum class HostCallbackAction { CONTINUE, STOP };

// observer 在本候选的一次 Decode 之后调用，失败候选也会通知；不是 Decode 前接收门禁。
// generation 是 channel 的 Reset 代次，不是逐帧序号。frame 仅在同步回调期间借用。
// record 仅在 decode_status == OK 时可作成功输出；失败后仍可能保留已匹配消息身份。
struct HostCandidateView {
  std::uint64_t generation = 0U;
  ByteView frame;
  CodecStatus decode_status = CodecStatus::INVALID_ARGUMENT;
  DecodedRecordView record;
  std::optional<std::size_t> matched_message_index;
  std::size_t required_field_count = 0U;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  bool output_tainted = false;
};

using HostCandidateObserverFunction = HostCallbackAction (*)(const HostCandidateView&, void*);

// 可省略 observer；context 仅同步借用。抛异常会被捕获并使该 channel 要求 Reset。
struct HostCandidateObserver {
  HostCandidateObserverFunction function = nullptr;
  void* context = nullptr;
};

// 仅 Codec 成功时调用业务 sink；复制 view 不会复制字段或字节，持久保存需深拷贝。
// Decode 字节字段还借用当前 candidate；Encode bytes 借用预分配的 channel 输出 Buffer。
// 不应把回调参数引用、frame 或 bytes 留到回调之外继续使用。
struct HostOutputView {
  HostAction action = HostAction::DECODE;
  std::uint64_t generation = 0U;
  std::size_t message_index = kNoHostMessageIndex;
  DecodedRecordView record;
  ByteView frame;  // Decode only; borrows the current candidate.
  ByteView bytes;  // Encode only; borrows the channel output buffer.
};

using HostOutputSinkFunction = HostCallbackAction (*)(const HostOutputView&, void*);

// sink 必须非空；context 由调用方持有。正常 STOP 不要求 Reset，异常则使 channel 故障。
// observer 返回 STOP 仍会交付当前成功候选的业务回调，只停止后续候选推进。
struct HostOutputSink {
  HostOutputSinkFunction function = nullptr;
  void* context = nullptr;
};

// 本次操作的聚合事实：codec_attempted 为 false 时，不解释默认 codec_status。
// codec_status 是最后一次尝试的状态，不是所有候选的汇总，可能 OK 而 Host 为 CODEC_FAILED。
// generation 为 channel 代次；callbacks_returned 仅计正常返回（包括 STOP），不计抛异常。
// framing_attempted 决定嵌套 framing 是否有意义；reset_required 指示回调故障后的恢复要求。
struct HostOperationResult {
  HostStatus status = HostStatus::INVALID_ARGUMENT;
  std::uint64_t generation = 0U;
  bool codec_attempted = false;
  CodecStatus codec_status = CodecStatus::INVALID_ARGUMENT;
  std::size_t bytes_consumed = 0U;
  // Actual Codec output. A CALLBACK_FAILED result may retain this fact, but did not complete
  // business delivery; only status == OK confirms the callback returned normally.
  std::size_t bytes_produced = 0U;
  std::size_t candidates = 0U;
  std::size_t decode_attempts = 0U;
  std::size_t decode_successes = 0U;
  std::size_t decode_failures = 0U;
  std::size_t observer_callbacks_returned = 0U;
  std::size_t business_callbacks_returned = 0U;
  bool framing_attempted = false;
  StreamSubmitResult framing;
  bool reset_required = false;
};

// framing 仅在 stream 且相应查询成功时解释；这不是并发快照。
struct HostObservation {
  HostStatus status = HostStatus::INVALID_ARGUMENT;
  HostAction action = HostAction::DECODE;
  std::uint64_t generation = 0U;
  bool reset_required = false;
  bool stream = false;
  StreamFramerObservation framing;
};

struct HostEndpointCreateResult;

// Immutable binding owner. Operations on one instance are serialized and non-waiting.
// Handles are non-owning and must be reacquired with Find() after a successful Reset().
// 保留冻结 compiled 状态，拥有每个 channel 的 Codec、可选 Framer 及 Encode Buffer。
// 不拥有 Socket、线程或路由。Flow 状态隔离不代表同一 Host 的不同 channel 可并发执行。
// 受保护操作的回调重入返回 REENTRANT_CALL，并发返回 WORKSPACE_BUSY；不得在回调中销毁 Host。
class PAE_API HostEndpoint final {
 public:
  HostEndpoint(const HostEndpoint&) = delete;
  HostEndpoint& operator=(const HostEndpoint&) = delete;
  HostEndpoint(HostEndpoint&&) = delete;
  HostEndpoint& operator=(HostEndpoint&&) = delete;
  ~HostEndpoint();

  // decode_stream_index 是绑定内从 0 开始的 channel 序号；ENCODE 仅允许 0。
  [[nodiscard]] HostFindResult Find(std::string_view endpoint_key, HostAction action,
                                    std::size_t decode_stream_index = 0U) const noexcept;
  [[nodiscard]] HostObservation Observe(const HostChannelHandle& handle) const noexcept;
  // 仅重置目标 channel、清除故障并推进代次，使该 channel 的旧句柄和输出视图失效。
  // 成功后重新 Find；其他 channel 的半帧及句柄不受此 Reset 影响。
  [[nodiscard]] HostStatus Reset(const HostChannelHandle& handle) noexcept;

  // 仅用于 COMPLETE_RECORD DECODE 绑定；一次输入执行一次 Codec，失败不调用业务 sink。
  [[nodiscard]] HostOperationResult Decode(const HostChannelHandle& handle, ByteView frame,
                                           HostOutputSink sink,
                                           HostCandidateObserver observer = {}) noexcept;
  // 仅用于 STREAM_CHUNK DECODE。bytes_consumed 是已消费前缀，后缀不会被保留。
  // 每个候选只 Decode 一次；部分输入可返回 OK 且 decode_attempts == 0。
  [[nodiscard]] HostOperationResult Push(const HostChannelHandle& handle, ByteView input,
                                         HostOutputSink sink,
                                         HostCandidateObserver observer = {}) noexcept;
  // 推进内部待交付工作，不重推已消费输入；STOP 后只另行提交未消费后缀一次。
  [[nodiscard]] HostOperationResult Continue(const HostChannelHandle& handle, HostOutputSink sink,
                                             HostCandidateObserver observer = {}) noexcept;
  // 仅用于 ENCODE 绑定；message_index 是该 Pipeline 允许的全局 Message 索引。
  // 一次 Encode 成功后同步交付一次，STOP 不撤销当前结果，也不触发重试。
  [[nodiscard]] HostOperationResult Encode(const HostChannelHandle& handle,
                                           std::size_t message_index, const EncodeValue* values,
                                           std::size_t value_count, HostOutputSink sink) noexcept;

  // 固定的创建计费事实，不执行 channel 状态观察或传输。
  [[nodiscard]] HostMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit HostEndpoint(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;

  friend struct HostEndpointCreateResult;
  friend class detail::HostEndpointTestAccess;
  friend PAE_API HostEndpointCreateResult CreateHostEndpoint(const CompiledProtocol&,
                                                             const HostBindingSpec*, std::size_t,
                                                             const HostLimits&) noexcept;
};

struct HostEndpointCreateResult {
  HostStatus status = HostStatus::INVALID_ARGUMENT;
  std::unique_ptr<HostEndpoint> host;
  HostMemoryReport memory;
};

// 同步借用 bindings 数组并复制 key；创建成功后不依赖这些输入或外部 compiled 对象寿命。
// 验证绑定和预算并预建执行资源；失败不交付 Host，不自动降级或启动传输。
[[nodiscard]] PAE_API HostEndpointCreateResult
CreateHostEndpoint(const CompiledProtocol& compiled, const HostBindingSpec* bindings,
                   std::size_t binding_count, const HostLimits& limits = {}) noexcept;

}  // namespace pae
