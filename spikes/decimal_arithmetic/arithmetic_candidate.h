#pragma once

#include <array>
#include <cstdint>
#include <limits>

// Disposable arithmetic candidate; not a production header or stable API.
namespace pae::decimal_spike {
struct Wide {
  std::array<std::uint32_t, 8> words{};
};
inline Wide From64(std::uint64_t n) noexcept {
  Wide r;
  r.words[0] = static_cast<std::uint32_t>(n);
  r.words[1] = static_cast<std::uint32_t>(n >> 32U);
  return r;
}
inline int Compare(const Wide& a, const Wide& b) noexcept {
  for (int i = 7; i >= 0; --i) {
    if (a.words[i] != b.words[i]) return a.words[i] < b.words[i] ? -1 : 1;
  }
  return 0;
}
inline bool Zero(const Wide& a) noexcept { return Compare(a, Wide{}) == 0; }
inline bool Add(const Wide& a, const Wide& b, Wide& out) noexcept {
  Wide r;
  std::uint64_t carry = 0;
  for (unsigned i = 0; i < 8; ++i) {
    const std::uint64_t n = std::uint64_t{a.words[i]} + b.words[i] + carry;
    r.words[i] = static_cast<std::uint32_t>(n);
    carry = n >> 32U;
  }
  if (carry) return false;
  out = r;
  return true;
}
// Modular subtraction also handles an extra high (257th) bit in long division.
inline Wide Subtract(const Wide& a, const Wide& b) noexcept {
  Wide r;
  std::uint64_t borrow = 0;
  for (unsigned i = 0; i < 8; ++i) {
    const std::uint64_t sub = std::uint64_t{b.words[i]} + borrow;
    r.words[i] = static_cast<std::uint32_t>(std::uint64_t{a.words[i]} - sub);
    borrow = std::uint64_t{a.words[i]} < sub ? 1 : 0;
  }
  return r;
}
inline bool Multiply(const Wide& a, const Wide& b, Wide& out) noexcept {
  std::array<std::uint32_t, 16> product{};
  for (unsigned i = 0; i < 8; ++i) {
    std::uint64_t carry = 0;
    for (unsigned j = 0; j < 8; ++j) {
      // (2^32-1)^2 + 2*(2^32-1) == 2^64-1.
      const std::uint64_t n = std::uint64_t{a.words[i]} * b.words[j] + product[i + j] + carry;
      product[i + j] = static_cast<std::uint32_t>(n);
      carry = n >> 32U;
    }
    product[i + 8] = static_cast<std::uint32_t>(carry);
  }
  for (unsigned i = 8; i < 16; ++i) {
    if (product[i]) return false;
  }
  Wide r;
  for (unsigned i = 0; i < 8; ++i) r.words[i] = product[i];
  out = r;
  return true;
}
inline bool Divide(const Wide& n, const Wide& d, Wide& quotient, Wide& remainder) noexcept {
  if (Zero(d)) return false;
  Wide q, r;
  for (int bit = 255; bit >= 0; --bit) {
    const bool high = (r.words[7] >> 31U) != 0;
    std::uint32_t carry = (n.words[bit / 32] >> (bit % 32)) & 1U;
    for (unsigned i = 0; i < 8; ++i) {
      const std::uint32_t next = r.words[i] >> 31U;
      r.words[i] = (r.words[i] << 1U) | carry;
      carry = next;
    }
    if (high || Compare(r, d) >= 0) {
      r = Subtract(r, d);
      q.words[bit / 32] |= std::uint32_t{1} << (bit % 32);
    }
  }
  quotient = q;
  remainder = r;
  return true;
}
inline bool To64(const Wide& a, std::uint64_t& out) noexcept {
  for (unsigned i = 2; i < 8; ++i) {
    if (a.words[i]) return false;
  }
  out = (std::uint64_t{a.words[1]} << 32U) | a.words[0];
  return true;
}
inline std::uint64_t Magnitude(std::int64_t n) noexcept {
  return n < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(n) : static_cast<std::uint64_t>(n);
}
struct Signed {
  Wide magnitude;
  bool negative = false;
};
inline Signed Canonical(Signed n) noexcept {
  if (Zero(n.magnitude)) n.negative = false;
  return n;
}
inline Signed FromSigned(std::int64_t n) noexcept { return {From64(Magnitude(n)), n < 0}; }
inline bool SignedAdd(Signed a, Signed b, Signed& out) noexcept {
  Signed r;
  if (a.negative == b.negative) {
    r.negative = a.negative;
    if (!Add(a.magnitude, b.magnitude, r.magnitude)) return false;
  } else if (Compare(a.magnitude, b.magnitude) >= 0) {
    r = {Subtract(a.magnitude, b.magnitude), a.negative};
  } else {
    r = {Subtract(b.magnitude, a.magnitude), b.negative};
  }
  out = Canonical(r);
  return true;
}
inline bool SignedMultiply(Signed a, Signed b, Signed& out) noexcept {
  Signed r;
  if (!Multiply(a.magnitude, b.magnitude, r.magnitude)) return false;
  r.negative = a.negative != b.negative;
  out = Canonical(r);
  return true;
}
inline bool ToSigned(Signed a, std::int64_t& out) noexcept {
  std::uint64_t n = 0;
  if (!To64(a.magnitude, n)) return false;
  const std::uint64_t sign = std::uint64_t{1} << 63U;
  if (n > (a.negative ? sign : sign - 1U)) return false;
  out = a.negative ? (n == sign ? (std::numeric_limits<std::int64_t>::min)()
                                : -static_cast<std::int64_t>(n))
                   : static_cast<std::int64_t>(n);
  return true;
}
inline std::uint64_t Power10(unsigned p) noexcept {
  std::uint64_t n = 1;
  for (unsigned i = 0; i < p; ++i) n *= 10;
  return n;
}
struct Rational {
  std::int64_t numerator;
  std::uint64_t denominator;
};
struct Decimal {
  std::int64_t coefficient;
  int scale;
};
enum class Status {
  OK,
  PARAMETER_INVALID,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
  INTERNAL_OVERFLOW
};
// Internal precondition: Decode/Encode receive only an unmodified Compile result.
// This disposable representation is not a production frozen-plan contract.
struct Plan {
  Signed a;
  Signed b;
  unsigned places = 0;
};
inline std::uint64_t Gcd(std::uint64_t a, std::uint64_t b) noexcept {
  while (b) {
    const auto next = a % b;
    a = b;
    b = next;
  }
  return a;
}
inline bool Parameter(Rational p, Signed& coefficient, unsigned& places) noexcept {
  if (p.denominator == 0 || p.denominator > Power10(18)) return false;
  const auto divisor = Gcd(Magnitude(p.numerator), p.denominator);
  const auto numerator = Magnitude(p.numerator) / divisor;
  auto denominator = p.denominator / divisor;
  unsigned twos = 0, fives = 0;
  while (denominator % 2 == 0) {
    denominator /= 2;
    ++twos;
  }
  while (denominator % 5 == 0) {
    denominator /= 5;
    ++fives;
  }
  places = twos > fives ? twos : fives;
  if (denominator != 1 || places > 18) return false;
  Wide r;
  if (!Multiply(From64(numerator), From64(Power10(places) / (p.denominator / divisor)), r))
    return false;
  coefficient = Canonical({r, p.numerator < 0});
  return true;
}
inline Status Compile(Rational scale, Rational bias, Plan& out) noexcept {
  if (scale.numerator == 0) return Status::PARAMETER_INVALID;
  Plan r;
  unsigned a_places = 0, b_places = 0;
  if (!Parameter(scale, r.a, a_places) || !Parameter(bias, r.b, b_places))
    return Status::PARAMETER_INVALID;
  r.places = a_places > b_places ? a_places : b_places;
  if (!SignedMultiply(r.a, {From64(Power10(r.places - a_places)), false}, r.a) ||
      !SignedMultiply(r.b, {From64(Power10(r.places - b_places)), false}, r.b))
    return Status::INTERNAL_OVERFLOW;
  out = r;
  return Status::OK;
}
inline Status Normalize(Signed coefficient, unsigned scale, Decimal& out) noexcept {
  if (Zero(coefficient.magnitude)) {
    out = {0, 0};
    return Status::OK;
  }
  while (scale) {
    Wide q, rem;
    Divide(coefficient.magnitude, From64(10), q, rem);
    if (!Zero(rem)) break;
    coefficient.magnitude = q;
    --scale;
  }
  std::int64_t n = 0;
  if (!ToSigned(coefficient, n)) return Status::LOGICAL_OUT_OF_RANGE;
  out = {n, static_cast<int>(scale)};
  return Status::OK;
}
inline bool FitsRaw(Signed raw, bool signed_wire, unsigned bytes) noexcept {
  if (bytes < 1 || bytes > 8) return false;
  std::uint64_t n = 0;
  if (!To64(raw.magnitude, n)) return false;
  if (!signed_wire) return !raw.negative && (bytes == 8 || n < (std::uint64_t{1} << (bytes * 8)));
  const auto sign = std::uint64_t{1} << (bytes * 8 - 1);
  return n <= (raw.negative ? sign : sign - 1);
}
inline Status Decode(const Plan& plan, Signed raw, bool signed_wire, unsigned bytes,
                     Decimal& out) noexcept {
  raw = Canonical(raw);
  if (!FitsRaw(raw, signed_wire, bytes)) return Status::RAW_OUT_OF_RANGE;
  Signed n;
  if (!SignedMultiply(raw, plan.a, n) || !SignedAdd(n, plan.b, n)) return Status::INTERNAL_OVERFLOW;
  return Normalize(n, plan.places, out);
}
inline Status Encode(const Plan& plan, Decimal logical, bool signed_wire, unsigned bytes,
                     Signed& out) noexcept {
  if (logical.scale < 0 || logical.scale > 18) return Status::DECIMAL_SCALE_OUT_OF_RANGE;
  const auto s = static_cast<unsigned>(logical.scale);
  const unsigned u = s > plan.places ? s : plan.places;
  Signed left, bias, denominator, numerator;
  if (!SignedMultiply(FromSigned(logical.coefficient), {From64(Power10(u - s)), false}, left) ||
      !SignedMultiply(plan.b, {From64(Power10(u - plan.places)), false}, bias) ||
      !SignedMultiply(plan.a, {From64(Power10(u - plan.places)), false}, denominator))
    return Status::INTERNAL_OVERFLOW;
  bias.negative = !bias.negative;
  if (!SignedAdd(left, bias, numerator)) return Status::INTERNAL_OVERFLOW;
  Wide quotient, remainder;
  if (!Divide(numerator.magnitude, denominator.magnitude, quotient, remainder))
    return Status::INTERNAL_OVERFLOW;
  if (!Zero(remainder)) return Status::RAW_NOT_INTEGRAL;
  const Signed raw = Canonical({quotient, numerator.negative != denominator.negative});
  if (!FitsRaw(raw, signed_wire, bytes)) return Status::RAW_OUT_OF_RANGE;
  out = raw;
  return Status::OK;
}
}  // namespace pae::decimal_spike
