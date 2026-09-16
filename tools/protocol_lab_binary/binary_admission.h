#pragma once

#include "candidate_materializer.h"

namespace pae::protocol_lab_binary {

// Allocation-free acceptance scan of the entire compiled document, before creating a Host
// Session. Does not replace Compiler validation or account for a full adapter/UI instance.
void ValidateBinaryAdmission(const protocol_plan::PlanBundle& plan, const Limits& limits = {});

}  // namespace pae::protocol_lab_binary
