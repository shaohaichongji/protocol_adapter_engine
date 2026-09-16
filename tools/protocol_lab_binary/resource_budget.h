#pragma once

#include <cstddef>

namespace pae::protocol_lab_binary {
struct ResourceLimits {
  std::size_t instance_bytes = 128U * 1024U * 1024U;
  std::size_t replacement_bytes = 256U * 1024U * 1024U;
};
struct ResourceInputs {
  std::size_t owner_bytes = 0U, plan_bytes = 0U, session_bytes = 0U;
  std::size_t description_bytes = 0U, binding_bytes = 0U, source_sidecar_bytes = 0U;
  std::size_t channels = 0U, decode_channels = 0U, encode_channels = 0U;
  std::size_t candidate_limit = 0U;
};
struct ResourceReport {
  ResourceInputs measured;
  std::size_t retained_dto_reserve = 0U, callback_reserve = 0U;
  std::size_t frozen_reserve = 0U, utf16_draft_reserve = 0U;
  std::size_t ui_view_reserve = 0U, hex_preview_reserve = 0U, encode_reserve = 0U;
  std::size_t resident_bytes = 0U, instance_admission_bytes = 0U;
  std::size_t preparation_peak_bytes = 0U, replacement_peak_bytes = 0U;
};
// Accounting, not RSS. Reserves are future storage ceilings, not evidence of UI/stream gates.
// old_instance_bytes must come from the still-live previous instance's report.
ResourceReport CalculateResourceReport(const ResourceInputs&, const ResourceLimits& = {},
                                       std::size_t old_instance_bytes = 0U);
}  // namespace pae::protocol_lab_binary
