#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "frontend.h"
#include "pae/codec.h"
#include "pae/compiler.h"

namespace {

int failures = 0;
void Check(bool condition, const char* name) {
  std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
  if (!condition) ++failures;
}

std::string Read(const std::string& relative) {
  std::ifstream input(std::string(PAE_TEST_SOURCE) + "/" + relative, std::ios::binary);
  return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void BasicCases() {
  using pae::yaml_frontend::Convert;
  using pae::yaml_frontend::Limits;
  using pae::yaml_frontend::Status;
  auto good = Convert(
      "name: \"true\"\nflag: true\nempty: null\nnumber: -9223372036854775808\n"
      "path/~: [\"中文\", 0]\n");
  Check(good.Succeeded() && good.Json().find("\"name\":\"true\"") != std::string_view::npos &&
            good.Json().find("\"number\":-9223372036854775808") != std::string_view::npos,
        "explicit_scalar_classification");
  const auto* escaped = good.Find("/path~1~0/0");
  if (escaped)
    std::cout << "location /path~1~0/0 key=" << escaped->key_line << ':' << escaped->key_column
              << " value=" << escaped->value_line << ':' << escaped->value_column
              << " approximate=" << escaped->value_approximate << '\n';
  Check(escaped && escaped->value_line == 5 && escaped->key_line == 0,
        "owned_pointer_utf8_array_location");
  Check(escaped && escaped->value_column == 10 && !escaped->value_approximate,
        "array_value_exact_column");
  const auto* root = good.Find("");
  Check(root && root->value_line == 1 && root->value_column == 1, "root_source_location");
  const auto* sequence_container = good.Find("/path~1~0");
  Check(sequence_container && sequence_container->value_approximate &&
            good.FindNearest("/path~1~0/9") == sequence_container,
        "missing_property_nearest_approximate_container");
  const auto* scalar = good.Find("/flag");
  Check(scalar && scalar->key_line == 2 && scalar->value_line == 2 &&
            scalar->key_column != scalar->value_column && !scalar->value_approximate,
        "distinct_key_and_value_positions");
  Check(good.parser_retained_bytes == 0 && good.parser_peak_bytes > 0,
        "parser_allocations_released");
  std::string owned_input = "a: \"中文\"\n";
  std::string owned_source = "public-synthetic.yaml";
  auto owned = Convert(owned_input, owned_source);
  owned_input.assign(owned_input.size(), 'x');
  owned_source.assign(owned_source.size(), 'x');
  Check(owned.Succeeded() && owned.Json() == "{\"a\":\"中文\"}" && owned.Find("/a") != nullptr &&
            owned.SourceIdentity() == "public-synthetic.yaml",
        "json_source_identity_and_map_own_storage");
  Check(Convert("a: 1\n").Json() == Convert("a: 1\n").Json(), "same_input_deterministic_bytes");
  auto duplicate = Convert("a: 1\n\"\\u0061\": 2\n");
  Check(duplicate.status == Status::PROFILE_REJECTED && duplicate.Json().empty() &&
            duplicate.entry_count == 0,
        "decoded_duplicate_fails_closed");
  const std::array<std::string_view, 14> rejected = {"a: +1\n",
                                                     "a: 01\n",
                                                     "a: 2.0\n",
                                                     "a: 1e3\n",
                                                     "a: -0\n",
                                                     "a: 18446744073709551616\n",
                                                     "a: -9223372036854775809\n",
                                                     "a: True\n",
                                                     "a: .nan\n",
                                                     "a: &x 1\n",
                                                     "a: !str x\n",
                                                     "<<: 1\n",
                                                     "a: \n",
                                                     "a: [1, 2\n"};
  for (auto text : rejected) {
    auto result = Convert(text);
    Check(!result.Succeeded() && result.Json().empty() && result.entry_count == 0,
          "profile_rejection_no_partial_result");
  }
  Check(!Convert("a: 1\n---\nb: 2\n").Succeeded(), "multiple_documents_rejected");
  Check(!Convert("a: &item 1\nb: *item\n").Succeeded(), "anchor_alias_rejected");
  Check(!Convert("%YAML 1.1\n---\na: 1\n").Succeeded(), "yaml11_rejected");
  Check(!Convert("\xEF\xBB\xBF"
                 "a: 1\n")
             .Succeeded(),
        "bom_rejected");
  Check(!Convert("a: \xFF\n").Succeeded(), "invalid_utf8_rejected");
  for (auto bad_escape : {"a: \"\\uD800\"\n", "a: \"\\uDC00\"\n", "a: \"\\U00110000\"\n",
                          "\"\\uD800\": 1\n", "\"\\uDC00\": 1\n", "\"\\U00110000\": 1\n"}) {
    const auto decoded = Convert(bad_escape);
    std::cout << "unicode_escape_status=" << static_cast<int>(decoded.status)
              << " json_bytes=" << decoded.json_length << '\n';
    Check(!decoded.Succeeded() && decoded.Json().empty(), "invalid_decoded_unicode_rejected");
  }
  const auto non_bmp = Convert("\"\\U0001F600\": \"\\U0001F600\"\n");
  if (const auto* source = non_bmp.Find("/😀"))
    std::cout << "non_bmp_source key=" << source->key_line << ':' << source->key_column
              << " value=" << source->value_line << ':' << source->value_column << '\n';
  Check(
      non_bmp.Succeeded() && non_bmp.Json() == "{\"😀\":\"😀\"}" && non_bmp.Find("/😀") != nullptr,
      "valid_non_bmp_key_value_escape_accepted");
  const auto* non_bmp_source = non_bmp.Find("/😀");
  Check(non_bmp_source && non_bmp_source->key_line == 1 && non_bmp_source->key_column == 1 &&
            !non_bmp_source->key_approximate && non_bmp_source->value_line == 1 &&
            non_bmp_source->value_column == 15 && !non_bmp_source->value_approximate,
        "quoted_unicode_key_value_node_starts");
  const auto single_quote = Convert("a: 'x'\n");
  const auto* single_source = single_quote.Find("/a");
  Check(single_quote.Succeeded() && single_source && single_source->value_line == 1 &&
            single_source->value_column == 4 && !single_source->value_approximate,
        "single_quoted_value_node_start");
  Check(Convert("emoji: \"😀\"\n").Succeeded(), "valid_non_bmp_utf8_literal_accepted");
  Check(!Convert("emoji: \"\\uD83D\\uDE00\"\n").Succeeded(),
        "surrogate_pair_as_two_escapes_rejected");
  Check(!Convert("").Succeeded(), "empty_document_rejected");
  Check(!Convert("1: text\n").Succeeded(), "non_string_key_rejected");
  Check(!Convert("? [a, b]\n: text\n").Succeeded(), "complex_key_rejected");
  Check(Convert("a: -foo\nb: .well-known\n").Succeeded(),
        "non_numeric_plain_strings_remain_strings");
  auto block = Convert("a: |-\n  hello\n  世界\n");
  Check(block.Succeeded() && block.Json() == "{\"a\":\"hello\\u000a世界\"}",
        "block_scalar_decode_and_json_escape");
  auto escaped_key = Convert("\"中文/~\": 1\n");
  Check(escaped_key.Succeeded() && escaped_key.Find("/中文~1~0") != nullptr,
        "unicode_pointer_escaping");
  Check(Convert("%YAML   1.2   # allowed\n---\na: 1\n").Succeeded(),
        "yaml12_directive_whitespace_comment");
  Check(!Convert("%YAML1.2\n---\na: 1\n").Succeeded(), "yaml12_directive_requires_whitespace");
  Limits tiny;
  tiny.input_bytes = 3;
  Check(Convert("abcd: 1", tiny).status == Status::INPUT_LIMIT, "input_precheck");
  tiny = Limits{};
  tiny.parser_bytes = 1;
  Check(Convert("a: 1", tiny).status == Status::PARSER_BUDGET, "parser_callback_budget");
  tiny = Limits{};
  tiny.auxiliary_bytes = 1;
  Check(Convert("a: 1", tiny).status == Status::AUXILIARY_BUDGET, "auxiliary_budget");
  tiny = Limits{};
  tiny.nodes = 0;
  Check(Convert("{}", tiny).status == Status::AUXILIARY_BUDGET,
        "zero_node_capacity_rejected_before_allocation");
  tiny = Limits{};
  tiny.nodes = 1;
  tiny.auxiliary_bytes = sizeof(pae::yaml_frontend::SourceEntry);
  Check(Convert("{}", tiny).status == Status::AUXILIARY_BUDGET,
        "zero_pointer_and_path_capacity_rejected");
  tiny.auxiliary_bytes += 2;
  tiny.json_bytes = 2;
  Check(Convert("{}", tiny).Succeeded(), "minimum_root_auxiliary_and_json_exact_capacity");
  tiny = Limits{};
  tiny.nodes = 2;
  tiny.auxiliary_bytes = 2 * sizeof(pae::yaml_frontend::SourceEntry) + 4;
  tiny.json_bytes = 7;
  Check(Convert("a: 1", tiny).Succeeded(), "auxiliary_and_json_exact_capacity");
  --tiny.auxiliary_bytes;
  Check(Convert("a: 1", tiny).status == Status::AUXILIARY_BUDGET,
        "auxiliary_exact_minus_one_rejected");
  Check(Convert("a: 1", std::string(100000, 's')).status == Status::AUXILIARY_BUDGET,
        "source_identity_shares_auxiliary_budget");
  tiny = Limits{};
  tiny.json_bytes = 2;
  Check(Convert("a: 1", tiny).status == Status::OUTPUT_LIMIT, "writer_growth_budget");
  tiny.json_bytes = 0;
  const auto no_output = Convert("a: 1", tiny);
  Check(no_output.status == Status::OUTPUT_LIMIT && no_output.frontend_allocation_calls == 0 &&
            no_output.Json().empty() && no_output.entry_count == 0,
        "zero_json_capacity_rejected_before_allocation");
  tiny.json_bytes = 7;
  Check(Convert("a: 1", tiny).Succeeded(), "writer_exact_output_limit");
  tiny.json_bytes = 6;
  Check(Convert("a: 1", tiny).status == Status::OUTPUT_LIMIT, "writer_exact_minus_one_rejected");
  tiny = Limits{};
  tiny.scalar_bytes = 1;
  Check(Convert("ab: 1", tiny).status == Status::PROFILE_REJECTED, "scalar_limit_after_parse");
  tiny = Limits{};
  tiny.depth = 2;
  Check(Convert("a: [[1]]", tiny).status == Status::PROFILE_REJECTED, "depth_limit_after_parse");
  tiny = Limits{};
  tiny.nodes = 2;
  Check(Convert("a: [1, 2]", tiny).status == Status::PROFILE_REJECTED, "node_limit_after_parse");
  tiny = Limits{};
  tiny.fail_parser_allocation = 1;
  Check(Convert("a: 1", tiny).status == Status::ALLOCATION_FAILED,
        "parser_allocation_fault_injection");
  const auto reference = Convert("a: [1, 2, 3]\n");
  tiny = Limits{};
  tiny.parser_bytes = reference.parser_peak_bytes;
  Check(reference.Succeeded() && Convert("a: [1, 2, 3]\n", tiny).Succeeded(),
        "parser_exact_peak_budget");
  if (reference.parser_peak_bytes > 0) {
    --tiny.parser_bytes;
    Check(Convert("a: [1, 2, 3]\n", tiny).status == Status::PARSER_BUDGET,
          "parser_peak_minus_one_budget");
  }
  bool all_fail_points_recovered = reference.Succeeded() && reference.parser_allocation_calls > 0;
  for (std::size_t point = 1; point <= reference.parser_allocation_calls; ++point) {
    tiny = Limits{};
    tiny.fail_parser_allocation = point;
    const auto failed = Convert("a: [1, 2, 3]\n", tiny);
    all_fail_points_recovered &= failed.status == Status::ALLOCATION_FAILED &&
                                 failed.parser_retained_bytes == 0 &&
                                 Convert("a: [1, 2, 3]\n").Succeeded();
  }
  Check(all_fail_points_recovered, "each_parser_fault_releases_and_recovers");
  const auto frontend_reference = Convert("a: 1");
  bool frontend_fail_points_recovered =
      frontend_reference.Succeeded() && frontend_reference.frontend_allocation_calls == 4;
  for (std::size_t point = 1; point <= frontend_reference.frontend_allocation_calls; ++point) {
    tiny = Limits{};
    tiny.fail_frontend_allocation = point;
    const auto failed = Convert("a: 1", tiny);
    frontend_fail_points_recovered &=
        failed.status == Status::ALLOCATION_FAILED && failed.frontend_allocation_calls == point &&
        failed.Json().empty() && failed.entry_count == 0 && failed.parser_retained_bytes == 0 &&
        Convert("a: 1").Succeeded();
  }
  Check(frontend_fail_points_recovered, "each_auxiliary_and_json_fault_releases_and_recovers");
  Check(Convert("a: 1").Succeeded(), "recovery_after_fault");
  bool a = false, b = false;
  std::thread first([&] { a = Convert("x: 2").Succeeded(); });
  std::thread second([&] { b = Convert("y: false").Succeeded(); });
  first.join();
  second.join();
  Check(a && b, "concurrent_instance_isolation");
  bool budget_failed = false, concurrent_ok = false;
  std::thread limited([&] {
    Limits limit;
    limit.parser_bytes = 1;
    const auto failed = Convert("a: 1", limit);
    budget_failed = failed.status == Status::PARSER_BUDGET && failed.parser_retained_bytes == 0;
  });
  std::thread normal([&] { concurrent_ok = Convert("b: 2").Succeeded(); });
  limited.join();
  normal.join();
  Check(budget_failed && concurrent_ok, "concurrent_failure_does_not_affect_peer");
}

void PublicCases() {
  using pae::yaml_frontend::Convert;
  const auto crc_yaml = Read("spikes/yaml_frontend/fixtures/synthetic_crc_slice.pae.yaml");
  const auto ascii_yaml =
      Read("spikes/yaml_frontend/fixtures/synthetic_ascii_literal_only.pae.yaml");
  auto crc = Convert(crc_yaml);
  auto ascii = Convert(ascii_yaml);
  Check(crc.Succeeded() && ascii.Succeeded(), "public_fixtures_convert");
  std::cout << "fixture_usage crc_input=" << crc_yaml.size() << " crc_json=" << crc.json_length
            << " crc_nodes=" << crc.entry_count << " crc_parser_peak=" << crc.parser_peak_bytes
            << " ascii_input=" << ascii_yaml.size() << " ascii_json=" << ascii.json_length
            << " ascii_nodes=" << ascii.entry_count
            << " ascii_parser_peak=" << ascii.parser_peak_bytes << '\n';
  if (!crc.Succeeded() || !ascii.Succeeded()) {
    std::cout << "fixture errors: " << crc.reason << ", " << ascii.reason << '\n';
    return;
  }
  auto original_crc =
      pae::CompileProtocolJson(Read("examples/config/synthetic_crc_slice.pae.json"));
  auto converted_crc = pae::CompileProtocolJson(crc.Json());
  auto original_ascii =
      pae::CompileProtocolJson(Read("examples/config/synthetic_ascii_literal_only.pae.json"));
  auto converted_ascii = pae::CompileProtocolJson(ascii.Json());
  Check(original_crc.Succeeded() && converted_crc.Succeeded() && original_ascii.Succeeded() &&
            converted_ascii.Succeeded(),
        "real_public_compiler_both_sources");
  if (!original_crc.Succeeded() || !converted_crc.Succeeded() || !original_ascii.Succeeded() ||
      !converted_ascii.Succeeded()) {
    if (const auto* d = converted_crc.Diagnostic())
      std::cout << "crc diagnostic: " << d->detail << '\n';
    if (const auto* d = converted_ascii.Diagnostic())
      std::cout << "ascii diagnostic: " << d->detail << '\n';
    return;
  }
  const auto& old_crc = *original_crc.Compiled();
  const auto& new_crc = *converted_crc.Compiled();
  Check(old_crc.PipelineCount() == new_crc.PipelineCount() &&
            old_crc.MessageCount() == new_crc.MessageCount() &&
            old_crc.FieldCount() == new_crc.FieldCount(),
        "binary_public_metadata_equivalence");
  const auto old_crc_protocol = old_crc.Protocol();
  const auto new_crc_protocol = new_crc.Protocol();
  const auto old_crc_pipeline = old_crc.Pipeline(0);
  const auto new_crc_pipeline = new_crc.Pipeline(0);
  const auto old_crc_message = old_crc.Message(0);
  const auto new_crc_message = new_crc.Message(0);
  const auto old_crc_field = old_crc.Field(0);
  const auto new_crc_field = new_crc.Field(0);
  Check(old_crc_protocol && new_crc_protocol &&
            old_crc_protocol->schema_version == new_crc_protocol->schema_version &&
            old_crc_protocol->id == new_crc_protocol->id && old_crc_pipeline && new_crc_pipeline &&
            old_crc_pipeline->id == new_crc_pipeline->id &&
            old_crc_pipeline->direction_id == new_crc_pipeline->direction_id && old_crc_message &&
            new_crc_message && old_crc_message->id == new_crc_message->id && old_crc_field &&
            new_crc_field && old_crc_field->id == new_crc_field->id &&
            old_crc_field->value_kind == new_crc_field->value_kind,
        "binary_public_identity_and_type_equivalence");
  const auto old_ascii_protocol = original_ascii.Compiled()->Protocol();
  const auto new_ascii_protocol = converted_ascii.Compiled()->Protocol();
  const auto old_ascii_message = original_ascii.Compiled()->Message(0);
  const auto new_ascii_message = converted_ascii.Compiled()->Message(0);
  Check(old_ascii_protocol && new_ascii_protocol &&
            old_ascii_protocol->schema_version == new_ascii_protocol->schema_version &&
            old_ascii_protocol->id == new_ascii_protocol->id && old_ascii_message &&
            new_ascii_message && old_ascii_message->id == new_ascii_message->id,
        "ascii_public_identity_equivalence");
  const std::array<std::uint8_t, 12> crc_frame = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
                                                  0x37, 0x38, 0x39, 0x29, 0xB1, 0xAA};
  const std::array<std::uint8_t, 9> payload = {0x31, 0x32, 0x33, 0x34, 0x35,
                                               0x36, 0x37, 0x38, 0x39};
  for (const auto* compiled : {&old_crc, &new_crc}) {
    auto created = pae::CreateCompleteRecordCodec(*compiled);
    Check(created.status == pae::CodecStatus::OK && created.codec != nullptr,
          "binary_codec_create");
    if (!created.codec) continue;
    auto decoded = created.codec->Decode(0, {crc_frame.data(), crc_frame.size()});
    Check(decoded.status == pae::CodecStatus::OK && decoded.record.FieldCount() == 1,
          "binary_known_decode");
    std::array<std::uint8_t, 12> output{};
    auto value = pae::EncodeValue::Bytes({0, 0}, {payload.data(), payload.size()});
    auto encoded = created.codec->Encode(0, 0, &value, 1, {output.data(), output.size()});
    Check(encoded.status == pae::CodecStatus::OK && output == crc_frame, "binary_known_encode");
  }
  for (const auto* compiled : {original_ascii.Compiled(), converted_ascii.Compiled()}) {
    auto created = pae::CreateCompleteRecordCodec(*compiled);
    Check(created.status == pae::CodecStatus::OK && created.codec != nullptr, "ascii_codec_create");
    if (!created.codec) continue;
    const std::string input = "PING\r\n";
    auto decoded = created.codec->Decode(
        0, {reinterpret_cast<const std::uint8_t*>(input.data()), input.size()});
    Check(decoded.status == pae::CodecStatus::OK && decoded.record.FieldCount() == 0,
          "ascii_known_decode");
    std::array<std::uint8_t, 6> output{};
    auto encoded = created.codec->Encode(0, 0, nullptr, 0, {output.data(), output.size()});
    Check(encoded.status == pae::CodecStatus::OK &&
              std::string_view(reinterpret_cast<const char*>(output.data()), output.size()) ==
                  "PONG\r\n",
          "ascii_known_encode");
  }
  auto invalid_yaml = ascii_yaml;
  invalid_yaml += "unknown_property: 1\n";
  auto invalid = Convert(invalid_yaml);
  Check(invalid.Succeeded(), "downstream_invalid_yaml_convert");
  if (invalid.Succeeded()) {
    auto compiled = pae::CompileProtocolJson(invalid.Json());
    Check(!compiled.Succeeded() && compiled.Diagnostic() != nullptr,
          "downstream_diagnostic_preserved");
    if (const auto* diagnostic = compiled.Diagnostic()) {
      std::cout << "downstream_pointer=" << diagnostic->json_pointer << '\n';
      const auto* source = invalid.FindNearest(diagnostic->json_pointer);
      if (source)
        std::cout << "downstream_source=" << invalid.Pointer(*source) << " key=" << source->key_line
                  << ':' << source->key_column << " value=" << source->value_line << ':'
                  << source->value_column << " approximate=" << source->value_approximate << '\n';
      Check(diagnostic->json_pointer == "/unknown_property" && source &&
                invalid.Pointer(*source) == "/unknown_property" && source->key_line == 41 &&
                source->key_column == 1 && source->value_line == 41 && source->value_column == 19 &&
                !source->value_approximate,
            "downstream_unknown_property_exact_yaml_source");
    }
  }
  auto missing_yaml = ascii_yaml;
  const auto missing_begin = missing_yaml.find("resource_profile: desktop\n");
  Check(missing_begin != std::string::npos, "missing_property_fixture_mutation_anchor");
  if (missing_begin != std::string::npos) {
    missing_yaml.erase(missing_begin, std::string("resource_profile: desktop\n").size());
    auto missing = Convert(missing_yaml);
    Check(missing.Succeeded(), "missing_property_yaml_converts");
    if (missing.Succeeded()) {
      auto compiled = pae::CompileProtocolJson(missing.Json());
      const auto* diagnostic = compiled.Diagnostic();
      const auto* exact = diagnostic ? missing.Find(diagnostic->json_pointer) : nullptr;
      const auto* nearest = diagnostic ? missing.FindNearest(diagnostic->json_pointer) : nullptr;
      if (diagnostic) std::cout << "missing_pointer=" << diagnostic->json_pointer << '\n';
      if (nearest)
        std::cout << "missing_nearest=" << missing.Pointer(*nearest)
                  << " value=" << nearest->value_line << ':' << nearest->value_column
                  << " approximate=" << nearest->value_approximate << '\n';
      Check(!compiled.Succeeded() && diagnostic &&
                diagnostic->json_pointer == "/resource_profile" && exact == nullptr && nearest &&
                missing.Pointer(*nearest).empty() && nearest->value_line == 2 &&
                nearest->value_column == 1 && !nearest->value_approximate,
            "missing_property_explicit_root_fallback");
    }
  }
}

}  // namespace

int main() {
  BasicCases();
  PublicCases();
  std::cout << "failures=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}
