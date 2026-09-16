# PAE 公开 COMPLETE_RECORD Codec 首片设计

状态：2026-09-14 总控确认作为阶段 1 实施输入；同日已完成限定范围实现与
Windows x64 Debug/Release 验证，待总控复核。详细证据见
[公开 COMPLETE_RECORD Codec 首片 Windows 验证](pae-public-codec-slice-validation.md)。这仍不是
稳定 SDK/ABI、动态库、Linux、真实协议或生产验收声明。

## 总控实施补充（优先于下文候选细节）

- 接受内部共享不可变编译状态、每 Codec 独占 workspace、短期 Decode view、调用者 Encode buffer，以及首片仅已知枚举 selector Encode。
- facade 必须先非阻塞取得实例级 busy guard，再接触 slots、输入映射缓存、raw 发布状态或结果代次；竞争失败只返回本次空结果与 WORKSPACE_BUSY，不清空或修改正在执行调用的状态。下文“调用开始失效”指成功取得 guard 的调用。view 读取、move/销毁不得与修改该实例的调用并发，消费者负责同步。
- EncodeValue 到 Core 输入项的映射存储必须在创建时预分配，按 Plan 上界明确容量，checked arithmetic 纳入预检与 MemoryReport；输入项数量超出固定容量时须在访问/写入前有界失败，明确返回状态和优先级并覆盖测试。不得每帧临时 vector 分配；不能仅按 size 忽略实际 capacity。
- 共享状态、facade 和输入映射的计费口径必须清楚，避免重复计费或隐藏分配；允许采用最小可验证实现，不为共享所有权引入通用框架。
- 首片不包含 Lab 全量物理范围和 Encode 同调用观察字段。它们是 Lab 迁移前的后续接口缺口，本轮不以重复 Decode 或 raw 反算绕过。
- 具体派发、写入范围与同步门槛见综合推进计划的“阶段 1 当前派发”。

本文只定义首个公开完整记录 Decode/Encode 切片。它把已有冻结 Plan 和
`protocol_core::DecodeCompleteRecord` / `EncodeCompleteRecord` 包装成不泄露私有 Plan
类型的 C++17 API，不增加 Schema 能力，不同时公开 Framer、Host、C ABI、DLL 包或
Lab 展示状态。

## 1. 现状与设计约束

- `pae::CompiledProtocol` 是 move-only owner，其 `Impl` 当前整体拥有
  `CompiledUiArtifacts`，后者内含冻结 `PlanOwner` 和 metadata sidecar。
- Core `ExecutionWorkspace` 永久绑定一个 `PlanBundle`，工作区不拥有 Plan；同一
  workspace 重入或并发使用会失败，多个 workspace 可共享同一不可变 Plan。
- Core Decode 的 `DecodedFieldSlot` 由调用者提供；BYTES 直接借用输入 Frame；
  Decimal 逻辑值为 `{int64_t coefficient, int32_t scale}`，不经过 `double`；转换字段
  的原始整数由 workspace 在成功 Decode 后发布。
- Core Encode 要求字段和枚举引用属于同一 Plan，输出 Buffer 由调用者拥有；
  失败时 `bytes_written == 0`，Buffer 可能已被改写，不可作为有效输出。
- 当前公开 metadata 已提供 pipeline/message/field/enum 索引及关联，因此首片
  无需把私有 `PlanBundle*`、`FieldRef` 或 `EnumValueRef` 暴露给外部使用方。

## 2. owner / workspace 生命期选择

| 方案 | 优点 | 代价与风险 | 结论 |
| --- | --- | --- | --- |
| 执行器借用 `CompiledProtocol` | 内部改动最少，无引用计数 | owner 销毁、move-assignment 或与执行并发移动会使 workspace 悬空；只靠文档无法失败关闭 | 不推荐作为公开默认 |
| 内部共享编译状态 | owner 和多个执行器共享同一不可变 Plan；执行器可独立存活，外部仍看不到 Plan | 需内部引用计数和一次编译状态分配；必须把该成本纳入报告 | **推荐** |
| 将 owner 转移给执行器 | 所有权单一，无共享计数 | 创建后无法继续通过编译 owner 查 metadata，多 workspace 需要再次转移或另造 owner | 不适合当前多流消费者 |

推荐保持 `CompiledProtocol` 的公开 move-only 语义，但将其内部 artifacts 放入不可变
`CompiledState`。`CompiledProtocol::Impl` 和每个 `CompleteRecordCodec::Impl` 各持有一份
内部 strong reference。引用计数只在编译状态、owner 和执行器创建/销毁时变化，
不进入每帧 Decode/Encode 路径。

实现时应对共享状态封装和引用管理做可测量计费：可使用局部 intrusive reference
或带计数 allocator 的 shared ownership，不应把标准库 control block 当成不可见的零成本。
此选择是实现约束，不把共享指针类型暴露在公开 API 中。

每个 `CompleteRecordCodec` 内部只拥有一个 `ExecutionWorkspace`。同一执行器必须
串行使用，重入/并发返回 `WORKSPACE_BUSY`；需要并发时，从同一
`CompiledProtocol` 创建多个执行器。不在执行器内加等待锁，不隐式排队。

已经取得的 metadata `string_view` 仍按现有保守契约处理：它们不因执行器恰好
保活内部状态而获得新的公开寿命保证；owner 移动后重新查询 metadata。

## 3. 推荐的公开 API 形状

以下是应落地到 `include/pae/codec.h` 的接口形状；名称可在实现复核时做局部
一致性调整，不改变本节语义。

```cpp
namespace pae {

enum class CodecStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_COMPILED_PROTOCOL,
  RESOURCE_LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  WORKSPACE_BUSY,
  INPUT_VALUES_TOO_MANY,
  UNKNOWN_MESSAGE,
  AMBIGUOUS_MESSAGE,
  OUTPUT_SLOTS_TOO_SMALL,
  INTEGRITY_FAILED,
  MESSAGE_NOT_ALLOWED,
  FIELD_REFERENCE_MISMATCH,
  DUPLICATE_FIELD,
  MISSING_FIELD,
  TYPE_MISMATCH,
  VALUE_NOT_REPRESENTABLE,
  BYTES_LENGTH_MISMATCH,
  UNKNOWN_ENUM_VALUE,
  ENUM_REFERENCE_MISMATCH,
  CONSTANT_FIELD_OVERRIDE,
  INPUT_OUTPUT_OVERLAP,
  BUFFER_TOO_SMALL,
  FINAL_REVIEW_FAILED,
  ASCII_CHARACTER_NOT_ALLOWED,
  ASCII_TERMINATOR_CONFLICT,
  OPERATION_NOT_SUPPORTED,
  COMPUTED_FIELD_OVERRIDE,
  LENGTH_MISMATCH,
  INTERNAL_ERROR,
};

enum class ValueKind { UINT64, INT64, BOOL, BYTES, ENUM, DECIMAL64 };
enum class ConversionError {
  NONE,
  DECIMAL_SCALE_OUT_OF_RANGE,
  RAW_NOT_INTEGRAL,
  RAW_OUT_OF_RANGE,
  LOGICAL_OUT_OF_RANGE,
};
enum class RawIntegerKind { UINT64, INT64 };

struct ByteView { const std::uint8_t* data; std::size_t size; };
struct MutableByteBuffer { std::uint8_t* data; std::size_t capacity; };
struct Decimal64 { std::int64_t coefficient; std::int32_t scale; };
struct FieldSelector { std::size_t message_index; std::size_t field_index; };
struct EnumSelector {
  std::size_t message_index;
  std::size_t field_index;
  std::size_t entry_index;
};

class EncodeValue final {
 public:
  static EncodeValue UInt64(FieldSelector, std::uint64_t) noexcept;
  static EncodeValue Int64(FieldSelector, std::int64_t) noexcept;
  static EncodeValue Bool(FieldSelector, bool) noexcept;
  static EncodeValue Bytes(FieldSelector, ByteView) noexcept;
  static EncodeValue Enum(EnumSelector) noexcept;
  static EncodeValue Decimal(FieldSelector, Decimal64) noexcept;
  // private tagged storage; no implicit numeric conversion and no public union mutation.
};

struct CompleteRecordCodecOptions {
  // 0 means the Plan maximum. A smaller explicit value preserves the existing
  // OUTPUT_SLOTS_TOO_SMALL path without allocating during Decode.
  std::size_t decoded_field_capacity = 0U;
  std::size_t execution_memory_limit_bytes = static_cast<std::size_t>(-1);
};

struct ExecutionMemoryReport {
  std::size_t core_workspace_bytes = 0U;
  std::size_t decoded_slot_bytes = 0U;
  std::size_t encode_mapping_bytes = 0U;
  std::size_t facade_bytes = 0U;
  std::size_t codec_accounted_total_bytes = 0U;
  std::size_t retained_compiled_state_facade_bytes = 0U;
  std::size_t allocation_count = 0U;
};

class DecodedFieldView final {
 public:
  std::size_t FlatFieldIndex() const noexcept;
  FieldSelector Field() const noexcept;
  ValueKind Kind() const noexcept;
  std::optional<std::uint64_t> UInt64() const noexcept;
  std::optional<std::int64_t> Int64() const noexcept;
  std::optional<bool> Bool() const noexcept;
  std::optional<ByteView> Bytes() const noexcept;
  std::optional<std::uint64_t> EnumRawValue() const noexcept;
  std::optional<std::size_t> KnownEnumFlatIndex() const noexcept;
  std::optional<Decimal64> Decimal() const noexcept;
  // Present only for a converted integer field. This is read from the Core
  // workspace's recorded raw value; it is never recomputed from Decimal64.
  std::optional<RawIntegerKind> ConversionRawKind() const noexcept;
  std::optional<std::uint64_t> ConversionRawUInt64() const noexcept;
  std::optional<std::int64_t> ConversionRawInt64() const noexcept;
};

class DecodedRecordView final {
 public:
  std::size_t MessageIndex() const noexcept;
  std::size_t FieldCount() const noexcept;
  std::optional<DecodedFieldView> Field(std::size_t ordinal) const noexcept;
};

struct DecodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  DecodedRecordView record;  // empty unless status == OK
  std::size_t required_field_count = 0U;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
  bool output_tainted = false;
};

struct EncodeResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::size_t bytes_written = 0U;  // zero unless status == OK
  std::size_t required_size = 0U;
  std::optional<std::size_t> failed_value_index;
  std::optional<std::size_t> failed_field_flat_index;
  ConversionError conversion_error = ConversionError::NONE;
};

class CompleteRecordCodec final {
 public:
  CompleteRecordCodec(const CompleteRecordCodec&) = delete;
  CompleteRecordCodec& operator=(const CompleteRecordCodec&) = delete;
  CompleteRecordCodec(CompleteRecordCodec&&) noexcept;
  CompleteRecordCodec& operator=(CompleteRecordCodec&&) noexcept;
  ~CompleteRecordCodec();

  DecodeResult Decode(std::size_t pipeline_index, ByteView frame) noexcept;
  EncodeResult Encode(std::size_t pipeline_index, std::size_t message_index,
                      const EncodeValue* values, std::size_t value_count,
                      MutableByteBuffer output) noexcept;
  ExecutionMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

struct CompleteRecordCodecCreateResult {
  CodecStatus status = CodecStatus::INVALID_ARGUMENT;
  std::unique_ptr<CompleteRecordCodec> codec;
  ExecutionMemoryReport memory;
};

CompleteRecordCodecCreateResult CreateCompleteRecordCodec(
    const CompiledProtocol&, const CompleteRecordCodecOptions& = {}) noexcept;

}  // namespace pae
```

`FieldSelector` / `EnumSelector` 是当前编译实例中的位置选择器，不是可持久化
的协议标识。公开层先校验索引、message 归属和 enum 归属，再用执行器自己的
Plan 构造私有 `FieldRef` / `EnumValueRef`；外部无法传入 Plan 指针，也不会把一个
Core workspace 与另一 Plan 组合。如果索引需跨编译实例存储，使用方应存储稳定
ID，重新编译后通过 metadata 再解析索引。

`CodecStatus` 与内部 `protocol_core::CodecStatus` 显式 `switch` 映射，不依赖两个 enum
的整数值相同。创建阶段的 owner/资源/分配失败与执行阶段的协议失败使用
同一有界 status 域，但不生成伪造的 message/field 结果。

## 4. 输入、输出与借用期

### 4.1 Decode

1. `Decode` 在调用开始即使上一次 `DecodedRecordView` 失效，同时清空上次 raw
   conversion 发布状态。
2. Core 直接写入执行器创建时一次分配的 slot 数组。公开层不再复制一遍
   字段，也不重复 Decode。
3. 仅 `status == OK` 时发布非空 `DecodedRecordView`。失败时 view 为空，
   无论 slot 内存中是否还有旧字节，都不是本次有效结果。
4. Record/field view 有效期到“同一执行器下一次成功取得 guard 的
   Decode/Encode、owner 发生 move-construction/move-assignment、owner 销毁”中最早发生者。
   move-construction 使来源 owner 的旧 view 失效；move-assignment 同时使来源与目标 owner
   的旧 view 失效。moved-from owner 在再次被赋值前返回
   `INVALID_COMPILED_PROTOCOL`，后续复用不得使任何旧 view 重新有效；移交后的 owner
   只在新的成功 Decode 后发布新 view。
5. BYTES 额外借用输入 Frame，因此还受 Frame 存储被修改、复用或销毁的
   更早期限约束。需长期保留时由消费者显式复制。
6. Decimal 的 logical 值直接返回 `Decimal64`；对转换字段，raw integer 直接来自
   `ExecutionWorkspace::GetLastRawInteger`。不从 logical 值反算 raw，不格式化成
   `double`。
7. ENUM 同时保留 raw unsigned value 和可选的 known enum 索引。unknown-allowed
   解码结果的 known 索引为空，raw 仍可读。

`decoded_field_capacity` 默认等于 Plan 的 `max_fields_per_message`。如果消费者为了
小内存上限显式设得更小，唯一候选的字段数超出容量时保留 Core 已有的
`OUTPUT_SLOTS_TOO_SMALL` 优先级，`required_field_count` 给出需求，不发布部分字段。

### 4.2 Encode

- `EncodeValue` 只能由有类型工厂构造；UINT64、INT64、BOOL、BYTES、ENUM 和
  Decimal64 不做隐式互转。
- ENUM 首片只接受已知 `EnumSelector`，与现有 Core Encode 能力一致；不借
  unknown-enum Decode 策略扩大为“任意 raw enum Encode”。
- BYTES 在调用期间借用消费者存储；公开层不做隐式大复制。Core 继续按
  Binary 长度或 ASCII 字符/边界契约校验。
- 输出 Buffer 由消费者提供；成功时只有 `[data, data + bytes_written)` 有效。
  `BUFFER_TOO_SMALL` 返回 `required_size`，其他失败不得把已改写 Buffer 作为输出；
  公开层不为其创建隐式 frame 副本。
- 输入顺序不影响配置顺序诊断；重复、缺失、常量覆盖、类型和表示范围
  错误继续由 Core 决定，公开层只将失败索引转换为公开 selector/flat index。

## 5. 最小调用示例

示例中的 `ReadAll` 和按 ID 查 metadata 是应用辅助代码，不是 PAE 热路径。

### 5.1 Binary COMPLETE_RECORD

```cpp
auto compiled_result = pae::CompileProtocolJson(ReadAll("device.pae.json"));
if (!compiled_result.Succeeded()) {
  ReportCompile(*compiled_result.Diagnostic());
  return;
}
pae::CompiledProtocol compiled = std::move(compiled_result).TakeCompiled();

// Resolve once from public metadata; no string lookup is performed per frame.
const std::size_t rx_pipeline = FindPipeline(compiled, "rx_complete");
const std::size_t tx_pipeline = FindPipeline(compiled, "tx_complete");
const std::size_t command = FindMessage(compiled, "set_command");
const pae::FieldSelector sequence = FindField(compiled, command, "sequence");
const pae::EnumSelector running = FindEnum(compiled, command, "mode_running");

auto create = pae::CreateCompleteRecordCodec(compiled);
if (create.status != pae::CodecStatus::OK) return;
std::unique_ptr<pae::CompleteRecordCodec> codec = std::move(create.codec);

std::array<std::uint8_t, 64> tx{};
const std::array values{
    pae::EncodeValue::UInt64(sequence, 7U),
    pae::EncodeValue::Enum(running),
};
const auto encoded = codec->Encode(tx_pipeline, command, values.data(), values.size(),
                                   {tx.data(), tx.size()});
if (encoded.status != pae::CodecStatus::OK) return;  // tx is not deliverable on failure
SendOnlyTheBytes(tx.data(), encoded.bytes_written);

std::array<std::uint8_t, 8> rx{0xA5, 0x02, 0x00, 0x07, 0x01, 0x00, 0x00, 0xAF};
const auto decoded = codec->Decode(rx_pipeline, {rx.data(), rx.size()});
if (decoded.status != pae::CodecStatus::OK) return;
for (std::size_t i = 0; i != decoded.record.FieldCount(); ++i) {
  Consume(*decoded.record.Field(i));  // view expires at the next call on codec
}
```

### 5.2 ASCII COMPLETE_RECORD

```cpp
auto compiled_result = pae::CompileProtocolJson(ReadAll("ascii-device.pae.json"));
if (!compiled_result.Succeeded()) return;
pae::CompiledProtocol compiled = std::move(compiled_result).TakeCompiled();

const std::size_t encode_pipeline = FindPipeline(compiled, "ascii_tx");
const std::size_t decode_pipeline = FindPipeline(compiled, "ascii_rx");
const std::size_t request = FindMessage(compiled, "request");
const pae::FieldSelector name = FindField(compiled, request, "name");

auto create = pae::CreateCompleteRecordCodec(compiled);
if (create.status != pae::CodecStatus::OK) return;
auto codec = std::move(create.codec);

const std::string_view alice = "ALICE";
const auto value = pae::EncodeValue::Bytes(
    name, {reinterpret_cast<const std::uint8_t*>(alice.data()), alice.size()});
std::array<std::uint8_t, 32> output{};
const auto encoded = codec->Encode(encode_pipeline, request, &value, 1U,
                                   {output.data(), output.size()});
if (encoded.status != pae::CodecStatus::OK) return;

const std::string_view reply = "RX ALICE!OK\r\n";
const auto decoded = codec->Decode(
    decode_pipeline,
    {reinterpret_cast<const std::uint8_t*>(reply.data()), reply.size()});
if (decoded.status == pae::CodecStatus::OK) {
  // ASCII text remains BYTES. The view borrows reply and is not NUL-terminated.
  Consume(*decoded.record.Field(0U));
}
```

本切片只处理已经形成的完整记录。ASCII stream 的分块、terminator 定界和 reset
属于后续 Framer/Host 切片，不进入此 `Decode` 调用。

## 6. 资源、操作上界与越界失败

- 执行器创建前用 checked add/multiply 计算 Core workspace、Decode slot 数组、facade
  和分配对齐成本。溢出或超过 `execution_memory_limit_bytes` 返回
  `RESOURCE_LIMIT_EXCEEDED`，不先分配再拒绝。
- `ExecutionMemoryReport` 的 Core 部分与 Plan `ExecutionResourceLayout` 的数量逐项对应；
  公开 slot/facade/共享状态包装成本分类列出，不把逻辑预算写成进程 RSS
  硬上限。
- 所有逐帧可变存储在执行器创建时一次建立；Decode/Encode 路径不新增
  heap allocation。失败返回固定 enum 和数值上下文，不在热路径构造详细字符串。
- Frame 大小、pipeline/message/field/enum 索引、指针+长度组合和 input/output
  overlap 在进入实质写入前失败关闭。`nullptr` 只能和零长度组合。
- 复杂度沿用当前 Plan 的 candidate/matcher/field/enum/integrity/text 上界和 operation
  counters。首片只记录可经测试观察的操作上界，不宣称性能指标。
- 同一执行器的并发调用不等待，直接返回 `WORKSPACE_BUSY`；不影响使用独立
  workspace 的其他执行器。

## 7. 内部适配与文件范围

阶段 1 实现建议严格限定为：

| 范围 | 预期改动 |
| --- | --- |
| `include/pae/compiler.h` | 仅增加内部共享状态接入所需的私有 friend/实现宣明；保持 move-only 公开语义 |
| `include/pae/codec.h` | 新增上述公开执行类型；头文件自洽，不包含 `src/` 私有头 |
| `src/public_api/compiler.cpp` | 将 artifacts 收口到可计费的不可变 `CompiledState`，保留现有 metadata 行为 |
| `src/public_api/codec.cpp` | 新增唯一 Core 适配层：创建 workspace/slot，映射 selector/value/status/result，发布借用 view |
| `src/public_api/compiled_state_internal.h` | 如 compiler/codec 需共享私有状态，只在此定义；不安装、不导出 |
| `src/protocol_core/*` | 原则上不改协议逻辑；只允许为资源测量增加有界、不暴露 Plan 的内部查询，并需独立测试 |
| `src/public_api/CMakeLists.txt` | `pae_public_api` 增加 `codec.cpp`，PRIVATE 链接 Core；`PAE::pae` 仍是消费者唯一 target |
| `tests/public_api/` | 公开头自洽、生命期、Binary/ASCII 执行、typed 值、失败、资源与无分配回归 |
| `examples/public_api_codec/` | 一个仅 include `pae/*` 并只链接 `PAE::pae` 的最小消费者 |

不应在本片修改 Schema、Plan 数据形状、Codec 诊断优先级、Framer/Host/Lab 行为或
历史 evidence 格式。如公开映射发现 Core 缺少已确认执行事实，先将其作为单独
缺口交总控，不在 facade 中猜测或二次实现。

## 8. 阶段 1 最小验收矩阵

| 组别 | Debug / Release 最小证据 |
| --- | --- |
| 公开头和边界 | `codec.h` 单独 include；公开头不出现 `protocol_plan` / `protocol_core` / `src/`；编译时 Schema 宏不改变头布局 |
| owner 生命期 | 编译 owner 创建执行器后 move/销毁，执行器仍正确工作；空/moved-from owner 创建失败；多执行器共享 Plan 但 workspace 隔离 |
| Binary Decode/Encode | 独立预期字节；UINT64/INT64/BOOL/BYTES/ENUM/Decimal64；unknown enum；raw/logical 直接发布；失败零交付 |
| ASCII Decode/Encode | 完整 ASCII record 的 BYTES view、字符限制、terminator conflict、capacity 失败；不把 stream chunk 当 record |
| 状态映射 | Core 每个可达 status/conversion error 有显式映射；失败字段/输入索引及 required size/count 保留 |
| 借用与污染 | 下一次调用使旧 view 失效；失败不暴露上次成功；BYTES 确认指向输入；Encode 失败 `bytes_written == 0` |
| 资源与并发 | 精确/差 1 byte 创建限制；整数溢出失败关闭；创建后首次及重复调用无分配；同执行器 busy；多执行器并发不误共享 |
| 内部共存 | 现有 Core、Compiler、Host、Lab 相关自动化不回退；本片不改变协议字节、候选顺序或诊断顺序 |
| Product-only | `PAE_BUILD_TESTING=OFF` 可构建 `PAE::pae`，不注册测试；不引入 Qt、Lab 或 transport 依赖 |
| 仓库外 consumer | 在独立目录仅 include 安装/导出头、只链接 `PAE::pae`，Binary/ASCII 两个最小程序在 Windows x64 D/R 编译运行，不出现私有 include 路径 |

静态库首片先闭合上述接口和消费者。动态库导出、安装包布局和跨编译器 ABI
属于后续交付阶段；本设计不把 Windows Debug/Release 仓库内通过扩大为它们已通过。

## 9. 实施停点

总控确认以下三点后再进入代码实施：

1. 接受“内部共享不可变 `CompiledState` + 每执行器独占 workspace”作为默认生命期。
2. 接受首片 Decode 返回执行器内部 slot 的短期 view，Encode 使用调用者 Buffer，
   不提供隐式 owning result 副本。
3. 接受首片 ENUM Encode 只接收已知枚举 selector；任意 raw enum Encode 不借本次
   公开 API 擅自扩大。

本文是实现输入，不是公开 API 稳定性承诺，也不是构建、自动化、Linux、真实协议、
硬件或生产验证证据。
