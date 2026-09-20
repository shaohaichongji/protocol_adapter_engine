#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

#include "binary_admission.h"
#include "prepared_binary.h"

namespace {
namespace b = pae::protocol_lab_binary;
namespace h = pae::host_endpoint;
namespace c = pae::protocol_core;
void Check(bool condition, const char* detail) {
  if (!condition) throw std::runtime_error(detail);
}
std::string Read(const std::string& path) {
  std::ifstream input(path);
  Check(input.good(), "fixture unavailable");
  return {std::istreambuf_iterator<char>(input), {}};
}
std::string Replace(std::string json, const std::string& old, const std::string& value) {
  const auto position = json.find(old);
  Check(position != std::string::npos, "replacement absent");
  json.replace(position, old.size(), value);
  return json;
}
auto Compile(const std::string& json) {
  auto result = pae::config_compiler::CompileJsonToPlanWithMetadata(json, 4U * 1024U * 1024U);
  if (!result.Succeeded() && result.Diagnostic()) std::cerr << result.Diagnostic()->detail << '\n';
  Check(result.Succeeded(), "fixture compile failed");
  return std::move(result).TakeArtifacts();
}
template <class Function>
void Reject(Function function) {
  bool rejected = false;
  try {
    function();
  } catch (const b::MaterializationError&) {
    rejected = true;
  }
  Check(rejected, "expected rejection");
}
auto Prepare(const std::string& json, const b::Limits& limits = {}) {
  const h::BindingSpec bindings[] = {{"device", h::Action::DECODE, "ui_pipeline", 2U},
                                     {"other", h::Action::DECODE, "alternate_pipeline", 1U},
                                     {"device", h::Action::ENCODE, "ui_pipeline", 1U}};
  return b::PreparedBinary::Create(Compile(json), bindings, 3U, {7U, 8U, 9U}, limits);
}
void Complete(const std::string& json) {
  auto owner = Prepare(json);
  const std::vector<std::uint8_t> frame{0x80U, 0x0DU, 3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU};
  auto first = owner->Decode(0U, 0U, {frame.data(), frame.size()});
  Check(first.host.status == h::Status::OK && first.host.observed_candidates == 1U &&
            first.host.successful_outputs == 1U && first.candidate,
        "complete candidate");
  auto saved = *first.candidate;
  Check(owner->Description().messages[0].fields[7].conversion &&
            owner->Description().messages[0].fields[7].conversion->scale_numerator == 1 &&
            owner->Description().messages[0].fields[7].conversion->scale_denominator == 1U,
        "owned conversion metadata");
  Check(saved.identity.revisions.tab == 7U && saved.identity.revisions.load == 8U &&
            saved.identity.revisions.session == 9U && saved.identity.operation == 1U &&
            saved.identity.binding == 0U && saved.identity.flow == 0U &&
            saved.identity.generation == 0U,
        "real observation identity");
  Check(saved.presentation[1].enum_display_text == "Active" &&
            saved.presentation[2].physical_bits.size() == 2U &&
            saved.presentation[6].byte_range->length == saved.value.fields[6].bytes_value.size(),
        "enum and physical association");
  const auto second = owner->Decode(0U, 1U, {frame.data(), frame.size()});
  Check(second.candidate && second.candidate->identity.flow == 1U &&
            second.candidate->identity.generation == 0U &&
            second.candidate->identity.operation == 2U,
        "same generation different flow");
  Check(owner->Reset(0U, 1U) == h::Status::OK, "reset");
  auto third = owner->Decode(0U, 1U, {frame.data(), frame.size()});
  Check(third.candidate && third.candidate->identity.generation == 1U &&
            owner->Observe(0U, 0U).generation == 0U,
        "reset isolation");
  const std::uint8_t alternate[]{0xC3U, 7U};
  auto other = owner->Decode(1U, 0U, {alternate, 2U});
  Check(other.candidate && other.candidate->identity.pipeline == 1U &&
            other.candidate->value.message_id == "alternate_record",
        "bound message membership");
  auto failed = owner->Decode(0U, 0U, {alternate, 2U});
  Check(failed.host.status == h::Status::CODEC_FAILED && failed.candidate &&
            failed.candidate->value.frame.empty() &&
            !failed.candidate->value.diagnostic_frame.empty() &&
            failed.candidate->presentation.empty() && !failed.candidate->integrity_storage,
        "unknown message diagnostic only");
  Check(owner->Decode(99U, 0U, {}).host.status == h::Status::INVALID_BINDING &&
            owner->Decode(0U, 2U, {}).host.status == h::Status::INVALID_BINDING &&
            owner->Decode(2U, 0U, {}).host.status == h::Status::INVALID_BINDING,
        "invalid routing");
  h::Candidate probe;
  auto foreign = Compile(json);
  probe.plan = foreign.Plan();
  Reject([&] { (void)owner->TestAssociate(probe, 0U, 0U, 0U); });
  probe.plan = &owner->TestPlan();
  probe.decoded.message_index = 1U;
  Reject([&] { (void)owner->TestAssociate(probe, 0U, 0U, 0U); });
  probe.decoded.message_index = c::kInvalidIndex;
  Reject([&] { (void)owner->TestAssociate(probe, 0U, 1U, 1U); });
  auto replacement = Prepare(json);
  Check(replacement->Instance() != owner->Instance(), "new preparation unique instance");
  b::Limits small_strings;
  small_strings.max_string_bytes =
      saved.value.budget.string_bytes + saved.value.fields.size() * 32U - 1U;
  auto constrained = Prepare(json, small_strings);
  const auto rejected = constrained->Decode(0U, 0U, {frame.data(), frame.size()});
  Check(rejected.host.status == h::Status::CALLBACK_FAILED && !rejected.candidate &&
            rejected.host.successful_outputs == 0U && constrained->Observe(0U, 0U).reset_required,
        "string association budget prevents publication and business delivery");
  Check(constrained->Decode(0U, 0U, {frame.data(), frame.size()}).host.status ==
            h::Status::RESET_REQUIRED,
        "copy fault requires reset");
  auto unknown_owner = Prepare(Replace(json, "\"reject\"", "\"preserve\""));
  auto unknown_frame = frame;
  unknown_frame[1] = 0x0FU;
  const auto unknown = unknown_owner->Decode(0U, 0U, {unknown_frame.data(), unknown_frame.size()});
  Check(unknown.candidate && unknown.host.status == h::Status::OK &&
            !unknown.candidate->value.fields[1].enum_value.known &&
            unknown.candidate->presentation[1].enum_display_text.empty(),
        "unknown enum is not assigned known display text");
  owner.reset();
  Check(saved.presentation[1].enum_display_text == "Active" && saved.value.frame == frame,
        "associated DTO lifetime");
  Check(saved.copy_peak_bytes >= saved.accounted_total_bytes, "associated budget");
}
void Admission(const std::string& json, const std::string& root) {
  // Bind only the one-field alternate Message. The unselected nine-field Message must still count.
  const h::BindingSpec binding{"device", h::Action::DECODE, "alternate_pipeline", 1U};
  b::Limits limits;
  limits.max_fields = 1U;
  Reject([&] { (void)b::PreparedBinary::Create(Compile(json), &binding, 1U, {}, limits); });
  limits = {};
  limits.max_field_bytes = 1U;
  Reject([&] { (void)b::PreparedBinary::Create(Compile(json), &binding, 1U, {}, limits); });
  const h::BindingSpec duplicate[]{binding, binding};
  Reject([&] { (void)b::PreparedBinary::Create(Compile(json), duplicate, 2U); });
  const h::BindingSpec missing{"device", h::Action::DECODE, "missing", 1U};
  Reject([&] { (void)b::PreparedBinary::Create(Compile(json), &missing, 1U); });
  auto ascii = Compile(Read(root + "/examples/config/synthetic_ascii_stream_slice.pae.json"));
  Reject([&] { b::ValidateBinaryAdmission(*ascii.Plan()); });
  auto stream = Compile(Read(root + "/examples/config/synthetic_stream_framing_slice.pae.json"));
  b::ValidateBinaryAdmission(*stream.Plan());
  for (const auto& pipeline : stream.Plan()->Pipelines()) {
    auto new_artifacts =
        Compile(Read(root + "/examples/config/synthetic_stream_framing_slice.pae.json"));
    const h::BindingSpec spec{"device", h::Action::DECODE, pipeline.id.View(), 2U};
    auto prepared = b::PreparedBinary::Create(std::move(new_artifacts), &spec, 1U);
    Check(prepared->Observe(0U, 0U).stream, "stream admission");
    Check(prepared->Decode(0U, 0U, {}).host.status == h::Status::WRONG_INPUT_KIND,
          "no implicit stream driver");
  }
}
void Bounded(const std::string& root) {
  const auto json =
      Replace(Read(root + "/tests/protocol_lab_ui/fixtures/synthetic_ui_v08.pae.json"), "\"0.8\"",
              "\"0.9\"");
  const h::BindingSpec spec{"device", h::Action::DECODE, "synthetic_rx", 1U};
  auto owner = b::PreparedBinary::Create(Compile(json), &spec, 1U);
  for (std::size_t payload = 0U; payload <= 3U; ++payload) {
    std::vector<std::uint8_t> frame(payload + 3U, 1U);
    frame[0] = 0xA5U;
    frame[1] = static_cast<std::uint8_t>(frame.size());
    frame.back() = static_cast<std::uint8_t>(0xA5U + frame[1] + payload);
    const auto result = owner->Decode(0U, 0U, {frame.data(), frame.size()});
    Check(result.candidate && result.host.status == h::Status::OK &&
              result.candidate->presentation[1].byte_range->length == payload &&
              result.candidate->integrity_storage->offset == 2U + payload,
          "bounded association");
    frame.back() ^= 1U;
    const auto failure = owner->Decode(0U, 0U, {frame.data(), frame.size()});
    Check(failure.candidate && failure.host.status == h::Status::CODEC_FAILED &&
              failure.candidate->value.message_index == 0U &&
              failure.candidate->presentation.empty() && !failure.candidate->integrity_storage,
          "matched failure retains diagnostic but never success mapping");
  }
}
void Resources(const std::string& json) {
  auto original = Prepare(json);
  const auto report = original->Resources();
  Check(report.measured.plan_bytes > 0U && report.measured.session_bytes > 0U &&
            report.measured.description_bytes > 0U && report.measured.source_sidecar_bytes > 0U &&
            report.retained_dto_reserve == 4U * b::Limits{}.max_total_bytes,
        "resource itemization");
  const h::BindingSpec binding{"device", h::Action::DECODE, "ui_pipeline", 2U};
  auto replacement = b::PreparedBinary::Create(Compile(json), &binding, 1U, {7U, 9U, 10U}, {}, {},
                                               {}, original.get());
  Check(replacement->Resources().replacement_peak_bytes ==
            report.instance_admission_bytes + replacement->Resources().preparation_peak_bytes,
        "old and new coexistence");
  b::ResourceLimits exact;
  exact.instance_bytes = report.preparation_peak_bytes;
  exact.replacement_bytes = report.preparation_peak_bytes;
  (void)b::CalculateResourceReport(report.measured, exact);
  --exact.instance_bytes;
  Reject([&] { (void)b::CalculateResourceReport(report.measured, exact); });
  exact = {};
  exact.replacement_bytes = replacement->Resources().replacement_peak_bytes - 1U;
  Reject([&] {
    (void)b::PreparedBinary::Create(Compile(json), &binding, 1U, {7U, 9U, 10U}, {}, {}, exact,
                                    original.get());
  });
  Check(original->Observe(0U, 0U).status == h::Status::OK &&
            original->Observe(0U, 0U).generation == 0U,
        "rejected preparation preserves original");
  Reject([&] {
    (void)b::PreparedBinary::Create(Compile(json), &binding, 1U, {99U, 9U, 10U}, {}, {}, {},
                                    original.get());
  });
  auto overflow = report.measured;
  overflow.owner_bytes = std::numeric_limits<std::size_t>::max();
  Reject([&] { (void)b::CalculateResourceReport(overflow); });
  const h::BindingSpec many{"device", h::Action::DECODE, "ui_pipeline", 64U};
  Reject([&] { (void)b::PreparedBinary::Create(Compile(json), &many, 1U); });
  std::cout << "instance admission=" << report.instance_admission_bytes
            << " preparation peak=" << report.preparation_peak_bytes << '\n';
}
}  // namespace
int main(int argc, char** argv) {
  try {
    Check(argc == 2, "expected repository root");
    const std::string root = argv[1];
    const auto json =
        Replace(Read(root + "/tests/protocol_lab_ui/fixtures/synthetic_ui_v05.pae.json"), "\"0.5\"",
                "\"0.9\"");
    Complete(json);
    Admission(json, root);
    Bounded(root);
    Resources(json);
    std::cout << "Binary preparation and association passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
