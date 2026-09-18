#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

#include "../../tools/protocol_lab_ui/document_session.h"

void Check(bool condition) {
  if (!condition) {
    std::cerr << "host session assertion failed\n";
    std::exit(EXIT_FAILURE);
  }
}

namespace ui = pae::protocol_lab_ui;
namespace ascii = pae::protocol_lab::ascii;
namespace host = pae::host_endpoint;
std::string Text() {
  std::ifstream input(std::filesystem::path{PAE_ASCII_STREAM_CONFIG}, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, {}};
}
pae::config_compiler::CompiledUiArtifacts Artifacts() {
  auto result = pae::config_compiler::CompileJsonToPlanWithUiDescription(
      Text(), pae::config_compiler::DerivedUiDescriptionMemoryLimit(
                  pae::protocol_plan::ResourceProfile::DESKTOP));
  Check(result.Succeeded());
  return std::move(result).TakeArtifacts();
}
void Load(ui::DocumentSession& session) {
  auto completion = std::make_unique<ui::CompileCompletion>();
  completion->document_id = session.id();
  completion->load_revision = session.BeginLoad();
  completion->artifacts = std::make_unique<pae::config_compiler::CompiledUiArtifacts>(Artifacts());
  Check(session.ApplyCompileCompletion(std::move(completion)));
}
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
std::unique_ptr<ui::AsciiHostAdapter> Adapter() {
  std::string error;
  auto result = ui::AsciiHostAdapter::CreatePrivate(
      Artifacts(), {{"device", host::Action::DECODE, 0}, {"device", host::Action::ENCODE, 0}},
      error);
  Check(result && error.empty());
  return result;
}
#else
std::unique_ptr<ascii::HostObserverAdapter> Adapter() {
  std::string error;
  auto result = ascii::HostObserverAdapter::Create(
      Artifacts(), {{"device", host::Action::DECODE, 0}, {"device", host::Action::ENCODE, 0}},
      error);
  Check(result && error.empty());
  return result;
}
#endif
int main() {
  ui::DocumentSession first{1}, second{2};
  Load(first);
  Load(second);
  Check(first.ApplyHostAdapter(Adapter()) && second.ApplyHostAdapter(Adapter()));
  Check(second.SetInspectDraftUtf16(u"4F 4E"));
  Check(second.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  Check(second.inspect_draft_utf16() == u"ON");
  Check(second.SetInspectDraftUtf16(u"ON\\q"));
  Check(!second.SetRepresentation(ui::ByteRepresentation::HEX));
  Check(second.representation() == ui::ByteRepresentation::ASCII_ESCAPED &&
        second.inspect_draft_utf16() == u"ON\\q");
  Check(second.diagnostic_detail().find("ASCII (escaped) -> Hex") != std::string::npos);
  Check(second.SelectHostFlow(0, 1) && second.representation() == ui::ByteRepresentation::HEX);
  Check(second.SelectHostFlow(0, 0) &&
        second.representation() == ui::ByteRepresentation::ASCII_ESCAPED &&
        second.inspect_draft_utf16() == u"ON\\q");
  Check(second.SetInspectDraftUtf16(u"ON"));
  Check(second.SetRepresentation(ui::ByteRepresentation::HEX) &&
        second.inspect_draft_utf16() == u"4F4E" && second.diagnostic_detail().empty());
  Check(first.prepared()->ascii_adapter == nullptr && first.HostActive());
  Check(first.mode() == ui::OperationMode::STREAM_INSPECT);
  Check(first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  Check(first.SetInspectDraftUtf16(u"RX A!") && first.SubmitStream());
  Check(first.StreamObservation()->buffered_bytes == 5);
  Check(first.SelectHostFlow(0, 1));
  Check(first.SetInspectDraftUtf16(u"ON"));
  Check(!first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  Check(first.representation() == ui::ByteRepresentation::HEX &&
        first.inspect_draft_utf16() == u"ON" && first.StreamObservation()->buffered_bytes == 0);
  Check(first.diagnostic_detail().find("Hex -> ASCII (escaped)") != std::string::npos);
  Check(first.diagnostic_detail().find("clear") != std::string::npos);
  Check(first.SetInspectDraftUtf16(u""));
  Check(first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  Check(first.SetInspectDraftUtf16(u"ON") && first.SubmitStream());
  Check(first.SelectHostFlow(1, 0));
  Check(first.mode() == ui::OperationMode::ENCODE && first.StreamHasDiscardableState());
  Check(first.SetDraft(0, std::vector<std::uint8_t>{'A'}));
  Check(first.SetDraft(2, std::vector<std::uint8_t>{'Z'}));
  Check(first.Encode());
  const auto frame = first.preview()->encoded_frame;
  Check(first.SelectHostFlow(0, 0));
  Check(first.inspect_draft_utf16() == u"RX A!" && !first.SubmitStream());
  Check(first.StreamObservation()->buffered_bytes == 5);
  Check(first.SetInspectDraftUtf16(u"OK\\r\\nBAD\\r\\nONLY\\r\\n") && first.SubmitStream());
  Check(first.inspect_result() && first.inspect_result()->fields[0].logical_value == "A");
  Check(first.ContinueStream() && first.inspect_failure() && !first.inspect_result());
  Check(first.ContinueStream() && first.inspect_result()->zero_field_success);
  Check(first.ResetStream() && first.StreamObservation()->generation == 1);
  Check(first.SelectHostFlow(0, 1) && first.StreamObservation()->buffered_bytes == 2);
  Check(first.SetInspectDraftUtf16(u"LY\\r\\n") && first.SubmitStream());
  Check(first.inspect_result()->zero_field_success);
  Check(first.SelectHostFlow(1, 0) && first.preview()->encoded_frame == frame);
  Check(!first.ApplyHostAdapter(nullptr) && first.preview()->encoded_frame == frame);
  Check(second.StreamObservation()->buffered_bytes == 0);
  Check(first.SelectHostFlow(0, 0));
  Check(first.SetInspectDraftUtf16(u"RX B!") && first.SubmitStream());
  Check(first.ApplyHostAdapter(Adapter()) && first.StreamObservation()->buffered_bytes == 0);
  first.Close();
  Check(!first.HostActive());
  Check(second.HostActive());
}
