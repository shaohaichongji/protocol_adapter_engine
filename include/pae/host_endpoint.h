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

struct HostBindingSpec {
  std::string_view endpoint_key;
  HostAction action = HostAction::DECODE;
  std::size_t pipeline_index = 0U;
  std::size_t decode_stream_count = 1U;
  StreamFramerOptions framing_options;
};

struct HostLimits {
  std::size_t max_bindings = 64U;
  std::size_t max_channels = 64U;
  std::size_t max_identity_bytes = 256U;
  std::size_t max_accounted_bytes = 64U * 1024U * 1024U;
  CompleteRecordCodecOptions codec_options;
};

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

struct HostCandidateObserver {
  HostCandidateObserverFunction function = nullptr;
  void* context = nullptr;
};

struct HostOutputView {
  HostAction action = HostAction::DECODE;
  std::uint64_t generation = 0U;
  std::size_t message_index = kNoHostMessageIndex;
  DecodedRecordView record;
  ByteView frame;  // Decode only; borrows the current candidate.
  ByteView bytes;  // Encode only; borrows the channel output buffer.
};

using HostOutputSinkFunction = HostCallbackAction (*)(const HostOutputView&, void*);

struct HostOutputSink {
  HostOutputSinkFunction function = nullptr;
  void* context = nullptr;
};

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
class PAE_API HostEndpoint final {
 public:
  HostEndpoint(const HostEndpoint&) = delete;
  HostEndpoint& operator=(const HostEndpoint&) = delete;
  HostEndpoint(HostEndpoint&&) = delete;
  HostEndpoint& operator=(HostEndpoint&&) = delete;
  ~HostEndpoint();

  [[nodiscard]] HostFindResult Find(std::string_view endpoint_key, HostAction action,
                                    std::size_t decode_stream_index = 0U) const noexcept;
  [[nodiscard]] HostObservation Observe(const HostChannelHandle& handle) const noexcept;
  [[nodiscard]] HostStatus Reset(const HostChannelHandle& handle) noexcept;

  [[nodiscard]] HostOperationResult Decode(const HostChannelHandle& handle, ByteView frame,
                                           HostOutputSink sink,
                                           HostCandidateObserver observer = {}) noexcept;
  [[nodiscard]] HostOperationResult Push(const HostChannelHandle& handle, ByteView input,
                                         HostOutputSink sink,
                                         HostCandidateObserver observer = {}) noexcept;
  [[nodiscard]] HostOperationResult Continue(const HostChannelHandle& handle, HostOutputSink sink,
                                             HostCandidateObserver observer = {}) noexcept;
  [[nodiscard]] HostOperationResult Encode(const HostChannelHandle& handle,
                                           std::size_t message_index, const EncodeValue* values,
                                           std::size_t value_count, HostOutputSink sink) noexcept;

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

[[nodiscard]] PAE_API HostEndpointCreateResult
CreateHostEndpoint(const CompiledProtocol& compiled, const HostBindingSpec* bindings,
                   std::size_t binding_count, const HostLimits& limits = {}) noexcept;

}  // namespace pae
