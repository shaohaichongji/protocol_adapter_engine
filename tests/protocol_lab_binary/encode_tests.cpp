#include <fstream>
#include <iostream>
#include <iterator>
#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include "prepared_binary.h"

namespace {
namespace b = pae::protocol_lab_binary;
namespace h = pae::host_endpoint;
namespace c = pae::protocol_core;
void Check(bool ok, const char* detail) {
  if (!ok) throw std::runtime_error(detail);
}
std::string Read(const std::string& path) {
  std::ifstream input(path);
  Check(input.good(), "fixture missing");
  return {std::istreambuf_iterator<char>(input), {}};
}
std::string Replace(std::string json, const std::string& from, const std::string& to) {
  const auto at = json.find(from);
  Check(at != std::string::npos, "replacement missing");
  json.replace(at, from.size(), to);
  return json;
}
auto Prepare(const std::string& json, std::string_view pipeline, b::Limits limits = {}) {
  auto result = pae::config_compiler::CompileJsonToPlanWithMetadata(json, 4U * 1024U * 1024U);
  if (!result.Succeeded() && result.Diagnostic()) std::cerr << result.Diagnostic()->detail << '\n';
  Check(result.Succeeded(), "compile");
  const h::BindingSpec bindings[] = {{"device", h::Action::DECODE, pipeline, 2U},
                                     {"device", h::Action::ENCODE, pipeline, 1U}};
  return b::PreparedBinary::Create(std::move(result).TakeArtifacts(), bindings, 2U, {7, 8, 9},
                                   limits);
}
void Typed(const std::string& json) {
  auto owner = Prepare(json, "ui_pipeline");
  std::vector<std::uint8_t> bytes{0xCA, 0xFE};
  std::vector<b::EncodeInput> values(8);
  values[0].field_id = "enabled";
  values[0].kind = c::LogicalValueKind::BOOL;
  values[0].bool_value = true;
  values[1].field_id = "mode";
  values[1].kind = c::LogicalValueKind::ENUM;
  values[1].enum_item_id = "active";
  values[2].field_id = "cross_bits";
  values[3].field_id = "msb_bits";
  values[4].field_id = "count";
  values[4].uint64_value = 1;
  values[5].field_id = "delta";
  values[5].kind = c::LogicalValueKind::INT64;
  values[6].field_id = "payload";
  values[6].kind = c::LogicalValueKind::BYTES;
  values[6].bytes_value = {bytes.data(), bytes.size()};
  values[7].field_id = "temperature";
  values[7].kind = c::LogicalValueKind::DECIMAL64;
  values[7].decimal64_value = {5, 0};
  const auto& first = owner->Encode(1, "typed_record", values.data(), values.size());
  Check(first.encoded && !first.candidate && first.host.codec_attempted &&
            first.host.successful_outputs == 1 && first.host.decode_successes == 0 &&
            first.host.observed_candidates == 0 && !first.host.framing_attempted,
        "Encode is not an RX observation");
  Check(first.encoded->frame ==
            std::vector<std::uint8_t>({0x80, 0x0D, 3, 0, 1, 0, 0xCA, 0xFE, 5, 0x5A}),
        "independent TX vector");
  Check(first.encoded->inputs[1].enum_value.item_id == "active" &&
            first.encoded->inputs[1].enum_value.display_text == "Active" &&
            first.encoded->inputs[1].enum_value.raw_value == 2 &&
            !first.encoded->inputs[7].raw_integer,
        "owned enum; conversion input not fabricated raw");
  Check(first.encoded->presentation.size() == 9 &&
            !first.encoded->presentation[0].physical_bits.empty() &&
            first.encoded->presentation[6].byte_range &&
            first.encoded->presentation[6].byte_range->offset == 6 &&
            first.encoded->presentation[8].byte_range &&
            first.encoded->presentation[8].byte_range->offset == 9,
        "input and constant mappings");
  Check(first.encoded->identity.binding == 1 && first.encoded->identity.flow == 0 &&
            first.encoded->identity.instance == owner->Instance() &&
            first.encoded->identity.revisions.tab == 7 &&
            first.encoded->work_peak_bytes >= first.encoded->accounted_total_bytes,
        "identity and budget");
  auto saved = *first.encoded;
  Check(owner->Decode(1, 0, {saved.frame.data(), saved.frame.size()}).host.status ==
                h::Status::INVALID_BINDING &&
            owner->Current(1, 0) == &first,
        "wrong-direction Decode preserves TX current");
  Check(owner->Submit(1, 0, {saved.frame.data(), saved.frame.size()}).host.status ==
                h::Status::INVALID_BINDING &&
            owner->Current(1, 0) == &first,
        "wrong-direction Submit preserves TX current");
  Check(owner->Continue(1, 0).host.status == h::Status::INVALID_BINDING &&
            owner->Current(1, 0) == &first,
        "wrong-direction Continue preserves TX current");
  const auto& rx = owner->Decode(0, 0, {saved.frame.data(), saved.frame.size()});
  Check(rx.candidate.has_value(), "RX current before invalid Encode");
  Check(owner->Encode(0, "typed_record", values.data(), values.size()).host.status ==
                h::Status::INVALID_BINDING &&
            owner->Current(0, 0) == &rx && owner->Current(1, 0) == &first,
        "wrong-direction Encode preserves RX and TX current");
  bytes[0] = 0;
  Check(owner->Current(1, 0)->encoded->inputs[6].bytes_value[0] == 0xCA, "owned input bytes");
  bytes[0] = 0xCA;
  auto missing = owner->Encode(1, "typed_record", values.data(), values.size() - 1);
  Check(!missing.host.codec_attempted && missing.encode_issue == b::EncodeIssue::MISSING_FIELD &&
            !missing.encoded,
        "missing input early reject clears TX");
  auto invalid = values;
  invalid[2].field_id = "marker";
  Check(owner->Encode(1, "typed_record", invalid.data(), invalid.size()).encode_issue ==
            b::EncodeIssue::INVALID_FIELD,
        "constant cannot be supplied");
  invalid = values;
  invalid[2].field_id = "count";
  Check(owner->Encode(1, "typed_record", invalid.data(), invalid.size()).encode_issue ==
            b::EncodeIssue::DUPLICATE_FIELD,
        "duplicate input");
  invalid = values;
  invalid[1].enum_item_id = "unknown";
  Check(owner->Encode(1, "typed_record", invalid.data(), invalid.size()).encode_issue ==
            b::EncodeIssue::UNKNOWN_ENUM,
        "enum ID not index");
  invalid = values;
  invalid[7].kind = c::LogicalValueKind::INT64;
  Check(owner->Encode(1, "typed_record", invalid.data(), invalid.size()).encode_issue ==
            b::EncodeIssue::TYPE_MISMATCH,
        "decimal logical type");
  invalid = values;
  invalid[6].bytes_value = {nullptr, 2};
  Check(owner->Encode(1, "typed_record", invalid.data(), invalid.size()).encode_issue ==
            b::EncodeIssue::INVALID_BYTES,
        "invalid bytes");
  Check(owner->Encode(1, "alternate_record", nullptr, 0).encode_issue ==
            b::EncodeIssue::MESSAGE_NOT_BOUND,
        "pipeline membership");
  Check(owner->Encode(0, "typed_record", values.data(), values.size()).host.status ==
            h::Status::INVALID_BINDING,
        "action isolation");
  invalid = values;
  invalid[4].uint64_value = 256;
  auto range = owner->Encode(1, "typed_record", invalid.data(), invalid.size());
  Check(range.host.codec_attempted && range.host.status == h::Status::CODEC_FAILED &&
            range.host.codec_status == c::CodecStatus::VALUE_NOT_REPRESENTABLE && !range.encoded &&
            !owner->Observe(1, 0).reset_required,
        "Core range failure is not callback fault");
  invalid = values;
  invalid[5].int64_value = -1;
  const auto& signed_result = owner->Encode(1, "typed_record", invalid.data(), invalid.size());
  Check(signed_result.encoded && signed_result.encoded->frame[5] == 0xFF, "signed TX");
  owner->TestFailEncodeCopy(true);
  auto copy = owner->Encode(1, "typed_record", values.data(), values.size());
  Check(copy.host.codec_attempted && copy.host.codec_status == c::CodecStatus::OK &&
            copy.host.status == h::Status::CALLBACK_FAILED && !copy.encoded &&
            copy.host.successful_outputs == 0 && owner->Observe(1, 0).reset_required &&
            !owner->Observe(0, 1).reset_required,
        "post Codec copy failure does not publish");
  Check(owner->Encode(1, "typed_record", values.data(), values.size()).host.status ==
            h::Status::RESET_REQUIRED,
        "reset required");
  owner->TestFailEncodeCopy(false);
  Check(owner->Reset(1, 0) == h::Status::INVALID_BINDING && owner->Observe(1, 0).reset_required,
        "Host Reset is Decode-only; Encode fault requires new Session");
  owner = Prepare(json, "ui_pipeline");
  const auto& reset_result = owner->Encode(1, "typed_record", values.data(), values.size());
  Check(reset_result.encoded && reset_result.encoded->identity.generation == 0 &&
            reset_result.encoded->identity.instance != saved.identity.instance,
        "Encode recovery uses fresh Session identity");
  b::Limits tiny;
  tiny.max_total_bytes = saved.work_peak_bytes;
  auto exact = Prepare(json, "ui_pipeline", tiny);
  Check(exact->Encode(1, "typed_record", values.data(), values.size()).encoded.has_value(),
        "Encode exact copy budget passes");
  --tiny.max_total_bytes;
  auto below = Prepare(json, "ui_pipeline", tiny);
  const auto& below_result = below->Encode(1, "typed_record", values.data(), values.size());
  Check(below_result.encode_issue == b::EncodeIssue::BUDGET_EXCEEDED &&
            !below_result.host.codec_attempted && !below->Observe(1, 0).reset_required,
        "Encode one below preflight rejects before Core");
  tiny.max_total_bytes = 1;
  auto constrained = Prepare(json, "ui_pipeline", tiny);
  auto rejected = constrained->Encode(1, "typed_record", values.data(), values.size());
  Check(!rejected.host.codec_attempted &&
            rejected.encode_issue == b::EncodeIssue::BUDGET_EXCEEDED &&
            !constrained->Observe(1, 0).reset_required,
        "preflight before Core");
  owner.reset();
  Check(saved.frame.size() == 10 && saved.inputs[6].bytes_value == bytes &&
            saved.inputs[1].enum_value.display_text == "Active",
        "owned result lifetime");
}
void Bounded(const std::string& json) {
  auto owner = Prepare(json, "synthetic_rx");
  b::EncodeInput input;
  input.field_id = "payload";
  input.kind = c::LogicalValueKind::BYTES;
  auto empty = owner->Encode(1, "bounded_record", &input, 1);
  Check(empty.encoded && empty.encoded->frame == std::vector<std::uint8_t>({0xA5, 3, 0xA8}) &&
            empty.encoded->presentation[1].byte_range && empty.encoded->integrity_storage &&
            empty.encoded->presentation[1].byte_range->length == 0 &&
            empty.encoded->integrity_storage->offset == 2,
        "zero payload length and SUM8");
  const std::uint8_t payload[] = {1, 2, 3, 4};
  input.bytes_value = {payload, 3};
  auto maximum = owner->Encode(1, "bounded_record", &input, 1);
  Check(maximum.encoded &&
            maximum.encoded->frame == std::vector<std::uint8_t>({0xA5, 6, 1, 2, 3, 0xB1}) &&
            maximum.encoded->integrity_storage && maximum.encoded->presentation[0].byte_range &&
            maximum.encoded->integrity_storage->offset == 5 &&
            maximum.encoded->presentation[0].byte_range->offset == 1,
        "maximum payload computed length and SUM8");
  input.bytes_value.size = 4;
  Check(owner->Encode(1, "bounded_record", &input, 1).host.status == h::Status::CODEC_FAILED,
        "Core rejects payload over bound");
}
void Constant(std::string json) {
  const auto start = json.find("\"id\": \"alternate_value\"");
  Check(start != std::string::npos, "constant field");
  const auto pos = json.find("\"source\": \"input\"", start);
  Check(pos != std::string::npos, "constant source");
  json.replace(pos, std::string("\"source\": \"input\"").size(),
               "\"source\": \"constant\", \"value\": 7");
  auto owner = Prepare(json, "alternate_pipeline");
  const auto& result = owner->Encode(1, "alternate_record", nullptr, 0);
  Check(result.encoded && result.encoded->inputs.empty() &&
            result.encoded->frame == std::vector<std::uint8_t>({0xC3, 7}),
        "zero-input Encode");
}
}  // namespace
int main(int argc, char** argv) {
#if defined(_MSC_VER) && defined(_DEBUG)
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  try {
    Check(argc == 2, "root argument");
    const std::string root = argv[1];
    const auto typed =
        Replace(Read(root + "/tests/protocol_lab_ui/fixtures/synthetic_ui_v05.pae.json"), "\"0.5\"",
                "\"0.9\"");
    Typed(typed);
    Constant(typed);
    Bounded(Replace(Read(root + "/tests/protocol_lab_ui/fixtures/synthetic_ui_v08.pae.json"),
                    "\"0.8\"", "\"0.9\""));
    std::cout << "Binary Encode checks passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
