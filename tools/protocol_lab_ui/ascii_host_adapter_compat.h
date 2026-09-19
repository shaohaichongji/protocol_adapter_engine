#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../protocol_lab_ascii/host_observer_adapter.h"
#include "ascii_host_adapter.h"
#include "ascii_host_types_compat.h"

namespace pae::protocol_lab_ui {

std::unique_ptr<AsciiHostAdapter> CreatePrivateAsciiHostAdapter(
    config_compiler::CompiledUiArtifacts artifacts, std::vector<AsciiHostBinding> bindings,
    std::string& error);

}  // namespace pae::protocol_lab_ui
