#pragma once

#include <string>

#include "lab_types.h"

namespace pae::protocol_lab {

std::string CommandName(Command command);
void PrintUsage();
bool ParseArguments(int argc, char** argv, Arguments& output, bool& early_success);

}  // namespace pae::protocol_lab
