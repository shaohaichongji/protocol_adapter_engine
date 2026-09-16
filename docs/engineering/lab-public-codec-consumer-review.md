# Lab 对公开完整记录编解码的消费者需求核对

状态：2026-09-14 阶段 0 只读核对结论，待总控与 PAE 公开接口设计合并复核。本文只描述
Lab 作为消费者对完整记录 Decode/Encode 的最小需求，不表示接口已经实现、构建或验证，也不扩大
既有协议与 Schema 语义。

## 1. 核对范围与结论

本次核对当前三条完整记录调用路径：

- Schema 0.9 Binary 通过 `PreparedBinary -> host_endpoint::Session` 执行；
- Schema 0.10 ASCII 通过 `OfflineAdapter -> protocol_core` 执行，并可切换至 Host observer；
- Schema 0.5～0.8 旧路径通过 `v06::ExecutionBridge -> protocol_core` 执行。

Lab 对阶段 1 公开 Codec 的核心需求不是复制现有 Lab DTO，而是让一次公开 Decode/Encode 调用直接提供
调用者安全使用的执行事实：稳定的 Pipeline/Message/Field 身份、精确 typed 值、必要的 raw/logical
对应关系、明确的失败位置和输出长度。公开接口不得要求 Lab 构造或核对私有 `PlanBundle*`、
`FieldRef`、`EnumValueRef`，也不应迫使 Lab 为展示再执行一次 Decode。

第一片应覆盖完整记录，不包含 stream framing、Host binding、Socket、线程、重试、证据文件或 UI
状态。Binary 与 ASCII 可以共享同一套调用形状，但各自不支持的方向仍须返回明确的
`OPERATION_NOT_SUPPORTED`，不能由 Lab 根据 Schema 猜测成功语义。

## 2. 当前真实消费路径

### 2.1 Binary Decode

当前 `PreparedBinary::DecodeOnce()` 先依据 binding/flow 和 Host observation 拒绝无效状态，再调用
`Session::Decode()`；Candidate callback 随后核对 Plan、generation、binding、flow 和 Pipeline 身份，并
复制出自持结果。`MaterializeCandidate()` 还会重新遍历私有 Plan 来验证字段引用、BYTES 借用区间、
枚举引用及转换字段的独立 raw integer：

- `tools/protocol_lab_binary/prepared_binary.cpp:315-369`
- `tools/protocol_lab_binary/candidate_materializer.cpp:76-144,175-268`

这说明公开 Decode 至少要直接给出：

1. 本次状态及是否实际调用 Codec；
2. 匹配到的 Message 身份；
3. `field_count`、失败字段身份和 conversion error；
4. 每个字段的稳定 Field 身份、typed logical 值；
5. BYTES 的调用期视图及其在输入 Frame 中的范围；
6. 枚举 raw 值、known 状态及 known item 身份；
7. Decimal 转换字段的精确 `Decimal64` logical 值和 Core 实际观察到的原始整数；
8. preserved unknown enum 对应的 `tainted` 事实。

若公开结果已经以内部可信关联生成上述事实，Lab 只需在自身预算内复制并附加 display text、物理
高亮和 UI revision，不应再次拿 Plan 指针证明字段属于本次执行。

### 2.2 Binary Encode

当前 Binary Encode 由 Lab 先按描述检查 Message 是否属于绑定 Pipeline，再把按 Field ID 提交的
`UINT64`、`INT64`、`BOOL`、`BYTES`、`ENUM`、`DECIMAL64` 转换成 Host 的 Plan 绑定值；枚举输入仍需
手工构造指向 `session_->Plan()` 的 `EnumValueRef`：

- `tools/protocol_lab_binary/prepared_binary_encode.cpp:99-188`
- `tools/protocol_lab_binary/prepared_binary_encode.cpp:190-268`

公开 Encode 输入应由 PAE 按稳定身份自行解析，不允许消费者提交 Plan 指针。最低输入字段为：

- Pipeline 与 Message 的稳定 ID 或由该 `CompiledProtocol` 解析出的受控 handle；
- Field ID；
- 明确的 value kind；
- 对应的精确 scalar、`ByteView`、枚举 item ID 或 `Decimal64` 值。

PAE 应区分 Message 不属于 Pipeline、未知/重复/缺失 Field、常量或计算字段覆盖、类型不符、未知
枚举、BYTES 长度、数值不可表示和输出缓冲不足。失败时需给出失败 input ordinal 与 Field 身份（若
已确定），且不把上一次成功 Frame 或字段结果当作本次结果。

成功结果至少包含实际 `bytes_written`、输出 Frame 和本次输入/生成字段的执行结果。特别是转换字段
的 raw integer 必须来自本次 Encode 的实际转换，不能由 Lab 从 logical 值反算；constant/computed
字段若要显示，也应由同一次执行产生可观察结果或明确的输出字段视图。

### 2.3 ASCII Decode/Encode

ASCII 离线 Decode 为选定 Pipeline 准备 caller-owned slot 数组，一次调用 Core；成功后要求字段均为
BYTES，并把借用值映射回输入 Frame 的实际范围。失败时保留诊断输入、Message/Field 身份：

- `tools/protocol_lab_ascii/ascii_offline_adapter.cpp:400-478`

ASCII Encode 按 Message 的 Encode action 提交 BYTES 字段，使用 Message 最大记录长度分配输出，
读取 `bytes_written` 后再依照 literal/field segment 生成展示范围：

- `tools/protocol_lab_ascii/ascii_offline_adapter.cpp:481-584`

因此公开 Codec 需同时满足：

- ASCII 字段 BYTES 可变长，零长度与非零长度指针规则明确；
- Decode 返回每个字段在输入记录中的实际 byte range；
- Encode 返回实际长度，不能要求调用方把整个 capacity 当作有效 Frame；
- 可查询或随成功结果返回各 Field 在输出记录中的实际 byte range；
- Decode-only/Encode-only action 的反向调用返回 `OPERATION_NOT_SUPPORTED`；
- literal-only 或零字段成功与失败可区分，不能以“字段数组为空”推断失败。

ASCII escaped text 的解析、控制字符的界面输入方式以及 Hex/ASCII 展示切换仍是 Lab 行为；PAE 只处理
已经物化的 bytes，并按已编译约束判断其是否允许。

### 2.4 旧 Binary 桥

旧 `ExecutionBridge` 同时承担结构查询、Core 调用、Lab Result 映射、执行 phase 观察和 Encode 后的
独立 Decode review；它还暴露借用 `PlanBundle` 供 UI 建模：

- `tools/protocol_lab/v06_execution.h:88-136`
- `tools/protocol_lab/v06_execution.cpp:382-426,429-505,675-801`

这些职责不能整体成为公开 PAE API。阶段 1 只需要替代其中结构匹配、单次 Decode/Encode 和 typed
结果部分。Lab 的 phase 计时、旧 values/result 格式、SHA、兼容诊断 ID 和 Evidence 仍留在适配层。
若 Evidence 流程明确需要独立复核，可以把它作为第二次、显式命名的 Lab 操作执行；UI 普通 Encode
不得为展示而隐藏地重复 Decode。

## 3. 公开 Codec 的最小消费者契约

### 3.1 对象与生命周期

公开编译首片已有 move-only `CompiledProtocol`，并明确 metadata 中的 `string_view` 借用其生命期。
Codec 接口应继续隐藏私有 Plan，同时满足以下消费者约束：

- 一个有效 `CompiledProtocol` 可以创建多个独立执行 workspace/instance；
- 编译产物的有效期覆盖所有执行对象；或者执行对象在内部安全共享所需冻结状态，但具体机制不向
  Lab 暴露；
- 同一 workspace 的操作串行；不同 workspace 在只读共享同一编译产物时允许并发，若暂不支持则
  必须明确返回/说明，而不是产生未定义行为；
- 输入 view 只需在调用期间有效；输出借用 view 的失效点必须明确，至少在下一次同 workspace 操作、
  Reset 或对象销毁时失效；
- Lab 能在 view 有效期内完成有界复制，不要求 PAE 默认创建另一份大对象；
- 编译产物 move 后必须重新获取 metadata/handle；失效 handle 不能误绑定新对象。

从 Lab 角度，推荐“`CompiledProtocol` 保持 owner，按其创建独立 `CompleteRecordCodec`/workspace”的
形状。是否用内部共享所有权或受控借用由 PAE 设计决定，但不能要求 Lab 把整个编译 owner 转移给
第一个 workspace，因为同一文档还需要 metadata，并可能需要多个执行通道。

### 3.2 Decode 请求与结果

最小请求：Pipeline 身份、输入 `ByteView`、调用者提供的 Field result slots 或显式的有界结果接收器。

最小结果：

| 类别 | 必需内容 |
| --- | --- |
| 总体 | status、codec_attempted、Message 身份、field_count/required_field_count、tainted |
| 失败定位 | failed Field 身份、conversion error；身份尚未确定时保持空值 |
| scalar | `UINT64`、`INT64`、`BOOL` |
| BYTES | 调用期 view 和实际 Frame byte range |
| ENUM | raw value、known、known item 身份 |
| DECIMAL | 精确 coefficient/scale，以及实际 raw integer kind/value |
| 缓冲 | slots 不足时返回 required count，不发布截断成功结果 |

`UNKNOWN_MESSAGE` 与 `AMBIGUOUS_MESSAGE` 必须可区分；Integrity、Length、ASCII character 等已有 Core
失败状态不得压平为一个 UI 字符串。公开层可以提供稳定 enum，Lab 自行映射用户文案和诊断 ID。

### 3.3 Encode 请求与结果

最小请求：Pipeline、Message、由 Field 身份关联的 typed input 数组，以及 caller-owned 输出 buffer。
PAE 负责解析 Field/Enum 身份并验证其属于同一编译产物，不接受消费者构造的内部引用。

最小结果：

| 类别 | 必需内容 |
| --- | --- |
| 总体 | status、codec_attempted、bytes_written、required_size |
| 失败定位 | failed input ordinal、failed Field 身份、conversion error |
| 成功字段 | 本次 INPUT/CONSTANT/COMPUTED 字段的稳定身份与实际 typed 结果，支持 UI 单次展示 |
| 精确值 | Decimal 不使用 `double`；转换 raw 值来自执行；枚举保留 item 身份和 raw 值 |
| 范围 | 变长 ASCII/Binary 成功输出中可观察字段的实际 byte/bit range 或可由公开 metadata 查询 |
| 缓冲 | buffer 不足不发布部分成功 Frame；`required_size` 可用于调用方重新分配后显式重试 |

公开 Encode 不应隐式保存输入、副本或上一次 Frame。Lab 若需要 Preview、历史结果或 Evidence，显式
复制到自己的状态对象。

### 3.4 预算与失败边界

Lab 需要在调用前查询或由创建结果获得：workspace accounted bytes、最大 Field slot 数、最大 Encode
输出、单字段/记录边界和必要 alignment。公开接口应使用 checked arithmetic，并做到：

- 超出输入、slot、workspace 或 output capacity 时返回确定状态；
- `BUFFER_TOO_SMALL` 同时返回所需大小，不越界写入；
- 失败结果不包含旧成功的 fields/frame；
- PAE 内部 allocation failure 与协议值不可表示可区分；
- `noexcept` 仅用于确实能兑现的边界，不把 C++ 容器分配异常伪装成协议失败；
- accounted memory 是契约定义的逻辑预算，不宣称进程 RSS 上限。

## 4. 继续保留在 Lab 的内容

以下内容不是阶段 1 公开 Codec 的职责：

- Hex、ASCII escaped、旧 Values JSON 的输入解析与草稿校验；
- `DocumentId`、load/plan/selection/input revision 和过期结果抑制；
- Tab/Flow 选择、Qt model、字段表、selector、颜色和高亮；
- display text、read-only annotation、物理位置字符串及本地化错误文案；
- 把调用期结果复制成 `UiFieldResult`、`InspectResult`、Preview 或 Binary retained view；
- Lab 自己的复制预算、active/temporary view 预算及 UI 发布原子性；
- Encode 后的 Preview 保存、Evidence/Replay/Compare、SHA、UDP 与事件时间；
- 旧 CLI 的格式兼容、exit code、phase 计数和测试注入 hook。

PAE 可返回通用 Field range、raw/logical、status 和资源事实；“怎样显示、保存多久、属于哪个 Tab/Flow”
始终由 Lab 决定。

## 5. 典型最小调用顺序

### 5.1 Decode

1. Lab 调用公开 Compiler，取得有效 `CompiledProtocol`，读取 metadata 建立选择模型。
2. Lab 为文档或独立执行通道创建 Codec workspace，并按公开资源要求准备 slots。
3. Lab 将 Hex/ASCII escaped 草稿解析为 bytes，核对自身 revision 未过期。
4. Lab 以 Pipeline 身份和 `ByteView` 调用一次 Decode。
5. 失败时只发布本次 status/诊断输入/可用失败身份；成功时在结果 view 有效期内有界复制 typed fields。
6. Lab 根据公开 metadata/range 生成表格和高亮，再以自己的 revision 原子发布。

### 5.2 Encode

1. Lab 从 metadata 取得可输入字段及其类型，验证并保存 typed drafts。
2. Lab 组装 Field ID + typed value 数组，按最大输出或上次 `required_size` 准备 buffer。
3. Lab 以 Pipeline、Message 和输入数组调用一次 Encode。
4. 失败时清除本次 Preview，只保留本次失败状态；buffer 不足由 Lab 显式扩容后重新调用。
5. 成功时复制有效 Frame `[0, bytes_written)` 以及同次执行返回的字段事实；不为 UI 展示追加 Decode。
6. Lab 生成 Preview/highlight 并按当前 revision 发布。

## 6. 最小针对性回归

阶段 1 实施后建议按以下层次验证；这些是建议，不是本轮已执行证据。

### 6.1 公开 Codec 专项

- owner/workspace：空 owner、move 后重取 handle、owner 与 workspace 生命周期、同 owner 两个 workspace；
- Binary Decode：成功、unknown/ambiguous、Integrity/Length 失败、BYTES 实际范围、known/unknown enum、
  tainted、Decimal exact logical + independently observed raw；
- Binary Encode：全部 typed kind、constant/computed、缺失/重复/未知 Field、类型不符、未知 enum、值不可
  表示、buffer exact/不足、变长实际 `bytes_written`；
- ASCII Decode/Encode：BYTES 变长、literal-only/零字段成功、实际 Field range、decode-only/encode-only
  的 `OPERATION_NOT_SUPPORTED`、ASCII 字符约束失败；
- 状态隔离：同 workspace 先成功再失败，失败结果不得保留旧 Frame/Field；另一 workspace 不受影响；
- 执行次数：普通 Encode/Decode 各只调用一次 Core；Encode 展示字段不得触发隐藏 review Decode；
- 越界：slot、输入、输出、workspace budget 的 exact limit 与 limit-1，确认无越界和无部分发布。

### 6.2 Lab 适配回归

阶段 4 真正迁移 Lab 时，至少复用现有：

- `pae.protocol_lab_binary.materializer/description/prepared/encode/integration_audit`；
- `pae.tools.protocol_lab_ascii.adapter`；
- `pae.tools.protocol_lab_ui.binary_stage1`、`ascii_session`、`inspect_state`、`document_state`；
- Binary/ASCII Qt smoke。

应新增一组适配断言：Lab 只包含公开 `include/pae` 头；普通 Encode 后 Core Decode 计数仍为零；公开
失败映射不残留上次成功 Preview；Decimal、unknown enum、BYTES range 和零字段成功的 UI 数据不退化。
Lab 回归通过不能替代仓库外公开 API consumer 验证，Qt smoke 也不能替代 Core 正确性验证。

## 7. 实施合并时的重点核对

总控合并 PAE 接口设计时，应重点检查以下差异：

1. 若设计只返回当前 `DecodeResult`/`EncodeResult`，则尚不能满足 Lab 对稳定身份、BYTES range、枚举
   item、Decimal raw 和 Encode 单次展示字段的需求；
2. 若执行对象消费并独占整个 `CompiledProtocol`，则会阻断同一文档继续查询 metadata 和创建多个
   workspace；需要调整生命周期形状；
3. 若公开输入继续出现 `PlanBundle*`、`FieldRef` 或 `EnumValueRef`，则私有 Plan 仍已泄露；
4. 若 Encode 成功只有 Frame 而没有必要字段执行事实，旧 UI 会继续依赖隐藏 Decode；应在阶段 1
   明确解决或明确限制首片 UI 迁移范围，不能静默保留重复执行；
5. actual byte/bit range 是 PAE 通用执行/布局事实；Qt highlight 与字符串格式化不是；
6. 第一片不需要设计 Framer/Host/DLL 完整接口，也不应把 Lab revision、Evidence 或 UI budget 塞入
   Codec。

## 8. 本轮证据边界

本报告依据当前源码、当前公开 Compiler 首片及现有测试源码静态核对形成。未修改任何执行代码或
公共头，未构建、未运行测试、未做仓库外消费、未做 Debug/Release 或人工 UI 验证。后续 PAE 设计
与本报告存在接口取舍差异时，应由总控按已确认边界合并后再派实现，不把本文当作已实现契约。
