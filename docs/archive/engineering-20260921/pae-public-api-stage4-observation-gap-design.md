# PAE 公开 API 阶段 4 Binary 观察缺口设计

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-15。状态：只读设计已完成，待总控复核；本文不是已确认接口契约，也不表示代码、
构建、SDK 或 Lab 迁移已经实施。

## 1. 基线、范围与方法

- 现场分支/HEAD：`main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`；开始时暂存区为空，
  共享 dirty worktree 原样保留。
- 本轮按阶段 4 当前状态，只核对 Binary Lab 现有启用路径与 public Compiler/metadata、Codec、Host；
  不设计 TLV、数组、嵌套、ASCII/流式迁移、稳定 ABI、Runtime 或 UI 新功能。
- 唯一写入是本文。没有修改源码、CMake、Schema、README、包、索引或原始证据；没有构建、测试、
  UI 运行、网络收发、目录清理、Stage、Commit 或 Push。
- 结论来自当前源码静态核对。既有测试文件和验证报告仅用于确认历史覆盖意图，不作为本轮重新执行
  的证据。

## 2. Findings 优先

### F1，首片阻断：公开 metadata 没有 Binary 物理布局和本次实际范围

**静态确认。** `FieldDescription` 目前只公开身份、logical `ValueKind`、Encode 来源和枚举范围，
没有 byte range、bit mask、可变 BYTES 长度或 Binary/ASCII 表示类别
（`include/pae/protocol_description.h:45`）。公开 `CompiledProtocol::Field()` 也只映射这些字段
（`src/public_api/compiler.cpp:381`）。

当前已启用的 Binary Decode UI 会：

- 用大小端后的物理 byte mask 表示跨字节 bitfield；
- 用实际帧长解析 bounded payload 的本次 byte range；
- 显示/高亮 integrity storage 和 computed-length storage；
- 用 BYTES 长度上下界做输入与展示判断。

这些事实现在从私有 `PlanBundle` 复制到 `OwnedDescription`
（`tools/protocol_lab_binary/owned_description.h:34`、`:52`），实际范围由
`ResolveActualFieldRange()` / `ResolveActualIntegrityStorage()` 解析
（`tools/protocol_lab_binary/owned_description.cpp:328`、`:342`）。Qt 路径确实读取 bit masks、
integrity/computed storage 和实际 range（`tools/protocol_lab_ui/document_tab.cpp:133`、`:2784`、
`:2804`）。因此仅替换 include/target 会丢失已经启用的 Hex 高亮和位置展示；让 UI 继续解析
Schema 或私有 Plan 不能算公开 API 迁移。

### F2，已满足：成功 Decode 的 typed/logical、conversion raw 和失败隔离

**静态确认，无需新增首片接口。** `DecodedFieldView` 已公开六种 logical 类型，并从同一次成功
Decode 的 Core workspace 读取 conversion raw；`ConversionRawKind/UInt64/Int64` 位于
`include/pae/codec.h:130`、`:145`，实现读取实际 workspace 而非反算 logical
（`src/public_api/codec.cpp:525`）。`HostCandidateView` 同时携带当前 frame、Codec status、record、
失败字段和 tainted 事实（`include/pae/host_endpoint.h:101`）。失败没有成功 record，业务 sink 只接收
成功，符合当前 Binary 结果物化边界。

结论是：Decode 结果值本身不是阶段 4 缺口；迁移适配层只需在同一 candidate callback 内复制
frame、record/field、BYTES 和 raw，不能再 Decode，也不能将 logical Decimal 反算 raw。

### F3，条件性阻断：公开 Encode 没有同调用 conversion raw 观察

**静态确认，但是否进入首片取决于总控确定的迁移出口。** `EncodeResult` 目前只有状态、输出长度、
失败输入/字段和 conversion error（`include/pae/codec.h:190`）；Host Encode callback 只有 message 和
输出 bytes（`include/pae/host_endpoint.h:119`、`:189`）。Core 已在本次 Encode 中计算并复核
`encode_conversion_raw_values_`（`src/protocol_core/complete_record_codec.cpp:1354`、`:1798`、
`:2127`），但成功返回后没有公开 success-only view。

当前非 Qt Binary 层的 Encode 已启用并有测试：`PreparedBinary::Encode()` 在
`tools/protocol_lab_binary/prepared_binary_encode.cpp:78`，同次 Host callback 复制 frame、全部字段
物理 presentation 和动态 integrity range（`:237`、`:253`、`:265`）。不过它明确只保留 caller
logical inputs，不伪造 conversion raw；当前 `BinaryHostAdapter` 的 UI 接口只接通
`DecodeComplete()`，没有 Encode 方法（`tools/protocol_lab_ui/binary_host_adapter.h:98`）。

因此：

- 若首片出口是“保持当前 Qt Binary Decode UI 无损迁移”，Encode raw 不阻断该首片；
- 若首片同时要求迁移 `pae_protocol_lab_binary_materializer` 已启用的 headless Encode 行为，物理布局
  必须进入首片，但现有行为仍可诚实显示“conversion raw 未观察”；
- 只有总控要求新增“Encode 实际 conversion raw”展示时，才需要下文 success-only Encode observation
  接口。不能为制造字段展示而对输出再次 Decode。

### F4，局部必需：Binary Encode 的 BYTES 长度边界未公开；完整约束模型可延期

**静态确认。** 当前 public metadata 已能通过 `value_kind`、`encode_value_source` 和枚举表决定
caller input 的字段集合、类型和已知枚举；Codec 继续权威检查缺失、重复、类型、数值/Wire 范围、
conversion 和 final review。真正缺少且当前 UI 已使用的输入事实是 fixed/bounded BYTES 的
`minimum/maximum` 长度（`tools/protocol_lab_ui/document_session.cpp:43`）。

Binary 首片不需要公开通用 constraints AST、Decimal 可接受全集或提前复刻 Codec 数值判断。数值
越界仍交给 Codec 并保留 `VALUE_NOT_REPRESENTABLE`；枚举表已经公开。ASCII 字符集、控制字符和文本
长度属于后续 ASCII 迁移，不进入本片。

## 3. 当前能力与首片判定

| 事实 | 当前 public API | Binary Decode UI 首片 | Headless Encode 同步迁移 |
| --- | --- | --- | --- |
| Protocol/Pipeline/Message/Field 身份与显示文本 | 已有，字符串借用 `CompiledProtocol` | 直接使用 | 直接使用 |
| Pipeline/Message Decode/Encode 可用性、输出上界 | 已有 `MessageExecutionDescription` | 直接使用 | 直接使用 |
| logical 类型、caller/constant/computed 来源、enum | 已有 | 直接使用 | 直接使用 |
| Decode frame/status/失败字段/typed fields | 已有 Codec/Host view | 直接使用 | 不适用 |
| Decode conversion raw | 已有，同次成功 workspace | 直接使用 | 不适用 |
| Binary/ASCII 表示类别 | 未明确公开 | **必需** | **必需** |
| 固定 byte range、物理 bit masks | 未公开 | **必需** | **必需** |
| bounded 本次实际 field/integrity range | 未公开 | **必需** | **必需** |
| computed-length/integrity storage 描述 | 未公开 | **必需，保留现有信息面板/失败高亮** | **必需，保留 presentation** |
| fixed/bounded BYTES 长度上下界 | 未公开 | 展示所需 | **输入预检必需** |
| Encode bytes、status、失败定位 | 已有 | 不适用 | 直接使用 |
| Encode actual conversion raw | 未公开 | 不阻断 | 仅新增 raw 展示时必需 |
| 完整数值/Decimal/ASCII constraints 模型 | 未公开 | 延期 | 延期，Codec 仍权威拒绝 |

## 4. 推荐的最小应用无关 API

以下名称是供总控选择的设计草案，不在本轮冻结。建议分为 **P（物理描述）** 和可选的
**E（Encode raw observation）** 两个可独立落地的加法子片；P 是 Binary Decode 无损迁移前置，E
只在总控要求首片新增 Encode raw 时进入。

### 4.1 P：无分配的 Binary 物理描述与实际范围查询

在 `protocol_description.h` 增加纯值类型，避免暴露内部 `WireCodec`、`PlanBundle` 或容器索引：

```cpp
enum class RecordRepresentation { BINARY, ASCII_TEXT };
enum class FieldPhysicalKind { BYTE_RANGE, BIT_MASKS };

struct ByteRange {
  std::size_t offset = 0U;
  std::size_t length = 0U;
};
struct ByteLengthBounds {
  std::size_t minimum = 0U;
  std::size_t maximum = 0U;
};
struct PhysicalBitMask {
  std::size_t frame_byte_index = 0U;
  std::uint8_t mask = 0U;
};
inline constexpr std::size_t kMaximumFieldPhysicalBitMasks = 8U;

struct FieldObservationDescription {
  std::size_t flat_field_index = 0U;
  std::size_t message_index = 0U;
  std::size_t field_index = 0U;
  FieldPhysicalKind physical_kind = FieldPhysicalKind::BYTE_RANGE;
  ByteRange maximum_byte_range;
  ByteLengthBounds byte_length;
  std::array<PhysicalBitMask, kMaximumFieldPhysicalBitMasks> bit_masks{};
  std::size_t bit_mask_count = 0U;
  bool actual_range_depends_on_frame_size = false;
};
```

`CompiledProtocol` 增加三类只读查询：

1. `MessageObservation(message_index)`：返回表示类别、min/max frame bytes、可选 integrity storage、
   `storage_at_payload_end` 和可选 computed-length storage；
2. `FieldObservation(flat_field_index)`：返回上述固定/最大布局及 BYTES 长度上下界；
3. `ResolveMessageObservation(message_index, actual_frame_size)` 与
   `ResolveFieldObservation(flat_field_index, actual_frame_size)`：验证实际帧长后返回本次 byte range、
   integrity storage 或固定 bit masks。

实际解析结果应带小型显式状态（`OK`、`INVALID_COMPILED_PROTOCOL`、`OUT_OF_RANGE`、
`FRAME_SIZE_MISMATCH`、`INTERNAL_ERROR`），而不是把损坏 owner、错误索引和不合法帧长都压成同一个
空值。成功的零长度 bounded payload 返回 `{offset, 0}`，不是“无范围”；bitfield 没有伪造的完整
byte range，只返回非零 mask。

选择固定 `std::array<..., 8>` 是基于当前 bit container 1/2/4/8-byte 上限，只表达当前 Binary
事实，不为未来 TLV/递归结构预留通用树。查询从冻结 execution plan 映射 numeric bit mask 到物理
字节；消费者只复制返回值，不实现大小端/位编号算法。

### 4.2 E：成功 Encode 的同调用 conversion raw view

若总控将 Encode actual raw 纳入本次迁移，建议新增窄接口，而不是发布“全部生成字段快照”：

```cpp
struct EncodeConversionRaw {
  FieldSelector field;
  RawIntegerKind kind = RawIntegerKind::UINT64;
  std::uint64_t uint64_value = 0U;
  std::int64_t int64_value = 0;
};

class EncodeObservationView final {
 public:
  bool HasValue() const noexcept;
  std::size_t MessageIndex() const noexcept;
  std::size_t ConversionRawCount() const noexcept;
  std::optional<EncodeConversionRaw> ConversionRaw(std::size_t ordinal) const noexcept;
};

struct EncodeResult {
  // existing members unchanged in meaning
  EncodeObservationView observation;
};
```

`HostOutputView` 在 `action == ENCODE` 的成功 callback 内转发同一个 observation。它只回答本次
Encode 真正计算并最终复核通过的 conversion raw；caller logical input 仍由调用者持有，字段顺序和
physical mapping 从 P 查询，输出 bytes 仍是实际 Frame。constant/computed 或非 conversion 字段不
应被伪造成“已观察 logical 值”；若将来确需全部生成值，应单独契约化 Core 发布点和资源，而不是在
本片解析输出 Frame。

## 5. 所有权、失效与失败语义

### 5.1 metadata / physical 查询

- `CompiledProtocol` 仍是不可变事实 owner；现有字符串 `string_view` 随 owner move/销毁失效。
- P 查询建议返回不含指针/字符串的值对象，因此返回对象本身可复制；其中的 Message/Field index 只
  在产生它的 `CompiledProtocol` 身份内有意义，不能用相同文本 ID 冒充同一 Plan。
- Lab 准备阶段必须以同一个 `CompiledProtocol` 创建 Host 并复制描述，配置重载后整体替换；旧
  description/result 不导入新实例。Tab/load/session/request 身份仍由 Lab 管理，不进入 PAE API。

### 5.2 Decode

- 沿用现有规则：只有 `CodecStatus::OK` 才有 `DecodedRecordView` 和 conversion raw；失败 candidate
  可保留当前 diagnostic frame、status、失败字段，但字段数和 raw 均为零。
- borrowed record/field/BYTES 在下一次成功取得同一 Codec guard 的 Decode/Encode、owner move 或
  销毁时失效；竞争返回 `WORKSPACE_BUSY` 的调用不得破坏正在执行调用的发布状态。

### 5.3 Encode

- E view 仅在 Core Encode 完成最终复核且 `CodecStatus::OK` 后发布。任何输入拒绝、conversion
  失败、buffer too small、final review failure 或 internal error 都返回无值 observation，
  `bytes_written == 0`，不得公开中间 raw 或将已修改 buffer 当交付结果。
- view 与 `DecodedRecordView` 使用同一 owner epoch / generation 失效模型；下一次成功取得 guard 的
  Codec 调用使旧 Encode view 失效。Host callback 只能同步读取，需持久化时由 Lab 在 callback 内按
  预检预算复制。
- `HostStatus::CALLBACK_FAILED` 可以保留“Codec 曾成功、bytes_produced 已发生”的既有事实，但不表示
  Lab 复制/业务交付成功；适配层复制失败不得发布部分 `ObservedEncode`。

## 6. 内部事实来源和代码影响

### 6.1 P 子片

推荐变更范围：

- `include/pae/protocol_description.h`、`include/pae/compiler.h`；
- `src/public_api/compiler.cpp`；
- `tests/public_api/public_consumer_metadata_tests.cpp` 及 public-only consumer；
- SDK install/export 白名单和相关契约/验证文档。

**不需要改变 Schema、Loader、DomainValidator、Frozen Plan 或 Core。** 所需 offset/width、bit
container byte order、bit mask、bounded payload、integrity 和 computed-length 已存在冻结 Plan；
只需由 public facade 进行有界、checked 的只读映射。建议按调用即时返回固定大小值，不增加
metadata heap allocation，也不把 Lab 的 `std::vector`/`std::string` DTO搬入 PAE。

### 6.2 E 子片

推荐变更范围还包括：

- `include/pae/codec.h`、`include/pae/host_endpoint.h`；
- `src/public_api/codec.cpp`、`src/public_api/host_endpoint.cpp`；
- `src/protocol_core/complete_record_codec.h/.cpp` 的**内部 success-only 读取器**；
- 对应 public Codec/Host/Core tests 和 package consumer。

**需要最小 Core 内部改动，但不需要 Plan 结构或计费数量改变。** Core 已有按 conversion slot 计费的
`encode_conversion_raw_values_`，建议只增加本次成功 Message/发布标志，并按字段的
`conversion_slot` 读取现有值；不要增加逐字段 raw 数组。新增标量位于嵌入 facade 的 workspace
对象，须由 `sizeof(Impl)`/facade 计费复核；现有 conversion vector payload 计费不应重复增加。每次
读取可在当前 Message 字段范围内有界扫描，不分配堆内存。

若实现者发现必须保存所有字段的 Encode logical/raw 快照或扩展 Frozen Plan，说明已超出本文推荐的
最小 E 子片，应停下交总控重新决定，而不是静默扩大 workspace 和公共语义。

## 7. 预算、操作量与线程边界

- P 查询的最大 bit mask 数固定为 8；所有 `offset + length` 使用减法式边界检查，拒绝 `size_t`
  溢出。重复查询不得分配。
- 编译 owner 的 metadata/Plan 共享计费保持现状；Lab 自有 `std::vector`/QString/Hex highlights 是
  消费侧副本，必须按真实 capacity 在 Lab 预算中另计，不能记入 PAE 后再重复宣称。
- E 不新增转换运算，也不增加第二次 Decode。只读取 Encode 已计算并通过 final review 的 slot；
  操作上界是当前 Message 字段数，不作性能结论。
- Codec/Host 继续单实例串行、非等待 busy guard；观察 view 不能与修改该实例的调用、move 或销毁
  并发。此接口不引入线程调度、注册路由或跨线程生命周期管理。

## 8. 建议实现与验证出口

若总控接受 P 子片，建议先实现并验证 P，再让 Lab 迁移 Decode；E 是否同批由总控结合另一份 Lab
迁移报告决定。

### P 必测

1. fixed UINT64/INT64/BOOL/ENUM/BYTES 的 byte range；大小端与 lsb0/msb0、跨字节 bit mask；
2. bounded payload 零/中间/最大长度的实际 range，错误帧长失败关闭；
3. fixed/dynamic integrity storage、computed-length storage；
4. Binary/ASCII 表示不混淆，Schema 0.1～0.9 旧行为不改；
5. 空/moved-from owner、越界 Message/Field、内部不一致失败；
6. 重复查询零分配，返回值不含私有 Plan 指针；public header self-contained/boundary。

### E 条件必测

1. 正/负 conversion raw 与独立 Encode Frame 对照；raw kind/Field selector 精确关联；
2. 无 conversion、零输入、constant/computed 字段不伪造 raw；
3. 输入早拒绝、conversion 失败、buffer不足、final review失败均无 observation；
4. 下一调用、move、销毁失效；busy/reentrant/callback failure 保持既有语义；
5. Host 同次 callback 转发，适配层一次复制；没有额外 Decode 和逐调用堆分配。

### 构建与包增量复验

- Windows public metadata/Codec/Host 专项 Debug/Release；
- Product-only、Testing-off 和 header boundary；
- Source、Static D/R、Shared D/R 包外综合 consumer；
- Shared DLL exports/dependents、匹配配置正例和四个错配配置门禁；
- 若只落 P，无需重跑无关 Lab/Qt/网络矩阵，待 Lab 消费改动后再跑 Binary 定向回归和一次用户授权
  的简短 UI 烟测。

这些是后续验证建议；本轮没有执行。

## 9. 明确延期与非目标

- 完整数值 constraints、Decimal 可输入全集、默认值/表单策略；Codec 仍是最终执行门禁。
- ASCII 文本字符集、控制字符和流式位置；留给阶段 4 后续 ASCII/流式迁移。
- 全部 Encode 生成字段 logical snapshot、历史记录或跨调用观察；首片只在需要时提供 conversion raw。
- TLV、数组、嵌套、递归 layout、通用表达式、C ABI、稳定 ABI、Runtime/Session、Transport、Qt 类型。
- 数字签名、来源认证、防篡改、生产/现场/硬件/Linux/性能声明。

## 10. 给总控的建议

1. **先批准 P 子片**：它是当前 Binary Decode UI 无损迁移的确定前置，且不需要改变 Plan/Core。
2. **将 E 作为显式开关决定**：若本次只迁移已接入 Qt 的 Decode，E 延期；若要同时保持 headless
   Binary Encode 并新增 actual raw 展示，再批准最小 Core success-only accessor。
3. 不批准“完整字段约束模型”进入本片；只补 Binary BYTES 长度边界、表示类别及物理事实。
4. PAE 接口稳定后再派 Lab 写入，避免 public header/CMake 与 Lab 适配共享文件并发修改。

当前限定结论：公开 Decode 值链可复用，P 是一个确定的阶段 4 缺口，E 是由迁移出口决定的条件
缺口；未发现必须修改 Schema 或 Frozen Plan 的理由。状态为“已完成派发范围，待总控复核”。
