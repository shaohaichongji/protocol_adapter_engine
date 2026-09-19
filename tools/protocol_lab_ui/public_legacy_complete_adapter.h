#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "owned_presentation_types.h"
#include "pae/codec.h"
#include "pae/compiler.h"
#include "ui_field_result.h"

namespace pae::protocol_lab_ui {

struct PublicLegacyCompleteLimits {
  std::size_t max_frame_bytes = 65536U;
  std::size_t max_fields = 1024U;
  std::size_t max_field_bytes = 1024U * 1024U;
  std::size_t max_description_bytes = 4U * 1024U * 1024U;
  std::size_t max_result_bytes = 8U * 1024U * 1024U;
  std::size_t instance_bytes = 128U * 1024U * 1024U;
  std::size_t replacement_bytes = 256U * 1024U * 1024U;
};

enum class PublicLegacyLocalStatus {
  OK,
  INVALID_INPUT,
  RESOURCE_LIMIT,
  PREPARATION_FAILED,
  CODEC_FAILED,
  REVIEW_FAILED,
  MATERIALIZATION_FAILED,
  ALLOCATION_FAILED,
};

enum class PublicLegacyPreparationError {
  NONE,
  INVALID_COMPILED,
  DESCRIPTION_INVALID,
  DESCRIPTION_LIMIT,
  INSTANCE_LIMIT,
  CODEC_PROBE_FAILED,
  CODEC_CREATE_FAILED,
  ALLOCATION_FAILED,
  INTERNAL_EXCEPTION,
};

struct PublicLegacyInput {
  std::size_t field_index = 0U;
  TypedDraft value;
};

struct PublicLegacyOperation {
  PublicLegacyLocalStatus local_status = PublicLegacyLocalStatus::INVALID_INPUT;
  pae::CodecStatus codec_status = pae::CodecStatus::INVALID_ARGUMENT;
  pae::CodecStatus review_status = pae::CodecStatus::INVALID_ARGUMENT;
  bool codec_called = false;
  bool review_decode_called = false;
  std::optional<std::size_t> message_index;
  std::string message_id;
  std::optional<std::size_t> failed_value_index;
  std::optional<std::size_t> failed_field_index;
  std::string failed_field_id;
  pae::ConversionError conversion_error = pae::ConversionError::NONE;
  std::vector<std::uint8_t> frame;
  std::vector<UiFieldResult> fields;
  std::size_t accounted_bytes = 0U;

  bool ok() const noexcept { return local_status == PublicLegacyLocalStatus::OK; }
};

class PublicLegacyCompleteAdapter final {
 public:
  struct Preparation {
    PublicLegacyLocalStatus status = PublicLegacyLocalStatus::PREPARATION_FAILED;
    PublicLegacyPreparationError error = PublicLegacyPreparationError::NONE;
    pae::CodecStatus codec_status = pae::CodecStatus::INVALID_ARGUMENT;
    std::unique_ptr<PublicLegacyCompleteAdapter> adapter;
    DocumentDescription description;
    std::string detail;
  };

  static Preparation AdoptCompiled(pae::CompiledProtocol compiled,
                                   const PublicLegacyCompleteLimits& limits = {},
                                   std::size_t previous_instance_bytes = 0U) noexcept;
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
  static void FailNextDescriptionCopyAllocationForTesting() noexcept;
  static void FailNextPreparationDiagnosticAllocationForTesting() noexcept;
#endif

  PublicLegacyCompleteAdapter(const PublicLegacyCompleteAdapter&) = delete;
  PublicLegacyCompleteAdapter& operator=(const PublicLegacyCompleteAdapter&) = delete;
  ~PublicLegacyCompleteAdapter();

  PublicLegacyOperation Decode(std::size_t pipeline_index,
                               const std::vector<std::uint8_t>& frame) noexcept;
  PublicLegacyOperation Encode(std::size_t pipeline_index, std::size_t message_index,
                               const std::vector<PublicLegacyInput>& inputs) noexcept;

  bool EncodeAvailable(std::size_t pipeline_index, std::size_t message_index) const noexcept;
  bool DecodeAvailable(std::size_t pipeline_index) const noexcept;
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
  void FailNextResultAllocationForTesting() noexcept { fail_next_result_allocation_ = true; }
#endif

  std::size_t AccountedInstanceBytes() const noexcept { return instance_bytes_; }
  std::size_t DescriptionAccountedBytes() const noexcept { return description_bytes_; }

 private:
  PublicLegacyCompleteAdapter() = default;

  PublicLegacyOperation Materialize(const pae::DecodeResult& decoded,
                                    const std::vector<std::uint8_t>& frame,
                                    bool review);

  pae::CompiledProtocol compiled_;
  std::unique_ptr<pae::CompleteRecordCodec> codec_;
  std::unique_ptr<pae::CompleteRecordCodec> review_codec_;
  PublicLegacyCompleteLimits limits_;
  std::size_t instance_bytes_ = 0U;
  std::size_t description_bytes_ = 0U;
#if defined(PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION)
  bool fail_next_result_allocation_ = false;
#endif
};

const char* PublicLegacyCodecStatusName(pae::CodecStatus status) noexcept;
const char* PublicLegacyConversionErrorName(pae::ConversionError error) noexcept;

}  // namespace pae::protocol_lab_ui
