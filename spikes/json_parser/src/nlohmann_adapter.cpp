#include <algorithm>
#include <cstdint>
#include <limits>
#include <new>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "spike_common.h"

namespace pae::json_spike {
namespace {

using Json = nlohmann::json;

struct DuplicateKeyError final {
  int depth = 0;
  std::string key;
};

ParseResult RejectWithStatsAtPath(ErrorCode code, const DocumentStats& stats,
                                  std::string_view json_pointer) {
  ParseResult result = RejectedAtPath(code, json_pointer);
  result.stats = stats;
  return result;
}

bool IsAllowedRootKey(std::string_view key) noexcept {
  return key == "schema_version" || key == "protocol_id" || key == "byte_offset" ||
         key == "messages" || key == "payload" || key == "probe_uint64" || key == "probe_int64" ||
         key == "probe_known_id" || key == "probe_reference_id" || key == "probe_range_start" ||
         key == "probe_range_end";
}

ParseResult AuditValue(const Json& value, std::size_t depth, const Limits& limits,
                       DocumentStats& stats, std::string& json_pointer) {
  if (depth > limits.max_depth) {
    return RejectWithStatsAtPath(ErrorCode::kDepthLimit, stats, json_pointer);
  }
  if (stats.node_count >= limits.max_nodes) {
    return RejectWithStatsAtPath(ErrorCode::kNodeLimit, stats, json_pointer);
  }

  ++stats.node_count;
  stats.max_depth = (std::max)(stats.max_depth, depth);

  if (value.is_number()) {
    ++stats.number_token_count;
  }

  if (value.is_string()) {
    const std::size_t length = value.get_ref<const Json::string_t&>().size();
    stats.max_string_bytes = (std::max)(stats.max_string_bytes, length);
    if (length > limits.max_string_bytes) {
      return RejectWithStatsAtPath(ErrorCode::kStringLimit, stats, json_pointer);
    }
    return Accepted(stats);
  }

  if (value.is_array()) {
    const std::size_t element_count = value.size();
    stats.max_array_elements = (std::max)(stats.max_array_elements, element_count);
    if (element_count > limits.max_array_elements) {
      return RejectWithStatsAtPath(ErrorCode::kArrayLimit, stats, json_pointer);
    }
    std::size_t element_index = 0U;
    for (const Json& element : value) {
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

  if (value.is_object()) {
    for (auto iterator = value.cbegin(); iterator != value.cend(); ++iterator) {
      const std::size_t key_length = iterator.key().size();
      stats.max_string_bytes = (std::max)(stats.max_string_bytes, key_length);
      if (key_length > limits.max_string_bytes) {
        const std::size_t previous_size = AppendJsonPointerToken(json_pointer, iterator.key());
        ParseResult result = RejectWithStatsAtPath(ErrorCode::kStringLimit, stats, json_pointer);
        RestoreJsonPointer(json_pointer, previous_size);
        return result;
      }

      const std::size_t previous_size = AppendJsonPointerToken(json_pointer, iterator.key());
      const ParseResult child =
          AuditValue(iterator.value(), depth + 1U, limits, stats, json_pointer);
      RestoreJsonPointer(json_pointer, previous_size);
      if (child.code != ErrorCode::kOk) {
        return child;
      }
    }
  }

  return Accepted(stats);
}

ParseResult ValidateProbeSchema(const Json& root, const DocumentStats& stats) {
  if (!root.is_object()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "");
  }

  for (auto iterator = root.cbegin(); iterator != root.cend(); ++iterator) {
    if (!IsAllowedRootKey(iterator.key())) {
      std::string json_pointer;
      AppendJsonPointerToken(json_pointer, iterator.key());
      return RejectWithStatsAtPath(ErrorCode::kUnknownField, stats, json_pointer);
    }
  }

  if (!root.contains("schema_version")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/schema_version");
  }
  if (!root.contains("protocol_id")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/protocol_id");
  }
  if (!root.contains("byte_offset")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/byte_offset");
  }
  if (!root.contains("messages")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/messages");
  }

  if (!root.at("schema_version").is_string()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/schema_version");
  }
  if (!root.at("protocol_id").is_string()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/protocol_id");
  }
  if (!root.at("messages").is_array()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/messages");
  }

  const Json& byte_offset = root.at("byte_offset");
  if (byte_offset.is_number_unsigned()) {
    // Accepted without a signed conversion.
  } else if (byte_offset.is_number_integer()) {
    if (byte_offset.get<std::int64_t>() < 0) {
      return RejectWithStatsAtPath(ErrorCode::kIntegerOutOfRange, stats, "/byte_offset");
    }
  } else if (byte_offset.is_number()) {
    return RejectWithStatsAtPath(ErrorCode::kIntegerNotExact, stats, "/byte_offset");
  } else {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/byte_offset");
  }

  if (root.contains("probe_uint64")) {
    const Json& probe_uint64 = root.at("probe_uint64");
    if (!probe_uint64.is_number_unsigned()) {
      return RejectWithStatsAtPath(
          probe_uint64.is_number() ? ErrorCode::kIntegerNotExact : ErrorCode::kTypeMismatch, stats,
          "/probe_uint64");
    }
  }

  if (root.contains("probe_int64")) {
    const Json& probe_int64 = root.at("probe_int64");
    if (probe_int64.is_number_unsigned()) {
      const std::uint64_t signed_max =
          static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)());
      if (probe_int64.get<std::uint64_t>() > signed_max) {
        return RejectWithStatsAtPath(ErrorCode::kIntegerOutOfRange, stats, "/probe_int64");
      }
    } else if (probe_int64.is_number_integer()) {
      // Accepted as an exact signed integer.
    } else {
      return RejectWithStatsAtPath(
          probe_int64.is_number() ? ErrorCode::kIntegerNotExact : ErrorCode::kTypeMismatch, stats,
          "/probe_int64");
    }
  }

  const bool has_known_id = root.contains("probe_known_id");
  const bool has_reference_id = root.contains("probe_reference_id");
  if (has_known_id || has_reference_id) {
    if (!has_known_id) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_known_id");
    }
    if (!has_reference_id) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_reference_id");
    }
    const Json& known_id = root.at("probe_known_id");
    const Json& reference_id = root.at("probe_reference_id");
    if (!known_id.is_string()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_known_id");
    }
    if (!reference_id.is_string()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_reference_id");
    }
    if (known_id.get_ref<const Json::string_t&>() !=
        reference_id.get_ref<const Json::string_t&>()) {
      return RejectWithStatsAtPath(ErrorCode::kReferenceNotFound, stats, "/probe_reference_id");
    }
  }

  const bool has_range_start = root.contains("probe_range_start");
  const bool has_range_end = root.contains("probe_range_end");
  if (has_range_start || has_range_end) {
    if (!has_range_start) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_range_start");
    }
    if (!has_range_end) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_range_end");
    }
    const Json& range_start = root.at("probe_range_start");
    const Json& range_end = root.at("probe_range_end");
    if (!range_start.is_number_unsigned()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_range_start");
    }
    if (!range_end.is_number_unsigned()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_range_end");
    }
    if (range_start.get<std::uint64_t>() > range_end.get<std::uint64_t>()) {
      return RejectWithStatsAtPath(ErrorCode::kSemanticConstraint, stats, "/probe_range_start");
    }
  }

  return Accepted(stats);
}

ParseResult ParseStrict(std::string_view input, const Limits& limits) {
  try {
    ParseResult preflight;
    if (!RunCommonPreflight(input, limits, preflight)) {
      return preflight;
    }

    std::unordered_map<int, std::unordered_set<std::string>> keys_by_depth;
    const Json::parser_callback_t callback = [&keys_by_depth](int depth, Json::parse_event_t event,
                                                              Json& parsed) {
      if (event == Json::parse_event_t::object_start) {
        keys_by_depth[depth + 1].clear();
      } else if (event == Json::parse_event_t::key) {
        auto& keys = keys_by_depth[depth];
        const auto [unused, inserted] = keys.emplace(parsed.get_ref<const Json::string_t&>());
        static_cast<void>(unused);
        if (!inserted) {
          throw DuplicateKeyError{depth, parsed.get_ref<const Json::string_t&>()};
        }
      } else if (event == Json::parse_event_t::object_end) {
        keys_by_depth.erase(depth + 1);
      }
      return true;
    };

    const Json root = Json::parse(input.begin(), input.end(), callback, true, false);
    DocumentStats stats;
    std::string json_pointer;
    const ParseResult audit = AuditValue(root, 1U, limits, stats, json_pointer);
    if (audit.code != ErrorCode::kOk) {
      return audit;
    }
    return ValidateProbeSchema(root, stats);
  } catch (const DuplicateKeyError& error) {
    if (error.depth == 1) {
      std::string json_pointer;
      AppendJsonPointerToken(json_pointer, error.key);
      return RejectedAtPath(ErrorCode::kDuplicateKey, json_pointer);
    }
    return Rejected(ErrorCode::kDuplicateKey);
  } catch (const Json::parse_error& error) {
    const std::size_t byte_offset =
        error.byte == 0U ? kNoByteOffset : static_cast<std::size_t>(error.byte - 1U);
    return Rejected(ErrorCode::kSyntaxError, byte_offset);
  } catch (const Json::out_of_range&) {
    return Rejected(ErrorCode::kIntegerNotExact);
  } catch (const std::bad_alloc&) {
    return Rejected(ErrorCode::kResourceExhausted);
  } catch (...) {
    return Rejected(ErrorCode::kInternalError);
  }
}

#define PAE_JSON_SPIKE_STRINGIFY_DETAIL(value) #value
#define PAE_JSON_SPIKE_STRINGIFY(value) PAE_JSON_SPIKE_STRINGIFY_DETAIL(value)

constexpr char kNlohmannVersion[] =
    PAE_JSON_SPIKE_STRINGIFY(NLOHMANN_JSON_VERSION_MAJOR) "." PAE_JSON_SPIKE_STRINGIFY(
        NLOHMANN_JSON_VERSION_MINOR) "." PAE_JSON_SPIKE_STRINGIFY(NLOHMANN_JSON_VERSION_PATCH);

#undef PAE_JSON_SPIKE_STRINGIFY
#undef PAE_JSON_SPIKE_STRINGIFY_DETAIL

}  // namespace
}  // namespace pae::json_spike

int main(int argc, char* argv[]) {
  return pae::json_spike::RunCandidate("nlohmann_json", pae::json_spike::kNlohmannVersion,
                                       &pae::json_spike::ParseStrict, argc, argv);
}
