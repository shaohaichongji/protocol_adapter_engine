#include <atomic>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <new>
#include <string>
#include <vector>

#include "public_binary_decode.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using namespace pae::protocol_lab_binary::public_decode;

namespace {
constexpr std::size_t kNoFailedAllocation = (std::numeric_limits<std::size_t>::max)();
std::atomic<std::size_t> g_fail_allocation_size{kNoFailedAllocation};
std::atomic<std::size_t> g_failed_allocation_size{kNoFailedAllocation};
std::atomic<std::size_t> g_failed_allocation_hits{0U};
std::atomic<bool> g_track_allocation_sizes{false};
std::atomic<std::size_t> g_recorded_allocation_count{0U};
std::array<std::size_t, 2048U> g_recorded_allocation_sizes{};
}  // namespace

void* operator new(std::size_t size) {
  if (g_track_allocation_sizes.load(std::memory_order_relaxed)) {
    const auto index = g_recorded_allocation_count.fetch_add(1U, std::memory_order_relaxed);
    if (index < g_recorded_allocation_sizes.size()) g_recorded_allocation_sizes[index] = size;
  }
  auto expected = size;
  if (g_fail_allocation_size.compare_exchange_strong(
          expected, kNoFailedAllocation, std::memory_order_relaxed)) {
    g_failed_allocation_size.store(size, std::memory_order_relaxed);
    g_failed_allocation_hits.fetch_add(1U, std::memory_order_relaxed);
    throw std::bad_alloc{};
  }
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

#undef assert
#define assert(expression)                                                                         \
  do {                                                                                             \
    if (!(expression)) {                                                                           \
      std::cerr << "PUBLIC_STREAM_CHECK_FAILED line=" << __LINE__ << " expression=" << #expression \
                << '\n';                                                                           \
      return 1;                                                                                    \
    }                                                                                              \
  } while (false)

namespace {

void ArmAllocationFailure(std::size_t size) {
  g_failed_allocation_size.store(kNoFailedAllocation, std::memory_order_relaxed);
  g_failed_allocation_hits.store(0U, std::memory_order_relaxed);
  g_fail_allocation_size.store(size, std::memory_order_relaxed);
}

bool AllocationFailureHit(std::size_t size) {
  g_fail_allocation_size.store(kNoFailedAllocation, std::memory_order_relaxed);
  return g_failed_allocation_hits.load(std::memory_order_relaxed) == 1U &&
         g_failed_allocation_size.load(std::memory_order_relaxed) == size;
}

std::vector<Binding> Bindings();
Preparation Prepare(const std::string& json, Limits limits);

std::size_t CaptureReserveAllocationSize(const std::string& json, const Limits& limits) {
  auto compiled = pae::CompileProtocolJson(json);
  if (!compiled.Succeeded()) return kNoFailedAllocation;
  auto bindings = Bindings();
  g_recorded_allocation_count.store(0U, std::memory_order_relaxed);
  g_track_allocation_sizes.store(true, std::memory_order_relaxed);
  auto prepared = Adapter::AdoptCompiled(std::move(compiled).TakeCompiled(),
                                         std::move(bindings), limits);
  g_track_allocation_sizes.store(false, std::memory_order_relaxed);
  if (prepared.status != LocalStatus::OK || !prepared.adapter) return kNoFailedAllocation;
  const auto count = (std::min)(g_recorded_allocation_count.load(std::memory_order_relaxed),
                                g_recorded_allocation_sizes.size());
  for (std::size_t begin = 0U; begin + 6U <= count; ++begin) {
    const auto candidate = g_recorded_allocation_sizes[begin];
    if (candidate < limits.max_stream_chunk_bytes) continue;
    bool same = true;
    for (std::size_t offset = 1U; offset < 6U; ++offset)
      same = same && g_recorded_allocation_sizes[begin + offset] == candidate;
    if (same) return candidate;
  }
  return kNoFailedAllocation;
}

std::vector<Binding> Bindings() {
  return {{"fixed", pae::HostAction::DECODE, "fixed_rx", 2U},
          {"sync", pae::HostAction::DECODE, "sync_fixed_rx", 2U},
          {"length", pae::HostAction::DECODE, "sync_length_rx", 2U}};
}

Preparation Prepare(const std::string& json, Limits limits = {}) {
  auto compiled = pae::CompileProtocolJson(json);
  if (!compiled.Succeeded()) return {};
  return Adapter::AdoptCompiled(std::move(compiled).TakeCompiled(), Bindings(), limits);
}

int Run(const std::string& json) {
  Limits limits;
  limits.max_stream_chunk_bytes = 32U;
  auto prepared = Prepare(json, limits);
  assert(prepared.status == LocalStatus::OK && prepared.adapter);
  auto& adapter = *prepared.adapter;

  const auto fixed = adapter.ObserveStream(0U, 0U);
  const auto sync = adapter.ObserveStream(1U, 0U);
  const auto length = adapter.ObserveStream(2U, 0U);
  assert(fixed && fixed->strategy == pae::PipelineFramingStrategy::FIXED_LENGTH &&
         fixed->maximum_candidate_frame_bytes == 3U && fixed->effective_max_submit_bytes == 32U);
  assert(sync && sync->strategy == pae::PipelineFramingStrategy::SYNC_FIXED_LENGTH &&
         sync->maximum_candidate_frame_bytes == 4U);
  assert(length && length->strategy == pae::PipelineFramingStrategy::SYNC_LENGTH_FIELD &&
         length->maximum_candidate_frame_bytes == 8U);
  for (std::size_t binding = 0U; binding < 3U; ++binding) {
    for (std::size_t flow = 0U; flow < 2U; ++flow) {
      const auto index = adapter.FlowIndex(binding, flow);
      assert(index < 6U && adapter.State(index)->stream.available &&
             adapter.State(index)->stream.frozen_input.capacity() == 32U);
    }
  }

  // The adapter association budget grows by exactly one reserved C per stream Flow.
  auto larger_limits = limits;
  larger_limits.max_stream_chunk_bytes = 64U;
  auto larger = Prepare(json, larger_limits);
  assert(larger.status == LocalStatus::OK && larger.adapter &&
         larger.adapter->InstanceAdmissionBytes() - adapter.InstanceAdmissionBytes() ==
             6U * (64U - 32U));
  auto candidate_too_large = limits;
  candidate_too_large.max_frame_bytes = 7U;
  assert(Prepare(json, candidate_too_large).status == LocalStatus::RESOURCE_LIMIT);

  // A distinctive reserve size proves a real bad_alloc in the per-Flow frozen buffer reserve.
  constexpr std::size_t kFailedReserveBytes = 65521U;
  auto reserve_failure_limits = limits;
  reserve_failure_limits.max_stream_chunk_bytes = kFailedReserveBytes;
  const auto reserve_allocation_size =
      CaptureReserveAllocationSize(json, reserve_failure_limits);
  if (reserve_allocation_size == kNoFailedAllocation) {
    const auto recorded = (std::min)(
        g_recorded_allocation_count.load(std::memory_order_relaxed),
        g_recorded_allocation_sizes.size());
    std::cerr << "RESERVE_CAPTURE count=" << recorded << " large=";
    for (std::size_t i = 0U; i < recorded; ++i)
      if (g_recorded_allocation_sizes[i] >= 60000U)
        std::cerr << i << ':' << g_recorded_allocation_sizes[i] << ',';
    std::cerr << '\n';
  }
  assert(reserve_allocation_size != kNoFailedAllocation &&
         reserve_allocation_size >= kFailedReserveBytes);
  auto reserve_failure_compiled = pae::CompileProtocolJson(json);
  assert(reserve_failure_compiled.Succeeded());
  ArmAllocationFailure(reserve_allocation_size);
  auto reserve_failed = Adapter::AdoptCompiled(
      std::move(reserve_failure_compiled).TakeCompiled(), Bindings(), reserve_failure_limits);
  if (!AllocationFailureHit(reserve_allocation_size) ||
      reserve_failed.status != LocalStatus::RESOURCE_LIMIT || reserve_failed.adapter) {
    std::cerr << "RESERVE_ALLOCATION_FAILURE hit="
              << g_failed_allocation_hits.load(std::memory_order_relaxed)
              << " size=" << g_failed_allocation_size.load(std::memory_order_relaxed)
              << " status=" << static_cast<int>(reserve_failed.status)
              << " adapter=" << static_cast<bool>(reserve_failed.adapter) << '\n';
  }
  assert(AllocationFailureHit(reserve_allocation_size) &&
         reserve_failed.status == LocalStatus::RESOURCE_LIMIT && !reserve_failed.adapter);
  std::cout << "ALLOCATION_FAILURE_HIT phase=prepare_frozen_reserve size="
            << reserve_allocation_size << " hits=1\n";
  auto reserve_recovered = Prepare(json, reserve_failure_limits);
  assert(reserve_recovered.status == LocalStatus::OK && reserve_recovered.adapter);
  auto exact_limits = limits;
  exact_limits.instance_bytes = adapter.InstanceAdmissionBytes();
  auto exact = Prepare(json, exact_limits);
  assert(exact.status == LocalStatus::OK && exact.adapter);
  exact_limits.instance_bytes = adapter.InstanceAdmissionBytes() - 1U;
  assert(Prepare(json, exact_limits).status == LocalStatus::RESOURCE_LIMIT);
  exact_limits.instance_bytes = adapter.InstanceAdmissionBytes();
  exact_limits.replacement_bytes = adapter.InstanceAdmissionBytes() + 7U;
  assert(Adapter::AdoptCompiled(std::move(pae::CompileProtocolJson(json)).TakeCompiled(),
                                Bindings(), exact_limits, 7U)
             .status == LocalStatus::OK);

  // A partial fixed frame is normal NEED_MORE. With no frozen suffix/internal work, Continue is
  // idle.
  const auto partial = adapter.SubmitStreamChunk(0U, 0U, {0xAAU});
  assert(partial.host_called && partial.host.status == pae::HostStatus::OK && !partial.candidate &&
         partial.after.buffered_bytes == 1U && !adapter.StreamContinueAvailable(0U, 0U));
  const auto idle = adapter.ContinueStream(0U, 0U);
  assert(!idle.host_called && idle.local_status == LocalStatus::OK &&
         idle.diagnostic == StreamDiagnostic::NO_WORK && !idle.after.reset_required);
  const auto partial_before_reject = *adapter.ObserveStream(0U, 0U);
  const auto oversize = adapter.SubmitStreamChunk(0U, 0U, std::vector<std::uint8_t>(33U));
  const auto partial_after_reject = *adapter.ObserveStream(0U, 0U);
  assert(!oversize.host_called && oversize.diagnostic == StreamDiagnostic::INVALID_CHUNK &&
         partial_after_reject.buffered_bytes == partial_before_reject.buffered_bytes &&
         partial_after_reject.generation == partial_before_reject.generation);

  // STOP retains the exact caller-owned suffix; each explicit Continue performs one Push step.
  const std::vector<std::uint8_t> glued{0x01U, 0x02U, 0x00U, 0x00U, 0x00U, 0xAAU, 0x03U, 0x04U};
  const auto first = adapter.SubmitStreamChunk(0U, 0U, glued);
  assert(first.candidate && first.candidate->success && first.host.candidates == 1U &&
         first.host.decode_attempts == 1U && first.host.decode_successes == 1U &&
         first.host.observer_callbacks_returned == 1U &&
         first.host.business_callbacks_returned == 1U &&
         first.after.frozen_input_bytes == glued.size() && first.after.frozen_cursor == 2U);
  const auto blocked = adapter.SubmitStreamChunk(0U, 0U, {0xAAU, 9U, 9U});
  assert(!blocked.host_called && blocked.diagnostic == StreamDiagnostic::CONTINUE_REQUIRED &&
         blocked.after.frozen_cursor == 2U);
  const auto failed = adapter.ContinueStream(0U, 0U);
  assert(failed.candidate && !failed.candidate->success && failed.host.decode_failures == 1U &&
         failed.host.business_callbacks_returned == 0U && failed.after.frozen_cursor == 5U);
  const auto recovered = adapter.ContinueStream(0U, 0U);
  assert(recovered.candidate && recovered.candidate->success &&
         recovered.candidate->fields[0].uint64_value == 0x0304U &&
         recovered.after.frozen_input_bytes == 0U && recovered.after.total_candidates == 3U &&
         recovered.after.total_decode_successes == 2U &&
         recovered.after.total_decode_failures == 1U);

  // Flow isolation: Reset of flow zero does not discard the other flow's buffered byte.
  assert(!adapter.SubmitStreamChunk(0U, 1U, {0xAAU}).candidate);
  const auto other_before = adapter.ObserveStream(0U, 1U);
  assert(other_before && other_before->buffered_bytes == 1U);
  const auto old_generation = adapter.ObserveStream(0U, 0U)->generation;
  assert(adapter.Reset(adapter.FlowIndex(0U, 0U)) == pae::HostStatus::OK);
  assert(adapter.ObserveStream(0U, 0U)->generation == old_generation + 1U &&
         adapter.ObserveStream(0U, 0U)->total_candidates == 0U &&
         adapter.ObserveStream(0U, 1U)->buffered_bytes == 1U);
  const auto other = adapter.SubmitStreamChunk(0U, 1U, {0x12U, 0x34U});
  assert(other.candidate && other.candidate->success &&
         other.candidate->fields[0].uint64_value == 0x1234U);

  // Sync-fixed recovery and a second candidate frozen behind STOP.
  const std::vector<std::uint8_t> sync_pair{0x00U, 0xA5U, 0x5AU, 1U, 2U, 0xA5U, 0x5AU, 3U, 4U};
  const auto sync_first = adapter.SubmitStreamChunk(1U, 0U, sync_pair);
  assert(sync_first.candidate && sync_first.candidate->success &&
         sync_first.host.framing.bytes_discarded == 1U && sync_first.after.frozen_cursor == 5U);
  const auto sync_second = adapter.ContinueStream(1U, 0U);
  assert(sync_second.candidate && sync_second.candidate->success &&
         sync_second.candidate->fields[0].uint64_value == 0x0304U);
  assert(!adapter.SubmitStreamChunk(1U, 1U, {0x00U, 0xA5U}).candidate);
  const auto sync_split = adapter.SubmitStreamChunk(1U, 1U, {0x5AU, 0x12U, 0x34U});
  assert(sync_split.candidate && sync_split.candidate->success &&
         sync_split.candidate->fields[0].uint64_value == 0x1234U);

  // Malformed declared length is reported while scanning resumes to one valid candidate.
  const std::vector<std::uint8_t> malformed_then_valid{0xC3U, 0x3CU, 0x09U, 0x00U, 0xC3U,
                                                       0x3CU, 0x06U, 0x12U, 0x34U, 0x55U};
  const auto length_step = adapter.SubmitStreamChunk(2U, 0U, malformed_then_valid);
  assert(length_step.candidate && length_step.candidate->success &&
         length_step.host.framing.malformed_candidates == 1U &&
         length_step.host.framing.bytes_discarded == 4U &&
         length_step.host.framing.last_issue == pae::StreamFramingIssue::MALFORMED_LENGTH);
  assert(!adapter.SubmitStreamChunk(2U, 1U, {0xC3U, 0x3CU, 0x06U, 0x12U}).candidate);
  const auto length_split = adapter.SubmitStreamChunk(2U, 1U, {0x34U, 0x55U});
  assert(length_split.candidate && length_split.candidate->success &&
         length_split.candidate->fields[1].uint64_value == 0x1234U);

  // A small work budget progresses only one bounded Host step per UI action.
  auto budget_limits = limits;
  budget_limits.max_stream_work_units = 5U;
  auto budgeted = Prepare(json, budget_limits);
  if (budgeted.status != LocalStatus::OK)
    std::cerr << "BUDGET_PREPARATION status=" << static_cast<int>(budgeted.status)
              << " host=" << static_cast<int>(budgeted.host_status) << '\n';
  assert(budgeted.status == LocalStatus::OK && budgeted.adapter);
  auto budget_step = budgeted.adapter->SubmitStreamChunk(0U, 0U, {0xAAU, 1U, 2U});
  bool saw_budget =
      budget_step.host.framing.stop_reason == pae::StreamFramerStopReason::WORK_BUDGET_REACHED;
  for (std::size_t attempt = 0U; !budget_step.candidate && attempt < 8U; ++attempt) {
    assert(budgeted.adapter->StreamContinueAvailable(0U, 0U));
    budget_step = budgeted.adapter->ContinueStream(0U, 0U);
    saw_budget = saw_budget || budget_step.host.framing.stop_reason ==
                                   pae::StreamFramerStopReason::WORK_BUDGET_REACHED;
  }
  assert(saw_budget && budget_step.candidate && budget_step.candidate->success);

  // Sync-copy may leave internal work after consuming the complete external suffix.
  auto internal_limits = limits;
  internal_limits.max_stream_work_units = 5U;
  auto internal = Prepare(json, internal_limits);
  assert(internal.status == LocalStatus::OK && internal.adapter);
  const auto sync_prefix = internal.adapter->SubmitStreamChunk(1U, 0U, {0xA5U, 0x5AU});
  assert(!sync_prefix.candidate && sync_prefix.after.frozen_input_bytes == 0U &&
         sync_prefix.after.has_internal_work);
  const auto internal_step = internal.adapter->ContinueStream(1U, 0U);
  assert(internal_step.host_called && internal_step.host.bytes_consumed == 0U &&
         !internal_step.candidate && !internal_step.after.reset_required);
  assert(internal.adapter->SubmitStreamChunk(1U, 0U, {1U, 2U}).candidate);

  // Callback materialization failure keeps confirmed consumption and faults only the target Flow.
  auto tiny_limits = limits;
  tiny_limits.max_result_bytes = sizeof(Candidate) + 8U;
  auto tiny = Prepare(json, tiny_limits);
  assert(tiny.status == LocalStatus::OK && tiny.adapter);
  const auto copy_failed = tiny.adapter->SubmitStreamChunk(0U, 0U, {0xAAU, 1U, 2U, 0xAAU, 3U, 4U});
  assert(copy_failed.host.status == pae::HostStatus::CALLBACK_FAILED &&
         copy_failed.host.bytes_consumed == 3U && !copy_failed.candidate &&
         copy_failed.diagnostic == StreamDiagnostic::COPY_FAILED_RESET_REQUIRED &&
         copy_failed.after.reset_required && copy_failed.after.frozen_cursor == 3U &&
         !tiny.adapter->ObserveStream(0U, 1U)->reset_required);
  assert(tiny.adapter->ContinueStream(0U, 0U).diagnostic == StreamDiagnostic::RESET_REQUIRED);
  assert(tiny.adapter->Reset(tiny.adapter->FlowIndex(0U, 0U)) == pae::HostStatus::OK &&
         !tiny.adapter->ObserveStream(0U, 0U)->reset_required &&
         tiny.adapter->ObserveStream(0U, 0U)->frozen_input_bytes == 0U);

  // The next three-byte allocation is Candidate::frame materialization inside the Host callback.
  auto allocation = Prepare(json, limits);
  assert(allocation.status == LocalStatus::OK && allocation.adapter);
  const std::vector<std::uint8_t> two_frames{0xAAU, 1U, 2U, 0xAAU, 3U, 4U};
  ArmAllocationFailure(3U);
  const auto allocation_failed =
      allocation.adapter->SubmitStreamChunk(0U, 0U, two_frames);
  assert(AllocationFailureHit(3U) &&
         allocation_failed.host.status == pae::HostStatus::CALLBACK_FAILED &&
         allocation_failed.host.bytes_consumed == 3U && !allocation_failed.candidate &&
         allocation_failed.diagnostic == StreamDiagnostic::COPY_FAILED_RESET_REQUIRED &&
         allocation_failed.after.reset_required && allocation_failed.after.frozen_cursor == 3U &&
         !allocation.adapter->ObserveStream(0U, 1U)->reset_required);
  std::cout << "ALLOCATION_FAILURE_HIT phase=callback_candidate_frame size=3 hits=1"
            << " consumed=" << allocation_failed.host.bytes_consumed << '\n';
  assert(allocation.adapter->Reset(allocation.adapter->FlowIndex(0U, 0U)) ==
             pae::HostStatus::OK &&
         !allocation.adapter->ObserveStream(0U, 0U)->reset_required);
  const auto after_allocation_reset =
      allocation.adapter->SubmitStreamChunk(0U, 0U, {0xAAU, 5U, 6U});
  assert(after_allocation_reset.candidate && after_allocation_reset.candidate->success &&
         after_allocation_reset.candidate->fields[0].uint64_value == 0x0506U);

  // Complete Decode cannot be used as a hidden fallback for a stream binding.
  const std::vector<std::uint8_t> complete{0xAAU, 0x00U, 0x01U};
  const auto& wrong_kind =
      adapter.Decode(adapter.FlowIndex(0U, 0U), {complete.data(), complete.size()});
  assert(wrong_kind.host.status == pae::HostStatus::WRONG_INPUT_KIND &&
         !wrong_kind.host.codec_attempted);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  assert(argc == 2);
  std::ifstream input(argv[1], std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{});
  assert(!json.empty());
  const auto result = Run(json);
  if (result == 0) std::cout << "Public Binary stream checks passed\n";
  return result;
}
