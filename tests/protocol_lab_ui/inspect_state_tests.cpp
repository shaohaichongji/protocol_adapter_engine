#include <cassert>
#include <string>
#include <vector>

#include "../../tools/protocol_lab_ui/document_session.h"
#include "../../tools/protocol_lab_ui/inspect_hex_input.h"
#include "test_support.h"

namespace {

class CountingObserver final : public pae::protocol_lab::v06::ExecutionObserver {
 public:
  void PhaseStarted(pae::protocol_lab::v06::ExecutionPhase) override { ++started; }
  void PhaseFinished(pae::protocol_lab::v06::ExecutionPhase, std::string_view) override {
    ++finished;
  }

  std::size_t started = 0U;
  std::size_t finished = 0U;
};

class StaleInspectObserver final : public pae::protocol_lab::v06::ExecutionObserver {
 public:
  explicit StaleInspectObserver(pae::protocol_lab_ui::DocumentSession& session)
      : session_(session) {}

  void PhaseStarted(pae::protocol_lab::v06::ExecutionPhase phase) override {
    if (!mutated_ && phase == pae::protocol_lab::v06::ExecutionPhase::STRUCTURAL_QUERY) {
      mutated_ = true;
      assert(session_.SetInspectDraft("AA:00"));
    }
  }
  void PhaseFinished(pae::protocol_lab::v06::ExecutionPhase, std::string_view) override {}

 private:
  pae::protocol_lab_ui::DocumentSession& session_;
  bool mutated_ = false;
};

void TestHexParser() {
  using namespace pae::protocol_lab_ui;
  auto parsed = ParseInspectHex("aA 01\t02\r\nFf", 4U);
  assert(parsed.ok());
  assert(parsed.bytes == std::vector<std::uint8_t>({0xAAU, 0x01U, 0x02U, 0xFFU}));
  assert(parsed.canonical_hex == "AA0102FF");

  parsed = ParseInspectHex("", 4U);
  assert(parsed.error == InspectHexError::EMPTY_INPUT && parsed.input_offset == 0U);
  parsed = ParseInspectHex(" \t\r\n", 4U);
  assert(parsed.error == InspectHexError::EMPTY_INPUT && parsed.input_offset == 0U);
  parsed = ParseInspectHex("AA:BB", 4U);
  assert(parsed.error == InspectHexError::INVALID_CHARACTER && parsed.input_offset == 2U);
  parsed = ParseInspectHex("0xAA", 4U);
  assert(parsed.error == InspectHexError::INVALID_CHARACTER && parsed.input_offset == 1U);
  parsed = ParseInspectHex("AA B", 4U);
  assert(parsed.error == InspectHexError::ODD_NIBBLE_COUNT && parsed.input_offset == 3U);
  parsed = ParseInspectHex(std::string("AA") + "\xC2\xA0" + "BB", 4U);
  assert(parsed.error == InspectHexError::INVALID_CHARACTER && parsed.input_offset == 2U);
  parsed = ParseInspectHex("AA BB CC DD  ", 4U);
  assert(parsed.error == InspectHexError::TEXT_LIMIT_EXCEEDED && parsed.input_offset == 12U);
  parsed = ParseInspectHex("AABBCC", 2U);
  assert(parsed.error == InspectHexError::FRAME_LIMIT_EXCEEDED && parsed.input_offset == 4U);
  parsed = ParseInspectHex("AA", 0U);
  assert(parsed.error == InspectHexError::INVALID_BUDGET);
  parsed = ParseInspectHex(std::string(196609U, ' '), 65536U);
  assert(parsed.error == InspectHexError::TEXT_LIMIT_EXCEEDED && parsed.input_offset == 196608U);
}

void TestSessionInspect() {
  using namespace pae::protocol_lab_ui;
  DocumentSession session{73U};
  const Revision load = session.BeginLoad();
  assert(session.ApplyCompileCompletion(
      test::CompileFixture(session.id(), load, "synthetic_ui_v07.pae.json")));
  assert(session.InspectFrameBudget() == 6U);
  assert(session.SetDraft(1U, std::uint64_t{42U}));
  assert(session.SetDraft(2U, std::vector<std::uint8_t>{0xABU}));
  const auto encode_drafts = session.drafts();
  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.SetInspectDraft("aa 00 06 2a ab 55"));
  assert(session.Inspect());
  assert(session.inspect_result().has_value());
  assert(session.inspect_result()->message_id == "length_record");
  assert(session.inspect_result()->input_frame ==
         std::vector<std::uint8_t>({0xAAU, 0x00U, 0x06U, 0x2AU, 0xABU, 0x55U}));
  assert(session.inspect_result()->fields.size() == 3U);
  assert(session.drafts().size() == encode_drafts.size());

  const std::string inspect_draft = session.inspect_draft();
  assert(session.SetMode(OperationMode::ENCODE));
  assert(session.inspect_draft() == inspect_draft);
  assert(session.Encode());
  assert(session.preview().has_value());
  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.inspect_result().has_value());

  assert(session.SetInspectDraft("AA:00"));
  assert(!session.inspect_result().has_value());
  CountingObserver lexical_observer;
  assert(!session.Inspect(&lexical_observer));
  assert(lexical_observer.started == 0U && lexical_observer.finished == 0U);
  assert(session.inspect_failure().has_value());
  assert(session.inspect_failure()->stage == InspectFailureStage::INPUT);
  assert(session.inspect_failure()->input_offset == 2U);
  assert(!session.inspect_result().has_value());

  assert(session.SetInspectDraft(std::string(19U, ' ')));
  CountingObserver text_budget_observer;
  assert(!session.Inspect(&text_budget_observer));
  assert(text_budget_observer.started == 0U && text_budget_observer.finished == 0U);
  assert(session.inspect_failure()->diagnostic_id == "UI_INSPECT_HEX_TEXT_LIMIT_EXCEEDED");

  assert(session.SetInspectDraft("00000000000000"));
  CountingObserver frame_budget_observer;
  assert(!session.Inspect(&frame_budget_observer));
  assert(frame_budget_observer.started == 0U && frame_budget_observer.finished == 0U);
  assert(session.inspect_failure()->diagnostic_id == "UI_INSPECT_FRAME_LIMIT_EXCEEDED");

  assert(session.SetInspectDraft("AA 00 05 2A AB 55"));
  assert(!session.Inspect());
  assert(session.inspect_failure().has_value());
  assert(session.inspect_failure()->stage == InspectFailureStage::CODEC);
  assert(session.inspect_failure()->status == "LENGTH_MISMATCH");
  assert(session.inspect_failure()->message_id == "length_record");
  assert(session.inspect_failure()->failed_field_id == "record_length");
  assert(session.inspect_failure()->failed_field_index == 0U);
  assert(!session.inspect_result().has_value());
  assert(!session.diagnostic_id().empty());

  const auto pipeline_revision = session.pipeline_selection_revision();
  assert(session.SelectPipeline(0U));
  assert(session.pipeline_selection_revision() == pipeline_revision + 1U);
  assert(session.inspect_draft().empty());
  assert(session.drafts().empty());
  assert(!session.inspect_failure().has_value());
  assert(session.diagnostic_id().empty() && session.diagnostic_detail().empty());

  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.SetInspectDraft("AA 00 06 2A AB 55"));
  StaleInspectObserver stale_observer{session};
  assert(!session.Inspect(&stale_observer));
  assert(session.inspect_draft() == "AA:00");
  assert(!session.inspect_result().has_value() && !session.inspect_failure().has_value());

  assert(session.SetInspectDraft("AA 00 06 2A AB 55"));
  assert(session.Inspect());
  const Revision reload = session.BeginLoad();
  assert(session.state() == DocumentState::LOADING);
  assert(session.inspect_draft().empty());
  assert(!session.inspect_result().has_value() && !session.inspect_failure().has_value());
  assert(session.ApplyCompileCompletion(
      test::CompileFixture(session.id(), reload, "synthetic_ui_v07.pae.json")));
  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.SetInspectDraft("AA:00"));
  assert(!session.Inspect());
  session.Close();
  assert(session.inspect_draft().empty());
  assert(!session.inspect_result().has_value() && !session.inspect_failure().has_value());
  assert(session.diagnostic_id().empty() && session.diagnostic_detail().empty());
}

void TestMessageSelectorCannotBypassMatch() {
  using namespace pae::protocol_lab_ui;
  DocumentSession session{74U};
  const Revision load = session.BeginLoad();
  assert(session.ApplyCompileCompletion(
      test::CompileFixture(session.id(), load, "synthetic_ui_inspect_multi_v05.pae.json")));
  assert(session.selection().has_value());
  assert(session.selection()->message_id == "message_a");
  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.SetInspectDraft("B2 07"));
  assert(session.Inspect());
  assert(session.inspect_result().has_value());
  assert(session.inspect_result()->message_id == "message_b");
}

void TestAmbiguousStructuralFailureMapping() {
  using namespace pae::protocol_lab_ui;
  const auto failure = MakeStructuralInspectFailure("AMBIGUOUS_MESSAGE", {0x00U, 0x00U});
  assert(failure.stage == InspectFailureStage::STRUCTURAL_QUERY);
  assert(failure.status == "AMBIGUOUS_MESSAGE");
  assert(failure.diagnostic_id == "UI_INSPECT_AMBIGUOUS_MESSAGE");
  assert(failure.input_frame == std::vector<std::uint8_t>({0x00U, 0x00U}));
  assert(!failure.message_id.has_value());
  assert(!failure.failed_field_id.has_value());
}

}  // namespace

int main() {
  TestHexParser();
  TestSessionInspect();
  TestMessageSelectorCannotBypassMatch();
  TestAmbiguousStructuralFailureMapping();
  return 0;
}
