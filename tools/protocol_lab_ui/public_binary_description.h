#pragma once

#include <string>

#include "pae/compiler.h"

namespace pae::protocol_lab_ui {

struct DocumentDescription;

// Copies only public metadata and P physical-query values. No Plan or private sidecar access.
// 将 Schema 0.9 Binary 公开元数据和物理事实复制成自有描述，不编译、不执行 Codec。
// 描述中的最大范围不是本次执行实际范围；成功才替换 output，失败保留原 output。
bool BuildPublicBinaryDescription(const pae::CompiledProtocol& compiled,
                                  DocumentDescription& output, std::string& error);

}  // namespace pae::protocol_lab_ui
