#pragma once

#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "plan_bundle.h"

namespace pae::protocol_plan {

inline constexpr std::size_t kInvalidPlanBuildIndex = (std::numeric_limits<std::size_t>::max)();

enum class PlanBuildError {
  NONE,
  INVALID_METADATA,
  RESOURCE_LIMIT_EXCEEDED,
  RESOURCE_REQUIREMENTS_MISMATCH,
  INVALID_FRAMING_PLAN,
  INVALID_PIPELINE_PLAN,
  INVALID_MESSAGE_PLAN,
  INVALID_MATCHER_PLAN,
  INVALID_FIELD_PLAN,
  FRAME_NOT_FULLY_DEFINED,
  AMBIGUOUS_MATCHER,
  ALLOCATION_FAILED,
  INTERNAL_ERROR,
};

struct PlanBuildDiagnostic {
  PlanBuildError code = PlanBuildError::INTERNAL_ERROR;
  std::size_t framing_index = kInvalidPlanBuildIndex;
  std::size_t pipeline_index = kInvalidPlanBuildIndex;
  std::size_t message_index = kInvalidPlanBuildIndex;
  std::size_t matcher_index = kInvalidPlanBuildIndex;
  std::size_t field_index = kInvalidPlanBuildIndex;
};

struct PlanDraft {
  std::string schema_version;
  std::string protocol_id;
  std::string protocol_version;
  ResourceProfile resource_profile = ResourceProfile::DESKTOP;
  ResourceRequirements resource_requirements;
  std::vector<FramingPlan> framing_profiles;
  std::vector<PipelinePlan> pipelines;
  std::vector<MessagePlan> messages;
};

struct PlanBuildResult {
  std::unique_ptr<const PlanBundle> plan;
  std::optional<PlanBuildDiagnostic> diagnostic;

  bool Succeeded() const noexcept { return plan != nullptr && !diagnostic.has_value(); }
};

class PlanBuilder final {
 public:
  [[nodiscard]] static PlanBuildResult Freeze(PlanDraft draft) noexcept;

 private:
  static PlanBuildResult FreezeImpl(PlanDraft draft);
};

}  // namespace pae::protocol_plan
