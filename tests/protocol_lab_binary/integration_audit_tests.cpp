#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "prepared_binary.h"

namespace {
namespace b = pae::protocol_lab_binary;
namespace h = pae::host_endpoint;
namespace c = pae::protocol_core;
void Check(bool value, const char* detail) {
  if (!value) throw std::runtime_error(detail);
}
std::string Read(const std::string& path) {
  std::ifstream input(path);
  Check(input.good(), "fixture missing");
  return {std::istreambuf_iterator<char>(input), {}};
}
std::string Replace(std::string json, const std::string& from, const std::string& to) {
  const auto position = json.find(from);
  Check(position != std::string::npos, "fixture mutation anchor missing");
  json.replace(position, from.size(), to);
  return json;
}
auto Prepare(const std::string& json, std::string_view pipeline) {
  auto compiled =
      pae::config_compiler::CompileJsonToPlanWithUiDescription(json, 4U * 1024U * 1024U);
  if (!compiled.Succeeded() && compiled.Diagnostic())
    std::cerr << compiled.Diagnostic()->detail << '\n';
  Check(compiled.Succeeded(), "fixture compile failed");
  const h::BindingSpec specs[] = {{"device", h::Action::DECODE, pipeline, 2U},
                                  {"device", h::Action::ENCODE, pipeline, 1U}};
  return b::PreparedBinary::Create(std::move(compiled).TakeArtifacts(), specs, 2U);
}
void Range(const std::optional<b::ByteRange>& range, std::size_t offset, std::size_t length) {
  Check(range && range->offset == offset && range->length == length, "actual range mismatch");
}
void CrcCase(const std::string& json, std::string_view message,
             const std::vector<std::uint8_t>& payload, const std::vector<std::uint8_t>& expected,
             std::size_t payload_field, std::size_t payload_offset, std::size_t crc_offset) {
  auto owner = Prepare(json, "synthetic_rx");
  b::EncodeInput input;
  input.field_id = "payload";
  input.kind = c::LogicalValueKind::BYTES;
  input.bytes_value = {payload.data(), payload.size()};
  const auto& encoded = owner->Encode(1, message, &input, 1);
  Check(encoded.host.status == h::Status::OK && encoded.encoded && !encoded.candidate &&
            encoded.host.codec_attempted && encoded.host.successful_outputs == 1U &&
            encoded.host.observed_candidates == 0U,
        "CRC TX must be one owned Encode result, not RX");
  Check(encoded.encoded->frame == expected && encoded.encoded->presentation.size() > payload_field,
        "CRC TX matches independent vector");
  Range(encoded.encoded->presentation[payload_field].byte_range, payload_offset, payload.size());
  Range(encoded.encoded->integrity_storage, crc_offset, 2U);
  const auto& decoded = owner->Decode(0, 0, {expected.data(), expected.size()});
  Check(decoded.host.status == h::Status::OK && decoded.candidate &&
            decoded.candidate->value.fields.size() > payload_field &&
            decoded.candidate->presentation.size() > payload_field,
        "independent CRC frame Decode");
  Check(decoded.candidate->value.fields[payload_field].bytes_value == payload,
        "CRC RX owned payload");
  Range(decoded.candidate->presentation[payload_field].byte_range, payload_offset, payload.size());
  Range(decoded.candidate->integrity_storage, crc_offset, 2U);
  auto bad = expected;
  bad.at(crc_offset) ^= 1U;
  const auto& failed = owner->Decode(0, 0, {bad.data(), bad.size()});
  Check(failed.host.status == h::Status::CODEC_FAILED &&
            failed.host.codec_status == c::CodecStatus::INTEGRITY_FAILED && failed.candidate &&
            failed.host.successful_outputs == 0U,
        "CRC corruption returns diagnostic candidate only");
  Check(failed.candidate->value.fields.empty() && failed.candidate->value.frame.empty() &&
            failed.candidate->value.diagnostic_frame == bad &&
            failed.candidate->presentation.empty() && !failed.candidate->integrity_storage,
        "CRC failure must clear old successful mapping");
  Check(owner->Decode(0, 0, {expected.data(), expected.size()}).host.status == h::Status::OK,
        "valid CRC frame recovers without Reset");
}
void Crc(const std::string& root) {
  auto fixed =
      Replace(Read(root + "/examples/config/synthetic_crc_slice.pae.json"), "\"0.6\"", "\"0.9\"");
  // Independent published check vector already used in protocol_core/crc_contract_tests.cpp.
  const std::vector<std::uint8_t> digits{'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  CrcCase(fixed, "crc_record", digits,
          {'1', '2', '3', '4', '5', '6', '7', '8', '9', 0x29, 0xB1, 0xAA}, 0, 0, 9);
  fixed = Replace(fixed, "\"big_endian\"", "\"little_endian\"");
  CrcCase(fixed, "crc_record", digits,
          {'1', '2', '3', '4', '5', '6', '7', '8', '9', 0xB1, 0x29, 0xAA}, 0, 0, 9);
  auto bounded = Replace(Read(root + "/tests/protocol_lab_ui/fixtures/synthetic_ui_v08.pae.json"),
                         "\"0.8\"", "\"0.9\"");
  bounded = Replace(bounded, "\"algorithm\": \"sum8\"",
                    "\"algorithm\": \"crc\", \"parameters\": {\"width\": 16, "
                    "\"poly\": \"1021\", \"init\": \"FFFF\", \"refin\": false, "
                    "\"refout\": false, \"xorout\": \"0000\"}");
  bounded = Replace(bounded, "\"storage\": {\"anchor\": \"payload_end\"}",
                    "\"storage\": {\"anchor\": \"payload_end\", \"byte_order\": \"big_endian\"}");
  // Independent dynamic vector from bounded_variable_record_contract_tests.cpp:427.
  CrcCase(bounded, "bounded_record", {0x10, 0x20}, {0xA5, 6, 0x10, 0x20, 0x47, 0x42}, 1, 2, 4);
  bounded = Replace(bounded, "\"range\": {\"byte_offset\": 0, \"end\": \"payload_end\"}",
                    "\"range\": {\"byte_offset\": 2, \"end\": \"payload_end\"}");
  // Empty covered range preserves init=FFFF, independent of the header.
  CrcCase(bounded, "bounded_record", {}, {0xA5, 4, 0xFF, 0xFF}, 1, 2, 2);
}
void SameState(const b::StreamState& before, const b::StreamState& after) {
  const auto& x = before.host;
  const auto& y = after.host;
  Check(before.capacity == after.capacity && before.frozen_size == after.frozen_size &&
            before.consumed == after.consumed && before.can_continue == after.can_continue &&
            x.status == y.status && x.generation == y.generation &&
            x.reset_required == y.reset_required && x.stream == y.stream &&
            x.framing.phase == y.framing.phase &&
            x.framing.buffered_bytes == y.framing.buffered_bytes &&
            x.framing.has_internal_work == y.framing.has_internal_work &&
            x.framing.effective_max_submit_bytes == y.framing.effective_max_submit_bytes &&
            x.framing.effective_max_work_units == y.framing.effective_max_work_units,
        "Encode changed RX stream observation/frozen state");
}
void Isolation(const std::string& root) {
  auto owner =
      Prepare(Read(root + "/examples/config/synthetic_stream_framing_slice.pae.json"), "fixed_rx");
  owner->SaveAndSelect(u"partial draft", 0, 1);
  owner->SaveAndSelect(u"frozen draft", 0, 0);
  const std::uint8_t partial[] = {0xAA};
  const std::uint8_t multiple[] = {0xAA, 1, 2, 0xAA, 3, 4};
  owner->Submit(0, 0, {partial, sizeof(partial)});
  owner->Submit(0, 1, {multiple, sizeof(multiple)});
  const auto half = owner->Stream(0, 0), frozen = owner->Stream(0, 1);
  const auto* half_current = owner->Current(0, 0);
  const auto* frozen_current = owner->Current(0, 1);
  Check(half.host.framing.buffered_bytes == 1U && frozen.can_continue && frozen.consumed == 3U &&
            frozen_current && frozen_current->candidate,
        "isolation preconditions");
  const auto frozen_operation = frozen_current->candidate->identity.operation;
  const auto unchanged = [&] {
    SameState(half, owner->Stream(0, 0));
    SameState(frozen, owner->Stream(0, 1));
    Check(owner->Current(0, 0) == half_current && owner->Current(0, 1) == frozen_current &&
              frozen_current->candidate &&
              frozen_current->candidate->identity.operation == frozen_operation &&
              owner->Draft(0, 0) == u"partial draft" && owner->Draft(0, 1) == u"frozen draft" &&
              owner->Selected().binding == 0U && owner->Selected().flow == 0U,
          "Encode changed RX saved results/drafts/selection");
  };
  b::EncodeInput value;
  value.field_id = "fixed_payload";
  value.uint64_value = 0x1234;
  const auto& success = owner->Encode(1, "fixed_message", &value, 1);
  Check(success.encoded && success.encoded->frame == std::vector<std::uint8_t>({0xAA, 0x12, 0x34}),
        "stream-bound Encode success");
  unchanged();
  const auto& early = owner->Encode(1, "fixed_message", nullptr, 0);
  Check(!early.host.codec_attempted && early.encode_issue == b::EncodeIssue::MISSING_FIELD,
        "Encode early refusal");
  unchanged();
  owner->TestFailEncodeCopy(true);
  const auto& failed = owner->Encode(1, "fixed_message", &value, 1);
  Check(failed.host.status == h::Status::CALLBACK_FAILED && failed.host.codec_attempted &&
            failed.host.codec_status == c::CodecStatus::OK && !failed.encoded &&
            failed.host.successful_outputs == 0U && owner->Observe(1, 0).reset_required,
        "Encode copy failure records actual Core success");
  unchanged();
  owner->TestFailEncodeCopy(false);
  const std::uint8_t tail[] = {0x12, 0x34};
  const auto& resumed_half = owner->Submit(0, 0, {tail, sizeof(tail)});
  Check(resumed_half.candidate && resumed_half.candidate->value.fields.size() == 1U &&
            resumed_half.candidate->value.fields[0].uint64_value == 0x1234U,
        "RX half packet survives Encode fault");
  const auto& resumed_frozen = owner->Continue(0, 1);
  Check(resumed_frozen.candidate && resumed_frozen.candidate->value.fields.size() == 1U &&
            resumed_frozen.candidate->value.fields[0].uint64_value == 0x0304U,
        "RX frozen bytes survive Encode fault");
}
void Decimal(const std::string& root) {
  auto json = Read(root + "/examples/config/synthetic_stream_framing_slice.pae.json");
  // Add logical = raw / 2 + 1 to the first input field, leaving the fixture on disk unchanged.
  json =
      Replace(json, "\"encode\": {\"source\": \"input\"}",
              "\"encode\": {\"source\": \"input\"}, \"conversion\": {\"kind\": \"linear\", "
              "\"output_type\": \"DECIMAL64\", \"scale\": {\"numerator\": 1, \"denominator\": 2}, "
              "\"bias\": {\"numerator\": 1, \"denominator\": 1}}");
  auto owner = Prepare(json, "fixed_rx");
  b::EncodeInput value;
  value.field_id = "fixed_payload";
  value.kind = c::LogicalValueKind::DECIMAL64;
  value.decimal64_value = {3, 0};
  const auto& good = owner->Encode(1, "fixed_message", &value, 1);
  Check(good.encoded && good.encoded->inputs.size() == 1U &&
            good.encoded->frame == std::vector<std::uint8_t>({0xAA, 0, 4}) &&
            !good.encoded->inputs[0].raw_integer &&
            good.encoded->inputs[0].decimal64_value.coefficient == 3,
        "non-unit Decimal inverse preserves logical input without fabricated raw");
  value.decimal64_value = {325, 2};
  const auto& bad = owner->Encode(1, "fixed_message", &value, 1);
  Check(bad.host.status == h::Status::CODEC_FAILED && bad.host.codec_attempted &&
            bad.host.codec_status == c::CodecStatus::VALUE_NOT_REPRESENTABLE && !bad.encoded &&
            bad.host.successful_outputs == 0U && !owner->Observe(1, 0).reset_required,
        "inexact inverse rejects without callback fault or stale TX");
}
}  // namespace
int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  try {
    Check(argc == 2, "expected repository root");
    Crc(argv[1]);
    Isolation(argv[1]);
    Decimal(argv[1]);
    std::cout << "Binary integration audit checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
