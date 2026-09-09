#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "business_adapter.h"

namespace {

using pae::examples::business_embedding::BusinessAdapter;
using pae::examples::business_embedding::Command;
using pae::examples::business_embedding::Measurement;
using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::ConversionError;
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
using pae::examples::business_embedding::BoundedRecordAdapter;
#endif

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool Expect(bool condition, const char* message) {
  if (!condition) std::cerr << "FAIL: " << message << '\n';
  return condition;
}

bool ReplaceOnce(std::string& text, std::string_view from, std::string_view to) {
  const std::size_t position = text.find(from);
  if (position == std::string::npos ||
      text.find(from, position + from.size()) != std::string::npos) {
    return false;
  }
  text.replace(position, from.size(), to);
  return true;
}

struct Observer {
  std::size_t measurement_calls = 0U;
  std::size_t bytes_calls = 0U;
  Measurement latest_measurement;
  std::vector<Measurement> measurement_copies;
  std::vector<std::uint8_t> latest_bytes;
  std::vector<std::vector<std::uint8_t>> byte_copies;

  pae::examples::business_embedding::Callbacks Callbacks() {
    return {[this](const Measurement& value) {
              ++measurement_calls;
              latest_measurement = value;
              measurement_copies.push_back(value);
            },
            [this](ByteView value) {
              ++bytes_calls;
              latest_bytes.assign(value.data, value.data + value.size);
              byte_copies.push_back(latest_bytes);
            }};
  }
};

bool TestInitialization(const std::string& config) {
  Observer observer;
  std::string error;
  auto missing_callbacks = BusinessAdapter::Initialize(config, {}, error);
  if (!Expect(missing_callbacks == nullptr &&
                  error == "both synchronous business callbacks are required",
              "missing callbacks are rejected before compilation or execution")) {
    return false;
  }
  auto valid = BusinessAdapter::Initialize(config, observer.Callbacks(), error);
  if (!Expect(valid != nullptr && error.empty(), "valid business bindings initialize"))
    return false;

  std::string missing = config;
  if (!Expect(ReplaceOnce(missing, "\"id\": \"alarm\"", "\"id\": \"renamed_alarm\""),
              "missing-field fixture mutation is unique")) {
    return false;
  }
  auto missing_adapter = BusinessAdapter::Initialize(missing, observer.Callbacks(), error);
  if (!Expect(missing_adapter == nullptr && error == "required business field id is missing",
              "missing field is rejected before execution")) {
    return false;
  }

  std::string wrong_type = config;
  if (!Expect(ReplaceOnce(wrong_type, "\"value_type\": \"BOOL\"", "\"value_type\": \"UINT64\""),
              "wrong-type fixture mutation is unique")) {
    return false;
  }
  auto wrong_adapter = BusinessAdapter::Initialize(wrong_type, observer.Callbacks(), error);
  return Expect(wrong_adapter == nullptr && error == "required business field type does not match",
                "wrong business type is rejected before execution");
}

bool TestConfigurationA(const std::string& config) {
  Observer observer;
  std::string error;
  auto adapter = BusinessAdapter::Initialize(config, observer.Callbacks(), error);
  if (!Expect(adapter != nullptr, "configuration A initializes")) return false;

  const std::vector<std::uint8_t> measurement_frame{0xA1U, 0x02U, 0x08U, 0x01U, 0xACU};
  const auto received =
      adapter->OnReceivedRecord({measurement_frame.data(), measurement_frame.size()});
  if (!Expect(received.Succeeded() && observer.measurement_calls == 1U &&
                  observer.latest_measurement.temperature.coefficient == 12 &&
                  observer.latest_measurement.temperature.scale == 0 &&
                  observer.latest_measurement.alarm,
              "independent RX frame delivers one exact business measurement")) {
    return false;
  }
  const Measurement retained_measurement = observer.measurement_copies.front();

  const auto first = adapter->SendCommand(Command{{125, 1}});
  const std::vector<std::uint8_t> expected_first{0xB2U, 0x00U, 0x19U, 0x5AU, 0x25U};
  if (!Expect(first.Succeeded() && observer.bytes_calls == 1U &&
                  observer.latest_bytes == expected_first,
              "first dynamic TX matches independent bytes")) {
    return false;
  }
  const auto retained_bytes = observer.byte_copies.front();

  const auto second = adapter->SendCommand(Command{{-5, 0}});
  const std::vector<std::uint8_t> expected_second{0xB2U, 0xFFU, 0xF6U, 0x5AU, 0x01U};
  if (!Expect(second.Succeeded() && observer.bytes_calls == 2U &&
                  observer.latest_bytes == expected_second && retained_bytes == expected_first,
              "second dynamic TX is independent and prior callback copy remains stable")) {
    return false;
  }

  const std::size_t measurements_before_failures = observer.measurement_calls;
  const std::size_t bytes_before_failures = observer.bytes_calls;
  auto bad_sum = measurement_frame;
  bad_sum.back() ^= 0x01U;
  const auto integrity = adapter->OnReceivedRecord({bad_sum.data(), bad_sum.size()});
  const std::vector<std::uint8_t> unknown_frame{0xC1U, 0x02U, 0x08U, 0x01U, 0xCCU};
  const auto unknown = adapter->OnReceivedRecord({unknown_frame.data(), unknown_frame.size()});
  const auto direction = adapter->OnReceivedRecord({expected_first.data(), expected_first.size()});
  const auto invalid_decimal = adapter->SendCommand(Command{{1, 19}});
  const auto non_integral = adapter->SendCommand(Command{{126, 1}});
  if (!Expect(integrity.codec_status == CodecStatus::INTEGRITY_FAILED &&
                  unknown.codec_status == CodecStatus::UNKNOWN_MESSAGE &&
                  direction.codec_status == CodecStatus::UNKNOWN_MESSAGE &&
                  invalid_decimal.codec_status == CodecStatus::INVALID_ARGUMENT &&
                  invalid_decimal.conversion_error == ConversionError::DECIMAL_SCALE_OUT_OF_RANGE &&
                  non_integral.codec_status == CodecStatus::VALUE_NOT_REPRESENTABLE &&
                  non_integral.conversion_error == ConversionError::RAW_NOT_INTEGRAL &&
                  observer.measurement_calls == measurements_before_failures &&
                  observer.bytes_calls == bytes_before_failures,
              "all RX/TX failures are explicit and deliver no success callback")) {
    return false;
  }

  const auto recovered_rx =
      adapter->OnReceivedRecord({measurement_frame.data(), measurement_frame.size()});
  const auto recovered_tx = adapter->SendCommand(Command{{125, 1}});
  return Expect(recovered_rx.Succeeded() && recovered_tx.Succeeded() &&
                    observer.measurement_calls == measurements_before_failures + 1U &&
                    observer.bytes_calls == bytes_before_failures + 1U &&
                    retained_measurement.temperature.coefficient == 12 &&
                    retained_measurement.alarm && retained_bytes == expected_first,
                "success-failure-success recovers and retained copies remain valid");
}

bool TestConfigurationB(const std::string& config) {
  Observer observer;
  std::string error;
  auto adapter = BusinessAdapter::Initialize(config, observer.Callbacks(), error);
  if (!Expect(adapter != nullptr, "configuration B initializes with the same host code"))
    return false;

  const std::vector<std::uint8_t> measurement_frame{0xA1U, 0x80U, 0x01U, 0x04U, 0x33U, 0x59U};
  const auto received =
      adapter->OnReceivedRecord({measurement_frame.data(), measurement_frame.size()});
  if (!Expect(received.Succeeded() && observer.measurement_calls == 1U &&
                  observer.latest_measurement.temperature.coefficient == 12 &&
                  observer.latest_measurement.temperature.scale == 0 &&
                  observer.latest_measurement.alarm,
              "configuration B maps its independent RX layout to the same business meaning")) {
    return false;
  }

  const auto first = adapter->SendCommand(Command{{125, 1}});
  const std::vector<std::uint8_t> expected_first{0xB2U, 0x5AU, 0x32U, 0x00U, 0x3EU};
  const auto second = adapter->SendCommand(Command{{-5, 0}});
  const std::vector<std::uint8_t> expected_second{0xB2U, 0x5AU, 0xECU, 0xFFU, 0xF7U};
  return Expect(first.Succeeded() && second.Succeeded() && observer.bytes_calls == 2U &&
                    observer.byte_copies[0] == expected_first &&
                    observer.byte_copies[1] == expected_second,
                "configuration B adapts scale, offset, endian, and layout without host changes");
}

#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
bool TestLengthConfiguration(const std::string& config) {
  Observer observer;
  std::string error;
  auto adapter = BusinessAdapter::Initialize(config, observer.Callbacks(), error);
  if (!Expect(adapter != nullptr, "Schema 0.7 length configuration initializes")) return false;

  const std::vector<std::uint8_t> measurement_frame{0xA1U, 0x06U, 0x02U, 0x08U, 0x01U, 0xB2U};
  const auto received =
      adapter->OnReceivedRecord({measurement_frame.data(), measurement_frame.size()});
  if (!Expect(received.Succeeded() && observer.measurement_calls == 1U &&
                  observer.latest_measurement.temperature.coefficient == 12 &&
                  observer.latest_measurement.alarm,
              "length-checked RX delivers the same business measurement")) {
    return false;
  }

  auto bad_length = measurement_frame;
  bad_length[1] = 0x05U;
  const auto rejected = adapter->OnReceivedRecord({bad_length.data(), bad_length.size()});
  if (!Expect(
          rejected.codec_status == CodecStatus::LENGTH_MISMATCH && observer.measurement_calls == 1U,
          "length mismatch reaches no business callback")) {
    return false;
  }

  const auto first = adapter->SendCommand(Command{{125, 1}});
  const std::vector<std::uint8_t> expected_first{0xB2U, 0x06U, 0x00U, 0x19U, 0x5AU, 0x2BU};
  const auto second = adapter->SendCommand(Command{{-5, 0}});
  const std::vector<std::uint8_t> expected_second{0xB2U, 0x06U, 0xFFU, 0xF6U, 0x5AU, 0x07U};
  return Expect(first.Succeeded() && second.Succeeded() && observer.bytes_calls == 2U &&
                    observer.byte_copies[0] == expected_first &&
                    observer.byte_copies[1] == expected_second,
                "computed length precedes SUM8 without changing host inputs");
}
#endif

#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
bool TestBoundedRecordConfiguration(const std::string& config) {
  std::vector<std::vector<std::uint8_t>> payloads;
  std::vector<std::vector<std::uint8_t>> frames;
  pae::examples::business_embedding::BoundedRecordCallbacks callbacks{
      [&payloads](ByteView value) { payloads.emplace_back(value.data, value.data + value.size); },
      [&frames](ByteView value) { frames.emplace_back(value.data, value.data + value.size); }};
  std::string error;
  auto adapter = BoundedRecordAdapter::Initialize(config, std::move(callbacks), error);
  if (!Expect(adapter != nullptr && error.empty(), "Schema 0.8 bounded host initializes"))
    return false;

  const std::vector<std::uint8_t> middle_payload{0x10U, 0x20U};
  const auto empty = adapter->EncodePayload({nullptr, 0U});
  const auto middle = adapter->EncodePayload({middle_payload.data(), middle_payload.size()});
  const std::vector<std::uint8_t> expected_empty{0xA5U, 0x03U, 0xA8U};
  const std::vector<std::uint8_t> expected_middle{0xA5U, 0x05U, 0x10U, 0x20U, 0xDAU};
  if (!Expect(empty.Succeeded() && middle.Succeeded() && frames.size() == 2U &&
                  frames[0] == expected_empty && frames[1] == expected_middle,
              "bounded host emits independent empty and middle vectors")) {
    return false;
  }

  const auto decoded = adapter->OnReceivedRecord({expected_middle.data(), expected_middle.size()});
  auto bad_length = expected_middle;
  bad_length[1] = 0x04U;
  const auto rejected = adapter->OnReceivedRecord({bad_length.data(), bad_length.size()});
  const std::vector<std::uint8_t> too_long{1U, 2U, 3U, 4U};
  const auto encode_rejected = adapter->EncodePayload({too_long.data(), too_long.size()});
  return Expect(
      decoded.Succeeded() && payloads.size() == 1U && payloads.front() == middle_payload &&
          rejected.codec_status == CodecStatus::LENGTH_MISMATCH &&
          encode_rejected.codec_status == CodecStatus::BYTES_LENGTH_MISMATCH && frames.size() == 2U,
      "bounded host copies borrowed payload and suppresses failure callbacks");
}
#endif

}  // namespace

int main(int argc, char** argv) {
  const int expected_argc =
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      4
#else
      3
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      + 1
#endif
      ;
  if (argc != expected_argc) {
    std::cerr << "usage: business_embedding_tests <config-a> <config-b> <length-config>\n";
    return 2;
  }
  const std::string config_a = ReadFile(argv[1]);
  const std::string config_b = ReadFile(argv[2]);
  const std::string length_config =
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      ReadFile(argv[3]);
#else
      {};
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
  const std::string variable_config = ReadFile(argv[expected_argc - 1]);
#endif
  if (!Expect(!config_a.empty() && !config_b.empty()
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
                  && !length_config.empty()
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
                  && !variable_config.empty()
#endif
                  ,
              "public synthetic configs are readable") ||
      !TestInitialization(config_a) || !TestConfigurationA(config_a) ||
      !TestConfigurationB(config_b)
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      || !TestLengthConfiguration(length_config)
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      || !TestBoundedRecordConfiguration(variable_config)
#endif
  ) {
    return 1;
  }
  std::cout << "BUSINESS_EMBEDDING_TESTS=PASS\n";
  return 0;
}
