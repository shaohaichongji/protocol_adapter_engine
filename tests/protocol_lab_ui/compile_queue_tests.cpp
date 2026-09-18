#include <cassert>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "../../tools/protocol_lab_ui/compile_worker.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
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
           completion->compiler_attempt_count == 1U && completion->public_compiled &&
           !completion->artifacts);
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
