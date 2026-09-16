#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

#include "pae/export.h"
#include "pae/protocol_description.h"

namespace pae {

class CompiledProtocol;
class HostEndpoint;

enum class CodecStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_COMPILED_PROTOCOL,
  RESOURCE_LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  WORKSPACE_BUSY,
  INPUT_VALUES_TOO_MANY,
  UNKNOWN_MESSAGE,
  AMBIGUOUS_MESSAGE,
  OUTPUT_SLOTS_TOO_SMALL,
  INTEGRITY_FAILED,
  MESSAGE_NOT_ALLOWED,
  FIELD_REFERENCE_MISMATCH,
  DUPLICATE_FIELD,
  MISSING_FIELD,
  TYPE_MISMATCH,
  VALUE_NOT_REPRESENTABLE,
  BYTES_LENGTH_MISMATCH,
  UNKNOWN_ENUM_VALUE,
  ENUM_REFERENCE_MISMATCH,
  CONSTANT_FIELD_OVERRIDE,
  INPUT_OUTPUT_OVERLAP,
  BUFFER_TOO_SMALL,
  FINAL_REVIEW_FAILED,
  ASCII_CHARACTER_NOT_ALLOWED,
  ASCII_TERMINATOR_CONFLICT,
  OPERATION_NOT_SUPPORTED,
  COMPUTED_FIELD_OVERRIDE,
  LENGTH_MISMATCH,
  INTERNAL_ERROR,
};

enum class ConversionError {
  NONE,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
};

enum class RawIntegerKind { UINT64, INT64 };

struct ByteView {
  const std::uint8_t* data = nullptr;
  std::size_t size = 0U;
};

struct MutableByteBuffer {
  std::uint8_t* data = nullptr;
  std::size_t capacity = 0U;
};

struct Decimal64 {
  std::int64_t coefficient = 0;
  std::int32_t scale = 0;
};

struct FieldSelector {
  std::size_t message_index = static_cast<std::size_t>(-1);
  std::size_t field_index = static_cast<std::size_t>(-1);
};

struct EnumSelector {
  std::size_t message_index = static_cast<std::size_t>(-1);
  std::size_t field_index = static_cast<std::size_t>(-1);
  std::size_t entry_index = static_cast<std::size_t>(-1);
};

class PAE_API EncodeValue final {
 public:
  static EncodeValue UInt64(FieldSelector field, std::uint64_t value) noexcept;
  static EncodeValue Int64(FieldSelector field, std::int64_t value) noexcept;
  static EncodeValue Bool(FieldSelector field, bool value) noexcept;
  static EncodeValue Bytes(FieldSelector field, ByteView value) noexcept;
  static EncodeValue Enum(EnumSelector value) noexcept;
  static EncodeValue Decimal(FieldSelector field, Decimal64 value) noexcept;

 private:
  EncodeValue() noexcept = default;

  FieldSelector field_;
  EnumSelector enum_;
  ValueKind kind_ = ValueKind::UINT64;
  std::uint64_t uint64_value_ = 0U;
  std::int64_t int64_value_ = 0;
  bool bool_value_ = false;
  ByteView bytes_value_;
  Decimal64 decimal64_value_;

  friend class CompleteRecordCodec;
};

inline constexpr std::size_t kUsePlanDecodedFieldCapacity = 0U;
inline constexpr std::size_t kUseDefaultExecutionMemoryLimit = static_cast<std::size_t>(-1);

struct CompleteRecordCodecOptions {
  std::size_t decoded_field_capacity = kUsePlanDecodedFieldCapacity;
  std::size_t execution_memory_limit_bytes = kUseDefaultExecutionMemoryLimit;
};

struct ExecutionMemoryReport {
  std::size_t core_workspace_bytes = 0U;
  std::size_t decoded_slot_bytes = 0U;
  std::size_t encode_mapping_bytes = 0U;
  std::size_t facade_bytes = 0U;
  std::size_t codec_accounted_total_bytes = 0U;
  std::size_t retained_compiled_state_facade_bytes = 0U;
  std::size_t allocation_count = 0U;
};

class CompleteRecordCodec;
struct CompleteRecordCodecCreateResult;

class PAE_API DecodedFieldView final {
 public:
  DecodedFieldView() noexcept = default;

  [[nodiscard]] bool HasValue() const noexcept;
  [[nodiscard]] std::size_t FlatFieldIndex() const noexcept;
  [[nodiscard]] FieldSelector Field() const noexcept;
  [[nodiscard]] ValueKind Kind() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> UInt64() const noexcept;
  [[nodiscard]] std::optional<std::int64_t> Int64() const noexcept;
  [[nodiscard]] std::optional<bool> Bool() const noexcept;
  // On a successful ASCII Decode, BYTES are a contiguous slice of this call's input. For a
  // nonempty input, even a zero-length field points at its real offset (possibly one-past-end).
  // The view expires with this Codec result and additionally borrows the unchanged input bytes.
  [[nodiscard]] std::optional<ByteView> Bytes() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> EnumRawValue() const noexcept;
  [[nodiscard]] std::optional<std::size_t> KnownEnumFlatIndex() const noexcept;
  [[nodiscard]] std::optional<Decimal64> Decimal() const noexcept;
  [[nodiscard]] std::optional<RawIntegerKind> ConversionRawKind() const noexcept;
  [[nodiscard]] std::optional<std::uint64_t> ConversionRawUInt64() const noexcept;
  [[nodiscard]] std::optional<std::int64_t> ConversionRawInt64() const noexcept;

 private:
  DecodedFieldView(const CompleteRecordCodec* owner, std::uint64_t owner_epoch,
                   std::uint64_t generation, std::size_t ordinal) noexcept;

  const CompleteRecordCodec* owner_ = nullptr;
  std::uint64_t owner_epoch_ = 0U;
  std::uint64_t generation_ = 0U;
  std::size_t ordinal_ = 0U;

  friend class DecodedRecordView;
};

class PAE_API DecodedRecordView final {
 public:
  DecodedRecordView() noexcept = default;

  [[nodiscard]] bool HasValue() const noexcept;
  [[nodiscard]] std::size_t MessageIndex() const noexcept;
  [[nodiscard]] std::size_t FieldCount() const noexcept;
  [[nodiscard]] std::optional<DecodedFieldView> Field(std::size_t ordinal) const noexcept;

 private:
  DecodedRecordView(const CompleteRecordCodec* owner, std::uint64_t owner_epoch,
                    std::uint64_t generation) noexcept;

  const CompleteRecordCodec* owner_ = nullptr;
  std::uint64_t owner_epoch_ = 0U;
  std::uint64_t generation_ = 0U;

  friend class CompleteRecordCodec;
};

struct DecodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  DecodedRecordView record;
  // Unique structural match from this call, not a success marker. Pre-match errors, unknown and
  // ambiguous frames leave it empty; post-match failure never publishes a usable record.
  std::optional<std::size_t> matched_message_index;
  std::size_t required_field_count = 0U;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  bool output_tainted = false;
};

struct EncodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t bytes_written = 0U;
  std::size_t required_size = 0U;
  std::optional<std::size_t> failed_value_index;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
};

class PAE_API CompleteRecordCodec final {
 public:
  CompleteRecordCodec(const CompleteRecordCodec&) = delete;
  CompleteRecordCodec& operator=(const CompleteRecordCodec&) = delete;
  CompleteRecordCodec(CompleteRecordCodec&& other) noexcept;
  CompleteRecordCodec& operator=(CompleteRecordCodec&& other) noexcept;
  ~CompleteRecordCodec();

  // Returned record/field views are borrowed from this instance. They expire on the next
  // successfully guarded Decode/Encode call, move construction/assignment of the owner, or owner
  // destruction. Reusing a moved-from owner does not revive its old views. BYTES additionally
  // borrow the Decode input. Reading views concurrently with a call or move is invalid.
  [[nodiscard]] DecodeResult Decode(std::size_t pipeline_index, ByteView frame) noexcept;

  // The output buffer remains caller-owned. On failure bytes_written is zero and modified buffer
  // contents are not a deliverable result.
  [[nodiscard]] EncodeResult Encode(std::size_t pipeline_index, std::size_t message_index,
                                    const EncodeValue* values, std::size_t value_count,
                                    MutableByteBuffer output) noexcept;

  [[nodiscard]] ExecutionMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit CompleteRecordCodec(std::unique_ptr<Impl> impl) noexcept;
  void AdvanceViewEpoch() noexcept;
  std::unique_ptr<Impl> impl_;
  std::uint64_t view_epoch_ = 1U;

  friend class DecodedFieldView;
  friend class DecodedRecordView;
  friend class HostEndpoint;
  friend struct CompleteRecordCodecCreateResult;
  friend PAE_API CompleteRecordCodecCreateResult
  CreateCompleteRecordCodec(const CompiledProtocol&, const CompleteRecordCodecOptions&) noexcept;
};

struct CompleteRecordCodecCreateResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::unique_ptr<CompleteRecordCodec> codec;
  ExecutionMemoryReport memory;
};

[[nodiscard]] PAE_API CompleteRecordCodecCreateResult CreateCompleteRecordCodec(
    const CompiledProtocol& compiled, const CompleteRecordCodecOptions& options = {}) noexcept;

}  // namespace pae
