#include <pae/codec.h>

#include <type_traits>

static_assert(!std::is_copy_constructible_v<pae::CompleteRecordCodec>);
static_assert(std::is_nothrow_move_constructible_v<pae::CompleteRecordCodec>);

int HeaderCodecCompiles() {
  const auto value = pae::EncodeValue::Int64(pae::FieldSelector{0U, 0U}, -1);
  static_cast<void>(value);
  return 0;
}
