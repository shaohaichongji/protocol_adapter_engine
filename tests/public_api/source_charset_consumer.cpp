#include <pae/codec.h>
#include <pae/compiler.h>

// Wide literals verify source decoding independently of the narrow execution charset.
static_assert(L"中文"[0] == L'\u4E2D');
static_assert(L"中文"[1] == L'\u6587');
static_assert(L"中文"[2] == L'\0');

#if defined(PAE_TEST_EXPECT_UTF8_EXECUTION)
static_assert(sizeof("中文") == 7U);
static_assert(static_cast<unsigned char>("中文"[0]) == 0xE4U);
static_assert(static_cast<unsigned char>("中文"[1]) == 0xB8U);
static_assert(static_cast<unsigned char>("中文"[2]) == 0xADU);
static_assert(static_cast<unsigned char>("中文"[3]) == 0xE6U);
static_assert(static_cast<unsigned char>("中文"[4]) == 0x96U);
static_assert(static_cast<unsigned char>("中文"[5]) == 0x87U);
#elif defined(PAE_TEST_EXPECT_936_EXECUTION)
static_assert(sizeof("中文") == 5U);
static_assert(static_cast<unsigned char>("中文"[0]) == 0xD6U);
static_assert(static_cast<unsigned char>("中文"[1]) == 0xD0U);
static_assert(static_cast<unsigned char>("中文"[2]) == 0xCEU);
static_assert(static_cast<unsigned char>("中文"[3]) == 0xC4U);
#endif

int main() {
  pae::CompiledProtocol empty;
  return pae::CreateCompleteRecordCodec(empty).status == pae::CodecStatus::INVALID_COMPILED_PROTOCOL
             ? 0
             : 1;
}
