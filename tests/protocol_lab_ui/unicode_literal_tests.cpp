#include <QByteArray>
#include <QString>
#include <cstdio>
#include <initializer_list>
#include <utility>
#include <vector>

#if defined(NDEBUG)
#error "Unicode regression must retain Release assertions"
#endif

namespace {
struct Product {
  QString actual;
  std::vector<ushort> expected;
};
std::vector<Product> products;
int failures = 0;

void CheckProduct(int index, const QString& actual, std::initializer_list<ushort> units,
                  std::initializer_list<unsigned char> bytes, bool record = true) {
  const QString expected = QString::fromUtf16(units.begin(), static_cast<int>(units.size()));
  const bool units_ok = actual == expected;
  const auto utf8 = actual.toUtf8();
  bool bytes_ok = utf8.size() == static_cast<int>(bytes.size());
  int offset = 0;
  for (auto byte : bytes) {
    if (offset >= utf8.size() || static_cast<unsigned char>(utf8[offset]) != byte) bytes_ok = false;
    ++offset;
  }
  std::printf("PRODUCT %d utf16=%s utf8=%s\n", index, units_ok ? "PASS" : "FAIL",
              bytes_ok ? "PASS" : "FAIL");
  if (!units_ok || !bytes_ok) ++failures;
  if (record) products.push_back({actual, std::vector<ushort>(units)});
}

// Handwritten numeric anchors independently validate representative extracted expectations.
void CheckAnchor(const char* label, std::initializer_list<ushort> units,
                 std::initializer_list<unsigned char> bytes) {
  const std::vector<ushort> key(units);
  bool found = false;
  for (const auto& product : products) {
    if (product.expected == key) {
      found = true;
      CheckProduct(0, product.actual, units, bytes, false);
      break;
    }
  }
  std::printf("ANCHOR %s found=%s\n", label, found ? "PASS" : "FAIL");
  if (!found) ++failures;
}
}  // namespace

int main() {
// This generated fragment recompiles every real product call with the test target's charset.
#include "unicode_product_literals.inc"
  const auto count = products.size();
  CheckAnchor("silent-chinese", {0x89E3, 0x6790, 0x7ED3, 0x679C},
              {0xE8, 0xA7, 0xA3, 0xE6, 0x9E, 0x90, 0xE7, 0xBB, 0x93, 0xE6, 0x9E, 0x9C});
  CheckAnchor(
      "range",
      {0x5B9E, 0x9645, 0x5B57, 0x8282, 0x8303, 0x56F4, 0xFF1A, 0x32, 0x20, 0x2B, 0x20, 0x30},
      {0xE5, 0xAE, 0x9E, 0xE9, 0x99, 0x85, 0xE5, 0xAD, 0x97, 0xE8, 0x8A, 0x82, 0xE8,
       0x8C, 0x83, 0xE5, 0x9B, 0xB4, 0xEF, 0xBC, 0x9A, 0x32, 0x20, 0x2B, 0x20, 0x30});
  CheckAnchor("html-placeholder",
              {0x3C, 0x62, 0x3E, 0x62A5, 0x6587, 0x3C, 0x2F, 0x62, 0x3E, 0xFF1A, 0x25, 0x31},
              {0x3C, 0x62, 0x3E, 0xE6, 0x8A, 0xA5, 0xE6, 0x96, 0x87, 0x3C, 0x2F, 0x62, 0x3E, 0xEF,
               0xBC, 0x9A, 0x25, 0x31});
  std::printf("PRODUCT_CHECKED=%zu ANCHORS=3 FAILURES=%d\n", count, failures);
  return failures == 0 ? 0 : 1;
}
