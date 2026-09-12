#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

namespace {

std::string Read(const wchar_t* path) {
  std::ifstream input(std::filesystem::path{path}, std::ios::binary);
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

std::unique_ptr<pae::protocol_lab_ui::CompileCompletion> Compile(
    pae::protocol_lab_ui::DocumentSession& session, std::string text) {
  auto compiled = pae::config_compiler::CompileJsonToPlanWithUiDescription(
      text, pae::config_compiler::DerivedUiDescriptionMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  auto completion = std::make_unique<pae::protocol_lab_ui::CompileCompletion>();
  completion->document_id = session.id();
  completion->load_revision = session.load_revision();
  completion->config_sha256 = "ascii-session-test";
  if (compiled.Succeeded()) {
    completion->artifacts = std::make_unique<pae::config_compiler::CompiledUiArtifacts>(
        std::move(compiled).TakeArtifacts());
  } else if (compiled.Diagnostic() != nullptr) {
    completion->diagnostic = *compiled.Diagnostic();
  }
  return completion;
}

std::string OneWayConfig(const char* action) {
  return std::string{
             R"JSON({"schema_version":"0.10","protocol_id":"one_way","protocol_version":"1","display_name":"One way","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:UI_ASCII_TEST","resource_profile":"desktop","framing_profiles":[{"id":"record","display_name":"Record","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:UI_ASCII_TEST","input_kind":"complete_record"}],"pipelines":[{"id":"pipeline","display_name":"Pipeline","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:UI_ASCII_TEST","direction_id":"test","input_framing_profile_id":"record","message_ids":["message"]}],"messages":[{"id":"message","display_name":"Message","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:UI_ASCII_TEST","direction_id":"test","layout":{"kind":"text","encoding":"ascii",)JSON"} +
         action + R"JSON(},"fields":[]}]})JSON";
}

}  // namespace

int main() {
  using namespace pae::protocol_lab_ui;
  DocumentSession session{801U};
  session.BeginLoad();
  assert(session.ApplyCompileCompletion(Compile(session, Read(PAE_ASCII_TEXT_CONFIG))));
  assert(session.IsAsciiDocument() && session.EncodeAvailable() && session.InspectAvailable());
  assert(session.SetDraft(0U, std::vector<std::uint8_t>{'A', 'L', 'I', 'C', 'E'}));
  assert(session.SetDraft(2U, std::vector<std::uint8_t>{'Z'}));
  assert(session.Encode());
  assert(session.preview()->encoded_frame ==
         std::vector<std::uint8_t>({'T', 'X', ' ', 'A', 'L', 'I', 'C', 'E', '!', 'Z', '\r', '\n'}));
  assert(session.preview()->fields[0].actual_range == (ByteRange{3U, 5U}));
  assert(session.preview()->tx_template_review);
  assert(session.SetRepresentation(ByteRepresentation::ASCII_ESCAPED));
  assert(!session.preview().has_value());
  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.SetInspectDraftUtf16(u"RX ALICE!OK\\r\\n"));
  assert(session.Inspect());
  assert(session.inspect_result()->message_id == "greeting");
  assert(session.inspect_result()->fields[0].actual_range == (ByteRange{3U, 5U}));
  assert(session.SetRepresentation(ByteRepresentation::HEX));
  assert(session.inspect_draft() == "525820414C494345214F4B0D0A");
  assert(!session.inspect_result().has_value() && session.Inspect());
  assert(session.SetRepresentation(ByteRepresentation::ASCII_ESCAPED));
  assert(session.SetInspectDraftUtf16(u"RX A\n!OK\\r\\n"));
  assert(!session.Inspect());
  assert(session.inspect_failure()->diagnostic_id == "UI_ASCII_INPUT_INVALID");
  assert(session.inspect_failure()->input_offset == 4U &&
         session.inspect_failure()->input_offset_is_utf16);
  assert(session.inspect_failure()->detail.find("input_utf16_code_unit_offset=4 (zero-based)") !=
         std::string::npos);

  assert(session.SetInspectDraftUtf16(u"RX ALICE!OK\\q"));
  assert(!session.Inspect());
  assert(session.inspect_failure()->input_offset == 11U &&
         session.inspect_failure()->input_offset_is_utf16);
  assert(session.inspect_failure()->detail ==
         "unknown ASCII escape at input_utf16_code_unit_offset=11 (zero-based)");

  DocumentSession literal{802U};
  literal.BeginLoad();
  assert(literal.ApplyCompileCompletion(Compile(literal, Read(PAE_ASCII_LITERAL_CONFIG))));
  assert(literal.Encode() && literal.preview()->zero_field_success &&
         literal.preview()->fields.empty());
  assert(literal.SetMode(OperationMode::INSPECT));
  assert(literal.SetRepresentation(ByteRepresentation::ASCII_ESCAPED));
  assert(literal.SetInspectDraftUtf16(u"PING\\r\\n"));
  assert(literal.Inspect() && literal.inspect_result()->zero_field_success);

  DocumentSession decode_only{803U};
  decode_only.BeginLoad();
  assert(decode_only.ApplyCompileCompletion(Compile(
      decode_only,
      OneWayConfig(
          "\"decode\":{\"segments\":[{\"kind\":\"literal\",\"text\":\"PING\\\\r\\\\n\"}]}"))));
  assert(!decode_only.EncodeAvailable() && decode_only.InspectAvailable());
  assert(!decode_only.Encode());
  assert(decode_only.diagnostic_id() == "OPERATION_NOT_SUPPORTED");

  DocumentSession encode_only{804U};
  encode_only.BeginLoad();
  assert(encode_only.ApplyCompileCompletion(Compile(
      encode_only,
      OneWayConfig(
          "\"encode\":{\"segments\":[{\"kind\":\"literal\",\"text\":\"PONG\\\\r\\\\n\"}]}"))));
  assert(encode_only.EncodeAvailable() && !encode_only.InspectAvailable());
  assert(encode_only.SetMode(OperationMode::INSPECT));
  assert(encode_only.SetInspectDraft("50494E470D0A"));
  assert(!encode_only.Inspect());
  assert(encode_only.inspect_failure()->status == "OPERATION_NOT_SUPPORTED");
  return 0;
}
