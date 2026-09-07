#include "sha256.h"

#include <algorithm>
#include <array>

namespace pae::protocol_lab {
namespace {

constexpr std::array<std::uint32_t, 64U> kSha256Constants{
    0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U, 0x3956C25BU, 0x59F111F1U, 0x923F82A4U,
    0xAB1C5ED5U, 0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U, 0x72BE5D74U, 0x80DEB1FEU,
    0x9BDC06A7U, 0xC19BF174U, 0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU, 0x2DE92C6FU,
    0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU, 0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
    0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U, 0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU,
    0x53380D13U, 0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U, 0xA2BFE8A1U, 0xA81A664BU,
    0xC24B8B70U, 0xC76C51A3U, 0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U, 0x19A4C116U,
    0x1E376C08U, 0x2748774CU, 0x34B0BCB5U, 0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
    0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U, 0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U,
    0xC67178F2U};

constexpr std::uint32_t RotateRight(std::uint32_t value, unsigned int count) noexcept {
  return (value >> count) | (value << (32U - count));
}

class Sha256 final {
 public:
  void Update(const std::uint8_t* data, std::size_t size) noexcept {
    total_bytes_ += static_cast<std::uint64_t>(size);
    while (size != 0U) {
      const std::size_t copied = std::min(size, buffer_.size() - buffer_size_);
      std::copy_n(data, copied, buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_));
      data += copied;
      size -= copied;
      buffer_size_ += copied;
      if (buffer_size_ == buffer_.size()) {
        Transform(buffer_.data());
        buffer_size_ = 0U;
      }
    }
  }

  std::array<std::uint8_t, 32U> Final() noexcept {
    const std::uint64_t total_bits = total_bytes_ * 8U;
    buffer_[buffer_size_++] = 0x80U;
    if (buffer_size_ > 56U) {
      std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.end(),
                std::uint8_t{0U});
      Transform(buffer_.data());
      buffer_size_ = 0U;
    }
    std::fill(buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.begin() + 56,
              std::uint8_t{0U});
    for (std::size_t index = 0U; index < 8U; ++index) {
      buffer_[63U - index] = static_cast<std::uint8_t>((total_bits >> (index * 8U)) & 0xFFU);
    }
    Transform(buffer_.data());

    std::array<std::uint8_t, 32U> digest{};
    for (std::size_t index = 0U; index < state_.size(); ++index) {
      digest[index * 4U] = static_cast<std::uint8_t>(state_[index] >> 24U);
      digest[index * 4U + 1U] = static_cast<std::uint8_t>(state_[index] >> 16U);
      digest[index * 4U + 2U] = static_cast<std::uint8_t>(state_[index] >> 8U);
      digest[index * 4U + 3U] = static_cast<std::uint8_t>(state_[index]);
    }
    return digest;
  }

 private:
  void Transform(const std::uint8_t* block) noexcept {
    std::array<std::uint32_t, 64U> schedule{};
    for (std::size_t index = 0U; index < 16U; ++index) {
      const std::size_t offset = index * 4U;
      schedule[index] = (static_cast<std::uint32_t>(block[offset]) << 24U) |
                        (static_cast<std::uint32_t>(block[offset + 1U]) << 16U) |
                        (static_cast<std::uint32_t>(block[offset + 2U]) << 8U) |
                        static_cast<std::uint32_t>(block[offset + 3U]);
    }
    for (std::size_t index = 16U; index < schedule.size(); ++index) {
      const std::uint32_t s0 = RotateRight(schedule[index - 15U], 7U) ^
                               RotateRight(schedule[index - 15U], 18U) ^
                               (schedule[index - 15U] >> 3U);
      const std::uint32_t s1 = RotateRight(schedule[index - 2U], 17U) ^
                               RotateRight(schedule[index - 2U], 19U) ^
                               (schedule[index - 2U] >> 10U);
      schedule[index] = schedule[index - 16U] + s0 + schedule[index - 7U] + s1;
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];
    for (std::size_t index = 0U; index < schedule.size(); ++index) {
      const std::uint32_t sum1 = RotateRight(e, 6U) ^ RotateRight(e, 11U) ^ RotateRight(e, 25U);
      const std::uint32_t choice = (e & f) ^ ((~e) & g);
      const std::uint32_t temporary1 =
          h + sum1 + choice + kSha256Constants[index] + schedule[index];
      const std::uint32_t sum0 = RotateRight(a, 2U) ^ RotateRight(a, 13U) ^ RotateRight(a, 22U);
      const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
      const std::uint32_t temporary2 = sum0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + temporary1;
      d = c;
      c = b;
      b = a;
      a = temporary1 + temporary2;
    }
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
  }

  std::array<std::uint32_t, 8U> state_{0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
                                       0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U};
  std::array<std::uint8_t, 64U> buffer_{};
  std::size_t buffer_size_ = 0U;
  std::uint64_t total_bytes_ = 0U;
};

std::string HexLower(const std::array<std::uint8_t, 32U>& data) {
  constexpr char kDigits[] = "0123456789abcdef";
  std::string output;
  output.resize(data.size() * 2U);
  for (std::size_t index = 0U; index < data.size(); ++index) {
    output[index * 2U] = kDigits[data[index] >> 4U];
    output[index * 2U + 1U] = kDigits[data[index] & 0x0FU];
  }
  return output;
}

}  // namespace

std::string HashBytes(const std::uint8_t* data, std::size_t size) {
  Sha256 hash;
  hash.Update(data, size);
  return HexLower(hash.Final());
}

std::string HashBytes(std::string_view data) {
  return HashBytes(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

std::string HashBytes(const std::vector<std::uint8_t>& data) {
  return HashBytes(data.data(), data.size());
}

}  // namespace pae::protocol_lab
