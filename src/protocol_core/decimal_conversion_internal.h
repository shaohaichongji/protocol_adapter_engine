#pragma once

#include <cstddef>
#include <cstdint>

#include "complete_record_codec.h"

namespace pae::protocol_core::detail {

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
// 私有整数转换层：Decimal64 的数学值为 coefficient * 10^(-scale)，scale 是小数位数。
// 冻结描述把转换比例和偏置写成 A/10^t、B/10^t；不使用 double 近似或舍入。
// 调用方提供已验证描述、1..8 字节整数宽度及按字节序读取的 raw_bits，本层不读 Frame。
enum class DecimalArithmeticStatus {
  OK,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
  INTERNAL_ERROR,
};

// logical = (raw * A + B) / 10^t，先消除小数尾零再检查 int64_t 系数范围。
// 输出仅在 OK 时有效；Codec 层负责状态映射、字段定位和整次失败不交付。
[[nodiscard]] DecimalArithmeticStatus DecodeDecimal(
    const protocol_plan::LinearConversionDescriptor& conversion, std::uint64_t raw_bits,
    std::size_t byte_width, Decimal64& output) noexcept;

// 精确反算 raw = (logical - bias) / conversion_scale；非整数或超 Wire 范围均拒绝。
// raw_bits 只在成功末尾写入，负 raw 以实际 byte_width 的补码位形交给调用方写字节序。
[[nodiscard]] DecimalArithmeticStatus EncodeDecimal(
    const protocol_plan::LinearConversionDescriptor& conversion, Decimal64 logical,
    std::size_t byte_width, std::uint64_t& raw_bits) noexcept;

// 比较规范化后的数学值，而非原始成员；任一输入 scale 非法时返回 false。
[[nodiscard]] bool DecimalEqual(Decimal64 left, Decimal64 right) noexcept;
#endif

}  // namespace pae::protocol_core::detail
