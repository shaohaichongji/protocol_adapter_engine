#pragma once

#include <string_view>

namespace pae::protocol_lab_ui {

enum class LabExecutionPhase {
  PREPARATION,
  STRUCTURAL_QUERY,
  MAIN_CODEC,
  REVIEW_DECODE,
  RESULT_MAPPING,
};

class LabExecutionObserver {
 public:
  virtual ~LabExecutionObserver() = default;
  virtual void PhaseStarted(LabExecutionPhase phase) = 0;
  virtual void PhaseFinished(LabExecutionPhase phase, std::string_view status) = 0;
};

}  // namespace pae::protocol_lab_ui
