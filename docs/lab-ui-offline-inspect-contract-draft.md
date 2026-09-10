# PAE Lab UI 离线 Inspect 契约草案

日期：2026-09-10

基线：`dc5c4433b51961a9e2d24f425fba672a7121290e`，独立 worktree 分支
`feat/lab-ui-c1`

状态：**方向与八项规则已确认；已实现并通过自动化验证；人工验收待执行
（CONFIRMED / IMPLEMENTED / AUTOMATED_VERIFIED / MANUAL_UNVERIFIED）**

2026-09-10用户集中确认总控归纳的八项规则：单完整记录、Pipeline内唯一匹配、模式草稿隔离、
宽容但有界的Inspect Hex输入、输入大小限制、失败不交付、明确物理位置及字段诊断、复用既有桥接。
本文后续“推荐”措辞属于这些规则内的实施细化，不允许改变已确认的接受域或生命周期语义。
确认契约不授权实现、构建、测试、运行、暂存、提交、合并或推送；源码实施等待独立任务授权。

## 1. 集中确认结论

2026-09-10实施授权更新：用户已授权整个离线Inspect检查点的限定源码、测试、说明及Windows
Debug/Release离线验证；不执行网络，不Stage、Commit、Push、合并或清理生成物。本文前述
“等待独立任务授权”记录的是确认时点，现由本次授权满足；尚未实现或验证的状态不因此升级。

2026-09-10实施结果更新：限定 UI-owned 实现已完成，Windows v142 Debug/Release 定向构建均通过，
每个配置的 Protocol Lab UI 离线矩阵均为 8/8 PASS，其中包含 6 项 headless 和 2 项 shown-window
Qt smoke。shown-window smoke 是程序驱动窗口回归，不是人工鼠标/键盘验收；人工验收仍待总控安排。

以下决策已获集中确认并已在本 checkpoint 实现：

1. Inspect 只处理已经完整到达内存的单条 record；用户必须先选 Pipeline，Message 由 Core 在
   该 Pipeline 内唯一结构匹配得出。
2. Inspect 不读取、也不信任 Encode 模式当前选中的 Message。即使 Encode Message 下拉框仍
   显示某项，也不得把该索引传给 Inspect 或用于覆盖匹配结果。
3. Encode 与 Inspect 是两个显式模式，各自保留独立输入草稿；切换模式不把 Encode 输出自动
   灌入 Inspect，也不把 Inspect 原始输入变成 Encode 的“有效输出”。
4. Inspect Hex 接受大小写十六进制数字及四种 ASCII 空白，拒绝前缀和其他分隔符；解析失败
   必须给出原始文本偏移，且不调用 Core。
5. Inspect 输入同时受所选 Pipeline 的最大 record 大小、Desktop profile 的 65,536-byte 上限
   与原始文本大小预算约束。
6. 只有 Core 返回 `OK` 的 Inspect 才产生“有效解码结果”；失败时保留原始输入和失败提示，
   但不得把部分字段或原始帧标成有效输出。
7. 字段详情必须同时显示字段身份、raw/logical 值和物理位置；位字段必须明确显示 zero-based
   frame byte offset、`0xNN` mask、byte 内 LSB0 bit 编号及全局 bit offset。
8. 共享 `ExecutionBridge` 不需要新增接口；本 checkpoint 复用现有 `InspectPipeline`。新增状态、
   Hex parser 和显示模型归 `tools/protocol_lab_ui` 所有。

总控确认摘要还明确：模式切换保留各自草稿，Pipeline切换清空两类草稿；字节上限
`B=min(Pipeline最大完整记录, Plan max_frame_bytes, 65536)`，原始文本上限`3*B`；
明确字段及原因的诊断改进纳入本检查点。编号归纳不同不改变上述已确认范围。

## 2. 范围与非目标

### 2.1 本 checkpoint 范围

- 继续使用现有 Loader/Compiler/Plan 与 UI sidecar，仅支持已存在的 Schema 0.5、0.6、0.7。
- 在单个已加载文档 Tab 中提供显式 `Encode` / `Inspect` 模式。
- Inspect 接收一条完整 record 的 Hex 文本，限定所选 Pipeline 进行结构匹配和 Decode。
- 呈现匹配到的 Message、字段 raw/logical 值、字段级错误提示以及精确 byte/bit 映射。
- 用 headless 单元测试、现有 Bridge 合约测试、shown-window smoke 与一次人工 walkthrough 完成
  单 checkpoint 验收。

### 2.2 明确非目标

- Schema 0.8 及其 Bundle、Evidence、Replay、持久化或审计语义。
- stream/framing、分包粘包、增量输入、`bytes_consumed`、同步头搜索或跨 chunk 状态。
- UDP/TCP/串口、Loopback、网络、硬件、设备生命周期与后台接收线程。
- 配置或结果 import/export、文件保存、历史记录、最近打开列表、跨 Tab 复制。
- 将人工 Inspect 结果提升为 Golden、硬件、现场或部署验收证据。
- 修改 Core 的结构匹配、完整 record codec、Schema 规则或资源预算。

## 3. 当前源码事实与复用边界

以下是基线源码事实，不是新增提案：

- `ExecutionBridge::InspectPipeline(frame, pipeline_id, ...)` 已存在。
- 该函数先用 `FindPipeline` 解析指定 Pipeline，再调用
  `protocol_core::internal::MatchCompleteRecordStructure`；只有结构匹配返回唯一 Message 时，
  才以匹配得到的 `message_index` 调用 `DecodeCompleteRecord`。
- `UNKNOWN_MESSAGE` 与 `AMBIGUOUS_MESSAGE` 在 structural-query 阶段返回，不进入主 Decode。
- Decode 失败时，Bridge 的 `Result` 已映射 `message_id`、codec status、diagnostic；Core 提供有效
  `failed_field_index` 时，Bridge 同时映射成成对的 `failed_field_id/index`。
- Decode 成功时，Bridge 已物化每个字段的 `raw_value`、`logical_value` 及既有 enum/decimal 信息。
- UI sidecar 映射已有字段 `byte_offset`、`byte_width`、`physical_bits`、`byte_range`、integrity
  storage，以及 Schema 0.7 的 computed-length storage。

因此推荐的共享 Bridge delta 为：**无**。不得为了 UI Inspect 新增“指定 Message 直接 Decode”
入口，也不得改用全 Plan 的 `Inspect` 来绕开显式 Pipeline 限定。

若实现阶段发现上述现有接口无法满足已确认契约，应暂停并提交新的最小接口评审；不得在 UI
checkpoint 内静默扩展共享 Lab/Core API。尤其不应为单纯的显示需求把 Qt 类型传入 Bridge。

## 4. 模式、选择与草稿契约

### 4.1 显式模式

推荐定义：

- `Encode`：需要 Pipeline + Message，编辑 typed field drafts，执行现有 Encode。
- `Inspect`：只需要 Pipeline，编辑独立 raw Hex draft，执行 `InspectPipeline`。

Pipeline 是两个模式共享的文档级选择。Message 是 Encode 模式选择；Inspect 的 Message 仅是最近
一次 Inspect 运行产生的只读匹配结果，不是可编辑选择。

### 4.2 独立草稿

- Encode 的 typed drafts 与 Inspect 的 Hex draft 分开存储、分开修订、分开校验。
- 模式切换保留两个草稿，便于来回比较，但立即隐藏另一模式的结果语义和高亮状态。
- 不提供“Encode output -> Inspect input”自动联动；如以后需要，必须是单独、显式、可撤销的
  用户动作，不属于本 checkpoint。
- Pipeline 改变时，清空 Encode Message/typed drafts、Inspect Hex draft、两种结果、失败身份和
  高亮，避免把一条帧错误地带到另一 Pipeline。
- 同一 Pipeline 内改变 Encode Message，只影响 Encode 草稿与 Encode 结果，不修改 Inspect
  Hex 草稿；但当前显示若处于 Encode 模式，应清除旧 Encode 结果。
- 新 Plan 成功替换旧 Plan、重新 Load、关闭 Tab 时，两个模式的全部草稿和结果均失效。

## 5. Hex 输入接受域、偏移与预算

### 5.1 接受域

推荐 Inspect parser 接受：

- ASCII 十六进制数字：`0-9`、`A-F`、`a-f`；
- ASCII 空白：space (`0x20`)、horizontal tab (`0x09`)、CR (`0x0D`)、LF (`0x0A`)。

空白在配对前忽略；输出统一规范化为 uppercase、无分隔符的 byte 序列。至少需要一个完整 byte。
明确拒绝 `0x`/`0X` 前缀、冒号、逗号、下划线、连字符、其他 ASCII 字符、非 ASCII 空白及任意
非 ASCII 字符。忽略空白后必须有偶数个 hex digit。

这里不复用 `ParseCanonicalUpperHexText` 改变其语义：该 parser 是 Values BYTES 的严格 canonical
接受域，仍应保持“大写、连续、偶数且非空”。Inspect paste parser 应作为 UI-owned 独立组件。

### 5.2 大小预算

对所选 Pipeline，定义：

`B = min(该 Pipeline 所含 Message 的最大完整 record 字节数, Plan max_frame_bytes, 65536)`。

固定 record 使用 `frame_size`；bounded record 使用其 `max_frame_length`。若无法从已验证 Plan
得到正的 `B`，Inspect 不可执行并报告内部契约错误。

同时应用两层硬预算：

- 解码后的 frame：`1..B` bytes；第 `B + 1` 个 byte 不得分配或传入 Core。
- 原始编辑文本：最多 `3 * B` 个 UTF-8 bytes；用于约束空白放大，不承诺接受任意数量或布局的
  空白。

parser 应单次线性扫描。decoded byte vector 的逻辑大小与 reserve 请求不超过 `B`；canonical Hex
string 的逻辑大小与 reserve 请求不超过 `2 * B`，二者合计 payload 请求不超过 `3 * B`。标准库
capacity 舍入、allocator bookkeeping、输入 `string_view` 和返回对象本身不计入该 payload 数值，
不得把“byte buffer 不超过 B”误写成 parser 总内存不超过 B。不得先按无限输入完整分配再检查。
UI 字符串转 UTF-8 前也应有等价的有限检查，避免仅在转换后才建立预算。

### 5.3 无效输入偏移

诊断至少携带：稳定的 UI diagnostic id、简短原因、`input_offset`。`input_offset` 定义为原始
UTF-8 文本中的 zero-based byte offset；因合法 alphabet 全为 ASCII，它对合法前缀是稳定的。
UI 可派生 one-based line/column，但不得用 line/column 替代原始 offset。

推荐偏移规则：

- 非法字符：该字符 UTF-8 编码的首 byte offset。
- 奇数个 nibble：最后一个未配对 hex digit 的 offset。
- 全空白或空文本：offset `0`。
- 原始文本超预算：第一个越过 `3 * B` 限制的 byte offset，即 `3 * B`。
- 解码 byte 超预算：将形成 zero-based byte index `B` 的第一个 nibble 的原始 offset。

任何 lexical/budget 失败均保留原始文本和光标定位信息，清除旧 Inspect 结果，并保证
`InspectPipeline` 调用次数为零。

## 6. 状态、revision 与失败清理

### 6.1 推荐状态模型

现有文档 load/close 状态继续有效；结果层新增彼此排他的语义状态即可，不建议把所有组合继续
膨胀为单一 `DocumentState` 枚举：

- active mode：`ENCODE` / `INSPECT`；
- Encode result：none / valid / failed；
- Inspect result：none / valid / failed；
- Inspect failure stage：input / structural-query / codec / materialization。

“failed”是当前尝试的诊断，不是有效 preview。只有 key 与当前 revisions 完全一致的 successful
result 才是 valid。

### 6.2 推荐 key 与 revision

Encode 可继续使用现有 key，但其 `input_revision` 明确只属于 typed drafts。Inspect 建议使用独立
`InspectResultKey`，至少绑定：

- `document_id`
- `load_revision`
- `plan_generation`
- `pipeline_selection_revision`
- `inspect_input_revision`
- `inspect_request_revision`
- Pipeline index + stable id

mode revision 不必写入结果 key：结果按模式分槽，active mode 只决定可见性；若实现继续共用单一
结果槽，则必须额外绑定 `mode_revision`。不得让 Encode 的 Message selection revision 使保留的
Inspect draft 失效，也不得让 Inspect revision 污染 Encode typed draft 的身份。

### 6.3 清理矩阵

| 事件 | Encode draft | Inspect draft | Encode result | Inspect result / failure | 高亮 |
|---|---|---|---|---|---|
| 只切换模式 | 保留 | 保留 | 保留但仅本模式可见 | 保留但仅本模式可见 | 重建为当前模式 |
| 改 Encode 字段 | 更新 | 保留 | 清除 | 保留 | 清除 Encode 高亮 |
| 改 Encode Message | 清空并重建 | 保留 | 清除 | 保留 | 清除 Encode 高亮 |
| 改 Inspect Hex | 保留 | 更新 | 保留 | 清除旧结果与旧失败身份 | 清除 Inspect 高亮 |
| 改 Pipeline | 清空 | 清空 | 清除 | 清除 | 全部清除 |
| 新 Plan 生效/重新 Load | 清空 | 清空 | 清除 | 清除 | 全部清除 |
| Load 失败 | 不可用 | 不可用 | 清除 | 清除 | 全部清除 |
| Close | 销毁 | 销毁 | 销毁 | 销毁 | 销毁 |

每次 Inspect 点击都递增 `inspect_request_revision`，先清除上一轮失败身份和有效结果，再解析输入。
同步实现也要在返回时检查 key；若以后改为异步，旧 completion 只能丢弃，不能覆盖新输入。

## 7. Inspect 执行与 Message 身份

推荐执行顺序固定为：

1. 验证文档 READY、Plan/Bridge 存活且 Pipeline 已选择。
2. 捕获 result key 与当前 Hex draft。
3. 按第 5 节解析并执行预算检查。
4. 仅调用一次 `ExecutionBridge::InspectPipeline(frame, selected_pipeline_id)`。
5. 根据 `ExecutionOutcome.stage`、structural status、codec Result 和 materialization failure 映射 UI。
6. 返回前确认 result key 仍为 current；不匹配即丢弃。

Message 身份规则：

- structural `OK`：只接受 Bridge/Core 匹配得到的 Message；UI 再确认该 Message 属于所选 Pipeline。
- `UNKNOWN_MESSAGE`：Message 显示“未匹配”，不显示字段成功结果。
- `AMBIGUOUS_MESSAGE`：Message 显示“匹配不唯一”，不自行挑选第一项或 Encode 当前项。
- codec 失败：保留已匹配 Message 身份，便于字段级错误定位。
- UI sidecar 无法按稳定 id/index 找到匹配 Message：归为 materialization/internal contract failure，
  不降级成成功。

## 8. 原始输入、有效输出与失败的视觉语义

必须用标签、颜色/图标和可见区域共同区分，不能只依赖颜色：

- `原始输入 / Raw input`：用户可编辑的 Hex，Inspect 成败都保留；它从不等于有效输出。
- `有效编码输出 / Valid encoded output`：仅 Encode `OK` 时存在，只读。
- `有效解码结果 / Valid decoded result`：仅 Inspect `OK` 且 key current 时存在，只读。
- `失败定位 / Failure location`：错误状态的临时标记，不得使用成功色或“已验证”文字。

Inspect 失败时不得呈现上一次成功字段表。Core 的 Decode 是 transactional；即使未来内部返回
部分 slot，也不得把 partial fields 作为有效结果显示。原始输入仍可按失败位置高亮。

## 9. 字段、raw/logical 与失败身份

### 9.1 成功字段表

每行至少绑定而不是按显示文本猜测：

- matched Message stable id + index；
- field stable id + index；
- field kind / wire codec；
- `raw_value`；
- `logical_value`；
- enum known/entry 或 decimal 细节（若适用）；
- 第 10 节定义的物理 byte/bit 映射。

`raw_value` 与 `logical_value` 必须分列。二者字符串相同也不能合并，因为转换字段、enum 与未来
错误分析依赖其不同语义。

### 9.2 失败身份优先级

推荐按以下优先级给出提示和高亮：

1. Hex lexical/budget：原始 `input_offset`，无 Message/field 身份。
2. structural unknown/ambiguous：Pipeline + structural status，无 field 身份。
3. codec 且 `failed_field_id/index` 成对存在：Message + field 身份，并用 sidecar 映射到物理位。
4. `INTEGRITY_FAILED` 且 Message 已知：显示 Message + integrity storage 区域；不伪造 field id。
5. 其他 codec/materialization 失败：Message（若已知）+ record-level status，不猜测字段。

失败状态下点击字段可更新该字段的配置说明，但不得用所选字段覆盖当前失败区域高亮。
特别是 `INTEGRITY_FAILED` 没有 field identity 时，后续字段选择必须继续保留 integrity
storage 高亮，不得因选中配置字段而伪造失败 field identity 或已交付字段值。

若 `failed_field_id` 与 `failed_field_index` 不成对、越界或与 sidecar 不一致，按 internal contract
failure 处理，不采用其中一个“尽量显示”。Schema 0.7 `LENGTH_MISMATCH` 应通过现有失败 field
身份定位 computed-length 字段；只有没有合法 field 身份时才使用 record-level 提示。

## 10. byte offset、bit offset 与 mask 显示

所有位置均相对完整 frame 起点，使用 zero-based 编号。位编号采用 **LSB0**：一个 byte 内最低
有效位为 bit 0，最高有效位为 bit 7。每个物理片段至少显示：

`byte[<frame_byte_index>] mask=0x<两位大写十六进制> bits={<升序 LSB0 位号>} global_bits={<升序位号>}`

其中 `global_bit = frame_byte_index * 8 + bit_in_byte`。示例：

`byte[3] mask=0x2C bits={2,3,5} global_bits={26,27,29}`

显示与高亮必须直接使用 `FieldDescriptor::physical_bits`，不能用 `byte_offset + bit_offset` 在
view 层二次猜测 endian 映射。若字段是整 byte `byte_range`，每个覆盖 byte 视为 `mask=0xFF`；
可额外显示紧凑范围 `[offset, offset + length)`，但不得用范围替代边界 byte 的 mask。

Hex pane 中：

- 全 byte mask `0xFF` 可使用整 byte 高亮；
- partial mask 必须有可见的局部位标识或伴随的精确 mask 文本，不能只把整个 byte 涂色后声称
  已显示精确位；
- 多 byte / 非连续物理位按 `frame_byte_index` 升序展示；
- hover/选择联动必须保持 field stable identity，不按 raw/logical 文本反查字段。

## 11. 单 checkpoint 测试与人工验收

### 11.1 Headless 自动化

至少覆盖：

- Hex parser：大小写、四种空白、规范化、空输入、奇数 nibble、非法字符、`0x`、非 ASCII、
  原始文本预算、decoded-byte 预算及每类精确 offset。
- session：Encode/Inspect 模式独立草稿；模式切换保留；Message 改变不污染 Inspect；Pipeline、
  Load、Close 清空；每种 revision stale-key 丢弃；失败后再成功和成功后再失败不残留状态；非法
  typed edit 的原文本、字段和原因跨模式保留，修正后才重新生成有效 Encode 结果。
- Bridge 使用：选定 Pipeline 内 unique/unknown/ambiguous；证明 Encode 当前 Message 不会传入
  Inspect；Decode integrity、length、conversion 等失败身份按现有能力映射。
- 映射显示模型：byte range、单 byte partial mask、跨 byte/non-contiguous mask、全局 bit offset，
  至少包含 big-endian 和 little-endian fixture。
- Core 未调用断言：所有 lexical/budget 失败，以及缺 Pipeline 状态。

### 11.2 Qt shown-window smoke

对 Schema 0.5、0.6、0.7 各至少一条：选择 Pipeline、粘贴/编辑 Hex、Inspect、验证 matched
Message、raw/logical 列与独立预期 byte/mask。另覆盖 unknown、ambiguous 或可稳定构造的结构失败，
以及一个字段级 codec failure。smoke 必须基于独立预期值，不能从被测模型读回后作为 expected。
CRC/integrity failure 还必须覆盖发布失败后再选择字段，确认 integrity storage 高亮
不被字段详情刷新覆盖。

### 11.3 人工验收清单

1. Encode 与 Inspect 模式及两个草稿的独立性可见且可理解。
2. Inspect 不要求选择 Message；故意让 Encode 停在另一 Message 后仍由结构唯一匹配得到正确项。
3. 非法 Hex 定位到准确字符，修正后旧错误消失。
4. raw input、valid encoded output、valid decoded result、failure location 视觉上可区分，且有文字标签。
5. 点击普通字段、bit field、失败字段时，byte offset、`0xNN` mask、LSB0 bit 与 global bit 一致。
6. CRC/integrity 和 Schema 0.7 length failure 不伪造成功字段。
7. 最大允许输入与刚越界输入行为可控，UI 不冻结、不进行无界分配。

本 checkpoint 的人工验收应明确记录 configuration、fixture、预期与实测。自动 smoke 不替代人工
鼠标/键盘可用性，也不构成性能阈值、硬件或现场验收。

## 12. 文件所有权与实现边界

推荐所有新增实现优先限制在：

- `tools/protocol_lab_ui/document_session.*`：模式、独立草稿、revisions、Inspect result/failure 状态；
- `tools/protocol_lab_ui/` 下新的 headless Hex input parser/model：接受域、预算、offset；
- `tools/protocol_lab_ui/document_tab.*`：控件、标签、交互和可见清理；
- `tools/protocol_lab_ui/field_table_model.*` 及 Hex view：raw/logical/offset/mask 呈现；
- `tests/protocol_lab_ui/`：headless、session、shown-window 与 smoke 覆盖；
- 对应 CMake 注册，仅纳入该 UI checkpoint 的目标和测试。

共享文件所有权：

- `tools/protocol_lab/v06_execution.*`：本方案不修改；复用 `InspectPipeline`。
- `src/protocol_core/*`、`src/config_compiler/*`、`src/protocol_plan/*`：不修改。
- `tools/protocol_lab/exact_value_text_internal.*`：不修改其 canonical Values 语义。
- Schema、CLI、Evidence、stream/network 模块与测试：不修改。

若实现者认为必须改动共享文件，应先给出无法通过 UI-owned adapter 完成的具体证据、最小签名、
调用方影响及回归矩阵，单独确认后再动手。

## 13. 已知风险与未关闭项

- 允许空白提高 paste 易用性，但引入独立于 canonical Values Hex 的接受域；因此 parser 名称、测试
  和 UI 提示必须清楚区分，避免复用时语义漂移。
- UTF-8 byte offset 对 headless 合约稳定，但 Qt 光标使用 UTF-16 index；非法非 ASCII 字符的
  定位需要显式转换，不能直接把两个 offset 混用。
- `INTEGRITY_FAILED` 可能没有 field identity；只能使用 Message integrity storage 元数据，不应
  强行归属某字段。
- 现有 Bridge 的 structural matcher 属于 internal Core query，由 Lab bridge 封装后复用；UI
  不应直接依赖该 internal API。
- 65,536-byte 最大输入下，文本控件替换、语法着色、mask overlay 和首屏 repaint 的性能阈值仍
  未定义、未评估；本契约只要求有界和不冻结，不声称达到具体门槛。
- 最新基础 UI 人工 walkthrough 已通过，但精确 mask 显示尚未独立人工检查；本 checkpoint 必须
  把它作为新验收项，不能沿用旧结论。
- active compilation 时关闭 Tab 的竞态尚未人工演示；本 checkpoint 不应把它误写为已人工通过，
  也不应因 Inspect 为同步路径而宣称该既有生命周期风险已经关闭。
- 本契约没有重新运行 build/test/UI，所有验证要求都是未来 checkpoint 的准入条件。

## 14. 确认门

在实现前，请至少确认第 1 节八项集中决策，特别是：

- Inspect Hex 是否接受四种 ASCII 空白并接受 lowercase；
- raw text 上限是否采用 `3 * B`，decoded 上限是否采用 Pipeline/Plan/Desktop 三者最小值；
- 模式切换保留独立草稿、Pipeline 切换清空两类草稿；
- bit 编号与显示是否采用本文的 zero-based byte + LSB0 + global bit + `0xNN mask`；
- 共享 Bridge delta 冻结为“无”。

确认门已经满足，限定实现与自动化验证已经完成。本文文件名保留 `draft` 以维持既有交付路径；
人工可用性、性能阈值、active compilation 期间关闭 Tab 的人工演示仍未完成，不得从自动化通过
推导这些结论。
