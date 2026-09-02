#include <yyjson.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <unordered_set>

#include "resource_profiles_generated.h"
#include "spike_common.h"

namespace pae::json_spike {
namespace {

constexpr yyjson_read_flag kReadFlags = YYJSON_READ_NUMBER_AS_RAW;

enum class PostParseMode {
  kBoundedTreeAuditOnly,
  kProbeSchema,
};

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }
};

using DocumentPtr = std::unique_ptr<yyjson_doc, DocumentDeleter>;

enum class AllocatorFailure {
  kNone,
  kInjected,
  kArenaExhausted,
  kTrackingCapacity,
  kContractViolation,
};

struct AllocationRecord {
  void* pointer = nullptr;
  std::size_t requested_bytes = 0U;
};

struct AllocatorStats {
  std::size_t allocation_attempts = 0U;
  std::size_t malloc_calls = 0U;
  std::size_t realloc_calls = 0U;
  std::size_t free_calls = 0U;
  std::size_t live_requested_bytes = 0U;
  std::size_t peak_live_requested_bytes = 0U;
  std::size_t live_blocks = 0U;
  std::size_t peak_live_blocks = 0U;
  std::size_t injected_failures = 0U;
  std::size_t arena_failures = 0U;
  std::size_t tracking_failures = 0U;
  std::size_t contract_failures = 0U;
  AllocatorFailure first_failure = AllocatorFailure::kNone;
};

struct LimitedPoolContext {
  yyjson_alc pool{};
  std::array<AllocationRecord, 8U> records{};
  std::size_t fail_on_attempt = 0U;
  AllocatorStats stats;
};

AllocationRecord* FindRecord(LimitedPoolContext& context, void* pointer) noexcept {
  for (AllocationRecord& record : context.records) {
    if (record.pointer == pointer) {
      return &record;
    }
  }
  return nullptr;
}

AllocationRecord* FindEmptyRecord(LimitedPoolContext& context) noexcept {
  return FindRecord(context, nullptr);
}

void RememberFirstFailure(LimitedPoolContext& context, AllocatorFailure failure) noexcept {
  if (context.stats.first_failure == AllocatorFailure::kNone) {
    context.stats.first_failure = failure;
  }
}

bool ShouldInjectFailure(LimitedPoolContext& context) noexcept {
  return context.fail_on_attempt != 0U &&
         context.stats.allocation_attempts == context.fail_on_attempt;
}

void RecordSuccessfulAllocation(LimitedPoolContext& context, AllocationRecord& record,
                                void* pointer, std::size_t size) noexcept {
  record.pointer = pointer;
  record.requested_bytes = size;
  context.stats.live_requested_bytes += size;
  ++context.stats.live_blocks;
  context.stats.peak_live_requested_bytes =
      (std::max)(context.stats.peak_live_requested_bytes, context.stats.live_requested_bytes);
  context.stats.peak_live_blocks =
      (std::max)(context.stats.peak_live_blocks, context.stats.live_blocks);
}

void* ProxyMalloc(void* context_pointer, std::size_t size) noexcept {
  auto& context = *static_cast<LimitedPoolContext*>(context_pointer);
  ++context.stats.malloc_calls;
  ++context.stats.allocation_attempts;
  if (ShouldInjectFailure(context)) {
    ++context.stats.injected_failures;
    RememberFirstFailure(context, AllocatorFailure::kInjected);
    return nullptr;
  }

  void* pointer = context.pool.malloc(context.pool.ctx, size);
  if (pointer == nullptr) {
    ++context.stats.arena_failures;
    RememberFirstFailure(context, AllocatorFailure::kArenaExhausted);
    return nullptr;
  }

  AllocationRecord* record = FindEmptyRecord(context);
  if (record == nullptr) {
    context.pool.free(context.pool.ctx, pointer);
    ++context.stats.tracking_failures;
    RememberFirstFailure(context, AllocatorFailure::kTrackingCapacity);
    return nullptr;
  }

  RecordSuccessfulAllocation(context, *record, pointer, size);
  return pointer;
}

void* ProxyRealloc(void* context_pointer, void* pointer, std::size_t old_size,
                   std::size_t size) noexcept {
  auto& context = *static_cast<LimitedPoolContext*>(context_pointer);
  ++context.stats.realloc_calls;
  ++context.stats.allocation_attempts;

  AllocationRecord* record = FindRecord(context, pointer);
  if (pointer == nullptr || size == 0U || record == nullptr ||
      record->requested_bytes != old_size) {
    ++context.stats.contract_failures;
    RememberFirstFailure(context, AllocatorFailure::kContractViolation);
    return nullptr;
  }
  if (ShouldInjectFailure(context)) {
    ++context.stats.injected_failures;
    RememberFirstFailure(context, AllocatorFailure::kInjected);
    return nullptr;
  }

  void* replacement = context.pool.realloc(context.pool.ctx, pointer, old_size, size);
  if (replacement == nullptr) {
    ++context.stats.arena_failures;
    RememberFirstFailure(context, AllocatorFailure::kArenaExhausted);
    return nullptr;
  }

  context.stats.live_requested_bytes -= record->requested_bytes;
  record->pointer = replacement;
  record->requested_bytes = size;
  context.stats.live_requested_bytes += size;
  context.stats.peak_live_requested_bytes =
      (std::max)(context.stats.peak_live_requested_bytes, context.stats.live_requested_bytes);
  return replacement;
}

void ProxyFree(void* context_pointer, void* pointer) noexcept {
  if (pointer == nullptr) {
    return;
  }

  auto& context = *static_cast<LimitedPoolContext*>(context_pointer);
  ++context.stats.free_calls;
  AllocationRecord* record = FindRecord(context, pointer);
  if (record == nullptr) {
    ++context.stats.contract_failures;
    RememberFirstFailure(context, AllocatorFailure::kContractViolation);
    return;
  }

  context.stats.live_requested_bytes -= record->requested_bytes;
  --context.stats.live_blocks;
  record->pointer = nullptr;
  record->requested_bytes = 0U;
  context.pool.free(context.pool.ctx, pointer);
}

bool HasAllocatorContractFailure(const LimitedPoolContext& context) noexcept {
  return context.stats.tracking_failures != 0U || context.stats.contract_failures != 0U;
}

ParserMemoryStats CaptureParserMemoryStats(const ParserMemoryStats& base,
                                           const LimitedPoolContext& context) noexcept {
  ParserMemoryStats result = base;
  result.allocator_contract_ok = !HasAllocatorContractFailure(context) &&
                                 context.stats.live_requested_bytes == 0U &&
                                 context.stats.live_blocks == 0U;
  result.failure_injected = context.stats.injected_failures != 0U;
  result.arena_exhausted = context.stats.arena_failures != 0U;
  result.allocation_attempts = context.stats.allocation_attempts;
  result.malloc_calls = context.stats.malloc_calls;
  result.realloc_calls = context.stats.realloc_calls;
  result.free_calls = context.stats.free_calls;
  result.peak_live_requested_bytes = context.stats.peak_live_requested_bytes;
  result.live_requested_bytes = context.stats.live_requested_bytes;
  result.live_blocks = context.stats.live_blocks;
  return result;
}

ParseResult RejectWithStatsAtPath(ErrorCode code, const DocumentStats& stats,
                                  std::string_view json_pointer,
                                  ErrorReason reason = ErrorReason::kNone) {
  ParseResult result = RejectedAtPath(code, json_pointer, reason);
  result.stats = stats;
  return result;
}

bool IsAllowedRootKey(std::string_view key) noexcept {
  return key == "schema_version" || key == "protocol_id" || key == "byte_offset" ||
         key == "messages" || key == "payload" || key == "probe_uint64" || key == "probe_int64" ||
         key == "probe_real64" || key == "probe_known_id" || key == "probe_reference_id" ||
         key == "probe_range_start" || key == "probe_range_end";
}

ParseResult ValidateUint64Token(yyjson_val* value, const DocumentStats& stats,
                                std::string_view json_pointer, std::uint64_t& parsed_value) {
  if (!yyjson_is_raw(value)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, json_pointer);
  }

  const std::string_view token{yyjson_get_raw(value), yyjson_get_len(value)};
  ErrorReason reason = ErrorReason::kNone;
  const ExactIntegerStatus status = ParseJsonUint64Token(token, parsed_value, reason);
  switch (status) {
    case ExactIntegerStatus::kOk:
      return Accepted(stats);
    case ExactIntegerStatus::kNotInteger:
      return RejectWithStatsAtPath(ErrorCode::kIntegerNotExact, stats, json_pointer);
    case ExactIntegerStatus::kOutOfRange:
      return RejectWithStatsAtPath(ErrorCode::kIntegerOutOfRange, stats, json_pointer, reason);
    case ExactIntegerStatus::kInvalidToken:
      return RejectWithStatsAtPath(ErrorCode::kInternalError, stats, json_pointer);
  }
  return RejectWithStatsAtPath(ErrorCode::kInternalError, stats, json_pointer);
}

ParseResult ValidateReal64Token(yyjson_val* value, const DocumentStats& stats,
                                std::string_view json_pointer, double& parsed_value) {
  if (!yyjson_is_raw(value)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, json_pointer);
  }

  const std::string_view token{yyjson_get_raw(value), yyjson_get_len(value)};
  ErrorReason reason = ErrorReason::kNone;
  const RealTokenStatus status = ParseJsonReal64Token(token, parsed_value, reason);
  switch (status) {
    case RealTokenStatus::kOk:
      return Accepted(stats);
    case RealTokenStatus::kOverflow:
    case RealTokenStatus::kUnderflow:
      return RejectWithStatsAtPath(ErrorCode::kRealOutOfRange, stats, json_pointer, reason);
    case RealTokenStatus::kInvalidToken:
      return RejectWithStatsAtPath(ErrorCode::kInternalError, stats, json_pointer, reason);
  }
  return RejectWithStatsAtPath(ErrorCode::kInternalError, stats, json_pointer);
}

ParseResult ValidateInt64Token(yyjson_val* value, const DocumentStats& stats,
                               std::string_view json_pointer, std::int64_t& parsed_value) {
  if (!yyjson_is_raw(value)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, json_pointer);
  }

  const std::string_view token{yyjson_get_raw(value), yyjson_get_len(value)};
  ErrorReason reason = ErrorReason::kNone;
  const ExactIntegerStatus status = ParseJsonInt64Token(token, parsed_value, reason);
  switch (status) {
    case ExactIntegerStatus::kOk:
      return Accepted(stats);
    case ExactIntegerStatus::kNotInteger:
      return RejectWithStatsAtPath(ErrorCode::kIntegerNotExact, stats, json_pointer);
    case ExactIntegerStatus::kOutOfRange:
      return RejectWithStatsAtPath(ErrorCode::kIntegerOutOfRange, stats, json_pointer, reason);
    case ExactIntegerStatus::kInvalidToken:
      return RejectWithStatsAtPath(ErrorCode::kInternalError, stats, json_pointer);
  }
  return RejectWithStatsAtPath(ErrorCode::kInternalError, stats, json_pointer);
}

ParseResult AccumulateDecodedStringBytes(std::size_t length, const Limits& limits,
                                         DocumentStats& stats, std::string_view json_pointer) {
  stats.max_string_bytes = (std::max)(stats.max_string_bytes, length);
  if (length > limits.max_string_bytes) {
    return RejectWithStatsAtPath(ErrorCode::kStringLimit, stats, json_pointer);
  }
  if (stats.total_decoded_string_bytes > limits.max_total_decoded_string_bytes ||
      length > limits.max_total_decoded_string_bytes - stats.total_decoded_string_bytes) {
    return RejectWithStatsAtPath(ErrorCode::kTotalStringLimit, stats, json_pointer);
  }
  stats.total_decoded_string_bytes += length;
  return Accepted(stats);
}

ParseResult AuditValue(yyjson_val* value, std::size_t depth, const Limits& limits,
                       DocumentStats& stats, std::string& json_pointer) {
  if (depth > limits.max_depth) {
    return RejectWithStatsAtPath(ErrorCode::kDepthLimit, stats, json_pointer);
  }
  if (stats.node_count >= limits.max_nodes) {
    return RejectWithStatsAtPath(ErrorCode::kNodeLimit, stats, json_pointer);
  }

  ++stats.node_count;
  stats.max_depth = (std::max)(stats.max_depth, depth);

  if (yyjson_is_raw(value)) {
    const std::size_t token_length = yyjson_get_len(value);
    ++stats.number_token_count;
    stats.max_number_token_bytes = (std::max)(stats.max_number_token_bytes, token_length);
    stats.number_lexemes_preserved = true;
    if (token_length > limits.max_number_token_bytes) {
      return RejectWithStatsAtPath(ErrorCode::kNumberTokenLimit, stats, json_pointer);
    }
    if (limits.capture_number_lexemes) {
      if (!stats.number_lexemes.empty()) {
        stats.number_lexemes.push_back('\0');
      }
      stats.number_lexemes.append(yyjson_get_raw(value), yyjson_get_len(value));
    }
    return Accepted(stats);
  }
  if (yyjson_is_num(value)) {
    ++stats.number_token_count;
  }

  if (yyjson_is_str(value)) {
    const std::size_t length = yyjson_get_len(value);
    return AccumulateDecodedStringBytes(length, limits, stats, json_pointer);
  }

  if (yyjson_is_arr(value)) {
    const std::size_t element_count = yyjson_arr_size(value);
    stats.max_array_elements = (std::max)(stats.max_array_elements, element_count);
    if (element_count > limits.max_array_elements) {
      return RejectWithStatsAtPath(ErrorCode::kArrayLimit, stats, json_pointer);
    }

    yyjson_arr_iter iterator;
    yyjson_arr_iter_init(value, &iterator);
    std::size_t element_index = 0U;
    while (yyjson_val* element = yyjson_arr_iter_next(&iterator)) {
      const std::size_t previous_size = AppendJsonPointerIndex(json_pointer, element_index);
      const ParseResult child = AuditValue(element, depth + 1U, limits, stats, json_pointer);
      RestoreJsonPointer(json_pointer, previous_size);
      if (child.code != ErrorCode::kOk) {
        return child;
      }
      ++element_index;
    }
    return Accepted(stats);
  }

  if (yyjson_is_obj(value)) {
    const std::size_t member_count = yyjson_obj_size(value);
    stats.max_object_members = (std::max)(stats.max_object_members, member_count);
    if (member_count > limits.max_object_members) {
      return RejectWithStatsAtPath(ErrorCode::kObjectLimit, stats, json_pointer);
    }

    std::unordered_set<std::string> keys;
    keys.reserve(member_count);

    yyjson_obj_iter key_iterator;
    yyjson_obj_iter_init(value, &key_iterator);
    while (yyjson_val* key = yyjson_obj_iter_next(&key_iterator)) {
      const std::size_t key_length = yyjson_get_len(key);
      const std::size_t previous_size =
          AppendJsonPointerToken(json_pointer, std::string_view{yyjson_get_str(key), key_length});
      ParseResult string_result =
          AccumulateDecodedStringBytes(key_length, limits, stats, json_pointer);
      RestoreJsonPointer(json_pointer, previous_size);
      if (string_result.code != ErrorCode::kOk) {
        return string_result;
      }

      const auto [unused, inserted] = keys.emplace(yyjson_get_str(key), key_length);
      static_cast<void>(unused);
      if (!inserted) {
        const std::size_t duplicate_pointer_size =
            AppendJsonPointerToken(json_pointer, std::string_view{yyjson_get_str(key), key_length});
        ParseResult result = RejectWithStatsAtPath(ErrorCode::kDuplicateKey, stats, json_pointer);
        RestoreJsonPointer(json_pointer, duplicate_pointer_size);
        return result;
      }
    }

    yyjson_obj_iter value_iterator;
    yyjson_obj_iter_init(value, &value_iterator);
    while (yyjson_val* key = yyjson_obj_iter_next(&value_iterator)) {
      yyjson_val* child_value = yyjson_obj_iter_get_val(key);
      const std::size_t previous_size = AppendJsonPointerToken(
          json_pointer, std::string_view{yyjson_get_str(key), yyjson_get_len(key)});
      const ParseResult child = AuditValue(child_value, depth + 1U, limits, stats, json_pointer);
      RestoreJsonPointer(json_pointer, previous_size);
      if (child.code != ErrorCode::kOk) {
        return child;
      }
    }
  }

  return Accepted(stats);
}

ParseResult ValidateProbeSchema(yyjson_val* root, const DocumentStats& stats) {
  if (!yyjson_is_obj(root)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "");
  }

  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(root, &iterator);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string_view key_view{yyjson_get_str(key), yyjson_get_len(key)};
    if (!IsAllowedRootKey(key_view)) {
      std::string json_pointer;
      AppendJsonPointerToken(json_pointer, key_view);
      return RejectWithStatsAtPath(ErrorCode::kUnknownField, stats, json_pointer);
    }
  }

  yyjson_val* schema_version = yyjson_obj_get(root, "schema_version");
  yyjson_val* protocol_id = yyjson_obj_get(root, "protocol_id");
  yyjson_val* byte_offset = yyjson_obj_get(root, "byte_offset");
  yyjson_val* messages = yyjson_obj_get(root, "messages");
  if (schema_version == nullptr) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/schema_version");
  }
  if (protocol_id == nullptr) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/protocol_id");
  }
  if (byte_offset == nullptr) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/byte_offset");
  }
  if (messages == nullptr) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/messages");
  }

  if (!yyjson_is_str(schema_version)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/schema_version");
  }
  if (!yyjson_is_str(protocol_id)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/protocol_id");
  }
  if (!yyjson_is_arr(messages)) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/messages");
  }

  std::uint64_t parsed_byte_offset = 0U;
  const ParseResult byte_offset_result =
      ValidateUint64Token(byte_offset, stats, "/byte_offset", parsed_byte_offset);
  if (byte_offset_result.code != ErrorCode::kOk) {
    return byte_offset_result;
  }

  yyjson_val* probe_uint64 = yyjson_obj_get(root, "probe_uint64");
  if (probe_uint64 != nullptr) {
    std::uint64_t parsed_probe_uint64 = 0U;
    const ParseResult probe_result =
        ValidateUint64Token(probe_uint64, stats, "/probe_uint64", parsed_probe_uint64);
    if (probe_result.code != ErrorCode::kOk) {
      return probe_result;
    }
  }

  yyjson_val* probe_int64 = yyjson_obj_get(root, "probe_int64");
  if (probe_int64 != nullptr) {
    std::int64_t parsed_probe_int64 = 0;
    const ParseResult probe_result =
        ValidateInt64Token(probe_int64, stats, "/probe_int64", parsed_probe_int64);
    if (probe_result.code != ErrorCode::kOk) {
      return probe_result;
    }
  }

  yyjson_val* probe_real64 = yyjson_obj_get(root, "probe_real64");
  if (probe_real64 != nullptr) {
    double parsed_probe_real64 = 0.0;
    const ParseResult probe_result =
        ValidateReal64Token(probe_real64, stats, "/probe_real64", parsed_probe_real64);
    if (probe_result.code != ErrorCode::kOk) {
      return probe_result;
    }
  }

  yyjson_val* known_id = yyjson_obj_get(root, "probe_known_id");
  yyjson_val* reference_id = yyjson_obj_get(root, "probe_reference_id");
  if (known_id != nullptr || reference_id != nullptr) {
    if (known_id == nullptr) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_known_id");
    }
    if (reference_id == nullptr) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_reference_id");
    }
    if (!yyjson_is_str(known_id)) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_known_id");
    }
    if (!yyjson_is_str(reference_id)) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_reference_id");
    }
    const std::string_view known{yyjson_get_str(known_id), yyjson_get_len(known_id)};
    const std::string_view reference{yyjson_get_str(reference_id), yyjson_get_len(reference_id)};
    if (known != reference) {
      return RejectWithStatsAtPath(ErrorCode::kReferenceNotFound, stats, "/probe_reference_id");
    }
  }

  yyjson_val* range_start = yyjson_obj_get(root, "probe_range_start");
  yyjson_val* range_end = yyjson_obj_get(root, "probe_range_end");
  if (range_start != nullptr || range_end != nullptr) {
    if (range_start == nullptr) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_range_start");
    }
    if (range_end == nullptr) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_range_end");
    }
    std::uint64_t parsed_range_start = 0U;
    std::uint64_t parsed_range_end = 0U;
    const ParseResult range_start_result =
        ValidateUint64Token(range_start, stats, "/probe_range_start", parsed_range_start);
    if (range_start_result.code != ErrorCode::kOk) {
      return range_start_result;
    }
    const ParseResult range_end_result =
        ValidateUint64Token(range_end, stats, "/probe_range_end", parsed_range_end);
    if (range_end_result.code != ErrorCode::kOk) {
      return range_end_result;
    }
    if (parsed_range_start > parsed_range_end) {
      return RejectWithStatsAtPath(ErrorCode::kSemanticConstraint, stats, "/probe_range_start");
    }
  }

  return Accepted(stats);
}

ParseResult ParseStrictWithAllocationFault(std::string_view input, const Limits& limits,
                                           std::size_t fail_on_attempt,
                                           PostParseMode mode = PostParseMode::kProbeSchema) {
  ParserMemoryStats parser_memory;
  parser_memory.hard_limit_enforced = true;
  parser_memory.limit_bytes = limits.max_parser_memory_bytes;

  try {
    ParseResult preflight;
    if (!RunCommonPreflight(input, limits, preflight)) {
      preflight.parser_memory = parser_memory;
      return preflight;
    }

    parser_memory.required_upper_bound_bytes =
        yyjson_read_max_memory_usage(input.size(), kReadFlags);
    if (parser_memory.required_upper_bound_bytes == 0U || limits.max_parser_memory_bytes == 0U) {
      ParseResult result = Rejected(ErrorCode::kResourceExhausted);
      result.parser_memory = parser_memory;
      return result;
    }

    parser_memory.reserved_bytes =
        (std::min)(parser_memory.required_upper_bound_bytes, limits.max_parser_memory_bytes);
    std::unique_ptr<unsigned char[]> parser_pool{new unsigned char[parser_memory.reserved_bytes]};
    LimitedPoolContext context;
    context.fail_on_attempt = fail_on_attempt;
    if (!yyjson_alc_pool_init(&context.pool, parser_pool.get(), parser_memory.reserved_bytes)) {
      ParseResult result = Rejected(ErrorCode::kResourceExhausted);
      result.parser_memory = parser_memory;
      return result;
    }
    yyjson_alc allocator{&ProxyMalloc, &ProxyRealloc, &ProxyFree, &context};

    yyjson_read_err error{};
    ParseResult result;
    {
      DocumentPtr document{yyjson_read_opts(const_cast<char*>(input.data()), input.size(),
                                            kReadFlags, &allocator, &error)};
      if (document == nullptr) {
        if (HasAllocatorContractFailure(context)) {
          result = Rejected(ErrorCode::kInternalError);
        } else if (error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION ||
                   context.stats.first_failure == AllocatorFailure::kInjected ||
                   context.stats.first_failure == AllocatorFailure::kArenaExhausted) {
          result = Rejected(ErrorCode::kResourceExhausted);
        } else {
          result = Rejected(ErrorCode::kSyntaxError, error.pos);
        }
      } else {
        yyjson_val* root = yyjson_doc_get_root(document.get());
        if (root == nullptr) {
          result = Rejected(ErrorCode::kSyntaxError);
        } else {
          DocumentStats stats;
          std::string json_pointer;
          const ParseResult audit = AuditValue(root, 1U, limits, stats, json_pointer);
          result = audit.code == ErrorCode::kOk && mode == PostParseMode::kProbeSchema
                       ? ValidateProbeSchema(root, stats)
                       : audit;
        }
      }
    }
    result.parser_memory = CaptureParserMemoryStats(parser_memory, context);
    if (!result.parser_memory.allocator_contract_ok) {
      result = Rejected(ErrorCode::kInternalError);
      result.parser_memory = CaptureParserMemoryStats(parser_memory, context);
    }
    return result;
  } catch (const std::bad_alloc&) {
    ParseResult result = Rejected(ErrorCode::kResourceExhausted);
    result.parser_memory = parser_memory;
    return result;
  } catch (...) {
    ParseResult result = Rejected(ErrorCode::kInternalError);
    result.parser_memory = parser_memory;
    return result;
  }
}

ParseResult ParseStrict(std::string_view input, const Limits& limits) {
  return ParseStrictWithAllocationFault(input, limits, 0U);
}

ParseResult ParseStrictAuditOnly(std::string_view input, const Limits& limits) {
  return ParseStrictWithAllocationFault(input, limits, 0U, PostParseMode::kBoundedTreeAuditOnly);
}

std::string MakeAllocatorStressInput(std::size_t element_count) {
  std::string result =
      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
      "\"byte_offset\":1,\"messages\":[],\"payload\":[";
  for (std::size_t index = 0U; index < element_count; ++index) {
    if (index != 0U) {
      result.push_back(',');
    }
    result.push_back('0');
  }
  result += "]}";
  return result;
}

int RunAllocatorSelfTest() {
  constexpr std::string_view kInput =
      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
      "\"byte_offset\":1,\"messages\":[]}";

  Limits zero_limit;
  zero_limit.max_parser_memory_bytes = 0U;
  const ParseResult zero_result = ParseStrict(kInput, zero_limit);

  Limits exhausted_limit;
  exhausted_limit.max_parser_memory_bytes = 64U;
  const ParseResult exhausted_result = ParseStrict(kInput, exhausted_limit);

  const Limits normal_limit;
  const ParseResult normal_result = ParseStrict(kInput, normal_limit);

  const ParseResult first_failure = ParseStrictWithAllocationFault(kInput, normal_limit, 1U);
  const ParseResult second_failure = ParseStrictWithAllocationFault(kInput, normal_limit, 2U);

  Limits preflight_limit = normal_limit;
  preflight_limit.max_input_bytes = kInput.size() - 1U;
  const ParseResult preflight_result = ParseStrictWithAllocationFault(kInput, preflight_limit, 1U);
  constexpr std::string_view kSyntaxErrorInput =
      "{\"schema_version\":\"0.1\",\"protocol_id\":\"probe\","
      "\"byte_offset\":1,\"messages\":[}";
  const ParseResult syntax_result =
      ParseStrictWithAllocationFault(kSyntaxErrorInput, normal_limit, 100U);

  Limits stress_limits;
  stress_limits.max_nodes = 10000U;
  stress_limits.max_array_elements = 4096U;
  const std::string stress_input = MakeAllocatorStressInput(2048U);
  const ParseResult stress_baseline = ParseStrict(stress_input, stress_limits);

  bool every_injection_passed = stress_baseline.code == ErrorCode::kOk &&
                                stress_baseline.parser_memory.realloc_calls > 0U &&
                                stress_baseline.parser_memory.allocation_attempts > 2U;
  std::size_t injection_cases = 0U;
  for (std::size_t attempt = 1U;
       every_injection_passed && attempt <= stress_baseline.parser_memory.allocation_attempts;
       ++attempt) {
    const ParseResult injected =
        ParseStrictWithAllocationFault(stress_input, stress_limits, attempt);
    every_injection_passed =
        injected.code == ErrorCode::kResourceExhausted && injected.parser_memory.failure_injected &&
        injected.parser_memory.allocator_contract_ok && injected.parser_memory.live_blocks == 0U &&
        injected.parser_memory.live_requested_bytes == 0U;
    ++injection_cases;
  }
  const ParseResult injection_not_reached = ParseStrictWithAllocationFault(
      stress_input, stress_limits, stress_baseline.parser_memory.allocation_attempts + 1U);

  const bool passed =
      zero_result.code == ErrorCode::kResourceExhausted &&
      zero_result.parser_memory.hard_limit_enforced &&
      exhausted_result.code == ErrorCode::kResourceExhausted &&
      exhausted_result.parser_memory.hard_limit_enforced &&
      exhausted_result.parser_memory.reserved_bytes <= exhausted_limit.max_parser_memory_bytes &&
      normal_result.code == ErrorCode::kOk && normal_result.parser_memory.hard_limit_enforced &&
      normal_result.parser_memory.reserved_bytes <= normal_limit.max_parser_memory_bytes &&
      normal_result.parser_memory.required_upper_bound_bytes > 0U &&
      normal_result.parser_memory.allocator_contract_ok &&
      normal_result.parser_memory.live_blocks == 0U &&
      first_failure.code == ErrorCode::kResourceExhausted &&
      first_failure.parser_memory.failure_injected &&
      first_failure.parser_memory.allocator_contract_ok &&
      second_failure.code == ErrorCode::kResourceExhausted &&
      second_failure.parser_memory.failure_injected &&
      second_failure.parser_memory.allocator_contract_ok &&
      preflight_result.code == ErrorCode::kInputLimit &&
      preflight_result.parser_memory.allocation_attempts == 0U &&
      !preflight_result.parser_memory.failure_injected &&
      syntax_result.code == ErrorCode::kSyntaxError &&
      !syntax_result.parser_memory.failure_injected &&
      syntax_result.parser_memory.allocator_contract_ok && every_injection_passed &&
      injection_not_reached.code == ErrorCode::kOk &&
      !injection_not_reached.parser_memory.failure_injected &&
      injection_not_reached.parser_memory.allocator_contract_ok;

  std::cout << "ALLOCATOR_SELF_TEST candidate=yyjson zero=" << ToString(zero_result.code)
            << " exhausted=" << ToString(exhausted_result.code)
            << " exhausted_reserved=" << exhausted_result.parser_memory.reserved_bytes
            << " normal=" << ToString(normal_result.code)
            << " normal_reserved=" << normal_result.parser_memory.reserved_bytes
            << " normal_upper_bound=" << normal_result.parser_memory.required_upper_bound_bytes
            << " normal_attempts=" << normal_result.parser_memory.allocation_attempts
            << " first_failure=" << ToString(first_failure.code)
            << " second_failure=" << ToString(second_failure.code)
            << " preflight=" << ToString(preflight_result.code)
            << " syntax=" << ToString(syntax_result.code)
            << " stress_attempts=" << stress_baseline.parser_memory.allocation_attempts
            << " stress_reallocs=" << stress_baseline.parser_memory.realloc_calls
            << " injection_cases=" << injection_cases
            << " injection_not_reached=" << ToString(injection_not_reached.code)
            << " pass=" << (passed ? "true" : "false") << '\n';
  return passed ? 0 : 1;
}

Limits MakeResourceLimits(const resource_profiles_generated::Record& profile) {
  Limits limits;
  limits.max_input_bytes = profile.max_input_bytes;
  limits.max_depth = profile.max_depth;
  limits.max_nodes = profile.max_nodes;
  limits.max_string_bytes = profile.max_string_bytes;
  limits.max_array_elements = profile.max_array_elements;
  limits.max_parser_memory_bytes = profile.max_parser_memory_bytes;
  limits.capture_number_lexemes = false;
  limits.max_object_members = profile.max_object_members;
  limits.max_number_token_bytes = profile.max_number_token_bytes;
  limits.max_total_decoded_string_bytes = profile.max_total_string_bytes;
  return limits;
}

std::string MakeNestedArrayValue(std::size_t array_layers) {
  return std::string(array_layers, '[') + "0" + std::string(array_layers, ']');
}

std::string MakeObjectValue(std::size_t member_count) {
  std::string result{"{"};
  for (std::size_t index = 0U; index < member_count; ++index) {
    if (index != 0U) {
      result.push_back(',');
    }
    result += "\"k" + std::to_string(index) + "\":0";
  }
  result.push_back('}');
  return result;
}

std::string MakeArrayValue(std::size_t element_count) {
  std::string result{"["};
  if (element_count != 0U) {
    result.reserve(element_count * 2U + 1U);
  }
  for (std::size_t index = 0U; index < element_count; ++index) {
    if (index != 0U) {
      result.push_back(',');
    }
    result.push_back('0');
  }
  result.push_back(']');
  return result;
}

std::string MakeNodeForestValue(std::size_t target_nodes, std::size_t max_array_elements) {
  std::string result{"["};
  std::size_t remaining_nodes = target_nodes - 1U;
  std::size_t group_count = 0U;
  while (remaining_nodes != 0U) {
    if (group_count != 0U) {
      result.push_back(',');
    }
    const std::size_t group_nodes = (std::min)(remaining_nodes, max_array_elements + 1U);
    const std::size_t leaf_count = group_nodes - 1U;
    result += MakeArrayValue(leaf_count);
    remaining_nodes -= group_nodes;
    ++group_count;
  }
  result.push_back(']');
  return result;
}

std::string MakeStringArrayValue(std::size_t decoded_bytes, std::size_t max_string_bytes) {
  std::string result{"["};
  std::size_t remaining = decoded_bytes;
  std::size_t string_count = 0U;
  while (remaining != 0U) {
    if (string_count != 0U) {
      result.push_back(',');
    }
    const std::size_t chunk = (std::min)(remaining, max_string_bytes);
    result.push_back('"');
    result.append(chunk, 'x');
    result.push_back('"');
    remaining -= chunk;
    ++string_count;
  }
  result.push_back(']');
  return result;
}

std::string MakeInputSizedValue(std::size_t input_bytes) {
  std::string result{"null"};
  result.append(input_bytes - result.size(), ' ');
  return result;
}

class ResourceProfileGate {
 public:
  ResourceProfileGate(const resource_profiles_generated::Record& profile, Limits limits)
      : profile_(profile), limits_(limits) {}

  ParseResult Check(std::string_view dimension, std::string_view boundary, const std::string& input,
                    ErrorCode expected_code, bool expected_pointer_present,
                    std::size_t DocumentStats::* metric = nullptr,
                    std::size_t expected_metric = 0U) {
    const ParseResult result = ParseStrictAuditOnly(input, limits_);
    const bool code_matches = result.code == expected_code;
    const bool pointer_matches = result.has_json_pointer == expected_pointer_present;
    const bool metric_matches = metric == nullptr || result.stats.*metric == expected_metric;
    const bool cleanup_matches = result.parser_memory.live_blocks == 0U &&
                                 result.parser_memory.live_requested_bytes == 0U &&
                                 (result.parser_memory.allocation_attempts == 0U ||
                                  result.parser_memory.allocator_contract_ok);
    const bool passed = code_matches && pointer_matches && metric_matches && cleanup_matches;
    if (!passed) {
      ++failures_;
    }

    std::cout << "RESOURCE_PROFILE_CASE profile=" << profile_.profile_id
              << " status=" << profile_.status << " generator_id=pae-json-resource-v0.1/"
              << profile_.profile_id << '/' << dimension << '/' << boundary
              << " dimension=" << dimension << " boundary=" << boundary
              << " input_bytes=" << input.size() << " expected=" << ToString(expected_code)
              << " actual=" << ToString(result.code) << " reason=" << ToString(result.reason)
              << " expected_pointer=" << (expected_pointer_present ? "PRESENT" : "ABSENT")
              << " actual_pointer=";
    if (!result.has_json_pointer) {
      std::cout << "ABSENT";
    } else if (result.json_pointer.empty()) {
      std::cout << "<root>";
    } else {
      std::cout << result.json_pointer;
    }
    std::cout << " expected_metric=";
    if (metric == nullptr) {
      std::cout << "NA";
    } else {
      std::cout << expected_metric;
    }
    std::cout << " actual_metric=";
    if (metric == nullptr) {
      std::cout << "NA";
    } else {
      std::cout << result.stats.*metric;
    }
    std::cout << " nodes=" << result.stats.node_count << " depth=" << result.stats.max_depth
              << " object_members=" << result.stats.max_object_members
              << " array_elements=" << result.stats.max_array_elements
              << " max_string_bytes=" << result.stats.max_string_bytes
              << " total_string_bytes=" << result.stats.total_decoded_string_bytes
              << " max_number_token_bytes=" << result.stats.max_number_token_bytes
              << " arena_cap=" << limits_.max_parser_memory_bytes
              << " arena_required_upper=" << result.parser_memory.required_upper_bound_bytes
              << " arena_reserved=" << result.parser_memory.reserved_bytes
              << " parser_peak_requested=" << result.parser_memory.peak_live_requested_bytes
              << " live_blocks=" << result.parser_memory.live_blocks
              << " pass=" << (passed ? "true" : "false") << '\n';
    return result;
  }

  void RecordExternalCheck(std::string_view name, bool passed) {
    if (!passed) {
      ++failures_;
    }
    std::cout << "RESOURCE_PROFILE_CHECK profile=" << profile_.profile_id << " name=" << name
              << " pass=" << (passed ? "true" : "false") << '\n';
  }

  std::size_t failures() const noexcept { return failures_; }

 private:
  const resource_profiles_generated::Record& profile_;
  Limits limits_;
  std::size_t failures_ = 0U;
};

const resource_profiles_generated::Record* FindResourceProfile(std::string_view profile_id) {
  for (const auto& profile : resource_profiles_generated::kRecords) {
    if (profile_id == profile.profile_id) {
      return &profile;
    }
  }
  return nullptr;
}

int RunResourceProfileGate(std::string_view profile_id) {
  const resource_profiles_generated::Record* profile = FindResourceProfile(profile_id);
  if (profile == nullptr) {
    std::cerr << "Unknown resource profile: " << profile_id << '\n';
    return 2;
  }

  const Limits limits = MakeResourceLimits(*profile);
  ResourceProfileGate gate{*profile, limits};

  gate.Check("depth", "exact", MakeNestedArrayValue(limits.max_depth - 1U), ErrorCode::kOk, false,
             &DocumentStats::max_depth, limits.max_depth);
  gate.Check("depth", "over", MakeNestedArrayValue(limits.max_depth), ErrorCode::kDepthLimit, true);

  gate.Check("object_members", "exact", MakeObjectValue(limits.max_object_members), ErrorCode::kOk,
             false, &DocumentStats::max_object_members, limits.max_object_members);
  gate.Check("object_members", "over", MakeObjectValue(limits.max_object_members + 1U),
             ErrorCode::kObjectLimit, true);

  gate.Check("array_elements", "exact", MakeArrayValue(limits.max_array_elements), ErrorCode::kOk,
             false, &DocumentStats::max_array_elements, limits.max_array_elements);
  gate.Check("array_elements", "over", MakeArrayValue(limits.max_array_elements + 1U),
             ErrorCode::kArrayLimit, true);

  gate.Check("single_string", "exact", "\"" + std::string(limits.max_string_bytes, 'x') + "\"",
             ErrorCode::kOk, false, &DocumentStats::max_string_bytes, limits.max_string_bytes);
  gate.Check("single_string", "over", "\"" + std::string(limits.max_string_bytes + 1U, 'x') + "\"",
             ErrorCode::kStringLimit, true);

  gate.Check("number_token", "exact", "1" + std::string(limits.max_number_token_bytes - 1U, '0'),
             ErrorCode::kOk, false, &DocumentStats::max_number_token_bytes,
             limits.max_number_token_bytes);
  gate.Check("number_token", "over", "1" + std::string(limits.max_number_token_bytes, '0'),
             ErrorCode::kNumberTokenLimit, true);

  gate.Check("nodes", "exact", MakeNodeForestValue(limits.max_nodes, limits.max_array_elements),
             ErrorCode::kOk, false, &DocumentStats::node_count, limits.max_nodes);
  gate.Check("nodes", "over", MakeNodeForestValue(limits.max_nodes + 1U, limits.max_array_elements),
             ErrorCode::kNodeLimit, true);

  gate.Check("total_decoded_strings", "exact",
             MakeStringArrayValue(limits.max_total_decoded_string_bytes, limits.max_string_bytes),
             ErrorCode::kOk, false, &DocumentStats::total_decoded_string_bytes,
             limits.max_total_decoded_string_bytes);
  gate.Check(
      "total_decoded_strings", "over",
      MakeStringArrayValue(limits.max_total_decoded_string_bytes + 1U, limits.max_string_bytes),
      ErrorCode::kTotalStringLimit, true);

  const std::string input_exact = MakeInputSizedValue(limits.max_input_bytes);
  const ParseResult input_exact_result =
      gate.Check("input_bytes", "exact", input_exact, ErrorCode::kOk, false);
  gate.Check("input_bytes", "over", input_exact + " ", ErrorCode::kInputLimit, false);

  const bool arena_cap_sufficient = input_exact_result.code == ErrorCode::kOk &&
                                    input_exact_result.parser_memory.required_upper_bound_bytes <=
                                        limits.max_parser_memory_bytes &&
                                    input_exact_result.parser_memory.reserved_bytes ==
                                        input_exact_result.parser_memory.required_upper_bound_bytes;
  gate.RecordExternalCheck("ARENA_CAP_SUFFICIENT", arena_cap_sufficient);

  Limits exhausted_limits = limits;
  exhausted_limits.max_parser_memory_bytes = 64U;
  const ParseResult exhausted_result = ParseStrictAuditOnly("null", exhausted_limits);
  const bool arena_hard_bound =
      exhausted_result.code == ErrorCode::kResourceExhausted &&
      exhausted_result.parser_memory.hard_limit_enforced &&
      exhausted_result.parser_memory.reserved_bytes <= exhausted_limits.max_parser_memory_bytes;
  gate.RecordExternalCheck("ARENA_HARD_BOUND_ENFORCED", arena_hard_bound);
  const bool exhaustion_cleanup = exhausted_result.parser_memory.live_blocks == 0U &&
                                  exhausted_result.parser_memory.live_requested_bytes == 0U;
  gate.RecordExternalCheck("ARENA_EXHAUSTION_CLEANUP_PASS", exhaustion_cleanup);

  const bool passed = gate.failures() == 0U;
  std::cout << "RESOURCE_PROFILE_SUMMARY profile=" << profile->profile_id
            << " status=" << profile->status << " scope=PARSER_PREFLIGHT_YYJSON_BOUNDED_TREE_AUDIT"
            << " structural_profile_gate=" << (passed ? "PASS" : "FAIL")
            << " candidate_capacity_probe=" << (passed ? "PASS" : "FAIL")
            << " capacity_freeze_state=OPEN"
            << " hash_manifest_state=OPEN"
            << " loader_compiler_peak_cap=" << profile->max_loader_compiler_peak_bytes
            << " loader_compiler_peak_bytes=NOT_MEASURED"
            << " schema_ir=NOT_BUILT plan_bundle=NOT_BUILT runtime_registration=NOT_RUN"
            << " linux_gate=DEFERRED failures=" << gate.failures() << '\n';
  return passed ? 0 : 1;
}

int RunNumberContractSelfTest() {
  struct NumberCase {
    const char* name;
    const char* input;
    ErrorCode expected_code;
    ErrorReason expected_reason;
    const char* expected_pointer;
  };

  constexpr NumberCase kCases[] = {
      {"unsigned_negative_zero",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":-0,\"messages\":[]}",
       ErrorCode::kIntegerOutOfRange, ErrorReason::kNegativeTokenForUnsigned, "/byte_offset"},
      {"signed_negative_zero",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],\"probe_int64\":-0}",
       ErrorCode::kOk, ErrorReason::kNone, nullptr},
      {"real_negative_zero",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],\"probe_real64\":-0.0}",
       ErrorCode::kOk, ErrorReason::kNone, nullptr},
      {"real_zero_huge_positive_exponent",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],\"probe_real64\":0e999}",
       ErrorCode::kOk, ErrorReason::kNone, nullptr},
      {"real_zero_huge_negative_exponent",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],\"probe_real64\":0e-4000}",
       ErrorCode::kOk, ErrorReason::kNone, nullptr},
      {"real_overflow",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],\"probe_real64\":1e999}",
       ErrorCode::kRealOutOfRange, ErrorReason::kRealOverflow, "/probe_real64"},
      {"real_underflow",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],\"probe_real64\":1e-4000}",
       ErrorCode::kRealOutOfRange, ErrorReason::kRealUnderflow, "/probe_real64"},
      {"real_min_subnormal",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0,\"messages\":[],"
       "\"probe_real64\":4.9406564584124654e-324}",
       ErrorCode::kOk, ErrorReason::kNone, nullptr},
      {"real_token_for_integer",
       "{\"schema_version\":\"0.1\",\"protocol_id\":\"number-contract\","
       "\"byte_offset\":0e999,\"messages\":[]}",
       ErrorCode::kIntegerNotExact, ErrorReason::kNone, "/byte_offset"},
  };

  const Limits limits;
  std::size_t failures = 0U;
  for (const NumberCase& test_case : kCases) {
    const ParseResult result = ParseStrict(test_case.input, limits);
    const bool pointer_matches =
        test_case.expected_pointer == nullptr
            ? !result.has_json_pointer
            : result.has_json_pointer && result.json_pointer == test_case.expected_pointer;
    const bool passed = result.code == test_case.expected_code &&
                        result.reason == test_case.expected_reason && pointer_matches &&
                        result.parser_memory.live_blocks == 0U &&
                        result.parser_memory.live_requested_bytes == 0U;
    if (!passed) {
      ++failures;
    }
    std::cout << "NUMBER_CONTRACT_CASE name=" << test_case.name
              << " expected=" << ToString(test_case.expected_code)
              << " actual=" << ToString(result.code)
              << " expected_reason=" << ToString(test_case.expected_reason)
              << " actual_reason=" << ToString(result.reason) << " expected_pointer="
              << (test_case.expected_pointer == nullptr ? "ABSENT" : test_case.expected_pointer)
              << " actual_pointer=";
    if (!result.has_json_pointer) {
      std::cout << "ABSENT";
    } else if (result.json_pointer.empty()) {
      std::cout << "<root>";
    } else {
      std::cout << result.json_pointer;
    }
    std::cout << " pass=" << (passed ? "true" : "false") << '\n';
  }

  Limits token_limits = limits;
  token_limits.max_number_token_bytes = 64U;
  const std::string exact_token = "1" + std::string(63U, '0');
  const std::string over_token = exact_token + "0";
  const ParseResult exact_result = ParseStrictAuditOnly(exact_token, token_limits);
  const ParseResult over_result = ParseStrictAuditOnly(over_token, token_limits);
  const bool token_limit_passed =
      exact_result.code == ErrorCode::kOk &&
      exact_result.stats.max_number_token_bytes == token_limits.max_number_token_bytes &&
      over_result.code == ErrorCode::kNumberTokenLimit &&
      over_result.byte_offset == kNoByteOffset && over_result.has_json_pointer &&
      over_result.json_pointer.empty();
  if (!token_limit_passed) {
    ++failures;
  }
  std::cout << "NUMBER_CONTRACT_CASE name=number_token_limit exact=" << ToString(exact_result.code)
            << " over=" << ToString(over_result.code)
            << " exact_bytes=" << exact_result.stats.max_number_token_bytes
            << " over_bytes=" << over_result.stats.max_number_token_bytes
            << " byte_offset=" << (over_result.byte_offset == kNoByteOffset ? "ABSENT" : "PRESENT")
            << " json_pointer="
            << (!over_result.has_json_pointer
                    ? "ABSENT"
                    : (over_result.json_pointer.empty() ? "<root>" : over_result.json_pointer))
            << " pass=" << (token_limit_passed ? "true" : "false") << '\n';

  std::cout << "NUMBER_CONTRACT_SUMMARY cases=" << (std::size(kCases) + 1U)
            << " failures=" << failures << " gate=" << (failures == 0U ? "PASS" : "FAIL")
            << " scope=YYJSON_RAW_NUMBER_TO_STRUCTURAL_DIAGNOSTIC"
            << " production_parser_selection=OPEN linux_gate=DEFERRED\n";
  return failures == 0U ? 0 : 1;
}

}  // namespace
}  // namespace pae::json_spike

int main(int argc, char* argv[]) {
  if (argc == 2 && std::string_view{argv[1]} == "--allocator-self-test") {
    return pae::json_spike::RunAllocatorSelfTest();
  }
  if (argc == 2 && std::string_view{argv[1]} == "--number-contract-self-test") {
    return pae::json_spike::RunNumberContractSelfTest();
  }
  if (argc == 3 && std::string_view{argv[1]} == "--resource-profile-gate") {
    return pae::json_spike::RunResourceProfileGate(argv[2]);
  }
  return pae::json_spike::RunCandidate("yyjson", YYJSON_VERSION_STRING,
                                       &pae::json_spike::ParseStrict, argc, argv);
}
