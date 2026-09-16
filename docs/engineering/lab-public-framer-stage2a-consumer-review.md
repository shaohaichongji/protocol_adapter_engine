# Lab 对公开 StreamFramer 阶段 2A 的后置消费复核

日期：2026-09-14。状态：已完成派发范围，待总控复核；本报告仅做消费侧静态复核，未迁移 Lab，未运行构建或测试，未获 Stage、Commit、Push 授权。

## 1. 结论

最终公开 `StreamFramer` 已具备 Lab 后续按“每个 pipeline/逻辑流一个实例”消费所需的 2A 基础语义：能力查询与创建分离，调用方持有未消费后缀，候选同步借用，`STOP` 提交当前候选，空 `Push` 与 `Continue` 同路，`Reset` 只作用于目标实例，`Observe` 仅作串行空闲查询，实例独立持有 compiled state 与可变 workspace。未发现会导致 Lab 丢帧、重复 Decode、跨流污染或错误延长候选寿命的确定消费阻断项。

存在一个应在 2A 最终收口前明确处理的可观察契约缺口：跨实例嵌套回调 `A -> B -> A` 时，最后一次对 A 的调用会安全返回 `WORKSPACE_BUSY`，而当前设计文字把“回调内调用同一个 Framer”统一约定为 `REENTRANT_CALL`。该路径不会进入 A 的内部状态机，也不会死锁、递归交付或改变 A 的状态，因此不是状态安全缺陷；但状态分类与公开约定不完全一致，且现有专项测试没有覆盖这一间接回环。建议 PAE 选择“实现识别活动 owner 链”或“把公开约定明确缩窄为直接回调重入可保证 `REENTRANT_CALL`，间接回环允许 `WORKSPACE_BUSY`”，并增加对应断言。未明确前，不宜宣称重入状态命名已完整覆盖。

上述结论不表示 2B Host、Lab 迁移、SDK、Linux、网络、硬件或现场验证完成。

## 2. 复核范围与证据边界

- 现场基线：`main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`；接管时暂存区为空，保留共享工作树全部既有修改。
- 复核了公开头与 facade 实现、公开 Codec 借用规则、独立 consumer 示例、public StreamFramer 专项测试、[PAE 阶段 2A 验证报告](pae-public-framer-stage2a-validation.md)、[公开 Framer/Host 设计](pae-public-framer-host-slice-design.md)、[阶段推进计划](pae-execution-delivery-organization-plan.md)及当前 Binary Lab 的冻结输入/观察结果路径。
- 本轮只读取既有 D/R `passed=37 failed=0`、定向 CTest 7/7 和仓库外 consumer `candidates=2 decode_calls=2` 的报告记录；没有重新执行命令，因此这些是 PAE 交付证据，不是本轮新增验证。
- 当前 Binary Lab 仍通过私有 `host_endpoint::Session` 组合 Framer/Core，并自行冻结输入、保存未消费游标、复制候选与展示材料。2A 复核只判断公开 primitive 是否保留这些消费事实，不把现有 Lab 视为已迁移。

## 3. 推荐消费序列与所有权

后续真正迁移到公开接口时，单条流的最小调用循环应保持如下边界：

1. 从公开 metadata 取得 `pipeline_index`，先查询 `QueryStreamFramingCapability`，再分别创建该流的 `StreamFramer` 和独立 `CompleteRecordCodec`；`available=true` 不是创建预算承诺。
2. Lab 在提交前冻结本次 chunk，并保存 `[bytes_consumed, size)` 的游标。一次 UI 动作只调用一次 `Push` 或 `Continue`；若仍有内部 pending，先以 `Continue` 推进一步，再决定是否重提未消费后缀。
3. sink 内把 `FrameCandidateView::bytes` 视为仅当前同步回调有效；每个候选最多调用一次 `Codec::Decode`。原始候选、字段值、诊断与展示材料均从这一次结果在回调内完成必要复制。
4. 特别是 Codec 的 `DecodedRecordView`/`DecodedFieldView` 虽以 Codec epoch 判断有效性，`BYTES` 还额外借用 Decode 输入；当该输入来自 Framer candidate 时，candidate 回调返回即已失效。不能用之后仍为真的 `HasValue()` 推导其中 `BYTES` 仍可读取，也不能把 view 保存到 UI 线程后再取值。
5. sink 返回 `STOP` 后，当前候选已交付并从 Framer 移除；Lab 只能重提本次未消费后缀，不能重提已提交候选，也不能用 Decode 失败要求 Framer 回扫。
6. `Reset` 丢弃目标实例的半包或 pending；Lab 同时清理该逻辑流对应的冻结后缀、Codec 结果和展示结果，不能影响其他流。配置/绑定替换应销毁旧实例并新建，不迁移半包。

这与当前 `PreparedBinary` 的冻结 buffer、`consumed` 游标及回调内复制方向一致。公开 2A 能替代切帧 primitive，但不能单独替代现有 Host 的 binding identity、generation、异常后的 `RESET_REQUIRED` 和成功/失败候选路由。

## 4. 分项复核

| 项目 | 结论 | 消费侧说明 |
| --- | --- | --- |
| capability / Create | 通过 | 从冻结 pipeline/profile 读取事实；越界、非 stream、资源 override 均失败关闭。能力与准入已正确分离。 |
| consumed / suffix | 通过 | facade 每次只调用一次内部 `PushStreamChunk`，原样映射精确消费前缀，不持有调用方后缀。Lab 必须继续拥有冻结输入和游标。 |
| STOP / candidate | 通过 | 当前候选在 callback 调用点提交；`STOP` 不重放且不预读后缀。候选数不代表 Decode 成功。 |
| empty Push / Continue | 通过 | `Continue` 直接走 `Push(ByteView{}, sink)`，消费量为 0；没有隐式循环排空。 |
| Codec 组合 / BYTES | 语义通过，组合证据有限 | 两候选/两次 Decode 已验证；公开头已明确 candidate 与 Codec BYTES 的双重借用。组合测试复制了 candidate 原始字节，但未直接断言 decoded BYTES 在回调内复制。 |
| Reset / 隔离 | 通过，facade 专项覆盖有限 | facade 测试覆盖半包 Reset 和双实例 buffer 隔离；pending Reset 由既有内部/Host 测试支撑，public facade 专项未单列该断言。实现直接串行委托现有 reset。 |
| owner / move | 通过 | Framer/Codec 各自保活 compiled state；Framer move 保留半包并使 moved-from 失败关闭。调用或 callback 中 move/destroy 是明确的调用方违规。 |
| direct reentry / concurrency | 通过 | 直接回调内 `Continue`、`Reset`、`Observe` 返回 `REENTRANT_CALL`；其他线程占用返回 `WORKSPACE_BUSY`，guard 在 callback context 前取得。`Continue` 与非空 `Push` 共用同一入口守卫。 |
| `A -> B -> A` 间接回环 | 安全拒绝，但状态命名有缺口 | B 回调期间 thread-local 当前 owner 是 B；回到仍持 lease 的 A 时 owner 等值检查未命中，A 的 lease 获取失败并返回 `WORKSPACE_BUSY`。A 状态安全，但与当前统一的 `REENTRANT_CALL` 文字不完全一致。 |
| Observe / MemoryReport | 通过 | `Observe` 有同一 facade guard，只允许串行空闲读取；`MemoryReport` 返回创建时不可变副本，不读取 workspace，因此无需把它解释为并发状态快照。 |
| memory report | 通过 | internal workspace、public facade、共享 compiled-state facade 分列；实例总计只合并前两者，有效 session limit 仅对应内部 workspace，均不是 RSS 上限。 |

## 5. 非阻断精度问题

1. `include/pae/stream_framer.h` 的借用注释写为不保留 `input[input_consumed, input.size)`，但公开结果字段实际名为 `bytes_consumed`。语义可由上下文确定，建议在后续 PAE 修订中统一名称，避免消费者误以为另有字段。
2. 验证报告称 callback 内 `Push/Continue/Reset/Observe` 均已断言；实际 public 测试对提交入口显式调用的是 `Continue`，它通过实现委托覆盖 `Push(empty)` 的共同 guard，但没有单列非空 `Push` 断言。实现证据足够支持当前结论，报告措辞宜改成“提交入口（经 Continue）”。
3. public facade 专项没有直接覆盖 pending 状态 Reset，也没有覆盖 decoded BYTES 的回调内复制；既有内部测试和头文件契约提供支持，但不应把它们表述为本专项直接验证。

这些项目不要求 Lab 现在迁移或扩展 UI 模型，也不构成重新运行整套 UI/人工验收的理由。

## 6. 留给 2B 与阶段 4 的内容

- 2B Host：endpoint/binding identity、handle scope/generation、一次 Decode 后的成功业务 sink 与失败 candidate observer 分流、可抛业务 callback 的捕获及目标流 `RESET_REQUIRED`、逐流复位与失败隔离、Host 聚合资源计费。
- 阶段 4 Lab 迁移：把当前私有 Host/Plan 类型替换为公开 Compiler metadata、StreamFramer、Codec/未来 Host；保留 Lab 自有 UTF-16 draft、冻结 chunk/未消费后缀、观察身份、结果与展示材料复制、UI 线程发布和人工烟测。
- 2A 不应加入通信、线程、自动重试、UI 展示字段、Evidence 事务或业务路由，也不应为了 Lab 保存 borrowed view。

本任务已停止写入；唯一新增文件为本报告，未修改代码、CMake、README 或 Qt 文件。
