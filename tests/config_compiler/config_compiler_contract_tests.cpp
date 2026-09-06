#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "../../src/config_compiler/validation_pipeline_internal.h"
#include "../../src/protocol_plan/plan_builder.h"
#include "config_compiler.h"
#include "plan_builder_test_peer.h"

namespace {

using pae::config_compiler::BudgetedSchemaIr;
using pae::config_compiler::CompileDiagnostic;
using pae::config_compiler::CompileError;
using pae::config_compiler::CompileJsonToPlan;
using pae::config_compiler::CompileResult;
using pae::config_compiler::CompileStage;
using pae::config_compiler::DomainValidationResult;
using pae::config_compiler::DomainValidator;
using pae::config_compiler::EncodeIr;
using pae::config_compiler::FieldIr;
using pae::config_compiler::FramingProfileIr;
using pae::config_compiler::FreezeBudgetedPlanDraft;
using pae::config_compiler::MakeDeterministicPlanSnapshot;
using pae::config_compiler::MatcherClauseIr;
using pae::config_compiler::MatcherKind;
using pae::config_compiler::MessageIr;
using pae::config_compiler::PipelineIr;
using pae::config_compiler::PlanDraftAssembler;
using pae::config_compiler::PlanDraftAssemblyResult;
using pae::config_compiler::ResourceBudgetResult;
using pae::config_compiler::ResourceBudgetValidator;
using pae::config_compiler::ResourceKind;
using pae::config_compiler::SchemaIr;
using pae::config_compiler::ValidatedSchemaIr;
using pae::config_compiler::ValueType;
using pae::config_compiler::WireIr;
using pae::protocol_plan::BudgetedPlanDraft;
using pae::protocol_plan::ByteOrder;
using pae::protocol_plan::EncodeSource;
using pae::protocol_plan::InputKind;
using pae::protocol_plan::PlanBuilder;
using pae::protocol_plan::PlanBuildError;
using pae::protocol_plan::PlanBuildResult;
using pae::protocol_plan::PlanBundle;
using pae::protocol_plan::PlanMemoryReport;
using pae::protocol_plan::ResourceProfile;
using pae::protocol_plan::WireCodec;

static_assert(!std::is_copy_constructible_v<PlanBundle>);
static_assert(!std::is_copy_assignable_v<PlanBundle>);
static_assert(!std::is_move_constructible_v<PlanBundle>);
static_assert(!std::is_move_assignable_v<PlanBundle>);
static_assert(!std::is_default_constructible_v<ValidatedSchemaIr>);
static_assert(!std::is_copy_constructible_v<ValidatedSchemaIr>);
static_assert(std::is_move_constructible_v<ValidatedSchemaIr>);
static_assert(!std::is_default_constructible_v<BudgetedSchemaIr>);
static_assert(!std::is_copy_constructible_v<BudgetedSchemaIr>);
static_assert(std::is_move_constructible_v<BudgetedSchemaIr>);
static_assert(!std::is_default_constructible_v<BudgetedPlanDraft>);
static_assert(!std::is_copy_constructible_v<BudgetedPlanDraft>);
static_assert(std::is_move_constructible_v<BudgetedPlanDraft>);
static_assert(!std::is_default_constructible_v<DomainValidationResult>);
static_assert(!std::is_copy_constructible_v<DomainValidationResult>);
static_assert(!std::is_default_constructible_v<ResourceBudgetResult>);
static_assert(!std::is_copy_constructible_v<ResourceBudgetResult>);
static_assert(!std::is_default_constructible_v<PlanDraftAssemblyResult>);
static_assert(!std::is_copy_constructible_v<PlanDraftAssemblyResult>);
static_assert(!std::is_default_constructible_v<PlanBuildResult>);
static_assert(!std::is_copy_constructible_v<PlanBuildResult>);
static_assert(!std::is_default_constructible_v<CompileResult>);
static_assert(!std::is_copy_constructible_v<CompileResult>);

std::string_view ToString(CompileStage value) noexcept {
  switch (value) {
    case CompileStage::INPUT_PROFILE:
      return "INPUT_PROFILE";
    case CompileStage::JSON_SYNTAX:
      return "JSON_SYNTAX";
    case CompileStage::JSON_RESOURCE:
      return "JSON_RESOURCE";
    case CompileStage::STRUCTURAL:
      return "STRUCTURAL";
    case CompileStage::DOMAIN_VALIDATION:
      return "DOMAIN_VALIDATION";
    case CompileStage::RESOURCE_BUDGET:
      return "RESOURCE_BUDGET";
    case CompileStage::PLAN_BUILD:
      return "PLAN_BUILD";
    case CompileStage::INTERNAL:
      return "INTERNAL";
  }
  return "UNKNOWN_STAGE";
}

std::string_view ToString(CompileError value) noexcept {
  switch (value) {
    case CompileError::NONE:
      return "NONE";
    case CompileError::EMPTY_INPUT:
      return "EMPTY_INPUT";
    case CompileError::INPUT_LIMIT_EXCEEDED:
      return "INPUT_LIMIT_EXCEEDED";
    case CompileError::UTF8_BOM_NOT_ALLOWED:
      return "UTF8_BOM_NOT_ALLOWED";
    case CompileError::INVALID_UTF8:
      return "INVALID_UTF8";
    case CompileError::INVALID_UNICODE_ESCAPE:
      return "INVALID_UNICODE_ESCAPE";
    case CompileError::NESTING_DEPTH_LIMIT_EXCEEDED:
      return "NESTING_DEPTH_LIMIT_EXCEEDED";
    case CompileError::JSON_SYNTAX_ERROR:
      return "JSON_SYNTAX_ERROR";
    case CompileError::JSON_ALLOCATION_FAILED:
      return "JSON_ALLOCATION_FAILED";
    case CompileError::JSON_PARSER_MEMORY_LIMIT_EXCEEDED:
      return "JSON_PARSER_MEMORY_LIMIT_EXCEEDED";
    case CompileError::JSON_NODE_LIMIT_EXCEEDED:
      return "JSON_NODE_LIMIT_EXCEEDED";
    case CompileError::OBJECT_MEMBER_LIMIT_EXCEEDED:
      return "OBJECT_MEMBER_LIMIT_EXCEEDED";
    case CompileError::ARRAY_ELEMENT_LIMIT_EXCEEDED:
      return "ARRAY_ELEMENT_LIMIT_EXCEEDED";
    case CompileError::STRING_LIMIT_EXCEEDED:
      return "STRING_LIMIT_EXCEEDED";
    case CompileError::DECODED_STRING_BUDGET_EXCEEDED:
      return "DECODED_STRING_BUDGET_EXCEEDED";
    case CompileError::NUMBER_TOKEN_LIMIT_EXCEEDED:
      return "NUMBER_TOKEN_LIMIT_EXCEEDED";
    case CompileError::DUPLICATE_KEY:
      return "DUPLICATE_KEY";
    case CompileError::ROOT_MUST_BE_OBJECT:
      return "ROOT_MUST_BE_OBJECT";
    case CompileError::MISSING_PROPERTY:
      return "MISSING_PROPERTY";
    case CompileError::UNKNOWN_PROPERTY:
      return "UNKNOWN_PROPERTY";
    case CompileError::TYPE_MISMATCH:
      return "TYPE_MISMATCH";
    case CompileError::INVALID_STRING_LENGTH:
      return "INVALID_STRING_LENGTH";
    case CompileError::INVALID_ID:
      return "INVALID_ID";
    case CompileError::INVALID_ENUM_VALUE:
      return "INVALID_ENUM_VALUE";
    case CompileError::INTEGER_NOT_EXACT:
      return "INTEGER_NOT_EXACT";
    case CompileError::INTEGER_OUT_OF_RANGE:
      return "INTEGER_OUT_OF_RANGE";
    case CompileError::INVALID_HEX_BYTES:
      return "INVALID_HEX_BYTES";
    case CompileError::EMPTY_ARRAY:
      return "EMPTY_ARRAY";
    case CompileError::UNSUPPORTED_FEATURE:
      return "UNSUPPORTED_FEATURE";
    case CompileError::DUPLICATE_ID:
      return "DUPLICATE_ID";
    case CompileError::DUPLICATE_REFERENCE:
      return "DUPLICATE_REFERENCE";
    case CompileError::UNKNOWN_REFERENCE:
      return "UNKNOWN_REFERENCE";
    case CompileError::DIRECTION_MISMATCH:
      return "DIRECTION_MISMATCH";
    case CompileError::FIELD_OUT_OF_BOUNDS:
      return "FIELD_OUT_OF_BOUNDS";
    case CompileError::FIELD_OVERLAP:
      return "FIELD_OVERLAP";
    case CompileError::FRAME_NOT_FULLY_DEFINED:
      return "FRAME_NOT_FULLY_DEFINED";
    case CompileError::VALUE_NOT_REPRESENTABLE:
      return "VALUE_NOT_REPRESENTABLE";
    case CompileError::MATCHER_OUT_OF_BOUNDS:
      return "MATCHER_OUT_OF_BOUNDS";
    case CompileError::MATCHER_CONFLICT:
      return "MATCHER_CONFLICT";
    case CompileError::AMBIGUOUS_MATCHER:
      return "AMBIGUOUS_MATCHER";
    case CompileError::RESOURCE_LIMIT_EXCEEDED:
      return "RESOURCE_LIMIT_EXCEEDED";
    case CompileError::COMPILER_ALLOCATION_FAILED:
      return "COMPILER_ALLOCATION_FAILED";
    case CompileError::INTERNAL_CONTRACT_VIOLATION:
      return "INTERNAL_CONTRACT_VIOLATION";
  }
  return "UNKNOWN_ERROR";
}

class TestRunner final {
 public:
  static constexpr std::size_t kExpectedCaseCount = 33U;

  void Pass(std::string_view case_id) {
    ++passed_;
    std::cout << "PASS case=" << case_id << '\n';
  }

  void Fail(std::string_view case_id, std::string_view reason) {
    ++failed_;
    std::cerr << "FAIL case=" << case_id << " reason=" << reason << '\n';
  }

  int Finish() const {
    const bool gate_passed = failed_ == 0U && passed_ == kExpectedCaseCount;
    std::cout << "CONFIG_COMPILER_TEST_SUMMARY passed=" << passed_ << " failed=" << failed_
              << " expected=" << kExpectedCaseCount << " gate=" << (gate_passed ? "PASS" : "FAIL")
              << '\n';
    return gate_passed ? 0 : 1;
  }

 private:
  std::size_t passed_ = 0U;
  std::size_t failed_ = 0U;
};

SchemaIr MakeCapabilityContractSchema() {
  SchemaIr schema;
  schema.schema_version = "0.1";
  schema.protocol_id = "capability_contract";
  schema.protocol_version = "1";
  schema.display_name = "Capability contract";
  schema.source_ref = "SYNTHETIC_FROM_SCRATCH:capability_contract";
  schema.resource_profile = ResourceProfile::DESKTOP;

  FramingProfileIr framing;
  framing.id = "complete_record";
  framing.display_name = "Complete record";
  framing.source_ref = "SYNTHETIC_FROM_SCRATCH:capability_contract#framing";
  framing.input_kind = InputKind::COMPLETE_RECORD;
  framing.origin.json_pointer = "/framing_profiles/0";
  schema.framing_profiles.push_back(std::move(framing));

  PipelineIr pipeline;
  pipeline.id = "capability_pipeline";
  pipeline.display_name = "Capability pipeline";
  pipeline.source_ref = "SYNTHETIC_FROM_SCRATCH:capability_contract#pipeline";
  pipeline.direction_id = "capability_direction";
  pipeline.input_framing_profile_id = "complete_record";
  pipeline.message_ids.push_back("capability_message");
  pipeline.origin.json_pointer = "/pipelines/0";
  schema.pipelines.push_back(std::move(pipeline));

  MessageIr message;
  message.id = "capability_message";
  message.display_name = "Capability message";
  message.source_ref = "SYNTHETIC_FROM_SCRATCH:capability_contract#message";
  message.direction_id = "capability_direction";
  message.frame_length_bytes = 2U;
  message.origin.json_pointer = "/messages/0";

  MatcherClauseIr length_matcher;
  length_matcher.kind = MatcherKind::FRAME_LENGTH_EQUALS;
  length_matcher.length_bytes = 2U;
  length_matcher.origin.json_pointer = "/messages/0/matcher/all/0";
  message.matcher_clauses.push_back(std::move(length_matcher));

  MatcherClauseIr fixed_matcher;
  fixed_matcher.kind = MatcherKind::FIXED_BYTES;
  fixed_matcher.byte_offset = 0U;
  fixed_matcher.bytes.push_back(0xA5U);
  fixed_matcher.origin.json_pointer = "/messages/0/matcher/all/1";
  message.matcher_clauses.push_back(std::move(fixed_matcher));

  FieldIr prefix;
  prefix.id = "prefix";
  prefix.display_name = "Prefix";
  prefix.source_ref = "SYNTHETIC_FROM_SCRATCH:capability_contract#prefix";
  prefix.value_type = ValueType::UINT64;
  prefix.wire = WireIr{WireCodec::UNSIGNED_INTEGER,
                       0U,
                       1U,
                       ByteOrder::NOT_APPLICABLE,
                       {"/messages/0/fields/0/wire"}};
  prefix.encode = EncodeIr{EncodeSource::CONSTANT, 0xA5U, {"/messages/0/fields/0/encode"}};
  prefix.origin.json_pointer = "/messages/0/fields/0";
  message.fields.push_back(std::move(prefix));

  FieldIr value;
  value.id = "value";
  value.display_name = "Value";
  value.source_ref = "SYNTHETIC_FROM_SCRATCH:capability_contract#value";
  value.value_type = ValueType::UINT64;
  value.wire = WireIr{WireCodec::UNSIGNED_INTEGER,
                      1U,
                      1U,
                      ByteOrder::NOT_APPLICABLE,
                      {"/messages/0/fields/1/wire"}};
  value.encode = EncodeIr{EncodeSource::INPUT, std::nullopt, {"/messages/0/fields/1/encode"}};
  value.origin.json_pointer = "/messages/0/fields/1";
  message.fields.push_back(std::move(value));

  schema.messages.push_back(std::move(message));
  return schema;
}

std::unique_ptr<BudgetedPlanDraft> AssembleCapabilityDraft(
    std::optional<std::size_t> plan_memory_limit, CompileDiagnostic& diagnostic) {
  auto validated = DomainValidator::Validate(MakeCapabilityContractSchema());
  if (!validated.Succeeded()) {
    diagnostic = *validated.Diagnostic();
    return nullptr;
  }
  auto budgeted = plan_memory_limit.has_value()
                      ? ResourceBudgetValidator::ValidateForTest(
                            std::move(validated).TakeCapability(), *plan_memory_limit)
                      : ResourceBudgetValidator::Validate(std::move(validated).TakeCapability());
  if (!budgeted.Succeeded()) {
    diagnostic = *budgeted.Diagnostic();
    return nullptr;
  }
  auto assembled = PlanDraftAssembler::Assemble(std::move(budgeted).TakeCapability());
  if (!assembled.Succeeded()) {
    diagnostic = *assembled.Diagnostic();
    return nullptr;
  }
  return std::make_unique<BudgetedPlanDraft>(std::move(assembled).TakeCapability());
}

bool MemoryReportIsAccounted(const PlanMemoryReport& report) noexcept {
  const std::size_t categorized = report.object_bytes + report.string_bytes + report.matcher_bytes +
                                  report.metadata_container_bytes +
                                  report.execution_descriptor_bytes + report.index_bytes +
                                  report.extension_bytes + report.alignment_bytes;
  return report.accounted_total_bytes != 0U && report.accounted_total_bytes == categorized &&
         report.allocation_count != 0U && report.upstream_allocation_count == 1U;
}

void RunPlanMemoryContractCases(TestRunner& runner) {
  CompileDiagnostic diagnostic;
  auto first_draft = AssembleCapabilityDraft(std::nullopt, diagnostic);
  auto second_draft = AssembleCapabilityDraft(std::nullopt, diagnostic);
  if (first_draft == nullptr || second_draft == nullptr) {
    runner.Fail("plan_memory_report_accounting", "could not assemble a validated budgeted draft");
    runner.Fail("plan_memory_budget_exact_boundary", "could not determine exact plan memory");
    runner.Fail("plan_memory_failure_injection_no_leak", "could not determine allocation points");
    runner.Fail("plan_memory_estimate_mismatch", "could not assemble mismatch test draft");
    return;
  }
  PlanBuildResult first = PlanBuilder::Freeze(std::move(*first_draft));
  PlanBuildResult second = PlanBuilder::Freeze(std::move(*second_draft));
  if (!first.Succeeded() || !second.Succeeded()) {
    runner.Fail("plan_memory_report_accounting", "valid budgeted plans did not freeze");
    runner.Fail("plan_memory_budget_exact_boundary", "could not determine exact plan memory");
    runner.Fail("plan_memory_failure_injection_no_leak", "could not determine allocation points");
    runner.Fail("plan_memory_estimate_mismatch", "could not assemble mismatch test draft");
    return;
  }
  const PlanMemoryReport first_report = first.Plan()->GetPlanMemoryReport();
  const PlanMemoryReport second_report = second.Plan()->GetPlanMemoryReport();
  if (MemoryReportIsAccounted(first_report) &&
      pae::protocol_plan::PlanMemoryReportsEqual(first_report, second_report)) {
    runner.Pass("plan_memory_report_accounting");
  } else {
    runner.Fail("plan_memory_report_accounting",
                "category sum, allocation count, or deterministic report contract failed");
  }

  auto exact_validated = DomainValidator::Validate(MakeCapabilityContractSchema());
  auto exact_budgeted = ResourceBudgetValidator::ValidateForTest(
      std::move(exact_validated).TakeCapability(), first_report.accounted_total_bytes);
  auto rejected_validated = DomainValidator::Validate(MakeCapabilityContractSchema());
  auto rejected_budgeted = ResourceBudgetValidator::ValidateForTest(
      std::move(rejected_validated).TakeCapability(), first_report.accounted_total_bytes - 1U);
  const CompileDiagnostic* rejected = rejected_budgeted.Diagnostic();
  if (exact_budgeted.Succeeded() && !rejected_budgeted.Succeeded() && rejected != nullptr &&
      rejected->stage == CompileStage::RESOURCE_BUDGET &&
      rejected->code == CompileError::RESOURCE_LIMIT_EXCEEDED &&
      rejected->resource_kind == ResourceKind::PLAN_ACCOUNTED_MEMORY &&
      rejected->required_bytes == first_report.accounted_total_bytes &&
      rejected->limit_bytes == first_report.accounted_total_bytes - 1U &&
      rejected->resource_profile == ResourceProfile::DESKTOP) {
    runner.Pass("plan_memory_budget_exact_boundary");
  } else {
    runner.Fail("plan_memory_budget_exact_boundary",
                "exact limit was rejected or limit-1 lacked a typed memory diagnostic");
  }

  bool failures_clean = true;
  for (std::size_t ordinal = 1U; ordinal <= first_report.allocation_count + 1U; ++ordinal) {
    CompileDiagnostic loop_diagnostic;
    auto draft = AssembleCapabilityDraft(std::nullopt, loop_diagnostic);
    pae::protocol_plan::test_only::PlanMemoryTestProbe probe;
    if (draft == nullptr) {
      failures_clean = false;
      break;
    }
    PlanBuildResult injected = PlanBuilder::Freeze(
        pae::test_support::ConfigurePlanMemoryFailure(std::move(*draft), ordinal, &probe));
    if (injected.Succeeded() || injected.Diagnostic() == nullptr ||
        injected.Diagnostic()->code != PlanBuildError::ALLOCATION_FAILED ||
        probe.LiveUpstreamBytes() != 0U || probe.LiveUpstreamAllocations() != 0U) {
      failures_clean = false;
      break;
    }
  }
  pae::protocol_plan::test_only::PlanMemoryTestProbe success_probe;
  bool success_lifetime = false;
  {
    CompileDiagnostic success_diagnostic;
    auto draft = AssembleCapabilityDraft(std::nullopt, success_diagnostic);
    if (draft != nullptr) {
      PlanBuildResult succeeded = PlanBuilder::Freeze(
          pae::test_support::ConfigurePlanMemoryFailure(std::move(*draft), 0U, &success_probe));
      success_lifetime = succeeded.Succeeded() &&
                         success_probe.LiveUpstreamBytes() == first_report.accounted_total_bytes &&
                         success_probe.LiveUpstreamAllocations() == 1U;
    }
  }
  success_lifetime = success_lifetime && success_probe.LiveUpstreamBytes() == 0U &&
                     success_probe.LiveUpstreamAllocations() == 0U;
  if (failures_clean && success_lifetime) {
    runner.Pass("plan_memory_failure_injection_no_leak");
  } else {
    runner.Fail("plan_memory_failure_injection_no_leak",
                "an injected allocation point leaked or the success owner lifetime was incorrect");
  }

  CompileDiagnostic mismatch_diagnostic;
  auto mismatch_draft = AssembleCapabilityDraft(std::nullopt, mismatch_diagnostic);
  if (mismatch_draft != nullptr) {
    PlanBuildResult mismatch = PlanBuilder::Freeze(
        pae::test_support::CorruptApprovedPlanMemory(std::move(*mismatch_draft)));
    if (!mismatch.Succeeded() && mismatch.Diagnostic() != nullptr &&
        mismatch.Diagnostic()->code == PlanBuildError::PLAN_MEMORY_ESTIMATE_MISMATCH) {
      runner.Pass("plan_memory_estimate_mismatch");
      return;
    }
  }
  runner.Fail("plan_memory_estimate_mismatch",
              "PlanBuilder accepted a report that differed from the approved budget report");
}

void RunCapabilityStateCase(TestRunner& runner) {
  constexpr std::string_view kCaseId = "capability_single_consumption_internal_violation";
  auto validated = DomainValidator::Validate(MakeCapabilityContractSchema());
  if (!validated.Succeeded()) {
    runner.Fail(kCaseId, "DomainValidator rejected the synthetic capability contract schema");
    return;
  }
  ValidatedSchemaIr validated_capability = std::move(validated).TakeCapability();
  auto budgeted = ResourceBudgetValidator::Validate(std::move(validated_capability));
  auto reused_validated = ResourceBudgetValidator::Validate(std::move(validated_capability));
  if (!budgeted.Succeeded()) {
    runner.Fail(kCaseId, "ResourceBudgetValidator rejected a validated synthetic schema");
    return;
  }
  if (reused_validated.Succeeded() || reused_validated.Diagnostic() == nullptr ||
      reused_validated.Diagnostic()->code != CompileError::INTERNAL_CONTRACT_VIOLATION) {
    runner.Fail(kCaseId, "moved-from validated capability was accepted twice");
    return;
  }

  BudgetedSchemaIr budgeted_capability = std::move(budgeted).TakeCapability();
  auto assembled = PlanDraftAssembler::Assemble(std::move(budgeted_capability));
  auto reused_budgeted = PlanDraftAssembler::Assemble(std::move(budgeted_capability));
  if (!assembled.Succeeded()) {
    runner.Fail(kCaseId, "PlanDraftAssembler rejected a budgeted synthetic schema");
    return;
  }
  if (reused_budgeted.Succeeded() || reused_budgeted.Diagnostic() == nullptr ||
      reused_budgeted.Diagnostic()->code != CompileError::INTERNAL_CONTRACT_VIOLATION) {
    runner.Fail(kCaseId, "moved-from budgeted capability was accepted twice");
    return;
  }

  BudgetedPlanDraft original = std::move(assembled).TakeCapability();
  BudgetedPlanDraft consumable = std::move(original);
  const auto moved_from_result = PlanBuilder::Freeze(std::move(original));
  const auto valid_result = PlanBuilder::Freeze(std::move(consumable));
  const bool passed = !moved_from_result.Succeeded() && moved_from_result.Plan() == nullptr &&
                      moved_from_result.Diagnostic() != nullptr &&
                      moved_from_result.Diagnostic()->code == PlanBuildError::INTERNAL_ERROR &&
                      valid_result.Succeeded();
  if (!passed) {
    runner.Fail(kCaseId,
                "moved-from capability was not rejected as an internal contract violation or "
                "the single consumable capability failed");
    return;
  }
  runner.Pass(kCaseId);
}

void RunCorruptedBudgetedDraftCase(TestRunner& runner) {
  constexpr std::string_view kCaseId = "corrupted_budgeted_draft_internal_violation";
  CompileResult result =
      FreezeBudgetedPlanDraft(pae::test_support::MakeCorruptedBudgetedPlanDraft());
  const bool passed = !result.Succeeded() && result.Plan() == nullptr &&
                      result.Diagnostic() != nullptr &&
                      result.Diagnostic()->stage == CompileStage::PLAN_BUILD &&
                      result.Diagnostic()->code == CompileError::INTERNAL_CONTRACT_VIOLATION;
  if (!passed) {
    runner.Fail(kCaseId, "test-only corrupted capability escaped the internal error boundary");
    return;
  }
  runner.Pass(kCaseId);
}

void RunPlanBuilderBitfieldDefenseCases(TestRunner& runner) {
  const auto reject_invalid_field = [&runner](std::string_view case_id, BudgetedPlanDraft draft) {
    const auto result = PlanBuilder::Freeze(std::move(draft));
    if (result.Succeeded() || result.Diagnostic() == nullptr ||
        result.Diagnostic()->code != PlanBuildError::INVALID_FIELD_PLAN ||
        result.Diagnostic()->message_index != 0U) {
      runner.Fail(case_id, "PlanBuilder accepted a corrupted bit-container invariant");
      return;
    }
    runner.Pass(case_id);
  };
  reject_invalid_field("bitfield_builder_single_byte_order_defense",
                       pae::test_support::MakeBitfieldDraftWithInvalidByteOrder());
  reject_invalid_field("bitfield_builder_numbering_enum_defense",
                       pae::test_support::MakeBitfieldDraftWithInvalidBitNumbering());
  const auto matcher_result =
      PlanBuilder::Freeze(pae::test_support::MakeBitfieldDraftWithMatcherConflict());
  if (matcher_result.Succeeded() || matcher_result.Diagnostic() == nullptr ||
      matcher_result.Diagnostic()->code != PlanBuildError::INVALID_MATCHER_PLAN ||
      matcher_result.Diagnostic()->message_index != 0U) {
    runner.Fail("bitfield_builder_matcher_conflict_defense",
                "PlanBuilder accepted determined bit container bytes that conflict with Matcher");
  } else {
    runner.Pass("bitfield_builder_matcher_conflict_defense");
  }
  const auto reject_resource_limit = [&runner](std::string_view case_id, BudgetedPlanDraft draft) {
    const auto result = PlanBuilder::Freeze(std::move(draft));
    if (result.Succeeded() || result.Diagnostic() == nullptr ||
        result.Diagnostic()->code != PlanBuildError::RESOURCE_LIMIT_EXCEEDED) {
      runner.Fail(case_id, "PlanBuilder accepted a corrupted bit-container resource count");
      return;
    }
    runner.Pass(case_id);
  };
  reject_resource_limit("bitfield_builder_message_container_limit_defense",
                        pae::test_support::MakeBitfieldDraftWithTooManyMessageContainers());
  reject_resource_limit("bitfield_builder_total_container_limit_defense",
                        pae::test_support::MakeBitfieldDraftWithTooManyTotalContainers());
}

bool ReadBinaryFile(const std::filesystem::path& path, std::string& output, std::string& error) {
  std::ifstream stream{path, std::ios::binary};
  if (!stream) {
    error = "cannot open test data file: " + path.generic_string();
    return false;
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  if (!stream.good() && !stream.eof()) {
    error = "cannot read test data file: " + path.generic_string();
    return false;
  }
  output = buffer.str();
  return true;
}

bool ReplaceOnce(std::string& value, std::string_view needle, std::string_view replacement) {
  const std::size_t offset = value.find(needle);
  if (offset == std::string::npos) {
    return false;
  }
  value.replace(offset, needle.size(), replacement);
  return true;
}

std::string RepeatUtf8(std::string_view token, std::size_t count) {
  std::string output;
  output.reserve(token.size() * count);
  for (std::size_t index = 0U; index < count; ++index) {
    output.append(token);
  }
  return output;
}

std::optional<std::size_t> FindJsonObjectEnd(std::string_view json, std::size_t object_begin) {
  if (object_begin >= json.size() || json[object_begin] != '{') {
    return std::nullopt;
  }
  std::size_t depth = 0U;
  bool in_string = false;
  bool escaped = false;
  for (std::size_t index = object_begin; index < json.size(); ++index) {
    const char character = json[index];
    if (in_string) {
      if (escaped) {
        escaped = false;
      } else if (character == '\\') {
        escaped = true;
      } else if (character == '"') {
        in_string = false;
      }
      continue;
    }
    if (character == '"') {
      in_string = true;
    } else if (character == '{') {
      ++depth;
    } else if (character == '}') {
      --depth;
      if (depth == 0U) {
        return index + 1U;
      }
    }
  }
  return std::nullopt;
}

bool MakeAmbiguousMatcherInput(std::string& json, std::string& error) {
  if (!ReplaceOnce(json, "\"message_ids\": [\"minimal_message\"]",
                   "\"message_ids\": [\"minimal_message\", \"minimal_message_b\"]")) {
    error = "pipeline message_ids mutation token was not found";
    return false;
  }
  const std::size_t messages_key = json.find("\"messages\": [");
  const std::size_t object_begin =
      messages_key == std::string::npos ? std::string::npos : json.find('{', messages_key);
  if (object_begin == std::string::npos) {
    error = "message object start was not found";
    return false;
  }
  const std::optional<std::size_t> object_end = FindJsonObjectEnd(json, object_begin);
  if (!object_end.has_value()) {
    error = "message object end was not found";
    return false;
  }
  std::string duplicate = json.substr(object_begin, *object_end - object_begin);
  if (!ReplaceOnce(duplicate, "\"id\": \"minimal_message\"", "\"id\": \"minimal_message_b\"")) {
    error = "duplicate message ID mutation token was not found";
    return false;
  }
  json.insert(*object_end, ",\n    " + duplicate);
  return true;
}

struct ExpectedFailure {
  CompileStage stage = CompileStage::INTERNAL;
  CompileError code = CompileError::INTERNAL_CONTRACT_VIOLATION;
  std::optional<std::string> json_pointer;
  std::optional<std::string> detail_substring;
};

void RunFailureCase(TestRunner& runner, std::string_view case_id, std::string_view input,
                    const ExpectedFailure& expected) {
  const CompileResult result = CompileJsonToPlan(input);
  if (result.Succeeded() || result.Plan() != nullptr || result.Diagnostic() == nullptr) {
    runner.Fail(case_id, "expected fail-closed diagnostic with no partial PlanBundle");
    return;
  }

  const CompileDiagnostic& actual = *result.Diagnostic();
  std::ostringstream mismatch;
  bool matched = true;
  if (actual.stage != expected.stage) {
    matched = false;
    mismatch << "expected_stage=" << ToString(expected.stage)
             << " actual_stage=" << ToString(actual.stage) << ' ';
  }
  if (actual.code != expected.code) {
    matched = false;
    mismatch << "expected_code=" << ToString(expected.code)
             << " actual_code=" << ToString(actual.code) << ' ';
  }
  if (expected.json_pointer.has_value() && actual.json_pointer != *expected.json_pointer) {
    matched = false;
    mismatch << "expected_pointer=" << *expected.json_pointer
             << " actual_pointer=" << actual.json_pointer << ' ';
  }
  if (expected.detail_substring.has_value() &&
      actual.detail.find(*expected.detail_substring) == std::string::npos) {
    matched = false;
    mismatch << "expected_detail_substring=" << *expected.detail_substring
             << " actual_detail=" << actual.detail << ' ';
  }

  if (!matched) {
    runner.Fail(case_id, mismatch.str());
    return;
  }
  runner.Pass(case_id);
}

bool ExpectSnapshotFragment(std::string_view snapshot, std::string_view fragment,
                            std::ostringstream& error) {
  if (snapshot.find(fragment) != std::string_view::npos) {
    return true;
  }
  error << "missing_snapshot_fragment=" << fragment << ' ';
  return false;
}

void RunSyntheticLabPlanCase(TestRunner& runner, const std::filesystem::path& input_path,
                             const std::filesystem::path& expected_snapshot_path) {
  constexpr std::string_view kCaseId = "valid_synthetic_lab_plan_snapshot";
  std::string input;
  std::string file_error;
  if (!ReadBinaryFile(input_path, input, file_error)) {
    runner.Fail(kCaseId, file_error);
    return;
  }

  CompileResult first = CompileJsonToPlan(input);
  CompileResult second = CompileJsonToPlan(input);
  if (!first.Succeeded() || !second.Succeeded()) {
    std::ostringstream error;
    error << "legal sample did not compile";
    if (first.Diagnostic() != nullptr) {
      error << " first_stage=" << ToString(first.Diagnostic()->stage)
            << " first_code=" << ToString(first.Diagnostic()->code)
            << " first_pointer=" << first.Diagnostic()->json_pointer;
    }
    if (second.Diagnostic() != nullptr) {
      error << " second_stage=" << ToString(second.Diagnostic()->stage)
            << " second_code=" << ToString(second.Diagnostic()->code)
            << " second_pointer=" << second.Diagnostic()->json_pointer;
    }
    runner.Fail(kCaseId, error.str());
    return;
  }

  const auto& first_plan = *first.Plan();
  const auto& requirements = first_plan.GetResourceRequirements();
  const std::string first_snapshot = MakeDeterministicPlanSnapshot(first_plan);
  const std::string second_snapshot = MakeDeterministicPlanSnapshot(*second.Plan());
  std::string expected_snapshot;
  if (!ReadBinaryFile(expected_snapshot_path, expected_snapshot, file_error)) {
    runner.Fail(kCaseId, file_error);
    return;
  }
  while (!expected_snapshot.empty() &&
         (expected_snapshot.back() == '\n' || expected_snapshot.back() == '\r')) {
    expected_snapshot.pop_back();
  }

  std::ostringstream error;
  bool valid = true;
  const auto Check = [&](bool condition, std::string_view label) {
    if (!condition) {
      valid = false;
      error << "failed_expectation=" << label << ' ';
    }
  };

  Check(first_plan.SchemaVersion() == "0.1", "schema_version");
  Check(first_plan.ProtocolId() == "synthetic_lab_exchange", "protocol_id");
  Check(first_plan.ProtocolVersion() == "0.1-synthetic", "protocol_version");
  Check(first_plan.FramingProfiles().size() == 1U, "framing_count");
  Check(first_plan.Pipelines().size() == 2U, "pipeline_count");
  Check(first_plan.Messages().size() == 2U, "message_count");
  Check(requirements.max_frame_bytes == 13U, "max_frame_bytes");
  Check(requirements.total_field_count == 10U, "total_field_count");
  Check(requirements.total_matcher_count == 4U, "total_matcher_count");
  Check(requirements.total_enum_entry_count == 4U, "total_enum_entry_count");
  const auto& framing_profiles = first_plan.FramingProfiles();
  const auto& pipelines = first_plan.Pipelines();
  const auto& messages = first_plan.Messages();
  const bool request_framing_reference_valid =
      pipelines[0].framing_profile_index < framing_profiles.size() &&
      framing_profiles[pipelines[0].framing_profile_index].id == "complete_record";
  Check(request_framing_reference_valid, "request_framing_reference");
  const bool request_message_reference_valid =
      pipelines[0].message_indices.size() == 1U &&
      pipelines[0].message_indices[0] < messages.size() &&
      messages[pipelines[0].message_indices[0]].id == "lab_command";
  Check(request_message_reference_valid, "request_message_reference");
  const bool response_message_reference_valid =
      pipelines[1].message_indices.size() == 1U &&
      pipelines[1].message_indices[0] < messages.size() &&
      messages[pipelines[1].message_indices[0]].id == "lab_report";
  Check(response_message_reference_valid, "response_message_reference");
  Check(first_plan.Messages()[0].id == "lab_command", "request_message_id");
  Check(first_plan.Messages()[0].frame_length_bytes == 13U, "request_frame_length");
  Check(first_plan.Messages()[0].fields.size() == 5U, "request_field_count");
  Check(first_plan.Messages()[0].fields[3].value_type == ValueType::BYTES, "request_bytes_field");
  Check(first_plan.Messages()[0].fields[4].value_type == ValueType::ENUM, "request_enum_field");
  Check(first_plan.Messages()[1].id == "lab_report", "response_message_id");
  Check(first_plan.Messages()[1].frame_length_bytes == 11U, "response_frame_length");
  Check(first_plan.Messages()[1].fields.size() == 5U, "response_field_count");
  Check(first_plan.Messages()[0].matchers[0].kind == MatcherKind::FRAME_LENGTH_EQUALS,
        "request_length_matcher");
  Check(first_plan.Messages()[0].matchers[1].kind == MatcherKind::FIXED_BYTES,
        "request_fixed_bytes_matcher");

  const auto& execution_layout = first_plan.GetExecutionResourceLayout();
  const auto& message_execution_plans = first_plan.MessageExecutionPlans();
  const auto& pipeline_execution_plans = first_plan.PipelineExecutionPlans();
  Check(execution_layout.max_fields_per_message == 5U, "execution_max_fields");
  Check(execution_layout.max_input_fields_per_message == 4U, "execution_max_inputs");
  Check(message_execution_plans.size() == 2U, "message_execution_count");
  Check(pipeline_execution_plans.size() == 2U, "pipeline_execution_count");
  if (message_execution_plans.size() == 2U) {
    const auto& request_execution = message_execution_plans[0];
    Check(request_execution.frame_size == 13U, "request_execution_frame_size");
    Check(request_execution.required_input_count == 4U, "request_execution_input_count");
    Check(request_execution.fixed_bytes.size() == 2U &&
              request_execution.fixed_bytes[0].offset == 2U &&
              request_execution.fixed_bytes[0].value == 0xA7U &&
              request_execution.fixed_bytes[1].offset == 3U &&
              request_execution.fixed_bytes[1].value == 0x31U,
          "request_fixed_bytes_normalized");
    Check(request_execution.fields.size() == 5U, "request_execution_field_count");
    if (request_execution.fields.size() == 5U) {
      Check(request_execution.fields[0].input_ordinal == 0U, "request_first_input_ordinal");
      Check(request_execution.fields[1].input_ordinal == (std::numeric_limits<std::size_t>::max)(),
            "request_constant_input_ordinal");
      Check(request_execution.fields[4].input_ordinal == 3U, "request_last_input_ordinal");
      Check(request_execution.fields[4].enum_values_count == 4U &&
                request_execution.fields[4].enum_lookup_count == 4U,
            "request_enum_execution_spans");
    }
    Check(request_execution.enum_raw_values.size() == 4U &&
              request_execution.enum_lookup_entries.size() == 4U &&
              request_execution.enum_lookup_entries[0].raw_value == 1U &&
              request_execution.enum_lookup_entries[0].entry_index == 0U &&
              request_execution.enum_lookup_entries[3].raw_value == 4U &&
              request_execution.enum_lookup_entries[3].entry_index == 3U,
          "request_enum_lookup_sorted_and_stable");
  }
  if (pipeline_execution_plans.size() == 2U) {
    const auto& request_pipeline_execution = pipeline_execution_plans[0];
    const auto& response_pipeline_execution = pipeline_execution_plans[1];
    Check(request_pipeline_execution.allowed_message_words.size() == 1U &&
              request_pipeline_execution.allowed_message_words[0] == 1U,
          "request_allowed_message_bitmap");
    Check(response_pipeline_execution.allowed_message_words.size() == 1U &&
              response_pipeline_execution.allowed_message_words[0] == 2U,
          "response_allowed_message_bitmap");
    Check(request_pipeline_execution.candidate_groups.size() == 1U &&
              request_pipeline_execution.candidate_groups[0].frame_size == 13U &&
              request_pipeline_execution.candidate_groups[0].message_indices.size() == 1U &&
              request_pipeline_execution.candidate_groups[0].message_indices[0] == 0U,
          "request_candidate_group");
    Check(response_pipeline_execution.candidate_groups.size() == 1U &&
              response_pipeline_execution.candidate_groups[0].frame_size == 11U &&
              response_pipeline_execution.candidate_groups[0].message_indices.size() == 1U &&
              response_pipeline_execution.candidate_groups[0].message_indices[0] == 1U,
          "response_candidate_group");
  }

  valid =
      ExpectSnapshotFragment(first_snapshot, "\"protocol_id\":\"synthetic_lab_exchange\"", error) &&
      valid;
  valid = ExpectSnapshotFragment(first_snapshot, "\"pipeline_count\":2", error) && valid;
  valid = ExpectSnapshotFragment(first_snapshot, "\"message_count\":2", error) && valid;
  valid = ExpectSnapshotFragment(first_snapshot, "\"total_field_count\":10", error) && valid;
  valid =
      ExpectSnapshotFragment(first_snapshot, "\"framing_profile_id\":\"complete_record\"", error) &&
      valid;
  valid =
      ExpectSnapshotFragment(first_snapshot, "\"message_ids\":[\"lab_command\"]", error) && valid;
  valid = ExpectSnapshotFragment(first_snapshot, "\"id\":\"lab_command\"", error) && valid;
  valid = ExpectSnapshotFragment(first_snapshot, "\"id\":\"lab_report\"", error) && valid;
  Check(first_snapshot == second_snapshot, "repeat_compile_snapshot_equivalence");
  Check(first_snapshot == expected_snapshot, "canonical_snapshot_byte_equivalence");

  if (!valid) {
    runner.Fail(kCaseId, error.str());
    return;
  }
  runner.Pass(kCaseId);
}

void RunUnicodeStringLengthCases(TestRunner& runner, const std::string& minimal_json) {
  const std::string original = "\"display_name\": \"最小完整记录协议\"";

  std::string valid_unicode = minimal_json;
  const std::string valid_replacement = "\"display_name\": \"" + RepeatUtf8(u8"协", 100U) + "\"";
  if (!ReplaceOnce(valid_unicode, original, valid_replacement)) {
    runner.Fail("structural_unicode_scalar_length_valid",
                "test mutation root display_name token was not found");
  } else {
    const CompileResult result = CompileJsonToPlan(valid_unicode);
    if (!result.Succeeded()) {
      runner.Fail("structural_unicode_scalar_length_valid",
                  "100 Unicode scalar values should satisfy maxLength=256");
    } else {
      runner.Pass("structural_unicode_scalar_length_valid");
    }
  }

  std::string oversized_unicode = minimal_json;
  const std::string oversized_replacement =
      "\"display_name\": \"" + RepeatUtf8(u8"协", 257U) + "\"";
  if (!ReplaceOnce(oversized_unicode, original, oversized_replacement)) {
    runner.Fail("structural_unicode_scalar_length_exceeded",
                "test mutation root display_name token was not found");
  } else {
    RunFailureCase(runner, "structural_unicode_scalar_length_exceeded", oversized_unicode,
                   {CompileStage::STRUCTURAL, CompileError::INVALID_STRING_LENGTH,
                    std::string{"/display_name"}, std::string{"Unicode scalar count"}});
  }
}

void RunMinimalFixtureCase(TestRunner& runner, const std::filesystem::path& fixture_path) {
  constexpr std::string_view kCaseId = "valid_minimal_fixture";
  std::string input;
  std::string file_error;
  if (!ReadBinaryFile(fixture_path, input, file_error)) {
    runner.Fail(kCaseId, file_error);
    return;
  }
  const CompileResult result = CompileJsonToPlan(input);
  if (!result.Succeeded()) {
    std::ostringstream error;
    error << "minimal fixture did not compile";
    if (result.Diagnostic() != nullptr) {
      error << " stage=" << ToString(result.Diagnostic()->stage)
            << " code=" << ToString(result.Diagnostic()->code)
            << " pointer=" << result.Diagnostic()->json_pointer;
    }
    runner.Fail(kCaseId, error.str());
    return;
  }
  const auto& requirements = result.Plan()->GetResourceRequirements();
  if (result.Plan()->Messages().size() != 1U || requirements.total_field_count != 2U ||
      requirements.total_matcher_count != 2U) {
    runner.Fail(kCaseId, "unexpected minimal PlanBundle resource counts");
    return;
  }
  runner.Pass(kCaseId);
}

bool LoadFixtureOrFail(TestRunner& runner, std::string_view case_id,
                       const std::filesystem::path& path, std::string& output) {
  std::string error;
  if (ReadBinaryFile(path, output, error)) {
    return true;
  }
  runner.Fail(case_id, error);
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  TestRunner runner;
  if (argc != 2) {
    runner.Fail("runner_arguments", "expected one relative test-data directory");
    return runner.Finish();
  }

  RunCapabilityStateCase(runner);
  RunCorruptedBudgetedDraftCase(runner);
  RunPlanBuilderBitfieldDefenseCases(runner);
  RunPlanMemoryContractCases(runner);

  const std::filesystem::path data_root{argv[1]};
  const std::filesystem::path fixture_root = data_root / "fixtures";
  const std::filesystem::path valid_fixture =
      fixture_root / "valid" / "minimal_complete_record.pae.json";

  RunSyntheticLabPlanCase(runner, data_root / "synthetic_lab_exchange_slice.pae.json",
                          data_root / "expected" / "synthetic_lab_exchange_slice.canonical.json");
  RunMinimalFixtureCase(runner, valid_fixture);

  std::string minimal_json;
  if (!LoadFixtureOrFail(runner, "load_minimal_fixture", valid_fixture, minimal_json)) {
    return runner.Finish();
  }
  RunUnicodeStringLengthCases(runner, minimal_json);

  std::string bom_json{"\xEF\xBB\xBF", 3U};
  bom_json += minimal_json;
  RunFailureCase(runner, "input_utf8_bom", bom_json,
                 {CompileStage::INPUT_PROFILE, CompileError::UTF8_BOM_NOT_ALLOWED, std::string{},
                  std::nullopt});

  std::string invalid_utf8{"{\"x\":\""};
  invalid_utf8.push_back(static_cast<char>(0xC3U));
  invalid_utf8.append("(\"}");
  RunFailureCase(
      runner, "input_invalid_utf8", invalid_utf8,
      {CompileStage::INPUT_PROFILE, CompileError::INVALID_UTF8, std::string{}, std::nullopt});

  RunFailureCase(
      runner, "json_duplicate_key", "{\"x\":1,\"x\":2}",
      {CompileStage::JSON_RESOURCE, CompileError::DUPLICATE_KEY, std::string{"/x"}, std::nullopt});
  RunFailureCase(
      runner, "json_trailing_comma", "{\"schema_version\":\"0.1\",}",
      {CompileStage::JSON_SYNTAX, CompileError::JSON_SYNTAX_ERROR, std::string{}, std::nullopt});

  std::string fixture;
  if (LoadFixtureOrFail(runner, "structural_unknown_property",
                        fixture_root / "invalid" / "structural_unknown_property.pae.json",
                        fixture)) {
    RunFailureCase(runner, "structural_unknown_property", fixture,
                   {CompileStage::STRUCTURAL, CompileError::UNKNOWN_PROPERTY,
                    std::string{"/unexpected_property"}, std::nullopt});
  }

  std::string missing_property = minimal_json;
  if (!ReplaceOnce(missing_property, "\"source_ref\": \"SYNTHETIC:minimal_complete_record\",",
                   "")) {
    runner.Fail("structural_missing_property", "test mutation source_ref token was not found");
  } else {
    RunFailureCase(runner, "structural_missing_property", missing_property,
                   {CompileStage::STRUCTURAL, CompileError::MISSING_PROPERTY,
                    std::string{"/source_ref"}, std::nullopt});
  }

  std::string type_mismatch = minimal_json;
  if (!ReplaceOnce(type_mismatch, "\"protocol_id\": \"synthetic_minimal_protocol\"",
                   "\"protocol_id\": 7")) {
    runner.Fail("structural_type_mismatch", "test mutation protocol_id token was not found");
  } else {
    RunFailureCase(runner, "structural_type_mismatch", type_mismatch,
                   {CompileStage::STRUCTURAL, CompileError::TYPE_MISMATCH,
                    std::string{"/protocol_id"}, std::nullopt});
  }

  if (LoadFixtureOrFail(runner, "structural_integer_fraction",
                        fixture_root / "invalid" / "structural_integer_fraction.pae.json",
                        fixture)) {
    RunFailureCase(runner, "structural_integer_fraction", fixture,
                   {CompileStage::STRUCTURAL, CompileError::INTEGER_NOT_EXACT,
                    std::string{"/messages/0/fields/0/wire/byte_offset"},
                    std::string{"fraction and exponent"}});
  }

  if (LoadFixtureOrFail(runner, "structural_unsigned_negative_zero",
                        fixture_root / "invalid" / "structural_unsigned_negative_zero.pae.json",
                        fixture)) {
    RunFailureCase(
        runner, "structural_unsigned_negative_zero", fixture,
        {CompileStage::STRUCTURAL, CompileError::INTEGER_OUT_OF_RANGE,
         std::string{"/messages/0/fields/0/wire/byte_offset"}, std::string{"negative token"}});
  }

  std::string duplicate_id = minimal_json;
  if (!ReplaceOnce(duplicate_id, "\"id\": \"message_type\"", "\"id\": \"payload_word\"")) {
    runner.Fail("domain_duplicate_id", "test mutation field ID was not found");
  } else {
    RunFailureCase(runner, "domain_duplicate_id", duplicate_id,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::DUPLICATE_ID,
                    std::string{"/messages/0/fields/1/id"}, std::nullopt});
  }

  std::string constant_matcher_conflict = minimal_json;
  if (!ReplaceOnce(constant_matcher_conflict, "\"bytes\": \"00 01\"", "\"bytes\": \"00 02\"")) {
    runner.Fail("domain_constant_matcher_conflict",
                "test mutation fixed matcher bytes token was not found");
  } else {
    RunFailureCase(
        runner, "domain_constant_matcher_conflict", constant_matcher_conflict,
        {CompileStage::DOMAIN_VALIDATION, CompileError::MATCHER_CONFLICT,
         std::string{"/messages/0/fields/1/encode/value"}, std::string{"constant field bytes"}});
  }

  std::string ambiguous_matcher = minimal_json;
  std::string ambiguous_mutation_error;
  if (!MakeAmbiguousMatcherInput(ambiguous_matcher, ambiguous_mutation_error)) {
    runner.Fail("domain_ambiguous_matcher", ambiguous_mutation_error);
  } else {
    RunFailureCase(runner, "domain_ambiguous_matcher", ambiguous_matcher,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::AMBIGUOUS_MATCHER,
                    std::string{"/pipelines/0/message_ids/1"},
                    std::string{"intersects an earlier message matcher"}});
  }

  if (LoadFixtureOrFail(runner, "domain_missing_framing_reference",
                        fixture_root / "invalid" / "domain_missing_framing_reference.pae.json",
                        fixture)) {
    RunFailureCase(runner, "domain_missing_framing_reference", fixture,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::UNKNOWN_REFERENCE,
                    std::string{"/pipelines/0/input_framing_profile_id"}, std::nullopt});
  }

  if (LoadFixtureOrFail(runner, "domain_direction_mismatch",
                        fixture_root / "invalid" / "domain_direction_mismatch.pae.json", fixture)) {
    RunFailureCase(runner, "domain_direction_mismatch", fixture,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::DIRECTION_MISMATCH,
                    std::string{"/pipelines/0/message_ids/0"}, std::nullopt});
  }

  if (LoadFixtureOrFail(runner, "domain_field_overlap",
                        fixture_root / "invalid" / "domain_field_overlap.pae.json", fixture)) {
    RunFailureCase(runner, "domain_field_overlap", fixture,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OVERLAP,
                    std::string{"/messages/0/fields/1"}, std::nullopt});
  }

  if (LoadFixtureOrFail(runner, "domain_frame_not_fully_defined",
                        fixture_root / "invalid" / "domain_frame_not_fully_defined.pae.json",
                        fixture)) {
    RunFailureCase(runner, "domain_frame_not_fully_defined", fixture,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::FRAME_NOT_FULLY_DEFINED,
                    std::string{"/messages/0"}, std::string{"define every frame byte"}});
  }

  std::string out_of_bounds = minimal_json;
  if (!ReplaceOnce(out_of_bounds, "\"frame_length_bytes\": 4", "\"frame_length_bytes\": 1")) {
    runner.Fail("domain_field_out_of_bounds",
                "test mutation frame_length_bytes token was not found");
  } else {
    RunFailureCase(runner, "domain_field_out_of_bounds", out_of_bounds,
                   {CompileStage::DOMAIN_VALIDATION, CompileError::FIELD_OUT_OF_BOUNDS,
                    std::string{"/messages/0/fields/0/wire"}, std::nullopt});
  }

  std::string oversized_input((4U * 1024U * 1024U) + 1U, ' ');
  RunFailureCase(runner, "resource_input_hard_limit", oversized_input,
                 {CompileStage::INPUT_PROFILE, CompileError::INPUT_LIMIT_EXCEEDED, std::string{},
                  std::nullopt});

  return runner.Finish();
}
