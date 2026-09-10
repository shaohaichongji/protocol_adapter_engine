#include <cassert>
#include <cstdint>
#include <vector>

#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
  DocumentSession session{41U};
  const Revision revision = session.BeginLoad();
  assert(session.state() == DocumentState::LOADING);
  assert(session.ApplyCompileCompletion(
      test::CompileFixture(session.id(), revision, "synthetic_ui_v07.pae.json")));
  assert(session.state() == DocumentState::READY);
  assert(session.selection().has_value());
  assert(session.SetDraft(1U, std::uint64_t{42U}));
  assert(session.SetDraft(2U, std::vector<std::uint8_t>{0xABU}));
  assert(session.Encode());
  assert(session.state() == DocumentState::PREVIEW_VALID);
  assert(session.preview()->encoded_frame ==
         std::vector<std::uint8_t>({0xAAU, 0x00U, 0x06U, 0x2AU, 0xABU, 0x55U}));
  session.InvalidateDraft(1U);
  assert(!session.preview().has_value());
  assert(session.drafts().find(1U) == session.drafts().end());
  assert(!session.Encode());
  assert(!session.preview().has_value());
  assert(session.SetDraft(1U, std::uint64_t{42U}));
  assert(session.Encode());
  const Revision input_revision = session.input_revision();
  session.InvalidateInput();
  assert(session.input_revision() == input_revision + 1U);
  assert(!session.preview().has_value());
  assert(session.state() == DocumentState::READY);

  const Revision current = session.BeginLoad();
  assert(!session.ApplyCompileCompletion(
      test::CompileFixture(session.id(), current - 1U, "synthetic_ui_v07.pae.json")));
  assert(session.state() == DocumentState::LOADING);
  session.Close();
  assert(session.state() == DocumentState::CLOSED);
  assert(session.prepared() == nullptr);
  return 0;
}
