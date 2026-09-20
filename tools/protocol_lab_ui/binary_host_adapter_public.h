#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../protocol_lab_binary/public_binary_decode.h"
#include "owned_presentation_types.h"
#include "pae/host_endpoint.h"
#include "ui_field_result.h"

namespace pae::protocol_lab_ui {

struct DocumentDescription;

struct BinaryHostBinding {
  std::string endpoint;
  pae::HostAction action = pae::HostAction::DECODE;
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
  std::vector<std::uint8_t> frame;
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<UiFieldResult> fields;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiDecodeFailure {
  pae::HostStatus host_status = pae::HostStatus::INVALID_ARGUMENT;
  pae::CodecStatus codec_status = pae::CodecStatus::INVALID_ARGUMENT;
  std::vector<std::uint8_t> diagnostic_frame;
  std::optional<std::size_t> message_index;
  std::optional<std::size_t> failed_field_index;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiDecodeView {
  bool ok = false;
  pae::HostOperationResult public_host;
  std::optional<BinaryUiDecodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};

enum class BinaryStreamPresentationStatus {
  PREFLIGHT_REJECTED,
  NO_CANDIDATE,
  DECODE_SUCCESS,
  DECODE_FAILURE,
  MATERIALIZATION_FAILURE,
};

struct BinaryUiStreamView {
  BinaryStreamPresentationStatus status = BinaryStreamPresentationStatus::PREFLIGHT_REJECTED;
  protocol_lab_binary::public_decode::LocalStatus local_status =
      protocol_lab_binary::public_decode::LocalStatus::INVALID_INPUT;
  protocol_lab_binary::public_decode::StreamDiagnostic diagnostic =
      protocol_lab_binary::public_decode::StreamDiagnostic::NOT_STREAM;
  bool host_called = false;
  pae::HostOperationResult public_host;
  StreamPresentationObservation before;
  StreamPresentationObservation after;
  std::optional<BinaryUiDecodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};

struct BinaryUiEncodeResult {
  std::vector<std::uint8_t> frame;
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<UiFieldResult> fields;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiEncodeView {
  bool ok = false;
  pae::HostOperationResult public_host;
  std::optional<BinaryUiEncodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};

struct BinaryUiCopyControls {
  std::size_t description_copy_limit = (std::numeric_limits<std::size_t>::max)();
  std::size_t result_copy_limit = (std::numeric_limits<std::size_t>::max)();
  void (*before_description_copy)(void*) = nullptr;
  void (*before_result_copy)(void*) = nullptr;
  void* context = nullptr;
};

const char* PublicBinaryCodecStatusName(pae::CodecStatus status) noexcept;

// Binary H2 application adapter. Owns one public H1/Host instance, never a private Plan/Session.
class BinaryHostAdapter final {
 public:
  static std::unique_ptr<BinaryHostAdapter> CreatePublic(
      pae::CompiledProtocol compiled, std::vector<BinaryHostBinding> bindings,
      BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
      std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
      std::string& error, const protocol_lab_binary::public_decode::Limits& limits = {},
      const BinaryUiCopyControls* copy_controls = nullptr);
  BinaryHostAdapter(const BinaryHostAdapter&) = delete;
  BinaryHostAdapter& operator=(const BinaryHostAdapter&) = delete;
  ~BinaryHostAdapter();

  const DocumentDescription& Description() const noexcept;
  DocumentDescription TakeDescription();
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
  std::size_t UiViewReserveBytes() const noexcept { return ui_view_reserve_bytes_; }
  std::size_t HexPreviewReserveBytes() const noexcept { return hex_preview_reserve_bytes_; }
  static std::size_t AccountDescriptionBytes(const DocumentDescription& description);
  std::uint64_t Instance() const noexcept { return instance_; }
  bool IsCompleteDecode(std::size_t binding, std::size_t flow) const noexcept;
  bool IsStreamDecode(std::size_t binding, std::size_t flow) const noexcept;
  bool IsCompleteEncode(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t FlowCount(std::size_t binding) const noexcept;
  BinaryUiDecodeView DecodeComplete(std::size_t binding, std::size_t flow,
                                    const std::vector<std::uint8_t>& frame,
                                    std::size_t active_view_bytes = 0U);
  BinaryUiDecodeView MapCurrent(std::size_t binding, std::size_t flow,
                                std::size_t active_view_bytes = 0U) const;
  BinaryUiStreamView SubmitStream(std::size_t binding, std::size_t flow,
                                  const std::vector<std::uint8_t>& chunk,
                                  std::size_t active_view_bytes = 0U);
  BinaryUiStreamView ContinueStream(std::size_t binding, std::size_t flow,
                                    std::size_t active_view_bytes = 0U);
  BinaryUiStreamView MapCurrentStream(std::size_t binding, std::size_t flow,
                                      std::size_t active_view_bytes = 0U);
  std::optional<StreamPresentationObservation> ObserveStream(std::size_t binding,
                                                             std::size_t flow) const noexcept;
  bool StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept;
  pae::HostStatus ResetStream(std::size_t binding, std::size_t flow) noexcept;
  BinaryUiEncodeView EncodeComplete(
      std::size_t binding, std::size_t flow, std::size_t message_index,
      const std::vector<protocol_lab_binary::public_decode::EncodeInput>& inputs,
      std::size_t active_view_bytes = 0U);
  BinaryUiEncodeView MapCurrentEncode(std::size_t binding, std::size_t flow,
                                      std::size_t active_view_bytes = 0U) const;
  const void* Current(std::size_t binding, std::size_t flow) const noexcept;
  std::u16string_view Draft(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t DraftLimit(std::size_t binding, std::size_t flow) const noexcept;
  void SaveAndSelect(std::u16string_view draft, std::size_t binding, std::size_t flow);
  bool SaveDraftsAndSelect(std::u16string_view inspect_draft,
                           const std::unordered_map<std::size_t, TypedDraft>& typed_drafts,
                           std::size_t source_message_index, std::size_t binding, std::size_t flow);
  bool SaveTypedDrafts(std::size_t binding, std::size_t flow,
                       const std::unordered_map<std::size_t, TypedDraft>& drafts);
  const std::unordered_map<std::size_t, TypedDraft>& TypedDrafts(std::size_t binding,
                                                                 std::size_t flow) const noexcept;
  std::optional<std::size_t> MessageSelection(std::size_t binding, std::size_t flow) const noexcept;
  bool SelectEncodeMessage(std::size_t binding, std::size_t flow,
                           std::size_t message_index) noexcept;
  void ClearCurrentEncode(std::size_t binding, std::size_t flow) noexcept;

 private:
  BinaryHostAdapter();
  std::size_t ResultCopyUpperBound(
      const protocol_lab_binary::public_decode::Operation& operation) const;
  std::size_t ResultCopyUpperBound(
      const pae::HostOperationResult& host,
      const std::optional<protocol_lab_binary::public_decode::Candidate>& candidate) const;
  BinaryUiDecodeView MapCandidate(
      const pae::HostOperationResult& host,
      const std::optional<protocol_lab_binary::public_decode::Candidate>& candidate,
      std::size_t active_view_bytes, std::size_t input_peak_bytes) const;
  BinaryUiDecodeView MapOperation(const protocol_lab_binary::public_decode::Operation& operation,
                                  std::size_t active_view_bytes) const;
  BinaryUiStreamView MapStreamStep(const protocol_lab_binary::public_decode::StreamStep& step,
                                   std::size_t active_view_bytes,
                                   std::size_t input_peak_bytes) const;
  BinaryUiStreamView ProjectStreamMappingFailure(
      const protocol_lab_binary::public_decode::StreamStep& step) const noexcept;
  BinaryUiStreamView StreamMappingFailure(
      const protocol_lab_binary::public_decode::StreamStep& step, std::size_t binding,
      std::size_t flow) noexcept;
  BinaryUiEncodeView MapEncodeOperation(
      const protocol_lab_binary::public_decode::Operation& operation,
      std::size_t active_view_bytes) const;
  std::unique_ptr<DocumentDescription> description_;
  std::unique_ptr<DocumentDescription> publication_description_;
  std::unique_ptr<protocol_lab_binary::public_decode::Adapter> owner_;
  std::vector<BinaryHostBinding> bindings_;
  std::vector<std::u16string> drafts_;
  std::vector<std::unordered_map<std::size_t, TypedDraft>> typed_drafts_;
  std::vector<std::optional<std::size_t>> message_selections_;
  std::vector<std::uint8_t> stream_mapping_faulted_;
  std::size_t selected_binding_ = 0U;
  std::size_t selected_flow_ = 0U;
  BinaryPreparationIdentity identity_;
  protocol_lab_binary::public_decode::Limits limits_;
  BinaryUiCopyControls copy_controls_;
  std::uint64_t instance_ = 0U;
  std::size_t ui_description_bytes_ = 0U;
  std::size_t description_copy_upper_bound_bytes_ = 0U;
  std::size_t externally_retained_bytes_ = 0U;
  std::size_t preparation_coexisting_bytes_ = 0U;
  std::size_t presentation_retained_bytes_ = 0U;
  std::size_t ui_view_reserve_bytes_ = 32U * 1024U * 1024U;
  std::size_t hex_preview_reserve_bytes_ = 4U * 1024U * 1024U;
  std::size_t utf16_draft_reserve_bytes_ = 0U;
};

}  // namespace pae::protocol_lab_ui
