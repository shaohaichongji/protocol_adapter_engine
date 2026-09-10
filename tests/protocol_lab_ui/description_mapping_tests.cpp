#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "../../tools/protocol_lab_ui/description_mapping.h"
#include "test_support.h"

namespace {

const pae::protocol_lab_ui::FieldDescriptor& FindField(
    const pae::protocol_lab_ui::DocumentDescription& description, std::string_view id) {
  for (const auto& message : description.messages) {
    for (const auto& field : message.fields) {
      if (field.id == id) return field;
    }
  }
  assert(false);
  return description.messages.front().fields.front();
}

void ExpectMasks(const pae::protocol_lab_ui::FieldDescriptor& field,
                 std::initializer_list<pae::protocol_lab_ui::PhysicalBitMask> expected) {
  assert(field.physical_bits.size() == expected.size());
  std::size_t index = 0U;
  for (const auto& item : expected) {
    assert(field.physical_bits[index].frame_byte_index == item.frame_byte_index);
    assert(field.physical_bits[index].uint8_mask == item.uint8_mask);
    ++index;
  }
}

pae::protocol_lab_ui::DocumentDescription Build(const char* fixture) {
  auto completion = pae::protocol_lab_ui::test::CompileFixture(1U, 1U, fixture);
  assert(completion->artifacts != nullptr);
  pae::protocol_lab_ui::DocumentDescription description;
  std::string error;
  assert(pae::protocol_lab_ui::BuildDocumentDescription(
      *completion->artifacts->Plan(), completion->artifacts->Description(), description, error));
  assert(error.empty());
  return description;
}

}  // namespace

int main() {
  const auto v05 = Build("synthetic_ui_v05.pae.json");
  assert(v05.schema_version == "0.5");
  assert(v05.display_name == "Public UI typed fields");
  ExpectMasks(FindField(v05, "enabled"), {{1U, 0x01U}});
  ExpectMasks(FindField(v05, "mode"), {{1U, 0x06U}});
  ExpectMasks(FindField(v05, "cross_bits"), {{0U, 0x0FU}, {1U, 0xF0U}});
  ExpectMasks(FindField(v05, "msb_bits"), {{2U, 0xC0U}, {3U, 0x7FU}});
  const std::optional<pae::protocol_lab_ui::ByteRange> expected_count_range{
      pae::protocol_lab_ui::ByteRange{4U, 1U}};
  assert(FindField(v05, "count").byte_range == expected_count_range);
  assert(FindField(v05, "marker").read_only_annotation == "constant; read-only");

  const auto v06 = Build("synthetic_ui_v06.pae.json");
  assert(v06.messages.front().integrity_storage.has_value());
  assert(v06.messages.front().integrity_storage->offset == 9U);
  assert(v06.messages.front().integrity_storage->length == 2U);

  const auto v07 = Build("synthetic_ui_v07.pae.json");
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
  assert(v07.messages.front().computed_length_storage.has_value());
  assert(v07.messages.front().computed_length_storage->offset == 1U);
  assert(v07.messages.front().computed_length_storage->length == 2U);
#endif
  assert(FindField(v07, "record_length").read_only_annotation == "computed length; read-only");
  return 0;
}
