#include <cassert>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>

#include "../../tools/protocol_lab_ui/canonical_input.h"
#include "../../tools/protocol_lab_ui/owned_presentation_types.h"

namespace ui = pae::protocol_lab_ui;

int main() {
  static_assert(std::is_nothrow_move_constructible_v<ui::DocumentDescription>);
  static_assert(std::is_nothrow_move_assignable_v<ui::DocumentDescription>);

  std::uint64_t unsigned_value = 0U;
  assert(ui::ParseCanonicalUint64("0", unsigned_value) && unsigned_value == 0U);
  assert(ui::ParseCanonicalUint64("18446744073709551615", unsigned_value) &&
         unsigned_value == (std::numeric_limits<std::uint64_t>::max)());
  for (const std::string invalid : {"", "00", "+1", "-1", " 1", "1 ",
                                    "18446744073709551616"}) {
    assert(!ui::ParseCanonicalUint64(invalid, unsigned_value));
  }

  std::int64_t signed_value = 0;
  assert(ui::ParseCanonicalInt64("0", signed_value) && signed_value == 0);
  assert(ui::ParseCanonicalInt64("9223372036854775807", signed_value) &&
         signed_value == (std::numeric_limits<std::int64_t>::max)());
  assert(ui::ParseCanonicalInt64("-9223372036854775808", signed_value) &&
         signed_value == (std::numeric_limits<std::int64_t>::min)());
  for (const std::string invalid : {"", "00", "-0", "+1", "--1", " 1", "1 ",
                                    "9223372036854775808", "-9223372036854775809"}) {
    assert(!ui::ParseCanonicalInt64(invalid, signed_value));
  }

  assert((ui::NormalizeDecimal64({1200, 3}) == ui::Decimal64{12, 1}));
  assert((ui::NormalizeDecimal64({-1200, 3}) == ui::Decimal64{-12, 1}));
  assert((ui::NormalizeDecimal64({0, 18}) == ui::Decimal64{}));
  assert((ui::NormalizeDecimal64({123, 0}) == ui::Decimal64{123, 0}));

  ui::TypedDraft decimal = ui::Decimal64{-9223372036854775807LL, 18};
  assert(std::get<ui::Decimal64>(decimal).coefficient == -9223372036854775807LL);
  assert(std::get<ui::Decimal64>(decimal).scale == 18);

  ui::FieldDescriptor field;
  field.id = "count";
  field.description = "Unsigned input";
  field.source_ref = "SYNTHETIC:owned-types#count";
  field.value_type = ui::FieldValueType::UINT64;
  field.encode_source = ui::FieldEncodeSource::INPUT;
  field.byte_range = ui::ByteRange{4U, 1U};
  assert(field.id == "count" && field.description == "Unsigned input" &&
         field.source_ref == "SYNTHETIC:owned-types#count" &&
         field.value_type == ui::FieldValueType::UINT64 &&
         field.encode_source == ui::FieldEncodeSource::INPUT &&
         (field.byte_range == ui::ByteRange{4U, 1U}));

  field.decimal_conversion = true;
  assert(field.decimal_conversion);
  field.read_only_annotation = "constant; read-only";
  field.encode_source = ui::FieldEncodeSource::CONSTANT;
  assert(field.read_only_annotation == "constant; read-only" &&
         field.encode_source == ui::FieldEncodeSource::CONSTANT);
  return 0;
}
