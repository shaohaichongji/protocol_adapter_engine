#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
#include "../../src/config_compiler/config_compiler.h"
#endif
#include "schema_dispatch.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
#include "pae/compiler.h"
#endif

namespace pae::protocol_lab_ui {

using DocumentId = std::uint64_t;
using Revision = std::uint64_t;
using ResultTicket = std::uint64_t;

inline constexpr std::size_t kMaximumConfigBytes = 4U * 1024U * 1024U;

struct CompileDiagnosticView {
  std::string stage;
  std::string code;
  std::string json_pointer;
  std::optional<std::size_t> byte_offset;
  std::string resource_kind;
  bool has_resource_budget = false;
  std::uint64_t required_bytes = 0U;
  std::uint64_t limit_bytes = 0U;
  std::string resource_profile;
  std::string detail;
};

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
CompileDiagnosticView ProjectCompileDiagnostic(
    const config_compiler::CompileDiagnostic& diagnostic);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) ||       \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) ||        \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
CompileDiagnosticView ProjectCompileDiagnostic(const pae::CompileDiagnostic& diagnostic);
#endif
std::string FormatCompileDiagnostic(const CompileDiagnosticView& diagnostic);

struct CompileCompletion {
  DocumentId document_id = 0U;
  Revision load_revision = 0U;
  std::string config_sha256;
  struct Diagnostic {
    std::string detail;

    Diagnostic() = default;
    Diagnostic(const char* value) : detail(value) {}
    explicit Diagnostic(std::string value) : detail(std::move(value)) {}
    template <typename Source>
    Diagnostic(const Source& source) : detail(source.detail) {}
  };
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  std::unique_ptr<config_compiler::CompiledProtocolArtifacts> artifacts;
#else
  // Keeps existing negative ownership assertions source-compatible without importing the
  // private compiler DTO into the installed-SDK build.
  std::nullptr_t artifacts = nullptr;
#endif
  std::optional<Diagnostic> diagnostic;
  std::optional<CompileDiagnosticView> structured_compile_diagnostic;
  SchemaDispatchStatus route = SchemaDispatchStatus::PRIVATE_LEGACY;
  std::string classification_error;
  std::size_t compiler_attempt_count = 0U;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  std::unique_ptr<pae::CompiledProtocol> public_compiled;
  std::optional<pae::CompileDiagnostic> public_diagnostic;
#endif
};

enum class SubmitStatus {
  ACCEPTED,
  CONFIG_TOO_LARGE,
  QUEUE_CAPACITY_EXCEEDED,
  DOCUMENT_CLOSED,
  WORKER_STOPPED,
};

class CompileWorker final {
 public:
  struct Request {
    DocumentId document_id = 0U;
    Revision load_revision = 0U;
    std::string config_text;
    std::uint64_t enqueue_sequence = 0U;
  };
  using CompileFunction = std::function<std::unique_ptr<CompileCompletion>(Request)>;

  CompileWorker();
  explicit CompileWorker(CompileFunction compile_function);
  CompileWorker(const CompileWorker&) = delete;
  CompileWorker& operator=(const CompileWorker&) = delete;
  ~CompileWorker();

  SubmitStatus Submit(DocumentId document_id, Revision load_revision, std::string_view config_text);
  void CloseDocument(DocumentId document_id);
  std::vector<ResultTicket> DrainReadyTickets();
  std::unique_ptr<CompileCompletion> TakeResult(ResultTicket ticket);

  std::size_t PendingCountForTesting() const;
  bool HasActiveRequestForTesting() const;
  std::size_t StoredResultCountForTesting() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> implementation_;
};

}  // namespace pae::protocol_lab_ui
