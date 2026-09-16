#include <pae/codec.h>
#include <pae/compiler.h>
#include <pae/stream_framer.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

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

static_assert(!std::is_copy_constructible_v<pae::StreamFramer>);
static_assert(std::is_nothrow_move_constructible_v<pae::StreamFramer>);

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
    std::cout << "PUBLIC_STREAM_FRAMER_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
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

struct CapturedFrames {
  std::array<std::array<std::uint8_t, 32U>, 16U> bytes{};
  std::array<std::size_t, 16U> sizes{};
  std::size_t count = 0U;
  bool stop_next = false;
};

pae::FrameSinkAction Capture(pae::FrameCandidateView candidate, void* opaque) noexcept {
  auto& captured = *static_cast<CapturedFrames*>(opaque);
  if (captured.count >= captured.bytes.size() ||
      candidate.bytes.size > captured.bytes[captured.count].size()) {
    return pae::FrameSinkAction::STOP;
  }
  for (std::size_t index = 0U; index < candidate.bytes.size; ++index) {
    captured.bytes[captured.count][index] = candidate.bytes.data[index];
  }
  captured.sizes[captured.count] = candidate.bytes.size;
  ++captured.count;
  if (captured.stop_next) {
    captured.stop_next = false;
    return pae::FrameSinkAction::STOP;
  }
  return pae::FrameSinkAction::CONTINUE;
}

bool Equals(const CapturedFrames& captured, std::size_t index,
            std::initializer_list<std::uint8_t> expected) {
  if (index >= captured.count || captured.sizes[index] != expected.size()) return false;
  std::size_t offset = 0U;
  for (std::uint8_t byte : expected) {
    if (captured.bytes[index][offset++] != byte) return false;
  }
  return true;
}

bool Equals(const CapturedFrames& captured, std::size_t index, std::string_view expected) {
  if (index >= captured.count || captured.sizes[index] != expected.size()) return false;
  for (std::size_t offset = 0U; offset < expected.size(); ++offset) {
    if (captured.bytes[index][offset] != static_cast<std::uint8_t>(expected[offset])) return false;
  }
  return true;
}

struct ReentrantContext {
  pae::StreamFramer* framer = nullptr;
  pae::StreamFramerStatus reset = pae::StreamFramerStatus::OK;
  pae::StreamFramerStatus observe = pae::StreamFramerStatus::OK;
  pae::StreamFramerStatus push = pae::StreamFramerStatus::OK;
  pae::StreamFramerStatus continue_call = pae::StreamFramerStatus::OK;
};

pae::FrameSinkAction Reenter(pae::FrameCandidateView, void* opaque) noexcept {
  auto& context = *static_cast<ReentrantContext*>(opaque);
  const std::array<std::uint8_t, 3U> frame{{0xAAU, 0x05U, 0x06U}};
  context.reset = context.framer->Reset();
  context.observe = context.framer->Observe().status;
  context.push = context.framer->Push(Bytes(frame), {Capture, nullptr}).status;
  context.continue_call = context.framer->Continue({Capture, nullptr}).status;
  return pae::FrameSinkAction::CONTINUE;
}

struct IndirectReentrantContext {
  pae::StreamFramer* first = nullptr;
  pae::StreamFramer* second = nullptr;
  pae::StreamSubmitResult second_result;
  pae::StreamSubmitResult loop_result;
  pae::StreamFramerStatus loop_observe = pae::StreamFramerStatus::OK;
  CapturedFrames loop_capture;
};

pae::FrameSinkAction IndirectSecond(pae::FrameCandidateView, void* opaque) noexcept {
  auto& context = *static_cast<IndirectReentrantContext*>(opaque);
  const std::array<std::uint8_t, 3U> loop_frame{{0xAAU, 0x07U, 0x08U}};
  context.loop_result = context.first->Push(Bytes(loop_frame), {Capture, &context.loop_capture});
  context.loop_observe = context.first->Observe().status;
  return pae::FrameSinkAction::CONTINUE;
}

pae::FrameSinkAction IndirectFirst(pae::FrameCandidateView, void* opaque) noexcept {
  auto& context = *static_cast<IndirectReentrantContext*>(opaque);
  const std::array<std::uint8_t, 3U> second_frame{{0xAAU, 0x03U, 0x04U}};
  context.second_result = context.second->Push(Bytes(second_frame), {IndirectSecond, &context});
  return pae::FrameSinkAction::CONTINUE;
}

struct BlockingContext {
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
};

pae::FrameSinkAction Block(pae::FrameCandidateView, void* opaque) noexcept {
  auto& context = *static_cast<BlockingContext*>(opaque);
  context.entered.store(true, std::memory_order_release);
  while (!context.release.load(std::memory_order_acquire)) std::this_thread::yield();
  return pae::FrameSinkAction::CONTINUE;
}

bool WaitUntilEntered(const BlockingContext& context) {
  for (std::size_t attempt = 0U; attempt < 1000000U; ++attempt) {
    if (context.entered.load(std::memory_order_acquire)) return true;
    std::this_thread::yield();
  }
  return false;
}

struct DecodeContext {
  pae::CompleteRecordCodec* codec = nullptr;
  std::size_t pipeline_index = 0U;
  std::size_t calls = 0U;
  std::size_t successes = 0U;
  std::array<std::uint8_t, 8U> copied{};
  std::size_t copied_size = 0U;
};

pae::FrameSinkAction DecodeOnce(pae::FrameCandidateView candidate, void* opaque) noexcept {
  auto& context = *static_cast<DecodeContext*>(opaque);
  ++context.calls;
  const auto decoded = context.codec->Decode(context.pipeline_index, candidate.bytes);
  if (decoded.status == pae::CodecStatus::OK) ++context.successes;
  context.copied_size = candidate.bytes.size;
  for (std::size_t index = 0U; index < candidate.bytes.size && index < context.copied.size();
       ++index) {
    context.copied[index] = candidate.bytes.data[index];
  }
  return pae::FrameSinkAction::CONTINUE;
}

void CheckCapabilities(Runner& runner, pae::CompiledProtocol& binary,
                       pae::CompiledProtocol& ascii) {
  runner.Check(pae::QueryStreamFramingCapability({}, 0U).status ==
                   pae::StreamFramerStatus::INVALID_COMPILED_PROTOCOL,
               "capability_empty_owner");
  for (std::size_t pipeline = 0U; pipeline < 3U; ++pipeline) {
    const auto capability = pae::QueryStreamFramingCapability(binary, pipeline);
    runner.Check(capability.status == pae::StreamFramerStatus::OK && capability.available,
                 "capability_binary_stream");
  }
  const auto stream = pae::QueryStreamFramingCapability(ascii, 0U);
  const auto record = pae::QueryStreamFramingCapability(ascii, 1U);
  const auto out_of_range = pae::QueryStreamFramingCapability(ascii, 99U);
  runner.Check(stream.status == pae::StreamFramerStatus::OK && stream.available,
               "capability_ascii_stream");
  runner.Check(record.status == pae::StreamFramerStatus::OK && !record.available,
               "capability_complete_record");
  runner.Check(out_of_range.status == pae::StreamFramerStatus::PIPELINE_OUT_OF_RANGE &&
                   !out_of_range.available,
               "capability_out_of_range");
  const auto rejected = pae::CreateStreamFramer(ascii, 1U);
  runner.Check(
      rejected.status == pae::StreamFramerStatus::INPUT_KIND_NOT_STREAM && !rejected.framer,
      "create_complete_record_rejected");
}

void CheckBinaryStrategies(Runner& runner, pae::CompiledProtocol& compiled) {
  auto fixed = pae::CreateStreamFramer(compiled, 0U);
  runner.Check(fixed.status == pae::StreamFramerStatus::OK && fixed.framer, "fixed_create");
  if (!fixed.framer) return;

  const auto memory = fixed.framer->MemoryReport();
  runner.Check(memory.internal_workspace_bytes > 0U && memory.facade_bytes > 0U &&
                   memory.framer_accounted_total_bytes ==
                       memory.internal_workspace_bytes + memory.facade_bytes &&
                   memory.retained_compiled_state_facade_bytes > 0U &&
                   memory.effective_session_limit_bytes >= memory.internal_workspace_bytes &&
                   memory.allocation_count >= 4U,
               "memory_categories");
  g_allocation_count.store(0U, std::memory_order_relaxed);
  g_track_allocations.store(true, std::memory_order_relaxed);
  auto measured = pae::CreateStreamFramer(compiled, 0U);
  g_track_allocations.store(false, std::memory_order_relaxed);
  runner.Check(measured.status == pae::StreamFramerStatus::OK && measured.framer &&
                   measured.memory.allocation_count == g_allocation_count.load(),
               "creation_allocation_count");
  bool failures_are_atomic = true;
  for (std::size_t index = 0U; index < measured.memory.allocation_count; ++index) {
    g_fail_after.store(index, std::memory_order_relaxed);
    auto failed = pae::CreateStreamFramer(compiled, 0U);
    g_fail_after.store(kNoAllocationFailure, std::memory_order_relaxed);
    failures_are_atomic = failures_are_atomic && !failed.framer &&
                          failed.status == pae::StreamFramerStatus::ALLOCATION_FAILED;
  }
  auto recovered_after_failure = pae::CreateStreamFramer(compiled, 0U);
  runner.Check(failures_are_atomic &&
                   recovered_after_failure.status == pae::StreamFramerStatus::OK &&
                   recovered_after_failure.framer,
               "creation_allocation_failures_are_atomic");
  pae::StreamFramerOptions exact;
  exact.max_session_memory_bytes = memory.internal_workspace_bytes;
  runner.Check(pae::CreateStreamFramer(compiled, 0U, exact).status == pae::StreamFramerStatus::OK,
               "memory_exact");
  --exact.max_session_memory_bytes;
  runner.Check(pae::CreateStreamFramer(compiled, 0U, exact).status ==
                   pae::StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED,
               "memory_minus_one");
  pae::StreamFramerOptions short_sync;
  short_sync.max_sync_bytes = 1U;
  runner.Check(pae::CreateStreamFramer(compiled, 1U, short_sync).status ==
                   pae::StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED,
               "sync_limit_rejected_before_publish");

  pae::StreamFramerOptions submit_limit;
  submit_limit.max_submit_bytes = 2U;
  submit_limit.max_work_units = 17U;
  auto limited = pae::CreateStreamFramer(compiled, 0U, submit_limit);
  CapturedFrames limited_frames;
  const std::array<std::uint8_t, 3U> limited_input{{0xAAU, 1U, 2U}};
  const auto initial_limit_observation = limited.framer->Observe();
  const auto limited_result =
      limited.framer->Push(Bytes(limited_input), {Capture, &limited_frames});
  const auto rejected_limit_observation = limited.framer->Observe();
  runner.Check(initial_limit_observation.effective_max_submit_bytes == 2U &&
                   initial_limit_observation.effective_max_work_units == 17U &&
                   limited_result.status == pae::StreamFramerStatus::RESOURCE_LIMIT_EXCEEDED &&
                   limited_result.bytes_consumed == 0U &&
                   limited_result.candidates_delivered == 0U && limited_frames.count == 0U &&
                   rejected_limit_observation.buffered_bytes == 0U,
               "submit_limit_zero_consumption_no_state_change");

  pae::StreamFramerOptions work_limit;
  work_limit.max_work_units = 4U;
  auto budgeted = pae::CreateStreamFramer(compiled, 0U, work_limit);
  CapturedFrames budget_frames;
  const auto first_budget = budgeted.framer->Push(Bytes(limited_input), {Capture, &budget_frames});
  const pae::ByteView budget_suffix{limited_input.data() + first_budget.bytes_consumed,
                                    limited_input.size() - first_budget.bytes_consumed};
  const auto second_budget = budgeted.framer->Push(budget_suffix, {Capture, &budget_frames});
  runner.Check(first_budget.stop_reason == pae::StreamFramerStopReason::WORK_BUDGET_REACHED &&
                   first_budget.bytes_consumed == 2U && first_budget.work_units_used == 4U &&
                   second_budget.bytes_consumed == 1U && second_budget.candidates_delivered == 1U &&
                   budget_frames.count == 1U && Equals(budget_frames, 0U, {0xAAU, 1U, 2U}),
               "work_budget_exact_suffix_progress");

  CapturedFrames fixed_frames;
  const std::array<std::uint8_t, 1U> first{{0xAAU}};
  auto submitted = fixed.framer->Push(Bytes(first), {Capture, &fixed_frames});
  const auto half = fixed.framer->Observe();
  runner.Check(submitted.status == pae::StreamFramerStatus::OK && submitted.bytes_consumed == 1U &&
                   submitted.stop_reason == pae::StreamFramerStopReason::NEED_MORE &&
                   half.status == pae::StreamFramerStatus::OK && half.buffered_bytes == 1U &&
                   !half.has_internal_work,
               "fixed_half_observe");
  const std::array<std::uint8_t, 5U> rest{{0x01U, 0x02U, 0xAAU, 0x03U, 0x04U}};
  g_allocation_count.store(0U, std::memory_order_relaxed);
  g_track_allocations.store(true, std::memory_order_relaxed);
  submitted = fixed.framer->Push(Bytes(rest), {Capture, &fixed_frames});
  g_track_allocations.store(false, std::memory_order_relaxed);
  runner.Check(submitted.status == pae::StreamFramerStatus::OK &&
                   submitted.bytes_consumed == rest.size() &&
                   submitted.candidates_delivered == 2U && g_allocation_count.load() == 0U &&
                   Equals(fixed_frames, 0U, {0xAAU, 0x01U, 0x02U}) &&
                   Equals(fixed_frames, 1U, {0xAAU, 0x03U, 0x04U}),
               "fixed_two_frames_no_allocation");

  auto sync = pae::CreateStreamFramer(compiled, 1U);
  CapturedFrames sync_frames;
  const std::array<std::uint8_t, 2U> sync_first{{0x00U, 0xA5U}};
  const std::array<std::uint8_t, 3U> sync_second{{0x5AU, 0x12U, 0x34U}};
  const auto sync_a = sync.framer->Push(Bytes(sync_first), {Capture, &sync_frames});
  const auto sync_b = sync.framer->Push(Bytes(sync_second), {Capture, &sync_frames});
  runner.Check(sync_a.bytes_consumed == 2U && sync_b.bytes_consumed == 3U &&
                   sync_b.candidates_delivered == 1U && sync_b.bytes_discarded == 0U &&
                   Equals(sync_frames, 0U, {0xA5U, 0x5AU, 0x12U, 0x34U}),
               "sync_fixed_cross_chunk");

  auto length = pae::CreateStreamFramer(compiled, 2U);
  CapturedFrames length_frames;
  const std::array<std::uint8_t, 8U> length_input{
      {0x00U, 0xC3U, 0x3CU, 0x06U, 0x12U, 0x34U, 0x55U, 0x7FU}};
  const auto length_result = length.framer->Push(Bytes(length_input), {Capture, &length_frames});
  runner.Check(length_result.bytes_consumed == length_input.size() &&
                   length_result.bytes_discarded == 2U &&
                   length_result.candidates_delivered == 1U &&
                   length_result.stop_reason == pae::StreamFramerStopReason::INPUT_EXHAUSTED &&
                   Equals(length_frames, 0U, {0xC3U, 0x3CU, 0x06U, 0x12U, 0x34U, 0x55U}),
               "sync_length_candidate_and_tail");
}

void CheckStopContinueResetAndMove(Runner& runner, pae::CompiledProtocol& compiled) {
  auto stopped = pae::CreateStreamFramer(compiled, 0U);
  CapturedFrames captured;
  captured.stop_next = true;
  const std::array<std::uint8_t, 6U> two{{0xAAU, 1U, 2U, 0xAAU, 3U, 4U}};
  auto result = stopped.framer->Push(Bytes(two), {Capture, &captured});
  runner.Check(result.status == pae::StreamFramerStatus::OK &&
                   result.stop_reason == pae::StreamFramerStopReason::SINK_STOP &&
                   result.bytes_consumed == 3U && result.candidates_delivered == 1U,
               "stop_exact_prefix");
  const pae::ByteView suffix{two.data() + result.bytes_consumed,
                             two.size() - result.bytes_consumed};
  result = stopped.framer->Push(suffix, {Capture, &captured});
  runner.Check(result.bytes_consumed == 3U && result.candidates_delivered == 1U &&
                   captured.count == 2U && Equals(captured, 1U, {0xAAU, 3U, 4U}),
               "stop_suffix_no_replay");

  pae::StreamFramerOptions one_frame;
  one_frame.max_frames_per_submit = 1U;
  auto pending_a = pae::CreateStreamFramer(compiled, 0U, one_frame);
  CapturedFrames pending_frames_a;
  result = pending_a.framer->Push(Bytes(two), {Capture, &pending_frames_a});
  const auto pending_observe = pending_a.framer->Observe();
  const auto empty_push = pending_a.framer->Push({}, {Capture, &pending_frames_a});
  runner.Check(result.bytes_consumed == 6U && result.candidates_delivered == 1U &&
                   result.stop_reason == pae::StreamFramerStopReason::WORK_BUDGET_REACHED &&
                   pending_observe.phase == pae::StreamFramingPhase::DELIVERY_PENDING &&
                   pending_observe.has_internal_work && empty_push.bytes_consumed == 0U &&
                   empty_push.candidates_delivered == 1U && pending_frames_a.count == 2U,
               "empty_push_advances_pending_once");

  auto pending_b = pae::CreateStreamFramer(compiled, 0U, one_frame);
  CapturedFrames pending_frames_b;
  static_cast<void>(pending_b.framer->Push(Bytes(two), {Capture, &pending_frames_b}));
  const auto continued = pending_b.framer->Continue({Capture, &pending_frames_b});
  runner.Check(continued.bytes_consumed == 0U && continued.candidates_delivered == 1U &&
                   pending_frames_b.count == 2U &&
                   pending_b.framer->Continue({Capture, &pending_frames_b}).stop_reason ==
                       pae::StreamFramerStopReason::INPUT_EXHAUSTED,
               "continue_equivalent_and_idle");

  auto first_flow = pae::CreateStreamFramer(compiled, 0U);
  auto second_flow = pae::CreateStreamFramer(compiled, 0U);
  CapturedFrames flows;
  const std::array<std::uint8_t, 2U> half{{0xAAU, 0x01U}};
  static_cast<void>(first_flow.framer->Push(Bytes(half), {Capture, &flows}));
  const std::array<std::uint8_t, 3U> whole{{0xAAU, 0x03U, 0x04U}};
  const auto other = second_flow.framer->Push(Bytes(whole), {Capture, &flows});
  runner.Check(other.candidates_delivered == 1U &&
                   first_flow.framer->Observe().buffered_bytes == 2U &&
                   second_flow.framer->Observe().buffered_bytes == 0U,
               "independent_streams");
  runner.Check(first_flow.framer->Reset() == pae::StreamFramerStatus::OK &&
                   first_flow.framer->Observe().buffered_bytes == 0U && flows.count == 1U,
               "reset_discards_half_only");

  auto moving = pae::CreateStreamFramer(compiled, 0U);
  CapturedFrames moved_frames;
  static_cast<void>(moving.framer->Push(Bytes(half), {Capture, &moved_frames}));
  pae::StreamFramer moved(std::move(*moving.framer));
  const std::array<std::uint8_t, 1U> last{{0x02U}};
  const auto moved_result = moved.Push(Bytes(last), {Capture, &moved_frames});
  runner.Check(
      moved_result.candidates_delivered == 1U &&
          moving.framer->Observe().status == pae::StreamFramerStatus::INVALID_COMPILED_PROTOCOL &&
          Equals(moved_frames, 0U, {0xAAU, 0x01U, 0x02U}),
      "move_preserves_stream_and_invalidates_source");
}

void CheckGuarding(Runner& runner, pae::CompiledProtocol& compiled) {
  auto reentrant = pae::CreateStreamFramer(compiled, 0U);
  ReentrantContext reentrant_context{reentrant.framer.get()};
  const std::array<std::uint8_t, 3U> frame{{0xAAU, 1U, 2U}};
  const auto result = reentrant.framer->Push(Bytes(frame), {Reenter, &reentrant_context});
  runner.Check(result.candidates_delivered == 1U &&
                   reentrant_context.reset == pae::StreamFramerStatus::REENTRANT_CALL &&
                   reentrant_context.observe == pae::StreamFramerStatus::REENTRANT_CALL &&
                   reentrant_context.push == pae::StreamFramerStatus::REENTRANT_CALL &&
                   reentrant_context.continue_call == pae::StreamFramerStatus::REENTRANT_CALL,
               "callback_reentry_rejected");

  auto first = pae::CreateStreamFramer(compiled, 0U);
  auto second = pae::CreateStreamFramer(compiled, 0U);
  IndirectReentrantContext indirect{first.framer.get(), second.framer.get()};
  const auto indirect_result = first.framer->Push(Bytes(frame), {IndirectFirst, &indirect});
  const auto first_after = first.framer->Observe();
  const auto second_after = second.framer->Observe();
  CapturedFrames after_capture;
  const auto after_result = first.framer->Push(Bytes(frame), {Capture, &after_capture});
  runner.Check(
      indirect_result.status == pae::StreamFramerStatus::OK &&
          indirect_result.candidates_delivered == 1U &&
          indirect.second_result.status == pae::StreamFramerStatus::OK &&
          indirect.second_result.candidates_delivered == 1U &&
          indirect.loop_result.status == pae::StreamFramerStatus::REENTRANT_CALL &&
          indirect.loop_result.bytes_consumed == 0U &&
          indirect.loop_result.candidates_delivered == 0U &&
          indirect.loop_observe == pae::StreamFramerStatus::REENTRANT_CALL &&
          indirect.loop_capture.count == 0U && first_after.status == pae::StreamFramerStatus::OK &&
          first_after.buffered_bytes == 0U && second_after.status == pae::StreamFramerStatus::OK &&
          second_after.buffered_bytes == 0U && after_result.status == pae::StreamFramerStatus::OK &&
          after_result.candidates_delivered == 1U && after_capture.count == 1U,
      "indirect_callback_loop_rejected_as_reentrant");

  auto busy = pae::CreateStreamFramer(compiled, 0U);
  BlockingContext blocking;
  pae::StreamSubmitResult background;
  std::thread worker([&] { background = busy.framer->Push(Bytes(frame), {Block, &blocking}); });
  const bool entered = WaitUntilEntered(blocking);
  const auto observed = busy.framer->Observe();
  const auto pushed = busy.framer->Continue({Capture, nullptr});
  const auto reset = busy.framer->Reset();
  blocking.release.store(true, std::memory_order_release);
  worker.join();
  runner.Check(entered && observed.status == pae::StreamFramerStatus::WORKSPACE_BUSY &&
                   pushed.status == pae::StreamFramerStatus::WORKSPACE_BUSY &&
                   reset == pae::StreamFramerStatus::WORKSPACE_BUSY &&
                   background.status == pae::StreamFramerStatus::OK,
               "concurrent_operations_busy");
}

void CheckAscii(Runner& runner, pae::CompiledProtocol& compiled) {
  auto framer = pae::CreateStreamFramer(compiled, 0U);
  CapturedFrames frames;
  const std::array<std::string_view, 3U> pieces{{"RX A!", "OK\r", "\nONLY\r\n"}};
  std::size_t consumed = 0U;
  for (std::string_view piece : pieces) {
    const auto result = framer.framer->Push(Bytes(piece), {Capture, &frames});
    consumed += result.bytes_consumed;
  }
  runner.Check(consumed == 15U && frames.count == 2U && Equals(frames, 0U, "RX A!OK\r\n") &&
                   Equals(frames, 1U, "ONLY\r\n"),
               "ascii_split_crlf_and_glued");

  auto overlong = pae::CreateStreamFramer(compiled, 0U);
  CapturedFrames recovered;
  const std::string long_prefix(12U, 'A');
  const auto first = overlong.framer->Push(Bytes(long_prefix), {Capture, &recovered});
  const auto discard = overlong.framer->Observe();
  const auto second = overlong.framer->Push(Bytes("\r\nONLY\r\n"), {Capture, &recovered});
  runner.Check(first.malformed_candidates == 1U && first.bytes_discarded == 12U &&
                   first.last_issue == pae::StreamFramingIssue::RECORD_TOO_LONG &&
                   discard.phase == pae::StreamFramingPhase::DISCARDING_UNTIL_CRLF &&
                   !discard.has_internal_work && second.bytes_discarded == 2U &&
                   second.candidates_delivered == 1U && Equals(recovered, 0U, "ONLY\r\n"),
               "ascii_overlong_discard_recovery");
}

void CheckCodecCompositionAndOwner(Runner& runner, pae::CompiledProtocol compiled) {
  auto framer = pae::CreateStreamFramer(compiled, 0U);
  auto codec = pae::CreateCompleteRecordCodec(compiled);
  pae::CompiledProtocol retained = std::move(compiled);
  retained = pae::CompiledProtocol{};
  runner.Check(framer.status == pae::StreamFramerStatus::OK && framer.framer &&
                   codec.status == pae::CodecStatus::OK && codec.codec,
               "framer_codec_retain_compiled_state");
  if (!framer.framer || !codec.codec) return;
  DecodeContext context{codec.codec.get(), 0U};
  const std::array<std::uint8_t, 6U> frames{{0xAAU, 0U, 1U, 0xAAU, 0U, 2U}};
  const auto result = framer.framer->Push(Bytes(frames), {DecodeOnce, &context});
  runner.Check(result.candidates_delivered == 2U && context.calls == 2U &&
                   context.successes == 2U && context.copied_size == 3U &&
                   context.copied[0] == 0xAAU && context.copied[2] == 2U,
               "one_public_decode_per_candidate");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) return 2;
  Runner runner;
  auto binary = Compile(argv[1], runner, "binary_compile");
  auto ascii = Compile(argv[2], runner, "ascii_compile");
  if (!binary.HasValue() || !ascii.HasValue()) return runner.Finish();

  CheckCapabilities(runner, binary, ascii);
  CheckBinaryStrategies(runner, binary);
  CheckStopContinueResetAndMove(runner, binary);
  CheckGuarding(runner, binary);
  CheckAscii(runner, ascii);
  CheckCodecCompositionAndOwner(runner, Compile(argv[1], runner, "owner_compile"));
  return runner.Finish();
}
