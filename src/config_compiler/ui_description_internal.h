#pragma once

#include <cstddef>
#include <optional>

#include "config_compiler.h"
#include "schema_ir.h"

namespace pae::config_compiler {

struct UiDescriptionTestProbe {
  std::size_t layout_count = 0U;
  std::size_t storage_allocation_count = 0U;
  std::size_t metadata_copy_count = 0U;
  std::size_t plan_freeze_count = 0U;
  std::size_t index_audit_count = 0U;
  std::size_t live_storage_count = 0U;
  std::size_t live_accounted_bytes = 0U;
  std::size_t released_storage_count = 0U;
  std::size_t released_accounted_bytes = 0U;
  bool fail_if_touched = false;
  bool fail_storage_allocation = false;
  bool fail_plan_freeze = false;
  bool fail_index_audit = false;
  // The scoped probe must outlive every sidecar produced while lifetime tracking is enabled.
  bool track_storage_lifetime = false;
};

class ScopedUiDescriptionTestProbe final {
 public:
  explicit ScopedUiDescriptionTestProbe(UiDescriptionTestProbe& probe) noexcept;
  ScopedUiDescriptionTestProbe(const ScopedUiDescriptionTestProbe&) = delete;
  ScopedUiDescriptionTestProbe& operator=(const ScopedUiDescriptionTestProbe&) = delete;
  ~ScopedUiDescriptionTestProbe();

 private:
  UiDescriptionTestProbe* previous_ = nullptr;
};

struct UiDescriptionLayoutTestInput {
  std::size_t pipeline_count = 0U;
  std::size_t message_count = 0U;
  std::size_t field_count = 0U;
  std::size_t enum_count = 0U;
  std::size_t string_bytes = 0U;
};

bool EstimateUiDescriptionLayoutForTest(const UiDescriptionLayoutTestInput& input,
                                        DescriptionMemoryReport& report) noexcept;

// Test-only injection at the UI compile call boundary immediately before Plan Freeze.
bool UiDescriptionPlanFreezeAllowedForTest(CompileDiagnostic& diagnostic);

class UiDescriptionBuilder final {
 public:
  UiDescriptionBuilder() = delete;
  static bool Build(const BudgetedSchemaIr& budgeted, std::size_t memory_limit_bytes,
                    UiDescriptionSidecar& sidecar, CompileDiagnostic& diagnostic);
  static bool Audit(const protocol_plan::PlanBundle& plan, const UiDescriptionSidecar& sidecar,
                    CompileDiagnostic& diagnostic);
};

}  // namespace pae::config_compiler
