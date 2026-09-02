#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pae::protocol_core::test_support {

struct ManifestEntry {
  std::string vector_id;
  std::string protocol_id;
  std::string protocol_version;
  std::string pipeline_id;
  std::string message_id;
  std::string direction_id;
  std::string authority_class;
  std::string review_status;
  std::string source_ref;
  std::filesystem::path frame_file;
  std::filesystem::path decode_expected_file;
  std::filesystem::path encode_input_file;
  std::string frame_file_sha256;
};

struct DecodeExpectedRow {
  std::string field_id;
  std::string value_kind;
  std::string logical_value;
  std::string raw_value;
};

struct EncodeInputRow {
  std::string field_id;
  std::string value_kind;
  std::string logical_value;
};

bool ReadTextFile(const std::filesystem::path& path, std::string& output, std::string& error);

bool ReadManifest(const std::filesystem::path& path, std::vector<ManifestEntry>& output,
                  std::string& error);

bool ReadHexFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                 std::string& error);

bool ReadDecodeExpected(const std::filesystem::path& path, std::vector<DecodeExpectedRow>& output,
                        std::string& error);

bool ReadEncodeInput(const std::filesystem::path& path, std::vector<EncodeInputRow>& output,
                     std::string& error);

bool ParseUnsignedDecimal(std::string_view text, std::uint64_t& output, std::string& error);

bool ParseHexBytes(std::string_view text, std::vector<std::uint8_t>& output, std::string& error);

}  // namespace pae::protocol_core::test_support
