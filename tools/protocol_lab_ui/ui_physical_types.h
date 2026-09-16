#pragma once

#include <cstddef>
#include <cstdint>

namespace pae::protocol_lab_ui {

struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t uint8_mask = 0U;
};

struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};

struct ByteLengthBounds {
  std::size_t minimum = 0U;
  std::size_t maximum = 0U;
};

inline bool operator==(const ByteLengthBounds& left, const ByteLengthBounds& right) noexcept {
  return left.minimum == right.minimum && left.maximum == right.maximum;
}

inline bool operator==(const ByteRange& left, const ByteRange& right) noexcept {
  return left.offset == right.offset && left.length == right.length;
}

}  // namespace pae::protocol_lab_ui
