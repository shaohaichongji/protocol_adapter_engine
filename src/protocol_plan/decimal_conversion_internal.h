#pragma once

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)

#include <cstdint>

#include "plan_types.h"

namespace pae::protocol_plan::detail {

enum class DecimalDescriptorError {
  NONE,
  ZERO_SCALE,
  DENOMINATOR_OUT_OF_RANGE,
  NON_TERMINATING_SCALE,
  NON_TERMINATING_BIAS,
  ARITHMETIC_OVERFLOW,
};

DecimalDescriptorError DeriveLinearConversion(ValueType raw_value_type,
                                              std::int64_t scale_numerator,
                                              std::uint64_t scale_denominator,
                                              std::int64_t bias_numerator,
                                              std::uint64_t bias_denominator,
                                              LinearConversionDescriptor& output) noexcept;

bool LinearConversionEqual(const LinearConversionDescriptor& left,
                           const LinearConversionDescriptor& right) noexcept;

}  // namespace pae::protocol_plan::detail

#endif
