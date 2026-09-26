#define RYML_SINGLE_HDR_DEFINE_NOW
#include "frontend.h"

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>

#include "rapidyaml.hpp"

namespace pae::yaml_frontend {
namespace {

struct Failure {
  Status status;
  const char* reason;
};

void Require(bool ok, Status status, const char* reason) {
  if (!ok) throw Failure{status, reason};
}

bool Fits(std::size_t used, std::size_t additional, std::size_t limit) {
  return used <= limit && additional <= limit - used;
}

struct ParserBudget {
  std::size_t limit = 0;
  std::size_t used = 0;
  std::size_t peak = 0;
  std::size_t calls = 0;
  std::size_t fail_at = 0;
};

struct alignas(std::max_align_t) Header {
  std::size_t bytes;
};

void* Allocate(std::size_t bytes, void*, void* context) {
  auto& budget = *static_cast<ParserBudget*>(context);
  ++budget.calls;
  Require(budget.calls != budget.fail_at, Status::ALLOCATION_FAILED,
          "injected parser allocation failure");
  Require(Fits(budget.used, sizeof(Header), budget.limit) &&
              Fits(budget.used + sizeof(Header), bytes, budget.limit),
          Status::PARSER_BUDGET, "parser allocation budget exceeded");
  Require(Fits(sizeof(Header), bytes, std::numeric_limits<std::size_t>::max()),
          Status::PARSER_BUDGET, "parser allocation overflow");
  auto* header = static_cast<Header*>(std::malloc(sizeof(Header) + bytes));
  if (!header) throw std::bad_alloc();
  header->bytes = bytes + sizeof(Header);
  budget.used += header->bytes;
  if (budget.used > budget.peak) budget.peak = budget.used;
  return header + 1;
}

void Free(void* memory, std::size_t, void* context) {
  if (!memory) return;
  auto& budget = *static_cast<ParserBudget*>(context);
  auto* header = static_cast<Header*>(memory) - 1;
  budget.used -= header->bytes;
  std::free(header);
}

[[noreturn]] void BasicError(ryml::csubstr, const ryml::ErrorDataBasic&, void*) {
  throw Failure{Status::INVALID_YAML, "YAML parser error"};
}
[[noreturn]] void ParseError(ryml::csubstr, const ryml::ErrorDataParse&, void*) {
  throw Failure{Status::INVALID_YAML, "YAML parser error"};
}
[[noreturn]] void VisitError(ryml::csubstr, const ryml::ErrorDataVisit&, void*) {
  throw Failure{Status::INVALID_YAML, "YAML parser error"};
}

ryml::Callbacks Callbacks(ParserBudget* budget) {
  ryml::Callbacks callbacks;
  callbacks.set_user_data(budget)
      .set_allocate(Allocate)
      .set_free(Free)
      .set_error_basic(BasicError)
      .set_error_parse(ParseError)
      .set_error_visit(VisitError);
  return callbacks;
}

bool IsUtf8(std::string_view input) {
  for (std::size_t i = 0; i < input.size();) {
    const auto first = static_cast<unsigned char>(input[i]);
    if (first < 0x80) {
      ++i;
      continue;
    }
    const std::size_t count = first >= 0xC2 && first <= 0xDF   ? 2
                              : first >= 0xE0 && first <= 0xEF ? 3
                              : first >= 0xF0 && first <= 0xF4 ? 4
                                                               : 0;
    if (!count || count > input.size() - i) return false;
    for (std::size_t n = 1; n < count; ++n)
      if ((static_cast<unsigned char>(input[i + n]) & 0xC0) != 0x80) return false;
    const auto second = static_cast<unsigned char>(input[i + 1]);
    if (count == 3 && ((first == 0xE0 && second < 0xA0) || (first == 0xED && second >= 0xA0)))
      return false;
    if (count == 4 && ((first == 0xF0 && second < 0x90) || (first == 0xF4 && second >= 0x90)))
      return false;
    i += count;
  }
  return true;
}

std::string_view View(ryml::csubstr value) { return {value.str, value.len}; }

bool EqualsIgnoreCase(std::string_view value, std::string_view expected);

bool NumericLooking(std::string_view value) {
  if (value.empty()) return false;
  const char first = value.front();
  if (first >= '0' && first <= '9') return true;
  if (first == '+' || first == '-')
    return value.size() > 1 && ((value[1] >= '0' && value[1] <= '9') || value[1] == '.');
  return first == '.' &&
         (value.size() > 1 && ((value[1] >= '0' && value[1] <= '9') ||
                               EqualsIgnoreCase(value, ".inf") || EqualsIgnoreCase(value, ".nan")));
}

bool EqualsIgnoreCase(std::string_view value, std::string_view expected) {
  if (value.size() != expected.size()) return false;
  for (std::size_t i = 0; i < value.size(); ++i) {
    char a = value[i], b = expected[i];
    if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + ('a' - 'A'));
    if (b >= 'A' && b <= 'Z') b = static_cast<char>(b + ('a' - 'A'));
    if (a != b) return false;
  }
  return true;
}

bool CanonicalInteger(std::string_view value) {
  if (value.empty()) return false;
  const std::size_t start = value.front() == '-' ? 1 : 0;
  if (start == value.size()) return false;
  if (value[start] == '0') return start == 0 && value.size() == 1;
  if (value[start] < '1' || value[start] > '9') return false;
  for (std::size_t i = start + 1; i < value.size(); ++i)
    if (value[i] < '0' || value[i] > '9') return false;
  if (start) {
    std::int64_t parsed = 0;
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
  }
  std::uint64_t parsed = 0;
  const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
  return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

void CheckDirectives(std::string_view input) {
  for (std::size_t cursor = 0; cursor < input.size();) {
    const auto end = input.find('\n', cursor);
    auto line = input.substr(cursor, end == std::string_view::npos ? end : end - cursor);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.empty() || line.front() == '#') {
      // Leading blank lines and comments do not change the version rule.
    } else if (line.substr(0, 5) == "%YAML") {
      auto directive = line.substr(5);
      Require(!directive.empty() && (directive.front() == ' ' || directive.front() == '\t'),
              Status::PROFILE_REJECTED, "invalid YAML version directive");
      while (!directive.empty() && (directive.front() == ' ' || directive.front() == '\t'))
        directive.remove_prefix(1);
      Require(directive.substr(0, 3) == "1.2" && (directive.size() == 3 || directive[3] == ' ' ||
                                                  directive[3] == '\t' || directive[3] == '#'),
              Status::PROFILE_REJECTED, "unsupported YAML version");
      directive.remove_prefix(3);
      while (!directive.empty() && (directive.front() == ' ' || directive.front() == '\t'))
        directive.remove_prefix(1);
      Require(directive.empty() || directive.front() == '#', Status::PROFILE_REJECTED,
              "invalid YAML version directive");
    } else if (line.front() == '%') {
      throw Failure{Status::PROFILE_REJECTED, "YAML directive forbidden"};
    } else if (line == "---") {
      // The optional document marker is handled by the parser.
    } else {
      break;
    }
    if (end == std::string_view::npos) break;
    cursor = end + 1;
  }
}

struct Writer {
  Result& result;
  const Limits& limits;
  const ryml::Tree& tree;
  const ryml::Parser& parser;
  std::size_t pointer_capacity;
  std::size_t path_capacity;
  std::unique_ptr<char[]> path;
  std::size_t path_length = 0;
  std::size_t pointer_used = 0;
  std::size_t node_count = 0;

  bool AtQuotedNodeStart(ryml::Location& location) const {
    const auto source = parser.source();
    if (location.offset == ryml::npos || !source.str || location.offset >= source.len) return false;
    const char current = source.str[location.offset];
    if (current == '"' || current == '\'') return true;
    if (location.offset == 0 || location.col == 0) return false;
    const char previous = source.str[location.offset - 1];
    if (previous != '"' && previous != '\'') return false;
    --location.offset;
    --location.col;
    return true;
  }

  void Bytes(std::string_view bytes) {
    Require(Fits(result.json_length, bytes.size(), limits.json_bytes), Status::OUTPUT_LIMIT,
            "JSON output limit exceeded");
    std::memcpy(result.json_storage.get() + result.json_length, bytes.data(), bytes.size());
    result.json_length += bytes.size();
  }
  void Char(char ch) {
    Require(Fits(result.json_length, 1, limits.json_bytes), Status::OUTPUT_LIMIT,
            "JSON output limit exceeded");
    result.json_storage[result.json_length++] = ch;
  }
  void String(std::string_view value) {
    Char('"');
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char ch : value) {
      if (ch == '"' || ch == '\\') {
        Char('\\');
        Char(static_cast<char>(ch));
      } else if (ch < 0x20) {
        Bytes("\\u00");
        Char(hex[ch >> 4]);
        Char(hex[ch & 0x0F]);
      } else {
        Char(static_cast<char>(ch));
      }
    }
    Char('"');
  }
  void PathChar(char ch) {
    Require(Fits(path_length, 1, path_capacity), Status::AUXILIARY_BUDGET,
            "source path limit exceeded");
    path[path_length++] = ch;
  }
  void PathSegment(std::string_view segment) {
    PathChar('/');
    for (char ch : segment) {
      if (ch == '~') {
        PathChar('~');
        PathChar('0');
      } else if (ch == '/') {
        PathChar('~');
        PathChar('1');
      } else {
        PathChar(ch);
      }
    }
  }
  void Record(ryml::id_type id) {
    Require(result.entry_count < limits.nodes, Status::AUXILIARY_BUDGET,
            "source entry limit exceeded");
    Require(Fits(pointer_used, path_length, pointer_capacity), Status::AUXILIARY_BUDGET,
            "source pointer pool exceeded");
    auto& entry = result.entries[result.entry_count++];
    entry.pointer_offset = pointer_used;
    entry.pointer_length = path_length;
    std::memcpy(result.pointer_storage.get() + pointer_used, path.get(), path_length);
    pointer_used += path_length;
    auto key = tree.has_key(id) ? tree.location(parser, id) : ryml::Location{};
    if (key.line != ryml::npos) {
      if (tree.is_key_quoted(id) && !AtQuotedNodeStart(key)) entry.key_approximate = true;
      entry.key_line = key.line + 1;
      entry.key_column = key.col + 1;
    }
    ryml::Location value;
    if (tree.has_val(id)) {
      const auto scalar = tree.val(id);
      const auto source = parser.source();
      const auto scalar_address = reinterpret_cast<std::uintptr_t>(scalar.str);
      const auto source_address = reinterpret_cast<std::uintptr_t>(source.str);
      if (scalar.str && source.str && scalar_address >= source_address &&
          scalar_address - source_address <= source.len)
        value = parser.val_location(scalar.str);
    } else if (!tree.has_key(id)) {
      value = tree.location(parser, id);
    }
    if (value.line == ryml::npos) {
      value = tree.location(parser, id);
      entry.value_approximate = true;
    }
    if (value.line != ryml::npos) {
      if (tree.has_val(id) && tree.is_val_quoted(id) && !AtQuotedNodeStart(value))
        entry.value_approximate = true;
      entry.value_line = value.line + 1;
      entry.value_column = value.col + 1;
    }
  }
  void Node(ryml::id_type id, std::size_t depth) {
    Require(depth <= limits.depth, Status::PROFILE_REJECTED, "YAML depth limit exceeded");
    Require(++node_count <= limits.nodes, Status::PROFILE_REJECTED, "YAML node limit exceeded");
    const auto type = tree.type(id);
    Require(!type.has_key_tag() && !type.has_val_tag(), Status::PROFILE_REJECTED,
            "YAML tag forbidden");
    Require(!type.has_anchor() && !type.is_ref(), Status::PROFILE_REJECTED,
            "YAML anchor or alias forbidden");
    Record(id);
    if (tree.is_map(id)) {
      Char('{');
      bool first = true;
      for (auto child = tree.first_child(id); child != ryml::NONE;
           child = tree.next_sibling(child)) {
        Require(tree.has_key(child), Status::PROFILE_REJECTED, "non-scalar YAML key");
        const auto key = View(tree.key(child));
        Require(key.size() <= limits.scalar_bytes, Status::PROFILE_REJECTED,
                "YAML scalar limit exceeded");
        Require(IsUtf8(key), Status::PROFILE_REJECTED, "invalid decoded YAML key UTF-8");
        Require(tree.is_key_quoted(child) ||
                    (!key.empty() && key != "<<" && !NumericLooking(key) &&
                     !EqualsIgnoreCase(key, "true") && !EqualsIgnoreCase(key, "false") &&
                     !EqualsIgnoreCase(key, "null")),
                Status::PROFILE_REJECTED, "non-string or merge YAML key");
        for (auto previous = tree.first_child(id); previous != child;
             previous = tree.next_sibling(previous)) {
          Require(tree.has_key(previous), Status::PROFILE_REJECTED, "non-scalar YAML key");
          Require(View(tree.key(previous)) != key, Status::PROFILE_REJECTED,
                  "decoded duplicate YAML key");
        }
        if (!first) Char(',');
        first = false;
        String(key);
        Char(':');
        const auto saved = path_length;
        PathSegment(key);
        Node(child, depth + 1);
        path_length = saved;
      }
      Char('}');
    } else if (tree.is_seq(id)) {
      Char('[');
      std::size_t index = 0;
      for (auto child = tree.first_child(id); child != ryml::NONE;
           child = tree.next_sibling(child), ++index) {
        if (index) Char(',');
        char digits[32];
        const auto rendered = std::to_chars(digits, digits + sizeof digits, index);
        Require(rendered.ec == std::errc{}, Status::AUXILIARY_BUDGET, "array index overflow");
        const auto saved = path_length;
        PathSegment({digits, static_cast<std::size_t>(rendered.ptr - digits)});
        Node(child, depth + 1);
        path_length = saved;
      }
      Char(']');
    } else {
      Require(tree.has_val(id), Status::PROFILE_REJECTED, "implicit empty YAML scalar");
      const auto value = View(tree.val(id));
      Require(value.size() <= limits.scalar_bytes, Status::PROFILE_REJECTED,
              "YAML scalar limit exceeded");
      Require(IsUtf8(value), Status::PROFILE_REJECTED, "invalid decoded YAML value UTF-8");
      if (tree.is_val_quoted(id)) {
        String(value);
      } else {
        Require(!value.empty(), Status::PROFILE_REJECTED, "implicit empty YAML scalar");
        if (value == "null" || value == "true" || value == "false") {
          Bytes(value);
        } else if (NumericLooking(value)) {
          Require(CanonicalInteger(value), Status::PROFILE_REJECTED,
                  "noncanonical or out-of-range YAML number");
          Bytes(value);
        } else {
          Require(!EqualsIgnoreCase(value, "null") && !EqualsIgnoreCase(value, "true") &&
                      !EqualsIgnoreCase(value, "false"),
                  Status::PROFILE_REJECTED, "noncanonical YAML boolean or null");
          String(value);
        }
      }
    }
  }
};

}  // namespace

const SourceEntry* Result::Find(std::string_view pointer) const noexcept {
  if (!Succeeded()) return nullptr;
  for (std::size_t i = 0; i < entry_count; ++i)
    if (Pointer(entries[i]) == pointer) return &entries[i];
  return nullptr;
}

const SourceEntry* Result::FindNearest(std::string_view pointer) const noexcept {
  if (!Succeeded()) return nullptr;
  for (;;) {
    if (const auto* found = Find(pointer)) return found;
    if (pointer.empty()) return nullptr;
    const auto slash = pointer.rfind('/');
    if (slash == std::string_view::npos) return nullptr;
    pointer = pointer.substr(0, slash);
  }
}

Result Convert(std::string_view input, std::string_view source_identity,
               const Limits& limits) noexcept {
  Result result;
  ParserBudget budget{limits.parser_bytes, 0, 0, 0, limits.fail_parser_allocation};
  std::size_t frontend_allocation_calls = 0;
  try {
    Require(input.size() <= limits.input_bytes, Status::INPUT_LIMIT, "YAML input limit exceeded");
    Require(input.size() < 3 || input.substr(0, 3) != "\xEF\xBB\xBF", Status::PROFILE_REJECTED,
            "UTF-8 BOM forbidden");
    Require(IsUtf8(input), Status::PROFILE_REJECTED, "invalid UTF-8");
    CheckDirectives(input);
    Require(limits.nodes <= std::numeric_limits<std::size_t>::max() / sizeof(SourceEntry),
            Status::AUXILIARY_BUDGET, "source entry size overflow");
    Require(limits.nodes != 0, Status::AUXILIARY_BUDGET, "zero source entry capacity");
    Require(limits.json_bytes != 0, Status::OUTPUT_LIMIT, "zero JSON output capacity");
    const auto entry_bytes = limits.nodes * sizeof(SourceEntry);
    Require(entry_bytes <= limits.auxiliary_bytes, Status::AUXILIARY_BUDGET,
            "source entry budget exceeded");
    const auto remaining = limits.auxiliary_bytes - entry_bytes;
    const auto path_capacity = remaining / 2;
    const auto pointer_capacity = remaining - path_capacity;
    Require(path_capacity != 0 && pointer_capacity != 0, Status::AUXILIARY_BUDGET,
            "zero source path or pointer capacity");
    Require(source_identity.size() <= pointer_capacity, Status::AUXILIARY_BUDGET,
            "source identity limit exceeded");
    const auto allocate = [&] {
      ++frontend_allocation_calls;
      Require(frontend_allocation_calls != limits.fail_frontend_allocation,
              Status::ALLOCATION_FAILED, "injected frontend allocation failure");
    };
    allocate();
    result.entries.reset(new (std::nothrow) SourceEntry[limits.nodes]);
    Require(result.entries != nullptr, Status::ALLOCATION_FAILED, "source entry allocation failed");
    allocate();
    result.pointer_storage.reset(new (std::nothrow) char[pointer_capacity]);
    Require(result.pointer_storage != nullptr, Status::ALLOCATION_FAILED,
            "source pointer allocation failed");
    allocate();
    std::unique_ptr<char[]> path(new (std::nothrow) char[path_capacity]);
    Require(path != nullptr, Status::ALLOCATION_FAILED, "source path allocation failed");
    allocate();
    result.json_storage.reset(new (std::nothrow) char[limits.json_bytes]);
    Require(result.json_storage != nullptr, Status::ALLOCATION_FAILED,
            "JSON output allocation failed");
    if (!source_identity.empty())
      std::memcpy(result.pointer_storage.get(), source_identity.data(), source_identity.size());
    result.source_identity_length = source_identity.size();
    const auto callbacks = Callbacks(&budget);
    ryml::EventHandlerTree handler(callbacks);
    ryml::ParserOptions options;
    options.locations(true);
    ryml::Parser parser(&handler, options);
    ryml::Tree tree(callbacks);
    ryml::parse_in_arena(&parser, ryml::csubstr(input.data(), input.size()), &tree);
    Require(tree.num_tag_directives() == 0, Status::PROFILE_REJECTED,
            "YAML tag directive forbidden");
    auto root = tree.root_id();
    if (tree.is_stream(root)) {
      Require(tree.num_children(root) == 1, Status::PROFILE_REJECTED,
              "multiple YAML documents forbidden");
      root = tree.first_child(root);
    }
    Require(tree.is_map(root), Status::PROFILE_REJECTED, "YAML root must be mapping");
    Writer writer{result, limits, tree, parser, pointer_capacity, path_capacity, std::move(path)};
    writer.pointer_used = source_identity.size();
    writer.Node(root, 1);
    result.status = Status::OK;
    result.reason = "ok";
  } catch (const Failure& failure) {
    result.status = failure.status;
    result.reason = failure.reason;
  } catch (const std::bad_alloc&) {
    result.status = Status::ALLOCATION_FAILED;
    result.reason = "frontend allocation failed";
  } catch (...) {
    result.status = Status::INVALID_YAML;
    result.reason = "unhandled YAML conversion error";
  }
  result.parser_peak_bytes = budget.peak;
  result.parser_allocation_calls = budget.calls;
  result.parser_retained_bytes = budget.used;
  result.frontend_allocation_calls = frontend_allocation_calls;
  if (!result.Succeeded()) {
    result.json_storage.reset();
    result.pointer_storage.reset();
    result.entries.reset();
    result.json_length = 0;
    result.entry_count = 0;
    result.source_identity_length = 0;
  }
  return result;
}

Result Convert(std::string_view input, const Limits& limits) noexcept {
  return Convert(input, {}, limits);
}

}  // namespace pae::yaml_frontend
