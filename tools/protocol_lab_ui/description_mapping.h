#pragma once

#include <cstddef>
#include <string>

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
#include "../../src/config_compiler/protocol_metadata.h"
#include "../../src/protocol_plan/plan_bundle.h"
#endif
#include "owned_presentation_types.h"
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY) && \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER)
#include "../protocol_lab_ascii/ascii_offline_adapter.h"
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
#include "../protocol_lab_ascii/public_ascii_offline_adapter.h"
#endif

namespace pae::protocol_lab_ui {

// 将编译侧描述复制为 Lab 自有展示数据，不执行 Codec；私有 Plan/legacy ASCII 入口受宏隔离。
// public-only 路径不读取私有 Plan；公开 Binary 描述在 public_binary_description 中构造。
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
bool BuildDocumentDescription(const protocol_plan::PlanBundle& plan,
                              const config_compiler::ProtocolMetadataStorage& sidecar,
                              DocumentDescription& output, std::string& error);
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER) || \
    defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
bool BuildDocumentDescription(const protocol_lab::ascii::DocumentDescription& source,
                              DocumentDescription& output, std::string& error);
#endif
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
bool BuildDocumentDescription(const protocol_lab_ascii::public_offline::OwnedDescription& source,
                              DocumentDescription& output, std::string& error);
#endif

// 格式化已知物理事实，不重新解释协议：byte 从 0 起，字节内位为 LSB0。
std::string FormatPhysicalLocation(const FieldDescriptor& field);
// 只按 bounded_payload 解析对应载荷字段；其他字段返回描述范围，不是通用动态布局解析器。
std::optional<ByteRange> ResolveActualFieldRange(const MessageDescriptor& message,
                                                 const FieldDescriptor& field,
                                                 std::size_t actual_frame_size) noexcept;
// 动态尾校验位置随实际载荷末尾移动；无可用布局时返回空，不进行完整性校验。
std::optional<ByteRange> ResolveActualIntegrityStorage(const MessageDescriptor& message,
                                                       std::size_t actual_frame_size) noexcept;
std::string FormatPhysicalLocation(const MessageDescriptor& message, const FieldDescriptor& field,
                                   std::optional<std::size_t> actual_frame_size);

}  // namespace pae::protocol_lab_ui
