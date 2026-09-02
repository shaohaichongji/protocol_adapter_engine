#include <rapidjson/document.h>
#include <rapidjson/error/error.h>
#include <rapidjson/rapidjson.h>

#include <algorithm>
#include <cstdint>
#include <new>
#include <string>
#include <string_view>
#include <unordered_set>

#include "spike_common.h"

namespace pae::json_spike {
namespace {

using JsonValue = rapidjson::Value;

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

ParseResult AuditValue(const JsonValue& value, std::size_t depth, const Limits& limits,
                       DocumentStats& stats, std::string& json_pointer) {
  if (depth > limits.max_depth) {
    return RejectWithStatsAtPath(ErrorCode::kDepthLimit, stats, json_pointer);
  }
  if (stats.node_count >= limits.max_nodes) {
    return RejectWithStatsAtPath(ErrorCode::kNodeLimit, stats, json_pointer);
  }

  ++stats.node_count;
  stats.max_depth = (std::max)(stats.max_depth, depth);

  if (value.IsNumber()) {
    ++stats.number_token_count;
  }

  if (value.IsString()) {
    const std::size_t length = value.GetStringLength();
    stats.max_string_bytes = (std::max)(stats.max_string_bytes, length);
    if (length > limits.max_string_bytes) {
      return RejectWithStatsAtPath(ErrorCode::kStringLimit, stats, json_pointer);
    }
    return Accepted(stats);
  }

  if (value.IsArray()) {
    const std::size_t element_count = value.Size();
    stats.max_array_elements = (std::max)(stats.max_array_elements, element_count);
    if (element_count > limits.max_array_elements) {
      return RejectWithStatsAtPath(ErrorCode::kArrayLimit, stats, json_pointer);
    }
    std::size_t element_index = 0U;
    for (auto iterator = value.Begin(); iterator != value.End(); ++iterator) {
      const std::size_t previous_size = AppendJsonPointerIndex(json_pointer, element_index);
      const ParseResult child = AuditValue(*iterator, depth + 1U, limits, stats, json_pointer);
      RestoreJsonPointer(json_pointer, previous_size);
      if (child.code != ErrorCode::kOk) {
        return child;
      }
      ++element_index;
    }
    return Accepted(stats);
  }

  if (value.IsObject()) {
    std::unordered_set<std::string> keys;
    keys.reserve(value.MemberCount());
    for (auto iterator = value.MemberBegin(); iterator != value.MemberEnd(); ++iterator) {
      const std::size_t key_length = iterator->name.GetStringLength();
      stats.max_string_bytes = (std::max)(stats.max_string_bytes, key_length);
      if (key_length > limits.max_string_bytes) {
        const std::size_t previous_size = AppendJsonPointerToken(
            json_pointer,
            std::string_view{iterator->name.GetString(), iterator->name.GetStringLength()});
        ParseResult result = RejectWithStatsAtPath(ErrorCode::kStringLimit, stats, json_pointer);
        RestoreJsonPointer(json_pointer, previous_size);
        return result;
      }

      const auto [unused, inserted] =
          keys.emplace(iterator->name.GetString(), iterator->name.GetStringLength());
      static_cast<void>(unused);
      if (!inserted) {
        const std::size_t previous_size = AppendJsonPointerToken(
            json_pointer,
            std::string_view{iterator->name.GetString(), iterator->name.GetStringLength()});
        ParseResult result = RejectWithStatsAtPath(ErrorCode::kDuplicateKey, stats, json_pointer);
        RestoreJsonPointer(json_pointer, previous_size);
        return result;
      }
    }

    for (auto iterator = value.MemberBegin(); iterator != value.MemberEnd(); ++iterator) {
      const std::size_t previous_size = AppendJsonPointerToken(
          json_pointer,
          std::string_view{iterator->name.GetString(), iterator->name.GetStringLength()});
      const ParseResult child =
          AuditValue(iterator->value, depth + 1U, limits, stats, json_pointer);
      RestoreJsonPointer(json_pointer, previous_size);
      if (child.code != ErrorCode::kOk) {
        return child;
      }
    }
  }

  return Accepted(stats);
}

ParseResult ValidateProbeSchema(const JsonValue& root, const DocumentStats& stats) {
  if (!root.IsObject()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "");
  }

  for (auto iterator = root.MemberBegin(); iterator != root.MemberEnd(); ++iterator) {
    const std::string_view key{iterator->name.GetString(), iterator->name.GetStringLength()};
    if (!IsAllowedRootKey(key)) {
      std::string json_pointer;
      AppendJsonPointerToken(json_pointer, key);
      return RejectWithStatsAtPath(ErrorCode::kUnknownField, stats, json_pointer);
    }
  }

  if (!root.HasMember("schema_version")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/schema_version");
  }
  if (!root.HasMember("protocol_id")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/protocol_id");
  }
  if (!root.HasMember("byte_offset")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/byte_offset");
  }
  if (!root.HasMember("messages")) {
    return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/messages");
  }

  if (!root["schema_version"].IsString()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/schema_version");
  }
  if (!root["protocol_id"].IsString()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/protocol_id");
  }
  if (!root["messages"].IsArray()) {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/messages");
  }

  const JsonValue& byte_offset = root["byte_offset"];
  if (byte_offset.IsUint64()) {
    // Accepted without a signed conversion.
  } else if (byte_offset.IsInt64()) {
    if (byte_offset.GetInt64() < 0) {
      return RejectWithStatsAtPath(ErrorCode::kIntegerOutOfRange, stats, "/byte_offset");
    }
  } else if (byte_offset.IsNumber()) {
    return RejectWithStatsAtPath(ErrorCode::kIntegerNotExact, stats, "/byte_offset");
  } else {
    return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/byte_offset");
  }

  if (root.HasMember("probe_uint64")) {
    const JsonValue& probe_uint64 = root["probe_uint64"];
    if (!probe_uint64.IsUint64()) {
      return RejectWithStatsAtPath(
          probe_uint64.IsNumber() ? ErrorCode::kIntegerNotExact : ErrorCode::kTypeMismatch, stats,
          "/probe_uint64");
    }
  }

  if (root.HasMember("probe_int64")) {
    const JsonValue& probe_int64 = root["probe_int64"];
    if (probe_int64.IsInt64()) {
      // Accepted as an exact signed integer.
    } else if (probe_int64.IsUint64()) {
      return RejectWithStatsAtPath(ErrorCode::kIntegerOutOfRange, stats, "/probe_int64");
    } else {
      return RejectWithStatsAtPath(
          probe_int64.IsNumber() ? ErrorCode::kIntegerNotExact : ErrorCode::kTypeMismatch, stats,
          "/probe_int64");
    }
  }

  const bool has_known_id = root.HasMember("probe_known_id");
  const bool has_reference_id = root.HasMember("probe_reference_id");
  if (has_known_id || has_reference_id) {
    if (!has_known_id) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_known_id");
    }
    if (!has_reference_id) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_reference_id");
    }
    const JsonValue& known_id = root["probe_known_id"];
    const JsonValue& reference_id = root["probe_reference_id"];
    if (!known_id.IsString()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_known_id");
    }
    if (!reference_id.IsString()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_reference_id");
    }
    const std::string_view known{known_id.GetString(), known_id.GetStringLength()};
    const std::string_view reference{reference_id.GetString(), reference_id.GetStringLength()};
    if (known != reference) {
      return RejectWithStatsAtPath(ErrorCode::kReferenceNotFound, stats, "/probe_reference_id");
    }
  }

  const bool has_range_start = root.HasMember("probe_range_start");
  const bool has_range_end = root.HasMember("probe_range_end");
  if (has_range_start || has_range_end) {
    if (!has_range_start) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_range_start");
    }
    if (!has_range_end) {
      return RejectWithStatsAtPath(ErrorCode::kMissingField, stats, "/probe_range_end");
    }
    const JsonValue& range_start = root["probe_range_start"];
    const JsonValue& range_end = root["probe_range_end"];
    if (!range_start.IsUint64()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_range_start");
    }
    if (!range_end.IsUint64()) {
      return RejectWithStatsAtPath(ErrorCode::kTypeMismatch, stats, "/probe_range_end");
    }
    if (range_start.GetUint64() > range_end.GetUint64()) {
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

    rapidjson::Document document;
    document.Parse<rapidjson::kParseValidateEncodingFlag>(input.data(), input.size());
    if (document.HasParseError()) {
      const ErrorCode code = document.GetParseError() == rapidjson::kParseErrorNumberTooBig
                                 ? ErrorCode::kIntegerNotExact
                                 : ErrorCode::kSyntaxError;
      return Rejected(code, document.GetErrorOffset());
    }

    DocumentStats stats;
    std::string json_pointer;
    const ParseResult audit = AuditValue(document, 1U, limits, stats, json_pointer);
    if (audit.code != ErrorCode::kOk) {
      return audit;
    }
    return ValidateProbeSchema(document, stats);
  } catch (const std::bad_alloc&) {
    return Rejected(ErrorCode::kResourceExhausted);
  } catch (...) {
    return Rejected(ErrorCode::kInternalError);
  }
}

}  // namespace
}  // namespace pae::json_spike

int main(int argc, char* argv[]) {
  return pae::json_spike::RunCandidate("rapidjson", RAPIDJSON_VERSION_STRING,
                                       &pae::json_spike::ParseStrict, argc, argv);
}
