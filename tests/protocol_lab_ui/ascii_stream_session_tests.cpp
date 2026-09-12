#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include "../../tools/protocol_lab_ui/document_session.h"

namespace {

std::string Read(const wchar_t* path) {
  std::ifstream input(std::filesystem::path{path}, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::unique_ptr<pae::protocol_lab_ui::CompileCompletion> Compile(
    pae::protocol_lab_ui::DocumentSession& session) {
  auto compiled = pae::config_compiler::CompileJsonToPlanWithUiDescription(
      Read(PAE_ASCII_STREAM_CONFIG), pae::config_compiler::DerivedUiDescriptionMemoryLimit(
                                         pae::protocol_plan::ResourceProfile::DESKTOP));
  auto completion = std::make_unique<pae::protocol_lab_ui::CompileCompletion>();
  completion->document_id = session.id();
  completion->load_revision = session.load_revision();
  completion->config_sha256 = "ascii-stream-session-test";
  assert(compiled.Succeeded());
  completion->artifacts = std::make_unique<pae::config_compiler::CompiledUiArtifacts>(
      std::move(compiled).TakeArtifacts());
  return completion;
}

}  // namespace

int main() {
  using namespace pae::protocol_lab_ui;
  DocumentSession first{901U};
  first.BeginLoad();
  assert(first.ApplyCompileCompletion(Compile(first)));
  assert(first.IsAsciiDocument() && first.StreamInspectAvailable() && !first.InspectAvailable());
  assert(first.SetMode(OperationMode::STREAM_INSPECT));
  assert(first.SetRepresentation(ByteRepresentation::ASCII_ESCAPED));
  assert(first.StreamChunkBudget() > 12U);

  assert(first.SetInspectDraftUtf16(u"RX A!"));
  assert(first.SubmitStream());
  assert(!first.inspect_result().has_value());
  const auto half = first.StreamObservation();
  assert(half.has_value() && half->buffered_bytes == 5U && !first.StreamContinueAvailable());
  const auto half_step = half->step_sequence;
  assert(!first.SubmitStream());
  assert(first.diagnostic_id() == "UI_STREAM_CHUNK_ALREADY_SUBMITTED");
  assert(first.StreamObservation()->step_sequence == half_step &&
         first.StreamObservation()->buffered_bytes == half->buffered_bytes);

  assert(first.SetInspectDraftUtf16(u"RX A!OK\\q"));
  assert(!first.SubmitStream());
  assert(first.inspect_failure().has_value() && first.inspect_failure()->input_offset == 7U &&
         first.inspect_failure()->input_offset_is_utf16 &&
         first.inspect_failure()->detail ==
             "unknown ASCII escape at input_utf16_code_unit_offset=7 (zero-based)");
  assert(first.StreamObservation()->step_sequence == half_step &&
         first.StreamObservation()->buffered_bytes == half->buffered_bytes);
  assert(first.SetInspectDraftUtf16(u"OK\\r\\nONLY\\r\\n"));
  assert(first.SubmitStream());
  assert(first.inspect_result().has_value() && first.inspect_result()->message_id == "greeting");
  assert(first.StreamContinueAvailable());
  assert(first.ContinueStream());
  assert(first.inspect_result().has_value() && first.inspect_result()->message_id == "decode_only");
  assert(!first.StreamContinueAvailable());

  assert(first.SetInspectDraftUtf16(u"BAD\\r\\n"));
  assert(first.SubmitStream());
  assert(first.inspect_failure().has_value() &&
         first.inspect_failure()->stage == InspectFailureStage::STRUCTURAL_QUERY);
  assert(first.stream_step().has_value() && first.stream_step()->after.total_candidates == 3U &&
         first.stream_step()->after.total_decode_successes == 2U);
  const auto failed_candidate_step = first.StreamObservation()->step_sequence;
  assert(!first.SubmitStream());
  assert(first.diagnostic_id() == "UI_STREAM_CHUNK_ALREADY_SUBMITTED" &&
         first.StreamObservation()->step_sequence == failed_candidate_step &&
         first.StreamObservation()->total_candidates == 3U);

  assert(first.SetInspectDraftUtf16(u"1234567890123"));
  assert(first.SubmitStream() && first.StreamHasDiscardableState());
  const auto generation = first.StreamObservation()->generation;
  assert(first.ResetStream());
  assert(first.StreamObservation()->generation == generation + 1U &&
         first.StreamObservation()->total_candidates == 0U && !first.StreamHasDiscardableState());

  DocumentSession second{902U};
  second.BeginLoad();
  assert(second.ApplyCompileCompletion(Compile(second)));
  assert(second.StreamObservation()->buffered_bytes == 0U);
  assert(first.SetInspectDraftUtf16(u"RX B!"));
  assert(first.SubmitStream());
  assert(first.StreamObservation()->buffered_bytes == 5U &&
         second.StreamObservation()->buffered_bytes == 0U);

  assert(first.SetRepresentation(ByteRepresentation::HEX));
  assert(first.StreamObservation()->buffered_bytes == 5U);
  assert(!first.inspect_result().has_value() && !first.inspect_failure().has_value());
  return 0;
}
