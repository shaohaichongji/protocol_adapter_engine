#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include "public_ascii_offline_adapter.h"

using namespace pae::protocol_lab_ascii::public_offline;

#undef assert
#define assert(expression)                                          \
  do {                                                              \
    if (!(expression)) {                                            \
      std::cerr << "PUBLIC_ASCII_A1_CHECK_FAILED line=" << __LINE__ \
                << " expression=" << #expression << '\n';           \
      return 1;                                                     \
    }                                                               \
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

PrepareResult Prepare(std::string_view json, const Limits& limits = {}, std::size_t previous = 0U) {
  auto compiled = pae::CompileProtocolJson(json);
  if (!compiled.Succeeded()) {
    if (compiled.Diagnostic()) {
      std::cerr << "PUBLIC_ASCII_A1_COMPILE_FAILED code="
                << static_cast<int>(compiled.Diagnostic()->code)
                << " detail=" << compiled.Diagnostic()->detail << '\n';
    }
    return {};
  }
  return Adapter::AdoptCompiled(std::move(compiled).TakeCompiled(), limits, previous);
}

std::string LiteralConfig(std::string_view actions) {
  return std::string{R"JSON({
"schema_version":"0.10","protocol_id":"a1_literal","protocol_version":"1",
"display_name":"A1 literal","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","input_kind":"complete_record"}],
"pipelines":[{"id":"ascii_pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii",)JSON"} +
         std::string{actions} + R"JSON(},"fields":[]}]})JSON";
}

std::string ZeroAndNulConfig() {
  return R"JSON({
"schema_version":"0.10","protocol_id":"a1_zero_nul","protocol_version":"1",
"display_name":"A1 zero NUL","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","input_kind":"complete_record"}],
"pipelines":[{"id":"ascii_pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii",
"decode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"literal","text":"A\u0000BA"}]},
"encode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"literal","text":"A\u0000BA"}]}},
"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","value_type":"BYTES","wire":{"codec":"ascii_text","min_byte_length":0,"max_byte_length":4},"encode":{"source":"input"}}]}]})JSON";
}

std::string MultiMessageConfig() {
  return R"JSON({
"schema_version":"0.10","protocol_id":"a1_multi","protocol_version":"1",
"display_name":"A1 multi","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","input_kind":"complete_record"}],
"pipelines":[{"id":"ascii_pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","input_framing_profile_id":"record","message_ids":["first","second"]}],
"messages":[
{"id":"first","display_name":"First","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"RX1"}]},"encode":{"segments":[{"kind":"literal","text":"TX1"}]}},"fields":[]},
{"id":"second","display_name":"Second","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"RX2"}]},"encode":{"segments":[{"kind":"literal","text":"TX2"}]}},"fields":[]}
]})JSON";
}

std::string MixedDirectionConfig() {
  return R"JSON({
"schema_version":"0.10","protocol_id":"a1_mixed","protocol_version":"1",
"display_name":"A1 mixed","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","input_kind":"complete_record"}],
"pipelines":[{"id":"ascii_pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","input_framing_profile_id":"record","message_ids":["rx","tx"]}],
"messages":[
{"id":"rx","display_name":"RX","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"literal","text":"RX"}]}},"fields":[]},
{"id":"tx","display_name":"TX","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:A1","direction_id":"test","layout":{"kind":"text","encoding":"ascii","encode":{"segments":[{"kind":"literal","text":"TX"}]}},"fields":[]}
]})JSON";
}

}  // namespace

int main(int argc, char** argv) {
  assert(argc == 2);
  const std::string root = argv[1];
  const auto primary_json = Read(root + "/examples/config/synthetic_ascii_text_slice.pae.json");
  auto prepared = Prepare(primary_json);
  assert(prepared.status == LocalStatus::OK && prepared.adapter);
  auto& adapter = *prepared.adapter;
  const auto& description = adapter.Description();
  assert(description.schema_version == "0.10" && description.pipelines.size() == 1U);
  assert(description.pipelines[0].message_indices == std::vector<std::size_t>{0U} &&
         description.pipelines[0].decode_message_indices == std::vector<std::size_t>{0U} &&
         description.pipelines[0].encode_message_indices == std::vector<std::size_t>{0U});
  assert(description.messages.size() == 1U && description.messages[0].decode &&
         description.messages[0].encode);
  assert(description.messages[0].decode->segments.size() == 5U &&
         description.messages[0].encode->segments.size() == 5U);
  assert(description.messages[0].fields.size() == 3U);
  assert(description.messages[0].fields[0].decode_referenced &&
         description.messages[0].fields[0].encode_referenced);
  assert(description.messages[0].fields[1].decode_referenced &&
         !description.messages[0].fields[1].encode_referenced);
  assert(!description.messages[0].fields[2].decode_referenced &&
         description.messages[0].fields[2].encode_referenced);
  assert(description.messages[0].fields[0].allowed_control_bytes == std::nullopt);

  auto rx = Bytes("RX ALICE!OK\r\n");
  auto decoded = adapter.Decode(0U, {rx.data(), rx.size()});
  assert(decoded.local_status == LocalStatus::OK && decoded.codec_called &&
         decoded.codec_status == pae::CodecStatus::OK && decoded.message_index == 0U &&
         decoded.matched_message_index == 0U && decoded.fields.size() == 2U);
  assert(decoded.fields[0].id == "name" && decoded.fields[0].range.offset == 3U &&
         decoded.fields[0].range.length == 5U && Text(decoded.fields[0].bytes) == "ALICE");
  assert(decoded.fields[1].id == "rx_code" && decoded.fields[1].range.offset == 9U &&
         decoded.fields[1].range.length == 2U);
  rx.assign(1U, 0U);
  assert(Text(decoded.frame) == "RX ALICE!OK\r\n" && Text(decoded.fields[0].bytes) == "ALICE");

  std::vector<InputField> tx{{0U, Bytes("TX ")}, {2U, Bytes("Z")}};
  auto encoded = adapter.Encode(0U, 0U, tx);
  assert(encoded.local_status == LocalStatus::OK && encoded.codec_called &&
         encoded.codec_status == pae::CodecStatus::OK && Text(encoded.frame) == "TX TX !Z\r\n");
  assert(encoded.fields.size() == 2U && encoded.fields[0].range.offset == 3U &&
         encoded.fields[0].range.length == 3U && encoded.fields[1].range.offset == 7U);
  tx[0].bytes.assign(1U, 0U);
  assert(Text(encoded.fields[0].bytes) == "TX ");

  auto invalid_pipeline = adapter.Decode(99U, {decoded.frame.data(), decoded.frame.size()});
  assert(invalid_pipeline.codec_called &&
         invalid_pipeline.local_status == LocalStatus::CODEC_FAILED &&
         invalid_pipeline.codec_status == pae::CodecStatus::INVALID_ARGUMENT);
  auto invalid_message = adapter.Encode(0U, 99U, {});
  assert(invalid_message.codec_called &&
         invalid_message.local_status == LocalStatus::CODEC_FAILED &&
         invalid_message.codec_status != pae::CodecStatus::OK);
  auto not_referenced = adapter.Encode(0U, 0U, {{1U, Bytes("OK")}});
  assert(not_referenced.codec_called && not_referenced.local_status == LocalStatus::CODEC_FAILED &&
         not_referenced.frame.empty() && not_referenced.fields.empty());

  auto bad = Bytes("RX A");
  bad.push_back(0x80U);
  const auto tail = Bytes("!OK\r\n");
  bad.insert(bad.end(), tail.begin(), tail.end());
  auto failed = adapter.Decode(0U, {bad.data(), bad.size()});
  assert(failed.local_status == LocalStatus::CODEC_FAILED && failed.codec_called &&
         failed.codec_status == pae::CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
         failed.matched_message_index == 0U && !failed.message_index && failed.fields.empty() &&
         failed.frame.empty() && failed.diagnostic_input_frame == bad);
  auto unknown = adapter.Decode(0U, {reinterpret_cast<const std::uint8_t*>("NO"), 2U});
  assert(unknown.codec_status == pae::CodecStatus::UNKNOWN_MESSAGE &&
         !unknown.matched_message_index && unknown.fields.empty());
  auto restored_bytes = Bytes("RX A!OK\r\n");
  auto restored = adapter.Decode(0U, {restored_bytes.data(), restored_bytes.size()});
  assert(restored.local_status == LocalStatus::OK && restored.fields.size() == 2U);

  auto literal = Prepare(Read(root + "/examples/config/synthetic_ascii_literal_only.pae.json"));
  assert(literal.status == LocalStatus::OK && literal.adapter);
  auto ping = Bytes("PING\r\n");
  assert(literal.adapter->Decode(0U, {ping.data(), ping.size()}).fields.empty());
  assert(Text(literal.adapter->Encode(0U, 0U, {}).frame) == "PONG\r\n");
  auto decode_only = Prepare(
      LiteralConfig(R"JSON("decode":{"segments":[{"kind":"literal","text":"PING\r\n"}]})JSON"));
  assert(decode_only.status == LocalStatus::OK && decode_only.adapter);
  assert(decode_only.adapter->Description().pipelines[0].decode_message_indices ==
             std::vector<std::size_t>{0U} &&
         decode_only.adapter->Description().pipelines[0].encode_message_indices.empty());
  assert(decode_only.adapter->Encode(0U, 0U, {}).codec_status ==
         pae::CodecStatus::OPERATION_NOT_SUPPORTED);
  auto encode_only = Prepare(
      LiteralConfig(R"JSON("encode":{"segments":[{"kind":"literal","text":"PONG\r\n"}]})JSON"));
  assert(encode_only.status == LocalStatus::OK && encode_only.adapter);
  assert(encode_only.adapter->Description().pipelines[0].decode_message_indices.empty() &&
         encode_only.adapter->Description().pipelines[0].encode_message_indices ==
             std::vector<std::size_t>{0U});
  assert(encode_only.adapter->Decode(0U, {ping.data(), ping.size()}).codec_status ==
         pae::CodecStatus::OPERATION_NOT_SUPPORTED);
#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  const auto decode_only_capacity = decode_only.adapter->PipelineCapacitiesForTesting(0U);
  const auto encode_only_capacity = encode_only.adapter->PipelineCapacitiesForTesting(0U);
  assert(decode_only_capacity && decode_only_capacity->message_indices == 1U &&
         decode_only_capacity->decode_message_indices == 1U &&
         decode_only_capacity->encode_message_indices == 0U);
  assert(encode_only_capacity && encode_only_capacity->message_indices == 1U &&
         encode_only_capacity->decode_message_indices == 0U &&
         encode_only_capacity->encode_message_indices == 1U);
  auto mixed_direction = Prepare(MixedDirectionConfig());
  assert(mixed_direction.status == LocalStatus::OK && mixed_direction.adapter);
  const auto mixed_capacity = mixed_direction.adapter->PipelineCapacitiesForTesting(0U);
  assert(mixed_capacity && mixed_capacity->message_indices == 2U &&
         mixed_capacity->decode_message_indices == 1U &&
         mixed_capacity->encode_message_indices == 1U);
#endif
  auto multi = Prepare(MultiMessageConfig());
  assert(multi.status == LocalStatus::OK && multi.adapter &&
         multi.adapter->Description().pipelines[0].message_indices.size() == 2U);
  assert(Text(multi.adapter->Encode(0U, 0U, {}).frame) == "TX1" &&
         Text(multi.adapter->Encode(0U, 1U, {}).frame) == "TX2");

  auto zero = Prepare(ZeroAndNulConfig());
  assert(zero.status == LocalStatus::OK && zero.adapter);
  const std::array<std::uint8_t, 4> nul_frame{'A', 0U, 'B', 'A'};
  auto zero_rx = zero.adapter->Decode(0U, {nul_frame.data(), nul_frame.size()});
  assert(zero_rx.local_status == LocalStatus::OK && zero_rx.fields.size() == 1U &&
         zero_rx.fields[0].range.offset == 0U && zero_rx.fields[0].range.length == 0U);
  auto zero_tx = zero.adapter->Encode(0U, 0U, {{0U, {}}});
  assert(zero_tx.local_status == LocalStatus::OK && zero_tx.fields.size() == 1U &&
         zero_tx.fields[0].range.offset == 0U && zero_tx.fields[0].range.length == 0U &&
         zero_tx.frame == std::vector<std::uint8_t>(nul_frame.begin(), nul_frame.end()));

  const auto description_bytes = adapter.DescriptionAccountedBytes();
  Limits description_limit;
  description_limit.max_description_bytes = description_bytes;
  assert(Prepare(primary_json, description_limit).status == LocalStatus::OK);
  description_limit.max_description_bytes = description_bytes - 1U;
  assert(Prepare(primary_json, description_limit).status == LocalStatus::RESOURCE_LIMIT);
  const auto result_bytes = decoded.accounted_bytes;
  Limits result_limit;
  result_limit.max_result_bytes = result_bytes;
  auto exact_result = Prepare(primary_json, result_limit);
  assert(exact_result.status == LocalStatus::OK && exact_result.adapter);
  auto exact_frame = Bytes("RX ALICE!OK\r\n");
  assert(exact_result.adapter->Decode(0U, {exact_frame.data(), exact_frame.size()}).local_status ==
         LocalStatus::OK);
  result_limit.max_result_bytes = result_bytes - 1U;
  auto short_result = Prepare(primary_json, result_limit);
  assert(short_result.status == LocalStatus::OK && short_result.adapter);
  auto short_failure = short_result.adapter->Decode(0U, {exact_frame.data(), exact_frame.size()});
  assert(short_failure.codec_status == pae::CodecStatus::OK &&
         short_failure.local_status == LocalStatus::MATERIALIZATION_FAILED &&
         short_failure.frame.empty() && short_failure.fields.empty());

#if defined(PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS)
  adapter.FailNextMaterializationForTesting();
  auto mapping_failure = adapter.Decode(0U, {exact_frame.data(), exact_frame.size()});
  assert(mapping_failure.codec_status == pae::CodecStatus::OK &&
         mapping_failure.local_status == LocalStatus::MATERIALIZATION_FAILED &&
         mapping_failure.fields.empty());
  adapter.FailNextMaterializationForTesting();
  auto tx_mapping_failure = adapter.Encode(0U, 0U, {{0U, Bytes("A")}, {2U, Bytes("Z")}});
  assert(tx_mapping_failure.codec_status == pae::CodecStatus::OK &&
         tx_mapping_failure.local_status == LocalStatus::MATERIALIZATION_FAILED &&
         tx_mapping_failure.frame.empty() && tx_mapping_failure.fields.empty());
  adapter.FailNextAllocationForTesting();
  auto allocation_failure = adapter.Encode(0U, 0U, {{0U, Bytes("A")}, {2U, Bytes("Z")}});
  assert(allocation_failure.codec_status == pae::CodecStatus::OK &&
         allocation_failure.local_status == LocalStatus::ALLOCATION_FAILED &&
         allocation_failure.frame.empty());
#endif

  const auto admission = adapter.InstanceAdmissionBytes();
  Limits exact_instance;
  exact_instance.instance_bytes = admission;
  assert(Prepare(primary_json, exact_instance).status == LocalStatus::OK);
  exact_instance.instance_bytes = admission - 1U;
  assert(Prepare(primary_json, exact_instance).status == LocalStatus::RESOURCE_LIMIT);
  exact_instance.instance_bytes = admission;
  exact_instance.replacement_bytes = admission + 9U;
  assert(Prepare(primary_json, exact_instance, 10U).status == LocalStatus::RESOURCE_LIMIT);
  exact_instance.replacement_bytes = admission + 10U;
  assert(Prepare(primary_json, exact_instance, 10U).status == LocalStatus::OK);

  auto owned = adapter.Decode(0U, {exact_frame.data(), exact_frame.size()});
  prepared.adapter.reset();
  assert(Text(owned.frame) == "RX ALICE!OK\r\n" && Text(owned.fields[0].bytes) == "ALICE");
  std::cout << "PUBLIC_ASCII_A1_TEST_PASS\n";
  return 0;
}
