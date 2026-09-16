#include <fstream>
#include <iostream>
#include <iterator>

#include "prepared_binary.h"

namespace {
namespace b = pae::protocol_lab_binary;
namespace h = pae::host_endpoint;
void Check(bool ok, const char* detail) {
  if (!ok) throw std::runtime_error(detail);
}
auto Prepare(const std::string& json, std::size_t work = 0U, const b::Limits& limits = {}) {
  auto compiled =
      pae::config_compiler::CompileJsonToPlanWithUiDescription(json, 4U * 1024U * 1024U);
  Check(compiled.Succeeded(), "compile");
  h::BindingSpec specs[] = {{"fixed", h::Action::DECODE, "fixed_rx", 2U},
                            {"sync", h::Action::DECODE, "sync_fixed_rx", 2U},
                            {"length", h::Action::DECODE, "sync_length_rx", 2U}};
  for (auto& spec : specs) {
    spec.framing_limits.max_submit_bytes = 32U;
    spec.framing_limits.max_work_units = work;
  }
  if (work) specs[0].framing_limits.max_work_units = 4U;
  return b::PreparedBinary::Create(std::move(compiled).TakeArtifacts(), specs, 3U, {}, limits);
}
auto Submit(b::PreparedBinary& owner, std::size_t binding, std::size_t flow,
            const std::vector<std::uint8_t>& bytes) {
  return owner.Submit(binding, flow, {bytes.data(), bytes.size()});
}
void Run(const std::string& json) {
  auto owner = Prepare(json);
  Check(owner->Stream(0, 0).capacity == 32U, "chunk limit");
  Check(!Submit(*owner, 0, 0, {0xAA}).candidate, "partial no candidate");
  Check(!owner->Stream(0, 0).can_continue && owner->Observe(0, 0).framing.buffered_bytes == 1U,
        "partial is not internal work");
  Check(owner->Continue(0, 0).host.status == h::Status::INVALID_ARGUMENT, "no empty flush");
  Check(!Submit(*owner, 0, 1, {0xAA, 0x12}).candidate, "second partial");
  auto first = Submit(*owner, 0, 0, {1, 2, 0, 0, 0, 0xAA, 3, 4});
  Check(first.candidate && first.host.decode_successes == 1U && first.host.successful_outputs == 1U,
        "first stop");
  Check(owner->Stream(0, 0).consumed == 2U && owner->Stream(0, 0).frozen_size == 8U,
        "exact suffix");
  Check(Submit(*owner, 0, 0, {0xAA, 9, 9}).host.status == h::Status::BUSY, "pending submit");
  auto bad = owner->Continue(0, 0);
  Check(bad.candidate && bad.candidate->value.fields.empty() && bad.host.decode_failures == 1U &&
            bad.host.successful_outputs == 0U && owner->Stream(0, 0).consumed == 5U,
        "failed candidate stops without skipping suffix");
  auto good = owner->Continue(0, 0);
  Check(good.candidate && good.host.decode_successes == 1U && !owner->Stream(0, 0).can_continue,
        "good after bad");
  Check(owner->Reset(0, 0) == h::Status::OK && owner->Observe(0, 1).framing.buffered_bytes == 2U,
        "reset isolation");
  const auto& wrong_kind = owner->Decode(0, 0, {nullptr, 0U});
  Check(wrong_kind.host.status == h::Status::WRONG_INPUT_KIND &&
            wrong_kind.host.generation == owner->Observe(0, 0).generation &&
            wrong_kind.host.generation == 1U,
        "Decode early rejection preserves current generation");
  auto second = Submit(*owner, 0, 1, {0x34});
  Check(second.candidate && second.candidate->identity.flow == 1U &&
            second.candidate->identity.generation == 0U,
        "second flow identity");
  auto sync = Submit(*owner, 1, 0, {0, 0xA5, 0x5A, 1, 2, 0xA5, 0x5A, 3, 4});
  Check(sync.candidate && sync.host.framing.bytes_discarded == 1U &&
            owner->Continue(1, 0).candidate.has_value(),
        "sync fixed recovery");
  auto length = Submit(*owner, 2, 0, {0xC3, 0x3C, 6, 1, 2, 0x55, 0xC3, 0x3C, 6, 3, 4, 0x55});
  Check(length.candidate && owner->Continue(2, 0).candidate.has_value(), "length suffix");
  const auto before = owner->Observe(0, 1);
  Check(Submit(*owner, 0, 1, std::vector<std::uint8_t>(33U)).host.status ==
                h::Status::LIMIT_EXCEEDED &&
            owner->Observe(0, 1).generation == before.generation &&
            owner->Stream(0, 1).frozen_size == 0U,
        "oversize early reject");
  Check(owner->Submit(0, 1, {nullptr, 1}).host.status == h::Status::INVALID_ARGUMENT &&
            owner->Continue(9, 0).host.status == h::Status::INVALID_BINDING,
        "invalid inputs");
  Submit(*owner, 0, 0, {0xAA, 1, 2, 0xAA, 3, 4});
  Check(owner->Reset(0, 0) == h::Status::OK && !owner->Stream(0, 0).can_continue &&
            owner->Stream(0, 0).frozen_size == 0U,
        "reset drops frozen");

  // Tiny work budget forces resumable steps; no automatic retry loop in the owner.
  auto limited = Prepare(json, 5U);
  auto step = Submit(*limited, 0, 0, {0xAA, 1, 2});
  bool saw_budget = false;
  for (unsigned n = 0; !step.candidate && n < 20U; ++n) {
    const auto state = limited->Stream(0, 0);
    Check(state.can_continue, "budget continuation available");
    saw_budget = true;
    step = limited->Continue(0, 0);
  }
  Check(step.candidate && saw_budget, "work budget resumes suffix");
  Submit(*limited, 1, 0, {0xA5, 0x5A});
  Check(limited->Stream(1, 0).frozen_size == 0U && limited->Observe(1, 0).framing.has_internal_work,
        "internal copy pending");
  auto internal = limited->Continue(1, 0);
  Check(internal.host.status == h::Status::OK && internal.host.framing.bytes_consumed == 0U &&
            !internal.candidate && !limited->Stream(1, 0).can_continue,
        "empty Push drains only internal work");
  Check(Submit(*limited, 1, 0, {1, 2}).candidate.has_value(), "complete after internal copy");

  b::Limits small;
  small.max_total_bytes = 1U;
  auto faulted = Prepare(json, 0U, small);
  Check(faulted->Reset(0, 0) == h::Status::OK, "fault test starts at generation one");
  auto failed = Submit(*faulted, 0, 0, {0xAA, 1, 2, 0xAA, 3, 4});
  Check(failed.host.status == h::Status::CALLBACK_FAILED && !failed.candidate &&
            failed.host.successful_outputs == 0U && faulted->Stream(0, 0).consumed == 3U &&
            faulted->Continue(0, 0).host.status == h::Status::RESET_REQUIRED &&
            !faulted->Observe(0, 1).reset_required,
        "copy failure consumption and isolation");
  const auto& fault_rejection = faulted->Decode(0, 0, {nullptr, 0U});
  Check(fault_rejection.host.status == h::Status::RESET_REQUIRED &&
            fault_rejection.host.generation == 1U && faulted->Stream(0, 0).consumed == 3U,
        "Decode fault rejection retains generation and frozen consumption");
}
}  // namespace
int main(int argc, char** argv) {
  try {
    Check(argc == 2, "fixture argument");
    std::ifstream input(argv[1]);
    Check(input.good(), "fixture missing");
    Run({std::istreambuf_iterator<char>(input), {}});
    std::cout << "Binary frozen stream checks passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
