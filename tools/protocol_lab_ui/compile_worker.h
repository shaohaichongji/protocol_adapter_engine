#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../../src/config_compiler/config_compiler.h"

namespace pae::protocol_lab_ui {

using DocumentId = std::uint64_t;
using Revision = std::uint64_t;
using ResultTicket = std::uint64_t;

inline constexpr std::size_t kMaximumConfigBytes = 4U * 1024U * 1024U;

struct CompileCompletion {
  DocumentId document_id = 0U;
  Revision load_revision = 0U;
  std::string config_sha256;
  std::unique_ptr<config_compiler::CompiledUiArtifacts> artifacts;
  std::optional<config_compiler::CompileDiagnostic> diagnostic;
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

  SubmitStatus Submit(DocumentId document_id, Revision load_revision,
                      std::string_view config_text);
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
