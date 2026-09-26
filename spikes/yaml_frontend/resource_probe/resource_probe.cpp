#define RYML_SINGLE_HDR_DEFINE_NOW
#include <windows.h>

// psapi.h requires Windows types to be declared first.
#include <psapi.h>

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "rapidyaml.hpp"

namespace {

struct Failure : std::runtime_error {
  using std::runtime_error::runtime_error;
};
struct BudgetFailure : Failure {
  using Failure::Failure;
};

void Require(bool ok, const char* why) {
  if (!ok) throw Failure(why);
}

struct Budget {
  std::size_t limit = std::numeric_limits<std::size_t>::max();
  std::size_t current = 0;
  std::size_t peak = 0;
  std::size_t calls = 0;
  std::size_t allocations = 0;
  std::size_t frees = 0;
  std::size_t fail_at = 0;
  bool free_size_mismatch = false;
  std::array<std::size_t, 16> allocation_sizes{};
};

struct alignas(std::max_align_t) AllocationHeader {
  std::size_t size;
};

void* Allocate(std::size_t size, void*, void* context) {
  auto& budget = *static_cast<Budget*>(context);
  ++budget.calls;
  if (budget.calls == budget.fail_at || size > budget.limit - budget.current ||
      size > std::numeric_limits<std::size_t>::max() - sizeof(AllocationHeader)) {
    throw BudgetFailure("local allocation limit or injected failure");
  }
  auto* block = static_cast<AllocationHeader*>(std::malloc(sizeof(AllocationHeader) + size));
  if (block == nullptr) throw std::bad_alloc();
  block->size = size;
  if (budget.allocations < budget.allocation_sizes.size())
    budget.allocation_sizes[budget.allocations] = size;
  ++budget.allocations;
  budget.current += size;
  if (budget.current > budget.peak) budget.peak = budget.current;
  return block + 1;
}

void Free(void* memory, std::size_t size, void* context) {
  if (memory == nullptr) return;
  auto& budget = *static_cast<Budget*>(context);
  auto* block = static_cast<AllocationHeader*>(memory) - 1;
  if (size != block->size) budget.free_size_mismatch = true;
  budget.current -= block->size;
  ++budget.frees;
  std::free(block);
}

[[noreturn]] void BasicError(ryml::csubstr message, const ryml::ErrorDataBasic&, void*) {
  throw Failure(std::string(message.str, message.len));
}
[[noreturn]] void ParseError(ryml::csubstr message, const ryml::ErrorDataParse&, void*) {
  throw Failure(std::string(message.str, message.len));
}
[[noreturn]] void VisitError(ryml::csubstr message, const ryml::ErrorDataVisit&, void*) {
  throw Failure(std::string(message.str, message.len));
}

ryml::Callbacks MakeCallbacks(Budget* budget) {
  ryml::Callbacks callbacks;
  callbacks.set_user_data(budget)
      .set_allocate(Allocate)
      .set_free(Free)
      .set_error_basic(BasicError)
      .set_error_parse(ParseError)
      .set_error_visit(VisitError);
  return callbacks;
}

bool IsInteger(std::string_view value) {
  if (value.empty()) return false;
  std::size_t start = value[0] == '-' ? 1 : 0;
  if (start == value.size()) return false;
  if (value[start] == '0') return start == 0 && value.size() == 1;
  if (value[start] < '1' || value[start] > '9') return false;
  for (std::size_t i = start + 1; i < value.size(); ++i) {
    if (value[i] < '0' || value[i] > '9') return false;
  }
  return true;
}

void CheckNumber(std::string_view value) {
  if (value.empty()) throw Failure("implicit empty scalar forbidden");
  const bool numeric = (value[0] >= '0' && value[0] <= '9') ||
                       ((value[0] == '-' || value[0] == '+') && value.size() > 1 &&
                        value[1] >= '0' && value[1] <= '9');
  if (!numeric) return;
  Require(IsInteger(value), "noncanonical numeric scalar");
  if (value[0] == '-') {
    std::int64_t parsed{};
    auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    Require(result.ec == std::errc() && result.ptr == value.data() + value.size(),
            "signed integer out of range");
  } else {
    std::uint64_t parsed{};
    auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    Require(result.ec == std::errc() && result.ptr == value.data() + value.size(),
            "unsigned integer out of range");
  }
}

std::string_view View(ryml::csubstr value) { return {value.str, value.len}; }

void CheckNode(const ryml::Tree& tree, ryml::id_type id, std::size_t depth, std::size_t& count,
               std::size_t max_depth) {
  Require(depth <= max_depth, "depth limit");
  Require(++count <= 512, "node limit");
  const auto type = tree.type(id);
  Require(!type.has_key_tag() && !type.has_val_tag(), "tag forbidden");
  Require(!type.has_anchor() && !type.is_ref(), "anchor or alias forbidden");
  Require(!type.val_is_null(), "implicit empty scalar forbidden");
  if (tree.has_key(id)) Require(tree.key(id).len <= 4096, "scalar limit");
  if (tree.has_val(id)) Require(tree.val(id).len <= 4096, "scalar limit");
  if (tree.has_val(id) && !type.is_val_quoted()) CheckNumber(View(tree.val(id)));
  if (tree.is_map(id)) {
    std::vector<std::string_view> keys;
    for (auto child = tree.first_child(id); child != ryml::NONE; child = tree.next_sibling(child)) {
      const auto child_type = tree.type(child);
      Require(tree.has_key(child), "mapping key must be scalar");
      const auto key = View(tree.key(child));
      Require(child_type.is_key_quoted() || key != "<<", "merge key forbidden");
      for (auto previous : keys) Require(previous != key, "decoded duplicate key");
      keys.push_back(key);
      CheckNode(tree, child, depth + 1, count, max_depth);
    }
  } else if (tree.is_seq(id)) {
    for (auto child = tree.first_child(id); child != ryml::NONE; child = tree.next_sibling(child)) {
      CheckNode(tree, child, depth + 1, count, max_depth);
    }
  }
}

std::size_t WorkingSet() {
  PROCESS_MEMORY_COUNTERS counters{};
  counters.cb = sizeof counters;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof counters)) return 0;
  return counters.WorkingSetSize;
}

std::string Parse(std::string_view input, Budget& budget, std::size_t* source_line = nullptr,
                  std::size_t max_depth = 16) {
  Require(input.size() <= 16384, "input limit before parser");
  Require(input.substr(0, 3) != "\xEF\xBB\xBF", "UTF-8 BOM forbidden");
  // YAML version directives are only valid before document content.
  std::size_t cursor = 0;
  while (cursor < input.size()) {
    const auto end = input.find('\n', cursor);
    auto line = input.substr(cursor, end == std::string_view::npos ? end : end - cursor);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    if (line.substr(0, 6) == "%YAML ")
      Require(line == "%YAML 1.2", "unsupported YAML version");
    else if (!line.empty() && line[0] != '#' && line[0] != '%' && line != "---")
      break;
    if (end == std::string_view::npos) break;
    cursor = end + 1;
  }
  std::string mutable_input(input);  // Input copy intentionally outside ryml callbacks.
  auto callbacks = MakeCallbacks(&budget);
  ryml::EventHandlerTree handler(callbacks);
  ryml::ParserOptions options;
  options.locations(true);
  ryml::Parser parser(&handler, options);
  ryml::Tree tree(callbacks);
  ryml::parse_in_arena(&parser, ryml::csubstr(mutable_input.data(), mutable_input.size()), &tree);
  Require(tree.num_tag_directives() == 0, "tag directive forbidden");
  auto root = tree.root_id();
  if (tree.is_stream(root)) {
    Require(tree.num_children(root) == 1, "multiple documents forbidden");
    root = tree.first_child(root);
  }
  Require(tree.is_map(root), "root must be object");
  std::size_t nodes = 0;
  CheckNode(tree, root, 1, nodes, max_depth);
  if (source_line != nullptr) {
    auto located = tree.first_child(root);
    if (located != ryml::NONE && tree.next_sibling(located) != ryml::NONE)
      located = tree.next_sibling(located);
    else
      located = root;
    *source_line = tree.location(parser, located).line;
  }
  auto output = ryml::emitrs_json<std::string>(tree, root);
  Require(output.size() <= 32768, "output limit");
  return output;
}

void CheckReleased(const Budget& budget) {
  if (budget.current != 0 || budget.allocations != budget.frees || budget.free_size_mismatch) {
    throw Failure("callbacks not balanced current=" + std::to_string(budget.current) +
                  " calls=" + std::to_string(budget.calls) + " allocations=" +
                  std::to_string(budget.allocations) + " frees=" + std::to_string(budget.frees));
  }
}

void Run() {
  std::string sample;
  for (std::size_t i = 0; i < 24; ++i) {
    sample += std::string(i * 2, ' ') + "level" + std::to_string(i) + ":\n";
  }
  sample += std::string(48, ' ') + "value: \"中文\"\n";
  Budget successful;
  std::size_t line = 0;
  const auto rss_before = WorkingSet();
  const auto result = Parse(sample, successful, &line, 64);
  const auto rss_after = WorkingSet();
  CheckReleased(successful);
  Require(result.find("中文") != std::string::npos, "UTF-8 output missing");
  Require(successful.calls > 1 && successful.peak > 0, "no callbacks observed");
  std::cout << "PASS success callbacks=" << successful.calls << " peak=" << successful.peak
            << " rss_before=" << rss_before << " rss_after=" << rss_after << " root_line=" << line
            << '\n';
  std::cout << "ALLOC sizes=";
  for (std::size_t i = 0; i < successful.allocations && i < successful.allocation_sizes.size(); ++i)
    std::cout << (i == 0 ? "" : ",") << successful.allocation_sizes[i];
  std::cout << '\n';

  for (std::size_t cap : {successful.peak - 1, successful.peak}) {
    Budget bounded;
    bounded.limit = cap;
    bool failed = false;
    try {
      (void)Parse(sample, bounded, nullptr, 64);
    } catch (const BudgetFailure&) {
      failed = true;
    }
    CheckReleased(bounded);
    Require(failed == (cap < successful.peak), "budget boundary mismatch");
    std::cout << "PASS cap=" << cap << " rejected=" << failed << '\n';
  }

  for (std::size_t point = 1; point <= successful.calls; ++point) {
    Budget injected;
    injected.fail_at = point;
    bool failed = false;
    try {
      (void)Parse(sample, injected, nullptr, 64);
    } catch (const BudgetFailure&) {
      failed = true;
    }
    Require(failed, "failure injection did not trigger");
    CheckReleased(injected);
    std::cout << "PASS injected_call=" << point << '\n';
  }

  Budget first;
  first.limit = successful.peak - 1;
  Budget second;
  bool first_failed = false;
  try {
    (void)Parse(sample, first, nullptr, 64);
  } catch (const BudgetFailure&) {
    first_failed = true;
  }
  Require(first_failed, "first instance should fail");
  CheckReleased(first);
  (void)Parse(sample, second, nullptr, 64);
  CheckReleased(second);
  Require(second.peak == successful.peak, "second instance affected by first budget");
  std::cout << "PASS two local instances isolated\n";

  Budget concurrent_low;
  concurrent_low.limit = successful.peak - 1;
  Budget concurrent_high;
  bool low_rejected = false;
  bool high_completed = false;
  std::exception_ptr low_error;
  std::exception_ptr high_error;
  std::thread low([&] {
    try {
      (void)Parse(sample, concurrent_low, nullptr, 64);
    } catch (const BudgetFailure&) {
      low_rejected = true;
    } catch (...) {
      low_error = std::current_exception();
    }
  });
  std::thread high([&] {
    try {
      (void)Parse(sample, concurrent_high, nullptr, 64);
      high_completed = true;
    } catch (...) {
      high_error = std::current_exception();
    }
  });
  low.join();
  high.join();
  if (low_error) std::rethrow_exception(low_error);
  if (high_error) std::rethrow_exception(high_error);
  CheckReleased(concurrent_low);
  CheckReleased(concurrent_high);
  Require(low_rejected && high_completed, "concurrent budgets interfered");
  std::cout << "PASS two concurrent local instances isolated\n";

  Budget located;
  std::size_t second_line = 0;
  (void)Parse("a: 1\nb: 2\n", located, &second_line);
  CheckReleased(located);
  Require(second_line == 1, "second field source location mismatch");
  std::cout << "PASS source location second field line=2\n";

  for (auto entry : std::vector<std::pair<std::string_view, std::string_view>>{
           {"n: 18446744073709551615\n", "18446744073709551615"},
           {"n: -9223372036854775808\n", "-9223372036854775808"},
           {"n: 0\n", "0"},
           {"n: true\n", "true"},
           {"n: null\n", "null"},
           {"version: \"0.11\"\nbytes: \"AA FF\"\n", "AA FF"},
           {"\"<<\": ordinary\n", "ordinary"},
           {"%YAML 1.2\n---\na: 1\n", "\"a\": 1"},
           {"a: 1\n\"\\u0061\": 2\n", "decoded duplicate key"},
           {"a: !custom value\n", "tag forbidden"},
           {"a: !!str value\n", "tag forbidden"},
           {"a: &x 1\n", "anchor or alias forbidden"},
           {"a: *x\n", "anchor or alias forbidden"},
           {"<<: {a: 1}\n", "merge key forbidden"},
           {"---\na: 1\n---\nb: 2\n", "multiple documents forbidden"},
           {"%TAG !e! tag:example.com,2026:\n---\na: 1\n", "tag directive forbidden"},
           {"%YAML 1.1\n---\na: 1\n", "unsupported YAML version"},
           {"\xEF\xBB\xBF"
            "a: 1\n",
            "UTF-8 BOM forbidden"},
           {"a:\n", "implicit empty scalar forbidden"},
           {"n: 2.0\n", "noncanonical numeric scalar"},
           {"n: -0\n", "noncanonical numeric scalar"},
           {"n: 01\n", "noncanonical numeric scalar"},
           {"n: 0xFF\n", "noncanonical numeric scalar"},
           {"n: +1\n", "noncanonical numeric scalar"},
           {"n: 18446744073709551616\n", "unsigned integer out of range"},
           {"n: -9223372036854775809\n", "signed integer out of range"},
       }) {
    Budget case_budget;
    bool matched = false;
    std::string observed;
    try {
      const auto output = Parse(entry.first, case_budget);
      observed = output;
      matched = output.find(entry.second) != std::string::npos;
    } catch (const Failure& error) {
      observed = error.what();
      matched = std::string_view(error.what()).find(entry.second) != std::string_view::npos;
    }
    CheckReleased(case_budget);
    if (!matched)
      throw Failure("semantic mismatch expected=" + std::string(entry.second) +
                    " observed=" + observed);
    std::cout << "PASS semantic=" << entry.second << '\n';
  }
}

}  // namespace

int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string_view(argv[1]) == "--convert") {
      std::ifstream file(argv[2], std::ios::binary);
      Require(file.good(), "cannot open input file");
      const std::string input((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
      Budget budget;
      std::cout << Parse(input, budget) << '\n';
      CheckReleased(budget);
      return 0;
    }
    Require(argc == 1, "unknown command");
    Run();
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "FAIL " << error.what() << '\n';
    return 1;
  }
}
