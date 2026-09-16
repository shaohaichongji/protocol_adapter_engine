#pragma once

#include <string>

#include "pae/compiler.h"

namespace pae::protocol_lab_ui {

struct DocumentDescription;

// Copies only public metadata and P physical-query values. No Plan or private sidecar access.
bool BuildPublicBinaryDescription(const pae::CompiledProtocol& compiled,
                                  DocumentDescription& output, std::string& error);

}  // namespace pae::protocol_lab_ui
