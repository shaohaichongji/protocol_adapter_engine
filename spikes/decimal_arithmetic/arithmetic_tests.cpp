#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <new>

#include "arithmetic_candidate.h"

namespace {
std::atomic<unsigned long long> allocations{0};
unsigned checks = 0;
unsigned failures = 0;
void Check(bool ok, const char* name) {
  ++checks;
  if (!ok) {
    ++failures;
    std::printf("FAIL %s\n", name);
  }
}
}  // namespace
// Replaceable new coverage, not a claim to intercept every CRT/OS allocation.
void* operator new(std::size_t size) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* p = std::malloc(size ? size : 1)) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

using namespace pae::decimal_spike;
namespace {
Wide Bit(unsigned bit) {
  Wide n;
  n.words[bit / 32] = std::uint32_t{1} << (bit % 32);
  return n;
}
bool Equal(const Wide& a, const Wide& b) { return Compare(a, b) == 0; }
bool Equal(Signed a, Signed b) {
  return Equal(a.magnitude, b.magnitude) && a.negative == b.negative;
}
bool Equal(Decimal a, Decimal b) { return a.coefficient == b.coefficient && a.scale == b.scale; }
void Arithmetic() {
  const auto max = (std::numeric_limits<std::uint64_t>::max)();
  Wide q, r, product, sum;
  Check(Multiply(From64(max), From64(max), product) && product.words[0] == 1 &&
            product.words[1] == 0 && product.words[2] == 0xFFFFFFFEU &&
            product.words[3] == 0xFFFFFFFFU,
        "independent (2^64-1)^2 limbs");
  Wide all;
  all.words.fill(0xFFFFFFFFU);
  sum = From64(77);
  Check(!Add(all, From64(1), sum) && Equal(sum, From64(77)), "addition overflow leaves output");
  Check(!Multiply(Bit(255), From64(2), sum) && Equal(sum, From64(77)),
        "multiply overflow leaves output");
  Check(!Divide(all, Wide{}, q, r), "division by zero");
  Check(
      Divide(all, Bit(255), q, r) && Equal(q, From64(1)) && Equal(r, Subtract(Bit(255), From64(1))),
      "independent high-bit quotient and remainder");
  Check(Divide(all, From64(3), q, r) && Zero(r), "max256 divisible by three");
  bool thirds = true;
  for (const auto word : q.words) thirds = thirds && word == 0x55555555U;
  Check(thirds, "independent max256/3 limbs");
  Wide low128;
  for (unsigned i = 0; i < 4; ++i) low128.words[i] = 0xFFFFFFFFU;
  Wide square128;
  square128.words = {1, 0, 0, 0, 0xFFFFFFFEU, 0xFFFFFFFFU, 0xFFFFFFFFU, 0xFFFFFFFFU};
  Check(Multiply(low128, low128, product) && Equal(product, square128),
        "independent (2^128-1)^2 all limbs");
  for (unsigned i = 0; i < 256; ++i) {
    for (unsigned j = 0; j < 256; ++j) {
      product = From64(77);
      const bool ok = Multiply(Bit(i), Bit(j), product);
      Check(i + j < 256 ? ok && Equal(product, Bit(i + j)) : !ok && Equal(product, From64(77)),
            "basis products exact or overflow");
    }
  }
  for (unsigned bit = 1; bit < 256; ++bit) {
    const auto previous = Subtract(Bit(bit), From64(1));
    Check(Add(previous, From64(1), sum) && Equal(sum, Bit(bit)), "all limb carry boundaries");
    Check(Divide(Bit(bit), From64(2), q, r) && Equal(q, Bit(bit - 1)) && Zero(r),
          "all bit divisions");
  }
  std::uint64_t state = 0xDEC042B;
  for (unsigned i = 0; i < 3000; ++i) {
    state = state * 6364136223846793005ULL + 1;
    const std::uint32_t a = static_cast<std::uint32_t>(state >> 32U);
    state = state * 6364136223846793005ULL + 1;
    const std::uint32_t b = static_cast<std::uint32_t>(state >> 32U) | 1U;
    std::uint64_t value = 0;
    Check(Multiply(From64(a), From64(b), product) && To64(product, value) &&
              value == std::uint64_t{a} * b,
          "native exact uint32 product reference");
    Check(Divide(From64(state), From64(b), q, r) && To64(q, value) && value == state / b &&
              To64(r, value) && value == state % b,
          "native uint64 division reference");
  }
  // Full-width identities supplement, but do not replace independent reference vectors.
  for (unsigned i = 0; i < 200; ++i) {
    Wide n, d;
    for (unsigned j = 0; j < 8; ++j) {
      state = state * 6364136223846793005ULL + 1;
      n.words[j] = static_cast<std::uint32_t>(state >> 32U);
      state = state * 6364136223846793005ULL + 1;
      d.words[j] = static_cast<std::uint32_t>(state >> 32U);
    }
    d.words[0] |= 1U;
    Check(Divide(n, d, q, r) && Compare(r, d) < 0 && Multiply(q, d, product) &&
              Add(product, r, sum) && Equal(n, sum),
          "full256 division reconstruction");
  }
}
void Conversion() {
  const auto min = (std::numeric_limits<std::int64_t>::min)();
  const auto max = (std::numeric_limits<std::int64_t>::max)();
  const auto umax = (std::numeric_limits<std::uint64_t>::max)();
  Plan plan;
  Decimal d{77, 0};
  Signed raw;
  Check(Compile({1, 10}, {-40, 1}, plan) == Status::OK, "compile temperature");
  Check(Decode(plan, FromSigned(523), true, 2, d) == Status::OK && Equal(d, {123, 1}),
        "523 to 12.3");
  Check(Encode(plan, {123, 1}, true, 2, raw) == Status::OK && Equal(raw, FromSigned(523)),
        "12.3 to 523");
  Check(Encode(plan, {1230, 2}, true, 2, raw) == Status::OK && Equal(raw, FromSigned(523)),
        "equivalent decimal");
  const auto saved = raw;
  Check(Encode(plan, {1235, 2}, true, 2, raw) == Status::RAW_NOT_INTEGRAL && Equal(raw, saved),
        "fractional raw rejected");
  Check(Encode(plan, {0, 19}, true, 2, raw) == Status::DECIMAL_SCALE_OUT_OF_RANGE,
        "invalid zero scale high");
  Check(Encode(plan, {0, -1}, true, 2, raw) == Status::DECIMAL_SCALE_OUT_OF_RANGE,
        "invalid zero scale low");
  Check(Compile({1, 3}, {0, 1}, plan) == Status::PARAMETER_INVALID, "nondecimal denominator");
  Check(Compile({1, std::uint64_t{1} << 19U}, {0, 1}, plan) == Status::PARAMETER_INVALID,
        "nineteen places");
  Check(Compile({0, 1}, {0, 1}, plan) == Status::PARAMETER_INVALID, "zero scale");
  Check(Compile({1, 0}, {0, 1}, plan) == Status::PARAMETER_INVALID, "zero denominator");
  Check(Compile({2, Power10(18) + 2}, {0, 1}, plan) == Status::PARAMETER_INVALID,
        "author bound before reduction");
  Check(Compile({3, 6}, {0, 1}, plan) == Status::OK &&
            Decode(plan, FromSigned(-1), true, 1, d) == Status::OK && Equal(d, {-5, 1}),
        "reduction and negative fraction");
  Check(Compile({-3, 20}, {0, 1}, plan) == Status::OK &&
            Decode(plan, FromSigned(2), true, 1, d) == Status::OK && Equal(d, {-3, 1}) &&
            Encode(plan, {-30, 2}, true, 1, raw) == Status::OK && Equal(raw, FromSigned(2)),
        "negative scale");
  Check(Compile({1, 1}, {min, 1}, plan) == Status::OK &&
            Decode(plan, {From64(umax), false}, false, 8, d) == Status::OK && Equal(d, {max, 0}),
        "UINT64_MAX cancellation");
  Check(Encode(plan, {max, 0}, false, 8, raw) == Status::OK && Equal(raw, {From64(umax), false}),
        "inverse cancellation");
  Check(Compile({1, 1}, {0, 1}, plan) == Status::OK, "identity");
  d = {77, 0};
  Check(Decode(plan, {From64(umax), false}, false, 8, d) == Status::LOGICAL_OUT_OF_RANGE &&
            Equal(d, {77, 0}),
        "logical overflow leaves output");
  Check(Encode(plan, {0, 18}, true, 8, raw) == Status::OK && Equal(raw, FromSigned(0)),
        "zero normalizes");
  Check(Decode(plan, FromSigned(min), true, 8, d) == Status::OK && Equal(d, {min, 0}),
        "minimum value");
  for (unsigned width = 1; width <= 8; ++width) {
    const auto lo =
        width == 8 ? min : -static_cast<std::int64_t>(std::uint64_t{1} << (width * 8 - 1));
    const auto hi =
        width == 8 ? max : static_cast<std::int64_t>((std::uint64_t{1} << (width * 8 - 1)) - 1);
    Check(Encode(plan, {lo, 0}, true, width, raw) == Status::OK && Equal(raw, FromSigned(lo)),
          "signed minimum width");
    Check(Encode(plan, {hi, 0}, true, width, raw) == Status::OK && Equal(raw, FromSigned(hi)),
          "signed maximum width");
    if (width < 8) {
      Check(Encode(plan, {lo - 1, 0}, true, width, raw) == Status::RAW_OUT_OF_RANGE,
            "signed low overflow");
      Check(Encode(plan, {hi + 1, 0}, true, width, raw) == Status::RAW_OUT_OF_RANGE,
            "signed high overflow");
      const auto unsigned_hi = static_cast<std::int64_t>((std::uint64_t{1} << (width * 8)) - 1);
      Check(Encode(plan, {unsigned_hi, 0}, false, width, raw) == Status::OK,
            "unsigned maximum width");
      Check(Encode(plan, {unsigned_hi + 1, 0}, false, width, raw) == Status::RAW_OUT_OF_RANGE,
            "unsigned overflow");
    }
    Check(Encode(plan, {-1, 0}, false, width, raw) == Status::RAW_OUT_OF_RANGE,
          "unsigned rejects negative");
  }
  Check(Compile({-1, 1}, {0, 1}, plan) == Status::OK &&
            Decode(plan, FromSigned(min), true, 8, d) == Status::LOGICAL_OUT_OF_RANGE,
        "minimum negation is wide");
  Check(Compile({1, 10}, {0, 1}, plan) == Status::OK &&
            Decode(plan, {From64(10000000000000000000ULL), false}, false, 8, d) == Status::OK &&
            Equal(d, {1000000000000000000LL, 0}),
        "normalize before coefficient range");
  // Largest common-decimal coefficients exercise >64-bit intermediate multiplication and division.
  Check(Compile({min, 1}, {1, Power10(18)}, plan) == Status::OK &&
            Decode(plan, FromSigned(0), true, 8, d) == Status::OK && Equal(d, {1, 18}),
        "123-bit coefficient plan");
  Check(Decode(plan, {From64(umax), false}, false, 8, d) == Status::LOGICAL_OUT_OF_RANGE,
        "187-bit intermediate range rejection");
  Check(Encode(plan, {1, 18}, true, 8, raw) == Status::OK && Equal(raw, FromSigned(0)),
        "wide denominator exact zero");
  // Independent identity: (INT64_MAX + INT64_MIN) / 2 == -0.5.
  // Common decimal coefficients are 5*INT64_MAX and 5*INT64_MIN (>64-bit).
  Check(Compile({max, 2}, {min, 2}, plan) == Status::OK, "compile wide nonzero cancellation");
  Check(plan.places == 1 && !plan.a.negative && plan.b.negative &&
            Compare(plan.a.magnitude, From64(umax)) > 0 &&
            Compare(plan.b.magnitude, From64(umax)) > 0,
        "cancellation exercises wide coefficients");
  Check(Decode(plan, FromSigned(1), true, 1, d) == Status::OK && Equal(d, {-5, 1}),
        "wide cancellation independently yields minus half");
  Check(Encode(plan, {-5, 1}, true, 1, raw) == Status::OK && Equal(raw, FromSigned(1)),
        "wide divisor independently yields nonzero quotient");
  Check(Encode(plan, {-50, 2}, true, 1, raw) == Status::OK && Equal(raw, FromSigned(1)),
        "wide divisor equivalent decimal nonzero quotient");
  raw = FromSigned(77);
  Check(
      Encode(plan, {-4, 1}, true, 1, raw) == Status::RAW_NOT_INTEGRAL && Equal(raw, FromSigned(77)),
      "wide divisor fractional quotient rejects and preserves output");
  Check(Compile({min, 2}, {0, 1}, plan) == Status::OK &&
            Decode(plan, FromSigned(2), true, 1, d) == Status::OK && Equal(d, {min, 0}) &&
            Encode(plan, {min, 0}, true, 1, raw) == Status::OK && Equal(raw, FromSigned(2)),
        "minimum numerator reduction and inverse");
  Check(Compile({1, Power10(18)}, {0, 1}, plan) == Status::OK &&
            Decode(plan, FromSigned(min), true, 8, d) == Status::OK && Equal(d, {min, 18}) &&
            Encode(plan, {min, 18}, true, 8, raw) == Status::OK && Equal(raw, FromSigned(min)),
        "eighteen places minimum coefficient");
  // Independently safe small-integer formula, fixed seed and bounded operands.
  for (std::int64_t a : {-7LL, -1LL, 1LL, 3LL, 11LL}) {
    for (std::int64_t b : {-40LL, 0LL, 17LL}) {
      Check(Compile({a, 100}, {b, 10}, plan) == Status::OK, "small reference plan");
      for (std::int64_t n = -100; n <= 100; ++n) {
        std::int64_t expected = n * a + b * 10;
        int places = 2;
        if (!expected) places = 0;
        while (places && expected % 10 == 0) {
          expected /= 10;
          --places;
        }
        Check(Decode(plan, FromSigned(n), true, 2, d) == Status::OK && Equal(d, {expected, places}),
              "independent small formula");
        Check(Encode(plan, {expected, places}, true, 2, raw) == Status::OK &&
                  Equal(raw, FromSigned(n)),
              "inverse small reference");
      }
    }
  }
}
}  // namespace
int main() {
  void* probe = ::operator new(1);
  ::operator delete(probe);
  Check(allocations.load() > 0, "allocation counter live");
  const auto before = allocations.load();
  Arithmetic();
  Conversion();
  const auto after = allocations.load();
  Check(before == after, "no replaceable new during arithmetic suite");
  std::printf("DECIMAL_SPIKE checks=%u failures=%u allocation_delta=%llu\n", checks, failures,
              after - before);
  return failures ? 1 : 0;
}
