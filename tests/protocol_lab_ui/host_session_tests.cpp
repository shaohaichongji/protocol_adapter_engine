#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

#include "../../tools/protocol_lab_ui/document_session.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) && \
    !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
#include "../../tools/protocol_lab_ui/ascii_host_adapter_compat.h"
#endif

void CheckAt(bool condition, int line) {
  if (!condition) {
    std::cerr << "host session assertion failed at line " << line << "\n";
    std::exit(EXIT_FAILURE);
  }
}
#define Check(condition) CheckAt((condition), __LINE__)

namespace ui = pae::protocol_lab_ui;
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
namespace ascii = pae::protocol_lab::ascii;
namespace host = pae::host_endpoint;
#endif
std::string Text() {
  std::ifstream input(std::filesystem::path{PAE_ASCII_STREAM_CONFIG}, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, {}};
}
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
pae::config_compiler::CompiledUiArtifacts Artifacts() {
  auto result = pae::config_compiler::CompileJsonToPlanWithUiDescription(
      Text(), pae::config_compiler::DerivedUiDescriptionMemoryLimit(
                  pae::protocol_plan::ResourceProfile::DESKTOP));
  Check(result.Succeeded());
  return std::move(result).TakeArtifacts();
}
#endif
void Load(ui::DocumentSession& session) {
  auto completion = std::make_unique<ui::CompileCompletion>();
  completion->document_id = session.id();
  completion->load_revision = session.BeginLoad();
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  auto compiled = pae::CompileProtocolJson(Text());
  Check(compiled.Succeeded());
  completion->route = ui::SchemaDispatchStatus::ASCII_PUBLIC;
  completion->compiler_attempt_count = 1U;
  completion->public_compiled =
      std::make_unique<pae::CompiledProtocol>(std::move(compiled).TakeCompiled());
#else
  completion->artifacts = std::make_unique<pae::config_compiler::CompiledUiArtifacts>(Artifacts());
#endif
  Check(session.ApplyCompileCompletion(std::move(completion)));
}
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
std::unique_ptr<ui::AsciiHostAdapter> Adapter() {
  std::string error;
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  auto compiled = pae::CompileProtocolJson(Text());
  Check(compiled.Succeeded());
  auto result = ui::AsciiHostAdapter::CreatePublic(
      std::move(compiled).TakeCompiled(),
      {{"device", ui::AsciiHostAction::DECODE, 0},
       {"device", ui::AsciiHostAction::ENCODE, 0},
       {"record", ui::AsciiHostAction::DECODE, 2},
       {"record", ui::AsciiHostAction::ENCODE, 1}},
      0U, error);
#else
  auto result = ui::CreatePrivateAsciiHostAdapter(
      Artifacts(),
      {{"device", ui::AsciiHostAction::DECODE, 0},
       {"device", ui::AsciiHostAction::ENCODE, 0}},
      error);
#endif
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

#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  Check(first.SelectPipeline(2U) && first.SelectMessage(1U));
  Check(!first.StreamInspectAvailable() && !first.StreamObservation().has_value() &&
        !first.StreamContinueAvailable() && !first.ResetStream());
  Check(first.InspectAvailable());
  Check(first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  Check(first.SetInspectDraftUtf16(u"ONLY\\r\\n") && first.Inspect());
  Check(first.inspect_result() && first.inspect_result()->message_id == "decode_only" &&
        first.inspect_result()->zero_field_success);

  Check(first.SelectPipeline(1U) && first.SelectMessage(2U));
  Check(!first.StreamInspectAvailable() && !first.StreamObservation().has_value() &&
        !first.StreamContinueAvailable() && !first.ResetStream());
  Check(!first.InspectAvailable() && first.EncodeAvailable());
#endif

  Check(first.ApplyHostAdapter(Adapter()) && second.ApplyHostAdapter(Adapter()));
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  Check(first.SelectHostFlow(2U, 0U));
  Check(!first.StreamInspectAvailable() && !first.StreamObservation().has_value() &&
        !first.StreamContinueAvailable() && !first.ResetStream());
  Check(first.InspectAvailable());
  Check(first.SetRepresentation(ui::ByteRepresentation::ASCII_ESCAPED));
  Check(first.SetInspectDraftUtf16(u"ONLY\\r\\n") && first.Inspect());
  Check(first.inspect_result() && first.inspect_result()->message_id == "decode_only" &&
        first.inspect_result()->zero_field_success);
  Check(first.SelectHostFlow(3U, 0U));
  Check(!first.StreamInspectAvailable() && !first.StreamObservation().has_value() &&
        !first.StreamContinueAvailable() && !first.ResetStream());
  Check(!first.InspectAvailable() && first.EncodeAvailable());
  Check(first.SelectHostFlow(0U, 0U));
#endif
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
  const auto generation = first.StreamObservation()->generation;
  Check(first.ResetStream() && first.StreamObservation()->generation == generation + 1U);
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
