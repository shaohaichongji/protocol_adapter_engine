# ASCII 有界流式接收首片契约

日期：2026-09-12。状态：六项推荐方向已由用户确认，契约已冻结；随后用户明确授权 PAE 首片
源码实施及 Windows Debug/Release 针对性验证。PAE 首片已实现并完成 Windows 离线验证，
总控定向复核发现的 Snapshot P2 已修复并关闭，当前进入交付范围收口，尚未 Stage、Commit、Push。
Lab 暂不实施；实施与 Git 交付分别授权。
本次授权仅覆盖第8节 PAE 侧范围及其验证报告，不以未来行为描述冒充现有能力。

验证证据见[本片 Windows 验证报告](windows-msvc-2026-ascii-stream-framing-slice.md)：初版
Debug/Release 各31/31；Snapshot 纠错先复现失败，再执行两个配置各6/6针对性回归。
不将初版矩阵记为已覆盖后发现的遗漏，不升级网络、Lab、Linux、性能、Golden 或现场结论。

基线：`main@407af0df6e7b77c05aa5afbc7f334822fe5ae205`，已核实远端 main 一致。
该合并提交交付 ASCII 完整记录 Lab 集成，不包含本文的 ASCII 流式能力。

## 1. 六项确认与范围

1. Schema 0.11、默认关闭能力门，首片仅 ASCII Text；0.10 完整记录和旧 Binary 不变。
2. 显式 CRLF 结束符、跨 chunk 匹配，候选帧包含 CRLF；不自动兼容 LF、不裁剪或补字节。
3. 流式 RX 模板以 CRLF 结束，合法记录内部不能提前出现 CRLF；不能证明安全则拒绝配置。
   TX 保持独立，不自动继承 RX 结束符。
4. 帧长上限包含 CRLF；超长后丢弃至下一 CRLF，再开始下一候选；半包保留，Reset 丢弃。
5. 每流独立 Workspace，同步 push、精确消费、STOP 不重复交付、预算与预分配。
6. PAE 先完成离线执行链，再接 Lab 手动 chunk 提交、观察和 Reset；最后集中验证。

不包含 UTF-8、通用分隔符、线上转义/反转义、文本数字转换、Binary/Text 混合包、端点注册、
TCP/UDP/串口、连接/线程/定时器、自动重连、Evidence/Replay/Compare 或稳定公共 ABI。
JSON 转义及 Lab ASCII escaped 是表示层，不是线上转义协议。

## 2. 分层与方向

宿主提供原始字节 chunk → Framer 形成完整候选帧 → ASCII Decode → 统一 BYTES 字段结果。
Framer 不创建中间字符串，不验证 ASCII 字符，也不匹配 Message；这些仍由 Core 处理。
Pipeline 在 Message 识别之前唯一决定 FramingProfile，不允许按 Decode 是否成功反选边界。

每逻辑流独占 FramingWorkspace 和 Decode ExecutionWorkspace；Plan 可只读共享并先于工作区创建、
晚于工作区销毁。回调中的帧和字段仅在相应借用期有效，宿主跨回调保存必须复制。
本片使用既有 direction_id 和 Pipeline 选择，不引入两端注册或 RX/TX 自动协商。
Encode 仍产出一条完整 TX 记录，不经 RX Framer，也不自动追加 CRLF。

## 3. 配置与版本门禁

新增默认 OFF 的 `PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING`，依赖已有 V10 ASCII、
V09 Framer、Loader 与 Core 能力。未满足依赖在 CMake 配置阶段明确失败，不隐式开关其他能力。
未启用新门时拒绝 0.11；旧版本不接受本策略。0.11 保留 0.10 Text 规则及 complete_record，
新增下列唯一流式策略，不将 0.9 三种 Binary 策略自动开放给 Text。

以下是已实现的精确 FramingProfile 形状，仅为配置片段，不是可单独加载的完整文档；
完整公开样例见[synthetic_ascii_stream_slice.pae.json](../examples/config/synthetic_ascii_stream_slice.pae.json)，
必须使用启用 V11 能力的构建加载：

```json
{
  "id": "ascii_rx",
  "display_name": "ASCII CRLF receive",
  "description": "Bounded records including CRLF.",
  "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_STREAM",
  "input_kind": "stream_chunk",
  "strategy": "ascii_crlf",
  "terminator_text": "\r\n",
  "maximum_frame_length": 64
}
```

- JSON 解码后的 terminator_text 必须恰为两个字节 0D 0A，不是反斜杠字符序列。
- maximum_frame_length 必填且为整数 M，2 ≤ M ≤ 有效帧资源上限，包含结束符。
- 不允许 minimum_frame_length、sync_bytes、frame_length_bytes、length_field 或其他未知属性。
  完整记录是否过短由 Core 判断；单独 CRLF 也是候选帧，不在 Framer 静默跳过。
- 其余 Strict JSON、重复键、类型、元数据、引用规则沿用原契约。
- stream Pipeline 至少引用一个 Decode 可用的 Message；只对其 RX action 检查 CRLF 与 M。
  每个 RX action 的最大记录长度必须 ≤ M；TX 的长度仍受原 Codec 资源限制，不受 RX 的 M 约束。
- TX-only Message 不作为 RX 候选；不改变独立动作的内部 OPERATION_NOT_SUPPORTED 语义。
- 普通 Lab CLI 与 Evidence 继续拒绝 0.11。Lab UI 未接入时也必须早拒绝，不能误走完整记录后端。

## 4. RX 边界证明与 Builder 复核

每个流式 RX action 的末段必须是以 CRLF 结尾的非空 literal。从字节 0 开始，任何合法记录的
首次 CRLF 必须恰在记录末尾。不能只检查单个字段或 literal 是否含 CRLF：还必须检查相邻片段
拼接处，以及 min=0 字段为空时跨多个片段产生的 CRLF。

Compiler 对 literal 字节、字段 min/max 与允许字符集合做有界边界分析；可用有限状态可达性
分析（普通、末字节 CR、已见 CRLF）及长度区间推导，不枚举字段的所有字节串。
保守分析可以拒绝无法证明的模板，但不得以测试样例未出现冲突作为安全证明。
允许单独 CR 或 LF 不必一律拒绝；只有可证明首次 CRLF 在末尾时才准入。
若保守分析会拒绝依赖现有字段终止约束才安全的配置，必须明确诊断原因，不静默改变字段规则。

Builder 发布前独立复核版本、策略联合成员、终止字节、M、RX 候选集合、长度与边界安全，
不只相信一个 Compiler 写入的布尔标志。无法证明时不发布部分 Plan。
Core 不改变已有完整记录模板匹配、歧义拒绝、字符验证、失败零字段交付与 TX-template 复核。

## 5. 切帧状态与边界优先级

| 状态 | 行为 |
| --- | --- |
| COLLECT | 缓存未完成候选，保留跨 chunk 的末尾 CR 匹配状态 |
| DELIVER_PENDING | 完整候选已形成，预算允许后进入同步 sink 一次 |
| DISCARD_UNTIL_CRLF | 候选已超长，仅计数和扫描至首个 CRLF，不缓存整个坏记录 |

规则：

- 首个 CRLF 结束当前候选，CRLF 保留在候选中；一个 chunk 可形成零条或多条候选。
- 恰好 M 字节且以 CRLF 结束：正常交付。已有 M 字节但没有结束符：立刻确认为超长，
  因任何补全都会超过 M，清除候选缓存并进入 DISCARD_UNTIL_CRLF。
- 进入丢弃状态时保留尾部 CR 匹配事实。例如第 M 字节为 CR、下一字节为 LF，该 LF 完成
  被丢弃记录的结束符，不能变为下一记录的开头；被丢弃记录绝不交付。
- 丢弃结束符本身也计为丢弃字节；随后从下一字节恢复 COLLECT。连续长垃圾不扩容，扫描受预算约束。
- 噪声不是隐式同步头之前的垃圾：没有超长时，它属于下一 CRLF 候选，并可能导致 Core Decode 失败。
- Decode 失败的候选已经消费；不回扫、不拆成其他候选、不重复交付。
- Reset 丢弃半包、pending 候选和丢弃模式状态，沿用现有 Reset 的计数生命周期规则；不冲刷半包为成功帧。
- 宿主断链或 EOF 不能隐式完成半包；是否 Reset、何时超时由宿主决定。

丢弃至 CRLF 只承诺确定性、有界恢复，不承诺找回真实业务边界或认证数据。

## 6. 消费、背压与资源

沿用[有界流式契约](bounded-stream-framing-contract.md)的 Push / Reset / 借用 / 并发与重入拒绝：

- bytes_consumed 仅计本次 chunk 已接受的连续前缀；不预取未消费后缀，宿主只重提该后缀。
- sink 被调用即为交付提交点，STOP 时当前帧计入已交付且不可重现；STOP 优先于同点预算耗尽。
- 空 push 可推进已有 pending 工作，不等待网络；真正半包返回 NEED_MORE。
- 超 max_submit_bytes 在推进状态前拒绝，消费和交付为 0，工作区原状态保持。
- 超长问题属于可恢复 Framing issue，不伪装成 API 调用失败；一次 push 可以先丢弃、再交付合法候选。
- frames_delivered 是候选交付数，不等于 Decode 成功数；bytes_discarded 包括先前缓存本次才判废的字节，
  不得据此要求它小于本次 bytes_consumed。malformed_candidates 对每条超长候选只增加一次。
- 无候选但处于丢弃状态且输入耗尽时返回 NEED_MORE，意为还缺恢复边界，不表示持有可解码半帧。

沿用既有有效 max_frame_bytes、max_submit_bytes、max_frames_per_submit、max_framer_work_units、
max_session_memory_bytes 及 Hard Limit，取更严者；不新增未经确认的公开数值上限。
缓冲容量为 M，不需 M+1 才能判超长。状态、固定结束符及其描述计入 Plan/Workspace 的真实预算；
Compiler 估算、Builder 复算与实际分配必须一致。所有长度、计数与预算运算检查溢出。
push 不分配或扩容；扫描、复制、状态推进纳入工作预算，预算耗尽后可继续且不重复计数/交付。
宿主示例不能在无进展时无界重复提交；必须区分需要新输入、剩余后缀及可继续内部工作。

Plan Snapshot 为 0.11 使用独立格式标识 `pae_plan_bundle_v0.11_ascii_stream_slice`，包含策略、
终止字节、M、RX 绑定与相关资源事实；旧代快照与指纹算法不变，不将新事实混入旧格式。

## 7. 诊断职责

| 场景 | 约定 |
| --- | --- |
| JSON 形状、未知键、重复键、类型 | 沿用 Structural 诊断，定位准确 JSON Pointer |
| 非 CRLF 终止文本 | ASCII_STREAM_TERMINATOR_INVALID，Domain |
| RX 模板末尾或内部边界无法证明 | ASCII_STREAM_BOUNDARY_UNPROVEN，Domain，指出相关 action/segment |
| 无 RX 候选或其最大长度超过 M | ASCII_STREAM_PROFILE_MISMATCH，Domain |
| 资源超限 | 沿用 Resource 诊断，不伪装成合法配置 |
| 损坏 Budgeted Draft | Builder INVALID_FRAMING_PLAN，不发布 Plan |
| 运行时超长 | 新增 FramingIssue RECORD_TOO_LONG，汇总计数，非逐字节错误回调 |
| ASCII/模板/字段 Decode 失败 | 保留原 Core 状态，字段有效交付数为 0 |

枚举数值及内部辅助名称不冻结为公共 ABI；如实现需改变上述职责或接受范围，先回报总控。

## 8. 分阶段任务与冲突控制

1. 总控：维护本契约、入口、路线、版本门禁和共享接口审查。
2. PAE：实现 Compiler/IR/Plan/Builder/Framer/快照及受影响 Core 版本门禁；提供独立期望向量和
   离线宿主示例。保持 Core 算法与完整记录语义；必要共享文件由此阶段集中修改。
3. PAE 接口与验证收口后，Lab 才开始 adapter/session/UI 实施；此前只能做只读差异分析。
   根 CMake、共享 DTO、Compiler/Plan 不由两侧同时写入。
4. Lab 使用手动 Submit chunk 与 Reset，不将一次输入编辑自动解释为新数据，不无限保留历史。
   结果有界：复用每 submit 回调上限和 STOP/续提能力；展示候选数、Decode 结果、消费数、
   丢弃数与停止原因。具体 DTO、展示容量及控件接线由总控在 Lab 实施前集中核定。
5. Lab ASCII escaped / Hex 仅将本次草稿转换为字节；语法失败不得调用 Push，也不得重置既有半包。
   Reload/Pipeline/模式变更必须显式处理流状态，禁止跨 Plan 或跨 Tab 沿用半包。
6. 每侧一次受影响 Windows Debug/Release 验证，总控一次必要集成及简短人工确认；不重复无变化历史矩阵。

## 9. 验收矩阵

- 配置：能力关闭拒绝、未知键/错类型、错结束符、边界冲突、空字段跨片段冲突、长度/资源越界、损坏 Draft。
- 基本流：完整一帧、每字节分片、CR 与 LF 分片、两帧粘包、完整帧加半包、纯 CRLF 候选。
- 精确上限：M 内成功、恰 M 且结束成功、M 无结束符判超长、末字节 CR 跨下一 LF 的丢弃恢复。
- 异常：长垃圾、多次预算耗尽、超长记录后合法记录、非法 ASCII/未知模板后下一合法记录。
- 交付：STOP 后只重提后缀、空 push pending 续交付、不重复帧；候选数与 Decode 成功数分开。
- 生命周期：半包 Reset、丢弃中 Reset、两流共享 Plan 但状态隔离、借用结果保存时主动复制。
- 兼容：0.10 ASCII、0.9 原三策略、旧 Binary 受影响集合、V11-off 构建、普通 CLI/Evidence 拒绝。
- Lab：合法/非法表示输入、分块动作、半包可见状态、失败后继续、Reset、跨 Tab 隔离及有界输出。

验收使用公开合成数据与手工明确的预期字节/字段，不只依赖同实现 Encode/Decode 往返。
Windows 离线通过不升级为 Linux、多线程压力、性能、网络、Golden、硬件或现场验收。

依据：[ASCII 完整记录契约](ascii-text-codec-minimal-contract.md)、
[既有有界流式契约](bounded-stream-framing-contract.md)、
[Lab ASCII 接入契约](lab-ascii-offline-integration-contract.md)。
