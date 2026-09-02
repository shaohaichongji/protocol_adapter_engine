#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace pae::json_spike {

constexpr std::size_t kNoByteOffset = static_cast<std::size_t>(-1);

enum class ErrorCode {
  kOk,
  kInputLimit,
  kBomNotAllowed,
  kInvalidUtf8,
  kSyntaxError,
  kDuplicateKey,
  kDepthLimit,
  kNodeLimit,
  kObjectLimit,
  kStringLimit,
  kArrayLimit,
  kNumberTokenLimit,
  kTotalStringLimit,
  kIntegerNotExact,
  kIntegerOutOfRange,
  kRealOutOfRange,
  kUnknownField,
  kMissingField,
  kTypeMismatch,
  kReferenceNotFound,
  kSemanticConstraint,
  kResourceExhausted,
  kInternalError,
};

enum class ByteOffsetKind {
  kNone,
  kExactInputByte,
  kParserReportedPosition,
};

struct Limits {
  std::size_t max_input_bytes = 256U * 1024U;
  std::size_t max_depth = 64U;
  std::size_t max_nodes = 4096U;
  std::size_t max_string_bytes = 4096U;
  std::size_t max_array_elements = 1024U;
  std::size_t max_parser_memory_bytes = 4U * 1024U * 1024U;
  bool capture_number_lexemes = false;
  std::size_t max_object_members = 4096U;
  std::size_t max_number_token_bytes = 64U;
  std::size_t max_total_decoded_string_bytes = 1024U * 1024U;
};

struct DocumentStats {
  std::size_t node_count = 0U;
  std::size_t max_depth = 0U;
  std::size_t max_object_members = 0U;
  std::size_t max_string_bytes = 0U;
  std::size_t max_array_elements = 0U;
  std::size_t max_number_token_bytes = 0U;
  std::size_t total_decoded_string_bytes = 0U;
  std::size_t number_token_count = 0U;
  bool number_lexemes_preserved = false;
  std::string number_lexemes;
};

enum class ExactIntegerStatus {
  kOk,
  kNotInteger,
  kOutOfRange,
  kInvalidToken,
};

enum class ErrorReason {
  kNone,
  kNegativeTokenForUnsigned,
  kNotInteger,
  kIntegerOutOfRange,
  kInvalidToken,
  kRealOverflow,
  kRealUnderflow,
};

enum class RealTokenStatus {
  kOk,
  kOverflow,
  kUnderflow,
  kInvalidToken,
};

enum class NumberTokenKind {
  kUnsignedInteger,
  kSignedInteger,
  kReal,
  kInvalid,
};

struct NumberTokenInfo {
  NumberTokenKind kind = NumberTokenKind::kInvalid;
  bool negative = false;
  bool negative_zero = false;
  bool has_fraction = false;
  bool has_exponent = false;
};

struct ParserMemoryStats {
  bool hard_limit_enforced = false;
  bool allocator_contract_ok = false;
  bool failure_injected = false;
  bool arena_exhausted = false;
  std::size_t limit_bytes = 0U;
  std::size_t reserved_bytes = 0U;
  std::size_t required_upper_bound_bytes = 0U;
  std::size_t allocation_attempts = 0U;
  std::size_t malloc_calls = 0U;
  std::size_t realloc_calls = 0U;
  std::size_t free_calls = 0U;
  std::size_t peak_live_requested_bytes = 0U;
  std::size_t live_requested_bytes = 0U;
  std::size_t live_blocks = 0U;
};

struct ParseResult {
  ErrorCode code = ErrorCode::kInternalError;
  ErrorReason reason = ErrorReason::kNone;
  std::size_t byte_offset = kNoByteOffset;
  ByteOffsetKind byte_offset_kind = ByteOffsetKind::kNone;
  bool has_json_pointer = false;
  std::string json_pointer;
  DocumentStats stats;
  ParserMemoryStats parser_memory;
};

using ParseFunction = ParseResult (*)(std::string_view input, const Limits& limits);

const char* ToString(ErrorCode code) noexcept;
const char* ToString(ByteOffsetKind kind) noexcept;
const char* ToString(ExactIntegerStatus status) noexcept;
const char* ToString(ErrorReason reason) noexcept;
const char* ToString(RealTokenStatus status) noexcept;
const char* ToString(NumberTokenKind kind) noexcept;
NumberTokenInfo ClassifyJsonNumberToken(std::string_view token) noexcept;
ExactIntegerStatus ParseJsonUint64Token(std::string_view token, std::uint64_t& value) noexcept;
ExactIntegerStatus ParseJsonUint64Token(std::string_view token, std::uint64_t& value,
                                        ErrorReason& reason) noexcept;
ExactIntegerStatus ParseJsonInt64Token(std::string_view token, std::int64_t& value) noexcept;
ExactIntegerStatus ParseJsonInt64Token(std::string_view token, std::int64_t& value,
                                       ErrorReason& reason) noexcept;
RealTokenStatus ParseJsonReal64Token(std::string_view token, double& value,
                                     ErrorReason& reason) noexcept;
ParseResult Accepted(DocumentStats stats = {});
ParseResult Rejected(ErrorCode code, std::size_t byte_offset = kNoByteOffset,
                     ByteOffsetKind byte_offset_kind = ByteOffsetKind::kParserReportedPosition,
                     ErrorReason reason = ErrorReason::kNone) noexcept;
ParseResult RejectedAtPath(ErrorCode code, std::string_view json_pointer,
                           ErrorReason reason = ErrorReason::kNone);

std::size_t AppendJsonPointerToken(std::string& json_pointer, std::string_view token);
std::size_t AppendJsonPointerIndex(std::string& json_pointer, std::size_t index);
void RestoreJsonPointer(std::string& json_pointer, std::size_t previous_size) noexcept;

bool RunCommonPreflight(std::string_view input, const Limits& limits,
                        ParseResult& rejection) noexcept;

int RunCandidate(const char* candidate_name, const char* candidate_version, ParseFunction parse,
                 int argc, char* argv[]);

}  // namespace pae::json_spike
