#include "fixture_reader.h"

#include <charconv>
#include <cstddef>
#include <fstream>
#include <sstream>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace pae::protocol_core::test_support {
namespace {

using Row = std::vector<std::string>;

bool IsAsciiWhitespace(char value) noexcept {
  return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

int HexDigit(char value) noexcept {
  if (value >= '0' && value <= '9') {
    return value - '0';
  }
  if (value >= 'A' && value <= 'F') {
    return value - 'A' + 10;
  }
  if (value >= 'a' && value <= 'f') {
    return value - 'a' + 10;
  }
  return -1;
}

bool IsUpperHexDigest(std::string_view value) noexcept {
  if (value.size() != 64U) {
    return false;
  }
  for (const char character : value) {
    if (!((character >= '0' && character <= '9') || (character >= 'A' && character <= 'F'))) {
      return false;
    }
  }
  return true;
}

bool IsSafeRelativePath(const std::filesystem::path& path) {
  if (path.empty() || path.is_absolute() || path.has_root_path()) {
    return false;
  }
  for (const auto& component : path) {
    if (component == ".." || component == ".") {
      return false;
    }
  }
  return true;
}

Row SplitTabs(const std::string& line) {
  Row columns;
  std::size_t begin = 0U;
  while (true) {
    const std::size_t separator = line.find('\t', begin);
    if (separator == std::string::npos) {
      columns.emplace_back(line.substr(begin));
      return columns;
    }
    columns.emplace_back(line.substr(begin, separator - begin));
    begin = separator + 1U;
  }
}

bool ReadTsv(const std::filesystem::path& path, const Row& expected_header, std::vector<Row>& rows,
             std::string& error) {
  std::string input;
  if (!ReadTextFile(path, input, error)) {
    return false;
  }

  std::istringstream stream{input};
  std::string line;
  std::size_t line_number = 0U;
  bool header_seen = false;
  while (std::getline(stream, line)) {
    ++line_number;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.empty()) {
      continue;
    }

    Row columns = SplitTabs(line);
    if (!header_seen) {
      header_seen = true;
      if (columns != expected_header) {
        error = "unexpected TSV header in " + path.generic_string();
        return false;
      }
      continue;
    }
    if (columns.size() != expected_header.size()) {
      error = "unexpected TSV column count at " + path.generic_string() + ":" +
              std::to_string(line_number);
      return false;
    }
    rows.emplace_back(std::move(columns));
  }
  if (!stream.eof()) {
    error = "cannot read TSV file: " + path.generic_string();
    return false;
  }
  if (!header_seen) {
    error = "TSV file has no header: " + path.generic_string();
    return false;
  }
  if (rows.empty()) {
    error = "TSV file has no data rows: " + path.generic_string();
    return false;
  }
  return true;
}

bool InsertUnique(std::unordered_set<std::string>& values, const std::string& value,
                  std::string_view kind, const std::filesystem::path& path, std::string& error) {
  if (value.empty()) {
    error = std::string{kind} + " is empty in " + path.generic_string();
    return false;
  }
  if (!values.insert(value).second) {
    error = "duplicate " + std::string{kind} + " '" + value + "' in " + path.generic_string();
    return false;
  }
  return true;
}

}  // namespace

bool ReadTextFile(const std::filesystem::path& path, std::string& output, std::string& error) {
  std::ifstream stream{path, std::ios::binary};
  if (!stream) {
    error = "cannot open fixture: " + path.generic_string();
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    error = "cannot read fixture: " + path.generic_string();
    return false;
  }
  output = buffer.str();
  return true;
}

bool ReadManifest(const std::filesystem::path& path, std::vector<ManifestEntry>& output,
                  std::string& error) {
  const Row header{"vector_id",        "protocol_id",  "protocol_version",     "pipeline_id",
                   "message_id",       "direction_id", "authority_class",      "review_status",
                   "source_ref",       "frame_file",   "decode_expected_file", "encode_input_file",
                   "frame_file_sha256"};
  std::vector<Row> rows;
  if (!ReadTsv(path, header, rows, error)) {
    return false;
  }

  std::unordered_set<std::string> vector_ids;
  output.clear();
  output.reserve(rows.size());
  for (const Row& row : rows) {
    ManifestEntry entry;
    entry.vector_id = row[0];
    entry.protocol_id = row[1];
    entry.protocol_version = row[2];
    entry.pipeline_id = row[3];
    entry.message_id = row[4];
    entry.direction_id = row[5];
    entry.authority_class = row[6];
    entry.review_status = row[7];
    entry.source_ref = row[8];
    entry.frame_file = std::filesystem::path{row[9]};
    entry.decode_expected_file = std::filesystem::path{row[10]};
    entry.encode_input_file = std::filesystem::path{row[11]};
    entry.frame_file_sha256 = row[12];

    if (!InsertUnique(vector_ids, entry.vector_id, "vector_id", path, error)) {
      return false;
    }
    if (entry.protocol_id.empty() || entry.protocol_version.empty() || entry.pipeline_id.empty() ||
        entry.message_id.empty() || entry.direction_id.empty() || entry.authority_class.empty() ||
        entry.review_status.empty() || entry.source_ref.empty()) {
      error = "manifest identity or authority column is empty for " + entry.vector_id;
      return false;
    }
    if (!IsSafeRelativePath(entry.frame_file) || !IsSafeRelativePath(entry.decode_expected_file) ||
        !IsSafeRelativePath(entry.encode_input_file)) {
      error = "manifest contains an unsafe fixture path for " + entry.vector_id;
      return false;
    }
    if (!IsUpperHexDigest(entry.frame_file_sha256)) {
      error = "manifest SHA-256 must be 64 uppercase hexadecimal digits for " + entry.vector_id;
      return false;
    }
    output.emplace_back(std::move(entry));
  }
  return true;
}

bool ParseHexBytes(std::string_view text, std::vector<std::uint8_t>& output, std::string& error) {
  output.clear();
  std::size_t cursor = 0U;
  while (cursor < text.size()) {
    while (cursor < text.size() && IsAsciiWhitespace(text[cursor])) {
      ++cursor;
    }
    if (cursor == text.size()) {
      break;
    }
    if (text.size() - cursor < 2U) {
      error = "hex byte token is truncated";
      return false;
    }
    const int high = HexDigit(text[cursor]);
    const int low = HexDigit(text[cursor + 1U]);
    if (high < 0 || low < 0) {
      error = "hex byte token contains a non-hexadecimal digit";
      return false;
    }
    cursor += 2U;
    if (cursor < text.size() && !IsAsciiWhitespace(text[cursor])) {
      error = "hex byte token must contain exactly two digits";
      return false;
    }
    output.push_back(static_cast<std::uint8_t>((high << 4U) | low));
  }
  if (output.empty()) {
    error = "hex byte sequence is empty";
    return false;
  }
  return true;
}

bool ReadHexFile(const std::filesystem::path& path, std::vector<std::uint8_t>& output,
                 std::string& error) {
  std::string input;
  if (!ReadTextFile(path, input, error)) {
    return false;
  }
  if (!ParseHexBytes(input, output, error)) {
    error = path.generic_string() + ": " + error;
    return false;
  }
  return true;
}

bool ReadDecodeExpected(const std::filesystem::path& path, std::vector<DecodeExpectedRow>& output,
                        std::string& error) {
  const Row header{"field_id", "value_kind", "logical_value", "raw_value"};
  std::vector<Row> rows;
  if (!ReadTsv(path, header, rows, error)) {
    return false;
  }
  std::unordered_set<std::string> field_ids;
  output.clear();
  output.reserve(rows.size());
  for (const Row& row : rows) {
    if (!InsertUnique(field_ids, row[0], "field_id", path, error)) {
      return false;
    }
    if (row[1] != "UINT64" && row[1] != "BYTES" && row[1] != "ENUM") {
      error = "unsupported decode value_kind '" + row[1] + "' in " + path.generic_string();
      return false;
    }
    if (row[2].empty() || row[3].empty()) {
      error = "decode expected value is empty for field '" + row[0] + "'";
      return false;
    }
    output.push_back(DecodeExpectedRow{row[0], row[1], row[2], row[3]});
  }
  return true;
}

bool ReadEncodeInput(const std::filesystem::path& path, std::vector<EncodeInputRow>& output,
                     std::string& error) {
  const Row header{"field_id", "value_kind", "logical_value"};
  std::vector<Row> rows;
  if (!ReadTsv(path, header, rows, error)) {
    return false;
  }
  std::unordered_set<std::string> field_ids;
  output.clear();
  output.reserve(rows.size());
  for (const Row& row : rows) {
    if (!InsertUnique(field_ids, row[0], "field_id", path, error)) {
      return false;
    }
    if (row[1] != "UINT64" && row[1] != "BYTES" && row[1] != "ENUM") {
      error = "unsupported encode value_kind '" + row[1] + "' in " + path.generic_string();
      return false;
    }
    if (row[2].empty()) {
      error = "encode input value is empty for field '" + row[0] + "'";
      return false;
    }
    output.push_back(EncodeInputRow{row[0], row[1], row[2]});
  }
  return true;
}

bool ParseUnsignedDecimal(std::string_view text, std::uint64_t& output, std::string& error) {
  if (text.empty()) {
    error = "unsigned decimal value is empty";
    return false;
  }
  std::uint64_t value = 0U;
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, 10);
  if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
    error = "invalid canonical unsigned decimal value: " + std::string{text};
    return false;
  }
  output = value;
  return true;
}

}  // namespace pae::protocol_core::test_support
