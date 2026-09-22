# Lab 对公开 Framer / Host 的消费者需求核对

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

状态：2026-09-14 阶段 2 只读核对结论，已完成派发范围，待总控合并复核。本文从当前
Binary/ASCII stream 与 Host 的实际调用路径提炼非 UI 的最小公开需求；本轮未实施公开接口，
未迁移 Lab，未运行构建或测试。

## 1. 结论与推荐分片

阶段 1/1B 已提供公开编译所有者、消费准备 metadata 和完整记录 `CompleteRecordCodec`，但当前
Framer 与 Host 仍直接暴露私有 Plan/Core 类型。若要让仓库外宿主和后续 Lab 不再持有
`PlanBundle*`、`FieldRef` 或重复 Decode，阶段 2 至少还要公开两层能力：

1. **Public Framer 首片**：由 `CompiledProtocol + Pipeline` 创建每流工作区，公开一次 Push 的
   精确消费、停止、Framing issue、内部待处理状态、候选回调借用期和 Reset；只形成候选，不宣称
   Decode 成功。
2. **Public Host 次片**：在不可变 endpoint/action/pipeline 绑定和独立流工作区之上组合 public
   Framer 与完整记录 Codec 语义；每个候选只 Decode 一次，同时向可选诊断观察者暴露成功/失败
   事实，只向业务 sink 交付成功结果。

推荐先 Framer、后 Host，而不是一次包装全部内部能力。Host 依赖 Framer 的消费与停止语义，先把
`bytes_consumed`、候选提交点和借用期独立冻结，可减少 Host 层同时处理绑定、Codec、回调故障和
多流生命周期的风险。

未发现需要改变现有 Framer/Core 流式语义的确定冲突。公开化不能借机修正或重解释既有行为；若
后续接口设计与本文列出的消费、STOP、Reset 或故障语义冲突，应先停下由总控决定。

## 2. 现场与当前调用链

本轮现场基线：

- 仓库：`<REPO_ROOT>`
- 分支：`main`
- HEAD：`dfb08f351cdb22a9b50c9e64e688e3b666e669bc`
- staged：空
- 工作树：已有大量已跟踪修改、删除和未跟踪文件；本轮全部保留

当前通用内部调用链：

```text
Host Session::Push
  -> StreamFramingWorkspace::PushStreamChunk
  -> candidate ByteView callback
  -> DecodeCompleteRecord（每候选一次）
  -> CandidateObserver（成功或失败）
  -> success-only business Sink
```

Lab 在此之上还有两套消费适配：

- ASCII `HostObserverAdapter`：每个 Decode binding 建立两条逻辑流；Submit 冻结输入，Continue
  只推进未消费后缀或内部工作；候选回调内复制原帧和字段；每步候选级 STOP。
- Binary `PreparedBinary`：同样按 binding/flow 保存冻结字节、消费游标、当前结果和草稿；回调内
  物化 typed 值、conversion raw 和展示范围；不执行第二次 Decode。

二者都说明：通用 PAE 必须公开精确执行事实和短期借用视图，但 Lab 的编辑、保存和展示状态不应
整体搬入 PAE。

## 3. Public Framer 最小消费者契约

### 3.1 创建与所有权

最小创建输入应为有效 `CompiledProtocol`、公开 Pipeline selector/index 和可选限额。消费者不能
提交私有 Plan 或 FramingProfile。创建应：

- 验证 Pipeline 确为当前支持的 `STREAM_CHUNK` 输入；
- 由 Framer 实例保活内部共享 compiled state，创建后原 `CompiledProtocol` owner 可被移动或销毁；
- 每个实例只绑定一个 Pipeline 和一条逻辑流，不共享可变 buffer/state；
- 一次性准备 frame buffer 和状态，返回实际计费与有效 limit；失败不发布半可用实例；
- 空/moved-from compiled owner、错误 Pipeline、非 stream 输入、资源越界分别失败关闭。

当前公开 metadata 尚不能直接区分 `COMPLETE_RECORD` 与 `STREAM_CHUNK`。阶段 2 至少应让消费者通过
公开 capability/query 或 Framer 创建结果确定输入种类，不能要求 Lab 解析 Schema、direction 文本
或私有 FramingProfile。strategy 名称是否进入首片可再收紧，但可否 Push 不能靠试错猜测。

### 3.2 Push 输入与消费所有权

一次 Push 接收一个 caller-owned `ByteView` 和同步 candidate sink，最多调用一次内部 Framer Push。
返回至少保留：

- API status；
- stop reason：`INPUT_EXHAUSTED`、`NEED_MORE`、`WORK_BUDGET_REACHED`、`SINK_STOP`；
- `bytes_consumed`、`frames_delivered`；
- `bytes_discarded`、`malformed_candidates`、最后 Framing issue；
- `work_units_used`。

`bytes_consumed` 只表示本次 input 已被 PAE 接受的连续前缀。返回后：

- PAE 不引用 `input[bytes_consumed, size)`；该未消费后缀仍归调用者；
- 已消费并缓存为半帧或内部 pending 的字节归 Framer 状态，不得再次提交；
- PAE 不应预先复制整块 input 后返回较小的 `bytes_consumed`；
- 入口参数/limit/owner mismatch 等失败为零消费、零候选且不改变原状态；
- callback/组合层故障已经发生的消费不能伪装成零消费或安全重放。

因此公开 Framer 不需要替调用者保存后缀，也不应内建输入队列。

### 3.3 Candidate、STOP 与继续

candidate 是 Framer 已确认的一条完整边界，只含候选字节和 Framing 身份，**不等于 Decode 成功、
业务成功或通信成功**。候选 `ByteView` 可能借用本次 input，也可能借用 workspace，仅在同步回调内
有效；跨回调、异步处理或界面保存必须在回调内复制。

sink 收到 candidate 即达到交付提交点：

- sink 返回 STOP 后，当前 candidate 已交付一次且不可在 Continue 中重现；
- `stop_reason=SINK_STOP`，其优先级高于同点的预算停止；
- STOP 后未消费 input 后缀仍归调用者，Framer 不预取或排队；
- CONTINUE 可在同一内部 Push 中继续处理，但仍受 frame/work limit 约束；
- sink 异常不能穿越 `noexcept` 边界；公开层应转换为明确失败，并保留真实消费事实。

公开 Framer 的 candidate sink 不应自行 Decode。独立 Framer 消费者需要协议结果时，可在该回调内
对 candidate 调用一次已有 public `CompleteRecordCodec::Decode()`；public Host 则应内部完成同一
次 Decode 并直接公开观察结果。两种路径都不得在显示层重复 Decode。

### 3.4 Observe、空输入 Continue 与 Reset

只读 Observation 至少要公开：

- collecting / delivery-pending / discard 等足以驱动调用的 phase；
- `buffered_bytes`；
- `has_internal_work`；
- 有效 `max_submit_bytes`、`max_work_units`，以及创建/内存报告中的必要限额。

Observation 不推进状态，也不承诺可与 Push/Reset 并发读取。`buffered_bytes == 0` 不能推出当前流
可接收新数据：discard 状态可能无缓存，pending 也可能只表现为内部工作。

零输入 Push 只用于上一调用遗留的 `has_internal_work`，例如 pending delivery、搜索或分段搬移；
此时 `bytes_consumed` 恒为 0。它不能刷新半帧、制造 EOF/超时、结束 discard 状态或重现已 STOP 的
candidate。Host 层可以像当前实现一样在没有内部工作时拒绝空 Push，避免无意义 Continue。

Reset 必须：

- 清除目标 Framer 的半帧、pending、discard 和内部游标；
- 不交付被丢弃的半帧，也不处理 caller 仍持有的未消费后缀；
- 不影响其他 Framer/逻辑流；
- 拒绝同实例回调重入和并发 mutation；
- 让上层能够开启新的 generation，拒绝旧代结果。

Framer 自身的历史统计是否归零不是 Lab 草稿语义；公开首片以每次 Push 事实和当前状态为主，避免
为 UI 引入无界累计历史。

## 4. Public Host 最小消费者契约

### 4.1 不可变绑定与显式方向

Session 创建前一次性接收有界 Binding 表。每项至少包含：

- caller-owned endpoint identity；
- 显式 `DECODE` 或 `ENCODE` action；
- 来自当前 `CompiledProtocol` 的 Pipeline selector；
- Decode 的逻辑流数量及可选 Framer limits。

不得根据 endpoint 名称、`rx/tx` 文本、Schema 或 direction 字符串推断动作。相同 endpoint 可分别有
Decode/Encode binding；相同 endpoint+action 重复绑定拒绝。创建期应验证真实关联能力、输入种类、
限额与总预算，整表成功后才发布 Session。

绑定表在 Session 内不可变。阶段 2 首片不需要 hot replace/unbind API：调用者准备一个独立新
Session，确认创建和自身复制预算成功后再原子替换活动 owner；创建失败或用户取消保留旧 Session。
旧 Handle 必须带实例作用域，不能因裸下标相同而命中新 Session。

### 4.2 Handle、compiled owner 与多流生命周期

公开 Session 应像现有 `CompleteRecordCodec` 一样保活内部 compiled state，不要求外部 owner 一直
存在，也不向外暴露 Plan。Session 销毁顺序必须保证各 Framer/Core workspace 先于 compiled state。

每个 Decode binding 的每条 logical stream 独立拥有 Framer、Codec workspace、generation 和 fault
状态；Encode 通道也独立。共享 immutable compiled state 不得共享可变执行状态。Reset 一条流不影响
另一流或 Encode；跨 Session/过期 Handle 应失败关闭。

首片保持同步串行调用：同一 Session 操作期间其他调用返回 BUSY；callback 内重入同样拒绝且不改变
状态。不引入线程、队列、Transport、重试或后台驱动。

### 4.3 Candidate Decode 观察与业务输出

Public Host 必须保留“诊断观察”和“成功业务交付”两条边界，不能只暴露成功 sink：

- 每个 Framer candidate 只调用一次 Decode；
- optional CandidateObserver 在该次 Decode 后收到候选原帧、generation、公开 `CodecStatus`、
  可用 Message/失败 Field/conversion/tainted 事实；
- Decode 成功时还可借用 typed fields、enum raw/known、Decimal 与 conversion raw；
- Decode 失败时字段交付为零，但原候选帧和已确认的失败位置仍可作为诊断事实；
- Framing 丢弃、malformed、无候选步和 API 早拒绝不得伪造 Decode observation；
- 业务 sink 只收到成功 Decode 或成功 Encode；零 Field 成功仍必须交付一次成功事实。

Candidate/Output 中不得出现 `PlanBundle*`、`FieldRef`、`EnumValueRef` 或 Core 私有 result。公开字段
身份应使用当前 compiled owner 的公开 Message/Field/Enum 索引；状态与 value 类型复用阶段 1 的
public domain。Host-specific view 可以复用相同访问语义，但不能让消费者创建第二个 Codec 再 Decode
一次来补原帧、raw 或失败详情。

Candidate 和成功 Output 的 frame/field/bytes 均只在当前同步 callback 内有效。调用者需要保留时
必须在借用期内复制；复制失败由调用者 callback 报告，Host 保留实际消费并将目标流置为明确的
Reset-required fault，不发布半个自有 DTO。

### 4.4 Observer STOP、business STOP 与异常

必须保留当前顺序：CandidateObserver 先于 success-only business sink。

- observer 正常 STOP：停止后续 candidates，但不取消当前成功 candidate 的一次 business delivery；
- 当前失败 candidate：永不进入 business sink；observer STOP 仍停止后续 candidate；
- business sink STOP：当前成功已交付，停止后续 candidates；
- observer 或 business callback 异常：捕获为 callback failure，停止后续处理，保留消费；observer
  异常时不再交付当前 business result；
- observed candidate 数、Decode success/failure 数和 business output 数分别统计，不能互相替代。

Result 至少应区分 Host/API status、是否尝试 Framing/Codec、最后 Codec status、Framer PushResult 和
上述计数。`Status::OK`、`frames_delivered`、`codec_attempted` 或 `successful_outputs` 任一单项都不能
单独解释为协议成功。

### 4.5 完整记录 Decode / Encode 的复用

Public Host 的完整记录 Decode、stream candidate Decode 与 Encode 应复用阶段 1 已公开的状态、typed
值、selector、failure 和 caller buffer 语义，不再建立第二套值类型域：

- 完整记录与 stream Push 是两个明确入口，半包不能送给完整记录 Decode；
- Decode 由 Pipeline 匹配 Message，不能用 TX Message selector 反选 RX；
- Encode 绑定还需 public Message selector 和 `EncodeValue` 列表，Message 必须属于该 Pipeline 且
  支持 Encode；
- Encode 成功 Output 是完整 record，不代表已发送或对端接受；
- failure 为零业务输出，不保留旧成功 bytes/fields。

阶段 1 的 `CompleteRecordCodec` 可以继续作为无 Host 的轻量入口；Public Host 是绑定、多流、Framer
和成功/诊断回调的组合层，不应强迫所有消费者都经过 Host。

## 5. Lab 自有状态与 PAE 通用状态的边界

| 当前 Lab 状态 | 归属 | 原因 |
| --- | --- | --- |
| 已消费前缀、stop reason、Framing issue、internal work、generation/fault | PAE 公开 | 这是执行和恢复正确性所需的通用事实。 |
| 未消费后缀字节 | caller/Lab | PAE 明确不引用、不保存；调用者按 `bytes_consumed` 管理。 |
| 冻结 chunk 与 cursor | Lab/宿主驱动 | 用于保证 Continue 只重提后缀；不是 Framer 内部输入队列。普通宿主也可用自己的 ring buffer。 |
| 未提交 Hex/ASCII/UTF-16 草稿、表示切换状态 | Lab | 编辑与展示语义，不是协议字节或 Host binding。非法草稿不得调用 PAE。 |
| 每流当前自有 DTO、诊断帧、字段显示文本和高亮 | Lab | PAE 只提供 callback-scoped execution view；跨回调保存和 UI 映射由 Lab 有界复制。 |
| Tab/load/session/selection/input revision | Lab | 用于拒绝异步 UI 旧结果；PAE 只需实例 scope、stream generation 和同步操作事实。 |
| binding endpoint/action/pipeline、stream count | PAE Host | 是宿主注册和方向隔离的通用事实。 |
| 未应用 binding draft、Apply/Cancel 提示 | Lab/宿主应用 | 活动 Session 不应因编辑草稿改变。Public Host 只创建不可变 binding table。 |
| 新旧 Session 替换事务 | 宿主应用 | PAE 保证单个 Session 创建失败原子性；应用负责候选 Session、确认和原子 owner swap。 |
| Framer/Core/Host accounted bytes 与 effective limits | PAE 公开 | 引擎与接入层资源事实。 |
| 草稿、冻结后缀、自有结果、展示复制及新旧实例共存峰值 | Lab/宿主预算 | 不能算进 PAE 热路径零分配或拿单 Session 报告冒充应用 RSS 上限。 |

## 6. 最小非 UI 调用序列

### 6.1 Standalone Framer

1. 编译配置，按公开 metadata 选择 stream Pipeline。
2. 以 `CompiledProtocol + Pipeline + limits` 创建一条 Framer instance，并记录 effective limits。
3. 调用者保存输入 buffer 和 offset；Push `buffer[offset..]`。
4. candidate callback 内立即消费或复制 candidate；需要 Decode 时调用一个同 compiled state 的
   `CompleteRecordCodec::Decode()` 一次。
5. `offset += bytes_consumed`；SINK_STOP 或 budget stop 后只保留/重提未消费后缀。
6. 若 Observation 显示 `has_internal_work`，可 Push empty；若只是 NEED_MORE，则等待新字节。
7. 断链、超时或放弃当前流时由宿主显式 Reset；不将半帧冲刷成 candidate。

### 6.2 Bound Host stream

1. 编译后构造有界 immutable bindings，创建 Session；保存 scoped Handle。
2. 为每条逻辑流分别保存 caller buffer/offset；调用 Session Push。
3. CandidateObserver 在借用期内复制成功或失败候选；每步单候选 UI 可返回 STOP。
4. business sink 仅处理成功结果；不能用其缺席推断“没有 candidate”。
5. 根据 Result 的消费、停止、Framing/Codec 与计数事实推进 offset；不得重放整个 chunk。
6. 仅在 `has_internal_work` 时以 empty Push 继续；新 Submit 必须等旧后缀和内部工作均处理完。
7. callback 或 Framing API fault 后保留实际消费并 Reset 目标 Decode 流；普通入口早拒绝按返回状态处理；
   其他流继续独立。
8. 变更 binding 时先创建新 Session；成功并经应用确认后替换 owner，旧 Handle/旧 generation 失效。

## 7. 首片优先级与有限自动化矩阵

### 7.1 首片优先级

| 优先级 | 建议范围 | 停点 |
| --- | --- | --- |
| P0 | Public Framer owner/create、PushResult、callback borrow、Observe、empty continue、Reset、busy/reentry、预算 | Binary 三策略和 ASCII CRLF 的仓库外 consumer 仅用公开头完成分片、STOP 后缀与 Reset；不接 Host。 |
| P1 | Public Host immutable binding、scoped Handle、多流、完整/stream Decode、Encode、CandidateObserver 与 success sink | 非 Lab 宿主完成显式方向绑定、失败候选观察、成功交付、流隔离和故障恢复；不接 UI。 |
| P2 | Lab 后置适配 | 总控核对 P0/P1 后另行派发；先 Binary/ASCII 非 Qt adapter，再 UI，不在本轮实施。 |

### 7.2 Public Framer 定向矩阵

1. owner：空/moved-from、owner 销毁后 instance 保活、错误 Pipeline、complete-record Pipeline 拒绝；
2. 策略：fixed、sync-fixed、sync-length、ASCII CRLF 的任意切分、粘包、跨块边界；
3. 消费：STOP 精确前缀、未消费后缀不被引用/复制、CONTINUE 多候选、limit 入口零消费；
4. 恢复：非法长度重同步、ASCII 超长 discard、candidate 后 Codec 失败不触发 Framer 回扫；
5. 预算：work/frame limit、pending 与搬移 empty continue、无剩余工作时不要求空循环；
6. 生命周期：callback view 仅回调期、Reset 半帧/pending/discard、busy/reentry、不同 instance 隔离；
7. 资源：exact/+1 create limits、accounted report、首次和重复 Push 的引擎分配边界。

### 7.3 Public Host 定向矩阵

1. binding：空/重复/未知/动作不可用、跨 Pipeline Message、错误输入种类、创建失败不半发布；
2. owner/handle：compiled owner 销毁后 Session 保活、跨 Session/expired Handle、不可变 binding；
3. candidate：成功、失败、零字段各观察一次；API早拒绝和 Framing discard 不伪造观察；Core 只调用一次；
4. STOP：observer STOP 仍交付当前成功一次，失败不进 business；business STOP；坏候选后好候选下一步继续；
5. fault：observer/business 异常保留消费、抑制相应业务输出并要求 Reset；入口错误零消费；
6. multi-stream：两流交错半包、Reset 一流、Encode 同时存在、generation 与状态不串扰；
7. result：Framing/Codec/observer/business 计数分离，unknown/ambiguous/integrity/conversion failure 位置；
8. resource：binding/channel/identity/accounted exact/+1，新旧 Session 共存由 consumer 单独计峰值；
9. external consumer：仅 `pae` 公开头和库，Debug/Release 运行；无 Qt、Lab 或私有 include。

## 8. 明确不进入本片

- Lab physical byte/bit range 和可变记录本次实际范围；
- Encode 同调用生成字段观察与 raw/logical 展示；
- 完整 Field 输入约束模型；
- Transport、Socket/串口、连接生命周期、线程、定时器、重试、自动转发；
- binding 持久化、UI 草稿、历史队列、Evidence/导出；
- SDK/ABI、Linux、硬件、现场或生产声明。

这些项目不应塞入 Public Framer/Host 首片，也不能通过重复 Decode、raw 反算或将 Lab DTO 整体搬入
PAE 来绕开。

## 9. 本轮交付边界

- 实际新增：`docs/engineering/lab-public-framer-host-consumer-review.md`
- 未修改：源码、CMake、其他报告、README/index、AGENTS.md、Qt 及既有工作树内容
- 未执行：配置、构建、测试、示例、UI、网络、格式化、清理或目录移动
- Git：未 Stage、未 Commit、未 Push
- 停点：报告完成后停止写入，等待总控合并复核；未实施接口、未迁移 Lab
