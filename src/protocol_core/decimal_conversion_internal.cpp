#include "decimal_conversion_internal.h"

#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)

#include <algorithm>
#include <array>
#include <limits>

namespace pae::protocol_core::detail {
namespace {

// 固定 8 个 32 位 limb，低位在前；是 256 位无符号幅值，不是宿主或 Wire 字节序。
// SignedWide 单独保存符号，避免负数移位和有符号溢出；局部定容对象不申请堆内存。
struct Wide {
  std::array<std::uint32_t, protocol_plan::kDecimalCoefficientWordCount> words{};
};

struct SignedWide {
  Wide magnitude;
  bool negative = false;
};

Wide From64(std::uint64_t value) noexcept {
  Wide result;
  result.words[0] = static_cast<std::uint32_t>(value);
  result.words[1] = static_cast<std::uint32_t>(value >> 32U);
  return result;
}

int Compare(const Wide& left, const Wide& right) noexcept {
  for (std::size_t index = left.words.size(); index-- > 0U;) {
    if (left.words[index] != right.words[index]) {
      return left.words[index] < right.words[index] ? -1 : 1;
    }
  }
  return 0;
}

bool IsZero(const Wide& value) noexcept { return Compare(value, Wide{}) == 0; }

bool Add(const Wide& left, const Wide& right, Wide& output) noexcept {
  // 逐 limb 加法在 uint64_t 中保留进位；最高进位非零时拒绝，不交付截断结果。
  Wide result;
  std::uint64_t carry = 0U;
  for (std::size_t index = 0U; index < result.words.size(); ++index) {
    const std::uint64_t sum = std::uint64_t{left.words[index]} + right.words[index] + carry;
    result.words[index] = static_cast<std::uint32_t>(sum);
    carry = sum >> 32U;
  }
  if (carry != 0U) return false;
  output = result;
  return true;
}

Wide Subtract(const Wide& left, const Wide& right) noexcept {
  // 按模 2^256 传播借位：符号加法先比较幅值；长除法另携带移出最高位的标志。
  Wide result;
  std::uint64_t borrow = 0U;
  for (std::size_t index = 0U; index < result.words.size(); ++index) {
    const std::uint64_t subtrahend = std::uint64_t{right.words[index]} + borrow;
    result.words[index] = static_cast<std::uint32_t>(std::uint64_t{left.words[index]} - subtrahend);
    borrow = std::uint64_t{left.words[index]} < subtrahend ? 1U : 0U;
  }
  return result;
}

bool Multiply(const Wide& left, const Wide& right, Wide& output) noexcept {
  // 先形成双宽乘积，再检查高半部；32x32 乘积加 limb 和进位仍容纳于 uint64_t。
  // 编译描述的接受域有既定中间上界；突破固定容量是内部异常，不是用户值舍入。
  std::array<std::uint32_t, protocol_plan::kDecimalCoefficientWordCount * 2U> product{};
  for (std::size_t left_index = 0U; left_index < left.words.size(); ++left_index) {
    std::uint64_t carry = 0U;
    for (std::size_t right_index = 0U; right_index < right.words.size(); ++right_index) {
      const std::size_t target = left_index + right_index;
      const std::uint64_t value = std::uint64_t{left.words[left_index]} * right.words[right_index] +
                                  product[target] + carry;
      product[target] = static_cast<std::uint32_t>(value);
      carry = value >> 32U;
    }
    product[left_index + left.words.size()] = static_cast<std::uint32_t>(carry);
  }
  for (std::size_t index = left.words.size(); index < product.size(); ++index) {
    if (product[index] != 0U) return false;
  }
  for (std::size_t index = 0U; index < left.words.size(); ++index) {
    output.words[index] = product[index];
  }
  return true;
}

bool Divide(const Wide& numerator, const Wide& denominator, Wide& quotient,
            Wide& remainder) noexcept {
  if (IsZero(denominator)) return false;
  // 无符号二进制长除法固定遍历 256 位，交付商与余数；符号由上层单独处理。
  // high_bit 保存余数左移的第 257 位，避免截掉它后错误地跳过本轮减法。
  Wide result_quotient;
  Wide result_remainder;
  for (int bit = 255; bit >= 0; --bit) {
    const bool high_bit = (result_remainder.words.back() >> 31U) != 0U;
    std::uint32_t carry = (numerator.words[static_cast<std::size_t>(bit) / 32U] >>
                           (static_cast<unsigned>(bit) % 32U)) &
                          1U;
    for (std::size_t index = 0U; index < result_remainder.words.size(); ++index) {
      const std::uint32_t next = result_remainder.words[index] >> 31U;
      result_remainder.words[index] = (result_remainder.words[index] << 1U) | carry;
      carry = next;
    }
    if (high_bit || Compare(result_remainder, denominator) >= 0) {
      result_remainder = Subtract(result_remainder, denominator);
      result_quotient.words[static_cast<std::size_t>(bit) / 32U] |=
          std::uint32_t{1U} << (static_cast<unsigned>(bit) % 32U);
    }
  }
  quotient = result_quotient;
  remainder = result_remainder;
  return true;
}

bool To64(const Wide& value, std::uint64_t& output) noexcept {
  // 收窄前检查所有高 limb，而不是静默取低 64 位。
  for (std::size_t index = 2U; index < value.words.size(); ++index) {
    if (value.words[index] != 0U) return false;
  }
  output = (std::uint64_t{value.words[1]} << 32U) | value.words[0];
  return true;
}

SignedWide Canonical(SignedWide value) noexcept {
  // 幅值为零统一为正零，符号操作不会留下可观察的负零。
  if (IsZero(value.magnitude)) value.negative = false;
  return value;
}

std::uint64_t Magnitude(std::int64_t value) noexcept {
  // 在无符号域做模减法取得幅值，不能直接对 INT64_MIN 取负或求绝对值。
  return value < 0 ? std::uint64_t{0U} - static_cast<std::uint64_t>(value)
                   : static_cast<std::uint64_t>(value);
}

SignedWide FromSigned(std::int64_t value) noexcept {
  return SignedWide{From64(Magnitude(value)), value < 0};
}

SignedWide FromDescriptor(const protocol_plan::SignedCoefficient256& value) noexcept {
  return Canonical(SignedWide{Wide{value.words}, value.negative});
}

bool SignedAdd(SignedWide left, SignedWide right, SignedWide& output) noexcept {
  // 同号相加、异号用大幅值减小幅值；先在宽整数域抵消，不提前按 int64_t 拒绝。
  SignedWide result;
  if (left.negative == right.negative) {
    result.negative = left.negative;
    if (!Add(left.magnitude, right.magnitude, result.magnitude)) return false;
  } else if (Compare(left.magnitude, right.magnitude) >= 0) {
    result = SignedWide{Subtract(left.magnitude, right.magnitude), left.negative};
  } else {
    result = SignedWide{Subtract(right.magnitude, left.magnitude), right.negative};
  }
  output = Canonical(result);
  return true;
}

bool SignedMultiply(SignedWide left, SignedWide right, SignedWide& output) noexcept {
  SignedWide result;
  if (!Multiply(left.magnitude, right.magnitude, result.magnitude)) return false;
  result.negative = left.negative != right.negative;
  output = Canonical(result);
  return true;
}

bool ToSigned(SignedWide value, std::int64_t& output) noexcept {
  std::uint64_t magnitude = 0U;
  if (!To64(value.magnitude, magnitude)) return false;
  const std::uint64_t sign = std::uint64_t{1U} << 63U;
  if (magnitude > (value.negative ? sign : sign - 1U)) return false;
  // 负侧允许幅值 2^63，并直接交付 INT64_MIN；其余才安全转换并取负。
  output = value.negative ? (magnitude == sign ? (std::numeric_limits<std::int64_t>::min)()
                                               : -static_cast<std::int64_t>(magnitude))
                          : static_cast<std::int64_t>(magnitude);
  return true;
}

std::uint64_t Power10(unsigned exponent) noexcept {
  // 内部前提：输入 scale 和合法描述的 decimal_places 均不超过 18，指数差也如此。
  std::uint64_t value = 1U;
  for (unsigned index = 0U; index < exponent; ++index) value *= 10U;
  return value;
}

bool FitsRaw(SignedWide raw, protocol_plan::ValueType type, std::size_t byte_width) noexcept {
  // 按原始类型及实际 Wire 宽度检查；无符号拒绝负数，有符号负侧多容纳一个极值。
  // 8 字节 UINT64 单独分支，不求 1 << 64；INT64 的符号位移最大为 63。
  if (byte_width == 0U || byte_width > 8U) return false;
  std::uint64_t magnitude = 0U;
  if (!To64(raw.magnitude, magnitude)) return false;
  if (type == protocol_plan::ValueType::UINT64) {
    return !raw.negative &&
           (byte_width == 8U || magnitude < (std::uint64_t{1U} << (byte_width * 8U)));
  }
  if (type != protocol_plan::ValueType::INT64) return false;
  const std::uint64_t sign = std::uint64_t{1U} << (byte_width * 8U - 1U);
  return magnitude <= (raw.negative ? sign : sign - 1U);
}

DecimalArithmeticStatus Normalize(SignedWide coefficient, unsigned scale,
                                  Decimal64& output) noexcept {
  // 仅在 scale>0 且能整除 10 时消尾零，不把 scale 降到负数；零统一为 {0,0}。
  // 必须先约去这些因子再收窄系数，否则会拒绝规范化后其实可表示的数学值。
  if (IsZero(coefficient.magnitude)) {
    output = Decimal64{};
    return DecimalArithmeticStatus::OK;
  }
  while (scale != 0U) {
    Wide quotient;
    Wide remainder;
    if (!Divide(coefficient.magnitude, From64(10U), quotient, remainder)) {
      return DecimalArithmeticStatus::INTERNAL_ERROR;
    }
    if (!IsZero(remainder)) break;
    coefficient.magnitude = quotient;
    --scale;
  }
  if (!ToSigned(coefficient, output.coefficient)) {
    return DecimalArithmeticStatus::LOGICAL_OUT_OF_RANGE;
  }
  output.scale = static_cast<std::int32_t>(scale);
  return DecimalArithmeticStatus::OK;
}

SignedWide RawSigned(std::uint64_t raw_bits, protocol_plan::ValueType type,
                     std::size_t byte_width) noexcept {
  // Codec 已按合法宽度读取 unsigned 位形；此处按原始类型解释补码，不做负数移位。
  // 全宽 mask 单独处理，INT64_MIN 的幅值在 uint64_t 中仍可表示。
  if (type == protocol_plan::ValueType::UINT64) return SignedWide{From64(raw_bits), false};
  const unsigned bits = static_cast<unsigned>(byte_width * 8U);
  const std::uint64_t sign = std::uint64_t{1U} << (bits - 1U);
  if ((raw_bits & sign) == 0U) return SignedWide{From64(raw_bits), false};
  const std::uint64_t mask =
      bits == 64U ? (std::numeric_limits<std::uint64_t>::max)() : (std::uint64_t{1U} << bits) - 1U;
  return Canonical(SignedWide{From64(((~raw_bits) & mask) + 1U), true});
}

}  // namespace

DecimalArithmeticStatus DecodeDecimal(const protocol_plan::LinearConversionDescriptor& conversion,
                                      std::uint64_t raw_bits, std::size_t byte_width,
                                      Decimal64& output) noexcept {
  SignedWide raw = RawSigned(raw_bits, conversion.raw_value_type, byte_width);
  if (!FitsRaw(raw, conversion.raw_value_type, byte_width)) {
    return DecimalArithmeticStatus::RAW_OUT_OF_RANGE;
  }
  SignedWide value;
  // A/B 与 t 已在 Plan 层约分、统一十进制分母并冻结；热路径不重新解释配置比例。
  // N=raw*A+B，logical=N/10^t。宽乘加允许大中间结果抵消，最终可表示域仍须检查。
  if (!SignedMultiply(raw, FromDescriptor(conversion.scale_coefficient), value) ||
      !SignedAdd(value, FromDescriptor(conversion.bias_coefficient), value)) {
    return DecimalArithmeticStatus::INTERNAL_ERROR;
  }
  return Normalize(value, conversion.decimal_places, output);
}

DecimalArithmeticStatus EncodeDecimal(const protocol_plan::LinearConversionDescriptor& conversion,
                                      Decimal64 logical, std::size_t byte_width,
                                      std::uint64_t& raw_bits) noexcept {
  // 先检查表示，即使 coefficient=0 也不能绕过非法 scale。
  if (logical.scale < 0 || logical.scale > 18) {
    return DecimalArithmeticStatus::DECIMAL_SCALE_OUT_OF_RANGE;
  }
  const unsigned logical_scale = static_cast<unsigned>(logical.scale);
  const unsigned common_scale =
      (std::max)(logical_scale, static_cast<unsigned>(conversion.decimal_places));
  SignedWide logical_value;
  SignedWide bias;
  SignedWide denominator;
  SignedWide numerator;
  // logical=C/10^s，令 u=max(s,t)：先对齐整数尺度，再减偏置和做一次精确除法。
  // raw=(C*10^(u-s)-B*10^(u-t))/(A*10^(u-t))，这里不提前舍入或截断中间项。
  if (!SignedMultiply(FromSigned(logical.coefficient),
                      SignedWide{From64(Power10(common_scale - logical_scale)), false},
                      logical_value) ||
      !SignedMultiply(FromDescriptor(conversion.bias_coefficient),
                      SignedWide{From64(Power10(common_scale - conversion.decimal_places)), false},
                      bias) ||
      !SignedMultiply(FromDescriptor(conversion.scale_coefficient),
                      SignedWide{From64(Power10(common_scale - conversion.decimal_places)), false},
                      denominator)) {
    return DecimalArithmeticStatus::INTERNAL_ERROR;
  }
  bias.negative = !bias.negative;
  if (!SignedAdd(logical_value, bias, numerator)) {
    return DecimalArithmeticStatus::INTERNAL_ERROR;
  }
  Wide quotient;
  Wide remainder;
  if (!Divide(numerator.magnitude, denominator.magnitude, quotient, remainder)) {
    return DecimalArithmeticStatus::INTERNAL_ERROR;
  }
  // 先检查整除，再检查商的符号与 Wire 范围；负比例由商的符号异或处理。
  if (!IsZero(remainder)) return DecimalArithmeticStatus::RAW_NOT_INTEGRAL;
  const SignedWide raw =
      Canonical(SignedWide{quotient, numerator.negative != denominator.negative});
  if (!FitsRaw(raw, conversion.raw_value_type, byte_width)) {
    return DecimalArithmeticStatus::RAW_OUT_OF_RANGE;
  }
  std::uint64_t magnitude = 0U;
  if (!To64(raw.magnitude, magnitude)) return DecimalArithmeticStatus::INTERNAL_ERROR;
  // 成功末尾才更新引用输出；取补码后按窄 Wire 宽度遮罩，不把失败空帧当作成功输出。
  raw_bits = raw.negative ? std::uint64_t{0U} - magnitude : magnitude;
  if (byte_width < 8U) raw_bits &= (std::uint64_t{1U} << (byte_width * 8U)) - 1U;
  return DecimalArithmeticStatus::OK;
}

bool DecimalEqual(Decimal64 left, Decimal64 right) noexcept {
  // 例如 {123,1} 与 {1230,2} 数学相等；先验证 scale 再规范化，也包括各种合法零表示。
  // Encode 最终复核重读实际字节后用此比较，不要求输入与输出的成员表示完全相同。
  Decimal64 normalized_left;
  Decimal64 normalized_right;
  if (left.scale < 0 || left.scale > 18 || right.scale < 0 || right.scale > 18 ||
      Normalize(FromSigned(left.coefficient), static_cast<unsigned>(left.scale), normalized_left) !=
          DecimalArithmeticStatus::OK ||
      Normalize(FromSigned(right.coefficient), static_cast<unsigned>(right.scale),
                normalized_right) != DecimalArithmeticStatus::OK) {
    return false;
  }
  return normalized_left.coefficient == normalized_right.coefficient &&
         normalized_left.scale == normalized_right.scale;
}

}  // namespace pae::protocol_core::detail

#endif
