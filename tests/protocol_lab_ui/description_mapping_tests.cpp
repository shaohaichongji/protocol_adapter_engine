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
  assert(pae::protocol_lab_ui::FormatPhysicalLocation(FindField(v05, "enabled")) ==
         "byte[1] mask=0x01 bits={0} global_bits={8}");
  ExpectMasks(FindField(v05, "mode"), {{1U, 0x06U}});
  ExpectMasks(FindField(v05, "cross_bits"), {{0U, 0x0FU}, {1U, 0xF0U}});
  assert(pae::protocol_lab_ui::FormatPhysicalLocation(FindField(v05, "cross_bits")) ==
         "byte[0] mask=0x0F bits={0,1,2,3} global_bits={0,1,2,3}; "
         "byte[1] mask=0xF0 bits={4,5,6,7} global_bits={12,13,14,15}");
  ExpectMasks(FindField(v05, "msb_bits"), {{2U, 0xC0U}, {3U, 0x7FU}});
  const std::optional<pae::protocol_lab_ui::ByteRange> expected_count_range{
      pae::protocol_lab_ui::ByteRange{4U, 1U}};
  assert(FindField(v05, "count").byte_range == expected_count_range);
  assert(pae::protocol_lab_ui::FormatPhysicalLocation(FindField(v05, "count")) ==
         "byte[4] mask=0xFF bits={0,1,2,3,4,5,6,7} "
         "global_bits={32,33,34,35,36,37,38,39}");
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

  const auto v08 = Build("synthetic_ui_v08.pae.json");
  assert(v08.schema_version == "0.8");
  assert(v08.messages.front().bounded_payload.has_value());
  assert(v08.messages.front().integrity_range_ends_at_payload);
  assert(v08.messages.front().integrity_storage_at_payload_end);
  assert(FindField(v08, "payload").byte_length_bounds ==
         std::optional<pae::protocol_lab_ui::ByteLengthBounds>(
             pae::protocol_lab_ui::ByteLengthBounds{0U, 3U}));
  return 0;
}
