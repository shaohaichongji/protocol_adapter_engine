#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "config_compiler.h"
#include "ui_description_internal.h"
#include "validation_pipeline_internal.h"

namespace {

using pae::config_compiler::CompiledUiArtifacts;
using pae::config_compiler::CompileError;
using pae::config_compiler::CompileJsonToPlan;
using pae::config_compiler::CompileJsonToPlanWithUiDescription;
using pae::config_compiler::CompileStage;
using pae::config_compiler::CompileUiArtifactsResult;
using pae::config_compiler::DerivedUiDescriptionMemoryLimit;
using pae::config_compiler::DescriptionMemoryReport;
using pae::config_compiler::EstimateUiDescriptionLayoutForTest;
using pae::config_compiler::JsonParserPoolUpperBoundForTest;
using pae::config_compiler::kCompilerDecodedStringHardLimitBytes;
using pae::config_compiler::ResourceKind;
using pae::config_compiler::ScopedUiDescriptionTestProbe;
using pae::config_compiler::UiDescriptionLayoutTestInput;
using pae::config_compiler::UiDescriptionSidecar;
using pae::config_compiler::UiDescriptionTestProbe;
using pae::protocol_plan::GetResourceProfileLimits;
using pae::protocol_plan::ResourceProfile;

static_assert(!std::is_copy_constructible_v<UiDescriptionSidecar>);
static_assert(std::is_nothrow_move_constructible_v<UiDescriptionSidecar>);
static_assert(!std::is_copy_constructible_v<CompiledUiArtifacts>);
static_assert(std::is_nothrow_move_constructible_v<CompiledUiArtifacts>);
static_assert(!std::is_copy_constructible_v<CompileUiArtifactsResult>);

class Runner final {
 public:
  void Check(bool condition, std::string_view case_id, std::string_view detail) {
    if (condition) {
      ++passed_;
      std::cout << "PASS case=" << case_id << '\n';
    } else {
      ++failed_;
      std::cerr << "FAIL case=" << case_id << " detail=" << detail << '\n';
    }
  }

  int Finish() const {
    std::cout << "UI_DESCRIPTION_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " gate=" << (failed_ == 0U ? "PASS" : "FAIL") << '\n';
    return failed_ == 0U ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

bool ReadFile(const std::filesystem::path& path, std::string& output) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  output = buffer.str();
  return stream.good() || stream.eof();
}

bool ReportIsConsistent(const DescriptionMemoryReport& report) {
  return report.index_bytes == 0U && report.allocation_count == 1U &&
         report.object_bytes + report.string_bytes + report.alignment_bytes ==
             report.accounted_total_bytes;
}

bool ReportsEqual(const DescriptionMemoryReport& left, const DescriptionMemoryReport& right) {
  return left.object_bytes == right.object_bytes && left.string_bytes == right.string_bytes &&
         left.index_bytes == right.index_bytes && left.alignment_bytes == right.alignment_bytes &&
         left.allocation_count == right.allocation_count &&
         left.accounted_total_bytes == right.accounted_total_bytes;
}

void CheckSuccessfulArtifact(Runner& runner, std::string_view case_prefix, const std::string& json,
                             std::string_view schema_version) {
  const std::size_t limit = DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP);
  CompileUiArtifactsResult result = CompileJsonToPlanWithUiDescription(json, limit);
  const CompiledUiArtifacts* artifacts = result.Artifacts();
  const std::string compile_detail = result.Diagnostic() == nullptr
                                         ? "sidecar compile failed"
                                         : "sidecar compile failed: " + result.Diagnostic()->detail;
  runner.Check(result.Succeeded() && artifacts != nullptr, std::string(case_prefix) + "_compile",
               compile_detail);
  if (artifacts == nullptr || artifacts->Plan() == nullptr) {
    return;
  }
  const auto& plan = *artifacts->Plan();
  const auto& sidecar = artifacts->Description();
  runner.Check(plan.SchemaVersion() == schema_version, std::string(case_prefix) + "_schema",
               "schema version regressed");
  runner.Check(sidecar.Pipelines().size() == plan.Pipelines().size() &&
                   sidecar.Messages().size() == plan.Messages().size() &&
                   sidecar.Fields().size() == plan.GetResourceRequirements().total_field_count &&
                   sidecar.Enums().size() == plan.GetResourceRequirements().total_enum_entry_count,
               std::string(case_prefix) + "_counts", "Plan/sidecar counts differ");
  runner.Check(ReportIsConsistent(artifacts->DescriptionMemory()),
               std::string(case_prefix) + "_accounting", "memory categories do not sum");
  runner.Check(!sidecar.Resolve(sidecar.Protocol().display_name).empty() &&
                   !sidecar.Resolve(sidecar.Protocol().source_ref).empty(),
               std::string(case_prefix) + "_metadata", "protocol metadata was not retained");

  std::size_t string_bytes = sidecar.Protocol().display_name.size +
                             sidecar.Protocol().description.size +
                             sidecar.Protocol().source_ref.size;
  for (const auto& metadata : sidecar.Pipelines()) {
    string_bytes +=
        metadata.display_name.size + metadata.description.size + metadata.source_ref.size;
  }
  for (const auto& metadata : sidecar.Messages()) {
    string_bytes +=
        metadata.display_name.size + metadata.description.size + metadata.source_ref.size;
  }
  for (const auto& metadata : sidecar.Fields()) {
    string_bytes +=
        metadata.display_name.size + metadata.description.size + metadata.source_ref.size;
  }
  bool enum_metadata_present = true;
  for (const auto& metadata : sidecar.Enums()) {
    string_bytes += metadata.display_name.size;
    enum_metadata_present =
        enum_metadata_present && !sidecar.Resolve(metadata.display_name).empty();
  }
  DescriptionMemoryReport estimated;
  const bool estimated_ok = EstimateUiDescriptionLayoutForTest(
      UiDescriptionLayoutTestInput{sidecar.Pipelines().size(), sidecar.Messages().size(),
                                   sidecar.Fields().size(), sidecar.Enums().size(), string_bytes},
      estimated);
  runner.Check(estimated_ok && ReportsEqual(estimated, artifacts->DescriptionMemory()),
               std::string(case_prefix) + "_layout_exact",
               "production report differs from shared layout estimator");
  runner.Check(enum_metadata_present, std::string(case_prefix) + "_enum_metadata",
               "enum display metadata was not retained");

  std::size_t expected_field_begin = 0U;
  std::size_t expected_enum_begin = 0U;
  bool ranges_match = true;
  for (std::size_t message_index = 0U; message_index < sidecar.Messages().size(); ++message_index) {
    const auto& metadata = sidecar.Messages()[message_index];
    ranges_match = ranges_match && metadata.field_begin == expected_field_begin &&
                   metadata.field_count == plan.Messages()[message_index].fields.size();
    for (std::size_t field_offset = 0U; field_offset < metadata.field_count; ++field_offset) {
      const auto& field = sidecar.Fields()[metadata.field_begin + field_offset];
      ranges_match = ranges_match && field.enum_begin == expected_enum_begin &&
                     field.enum_count ==
                         plan.Messages()[message_index].fields[field_offset].enum_entries.size();
      expected_enum_begin += field.enum_count;
    }
    expected_field_begin += metadata.field_count;
  }
  ranges_match = ranges_match && expected_field_begin == sidecar.Fields().size() &&
                 expected_enum_begin == sidecar.Enums().size();
  runner.Check(ranges_match, std::string(case_prefix) + "_ranges",
               "embedded field/enum ranges differ from Plan order");
}

}  // namespace

int main(int argc, char** argv) {
  Runner runner;
  if (argc != 2) {
    runner.Check(false, "arguments", "expected copied test data directory");
    return runner.Finish();
  }
  const std::filesystem::path data_root{argv[1]};
  std::string minimal;
  runner.Check(ReadFile(data_root / "minimal_complete_record.pae.json", minimal), "load_minimal",
               "could not read minimal fixture");
  if (minimal.empty()) {
    return runner.Finish();
  }

  {
    UiDescriptionTestProbe probe;
    probe.fail_if_touched = true;
    ScopedUiDescriptionTestProbe scoped{probe};
    const auto valid = CompileJsonToPlan(minimal);
    const auto invalid = CompileJsonToPlan("{");
    runner.Check(valid.Succeeded() && !invalid.Succeeded() && probe.layout_count == 0U &&
                     probe.storage_allocation_count == 0U && probe.metadata_copy_count == 0U &&
                     probe.plan_freeze_count == 0U && probe.index_audit_count == 0U &&
                     probe.live_storage_count == 0U && probe.released_storage_count == 0U,
                 "plan_only_zero_sidecar_work", "legacy entry touched a sidecar-only stage");
  }

  UiDescriptionTestProbe allocation_failure_probe;
  allocation_failure_probe.fail_storage_allocation = true;
  allocation_failure_probe.track_storage_lifetime = true;
  {
    ScopedUiDescriptionTestProbe scoped{allocation_failure_probe};
    const auto result = CompileJsonToPlanWithUiDescription(
        minimal, DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP));
    runner.Check(!result.Succeeded() && result.Artifacts() == nullptr &&
                     result.Diagnostic() != nullptr &&
                     result.Diagnostic()->code == CompileError::COMPILER_ALLOCATION_FAILED,
                 "storage_allocation_failure_no_publish",
                 "injected sidecar allocation failure published artifacts or wrong diagnostic");
  }
  runner.Check(allocation_failure_probe.layout_count == 1U &&
                   allocation_failure_probe.storage_allocation_count == 1U &&
                   allocation_failure_probe.metadata_copy_count == 0U &&
                   allocation_failure_probe.plan_freeze_count == 0U &&
                   allocation_failure_probe.index_audit_count == 0U &&
                   allocation_failure_probe.live_storage_count == 0U &&
                   allocation_failure_probe.live_accounted_bytes == 0U &&
                   allocation_failure_probe.released_storage_count == 0U,
               "storage_allocation_failure_accounting",
               "allocation failure retained storage or crossed a later stage");

  UiDescriptionTestProbe success_probe;
  success_probe.track_storage_lifetime = true;
  {
    ScopedUiDescriptionTestProbe scoped{success_probe};
    const auto result = CompileJsonToPlanWithUiDescription(
        minimal, DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP));
    const std::string detail = result.Diagnostic() == nullptr
                                   ? "valid UI compilation failed"
                                   : "valid UI compilation failed: " + result.Diagnostic()->detail;
    runner.Check(result.Succeeded(), "probe_success", detail);
  }
  runner.Check(
      success_probe.layout_count == 1U && success_probe.storage_allocation_count == 1U &&
          success_probe.metadata_copy_count == 1U && success_probe.plan_freeze_count == 1U &&
          success_probe.index_audit_count == 1U && success_probe.live_storage_count == 0U &&
          success_probe.live_accounted_bytes == 0U && success_probe.released_storage_count == 1U &&
          success_probe.released_accounted_bytes > 0U,
      "probe_stages_once", "sidecar stages did not each execute exactly once");

  UiDescriptionTestProbe freeze_failure_probe;
  freeze_failure_probe.fail_plan_freeze = true;
  freeze_failure_probe.track_storage_lifetime = true;
  {
    ScopedUiDescriptionTestProbe scoped{freeze_failure_probe};
    const auto result = CompileJsonToPlanWithUiDescription(
        minimal, DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP));
    runner.Check(!result.Succeeded() && result.Artifacts() == nullptr &&
                     result.Diagnostic() != nullptr &&
                     result.Diagnostic()->code == CompileError::COMPILER_ALLOCATION_FAILED,
                 "plan_freeze_failure_no_partial_publish",
                 "injected Plan Freeze failure published Plan or sidecar");
  }
  runner.Check(
      freeze_failure_probe.layout_count == 1U &&
          freeze_failure_probe.storage_allocation_count == 1U &&
          freeze_failure_probe.metadata_copy_count == 1U &&
          freeze_failure_probe.plan_freeze_count == 1U &&
          freeze_failure_probe.index_audit_count == 0U &&
          freeze_failure_probe.live_storage_count == 0U &&
          freeze_failure_probe.live_accounted_bytes == 0U &&
          freeze_failure_probe.released_storage_count == 1U &&
          freeze_failure_probe.released_accounted_bytes == success_probe.released_accounted_bytes,
      "plan_freeze_failure_releases_sidecar",
      "Plan Freeze failure did not release accounted sidecar storage");

  UiDescriptionTestProbe audit_failure_probe;
  audit_failure_probe.fail_index_audit = true;
  audit_failure_probe.track_storage_lifetime = true;
  {
    ScopedUiDescriptionTestProbe scoped{audit_failure_probe};
    const auto result = CompileJsonToPlanWithUiDescription(
        minimal, DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP));
    runner.Check(!result.Succeeded() && result.Artifacts() == nullptr &&
                     result.Diagnostic() != nullptr &&
                     result.Diagnostic()->stage == CompileStage::INTERNAL,
                 "audit_failure_no_partial_publish", "audit failure published Plan or sidecar");
  }
  runner.Check(
      audit_failure_probe.layout_count == 1U &&
          audit_failure_probe.storage_allocation_count == 1U &&
          audit_failure_probe.metadata_copy_count == 1U &&
          audit_failure_probe.plan_freeze_count == 1U &&
          audit_failure_probe.index_audit_count == 1U &&
          audit_failure_probe.live_storage_count == 0U &&
          audit_failure_probe.live_accounted_bytes == 0U &&
          audit_failure_probe.released_storage_count == 1U &&
          audit_failure_probe.released_accounted_bytes == success_probe.released_accounted_bytes,
      "audit_failure_reached_final_gate",
      "audit failure injection did not reach the final publication gate");

  auto first = CompileJsonToPlanWithUiDescription(
      minimal, DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP));
  runner.Check(first.Succeeded(), "measure_actual", "initial sidecar compilation failed");
  if (first.Artifacts() != nullptr) {
    const auto& sidecar_memory = first.Artifacts()->DescriptionMemory();
    const std::size_t plan_storage =
        first.Artifacts()->Plan()->GetPlanMemoryReport().accounted_total_bytes;
    const std::size_t parser_pool = JsonParserPoolUpperBoundForTest(minimal.size());
    std::cout << "UI_DESCRIPTION_MEMORY_EVIDENCE sidecar_object_bytes="
              << sidecar_memory.object_bytes
              << " sidecar_string_bytes=" << sidecar_memory.string_bytes
              << " sidecar_index_bytes=" << sidecar_memory.index_bytes
              << " sidecar_alignment_bytes=" << sidecar_memory.alignment_bytes
              << " sidecar_accounted_total_bytes=" << sidecar_memory.accounted_total_bytes
              << " coexisting_metadata_copy_bytes=" << sidecar_memory.string_bytes
              << " ui_added_heap_peak_upper_bound_bytes=" << sidecar_memory.accounted_total_bytes
              << " parser_pool_upper_bound_bytes=" << parser_pool
              << " plan_accounted_total_bytes=" << plan_storage << '\n';
    const std::size_t exact = first.Artifacts()->DescriptionMemory().accounted_total_bytes;
    auto exact_result = CompileJsonToPlanWithUiDescription(minimal, exact);
    auto below_result = CompileJsonToPlanWithUiDescription(minimal, exact - 1U);
    const auto* below = below_result.Diagnostic();
    runner.Check(exact_result.Succeeded(), "limit_exact", "exact required limit failed");
    runner.Check(!below_result.Succeeded() && below != nullptr &&
                     below->stage == CompileStage::RESOURCE_BUDGET &&
                     below->code == CompileError::RESOURCE_LIMIT_EXCEEDED &&
                     below->resource_kind == ResourceKind::UI_DESCRIPTION_ACCOUNTED_MEMORY &&
                     below->required_bytes == exact && below->limit_bytes == exact - 1U &&
                     below_result.Artifacts() == nullptr,
                 "limit_below", "below-limit failure was not stable or published artifacts");
  }

  DescriptionMemoryReport overflow_report;
  runner.Check(
      !EstimateUiDescriptionLayoutForTest(
          UiDescriptionLayoutTestInput{0U, 0U, 0U, (std::numeric_limits<std::size_t>::max)(), 0U},
          overflow_report),
      "layout_overflow", "checked layout accepted overflowing descriptor count");
  const auto* desktop_limits = GetResourceProfileLimits(ResourceProfile::DESKTOP);
  const auto* constrained_limits = GetResourceProfileLimits(ResourceProfile::CONSTRAINED);
  DescriptionMemoryReport desktop_maximum;
  DescriptionMemoryReport constrained_maximum;
  const bool desktop_derived =
      desktop_limits != nullptr &&
      EstimateUiDescriptionLayoutForTest(
          UiDescriptionLayoutTestInput{desktop_limits->max_pipelines, desktop_limits->max_messages,
                                       desktop_limits->max_total_fields,
                                       desktop_limits->max_total_enum_entries,
                                       kCompilerDecodedStringHardLimitBytes},
          desktop_maximum);
  const bool constrained_derived =
      constrained_limits != nullptr &&
      EstimateUiDescriptionLayoutForTest(
          UiDescriptionLayoutTestInput{
              constrained_limits->max_pipelines, constrained_limits->max_messages,
              constrained_limits->max_total_fields, constrained_limits->max_total_enum_entries,
              kCompilerDecodedStringHardLimitBytes},
          constrained_maximum);
  runner.Check(
      desktop_derived && constrained_derived &&
          DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP) ==
              desktop_maximum.accounted_total_bytes &&
          DerivedUiDescriptionMemoryLimit(ResourceProfile::CONSTRAINED) ==
              constrained_maximum.accounted_total_bytes &&
          desktop_maximum.accounted_total_bytes > constrained_maximum.accounted_total_bytes,
      "derived_profile_limits", "profile-derived limits are not ordered and nonzero");
  std::cout << "UI_DESCRIPTION_DERIVED_LIMIT profile=desktop object_bytes="
            << desktop_maximum.object_bytes << " string_bytes=" << desktop_maximum.string_bytes
            << " index_bytes=" << desktop_maximum.index_bytes
            << " alignment_bytes=" << desktop_maximum.alignment_bytes
            << " total_bytes=" << desktop_maximum.accounted_total_bytes
            << " profile=constrained object_bytes=" << constrained_maximum.object_bytes
            << " string_bytes=" << constrained_maximum.string_bytes
            << " index_bytes=" << constrained_maximum.index_bytes
            << " alignment_bytes=" << constrained_maximum.alignment_bytes
            << " total_bytes=" << constrained_maximum.accounted_total_bytes << '\n';

  auto invalid_ui = CompileJsonToPlanWithUiDescription(
      "{", DerivedUiDescriptionMemoryLimit(ResourceProfile::DESKTOP));
  runner.Check(!invalid_ui.Succeeded() && invalid_ui.Artifacts() == nullptr,
               "invalid_no_partial_publish", "invalid input published partial artifacts");

  CheckSuccessfulArtifact(runner, "schema_0_1", minimal, "0.1");
  std::string enum_sample;
  runner.Check(ReadFile(data_root / "synthetic_lab_exchange_slice.pae.json", enum_sample),
               "load_enum_sample", "could not read enum sample");
  if (!enum_sample.empty()) {
    CheckSuccessfulArtifact(runner, "schema_0_1_enum", enum_sample, "0.1");
  }
  const std::vector<std::pair<const char*, const char*>> version_files{
#if defined(PAE_ENABLE_SCHEMA_V05_COMPILER)
      {"0.5", "synthetic_decimal_compile_slice.pae.json"},
#endif
#if defined(PAE_ENABLE_SCHEMA_V06_CRC_COMPILER)
      {"0.6", "synthetic_crc_slice.pae.json"},
#endif
#if defined(PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER)
      {"0.7", "synthetic_length_slice.pae.json"},
#endif
#if defined(PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER)
      {"0.8", "synthetic_bounded_variable_record.pae.json"},
#endif
  };
  for (const auto& [version, filename] : version_files) {
    std::string json;
    const std::string load_case = std::string{"load_schema_"} + version;
    runner.Check(ReadFile(data_root / filename, json), load_case, "could not read schema fixture");
    if (!json.empty()) {
      CheckSuccessfulArtifact(runner, std::string{"schema_"} + version, json, version);
    }
  }
  return runner.Finish();
}
