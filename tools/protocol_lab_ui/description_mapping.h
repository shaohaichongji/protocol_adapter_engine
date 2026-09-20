#pragma once

#include <cstddef>
#include <string>

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
#include "../../src/config_compiler/ui_description.h"
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

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
bool BuildDocumentDescription(const protocol_plan::PlanBundle& plan,
                              const config_compiler::UiDescriptionSidecar& sidecar,
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

std::string FormatPhysicalLocation(const FieldDescriptor& field);
std::optional<ByteRange> ResolveActualFieldRange(const MessageDescriptor& message,
                                                 const FieldDescriptor& field,
                                                 std::size_t actual_frame_size) noexcept;
std::optional<ByteRange> ResolveActualIntegrityStorage(const MessageDescriptor& message,
                                                       std::size_t actual_frame_size) noexcept;
std::string FormatPhysicalLocation(const MessageDescriptor& message, const FieldDescriptor& field,
                                   std::optional<std::size_t> actual_frame_size);

}  // namespace pae::protocol_lab_ui
