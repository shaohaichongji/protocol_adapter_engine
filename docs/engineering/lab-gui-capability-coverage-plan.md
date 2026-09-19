# Lab 图形化能力覆盖盘点与分片计划

日期：2026-09-19

状态：**已完成只读定向盘点，待总控确认首片实施授权**

盘点基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`；盘点开始时工作树干净，未 Push

## 1. 结论

1. PAE 当前公开接口已覆盖配置编译、冻结元数据、完整记录 Decode/Encode、Binary/ASCII
   framing 查询、stream `Push/Continue/Reset`、Host binding/Flow 隔离和物理布局查询。主要缺口不在
   PAE 执行能力，而在 Lab 尚未把这些公开能力组织成一致、可观察的产品入口。
2. 当前 standalone Qt Lab 已通过 public API 覆盖 Binary complete Decode、ASCII complete
   Decode/Encode 与 ASCII stream；Binary complete Encode 和 Binary stream 仍未接入。现有 public
   Binary H1/H2 代码主动限制为 complete Decode，不是 PAE API 不支持。
3. 推荐首片为 **Binary complete-record Encode**。它不需要新增 PAE 协议能力，也不依赖 stream
   状态机，能复用现有 typed draft、字段表、Hex 输出、物理高亮、失败清空和 Flow 切换框架，是风险
   最小且用户价值明确的缺口。
4. 旧 non-Qt Binary Encode/stream adapter 可复用其 DTO 设计、预算方法、失败清空、冻结 suffix 和
   自动测试语义；其实现直接依赖 private Compiler/Core/Plan/Host，不能进入 installed-SDK Lab，也不能
   作为绕过 public API 的兼容层。
5. complete Encode 的基本产品入口没有 public API 阻塞；但若要求 Encode 成功后像 Decode 一样展示
   **所有字段的实际 logical/raw 值**，公开 `EncodeResult`/`HostOutputView` 目前只提供状态、输出 bytes
   和 Message identity，不提供 encode-side 字段结果或 conversion raw trace。首片应诚实展示 caller
   typed inputs、输出帧、实际物理范围、length/integrity/constant/computed 标注，不得为了补展示而重复
   Decode 或从 logical 反算 raw；更丰富的 encode trace 作为独立 public API 候选报告总控。
6. Binary stream 可作为第二片。公开 Framer/Host 已表达 candidate、业务输出、暂停、未消费输入与
   Reset；当前缺的是 Lab public Binary owner 与 UI 状态映射，不应复制旧 private Session。
7. Compiler 诊断当前在 worker 中保留了公开完整对象，但进入文档状态时只展示 `detail`。首片无需
   顺手重构；后续应把 stage/code/json pointer/byte offset/resource kind/required/limit 作为结构化
   诊断面板，而不是把所有 metadata/API 变成按钮。

本报告是源码、CMake、现有报告与测试入口的静态盘点；本轮没有构建、测试、启动 Lab 或改动任何
源码/CMake/脚本/依赖/SDK/部署。

## 2. 盘点边界与判定原则

- 保护 `deliverables/sdk`、`deliverables/lab/98df5e0`、`local_private`、当前部署和 Qt 输入；不把新功能
  写入或覆盖 `98df5e0` 产品目录。
- GUI 的目标是覆盖可观察的用户工作流，不是为每个 C++ API 生成按钮。
- PAE Core/Compiler/Framer/Host 继续拥有执行语义；Lab 只负责选择、输入、调用、状态映射和展示。
- candidate 形成、Decode 成功、业务 sink 返回、一次 `Push` 返回和整个输入耗尽是不同事实，GUI
  必须分别表达。
- static review、自动测试、D/R 构建、人工 UI 观察、发布与现场证据分别记录，不能相互替代。
- public API 缺信息时停在报告和决策点；不得读取 private Plan、复制 private Host Session 或在 UI
  重写协议语义。

## 3. 当前能力覆盖矩阵

| 能力 | PAE public 事实 | 当前 Lab 图形入口 | 判定与后续 |
| --- | --- | --- | --- |
| 编译诊断 | `CompileDiagnostic` 提供 stage、code、JSON pointer、byte offset、resource kind、required/limit/profile | worker 保留完整 `public_diagnostic`，Session/标签主要只显示 `detail` | **部分覆盖**；后续做结构化诊断面板，不为各字段建独立操作按钮 |
| Protocol/Pipeline/Message/Field/Enum metadata | `CompiledProtocol` 提供稳定只读查询及 Pipeline-Message execution availability | selector、字段表、详情区已消费大部分 metadata | **基本覆盖**；Binary 映射尚缺 encode association，字段参与信息需按实际 execution 填充 |
| 编译/执行资源 | compiler、codec、framer、host 均有 `MemoryReport` | 当前主要用于 adapter admission/测试，GUI 没有统一资源摘要 | **内部保证强、用户入口弱**；只建议“资源摘要/失败原因”，不把 allocation 字段逐一按钮化 |
| complete Decode | public Codec/Host | Binary 0.9 与 ASCII complete 已有 UI | **已覆盖**；保留成功结果与失败定位互斥、失败清旧结果 |
| complete Encode | public Codec/Host，支持六类 typed `EncodeValue` | ASCII 已有；Binary public H2 明确禁用 Encode | **Binary 缺口**；推荐首片 |
| framing capability/description | public 查询提供 input kind、strategy、maximum candidate bytes | ASCII stream 映射使用；Binary H1 未使用且主动拒绝 stream | **Binary 缺口**；第二片前置 |
| stream Push/Continue/Reset | public Framer 与 Host 均提供 | ASCII 0.11 已有；Binary public owner 无入口 | **Binary 缺口**；第二片 |
| Host bindings/Flow | public endpoint/action/pipeline/decode stream count，handle 代际和 Reset | Binary complete Decode 已有 Flow drafts/results 隔离；ASCII Host/stream 已覆盖 | **部分覆盖**；Encode action 与 Binary stream Flow 尚缺 |
| candidate observation | Host candidate observer 提供每个 candidate 的 Decode 状态、匹配、失败字段、conversion error | ASCII stream adapter 保留本步 candidate；Binary public 无 stream | **ASCII 已有、Binary 缺口** |
| business output | Host output sink 只在成功业务输出时触发，计数独立 | ASCII stream adapter已有 observer/business 计数 | **ASCII 已有**；Binary 第二片复用语义，不合并为“候选成功” |
| pause/继续 | framing stop reason、`bytes_consumed`、internal work、phase、reset-required | ASCII 公开 stream 保存 frozen suffix/cursor 并给出 Continue availability | **ASCII 已有**；Binary 第二片按同一契约实现 |
| physical raw/逻辑结果 | Decode field view 提供 logical、enum raw、Decimal conversion raw；物理查询提供 range/bit masks | Binary Decode 已显示 raw/logical 与实际物理位置；ASCII 按文本语义展示 | **Decode 已覆盖**；Encode 的实际字段 trace 有 public 信息缺口，见第 6 节 |
| length/integrity | Message/Field physical 与 resolved query 提供长度边界、computed length storage、integrity storage | Binary详情/Hex高亮已有基础；Encode 尚无 public UI | **首片可覆盖位置、输出和约束**，不能虚构“独立复算通过” |
| conversion | typed Decimal64 input；Decode 暴露 conversion raw；失败暴露 `ConversionError` | Binary Decode 已显示；Binary Encode 无入口 | **首片显示 typed input 与失败原因**；成功 raw trace 不在当前 public Encode 结果中 |

## 4. 当前 public Binary 接线的精确缺口

### 4.1 非 Qt owner

[`tools/protocol_lab_binary/public_binary_decode.h`](../../tools/protocol_lab_binary/public_binary_decode.h)
和对应 `.cpp` 当前只建立 `HostAction::DECODE` binding，要求 Pipeline 中所有关联 Message 均
`decode_available`，并在创建后拒绝 `Observe(handle).stream == true` 的 channel。执行接口只有
`Decode(flow, frame)` 与 Decode channel `Reset`。

因此它不能通过简单显示一个现有按钮获得 Encode/stream 能力；必须先把 owner 的 action、binding、
结果与状态模型补齐。这里是 Lab adapter 缺口，不是 PAE Core 缺口。

### 4.2 UI description 与 application adapter

[`tools/protocol_lab_ui/public_binary_description.cpp`](../../tools/protocol_lab_ui/public_binary_description.cpp)
已经从 public metadata/physical query 复制 Message、Field、Enum、最大/实际物理布局、integrity 和
computed length storage，但目前：

- Pipeline 只记录 `message_indices` 和 `decode_message_indices`，`PipelineDescriptor` 本身也没有
  `encode_message_indices`；
- Field 的 `decode_referenced` 被固定为 true，没有按各 Pipeline/Message execution 汇总；
- Message 的 `encode_available/decode_available` 没有按当前公开 execution 事实重新赋值；
- 没有 framing description，因此无法判断 Binary Pipeline 是 complete record 还是 stream。

[`binary_host_adapter_public.h`](../../tools/protocol_lab_ui/binary_host_adapter_public.h) 只暴露
`DecodeComplete`/`MapCurrent`。`DocumentSession::EncodeAvailable()` 对 Binary Host 文档直接返回 false，
`DocumentTab` 也对 Binary Host 隐藏 Encode 按钮。这些限制需要按能力解除，不能简单删除条件而让
调用落到旧 private bridge。

### 4.3 已有展示基础

当前字段表和 `DocumentSession` 已拥有 `TypedDraft`：`UINT64`、`INT64`、`BYTES`、`ENUM`、`BOOL`、
`Decimal64`，也已经区分 `INPUT`、`CONSTANT`、`COMPUTED`。Hex view、字段物理高亮、动态 payload
实际范围、integrity/computed length 详情、conversion 说明、失败字段定位和失败清除旧结果均可复用。

需补的是 public Binary execution 到这些中立 DTO 的映射，而不是再造一套 Qt editor 或把 PAE 类型
直接泄漏进 model/view。

## 5. 旧 Binary adapter 与 ASCII 路径的可复用边界

### 5.1 可复用的设计与测试语义

旧 [`prepared_binary.h`](../../tools/protocol_lab_binary/prepared_binary.h)、
[`prepared_binary_encode.cpp`](../../tools/protocol_lab_binary/prepared_binary_encode.cpp) 和 stream 实现
提供了可沿用的工程约束：

- typed call-scoped input，不把 Plan identity 或内部 FieldRef 暴露给 UI；
- caller-input 字段必填、重复/未知/类型/enum/BYTES 检查，constant/computed 只读；
- successful TX frame 与字段物理映射、integrity storage 一次性复制后发布；
- 每次执行先清 pending；Encode/回调/复制失败不得保留上次成功输出；
- stream 输入冻结、cursor/未消费 suffix、`can_continue`、无进展和 reset-required 约束；
- binding/flow 独立状态和原子 publication；
- 预检上界、实际 capacity、替换峰值与失败注入测试。

ASCII public A1/A2/stream 还提供了 public-only owner、Host callback 同步复制、candidate/业务输出分离、
step observation、Continue/Reset UI 映射的现成模式。Binary 应复用模式，不复用 ASCII 协议语义。

### 5.2 不能复用的实现

旧 Binary materializer/PreparedBinary 直接持有 `CompiledUiArtifacts`、private Plan、private Host Session、
`protocol_core` 值类型，并链接 `pae_config_compiler`/private host target。把这些源文件加入 standalone
白名单会破坏已验证的 installed-SDK public-only 边界。

因此允许“参考并迁移语义”，不允许：

- 在 public adapter 中 include `src/**`；
- 从 UI 读取 private Plan 解析 constant/computed/integrity/conversion；
- 同一操作重复调用 Encode/Decode 只为生成展示字段；
- 由 UI 自行实现 framing、integrity 或 conversion；
- 复制旧 Session 作为临时第二执行引擎。

## 6. 推荐首片：Binary complete-record Encode

### 6.1 首片用户闭环

用户选择具备 Encode execution 的 Binary complete-record Pipeline/Message，输入所有 caller-input 字段，
点击 Encode 后得到：

1. PAE public Host 的明确状态与 `CodecStatus`；
2. 仅在 `HostStatus::OK` 时发布的 encoded frame；
3. caller typed inputs 的 logical 表示，Enum 同时显示选项 id/display/raw；
4. 输出帧中每个字段的实际 byte range/bit masks，BYTES 长度与 resolved range 一致；
5. computed length/integrity storage 的实际位置和 Hex 高亮；
6. constant/computed 字段为只读并说明由 PAE 生成；
7. type、missing、duplicate、enum、representability、conversion、buffer/callback/预算失败的定位；
8. 任何执行失败、回调复制失败或输入变更立即清除旧成功输出。

“integrity 展示”在首片表示 storage/coverage 配置、输出位置和 PAE Encode 成功事实，不宣称 GUI 做了
第二套 integrity 复算。“conversion 展示”在首片表示 logical Decimal64 输入和 PAE 失败原因；成功时
不伪造未公开的 raw conversion 值。

### 6.2 建议修改文件范围

以下是未来实施建议，不是本轮修改授权：

1. `tools/protocol_lab_binary/public_binary_decode.{h,cpp}`：最小扩展为支持 DECODE/ENCODE binding 的
   public owner，增加 public typed input、Encode operation/result、按 binding+flow 定位和失败清空。
   为控制首片，不在这一轮同时接 stream；文件命名与 namespace 的泛化可在功能稳定后单独整理，
   不为命名先做大搬迁。
2. `tools/protocol_lab_ui/owned_presentation_types.h`：为 Pipeline 增加 encode association；若需要，增加
   中立的 Binary encode result/failure DTO，保持 Qt 与 PAE public 类型隔离。
3. `tools/protocol_lab_ui/public_binary_description.cpp`：按
   `PipelineMessageExecution` 填充 Decode/Encode availability，保留 Message/Field/Enum/physical 一致性
   门禁。
4. `tools/protocol_lab_ui/binary_host_adapter_public.{h,cpp}`：把 UI typed drafts 映射为 public
   `EncodeValue`，同步复制 Host output bytes、实际物理映射和失败事实，执行预算/替换预算/失败注入。
5. `tools/protocol_lab_ui/document_session.{h,cpp}`：Binary Host route 的 `EncodeAvailable`、typed draft、
   `Encode` publication、Flow state 和旧结果清空；禁止 fallback 到 private bridge。
6. `tools/protocol_lab_ui/document_tab.cpp`：按当前 action/input kind 显示 Encode，不再按“Binary 文档”
   一刀切隐藏；复用字段 editor、Hex view、details，不新增 API 按钮墙。
7. `tools/protocol_lab_binary/CMakeLists.txt`、`tools/protocol_lab_ui/CMakeLists.txt`、standalone CMake/输入
   manifest：只加入上述 public Lab 源，继续只链接 `PAE::pae`，不得把 private target 带回。
8. `tests/protocol_lab_binary/public_binary_decode_tests.cpp` 或新增同目录 encode 专项，及
   `tests/protocol_lab_ui/binary_public_h2_tests.cpp`：补 non-Qt 和 headless Qt 两层定向验证。

### 6.3 前置依赖与停报条件

- public metadata 必须能唯一确定 Pipeline/Message association、encode availability、Field selector 和
  Enum selector；不允许字符串猜测。
- public `HostEndpoint::Encode` 必须是唯一执行路径；一次 UI 操作只调用一次 Encode。
- output buffer 上界取公开 Message physical record bounds/adapter 限额，并纳入预检；不得无界扩容。
- 若产品验收要求成功 Encode 后展示 constant/computed/converted 字段的实际 logical/raw 值，则当前
  public Encode 结果不足。此时只交付上述受限且诚实的首片，另报 encode trace API 候选，不读取
  private Plan、不重复 Decode。
- 若发现 Schema 0.9 某合法 encode-only Message 无法由当前 physical/metadata public query 描述，先报
  具体 fixture 与缺失字段，不扩大 Compiler/Core 契约。

## 7. 第二片：Binary stream Push/Continue/Reset

### 7.1 所需 public Lab 状态

在 complete Encode 首片复核后，public Binary owner 才增加 stream binding：

- 由 `QueryPipelineFramingDescription` 判定 `STREAM_CHUNK`、strategy 和 maximum candidate bytes；
- 每个 Decode stream Flow 保存独立 handle、draft、frozen input、cursor、step sequence、累计计数和
  当前 candidate/business result；
- `Push` 只提交尚未消费 suffix，严格核对 `bytes_consumed`；
- `Continue` 只在 frozen suffix 或 `has_internal_work` 时启用；
- `Reset` 成功后重新 `Find` handle，清空该 Flow 的 suffix、candidate、业务结果和错误；
- reset-required、callback copy failure、无进展契约违例都 fail closed。

### 7.2 GUI 必须分开的事实

| GUI 事实 | public 依据 | 不得误称 |
| --- | --- | --- |
| 本步形成 candidate | `candidates`/observer callback 与 candidate view | 不等于 Decode 成功 |
| candidate Decode 结果 | `decode_status`、matched/failed field/conversion | 不等于业务 sink 完成 |
| 业务输出 | `business_callbacks_returned`/output sink | 不等于输入全部消费 |
| 暂停 | `SINK_STOP`、work budget 或 delivery pending | 不等于失败，也不等于 suffix 已丢弃 |
| 可继续 | 未消费 frozen suffix 或 `has_internal_work`，且非 reset-required | 不等于允许提交新 chunk |
| 本次输入消耗 | `bytes_consumed` | 不等于累计输入或 candidate 长度 |
| 丢弃/畸形 | discarded/malformed/last issue | 不等于 Codec 业务失败 |

Binary stream UI 可复用当前 ASCII 的 Submit/Continue/Reset 控件和 step summary，但 candidate 的
Binary physical/result 映射必须走 Binary public description；不得把 ASCII CRLF 特有语义套到 fixed/
sync/length framing。

## 8. Compiler、metadata 与资源的后续图形入口

这部分建议排在 Binary Encode/stream 主工作流之后，避免先做“API 浏览器”：

1. **结构化编译诊断：**在 `CompileCompletion` 到 Session 的发布链保留完整 public diagnostic，界面
   展示 stage/code、JSON pointer/byte offset、detail；仅资源失败再展示 kind、required、limit/profile。
2. **只读协议摘要：**在现有 selectors/details 上补 Pipeline input kind/framing、Message Decode/Encode
   availability、Field encode source 和长度边界，不新增逐 API 按钮。
3. **资源摘要：**显示 compiler metadata、active codec/framer/host 的 accounted bytes 与 effective limit；
   明确它不是 RSS。只在诊断/详情面板展示，继续由 adapter 做硬 admission。
4. **内部/CLI/Evidence 边界：**SDK manifest/hash/provenance、模块来源、负向包门禁、完整 memory 明细和
   自动矩阵属于构建/证据入口；不自动转成最终用户 GUI 功能。CLI/测试 fixture 的批处理能力也不因
   存在就要求添加按钮。

## 9. 自动验证与最多人工观察

### 9.1 Binary complete Encode 自动验证

non-Qt public adapter Debug/Release 专项至少覆盖：

- UINT64/INT64/BOOL/BYTES/ENUM/DECIMAL64 typed input；
- encode-only、decode-only、mixed Pipeline association；
- missing/duplicate/unknown/type mismatch/unknown enum/BYTES length/conversion representability；
- fixed 与 bounded payload 输出，computed length 和 integrity storage resolved range；
- constant/computed override 被拒绝且保持只读；
- 成功结果身份、message/field mapping、bit masks/range 与输出 frame 一致；
- codec failure、callback copy failure、预算/分配失败清除旧成功结果；
- binding/Flow draft/result 隔离、stale publication 拒绝和 replacement budget；
- Release 专项断言继续用 `/UNDEBUG`，不得被 `NDEBUG` 关闭。

Qt headless D/R 定向覆盖：

- Binary complete Encode 可见/启用条件与 Decode/stream route 不串路；
- 六类 editor、constant/computed read-only、invalid draft 生命周期；
- Encode 成功展示 Hex/物理高亮/length/integrity/conversion 说明；
- 失败后 Hex、field results、旧 preview 同步清空并定位失败字段；
- Flow 切换保留各自 draft/result，Reload/Apply 的 stale result 不发布；
- standalone product target 无 private source/target fallback。

验证顺序建议为仓库内定向 non-Qt D/R → Qt headless D/R → 新 standalone static D/R → shared D/R
运行来源。不得覆盖现有 `deliverables/lab/98df5e0`；新部署使用新的检查点命名目录，例如
`deliverables/lab/<new-head>-gui-capability/`，具体路径在实施授权时冻结。

### 9.2 首片最多两组简短人工观察

自动验证和总控复核通过后，仅建议：

1. 一组 typed Encode：修改普通/Enum/Decimal64/BYTES 输入，确认输出 Hex、动态长度、computed
   length/integrity 高亮与只读字段说明可理解；
2. 一组失败与 Flow：先成功，再制造 representability/缺失输入失败，确认旧输出清空；切换 Flow
   确认 draft/result 隔离。

Binary stream 另片完成后再最多增加两组：跨 chunk/多 candidate 的 Push→Continue，以及 malformed/
STOP→Reset。不得把两片人工观察提前合并成复杂验收脚本。

## 10. 当前未验证、风险与建议顺序

- 本轮未构建、未测试、未启动 GUI；所有结论是当前源码/CMake/API 的静态事实和基于既有定向证据的
  实施规划。
- public Encode 缺少 per-field encode trace 是“丰富成功展示”的真实限制，不阻塞 typed input + output
  frame + physical mapping 的最小闭环。
- shared SDK 仍有既有 C4251/stable ABI 边界；Qt 正式许可、Linux、异工具链、硬件、现场和正式发布
  均不在本计划验证范围。
- 历史访问违例/焦点观察不能因新增能力测试通过而自动关闭。

推荐串行顺序：

1. 总控确认 Binary complete Encode 的受限成功展示契约；
2. 实施并复核 non-Qt public owner + Qt headless/UI 首片；
3. 最多两组人工观察并收口；
4. 再实施 Binary stream；
5. 最后补结构化 compiler/resource 诊断面板；
6. 若总控确认确需 encode-side 全字段 actual logical/raw，再单独评审 public encode trace 契约。

## 11. Git 与停止状态

本轮只新增本报告。未修改源码、CMake、脚本、依赖、SDK、Qt 或部署；未构建、测试或启动 Lab；
未 Stage、Commit、Push、发布或删除。达到只读盘点停点后停止写入，等待总控复核。
