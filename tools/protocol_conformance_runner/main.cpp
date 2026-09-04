#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "complete_record_codec.h"
#include "config_compiler.h"
#include "fixture_reader.h"
#include "plan_bundle.h"

namespace {

using pae::config_compiler::CompileJsonToPlan;
using pae::protocol_core::ByteView;
using pae::protocol_core::CodecStatus;
using pae::protocol_core::DecodeCompleteRecord;
using pae::protocol_core::DecodedFieldSlot;
using pae::protocol_core::EncodeCompleteRecord;
using pae::protocol_core::EncodeFieldValue;
using pae::protocol_core::EnumValueRef;
using pae::protocol_core::ExecutionWorkspace;
using pae::protocol_core::FieldRef;
using pae::protocol_core::kInvalidIndex;
using pae::protocol_core::LogicalValueKind;
using pae::protocol_core::MutableByteBuffer;
using pae::protocol_core::test_support::DecodeExpectedRow;
using pae::protocol_core::test_support::EncodeInputRow;
using pae::protocol_core::test_support::ManifestEntry;
using pae::protocol_core::test_support::ParseHexBytes;
using pae::protocol_core::test_support::ParseUnsignedDecimal;
using pae::protocol_core::test_support::ReadDecodeExpected;
using pae::protocol_core::test_support::ReadEncodeInput;
using pae::protocol_core::test_support::ReadHexFile;
using pae::protocol_core::test_support::ReadManifest;
using pae::protocol_core::test_support::ReadTextFile;
using pae::protocol_plan::ByteOrder;
using pae::protocol_plan::EncodeSource;
using pae::protocol_plan::MatcherKind;
using pae::protocol_plan::PlanBundle;
using pae::protocol_plan::UnknownEnumPolicy;
using pae::protocol_plan::ValueType;

constexpr std::array<std::string_view, 6U> kEvidenceGrades{"CONTRACT_FACT",
                                                           "OBSERVED_CAPTURE",
                                                           "IMPLEMENTATION_CORROBORATED",
                                                           "DOCUMENT_DERIVED_REVIEWED",
                                                           "SYNTHETIC_REVIEWED",
                                                           "OPEN"};

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
    total_bytes_ += size;
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

struct Arguments {
  std::filesystem::path config_path;
  std::filesystem::path corpus_root;
};

struct LoadedVector {
  ManifestEntry manifest;
  std::vector<std::uint8_t> frame;
  std::vector<DecodeExpectedRow> decode_expected;
  std::vector<EncodeInputRow> encode_input;
};

struct OwnedEncodeValues {
  std::vector<std::vector<std::uint8_t>> byte_storage;
  std::vector<EncodeFieldValue> values;
};

struct Coverage {
  bool fixed_bytes_failure = false;
  bool unknown_enum_failure = false;
  bool missing_field_failure = false;
  bool duplicate_field_failure = false;
  bool type_mismatch_failure = false;
  bool width_overflow_failure = false;
  bool buffer_too_small_failure = false;
};

class CheckRunner final {
 public:
  void Pass(std::string_view vector_id, std::string_view check) {
    ++passed_;
    std::cout << "PASS vector=" << vector_id << " check=" << check << '\n';
  }

  void Fail(std::string_view vector_id, std::string_view check, std::string_view detail) {
    ++failed_;
    std::cerr << "FAIL vector=" << vector_id << " check=" << check;
    if (!detail.empty()) {
      std::cerr << " detail=" << detail;
    }
    std::cerr << '\n';
  }

  void Skip(std::string_view vector_id, std::string_view check, std::string_view detail) {
    ++skipped_;
    std::cout << "SKIP vector=" << vector_id << " check=" << check << " detail=" << detail << '\n';
  }

  [[nodiscard]] bool Passed() const noexcept { return failed_ == 0U; }
  [[nodiscard]] std::size_t PassedCount() const noexcept { return passed_; }
  [[nodiscard]] std::size_t FailedCount() const noexcept { return failed_; }
  [[nodiscard]] std::size_t SkippedCount() const noexcept { return skipped_; }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
  std::size_t skipped_ = 0U;
};

bool TextEquals(std::string_view actual, std::string_view expected) noexcept {
  return actual.size() == expected.size() &&
         std::equal(actual.begin(), actual.end(), expected.begin(), expected.end());
}

bool BytesEqual(ByteView actual, const std::vector<std::uint8_t>& expected) noexcept {
  return actual.size == expected.size() && (actual.size == 0U || actual.data != nullptr) &&
         std::equal(expected.begin(), expected.end(), actual.data);
}

bool SlotsUntouched(const std::vector<DecodedFieldSlot>& slots) noexcept {
  for (const DecodedFieldSlot& slot : slots) {
    if (slot.field.plan_scope != nullptr || slot.field.message_index != kInvalidIndex ||
        slot.field.field_index != kInvalidIndex || slot.value_kind != LogicalValueKind::UINT64 ||
        slot.uint64_value != 0U || slot.bytes_value.data != nullptr ||
        slot.bytes_value.size != 0U || slot.enum_value.known || slot.enum_value.raw_value != 0U ||
        slot.enum_value.reference.plan_scope != nullptr ||
        slot.enum_value.reference.message_index != kInvalidIndex ||
        slot.enum_value.reference.field_index != kInvalidIndex ||
        slot.enum_value.reference.entry_index != kInvalidIndex) {
      return false;
    }
  }
  return true;
}

bool ParseArguments(int argc, char** argv, Arguments& output) {
  if (argc == 2 && std::string_view{argv[1]} == "--help") {
    return false;
  }
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if ((argument == "--config" || argument == "--corpus-root") && index + 1 < argc) {
      const std::filesystem::path value{argv[++index]};
      if (argument == "--config") {
        output.config_path = value;
      } else {
        output.corpus_root = value;
      }
      continue;
    }
    return false;
  }
  return !output.config_path.empty() && !output.corpus_root.empty();
}

std::size_t FindPipelineIndex(const PlanBundle& plan, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < plan.Pipelines().size(); ++index) {
    if (TextEquals(plan.Pipelines()[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindMessageIndex(const PlanBundle& plan, std::string_view id) noexcept {
  for (std::size_t index = 0U; index < plan.Messages().size(); ++index) {
    if (TextEquals(plan.Messages()[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindFieldIndex(const pae::protocol_plan::FrozenMessagePlan& message,
                           std::string_view id) noexcept {
  for (std::size_t index = 0U; index < message.fields.size(); ++index) {
    if (TextEquals(message.fields[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

std::size_t FindEnumIndex(const pae::protocol_plan::FrozenFieldPlan& field,
                          std::string_view id) noexcept {
  for (std::size_t index = 0U; index < field.enum_entries.size(); ++index) {
    if (TextEquals(field.enum_entries[index].id, id)) {
      return index;
    }
  }
  return kInvalidIndex;
}

bool IsEvidenceGradeAllowed(std::string_view grade) noexcept {
  return std::find(kEvidenceGrades.begin(), kEvidenceGrades.end(), grade) != kEvidenceGrades.end();
}

bool IsProtocolGoldenEvidence(const ManifestEntry& entry) noexcept {
  const bool authority_ok =
      entry.authority_class == "CONTRACT_FACT" || entry.authority_class == "OBSERVED_CAPTURE";
  const bool independently_reviewed = entry.review_status == "INDEPENDENT_PROTOCOL_REVIEWED" ||
                                      entry.review_status == "INDEPENDENT_ENGINEERING_REVIEWED";
  return authority_ok && independently_reviewed;
}

bool ResolveCorpusPath(const std::filesystem::path& root, const std::filesystem::path& relative,
                       std::filesystem::path& output, std::string& error) {
  std::error_code code;
  const std::filesystem::path canonical_root = std::filesystem::weakly_canonical(root, code);
  if (code) {
    error = "cannot canonicalize corpus root: " + root.generic_string();
    return false;
  }
  const std::filesystem::path candidate = std::filesystem::weakly_canonical(root / relative, code);
  if (code) {
    error = "cannot canonicalize corpus path: " + relative.generic_string();
    return false;
  }
  const std::filesystem::path relation = candidate.lexically_relative(canonical_root);
  if (relation.empty() || relation.is_absolute()) {
    error = "corpus path escapes root: " + relative.generic_string();
    return false;
  }
  for (const auto& component : relation) {
    if (component == "..") {
      error = "corpus path escapes root: " + relative.generic_string();
      return false;
    }
  }
  output = candidate;
  return true;
}

bool ComputeFileSha256(const std::filesystem::path& path, std::string& output, std::string& error) {
  std::ifstream stream{path, std::ios::binary};
  if (!stream) {
    error = "cannot open file for SHA-256: " + path.generic_string();
    return false;
  }
  Sha256 hash;
  std::array<char, 4096U> buffer{};
  while (stream) {
    stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    const std::streamsize count = stream.gcount();
    if (count > 0) {
      hash.Update(reinterpret_cast<const std::uint8_t*>(buffer.data()),
                  static_cast<std::size_t>(count));
    }
  }
  if (!stream.eof()) {
    error = "cannot read file for SHA-256: " + path.generic_string();
    return false;
  }
  const auto digest = hash.Final();
  std::ostringstream text;
  text << std::uppercase << std::hex << std::setfill('0');
  for (const std::uint8_t value : digest) {
    text << std::setw(2) << static_cast<unsigned int>(value);
  }
  output = text.str();
  return true;
}

bool LoadVector(const std::filesystem::path& corpus_root, const ManifestEntry& entry,
                LoadedVector& output, std::string& error) {
  std::filesystem::path frame_path;
  std::filesystem::path decode_path;
  std::filesystem::path encode_path;
  if (!ResolveCorpusPath(corpus_root, entry.frame_file, frame_path, error) ||
      !ResolveCorpusPath(corpus_root, entry.decode_expected_file, decode_path, error) ||
      !ResolveCorpusPath(corpus_root, entry.encode_input_file, encode_path, error)) {
    return false;
  }
  output.manifest = entry;
  return ReadHexFile(frame_path, output.frame, error) &&
         ReadDecodeExpected(decode_path, output.decode_expected, error) &&
         ReadEncodeInput(encode_path, output.encode_input, error);
}

bool BuildEncodeValues(const PlanBundle& plan, std::size_t message_index,
                       const std::vector<EncodeInputRow>& rows, OwnedEncodeValues& output,
                       std::string& error) {
  if (message_index >= plan.Messages().size()) {
    error = "message index is outside PlanBundle";
    return false;
  }
  const auto& message = plan.Messages()[message_index];
  output.byte_storage.clear();
  output.values.clear();
  output.byte_storage.reserve(rows.size());
  output.values.reserve(rows.size() + 1U);
  for (const EncodeInputRow& row : rows) {
    const std::size_t field_index = FindFieldIndex(message, row.field_id);
    if (field_index == kInvalidIndex) {
      error = "encode input references unknown field '" + row.field_id + "'";
      return false;
    }
    const auto& field = message.fields[field_index];
    if (field.encode_source != EncodeSource::INPUT) {
      error = "encode input supplies constant field '" + row.field_id + "'";
      return false;
    }

    EncodeFieldValue value;
    value.field = FieldRef{&plan, message_index, field_index};
    if (row.value_kind == "UINT64") {
      value.value_kind = LogicalValueKind::UINT64;
      if (!ParseUnsignedDecimal(row.logical_value, value.uint64_value, error)) {
        return false;
      }
    } else if (row.value_kind == "BYTES") {
      value.value_kind = LogicalValueKind::BYTES;
      output.byte_storage.emplace_back();
      if (!ParseHexBytes(row.logical_value, output.byte_storage.back(), error)) {
        return false;
      }
      const auto& bytes = output.byte_storage.back();
      value.bytes_value = ByteView{bytes.data(), bytes.size()};
    } else if (row.value_kind == "ENUM") {
      value.value_kind = LogicalValueKind::ENUM;
      const std::size_t enum_index = FindEnumIndex(field, row.logical_value);
      if (enum_index == kInvalidIndex) {
        error = "encode input references unknown enum entry '" + row.logical_value + "'";
        return false;
      }
      value.enum_value = EnumValueRef{&plan, message_index, field_index, enum_index};
    } else {
      error = "unsupported encode value kind '" + row.value_kind + "'";
      return false;
    }
    output.values.push_back(value);
  }
  return true;
}

bool CheckManifestBinding(const PlanBundle& plan, const LoadedVector& vector, std::string& error) {
  const ManifestEntry& entry = vector.manifest;
  const std::size_t pipeline_index = FindPipelineIndex(plan, entry.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, entry.message_id);
  if (entry.protocol_id != plan.ProtocolId() || entry.protocol_version != plan.ProtocolVersion() ||
      pipeline_index == kInvalidIndex || message_index == kInvalidIndex ||
      !IsEvidenceGradeAllowed(entry.authority_class)) {
    error = "protocol identity, stable ID, or evidence grade mismatch";
    return false;
  }
  const auto& pipeline = plan.Pipelines()[pipeline_index];
  const auto& message = plan.Messages()[message_index];
  if (!TextEquals(pipeline.direction_id, entry.direction_id) ||
      !TextEquals(message.direction_id, entry.direction_id) ||
      std::find(pipeline.message_indices.begin(), pipeline.message_indices.end(), message_index) ==
          pipeline.message_indices.end() ||
      vector.frame.size() != message.frame_length_bytes) {
    error = "direction, pipeline membership, or frame length mismatch";
    return false;
  }
  return true;
}

bool CheckFrameSha256(const std::filesystem::path& corpus_root, const LoadedVector& vector,
                      std::string& error) {
  std::filesystem::path frame_path;
  if (!ResolveCorpusPath(corpus_root, vector.manifest.frame_file, frame_path, error)) {
    return false;
  }
  std::string actual;
  if (!ComputeFileSha256(frame_path, actual, error)) {
    return false;
  }
  if (actual != vector.manifest.frame_file_sha256) {
    error = "frame SHA-256 mismatch; expected=" + vector.manifest.frame_file_sha256 +
            " actual=" + actual;
    return false;
  }
  return true;
}

bool CheckDecodeExact(const PlanBundle& plan, const LoadedVector& vector, std::string& error) {
  const std::size_t pipeline_index = FindPipelineIndex(plan, vector.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, vector.manifest.message_id);
  if (pipeline_index == kInvalidIndex || message_index == kInvalidIndex) {
    error = "stable ID cannot be resolved";
    return false;
  }
  const auto& message = plan.Messages()[message_index];
  std::vector<DecodedFieldSlot> slots(message.fields.size());
  ExecutionWorkspace workspace{plan};
  const auto result = DecodeCompleteRecord(plan, workspace, pipeline_index,
                                           ByteView{vector.frame.data(), vector.frame.size()},
                                           slots.data(), slots.size());
  if (result.status != CodecStatus::OK || result.message_index != message_index ||
      result.field_count != message.fields.size() || result.tainted ||
      vector.decode_expected.size() != message.fields.size()) {
    error = "Decode result metadata mismatch";
    return false;
  }

  std::vector<bool> seen(message.fields.size(), false);
  for (const DecodeExpectedRow& expected : vector.decode_expected) {
    const std::size_t field_index = FindFieldIndex(message, expected.field_id);
    if (field_index == kInvalidIndex || seen[field_index]) {
      error = "Decode expected field set is invalid";
      return false;
    }
    seen[field_index] = true;
    const auto& field = message.fields[field_index];
    const DecodedFieldSlot& slot = slots[field_index];
    if (slot.field.plan_scope != &plan || slot.field.message_index != message_index ||
        slot.field.field_index != field_index) {
      error = "decoded FieldRef mismatch for " + expected.field_id;
      return false;
    }

    if (expected.value_kind == "UINT64") {
      std::uint64_t logical = 0U;
      std::uint64_t raw = 0U;
      if (field.value_type != ValueType::UINT64 || slot.value_kind != LogicalValueKind::UINT64 ||
          !ParseUnsignedDecimal(expected.logical_value, logical, error) ||
          !ParseUnsignedDecimal(expected.raw_value, raw, error) || logical != raw ||
          slot.uint64_value != logical) {
        error = "decoded UINT64 mismatch for " + expected.field_id;
        return false;
      }
    } else if (expected.value_kind == "BYTES") {
      std::vector<std::uint8_t> logical;
      std::vector<std::uint8_t> raw;
      if (field.value_type != ValueType::BYTES || slot.value_kind != LogicalValueKind::BYTES ||
          !ParseHexBytes(expected.logical_value, logical, error) ||
          !ParseHexBytes(expected.raw_value, raw, error) || logical != raw ||
          !BytesEqual(slot.bytes_value, logical)) {
        error = "decoded BYTES mismatch for " + expected.field_id;
        return false;
      }
    } else if (expected.value_kind == "ENUM") {
      const std::size_t enum_index = FindEnumIndex(field, expected.logical_value);
      std::uint64_t raw = 0U;
      if (field.value_type != ValueType::ENUM || slot.value_kind != LogicalValueKind::ENUM ||
          enum_index == kInvalidIndex || !ParseUnsignedDecimal(expected.raw_value, raw, error) ||
          !slot.enum_value.known || slot.enum_value.raw_value != raw ||
          slot.enum_value.reference.plan_scope != &plan ||
          slot.enum_value.reference.message_index != message_index ||
          slot.enum_value.reference.field_index != field_index ||
          slot.enum_value.reference.entry_index != enum_index ||
          field.enum_entries[enum_index].raw_value != raw) {
        error = "decoded ENUM mismatch for " + expected.field_id;
        return false;
      }
    } else {
      error = "unsupported Decode expected value kind";
      return false;
    }
  }
  return std::all_of(seen.begin(), seen.end(), [](bool value) { return value; });
}

bool CheckEncodeExact(const PlanBundle& plan, const LoadedVector& vector, bool repeat,
                      std::string& error) {
  const std::size_t pipeline_index = FindPipelineIndex(plan, vector.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, vector.manifest.message_id);
  OwnedEncodeValues owned;
  if (pipeline_index == kInvalidIndex || message_index == kInvalidIndex ||
      !BuildEncodeValues(plan, message_index, vector.encode_input, owned, error)) {
    return false;
  }
  std::vector<std::uint8_t> output(vector.frame.size(), 0xCCU);
  std::vector<std::uint8_t> repeated(vector.frame.size(), 0x55U);
  ExecutionWorkspace workspace{plan};
  const auto first =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, owned.values.data(),
                           owned.values.size(), MutableByteBuffer{output.data(), output.size()});
  if (first.status != CodecStatus::OK || first.bytes_written != vector.frame.size() ||
      output != vector.frame) {
    error = "Encode output does not exactly match stored frame";
    return false;
  }
  if (!repeat) {
    return true;
  }
  const auto second = EncodeCompleteRecord(plan, workspace, pipeline_index, message_index,
                                           owned.values.data(), owned.values.size(),
                                           MutableByteBuffer{repeated.data(), repeated.size()});
  if (second.status != CodecStatus::OK || second.bytes_written != vector.frame.size() ||
      repeated != output) {
    error = "repeated Encode is not deterministic";
    return false;
  }
  return true;
}

bool ExpectDecodeFailure(const PlanBundle& plan, std::size_t pipeline_index,
                         const std::vector<std::uint8_t>& frame, CodecStatus status,
                         std::string& error) {
  const std::size_t capacity = plan.GetExecutionResourceLayout().max_fields_per_message;
  std::vector<DecodedFieldSlot> slots(capacity);
  ExecutionWorkspace workspace{plan};
  const auto result =
      DecodeCompleteRecord(plan, workspace, pipeline_index, ByteView{frame.data(), frame.size()},
                           slots.data(), slots.size());
  if (result.status != status || result.field_count != 0U || !SlotsUntouched(slots)) {
    error = "Decode failure status or atomicity mismatch";
    return false;
  }
  return true;
}

bool OutputIsUntouched(const std::vector<std::uint8_t>& output, std::uint8_t sentinel) noexcept {
  return std::all_of(output.begin(), output.end(),
                     [sentinel](std::uint8_t value) { return value == sentinel; });
}

bool ExpectEncodeFailure(const PlanBundle& plan, std::size_t pipeline_index,
                         std::size_t message_index, const std::vector<EncodeFieldValue>& values,
                         std::size_t output_capacity, CodecStatus status, std::string& error) {
  constexpr std::uint8_t kSentinel = 0xCCU;
  std::vector<std::uint8_t> output(output_capacity, kSentinel);
  ExecutionWorkspace workspace{plan};
  const auto result =
      EncodeCompleteRecord(plan, workspace, pipeline_index, message_index, values.data(),
                           values.size(), MutableByteBuffer{output.data(), output.size()});
  if (result.status != status || result.bytes_written != 0U ||
      !OutputIsUntouched(output, kSentinel)) {
    error = "Encode failure status or fail-closed output mismatch";
    return false;
  }
  return true;
}

bool FixedMatcherOverlaps(const pae::protocol_plan::FrozenMessagePlan& message,
                          const pae::protocol_plan::FrozenFieldPlan& field) noexcept {
  const std::uint64_t field_end = field.byte_offset + field.byte_width;
  for (const auto& matcher : message.matchers) {
    if (matcher.kind != MatcherKind::FIXED_BYTES) {
      continue;
    }
    const std::uint64_t matcher_end = matcher.byte_offset + matcher.bytes.size();
    if (field.byte_offset < matcher_end && matcher.byte_offset < field_end) {
      return true;
    }
  }
  return false;
}

bool StoreUnsigned(std::uint64_t value, std::uint8_t* output, std::size_t width,
                   ByteOrder byte_order) noexcept {
  if (byte_order == ByteOrder::BIG) {
    for (std::size_t index = 0U; index < width; ++index) {
      output[width - index - 1U] = static_cast<std::uint8_t>(value & 0xFFU);
      value >>= 8U;
    }
    return true;
  }
  if (byte_order == ByteOrder::LITTLE) {
    for (std::size_t index = 0U; index < width; ++index) {
      output[index] = static_cast<std::uint8_t>(value & 0xFFU);
      value >>= 8U;
    }
    return true;
  }
  return false;
}

void RunNegativeChecks(const PlanBundle& plan, const LoadedVector& vector, CheckRunner& runner,
                       Coverage& coverage) {
  const std::string& vector_id = vector.manifest.vector_id;
  const std::size_t pipeline_index = FindPipelineIndex(plan, vector.manifest.pipeline_id);
  const std::size_t message_index = FindMessageIndex(plan, vector.manifest.message_id);
  const auto& message = plan.Messages()[message_index];
  std::string error;

  std::vector<std::uint8_t> short_frame = vector.frame;
  short_frame.pop_back();
  if (ExpectDecodeFailure(plan, pipeline_index, short_frame, CodecStatus::UNKNOWN_MESSAGE, error)) {
    runner.Pass(vector_id, "decode_truncated_complete_record");
  } else {
    runner.Fail(vector_id, "decode_truncated_complete_record", error);
  }

  error.clear();
  std::vector<std::uint8_t> long_frame = vector.frame;
  long_frame.push_back(0U);
  if (ExpectDecodeFailure(plan, pipeline_index, long_frame, CodecStatus::UNKNOWN_MESSAGE, error)) {
    runner.Pass(vector_id, "decode_extra_complete_record");
  } else {
    runner.Fail(vector_id, "decode_extra_complete_record", error);
  }

  const auto fixed =
      std::find_if(message.matchers.begin(), message.matchers.end(), [](const auto& matcher) {
        return matcher.kind == MatcherKind::FIXED_BYTES && !matcher.bytes.empty();
      });
  if (fixed == message.matchers.end()) {
    runner.Skip(vector_id, "decode_wrong_fixed_bytes", "message has no fixed-bytes matcher");
  } else {
    error.clear();
    std::vector<std::uint8_t> wrong = vector.frame;
    wrong[static_cast<std::size_t>(fixed->byte_offset)] ^= 0xFFU;
    if (ExpectDecodeFailure(plan, pipeline_index, wrong, CodecStatus::UNKNOWN_MESSAGE, error)) {
      coverage.fixed_bytes_failure = true;
      runner.Pass(vector_id, "decode_wrong_fixed_bytes");
    } else {
      runner.Fail(vector_id, "decode_wrong_fixed_bytes", error);
    }
  }

  const auto enum_field =
      std::find_if(message.fields.begin(), message.fields.end(), [&message](const auto& field) {
        return field.value_type == ValueType::ENUM &&
               field.unknown_enum_policy == UnknownEnumPolicy::REJECT &&
               !FixedMatcherOverlaps(message, field);
      });
  if (enum_field == message.fields.end()) {
    runner.Skip(vector_id, "decode_unknown_enum_rejected", "no mutable rejecting ENUM field");
  } else {
    const auto is_known = [&enum_field](std::uint64_t raw) {
      return std::any_of(enum_field->enum_entries.begin(), enum_field->enum_entries.end(),
                         [raw](const auto& entry) { return entry.raw_value == raw; });
    };
    const std::uint64_t maximum = enum_field->byte_width == 8U
                                      ? std::numeric_limits<std::uint64_t>::max()
                                      : (std::uint64_t{1U} << (enum_field->byte_width * 8U)) - 1U;
    const std::uint64_t search_limit =
        std::min(maximum, static_cast<std::uint64_t>(enum_field->enum_entries.size()));
    std::uint64_t unknown = 0U;
    while (unknown <= search_limit && is_known(unknown)) {
      ++unknown;
    }
    if (unknown > search_limit) {
      runner.Skip(vector_id, "decode_unknown_enum_rejected",
                  "ENUM exhausts all representable raw values");
    } else {
      std::vector<std::uint8_t> wrong = vector.frame;
      StoreUnsigned(unknown, wrong.data() + static_cast<std::size_t>(enum_field->byte_offset),
                    static_cast<std::size_t>(enum_field->byte_width), enum_field->byte_order);
      error.clear();
      if (ExpectDecodeFailure(plan, pipeline_index, wrong, CodecStatus::UNKNOWN_ENUM_VALUE,
                              error)) {
        coverage.unknown_enum_failure = true;
        runner.Pass(vector_id, "decode_unknown_enum_rejected");
      } else {
        runner.Fail(vector_id, "decode_unknown_enum_rejected", error);
      }
    }
  }

  OwnedEncodeValues owned;
  if (!BuildEncodeValues(plan, message_index, vector.encode_input, owned, error) ||
      owned.values.empty()) {
    runner.Fail(vector_id, "encode_negative_fixture_ready", error);
    return;
  }

  std::vector<EncodeFieldValue> values = owned.values;
  values.pop_back();
  error.clear();
  if (ExpectEncodeFailure(plan, pipeline_index, message_index, values, vector.frame.size(),
                          CodecStatus::MISSING_FIELD, error)) {
    coverage.missing_field_failure = true;
    runner.Pass(vector_id, "encode_missing_field_fail_closed");
  } else {
    runner.Fail(vector_id, "encode_missing_field_fail_closed", error);
  }

  if (owned.values.size() < message.fields.size()) {
    values = owned.values;
    values.push_back(values.front());
    error.clear();
    if (ExpectEncodeFailure(plan, pipeline_index, message_index, values, vector.frame.size(),
                            CodecStatus::DUPLICATE_FIELD, error)) {
      coverage.duplicate_field_failure = true;
      runner.Pass(vector_id, "encode_duplicate_field_fail_closed");
    } else {
      runner.Fail(vector_id, "encode_duplicate_field_fail_closed", error);
    }
  } else {
    runner.Skip(vector_id, "encode_duplicate_field_fail_closed",
                "descriptor limit leaves no duplicate probe capacity");
  }

  values = owned.values;
  values.front().value_kind = values.front().value_kind == LogicalValueKind::UINT64
                                  ? LogicalValueKind::BYTES
                                  : LogicalValueKind::UINT64;
  error.clear();
  if (ExpectEncodeFailure(plan, pipeline_index, message_index, values, vector.frame.size(),
                          CodecStatus::TYPE_MISMATCH, error)) {
    coverage.type_mismatch_failure = true;
    runner.Pass(vector_id, "encode_type_mismatch_fail_closed");
  } else {
    runner.Fail(vector_id, "encode_type_mismatch_fail_closed", error);
  }

  const auto overflow_value = std::find_if(
      owned.values.begin(), owned.values.end(), [&message](const EncodeFieldValue& value) {
        const auto& field = message.fields[value.field.field_index];
        return value.value_kind == LogicalValueKind::UINT64 && field.byte_width < 8U;
      });
  if (overflow_value == owned.values.end()) {
    runner.Skip(vector_id, "encode_width_overflow_fail_closed", "no narrow UINT64 input field");
  } else {
    values = owned.values;
    const std::size_t value_index =
        static_cast<std::size_t>(std::distance(owned.values.begin(), overflow_value));
    const auto& field = message.fields[overflow_value->field.field_index];
    values[value_index].uint64_value = std::uint64_t{1U} << (field.byte_width * 8U);
    error.clear();
    if (ExpectEncodeFailure(plan, pipeline_index, message_index, values, vector.frame.size(),
                            CodecStatus::VALUE_NOT_REPRESENTABLE, error)) {
      coverage.width_overflow_failure = true;
      runner.Pass(vector_id, "encode_width_overflow_fail_closed");
    } else {
      runner.Fail(vector_id, "encode_width_overflow_fail_closed", error);
    }
  }

  error.clear();
  if (ExpectEncodeFailure(plan, pipeline_index, message_index, owned.values,
                          vector.frame.size() - 1U, CodecStatus::BUFFER_TOO_SMALL, error)) {
    coverage.buffer_too_small_failure = true;
    runner.Pass(vector_id, "encode_buffer_too_small_fail_closed");
  } else {
    runner.Fail(vector_id, "encode_buffer_too_small_fail_closed", error);
  }
}

void RecordCoverage(std::string_view id, bool covered, CheckRunner& runner) {
  if (covered) {
    runner.Pass("corpus", id);
  } else {
    runner.Fail("corpus", id, "required negative-path coverage was not exercised");
  }
}

}  // namespace

int main(int argc, char** argv) {
  Arguments arguments;
  if (!ParseArguments(argc, argv, arguments)) {
    std::cerr << "usage: pae_protocol_conformance_runner --config <protocol.pae.json> "
                 "--corpus-root <corpus-directory>\n";
    return 2;
  }

  std::string error;
  std::string json;
  if (!ReadTextFile(arguments.config_path, json, error)) {
    std::cerr << "CONFIG_LOAD_FAILED detail=" << error << '\n';
    return 2;
  }
  auto compile_result = CompileJsonToPlan(json);
  if (!compile_result.Succeeded()) {
    const auto* diagnostic = compile_result.Diagnostic();
    std::cerr << "CONFIG_COMPILE_FAILED";
    if (diagnostic != nullptr) {
      std::cerr << " pointer=" << diagnostic->json_pointer << " detail=" << diagnostic->detail;
    }
    std::cerr << '\n';
    return 2;
  }
  auto plan_owner = std::move(compile_result).TakePlan();
  const PlanBundle& plan = *plan_owner;

  std::vector<ManifestEntry> manifest;
  if (!ReadManifest(arguments.corpus_root / "manifest_v0.1.tsv", manifest, error)) {
    std::cerr << "MANIFEST_LOAD_FAILED detail=" << error << '\n';
    return 2;
  }

  CheckRunner runner;
  Coverage coverage;
  bool protocol_golden_evidence = true;
  std::size_t vector_count = 0U;
  for (const ManifestEntry& entry : manifest) {
    LoadedVector vector;
    error.clear();
    if (!LoadVector(arguments.corpus_root, entry, vector, error)) {
      runner.Fail(entry.vector_id, "fixture_load", error);
      protocol_golden_evidence = false;
      continue;
    }
    ++vector_count;
    protocol_golden_evidence = protocol_golden_evidence && IsProtocolGoldenEvidence(entry);
    std::cout << "VECTOR id=" << entry.vector_id << " evidence=" << entry.authority_class
              << " review=" << entry.review_status << '\n';

    error.clear();
    if (CheckManifestBinding(plan, vector, error)) {
      runner.Pass(entry.vector_id, "manifest_plan_binding");
    } else {
      runner.Fail(entry.vector_id, "manifest_plan_binding", error);
      continue;
    }

    error.clear();
    if (CheckFrameSha256(arguments.corpus_root, vector, error)) {
      runner.Pass(entry.vector_id, "frame_sha256");
    } else {
      runner.Fail(entry.vector_id, "frame_sha256", error);
    }

    error.clear();
    if (CheckDecodeExact(plan, vector, error)) {
      runner.Pass(entry.vector_id, "decode_exact");
    } else {
      runner.Fail(entry.vector_id, "decode_exact", error);
    }

    error.clear();
    if (CheckEncodeExact(plan, vector, false, error)) {
      runner.Pass(entry.vector_id, "encode_exact");
    } else {
      runner.Fail(entry.vector_id, "encode_exact", error);
    }

    error.clear();
    if (CheckEncodeExact(plan, vector, true, error)) {
      runner.Pass(entry.vector_id, "encode_repeat_deterministic");
    } else {
      runner.Fail(entry.vector_id, "encode_repeat_deterministic", error);
    }

    RunNegativeChecks(plan, vector, runner, coverage);
  }

  RecordCoverage("coverage_fixed_bytes_failure", coverage.fixed_bytes_failure, runner);
  RecordCoverage("coverage_unknown_enum_failure", coverage.unknown_enum_failure, runner);
  RecordCoverage("coverage_missing_field_failure", coverage.missing_field_failure, runner);
  RecordCoverage("coverage_duplicate_field_failure", coverage.duplicate_field_failure, runner);
  RecordCoverage("coverage_type_mismatch_failure", coverage.type_mismatch_failure, runner);
  RecordCoverage("coverage_width_overflow_failure", coverage.width_overflow_failure, runner);
  RecordCoverage("coverage_buffer_too_small_failure", coverage.buffer_too_small_failure, runner);

  const bool engine_poc_pass = runner.Passed() && vector_count == manifest.size();
  const bool protocol_golden_pass = engine_poc_pass && protocol_golden_evidence;
  std::cout << "PROTOCOL_CONFORMANCE_SUMMARY vectors=" << vector_count
            << " passed=" << runner.PassedCount() << " failed=" << runner.FailedCount()
            << " skipped=" << runner.SkippedCount()
            << " ENGINE_POC_PASS=" << (engine_poc_pass ? "PASS" : "FAIL")
            << " PROTOCOL_GOLDEN_PASS=" << (protocol_golden_pass ? "PASS" : "NOT_SATISFIED")
            << '\n';
  return engine_poc_pass ? 0 : 1;
}
