#pragma once

#include <cstddef>
#include <memory>

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

struct FrameCandidateView {
  ByteView bytes;
  std::size_t pipeline_index = 0U;
};

enum class FrameSinkAction { CONTINUE, STOP };
using FrameSinkFunction = FrameSinkAction (*)(FrameCandidateView, void*) noexcept;

struct FrameSink {
  FrameSinkFunction function = nullptr;
  void* context = nullptr;
};

// A zero override selects the compiled resource profile default. Overrides can only select values
// accepted by the existing bounded Framer and never remove its hard limits.
struct StreamFramerOptions {
  std::size_t max_submit_bytes = 0U;
  std::size_t max_frames_per_submit = 0U;
  std::size_t max_work_units = 0U;
  std::size_t max_sync_bytes = 0U;
  std::size_t max_session_memory_bytes = 0U;
};

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
  [[nodiscard]] StreamSubmitResult Push(ByteView input, FrameSink sink) noexcept;
  [[nodiscard]] StreamSubmitResult Continue(FrameSink sink) noexcept;
  [[nodiscard]] StreamFramerStatus Reset() noexcept;

  // Observe is a serialized idle-state query, not a concurrent snapshot.
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
[[nodiscard]] PAE_API StreamFramingCapability
QueryStreamFramingCapability(const CompiledProtocol& compiled, std::size_t pipeline_index) noexcept;

[[nodiscard]] PAE_API StreamFramerCreateResult
CreateStreamFramer(const CompiledProtocol& compiled, std::size_t pipeline_index,
                   const StreamFramerOptions& options = {}) noexcept;

}  // namespace pae
