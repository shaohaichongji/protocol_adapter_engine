#include <cassert>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
  const auto compile_failure = [](DocumentId document, Revision revision, std::string pointer,
                                  std::optional<std::size_t> offset, std::string detail) {
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = document;
    completion->load_revision = revision;
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
    completion->route = SchemaDispatchStatus::LEGACY_PUBLIC;
    pae::CompileDiagnostic source;
    source.stage = pae::CompileStage::DOMAIN_VALIDATION;
    source.code = pae::CompileError::UNKNOWN_REFERENCE;
    source.json_pointer = std::move(pointer);
    source.byte_offset = offset;
    source.detail = std::move(detail);
    source.resource_kind = pae::ResourceKind::NONE;
    source.resource_profile = pae::ResourceProfile::DESKTOP;
    completion->public_diagnostic = source;
    completion->structured_compile_diagnostic = ProjectCompileDiagnostic(source);
#elif !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    pae::config_compiler::CompileDiagnostic source;
    source.stage = pae::config_compiler::CompileStage::DOMAIN_VALIDATION;
    source.code = pae::config_compiler::CompileError::UNKNOWN_REFERENCE;
    source.json_pointer = std::move(pointer);
    source.byte_offset = offset;
    source.detail = std::move(detail);
    source.resource_kind = pae::config_compiler::ResourceKind::NONE;
    source.resource_profile = pae::protocol_plan::ResourceProfile::DESKTOP;
    completion->diagnostic = source;
    completion->structured_compile_diagnostic = ProjectCompileDiagnostic(source);
#else
#error "document_state structured diagnostic test requires a compiled route"
#endif
    return completion;
  };
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
  assert(!session.compile_diagnostic());

  assert(!session.ApplyCompileCompletion(
      compile_failure(session.id(), current, "", std::nullopt, "current compile detail")));
  assert(session.state() == DocumentState::CONFIG_ERROR);
  assert(session.compile_diagnostic());
  assert(session.compile_diagnostic()->json_pointer.empty());
  assert(!session.compile_diagnostic()->byte_offset);
  assert(session.compile_diagnostic()->detail == "current compile detail");

  DocumentSession other{42U};
  const Revision other_revision = other.BeginLoad();
  assert(!other.ApplyCompileCompletion(
      compile_failure(other.id(), other_revision, "/other", 0U, "other compile detail")));
  assert(other.compile_diagnostic() && other.compile_diagnostic()->json_pointer == "/other" &&
         other.compile_diagnostic()->byte_offset && *other.compile_diagnostic()->byte_offset == 0U);
  assert(session.compile_diagnostic()->detail == "current compile detail");

  const Revision replacement = session.BeginLoad();
  assert(!session.compile_diagnostic());
  assert(!session.ApplyCompileCompletion(
      compile_failure(session.id(), replacement - 1U, "/stale", 9U, "stale detail")));
  assert(session.state() == DocumentState::LOADING);
  assert(!session.compile_diagnostic());

  auto ordinary_failure = std::make_unique<CompileCompletion>();
  ordinary_failure->document_id = session.id();
  ordinary_failure->load_revision = replacement;
  ordinary_failure->diagnostic =
      CompileCompletion::Diagnostic{"bounded scheduler rejected request"};
  assert(!session.ApplyCompileCompletion(std::move(ordinary_failure)));
  assert(session.state() == DocumentState::CONFIG_ERROR);
  assert(!session.compile_diagnostic());

  const Revision success_revision = session.BeginLoad();
  assert(session.ApplyCompileCompletion(
      test::CompileFixture(session.id(), success_revision, "synthetic_ui_v07.pae.json")));
  assert(session.state() == DocumentState::READY);
  assert(!session.compile_diagnostic());
  session.Close();
  assert(session.state() == DocumentState::CLOSED);
  assert(session.prepared() == nullptr);
  assert(!session.compile_diagnostic());
  other.Close();
  assert(!other.compile_diagnostic());
  return 0;
}
