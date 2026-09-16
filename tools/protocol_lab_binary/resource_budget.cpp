#include "resource_budget.h"

#include <limits>

#include "candidate_materializer.h"

namespace pae::protocol_lab_binary {
namespace {
void Require(bool condition) {
  if (!condition) throw MaterializationError("Binary instance/replacement budget exceeded");
}
std::size_t Add(std::size_t a, std::size_t b) {
  Require(b <= std::numeric_limits<std::size_t>::max() - a);
  return a + b;
}
std::size_t Mul(std::size_t a, std::size_t b) {
  Require(a == 0U || b <= std::numeric_limits<std::size_t>::max() / a);
  return a * b;
}
}  // namespace
ResourceReport CalculateResourceReport(const ResourceInputs& input, const ResourceLimits& limits,
                                       std::size_t old_instance_bytes) {
  const ResourceLimits hard;
  Require(limits.instance_bytes <= hard.instance_bytes &&
          limits.replacement_bytes <= hard.replacement_bytes && input.channels <= 64U &&
          input.decode_channels <= input.channels &&
          input.encode_channels == input.channels - input.decode_channels &&
          input.candidate_limit <= Limits{}.max_total_bytes &&
          old_instance_bytes <= hard.instance_bytes);
  ResourceReport out;
  out.measured = input;
  out.resident_bytes = Add(Add(Add(input.owner_bytes, input.plan_bytes), input.session_bytes),
                           Add(input.description_bytes, input.binding_bytes));
  // One saved result per logical channel, and one in-flight copy for serialized operations.
  out.retained_dto_reserve = Mul(input.channels, input.candidate_limit);
  out.callback_reserve = input.candidate_limit;
  constexpr std::size_t chunk = 65536U;
  out.frozen_reserve = Mul(input.decode_channels, chunk);
  // Worst escaped byte = four UTF16 code units, two copies (draft + switching temporary).
  out.utf16_draft_reserve = Mul(input.channels, Mul(2U, Mul(Add(Mul(4U, chunk), 1U), 2U)));
  // One active view and a temporary replacement. Hidden channels use saved DTOs above.
  out.ui_view_reserve = Mul(2U, input.candidate_limit);
  out.hex_preview_reserve = Mul(2U, Mul(Add(Mul(3U, chunk), 1U), 2U));
  // Future persistent Encode editor: typed input copy plus output frame. Current owned TX
  // results and resolved call arguments are bounded by retained_dto/callback reserves above.
  out.encode_reserve = Mul(input.encode_channels, Add(input.candidate_limit, chunk));
  auto reserved = Add(Add(out.retained_dto_reserve, out.callback_reserve),
                      Add(out.frozen_reserve, out.utf16_draft_reserve));
  reserved =
      Add(reserved, Add(Add(out.ui_view_reserve, out.hex_preview_reserve), out.encode_reserve));
  out.instance_admission_bytes = Add(out.resident_bytes, reserved);
  out.preparation_peak_bytes = Add(out.instance_admission_bytes, input.source_sidecar_bytes);
  out.replacement_peak_bytes = Add(old_instance_bytes, out.preparation_peak_bytes);
  Require(out.instance_admission_bytes <= limits.instance_bytes &&
          out.preparation_peak_bytes <= limits.instance_bytes &&
          out.replacement_peak_bytes <= limits.replacement_bytes);
  return out;
}
}  // namespace pae::protocol_lab_binary
