#pragma once

#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <thread>

#include "../../tools/protocol_lab_ui/compile_worker.h"

// Keep contract checks active in Release builds as well as Debug builds.
#undef assert
#define assert(condition)                                                         \
  do {                                                                            \
    if (!(condition)) {                                                           \
      std::fprintf(stderr, "check failed: %s (%s:%d)\n", #condition, __FILE__, \
                   __LINE__);                                                     \
      std::abort();                                                               \
    }                                                                             \
  } while (false)

namespace pae::protocol_lab_ui::test {

inline std::filesystem::path FixturePath(const char* name) {
#if defined(PAE_UI_FIXTURE_DIR)
  return std::filesystem::path{PAE_UI_FIXTURE_DIR} / name;
#else
#error "PAE_UI_FIXTURE_DIR must name the fixture directory"
#endif
}

inline std::string ReadFixture(const char* name) {
  std::ifstream input(FixturePath(name), std::ios::binary);
  return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

inline std::unique_ptr<CompileCompletion> CompileFixture(DocumentId document_id,
                                                         Revision revision,
                                                         const char* name) {
  const std::string text = ReadFixture(name);
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  auto result = pae::CompileProtocolJson(text);
#else
  auto result = config_compiler::CompileJsonToPlanWithUiDescription(
      text, config_compiler::DerivedUiDescriptionMemoryLimit(
                protocol_plan::ResourceProfile::DESKTOP));
#endif
  auto completion = std::make_unique<CompileCompletion>();
  completion->document_id = document_id;
  completion->load_revision = revision;
  completion->config_sha256 = "synthetic-test-hash";
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  completion->route = SchemaDispatchStatus::LEGACY_PUBLIC;
  completion->compiler_attempt_count = 1U;
  if (result.Succeeded()) {
    completion->public_compiled =
        std::make_unique<pae::CompiledProtocol>(std::move(result).TakeCompiled());
  } else if (result.Diagnostic() != nullptr) {
    completion->public_diagnostic = *result.Diagnostic();
    std::fprintf(stderr, "public fixture compile failed: %s\n",
                 result.Diagnostic()->detail.c_str());
  }
#else
  if (result.Succeeded()) {
    completion->artifacts = std::make_unique<config_compiler::CompiledUiArtifacts>(
        std::move(result).TakeArtifacts());
  } else if (result.Diagnostic() != nullptr) {
    completion->diagnostic = *result.Diagnostic();
    std::fprintf(stderr, "fixture compile failed: %s\n", result.Diagnostic()->detail.c_str());
  }
#endif
  return completion;
}

template <typename Predicate>
inline bool WaitUntil(Predicate predicate,
                      std::chrono::milliseconds timeout = std::chrono::milliseconds{3000}) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (predicate()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  return predicate();
}

}  // namespace pae::protocol_lab_ui::test
