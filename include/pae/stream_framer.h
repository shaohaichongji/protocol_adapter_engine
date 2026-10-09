#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "pae/codec.h"
#include "pae/export.h"

namespace pae {

class CompiledProtocol;

enum class StreamFramerStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_COMPILED_PROTOCOL,
  PIPELINE_OUT_OF_RANGE,
  INPUT_KIND_NOT_STREAM,
  RESOURCE_LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  WORKSPACE_BUSY,
  REENTRANT_CALL,
  INTERNAL_ERROR,
};

// status 表示调用是否有效；停止原因描述正常的有界推进，OK 不等于已交付候选。
enum class StreamFramerStopReason {
  INPUT_EXHAUSTED,
  NEED_MORE,
  WORK_BUDGET_REACHED,
  SINK_STOP,
};

enum class StreamFramingIssue {
  NONE,
  MALFORMED_LENGTH,
  RECORD_TOO_LONG,
};

enum class StreamFramingPhase {
  COLLECTING,
  DELIVERY_PENDING,
  DISCARDING_UNTIL_CRLF,
};

struct StreamFramingCapability {
  StreamFramerStatus status = StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
  bool available = false;
};

enum class PipelineInputKind { COMPLETE_RECORD, STREAM_CHUNK };

enum class PipelineFramingStrategy {
  COMPLETE_RECORD,
  FIXED_LENGTH,
  SYNC_FIXED_LENGTH,
  SYNC_LENGTH_FIELD,
  ASCII_CRLF,
};

enum class PipelineFramingQueryStatus {
  OK,
  INVALID_COMPILED_PROTOCOL,
  PIPELINE_OUT_OF_RANGE,
  INTERNAL_CONTRACT_VIOLATION,
};

struct PipelineFramingDescription {
  std::size_t pipeline_index = 0U;
  PipelineInputKind input_kind = PipelineInputKind::COMPLETE_RECORD;
  PipelineFramingStrategy strategy = PipelineFramingStrategy::COMPLETE_RECORD;
  std::optional<std::size_t> maximum_candidate_frame_bytes;
};

struct PipelineFramingQueryResult {
  PipelineFramingQueryStatus status = PipelineFramingQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<PipelineFramingDescription> value;
};

// 仅在同步 sink 回调内借用 bytes；需留存时复制字节。候选形成不等于 Decode/校验成功。
// pipeline_index 是创建 Framer 时绑定的全局 Pipeline 索引。
struct FrameCandidateView {
  ByteView bytes;
  std::size_t pipeline_index = 0U;
};

// STOP 确认当前候选已交付，只停止后续推进；Continue() 不会重新交付该候选。
enum class FrameSinkAction { CONTINUE, STOP };
using FrameSinkFunction = FrameSinkAction (*)(FrameCandidateView, void*) noexcept;

// function 必须非空且不得抛异常；context 由调用方持有，仅在同步调用期间借用。
struct FrameSink {
  FrameSinkFunction function = nullptr;
  void* context = nullptr;
};

// A zero override selects the compiled resource profile default. Overrides can only select values
// accepted by the existing bounded Framer and never remove its hard limits.
// 0 使用编译资源配置的默认值；非零覆盖仍须满足硬限制，不意味着可取消预算。
// submit/session/sync 以字节计，frames 为候选数；work_units 是逻辑操作量，不是耗时。
struct StreamFramerOptions {
  std::size_t max_submit_bytes = 0U;
  std::size_t max_frames_per_submit = 0U;
  std::size_t max_work_units = 0U;
  std::size_t max_sync_bytes = 0U;
  std::size_t max_session_memory_bytes = 0U;
};

// session 限制约束内部 Workspace；总计另加 facade，共享 compiled 状态单列，不是 RSS。
// allocation_count 记录创建分配数，不是逐帧动态分配数。
struct StreamFramingMemoryReport {
  // The effective session limit applies to internal_workspace_bytes. The accounted total adds the
  // facade; retained compiled state is shared and listed separately. None is a process RSS limit.
  std::size_t internal_workspace_bytes = 0U;
  std::size_t facade_bytes = 0U;
  std::size_t framer_accounted_total_bytes = 0U;
  std::size_t retained_compiled_state_facade_bytes = 0U;
  std::size_t effective_session_limit_bytes = 0U;
  std::size_t allocation_count = 0U;
};

// bytes_consumed 是本次输入已消费的前缀；调用方只需另行提交未消费后缀。
// candidates_delivered 是 sink 调用数，不是 Codec 成功数。其余计数也针对本次调用。
// bytes_discarded 可能包含此前缓存的字节，不能假定它小于本次 bytes_consumed。
struct StreamSubmitResult {
  StreamFramerStatus status = StreamFramerStatus::INVALID_ARGUMENT;
  StreamFramerStopReason stop_reason = StreamFramerStopReason::INPUT_EXHAUSTED;
  std::size_t bytes_consumed = 0U;
  std::size_t candidates_delivered = 0U;
  std::size_t bytes_discarded = 0U;
  std::size_t malformed_candidates = 0U;
  StreamFramingIssue last_issue = StreamFramingIssue::NONE;
  std::size_t work_units_used = 0U;
};

// has_internal_work 表示可无新输入继续推进；缓存半帧不一定有内部工作。
// buffered_bytes 非零不保证 Continue() 会交付候选，可能仍需要外部输入。
struct StreamFramerObservation {
  StreamFramerStatus status = StreamFramerStatus::INVALID_COMPILED_PROTOCOL;
  StreamFramingPhase phase = StreamFramingPhase::COLLECTING;
  std::size_t buffered_bytes = 0U;
  bool has_internal_work = false;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
};

class StreamFramer;
struct StreamFramerCreateResult;

// 每个实例拥有独立流状态并保留冻结 compiled 状态，外部 CompiledProtocol 可先释放。
// 不拥有传输、不做 Decode。只能在空闲时移动；移动保留半帧状态并使源对象失效。
class PAE_API StreamFramer final {
 public:
  StreamFramer(const StreamFramer&) = delete;
  StreamFramer& operator=(const StreamFramer&) = delete;
  StreamFramer(StreamFramer&& other) noexcept;
  StreamFramer& operator=(StreamFramer&& other) noexcept;
  ~StreamFramer();

  // Candidate bytes are borrowed only for the synchronous noexcept callback. This call never owns
  // or retains input[bytes_consumed, input.size). A zero-size input is equivalent to Continue().
  // Moving or destroying this instance during an operation or callback is invalid.
  // 未消费后缀不会被保留；即便 bytes_consumed == input.size，也可能仍有待交付内部候选。
  // 回调重入同实例返回 REENTRANT_CALL，并发受保护操作返回 WORKSPACE_BUSY，不等待。
  [[nodiscard]] StreamSubmitResult Push(ByteView input, FrameSink sink) noexcept;
  // 仅推进已有内部工作，等同空输入 Push；不会凭空补齐半帧或重放已确认的候选。
  [[nodiscard]] StreamSubmitResult Continue(FrameSink sink) noexcept;
  // 丢弃半帧、待交付候选及丢弃恢复状态，不调用 sink，也不改变绑定的 Pipeline。
  [[nodiscard]] StreamFramerStatus Reset() noexcept;

  // Observe is a serialized idle-state query, not a concurrent snapshot.
  // Observe 也受操作保护；MemoryReport 只返回创建时固定的内存计费事实。
  [[nodiscard]] StreamFramerObservation Observe() const noexcept;
  [[nodiscard]] StreamFramingMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit StreamFramer(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;

  friend struct StreamFramerCreateResult;
  friend PAE_API StreamFramerCreateResult CreateStreamFramer(const CompiledProtocol&, std::size_t,
                                                             const StreamFramerOptions&) noexcept;
};

struct StreamFramerCreateResult {
  StreamFramerStatus status = StreamFramerStatus::INVALID_ARGUMENT;
  std::unique_ptr<StreamFramer> framer;
  StreamFramingMemoryReport memory;
};

// Capability is a compiled Pipeline fact only; available=true does not guarantee that caller
// overrides or runtime memory admission will permit CreateStreamFramer().
// available 是能力事实，创建时仍需检查选项、预算与分配结果。
[[nodiscard]] PAE_API StreamFramingCapability
QueryStreamFramingCapability(const CompiledProtocol& compiled, std::size_t pipeline_index) noexcept;

// Returns immutable Pipeline framing facts copied from the frozen Plan. For complete-record input,
// maximum_candidate_frame_bytes is empty. For stream input it is the cross-chunk candidate limit
// (ASCII CRLF includes the terminator), not a per-Push submit or work limit.
// COMPLETE_RECORD 的 maximum_candidate_frame_bytes 为空，不是候选长度限制为 0。
[[nodiscard]] PAE_API PipelineFramingQueryResult QueryPipelineFramingDescription(
    const CompiledProtocol& compiled, std::size_t pipeline_index) noexcept;

// 仅接受 STREAM_CHUNK Pipeline；创建失败不交付 Framer，成功后拥有独立 Workspace。
[[nodiscard]] PAE_API StreamFramerCreateResult
CreateStreamFramer(const CompiledProtocol& compiled, std::size_t pipeline_index,
                   const StreamFramerOptions& options = {}) noexcept;

}  // namespace pae
