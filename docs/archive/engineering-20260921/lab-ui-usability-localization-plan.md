# Lab UI 可用性与中文化方案

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-19

状态：**设计规格，待 G1 Binary Encode 首片完成并经总控复核后串行实施**

盘点基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`；盘点期间 G1 已开始修改其独占文件，本文不要求 G1 同步布局，也不修改 UI 源码。

## 1. 结论

1. 当前 Lab 已具备字段表、详情、Hex、高亮、Host binding/Flow 和诊断基础，但信息以单页纵向堆叠，
   配置、绑定、操作、输入、结果和诊断的主次不够清楚；多数用户文案仍是英文或中英混排。
2. 第一轮可用性改造应以“**中文优先、稳定技术标识保留、执行状态显式、长诊断进入可展开面板**”为目标，
   不应先建设语言切换器、翻译平台、主题系统或新的 PAE 能力。
3. 推荐保留现有“字段表选择 → Hex 精确高亮”能力，将页面整理为四层：文档与编译、绑定与上下文、
   操作与输入、结果与诊断。颜色只做辅助，所有状态同时有中文文字和图标/形状。
4. 面向用户的按钮、标签、提示和可解释摘要使用中文；协议/字段 ID、Schema version、API 名、稳定错误码、
   canonical 输入、Hex、日志与配置自带文本保持原样。错误显示采用“中文摘要 + 稳定码 + 可展开技术详情”，
   不解析英文 detail 来猜译文。
5. 当前类没有 `Q_OBJECT`，也没有 `QTranslator`、TS/QM 或 `lupdate/lrelease` 接线。首轮建议使用固定
   translation context 的中文源字符串，例如 `QCoreApplication::translate("PaeLabUi", "加载配置")`；
   未安装 translator 时直接显示中文，同时保留以后抽取翻译的入口。暂不引入完整多语言系统。

本文只依据源码、当前工程文档与官方公开资料进行静态设计；仓库内未发现当前界面截图，本轮按约束未启动
用户 Lab，因此不声称已经完成视觉验收。

## 2. 产品边界

- PAE 继续拥有配置编译、冻结 Plan、Codec、Framer、Host 和稳定诊断语义；Lab 只做选择、输入、调用、
  状态映射和展示。
- 不把 Socket、串口、抓包、连接管理、重试、设备线程或业务路由加入 Lab。
- G1 的受限成功展示保持不变：Binary Encode 只展示调用方输入、最终字节、可证明的物理范围和失败事实；
  不伪造 generated raw、conversion trace 或 integrity algorithm/coverage。
- 不读取 private Plan，不重复 Decode/Encode 来补展示，不从 logical 值反算 raw。
- 不覆盖 `deliverables/lab/98df5e0`、`deliverables/sdk/98df5e0` 或 `local_private`；后续 UI 验证使用新的构建/
  部署根。

## 3. 当前界面盘点

以下行号是本轮静态盘点定位；G1 合入后应按符号重新定位，不能按旧行号机械修改。

| 区域 | 当前事实 | 可用性问题 | 源码定位 |
| --- | --- | --- | --- |
| 主窗口 | 标题、File 菜单、空白 Tab 和离线状态栏均为英文 | 首次打开时不知道“离线”意味着不做通信，也没有中文任务入口 | [`application_window.cpp`](../../../tools/protocol_lab_ui/application_window.cpp#L119-L154) |
| 文档入口 | 路径框、Browse、Load/Reload 与 Schema/Protocol 身份纵向排列 | 路径、编译状态和当前生效文档没有形成一个清晰分区 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L2527-L2544) |
| Host | binding 草稿表、Apply、active binding、Flow 与长状态文本同区 | “草稿”“准备中”“已生效”的状态主要藏在英文句子里 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L1885-L1917) |
| 操作选择 | Mode、Pipeline、Message、Representation 和多个按钮挤在同一横行 | 1280 宽下尚可，中文变长或高 DPI 后容易拥挤；操作主按钮不突出 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L2546-L2583) |
| 输入 | complete/stream 共用一个限定高度的输入框 | 输入格式说明中英混排；stream 冻结后只靠只读状态表达 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L2585-L2602) |
| 结果 | 字段表 + 详情水平分割，Hex 在下方，已有字段选择到 Hex 高亮 | 表头、来源、只读原因、详情标签多为英文；“输入值”和“执行结果”容易混列 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L2604-L2630)、[`field_table_model.cpp`](../../../tools/protocol_lab_ui/field_table_model.cpp#L183-L255) |
| 流状态 | phase、buffer、work、candidate 等十余项拼成两行英文文本 | 状态可复制但难扫描；候选、业务输出、暂停/需重置不突出 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L2971-L3017) |
| 诊断 | 页面底部一个自动换行 `QLabel` 显示 `diagnostic_id: detail`，下接 timing | 长诊断会挤压主内容；结构化 compile diagnostic 未完整进入可导航视图 | [`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L2632-L2638)、[`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L3020-L3025) |
| Hex | 16 字节/行、Offset 列、选中字段的 byte mask 高亮 | Tooltip 与 Offset 英文；固定 36 px 列宽需在中文字体/DPI 下复核 | [`hex_view.cpp`](../../../tools/protocol_lab_ui/hex_view.cpp#L17-L82)、[`hex_view.cpp`](../../../tools/protocol_lab_ui/hex_view.cpp#L130-L142) |

### 3.1 当前英文文案面

需要中文化的用户显示文本主要包括：

- 窗口、菜单、文件对话框、Tab、离线边界和“两文档上限”；
- `Mode/Pipeline/Encode Message/Representation`、`Browse/Load/Reload/Encode/Inspect/Continue/Reset`；
- Host 草稿表头、添加/删除/应用、候选准备、发布、取消、当前 Session/Flow 状态；
- 字段表 `Id/Name/Type/Source/Value/Raw/Logical/Physical...`，只读原因和 canonical 输入错误；
- 结果条、stream 状态、字段详情、动态范围、integrity/computed/conversion 注释；
- 对话框标题、确认问题、输入容量提示和当前可恢复操作建议。

不可机械翻译的内容见第 8 节。尤其 `OK`、`INVALID_ARGUMENT`、`UI_INSPECT_HEX_EMPTY`、
`TX_TEMPLATE`、`schema_version`、Pipeline/Message/Field ID 不能直接替换为中文字符串。

### 3.2 自动化耦合

当前仅 Host 五个控件设置了稳定 `objectName`：`hostBindingDraft`、`hostApply`、`hostBinding`、
`hostFlow`、`hostStatus`。其余 smoke 路径还会比较 `Submit chunk`、`Actual byte range`、
`Integrity storage` 等可见文本，或搜索英文诊断片段。见
[`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L859-L974) 和
[`document_tab.cpp`](../../../tools/protocol_lab_ui/document_tab.cpp#L1178-L1352)。

因此不能全局替换 `QStringLiteral` 后再逐个修测试。应先为关键控件增加稳定 `objectName`，让自动化按
对象、枚举 `itemData`、model role 和稳定诊断码定位；可见文案只用于面向用户的语义断言。

## 4. 官方工具参考与取舍

### 4.1 Wireshark：字段与字节联动

官方 User’s Guide 描述了 Packet Details 字段树与 Packet Bytes 十六进制区：选中字段会在字节区高亮，
generated fields 用方括号与真实包内字段区分。

- 直接来源：[Wireshark - Packet Details](https://www.wireshark.org/docs/wsug_html_chunked/ChUsePacketDetailsPaneSection.html)、
  [Packet Bytes](https://www.wireshark.org/docs/wsug_html_chunked/ChUsePacketBytesPaneSection.html)
- 可借鉴：字段/详情选择与 Hex 高亮保持联动；生成字段使用明确徽标或“由引擎生成”说明；偏移、Hex、
  ASCII 视图保持稳定技术格式。
- 不适用：Packet List、抓包、显示过滤器、重组数据源、网络协议层级和 Capture 状态都不是离线 Lab 职责。

### 4.2 Qt Creator：结构化问题视图

Qt Creator 官方 Issues 文档强调按类型过滤问题、搜索、选择一项查看详情，并能跳转到对应位置；它把
结构化问题与原始输出分开，而不是把全部内容塞进状态栏。

- 直接来源：[Qt Creator - View issues](https://doc.qt.io/qtcreator/creator-reference-issues-view.html)
- 可借鉴：诊断列表保留 severity/stage/code/location 摘要，选中后显示 detail、资源上限和技术原文；
  允许复制稳定码与完整详情。
- 不适用：build output parser、源码跳转、Clang 诊断和在线搜索不进入 Lab 首轮。

### 4.3 010 Editor：结果表与 Hex 选择

010 Editor 官方手册的 Template Results 以 Name/Value/Start/Size/Type 等列展示结构化变量，选择变量会
定位对应字节；不可编辑值不提供编辑入口。

- 直接来源：[010 Editor Manual - Working with Template Results](https://www.sweetscape.com/010editor/manual/TemplateResults.htm)
- 可借鉴：字段结果列与物理起点/长度并列；选择行联动 Hex；可编辑与只读在控件形态上区分，而不是
  仅依赖错误提示。
- 不适用：Binary Template 语言、模板运行/调试、文件直接编辑和仓库下载不属于配置驱动的 PAE Lab。

这三项只提供交互模式参考，不作为视觉复制目标，也不证明 PAE/Lab 应获得对应工具的全部能力。

## 5. 推荐信息架构

### 5.1 主次分区

```text
┌ 文档与编译 ──────────────────────────────────────────────────────┐
│ [配置路径........................] [浏览] [加载/重新加载]          │
│ Schema / Protocol / 文件名                 [当前状态徽标]          │
└───────────────────────────────────────────────────────────────────┘
┌ 绑定与上下文（Host 文档可见，可折叠）────────────────────────────┐
│ 绑定草稿表                       [添加] [删除] [应用绑定]          │
│ 当前绑定 [....]   流编号 [0/1]   [草稿/准备中/已生效徽标]         │
└───────────────────────────────────────────────────────────────────┘
┌ 操作与输入 ──────────────────────────────────────────────────────┐
│ 操作 [解析/组包/流式解析]  执行流 [...]  消息 [...]  输入格式 [...]│
│ [原始输入或数据块；组包模式下由字段表提供调用方输入] [主操作按钮]│
└───────────────────────────────────────────────────────────────────┘
┌ 结果状态条：图标 + 状态 + 一句话摘要 + 稳定码（若有）────────────┐
├ 字段与结果 ───────────────────────┬ 详情 / 诊断 / 流状态 ────────┤
│ 字段表：名称、类型、值来源、      │ 当前字段说明                  │
│ 调用方输入、原始值、逻辑值、位置  │ 结构化诊断及完整技术详情      │
│                                   │ candidate / business 计数      │
├───────────────────────────────────┴───────────────────────────────┤
│ Hex：Offset + 16 字节；选中字段联动高亮；完整帧可复制             │
└───────────────────────────────────────────────────────────────────┘
状态栏：离线模式｜不执行通信｜当前文档数（不承载长错误）
```

实现上可沿用当前两个 `QSplitter`：字段表留在左侧，右侧由单一 `QTextBrowser` 改为
`QTabWidget`（字段详情/诊断/流状态），Hex 仍在下方。诊断页必须可滚动、可复制、可被 splitter 扩展；
顶部结果条只显示短摘要，不复制整段 detail。

### 5.2 操作区规则

- `操作` 决定页面任务：`解析（Decode）`、`组包（Encode）`、`流式解析`。首次出现可括注英文，按钮本身
  使用短中文。
- `执行流` 对应 Pipeline；`流编号` 专指 Host Flow 0/1，避免两个概念都译成“流”。
- `消息` 只在当前 action 需要 selector 时启用；显示名优先，ID 作为次级文本或 Tooltip。
- `输入格式` 对应 Hex / ASCII escaped；canonical 技术格式名保留英文，外围说明用中文。
- 主操作按钮同一时刻只突出一个：`解析完整记录`、`组包`、`提交数据块` 或 `继续处理`；`重置流状态`
  是危险的次级动作，紧邻流状态而不是与主操作混成按钮墙。

### 5.3 字段、详情与 Hex

- 字段表建议列：`字段 ID`、`名称`、`类型`、`值来源`、`调用方输入`、`原始值`、`逻辑值`、
  `物理位置（字节/位/掩码）`。
- 组包时只为 `CALLER_INPUT` 提供 editor；`CONSTANT/COMPUTED/NOT_REFERENCED` 使用只读外观和中文说明，
  不等用户编辑后才报错。
- G1 成功后，调用方输入列可以显示提交值，但 generated raw/logical 保持空白，并显示“未由公开结果观察”，
  不能填入猜测值。
- 选择字段继续驱动 Hex 精确高亮；选中 Hex 反查字段可作为后续增强，只有在公开物理映射唯一且不引入
  新协议解释时才做。
- 长路径、长 ID 和长中文说明在单行区使用 `QFontMetrics::elidedText()`，Tooltip/详情保留完整值；路径和
  ID 优先中间省略，普通说明优先右侧省略。Qt 官方定义按像素宽度而非字符数省略：
  [QFontMetrics::elidedText](https://doc.qt.io/qt-6.8/qfontmetrics.html#elidedText)。

## 6. 状态识别规格

状态不能只靠颜色；每种状态同时包含中文文本、固定图标/符号和可复制技术信息。

| 状态 | 建议文案 | 视觉语义 | 必须说明 |
| --- | --- | --- | --- |
| 未加载/就绪 | `未加载配置` / `已就绪` | 灰色圆点 / 蓝色信息图标 | 是否已有 compiled document |
| 草稿 | `绑定草稿·尚未应用` | 铅笔图标、琥珀色轮廓 | 当前执行仍使用旧的已生效绑定 |
| 准备中 | `正在准备候选会话…` | 进度指示、蓝色 | active Session 未替换 |
| 已生效 | `绑定已生效` | 实心蓝色徽标 | binding、Flow、generation |
| 成功 | `解析成功` / `组包成功` | 勾号、绿色 | Message、字段数或输出字节数 |
| 失败 | `解析失败` / `组包失败` | 叉号、红色 | 稳定码、失败字段/value、旧成功结果已清除 |
| 候选 | `已形成候选记录` | 空心菱形、蓝灰色 | **不等于** Decode 成功或业务输出成功 |
| 暂停 | `已暂停·可继续处理` | 暂停符号、琥珀色 | stop reason、未消费字节、是否有 internal work |
| 需重置 | `流状态异常·需要重置` | 重置图标、红色轮廓 | 旧 handle 不可继续，重置后重新 Find |

stream 面板把密集文本拆为三组：`当前阶段`、`最近一步`、`累计计数`。`候选数`、`解析成功数`、
`业务输出数`必须分列；`bytes_consumed`、`frozen cursor/total`、`work used/limit`保留技术名或 Tooltip，
避免将一次 `Push` 完成误写成整个输入处理完成。

## 7. 中文术语表

| 技术词 | 用户界面统一译法 | 保留规则 |
| --- | --- | --- |
| Decode | 解析 | 首次说明可写“解析（Decode）”；稳定 action/日志仍用 `DECODE` |
| Encode | 组包 | 首次说明可写“组包（Encode）”；稳定 action/日志仍用 `ENCODE` |
| Inspect | 解析输入 / 查看结果 | 不译成“检测”；按钮按 complete/stream 写具体动作 |
| Pipeline | 执行流 | ID 原样；不与 Host Flow 混称“流编号” |
| Flow | 流编号 | 显示 `Flow 0/1` 可写为“流编号 0/1” |
| Message | 消息 | display name 可显示，Message ID 原样 |
| Field | 字段 | Field ID 原样 |
| Binding | 绑定 | 区分“绑定草稿”和“已生效绑定” |
| Action | 操作 | 技术详情保留 `DECODE/ENCODE` |
| Representation | 输入格式 | 选项保留 `Hex`、`ASCII escaped` |
| Complete record | 完整记录 | 不写成“完整帧成功”，除非执行确实成功 |
| Stream chunk | 流输入块 / 数据块 | 主按钮使用“提交数据块” |
| Continue | 继续处理 | 只在 suffix/internal work 允许时启用 |
| Reset | 重置流状态 | 明确会清除的 Flow 状态 |
| Candidate | 候选记录 | 不等于有效解析结果或业务输出 |
| Business output | 业务输出 | 与候选观察计数分开 |
| Draft | 草稿（未应用） | 不与 active state 混合 |
| Active | 已生效 | 显示 session/binding/flow identity |
| Raw | 原始值 | 只有公开执行结果实际提供时显示 |
| Logical | 逻辑值 | Decimal64 保持 coefficient/scale 精度 |
| Physical location | 物理位置（字节/位/掩码） | 保留 zero-based、LSB0 技术说明 |
| Diagnostic | 诊断 | 稳定 code 原样，中文只解释含义 |
| Integrity storage | 完整性存储位置 | 不扩写成算法、覆盖范围或独立复算通过 |
| Computed length | 自动生成长度 | 说明由 PAE 生成且只读 |

## 8. 中文化边界与动态文本策略

### 8.1 应翻译

- 固定窗口标题、菜单、按钮、分组标题、表头、Tooltip、确认对话框；
- Lab 自己产生的状态摘要、输入语法解释、恢复建议；
- 已知稳定状态的短解释，例如 `UI_INSPECT_HEX_EMPTY` → “请输入至少一个完整字节”。

### 8.2 保持原样

- Protocol/Pipeline/Message/Field/Enum ID、`schema_version`、`source_ref`；
- 用户配置提供的 `display_name`、`description` 和枚举显示名。它们是用户数据，不经过翻译表；
- `CompileDiagnostic.code`、Codec/Host/Framer status、Lab `UI_*` 稳定错误码；
- API/类型名、canonical `true/false`、`coefficient@scale`、Hex、ASCII escaped、路径；
- 日志、smoke/performance 输出和机器解析标记。

### 8.3 组合显示

- 选择器：`显示名  [id]`；只有 ID 时直接显示 ID。
- 错误条：`组包失败：字段值不可表示  [VALUE_NOT_REPRESENTABLE]`。
- 诊断详情：分别显示“中文摘要”“稳定码”“阶段”“JSON Pointer/字节偏移”“资源 required/limit/profile”
  和“技术详情原文”。没有公开字段时不补猜测值。
- 不通过匹配英文 `detail` 内容生成中文。中文摘要应由稳定 enum/code 的显式 mapping 产生；未知码显示
  `未知诊断（原始码：...）`，技术详情照常可复制。
- PAE public diagnostic 在 worker 已保留结构化对象但 Session 当前主要发布 detail；结构化诊断分片应
  复制公开字段到 Lab-owned DTO，不改变 PAE API。

## 9. 直接中文字符串与完整多语言系统

| 方案 | 成本 | 优点 | 风险/限制 | 本轮建议 |
| --- | --- | --- | --- | --- |
| 直接 `QStringLiteral("中文")` | 最低 | 改动小；当前 `/utf-8` 已在开发树和 standalone MSVC 选项中启用 | 不可由 `lupdate` 抽取；以后多语言要再改源码 | 不作为统一写法 |
| 固定 context 的中文 `translate()`/`tr()` 源字符串 | 低 | 无 QM 时直接显示中文；可逐步抽取；稳定 context 便于后续迁移 | 当前类无 `Q_OBJECT`，裸 `tr()` context 易落到基类；动态切换仍需额外代码 | **首轮推荐** |
| 完整 `QTranslator + TS/QM + Linguist + 语言切换` | 中高 | 真正支持多语言、翻译审校和运行时选择 | 增加 CMake/部署/资源/回退/LanguageChange/测试矩阵；当前没有第二语言需求 | 本轮不建设 |

Qt 官方将 internationalization 定义为无需工程改动即可适配语言，将 localization 定义为加入具体地区/
语言内容；`QTranslator`、Linguist 和 `lupdate/lrelease` 是完整链路。参考
[Internationalization with Qt](https://doc.qt.io/qt-6/internationalization.html) 和
[Qt Linguist Manual](https://doc.qt.io/qt-6/qtlinguist-index.html)。

首轮建议增加一个轻量 UI 文案入口（可在现有文件内先使用
`QCoreApplication::translate("PaeLabUi", "中文源文案")`），不增加 TS/QM、语言菜单或运行时切换。
若以后确认英文 UI 是产品需求，再单独冻结 source language、translation context、资源路径、缺失翻译回退
和部署契约；不要在本次中文化中顺带建设。

## 10. Qt 5.13 字体、DPI 与截断

当前窗口固定初始大小 `1280x820`，Host 表和输入框有固定最大高度，Hex 字节列固定 `36 px`；源码没有
显式字体、`AA_EnableHighDpiScaling`、`AA_UseHighDpiPixmaps` 或翻译加载。后续应：

1. 使用系统 UI 字体和 Qt style 的 size hint，不硬编码微软雅黑等本机字体；技术值可使用系统等宽字体，
   但必须验证中文 fallback 不出现方框。
2. 在 Qt 5.13 路径核对是否需要在 `QApplication` 构造前设置 `Qt::AA_EnableHighDpiScaling` 和
   `Qt::AA_UseHighDpiPixmaps`；不得按 Qt 6 默认行为直接推断。Qt 官方 High DPI 文档说明 Widgets 使用
   device-independent pixels，且 Qt 5 的高 DPI 行为依赖 opt-in：
   [Qt High DPI](https://doc.qt.io/qt-6.10/highdpi.html)。
3. 不按中文字符数估算宽度；使用 `QFontMetrics::horizontalAdvance/boundingRect/elidedText` 和实际
   `devicePixelRatio`/style metrics。Hex 每字节列至少容纳 `FF` 加 cell padding，不能简单把 36 乘缩放率。
4. 路径/ID 用中间省略并提供完整 Tooltip/复制；普通说明允许换行或右省略；按钮优先短词，不用
   “执行当前选择的完整记录解析”这类长句。
5. splitter 位置、表头和详情面板在 100%、125%、150%、200% 缩放下不得让主操作、稳定码、失败字段
   或流重置动作不可见；至少保证键盘 Tab 顺序可到达。
6. 红绿状态同时使用图标/文本；高亮色需与字段失败红、Hex 选择黄区分，并在 Windows 高对比主题下
   保持可辨识。

## 11. 自动化兼容规则

1. 保留现有五个 Host `objectName`，新增控件采用稳定英文机器名，例如：
   `configPath`、`loadConfig`、`operationMode`、`pipelineSelector`、`messageSelector`、
   `inputRepresentation`、`primaryAction`、`resultStatus`、`diagnosticView`、`fieldTable`、`hexView`。
2. 测试按 `objectName`、enum/itemData、model role、稳定错误码和状态 DTO 断言；不靠中文或英文按钮文本
   找控件。
3. `objectName`、diagnostic ID、JSON key、日志 token 永不翻译。`accessibleName/accessibleDescription` 可中文，
   但也不作为机器协议。
4. 对确需验证文案的测试，只断言中文摘要与稳定码同时存在，不把完整句号、空格或换行当接口。
5. 将当前 embedded smoke 对 `Submit chunk`、`Actual byte range`、`review kind TX_TEMPLATE` 等英文片段的
   检查改为状态/role/稳定标志；这属于中文化分片的必要兼容修改，不改变执行语义。
6. G1 新增 objectName 或测试约定优先；本方案实施前先读取 G1 最终差异，不覆盖其 typed draft、
   Encode 结果和失败清旧断言。

## 12. 建议实施分片

所有分片均在 G1 完成并经总控复核后串行派发；以下是建议范围，不构成本轮实施授权。

### U1：中文核心操作面与稳定自动化锚点（最高优先级）

目标：不重排大布局，先让首次使用路径全中文可理解，并解除 smoke 对英文可见文本的定位依赖。

- 文件：`application_window.cpp`、`document_tab.{h,cpp}`、`field_table_model.cpp`、
  `exact_value_delegate.cpp`、`hex_view.cpp`；必要时只在 `tests/protocol_lab_ui/CMakeLists.txt` 增加专项。
- 内容：窗口/菜单/配置/操作/输入/表头/只读原因/确认框中文化；增加稳定 objectName；错误显示改为
  中文摘要 + 稳定码，技术 detail 保留；不改 Session/adapter 语义。
- 自动验证：现有 headless 和 Qt smoke D/R；新增控件定位、中文摘要与稳定码并存、英文机器标识未改变。
- 人工观察：1 组，打开一个 Binary 0.9 与一个 ASCII 0.11 合成配置，核对首屏和主操作，不使用私有输入。

### U2：页面分区、状态条和长诊断面板

目标：落实第 5、6 节布局，使草稿/已生效、成功/失败、候选/暂停一眼可辨。

- 文件：`document_tab.{h,cpp}`；若状态 DTO 需要纯 Lab-owned 枚举，限定在
  `owned_presentation_types.h`，实施前避开并复核 G1 最终所有权。
- 内容：QGroupBox/折叠区、右侧详情/诊断/流状态 Tab、顶部短结果条；底部不再放长 `QLabel`；
  status bar 只保留离线边界与全局摘要。
- 自动验证：各 DocumentState/Host publication/stream step 的状态映射；长 detail 不改变主操作可见性；
  失败清旧结果、Flow 隔离和 G1 Encode 结果不退化。
- 人工观察：1 组，在 150% Windows 缩放下观察长路径、长诊断、暂停后继续/重置；不超过一次短流程。

### U3：结构化诊断与字段—Hex 解释增强

目标：把已有 public compile diagnostic 完整带到 Lab-owned 展示，并优化字段表、详情和 Hex 的解释关系。

- 文件：`compile_worker.{h,cpp}`、`document_session.{h,cpp}`、`document_tab.{h,cpp}`、
  `field_table_model.cpp`、`hex_view.{h,cpp}`；不改 PAE public API。
- 内容：stage/code/json pointer/byte offset/resource required/limit/profile 分列；字段选择联动 Hex 与详情；
  generated/只读/未观察使用明确标注；可选的 Hex 反查字段必须以唯一公开映射为前提。
- 自动验证：结构化字段逐项复制、reload/close 陈旧结果拒绝、动态 payload/bit mask 不越界、未知错误码回退。
- 人工观察：通常复用 U2 的第二组，不另开全面 UI 验收。

### U4：完整多语言（条件触发，最低优先级）

仅当用户明确需要中文/英文切换时启动：增加 TS/QM、`QTranslator`、CMake/standalone 部署白名单、
LanguageChange 或重建 UI 策略、缺失翻译回退和双语言自动矩阵。不能把 U1 的中文可用性完成与 U4 绑定。

## 13. 验证矩阵与停点

| 层次 | 最小验证 | 不代表 |
| --- | --- | --- |
| 静态 | 用户文案清单、稳定码/ID 未改、objectName 唯一、无 private include | 界面可用 |
| headless | DTO/state/诊断映射、G1 六类型和失败清旧、ASCII/Binary 隔离 | 字体、DPI、真实绘制 |
| Qt offscreen/smoke | 控件可定位、enabled/visible/read-only、Tab/Flow/reload/close 生命周期 | 人眼布局 |
| 短人工观察 | 最多两组：100% 主流程；150% 长中文/长诊断/暂停恢复 | 发布、硬件、现场或长期稳定 |
| installed-SDK | static/shared D/R 仅在实际共享 UI/CMake 变化后按范围回归 | 稳定 ABI、Linux、正式分发 |

停报条件：

- G1 尚未完成或目标文件仍在并发写入；
- 中文化要求改变稳定错误码、配置 ID、API、Schema 或日志协议；
- 需要通过 private Plan、重复执行或反算 raw 才能满足展示；
- Qt 5.13 中文字体/DPI 问题只能靠修改全局 PATH、本机 Qt 或覆盖用户部署解决；
- 需要新增通信、抓包、模板执行或设备管理能力。

## 14. 本轮未验证

- 未启动 Lab、未读取运行中窗口、未生成截图，未验证当前 1280×820 实际布局。
- 未构建、未运行测试，未验证 Qt 5.13 在 125%/150%/200% 缩放或高对比主题下的绘制。
- 未验证任何真实协议、私有配置、硬件、现场、Linux、稳定 ABI 或正式发布。
- 未修改 UI、CMake、测试、SDK、部署或 G1 文件；本文是 G1 后续串行实施规格，不是实现完成证明。
