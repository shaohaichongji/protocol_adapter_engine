#include <atomic>
#include <cassert>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "../../tools/protocol_lab_ui/compile_worker.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) ||       \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) ||        \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  {
    pae::CompileDiagnostic source;
    source.stage = pae::CompileStage::DOMAIN_VALIDATION;
    source.code = pae::CompileError::UNKNOWN_REFERENCE;
    source.json_pointer.clear();
    source.byte_offset.reset();
    source.detail = "<detail attr=\"x\">& raw text</detail>\n" + std::string(4096U, 'x');
    source.resource_kind = pae::ResourceKind::NONE;
    source.required_bytes = 0U;
    source.limit_bytes = 0U;
    source.resource_profile = pae::ResourceProfile::CONSTRAINED;
    const auto root = ProjectCompileDiagnostic(source);
    assert(root.stage == "DOMAIN_VALIDATION");
    assert(root.code == "UNKNOWN_REFERENCE");
    assert(root.json_pointer.empty());
    assert(!root.byte_offset);
    assert(root.resource_kind == "NONE" && !root.has_resource_budget);
    assert(root.required_bytes == 0U && root.limit_bytes == 0U);
    assert(root.resource_profile == "CONSTRAINED");
    const auto root_text = FormatCompileDiagnostic(root);
    assert(root_text.find("JSON 位置：根（空 pointer）") != std::string::npos);
    assert(root_text.find("字节偏移：未提供") != std::string::npos);
    assert(root_text.find("需要 / 限制：无资源预算数据") != std::string::npos);
    assert(root_text.find(source.detail) != std::string::npos);

    source.stage = pae::CompileStage::RESOURCE_BUDGET;
    source.code = pae::CompileError::RESOURCE_LIMIT_EXCEEDED;
    source.json_pointer = "/pipelines/0";
    source.byte_offset = 0U;
    source.resource_kind = pae::ResourceKind::METADATA_ACCOUNTED_MEMORY;
    source.required_bytes = 0U;
    source.limit_bytes = (std::numeric_limits<std::size_t>::max)();
    source.resource_profile = pae::ResourceProfile::DESKTOP;
    source.detail = "budget";
    const auto budget = ProjectCompileDiagnostic(source);
    assert(budget.stage == "RESOURCE_BUDGET");
    assert(budget.code == "RESOURCE_LIMIT_EXCEEDED");
    assert(budget.json_pointer == "/pipelines/0");
    assert(budget.byte_offset && *budget.byte_offset == 0U);
    assert(budget.resource_kind == "METADATA_ACCOUNTED_MEMORY" && budget.has_resource_budget);
    assert(budget.required_bytes == 0U);
    assert(budget.limit_bytes ==
           static_cast<std::uint64_t>((std::numeric_limits<std::size_t>::max)()));
    const auto budget_text = FormatCompileDiagnostic(budget);
    assert(budget_text.find("字节偏移：0") != std::string::npos);
    assert(budget_text.find("需要 / 限制：0 / ") != std::string::npos);
    source.byte_offset = 17U;
    assert(ProjectCompileDiagnostic(source).byte_offset == 17U);
  }
#endif
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  {
    pae::config_compiler::CompileDiagnostic source;
    source.stage = pae::config_compiler::CompileStage::JSON_SYNTAX;
    source.code = pae::config_compiler::CompileError::JSON_SYNTAX_ERROR;
    source.json_pointer = "/";
    source.byte_offset = 0U;
    source.detail = "private detail";
    source.resource_kind = pae::config_compiler::ResourceKind::UI_DESCRIPTION_ACCOUNTED_MEMORY;
    source.required_bytes = 0U;
    source.limit_bytes = 9U;
    source.resource_profile = pae::protocol_plan::ResourceProfile::DESKTOP;
    const auto private_view = ProjectCompileDiagnostic(source);
    assert(private_view.stage == "JSON_SYNTAX");
    assert(private_view.code == "JSON_SYNTAX_ERROR");
    assert(private_view.byte_offset && *private_view.byte_offset == 0U);
    assert(private_view.resource_kind == "UI_DESCRIPTION_ACCOUNTED_MEMORY");
    assert(private_view.has_resource_budget && private_view.required_bytes == 0U &&
           private_view.limit_bytes == 9U);
    assert(private_view.resource_profile == "DESKTOP");
  }
#endif
  {
    CompileWorker diagnostic_worker;
    DocumentId next_document = 700U;
    const auto compile_failure = [&](std::string_view json, SchemaDispatchStatus route) {
      const DocumentId document = next_document++;
      assert(diagnostic_worker.Submit(document, 1U, json) == SubmitStatus::ACCEPTED);
      assert(test::WaitUntil([&diagnostic_worker] {
        return !diagnostic_worker.HasActiveRequestForTesting() &&
               diagnostic_worker.StoredResultCountForTesting() == 1U;
      }));
      const auto tickets = diagnostic_worker.DrainReadyTickets();
      assert(tickets.size() == 1U);
      auto completion = diagnostic_worker.TakeResult(tickets.front());
      assert(completion && completion->document_id == document && completion->route == route &&
             completion->structured_compile_diagnostic);
      return completion;
    };
#if defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
    {
      auto completion =
          compile_failure(R"({"schema_version":"0.5"})", SchemaDispatchStatus::LEGACY_PUBLIC);
      assert(completion->public_diagnostic);
      assert(completion->structured_compile_diagnostic->detail ==
             completion->public_diagnostic->detail);
    }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    {
      auto completion =
          compile_failure(R"({"schema_version":"0.9"})", SchemaDispatchStatus::BINARY_PUBLIC);
      assert(completion->public_diagnostic);
      assert(completion->structured_compile_diagnostic->code != "UNKNOWN");
    }
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
    {
      auto completion =
          compile_failure(R"({"schema_version":"0.10"})", SchemaDispatchStatus::ASCII_PUBLIC);
      assert(completion->public_diagnostic);
      assert(completion->structured_compile_diagnostic->stage != "UNKNOWN");
    }
#endif
  }
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
  {
    std::ifstream input(std::filesystem::path{PAE_ASCII_STREAM_CONFIG}, std::ios::binary);
    const std::string config{std::istreambuf_iterator<char>{input}, {}};
    CompileWorker public_worker;
    assert(public_worker.Submit(99U, 7U, config) == SubmitStatus::ACCEPTED);
    assert(test::WaitUntil([&public_worker] {
      return !public_worker.HasActiveRequestForTesting() &&
             public_worker.StoredResultCountForTesting() == 1U;
    }));
    const auto tickets = public_worker.DrainReadyTickets();
    assert(tickets.size() == 1U);
    const auto completion = public_worker.TakeResult(tickets.front());
    assert(completion && completion->route == SchemaDispatchStatus::ASCII_PUBLIC &&
           completion->compiler_attempt_count == 1U && completion->public_compiled);
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
    assert(!completion->artifacts);
#endif
  }
#endif
  std::atomic<std::size_t> idle_compile_calls{0U};
  {
    CompileWorker idle_worker([&idle_compile_calls](CompileWorker::Request) {
      ++idle_compile_calls;
      return std::unique_ptr<CompileCompletion>{};
    });
  }
  assert(idle_compile_calls.load() == 0U);

  struct Control {
    std::mutex mutex;
    std::condition_variable wake;
    std::vector<std::pair<DocumentId, Revision>> started;
    std::size_t permits = 0U;
  } control;

  CompileWorker worker([&control](CompileWorker::Request request) {
    {
      std::unique_lock<std::mutex> lock(control.mutex);
      control.started.emplace_back(request.document_id, request.load_revision);
      control.wake.notify_all();
      control.wake.wait(lock, [&control] { return control.permits != 0U; });
      --control.permits;
    }
    auto completion = std::make_unique<CompileCompletion>();
    completion->document_id = request.document_id;
    completion->load_revision = request.load_revision;
    return completion;
  });

  assert(worker.Submit(1U, 1U, "first") == SubmitStatus::ACCEPTED);
  assert(test::WaitUntil([&worker] { return worker.HasActiveRequestForTesting(); }));
  assert(worker.Submit(1U, 2U, "old") == SubmitStatus::ACCEPTED);
  assert(worker.Submit(2U, 1U, "other") == SubmitStatus::ACCEPTED);
  assert(worker.Submit(1U, 3U, "latest") == SubmitStatus::ACCEPTED);
  assert(worker.PendingCountForTesting() == 2U);

  {
    std::lock_guard<std::mutex> lock(control.mutex);
    ++control.permits;
    control.wake.notify_all();
  }
  assert(test::WaitUntil([&control] {
    std::lock_guard<std::mutex> lock(control.mutex);
    return control.started.size() >= 2U;
  }));
  {
    std::lock_guard<std::mutex> lock(control.mutex);
    assert(control.started[1].first == 2U && control.started[1].second == 1U);
  }

  worker.CloseDocument(1U);
  assert(worker.PendingCountForTesting() == 0U);
  {
    std::lock_guard<std::mutex> lock(control.mutex);
    ++control.permits;
    control.wake.notify_all();
  }
  assert(test::WaitUntil([&worker] { return !worker.HasActiveRequestForTesting(); }));
  return 0;
}
