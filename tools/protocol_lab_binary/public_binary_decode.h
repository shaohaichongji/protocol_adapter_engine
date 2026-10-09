#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "pae/compiler.h"
#include "pae/host_endpoint.h"

namespace pae::protocol_lab_binary::public_decode {

// Binary H1 非 Qt 公开适配层：以 Host 执行完整记录 Decode/Encode 和受支持的 Binary stream。
// 名称保留历史 public_decode，不表示只支持 Decode；UI 表示/映射由上游 H2 负责。
// bytes 类限额以字节计，fields 以字段数计；work_units 是逻辑工作量，0 请求 Framer 默认值。
// 实例/结果按本实现的逻辑容量计费，不承诺进程 RSS 或所有瞬时分配的硬上限。
struct Limits {
  std::size_t max_frame_bytes = 65536U;
  std::size_t max_stream_chunk_bytes = 65536U;
  std::size_t max_stream_work_units = 0U;
  std::size_t max_fields = 1024U;
  std::size_t max_field_bytes = 1024U * 1024U;
  std::size_t max_result_bytes = 8U * 1024U * 1024U;
  std::size_t max_description_bytes = 4U * 1024U * 1024U;
  std::size_t instance_bytes = 128U * 1024U * 1024U;
  std::size_t replacement_bytes = 256U * 1024U * 1024U;
};

enum class LocalStatus { OK, INVALID_INPUT, INVALID_BINDING, RESOURCE_LIMIT, PREPARATION_FAILED,
                         MATERIALIZATION_FAILED };

enum class StreamDiagnostic {
  NONE,
  NOT_STREAM,
  INVALID_CHUNK,
  CONTINUE_REQUIRED,
  NO_WORK,
  RESET_REQUIRED,
  COPY_FAILED_RESET_REQUIRED,
  CONTRACT_VIOLATION_RESET_REQUIRED,
};

// kind 决定有效值成员；索引均来自同一冻结配置，不是可跨配置复用的业务标识。
// Decimal 是逻辑值，conversion_raw_* 是 Decode 实际留存的转换前整数，不从逻辑值反算。
// byte_range 为当前完整记录的字节范围；bit_masks[0, bit_mask_count) 才是有效物理位映射。
struct Field {
  std::size_t flat_index = 0U;
  std::string id;
  ValueKind kind = ValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  Decimal64 decimal;
  std::vector<std::uint8_t> bytes;
  std::optional<std::uint64_t> enum_raw;
  std::optional<std::size_t> known_enum_flat_index;
  std::optional<RawIntegerKind> conversion_raw_kind;
  std::optional<std::uint64_t> conversion_raw_uint64;
  std::optional<std::int64_t> conversion_raw_int64;
  std::optional<ByteRange> byte_range;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
};

// 成功时发布自有 frame/字段/身份；失败只保留候选帧与诊断，不能当作成功字段记录使用。
// vector/string 数据已脱离同步回调 view；但从 Adapter 返回的引用仍借用它的内部状态。
struct Candidate {
  bool success = false;
  std::optional<std::size_t> message_index;
  std::string message_id;
  CodecStatus codec_status = CodecStatus::INVALID_ARGUMENT;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  std::vector<std::uint8_t> frame;
  std::vector<Field> fields;
  std::optional<ByteRange> integrity_storage;
  std::optional<ByteRange> computed_length_storage;
  std::size_t accounted_bytes = 0U;
};

// 类型化逻辑输入；BYTES 自有，Enum 用字段内条目序号而非 enum raw 或全局条目索引。
// 只提交 CALLER_INPUT 字段，常量/计算字段由冻结计划写入；不在此解析 UI 文本表示。
struct EncodeInput {
  std::size_t field_index = 0U;
  ValueKind kind = ValueKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
  bool bool_value = false;
  std::vector<std::uint8_t> bytes;
  std::size_t enum_entry_index = 0U;
  Decimal64 decimal;

  static EncodeInput UInt64(std::size_t field, std::uint64_t value);
  static EncodeInput Int64(std::size_t field, std::int64_t value);
  static EncodeInput Bool(std::size_t field, bool value);
  static EncodeInput Bytes(std::size_t field, std::vector<std::uint8_t> value);
  static EncodeInput Enum(std::size_t field, std::size_t entry);
  static EncodeInput Decimal(std::size_t field, Decimal64 value);
};

struct EncodedField {
  std::size_t field_index = 0U;
  std::string id;
  std::optional<ByteRange> byte_range;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
};

// 自有 TX 字节、本次输入副本及物理描述投影；不为展示额外 Decode TX 来生成 RX 值。
struct Encoded {
  std::size_t message_index = 0U;
  std::string message_id;
  std::vector<EncodeInput> inputs;
  std::vector<std::uint8_t> frame;
  std::vector<EncodedField> fields;
  std::optional<ByteRange> integrity_storage;
  std::optional<ByteRange> computed_length_storage;
  std::size_t accounted_bytes = 0U;
};

// local_status、host、可选 DTO 分层判断；默认 local OK 不证明 Host 已执行或业务成功。
// CALLBACK_FAILED 可保留 Codec 已执行/成功事实，但不得发布半份 candidate/encoded。
struct Operation {
  LocalStatus local_status = LocalStatus::OK;
  HostOperationResult host;
  std::optional<Candidate> candidate;
  std::optional<Encoded> encoded;
};

// Framer 缓冲/内部待办与 Adapter 冻结 chunk/cursor 是两层状态；长度和 cursor 都以字节计。
// generation 是 Reset 代次，step_sequence 是本地推进次数；Observe 不是并发快照。
struct StreamObservation {
  PipelineFramingStrategy strategy = PipelineFramingStrategy::COMPLETE_RECORD;
  StreamFramingPhase phase = StreamFramingPhase::COLLECTING;
  std::size_t maximum_candidate_frame_bytes = 0U;
  std::size_t effective_max_submit_bytes = 0U;
  std::size_t effective_max_work_units = 0U;
  std::size_t buffered_bytes = 0U;
  std::size_t frozen_input_bytes = 0U;
  std::size_t frozen_cursor = 0U;
  bool has_internal_work = false;
  bool reset_required = false;
  std::uint64_t generation = 0U;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_decode_failures = 0U;
  std::uint64_t total_observer_callbacks = 0U;
  std::uint64_t total_business_callbacks = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
};

// host_called 区分前置拒绝；diagnostic 可在 local OK 时提示 Continue/Reset，不能只看 local。
// 失败候选可能正常消费并返回 CODEC_FAILED；Host 实际 bytes_consumed 不因 DTO 复制失败撤销。
struct StreamStep {
  LocalStatus local_status = LocalStatus::OK;
  StreamDiagnostic diagnostic = StreamDiagnostic::NONE;
  bool host_called = false;
  HostOperationResult host;
  StreamObservation before;
  StreamObservation after;
  std::optional<Candidate> candidate;
};

// 每个 Flow 自有冻结输入和当前一步结果；cursor 只按核验后的实际消费推进。
struct StreamState {
  bool available = false;
  PipelineFramingStrategy strategy = PipelineFramingStrategy::COMPLETE_RECORD;
  std::size_t maximum_candidate_frame_bytes = 0U;
  std::size_t capacity = 0U;
  std::vector<std::uint8_t> frozen_input;
  std::size_t cursor = 0U;
  bool faulted = false;
  std::uint64_t step_sequence = 0U;
  std::uint64_t total_candidates = 0U;
  std::uint64_t total_decode_successes = 0U;
  std::uint64_t total_decode_failures = 0U;
  std::uint64_t total_observer_callbacks = 0U;
  std::uint64_t total_business_callbacks = 0U;
  std::size_t total_discarded_bytes = 0U;
  std::size_t total_malformed_candidates = 0U;
  StreamStep current;
};

// draft 仅存上游文本，不自动解码；current 与 stream.current 是可被后续操作替换的状态槽位。
struct FlowState {
  std::string draft;
  Operation current;
  HostChannelHandle handle;
  StreamState stream;
};

// 将公开 metadata 的借用 id 复制为自有字符串；完整物理布局仍在同步调用中向 compiled 查询。
struct OwnedMessageName {
  std::size_t index = 0U;
  std::string id;
  std::vector<std::string> field_ids;
};

struct Preparation {
  LocalStatus status = LocalStatus::PREPARATION_FAILED;
  HostStatus host_status = HostStatus::INVALID_ARGUMENT;
  std::unique_ptr<class Adapter> adapter;
  std::optional<CompileDiagnostic> diagnostic;
};

struct Binding {
  std::string endpoint;
  HostAction action = HostAction::DECODE;
  std::string pipeline_id;
  std::size_t flow_count = 2U;
};

// 只通过公开 Compiler/Host/物理描述执行与观察，不管理 Socket、设备线程或业务路由。
// 同实例调用、状态读取、Reset 和销毁需串行；Flow 状态隔离不代表可并发操作。
class Adapter final {
 public:
  // 便利入口只编译一次 JSON，并创建一个 DECODE 绑定、两个 Flow；多绑定/Encode 用 AdoptCompiled。
  static Preparation Create(std::string_view json, std::string_view endpoint,
                            std::string_view pipeline_id, const Limits& limits = {},
                            std::size_t previous_instance_bytes = 0U);
  static Preparation AdoptCompiled(CompiledProtocol compiled, std::vector<Binding> bindings,
                                   const Limits& limits = {},
                                   std::size_t previous_instance_bytes = 0U);
  Adapter(const Adapter&) = delete;
  Adapter& operator=(const Adapter&) = delete;
  ~Adapter();

  // flow 是展平索引：先 FlowIndex(binding, flow)；仅用于完整记录 DECODE，不偷偷回退切帧。
  // 返回内部引用；需要独立长期保存时复制 DTO/Operation，不能把引用寿命等同于自有字节寿命。
  const Operation& Decode(std::size_t flow, ByteView frame);
  const Operation& Encode(std::size_t binding, std::size_t message_index,
                          const std::vector<EncodeInput>& inputs);
  // stream 的 flow 是绑定内序号。提交时复制冻结 chunk；有后缀或内部待办须先 Continue。
  // 返回 StreamState/current 或拒绝槽位的引用，后续操作可覆盖；需要保留快照时复制值对象。
  const StreamStep& SubmitStreamChunk(std::size_t binding, std::size_t flow,
                                      const std::vector<std::uint8_t>& chunk) noexcept;
  // 每步至多一个候选：推进未消费后缀或 Framer 内部工作，不重喂已消费前缀。
  const StreamStep& ContinueStream(std::size_t binding, std::size_t flow) noexcept;
  std::optional<StreamObservation> ObserveStream(std::size_t binding,
                                                 std::size_t flow) const noexcept;
  bool StreamContinueAvailable(std::size_t binding, std::size_t flow) const noexcept;
  // flow 是展平索引；成功 Reset 后重新 Find handle。stream 清冻结输入/计数，其他 Flow 不动。
  HostStatus Reset(std::size_t flow) noexcept;
  // 只清当前结果，不替代 Host/Framer Reset，也不清冻结输入或 draft。
  void ClearCurrent(std::size_t flow) noexcept;
  // 同步复制有界文本，失败不替换旧 draft；不把文本转换为执行字节。
  bool SetDraft(std::size_t flow, std::string_view text);
  const FlowState* State(std::size_t flow) const noexcept;
  std::size_t FlowCount(std::size_t binding) const noexcept;
  std::size_t FlowIndex(std::size_t binding, std::size_t flow) const noexcept;
  std::size_t InstanceAdmissionBytes() const noexcept { return instance_bytes_; }

 private:
  Adapter() = default;
  const StreamStep& RunStreamStep(std::size_t binding, std::size_t flow) noexcept;
  // 保留 metadata 查询 owner；Host 自行保留冻结执行状态，不借用 compiled_ 对象的地址/寿命。
  // 成员倒序析构先释放 Flow/Host 再释放 compiled_；内部 State/结果引用依赖 Adapter 寿命。
  CompiledProtocol compiled_;
  std::vector<OwnedMessageName> messages_;
  std::unique_ptr<HostEndpoint> host_;
  std::vector<Binding> bindings_;
  std::vector<std::size_t> flow_begin_;
  std::vector<FlowState> flows_;
  Operation rejected_;
  StreamStep rejected_stream_;
  Limits limits_;
  std::size_t instance_bytes_ = 0U;
};

}  // namespace pae::protocol_lab_binary::public_decode
