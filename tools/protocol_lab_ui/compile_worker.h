#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#if defined(PAE_BUILD_PROTOCOL_LAB_YAML_ENTRY)
#include "pae/yaml_frontend.h"
#endif

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
#include "../../src/config_compiler/config_compiler.h"
#endif
#include "schema_dispatch.h"
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
#include "pae/compiler.h"
#endif

namespace pae::protocol_lab_ui {

using DocumentId = std::uint64_t;
using Revision = std::uint64_t;
using ResultTicket = std::uint64_t;

// 输入文本的字节上限；不表示整个结果邮箱或进程 RSS 的硬上限。
inline constexpr std::size_t kMaximumConfigBytes = 4U * 1024U * 1024U;

enum class ConfigSourceFormat { JSON, YAML };

// 自持的诊断展示 DTO；可选位置缺失与偏移 0 含义不同，不能互相替代。
struct CompileDiagnosticView {
  std::string stage;
  std::string code;
  std::string json_pointer;
  std::optional<std::size_t> byte_offset;
  std::string resource_kind;
  bool has_resource_budget = false;
  std::uint64_t required_bytes = 0U;
  std::uint64_t limit_bytes = 0U;
  std::string resource_profile;
  std::string detail;
  bool yaml_source = false;
  std::optional<std::size_t> yaml_line;
  std::optional<std::size_t> yaml_column;
  bool yaml_approximate = false;
};

#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
CompileDiagnosticView ProjectCompileDiagnostic(
    const config_compiler::CompileDiagnostic& diagnostic);
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) ||       \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) ||        \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
CompileDiagnosticView ProjectCompileDiagnostic(const pae::CompileDiagnostic& diagnostic);
#endif
std::string FormatCompileDiagnostic(const CompileDiagnosticView& diagnostic);
#if defined(PAE_BUILD_PROTOCOL_LAB_YAML_ENTRY)
void AnnotateYamlSource(CompileDiagnosticView& diagnostic,
                        const pae::yaml::ConversionResult& conversion) noexcept;
#endif

// 后台生成、经邮箱移交的唯一结果 owner；原配置身份与编译产物一起送往文档。
// 若输入是 YAML，config_sha256 对应实际编译的生成 JSON，而不是原 YAML 字节。
struct CompileCompletion {
  DocumentId document_id = 0U;
  Revision load_revision = 0U;
  std::string config_sha256;
  struct Diagnostic {
    std::string detail;

    Diagnostic() = default;
    Diagnostic(const char* value) : detail(value) {}
    explicit Diagnostic(std::string value) : detail(std::move(value)) {}
    template <typename Source>
    Diagnostic(const Source& source) : detail(source.detail) {}
  };
#if !defined(PAE_PROTOCOL_LAB_STANDALONE_PUBLIC_ONLY)
  std::unique_ptr<config_compiler::CompiledProtocolArtifacts> artifacts;
#else
  // Keeps existing negative ownership assertions source-compatible without importing the
  // private compiler DTO into the installed-SDK build.
  std::nullptr_t artifacts = nullptr;
#endif
  std::optional<Diagnostic> diagnostic;
  std::optional<CompileDiagnosticView> structured_compile_diagnostic;
  SchemaDispatchStatus route = SchemaDispatchStatus::PRIVATE_LEGACY;
  std::string classification_error;
  std::size_t compiler_attempt_count = 0U;
#if defined(PAE_BUILD_PROTOCOL_LAB_YAML_ENTRY)
  std::unique_ptr<pae::yaml::ConversionResult> yaml_conversion;
#endif
#if defined(PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2) || \
    defined(PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI) || \
    defined(PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE)
  std::unique_ptr<pae::CompiledProtocol> public_compiled;
  std::optional<pae::CompileDiagnostic> public_diagnostic;
#endif
};

enum class SubmitStatus {
  ACCEPTED,
  CONFIG_TOO_LARGE,
  QUEUE_CAPACITY_EXCEEDED,
  DOCUMENT_CLOSED,
  WORKER_STOPPED,
};

// 单后台线程调度转换/编译，不执行 GUI Encode/Inspect，也不管理通信。
// 内部锁保护队列和邮箱，不赋予 DocumentSession 或协议执行对象并发调用能力。
class CompileWorker final {
 public:
  struct Request {
    DocumentId document_id = 0U;
    Revision load_revision = 0U;
    // Submit 复制输入，后台不借用调用者的 string_view；source_identity 用于来源定位。
    std::string config_text;
    ConfigSourceFormat source_format = ConfigSourceFormat::JSON;
    std::string source_identity;
    std::uint64_t enqueue_sequence = 0U;
  };
  using CompileFunction = std::function<std::unique_ptr<CompileCompletion>(Request)>;

  CompileWorker();
  explicit CompileWorker(CompileFunction compile_function);
  CompileWorker(const CompileWorker&) = delete;
  CompileWorker& operator=(const CompileWorker&) = delete;
  // 停止接收/清理待处理结果后 join；正在运行的同步编译必须自行返回，不能强制取消。
  ~CompileWorker();

  // ACCEPTED 只表示入队。最多两个不同文档的 pending；同文档替换 pending 并更新序号。
  // active 不属于 pending 容量，也不会被新提交抢占；YAML 另有更紧的前端输入检查。
  SubmitStatus Submit(DocumentId document_id, Revision load_revision, std::string_view config_text,
                      ConfigSourceFormat source_format = ConfigSourceFormat::JSON,
                      std::string_view source_identity = {});
  // 删除该文档 pending/邮箱结果，拒绝后续提交，并丢弃 active 的晚到结果。
  void CloseDocument(DocumentId document_id);
  // 取走通知但保留邮箱 owner；必须配合 TakeResult，不能把 ticket 当作结果指针。
  std::vector<ResultTicket> DrainReadyTickets();
  // 一次性移交并移除 owner；未知、已关闭或已取走的 ticket 返回 nullptr。
  std::unique_ptr<CompileCompletion> TakeResult(ResultTicket ticket);

  std::size_t PendingCountForTesting() const;
  bool HasActiveRequestForTesting() const;
  std::size_t StoredResultCountForTesting() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> implementation_;
};

}  // namespace pae::protocol_lab_ui
