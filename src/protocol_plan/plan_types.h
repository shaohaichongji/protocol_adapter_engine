#pragma once

#include <cstddef>
#include <cstdint>

namespace pae::protocol_plan {

enum class ResourceProfile {
  DESKTOP,
  CONSTRAINED,
};

struct ResourceProfileLimits {
  std::uint64_t max_frame_bytes = 0U;
  std::size_t max_framing_profiles = 0U;
  std::size_t max_pipelines = 0U;
  std::size_t max_messages = 0U;
  std::size_t max_fields_per_message = 0U;
  std::size_t max_total_fields = 0U;
  std::size_t max_matchers_per_message = 0U;
  std::size_t max_total_matchers = 0U;
  std::size_t max_enum_entries_per_field = 0U;
  std::size_t max_total_enum_entries = 0U;
  std::size_t max_session_memory_bytes = 0U;
  std::size_t max_plan_memory_bytes = 0U;
};

inline constexpr ResourceProfileLimits kDesktopResourceProfileLimits{
    64U * 1024U, 32U,   32U,   64U,    2048U,        8192U,
    64U,         4096U, 4096U, 65536U, 512U * 1024U, 128U * 1024U * 1024U};
inline constexpr ResourceProfileLimits kConstrainedResourceProfileLimits{
    4U * 1024U, 8U, 8U, 16U, 256U, 1024U, 32U, 512U, 256U, 4096U, 128U * 1024U, 8U * 1024U * 1024U};

inline constexpr std::size_t kV01MaxPlanMemoryHardLimit = 256U * 1024U * 1024U;

constexpr const ResourceProfileLimits* GetResourceProfileLimits(ResourceProfile profile) noexcept {
  switch (profile) {
    case ResourceProfile::DESKTOP:
      return &kDesktopResourceProfileLimits;
    case ResourceProfile::CONSTRAINED:
      return &kConstrainedResourceProfileLimits;
  }
  return nullptr;
}

enum class InputKind {
  COMPLETE_RECORD,
};

enum class MatcherKind {
  FRAME_LENGTH_EQUALS,
  FIXED_BYTES,
};

enum class ValueType {
  UINT64,
  BYTES,
  ENUM,
  BOOL,
};

enum class WireCodec {
  UNSIGNED_INTEGER,
  BYTES,
  BITFIELD,
};

enum class BitNumbering {
  LSB0,
  MSB0,
};

enum class ByteOrder {
  NOT_APPLICABLE,
  BIG,
  LITTLE,
};

enum class EncodeSource {
  INPUT,
  CONSTANT,
};

enum class UnknownEnumPolicy {
  REJECT,
  PRESERVE,
};

enum class IntegrityAlgorithm {
  SUM8,
};

struct ResourceRequirements {
  std::uint64_t max_frame_bytes = 0U;
  std::size_t framing_profile_count = 0U;
  std::size_t pipeline_count = 0U;
  std::size_t message_count = 0U;
  std::size_t total_field_count = 0U;
  std::size_t total_matcher_count = 0U;
  std::size_t total_enum_entry_count = 0U;
  std::size_t total_bit_container_count = 0U;
  std::size_t total_integrity_rule_count = 0U;
};

struct PlanMemoryReport {
  std::size_t object_bytes = 0U;
  std::size_t string_bytes = 0U;
  std::size_t matcher_bytes = 0U;
  std::size_t metadata_container_bytes = 0U;
  std::size_t execution_descriptor_bytes = 0U;
  std::size_t index_bytes = 0U;
  std::size_t extension_bytes = 0U;
  std::size_t alignment_bytes = 0U;
  std::size_t allocation_count = 0U;
  std::size_t upstream_allocation_count = 0U;
  std::size_t accounted_total_bytes = 0U;
};

}  // namespace pae::protocol_plan
