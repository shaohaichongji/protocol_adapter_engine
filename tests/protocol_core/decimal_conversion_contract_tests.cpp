#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

#include "complete_record_codec.h"
#include "config_compiler.h"

namespace {

bool Expect(bool condition, const char* message) {
  if (!condition) std::cerr << "FAILED: " << message << '\n';
  return condition;
}

std::string ReadText(const char* path) {
  std::ifstream input(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

pae::protocol_core::EncodeFieldValue DecimalValue(const pae::protocol_plan::PlanBundle& plan,
                                                  std::size_t field_index, std::int64_t coefficient,
                                                  std::int32_t scale) {
  pae::protocol_core::EncodeFieldValue value;
  value.field = {&plan, 0U, field_index};
  value.value_kind = pae::protocol_core::LogicalValueKind::DECIMAL64;
  value.decimal64_value = {coefficient, scale};
  return value;
}

pae::config_compiler::CompileResult CompileNoConversionPlan() {
  return pae::config_compiler::CompileJsonToPlan(R"json({
    "schema_version":"0.5","protocol_id":"decimal_no_conversion","protocol_version":"1",
    "display_name":"Synthetic","description":"","source_ref":"SYNTHETIC_FROM_SCRATCH:decimal_core",
    "resource_profile":"desktop",
    "framing_profiles":[{"id":"record","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:decimal_core","input_kind":"complete_record"}],
    "pipelines":[{"id":"sample_pipeline","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:decimal_core","direction_id":"rx",
      "input_framing_profile_id":"record","message_ids":["sample"]}],
    "messages":[{"id":"sample","display_name":"Synthetic","description":"",
      "source_ref":"SYNTHETIC_FROM_SCRATCH:decimal_core","direction_id":"rx",
      "frame_length_bytes":1,"matcher":{"all":[{"kind":"frame_length_equals","length_bytes":1}]},
      "fields":[{"id":"legacy","display_name":"Synthetic","description":"",
        "source_ref":"SYNTHETIC_FROM_SCRATCH:decimal_core","value_type":"UINT64",
        "wire":{"codec":"unsigned_integer","byte_offset":0,"byte_width":1},
        "encode":{"source":"input"}}]}]})json");
}

}  // namespace

int main(int argc, char** argv) {
  using namespace pae::protocol_core;
  if (!Expect(argc == 2, "fixture path is required")) return 1;
  auto compiled = pae::config_compiler::CompileJsonToPlan(ReadText(argv[1]));
  if (!compiled.Succeeded() && compiled.Diagnostic() != nullptr) {
    std::cerr << "compile diagnostic stage=" << static_cast<int>(compiled.Diagnostic()->stage)
              << " code=" << static_cast<int>(compiled.Diagnostic()->code)
              << " pointer=" << compiled.Diagnostic()->json_pointer
              << " detail=" << compiled.Diagnostic()->detail << '\n';
  }
  if (!Expect(compiled.Succeeded(), "Schema 0.5 Core fixture compiles")) return 1;
  auto plan = std::move(compiled).TakePlan();
  const auto& layout = plan->GetExecutionResourceLayout();
  const std::size_t expected_workspace_bytes =
      layout.encode_value_index_count * sizeof(std::size_t) +
      layout.encode_presence_word_count * sizeof(std::uint64_t) +
      layout.bit_container_value_count * sizeof(std::uint64_t) +
      layout.conversion_value_count *
          (sizeof(std::int64_t) + sizeof(std::int32_t) + sizeof(std::uint64_t) +
           sizeof(pae::protocol_plan::ValueType) + sizeof(std::size_t) + sizeof(std::uint64_t));
  const auto maximum_workspace_payload =
      [](const pae::protocol_plan::ResourceProfileLimits& limits) {
        constexpr std::size_t kConversionBytesPerSlot =
            sizeof(std::int64_t) + sizeof(std::int32_t) + sizeof(std::uint64_t) +
            sizeof(pae::protocol_plan::ValueType) + sizeof(std::size_t) + sizeof(std::uint64_t);
        const std::size_t presence_words = limits.max_fields_per_message / 64U +
                                           (limits.max_fields_per_message % 64U == 0U ? 0U : 1U);
        return limits.max_fields_per_message * sizeof(std::size_t) +
               presence_words * sizeof(std::uint64_t) +
               limits.max_fields_per_message * sizeof(std::uint64_t) +
               limits.max_fields_per_message * kConversionBytesPerSlot;
      };
  if (!Expect(plan->SchemaVersion() == "0.5", "Schema generation is preserved") ||
      !Expect(internal::SupportsCompleteRecordSchema("0.1") &&
                  internal::SupportsCompleteRecordSchema("0.4") &&
                  internal::SupportsCompleteRecordSchema("0.5") &&
                  !internal::SupportsCompleteRecordSchema("0.6") &&
                  !internal::SupportsCompleteRecordSchema(""),
              "Core runtime schema capability gate is explicit") ||
      !Expect(plan->Conversions().size() == 4U, "four conversion descriptors are frozen") ||
      !Expect(plan->GetExecutionResourceLayout().conversion_value_count == 4U,
              "workspace conversion capacity is exact") ||
      !Expect(layout.estimated_workspace_bytes == expected_workspace_bytes,
              "workspace payload accounting uses actual element sizes") ||
      !Expect(
          maximum_workspace_payload(pae::protocol_plan::kConstrainedResourceProfileLimits) <=
                  pae::protocol_plan::kConstrainedResourceProfileLimits.max_session_memory_bytes &&
              maximum_workspace_payload(pae::protocol_plan::kDesktopResourceProfileLimits) <=
                  pae::protocol_plan::kDesktopResourceProfileLimits.max_session_memory_bytes,
          "legal per-message count limits bound workspace payload below session limits")) {
    return 1;
  }

  auto no_conversion_compiled = CompileNoConversionPlan();
  if (!Expect(no_conversion_compiled.Succeeded(), "Schema 0.5 without conversion compiles")) {
    return 1;
  }
  auto no_conversion_plan = std::move(no_conversion_compiled).TakePlan();
  ExecutionWorkspace no_conversion_workspace(*no_conversion_plan);
  EncodeFieldValue legacy;
  legacy.field = {no_conversion_plan.get(), 0U, 0U};
  legacy.value_kind = LogicalValueKind::UINT64;
  legacy.uint64_value = 0x5AU;
  std::uint8_t legacy_byte = 0U;
  const auto legacy_encoded = EncodeCompleteRecord(*no_conversion_plan, no_conversion_workspace, 0U,
                                                   0U, &legacy, 1U, {&legacy_byte, 1U});
  if (!Expect(legacy_encoded.status == CodecStatus::OK && legacy_byte == 0x5AU &&
                  no_conversion_plan->GetExecutionResourceLayout().conversion_value_count == 0U,
              "Schema 0.5 no-conversion plan keeps inherited Core semantics")) {
    return 1;
  }

  ExecutionWorkspace workspace(*plan);
  std::array<EncodeFieldValue, 4> values{
      DecimalValue(*plan, 0U, 123, 1),
      DecimalValue(*plan, 1U, (std::numeric_limits<std::int64_t>::max)(), 0),
      DecimalValue(*plan, 2U, 0, 0),
      DecimalValue(*plan, 3U, (std::numeric_limits<std::int64_t>::max)(), 0)};
  std::array<std::uint8_t, 34> frame{};
  const auto encoded = EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(), values.size(),
                                            {frame.data(), frame.size()});
  const std::array<std::uint8_t, 34> expected{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0B, 0xFF,
                                              0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
                                              0x00, 0x00, 0x00, 0x00, 0x00, 0xA5, 0x2A};
  if (!Expect(encoded.status == CodecStatus::OK, "independent Encode vector succeeds") ||
      !Expect(frame == expected, "Encode bytes match independently calculated vector") ||
      !Expect(workspace.LastOperationCounts().decimal_conversion_visits == 8U,
              "Encode performs one preflight and one final conversion per field") ||
      !Expect(workspace.LastRawIntegerCount() == 0U,
              "Encode invalidates prior Decode raw diagnostics")) {
    return 1;
  }

  std::array<DecodedFieldSlot, 5> slots{};
  const auto decoded = DecodeCompleteRecord(*plan, workspace, 0U, {frame.data(), frame.size()},
                                            slots.data(), slots.size());
  if (!Expect(decoded.status == CodecStatus::OK, "independent Decode vector succeeds") ||
      !Expect(decoded.field_count == 5U, "whole mixed record is delivered") ||
      !Expect(slots[0].value_kind == LogicalValueKind::DECIMAL64 &&
                  slots[0].decimal64_value.coefficient == 123 &&
                  slots[0].decimal64_value.scale == 1,
              "temperature is normalized Decimal64") ||
      !Expect(slots[1].decimal64_value.coefficient == (std::numeric_limits<std::int64_t>::max)() &&
                  slots[1].decimal64_value.scale == 0,
              "wide intermediate cancellation is exact") ||
      !Expect(slots[3].decimal64_value.coefficient == (std::numeric_limits<std::int64_t>::max)() &&
                  slots[3].decimal64_value.scale == 0,
              "negative scale with INT64_MIN raw value is exact") ||
      !Expect(slots[4].value_kind == LogicalValueKind::UINT64 && slots[4].uint64_value == 0xA5U,
              "unconverted inherited field remains unchanged") ||
      !Expect(workspace.LastRawIntegerCount() == 4U,
              "successful Decode publishes raw diagnostics") ||
      !Expect(workspace.LastOperationCounts().decimal_conversion_visits == 4U,
              "Decode converts each field exactly once")) {
    return 1;
  }
  RawIntegerValue raw;
  if (!Expect(workspace.GetLastRawInteger(0U, raw) && raw.kind == RawIntegerKind::INT64 &&
                  raw.int64_value == 523 && raw.field.message_index == 0U &&
                  raw.field.field_index == 0U,
              "signed raw diagnostic is bound to the converted field") ||
      !Expect(workspace.GetLastRawInteger(1U, raw) && raw.kind == RawIntegerKind::UINT64 &&
                  raw.uint64_value == (std::numeric_limits<std::uint64_t>::max)(),
              "unsigned raw diagnostic preserves UINT64_MAX") ||
      !Expect(workspace.GetLastRawInteger(3U, raw) && raw.kind == RawIntegerKind::INT64 &&
                  raw.int64_value == (std::numeric_limits<std::int64_t>::min)(),
              "signed raw diagnostic preserves INT64_MIN") ||
      !Expect(!workspace.GetLastRawInteger(4U, raw),
              "raw diagnostic bounds reject the first out-of-range index")) {
    return 1;
  }

  const auto raw_before_failed_encode = DecodeCompleteRecord(
      *plan, workspace, 0U, {expected.data(), expected.size()}, slots.data(), slots.size());
  RawIntegerValue failed_encode_raw;
  if (!Expect(raw_before_failed_encode.status == CodecStatus::OK &&
                  raw_before_failed_encode.field_count == 5U &&
                  workspace.LastRawIntegerCount() == 4U &&
                  workspace.GetLastRawInteger(0U, failed_encode_raw) &&
                  failed_encode_raw.kind == RawIntegerKind::INT64 &&
                  failed_encode_raw.int64_value == 523 &&
                  failed_encode_raw.field.plan_scope == plan.get() &&
                  failed_encode_raw.field.message_index == 0U &&
                  failed_encode_raw.field.field_index == 0U,
              "failed-Encode transition starts with independently readable raw diagnostics")) {
    return 1;
  }
  values[0].decimal64_value = {1, 19};
  const auto failed_encode_after_raw = EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(), {frame.data(), frame.size()});
  if (!Expect(failed_encode_after_raw.status == CodecStatus::INVALID_ARGUMENT &&
                  failed_encode_after_raw.conversion_error ==
                      ConversionError::DECIMAL_SCALE_OUT_OF_RANGE &&
                  failed_encode_after_raw.failed_value_index == 0U &&
                  failed_encode_after_raw.failed_field_index == 0U &&
                  failed_encode_after_raw.required_size == 0U &&
                  failed_encode_after_raw.bytes_written == 0U &&
                  workspace.LastRawIntegerCount() == 0U &&
                  !workspace.GetLastRawInteger(0U, failed_encode_raw),
              "failed Encode invalidates previously readable raw diagnostics")) {
    return 1;
  }

  const auto raw_before_failed_decode = DecodeCompleteRecord(
      *plan, workspace, 0U, {expected.data(), expected.size()}, slots.data(), slots.size());
  RawIntegerValue failed_decode_raw;
  if (!Expect(raw_before_failed_decode.status == CodecStatus::OK &&
                  raw_before_failed_decode.field_count == 5U &&
                  workspace.LastRawIntegerCount() == 4U &&
                  workspace.GetLastRawInteger(0U, failed_decode_raw) &&
                  failed_decode_raw.kind == RawIntegerKind::INT64 &&
                  failed_decode_raw.int64_value == 523 &&
                  failed_decode_raw.field.plan_scope == plan.get() &&
                  failed_decode_raw.field.message_index == 0U &&
                  failed_decode_raw.field.field_index == 0U,
              "failed-Decode transition starts with independently readable raw diagnostics")) {
    return 1;
  }
  auto invalid_sum_after_raw = expected;
  invalid_sum_after_raw[33] ^= 0x01U;
  const auto failed_decode_after_raw = DecodeCompleteRecord(
      *plan, workspace, 0U, {invalid_sum_after_raw.data(), invalid_sum_after_raw.size()},
      slots.data(), slots.size());
  if (!Expect(failed_decode_after_raw.status == CodecStatus::INTEGRITY_FAILED &&
                  failed_decode_after_raw.field_count == 0U &&
                  workspace.LastRawIntegerCount() == 0U &&
                  !workspace.GetLastRawInteger(0U, failed_decode_raw),
              "failed Decode invalidates previously readable raw diagnostics")) {
    return 1;
  }

  values[0].decimal64_value = {1230, 2};
  const auto equivalent = EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(),
                                               values.size(), {frame.data(), frame.size()});
  if (!Expect(equivalent.status == CodecStatus::OK && frame == expected &&
                  workspace.LastRawIntegerCount() == 0U && !workspace.GetLastRawInteger(0U, raw),
              "successful Encode invalidates prior Decode raw diagnostics"))
    return 1;

  values[0].decimal64_value = {1235, 2};
  const auto non_integral =
      EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(), values.size(), {nullptr, 0U});
  if (!Expect(non_integral.status == CodecStatus::VALUE_NOT_REPRESENTABLE &&
                  non_integral.conversion_error == ConversionError::RAW_NOT_INTEGRAL &&
                  non_integral.required_size == 0U && non_integral.bytes_written == 0U,
              "non-integral inverse precedes output capacity validation"))
    return 1;

  const auto expect_bad_scale = [&](std::int64_t coefficient, std::int32_t scale,
                                    const char* message) {
    values[0].decimal64_value = {coefficient, scale};
    const auto bad_scale = EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(),
                                                values.size(), {frame.data(), frame.size()});
    return Expect(bad_scale.status == CodecStatus::INVALID_ARGUMENT &&
                      bad_scale.conversion_error == ConversionError::DECIMAL_SCALE_OUT_OF_RANGE &&
                      bad_scale.failed_value_index == 0U && bad_scale.failed_field_index == 0U &&
                      bad_scale.required_size == 0U && bad_scale.bytes_written == 0U &&
                      workspace.LastRawIntegerCount() == 0U &&
                      workspace.LastOperationCounts().decimal_conversion_visits == 1U,
                  message);
  };
  if (!expect_bad_scale(1, -1, "negative Decimal scale is INVALID_ARGUMENT") ||
      !expect_bad_scale(1, 19, "Decimal scale above 18 is INVALID_ARGUMENT") ||
      !expect_bad_scale(0, -1, "zero coefficient does not bypass negative scale validation") ||
      !expect_bad_scale(0, 19, "zero coefficient does not bypass high scale validation")) {
    return 1;
  }

  values[0].decimal64_value = {(std::numeric_limits<std::int64_t>::max)(), 0};
  const auto wire_overflow = EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(),
                                                  values.size(), {frame.data(), frame.size()});
  if (!Expect(wire_overflow.status == CodecStatus::VALUE_NOT_REPRESENTABLE &&
                  wire_overflow.conversion_error == ConversionError::RAW_OUT_OF_RANGE,
              "inverse result outside signed Wire range is distinguished"))
    return 1;

  values[0] = DecimalValue(*plan, 0U, 123, 1);
  values[1].value_kind = LogicalValueKind::UINT64;
  const auto wrong_type = EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(),
                                               values.size(), {frame.data(), frame.size()});
  if (!Expect(
          wrong_type.status == CodecStatus::TYPE_MISMATCH && wrong_type.failed_field_index == 1U,
          "converted field rejects raw integer input"))
    return 1;
  values[1] = DecimalValue(*plan, 1U, (std::numeric_limits<std::int64_t>::max)(), 0);

  std::array<EncodeFieldValue, 4> reordered{values[3], values[2], values[1], values[0]};
  reordered[3].decimal64_value = {1235, 2};
  reordered[2].value_kind = LogicalValueKind::UINT64;
  const auto config_order = EncodeCompleteRecord(*plan, workspace, 0U, 0U, reordered.data(),
                                                 reordered.size(), {nullptr, 0U});
  if (!Expect(config_order.status == CodecStatus::VALUE_NOT_REPRESENTABLE &&
                  config_order.conversion_error == ConversionError::RAW_NOT_INTEGRAL &&
                  config_order.failed_field_index == 0U,
              "numeric failures use configuration order, independent of input order"))
    return 1;

  auto bad_sum = expected;
  bad_sum[33] ^= 2U;
  bad_sum[16] = 0xFFU;
  const auto integrity_first = DecodeCompleteRecord(
      *plan, workspace, 0U, {bad_sum.data(), bad_sum.size()}, slots.data(), slots.size());
  if (!Expect(integrity_first.status == CodecStatus::INTEGRITY_FAILED &&
                  integrity_first.field_count == 0U && workspace.LastRawIntegerCount() == 0U,
              "SUM8 failure precedes conversion and publishes nothing"))
    return 1;

  auto overflow = expected;
  for (std::size_t index = 16U; index < 24U; ++index) overflow[index] = 0xFFU;
  overflow[33] = 0x22U;
  const auto logical_overflow = DecodeCompleteRecord(
      *plan, workspace, 0U, {overflow.data(), overflow.size()}, slots.data(), slots.size());
  if (!Expect(logical_overflow.status == CodecStatus::VALUE_NOT_REPRESENTABLE &&
                  logical_overflow.conversion_error == ConversionError::LOGICAL_OUT_OF_RANGE &&
                  logical_overflow.failed_field_index == 2U && logical_overflow.field_count == 0U &&
                  workspace.LastRawIntegerCount() == 0U,
              "Decode logical overflow fails transactionally"))
    return 1;

  test_only::FailNextDecimalConversionOnce();
  const auto internal_error = DecodeCompleteRecord(
      *plan, workspace, 0U, {expected.data(), expected.size()}, slots.data(), slots.size());
  if (!Expect(internal_error.status == CodecStatus::INTERNAL_ERROR &&
                  internal_error.conversion_error == ConversionError::NONE,
              "legal-plan arithmetic fault maps to INTERNAL_ERROR"))
    return 1;

  values[0] = DecimalValue(*plan, 0U, 123, 1);
  test_only::FailNextDecimalFinalReviewOnce();
  const auto final_internal_error = EncodeCompleteRecord(
      *plan, workspace, 0U, 0U, values.data(), values.size(), {frame.data(), frame.size()});
  if (!Expect(final_internal_error.status == CodecStatus::INTERNAL_ERROR &&
                  final_internal_error.conversion_error == ConversionError::NONE &&
                  final_internal_error.failed_field_index == 0U &&
                  final_internal_error.bytes_written == 0U &&
                  workspace.LastOperationCounts().decimal_conversion_visits == 5U,
              "final-review arithmetic fault remains INTERNAL_ERROR after successful preflight")) {
    return 1;
  }

  test_only::CorruptDecimalFieldBeforeFinalReviewOnce();
  const auto final_review = EncodeCompleteRecord(*plan, workspace, 0U, 0U, values.data(),
                                                 values.size(), {frame.data(), frame.size()});
  if (!Expect(final_review.status == CodecStatus::FINAL_REVIEW_FAILED &&
                  final_review.bytes_written == 0U,
              "final review rereads and reconverts actual bytes"))
    return 1;

  std::cout << "DEC-042B Core conversion contract checks passed\n";
  return 0;
}
