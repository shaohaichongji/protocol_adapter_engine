#include <type_traits>

#include "../../../src/config_compiler/schema_ir.h"
#include "../../../src/protocol_plan/plan_builder.h"

static_assert(!std::is_default_constructible_v<pae::config_compiler::ValidatedSchemaIr>);
static_assert(!std::is_copy_constructible_v<pae::config_compiler::ValidatedSchemaIr>);
static_assert(std::is_move_constructible_v<pae::config_compiler::ValidatedSchemaIr>);
static_assert(!std::is_default_constructible_v<pae::config_compiler::BudgetedSchemaIr>);
static_assert(!std::is_copy_constructible_v<pae::config_compiler::BudgetedSchemaIr>);
static_assert(std::is_move_constructible_v<pae::config_compiler::BudgetedSchemaIr>);
static_assert(!std::is_default_constructible_v<pae::protocol_plan::BudgetedPlanDraft>);
static_assert(!std::is_copy_constructible_v<pae::protocol_plan::BudgetedPlanDraft>);
static_assert(std::is_move_constructible_v<pae::protocol_plan::BudgetedPlanDraft>);
