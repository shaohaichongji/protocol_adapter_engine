#pragma once

#include <cstddef>
#include <cstdint>

#include "complete_record_codec.h"

namespace pae::protocol_core::detail {

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
enum class DecimalArithmeticStatus {
  OK,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
  INTERNAL_ERROR,
};

[[nodiscard]] DecimalArithmeticStatus DecodeDecimal(
    const protocol_plan::LinearConversionDescriptor& conversion, std::uint64_t raw_bits,
    std::size_t byte_width, Decimal64& output) noexcept;

[[nodiscard]] DecimalArithmeticStatus EncodeDecimal(
    const protocol_plan::LinearConversionDescriptor& conversion, Decimal64 logical,
    std::size_t byte_width, std::uint64_t& raw_bits) noexcept;

[[nodiscard]] bool DecimalEqual(Decimal64 left, Decimal64 right) noexcept;
#endif

}  // namespace pae::protocol_core::detail
