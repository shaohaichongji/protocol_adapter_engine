#include "compile_worker.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "../protocol_lab/sha256.h"

namespace pae::protocol_lab_ui {
namespace {

std::unique_ptr<CompileCompletion> CompileRequest(CompileWorker::Request request) {
  auto completion = std::make_unique<CompileCompletion>();
  completion->document_id = request.document_id;
  completion->load_revision = request.load_revision;
  completion->config_sha256 = protocol_lab::HashBytes(request.config_text);
  const std::size_t desktop_limit = config_compiler::DerivedUiDescriptionMemoryLimit(
      protocol_plan::ResourceProfile::DESKTOP);
  auto result = config_compiler::CompileJsonToPlanWithUiDescription(request.config_text,
                                                                     desktop_limit);
  if (!result.Succeeded()) {
    if (result.Diagnostic() != nullptr) completion->diagnostic = *result.Diagnostic();
    return completion;
  }
  auto artifacts = std::make_unique<config_compiler::CompiledUiArtifacts>(
      std::move(result).TakeArtifacts());
  const auto* plan = artifacts->Plan();
  if (plan == nullptr || artifacts->DescriptionMemory().accounted_total_bytes >
                             config_compiler::DerivedUiDescriptionMemoryLimit(
                                 plan->GetResourceProfile())) {
    completion->diagnostic = config_compiler::CompileDiagnostic{
        config_compiler::CompileStage::INTERNAL,
        config_compiler::CompileError::INTERNAL_CONTRACT_VIOLATION,
        "",
        std::nullopt,
        "UI description exceeds the derived limit for its Plan resource profile"};
    return completion;
  }
  completion->artifacts = std::move(artifacts);
  return completion;
}

}  // namespace

struct CompileWorker::Impl final {
  explicit Impl(CompileFunction function) : compile(std::move(function)) {
    // Start only after every member (including counters and stopping) has completed initialization.
    thread = std::thread([this] { Run(); });
  }

  ~Impl() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
      pending.clear();
      stored_results.clear();
      ready_tickets.clear();
    }
    wake.notify_one();
    if (thread.joinable()) thread.join();
  }

  void Run() {
    for (;;) {
      Request request;
      {
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this] { return stopping || !pending.empty(); });
        if (stopping) return;
        auto selected = std::min_element(
            pending.begin(), pending.end(),
            [](const auto& left, const auto& right) {
              return left.second.enqueue_sequence < right.second.enqueue_sequence;
            });
        request = std::move(selected->second);
        pending.erase(selected);
        active = std::make_pair(request.document_id, request.load_revision);
      }

      const DocumentId request_document = request.document_id;
      const Revision request_revision = request.load_revision;
      std::unique_ptr<CompileCompletion> completion;
      try {
        completion = compile(std::move(request));
      } catch (...) {
        completion = std::make_unique<CompileCompletion>();
        completion->document_id = request_document;
        completion->load_revision = request_revision;
        completion->diagnostic = config_compiler::CompileDiagnostic{
            config_compiler::CompileStage::INTERNAL,
            config_compiler::CompileError::INTERNAL_CONTRACT_VIOLATION,
            "",
            std::nullopt,
            "compile worker function threw an exception"};
      }

      {
        std::lock_guard<std::mutex> lock(mutex);
        const DocumentId active_document = active.has_value() ? active->first : 0U;
        active.reset();
        if (!stopping && completion != nullptr &&
            closed_documents.find(active_document) == closed_documents.end()) {
          const ResultTicket ticket = next_ticket++;
          ready_tickets.push_back(ticket);
          stored_results.emplace(ticket, std::move(completion));
        }
      }
      wake.notify_one();
    }
  }

  CompileFunction compile;
  mutable std::mutex mutex;
  std::condition_variable wake;
  std::unordered_map<DocumentId, Request> pending;
  std::optional<std::pair<DocumentId, Revision>> active;
  std::unordered_set<DocumentId> closed_documents;
  std::unordered_map<ResultTicket, std::unique_ptr<CompileCompletion>> stored_results;
  std::deque<ResultTicket> ready_tickets;
  std::thread thread;
  std::uint64_t next_enqueue_sequence = 1U;
  ResultTicket next_ticket = 1U;
  bool stopping = false;
};

CompileWorker::CompileWorker() : CompileWorker(CompileRequest) {}

CompileWorker::CompileWorker(CompileFunction compile_function)
    : implementation_(std::make_unique<Impl>(std::move(compile_function))) {}

CompileWorker::~CompileWorker() = default;

SubmitStatus CompileWorker::Submit(DocumentId document_id, Revision load_revision,
                                   std::string_view config_text) {
  if (config_text.size() > kMaximumConfigBytes) return SubmitStatus::CONFIG_TOO_LARGE;
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  if (implementation_->stopping) return SubmitStatus::WORKER_STOPPED;
  if (implementation_->closed_documents.find(document_id) !=
      implementation_->closed_documents.end()) {
    return SubmitStatus::DOCUMENT_CLOSED;
  }
  const auto existing = implementation_->pending.find(document_id);
  if (existing == implementation_->pending.end() && implementation_->pending.size() >= 2U) {
    return SubmitStatus::QUEUE_CAPACITY_EXCEEDED;
  }
  Request replacement;
  replacement.document_id = document_id;
  replacement.load_revision = load_revision;
  replacement.config_text.assign(config_text.data(), config_text.size());
  replacement.enqueue_sequence = implementation_->next_enqueue_sequence++;
  implementation_->pending[document_id] = std::move(replacement);
  implementation_->wake.notify_one();
  return SubmitStatus::ACCEPTED;
}

void CompileWorker::CloseDocument(DocumentId document_id) {
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  implementation_->closed_documents.insert(document_id);
  implementation_->pending.erase(document_id);
  for (auto item = implementation_->stored_results.begin();
       item != implementation_->stored_results.end();) {
    if (item->second != nullptr && item->second->document_id == document_id) {
      const ResultTicket ticket = item->first;
      implementation_->ready_tickets.erase(
          std::remove(implementation_->ready_tickets.begin(),
                      implementation_->ready_tickets.end(), ticket),
          implementation_->ready_tickets.end());
      item = implementation_->stored_results.erase(item);
    } else {
      ++item;
    }
  }
}

std::vector<ResultTicket> CompileWorker::DrainReadyTickets() {
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  std::vector<ResultTicket> tickets(implementation_->ready_tickets.begin(),
                                    implementation_->ready_tickets.end());
  implementation_->ready_tickets.clear();
  return tickets;
}

std::unique_ptr<CompileCompletion> CompileWorker::TakeResult(ResultTicket ticket) {
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  const auto found = implementation_->stored_results.find(ticket);
  if (found == implementation_->stored_results.end()) return nullptr;
  auto result = std::move(found->second);
  implementation_->stored_results.erase(found);
  return result;
}

std::size_t CompileWorker::PendingCountForTesting() const {
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  return implementation_->pending.size();
}

bool CompileWorker::HasActiveRequestForTesting() const {
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  return implementation_->active.has_value();
}

std::size_t CompileWorker::StoredResultCountForTesting() const {
  std::lock_guard<std::mutex> lock(implementation_->mutex);
  return implementation_->stored_results.size();
}

}  // namespace pae::protocol_lab_ui
