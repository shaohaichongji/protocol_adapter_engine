#include <cassert>
#include <cstdint>
#include <vector>

#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
  DocumentSession crc{100U};
  DocumentSession length{101U};
  assert(crc.ApplyCompileCompletion(
      test::CompileFixture(crc.id(), crc.BeginLoad(), "synthetic_ui_v06.pae.json")));
  assert(length.ApplyCompileCompletion(
      test::CompileFixture(length.id(), length.BeginLoad(), "synthetic_ui_v07.pae.json")));

  assert(crc.SetDraft(0U, std::vector<std::uint8_t>{'1', '2', '3', '4', '5', '6', '7', '8', '9'}));
  assert(length.SetDraft(1U, std::uint64_t{7U}));
  assert(length.SetDraft(2U, std::vector<std::uint8_t>{0xCCU}));
  assert(crc.Encode());
  assert(length.Encode());
  assert(crc.preview()->encoded_frame == std::vector<std::uint8_t>(
                                             {'1', '2', '3', '4', '5', '6', '7', '8', '9',
                                              0x29U, 0xB1U, 0xAAU}));
  const auto length_frame = length.preview()->encoded_frame;
  crc.InvalidateInput();
  assert(!crc.preview().has_value());
  assert(length.preview().has_value());
  assert(length.preview()->encoded_frame == length_frame);
  assert(crc.plan_generation() == 1U && length.plan_generation() == 1U);
  assert(crc.id() != length.id());
  return 0;
}
