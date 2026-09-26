#include <iostream>
#include <string>
#include <utility>

#include "pae/yaml_frontend.h"

namespace {

int failures = 0;

void Check(bool condition, const char* name) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL " << name << '\n';
  } else {
    std::cout << "PASS " << name << '\n';
  }
}

}  // namespace

int main() {
  const auto limits = pae::yaml::TrialResourceLimitsV01();
  Check(limits.input_bytes == 16U * 1024U && limits.parser_bytes == 128U * 1024U &&
            limits.auxiliary_bytes == 128U * 1024U && limits.json_bytes == 32U * 1024U &&
            limits.nodes == 512U && limits.depth == 16U && limits.scalar_bytes == 4U * 1024U,
        "trial_limits_v01");

  std::string yaml = "a: 1\nnested:\n  value: true\n";
  std::string identity = "synthetic.pae.yaml";
  auto result = pae::yaml::ConvertToStrictJson(yaml, identity);
  yaml.clear();
  identity.clear();
  Check(result.Succeeded() && result.Status() == pae::yaml::ConversionStatus::OK &&
            result.Json().find("\"value\":true") != std::string_view::npos &&
            result.SourceIdentity() == "synthetic.pae.yaml",
        "success_owns_json_and_identity");
  const auto exact = result.FindSource("/nested/value");
  Check(exact.has_value() && exact->value_line == 3U && !exact->ancestor_fallback,
        "exact_source_value");
  const auto nearest = result.FindNearestSource("/nested/missing");
  Check(nearest.has_value() && nearest->ancestor_fallback && nearest->value_approximate &&
            nearest->value_line == 2U,
        "nearest_ancestor_marked");
  Check(!result.FindSource("/nested/missing").has_value(), "missing_exact_not_fabricated");

  auto moved = std::move(result);
  Check(!result.Succeeded() && result.Json().empty() &&
            !result.FindNearestSource("/nested").has_value(),
        "moved_from_safe");
  Check(moved.Succeeded() && moved.SourceIdentity() == "synthetic.pae.yaml",
        "move_preserves_owner");
  auto assigned = pae::yaml::ConvertToStrictJson("other: false\n");
  assigned = std::move(moved);
  Check(assigned.Succeeded() && assigned.FindSource("/nested/value").has_value() &&
            !moved.Succeeded(),
        "move_assignment_releases_and_transfers");

  const auto invalid = pae::yaml::ConvertToStrictJson("a: [\n");
  Check(!invalid.Succeeded() && invalid.Status() != pae::yaml::ConversionStatus::OK &&
            invalid.Json().empty() && invalid.SourceIdentity().empty() &&
            !invalid.FindSource("/a").has_value() && invalid.Reason()[0] != '\0',
        "failed_conversion_has_no_payload");
  const auto oversized = pae::yaml::ConvertToStrictJson(std::string(limits.input_bytes + 1U, 'x'));
  Check(oversized.Status() == pae::yaml::ConversionStatus::INPUT_LIMIT && oversized.Json().empty(),
        "public_input_limit");

  std::cout << "failures=" << failures << '\n';
  return failures == 0 ? 0 : 1;
}
