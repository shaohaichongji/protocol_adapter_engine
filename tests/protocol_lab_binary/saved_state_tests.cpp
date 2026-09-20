#include <fstream>
#include <iostream>
#include <iterator>
#include <type_traits>

#include "prepared_binary.h"

namespace {
namespace b = pae::protocol_lab_binary;
namespace h = pae::host_endpoint;
void Check(bool ok, const char* detail) {
  if (!ok) throw std::runtime_error(detail);
}
template <class F>
void Reject(F call) {
  bool rejected = false;
  try {
    call();
  } catch (const b::MaterializationError&) {
    rejected = true;
  }
  Check(rejected, "expected rejection");
}
auto Prepare(const std::string& json, b::Limits limits = {},
             const b::PreparedBinary* previous = nullptr, b::ResourceLimits resources = {}) {
  auto compiled =
      pae::config_compiler::CompileJsonToPlanWithMetadata(json, 4U * 1024U * 1024U);
  Check(compiled.Succeeded(), "compile");
  const h::BindingSpec specs[] = {{"fixed", h::Action::DECODE, "fixed_rx", 2U, {32U}},
                                  {"sync", h::Action::DECODE, "sync_fixed_rx", 2U, {32U}}};
  return b::PreparedBinary::Create(std::move(compiled).TakeArtifacts(), specs, 2U, {}, limits, {},
                                   resources, previous);
}
void Run(const std::string& json) {
  auto owner = Prepare(json);
  static_assert(std::is_same_v<decltype(owner->Continue(0, 0)), const b::ObservedOperation&>);
  Check(!owner->Current(0, 0) && owner->Draft(0, 0).empty(), "initial state");
  Check(owner->DraftLimit(0, 0) == 128U, "draft cap follows chunk cap");
  owner->SaveAndSelect(u"unfinished Hex: Q", 0, 1);
  Check(owner->Draft(0, 0) == u"unfinished Hex: Q" && owner->Selected().flow == 1U,
        "opaque invalid draft saved");
  owner->SaveAndSelect(u"AA 12", 0, 0);
  const std::uint8_t chunk[] = {0xAA, 1, 2, 0xAA, 3, 4};
  const auto& first = owner->Submit(0, 0, {chunk, sizeof(chunk)});
  Check(first.candidate && owner->Current(0, 0) == &first, "actual result saved without copy");
  const auto operation = first.candidate->identity.operation;
  const auto before = owner->Stream(0, 0);
  owner->SaveAndSelect(owner->Draft(0, 0), 0, 1);
  Check(owner->Current(0, 0) == &first && owner->Draft(0, 1) == u"AA 12" &&
            owner->Stream(0, 0).consumed == before.consumed && owner->Stream(0, 0).can_continue,
        "switch preserves frozen/current");
  owner->SaveAndSelect(u"AA 12", 1, 0);
  owner->SaveAndSelect(u"A5", 0, 0);
  Check(owner->Current(0, 0)->candidate->identity.operation == operation &&
            owner->Draft(1, 0) == u"A5",
        "binding switch no execution");
  std::u16string exact(owner->DraftLimit(0, 0), u'X');
  owner->SaveAndSelect(exact, 0, 0);
  Reject([&] { owner->SaveAndSelect(exact + u'X', 0, 1); });
  Check(
      owner->Draft(0, 0) == exact && owner->Selected().flow == 0U && owner->Current(0, 0) == &first,
      "one over keeps old state");
  Reject([&] { owner->SaveAndSelect(u"changed", 99, 0); });
  owner->TestFailSave(true);
  Reject([&] { owner->SaveAndSelect(u"changed", 0, 1); });
  owner->TestFailSave(false);
  Check(owner->Draft(0, 0) == exact && owner->Selected().flow == 0U &&
            owner->Stream(0, 0).consumed == before.consumed && owner->Current(0, 0) == &first,
        "injected prepublication failure preserves all state");
  owner->SaveAndSelect(u"", 0, 1);
  Check(owner->Draft(0, 0).empty(), "empty draft");
  const std::uint8_t partial[] = {0xAA, 0x12};
  const auto& partial_result = owner->Submit(0, 1, {partial, sizeof(partial)});
  Check(owner->Current(0, 1) == &partial_result && !partial_result.candidate &&
            owner->Current(0, 0)->candidate.has_value(),
        "independent current results");
  const auto& next = owner->Continue(0, 0);
  Check(next.candidate && next.candidate->identity.operation > operation, "next replaces result");
  owner->Submit(0, 0, {nullptr, 1U});
  Check(owner->Current(0, 0)->host.status == h::Status::INVALID_ARGUMENT &&
            !owner->Current(0, 0)->candidate,
        "early reject clears stale success");
  const auto* other = owner->Current(0, 1);
  owner->Continue(99, 0);
  Check(owner->Current(0, 1) == other, "invalid route cannot publish to flow");
  owner->SaveAndSelect(u"AA 12", 0, 0);
  owner->Reset(0, 0);
  Check(!owner->Current(0, 0) && owner->Draft(0, 0).empty() && owner->Current(0, 1) == other &&
            owner->Observe(0, 1).framing.buffered_bytes == 2U && owner->Draft(0, 1) == u"AA 12",
        "reset clears target only");
  owner->Submit(0, 0, {chunk, 3U});
  owner->Submit(0, 0, {partial, 1U});
  Check(!owner->Current(0, 0)->candidate && owner->Current(0, 0)->host.framing_attempted,
        "no candidate step replaces old success");
  owner->Reset(0, 0);
  const std::uint8_t bad[] = {0, 0, 0};
  owner->Submit(0, 0, {bad, sizeof(bad)});
  Check(owner->Current(0, 0)->candidate && owner->Current(0, 0)->candidate->value.fields.empty(),
        "failed diagnostic saved");
  b::ResourceLimits small;
  small.replacement_bytes = owner->Resources().instance_admission_bytes;
  Reject([&] { (void)Prepare(json, {}, owner.get(), small); });
  Check(owner->Current(0, 1) == other && owner->Observe(0, 1).framing.buffered_bytes == 2U &&
            owner->Draft(0, 1) == u"AA 12",
        "failed rebind keeps saved and stream state");
  auto replacement = Prepare(json, {}, owner.get());
  Check(!replacement->Current(0, 1) && replacement->Draft(0, 1).empty() &&
            replacement->Instance() != owner->Instance(),
        "new instance has no stale saved state");
  b::Limits tiny;
  tiny.max_total_bytes = 1U;
  auto fault = Prepare(json, tiny);
  fault->Submit(0, 0, {chunk, sizeof(chunk)});
  Check(fault->Current(0, 0)->host.status == h::Status::CALLBACK_FAILED &&
            !fault->Current(0, 0)->candidate && fault->Stream(0, 0).consumed == 3U,
        "callback failure saved without candidate");
}
}  // namespace
int main(int argc, char** argv) {
  try {
    Check(argc == 2, "fixture argument");
    std::ifstream input(argv[1]);
    Check(input.good(), "fixture missing");
    Run({std::istreambuf_iterator<char>(input), {}});
    std::cout << "Binary saved-state checks passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
