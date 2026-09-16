#include <pae/compiler.h>
#include <pae/host_endpoint.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace {

std::atomic<std::size_t> g_allocations{0U};

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
    std::cout << "PUBLIC_ASCII_FACTS_SUMMARY passed=" << passed_ << " failed=" << failed_ << '\n';
    return failed_ == 0U ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

std::string Read(const char* path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

pae::CompiledProtocol Compile(std::string_view json, Runner& runner, std::string_view label) {
  auto result = pae::CompileProtocolJson(json);
  runner.Check(result.Succeeded(), label);
  return result.Succeeded() ? std::move(result).TakeCompiled() : pae::CompiledProtocol{};
}

pae::ByteView View(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()), value.size()};
}

bool LiteralEquals(const pae::AsciiSegmentDescription& segment, std::string_view text) {
  if (segment.kind != pae::AsciiSegmentKind::LITERAL || !segment.literal.has_value() ||
      segment.field_index.has_value() || segment.literal->size != text.size()) {
    return false;
  }
  for (std::size_t index = 0U; index < text.size(); ++index) {
    if (segment.literal->data[index] != static_cast<std::uint8_t>(text[index])) return false;
  }
  return true;
}

void CheckDescription(Runner& runner, pae::CompiledProtocol& compiled) {
  const auto rx = compiled.AsciiAction(0U, pae::AsciiAction::DECODE);
  const auto tx = compiled.AsciiAction(0U, pae::AsciiAction::ENCODE);
  runner.Check(rx.status == pae::AsciiQueryStatus::OK && rx.value.has_value() &&
                   rx.value->message_index == 0U && rx.value->segment_count == 5U &&
                   rx.value->record_length.minimum <= 12U &&
                   rx.value->record_length.maximum >= 12U &&
                   tx.status == pae::AsciiQueryStatus::OK && tx.value.has_value() &&
                   tx.value->segment_count == 5U,
               "independent_rx_tx_action_bounds");
  const auto rx_literal = compiled.AsciiSegment(0U, pae::AsciiAction::DECODE, 0U);
  const auto tx_literal = compiled.AsciiSegment(0U, pae::AsciiAction::ENCODE, 0U);
  runner.Check(rx_literal.status == pae::AsciiQueryStatus::OK && rx_literal.value.has_value() &&
                   LiteralEquals(*rx_literal.value, "RX ") && tx_literal.value.has_value() &&
                   LiteralEquals(*tx_literal.value, "TX "),
               "ordered_independent_literal_bytes");
  const auto rx_field = compiled.AsciiSegment(0U, pae::AsciiAction::DECODE, 3U);
  const auto tx_field = compiled.AsciiSegment(0U, pae::AsciiAction::ENCODE, 3U);
  runner.Check(rx_field.value.has_value() && tx_field.value.has_value() &&
                   rx_field.value->kind == pae::AsciiSegmentKind::FIELD &&
                   rx_field.value->field_index == 1U && !rx_field.value->literal.has_value() &&
                   tx_field.value->field_index == 2U && !tx_field.value->literal.has_value(),
               "field_payload_identity_and_mutual_exclusion");
  const auto name = compiled.AsciiField(0U);
  const auto rx_only = compiled.AsciiField(1U);
  const auto tx_only = compiled.AsciiField(2U);
  runner.Check(name.value.has_value() && name.value->byte_length.minimum == 1U &&
                   name.value->byte_length.maximum == 8U && name.value->decode_referenced &&
                   name.value->encode_referenced && rx_only.value.has_value() &&
                   rx_only.value->byte_length.minimum == 2U &&
                   rx_only.value->byte_length.maximum == 2U && rx_only.value->decode_referenced &&
                   !rx_only.value->encode_referenced && tx_only.value.has_value() &&
                   !tx_only.value->decode_referenced && tx_only.value->encode_referenced,
               "field_bounds_and_action_references");
  const auto bad_action = compiled.AsciiAction(0U, static_cast<pae::AsciiAction>(99));
  const auto bad_segment = compiled.AsciiSegment(0U, pae::AsciiAction::DECODE, 5U);
  const auto bad_field = compiled.AsciiField(3U);
  runner.Check(bad_action.status == pae::AsciiQueryStatus::INVALID_SELECTOR &&
                   !bad_action.value.has_value() &&
                   bad_segment.status == pae::AsciiQueryStatus::INDEX_OUT_OF_RANGE &&
                   !bad_segment.value.has_value() &&
                   bad_field.status == pae::AsciiQueryStatus::INDEX_OUT_OF_RANGE &&
                   !bad_field.value.has_value(),
               "invalid_query_fails_without_value");
  const std::size_t before = g_allocations.load(std::memory_order_relaxed);
  bool repeated_ok = true;
  for (std::size_t repeat = 0U; repeat < 128U; ++repeat) {
    repeated_ok &=
        compiled.AsciiAction(0U, pae::AsciiAction::DECODE).status == pae::AsciiQueryStatus::OK &&
        compiled.AsciiSegment(0U, pae::AsciiAction::ENCODE, 1U).status ==
            pae::AsciiQueryStatus::OK &&
        compiled.AsciiField(0U).status == pae::AsciiQueryStatus::OK;
  }
  const std::size_t after = g_allocations.load(std::memory_order_relaxed);
  runner.Check(repeated_ok && before == after, "repeated_frozen_queries_no_allocation");
}

std::string OneFieldConfig(std::string_view action, std::string_view wire) {
  return std::string{R"JSON({
"schema_version":"0.10","protocol_id":"ascii_facts","protocol_version":"1",
"display_name":"ASCII facts","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","input_kind":"complete_record"}],
"pipelines":[{"id":"pipe","display_name":"Pipe","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii",)JSON"} +
         std::string{action} + R"JSON(},
"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","value_type":"BYTES","wire":)JSON" +
         std::string{wire} + R"JSON(}]}]})JSON";
}

void CheckZeroAndTail(Runner& runner) {
  const std::string action =
      R"JSON("decode":{"segments":[{"kind":"literal","text":"PRE"},{"kind":"field","field_id":"value"}]})JSON";
  const std::string wire =
      R"JSON({"codec":"ascii_text","min_byte_length":0,"max_byte_length":3})JSON";
  auto compiled = Compile(OneFieldConfig(action, wire), runner, "tail_config_compiles");
  if (!compiled.HasValue()) return;
  const auto description = compiled.AsciiAction(0U, pae::AsciiAction::DECODE);
  const auto field = compiled.AsciiField(0U);
  runner.Check(description.value.has_value() && description.value->record_length.minimum == 3U &&
                   description.value->record_length.maximum == 6U && field.value.has_value() &&
                   field.value->byte_length.minimum == 0U &&
                   field.value->byte_length.maximum == 3U && field.value->decode_referenced &&
                   !field.value->encode_referenced &&
                   compiled.AsciiAction(0U, pae::AsciiAction::ENCODE).status ==
                       pae::AsciiQueryStatus::ACTION_NOT_AVAILABLE,
               "tail_zero_action_and_field_description");
  auto created = pae::CreateCompleteRecordCodec(compiled);
  const std::string empty = "PRE";
  const auto decoded = created.codec->Decode(0U, View(empty));
  const auto value = decoded.record.Field(0U);
  runner.Check(decoded.status == pae::CodecStatus::OK && decoded.matched_message_index == 0U &&
                   value.has_value() && value->Bytes().has_value() &&
                   value->Bytes()->data == View(empty).data + empty.size() &&
                   value->Bytes()->size == 0U,
               "zero_length_tail_slice_keeps_one_past_end_position");
  const std::string full = "PREABC";
  const auto second = created.codec->Decode(0U, View(full));
  const auto new_value = second.record.Field(0U);
  runner.Check(!decoded.record.HasValue() && second.status == pae::CodecStatus::OK &&
                   new_value.has_value() && new_value->Bytes().has_value() &&
                   new_value->Bytes()->data == View(full).data + 3U &&
                   new_value->Bytes()->size == 3U,
               "tail_full_length_and_old_view_invalidation");
  const std::string middle_action =
      R"JSON("decode":{"segments":[{"kind":"literal","text":"PRE"},{"kind":"field","field_id":"value"},{"kind":"literal","text":"!Z"}]})JSON";
  auto middle = Compile(OneFieldConfig(middle_action, wire), runner, "middle_config_compiles");
  if (!middle.HasValue()) return;
  auto middle_codec = pae::CreateCompleteRecordCodec(middle);
  const std::string middle_frame = "PRE!Z";
  const auto middle_result = middle_codec.codec->Decode(0U, View(middle_frame));
  const auto middle_value = middle_result.record.Field(0U);
  runner.Check(middle_result.status == pae::CodecStatus::OK && middle_value.has_value() &&
                   middle_value->Bytes().has_value() &&
                   middle_value->Bytes()->data == View(middle_frame).data + 3U &&
                   middle_value->Bytes()->size == 0U,
               "zero_length_middle_slice_keeps_real_position");
}

void CheckNulAndOneWay(Runner& runner) {
  const std::string action =
      R"JSON("decode":{"segments":[{"kind":"literal","text":"A\u0000B"},{"kind":"field","field_id":"value"}]})JSON";
  const std::string wire =
      R"JSON({"codec":"ascii_text","min_byte_length":1,"max_byte_length":1})JSON";
  auto compiled = Compile(OneFieldConfig(action, wire), runner, "nul_literal_config_compiles");
  if (!compiled.HasValue()) return;
  const auto segment = compiled.AsciiSegment(0U, pae::AsciiAction::DECODE, 0U);
  const std::string expected_literal{"A\0B", 3U};
  runner.Check(segment.value.has_value() && LiteralEquals(*segment.value, expected_literal),
               "literal_nul_preserved_with_explicit_length");
  const std::string frame{"A\0BZ", 4U};
  auto created = pae::CreateCompleteRecordCodec(compiled);
  const auto decoded = created.codec->Decode(0U, View(frame));
  runner.Check(decoded.status == pae::CodecStatus::OK && decoded.record.HasValue() &&
                   decoded.matched_message_index == 0U,
               "embedded_nul_literal_executes");
  auto moved = std::move(compiled);
  runner.Check(
      compiled.AsciiAction(0U, pae::AsciiAction::DECODE).status ==
              pae::AsciiQueryStatus::INVALID_COMPILED_PROTOCOL &&
          moved.AsciiAction(0U, pae::AsciiAction::DECODE).status == pae::AsciiQueryStatus::OK,
      "moved_compiled_owner_query_invalidated");
}

void CheckOtherOwners(Runner& runner, const char* stream_path, const char* binary_path) {
  auto stream = Compile(Read(stream_path), runner, "ascii_stream_config_compiles");
  auto binary = Compile(Read(binary_path), runner, "binary_config_compiles");
  if (!stream.HasValue() || !binary.HasValue()) return;
  runner.Check(
      stream.AsciiAction(1U, pae::AsciiAction::DECODE).status == pae::AsciiQueryStatus::OK &&
          stream.AsciiAction(1U, pae::AsciiAction::ENCODE).status ==
              pae::AsciiQueryStatus::ACTION_NOT_AVAILABLE &&
          stream.AsciiAction(2U, pae::AsciiAction::DECODE).status ==
              pae::AsciiQueryStatus::ACTION_NOT_AVAILABLE &&
          stream.AsciiAction(2U, pae::AsciiAction::ENCODE).status == pae::AsciiQueryStatus::OK,
      "one_way_literal_only_actions_are_distinct");
  const auto literal_only = stream.AsciiAction(1U, pae::AsciiAction::DECODE);
  runner.Check(literal_only.value.has_value() && literal_only.value->segment_count == 1U &&
                   stream.Message(1U)->field_count == 0U &&
                   stream.AsciiSegment(1U, pae::AsciiAction::DECODE, 0U).value.has_value(),
               "literal_only_action_not_empty_template");
  runner.Check(
      binary.AsciiAction(0U, pae::AsciiAction::DECODE).status ==
              pae::AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED &&
          binary.AsciiSegment(0U, pae::AsciiAction::DECODE, 0U).status ==
              pae::AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED &&
          binary.AsciiField(0U).status == pae::AsciiQueryStatus::REPRESENTATION_NOT_SUPPORTED,
      "binary_query_rejected_without_changing_binary_physical");
  auto codec = pae::CreateCompleteRecordCodec(binary);
  const std::array<std::uint8_t, 13U> unknown{};
  const auto decoded = codec.codec->Decode(0U, {unknown.data(), unknown.size()});
  runner.Check(!decoded.matched_message_index.has_value() && !decoded.record.HasValue(),
               "binary_unknown_identity_not_fabricated");
}

void CheckAmbiguity(Runner& runner) {
  const std::string config = R"JSON({
"schema_version":"0.10","protocol_id":"ascii_ambiguous","protocol_version":"1",
"display_name":"ASCII ambiguous","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","input_kind":"complete_record"}],
"pipelines":[{"id":"pipe","display_name":"Pipe","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","direction_id":"test","input_framing_profile_id":"record","message_ids":["first","second"]}],
"messages":[
{"id":"first","display_name":"First","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"X"},{"kind":"field","field_id":"value"}]}},"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","value_type":"BYTES","wire":{"codec":"ascii_text","min_byte_length":1,"max_byte_length":1}}]},
{"id":"second","display_name":"Second","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"X"},{"kind":"field","field_id":"value"}]}},"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_FACTS","value_type":"BYTES","wire":{"codec":"ascii_text","min_byte_length":1,"max_byte_length":1}}]}
]})JSON";
  auto compiled = Compile(config, runner, "ambiguous_ascii_config_compiles");
  if (!compiled.HasValue()) return;
  auto created = pae::CreateCompleteRecordCodec(compiled);
  const auto ambiguous = created.codec->Decode(0U, View("XA"));
  const auto unknown = created.codec->Decode(0U, View("YA"));
  runner.Check(ambiguous.status == pae::CodecStatus::AMBIGUOUS_MESSAGE &&
                   !ambiguous.matched_message_index.has_value() && !ambiguous.record.HasValue() &&
                   unknown.status == pae::CodecStatus::UNKNOWN_MESSAGE &&
                   !unknown.matched_message_index.has_value(),
               "real_reachable_ambiguous_and_unknown_no_identity");
}

void CheckSlices(Runner& runner, pae::CompiledProtocol& compiled) {
  auto created = pae::CreateCompleteRecordCodec(compiled);
  runner.Check(created.status == pae::CodecStatus::OK && created.codec != nullptr,
               "ascii_codec_create");
  if (!created.codec) return;
  const std::string reply = "RX A!OK\r\n";
  const auto success = created.codec->Decode(0U, View(reply));
  const auto first = success.record.Field(0U);
  runner.Check(success.status == pae::CodecStatus::OK && success.matched_message_index == 0U &&
                   success.record.HasValue() && success.record.MessageIndex() == 0U &&
                   first.has_value() && first->Bytes().has_value() &&
                   first->Bytes()->data == View(reply).data + 3U && first->Bytes()->size == 1U,
               "success_identity_and_rx_slice");
  std::string bad = "RX ";
  bad.push_back(static_cast<char>(0x80));
  bad += "!OK\r\n";
  const auto failure = created.codec->Decode(0U, View(bad));
  runner.Check(failure.status == pae::CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
                   failure.matched_message_index == 0U && !failure.record.HasValue() &&
                   !success.record.HasValue() && first.has_value() && !first->HasValue(),
               "unique_failure_keeps_identity_but_clears_old_views");
  const auto unknown = created.codec->Decode(0U, View("NO\r\n"));
  runner.Check(unknown.status == pae::CodecStatus::UNKNOWN_MESSAGE &&
                   !unknown.matched_message_index.has_value() && !unknown.record.HasValue(),
               "unknown_has_no_identity");
  auto small = pae::CreateCompleteRecordCodec(compiled, {1U});
  const auto capacity = small.codec->Decode(0U, View(reply));
  runner.Check(capacity.status == pae::CodecStatus::OUTPUT_SLOTS_TOO_SMALL &&
                   capacity.matched_message_index == 0U && !capacity.record.HasValue(),
               "post_match_capacity_failure_keeps_identity");
}

struct Capture {
  std::optional<std::size_t> observer_match;
  pae::CodecStatus observer_status = pae::CodecStatus::INVALID_ARGUMENT;
  std::size_t observer_calls = 0U;
  std::size_t business_calls = 0U;
  std::optional<std::size_t> business_message;
  bool observer_record_valid = false;
};

pae::HostCallbackAction Observe(const pae::HostCandidateView& candidate, void* opaque) {
  auto& capture = *static_cast<Capture*>(opaque);
  capture.observer_match = candidate.matched_message_index;
  capture.observer_status = candidate.decode_status;
  capture.observer_record_valid = candidate.record.HasValue();
  ++capture.observer_calls;
  return pae::HostCallbackAction::CONTINUE;
}

pae::HostCallbackAction Business(const pae::HostOutputView& output, void* opaque) {
  auto& capture = *static_cast<Capture*>(opaque);
  capture.business_message = output.message_index;
  ++capture.business_calls;
  return pae::HostCallbackAction::CONTINUE;
}

void CheckHost(Runner& runner, pae::CompiledProtocol& compiled) {
  const pae::HostBindingSpec binding{"ascii", pae::HostAction::DECODE, 0U};
  auto created = pae::CreateHostEndpoint(compiled, &binding, 1U);
  runner.Check(created.status == pae::HostStatus::OK && created.host != nullptr,
               "ascii_host_create");
  if (!created.host) return;
  const auto found = created.host->Find("ascii", pae::HostAction::DECODE);
  Capture capture;
  std::string bad = "RX ";
  bad.push_back(static_cast<char>(0x80));
  bad += "!OK\r\n";
  const auto failed =
      created.host->Decode(found.handle, View(bad), {Business, &capture}, {Observe, &capture});
  runner.Check(failed.status == pae::HostStatus::CODEC_FAILED && capture.observer_calls == 1U &&
                   capture.business_calls == 0U &&
                   capture.observer_status == pae::CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
                   capture.observer_match == 0U && !capture.observer_record_valid,
               "host_failed_candidate_has_same_match_no_business");
  const auto early =
      created.host->Decode(found.handle, {nullptr, 1U}, {Business, &capture}, {Observe, &capture});
  runner.Check(early.status == pae::HostStatus::INVALID_ARGUMENT && capture.observer_calls == 1U,
               "host_early_reject_no_candidate");
  const auto success = created.host->Decode(found.handle, View("RX A!OK\r\n"), {Business, &capture},
                                            {Observe, &capture});
  runner.Check(success.status == pae::HostStatus::OK && capture.observer_calls == 2U &&
                   capture.business_calls == 1U && capture.observer_match == 0U &&
                   capture.business_message == 0U && capture.observer_record_valid,
               "host_success_observer_and_business_share_match");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5) return 2;
  Runner runner;
  auto ascii = Compile(Read(argv[1]), runner, "ascii_compile");
  if (!ascii.HasValue()) return runner.Finish();
  CheckDescription(runner, ascii);
  CheckSlices(runner, ascii);
  CheckHost(runner, ascii);
  CheckZeroAndTail(runner);
  CheckNulAndOneWay(runner);
  CheckOtherOwners(runner, argv[2], argv[3]);
  CheckAmbiguity(runner);
  return runner.Finish();
}

void* operator new(std::size_t size) {
  g_allocations.fetch_add(1U, std::memory_order_relaxed);
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
