#include <yaml.h>

#include <charconv>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

struct Limits {
  std::size_t input = 16384;
  std::size_t output = 32768;
  std::size_t scalar = 4096;
  std::size_t nodes = 512;
  std::size_t depth = 16;
};

struct Failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Node {
  enum class Kind { kObject, kArray, kString, kLiteral } kind;
  std::string value;
  std::vector<std::pair<std::string, Node>> members;
  std::vector<Node> elements;
  std::size_t line = 0;
  std::size_t column = 0;
};

struct EventStats {
  std::size_t created = 0;
  std::size_t released = 0;
};

class Event {
 public:
  Event(yaml_event_t value, EventStats* stats) : value_(value), stats_(stats) {
    if (stats_ != nullptr) ++stats_->created;
  }
  ~Event() { Release(); }
  Event(const Event&) = delete;
  Event& operator=(const Event&) = delete;
  Event(Event&& other) noexcept : value_(other.value_), stats_(other.stats_), owned_(other.owned_) {
    other.owned_ = false;
  }
  Event& operator=(Event&& other) noexcept {
    if (this != &other) {
      Release();
      value_ = other.value_;
      stats_ = other.stats_;
      owned_ = other.owned_;
      other.owned_ = false;
    }
    return *this;
  }

  const yaml_event_t& value() const { return value_; }
  yaml_event_type_t type() const { return value_.type; }

 private:
  void Release() {
    if (owned_) {
      yaml_event_delete(&value_);
      if (stats_ != nullptr) ++stats_->released;
      owned_ = false;
    }
  }

  yaml_event_t value_{};
  EventStats* stats_ = nullptr;
  bool owned_ = true;
};

void Require(bool condition, const char* message) {
  if (!condition) throw Failure(message);
}

bool IsInteger(std::string_view value) {
  if (value.empty()) return false;
  std::size_t pos = value.front() == '-' ? 1 : 0;
  if (pos == value.size()) return false;
  if (value[pos] == '0') return pos + 1 == value.size() && pos == 0;
  if (value[pos] < '1' || value[pos] > '9') return false;
  for (++pos; pos < value.size(); ++pos) {
    if (value[pos] < '0' || value[pos] > '9') return false;
  }
  return true;
}

bool NumericLooking(std::string_view value) {
  if (value.empty()) return false;
  if (value.front() >= '0' && value.front() <= '9') return true;
  return (value.front() == '-' || value.front() == '+') && value.size() > 1 && value[1] >= '0' &&
         value[1] <= '9';
}

std::string ClassifyPlain(std::string_view value) {
  Require(!value.empty(), "implicit empty scalar forbidden");
  if (value == "null" || value == "true" || value == "false") return std::string(value);
  if (!NumericLooking(value)) return {};
  Require(IsInteger(value), "noncanonical numeric scalar");
  if (value.front() == '-') {
    std::int64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    Require(result.ec == std::errc() && result.ptr == value.data() + value.size(),
            "signed integer out of range");
  } else {
    std::uint64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    Require(result.ec == std::errc() && result.ptr == value.data() + value.size(),
            "unsigned integer out of range");
  }
  return std::string(value);
}

class Reader {
 public:
  Reader(std::string_view input, Limits limits, EventStats* stats)
      : limits_(limits), stats_(stats) {
    Require(input.size() <= limits.input, "input limit before parser");
    Require(input.substr(0, 3) != "\xEF\xBB\xBF", "UTF-8 BOM forbidden");
    Require(yaml_parser_initialize(&parser_) != 0, "parser initialization");
    initialized_ = true;
    yaml_parser_set_input_string(&parser_, reinterpret_cast<const unsigned char*>(input.data()),
                                 input.size());
  }

  ~Reader() {
    if (initialized_) yaml_parser_delete(&parser_);
  }

  Reader(const Reader&) = delete;
  Reader& operator=(const Reader&) = delete;

  Node Parse() {
    auto event = Next();
    Require(event.type() == YAML_STREAM_START_EVENT, "expected stream start");
    event = Next();
    Require(event.type() == YAML_DOCUMENT_START_EVENT, "expected document start");
    if (event.value().data.document_start.version_directive != nullptr) {
      const auto* version = event.value().data.document_start.version_directive;
      Require(version->major == 1 && version->minor == 2, "unsupported YAML version");
    }
    Require(event.value().data.document_start.tag_directives.start ==
                event.value().data.document_start.tag_directives.end,
            "tag directive forbidden");
    Node root = ParseNode(Next(), 1);
    Require(root.kind == Node::Kind::kObject, "root must be object");
    event = Next();
    Require(event.type() == YAML_DOCUMENT_END_EVENT, "expected document end");
    event = Next();
    Require(event.type() == YAML_STREAM_END_EVENT, "multiple documents forbidden");
    return root;
  }

 private:
  Event Next() {
    yaml_event_t event{};
    if (yaml_parser_parse(&parser_, &event) == 0) {
      const std::string message =
          parser_.problem == nullptr ? "YAML parse failure" : parser_.problem;
      throw Failure(message);
    }
    return Event(event, stats_);
  }

  Node ParseNode(Event event, std::size_t depth) {
    Require(depth <= limits_.depth, "depth limit during event parsing");
    Require(++nodes_ <= limits_.nodes, "node limit during event parsing");
    Node node{};
    const yaml_event_t& raw = event.value();
    node.line = raw.start_mark.line + 1;
    node.column = raw.start_mark.column + 1;
    if (event.type() == YAML_SCALAR_EVENT) {
      Require(raw.data.scalar.anchor == nullptr, "anchor forbidden");
      Require(raw.data.scalar.tag == nullptr, "tag forbidden");
      Require(raw.data.scalar.length <= limits_.scalar, "scalar limit during event parsing");
      node.value.assign(reinterpret_cast<const char*>(raw.data.scalar.value),
                        raw.data.scalar.length);
      if (raw.data.scalar.style == YAML_PLAIN_SCALAR_STYLE) {
        auto literal = ClassifyPlain(node.value);
        node.kind = literal.empty() ? Node::Kind::kString : Node::Kind::kLiteral;
      } else {
        node.kind = Node::Kind::kString;
      }
    } else if (event.type() == YAML_SEQUENCE_START_EVENT) {
      Require(raw.data.sequence_start.anchor == nullptr, "anchor forbidden");
      Require(raw.data.sequence_start.tag == nullptr, "tag forbidden");
      node.kind = Node::Kind::kArray;
      while (true) {
        auto child = Next();
        if (child.type() == YAML_SEQUENCE_END_EVENT) return node;
        node.elements.push_back(ParseNode(std::move(child), depth + 1));
      }
    } else if (event.type() == YAML_MAPPING_START_EVENT) {
      Require(raw.data.mapping_start.anchor == nullptr, "anchor forbidden");
      Require(raw.data.mapping_start.tag == nullptr, "tag forbidden");
      node.kind = Node::Kind::kObject;
      while (true) {
        auto key_event = Next();
        if (key_event.type() == YAML_MAPPING_END_EVENT) return node;
        const yaml_event_t& key_raw = key_event.value();
        const bool merge_key =
            key_event.type() == YAML_SCALAR_EVENT &&
            key_raw.data.scalar.style == YAML_PLAIN_SCALAR_STYLE &&
            key_raw.data.scalar.length == 2 &&
            std::string_view(reinterpret_cast<const char*>(key_raw.data.scalar.value), 2) == "<<";
        Require(!merge_key, "merge key forbidden");
        Node key = ParseNode(std::move(key_event), depth + 1);
        Require(key.kind == Node::Kind::kString, "mapping key must be string");
        for (const auto& member : node.members) {
          Require(member.first != key.value, "decoded duplicate key");
        }
        node.members.emplace_back(std::move(key.value), ParseNode(Next(), depth + 1));
      }
    } else {
      throw Failure("unsupported event or alias");
    }
    return node;
  }

  yaml_parser_t parser_{};
  Limits limits_;
  EventStats* stats_ = nullptr;
  std::size_t nodes_ = 0;
  bool initialized_ = false;
};

void Append(std::string& output, std::string_view text, std::size_t limit) {
  Require(text.size() <= limit && output.size() <= limit - text.size(), "output limit");
  output.append(text);
}

void AppendString(std::string& output, std::string_view value, std::size_t limit) {
  Append(output, "\"", limit);
  constexpr char hex[] = "0123456789abcdef";
  for (unsigned char byte : value) {
    switch (byte) {
      case '"':
        Append(output, "\\\"", limit);
        break;
      case '\\':
        Append(output, "\\\\", limit);
        break;
      case '\n':
        Append(output, "\\n", limit);
        break;
      case '\r':
        Append(output, "\\r", limit);
        break;
      case '\t':
        Append(output, "\\t", limit);
        break;
      default:
        if (byte < 0x20) {
          const char escaped[] = {'\\', 'u', '0', '0', hex[byte >> 4], hex[byte & 15]};
          Append(output, std::string_view(escaped, sizeof escaped), limit);
        } else {
          Append(output, std::string_view(reinterpret_cast<const char*>(&byte), 1), limit);
        }
    }
  }
  Append(output, "\"", limit);
}

void Emit(const Node& node, std::string& output, std::size_t limit) {
  switch (node.kind) {
    case Node::Kind::kObject:
      Append(output, "{", limit);
      for (std::size_t i = 0; i < node.members.size(); ++i) {
        if (i != 0) Append(output, ",", limit);
        AppendString(output, node.members[i].first, limit);
        Append(output, ":", limit);
        Emit(node.members[i].second, output, limit);
      }
      Append(output, "}", limit);
      break;
    case Node::Kind::kArray:
      Append(output, "[", limit);
      for (std::size_t i = 0; i < node.elements.size(); ++i) {
        if (i != 0) Append(output, ",", limit);
        Emit(node.elements[i], output, limit);
      }
      Append(output, "]", limit);
      break;
    case Node::Kind::kString:
      AppendString(output, node.value, limit);
      break;
    case Node::Kind::kLiteral:
      Append(output, node.value, limit);
      break;
  }
}

std::string Convert(std::string_view input, Limits limits = {}, EventStats* stats = nullptr) {
  Reader reader(input, limits, stats);
  Node root = reader.Parse();
  std::string output;
  Emit(root, output, limits.output);
  return output;
}

void CheckEqual(std::string_view name, std::string_view input, std::string_view expected) {
  EventStats stats;
  const auto actual = Convert(input, {}, &stats);
  Require(stats.created == stats.released, "event ownership mismatch on success");
  if (actual != expected) {
    throw Failure(std::string(name) + ": expected " + std::string(expected) + ", actual " + actual);
  }
  std::cout << "PASS " << name << '\n';
}

void CheckReject(std::string_view name, std::string_view input, std::string_view diagnostic,
                 Limits limits = {}) {
  EventStats stats;
  try {
    (void)Convert(input, limits, &stats);
  } catch (const Failure& error) {
    Require(stats.created == stats.released, "event ownership mismatch on rejection");
    Require(std::string_view(error.what()).find(diagnostic) != std::string_view::npos,
            "wrong rejection reason");
    std::cout << "PASS " << name << " rejected: " << error.what() << '\n';
    return;
  }
  throw Failure(std::string(name) + ": unexpectedly accepted");
}

void CheckLocation() {
  EventStats stats;
  Reader reader("name: 中文\nitems: [1]\n", {}, &stats);
  const auto root = reader.Parse();
  Require(stats.created == stats.released, "event ownership mismatch on location test");
  Require(root.members.size() == 2 && root.members[1].second.line == 2 &&
              root.members[1].second.column == 8,
          "source line/column mismatch");
  std::cout << "PASS source_location line=2 column=8\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--convert") {
      std::ifstream input(argv[2], std::ios::binary);
      Require(input.good(), "cannot open YAML input");
      const std::string bytes((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
      std::cout << Convert(bytes) << '\n';
      return 0;
    }
    Require(argc == 1, "usage: probe [--convert YAML_FILE]");
    CheckEqual("nested_utf8_escape", "# note\nname: \"中文\\nA\"\nitems: [true, null, 2]\n",
               "{\"name\":\"中文\\nA\",\"items\":[true,null,2]}");
    CheckEqual("uint64_max", "n: 18446744073709551615\n", "{\"n\":18446744073709551615}");
    CheckEqual("int64_min", "n: -9223372036854775808\n", "{\"n\":-9223372036854775808}");
    CheckEqual("quoted_version_hex", "version: \"0.11\"\nbytes: \"AA FF\"\n",
               "{\"version\":\"0.11\",\"bytes\":\"AA FF\"}");
    CheckEqual("quoted_merge_literal", "\"<<\": ordinary\n", "{\"<<\":\"ordinary\"}");
    CheckEqual("binary_minimal_structure",
               "schema_version: \"0.11\"\nprotocol_id: binary_demo\nmessages:\n  - id: ping\n    "
               "frame_bytes: \"AA 00 07\"\n",
               "{\"schema_version\":\"0.11\",\"protocol_id\":\"binary_demo\",\"messages\":[{\"id\":"
               "\"ping\",\"frame_bytes\":\"AA 00 07\"}]}");
    CheckEqual("ascii_minimal_structure",
               "schema_version: \"0.11\"\nprotocol_id: ascii_demo\nmessages:\n  - id: reply\n    "
               "frame_text: \"OK\\r\\n\"\n",
               "{\"schema_version\":\"0.11\",\"protocol_id\":\"ascii_demo\",\"messages\":[{\"id\":"
               "\"reply\",\"frame_text\":\"OK\\r\\n\"}]}");
    CheckLocation();
    CheckReject("decoded_duplicate", "a: 1\n\"\\u0061\": 2\n", "decoded duplicate key");
    CheckReject("tag", "a: !custom value\n", "tag forbidden");
    CheckReject("standard_tag", "a: !!str value\n", "tag forbidden");
    CheckReject("anchor", "a: &x 1\n", "anchor forbidden");
    CheckReject("alias", "a: *x\n", "unsupported event or alias");
    CheckReject("merge", "<<: {a: 1}\n", "merge key forbidden");
    CheckReject("multiple_documents", "---\na: 1\n---\nb: 2\n", "multiple documents forbidden");
    CheckReject("yaml_1_1", "%YAML 1.1\n---\na: 1\n", "unsupported YAML version");
    CheckReject("tag_directive", "%TAG !e! tag:example.com,2026:\n---\na: 1\n",
                "tag directive forbidden");
    CheckReject("bom",
                "\xEF\xBB\xBF"
                "a: 1\n",
                "UTF-8 BOM forbidden");
    CheckReject("implicit_empty", "a:\n", "implicit empty scalar forbidden");
    CheckReject("decimal", "n: 2.0\n", "noncanonical numeric scalar");
    CheckReject("negative_zero", "n: -0\n", "noncanonical numeric scalar");
    CheckReject("leading_zero", "n: 01\n", "noncanonical numeric scalar");
    CheckReject("hex_looking", "n: 0xFF\n", "noncanonical numeric scalar");
    CheckReject("positive_sign", "n: +1\n", "noncanonical numeric scalar");
    CheckReject("uint64_overflow", "n: 18446744073709551616\n", "unsigned integer out of range");
    CheckReject("int64_underflow", "n: -9223372036854775809\n", "signed integer out of range");
    CheckReject("input_limit", "a: 1\n", "input limit before parser",
                Limits{4, 8192, 1024, 128, 16});
    CheckReject("node_limit", "a: [1, 2, 3]\n", "node limit during event parsing",
                Limits{4096, 8192, 1024, 3, 16});
    CheckReject("depth_limit", "a: {b: {c: 1}}\n", "depth limit during event parsing",
                Limits{4096, 8192, 1024, 128, 3});
    CheckReject("scalar_limit", "a: abcde\n", "scalar limit during event parsing",
                Limits{4096, 8192, 4, 128, 16});
    CheckReject("output_limit", "a: 1\n", "output limit", Limits{4096, 4, 1024, 128, 16});
    std::cout << "PASS 31 probe cases with event ownership checks\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
