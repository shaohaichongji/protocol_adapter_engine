# Lab Binary stream G2 公开消费预检

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 1. 结论与本轮边界

本报告建议 G2 首片以 **Schema 0.9 + Binary `STREAM_CHUNK`** 为边界，一次覆盖当前公开
`synthetic_stream_framing_slice.pae.json` 中的三种 strategy：`FIXED_LENGTH`、
`SYNC_FIXED_LENGTH`、`SYNC_LENGTH_FIELD`。三者均应由公开
`QueryPipelineFramingDescription` 与 `HostEndpoint::Push/Continue/Reset` 驱动，Lab 不解释具体切帧算法，
因此只先支持其中一种 strategy 反而会增加一层没有契约依据的产品策略。

首片可落地，但当前代码还没有可直接接线的公开 Binary stream consumer：

- 当前公开 Binary owner 只支持 complete Decode/Encode，并显式拒绝 Decode stream binding；
- Binary UI description 没有查询公开 framing description，现有 `PipelineDescriptor` 只有
  `stream_ascii_crlf`，不能表达三种 Binary strategy；
- Session/DocumentTab 虽已有 Push/Continue/Reset 控件与状态区，但接口、状态类型和编译门禁仍以
  ASCII stream 命名和路由；
- 现有历史 private Binary stream owner/测试只能作为行为参照，不能回接 private owner，也不能当作
  当前 public owner 已通过的证据。

这不是公共 PAE 接口缺口。当前公开查询、Host stream 操作、观察与回调事实足以实现 G2；缺口位于
Lab 自有 owner、展示 DTO、Session/UI 路由和专项验证。建议实施时不修改 PAE Core、Schema、公共 API、
Host 或 SDK。

本轮仅静态阅读并新增本报告；未修改代码/CMake/测试，未构建、未运行测试或 Lab，未重打 SDK，
也未执行 Stage/Commit/Push/发布/删除。

## 2. 已确认的当前源码事实

### 2.1 Binary public owner 与 G1 保持完整，但尚无 stream 路径

- [`public_binary_decode.h`](../../../tools/protocol_lab_binary/public_binary_decode.h#L100) 的
  `Operation` 只保存一次 complete Host 结果以及 Decode candidate 或 Encode bytes；
  [`FlowState`](../../../tools/protocol_lab_binary/public_binary_decode.h#L107) 只保存 draft、当前 operation
  与一个 `HostChannelHandle`。
- [`public_binary_decode.cpp`](../../../tools/protocol_lab_binary/public_binary_decode.cpp#L425) 在绑定 Decode
  channel 时对 `Observe(handle).stream == true` 直接返回失败；没有 Push、Continue、stream observation、
  冻结输入 cursor 或 stream 计数。
- complete Decode 仍通过 `HostEndpoint::Decode` 一次执行；成功 business output 在 output callback 中物化，
  失败 candidate 在 candidate observer 中物化，没有第二次 Decode。G2 必须保留这一 G1 行为。
- complete Encode 已由同一个 compiled owner 和 Host 执行。G2 只扩展 Decode stream binding，不改变
  G1 Encode binding、输入校验、review 或展示。
- Reset 已遵循 public Host 句柄失效契约：Reset 后重新 `Find`。G2 stream Reset 必须复用这一原则，
  不能继续使用旧 handle。

### 2.2 UI 已有外壳，但公开 Binary 描述和 Session 尚未连接

- [`owned_presentation_types.h`](../../../tools/protocol_lab_ui/owned_presentation_types.h#L125) 的
  `PipelineDescriptor` 仅有 `stream_ascii_crlf` 与 maximum frame length，没有 protocol-neutral input kind、
  framing strategy 和 maximum candidate bytes。
- `public_binary_description.cpp` 当前映射字段、消息和 Pipeline execution 关联，但未调用
  `QueryPipelineFramingDescription`。因此 Binary complete 与 stream 当前无法在准备阶段可靠分流。
- [`binary_host_adapter_public.h`](../../../tools/protocol_lab_ui/binary_host_adapter_public.h#L123) 只暴露
  `DecodeComplete`/`EncodeComplete` 及 complete result mapping，没有 stream DTO 和操作入口。
- [`document_session.cpp`](../../../tools/protocol_lab_ui/document_session.cpp#L2302) 已有
  `SubmitStream`/`ContinueStream`/`ResetStream`，但当前只路由 ASCII public/private backend；
  `document_session.h` 中相应公开面和 `AsciiStreamObservation` 也受
  `PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER` 门禁控制。
- [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L4048) 已有共同的 Submit、Continue、
  Reset 控件和状态刷新入口。Binary G2 应复用该 UI 骨架，但 Binary 输入保持 Hex，不引入 ASCII escaped
  语义。
- `schema_dispatch.cpp` 已将 Schema 0.9 路由到 `BINARY_PUBLIC`，`compile_worker` 也只形成一个
  `public_compiled` owner；G2 不需要新增编译分支或重复编译。

### 2.3 公开能力和既有证据边界

- `QueryPipelineFramingDescription` 可给出 input kind、strategy 和 maximum candidate bytes；公开
  Host observation 可给出 phase、buffered bytes、internal work、累计 candidate/Decode/business 计数及
  reset-required；Push/Continue 的结果可给出本次 consumed、framing、candidate 与 Decode 事实。
- `tests/public_api/public_stream_framer_tests.cpp` 已有三种 Binary strategy、精确 suffix 和 work budget
  的公开 API 测试；`tests/public_api/public_host_endpoint_tests.cpp` 已有 Push/Continue、STOP、单次
  candidate/Decode、success/failure/business 分离、callback fault/Reset 等测试。它们是现有源码证据，
  本轮没有重跑。
- `tests/protocol_lab_binary/stream_tests.cpp` 与 `lab-binary-stream-validation.md` 是历史 private owner
  证据，不代表 G2 public owner 或 Qt 接线已经验证。

## 3. 建议的最小公开消费架构

### 3.1 owner 与路由

继续由 `public_binary_decode::Adapter` 独占以下资源：

1. 一个 `pae::CompiledProtocol`；
2. 一个 `pae::HostEndpoint`；
3. 每个 `(binding, Flow)` 的唯一 Host handle 与运行状态；
4. 每个 stream Flow 的唯一冻结输入与 cursor；
5. 当前一步 candidate/result/failure 的唯一物化结果。

准备阶段按每个 binding 的公开 framing description 分流：

- `COMPLETE_RECORD`：沿用 G1 `DecodeComplete`/`EncodeComplete`；
- `STREAM_CHUNK` 且 strategy 为三种 Binary strategy 之一：开放 G2 Submit/Continue/Reset；
- `ASCII_CRLF`：仍由既有 ASCII 路径负责；
- 查询失败、strategy 与 observation 不一致或 maximum candidate bytes 超出 Lab 配额：准备失败，
  不 fallback 到 private owner，也不尝试 complete Decode。

### 3.2 每 Flow 状态与状态转移

建议在现有 `FlowState` 中增加独立 stream sidecar，而不是建立第二个 adapter/Host：

- public framing description 与 `M = maximum_candidate_frame_bytes`；
- `frozen_input`、`cursor`、`C = 本次允许提交的最大 chunk`；
- latest Host observation、latest step facts、累计计数；
- current materialized candidate/result/failure；
- `faulted/reset_required/no_progress`；
- 现有 UI draft 继续是可编辑文本，不把它当作冻结执行输入。

操作契约：

1. **Push**：只有当前冻结后缀为空、Host 无 internal work 且不要求 Reset 时才接受新 Hex 输入；解析后
   冻结一次，调用一次 `HostEndpoint::Push`。若本步 STOP 或 budget pause，只推进 public result 报告的
   consumed 数量，未消费 suffix 保留在 owner 内。
2. **Continue（总控纠正）**：不重新读取编辑器、不重建 input。若 owner 仍有冻结后缀，使用其
   `frozen_input[cursor..end)` 调用一次 `HostEndpoint::Push`，仅按本步 consumed 推进 cursor；
   后缀为空且 Host 有 internal work 时，才调用一次 `HostEndpoint::Continue`。公开 Continue 是
   空输入推进，不会替 Lab 保存或消费外部后缀。每次点击最多一次 Host step，不自动循环。
3. **Reset**：仅清目标 `(binding, Flow)` 的冻结 suffix、cursor、计数、candidate/result/failure 和
   fault；调用 Host Reset 后重新 `Find` handle。其他 Flow、其他 Tab、G1 complete operation 不受影响。
4. **新一步发布**：进入 Push/Continue 前先清除旧 candidate/result/highlight；随后原子发布本步事实。
   candidate-empty 不是成功复用旧结果，Decode failure 也不能残留上一帧字段。
5. **异常**：callback copy/budget/contract/no-progress/handle 错误都 fail closed 并要求 Reset；保留 Host
   已确认 consumed 的 cursor，不回滚为“看似未消费”。

### 3.3 单次执行与单次物化

每次 Push/Continue 最多形成一个当前 candidate，Host 内部对该 candidate 最多 Decode 一次。Lab 不调用
第二次 complete Decode 来生成展示：

- failed candidate 由 candidate observer 物化一次；
- successful candidate 的业务字段由 output callback 物化一次；candidate observer 只记录发生/STOP
  决策与计数，不再复制成功字段；
- `HostOperationResult`、observation 和 callback 计数交叉校验“一 candidate / 一 Decode / 至多一次
  business output”，不以 UI 显示结果反推执行次数。

这样保持 G1 已有的 success/output、failure/observer 所有权模型，同时支持 candidate、Decode、business
三类事实分开显示。

## 4. 展示、预算与隔离要求

### 4.1 左右工作台最小接线

- 左侧继续使用 Binary Hex 编辑器和既有 binding/Flow 选择；Submit 冻结当次解析结果，Continue 不再读
  编辑器。
- 右侧复用当前 stream 状态区，至少分开展示：phase、strategy、本步 consumed、冻结 suffix、buffered、
  internal work、candidate count、Decode success/failure、business delivered、discard/malformed、
  reset-required。
- candidate success 映射到既有 Binary `InspectResult`/字段表；candidate failure 显示诊断并清空旧字段；
  candidate-empty 只更新状态，不显示上一帧。
- `Flow` 切换只发布对应 Flow 的 draft、suffix、计数和当前结果；Tab 间 owner/session/widget 状态完全独立。

### 4.2 配额口径

继续区分三个维度，不把它们折算为 RSS：

- `C`：单次冻结 submit chunk 上限；
- `M`：公开 framing query 给出的最大 candidate bytes；
- work budget：一次 Host step 的内部工作额度。

准备阶段应拒绝 `M` 超出 Lab frame/result materialization 配额。运行阶段至少显式计入：

- 每个 stream Flow 的 `frozen_input.capacity()`；
- 当前 candidate/result 的 materialization capacity；
- replacement 时旧/新发布结果短时共存；
- Hex 解析临时 vector 与 owner 冻结 copy 的调用期峰值。

ASCII public owner 的 state machine、suffix/cursor 和 fault/reset 结构可作为实现模式参考，但不能复制
ASCII framing 判定、文本 DTO、escaped 输入语义或私有执行代码。新 DTO 应表达 protocol-neutral observation
与 Binary candidate，而不是把 Binary 数据塞进 `AsciiStreamObservation/AsciiStreamStepResult`。

## 5. G1 Encode、ASCII stream 与编译门禁兼容性

- G1 Encode：complete Encode binding、typed draft、review 和展示路径不变；stream 只对 Decode binding 开放。
- G1 complete Decode：`COMPLETE_RECORD` 路径不变，不能因增加 stream route 而先走 Push 再 fallback。
- ASCII stream：继续使用其现有 public owner/兼容隔离，G2 不修改 ASCII framing 或输入解释。
- 当前公共 stream UI API 被 `PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER` 包围。建议在 Lab UI 局部引入
  protocol-neutral 的 stream UI 编译门禁，其值由“ASCII public stream 或 Binary public H2/G2”导出；
  不建议新增根级 G2 产品开关，也不建议让 Binary 功能语义依赖一个 ASCII 命名宏。
- 如果首片构建矩阵始终同时开启 ASCII stream，可暂时复用现有控件实现，但代码中的 Binary 路由仍应
  依赖 protocol-neutral local gate。是否同时支持“关闭 ASCII、只开 Binary G2”的构建矩阵，需在实施
  授权前由总控确认；这不构成 PAE API 缺口。

## 6. 建议实施切片与精确文件范围

### G2-A：非 Qt public owner，先行门禁

建议修改：

- `tools/protocol_lab_binary/public_binary_decode.h`
- `tools/protocol_lab_binary/public_binary_decode.cpp`
- `tests/protocol_lab_binary/CMakeLists.txt`
- 新增 `tests/protocol_lab_binary/public_binary_stream_tests.cpp`（避免把 public G2 混入历史 private
  `stream_tests.cpp`）

目标：三种 strategy、Push/Continue/Reset、STOP suffix、work budget/internal pending、单 candidate/
单 Decode、copy fault/reset、no-progress、Flow 隔离和配额峰值全部在非 Qt 层闭合。该片通过后才进入 UI。

### G2-B：公开描述与 Lab 自有 adapter/DTO

建议修改：

- `tools/protocol_lab_ui/owned_presentation_types.h`
- `tools/protocol_lab_ui/public_binary_description.cpp`
- `tools/protocol_lab_ui/binary_host_adapter_public.h`
- `tools/protocol_lab_ui/binary_host_adapter_public.cpp`
- `tests/protocol_lab_ui/binary_public_h2_tests.cpp`

目标：映射公开 input kind/strategy/M，形成 Binary stream step/view，并在 adapter 层完成 candidate/result/
failure 的原子替换。建议把 protocol-neutral observation DTO 放在 Lab-owned presentation 层，Binary step
保持 Binary 类型；不要重命名或重构整个 ASCII DTO 体系。

### G2-C：Session/UI 与 fixture 部署

建议修改：

- `tools/protocol_lab_ui/document_session.h`
- `tools/protocol_lab_ui/document_session.cpp`
- `tools/protocol_lab_ui/document_tab.h`
- `tools/protocol_lab_ui/document_tab.cpp`
- `tools/protocol_lab_ui/CMakeLists.txt`
- `tests/protocol_lab_ui/CMakeLists.txt`
- `tests/protocol_lab_ui/binary_public_h2_tests.cpp`，以及一个定向 Binary stream Qt smoke 测试文件
- 如选择直接部署现有 canonical fixture，才修改 `cmake/PaeQt513.cmake` 的 fixture whitelist

不建议修改 `schema_dispatch.cpp`、`compile_worker.*`、根 `CMakeLists.txt`、PAE public header/Core/Host。
当前 canonical fixture 是 `examples/config/synthetic_stream_framing_slice.pae.json`；产品构建尚未部署它，
且现有 Qt fixture whitelist 未包含它。优先复用该 canonical fixture并显式扩 whitelist，避免复制第二份
配置；共享 helper 的这项改动必须在后续实施授权中明确列入。

## 7. 针对性验证建议

### 7.1 非 Qt D/R（G2-A 必须先通过）

每个 Debug/Release 均至少覆盖：

1. 三种 strategy 各自的 split、glued、malformed/discard 与恢复；
2. STOP 后精确保留未消费 suffix，Continue 只取得恰好下一个 candidate；
3. work budget 暂停与 internal pending；
4. success/failure/business 计数恒等式与一次 Decode；
5. callback copy fault、allocation/budget rejection、no-progress 后 Reset；
6. Reset 重新 Find handle，且只清选中 Flow；
7. 两个 Flow 的 draft/suffix/counters/result 隔离；
8. G1 complete Decode/Encode 回归。

Release 专项必须继续显式解除 `NDEBUG` 对断言的影响，不能用“进程退出 0”替代断言生效证据。

### 7.2 Qt/session D/R（G2-B/C）

至少覆盖：

- Schema 0.9 三个 stream pipeline 被正确识别，complete binding 仍走 G1；
- Hex Submit、重复 Submit 拒绝、Continue 不读取修改后的 editor；
- candidate-empty、failure、success 均不残留上一结果/highlight；
- binding/Flow 切换发布正确 draft/suffix/count/result；
- Reset 只影响目标 Flow，两个 Tab 完全隔离；
- G1 Encode 和 ASCII stream 定向回归；
- fixture 从全新部署目录实际可用。

不建议跑全仓，也不建议把历史 private Binary stream 测试作为 G2 通过条件。

### 7.3 最多一次简短人工体验

自动 D/R 与部署来源复核通过后，仅需一次简短体验：

1. `FIXED_LENGTH` split + Continue/下一 candidate；
2. `SYNC_LENGTH_FIELD` malformed/discard + Reset；
3. 在两个 Flow 或两个 Tab 间切换，确认 draft/suffix/result 无串写，同时快速确认 G1 Encode 仍可用。

人工体验只证明本次 UI 交互观察，不替代 D/R、SDK 来源或现场验收。

## 8. 实施前需由总控确认的两项局部决定

1. **构建矩阵**：首片是否要求支持“ASCII stream OFF、Binary G2 ON”。建议支持，并在 Lab UI 局部使用
   protocol-neutral gate；若本轮仅验证现有产品矩阵，也应把单独矩阵列为未验证，而不是把 Binary 语义
   永久绑在 ASCII 宏上。
2. **fixture 部署**：建议直接复用 canonical
   `examples/config/synthetic_stream_framing_slice.pae.json` 并扩 Qt deployment whitelist；若不允许改共享
   helper，再由总控决定是否增加测试专用副本。不要在实施任务中自行复制并造成配置双源。

除上述两项外，没有发现需要扩大公共契约或修改 PAE 的阻塞项。达到 G2-A 非 Qt 门禁后再串行进入
G2-B/C，可避免 UI 与 owner 同时变化而掩盖单次执行、suffix ownership 或预算问题。

## 9. 本轮证据状态

- 基线：`main@04fa429e1e8361b89f8622e2dc998d8869309c1c`。
- 本轮开始时共享工作树已有总控修改：
  `docs/engineering/pae-execution-delivery-organization-plan.md`；本任务未触碰。
- 已读：public Binary owner/description/adapter、DocumentSession/DocumentTab、公开 stream/Host 接口、
  相关 CMake/fixture、G1/ASCII/public API/历史 private Binary stream 测试与验证文档。
- 本轮未运行构建、测试或 Lab；文中“已有测试”均指仓库既有源码/历史记录，不是本轮复跑结果。
- 本轮唯一写入是本报告；无 Stage/Commit/Push/发布/删除。
