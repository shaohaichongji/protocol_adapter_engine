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

template <typename Stage>
const char* CompileStageToken(Stage stage) noexcept {
  switch (stage) {
    case Stage::INPUT_PROFILE:
      return "INPUT_PROFILE";
    case Stage::JSON_SYNTAX:
      return "JSON_SYNTAX";
    case Stage::JSON_RESOURCE:
      return "JSON_RESOURCE";
    case Stage::STRUCTURAL:
      return "STRUCTURAL";
    case Stage::DOMAIN_VALIDATION:
      return "DOMAIN_VALIDATION";
    case Stage::RESOURCE_BUDGET:
      return "RESOURCE_BUDGET";
    case Stage::PLAN_BUILD:
      return "PLAN_BUILD";
    case Stage::INTERNAL:
      return "INTERNAL";
  }
  return "UNKNOWN";
}

#define PAE_COMPILE_ERROR_COMMON_CASES(Error)    \
  case Error::NONE:                              \
    return "NONE";                               \
  case Error::EMPTY_INPUT:                       \
    return "EMPTY_INPUT";                        \
  case Error::INPUT_LIMIT_EXCEEDED:              \
    return "INPUT_LIMIT_EXCEEDED";               \
  case Error::UTF8_BOM_NOT_ALLOWED:              \
    return "UTF8_BOM_NOT_ALLOWED";               \
  case Error::INVALID_UTF8:                      \
    return "INVALID_UTF8";                       \
  case Error::INVALID_UNICODE_ESCAPE:            \
    return "INVALID_UNICODE_ESCAPE";             \
  case Error::NESTING_DEPTH_LIMIT_EXCEEDED:      \
    return "NESTING_DEPTH_LIMIT_EXCEEDED";       \
  case Error::JSON_SYNTAX_ERROR:                 \
    return "JSON_SYNTAX_ERROR";                  \
  case Error::JSON_ALLOCATION_FAILED:            \
    return "JSON_ALLOCATION_FAILED";             \
  case Error::JSON_PARSER_MEMORY_LIMIT_EXCEEDED: \
    return "JSON_PARSER_MEMORY_LIMIT_EXCEEDED";  \
  case Error::JSON_NODE_LIMIT_EXCEEDED:          \
    return "JSON_NODE_LIMIT_EXCEEDED";           \
  case Error::OBJECT_MEMBER_LIMIT_EXCEEDED:      \
    return "OBJECT_MEMBER_LIMIT_EXCEEDED";       \
  case Error::ARRAY_ELEMENT_LIMIT_EXCEEDED:      \
    return "ARRAY_ELEMENT_LIMIT_EXCEEDED";       \
  case Error::STRING_LIMIT_EXCEEDED:             \
    return "STRING_LIMIT_EXCEEDED";              \
  case Error::DECODED_STRING_BUDGET_EXCEEDED:    \
    return "DECODED_STRING_BUDGET_EXCEEDED";     \
  case Error::NUMBER_TOKEN_LIMIT_EXCEEDED:       \
    return "NUMBER_TOKEN_LIMIT_EXCEEDED";        \
  case Error::DUPLICATE_KEY:                     \
    return "DUPLICATE_KEY";                      \
  case Error::ROOT_MUST_BE_OBJECT:               \
    return "ROOT_MUST_BE_OBJECT";                \
  case Error::MISSING_PROPERTY:                  \
    return "MISSING_PROPERTY";                   \
  case Error::UNKNOWN_PROPERTY:                  \
    return "UNKNOWN_PROPERTY";                   \
  case Error::TYPE_MISMATCH:                     \
    return "TYPE_MISMATCH";                      \
  case Error::INVALID_STRING_LENGTH:             \
    return "INVALID_STRING_LENGTH";              \
  case Error::INVALID_ID:                        \
    return "INVALID_ID";                         \
  case Error::INVALID_ENUM_VALUE:                \
    return "INVALID_ENUM_VALUE";                 \
  case Error::INTEGER_NOT_EXACT:                 \
    return "INTEGER_NOT_EXACT";                  \
  case Error::INTEGER_OUT_OF_RANGE:              \
    return "INTEGER_OUT_OF_RANGE";               \
  case Error::INVALID_HEX_BYTES:                 \
    return "INVALID_HEX_BYTES";                  \
  case Error::EMPTY_ARRAY:                       \
    return "EMPTY_ARRAY";                        \
  case Error::UNSUPPORTED_FEATURE:               \
    return "UNSUPPORTED_FEATURE";                \
  case Error::DUPLICATE_ID:                      \
    return "DUPLICATE_ID";                       \
  case Error::DUPLICATE_REFERENCE:               \
    return "DUPLICATE_REFERENCE";                \
  case Error::UNKNOWN_REFERENCE:                 \
    return "UNKNOWN_REFERENCE";                  \
  case Error::DIRECTION_MISMATCH:                \
    return "DIRECTION_MISMATCH";                 \
  case Error::FIELD_OUT_OF_BOUNDS:               \
    return "FIELD_OUT_OF_BOUNDS";                \
  case Error::FIELD_OVERLAP:                     \
    return "FIELD_OVERLAP";                      \
  case Error::FRAME_NOT_FULLY_DEFINED:           \
    return "FRAME_NOT_FULLY_DEFINED";            \
  case Error::VALUE_NOT_REPRESENTABLE:           \
    return "VALUE_NOT_REPRESENTABLE";            \
  case Error::MATCHER_OUT_OF_BOUNDS:             \
    return "MATCHER_OUT_OF_BOUNDS";              \
  case Error::MATCHER_CONFLICT:                  \
    return "MATCHER_CONFLICT";                   \
  case Error::INTEGRITY_RANGE_OUT_OF_BOUNDS:     \
    return "INTEGRITY_RANGE_OUT_OF_BOUNDS";      \
  case Error::INTEGRITY_STORAGE_OUT_OF_BOUNDS:   \
    return "INTEGRITY_STORAGE_OUT_OF_BOUNDS";    \
  case Error::INTEGRITY_SELF_INCLUDED:           \
    return "INTEGRITY_SELF_INCLUDED";            \
  case Error::INTEGRITY_STORAGE_CONFLICT:        \
    return "INTEGRITY_STORAGE_CONFLICT";         \
  case Error::AMBIGUOUS_MATCHER:                 \
    return "AMBIGUOUS_MATCHER";                  \
  case Error::RESOURCE_LIMIT_EXCEEDED:           \
    return "RESOURCE_LIMIT_EXCEEDED";            \
  case Error::COMPILER_ALLOCATION_FAILED:        \
    return "COMPILER_ALLOCATION_FAILED";         \
  case Error::INTERNAL_CONTRACT_VIOLATION:       \
    return "INTERNAL_CONTRACT_VIOLATION"

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
const char* CompileErrorToken(config_compiler::CompileError code) noexcept {
  switch (code) {
    PAE_COMPILE_ERROR_COMMON_CASES(config_compiler::CompileError);
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
    case config_compiler::CompileError::ASCII_LITERAL_INVALID:
      return "ASCII_LITERAL_INVALID";
    case config_compiler::CompileError::ASCII_CONTROL_BYTE_INVALID:
      return "ASCII_CONTROL_BYTE_INVALID";
    case config_compiler::CompileError::ASCII_FIELD_LENGTH_INVALID:
      return "ASCII_FIELD_LENGTH_INVALID";
    case config_compiler::CompileError::ASCII_FIELD_REFERENCE_INVALID:
      return "ASCII_FIELD_REFERENCE_INVALID";
    case config_compiler::CompileError::ASCII_FIELD_BOUNDARY_AMBIGUOUS:
      return "ASCII_FIELD_BOUNDARY_AMBIGUOUS";
    case config_compiler::CompileError::ASCII_TEMPLATE_EMPTY:
      return "ASCII_TEMPLATE_EMPTY";
#if defined(PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING)
    case config_compiler::CompileError::ASCII_STREAM_TERMINATOR_INVALID:
      return "ASCII_STREAM_TERMINATOR_INVALID";
    case config_compiler::CompileError::ASCII_STREAM_BOUNDARY_UNPROVEN:
      return "ASCII_STREAM_BOUNDARY_UNPROVEN";
    case config_compiler::CompileError::ASCII_STREAM_PROFILE_MISMATCH:
      return "ASCII_STREAM_PROFILE_MISMATCH";
#endif
#endif
  }
  return "UNKNOWN";
}

const char* ResourceKindToken(config_compiler::ResourceKind kind) noexcept {
  switch (kind) {
    case config_compiler::ResourceKind::NONE:
      return "NONE";
    case config_compiler::ResourceKind::PLAN_ACCOUNTED_MEMORY:
      return "PLAN_ACCOUNTED_MEMORY";
    case config_compiler::ResourceKind::UI_DESCRIPTION_ACCOUNTED_MEMORY:
      return "UI_DESCRIPTION_ACCOUNTED_MEMORY";
  }
  return "UNKNOWN";
}
#endif

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) ||       \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) ||        \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
const char* CompileErrorToken(pae::CompileError code) noexcept {
  switch (code) {
    PAE_COMPILE_ERROR_COMMON_CASES(pae::CompileError);
    case pae::CompileError::ASCII_LITERAL_INVALID:
      return "ASCII_LITERAL_INVALID";
    case pae::CompileError::ASCII_CONTROL_BYTE_INVALID:
      return "ASCII_CONTROL_BYTE_INVALID";
    case pae::CompileError::ASCII_FIELD_LENGTH_INVALID:
      return "ASCII_FIELD_LENGTH_INVALID";
    case pae::CompileError::ASCII_FIELD_REFERENCE_INVALID:
      return "ASCII_FIELD_REFERENCE_INVALID";
    case pae::CompileError::ASCII_FIELD_BOUNDARY_AMBIGUOUS:
      return "ASCII_FIELD_BOUNDARY_AMBIGUOUS";
    case pae::CompileError::ASCII_TEMPLATE_EMPTY:
      return "ASCII_TEMPLATE_EMPTY";
    case pae::CompileError::ASCII_STREAM_TERMINATOR_INVALID:
      return "ASCII_STREAM_TERMINATOR_INVALID";
    case pae::CompileError::ASCII_STREAM_BOUNDARY_UNPROVEN:
      return "ASCII_STREAM_BOUNDARY_UNPROVEN";
    case pae::CompileError::ASCII_STREAM_PROFILE_MISMATCH:
      return "ASCII_STREAM_PROFILE_MISMATCH";
  }
  return "UNKNOWN";
}

const char* ResourceKindToken(pae::ResourceKind kind) noexcept {
  switch (kind) {
    case pae::ResourceKind::NONE:
      return "NONE";
    case pae::ResourceKind::PLAN_ACCOUNTED_MEMORY:
      return "PLAN_ACCOUNTED_MEMORY";
    case pae::ResourceKind::METADATA_ACCOUNTED_MEMORY:
      return "METADATA_ACCOUNTED_MEMORY";
  }
  return "UNKNOWN";
}
#endif

#undef PAE_COMPILE_ERROR_COMMON_CASES

template <typename Profile>
const char* ResourceProfileToken(Profile profile) noexcept {
  switch (profile) {
    case Profile::DESKTOP:
      return "DESKTOP";
    case Profile::CONSTRAINED:
      return "CONSTRAINED";
  }
  return "UNKNOWN";
}

std::unique_ptr<CompileCompletion> CompileRequest(CompileWorker::Request request) {
  auto completion = std::make_unique<CompileCompletion>();
  completion->document_id = request.document_id;
  completion->load_revision = request.load_revision;
  completion->config_sha256 = protocol_lab::HashBytes(request.config_text);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  const auto dispatch = ClassifySchemaVersion(request.config_text);
  completion->route = dispatch.status;
  if (dispatch.status == SchemaDispatchStatus::CLASSIFICATION_FAILED) {
    completion->classification_error = dispatch.detail;
    return completion;
  }
  if (dispatch.status == SchemaDispatchStatus::BINARY_PUBLIC
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
      || dispatch.status == SchemaDispatchStatus::LEGACY_PUBLIC
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
      || dispatch.status == SchemaDispatchStatus::ASCII_PUBLIC
#endif
  ) {
    ++completion->compiler_attempt_count;
    auto result = pae::CompileProtocolJson(request.config_text);
    if (!result.Succeeded()) {
      if (result.Diagnostic()) { completion->public_diagnostic = *result.Diagnostic();
        completion->structured_compile_diagnostic = ProjectCompileDiagnostic(*result.Diagnostic());
      }
      return completion;
    }
    completion->public_compiled =
        std::make_unique<pae::CompiledProtocol>(std::move(result).TakeCompiled());
    return completion;
  }
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  const std::size_t desktop_limit =
      config_compiler::DerivedProtocolMetadataMemoryLimit(protocol_plan::ResourceProfile::DESKTOP);
  ++completion->compiler_attempt_count;
  auto result =
      config_compiler::CompileJsonToPlanWithMetadata(request.config_text, desktop_limit);
  if (!result.Succeeded()) {
    if (result.Diagnostic() != nullptr) { completion->diagnostic = *result.Diagnostic();
      completion->structured_compile_diagnostic = ProjectCompileDiagnostic(*result.Diagnostic());
    }
    return completion;
  }
  auto artifacts = std::make_unique<config_compiler::CompiledProtocolArtifacts>(
      std::move(result).TakeArtifacts());
  const auto* plan = artifacts->Plan();
  if (plan == nullptr ||
      artifacts->DescriptionMemory().accounted_total_bytes >
          config_compiler::DerivedProtocolMetadataMemoryLimit(plan->GetResourceProfile())) {
    const config_compiler::CompileDiagnostic diagnostic{
        config_compiler::CompileStage::INTERNAL,
        config_compiler::CompileError::INTERNAL_CONTRACT_VIOLATION, "", std::nullopt,
        "protocol metadata exceeds the derived limit for its Plan resource profile"};
    completion->diagnostic = diagnostic;
    completion->structured_compile_diagnostic = ProjectCompileDiagnostic(diagnostic);
    return completion;
  }
  completion->artifacts = std::move(artifacts);
#else
  completion->diagnostic =
      CompileCompletion::Diagnostic{"schema is not routed to an installed-SDK public compiler"};
#endif
  return completion;
}

}  // namespace

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
CompileDiagnosticView ProjectCompileDiagnostic(
    const config_compiler::CompileDiagnostic& diagnostic) {
  static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
  return {CompileStageToken(diagnostic.stage),
          CompileErrorToken(diagnostic.code),
          diagnostic.json_pointer,
          diagnostic.byte_offset,
          ResourceKindToken(diagnostic.resource_kind),
          diagnostic.resource_kind != config_compiler::ResourceKind::NONE,
          static_cast<std::uint64_t>(diagnostic.required_bytes),
          static_cast<std::uint64_t>(diagnostic.limit_bytes),
          ResourceProfileToken(diagnostic.resource_profile),
          diagnostic.detail};
}
#endif

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) ||       \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) ||        \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
CompileDiagnosticView ProjectCompileDiagnostic(const pae::CompileDiagnostic& diagnostic) {
  static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
  return {CompileStageToken(diagnostic.stage),
          CompileErrorToken(diagnostic.code),
          diagnostic.json_pointer,
          diagnostic.byte_offset,
          ResourceKindToken(diagnostic.resource_kind),
          diagnostic.resource_kind != pae::ResourceKind::NONE,
          static_cast<std::uint64_t>(diagnostic.required_bytes),
          static_cast<std::uint64_t>(diagnostic.limit_bytes),
          ResourceProfileToken(diagnostic.resource_profile),
          diagnostic.detail};
}
#endif

std::string FormatCompileDiagnostic(const CompileDiagnosticView& diagnostic) {
  std::string text = "阶段：" + diagnostic.stage + "\n错误码：" + diagnostic.code;
  text += "\nJSON 位置：";
  text += diagnostic.json_pointer.empty() ? "根（空 pointer）" : diagnostic.json_pointer;
  text += "\n字节偏移：";
  text += diagnostic.byte_offset ? std::to_string(*diagnostic.byte_offset) : "未提供";
  text += "\n资源类型：" + diagnostic.resource_kind;
  text += "\n需要 / 限制：";
  if (diagnostic.has_resource_budget) {
    text +=
        std::to_string(diagnostic.required_bytes) + " / " + std::to_string(diagnostic.limit_bytes);
  } else {
    text += "无资源预算数据";
  }
  text += "\n资源配置：" + diagnostic.resource_profile;
  text += "\n技术详情：" + diagnostic.detail;
  return text;
}

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
            pending.begin(), pending.end(), [](const auto& left, const auto& right) {
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
        completion->diagnostic =
            CompileCompletion::Diagnostic{"compile worker function threw an exception"};
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
          std::remove(implementation_->ready_tickets.begin(), implementation_->ready_tickets.end(),
                      ticket),
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
