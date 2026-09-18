#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

#include "public_ascii_host_adapter.h"

using namespace pae::protocol_lab_ascii::public_offline;

static_assert(std::is_enum_v<StreamDiagnostic>);
static_assert(std::is_nothrow_copy_assignable_v<StreamDiagnostic>);

namespace {
std::atomic<bool> g_fail_next_allocation{false};
}

void* operator new(std::size_t size) {
  if (g_fail_next_allocation.exchange(false, std::memory_order_relaxed)) throw std::bad_alloc{};
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

#undef assert
#define assert(expression)                                              \
  do {                                                                  \
    if (!(expression)) {                                                \
      std::cerr << "PUBLIC_ASCII_STREAM_CHECK_FAILED line=" << __LINE__ \
                << " expression=" << #expression << '\n';               \
      return 1;                                                         \
    }                                                                   \
  } while (false)

namespace {

std::string Read(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::uint8_t*>(text.data()),
          reinterpret_cast<const std::uint8_t*>(text.data()) + text.size()};
}

std::string_view Text(const std::vector<std::uint8_t>& bytes) {
  return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}

HostPrepareResult Prepare(std::string_view json, std::vector<HostBinding> bindings,
                          const Limits& limits = {}, std::size_t previous = 0U) {
  auto compiled = pae::CompileProtocolJson(json);
  if (!compiled.Succeeded()) return {};
  return HostAdapter::Create(std::move(compiled).TakeCompiled(), std::move(bindings), limits,
                             previous);
}

HostBinding StreamBinding(const pae::StreamFramerOptions& options = {}) {
  HostBinding binding{"stream", pae::HostAction::DECODE, 0U};
  binding.framing_options = options;
  return binding;
}

int RunDiagnosticAllocationProbe(const std::string& root, std::string_view scenario) {
#if defined(_WIN32)
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
  std::set_terminate([] { std::_Exit(86); });
  const auto stream_json = Read(root + "/examples/config/synthetic_ascii_stream_slice.pae.json");
  auto prepared = Prepare(stream_json, {StreamBinding()});
  if (!prepared.adapter) return 2;
  auto chunk = Bytes("ONLY\r\n");

  if (scenario == "reject") {
    g_fail_next_allocation.store(true, std::memory_order_relaxed);
    const auto rejected = prepared.adapter->SubmitStreamChunk(99U, 0U, chunk);
    const bool allocation_was_untouched =
        g_fail_next_allocation.exchange(false, std::memory_order_relaxed);
    if (!allocation_was_untouched || rejected.status != LocalStatus::INVALID_INPUT ||
        rejected.diagnostic != StreamDiagnostic::CHANNEL_NOT_ASCII_STREAM) {
      return 3;
    }
  } else if (scenario == "candidate") {
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
    prepared.adapter->FailNextCallbackAllocationForTesting();
    const auto failed = prepared.adapter->SubmitStreamChunk(0U, 0U, chunk);
    if (failed.status != LocalStatus::ALLOCATION_FAILED ||
        failed.diagnostic != StreamDiagnostic::CANDIDATE_COPY_FAILED_RESET_REQUIRED ||
        !failed.candidate || failed.host.bytes_consumed != chunk.size() ||
        !failed.after.reset_required) {
      return 4;
    }
    StreamDiagnostic copied = StreamDiagnostic::NONE;
    g_fail_next_allocation.store(true, std::memory_order_relaxed);
    copied = failed.diagnostic;
    const bool allocation_was_untouched =
        g_fail_next_allocation.exchange(false, std::memory_order_relaxed);
    if (!allocation_was_untouched || copied != failed.diagnostic) return 5;
#else
    return 6;
#endif
  } else {
    return 7;
  }
  std::cout << "PUBLIC_ASCII_STREAM_NOEXCEPT_DIAGNOSTIC_PASS " << scenario << '\n';
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 4 && std::string_view{argv[1]} == "--diagnostic-allocation-probe") {
    return RunDiagnosticAllocationProbe(argv[3], argv[2]);
  }
  assert(argc == 2);
  const std::string root = argv[1];
  const auto stream_json = Read(root + "/examples/config/synthetic_ascii_stream_slice.pae.json");
  const auto complete_json = Read(root + "/examples/config/synthetic_ascii_text_slice.pae.json");

  auto prepared = Prepare(stream_json, {StreamBinding()});
  assert(prepared.status == LocalStatus::OK && prepared.host_status == pae::HostStatus::OK &&
         prepared.adapter);
  auto& adapter = *prepared.adapter;
  assert(adapter.FlowCount(0U) == 2U && adapter.StreamChunkCapacity(0U, 0U) > 0U);
  auto initial = adapter.ObserveStream(0U, 0U);
  assert(initial && initial->maximum_candidate_frame_bytes == 12U &&
         initial->effective_max_submit_bytes >= 12U && initial->frozen_input_bytes == 0U &&
         adapter.StreamChunkCapacity(0U, 0U) ==
             (std::min)(std::size_t{64U * 1024U}, initial->effective_max_submit_bytes) &&
         !initial->reset_required);
  const auto empty = adapter.SubmitStreamChunk(0U, 0U, {});
  assert(empty.status == LocalStatus::INVALID_INPUT &&
         empty.diagnostic == StreamDiagnostic::INVALID_CHUNK_OR_CONTINUE_REQUIRED);
  const auto oversized = adapter.SubmitStreamChunk(
      0U, 0U, std::vector<std::uint8_t>(adapter.StreamChunkCapacity(0U, 0U) + 1U));
  assert(oversized.status == LocalStatus::INVALID_INPUT &&
         oversized.diagnostic == StreamDiagnostic::INVALID_CHUNK_OR_CONTINUE_REQUIRED);

  auto first = adapter.SubmitStreamChunk(0U, 0U, Bytes("RX A!OK\r"));
  assert(first.status == LocalStatus::OK && first.host_called && !first.candidate &&
         first.host.framing.bytes_consumed == 8U && first.after.buffered_bytes == 8U);
  auto second = adapter.SubmitStreamChunk(0U, 0U, Bytes("\n"));
  assert(second.status == LocalStatus::OK && second.candidate &&
         second.candidate->local_status == LocalStatus::OK &&
         Text(second.candidate->frame) == "RX A!OK\r\n" && second.candidate->fields.size() == 2U &&
         second.host.decode_attempts == 1U && second.host.observer_callbacks_returned == 1U &&
         second.host.business_callbacks_returned == 1U);

  auto glued = adapter.SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\nBAD\r\nONLY\r\n"));
  assert(glued.status == LocalStatus::OK && glued.candidate &&
         glued.candidate->local_status == LocalStatus::OK && glued.host.bytes_consumed == 6U &&
         glued.after.frozen_cursor == 6U && glued.after.frozen_input_bytes == 17U);
  const auto submit_before_continue = adapter.SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\n"));
  assert(submit_before_continue.status == LocalStatus::INVALID_INPUT &&
         submit_before_continue.diagnostic == StreamDiagnostic::INVALID_CHUNK_OR_CONTINUE_REQUIRED);
  auto failed = adapter.ContinueStream(0U, 0U);
  assert(failed.status == LocalStatus::OK && failed.candidate &&
         failed.candidate->local_status == LocalStatus::CODEC_FAILED &&
         Text(failed.candidate->diagnostic_input_frame) == "BAD\r\n" &&
         failed.host.status == pae::HostStatus::CODEC_FAILED && !failed.after.reset_required);
  auto recovered = adapter.ContinueStream(0U, 0U);
  assert(recovered.status == LocalStatus::OK && recovered.candidate &&
         recovered.candidate->local_status == LocalStatus::OK &&
         recovered.candidate->fields.empty() && recovered.after.frozen_input_bytes == 0U &&
         recovered.after.total_candidates == 4U && recovered.after.total_decode_successes == 3U &&
         recovered.after.total_observer_callbacks == 4U &&
         recovered.after.total_business_callbacks == 3U);

  assert(adapter.SubmitStreamChunk(0U, 1U, Bytes("ON")).status == LocalStatus::OK);
  const auto flow_one_before_reset = adapter.ObserveStream(0U, 1U);
  assert(flow_one_before_reset && flow_one_before_reset->buffered_bytes == 2U);
  const auto generation = adapter.ObserveStream(0U, 0U)->generation;
  assert(adapter.Reset(0U, 0U));
  const auto reset = adapter.ObserveStream(0U, 0U);
  assert(reset && reset->generation == generation + 1U && reset->total_candidates == 0U &&
         !reset->reset_required && adapter.ObserveStream(0U, 1U)->buffered_bytes == 2U);
  auto other_flow = adapter.SubmitStreamChunk(0U, 1U, Bytes("LY\r\n"));
  assert(other_flow.candidate && other_flow.candidate->local_status == LocalStatus::OK);

  auto overlong = adapter.SubmitStreamChunk(0U, 0U, Bytes("1234567890123"));
  assert(overlong.status == LocalStatus::OK && !overlong.candidate &&
         overlong.after.total_malformed_candidates == 1U &&
         overlong.after.phase == pae::StreamFramingPhase::DISCARDING_UNTIL_CRLF);
  auto after_discard = adapter.SubmitStreamChunk(0U, 0U, Bytes("\r\nONLY\r\n"));
  assert(after_discard.candidate && after_discard.candidate->local_status == LocalStatus::OK &&
         after_discard.after.total_discarded_bytes >= 15U);

  pae::StreamFramerOptions budget_options;
  budget_options.max_work_units = 5U;
  auto budgeted = Prepare(stream_json, {StreamBinding(budget_options)});
  if (!budgeted.adapter) {
    std::cerr << "budget prepare local=" << static_cast<int>(budgeted.status)
              << " host=" << static_cast<int>(budgeted.host_status) << '\n';
  }
  assert(budgeted.adapter);
  auto budget_step = budgeted.adapter->SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\n"));
  assert(budget_step.status == LocalStatus::OK && budget_step.host.framing.work_units_used > 0U &&
         (budget_step.after.frozen_cursor < budget_step.after.frozen_input_bytes ||
          budget_step.after.has_internal_work || budget_step.candidate));
  while (budgeted.adapter->StreamContinueAvailable(0U, 0U)) {
    budget_step = budgeted.adapter->ContinueStream(0U, 0U);
    assert(budget_step.status == LocalStatus::OK && !budget_step.after.reset_required);
  }
  assert(budget_step.candidate && budget_step.candidate->local_status == LocalStatus::OK);

#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  adapter.FailNextCallbackAllocationForTesting();
  auto allocation = adapter.SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\n"));
  assert(allocation.status == LocalStatus::ALLOCATION_FAILED && allocation.candidate &&
         allocation.diagnostic == StreamDiagnostic::CANDIDATE_COPY_FAILED_RESET_REQUIRED &&
         allocation.candidate->codec_called &&
         allocation.candidate->codec_status == pae::CodecStatus::OK &&
         allocation.candidate->frame.empty() && allocation.host.bytes_consumed == 6U &&
         allocation.after.reset_required);
  const auto reset_required = adapter.SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\n"));
  assert(!reset_required.host_called &&
         reset_required.diagnostic == StreamDiagnostic::RESET_REQUIRED);
  assert(adapter.Reset(0U, 0U));
#endif

  auto complete = Prepare(complete_json, {{"complete-rx", pae::HostAction::DECODE, 0U},
                                          {"complete-tx", pae::HostAction::ENCODE, 0U}});
  assert(complete.adapter && !complete.adapter->ObserveStream(0U, 0U));
  auto rejected = complete.adapter->SubmitStreamChunk(0U, 0U, Bytes("ONLY\r\n"));
  assert(rejected.status == LocalStatus::INVALID_INPUT && !rejected.host_called &&
         rejected.diagnostic == StreamDiagnostic::CHANNEL_NOT_ASCII_STREAM);
  auto complete_frame = Bytes("RX A!OK\r\n");
  assert(complete.adapter->Decode(0U, 0U, {complete_frame.data(), complete_frame.size()})
             .local_status == LocalStatus::OK);
  auto complete_encoded = complete.adapter->Encode(1U, 0U, {{0U, Bytes("A")}, {2U, Bytes("Z")}});
  assert(complete_encoded.local_status == LocalStatus::OK &&
         Text(complete_encoded.frame) == "TX A!Z\r\n");

  pae::StreamFramerOptions smaller_capacity_options;
  smaller_capacity_options.max_submit_bytes = 13U;
  pae::StreamFramerOptions larger_capacity_options;
  larger_capacity_options.max_submit_bytes = 17U;
  auto smaller_capacity = Prepare(stream_json, {StreamBinding(smaller_capacity_options)});
  auto larger_capacity = Prepare(stream_json, {StreamBinding(larger_capacity_options)});
  assert(smaller_capacity.adapter && larger_capacity.adapter &&
         smaller_capacity.adapter->StreamChunkCapacity(0U, 0U) == 13U &&
         larger_capacity.adapter->StreamChunkCapacity(0U, 0U) == 17U &&
         larger_capacity.adapter->AccountedBytes() - smaller_capacity.adapter->AccountedBytes() ==
             2U * (17U - 13U));

  const auto accounted = adapter.AccountedBytes();
  Limits exact;
  exact.instance_bytes = accounted;
  assert(Prepare(stream_json, {StreamBinding()}, exact).adapter);
  --exact.instance_bytes;
  assert(Prepare(stream_json, {StreamBinding()}, exact).status == LocalStatus::RESOURCE_LIMIT);
  exact.instance_bytes = accounted;
  exact.replacement_bytes = accounted + 17U;
  assert(Prepare(stream_json, {StreamBinding()}, exact, 17U).adapter);
  --exact.replacement_bytes;
  assert(Prepare(stream_json, {StreamBinding()}, exact, 17U).status == LocalStatus::RESOURCE_LIMIT);

  std::cout << "PUBLIC_ASCII_STREAM_TEST_PASS accounted=" << accounted << '\n';
  return 0;
}
