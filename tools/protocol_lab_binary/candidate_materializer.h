#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "../../src/host_endpoint/host_endpoint.h"

namespace pae::protocol_lab_binary {

// Callers may tighten, but cannot enlarge, these per-candidate hard limits.
struct Limits {
  std::size_t max_frame_bytes = 65536U;
  std::size_t max_fields = 1024U;
  std::size_t max_field_bytes = 1024U * 1024U;
  std::size_t max_string_bytes = 4U * 1024U * 1024U;
  std::size_t max_total_bytes = 8U * 1024U * 1024U;
  std::size_t max_identity_bytes = 256U;
};

class MaterializationError : public std::runtime_error {
 public:
  explicit MaterializationError(const char* message) : std::runtime_error(message) {}
};

struct RawInteger {
  protocol_core::RawIntegerKind kind = protocol_core::RawIntegerKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
};

struct EnumValue {
  std::uint64_t raw_value = 0U;
  bool known = false;
  std::string item_id;
  // The frozen Plan has no display metadata. Empty until a separately owned sidecar exists.
  std::string display_text;
};

struct FieldValue {
  std::size_t index = protocol_core::kInvalidIndex;
  std::string id;
  protocol_core::LogicalValueKind kind = protocol_core::LogicalValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  std::vector<std::uint8_t> bytes_value;
  EnumValue enum_value;
  protocol_core::Decimal64 decimal64_value;
  std::optional<RawInteger> raw_integer;
};

struct CopyBudget {
  std::size_t object_bytes = 0U;
  std::size_t frame_bytes = 0U;
  std::size_t field_slot_bytes = 0U;  // Includes inline raw/enum scalar storage.
  std::size_t field_bytes = 0U;
  std::size_t string_bytes = 0U;  // Capacity + terminator; conservatively also charges SSO.
  std::size_t accounted_total_bytes = 0U;
  std::size_t temporary_bytes = 0U;  // Fixed association scratch used during this call.
  std::size_t materialization_peak_bytes = 0U;
};

struct CandidateResult {
  std::uint64_t generation = 0U;
  protocol_core::DecodeResult decoded;
  std::size_t message_index = protocol_core::kInvalidIndex;
  std::string message_id;
  std::vector<std::uint8_t> frame;
  std::vector<std::uint8_t> diagnostic_frame;
  std::vector<FieldValue> fields;
  CopyBudget budget;
};

#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
// Instrumented target only: controlled exceptions at non-noexcept copy boundaries. This does not
// inject into every allocator call (notably MSVC Debug container proxy construction/moves).
struct CopyTestHooks {
  void (*before_copy)(void*) = nullptr;
  void* context = nullptr;
};
#endif

// Schema 0.9 only, called synchronously inside the Host Candidate callback. Returns an owned DTO
// or throws (including bad_alloc); never publishes partial results. No Decode, Plan ownership,
// physical ranges, sidecar descriptions, session/flow identity, or UI copies are provided here.
// Budget covers only this DTO and local scratch, not allocator bookkeeping, stack frames, caller
// retained DTOs, Plan/Session, input/frozen chunks, sidecars, adapter admission, or UI/RSS.
[[nodiscard]] CandidateResult MaterializeCandidate(const host_endpoint::Candidate& candidate,
                                                   const Limits& limits = {}
#if defined(PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS)
                                                   ,
                                                   const CopyTestHooks* hooks = nullptr
#endif
);

}  // namespace pae::protocol_lab_binary
