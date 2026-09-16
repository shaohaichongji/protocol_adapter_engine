#include <pae/compiler.h>
#include <pae/host_endpoint.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

#include "codec_test_support.h"
#include "host_endpoint_test_support.h"

namespace {
std::atomic<bool> g_track_allocations{false};
std::atomic<std::size_t> g_allocation_count{0U};
constexpr std::size_t kNoAllocationFailure = static_cast<std::size_t>(-1);
std::atomic<std::size_t> g_fail_after{kNoAllocationFailure};
}  // namespace

void* operator new(std::size_t size) {
  if (g_track_allocations.load(std::memory_order_relaxed)) {
    g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  }
  const std::size_t remaining = g_fail_after.load(std::memory_order_relaxed);
  if (remaining != kNoAllocationFailure) {
    if (remaining == 0U) {
      g_fail_after.store(kNoAllocationFailure, std::memory_order_relaxed);
      throw std::bad_alloc{};
    }
    g_fail_after.store(remaining - 1U, std::memory_order_relaxed);
  }
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

namespace {

static_assert(!std::is_copy_constructible_v<pae::HostEndpoint>);
static_assert(!std::is_move_constructible_v<pae::HostEndpoint>);
static_assert(std::is_copy_constructible_v<pae::HostChannelHandle>);

class Runner final {
 public:
  void Check(bool condition, std::string_view name) {
    if (condition) {
      ++passed_;
      std::cout << "PASS case=" << name << '\n';
    } else {
      ++failed_;
      std::cerr << "FAIL case=" << name << '\n';
    }
  }

  int Finish() const {
    std::cout << "PUBLIC_HOST_ENDPOINT_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " gate=" << (failed_ == 0U ? "PASS" : "FAIL") << '\n';
    return failed_ == 0U ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

pae::CompiledProtocol Compile(const char* path, Runner& runner, std::string_view name) {
  auto result = pae::CompileProtocolJson(ReadFile(path));
  runner.Check(result.Succeeded(), name);
  return result.Succeeded() ? std::move(result).TakeCompiled() : pae::CompiledProtocol{};
}

pae::ByteView Bytes(std::string_view value) noexcept {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

template <std::size_t Size>
pae::ByteView Bytes(const std::array<std::uint8_t, Size>& value) noexcept {
  return {value.data(), value.size()};
}

struct Capture {
  std::size_t observer_calls = 0U;
  std::size_t business_calls = 0U;
  bool observer_stop = false;
  bool business_stop = false;
  bool observer_throws = false;
  bool business_throws = false;
  bool failure_record_empty = true;
  pae::CodecStatus last_codec = pae::CodecStatus::INVALID_ARGUMENT;
  std::size_t last_message = pae::kNoHostMessageIndex;
  std::array<std::uint8_t, 64U> bytes{};
  std::size_t byte_count = 0U;
  std::array<std::uint8_t, 64U> frame{};
  std::size_t frame_count = 0U;
  std::array<std::uint8_t, 64U> business_frame{};
  std::size_t business_frame_count = 0U;
  pae::DecodedRecordView borrowed_record;
};

pae::HostCallbackAction ObserveCandidate(const pae::HostCandidateView& candidate, void* opaque) {
  auto& capture = *static_cast<Capture*>(opaque);
  ++capture.observer_calls;
  capture.last_codec = candidate.decode_status;
  capture.failure_record_empty =
      candidate.decode_status == pae::CodecStatus::OK || !candidate.record.HasValue();
  capture.frame_count = candidate.frame.size;
  for (std::size_t index = 0U; index < candidate.frame.size && index < capture.frame.size();
       ++index) {
    capture.frame[index] = candidate.frame.data[index];
  }
  if (capture.observer_throws) throw std::runtime_error("observer fault");
  return capture.observer_stop ? pae::HostCallbackAction::STOP : pae::HostCallbackAction::CONTINUE;
}

pae::HostCallbackAction CaptureOutput(const pae::HostOutputView& output, void* opaque) {
  auto& capture = *static_cast<Capture*>(opaque);
  ++capture.business_calls;
  capture.last_message = output.message_index;
  if (output.action == pae::HostAction::DECODE) {
    if (!output.record.HasValue()) throw std::runtime_error("missing record");
    capture.borrowed_record = output.record;
    capture.business_frame_count = output.frame.size;
    for (std::size_t index = 0U; index < output.frame.size && index < capture.business_frame.size();
         ++index) {
      capture.business_frame[index] = output.frame.data[index];
    }
    if (output.record.FieldCount() != 0U) {
      const auto field = output.record.Field(0U);
      if (field && field->Kind() == pae::ValueKind::BYTES) {
        const auto bytes = field->Bytes();
        if (bytes) {
          capture.byte_count = bytes->size;
          for (std::size_t index = 0U; index < bytes->size && index < capture.bytes.size();
               ++index) {
            capture.bytes[index] = bytes->data[index];
          }
        }
      }
    }
  } else {
    capture.byte_count = output.bytes.size;
    for (std::size_t index = 0U; index < output.bytes.size && index < capture.bytes.size();
         ++index) {
      capture.bytes[index] = output.bytes.data[index];
    }
  }
  if (capture.business_throws) throw std::runtime_error("business fault");
  return capture.business_stop ? pae::HostCallbackAction::STOP : pae::HostCallbackAction::CONTINUE;
}

pae::HostOutputSink Sink(Capture& capture) noexcept { return {CaptureOutput, &capture}; }
pae::HostCandidateObserver Observer(Capture& capture) noexcept {
  return {ObserveCandidate, &capture};
}

void CountCodecEntry(void* opaque) noexcept {
  static_cast<std::atomic<std::size_t>*>(opaque)->fetch_add(1U, std::memory_order_relaxed);
}

std::array<pae::HostBindingSpec, 3U> AsciiBindings() {
  return {{{"ascii", pae::HostAction::DECODE, 0U, 2U, {}},
           {"record", pae::HostAction::DECODE, 2U, 1U, {}},
           {"ascii", pae::HostAction::ENCODE, 0U, 1U, {}}}};
}

std::array<pae::HostBindingSpec, 2U> BinaryBindings() {
  pae::StreamFramerOptions one_frame;
  one_frame.max_frames_per_submit = 1U;
  return {{{"binary", pae::HostAction::DECODE, 0U, 2U, one_frame},
           {"binary", pae::HostAction::ENCODE, 0U, 1U, {}}}};
}

void CheckBindingAndBudget(Runner& runner, pae::CompiledProtocol& compiled) {
  runner.Check(
      pae::CreateHostEndpoint({}, nullptr, 0U).status == pae::HostStatus::INVALID_COMPILED_PROTOCOL,
      "empty_compiled_rejected");
  runner.Check(
      pae::CreateHostEndpoint(compiled, nullptr, 0U).status == pae::HostStatus::INVALID_ARGUMENT,
      "empty_binding_list_rejected");

  auto valid = AsciiBindings();
  auto duplicate = valid;
  duplicate[1] = duplicate[0];
  runner.Check(pae::CreateHostEndpoint(compiled, duplicate.data(), duplicate.size()).status ==
                   pae::HostStatus::DUPLICATE_BINDING,
               "duplicate_endpoint_action_rejected");

  auto bad = valid;
  bad[0].endpoint_key = {};
  runner.Check(pae::CreateHostEndpoint(compiled, bad.data(), bad.size()).status ==
                   pae::HostStatus::INVALID_BINDING,
               "empty_identity_rejected");
  bad = valid;
  bad[0].pipeline_index = 99U;
  runner.Check(pae::CreateHostEndpoint(compiled, bad.data(), bad.size()).status ==
                   pae::HostStatus::INVALID_BINDING,
               "pipeline_index_rejected");
  bad = valid;
  bad[0].decode_stream_count = 0U;
  runner.Check(pae::CreateHostEndpoint(compiled, bad.data(), bad.size()).status ==
                   pae::HostStatus::INVALID_BINDING,
               "zero_decode_streams_rejected");
  bad = valid;
  bad[2].pipeline_index = 2U;
  runner.Check(pae::CreateHostEndpoint(compiled, bad.data(), bad.size()).status ==
                   pae::HostStatus::INVALID_BINDING,
               "encode_action_capability_rejected");
  bad = valid;
  bad[1].framing_options.max_work_units = 1U;
  runner.Check(pae::CreateHostEndpoint(compiled, bad.data(), bad.size()).status ==
                   pae::HostStatus::INVALID_BINDING,
               "complete_record_framing_override_rejected");

  g_allocation_count.store(0U, std::memory_order_relaxed);
  g_track_allocations.store(true, std::memory_order_relaxed);
  auto created = pae::CreateHostEndpoint(compiled, valid.data(), valid.size());
  g_track_allocations.store(false, std::memory_order_relaxed);
  if (created.memory.allocation_count != g_allocation_count.load()) {
    std::cerr << "creation allocation report=" << created.memory.allocation_count
              << " measured=" << g_allocation_count.load() << '\n';
  }
  std::cout << "PUBLIC_HOST_MEMORY accounted=" << created.memory.host_accounted_total_bytes
            << " allocations=" << created.memory.allocation_count
            << " measured_allocations=" << g_allocation_count.load() << '\n';
  runner.Check(created.status == pae::HostStatus::OK && created.host &&
                   created.memory.allocation_count == g_allocation_count.load(),
               "creation_allocation_count_exact");
  const auto& memory = created.memory;
  const std::size_t sum = memory.facade_bytes + memory.scope_payload_bytes +
                          memory.binding_storage_bytes + memory.identity_storage_bytes +
                          memory.channel_storage_bytes + memory.codec_accounted_bytes +
                          memory.framer_accounted_bytes + memory.encode_buffer_bytes;
  runner.Check(
      memory.host_accounted_total_bytes == sum && memory.retained_compiled_state_facade_bytes != 0U,
      "memory_categories_exact_sum");

  pae::HostLimits exact;
  exact.max_accounted_bytes = memory.host_accounted_total_bytes;
  auto exact_result = pae::CreateHostEndpoint(compiled, valid.data(), valid.size(), exact);
  --exact.max_accounted_bytes;
  auto below_result = pae::CreateHostEndpoint(compiled, valid.data(), valid.size(), exact);
  runner.Check(exact_result.status == pae::HostStatus::OK && exact_result.host,
               "aggregate_budget_exact");
  runner.Check(
      below_result.status == pae::HostStatus::RESOURCE_LIMIT_EXCEEDED && !below_result.host,
      "aggregate_budget_minus_one");

  bool atomic_failures = true;
  for (std::size_t allocation = 0U; allocation < memory.allocation_count; ++allocation) {
    g_fail_after.store(allocation, std::memory_order_relaxed);
    auto failed = pae::CreateHostEndpoint(compiled, valid.data(), valid.size());
    g_fail_after.store(kNoAllocationFailure, std::memory_order_relaxed);
    atomic_failures = atomic_failures && !failed.host && failed.status != pae::HostStatus::OK;
  }
  auto recovered = pae::CreateHostEndpoint(compiled, valid.data(), valid.size());
  runner.Check(atomic_failures && recovered.status == pae::HostStatus::OK && recovered.host,
               "creation_failure_atomic_and_recoverable");
}

void CheckCompleteRecordAndHandles(Runner& runner, pae::CompiledProtocol compiled) {
  auto bindings = AsciiBindings();
  auto created = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  runner.Check(created.status == pae::HostStatus::OK && created.host, "ascii_host_create");
  if (!created.host) return;
  auto& host = *created.host;
  const auto record = host.Find("record", pae::HostAction::DECODE);
  const auto record_second = host.Find("record", pae::HostAction::DECODE, 1U);
  runner.Check(
      record.status == pae::HostStatus::OK &&
          record_second.status == pae::HostStatus::INVALID_BINDING &&
          host.Find("Record", pae::HostAction::DECODE).status == pae::HostStatus::INVALID_BINDING,
      "identity_exact_and_stream_bound");

  Capture capture;
  std::atomic<std::size_t> codec_calls{0U};
  pae::public_api_internal::test_only::SetCodecEnteredHook(CountCodecEntry, &codec_calls);
  const auto decoded =
      host.Decode(record.handle, Bytes("ONLY\r\n"), Sink(capture), Observer(capture));
  const pae::DecodedRecordView borrowed_record = capture.borrowed_record;
  pae::public_api_internal::test_only::SetCodecEnteredHook(nullptr, nullptr);
  runner.Check(decoded.status == pae::HostStatus::OK && decoded.bytes_consumed == 6U &&
                   decoded.candidates == 1U && decoded.decode_attempts == 1U &&
                   decoded.decode_successes == 1U && decoded.decode_failures == 0U &&
                   decoded.observer_callbacks_returned == 1U &&
                   decoded.business_callbacks_returned == 1U && capture.last_message == 1U &&
                   capture.frame_count == 6U && capture.business_frame_count == 6U &&
                   capture.frame[0] == capture.business_frame[0] &&
                   capture.frame[5] == capture.business_frame[5] && codec_calls.load() == 1U,
               "complete_zero_field_single_decode_and_delivery");

  capture = {};
  const auto failed =
      host.Decode(record.handle, Bytes("BAD\r\n"), Sink(capture), Observer(capture));
  runner.Check(failed.status == pae::HostStatus::CODEC_FAILED && failed.bytes_consumed == 5U &&
                   failed.decode_failures == 1U && failed.observer_callbacks_returned == 1U &&
                   failed.business_callbacks_returned == 0U && capture.failure_record_empty,
               "failed_candidate_observed_without_success_fields");
  runner.Check(host.Push(record.handle, Bytes("ONLY\r\n"), Sink(capture)).status ==
                   pae::HostStatus::WRONG_INPUT_KIND,
               "complete_record_push_rejected");

  const auto old = record.handle;
  runner.Check(host.Reset(old) == pae::HostStatus::OK &&
                   host.Observe(old).status == pae::HostStatus::STALE_HANDLE &&
                   !borrowed_record.HasValue(),
               "reset_invalidates_old_handle_and_codec_view");
  const auto fresh = host.Find("record", pae::HostAction::DECODE);
  runner.Check(fresh.status == pae::HostStatus::OK && host.Observe(fresh.handle).generation == 2U,
               "find_reacquires_new_generation");

  auto other_created = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  runner.Check(other_created.host->Observe(fresh.handle).status == pae::HostStatus::FOREIGN_HANDLE,
               "foreign_handle_rejected");
  pae::HostChannelHandle expired;
  {
    auto temporary = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
    expired = temporary.host->Find("record", pae::HostAction::DECODE).handle;
  }
  runner.Check(host.Observe(expired).status == pae::HostStatus::EXPIRED_HANDLE,
               "destroyed_owner_handle_expired");

  runner.Check(pae::detail::HostEndpointTestAccess::SetGeneration(
                   host, fresh.handle, (std::numeric_limits<std::uint64_t>::max)()),
               "generation_test_hook");
  const auto exhausted = host.Find("record", pae::HostAction::DECODE);
  runner.Check(
      exhausted.status == pae::HostStatus::OK &&
          host.Reset(exhausted.handle) == pae::HostStatus::GENERATION_EXHAUSTED &&
          host.Observe(exhausted.handle).generation == (std::numeric_limits<std::uint64_t>::max)(),
      "generation_exhaustion_fails_closed");

  compiled = pae::CompiledProtocol{};
  Capture retained;
  const auto other_record = other_created.host->Find("record", pae::HostAction::DECODE);
  runner.Check(
      other_created.host->Decode(other_record.handle, Bytes("ONLY\r\n"), Sink(retained)).status ==
          pae::HostStatus::OK,
      "host_retains_compiled_state");
}

void CheckBinaryStreamAndEncode(Runner& runner, pae::CompiledProtocol& compiled) {
  auto bindings = BinaryBindings();
  auto created = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  runner.Check(created.status == pae::HostStatus::OK && created.host, "binary_host_create");
  if (!created.host) return;
  auto& host = *created.host;
  auto rx = host.Find("binary", pae::HostAction::DECODE, 0U);
  const auto rx2 = host.Find("binary", pae::HostAction::DECODE, 1U);
  auto tx = host.Find("binary", pae::HostAction::ENCODE);
  Capture capture;
  const std::array<std::uint8_t, 6U> pair{{0xAAU, 0x00U, 0x01U, 0xAAU, 0x00U, 0x02U}};
  std::atomic<std::size_t> codec_calls{0U};
  pae::public_api_internal::test_only::SetCodecEnteredHook(CountCodecEntry, &codec_calls);
  const auto first = host.Push(rx.handle, Bytes(pair), Sink(capture), Observer(capture));
  const auto second = host.Continue(rx.handle, Sink(capture), Observer(capture));
  pae::public_api_internal::test_only::SetCodecEnteredHook(nullptr, nullptr);
  runner.Check(first.status == pae::HostStatus::OK && first.bytes_consumed == pair.size() &&
                   first.candidates == 1U &&
                   first.framing.stop_reason == pae::StreamFramerStopReason::WORK_BUDGET_REACHED &&
                   second.status == pae::HostStatus::OK && second.bytes_consumed == 0U &&
                   second.candidates == 1U && capture.business_calls == 2U &&
                   capture.observer_calls == 2U && codec_calls.load() == 2U,
               "stream_pending_continue_single_decode_each");

  Capture empty_capture;
  const auto empty_first =
      host.Push(rx2.handle, Bytes(pair), Sink(empty_capture), Observer(empty_capture));
  const auto empty_second =
      host.Push(rx2.handle, pae::ByteView{}, Sink(empty_capture), Observer(empty_capture));
  runner.Check(empty_first.bytes_consumed == first.bytes_consumed &&
                   empty_first.candidates == first.candidates &&
                   empty_first.framing.stop_reason == first.framing.stop_reason &&
                   empty_second.bytes_consumed == second.bytes_consumed &&
                   empty_second.candidates == second.candidates &&
                   empty_second.framing.stop_reason == second.framing.stop_reason &&
                   empty_capture.business_calls == 2U,
               "empty_push_equivalent_to_continue");

  capture = {};
  capture.observer_stop = true;
  const auto stopped = host.Push(rx.handle, Bytes(pair), Sink(capture), Observer(capture));
  runner.Check(stopped.bytes_consumed == 3U && stopped.decode_successes == 1U &&
                   stopped.observer_callbacks_returned == 1U &&
                   stopped.business_callbacks_returned == 1U &&
                   stopped.framing.stop_reason == pae::StreamFramerStopReason::SINK_STOP,
               "observer_stop_keeps_current_success_and_suffix");
  const pae::ByteView suffix{pair.data() + stopped.bytes_consumed,
                             pair.size() - stopped.bytes_consumed};
  capture = {};
  runner.Check(
      host.Push(rx.handle, suffix, Sink(capture), Observer(capture)).decode_successes == 1U,
      "unconsumed_suffix_resubmitted_once");

  Capture failed_capture;
  const std::array<std::uint8_t, 3U> bad{{0x00U, 0x00U, 0x00U}};
  const auto failed =
      host.Push(rx.handle, Bytes(bad), Sink(failed_capture), Observer(failed_capture));
  runner.Check(failed.status == pae::HostStatus::CODEC_FAILED && failed.decode_failures == 1U &&
                   failed_capture.business_calls == 0U && failed_capture.failure_record_empty,
               "stream_codec_failure_not_delivered_or_replayed");

  Capture observer_fault;
  observer_fault.observer_throws = true;
  const auto fault =
      host.Push(rx.handle, Bytes(pair), Sink(observer_fault), Observer(observer_fault));
  Capture isolated;
  const std::array<std::uint8_t, 3U> one{{0xAAU, 0x00U, 0x03U}};
  const auto other = host.Push(rx2.handle, Bytes(one), Sink(isolated), Observer(isolated));
  runner.Check(fault.status == pae::HostStatus::CALLBACK_FAILED && fault.bytes_consumed == 3U &&
                   fault.observer_callbacks_returned == 0U &&
                   fault.business_callbacks_returned == 0U && fault.reset_required &&
                   host.Push(rx.handle, Bytes(one), Sink(isolated)).status ==
                       pae::HostStatus::RESET_REQUIRED &&
                   other.status == pae::HostStatus::OK && other.decode_successes == 1U,
               "observer_exception_exact_consumption_and_channel_isolation");
  runner.Check(host.Reset(rx.handle) == pae::HostStatus::OK &&
                   host.Observe(rx.handle).status == pae::HostStatus::STALE_HANDLE,
               "stream_fault_reset_stales_handle");
  rx = host.Find("binary", pae::HostAction::DECODE, 0U);

  Capture business_fault;
  business_fault.business_throws = true;
  const auto business = host.Push(rx.handle, Bytes(one), Sink(business_fault));
  runner.Check(business.status == pae::HostStatus::CALLBACK_FAILED &&
                   business.business_callbacks_returned == 0U && business.reset_required,
               "business_exception_requires_target_reset");
  runner.Check(host.Reset(rx.handle) == pae::HostStatus::OK, "business_fault_reset");
  rx = host.Find("binary", pae::HostAction::DECODE, 0U);

  const std::array<pae::EncodeValue, 1U> values{{pae::EncodeValue::UInt64({0U, 0U}, 7U)}};
  Capture encoded;
  const auto encode = host.Encode(tx.handle, 0U, values.data(), values.size(), Sink(encoded));
  runner.Check(encode.status == pae::HostStatus::OK && encode.bytes_produced == 3U &&
                   encoded.byte_count == 3U && encoded.bytes[0] == 0xAAU &&
                   encoded.bytes[1] == 0x00U && encoded.bytes[2] == 0x07U,
               "encode_preallocated_output_delivered");
  runner.Check(
      host.Decode(tx.handle, Bytes(one), Sink(encoded)).status == pae::HostStatus::WRONG_ACTION,
      "wrong_action_rejected");

  Capture encode_fault;
  encode_fault.business_throws = true;
  const auto encode_failed =
      host.Encode(tx.handle, 0U, values.data(), values.size(), Sink(encode_fault));
  runner.Check(encode_failed.status == pae::HostStatus::CALLBACK_FAILED &&
                   encode_failed.reset_required &&
                   host.Encode(tx.handle, 0U, values.data(), values.size(), Sink(encoded)).status ==
                       pae::HostStatus::RESET_REQUIRED &&
                   host.Reset(tx.handle) == pae::HostStatus::OK,
               "encode_callback_fault_isolated_and_resettable");
  runner.Check(host.Observe(tx.handle).status == pae::HostStatus::STALE_HANDLE,
               "encode_reset_stales_old_handle");
  tx = host.Find("binary", pae::HostAction::ENCODE);

  Capture no_alloc;
  g_allocation_count.store(0U, std::memory_order_relaxed);
  g_track_allocations.store(true, std::memory_order_relaxed);
  const auto hot_push = host.Push(rx.handle, Bytes(one), Sink(no_alloc), Observer(no_alloc));
  const auto hot_encode = host.Encode(tx.handle, 0U, values.data(), values.size(), Sink(no_alloc));
  const auto hot_observe = host.Observe(rx.handle);
  g_track_allocations.store(false, std::memory_order_relaxed);
  runner.Check(hot_push.status == pae::HostStatus::OK && hot_encode.status == pae::HostStatus::OK &&
                   hot_observe.status == pae::HostStatus::OK && g_allocation_count.load() == 0U,
               "hot_operations_zero_engine_allocations");
}

void CheckAsciiStream(Runner& runner, pae::CompiledProtocol& compiled) {
  auto bindings = AsciiBindings();
  auto created = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  auto rx = created.host->Find("ascii", pae::HostAction::DECODE);
  Capture capture;
  const auto first =
      created.host->Push(rx.handle, Bytes("RX A!OK\r"), Sink(capture), Observer(capture));
  const auto second =
      created.host->Push(rx.handle, Bytes("\nONLY\r\n"), Sink(capture), Observer(capture));
  runner.Check(first.status == pae::HostStatus::OK && first.decode_attempts == 0U &&
                   second.decode_attempts == 2U && second.decode_successes == 2U &&
                   capture.business_calls == 2U && capture.observer_calls == 2U,
               "ascii_split_crlf_and_glued_records");

  Capture observer_stop;
  observer_stop.observer_stop = true;
  const auto observed_stop = created.host->Push(rx.handle, Bytes("ONLY\r\nONLY\r\n"),
                                                Sink(observer_stop), Observer(observer_stop));
  runner.Check(observed_stop.bytes_consumed == 6U && observed_stop.decode_successes == 1U &&
                   observed_stop.observer_callbacks_returned == 1U &&
                   observed_stop.business_callbacks_returned == 1U &&
                   observed_stop.framing.stop_reason == pae::StreamFramerStopReason::SINK_STOP,
               "observer_stop_delivers_current_success_once");
  Capture observer_failure_stop;
  observer_failure_stop.observer_stop = true;
  const auto failure_stop =
      created.host->Push(rx.handle, Bytes("BAD\r\nONLY\r\n"), Sink(observer_failure_stop),
                         Observer(observer_failure_stop));
  runner.Check(failure_stop.status == pae::HostStatus::CODEC_FAILED &&
                   failure_stop.bytes_consumed == 5U && failure_stop.decode_failures == 1U &&
                   failure_stop.observer_callbacks_returned == 1U &&
                   failure_stop.business_callbacks_returned == 0U,
               "observer_stop_failed_candidate_never_delivered");
  Capture business_stop;
  business_stop.business_stop = true;
  const auto delivered_stop = created.host->Push(rx.handle, Bytes("ONLY\r\nONLY\r\n"),
                                                 Sink(business_stop), Observer(business_stop));
  runner.Check(delivered_stop.bytes_consumed == 6U && delivered_stop.decode_successes == 1U &&
                   delivered_stop.business_callbacks_returned == 1U &&
                   delivered_stop.framing.stop_reason == pae::StreamFramerStopReason::SINK_STOP,
               "business_stop_commits_current_success_once");

  Capture encode_capture;
  auto tx = created.host->Find("ascii", pae::HostAction::ENCODE);
  const std::array<std::uint8_t, 1U> name{{'A'}};
  const std::array<std::uint8_t, 1U> tag{{'Z'}};
  const std::array<pae::EncodeValue, 2U> values{{pae::EncodeValue::Bytes({0U, 0U}, Bytes(name)),
                                                 pae::EncodeValue::Bytes({0U, 2U}, Bytes(tag))}};
  const auto encoded =
      created.host->Encode(tx.handle, 0U, values.data(), values.size(), Sink(encode_capture));
  const std::string_view expected = "TX A!Z\r\n";
  bool same = encode_capture.byte_count == expected.size();
  for (std::size_t index = 0U; same && index < expected.size(); ++index) {
    same = encode_capture.bytes[index] == static_cast<std::uint8_t>(expected[index]);
  }
  runner.Check(encoded.status == pae::HostStatus::OK && same, "ascii_encode_public_values");

  Capture literal_capture;
  const auto literal = created.host->Encode(tx.handle, 2U, nullptr, 0U, Sink(literal_capture));
  runner.Check(literal.status == pae::HostStatus::OK && literal.bytes_produced == 6U &&
                   literal_capture.last_message == 2U && literal_capture.byte_count == 6U &&
                   created.memory.encode_buffer_bytes == 11U,
               "same_handle_selects_second_message_with_max_buffer");

  std::atomic<std::size_t> codec_calls{0U};
  pae::public_api_internal::test_only::SetCodecEnteredHook(CountCodecEntry, &codec_calls);
  const auto decode_only = created.host->Encode(tx.handle, 1U, nullptr, 0U, Sink(literal_capture));
  const auto out_of_range =
      created.host->Encode(tx.handle, 99U, nullptr, 0U, Sink(literal_capture));
  pae::public_api_internal::test_only::SetCodecEnteredHook(nullptr, nullptr);
  runner.Check(decode_only.status == pae::HostStatus::INVALID_ARGUMENT &&
                   out_of_range.status == pae::HostStatus::INVALID_ARGUMENT &&
                   !decode_only.codec_attempted && !out_of_range.codec_attempted &&
                   codec_calls.load() == 0U,
               "invalid_message_selectors_rejected_before_codec");

  const std::array<pae::HostBindingSpec, 1U> other_pipeline_binding{
      {{"other", pae::HostAction::ENCODE, 1U, 1U, {}}}};
  auto other_pipeline = pae::CreateHostEndpoint(compiled, other_pipeline_binding.data(),
                                                other_pipeline_binding.size());
  const auto other_tx = other_pipeline.host->Find("other", pae::HostAction::ENCODE);
  codec_calls.store(0U);
  pae::public_api_internal::test_only::SetCodecEnteredHook(CountCodecEntry, &codec_calls);
  const auto cross_pipeline = other_pipeline.host->Encode(other_tx.handle, 0U, values.data(),
                                                          values.size(), Sink(literal_capture));
  pae::public_api_internal::test_only::SetCodecEnteredHook(nullptr, nullptr);
  runner.Check(cross_pipeline.status == pae::HostStatus::INVALID_ARGUMENT &&
                   !cross_pipeline.codec_attempted && codec_calls.load() == 0U,
               "cross_pipeline_message_rejected_before_codec");

  Capture encode_fault;
  encode_fault.business_throws = true;
  const auto callback_failed =
      created.host->Encode(tx.handle, 0U, values.data(), values.size(), Sink(encode_fault));
  const auto fault_precedes_selector =
      created.host->Encode(tx.handle, 99U, nullptr, 0U, Sink(literal_capture));
  runner.Check(callback_failed.status == pae::HostStatus::CALLBACK_FAILED &&
                   callback_failed.bytes_produced == expected.size() &&
                   callback_failed.business_callbacks_returned == 0U &&
                   fault_precedes_selector.status == pae::HostStatus::RESET_REQUIRED &&
                   created.host->Reset(tx.handle) == pae::HostStatus::OK,
               "encode_callback_fault_preserves_generation_fact_and_requires_reset");
  tx = created.host->Find("ascii", pae::HostAction::ENCODE);
  Capture after_reset;
  runner.Check(created.host->Encode(tx.handle, 2U, nullptr, 0U, Sink(after_reset)).status ==
                       pae::HostStatus::OK &&
                   after_reset.last_message == 2U,
               "reset_refind_can_select_different_message");
}

struct ReentrantContext {
  pae::HostEndpoint* host = nullptr;
  pae::HostChannelHandle handle;
  pae::HostStatus observe = pae::HostStatus::OK;
  pae::HostStatus reset = pae::HostStatus::OK;
  pae::HostStatus find = pae::HostStatus::OK;
};

pae::HostCallbackAction DirectReentrant(const pae::HostOutputView&, void* opaque) {
  auto& context = *static_cast<ReentrantContext*>(opaque);
  context.observe = context.host->Observe(context.handle).status;
  context.reset = context.host->Reset(context.handle);
  context.find = context.host->Find("record", pae::HostAction::DECODE).status;
  return pae::HostCallbackAction::CONTINUE;
}

struct IndirectContext {
  pae::HostEndpoint* first = nullptr;
  pae::HostEndpoint* second = nullptr;
  pae::HostChannelHandle first_handle;
  pae::HostChannelHandle second_handle;
  pae::HostOperationResult second_result;
  pae::HostStatus loop_observe = pae::HostStatus::OK;
};

pae::HostCallbackAction IndirectSecond(const pae::HostOutputView&, void* opaque) {
  auto& context = *static_cast<IndirectContext*>(opaque);
  context.loop_observe = context.first->Observe(context.first_handle).status;
  return pae::HostCallbackAction::CONTINUE;
}

pae::HostCallbackAction IndirectFirst(const pae::HostOutputView&, void* opaque) {
  auto& context = *static_cast<IndirectContext*>(opaque);
  context.second_result =
      context.second->Decode(context.second_handle, Bytes("ONLY\r\n"), {IndirectSecond, &context});
  return pae::HostCallbackAction::CONTINUE;
}

struct BlockingContext {
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
};

pae::HostCallbackAction Block(const pae::HostOutputView&, void* opaque) {
  auto& context = *static_cast<BlockingContext*>(opaque);
  context.entered.store(true, std::memory_order_release);
  while (!context.release.load(std::memory_order_acquire)) std::this_thread::yield();
  return pae::HostCallbackAction::CONTINUE;
}

bool WaitUntilEntered(const BlockingContext& context) {
  for (std::size_t attempt = 0U; attempt < 1000000U; ++attempt) {
    if (context.entered.load(std::memory_order_acquire)) return true;
    std::this_thread::yield();
  }
  return false;
}

void CheckGuards(Runner& runner, pae::CompiledProtocol& compiled) {
  auto bindings = AsciiBindings();
  auto first = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  auto second = pae::CreateHostEndpoint(compiled, bindings.data(), bindings.size());
  const auto first_handle = first.host->Find("record", pae::HostAction::DECODE).handle;
  const auto second_handle = second.host->Find("record", pae::HostAction::DECODE).handle;

  ReentrantContext direct{first.host.get(), first_handle};
  const auto direct_result =
      first.host->Decode(first_handle, Bytes("ONLY\r\n"), {DirectReentrant, &direct});
  runner.Check(direct_result.status == pae::HostStatus::OK &&
                   direct.observe == pae::HostStatus::REENTRANT_CALL &&
                   direct.reset == pae::HostStatus::REENTRANT_CALL &&
                   direct.find == pae::HostStatus::REENTRANT_CALL,
               "direct_callback_reentry_classified");

  IndirectContext indirect{first.host.get(), second.host.get(), first_handle, second_handle};
  const auto indirect_result =
      first.host->Decode(first_handle, Bytes("ONLY\r\n"), {IndirectFirst, &indirect});
  runner.Check(indirect_result.status == pae::HostStatus::OK &&
                   indirect.second_result.status == pae::HostStatus::OK &&
                   indirect.loop_observe == pae::HostStatus::REENTRANT_CALL,
               "indirect_a_b_a_reentry_classified");

  BlockingContext blocking;
  pae::HostOperationResult background;
  std::thread worker([&] {
    background = first.host->Decode(first_handle, Bytes("ONLY\r\n"), {Block, &blocking});
  });
  const bool entered = WaitUntilEntered(blocking);
  const auto busy_observe = first.host->Observe(first_handle);
  const auto busy_find = first.host->Find("record", pae::HostAction::DECODE);
  const auto busy_reset = first.host->Reset(first_handle);
  blocking.release.store(true, std::memory_order_release);
  worker.join();
  runner.Check(entered && busy_observe.status == pae::HostStatus::WORKSPACE_BUSY &&
                   busy_find.status == pae::HostStatus::WORKSPACE_BUSY &&
                   busy_reset == pae::HostStatus::WORKSPACE_BUSY &&
                   background.status == pae::HostStatus::OK,
               "cross_thread_operations_busy");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  Runner runner;
  auto binary = Compile(argv[1], runner, "binary_compile");
  auto ascii = Compile(argv[2], runner, "ascii_compile");
  if (!binary.HasValue() || !ascii.HasValue()) return runner.Finish();
  CheckBindingAndBudget(runner, ascii);
  CheckCompleteRecordAndHandles(runner, Compile(argv[2], runner, "handle_compile"));
  CheckBinaryStreamAndEncode(runner, binary);
  CheckAsciiStream(runner, ascii);
  CheckGuards(runner, ascii);
  return runner.Finish();
}
