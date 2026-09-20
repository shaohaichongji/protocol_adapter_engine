#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <new>

#include "../../tools/protocol_lab_ui/binary_host_adapter.h"
#include "../../tools/protocol_lab_ui/document_session.h"
#include "test_support.h"

namespace ui = pae::protocol_lab_ui;
namespace host = pae::host_endpoint;
namespace binary = pae::protocol_lab_binary;

int main() {
  std::ifstream input(std::filesystem::path{PAE_BINARY_UI_CONFIG}, std::ios::binary);
  const std::string json{std::istreambuf_iterator<char>{input}, {}};
  auto compiled = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(compiled.Succeeded());

  std::vector<ui::BinaryHostBinding> bindings{
      {"device", host::Action::DECODE, "ui_pipeline", 2U},
      {"other", host::Action::DECODE, "alternate_pipeline", 2U}};
  std::string error;
  auto adapter = ui::BinaryHostAdapter::Create(
      std::move(compiled).TakeArtifacts(), std::move(bindings),
      ui::BinaryPreparationIdentity{71U, 3U, 1U, 9U, "stage1-hash"}, nullptr, 0U, 0U, error);
  assert(adapter && error.empty());
  assert(adapter->Description().schema_version == "0.9");
  assert(adapter->Description().messages[0].fields[7].decimal_conversion);
  assert(adapter->UiDescriptionBytes() > 0U);

  const auto description_copy_upper = adapter->DescriptionCopyUpperBoundBytes();
  assert(description_copy_upper > 0U);
  assert(ui::BinaryHostAdapter::AccountDescriptionBytes(adapter->Description()) <=
         description_copy_upper);
  int description_copy_calls = 0;
  ui::BinaryUiCopyControls description_reject_controls;
  description_reject_controls.description_copy_limit = description_copy_upper - 1U;
  description_reject_controls.before_description_copy =
      +[](void* context) { ++*static_cast<int*>(context); };
  description_reject_controls.context = &description_copy_calls;
  auto description_reject_compile = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(description_reject_compile.Succeeded());
  std::string description_reject_error;
  auto description_reject = ui::BinaryHostAdapter::Create(
      std::move(description_reject_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U},
       {"other", host::Action::DECODE, "alternate_pipeline", 2U}},
      ui::BinaryPreparationIdentity{83U, 1U, 1U, 1U, "description-minus"}, nullptr, 0U, 0U,
      description_reject_error, {}, &description_reject_controls);
  assert(!description_reject && description_copy_calls == 0 &&
         description_reject_error == "Binary UI description copy preflight exceeded");
  ui::BinaryUiCopyControls description_exact_controls = description_reject_controls;
  description_exact_controls.description_copy_limit = description_copy_upper;
  auto description_exact_compile = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(description_exact_compile.Succeeded());
  std::string description_exact_error;
  auto description_exact = ui::BinaryHostAdapter::Create(
      std::move(description_exact_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U},
       {"other", host::Action::DECODE, "alternate_pipeline", 2U}},
      ui::BinaryPreparationIdentity{84U, 1U, 1U, 1U, "description-exact"}, nullptr, 0U, 0U,
      description_exact_error, {}, &description_exact_controls);
  assert(description_exact && description_copy_calls == 1 && description_exact_error.empty());
  assert(ui::BinaryHostAdapter::AccountDescriptionBytes(description_exact->Description()) <=
         description_copy_upper);

  const auto description_actual =
      ui::BinaryHostAdapter::AccountDescriptionBytes(adapter->Description());
  const auto exact_first_bytes = adapter->AccountedPreparationBytes() +
                                 adapter->DescriptionCopyUpperBoundBytes() - description_actual;
  binary::ResourceLimits exact_first_limits;
  exact_first_limits.instance_bytes = exact_first_bytes;
  auto exact_first_compiled = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(exact_first_compiled.Succeeded());
  std::string exact_first_error;
  auto exact_first =
      ui::BinaryHostAdapter::Create(std::move(exact_first_compiled).TakeArtifacts(),
                                    {{"device", host::Action::DECODE, "ui_pipeline", 2U},
                                     {"other", host::Action::DECODE, "alternate_pipeline", 2U}},
                                    ui::BinaryPreparationIdentity{81U, 1U, 1U, 1U, "exact-first"},
                                    nullptr, 0U, 0U, exact_first_error, exact_first_limits);
  assert(exact_first && exact_first_error.empty());
  auto minus_first_compiled = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(minus_first_compiled.Succeeded() && exact_first_bytes > 0U);
  exact_first_limits.instance_bytes = exact_first_bytes - 1U;
  std::string minus_first_error;
  auto minus_first =
      ui::BinaryHostAdapter::Create(std::move(minus_first_compiled).TakeArtifacts(),
                                    {{"device", host::Action::DECODE, "ui_pipeline", 2U},
                                     {"other", host::Action::DECODE, "alternate_pipeline", 2U}},
                                    ui::BinaryPreparationIdentity{82U, 1U, 1U, 1U, "minus-first"},
                                    nullptr, 0U, 0U, minus_first_error, exact_first_limits);
  assert(!minus_first && !minus_first_error.empty());

  const std::vector<std::uint8_t> frame{0x80U, 0x0DU, 3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU};
  const auto success = adapter->DecodeComplete(0U, 0U, frame);
  assert(success.ok && success.host.codec_attempted && success.host.observed_candidates == 1U);
  assert(success.result.has_value() && !success.failure.has_value());
  assert(success.result->fields.size() == 9U);
  assert(success.result->fields[0].raw_value == "未单独提供");
  assert(success.result->fields[0].logical_value == "true");
  assert(success.result->fields[1].raw_value == "2");
  assert(success.result->fields[1].logical_value == "Active [active]");
  assert(success.result->fields[6].raw_value == "CAFE");
  assert(success.result->fields[7].raw_value == "5");
  assert(success.result->fields[7].logical_value == "5@0");
  assert(adapter->Description().messages[0].fields[2].physical_bits.size() == 2U);

  const auto result_copy_upper = adapter->CurrentResultCopyUpperBoundBytes(0U, 0U);
  assert(result_copy_upper > 0U);
  assert(success.result->accounted_bytes <= result_copy_upper);
  int result_copy_calls = 0;
  ui::BinaryUiCopyControls result_reject_controls;
  result_reject_controls.result_copy_limit = result_copy_upper - 1U;
  result_reject_controls.before_result_copy = +[](void* context) { ++*static_cast<int*>(context); };
  result_reject_controls.context = &result_copy_calls;
  auto result_reject_compile = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(result_reject_compile.Succeeded());
  std::string result_reject_error;
  auto result_reject_adapter = ui::BinaryHostAdapter::Create(
      std::move(result_reject_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{85U, 1U, 1U, 1U, "result-minus"}, nullptr, 0U, 0U,
      result_reject_error, {}, &result_reject_controls);
  assert(result_reject_adapter);
  bool result_preflight_threw = false;
  try {
    (void)result_reject_adapter->DecodeComplete(0U, 0U, frame);
  } catch (const pae::protocol_lab_binary::MaterializationError&) {
    result_preflight_threw = true;
  }
  assert(result_preflight_threw && result_copy_calls == 0 &&
         result_reject_adapter->Current(0U, 0U) != nullptr);
  ui::BinaryUiCopyControls result_exact_controls = result_reject_controls;
  result_exact_controls.result_copy_limit = result_copy_upper;
  auto result_exact_compile = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(result_exact_compile.Succeeded());
  std::string result_exact_error;
  auto result_exact_adapter = ui::BinaryHostAdapter::Create(
      std::move(result_exact_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{86U, 1U, 1U, 1U, "result-exact"}, nullptr, 0U, 0U,
      result_exact_error, {}, &result_exact_controls);
  assert(result_exact_adapter);
  const auto result_exact = result_exact_adapter->DecodeComplete(0U, 0U, frame);
  assert(result_exact.ok && result_copy_calls == 1 && result_exact.result &&
         result_exact.result->accounted_bytes <= result_copy_upper);

  int combined_copy_calls = 0;
  ui::BinaryUiCopyControls combined_controls;
  combined_controls.before_result_copy = +[](void* context) { ++*static_cast<int*>(context); };
  combined_controls.context = &combined_copy_calls;
  auto combined_compile = pae::config_compiler::CompileJsonToPlanWithMetadata(
      json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(combined_compile.Succeeded());
  std::string combined_error;
  auto combined_adapter =
      ui::BinaryHostAdapter::Create(std::move(combined_compile).TakeArtifacts(),
                                    {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
                                    ui::BinaryPreparationIdentity{87U, 1U, 1U, 1U, "combined-view"},
                                    nullptr, 0U, 0U, combined_error, {}, &combined_controls);
  assert(combined_adapter && combined_error.empty());
  assert(combined_adapter->DecodeComplete(0U, 0U, frame).ok);
  assert(combined_adapter->DecodeComplete(0U, 1U, frame).ok);
  const auto active_view = combined_adapter->MapCurrent(0U, 0U);
  assert(active_view.result);
  const auto temporary_upper = combined_adapter->CurrentResultCopyUpperBoundBytes(0U, 1U);
  const auto mapped_view_budget = combined_adapter->UiViewReserveBytes() / 2U;
  assert(active_view.result->accounted_bytes <= temporary_upper &&
         temporary_upper <= mapped_view_budget);
  const auto exact_presentation = mapped_view_budget - temporary_upper;
  assert(combined_adapter->SetPresentationRetainedBytes(exact_presentation));
  combined_copy_calls = 0;
  const auto exact_combined =
      combined_adapter->MapCurrent(0U, 1U, active_view.result->accounted_bytes);
  assert(exact_combined.result && combined_copy_calls == 1 &&
         exact_combined.result->accounted_bytes <= temporary_upper);
  assert(combined_adapter->SetPresentationRetainedBytes(exact_presentation + 1U));
  combined_copy_calls = 0;
  bool combined_minus_threw = false;
  const auto selected_before_minus = combined_adapter->Selected();
  try {
    (void)combined_adapter->MapCurrent(0U, 1U, active_view.result->accounted_bytes);
  } catch (const pae::protocol_lab_binary::MaterializationError&) {
    combined_minus_threw = true;
  }
  assert(combined_minus_threw && combined_copy_calls == 0 &&
         combined_adapter->Current(0U, 0U) != nullptr &&
         combined_adapter->Current(0U, 1U) != nullptr &&
         combined_adapter->Selected().binding == selected_before_minus.binding &&
         combined_adapter->Selected().flow == selected_before_minus.flow);

  const std::vector<std::uint8_t> bad{0xC3U, 7U};
  const auto failure = adapter->DecodeComplete(0U, 0U, bad);
  assert(!failure.ok && !failure.result.has_value() && failure.failure.has_value());
  assert(!failure.failure->diagnostic_frame.empty());
  assert(adapter->Current(0U, 0U) != nullptr);

  auto compile = [&] {
    return pae::config_compiler::CompileJsonToPlanWithMetadata(
        json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                  pae::protocol_plan::ResourceProfile::DESKTOP));
  };
  ui::DocumentSession session{71U};
  const auto load = session.BeginLoad();
  auto initial = compile();
  assert(initial.Succeeded());
  auto completion = std::make_unique<ui::CompileCompletion>();
  completion->document_id = session.id();
  completion->load_revision = load;
  completion->config_sha256 = "stage1-hash";
  completion->artifacts = std::make_unique<pae::config_compiler::CompiledProtocolArtifacts>(
      std::move(initial).TakeArtifacts());
  assert(session.ApplyCompileCompletion(std::move(completion)));
  assert(session.IsBinaryHostDocument() && !session.BinaryHostActive());
  assert(!session.EncodeAvailable() && !session.InspectAvailable());

  auto retry_compile = compile();
  assert(retry_compile.Succeeded());
  std::string retry_error;
  auto rejected = ui::BinaryHostAdapter::Create(
      std::move(retry_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{71U, load, 1U, 10U, "stage1-hash"}, nullptr,
      (std::numeric_limits<std::size_t>::max)(), 0U, retry_error);
  assert(!rejected && !retry_error.empty());
  assert(!session.BinaryHostActive());

  auto session_compile = compile();
  assert(session_compile.Succeeded());
  std::string session_error;
  auto session_adapter = ui::BinaryHostAdapter::Create(
      std::move(session_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{71U, load, 1U, 11U, "stage1-hash"}, nullptr, 0U, 0U,
      session_error);
  auto publication = session.PrepareBinaryHostPublication(std::move(session_adapter), 11U);
  assert(publication);
  session.PublishBinaryHostPublication(std::move(*publication));
  assert(session.BinaryHostActive() && session.InspectAvailable());
  assert(session.SetInspectDraft("80 0D 03 00 01 00 CA FE 05 5A"));
  assert(session.Inspect());
  assert(session.inspect_result() && session.inspect_result()->fields.size() == 9U);
  assert(session.SetInspectDraft("C3 07"));
  assert(!session.Inspect());
  assert(!session.inspect_result() && session.inspect_failure() &&
         !session.inspect_failure()->input_frame.empty());
  assert(session.SetInspectDraft("80 0D 03 00 01 00 CA FE 05 5A"));
  assert(session.Inspect() && session.inspect_result());
  const auto* current_before_local_reject =
      session.prepared()->binary_host_adapter->Current(0U, 0U);
  assert(session.SetInspectDraft("not hex"));
  assert(!session.Inspect());
  assert(!session.inspect_result() && session.inspect_failure());
  assert(session.prepared()->binary_host_adapter->Current(0U, 0U) == current_before_local_reject);
  const auto failed_flow = session.PrepareBinaryHostFlow(0U, 1U, +[] { throw std::bad_alloc{}; });
  assert(!failed_flow && session.BinaryHostBindingIndex() == 0U &&
         session.BinaryHostFlowIndex() == 0U && session.inspect_draft() == "not hex" &&
         session.inspect_failure());
  auto flow_one = session.PrepareBinaryHostFlow(0U, 1U);
  assert(flow_one && session.PublishBinaryHostFlow(std::move(*flow_one)));
  assert(session.inspect_draft().empty());
  auto flow_zero = session.PrepareBinaryHostFlow(0U, 0U);
  assert(flow_zero && session.PublishBinaryHostFlow(std::move(*flow_zero)));
  assert(session.inspect_draft() == "not hex");
  assert(session.BinaryHasDiscardableState());

  auto replacement_probe_compile = compile();
  assert(replacement_probe_compile.Succeeded());
  std::string replacement_probe_error;
  constexpr std::size_t kReplacementRetainedUi = 101U;
  constexpr std::size_t kReplacementCoexisting = 37U;
  auto replacement_probe = ui::BinaryHostAdapter::Create(
      std::move(replacement_probe_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{71U, load, 2U, 20U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), kReplacementRetainedUi, kReplacementCoexisting,
      replacement_probe_error);
  assert(replacement_probe && replacement_probe_error.empty());
  const auto exact_replacement_bytes =
      session.prepared()->binary_host_adapter->AccountedInstanceBytes() +
      replacement_probe->AccountedPreparationBytes() + kReplacementCoexisting +
      replacement_probe->DescriptionCopyUpperBoundBytes() -
      ui::BinaryHostAdapter::AccountDescriptionBytes(replacement_probe->Description());
  binary::ResourceLimits exact_replacement_limits;
  exact_replacement_limits.replacement_bytes = exact_replacement_bytes;
  auto exact_replacement_compile = compile();
  assert(exact_replacement_compile.Succeeded());
  std::string exact_replacement_error;
  auto exact_replacement = ui::BinaryHostAdapter::Create(
      std::move(exact_replacement_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{71U, load, 2U, 21U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), kReplacementRetainedUi, kReplacementCoexisting,
      exact_replacement_error, exact_replacement_limits);
  assert(exact_replacement && exact_replacement_error.empty());
  auto minus_replacement_compile = compile();
  assert(minus_replacement_compile.Succeeded() && exact_replacement_bytes > 0U);
  exact_replacement_limits.replacement_bytes = exact_replacement_bytes - 1U;
  std::string minus_replacement_error;
  auto minus_replacement = ui::BinaryHostAdapter::Create(
      std::move(minus_replacement_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{71U, load, 2U, 22U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), kReplacementRetainedUi, kReplacementCoexisting,
      minus_replacement_error, exact_replacement_limits);
  assert(!minus_replacement && !minus_replacement_error.empty());

  auto throwing_compile = compile();
  assert(throwing_compile.Succeeded());
  std::string throwing_error;
  auto throwing_candidate = ui::BinaryHostAdapter::Create(
      std::move(throwing_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 2U}},
      ui::BinaryPreparationIdentity{71U, load, 2U, 23U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), 0U, 0U, throwing_error);
  assert(throwing_candidate);
  const auto* owner_before_throw = session.prepared()->binary_host_adapter.get();
  const auto selection_before_throw = *session.selection();
  const auto draft_before_throw = session.inspect_draft();
  const auto result_count_before_throw = session.inspect_result()->fields.size();
  const auto failed_publication = session.PrepareBinaryHostPublication(
      std::move(throwing_candidate), 23U, +[] { throw std::bad_alloc{}; });
  assert(!failed_publication);
  assert(session.prepared()->binary_host_adapter.get() == owner_before_throw);
  assert(session.selection()->pipeline_id == selection_before_throw.pipeline_id &&
         session.selection()->message_id == selection_before_throw.message_id);
  assert(session.inspect_draft() == draft_before_throw && session.inspect_result() &&
         session.inspect_result()->fields.size() == result_count_before_throw);

  auto stale_compile = compile();
  assert(stale_compile.Succeeded());
  std::string stale_error;
  auto stale = ui::BinaryHostAdapter::Create(
      std::move(stale_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 1U}},
      ui::BinaryPreparationIdentity{999U, load, 2U, 12U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), 0U, 0U, stale_error);
  assert(!stale && stale_error == "Binary UI replacement identity is stale or mismatched");
  assert(session.BinaryHostActive() && session.BinarySessionRevision() == 1U &&
         session.inspect_draft() == "not hex");
  const std::vector<ui::BinaryPreparationIdentity> stale_identities{
      {71U, load + 1U, 2U, 30U, "stage1-hash"},
      {71U, load, 3U, 30U, "stage1-hash"},
      {71U, load, 2U, 30U, "different-hash"}};
  for (const auto& stale_identity : stale_identities) {
    auto stale_dimension_compile = compile();
    assert(stale_dimension_compile.Succeeded());
    std::string stale_dimension_error;
    auto stale_dimension = ui::BinaryHostAdapter::Create(
        std::move(stale_dimension_compile).TakeArtifacts(),
        {{"device", host::Action::DECODE, "ui_pipeline", 1U}}, stale_identity,
        session.prepared()->binary_host_adapter.get(), 0U, 0U, stale_dimension_error);
    assert(!stale_dimension && !stale_dimension_error.empty());
  }
  auto stale_request_compile = compile();
  assert(stale_request_compile.Succeeded());
  std::string stale_request_error;
  auto stale_request = ui::BinaryHostAdapter::Create(
      std::move(stale_request_compile).TakeArtifacts(),
      {{"device", host::Action::DECODE, "ui_pipeline", 1U}},
      ui::BinaryPreparationIdentity{71U, load, 2U, 31U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), 0U, 0U, stale_request_error);
  assert(stale_request);
  assert(!session.PrepareBinaryHostPublication(std::move(stale_request), 32U));
  assert(session.prepared()->binary_host_adapter.get() == owner_before_throw);

  auto wrong_backend_compile = compile();
  assert(wrong_backend_compile.Succeeded());
  std::string wrong_backend_error;
  auto wrong_backend = ui::BinaryHostAdapter::Create(
      std::move(wrong_backend_compile).TakeArtifacts(),
      {{"device", host::Action::ENCODE, "ui_pipeline", 1U}},
      ui::BinaryPreparationIdentity{71U, load, 2U, 33U, "stage1-hash"},
      session.prepared()->binary_host_adapter.get(), 0U, 0U, wrong_backend_error);
  assert(wrong_backend);
  assert(!session.PrepareBinaryHostPublication(std::move(wrong_backend), 33U));
  assert(session.prepared()->binary_host_adapter.get() == owner_before_throw);

  std::ifstream stream_input(std::filesystem::path{PAE_BINARY_STREAM_CONFIG}, std::ios::binary);
  const std::string stream_json{std::istreambuf_iterator<char>{stream_input}, {}};
  auto stream_compiled = pae::config_compiler::CompileJsonToPlanWithMetadata(
      stream_json, pae::config_compiler::DerivedProtocolMetadataMemoryLimit(
                       pae::protocol_plan::ResourceProfile::DESKTOP));
  assert(stream_compiled.Succeeded());
  std::string stream_error;
  auto stream_adapter = ui::BinaryHostAdapter::Create(
      std::move(stream_compiled).TakeArtifacts(),
      {{"stream", host::Action::DECODE, "fixed_rx", 1U}},
      ui::BinaryPreparationIdentity{72U, 1U, 1U, 1U, "stream-hash"}, nullptr, 0U, 0U, stream_error);
  assert(stream_adapter && !stream_adapter->IsCompleteDecode(0U, 0U));
  const auto stream_rejected = stream_adapter->DecodeComplete(0U, 0U, {0xAAU, 1U, 2U});
  assert(!stream_rejected.ok && stream_rejected.host.status == host::Status::WRONG_INPUT_KIND);
  assert(stream_adapter->Current(0U, 0U) == nullptr);
  return 0;
}
