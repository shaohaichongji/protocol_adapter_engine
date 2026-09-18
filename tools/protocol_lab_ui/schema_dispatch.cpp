#include "schema_dispatch.h"

#include <yyjson.h>

#include <cstddef>
#include <memory>
#include <new>

namespace pae::protocol_lab_ui {
namespace {
constexpr std::size_t kMaximumInputBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaximumParserBytes = 64U * 1024U * 1024U;
constexpr std::size_t kMaximumDepth = 64U;
constexpr std::size_t kMaximumNodes = 524288U;
constexpr std::size_t kMaximumObjectMembers = 4096U;
constexpr std::size_t kMaximumArrayElements = 16384U;
constexpr yyjson_read_flag kReadFlags = YYJSON_READ_NUMBER_AS_RAW;

struct DocumentDeleter {
  void operator()(yyjson_doc* document) const noexcept { yyjson_doc_free(document); }
};

bool AuditBounds(yyjson_val* value, std::size_t depth, std::size_t& nodes) noexcept {
  if (!value || depth > kMaximumDepth || ++nodes > kMaximumNodes) return false;
  if (yyjson_is_obj(value)) {
    if (yyjson_obj_size(value) > kMaximumObjectMembers) return false;
    yyjson_obj_iter iterator;
    yyjson_obj_iter_init(value, &iterator);
    while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
      if (++nodes > kMaximumNodes || !AuditBounds(yyjson_obj_iter_get_val(key), depth + 1U, nodes))
        return false;
    }
  } else if (yyjson_is_arr(value)) {
    if (yyjson_arr_size(value) > kMaximumArrayElements) return false;
    yyjson_arr_iter iterator;
    yyjson_arr_iter_init(value, &iterator);
    while (yyjson_val* child = yyjson_arr_iter_next(&iterator))
      if (!AuditBounds(child, depth + 1U, nodes)) return false;
  }
  return true;
}

SchemaDispatchResult Fail(const char* detail) {
  return {SchemaDispatchStatus::CLASSIFICATION_FAILED, detail};
}
}  // namespace

SchemaDispatchResult ClassifySchemaVersion(std::string_view json_bytes) {
  if (json_bytes.empty() || json_bytes.size() > kMaximumInputBytes)
    return Fail("schema dispatch input size is outside the bounded limit");
  const auto parser_bytes = yyjson_read_max_memory_usage(json_bytes.size(), kReadFlags);
  if (parser_bytes == 0U || parser_bytes > kMaximumParserBytes)
    return Fail("schema dispatch parser budget exceeded");
  auto memory = std::unique_ptr<unsigned char[]>(new (std::nothrow) unsigned char[parser_bytes]);
  if (!memory) return Fail("schema dispatch parser allocation failed");
  yyjson_alc allocator{};
  if (!yyjson_alc_pool_init(&allocator, memory.get(), parser_bytes))
    return Fail("schema dispatch bounded parser pool initialization failed");
  yyjson_read_err error{};
  auto document = std::unique_ptr<yyjson_doc, DocumentDeleter>{yyjson_read_opts(
      const_cast<char*>(json_bytes.data()), json_bytes.size(), kReadFlags, &allocator, &error)};
  if (!document) return Fail("schema dispatch rejected JSON syntax or parser memory");
  yyjson_val* root = yyjson_doc_get_root(document.get());
  if (!yyjson_is_obj(root)) return Fail("schema dispatch requires a root JSON object");
  std::size_t nodes = 0U;
  if (!AuditBounds(root, 1U, nodes)) return Fail("schema dispatch JSON structure limit exceeded");

  yyjson_val* version = nullptr;
  yyjson_obj_iter iterator;
  yyjson_obj_iter_init(root, &iterator);
  while (yyjson_val* key = yyjson_obj_iter_next(&iterator)) {
    const std::string_view name{yyjson_get_str(key), yyjson_get_len(key)};
    if (name != "schema_version") continue;
    if (version) return Fail("duplicate root schema_version member");
    version = yyjson_obj_iter_get_val(key);
  }
  if (!version) return Fail("missing root schema_version member");
  if (!yyjson_is_str(version)) return Fail("root schema_version must be a string");
  const std::string_view value{yyjson_get_str(version), yyjson_get_len(version)};
  if (value == "0.9") return {SchemaDispatchStatus::BINARY_PUBLIC, {}};
  if (value == "0.5" || value == "0.6" || value == "0.7" || value == "0.8")
    return {SchemaDispatchStatus::PRIVATE_LEGACY, {}};
#if defined(PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC)
  if (value == "0.10")
    return {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2)
        SchemaDispatchStatus::ASCII_PUBLIC,
#else
        SchemaDispatchStatus::PRIVATE_ASCII,
#endif
        {}};
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER)
  if (value == "0.11")
    return {
#if defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI)
        SchemaDispatchStatus::ASCII_PUBLIC,
#else
        SchemaDispatchStatus::PRIVATE_ASCII,
#endif
        {}};
#endif
  return Fail("root schema_version is unsupported for this UI build");
}
}  // namespace pae::protocol_lab_ui
