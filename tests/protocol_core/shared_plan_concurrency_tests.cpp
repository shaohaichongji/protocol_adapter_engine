#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "complete_record_codec.h"
#include "test_plan_factory.h"

namespace {

using pae::config_compiler::CompileResult;
using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodeCompleteRecord;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::EncodeCompleteRecord;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;
using pae::protocol_plan::ByteOrder;
using pae::protocol_plan::EncodeSource;
using pae::protocol_plan::FieldPlan;
using pae::protocol_plan::FramingPlan;
using pae::protocol_plan::InputKind;
using pae::protocol_plan::MatcherKind;
using pae::protocol_plan::MatcherPlan;
using pae::protocol_plan::MessagePlan;
using pae::protocol_plan::PipelinePlan;
using pae::protocol_plan::ResourceProfile;
using pae::protocol_plan::ValueType;
using pae::protocol_plan::WireCodec;
using pae::test_support::CompileTestPlan;

constexpr std::size_t kThreadCount = 4U;
constexpr std::size_t kIterationsPerThread = 4096U;
constexpr std::size_t kBusyFieldCount = 2048U;
constexpr std::size_t kBusyIterationsPerThread = 64U;
constexpr std::array<std::string_view, 2U> kExpectedCaseIds{
    "shared_plan_independent_workspace_concurrency",
    "shared_workspace_rejects_concurrent_use",
};

class CyclicBarrier final {
 public:
  explicit CyclicBarrier(std::size_t participant_count) : participant_count_(participant_count) {}

  void Wait() {
    std::unique_lock<std::mutex> lock{mutex_};
    const std::size_t generation = generation_;
    ++arrived_count_;
    if (arrived_count_ == participant_count_) {
      arrived_count_ = 0U;
      ++generation_;
      condition_.notify_all();
      return;
    }
    condition_.wait(lock, [this, generation] { return generation_ != generation; });
  }

 private:
  const std::size_t participant_count_;
  std::mutex mutex_;
  std::condition_variable condition_;
  std::size_t arrived_count_ = 0U;
  std::size_t generation_ = 0U;
};

CompileResult BuildPlan() {
  MessagePlan message;
  message.id = "shared_message";
  message.direction_id = "shared_direction";
  message.frame_length_bytes = 2U;

  MatcherPlan length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = 2U;
  message.matchers.push_back(length_matcher);

  MatcherPlan fixed_matcher;
  fixed_matcher.kind = MatcherKind::FIXED_BYTES;
  fixed_matcher.byte_offset = 0U;
  fixed_matcher.bytes.push_back(0xA5U);
  message.matchers.push_back(std::move(fixed_matcher));

  FieldPlan prefix;
  prefix.id = "prefix";
  prefix.value_type = ValueType::UINT64;
  prefix.wire_codec = WireCodec::UNSIGNED_INTEGER;
  prefix.byte_offset = 0U;
  prefix.byte_width = 1U;
  prefix.byte_order = ByteOrder::NOT_APPLICABLE;
  prefix.encode_source = EncodeSource::CONSTANT;
  prefix.constant_value = 0xA5U;
  message.fields.push_back(std::move(prefix));

  FieldPlan value;
  value.id = "value";
  value.value_type = ValueType::UINT64;
  value.wire_codec = WireCodec::UNSIGNED_INTEGER;
  value.byte_offset = 1U;
  value.byte_width = 1U;
  value.byte_order = ByteOrder::NOT_APPLICABLE;
  value.encode_source = EncodeSource::INPUT;
  message.fields.push_back(std::move(value));

  PipelinePlan pipeline;
  pipeline.id = "shared_pipeline";
  pipeline.direction_id = "shared_direction";
  pipeline.framing_profile_index = 0U;
  pipeline.message_indices.push_back(0U);

  return CompileTestPlan("shared_plan_protocol", ResourceProfile::DESKTOP,
                         {FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                         {std::move(pipeline)}, {std::move(message)});
}

CompileResult BuildBusyPlan() {
  MessagePlan message;
  message.id = "busy_message";
  message.direction_id = "busy_direction";
  message.frame_length_bytes = kBusyFieldCount;

  MatcherPlan length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = kBusyFieldCount;
  message.matchers.push_back(length_matcher);

  message.fields.reserve(kBusyFieldCount);
  for (std::size_t field_index = 0U; field_index < kBusyFieldCount; ++field_index) {
    FieldPlan field;
    field.id = "field_" + std::to_string(field_index);
    field.value_type = ValueType::UINT64;
    field.wire_codec = WireCodec::UNSIGNED_INTEGER;
    field.byte_offset = field_index;
    field.byte_width = 1U;
    field.byte_order = ByteOrder::NOT_APPLICABLE;
    field.encode_source = EncodeSource::INPUT;
    message.fields.push_back(std::move(field));
  }

  PipelinePlan pipeline;
  pipeline.id = "busy_pipeline";
  pipeline.direction_id = "busy_direction";
  pipeline.framing_profile_index = 0U;
  pipeline.message_indices.push_back(0U);

  return CompileTestPlan("shared_workspace_protocol", ResourceProfile::DESKTOP,
                         {FramingPlan{"complete_record", InputKind::COMPLETE_RECORD}},
                         {std::move(pipeline)}, {std::move(message)});
}

bool RunWorker(const pae::protocol_plan::PlanBundle& plan, std::size_t thread_index,
               std::mutex& start_mutex, std::condition_variable& start_condition,
               std::size_t& ready_count, bool& start, std::atomic<bool>& failed) {
  ExecutionWorkspace workspace{plan};
  EncodeFieldValue input;
  input.field = FieldRef{&plan, 0U, 1U};
  input.value_kind = LogicalValueKind::UINT64;
  input.uint64_value = static_cast<std::uint64_t>(thread_index + 1U);
  const std::array<std::uint8_t, 2U> expected{0xA5U, static_cast<std::uint8_t>(input.uint64_value)};
  const std::array<std::uint8_t, 2U> unknown{0x5AU, expected[1]};

  {
    std::unique_lock<std::mutex> lock{start_mutex};
    ++ready_count;
    start_condition.notify_all();
    start_condition.wait(lock, [&start] { return start; });
  }

  for (std::size_t iteration = 0U;
       iteration < kIterationsPerThread && !failed.load(std::memory_order_relaxed); ++iteration) {
    std::array<DecodedFieldSlot, 2U> slots{};
    const auto decoded =
        DecodeCompleteRecord(plan, workspace, 0U, ByteView{expected.data(), expected.size()},
                             slots.data(), slots.size());
    if (decoded.status != CodecStatus::OK || decoded.field_count != slots.size() ||
        slots[0].uint64_value != 0xA5U || slots[1].uint64_value != input.uint64_value) {
      failed.store(true, std::memory_order_relaxed);
      return false;
    }

    std::array<std::uint8_t, 2U> output{0xCCU, 0xCCU};
    const auto encoded = EncodeCompleteRecord(plan, workspace, 0U, 0U, &input, 1U,
                                              MutableByteBuffer{output.data(), output.size()});
    if (encoded.status != CodecStatus::OK || encoded.bytes_written != output.size() ||
        output != expected) {
      failed.store(true, std::memory_order_relaxed);
      return false;
    }

    const auto unknown_result = DecodeCompleteRecord(
        plan, workspace, 0U, ByteView{unknown.data(), unknown.size()}, slots.data(), slots.size());
    if (unknown_result.status != CodecStatus::UNKNOWN_MESSAGE || unknown_result.field_count != 0U) {
      failed.store(true, std::memory_order_relaxed);
      return false;
    }

    output.fill(0xCCU);
    const auto missing = EncodeCompleteRecord(plan, workspace, 0U, 0U, nullptr, 0U,
                                              MutableByteBuffer{output.data(), output.size()});
    if (missing.status != CodecStatus::MISSING_FIELD || missing.bytes_written != 0U ||
        output != std::array<std::uint8_t, 2U>{0xCCU, 0xCCU}) {
      failed.store(true, std::memory_order_relaxed);
      return false;
    }
  }
  return !failed.load(std::memory_order_relaxed);
}

bool RunSharedPlanStress() {
  CompileResult frozen = BuildPlan();
  if (!frozen.Succeeded()) {
    return false;
  }

  std::mutex start_mutex;
  std::condition_variable start_condition;
  std::size_t ready_count = 0U;
  bool start = false;
  std::atomic<bool> failed{false};
  std::array<bool, kThreadCount> worker_results{};
  std::vector<std::thread> workers;
  workers.reserve(kThreadCount);
  for (std::size_t thread_index = 0U; thread_index < kThreadCount; ++thread_index) {
    workers.emplace_back([&, thread_index] {
      worker_results[thread_index] = RunWorker(*frozen.Plan(), thread_index, start_mutex,
                                               start_condition, ready_count, start, failed);
    });
  }

  {
    std::unique_lock<std::mutex> lock{start_mutex};
    start_condition.wait(lock, [&ready_count] { return ready_count == kThreadCount; });
    start = true;
  }
  start_condition.notify_all();

  for (std::thread& worker : workers) {
    worker.join();
  }
  return !failed.load(std::memory_order_relaxed) &&
         std::all_of(worker_results.begin(), worker_results.end(),
                     [](bool value) { return value; });
}

bool RunSharedWorkspaceBusyStress() {
  CompileResult frozen = BuildBusyPlan();
  if (!frozen.Succeeded()) {
    return false;
  }

  ExecutionWorkspace shared_workspace{*frozen.Plan()};
  CyclicBarrier barrier{kThreadCount};
  std::atomic<std::size_t> busy_count{0U};
  std::atomic<std::size_t> success_count{0U};
  std::atomic<bool> unexpected{false};
  std::vector<std::thread> workers;
  workers.reserve(kThreadCount);
  for (std::size_t thread_index = 0U; thread_index < kThreadCount; ++thread_index) {
    workers.emplace_back([&, thread_index] {
      std::vector<EncodeFieldValue> values(kBusyFieldCount);
      std::vector<std::uint8_t> expected(kBusyFieldCount);
      std::vector<std::uint8_t> output(kBusyFieldCount, 0xCCU);
      for (std::size_t field_index = 0U; field_index < kBusyFieldCount; ++field_index) {
        const auto value = static_cast<std::uint8_t>((thread_index + field_index) & 0xFFU);
        values[field_index].field = FieldRef{frozen.Plan(), 0U, field_index};
        values[field_index].value_kind = LogicalValueKind::UINT64;
        values[field_index].uint64_value = value;
        expected[field_index] = value;
      }

      for (std::size_t iteration = 0U; iteration < kBusyIterationsPerThread; ++iteration) {
        std::fill(output.begin(), output.end(), std::uint8_t{0xCCU});
        barrier.Wait();
        const auto result =
            EncodeCompleteRecord(*frozen.Plan(), shared_workspace, 0U, 0U, values.data(),
                                 values.size(), MutableByteBuffer{output.data(), output.size()});
        if (result.status == CodecStatus::WORKSPACE_BUSY) {
          busy_count.fetch_add(1U, std::memory_order_relaxed);
          if (result.bytes_written != 0U ||
              !std::all_of(output.begin(), output.end(),
                           [](std::uint8_t value) { return value == 0xCCU; })) {
            unexpected.store(true, std::memory_order_relaxed);
          }
        } else if (result.status == CodecStatus::OK && result.bytes_written == output.size() &&
                   output == expected) {
          success_count.fetch_add(1U, std::memory_order_relaxed);
        } else {
          unexpected.store(true, std::memory_order_relaxed);
        }
      }
    });
  }

  for (std::thread& worker : workers) {
    worker.join();
  }
  return !unexpected.load(std::memory_order_relaxed) &&
         busy_count.load(std::memory_order_relaxed) != 0U &&
         success_count.load(std::memory_order_relaxed) != 0U;
}

}  // namespace

int main() {
  const std::array<bool, kExpectedCaseIds.size()> results{RunSharedPlanStress(),
                                                          RunSharedWorkspaceBusyStress()};
  std::size_t passed_count = 0U;
  for (std::size_t case_index = 0U; case_index < kExpectedCaseIds.size(); ++case_index) {
    passed_count += results[case_index] ? 1U : 0U;
    std::cout << (results[case_index] ? "PASS" : "FAIL") << " case=" << kExpectedCaseIds[case_index]
              << '\n';
  }
  const std::size_t failed_count = kExpectedCaseIds.size() - passed_count;
  const bool passed = failed_count == 0U;
  std::cout << "SHARED_PLAN_CONCURRENCY_TEST_SUMMARY passed=" << passed_count
            << " failed=" << failed_count << " expected=" << kExpectedCaseIds.size()
            << " gate=" << (passed ? "PASS" : "FAIL") << '\n';
  return passed ? 0 : 1;
}
