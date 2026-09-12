#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ascii_offline_adapter.h"

namespace {

using pae::config_compiler::CompileJsonToPlanWithUiDescription;
using pae::protocol_core::CodecStatus;
using pae::protocol_lab::ascii::AdapterStatus;
using pae::protocol_lab::ascii::ExecutionIdentity;
using pae::protocol_lab::ascii::ExecutionResult;
using pae::protocol_lab::ascii::InputField;
using pae::protocol_lab::ascii::OfflineAdapter;
using pae::protocol_lab::ascii::ReviewKind;
using pae::protocol_plan::ResourceProfile;

bool Expect(bool condition, std::string_view label) {
  if (!condition) std::cerr << "FAILED: " << label << '\n';
  return condition;
}

std::string ReadFile(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::vector<std::uint8_t> Bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()),
          reinterpret_cast<const std::uint8_t*>(value.data()) + value.size()};
}

std::string_view AsText(const std::vector<std::uint8_t>& value) {
  return {reinterpret_cast<const char*>(value.data()), value.size()};
}

std::unique_ptr<OfflineAdapter> Prepare(std::string_view config, std::string& error) {
  const std::size_t limit =
      pae::config_compiler::DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP);
  auto compiled = CompileJsonToPlanWithUiDescription(config, limit);
  if (!compiled.Succeeded()) {
    error = compiled.Diagnostic() == nullptr ? "compile failed without diagnostic"
                                             : compiled.Diagnostic()->detail;
    return nullptr;
  }
  auto artifacts = std::move(compiled).TakeArtifacts();
  if (!OfflineAdapter::Supports(artifacts)) {
    error = "compiled artifacts did not route to the ASCII adapter";
    return nullptr;
  }
  return OfflineAdapter::AdoptCompiledArtifacts(std::move(artifacts), error);
}

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
std::unique_ptr<OfflineAdapter> PrepareWithLimits(
    std::string_view config, const pae::protocol_framing::FramingLimitOverrides& limits,
    std::string& error) {
  const std::size_t limit =
      pae::config_compiler::DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP);
  auto compiled = CompileJsonToPlanWithUiDescription(config, limit);
  if (!compiled.Succeeded()) {
    error = compiled.Diagnostic() == nullptr ? "compile failed without diagnostic"
                                             : compiled.Diagnostic()->detail;
    return nullptr;
  }
  return OfflineAdapter::AdoptCompiledArtifacts(std::move(compiled).TakeArtifacts(), error, limits);
}
#endif

ExecutionIdentity InspectIdentity(std::string pipeline_id = "ascii_pipeline") {
  ExecutionIdentity identity;
  identity.document_id = 17U;
  identity.load_revision = 3U;
  identity.plan_generation = 5U;
  identity.selection_revision = 7U;
  identity.input_revision = 11U;
  identity.pipeline_index = 0U;
  identity.pipeline_id = std::move(pipeline_id);
  return identity;
}

ExecutionIdentity EncodeIdentity(std::string message_id = "greeting") {
  ExecutionIdentity identity = InspectIdentity();
  identity.message_index = 0U;
  identity.message_id = std::move(message_id);
  return identity;
}

std::string LiteralOnlyConfig(std::string_view actions) {
  return std::string{R"JSON({
"schema_version":"0.10","protocol_id":"ascii_literal_action","protocol_version":"1",
"display_name":"ASCII literal action","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","input_kind":"complete_record"}],
"pipelines":[{"id":"ascii_pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii",)JSON"} +
         std::string{actions} + R"JSON(},"fields":[]}]})JSON";
}

std::string TerminatorConflictConfig() {
  return R"JSON({
"schema_version":"0.10","protocol_id":"ascii_conflict","protocol_version":"1",
"display_name":"ASCII conflict","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST",
"resource_profile":"desktop",
"framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","input_kind":"complete_record"}],
"pipelines":[{"id":"ascii_pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],
"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","direction_id":"test",
"layout":{"kind":"text","encoding":"ascii","decode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"literal","text":"ABA"}]},"encode":{"segments":[{"kind":"field","field_id":"value"},{"kind":"literal","text":"ABA"}]}},
"fields":[{"id":"value","display_name":"Value","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:ASCII_ADAPTER_TEST","value_type":"BYTES","wire":{"codec":"ascii_text","min_byte_length":0,"max_byte_length":4,"allowed_control_bytes":"09 0A"},"encode":{"source":"input"}}]}]})JSON";
}

bool CheckDescriptionAndExecution(const char* config_path) {
  std::string error;
  auto adapter = Prepare(ReadFile(config_path), error);
  if (!Expect(adapter != nullptr, "primary ASCII artifacts adopt: " + error)) return false;
  const auto& description = adapter->Description();
  bool ok =
      Expect(description.schema_version == "0.10" && description.pipelines.size() == 1U &&
                 description.messages.size() == 1U,
             "owned top-level ASCII description") &&
      Expect(description.pipelines[0].message_indices == std::vector<std::size_t>{0U} &&
                 description.pipelines[0].decode_message_indices == std::vector<std::size_t>{0U},
             "Pipeline carries authored Messages and Decode candidates") &&
      Expect(description.messages[0].decode.has_value() &&
                 description.messages[0].encode.has_value() &&
                 description.messages[0].decode->segments.size() == 5U &&
                 description.messages[0].encode->segments.size() == 5U,
             "independent ordered RX and TX actions copied") &&
      Expect(description.messages[0].encode->referenced_field_indices ==
                 std::vector<std::size_t>({0U, 2U}),
             "TX input field order copied") &&
      Expect(description.messages[0].fields.size() == 3U &&
                 description.messages[0].display_name == "Greeting" &&
                 description.messages[0].fields[0].display_name == "Name" &&
                 description.messages[0].fields[0].min_byte_length == 1U &&
                 description.messages[0].fields[0].max_byte_length == 8U &&
                 description.messages[0].fields[1].decode_referenced &&
                 !description.messages[0].fields[1].encode_referenced &&
                 description.messages[0].fields[2].encode_referenced,
             "field bounds, action use, and author metadata copied");

  std::vector<std::uint8_t> inspect_input = Bytes("RX ALICE!OK\r\n");
  ExecutionResult inspected = adapter->Inspect(InspectIdentity(), inspect_input);
  ok &=
      Expect(
          inspected.status == AdapterStatus::OK && inspected.core_called &&
              inspected.core_status == CodecStatus::OK && inspected.message_index == 0U &&
              inspected.message_id == "greeting" && inspected.fields.size() == 2U &&
              inspected.identity.document_id == 17U && inspected.identity.plan_generation == 5U &&
              inspected.identity.input_revision == 11U && inspected.diagnostic_input_frame.empty(),
          "Inspect delegates uniqueness and Decode to Core") &&
      Expect(inspected.fields[0].field_id == "name" && inspected.fields[0].range.offset == 3U &&
                 inspected.fields[0].range.length == 5U &&
                 AsText(inspected.fields[0].bytes) == "ALICE" &&
                 inspected.fields[1].field_id == "rx_code" &&
                 inspected.fields[1].range.offset == 9U && inspected.fields[1].range.length == 2U &&
                 AsText(inspected.fields[1].bytes) == "OK",
             "Inspect copies fields and validated actual ranges");
  inspect_input.assign(1U, 0U);
  ok &= Expect(
      AsText(inspected.frame) == "RX ALICE!OK\r\n" && AsText(inspected.fields[0].bytes) == "ALICE",
      "Inspect result owns frame and field bytes after input mutation");

  std::vector<InputField> inputs{{0U, "name", Bytes("ALICE")}, {2U, "tx_tag", Bytes("Z")}};
  ExecutionResult encoded = adapter->Encode(EncodeIdentity(), inputs);
  ok &= Expect(encoded.status == AdapterStatus::OK && encoded.core_called &&
                   encoded.core_status == CodecStatus::OK &&
                   encoded.review_kind == ReviewKind::TX_TEMPLATE &&
                   AsText(encoded.frame) == "TX ALICE!Z\r\n" && encoded.fields.size() == 2U,
               "Encode reports Core success and TX template review kind") &&
        Expect(encoded.fields[0].range.offset == 3U && encoded.fields[0].range.length == 5U &&
                   encoded.fields[1].range.offset == 9U && encoded.fields[1].range.length == 1U,
               "Encode projects actual field ranges over valid output");
  inputs[0].bytes.assign(1U, 0U);
  ok &= Expect(AsText(encoded.fields[0].bytes) == "ALICE",
               "Encode result owns field bytes after input mutation");

  auto invalid_identity = adapter->Inspect(InspectIdentity("wrong_pipeline"), Bytes("RX A!OK\r\n"));
  ok &= Expect(
      invalid_identity.status == AdapterStatus::INVALID_REQUEST && !invalid_identity.core_called,
      "identity mismatch is rejected before Core");
  for (const std::string_view invalid : {"RX BOB?OK\r\n", "RX BOB!OK\r", "RX BOB!OK\r\nX"}) {
    const auto rejected_literal = adapter->Inspect(InspectIdentity(), Bytes(invalid));
    ok &= Expect(rejected_literal.status == AdapterStatus::CORE_FAILED &&
                     rejected_literal.core_status == CodecStatus::UNKNOWN_MESSAGE &&
                     rejected_literal.frame.empty() && rejected_literal.fields.empty(),
                 "literal mismatch, truncation, and trailing bytes remain Core failures");
  }
  std::vector<std::uint8_t> invalid_ascii = Bytes("RX A");
  invalid_ascii.push_back(0x80U);
  const auto suffix = Bytes("!OK\r\n");
  invalid_ascii.insert(invalid_ascii.end(), suffix.begin(), suffix.end());
  auto rejected = adapter->Inspect(InspectIdentity(), invalid_ascii);
  ok &= Expect(rejected.status == AdapterStatus::CORE_FAILED && rejected.core_called &&
                   rejected.core_status == CodecStatus::ASCII_CHARACTER_NOT_ALLOWED &&
                   rejected.frame.empty() && rejected.fields.empty() &&
                   rejected.diagnostic_input_frame == invalid_ascii,
               "Core failure preserves only labelled diagnostic Inspect input");
  return ok;
}

bool CheckLiteralOnlyAndOneWay(const char* config_path) {
  std::string error;
  auto adapter = Prepare(ReadFile(config_path), error);
  if (!Expect(adapter != nullptr, "literal-only artifacts adopt: " + error)) return false;
  const auto& description = adapter->Description();
  bool ok = Expect(description.messages[0].fields.empty() &&
                       description.messages[0].decode->referenced_field_indices.empty() &&
                       description.messages[0].encode->referenced_field_indices.empty(),
                   "literal-only description has explicit zero fields");
  auto encoded = adapter->Encode(EncodeIdentity("ping"), {});
  ok &= Expect(encoded.status == AdapterStatus::OK && AsText(encoded.frame) == "PONG\r\n" &&
                   encoded.fields.empty() && encoded.review_kind == ReviewKind::TX_TEMPLATE,
               "literal-only zero-input Encode succeeds") &&
        Expect(adapter->Inspect(InspectIdentity(), Bytes("PING\r\n")).status == AdapterStatus::OK,
               "literal-only zero-slot Inspect succeeds");
  const auto inspected = adapter->Inspect(InspectIdentity(), Bytes("PING\r\n"));
  ok &= Expect(inspected.fields.empty() && inspected.frame.size() == 6U,
               "literal-only success explicitly owns zero fields");

  auto decode_only = Prepare(
      LiteralOnlyConfig(R"JSON("decode":{"segments":[{"kind":"literal","text":"PING\r\n"}]})JSON"),
      error);
  ok &= Expect(decode_only != nullptr, "Decode-only literal artifacts adopt");
  if (decode_only != nullptr) {
    auto unsupported = decode_only->Encode(EncodeIdentity("message"), {});
    ok &= Expect(unsupported.status == AdapterStatus::CORE_FAILED && unsupported.core_called &&
                     unsupported.core_status == CodecStatus::OPERATION_NOT_SUPPORTED &&
                     unsupported.review_kind == ReviewKind::NOT_APPLICABLE &&
                     unsupported.frame.empty(),
                 "missing Encode action returns Core OPERATION_NOT_SUPPORTED");
  }
  auto encode_only = Prepare(
      LiteralOnlyConfig(R"JSON("encode":{"segments":[{"kind":"literal","text":"PONG\r\n"}]})JSON"),
      error);
  ok &= Expect(encode_only != nullptr, "Encode-only literal artifacts adopt");
  if (encode_only != nullptr) {
    auto unsupported = encode_only->Inspect(InspectIdentity(), Bytes("PING\r\n"));
    ok &= Expect(unsupported.status == AdapterStatus::CORE_FAILED && unsupported.core_called &&
                     unsupported.core_status == CodecStatus::OPERATION_NOT_SUPPORTED &&
                     unsupported.fields.empty(),
                 "Pipeline without Decode action returns Core OPERATION_NOT_SUPPORTED");
  }
  return ok;
}

bool CheckFailureStatusAndOwnership() {
  std::string error;
  auto adapter = Prepare(TerminatorConflictConfig(), error);
  if (!Expect(adapter != nullptr, "terminator conflict artifacts adopt: " + error)) return false;
  bool ok = Expect(adapter->Description().messages[0].fields[0].allowed_control_bytes ==
                       std::vector<std::uint8_t>({0x09U, 0x0AU}),
                   "explicit control-byte constraints copied from the Frozen Plan");
  std::vector<InputField> inputs{{0U, "value", Bytes("AB")}};
  auto conflict = adapter->Encode(EncodeIdentity("message"), inputs);
  ok &= Expect(conflict.status == AdapterStatus::CORE_FAILED && conflict.core_called &&
                   conflict.core_status == CodecStatus::ASCII_TERMINATOR_CONFLICT &&
                   conflict.failed_field_index == 0U && conflict.failed_field_id == "value" &&
                   conflict.frame.empty() && conflict.fields.empty() &&
                   pae::protocol_lab::ascii::CodecStatusName(conflict.core_status) ==
                       "ASCII_TERMINATOR_CONFLICT",
               "Core terminator conflict and available field identity are preserved");

  inputs[0].bytes.clear();
  const auto empty_encoded = adapter->Encode(EncodeIdentity("message"), inputs);
  const auto empty_inspected = adapter->Inspect(InspectIdentity(), Bytes("ABA"));
  ok &= Expect(
      empty_encoded.status == AdapterStatus::OK && AsText(empty_encoded.frame) == "ABA" &&
          empty_encoded.fields.size() == 1U && empty_encoded.fields[0].range.offset == 0U &&
          empty_encoded.fields[0].range.length == 0U &&
          empty_inspected.status == AdapterStatus::OK && empty_inspected.fields.size() == 1U &&
          empty_inspected.fields[0].range.offset == 0U &&
          empty_inspected.fields[0].range.length == 0U,
      "zero-length field keeps an explicit empty actual range");

  ExecutionResult owned;
  {
    auto temporary = Prepare(TerminatorConflictConfig(), error);
    inputs[0].bytes = Bytes("A");
    owned = temporary->Encode(EncodeIdentity("message"), inputs);
  }
  ok &= Expect(owned.status == AdapterStatus::OK && AsText(owned.frame) == "AABA" &&
                   owned.fields.size() == 1U && AsText(owned.fields[0].bytes) == "A",
               "execution result remains owned after adapter destruction");
  return ok;
}

bool CheckLegacyIsolation(const char* config_path) {
  const std::size_t limit =
      pae::config_compiler::DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP);
  auto compiled = CompileJsonToPlanWithUiDescription(ReadFile(config_path), limit);
  if (!Expect(compiled.Succeeded(), "legacy Binary UI artifacts still compile")) return false;
  if (!Expect(compiled.Artifacts() != nullptr && !OfflineAdapter::Supports(*compiled.Artifacts()),
              "schema dispatch leaves legacy Binary artifacts on the old path")) {
    return false;
  }
  std::string error;
  auto adapter = OfflineAdapter::AdoptCompiledArtifacts(std::move(compiled).TakeArtifacts(), error);
  return Expect(adapter == nullptr && error.find("supported ASCII") != std::string::npos,
                "ASCII adapter rejects legacy Binary artifacts for old-path dispatch");
}

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
bool SameEngineState(const pae::protocol_lab::ascii::StreamObservation& left,
                     const pae::protocol_lab::ascii::StreamObservation& right) {
  return left.phase == right.phase && left.buffered_bytes == right.buffered_bytes &&
         left.has_internal_work == right.has_internal_work &&
         left.frozen_input_bytes == right.frozen_input_bytes &&
         left.frozen_cursor == right.frozen_cursor &&
         left.total_discarded_bytes == right.total_discarded_bytes &&
         left.total_malformed_candidates == right.total_malformed_candidates &&
         left.total_candidates == right.total_candidates &&
         left.total_decode_successes == right.total_decode_successes;
}

bool CheckStreamObserver(const char* config_path) {
  std::string error;
  auto adapter = Prepare(ReadFile(config_path), error);
  if (!Expect(adapter != nullptr, "Schema 0.11 stream artifacts adopt: " + error)) return false;
  bool ok = Expect(adapter->Description().schema_version == "0.11" &&
                       adapter->Description().pipelines.size() == 3U &&
                       adapter->Description().pipelines[0].stream_ascii_crlf &&
                       adapter->Description().pipelines[0].maximum_frame_length == 12U &&
                       !adapter->Description().pipelines[1].stream_ascii_crlf,
                   "description distinguishes stream and complete-record Pipelines");
  const auto initial = adapter->ObserveStream(0U);
  ok &= Expect(initial.has_value() && initial->buffered_bytes == 0U &&
                   adapter->StreamChunkCapacity(0U) ==
                       (std::min)(std::size_t{65536U}, initial->effective_max_submit_bytes),
               "chunk capacity is min(65536, effective max_submit_bytes)");

  auto first = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("RX A!"));
  ok &= Expect(first.status == AdapterStatus::OK && first.push_called &&
                   first.framing.frames_delivered == 0U && !first.candidate.has_value() &&
                   first.after.buffered_bytes == 5U && first.after.frozen_input_bytes == 0U,
               "half-frame Submit consumes a new chunk without inventing a candidate");
  const auto half_frame = *adapter->ObserveStream(0U);
  std::vector<std::uint8_t> oversized(adapter->StreamChunkCapacity(0U) + 1U, 'X');
  auto rejected = adapter->SubmitStreamChunk(InspectIdentity(), oversized);
  ok &= Expect(rejected.status == AdapterStatus::INVALID_REQUEST && !rejected.push_called &&
                   SameEngineState(half_frame, *adapter->ObserveStream(0U)),
               "capacity rejection preserves the half-frame and all counters");

  auto candidate = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("OK\r\nONLY\r\n"));
  ok &= Expect(
      candidate.status == AdapterStatus::OK && candidate.push_called &&
          candidate.framing.stop_reason == pae::protocol_framing::SubmitStopReason::SINK_STOP &&
          candidate.framing.frames_delivered == 1U && candidate.candidate.has_value() &&
          candidate.candidate->status == AdapterStatus::OK &&
          AsText(candidate.candidate->frame) == "RX A!OK\r\n" &&
          candidate.after.frozen_input_bytes == 10U && candidate.after.frozen_cursor == 4U,
      "candidate STOP freezes only the unconsumed suffix");
  auto continued = adapter->ContinueStream(InspectIdentity());
  ok &= Expect(continued.status == AdapterStatus::OK && continued.push_called &&
                   continued.framing.frames_delivered == 1U && continued.candidate.has_value() &&
                   continued.candidate->status == AdapterStatus::OK &&
                   AsText(continued.candidate->frame) == "ONLY\r\n" &&
                   continued.after.frozen_input_bytes == 0U &&
                   continued.after.total_candidates == 2U &&
                   continued.after.total_decode_successes == 2U,
               "Continue submits the frozen suffix once and owns the second result");
  auto nothing = adapter->ContinueStream(InspectIdentity());
  ok &= Expect(nothing.status == AdapterStatus::INVALID_REQUEST && !nothing.push_called,
               "Continue without suffix or internal work is rejected before Push");

  auto split_cr = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("RX B!OK\r"));
  auto split_lf = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("\n"));
  ok &= Expect(split_cr.status == AdapterStatus::OK && !split_cr.candidate.has_value() &&
                   split_lf.candidate.has_value() &&
                   AsText(split_lf.candidate->frame) == "RX B!OK\r\n",
               "CRLF split across Submit chunks yields one candidate");

  auto decode_failure = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("BAD\r\n"));
  ok &= Expect(decode_failure.status == AdapterStatus::OK && decode_failure.candidate.has_value() &&
                   decode_failure.candidate->status == AdapterStatus::CORE_FAILED &&
                   decode_failure.after.total_candidates == 4U &&
                   decode_failure.after.total_decode_successes == 3U,
               "Decode failure remains a delivered candidate and does not become protocol success");

  auto overlong = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("1234567890123"));
  ok &= Expect(overlong.status == AdapterStatus::OK &&
                   overlong.after.phase ==
                       pae::protocol_framing::StreamFramingPhase::DISCARDING_UNTIL_CRLF &&
                   overlong.after.total_malformed_candidates == 1U,
               "overlong candidate enters bounded discard state");
  ok &= Expect(adapter->StreamHasDiscardableState(0U), "discard state requires confirmation");
  const auto generation = overlong.after.generation;
  ok &= Expect(adapter->ResetStream(0U, "ascii_pipeline", error), "explicit stream Reset succeeds");
  const auto reset = *adapter->ObserveStream(0U);
  ok &= Expect(reset.generation == generation + 1U && reset.step_sequence == 0U &&
                   reset.total_candidates == 0U && reset.total_decode_successes == 0U &&
                   reset.total_discarded_bytes == 0U && reset.total_malformed_candidates == 0U &&
                   reset.buffered_bytes == 0U && !adapter->StreamHasDiscardableState(0U),
               "Reset clears workspace, frozen input, results counters, and increments generation");

  auto complete_encode_identity = InspectIdentity("encode_only_pipeline");
  complete_encode_identity.pipeline_index = 1U;
  complete_encode_identity.message_index = 2U;
  complete_encode_identity.message_id = "encode_only";
  const auto complete_encoded = adapter->Encode(complete_encode_identity, {});
  auto complete_inspect_identity = InspectIdentity("decode_only_pipeline");
  complete_inspect_identity.pipeline_index = 2U;
  const auto complete_inspected = adapter->Inspect(complete_inspect_identity, Bytes("ONLY\r\n"));
  ok &= Expect(complete_encoded.status == AdapterStatus::OK &&
                   AsText(complete_encoded.frame) == "SEND\r\n" &&
                   complete_inspected.status == AdapterStatus::OK &&
                   complete_inspected.message_id == "decode_only",
               "Schema 0.11 complete-record Encode and Inspect remain on the normal adapter path");

  auto second = Prepare(ReadFile(config_path), error);
  auto tab_one = adapter->SubmitStreamChunk(InspectIdentity(), Bytes("RX C!"));
  ok &= Expect(second != nullptr && second->ObserveStream(0U)->buffered_bytes == 0U &&
                   tab_one.after.buffered_bytes != second->ObserveStream(0U)->buffered_bytes,
               "two adapters isolate stream workspace and counters like two document tabs");

  pae::protocol_framing::FramingLimitOverrides low_work;
  low_work.max_submit_bytes = 8U;
  low_work.max_work_units = 6U;
  auto budgeted = PrepareWithLimits(ReadFile(config_path), low_work, error);
  const auto budgeted_capacity = budgeted == nullptr ? 0U : budgeted->StreamChunkCapacity(0U);
  ok &= Expect(budgeted != nullptr && budgeted_capacity == 8U,
               "effective override drives the Lab chunk capacity; actual=" +
                   std::to_string(budgeted_capacity) + "; error=" + error);
  if (budgeted != nullptr) {
    auto step = budgeted->SubmitStreamChunk(InspectIdentity(), Bytes("RX A!"));
    ok &= Expect(
        step.framing.stop_reason == pae::protocol_framing::SubmitStopReason::WORK_BUDGET_REACHED &&
            step.framing.bytes_consumed == 3U && step.after.frozen_cursor == 3U,
        "work budget freezes a strict suffix after actual bytes_consumed");
    step = budgeted->ContinueStream(InspectIdentity());
    ok &= Expect(step.framing.bytes_consumed == 2U && step.after.frozen_input_bytes == 0U,
                 "Continue consumes only the final frozen suffix without replay");
  }

  auto copy_failure = Prepare(ReadFile(config_path), error);
  if (copy_failure != nullptr) {
    copy_failure->SetCandidateCopyLimitForTesting(0U, 4U);
    const auto failed_copy =
        copy_failure->SubmitStreamChunk(InspectIdentity(), Bytes("RX A!OK\r\n"));
    ok &= Expect(failed_copy.status == AdapterStatus::MATERIALIZATION_FAILED &&
                     failed_copy.push_called && failed_copy.framing.frames_delivered == 1U &&
                     failed_copy.framing.bytes_consumed == 9U && failed_copy.after.reset_required &&
                     !failed_copy.candidate.has_value(),
                 "candidate copy failure preserves step facts and requires explicit Reset");
  } else {
    ok &= Expect(false, "copy-failure adapter adopts: " + error);
  }
  return ok;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  const int expected_argc =
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
      5;
#else
      4;
#endif
  if (argc != expected_argc) {
    std::cerr << "usage: ascii_offline_adapter_tests <ascii-config> <literal-config> "
                 "<legacy-binary-config> [ascii-stream-config]\n";
    return 2;
  }
  bool ok = CheckDescriptionAndExecution(argv[1]) && CheckLiteralOnlyAndOneWay(argv[2]) &&
            CheckFailureStatusAndOwnership() && CheckLegacyIsolation(argv[3]);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  ok = ok && CheckStreamObserver(argv[4]);
#endif
  return ok ? 0 : 1;
}
