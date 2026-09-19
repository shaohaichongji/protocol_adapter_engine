#include <cassert>
#include <string>
#include <type_traits>
#include <vector>

#include "../../tools/protocol_lab_ui/public_legacy_complete_adapter.h"
#include "test_support.h"

#if !defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
#error "public legacy fault-injection tests require test instrumentation"
#endif

namespace {

using namespace pae::protocol_lab_ui;

pae::CompiledProtocol Compile(const char* fixture) {
  auto result = pae::CompileProtocolJson(test::ReadFixture(fixture));
  assert(result.Succeeded());
  return std::move(result).TakeCompiled();
}

pae::CompiledProtocol CompileJson(std::string json) {
  auto result = pae::CompileProtocolJson(json);
  assert(result.Succeeded());
  return std::move(result).TakeCompiled();
}

void TestSinglePublicCompilerOwner() {
  CompileWorker worker;
  assert(worker.Submit(1U, 7U, test::ReadFixture("synthetic_ui_v07.pae.json")) ==
         SubmitStatus::ACCEPTED);
  std::vector<ResultTicket> tickets;
  assert(test::WaitUntil([&] {
    tickets = worker.DrainReadyTickets();
    return !tickets.empty();
  }));
  assert(tickets.size() == 1U);
  auto completion = worker.TakeResult(tickets.front());
  assert(completion);
  assert(completion->route == SchemaDispatchStatus::LEGACY_PUBLIC);
  assert(completion->compiler_attempt_count == 1U);
  assert(completion->public_compiled && !completion->artifacts);
  assert(!completion->public_diagnostic && !completion->diagnostic);
}

void TestOwnedDescriptionAndCompleteExecution() {
  auto prepared = PublicLegacyCompleteAdapter::AdoptCompiled(Compile("synthetic_ui_v07.pae.json"));
  assert(prepared.status == PublicLegacyLocalStatus::OK && prepared.adapter);
  assert(prepared.description.schema_version == "0.7");
  assert(prepared.description.messages.size() == 1U);
  assert(prepared.description.messages[0].fields[0].encode_source ==
         FieldEncodeSource::COMPUTED);

  std::vector<PublicLegacyInput> inputs;
  inputs.push_back({1U, std::uint64_t{42U}});
  inputs.push_back({2U, std::vector<std::uint8_t>{0xABU}});
  const auto encoded = prepared.adapter->Encode(0U, 0U, inputs);
  assert(encoded.ok() && encoded.codec_called && encoded.review_decode_called);
  assert(encoded.codec_status == pae::CodecStatus::OK);
  assert(encoded.review_status == pae::CodecStatus::OK);
  assert(encoded.message_index == 0U && encoded.message_id == "length_record");
  assert(encoded.frame ==
         std::vector<std::uint8_t>({0xAAU, 0x00U, 0x06U, 0x2AU, 0xABU, 0x55U}));
  assert(encoded.fields.size() == 3U && encoded.fields[0].raw_value == "6");

  const auto decoded = prepared.adapter->Decode(0U, encoded.frame);
  assert(decoded.ok() && decoded.codec_called && !decoded.review_decode_called);
  assert(decoded.message_id == "length_record" && decoded.fields.size() == 3U);

  const auto bad = prepared.adapter->Decode(
      0U, {0xAAU, 0x00U, 0x05U, 0x2AU, 0xABU, 0x55U});
  assert(!bad.ok() && bad.codec_status == pae::CodecStatus::LENGTH_MISMATCH);
  assert(bad.message_index == 0U && bad.message_id == "length_record");
  assert(bad.failed_field_index == 0U && bad.failed_field_id == "record_length");
  assert(bad.fields.empty());
}

void TestPipelineRestrictionAndBudget() {
  auto multi = PublicLegacyCompleteAdapter::AdoptCompiled(
      Compile("synthetic_ui_inspect_multi_v05.pae.json"));
  assert(multi.adapter);
  const auto selected = multi.adapter->Decode(0U, {0xB2U, 0x07U});
  assert(selected.ok() && selected.message_index == 1U && selected.message_id == "message_b");
  auto restricted =
      PublicLegacyCompleteAdapter::AdoptCompiled(Compile("synthetic_ui_v05.pae.json"));
  assert(restricted.adapter);
  const auto wrong_pipeline = restricted.adapter->Decode(
      1U, std::vector<std::uint8_t>(10U, 0U));
  assert(!wrong_pipeline.ok() && wrong_pipeline.codec_status != pae::CodecStatus::OK);

  auto baseline = PublicLegacyCompleteAdapter::AdoptCompiled(Compile("synthetic_ui_v07.pae.json"));
  assert(baseline.adapter && baseline.adapter->AccountedInstanceBytes() > 0U);
  PublicLegacyCompleteLimits limits;
  limits.instance_bytes = baseline.adapter->AccountedInstanceBytes() - 1U;
  auto rejected =
      PublicLegacyCompleteAdapter::AdoptCompiled(Compile("synthetic_ui_v07.pae.json"), limits);
  assert(!rejected.adapter && rejected.status == PublicLegacyLocalStatus::RESOURCE_LIMIT);

  limits.instance_bytes = baseline.adapter->AccountedInstanceBytes();
  limits.replacement_bytes = limits.instance_bytes * 2U - 1U;
  rejected = PublicLegacyCompleteAdapter::AdoptCompiled(
      Compile("synthetic_ui_v07.pae.json"), limits, limits.instance_bytes);
  assert(!rejected.adapter && rejected.status == PublicLegacyLocalStatus::RESOURCE_LIMIT);
}

bool TestPreflightBeforeOwnedCopies() {
  PublicLegacyCompleteLimits limits;
  limits.max_description_bytes = sizeof(DocumentDescription) - 1U;
  auto compiled = Compile("synthetic_ui_v07.pae.json");
  PublicLegacyCompleteAdapter::FailNextDescriptionCopyAllocationForTesting();
  auto rejected = PublicLegacyCompleteAdapter::AdoptCompiled(std::move(compiled), limits);
  return !rejected.adapter && rejected.status == PublicLegacyLocalStatus::RESOURCE_LIMIT;
}

bool TestUnifiedResultPreflight() {
  constexpr std::size_t kPayloadBytes = 4096U;
  auto bytes = PublicLegacyCompleteAdapter::AdoptCompiled(CompileJson(R"json({
    "schema_version":"0.5","protocol_id":"large_bytes","protocol_version":"1",
    "display_name":"Large bytes","description":"Large bytes result-budget fixture.","source_ref":"SYNTHETIC:large-bytes",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Record","description":"Complete record.","source_ref":"SYNTHETIC:large-bytes#framing","input_kind":"complete_record"}],
    "pipelines":[{"id":"pipeline","display_name":"Pipeline","description":"Test pipeline.","source_ref":"SYNTHETIC:large-bytes#pipeline","direction_id":"test","input_framing_profile_id":"record","message_ids":["bytes"]}],
    "messages":[{"id":"bytes","display_name":"Bytes","description":"Large bytes record.","source_ref":"SYNTHETIC:large-bytes#message","direction_id":"test","frame_length_bytes":4096,
      "matcher":{"all":[{"kind":"frame_length_equals","length_bytes":4096}]},
      "fields":[{"id":"payload","display_name":"Payload","description":"Large payload.","source_ref":"SYNTHETIC:large-bytes#payload","value_type":"BYTES","wire":{"codec":"bytes","byte_offset":0,"byte_length":4096},"encode":{"source":"input"}}]}]
  })json"));
  assert(bytes.adapter);
  const std::vector<std::uint8_t> frame(kPayloadBytes, 0xABU);
  const auto baseline = bytes.adapter->Decode(0U, frame);
  assert(baseline.ok() && baseline.accounted_bytes > 1U);

  PublicLegacyCompleteLimits limits;
  limits.max_result_bytes = baseline.accounted_bytes - 1U;
  auto limited = PublicLegacyCompleteAdapter::AdoptCompiled(
      CompileJson(R"json({
        "schema_version":"0.5","protocol_id":"large_bytes","protocol_version":"1",
        "display_name":"Large bytes","description":"Large bytes result-budget fixture.","source_ref":"SYNTHETIC:large-bytes",
        "resource_profile":"desktop",
        "framing_profiles":[{"id":"record","display_name":"Record","description":"Complete record.","source_ref":"SYNTHETIC:large-bytes#framing","input_kind":"complete_record"}],
        "pipelines":[{"id":"pipeline","display_name":"Pipeline","description":"Test pipeline.","source_ref":"SYNTHETIC:large-bytes#pipeline","direction_id":"test","input_framing_profile_id":"record","message_ids":["bytes"]}],
        "messages":[{"id":"bytes","display_name":"Bytes","description":"Large bytes record.","source_ref":"SYNTHETIC:large-bytes#message","direction_id":"test","frame_length_bytes":4096,
          "matcher":{"all":[{"kind":"frame_length_equals","length_bytes":4096}]},
          "fields":[{"id":"payload","display_name":"Payload","description":"Large payload.","source_ref":"SYNTHETIC:large-bytes#payload","value_type":"BYTES","wire":{"codec":"bytes","byte_offset":0,"byte_length":4096},"encode":{"source":"input"}}]}]
      })json"),
      limits);
  assert(limited.adapter);
  limited.adapter->FailNextResultAllocationForTesting();
  const auto success_rejected = limited.adapter->Decode(0U, frame);
  if (success_rejected.local_status != PublicLegacyLocalStatus::RESOURCE_LIMIT ||
      !success_rejected.frame.empty() || !success_rejected.fields.empty())
    return false;

  auto length = PublicLegacyCompleteAdapter::AdoptCompiled(Compile("synthetic_ui_v07.pae.json"));
  assert(length.adapter);
  const std::vector<std::uint8_t> broken{0xAAU, 0x00U, 0x05U, 0x2AU, 0xABU, 0x55U};
  const auto failed_baseline = length.adapter->Decode(0U, broken);
  assert(failed_baseline.local_status == PublicLegacyLocalStatus::CODEC_FAILED &&
         failed_baseline.accounted_bytes > 1U);
  limits.max_result_bytes = failed_baseline.accounted_bytes - 1U;
  auto failed_limited =
      PublicLegacyCompleteAdapter::AdoptCompiled(Compile("synthetic_ui_v07.pae.json"), limits);
  assert(failed_limited.adapter);
  failed_limited.adapter->FailNextResultAllocationForTesting();
  const auto failure_rejected = failed_limited.adapter->Decode(0U, broken);
  return failure_rejected.local_status == PublicLegacyLocalStatus::RESOURCE_LIMIT &&
         failure_rejected.frame.empty() && failure_rejected.fields.empty();
}

void TestNoexceptDiagnosticAllocationFailure() {
  auto compiled = Compile("synthetic_ui_v07.pae.json");
  PublicLegacyCompleteAdapter::FailNextPreparationDiagnosticAllocationForTesting();
  auto rejected = PublicLegacyCompleteAdapter::AdoptCompiled(std::move(compiled));
  assert(!rejected.adapter && rejected.status == PublicLegacyLocalStatus::ALLOCATION_FAILED &&
         rejected.error == PublicLegacyPreparationError::ALLOCATION_FAILED &&
         rejected.detail.empty());
}

}  // namespace

int main() {
  static_assert(!std::is_copy_constructible_v<PublicLegacyCompleteAdapter>);
  if (!TestPreflightBeforeOwnedCopies()) {
    std::fprintf(stderr, "budget preflight did not reject before owned description allocation\n");
    return 11;
  }
  if (!TestUnifiedResultPreflight()) {
    std::fprintf(stderr, "result preflight did not reject before owned materialization\n");
    return 12;
  }
  TestNoexceptDiagnosticAllocationFailure();
  TestSinglePublicCompilerOwner();
  TestOwnedDescriptionAndCompleteExecution();
  TestPipelineRestrictionAndBudget();
  return 0;
}
