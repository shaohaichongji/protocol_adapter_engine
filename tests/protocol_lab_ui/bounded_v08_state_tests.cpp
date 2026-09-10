#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

namespace {

using pae::protocol_lab_ui::ByteRange;
using pae::protocol_lab_ui::DocumentSession;
using pae::protocol_lab_ui::DocumentState;
using pae::protocol_lab_ui::InspectFailureStage;
using pae::protocol_lab_ui::OperationMode;

const pae::protocol_lab_ui::MessageDescriptor& Load(DocumentSession& session) {
  const auto revision = session.BeginLoad();
  assert(session.ApplyCompileCompletion(pae::protocol_lab_ui::test::CompileFixture(
      session.id(), revision, "synthetic_ui_v08.pae.json")));
  assert(session.description() != nullptr && session.description()->schema_version == "0.8");
  assert(session.selection().has_value());
  return session.description()->messages[session.selection()->message_index];
}

void TestCopiedBoundsAndActualLayout() {
  DocumentSession session{80U};
  const auto& message = Load(session);
  assert(message.frame_size == 6U);
  assert(message.bounded_payload.has_value());
  assert(message.bounded_payload->header_length == 2U);
  assert(message.bounded_payload->min_payload_length == 0U);
  assert(message.bounded_payload->max_payload_length == 3U);
  assert(message.bounded_payload->trailer_length == 1U);
  assert(message.integrity_storage_at_payload_end);
  const auto& payload = message.fields[1];
  assert(payload.byte_length_bounds == std::optional<pae::protocol_lab_ui::ByteLengthBounds>(
                                           pae::protocol_lab_ui::ByteLengthBounds{0U, 3U}));
  assert(pae::protocol_lab_ui::ResolveActualFieldRange(message, payload, 3U) ==
         std::optional<ByteRange>(ByteRange{2U, 0U}));
  assert(pae::protocol_lab_ui::ResolveActualFieldRange(message, payload, 5U) ==
         std::optional<ByteRange>(ByteRange{2U, 2U}));
  assert(pae::protocol_lab_ui::ResolveActualFieldRange(message, payload, 6U) ==
         std::optional<ByteRange>(ByteRange{2U, 3U}));
  assert(!pae::protocol_lab_ui::ResolveActualFieldRange(message, payload, 2U).has_value());
  assert(pae::protocol_lab_ui::ResolveActualIntegrityStorage(message, 3U) ==
         std::optional<ByteRange>(ByteRange{2U, 1U}));
  assert(pae::protocol_lab_ui::ResolveActualIntegrityStorage(message, 5U) ==
         std::optional<ByteRange>(ByteRange{4U, 1U}));
  assert(pae::protocol_lab_ui::FormatPhysicalLocation(message, payload, std::nullopt)
             .find("payload length 0..3 bytes; current range unavailable") != std::string::npos);
}

void TestEncodeBoundsAndRecovery() {
  DocumentSession session{81U};
  Load(session);
  assert(session.SetDraft(1U, std::vector<std::uint8_t>{}));
  assert(session.Encode());
  assert(session.state() == DocumentState::PREVIEW_VALID);
  assert(session.preview()->encoded_frame == std::vector<std::uint8_t>({0xA5U, 0x03U, 0xA8U}));
  assert(session.preview()->fields[0].raw_value == "3");
  assert(session.preview()->fields[1].raw_value.empty());

  assert(session.SetDraft(1U, std::vector<std::uint8_t>{0x00U, 0xFFU, 0x01U}));
  assert(session.Encode());
  assert(session.preview()->encoded_frame ==
         std::vector<std::uint8_t>({0xA5U, 0x06U, 0x00U, 0xFFU, 0x01U, 0xABU}));

  assert(!session.SetDraft(1U, std::vector<std::uint8_t>{1U, 2U, 3U, 4U}));
  assert(session.SetInvalidDraft(1U, "01020304", "Payload length must be 0..3 bytes"));
  assert(!session.preview().has_value());
  assert(!session.Encode());
  assert(session.diagnostic_id() == "UI_INPUT_INVALID");
  assert(session.SetDraft(1U, std::vector<std::uint8_t>{0x10U, 0x20U}));
  assert(session.Encode());
  assert(session.preview()->encoded_frame ==
         std::vector<std::uint8_t>({0xA5U, 0x05U, 0x10U, 0x20U, 0xDAU}));
}

void TestInspectDynamicTrailerAndFailureRecovery() {
  DocumentSession session{82U};
  Load(session);
  assert(session.SetMode(OperationMode::INSPECT));
  assert(session.InspectFrameBudget() == 6U);
  assert(session.SetInspectDraft("A5 03 A8"));
  assert(session.Inspect());
  assert(session.inspect_result()->fields[0].raw_value == "3");
  assert(session.inspect_result()->fields[1].raw_value.empty());

  assert(session.SetInspectDraft("A5 05 10 20 DB"));
  assert(!session.Inspect());
  assert(!session.inspect_result().has_value());
  assert(session.inspect_failure().has_value());
  assert(session.inspect_failure()->stage == InspectFailureStage::CODEC);
  assert(session.inspect_failure()->status == "INTEGRITY_FAILED");

  assert(session.SetInspectDraft("A5 05 10 20 DA"));
  assert(session.Inspect());
  assert(session.inspect_result()->input_frame.size() == 5U);
  assert(session.inspect_result()->fields[1].raw_value == "1020");
}

}  // namespace

int main() {
  TestCopiedBoundsAndActualLayout();
  TestEncodeBoundsAndRecovery();
  TestInspectDynamicTrailerAndFailureRecovery();
  return 0;
}
