#pragma once

#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
#include "binary_host_adapter_public.h"
#else

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "../protocol_lab_binary/prepared_binary.h"
#include "description_mapping.h"
#include "ui_field_result.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
#include "../protocol_lab_binary/public_binary_decode.h"
#endif

namespace pae::protocol_lab_ui {

struct BinaryHostBinding {
  std::string endpoint;
  host_endpoint::Action action = host_endpoint::Action::DECODE;
  std::string pipeline_id;
  std::size_t streams = 1U;
};

struct BinaryPreparationIdentity {
  std::uint64_t document = 0U;
  std::uint64_t load = 0U;
  std::uint64_t session = 0U;
  std::uint64_t request = 0U;
  std::string config_sha256;
};

struct BinaryUiDecodeResult {
  protocol_lab_binary::ObservationIdentity identity;
  std::vector<std::uint8_t> frame;
  std::size_t message_index = protocol_core::kInvalidIndex;
  std::string message_id;
  std::vector<UiFieldResult> fields;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiDecodeFailure {
  host_endpoint::Status host_status = host_endpoint::Status::INVALID_ARGUMENT;
  protocol_core::CodecStatus codec_status = protocol_core::CodecStatus::INVALID_ARGUMENT;
  std::vector<std::uint8_t> diagnostic_frame;
  std::optional<std::size_t> message_index;
  std::optional<std::size_t> failed_field_index;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiDecodeView {
  bool ok = false;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  pae::HostOperationResult public_host;
#endif
  host_endpoint::Result host;
  std::optional<BinaryUiDecodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
const char* PublicBinaryCodecStatusName(pae::CodecStatus status) noexcept;
#endif

// Internal Lab-only fault controls. A hook runs only after capacity-upper-bound preflight admits
// its copy.
struct BinaryUiCopyControls {
  std::size_t description_copy_limit = (std::numeric_limits<std::size_t>::max)();
  std::size_t result_copy_limit = (std::numeric_limits<std::size_t>::max)();
  void (*before_description_copy)(void*) = nullptr;
  void (*before_result_copy)(void*) = nullptr;
  void* context = nullptr;
};

class BinaryHostAdapter final {
 public:
  static std::unique_ptr<BinaryHostAdapter> Create(
      config_compiler::CompiledUiArtifacts artifacts, std::vector<BinaryHostBinding> bindings,
      BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
      std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
      std::string& error, const protocol_lab_binary::ResourceLimits& resource_limits = {},
      const BinaryUiCopyControls* copy_controls = nullptr);
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  static std::unique_ptr<BinaryHostAdapter> CreatePublic(
      pae::CompiledProtocol compiled, std::vector<BinaryHostBinding> bindings,
      BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
      std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
      std::string& error, const protocol_lab_binary::ResourceLimits& resource_limits = {});
#endif

  const DocumentDescription& Description() const noexcept { return description_; }
  DocumentDescription TakeDescription() {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) return description_;  // UI publication and public result mapper own copies.
#endif
    return std::move(description_);
  }
  const std::vector<BinaryHostBinding>& Bindings() const noexcept { return bindings_; }
  const BinaryPreparationIdentity& Identity() const noexcept { return identity_; }
  std::size_t UiDescriptionBytes() const noexcept { return ui_description_bytes_; }
  std::size_t AccountedInstanceBytes() const noexcept;
  std::size_t AccountedPreparationBytes() const noexcept;
  std::size_t DescriptionCopyUpperBoundBytes() const noexcept {
    return description_copy_upper_bound_bytes_;
  }
  std::size_t CurrentResultCopyUpperBoundBytes(std::size_t binding, std::size_t flow) const;
  bool SetPresentationRetainedBytes(std::size_t bytes) noexcept;
  std::size_t PresentationRetainedBytes() const noexcept { return presentation_retained_bytes_; }
  std::size_t UiViewReserveBytes() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) return ui_view_reserve_bytes_;
#endif
    return owner_->Resources().ui_view_reserve;
  }
  std::size_t HexPreviewReserveBytes() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) return hex_preview_reserve_bytes_;
#endif
    return owner_->Resources().hex_preview_reserve;
  }
  static std::size_t AccountDescriptionBytes(const DocumentDescription& description);
  std::uint64_t Instance() const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) return public_instance_;
#endif
    return owner_->Instance();
  }
  bool IsCompleteDecode(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t FlowCount(std::size_t binding) const noexcept;
  BinaryUiDecodeView DecodeComplete(std::size_t binding, std::size_t flow,
                                    const std::vector<std::uint8_t>& frame,
                                    std::size_t active_view_bytes = 0U);
  BinaryUiDecodeView MapCurrent(std::size_t binding, std::size_t flow,
                                std::size_t active_view_bytes = 0U) const;
  const void* Current(std::size_t binding, std::size_t flow) const noexcept;
  std::u16string_view Draft(std::size_t binding, std::size_t flow) const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) {
      const auto flat = public_owner_->FlowIndex(binding, flow);
      return flat < public_drafts_.size() ? std::u16string_view(public_drafts_[flat])
                                          : std::u16string_view{};
    }
#endif
    return owner_->Draft(binding, flow);
  }
  std::size_t DraftLimit(std::size_t binding, std::size_t flow) const noexcept {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) return public_limits_.max_frame_bytes * 2U;
#endif
    return owner_->DraftLimit(binding, flow);
  }
  void SaveAndSelect(std::u16string_view draft, std::size_t binding, std::size_t flow) {
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
    if (public_owner_) {
      const auto flat = public_owner_->FlowIndex(binding, flow);
      if (flat >= public_drafts_.size()) throw std::out_of_range("Binary flow out of range");
      std::string ascii;
      ascii.reserve(draft.size());
      for (auto unit : draft) {
        if (unit > 0x7FU) throw std::invalid_argument("Binary Hex draft is not ASCII");
        ascii.push_back(static_cast<char>(unit));
      }
      if (!public_owner_->SetDraft(flat, ascii))
        throw std::length_error("Binary draft exceeds public flow budget");
      public_drafts_[flat].assign(draft);
      public_selected_binding_ = binding;
      public_selected_flow_ = flow;
      return;
    }
#endif
    owner_->SaveAndSelect(draft, binding, flow);
  }
  protocol_lab_binary::Selection Selected() const noexcept { return owner_->Selected(); }

 private:
  BinaryUiDecodeView MapObserved(const protocol_lab_binary::ObservedOperation& operation,
                                 std::size_t binding, std::size_t flow,
                                 std::size_t active_view_bytes) const;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  BinaryUiDecodeView MapPublic(
      const protocol_lab_binary::public_decode::Operation& operation,
      std::size_t active_view_bytes) const;
  std::size_t PublicResultCopyUpperBound(
      const protocol_lab_binary::public_decode::Operation& operation) const;
#endif
  std::unique_ptr<protocol_lab_binary::PreparedBinary> owner_;
  DocumentDescription description_;
  std::vector<BinaryHostBinding> bindings_;
  BinaryPreparationIdentity identity_;
  std::size_t ui_description_bytes_ = 0U;
  std::size_t externally_retained_bytes_ = 0U;
  std::size_t preparation_overhead_bytes_ = 0U;
  std::size_t description_copy_upper_bound_bytes_ = 0U;
  std::size_t presentation_retained_bytes_ = 0U;
  BinaryUiCopyControls copy_controls_;
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  std::unique_ptr<protocol_lab_binary::public_decode::Adapter> public_owner_;
  protocol_lab_binary::public_decode::Limits public_limits_;
  std::vector<std::u16string> public_drafts_;
  std::uint64_t public_instance_ = 0U;
  std::size_t public_selected_binding_ = 0U;
  std::size_t public_selected_flow_ = 0U;
  std::size_t ui_view_reserve_bytes_ = 0U;
  std::size_t hex_preview_reserve_bytes_ = 0U;
#endif
};

}  // namespace pae::protocol_lab_ui
#endif  // PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2
