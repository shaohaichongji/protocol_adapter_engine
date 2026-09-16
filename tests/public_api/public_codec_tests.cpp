#include <pae/codec.h>
#include <pae/compiler.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "codec_test_support.h"

namespace {

std::atomic<std::size_t> g_allocation_count{0U};

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
    std::cout << "PUBLIC_CODEC_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
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

bool BytesEqual(pae::ByteView actual, const std::uint8_t* expected,
                std::size_t expected_size) noexcept {
  if (actual.size != expected_size || (expected_size != 0U && actual.data == nullptr)) return false;
  for (std::size_t index = 0U; index < expected_size; ++index) {
    if (actual.data[index] != expected[index]) return false;
  }
  return true;
}

struct BusyGate {
  std::atomic<bool> entered{false};
  std::atomic<bool> release{false};
};

void HoldAfterFacadeGuard(void* context) noexcept {
  auto& gate = *static_cast<BusyGate*>(context);
  gate.entered.store(true, std::memory_order_release);
  while (!gate.release.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }
}

bool WaitUntilEntered(const BusyGate& gate) {
  for (std::size_t attempt = 0U; attempt < 1000000U; ++attempt) {
    if (gate.entered.load(std::memory_order_acquire)) return true;
    std::this_thread::yield();
  }
  return false;
}

void CheckBinary(Runner& runner, const char* config_path) {
  auto compiled = Compile(config_path, runner, "binary_compile");
  pae::CompleteRecordCodecCreateResult empty =
      pae::CreateCompleteRecordCodec(pae::CompiledProtocol{});
  runner.Check(empty.status == pae::CodecStatus::INVALID_COMPILED_PROTOCOL && !empty.codec,
               "empty_owner_rejected");

  auto created = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(created.status == pae::CodecStatus::OK && created.codec != nullptr,
               "binary_codec_create");
  if (!created.codec) return;
  const auto memory = created.memory;
  runner.Check(memory.core_workspace_bytes != 0U && memory.decoded_slot_bytes != 0U &&
                   memory.encode_mapping_bytes != 0U && memory.facade_bytes != 0U &&
                   memory.codec_accounted_total_bytes ==
                       memory.core_workspace_bytes + memory.decoded_slot_bytes +
                           memory.encode_mapping_bytes + memory.facade_bytes &&
                   memory.retained_compiled_state_facade_bytes != 0U &&
                   memory.allocation_count >= 4U,
               "memory_categories_exact_sum");

  const auto exact = pae::CreateCompleteRecordCodec(
      compiled, {pae::kUsePlanDecodedFieldCapacity, memory.codec_accounted_total_bytes});
  const auto below = pae::CreateCompleteRecordCodec(
      compiled, {pae::kUsePlanDecodedFieldCapacity, memory.codec_accounted_total_bytes - 1U});
  runner.Check(exact.status == pae::CodecStatus::OK && exact.codec != nullptr,
               "memory_limit_exact");
  runner.Check(below.status == pae::CodecStatus::RESOURCE_LIMIT_EXCEEDED && !below.codec &&
                   below.memory.codec_accounted_total_bytes == memory.codec_accounted_total_bytes,
               "memory_limit_minus_one");

  auto small_created = pae::CreateCompleteRecordCodec(compiled, {1U, 1000000U});
  runner.Check(small_created.status == pae::CodecStatus::OK && small_created.codec != nullptr,
               "small_decode_capacity_create");
  auto move_a = pae::CreateCompleteRecordCodec(compiled);
  auto move_b = pae::CreateCompleteRecordCodec(compiled);
  auto move_c = pae::CreateCompleteRecordCodec(compiled);
  auto move_d = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(move_a.status == pae::CodecStatus::OK && move_a.codec &&
                   move_b.status == pae::CodecStatus::OK && move_b.codec &&
                   move_c.status == pae::CodecStatus::OK && move_c.codec &&
                   move_d.status == pae::CodecStatus::OK && move_d.codec,
               "move_test_codecs_create");

  std::unique_ptr<pae::CompleteRecordCodec> codec = std::move(created.codec);
  compiled = pae::CompiledProtocol{};

  const std::array<std::uint8_t, 4> payload{0x11U, 0x22U, 0x33U, 0x44U};
  const std::array values{
      pae::EncodeValue::UInt64({0U, 0U}, 13U),
      pae::EncodeValue::UInt64({0U, 2U}, 0x030201U),
      pae::EncodeValue::Bytes({0U, 3U}, {payload.data(), payload.size()}),
      pae::EncodeValue::Enum({0U, 4U, 1U}),
  };
  const std::array<std::uint8_t, 13> expected{0x0DU, 0x00U, 0xA7U, 0x31U, 0x01U, 0x02U, 0x03U,
                                              0x11U, 0x22U, 0x33U, 0x44U, 0x02U, 0x00U};
  std::array<std::uint8_t, 13> output{};
  output.fill(0xCCU);
  const std::size_t before_first_calls = g_allocation_count.load(std::memory_order_relaxed);
  auto encoded =
      codec->Encode(0U, 0U, values.data(), values.size(), {output.data(), output.size()});
  auto decoded = codec->Decode(0U, {expected.data(), expected.size()});
  const std::size_t after_first_calls = g_allocation_count.load(std::memory_order_relaxed);
  runner.Check(encoded.status == pae::CodecStatus::OK && encoded.bytes_written == expected.size() &&
                   output == expected,
               "binary_independent_encode");
  runner.Check(before_first_calls == after_first_calls, "first_calls_no_allocation");

  const auto first = decoded.record.Field(0U);
  const auto bytes = decoded.record.Field(3U);
  const auto enum_value = decoded.record.Field(4U);
  runner.Check(decoded.status == pae::CodecStatus::OK && decoded.record.HasValue() &&
                   decoded.record.MessageIndex() == 0U && decoded.record.FieldCount() == 5U,
               "binary_decode_record");
  runner.Check(first.has_value() && first->Kind() == pae::ValueKind::UINT64 &&
                   first->UInt64() == 13U && first->FlatFieldIndex() == 0U,
               "binary_uint64_view");
  runner.Check(bytes.has_value() && bytes->Kind() == pae::ValueKind::BYTES &&
                   bytes->Bytes().has_value() && bytes->Bytes()->data == expected.data() + 7U &&
                   BytesEqual(*bytes->Bytes(), payload.data(), payload.size()),
               "binary_bytes_borrow_input");
  runner.Check(enum_value.has_value() && enum_value->Kind() == pae::ValueKind::ENUM &&
                   enum_value->EnumRawValue() == 2U && enum_value->KnownEnumFlatIndex() == 1U,
               "binary_enum_raw_and_known");

  if (small_created.codec) {
    const auto too_small = small_created.codec->Decode(0U, {expected.data(), expected.size()});
    runner.Check(too_small.status == pae::CodecStatus::OUTPUT_SLOTS_TOO_SMALL &&
                     too_small.required_field_count == 5U && !too_small.record.HasValue(),
                 "decode_capacity_failure_zero_delivery");
  }

  std::array<std::uint8_t, 12> short_output{};
  const auto output_small = codec->Encode(0U, 0U, values.data(), values.size(),
                                          {short_output.data(), short_output.size()});
  runner.Check(output_small.status == pae::CodecStatus::BUFFER_TOO_SMALL &&
                   output_small.required_size == 13U && output_small.bytes_written == 0U,
               "encode_buffer_small_zero_delivery");
  runner.Check(!decoded.record.HasValue() && !first->HasValue(), "next_call_invalidates_views");

  auto wrong_type_values = values;
  wrong_type_values[0] = pae::EncodeValue::Int64({0U, 0U}, 13);
  const auto wrong_type = codec->Encode(0U, 0U, wrong_type_values.data(), wrong_type_values.size(),
                                        {output.data(), output.size()});
  runner.Check(wrong_type.status == pae::CodecStatus::TYPE_MISMATCH &&
                   wrong_type.bytes_written == 0U && wrong_type.failed_field_flat_index == 0U,
               "typed_mismatch_preserved");

  auto bad_enum_values = values;
  bad_enum_values[3] = pae::EncodeValue::Enum({0U, 4U, 99U});
  const auto bad_enum = codec->Encode(0U, 0U, bad_enum_values.data(), bad_enum_values.size(),
                                      {output.data(), output.size()});
  runner.Check(
      bad_enum.status == pae::CodecStatus::ENUM_REFERENCE_MISMATCH && bad_enum.bytes_written == 0U,
      "known_enum_selector_validated");

  const auto over_capacity = codec->Encode(0U, 0U, nullptr, 6U, {output.data(), output.size()});
  runner.Check(over_capacity.status == pae::CodecStatus::INPUT_VALUES_TOO_MANY &&
                   over_capacity.bytes_written == 0U,
               "encode_mapping_capacity_precedes_input_read");

  const auto success_before_failure = codec->Decode(0U, {expected.data(), expected.size()});
  std::array<std::uint8_t, 1> unknown{0x00U};
  const auto failed = codec->Decode(0U, {unknown.data(), unknown.size()});
  runner.Check(success_before_failure.status == pae::CodecStatus::OK &&
                   failed.status == pae::CodecStatus::UNKNOWN_MESSAGE &&
                   !failed.record.HasValue() && !success_before_failure.record.HasValue(),
               "failure_does_not_publish_old_success");

  const std::size_t before_calls = g_allocation_count.load(std::memory_order_relaxed);
  encoded = codec->Encode(0U, 0U, values.data(), values.size(), {output.data(), output.size()});
  decoded = codec->Decode(0U, {expected.data(), expected.size()});
  const auto no_allocation_field = decoded.record.Field(3U);
  const auto no_allocation_bytes = no_allocation_field->Bytes();
  const std::size_t after_calls = g_allocation_count.load(std::memory_order_relaxed);
  runner.Check(encoded.status == pae::CodecStatus::OK && decoded.status == pae::CodecStatus::OK &&
                   no_allocation_bytes.has_value() && before_calls == after_calls,
               "repeated_calls_no_allocation");

  BusyGate gate;
  pae::public_api_internal::test_only::SetCodecEnteredHook(&HoldAfterFacadeGuard, &gate);
  pae::DecodeResult active_result;
  std::thread active(
      [&] { active_result = codec->Decode(0U, {expected.data(), expected.size()}); });
  const bool entered = WaitUntilEntered(gate);
  const auto competing = codec->Decode(0U, {expected.data(), expected.size()});
  pae::public_api_internal::test_only::SetCodecEnteredHook(nullptr, nullptr);
  gate.release.store(true, std::memory_order_release);
  active.join();
  runner.Check(entered && competing.status == pae::CodecStatus::WORKSPACE_BUSY &&
                   !competing.record.HasValue() && active_result.status == pae::CodecStatus::OK &&
                   active_result.record.HasValue(),
               "facade_busy_precedes_cache_mutation");

  if (move_a.codec && move_b.codec && move_c.codec && move_d.codec) {
    const auto old_a = move_a.codec->Decode(0U, {expected.data(), expected.size()});
    const auto old_a_field = old_a.record.Field(0U);
    const auto old_b = move_b.codec->Decode(0U, {expected.data(), expected.size()});
    const auto old_b_field = old_b.record.Field(0U);
    *move_a.codec = std::move(*move_b.codec);
    runner.Check(!old_a.record.HasValue() && old_a_field.has_value() && !old_a_field->HasValue() &&
                     !old_b.record.HasValue() && old_b_field.has_value() &&
                     !old_b_field->HasValue(),
                 "move_assignment_invalidates_source_and_destination_views");
    runner.Check(move_b.codec->Decode(0U, {expected.data(), expected.size()}).status ==
                     pae::CodecStatus::INVALID_COMPILED_PROTOCOL,
                 "move_assignment_source_is_empty");
    const auto assigned_view = move_a.codec->Decode(0U, {expected.data(), expected.size()});
    runner.Check(assigned_view.status == pae::CodecStatus::OK && assigned_view.record.HasValue() &&
                     assigned_view.record.Field(0U)->HasValue(),
                 "move_assignment_transferred_codec_publishes_new_view");

    const auto old_c = move_c.codec->Decode(0U, {expected.data(), expected.size()});
    const auto old_c_field = old_c.record.Field(0U);
    pae::CompleteRecordCodec moved_codec(std::move(*move_c.codec));
    runner.Check(!old_c.record.HasValue() && old_c_field.has_value() && !old_c_field->HasValue(),
                 "move_construction_invalidates_source_views");
    runner.Check(move_c.codec->Decode(0U, {expected.data(), expected.size()}).status ==
                     pae::CodecStatus::INVALID_COMPILED_PROTOCOL,
                 "move_construction_source_is_empty");
    const auto constructed_view = moved_codec.Decode(0U, {expected.data(), expected.size()});
    runner.Check(constructed_view.status == pae::CodecStatus::OK &&
                     constructed_view.record.HasValue() &&
                     constructed_view.record.Field(0U)->HasValue(),
                 "move_constructed_codec_publishes_new_view");

    const auto old_d = move_d.codec->Decode(0U, {expected.data(), expected.size()});
    const auto old_d_field = old_d.record.Field(0U);
    *move_b.codec = std::move(*move_d.codec);
    runner.Check(!old_b.record.HasValue() && old_b_field.has_value() && !old_b_field->HasValue() &&
                     !old_d.record.HasValue() && old_d_field.has_value() &&
                     !old_d_field->HasValue(),
                 "moved_from_reuse_does_not_revive_old_views");
    const auto reused_view = move_b.codec->Decode(0U, {expected.data(), expected.size()});
    runner.Check(reused_view.status == pae::CodecStatus::OK && reused_view.record.HasValue() &&
                     reused_view.record.Field(0U)->HasValue(),
                 "reused_moved_from_codec_publishes_new_view");
  }
}

void CheckInt64AndTypedValues(Runner& runner, const char* config_path) {
  auto compiled = Compile(config_path, runner, "int64_compile");
  auto created = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(created.status == pae::CodecStatus::OK && created.codec != nullptr,
               "int64_codec_create");
  if (!created.codec) return;

  const std::array<std::uint8_t, 2> payload{0xCAU, 0xFEU};
  const std::array values{
      pae::EncodeValue::Int64({0U, 0U}, -2),
      pae::EncodeValue::Int64({0U, 1U}, -8388608),
      pae::EncodeValue::Int64({0U, 3U}, (std::numeric_limits<std::int64_t>::min)()),
      pae::EncodeValue::UInt64({0U, 4U}, 255U),
      pae::EncodeValue::Bool({0U, 5U}, true),
      pae::EncodeValue::Bytes({0U, 6U}, {payload.data(), payload.size()}),
      pae::EncodeValue::Enum({0U, 7U, 0U}),
  };
  const std::array<std::uint8_t, 25> expected{
      0xFFU, 0xFFU, 0xFEU, 0x00U, 0x00U, 0x80U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFEU, 0x80U, 0x00U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFFU, 0xA1U, 0xCAU, 0xFEU, 0x02U, 0x60U};
  std::array<std::uint8_t, 25> output{};
  const auto encoded =
      created.codec->Encode(0U, 0U, values.data(), values.size(), {output.data(), output.size()});
  const auto decoded = created.codec->Decode(0U, {expected.data(), expected.size()});
  const auto signed_value = decoded.record.Field(0U);
  const auto bool_value = decoded.record.Field(5U);
  runner.Check(encoded.status == pae::CodecStatus::OK && output == expected,
               "int64_independent_encode");
  runner.Check(decoded.status == pae::CodecStatus::OK && signed_value.has_value() &&
                   signed_value->Kind() == pae::ValueKind::INT64 && signed_value->Int64() == -2 &&
                   bool_value.has_value() && bool_value->Kind() == pae::ValueKind::BOOL &&
                   bool_value->Bool() == true,
               "int64_bool_independent_decode");
}

void CheckDecimal(Runner& runner, const char* config_path) {
  auto compiled = Compile(config_path, runner, "decimal_compile");
  auto created = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(created.status == pae::CodecStatus::OK && created.codec != nullptr,
               "decimal_codec_create");
  if (!created.codec) return;

  const std::array values{
      pae::EncodeValue::Decimal({0U, 0U}, {123, 1}),
      pae::EncodeValue::Decimal({0U, 1U}, {(std::numeric_limits<std::int64_t>::max)(), 0}),
      pae::EncodeValue::Decimal({0U, 2U}, {0, 0}),
      pae::EncodeValue::Decimal({0U, 3U}, {(std::numeric_limits<std::int64_t>::max)(), 0}),
  };
  const std::array<std::uint8_t, 34> expected{
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x02U, 0x0BU, 0xFFU, 0xFFU, 0xFFU, 0xFFU,
      0xFFU, 0xFFU, 0xFFU, 0xFFU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
      0x80U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xA5U, 0x2AU};
  std::array<std::uint8_t, 34> output{};
  const auto encoded =
      created.codec->Encode(0U, 0U, values.data(), values.size(), {output.data(), output.size()});
  const auto decoded = created.codec->Decode(0U, {expected.data(), expected.size()});
  const auto temperature = decoded.record.Field(0U);
  const auto cancelled = decoded.record.Field(1U);
  const auto temperature_decimal =
      temperature.has_value() ? temperature->Decimal() : std::optional<pae::Decimal64>{};
  runner.Check(encoded.status == pae::CodecStatus::OK && output == expected,
               "decimal_exact_encode");
  runner.Check(decoded.status == pae::CodecStatus::OK && temperature.has_value() &&
                   temperature->Kind() == pae::ValueKind::DECIMAL64 &&
                   temperature_decimal.has_value() && temperature_decimal->coefficient == 123 &&
                   temperature_decimal->scale == 1 &&
                   temperature->ConversionRawKind() == pae::RawIntegerKind::INT64 &&
                   temperature->ConversionRawInt64() == 523 && cancelled.has_value() &&
                   cancelled->ConversionRawKind() == pae::RawIntegerKind::UINT64 &&
                   cancelled->ConversionRawUInt64() == (std::numeric_limits<std::uint64_t>::max)(),
               "decimal_logical_and_recorded_raw");

  auto failed_values = values;
  failed_values[0] =
      pae::EncodeValue::Decimal({0U, 0U}, {(std::numeric_limits<std::int64_t>::max)(), 0});
  const auto conversion_failure = created.codec->Encode(
      0U, 0U, failed_values.data(), failed_values.size(), {output.data(), output.size()});
  runner.Check(conversion_failure.status == pae::CodecStatus::VALUE_NOT_REPRESENTABLE &&
                   conversion_failure.conversion_error == pae::ConversionError::RAW_OUT_OF_RANGE &&
                   conversion_failure.failed_value_index == 0U &&
                   conversion_failure.failed_field_flat_index == 0U &&
                   conversion_failure.bytes_written == 0U,
               "conversion_failure_position_propagated");
}

void CheckAscii(Runner& runner, const char* config_path) {
  auto compiled = Compile(config_path, runner, "ascii_compile");
  auto first = pae::CreateCompleteRecordCodec(compiled);
  auto second = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(first.status == pae::CodecStatus::OK && first.codec &&
                   second.status == pae::CodecStatus::OK && second.codec,
               "multiple_workspaces_same_plan");
  if (!first.codec || !second.codec) return;

  const std::string name = "ALICE";
  const std::string tag = "Z";
  const std::array values{
      pae::EncodeValue::Bytes({0U, 0U},
                              {reinterpret_cast<const std::uint8_t*>(name.data()), name.size()}),
      pae::EncodeValue::Bytes({0U, 2U},
                              {reinterpret_cast<const std::uint8_t*>(tag.data()), tag.size()}),
  };
  std::array<std::uint8_t, 32> output{};
  const auto encoded =
      first.codec->Encode(0U, 0U, values.data(), values.size(), {output.data(), output.size()});
  const std::string expected = "TX ALICE!Z\r\n";
  runner.Check(encoded.status == pae::CodecStatus::OK && encoded.bytes_written == expected.size() &&
                   std::string_view(reinterpret_cast<const char*>(output.data()),
                                    encoded.bytes_written) == expected,
               "ascii_independent_encode");

  const std::string reply = "RX ALICE!OK\r\n";
  const pae::ByteView reply_view{reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size()};
  const auto decoded_first = first.codec->Decode(0U, reply_view);
  const auto decoded_second = second.codec->Decode(0U, reply_view);
  const auto name_view = decoded_first.record.Field(0U);
  runner.Check(decoded_first.status == pae::CodecStatus::OK &&
                   decoded_second.status == pae::CodecStatus::OK &&
                   decoded_first.record.HasValue() && decoded_second.record.HasValue(),
               "workspace_results_are_isolated");
  runner.Check(name_view.has_value() && name_view->Bytes().has_value() &&
                   name_view->Bytes()->data == reply_view.data + 3U &&
                   name_view->Bytes()->size == 5U,
               "ascii_bytes_borrow_record");

  std::string invalid = "RX ";
  invalid.push_back(static_cast<char>(0x80U));
  invalid += "!OK\r\n";
  const auto invalid_result = first.codec->Decode(
      0U, {reinterpret_cast<const std::uint8_t*>(invalid.data()), invalid.size()});
  runner.Check(invalid_result.status == pae::CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
                   !invalid_result.record.HasValue(),
               "ascii_failure_zero_delivery");
  runner.Check(decoded_second.record.HasValue(), "other_workspace_view_remains_valid");
}

void CheckAsciiOneWay(Runner& runner, const char* config_path) {
  auto compiled = Compile(config_path, runner, "ascii_one_way_compile");
  auto created = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(created.status == pae::CodecStatus::OK && created.codec,
               "ascii_one_way_codec_create");
  if (!created.codec) return;

  const std::string stream_name = "A";
  const std::string stream_tag = "Z";
  const std::array stream_values{
      pae::EncodeValue::Bytes({0U, 0U}, {reinterpret_cast<const std::uint8_t*>(stream_name.data()),
                                         stream_name.size()}),
      pae::EncodeValue::Bytes(
          {0U, 2U}, {reinterpret_cast<const std::uint8_t*>(stream_tag.data()), stream_tag.size()}),
  };
  std::array<std::uint8_t, 12> stream_output{};
  const auto stream_encoded =
      created.codec->Encode(0U, 0U, stream_values.data(), stream_values.size(),
                            {stream_output.data(), stream_output.size()});
  const std::string stream_reply = "RX A!OK\r\n";
  const auto stream_decoded = created.codec->Decode(
      0U, {reinterpret_cast<const std::uint8_t*>(stream_reply.data()), stream_reply.size()});
  runner.Check(
      stream_encoded.status == pae::CodecStatus::OK && stream_encoded.bytes_written == 8U &&
          stream_decoded.status == pae::CodecStatus::OK && stream_decoded.record.FieldCount() == 2U,
      "stream_pipeline_complete_record_actions_match_metadata");

  std::array<std::uint8_t, 6> output{};
  const auto literal_encode =
      created.codec->Encode(1U, 2U, nullptr, 0U, {output.data(), output.size()});
  const std::string_view expected = "SEND\r\n";
  runner.Check(literal_encode.status == pae::CodecStatus::OK &&
                   literal_encode.bytes_written == expected.size() &&
                   std::string_view(reinterpret_cast<const char*>(output.data()),
                                    literal_encode.bytes_written) == expected,
               "ascii_literal_only_zero_input_encode");

  const std::string decode_text = "ONLY\r\n";
  const auto literal_decode = created.codec->Decode(
      2U, {reinterpret_cast<const std::uint8_t*>(decode_text.data()), decode_text.size()});
  runner.Check(literal_decode.status == pae::CodecStatus::OK && literal_decode.record.HasValue() &&
                   literal_decode.record.FieldCount() == 0U,
               "ascii_literal_only_zero_field_decode");

  const auto decode_unsupported = created.codec->Decode(
      1U, {reinterpret_cast<const std::uint8_t*>(expected.data()), expected.size()});
  const auto encode_unsupported =
      created.codec->Encode(2U, 1U, nullptr, 0U, {output.data(), output.size()});
  runner.Check(decode_unsupported.status == pae::CodecStatus::OPERATION_NOT_SUPPORTED &&
                   !decode_unsupported.record.HasValue() &&
                   encode_unsupported.status == pae::CodecStatus::OPERATION_NOT_SUPPORTED &&
                   encode_unsupported.bytes_written == 0U,
               "ascii_unsupported_actions");
}

void CheckPreservedUnknownEnum(Runner& runner, const char* config_path) {
  auto compiled = Compile(config_path, runner, "preserved_enum_compile");
  auto created = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(created.status == pae::CodecStatus::OK && created.codec,
               "preserved_enum_codec_create");
  if (!created.codec) return;

  const std::array<std::uint8_t, 10> frame{0x80U, 0x0FU, 3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU};
  const auto decoded = created.codec->Decode(0U, {frame.data(), frame.size()});
  const auto field = decoded.record.Field(1U);
  runner.Check(decoded.status == pae::CodecStatus::OK && decoded.output_tainted &&
                   field.has_value() && field->Kind() == pae::ValueKind::ENUM &&
                   field->EnumRawValue() == 3U && !field->KnownEnumFlatIndex().has_value(),
               "preserved_unknown_enum_raw_empty_known_tainted");
}

}  // namespace

void* operator new(std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* storage = std::malloc(size)) return storage;
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
  g_allocation_count.fetch_add(1U, std::memory_order_relaxed);
  if (void* storage = std::malloc(size)) return storage;
  throw std::bad_alloc{};
}

void operator delete(void* storage) noexcept { std::free(storage); }
void operator delete[](void* storage) noexcept { std::free(storage); }
void operator delete(void* storage, std::size_t) noexcept { std::free(storage); }
void operator delete[](void* storage, std::size_t) noexcept { std::free(storage); }

int main(int argc, char** argv) {
  Runner runner;
  runner.Check(argc == 7, "arguments");
  if (argc != 7) return runner.Finish();
  CheckBinary(runner, argv[1]);
  CheckInt64AndTypedValues(runner, argv[2]);
  CheckDecimal(runner, argv[3]);
  CheckAscii(runner, argv[4]);
  CheckPreservedUnknownEnum(runner, argv[5]);
  CheckAsciiOneWay(runner, argv[6]);
  return runner.Finish();
}
