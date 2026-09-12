# Lab ASCII 离线分块接收观察契约

日期：2026-09-12。状态：用户已确认六项推荐方案，并独立授权按本文串行实施及 Windows Debug/Release 验证。
最小观察接口已实现并经总控复核，Windows Debug/Release 各2/2通过；
Lab 接线与限定修复已完成；用户于2026-09-13确认人工验收全部符合预期并关闭Lab。
Stage、Commit、Push 均不在本轮授权内。

## 1. 基线与六项确认

PAE 基线为 `main@716ff00ea8aa3d3f46b914008890f9706670d32b`，已完成提交推送。
该提交包含 Schema 0.11 引擎首片，不包含 Lab 流式接入。既有 Lab ASCII 完整记录检查点为
`407af0df6e7b77c05aa5afbc7f334822fe5ae205`。实施前重新检查 HEAD、工作树与依赖。

1. 新增 Stream Inspect，仅接 Schema 0.11 的 stream_chunk + ascii_crlf；保留 Encode 和完整记录 Inspect。
2. 手动 Submit chunk、Continue、Reset；编辑不提交，续提不重复整个 chunk。
3. 每步至多交付一个候选，仅保存当前结果；新 chunk 字节上限为 min(64 KiB, 引擎有效提交上限)。
4. 展示精确消费、剩余、缓存、丢弃、候选、Decode 与停止事实；补充最小只读状态观察接口。
5. 每 Tab 独立流状态；非法草稿不破坏半包；切换配置、Pipeline 或离开流模式显式处理未完成状态。
6. 新能力默认关闭，限定 adapter/UI；现有 Lab Qt/MSVC 工具链 Debug/Release 针对性验证后人工验收。

本契约细化 [PAE ASCII 流式契约](ascii-stream-framing-contract.md)第8节。
ASCII 输入、动作参与、字段范围及旧功能仍遵循 [Lab 完整记录契约](lab-ascii-offline-integration-contract.md)。
不增加网络、串口、设备、端点注册、线程、定时提交、UTF-8、通用分隔符、历史列表、导出或 Evidence。
既有双 Tab 上限不变。不将 Lab 表示语法当成线上转义协议。

## 2. 模式与分层

- Stream Inspect 仅在上述流式 Pipeline 可用；完整记录 Pipeline 使用原 Inspect。
  不把一个 chunk 直接送入完整记录 Inspect，不自动开放 Schema 0.9 Binary 流式 UI。
- Encode 仍独立执行 TX 模板，不经 RX Framer、不补 CRLF；单向动作限制在后端再次校验。
- Schema 0.11 complete_record 沿用完整记录路径；旧 Schema 接受域不扩大。
- QWidget 不重解析配置、不实现切帧或消息匹配。描述中的 input_kind、strategy、M 和有效资源限制
  从 Frozen Plan 提取；作者描述仍来自 Sidecar。Decode 由 Core 唯一匹配，不使用 TX 消息下拉反选 RX。
- 每个 Tab 的所选流拥有独立 FramingWorkspace 和 Decode Workspace；复用已有 Plan 所有权，
  不复制拥有者、不形成双重释放。工作区先于 Plan 销毁；切换 Tab 不销毁流。

## 3. 提交与续提

每次按钮动作最多调用一次 Push，不在 UI 线程自动循环耗尽所有数据。

| 动作 | 条件及效果 |
| --- | --- |
| Submit chunk | 无待续提任务；先完整解析及容量检查，再冻结本次字节和游标，调用 Push |
| Continue | 存在未消费后缀或可推进内部工作；只提交后缀，或在仅内部工作时空 Push |
| Reset | 清除流工作区、未消费输入、当前结果和本流计数；不能把半包当完整帧交付 |

sink 收到一个候选即返回 STOP，无论 Decode 成功或失败，当前候选均只计一次。
bytes_consumed 只推进冻结输入的游标；整个 chunk 不重放。输入全消费且无内部工作时结束本次任务，
清除已提交草稿或明确标记其已提交并阻止无编辑的重复提交。保留半包时允许提交下一个新 chunk。
有待续提任务时禁止新 Submit，待处理字节不可被草稿编辑或表示切换替换。
空草稿不作为新数据提交；空 Push 仅用于明确的内部续提，半包/丢弃等待边界不能靠空提交完成。
预算耗尽允许手动续提；如持续无消费、无交付且无状态进展，显示诊断并停止续提，要求 Reset 或调整配置，
不得自动忙循环。API 非成功和结果复制失败不得伪装成重试安全：保留实际消费事实，进入需要 Reset 的故障状态。

## 4. 容量、输入与结果所有权

- chunk 上限 C = min(65536, 引擎有效 max_submit_bytes)，按解析后的字节计；C 不限制跨 chunk 的帧长 M。
  不沿用完整记录输入框的 M 作为 chunk 上限；一个合法 chunk 可以包含多条记录。
- 编辑文本容量按 C 和最坏转义膨胀检查溢出后计算，Hex 空白也受有界文本容量限制；实现须记录确切公式。
  超文本容量的编辑整次拒绝，不截断；解析后超 C 整次拒绝，不调用 Push、不改变工作区。
- ASCII escaped / Hex 沿用完整记录输入语法。非法转义、Unicode 或容量错误仅更新草稿诊断，
  不调用 Push、Reset，不清除已有半包、丢弃模式或待提交后缀。
- 仅保留本步候选及结果，无候选时明确显示“本步无候选”，不残留上步成功字段、范围或高亮。
  候选失败保留标明为诊断材料的原帧，不交付部分有效字段；零字段成功独立表达。
- DTO 复制帧及有效字段，不长期保存借用指针。结果内存按候选上限及字段上限有界预算，检查乘加溢出；
  不默认将字段字节重复存入多份历史。回调 noexcept 内失败须捕获并 STOP，不能越过借用期补读。
  引擎 Push 的预分配与无热路径扩容承诺不等于 Qt/adapter 零分配；两者计费分别说明。
- 输入语法位置沿用零基 UTF-16 code-unit；帧中字段范围使用候选帧内零基字节偏移，不伪称全流偏移。

## 5. 最小观察 DTO 与状态失效

以下冻结语义，不冻结具体 C++ 类型名或公共 ABI：

- 描述：Pipeline 身份、input_kind、strategy、M、有效提交和工作预算、Lab 的 C。
- 执行身份：文档、加载、Plan、Pipeline 修订，加 stream generation 和本次操作序号；
  草稿修订只关联本次冻结输入，不自动改变引擎流状态。过期结果不得覆盖新流。
- 每步事实：API 状态、停止原因、消费字节、冻结 chunk 剩余字节、缓存字节、丢弃字节、
  malformed_candidates、FramingIssue、候选数、Decode 成功数、Core 状态及可用消息身份。
- 工作区只读观察：收集、待交付、丢弃至 CRLF，以及是否有可继续内部工作。
  BufferedBytes=0 不能推断为空闲；丢弃模式可能没有缓存。观察不得推进状态，串行调用、不承诺并发读取安全。
- 本流累计候选/Decode 成功计数与单步值分开；所有累计运算检查溢出，Reset 开启新 generation 并归零。
  frames_delivered 是候选数；bytes_discarded 可含之前缓存，不要求小于本步 bytes_consumed。

编辑或合法字节等价表示切换清除旧结果显示，但保留引擎流状态；非法切换保留原表示和草稿。
切换 Tab 保留各自状态。Reload、Pipeline 切换、离开 Stream Inspect 或关闭 Tab，若存在半包、
待交付、丢弃模式或未消费后缀，先明确提示将丢弃状态；取消则不执行转换，确认后统一清理并更新修订号。
确认 Reload 后即终止旧流；新配置加载失败也不恢复旧半包。仅未提交草稿沿用既有草稿策略。
Reset 本身是显式丢弃动作，不另设重复确认；是否保留未提交草稿须与冻结输入分开，不得变成自动重放。

## 6. 门禁、实施顺序与冲突控制

新 Lab 流式能力显式默认 OFF，依赖 V11、已有 ASCII adapter 及其引擎依赖；缺依赖配置阶段拒绝，
不暗中开启。独立无 Qt adapter 测试构建可用，UI 消费时须明确开启新能力。
不得直接删除 V11 与 Lab 的全部互斥检查：仅精确开放新 adapter/UI 组合；普通 Lab CLI/Evidence 继续拒绝0.11。
所有库与消费者的共享布局宏一致；Core 不依赖 Qt 或 Lab。

1. 总控先冻结本文、最小观察接口/DTO及顶层门禁；获得实施授权后才改代码。
2. 先完成 PAE 最小只读观察接口及相关测试，由总控定向复核。不改切帧、Schema 或 Core 语义。
3. 再派发《Lab应用推进》实施 adapter/session/UI、专属测试和报告；共享文件不得两侧并发写。
4. 总控集中集成与人工验收。保留已有变更，不私自提交、合并或跨工作树复制实现。

使用既有 Qt 5.13.x x64、v142 14.29 导入规则；全链用相同兼容工具链重编译，不链接另一 MSVC
工具链遗留产物。不升级 Qt。具体构建目录、开关和产物路径写入实施报告。

## 7. 验收与停点

Windows Debug/Release 针对性矩阵：

1. 完整一帧、逐字节分片、CR/LF 分片、两帧粘包逐步 Continue、完整帧加半包；无重复交付。
2. 恰 M 成功、M 无 CRLF 超长、跨 chunk 丢弃恢复；候选与 Decode 成功分别计数。
3. 未知模板/非法 ASCII 候选后继续合法候选，零字段成功；失败清理旧显示。
4. STOP 后只重提后缀、预算耗尽、内部 pending 空续提、无进展保护及候选复制失败处理。
5. C 边界、超 C、文本容量整次拒绝、非法草稿保留半包、表示切换不提交。
6. 半包/丢弃/pending Reset，转换确认与取消，Reload 失败、Tab 隔离、过期结果拒绝、借用期结束后的自有结果。
7. 默认 OFF、缺依赖拒绝、限定新组合通过、CLI/Evidence 拒绝；0.10 ASCII 和旧 Binary 受影响回归。

人工验收集中检查：分片与粘包续提、非法输入后恢复、超长恢复、Reset 与跨 Tab 隔离；提供准确 EXE/JSON
完整路径及独立期望值。既有 PAE 自动测试不代表新 UI 验收完成，不重复无变化成功矩阵。
不声称 Linux、性能、网络、Golden、硬件或现场通过。

当前进展：最小观察接口已完成并复核，见[第一阶段报告](lab-ascii-stream-observation-stage1-validation.md)。
Lab 接线及限定修复已完成，三项修复Debug/Release各3/3回归通过，用户人工验收通过，
见[Lab验证报告](lab-ascii-stream-observer-validation.md)。当前进入提交候选范围与信息收口。
没有 Git 写操作授权；未暂存、Commit或Push。
