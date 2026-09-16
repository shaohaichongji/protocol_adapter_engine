# Lab 对公开 Host 阶段 2B 的后置消费复核

日期：2026-09-14。状态：selector 修正后针对性复核已完成，待总控复核；本报告仅做消费侧静态复核，未迁移 Lab，未运行构建或测试，未获 Stage、Commit、Push 授权。

## 0. Selector 修正后的当前结论

首次复核确认的“Encode Message 固定在 binding”阻断已经关闭。最终公开接口恢复为 Encode binding 只固定 Pipeline，同一 `HostChannelHandle` 在每次 `Encode` 调用期传入 `message_index`；创建期保存该 Pipeline 的有界可 Encode Message selector 集合，并按全部可 Encode Message 的最大可靠输出上界预分配单一 channel buffer且纳入 Host 聚合计费。修正后的契约、头文件、实现、专项断言和 external consumer 相互一致。

针对性核对未发现该修正范围内的剩余确定阻断：同一 Handle 多 Message、越界/Decode-only/跨 Pipeline selector 在 Codec 前拒绝、最大 buffer 复用、callback fault 后 `RESET_REQUIRED`、Reset/重新 Find 后改选 Message均已覆盖；首次报告提出的 Reset 后 Codec view 直接失效和 callback 失败时 `bytes_produced` 的解释也已补齐。可以从消费侧解除本报告对阶段 2B 收口及 README 同步的阻断，但这仍需总控结合 PAE 交付完成最终限定复核，不表示 Lab 已迁移或生产验收完成。

本轮没有重新审查未变更的 Host 全部能力；下文第 1～6 节保留首次复核的阻断事实、依据和修正建议，作为修正前历史。当前状态以本节和第 7 节为准。

## 1. 首次复核结论（修正前历史）

首次复核时的 public `HostEndpoint` 在 Handle scope/generation、Decode/Framer 组合、候选与成功输出分流、callback 异常隔离、Reset、资源计费和重入并发方面基本保持了已确认的 Host 消费语义；但当时 Encode binding 把 `message_index` 固定在创建期，同时 `Encode()` 不接收 Message selector，而唯一键仍是 `endpoint_key + action`。这在修正前构成阶段 2B 的**确定消费阻断项**；该历史实现不应按既有目标最终收口，也不应据此启动 Lab 迁移或 README 接口定稿。

阻断原因不是 Lab 偏好的 UI 形态，而是合法协议能力被缩小：Pipeline 本来可以关联多个可 Encode Message，endpoint 的 Encode binding 绑定 Pipeline，具体 Message 由一次 Encode 调用选择。当时接口既无法用同一 Handle选择第二个 Message，也因同一 endpoint+action 重复 binding 被拒绝而不能为每个 Message建立同 endpoint 的替代 binding。

除该项外，未发现会导致 Handle 串用、失败候选进入业务、callback 异常后错误重放、Reset 污染其他流、借用越界或预算漏记的第二个确定阻断项。此结论不表示 2B 修正、Lab/Qt 迁移、SDK、Linux、网络、硬件或现场验证完成。

## 2. 现场与证据边界

- 仓库基线：`main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`；接管时暂存区为空，完整保留共享工作树既有修改。
- 复核了 [阶段 2B 契约](pae-public-host-stage2b-contract.md)、[阶段 2B 验证报告](pae-public-host-stage2b-validation.md)、[原 Framer/Host 设计](pae-public-framer-host-slice-design.md)、[原 Lab 消费者报告](lab-public-framer-host-consumer-review.md)、最终 `include/pae/host_endpoint.h`、`src/public_api/host_endpoint.cpp`、Codec 增量、public Host 测试/示例及真实 Lab Host/Encode 路径。
- 首次复核只读取了当时既有 Debug/Release Host 专项各 `52/52`、定向 CTest 各 `10/10` 和 external consumer PASS 日志，没有重新执行构建或测试。这些是 PAE 交付证据，不是该轮新增验证。
- 当前 Binary UI Stage 1 只开放 Decode binding，但非 Qt `PreparedBinary` 已支持 Encode 调用期 `message_id`；现有 ASCII Host UI 也把当前 `SelectionKey` 的 Pipeline/Message identity 传入每次 Encode。因此不能用“当前某个 Binary UI 页面暂未开放 Encode”消除通用 Host 与后续 Lab 的多 Message需求。

## 3. 修正前确定阻断：Encode Message 被固化到 binding

### 3.1 修正前接口实际能力

- `HostBindingSpec` 同时保存 `pipeline_index` 和 `message_index`；创建 Encode binding 时强制该 Message 属于 Pipeline 且 `encode_available`。
- `HostEndpoint::Encode(handle, values, count, sink)` 没有 Message selector，直接使用 binding 中固定的 `message_index`。
- binding 唯一键是 `endpoint_key + action`；同一 endpoint 不能建立第二个 Encode binding。`Find()` 也只按 endpoint/action 找一个 Encode channel。

这意味着一个 endpoint 的一个 Encode channel 永久只能发送一个 Message。换 endpoint 字符串不是等价绕行：endpoint 是调用方业务身份，不应为引擎内部 Message 选择而伪造；同时这会拆成多个 Codec、buffer、fault/generation 通道，改变原有生命周期和资源语义。

### 3.2 真实反例

1. `examples/config/synthetic_ascii_stream_slice.pae.json` 的 `ascii_pipeline` 同时关联 `greeting`、`decode_only`、`encode_only`；其中 `greeting` 和 `encode_only` 都支持 Encode。现有 public Host 测试只把该 Pipeline 固定到 `greeting`，没有尝试用同一 Encode handle 发送 `encode_only`。
2. `examples/config/synthetic_length_slice.pae.json` 的 `synthetic_rx` 同时关联三个具有 Encode source/computed 规则的 Message。public Codec 可以按调用期 Message index 编码，当前 Host 只能在创建时任选其一。
3. 现有私有 `host_endpoint::Session::Encode` 接收 `message`，`PreparedBinary::Encode` 接收 `message_id` 并在 bound Pipeline 的 Message 集合内解析；ASCII `HostObserverAdapter::Encode` 同样从当前执行 identity 取得 Message 后传入 Session。新 public Host 无法保持该调用序列。

因此，现有 `52/52` 和 external consumer 的单 Message Encode 成功，只证明“固定一个 Message 的 Host”可用，不能证明阶段 2B 原定的 Pipeline-bound 多 Message Encode 消费能力。

### 3.3 推荐最小修正方向

保持 `endpoint+action -> Pipeline/channel` 的不可变 binding 与单一 fault/generation 身份，在 `HostEndpoint::Encode` 调用期增加 public `message_index` selector：

1. Encode binding 创建时验证 Pipeline 至少存在一个可 Encode Message，不固定具体 Message。
2. 创建期把该 Pipeline 所有可 Encode Message 的公开输出上界取最大值，按最大值预分配该 Encode channel buffer并计入聚合预算。
3. 每次 Encode 在取得 Host guard、解析 Handle、校验 action/fault 后，验证 Message 确属该 Pipeline且 `encode_available`，再调用同一 Codec 一次。
4. 增加同一 Handle 依次编码 `greeting` 和 `encode_only`、跨 Pipeline Message拒绝、Decode-only Message拒绝、不同输出上界复用同一预配 buffer、callback fault/Reset 后重新选择 Message 的 Debug/Release 断言。

把 Message 纳入 binding 唯一键并为每个 Message建独立 channel 是另一种新契约，但会改变 endpoint 唯一性、Find 形状、fault/generation 隔离和资源成本，不是对当前目标的最小修正。

## 4. 其余分项复核

| 项目 | 结论 | 消费侧证据与限制 |
| --- | --- | --- |
| scope / Handle | 通过 | Handle 用 weak scope、channel index、generation；default、expired、foreign、stale 分开失败。旧 Host 地址复用不能让旧 Handle 命中新 scope。 |
| Reset / generation | 通过 | 先检查 generation 上限；stream Reset 成功后推进 Codec epoch，再递增 generation、清 fault；失败不伪造新代。旧 Handle stale，需重新 Find。 |
| Decode 与精确 consumed | 通过 | COMPLETE_RECORD 一旦进入 Codec 即消费完整候选；stream 直接保留一次 Framer 的 `bytes_consumed`。observer/business 异常通过 Framer STOP 提交当前候选，不重放未消费后缀。 |
| 候选、成功与计数 | 通过 | 每候选一次 Decode；observer 先行且可见成功/失败，失败 record 为空且不进 business；候选、attempt、success/failure、两个 callback 正常返回数分列。 |
| callback 异常隔离 | 通过 | observer 异常抑制当前 business；business 异常不重试。只标记目标 channel `RESET_REQUIRED`，其他 stream/Encode channel 不受影响。 |
| Output / view 生命周期 | 通过 | Decode raw frame、record/field/BYTES 和 Encode bytes 都只在同步 callback 内借用；测试在 callback 内复制 raw、typed BYTES、Encode bytes。Reset 显式推进 Codec view epoch。 |
| 直接/间接重入与 BUSY | 通过 | Host 使用 thread-local 活动 callback 链，直接重入和 `A -> B -> A` 均为 `REENTRANT_CALL`；跨线程竞争由 Host guard 返回 `WORKSPACE_BUSY`。 |
| compiled owner / 私有边界 | 通过 | Host及子 Codec/Framer 保活同一冻结状态；公开头只依赖 `pae/**`，没有 Qt、Lab、Plan/Core/内部 Host 类型泄漏。 |
| 聚合预算 / 创建原子性 | 通过 | facade、scope、binding/identity/channel、Codec、Framer、Encode buffer 分列；共享 compiled-state facade 只列一次。exact/-1、逐分配点失败与恢复、D/R allocation count、热路径零引擎分配均有既有证据。逻辑报告不是 RSS 或新旧 Host 共存峰值。 |

## 5. 文档与证据精度问题

1. 阶段 2B 详细契约已把“Encode 创建时固定 Message”写成当前规则，但这相对原设计输入和真实消费者是实质能力收缩，并非可由实现自行选择的内部命名。应先修正/重新确认契约，不能用“实现符合新写契约”消解阻断。
2. public Host 专项与 external consumer 都只覆盖每个 Encode binding 一个 Message；即使测试所用 ASCII Pipeline 本身存在第二个可 Encode Message，也没有覆盖同 Handle 多 Message。因此 `52/52` 不包含本阻断的反例。
3. validation 声称 Reset 使目标 Codec 借用 view 失效；源码确实调用 `AdvanceViewEpoch()`，但 Host 专项没有保存一个 callback view 并在 Reset 后直接断言 `HasValue()==false`。由于公开 Host 契约本就把 view 缩窄为 callback 期，这不是消费阻断，但验证报告宜区分静态实现证据与专项直测。
4. Encode callback 抛异常时，当前实现保留已经由 Codec 产生的非零 `bytes_produced`，同时 `business_callbacks_returned==0`、status 为 `CALLBACK_FAILED`。这可以作为“实际生成事实”，但不是可交付输出；公开契约/字段注释应明确，避免消费者把 `bytes_produced>0` 单独解释为发送材料已交付。

## 6. 后续范围与停点

- 阻断修正只需要恢复 Pipeline-bound、调用期 Message选择及相应最大输出上界预算，不要求引入 UI DTO、Message 字符串 key、动态 binding、通信、线程或全量 Lab 模型。
- 修正后应重跑 public Host Debug/Release 专项、public-only consumer及受影响 Codec/metadata 定向回归；是否需要更广测试由总控按实际接口差异决定。本轮未执行这些验证。
- Lab 自有的 UTF-16 draft、选择 revision、冻结输入/未消费后缀、结果和展示材料复制、新旧 owner 共存峰值仍留在阶段 4，不并入 PAE Host。
- 未验证：全量 CTest、Lab/UI迁移、人工 NetAssist、SDK/动态库、Linux、网络、硬件、Golden、现场、正式性能或生产。

## 7. Selector 修正后针对性复核记录

### 7.1 接口与实现

- `HostBindingSpec` 已不再包含 `message_index`；`HostEndpoint::Encode` 改为显式接收调用期 `message_index`。endpoint+action 唯一键、一个 Encode channel 的 fault/generation 身份没有漂移。
- 创建期 `QueryEncodePipelineCapacity` 遍历绑定 Pipeline 的关联 Message，仅收集 `encode_available` 且具有 `EXACT` 或 `UPPER_BOUND` 可靠输出上界的 selector，并取最大 `encode_output_size`。实现把 selector 数组字节计入 `binding_storage_bytes`，把最大 buffer 计入 `encode_buffer_bytes`，分配点计入 `allocation_count`。
- 每次 Encode 先执行 callback-chain、Host guard、Handle、action 和 fault 检查，再在保存的 selector 集合中验证 Message；非法 selector 返回 `INVALID_ARGUMENT` 且 `codec_attempted=false`。合法 selector 传给同一 channel Codec，成功 Output 回报实际 `message_index`。
- fault 的优先级高于 selector 校验：callback 异常后即使传入非法 selector仍返回 `RESET_REQUIRED`。成功 Reset 令旧 Handle stale；重新 Find 后可在同一 Pipeline 选择另一个 Message。

### 7.2 既有验证证据核对

- 修正前证据保留为 Host `52/52` 和旧 API signature，明确其局限是能力无法表达而非测试运行失败。
- 修正后 Debug/Release Host 专项日志均为 `passed=56 failed=0`。新增断言直接覆盖同一 ASCII Handle 依次编码 `greeting` 与 `encode_only`、11 字节最大 buffer、Decode-only/越界/跨 Pipeline selector 零 Codec 调用、callback fault 的非零 `bytes_produced` 与零正常返回、Reset/重新 Find 后改选 Message。
- Reset view 精度项已增加直接断言：callback 保存的 `DecodedRecordView` 在 Host Reset 后 `HasValue()==false`。
- Debug/Release external consumer 均输出 `PUBLIC_HOST_CONSUMER_PASS candidates=1 successes=3 greeting_bytes=8 literal_bytes=6`，证明仓库外 public-only 消费者用同一 Encode Handle完成两个 Message。定向 CTest 日志仍为两配置各 10/10。
- 修正后资源报告记录 Debug `accounted=6415, allocations=80, measured_allocations=80`、Release `accounted=6111, allocations=42, measured_allocations=42`；相对修正前新增的是每个 Encode binding 的 selector 存储与创建分配，最大 Encode buffer继续按 Pipeline 上界计费。exact/-1、创建失败原子性和热路径零分配由同轮既有日志覆盖。

本轮只读取最终源码、测试和 `out/public-host-stage2b/encode-selector-fix/` 既有日志，未重新构建或运行测试。未验证范围仍包括全量 CTest、Lab/UI迁移、人工 NetAssist、SDK/动态库、Linux、网络、硬件、Golden、现场、正式性能和生产。

本任务已停止写入；首次复核唯一新增、修正后复核唯一更新的文件均为本报告，未修改代码、CMake、README、Qt、计划或其他报告，未 Stage、Commit、Push。
