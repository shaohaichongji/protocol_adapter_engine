#include <utility>

#include "../../../src/config_compiler/schema_ir.h"

auto BypassDomainValidator() {
  pae::config_compiler::SchemaIr schema;
  return pae::config_compiler::BudgetedSchemaIr{std::move(schema)};
}
