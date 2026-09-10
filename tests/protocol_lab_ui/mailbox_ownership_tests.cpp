#include <cassert>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>

#include "../../tools/protocol_lab_ui/compile_worker.h"
#include "test_support.h"

int main() {
  using namespace pae::protocol_lab_ui;
  struct Gate {
    std::mutex mutex;
    std::condition_variable wake;
    bool entered = false;
    bool released = false;
    std::size_t calls = 0U;
  } gate;
  {
    CompileWorker worker([&gate](CompileWorker::Request request) {
      {
        std::unique_lock<std::mutex> lock(gate.mutex);
        ++gate.calls;
        gate.entered = true;
        gate.wake.notify_all();
        gate.wake.wait(lock, [&gate] { return gate.released; });
      }
      auto result = std::make_unique<CompileCompletion>();
      result->document_id = request.document_id;
      result->load_revision = request.load_revision;
      return result;
    });

    const std::string oversized(kMaximumConfigBytes + 1U, 'x');
    assert(worker.Submit(5U, 1U, oversized) == SubmitStatus::CONFIG_TOO_LARGE);
    assert(worker.Submit(6U, 1U, "bounded") == SubmitStatus::ACCEPTED);
    assert(test::WaitUntil([&gate] {
      std::lock_guard<std::mutex> lock(gate.mutex);
      return gate.entered;
    }));
    worker.CloseDocument(6U);
    {
      std::lock_guard<std::mutex> lock(gate.mutex);
      gate.released = true;
      gate.wake.notify_all();
    }
    assert(test::WaitUntil([&worker] { return !worker.HasActiveRequestForTesting(); }));
    assert(worker.StoredResultCountForTesting() == 0U);
    assert(worker.DrainReadyTickets().empty());
    assert(worker.Submit(6U, 2U, "late") == SubmitStatus::DOCUMENT_CLOSED);
  }
  std::lock_guard<std::mutex> lock(gate.mutex);
  assert(gate.calls == 1U);
  return 0;
}
