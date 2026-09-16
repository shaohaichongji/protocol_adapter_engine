#include "public_binary_decode.h"

#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif

using namespace pae::protocol_lab_binary::public_decode;

#undef assert
#define assert(expression) do { if (!(expression)) { \
  std::cerr << "PUBLIC_H1_CHECK_FAILED line=" << __LINE__ << " expression=" << #expression << '\n'; \
  return 1; \
} } while (false)

int main(int argc, char** argv) {
#ifdef _MSC_VER
  _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
  _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
  assert(argc == 2);
  const std::string path = std::string(argv[1]) +
      "/tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json";
  std::ifstream file(path, std::ios::binary);
  const std::string json(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
  assert(!json.empty());
  auto prepared = Adapter::Create(json, "rx", "ui_pipeline");
  if (prepared.status != LocalStatus::OK || !prepared.adapter) {
    std::cerr << "PREPARATION_FAILED status=" << static_cast<int>(prepared.status) << '\n';
    std::cerr << "HOST_STATUS=" << static_cast<int>(prepared.host_status) << '\n';
    if (prepared.diagnostic)
      std::cerr << "COMPILE_DIAGNOSTIC=" << static_cast<int>(prepared.diagnostic->code)
                << " detail=" << prepared.diagnostic->detail << '\n';
    return 1;
  }
  auto& adapter = *prepared.adapter;
  assert(adapter.SetDraft(0U, "800D03000100CAFE055A"));
  assert(adapter.SetDraft(1U, "flow-two"));
  assert(!adapter.SetDraft(1U, std::string(131073U, 'x')) &&
         adapter.State(1U)->draft == "flow-two");
  const std::array<std::uint8_t, 10> frame{0x80U, 0x0DU, 3U, 0U, 1U, 0U, 0xCAU, 0xFEU, 5U, 0x5AU};
  const auto& first = adapter.Decode(0U, {frame.data(), frame.size()});
  assert(first.host.status == pae::HostStatus::OK && first.host.decode_attempts == 1U);
  assert(first.candidate && first.candidate->success && first.candidate->fields.size() == 9U);
  const auto& value = *first.candidate;
  assert(value.message_id == "typed_record" && value.frame.size() == frame.size());
  assert(value.fields[0].kind == pae::ValueKind::BOOL && value.fields[0].bool_value);
  assert(value.fields[0].bit_mask_count > 0U && !value.fields[0].byte_range);
  assert(value.fields[1].enum_raw == 2U && value.fields[1].known_enum_flat_index);
  assert(value.fields[6].bytes.size() == 2U && value.fields[6].bytes[0] == 0xCAU &&
         value.fields[6].byte_range && value.fields[6].byte_range->offset == 6U &&
         value.fields[6].byte_range->length == 2U);
  assert(value.fields[7].kind == pae::ValueKind::DECIMAL64 &&
         value.fields[7].conversion_raw_kind == pae::RawIntegerKind::INT64 &&
         value.fields[7].conversion_raw_int64 == 5);
  assert(!value.integrity_storage && !value.computed_length_storage);
  assert(adapter.State(1U)->draft == "flow-two" && !adapter.State(1U)->current.candidate);
  const auto& second = adapter.Decode(1U, {frame.data(), frame.size()});
  assert(second.candidate && second.candidate->success);
  assert(adapter.State(0U)->current.candidate && adapter.State(0U)->current.candidate->success);
  const auto& rejected = adapter.Decode(0U, {nullptr, 1U});
  assert(rejected.local_status == LocalStatus::INVALID_INPUT &&
         adapter.State(0U)->current.candidate->success);
  auto unknown = frame;
  unknown[1] = 0x0FU;
  const auto& unknown_enum = adapter.Decode(0U, {unknown.data(), unknown.size()});
  assert(unknown_enum.candidate && unknown_enum.candidate->success &&
         unknown_enum.candidate->fields[1].enum_raw &&
         !unknown_enum.candidate->fields[1].known_enum_flat_index);
  const auto& failed = adapter.Decode(0U, {frame.data(), frame.size() - 1U});
  assert(failed.host.codec_attempted && failed.candidate && !failed.candidate->success &&
         failed.candidate->fields.empty());
  const auto& restored = adapter.Decode(0U, {frame.data(), frame.size()});
  assert(restored.candidate && restored.candidate->success);
  assert(adapter.Reset(0U) == pae::HostStatus::OK);
  assert(adapter.State(0U)->draft.empty() && !adapter.State(0U)->current.candidate &&
         adapter.State(1U)->draft == "flow-two" && adapter.State(1U)->current.candidate);
  assert(adapter.Decode(0U, {frame.data(), frame.size()}).candidate->success);
  assert(adapter.State(1U)->current.candidate && adapter.State(1U)->current.candidate->success);
  const auto exact = adapter.InstanceAdmissionBytes();
  Limits exact_limits;
  exact_limits.instance_bytes = exact;
  auto exact_prepared = Adapter::Create(json, "rx", "ui_pipeline", exact_limits);
  assert(exact_prepared.status == LocalStatus::OK);
  exact_limits.instance_bytes = exact - 1U;
  auto minus_one = Adapter::Create(json, "rx", "ui_pipeline", exact_limits);
  assert(minus_one.status == LocalStatus::RESOURCE_LIMIT);
  exact_limits.instance_bytes = exact;
  exact_limits.replacement_bytes = exact + 10U;
  assert(Adapter::Create(json, "rx", "ui_pipeline", exact_limits, 10U).status == LocalStatus::OK);
  exact_limits.replacement_bytes = exact + 9U;
  assert(Adapter::Create(json, "rx", "ui_pipeline", exact_limits, 10U).status == LocalStatus::RESOURCE_LIMIT);
  Limits full_draft_limits;
  full_draft_limits.max_frame_bytes = 16U;
  auto full_draft = Adapter::Create(json, "draft", "ui_pipeline", full_draft_limits);
  assert(full_draft.status == LocalStatus::OK && full_draft.adapter);
  Limits next_draft_limits = full_draft_limits;
  next_draft_limits.max_frame_bytes = 17U;
  auto next_draft = Adapter::Create(json, "draft", "ui_pipeline", next_draft_limits);
  assert(next_draft.status == LocalStatus::OK && next_draft.adapter);
  assert(next_draft.adapter->InstanceAdmissionBytes() -
         full_draft.adapter->InstanceAdmissionBytes() == 12U);
  const auto full_draft_account = full_draft.adapter->InstanceAdmissionBytes();
  full_draft_limits.instance_bytes = full_draft_account;
  auto full_exact = Adapter::Create(json, "draft", "ui_pipeline", full_draft_limits);
  assert(full_exact.status == LocalStatus::OK && full_exact.adapter);
  assert(full_exact.adapter->SetDraft(0U, std::string(32U, 'A')));
  assert(full_exact.adapter->SetDraft(1U, std::string(32U, 'B')));
  assert(full_exact.adapter->SetDraft(0U, std::string(32U, 'C')));
  assert(full_exact.adapter->State(0U)->draft == std::string(32U, 'C') &&
         full_exact.adapter->State(1U)->draft == std::string(32U, 'B'));
  full_draft_limits.instance_bytes = full_draft_account - 1U;
  assert(Adapter::Create(json, "draft", "ui_pipeline", full_draft_limits).status ==
         LocalStatus::RESOURCE_LIMIT);
  full_draft_limits.instance_bytes = full_draft_account;
  full_draft_limits.replacement_bytes = full_draft_account + 10U;
  assert(Adapter::Create(json, "draft", "ui_pipeline", full_draft_limits, 10U).status ==
         LocalStatus::OK);
  full_draft_limits.replacement_bytes = full_draft_account + 9U;
  assert(Adapter::Create(json, "draft", "ui_pipeline", full_draft_limits, 10U).status ==
         LocalStatus::RESOURCE_LIMIT);

  std::ifstream bounded_file(std::string(argv[1]) +
      "/examples/config/synthetic_bounded_variable_record.pae.json", std::ios::binary);
  const std::string bounded_json(std::istreambuf_iterator<char>{bounded_file},
                                 std::istreambuf_iterator<char>{});
  auto bounded = Adapter::Create(bounded_json, "bounded", "synthetic_rx");
  assert(bounded.status == LocalStatus::OK && bounded.adapter);
  const std::array<std::uint8_t, 3> zero{0xA5U, 3U, 0xA8U};
  const auto& zero_result = bounded.adapter->Decode(0U, {zero.data(), zero.size()});
  assert(zero_result.candidate && zero_result.candidate->success &&
         zero_result.candidate->fields[1].byte_range &&
         zero_result.candidate->fields[1].byte_range->offset == 2U &&
         zero_result.candidate->fields[1].byte_range->length == 0U &&
         zero_result.candidate->fields[1].bytes.empty() &&
         zero_result.candidate->integrity_storage &&
         zero_result.candidate->integrity_storage->offset == 2U &&
         zero_result.candidate->computed_length_storage &&
         zero_result.candidate->computed_length_storage->offset == 1U);
  const std::array<std::uint8_t, 5> middle{0xA5U, 5U, 0x11U, 0x22U, 0xDDU};
  const auto& middle_result = bounded.adapter->Decode(0U, {middle.data(), middle.size()});
  assert(middle_result.candidate && middle_result.candidate->success &&
         middle_result.candidate->fields[1].byte_range->length == 2U &&
         middle_result.candidate->integrity_storage->offset == 4U);
  const std::array<std::uint8_t, 6> maximum{0xA5U, 6U, 0x11U, 0x22U, 0x33U, 0x11U};
  const auto& maximum_result = bounded.adapter->Decode(0U, {maximum.data(), maximum.size()});
  assert(maximum_result.candidate && maximum_result.candidate->success &&
         maximum_result.candidate->fields[1].byte_range->length == 3U &&
         maximum_result.candidate->integrity_storage->offset == 5U);
  auto bad_sum = maximum;
  bad_sum[5] = 0U;
  const auto& sum_failure = bounded.adapter->Decode(0U, {bad_sum.data(), bad_sum.size()});
  assert(sum_failure.candidate && !sum_failure.candidate->success &&
         sum_failure.candidate->codec_status == pae::CodecStatus::INTEGRITY_FAILED &&
         sum_failure.candidate->fields.empty() && !sum_failure.candidate->integrity_storage);
  assert(bounded.adapter->Decode(0U, {maximum.data(), maximum.size()}).candidate->success);

  std::ifstream int_file(std::string(argv[1]) + "/examples/config/synthetic_int64_slice.pae.json",
                         std::ios::binary);
  const std::string int_json(std::istreambuf_iterator<char>{int_file},
                             std::istreambuf_iterator<char>{});
  auto int_adapter = Adapter::Create(int_json, "int", "sample_pipeline");
  assert(int_adapter.status == LocalStatus::OK && int_adapter.adapter);
  const std::array<std::uint8_t, 25> int_frame{
      0xFFU, 0xFFU, 0xFEU, 0x00U, 0x00U, 0x80U, 0xFFU, 0xFFU, 0xFFU, 0xFFU, 0xFEU, 0x80U, 0x00U,
      0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0xFFU, 0xA1U, 0xCAU, 0xFEU, 0x02U, 0x60U};
  const auto& int_result = int_adapter.adapter->Decode(0U, {int_frame.data(), int_frame.size()});
  assert(int_result.candidate && int_result.candidate->success &&
         int_result.candidate->integrity_storage &&
         int_result.candidate->integrity_storage->offset == 24U &&
         int_result.candidate->fields[0].int64_value == -2);
  const auto result_exact = int_result.candidate->accounted_bytes;
  Limits result_limits;
  result_limits.max_result_bytes = result_exact;
  auto result_exact_adapter = Adapter::Create(int_json, "int", "sample_pipeline", result_limits);
  assert(result_exact_adapter.status == LocalStatus::OK && result_exact_adapter.adapter);
  assert(result_exact_adapter.adapter->Decode(0U, {int_frame.data(), int_frame.size()}).candidate->success);
  result_limits.max_result_bytes = result_exact - 1U;
  auto result_minus_adapter = Adapter::Create(int_json, "int", "sample_pipeline", result_limits);
  assert(result_minus_adapter.status == LocalStatus::OK && result_minus_adapter.adapter);
  const auto& copy_failed = result_minus_adapter.adapter->Decode(0U, {int_frame.data(), int_frame.size()});
  assert(copy_failed.host.status == pae::HostStatus::CALLBACK_FAILED &&
         copy_failed.host.reset_required && !copy_failed.candidate &&
         copy_failed.local_status == LocalStatus::MATERIALIZATION_FAILED);
  assert(result_minus_adapter.adapter->Reset(0U) == pae::HostStatus::OK);
  std::ifstream crc_file(std::string(argv[1]) + "/examples/config/synthetic_crc_slice.pae.json",
                         std::ios::binary);
  const std::string crc_json(std::istreambuf_iterator<char>{crc_file},
                             std::istreambuf_iterator<char>{});
  auto empty_fields_json = crc_json;
  const auto empty_begin = empty_fields_json.find("\"fields\": [");
  const auto empty_end = empty_fields_json.find("      ]", empty_begin);
  assert(empty_begin != std::string::npos && empty_end != std::string::npos);
  empty_fields_json.replace(empty_begin, empty_end + 7U - empty_begin, "\"fields\": []");
  const auto empty_compiled = pae::CompileProtocolJson(empty_fields_json);
  assert(!empty_compiled.Succeeded() && empty_compiled.Diagnostic() &&
         empty_compiled.Diagnostic()->code == pae::CompileError::EMPTY_ARRAY);
  auto crc = Adapter::Create(crc_json, "crc", "synthetic_rx");
  assert(crc.status == LocalStatus::OK && crc.adapter);
  const std::array<std::uint8_t, 12> crc_frame{
      '1', '2', '3', '4', '5', '6', '7', '8', '9', 0x29U, 0xB1U, 0xAAU};
  const auto& crc_ok = crc.adapter->Decode(0U, {crc_frame.data(), crc_frame.size()});
  assert(crc_ok.candidate && crc_ok.candidate->success &&
         crc_ok.candidate->integrity_storage &&
         crc_ok.candidate->integrity_storage->offset == 9U &&
         crc_ok.candidate->integrity_storage->length == 2U);
  const auto crc_account = crc_ok.candidate->accounted_bytes;
  Limits crc_result_limits;
  crc_result_limits.max_result_bytes = crc_account;
  auto crc_exact = Adapter::Create(crc_json, "crc", "synthetic_rx", crc_result_limits);
  assert(crc_exact.status == LocalStatus::OK && crc_exact.adapter &&
         crc_exact.adapter->Decode(0U, {crc_frame.data(), crc_frame.size()}).candidate->success);
  crc_result_limits.max_result_bytes = crc_account - 1U;
  auto crc_minus = Adapter::Create(crc_json, "crc", "synthetic_rx", crc_result_limits);
  assert(crc_minus.status == LocalStatus::OK && crc_minus.adapter);
  const auto& crc_copy_reject = crc_minus.adapter->Decode(0U, {crc_frame.data(), crc_frame.size()});
  assert(crc_copy_reject.host.status == pae::HostStatus::CALLBACK_FAILED &&
         !crc_copy_reject.candidate && crc_copy_reject.host.reset_required);
  auto crc_bad = crc_frame;
  crc_bad[9] ^= 1U;
  const auto& crc_failure = crc.adapter->Decode(0U, {crc_bad.data(), crc_bad.size()});
  assert(crc_failure.candidate && !crc_failure.candidate->success &&
         crc_failure.candidate->codec_status == pae::CodecStatus::INTEGRITY_FAILED &&
         crc_failure.candidate->fields.empty() &&
         !crc_failure.candidate->integrity_storage);
  assert(crc.adapter->Decode(0U, {crc_frame.data(), crc_frame.size()}).candidate->success);
  std::cout << "PUBLIC_BINARY_H1_TEST_PASS\n";
}
