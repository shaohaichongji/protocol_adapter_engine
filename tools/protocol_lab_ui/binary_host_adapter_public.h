#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "../protocol_lab_binary/public_binary_decode.h"
#include "owned_presentation_types.h"
#include "pae/host_endpoint.h"
#include "ui_field_result.h"

namespace pae::protocol_lab_ui {

struct DocumentDescription;

// endpoint/action/Pipeline 绑定；streams 是该绑定的 Flow 数，Encode 只允许一个 Flow。
struct BinaryHostBinding {
  std::string endpoint;
  pae::HostAction action = pae::HostAction::DECODE;
  std::string pipeline_id;
  std::size_t streams = 1U;
};

// 文档、加载、会话与请求的准备身份，不是帧或 Message 的执行身份。
struct BinaryPreparationIdentity {
  std::uint64_t document = 0U;
  std::uint64_t load = 0U;
  std::uint64_t session = 0U;
  std::uint64_t request = 0U;
  std::string config_sha256;
};

struct BinaryUiDecodeResult {
  std::vector<std::uint8_t> frame;
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<UiFieldResult> fields;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiDecodeFailure {
  pae::HostStatus host_status = pae::HostStatus::INVALID_ARGUMENT;
  pae::CodecStatus codec_status = pae::CodecStatus::INVALID_ARGUMENT;
  std::vector<std::uint8_t> diagnostic_frame;
  std::optional<std::size_t> message_index;
  std::optional<std::size_t> failed_field_index;
  std::size_t accounted_bytes = 0U;
};

// H2 按值返回自有展示 DTO；public_host 保留执行事实，ok 表示成功结果已物化。
// 不同于 H1 的内部结果引用，DTO 的字符串和容器不借用 adapter。
struct BinaryUiDecodeView {
  bool ok = false;
  pae::HostOperationResult public_host;
  std::optional<BinaryUiDecodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};

enum class BinaryStreamPresentationStatus {
  PREFLIGHT_REJECTED,
  NO_CANDIDATE,
  DECODE_SUCCESS,
  DECODE_FAILURE,
  MATERIALIZATION_FAILURE,
};

// Host 消费事实与展示状态分开；NO_CANDIDATE 不等于成功解码一条记录。
struct BinaryUiStreamView {
  BinaryStreamPresentationStatus status = BinaryStreamPresentationStatus::PREFLIGHT_REJECTED;
  protocol_lab_binary::public_decode::LocalStatus local_status =
      protocol_lab_binary::public_decode::LocalStatus::INVALID_INPUT;
  protocol_lab_binary::public_decode::StreamDiagnostic diagnostic =
      protocol_lab_binary::public_decode::StreamDiagnostic::NOT_STREAM;
  bool host_called = false;
  pae::HostOperationResult public_host;
  StreamPresentationObservation before;
  StreamPresentationObservation after;
  std::optional<BinaryUiDecodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};

struct BinaryUiEncodeResult {
  std::vector<std::uint8_t> frame;
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<UiFieldResult> fields;
  std::size_t accounted_bytes = 0U;
};

struct BinaryUiEncodeView {
  bool ok = false;
  pae::HostOperationResult public_host;
  std::optional<BinaryUiEncodeResult> result;
  std::optional<BinaryUiDecodeFailure> failure;
};

// 配置按值保存，但函数指针及 context 不归 adapter 所有，须在调用期间有效。
struct BinaryUiCopyControls {
  std::size_t description_copy_limit = (std::numeric_limits<std::size_t>::max)();
  std::size_t result_copy_limit = (std::numeric_limits<std::size_t>::max)();
  void (*before_description_copy)(void*) = nullptr;
  void (*before_result_copy)(void*) = nullptr;
  void* context = nullptr;
};

const char* PublicBinaryCodecStatusName(pae::CodecStatus status) noexcept;

// Binary H2 application adapter. Owns one public H1/Host instance, never a private Plan/Session.
// 上接 DocumentSession，下接公开 H1/Host；只做描述与结果投影，不另建协议执行引擎。
// 调用、映射、草稿变更和销毁应串行协调；实例编号的 atomic 不提供对象线程安全。
class BinaryHostAdapter final {
 public:
  // 接管 compiled；替换准备计入旧实例与外部共存占用，失败不修改 previous。
  static std::unique_ptr<BinaryHostAdapter> CreatePublic(
      pae::CompiledProtocol compiled, std::vector<BinaryHostBinding> bindings,
      BinaryPreparationIdentity identity, const BinaryHostAdapter* previous,
      std::size_t externally_retained_bytes, std::size_t preparation_coexisting_bytes,
      std::string& error, const protocol_lab_binary::public_decode::Limits& limits = {},
      const BinaryUiCopyControls* copy_controls = nullptr);
  BinaryHostAdapter(const BinaryHostAdapter&) = delete;
  BinaryHostAdapter& operator=(const BinaryHostAdapter&) = delete;
  ~BinaryHostAdapter();

  // Description 是内部借用引用；TakeDescription 一次性移交独立发布副本，内部描述仍保留。
  const DocumentDescription& Description() const noexcept;
  DocumentDescription TakeDescription();
  // Bindings/Identity 同样借用本实例持有的数据，实例销毁后不可继续使用。
  const std::vector<BinaryHostBinding>& Bindings() const noexcept { return bindings_; }
  const BinaryPreparationIdentity& Identity() const noexcept { return identity_; }
  std::size_t UiDescriptionBytes() const noexcept { return ui_description_bytes_; }
  // H1、描述、外部保留量与 UI/Hex/UTF-16 预留的逻辑计费，不是进程 RSS 上限。
  std::size_t AccountedInstanceBytes() const noexcept;
  std::size_t AccountedPreparationBytes() const noexcept;
  std::size_t DescriptionCopyUpperBoundBytes() const noexcept {
    return description_copy_upper_bound_bytes_;
  }
  std::size_t CurrentResultCopyUpperBoundBytes(std::size_t binding, std::size_t flow) const;
  // 登记上层仍持有的展示占用，不分配、接管或释放上层对象。
  bool SetPresentationRetainedBytes(std::size_t bytes) noexcept;
  std::size_t PresentationRetainedBytes() const noexcept { return presentation_retained_bytes_; }
  std::size_t UiViewReserveBytes() const noexcept { return ui_view_reserve_bytes_; }
  std::size_t HexPreviewReserveBytes() const noexcept { return hex_preview_reserve_bytes_; }
  static std::size_t AccountDescriptionBytes(const DocumentDescription& description);
  std::uint64_t Instance() const noexcept { return instance_; }
  bool IsCompleteDecode(std::size_t binding, std::size_t flow) const noexcept;
  bool IsStreamDecode(std::size_t binding, std::size_t flow) const noexcept;
  bool IsCompleteEncode(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t FlowCount(std::size_t binding) const noexcept;
  BinaryUiDecodeView DecodeComplete(std::size_t binding, std::size_t flow,
                                    const std::vector<std::uint8_t>& frame,
                                    std::size_t active_view_bytes = 0U);
  // 复制 H1 当前结果，不再次 Decode；完整记录的展示复制异常由调用方处理。
  BinaryUiDecodeView MapCurrent(std::size_t binding, std::size_t flow,
                                std::size_t active_view_bytes = 0U) const;
  BinaryUiStreamView SubmitStream(std::size_t binding, std::size_t flow,
                                  const std::vector<std::uint8_t>& chunk,
                                  std::size_t active_view_bytes = 0U);
  BinaryUiStreamView ContinueStream(std::size_t binding, std::size_t flow,
                                    std::size_t active_view_bytes = 0U);
  // 不推进 Host；展示复制失败仍会标记此 Flow 的映射故障，要求 Reset。
  BinaryUiStreamView MapCurrentStream(std::size_t binding, std::size_t flow,
                                      std::size_t active_view_bytes = 0U);
  std::optional<StreamPresentationObservation> ObserveStream(std::size_t binding,
                                                             std::size_t flow) const noexcept;
  bool StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept;
  // 仅在 H1 Reset 成功后解除 H2 映射故障。
  pae::HostStatus ResetStream(std::size_t binding, std::size_t flow) noexcept;
  BinaryUiEncodeView EncodeComplete(
      std::size_t binding, std::size_t flow, std::size_t message_index,
      const std::vector<protocol_lab_binary::public_decode::EncodeInput>& inputs,
      std::size_t active_view_bytes = 0U);
  // 只投影既有 TX 结果，不再次 Encode，也不为展示补做 RX Decode。
  BinaryUiEncodeView MapCurrentEncode(std::size_t binding, std::size_t flow,
                                      std::size_t active_view_bytes = 0U) const;
  // Current/Draft/TypedDrafts 借用内部状态；后续操作可能改变内容，不能当独立快照保存。
  const void* Current(std::size_t binding, std::size_t flow) const noexcept;
  std::u16string_view Draft(std::size_t binding, std::size_t flow) const noexcept;
  // 返回文本 code unit 上限；完整记录 Hex 与 stream 输入文本使用不同长度规则。
  std::size_t DraftLimit(std::size_t binding, std::size_t flow) const noexcept;
  // 保存当前选中源 Flow 的草稿，再切换到参数指定的目标 Flow。
  void SaveAndSelect(std::u16string_view draft, std::size_t binding, std::size_t flow);
  // Inspect、类型化草稿和 source_message_index 属于当前源 Flow，不属于目标参数。
  bool SaveDraftsAndSelect(std::u16string_view inspect_draft,
                           const std::unordered_map<std::size_t, TypedDraft>& typed_drafts,
                           std::size_t source_message_index, std::size_t binding, std::size_t flow);
  // 按指定 Flow 保存；字段索引解释依赖该 Flow 当前的 Message 选择。
  bool SaveTypedDrafts(std::size_t binding, std::size_t flow,
                       const std::unordered_map<std::size_t, TypedDraft>& drafts);
  const std::unordered_map<std::size_t, TypedDraft>& TypedDrafts(std::size_t binding,
                                                                 std::size_t flow) const noexcept;
  std::optional<std::size_t> MessageSelection(std::size_t binding, std::size_t flow) const noexcept;
  // Message 改变时清空该 Flow 的旧类型化草稿和 TX 结果，避免沿用旧字段索引。
  bool SelectEncodeMessage(std::size_t binding, std::size_t flow,
                           std::size_t message_index) noexcept;
  void ClearCurrentEncode(std::size_t binding, std::size_t flow) noexcept;

 private:
  BinaryHostAdapter();
  std::size_t ResultCopyUpperBound(
      const protocol_lab_binary::public_decode::Operation& operation) const;
  std::size_t ResultCopyUpperBound(
      const pae::HostOperationResult& host,
      const std::optional<protocol_lab_binary::public_decode::Candidate>& candidate) const;
  BinaryUiDecodeView MapCandidate(
      const pae::HostOperationResult& host,
      const std::optional<protocol_lab_binary::public_decode::Candidate>& candidate,
      std::size_t active_view_bytes, std::size_t input_peak_bytes) const;
  BinaryUiDecodeView MapOperation(const protocol_lab_binary::public_decode::Operation& operation,
                                  std::size_t active_view_bytes) const;
  BinaryUiStreamView MapStreamStep(const protocol_lab_binary::public_decode::StreamStep& step,
                                   std::size_t active_view_bytes,
                                   std::size_t input_peak_bytes) const;
  BinaryUiStreamView ProjectStreamMappingFailure(
      const protocol_lab_binary::public_decode::StreamStep& step) const noexcept;
  BinaryUiStreamView StreamMappingFailure(
      const protocol_lab_binary::public_decode::StreamStep& step, std::size_t binding,
      std::size_t flow) noexcept;
  BinaryUiEncodeView MapEncodeOperation(
      const protocol_lab_binary::public_decode::Operation& operation,
      std::size_t active_view_bytes) const;
  // 内部映射描述与可移交发布描述分别拥有；移交后仍能用内部描述映射执行结果。
  std::unique_ptr<DocumentDescription> description_;
  std::unique_ptr<DocumentDescription> publication_description_;
  std::unique_ptr<protocol_lab_binary::public_decode::Adapter> owner_;
  std::vector<BinaryHostBinding> bindings_;
  // 经 H1 FlowIndex 扁平索引隔离的草稿、Message 选择与映射故障。
  std::vector<std::u16string> drafts_;
  std::vector<std::unordered_map<std::size_t, TypedDraft>> typed_drafts_;
  std::vector<std::optional<std::size_t>> message_selections_;
  std::vector<std::uint8_t> stream_mapping_faulted_;
  std::size_t selected_binding_ = 0U;
  std::size_t selected_flow_ = 0U;
  BinaryPreparationIdentity identity_;
  protocol_lab_binary::public_decode::Limits limits_;
  BinaryUiCopyControls copy_controls_;
  std::uint64_t instance_ = 0U;
  std::size_t ui_description_bytes_ = 0U;
  std::size_t description_copy_upper_bound_bytes_ = 0U;
  std::size_t externally_retained_bytes_ = 0U;
  std::size_t preparation_coexisting_bytes_ = 0U;
  std::size_t presentation_retained_bytes_ = 0U;
  std::size_t ui_view_reserve_bytes_ = 32U * 1024U * 1024U;
  std::size_t hex_preview_reserve_bytes_ = 4U * 1024U * 1024U;
  std::size_t utf16_draft_reserve_bytes_ = 0U;
};

}  // namespace pae::protocol_lab_ui
