# Lab ASCII 离线接入契约

日期：2026-09-12。
状态：六项方向已由用户确认；本文件细化接入接口、输入语法、分工与验收。
当前进展：独立授权的内部适配与UI接线已实现，Windows Debug/Release自动验证及限定人工复核
已完成，详见[第2阶段验证报告](lab-ascii-ui-stage2-validation.md)。基线整合处于未提交merge状态，
尚未生成合并提交或推送本检查点；原merge自动暂存内容保留，后续增量尚未主动Stage。
以下基线及实施次序保留契约冻结时语境；本文件不是 Git 写操作授权。

## 1. 基线与依赖

- PAE：`main@256c5b7318f73effe7834800dc2c14f4a9466f7b`，ASCII 完整记录已提交推送。
- Lab：`feat/lab-ui-c1@8bfe50fcb71a2e572bdc907b7392803038e6afdb`，BYTES 标准编辑提交修复
  已提交推送，但尚未合入 main。实施前重新核对实际 HEAD、工作树和 ancestry。
- Lab 依赖 PAE；Core 不依赖 Lab、Qt、设备、网络或 UI。Schema 首先服务 PAE，不增加 UI 属性。
- 继续使用用户指定 DEI 第三方目录中的既有 Qt；当前仓库导入规则为 Qt 5.13.x、x64、
  `v142,version=14.29.30133`，编译器版本约束见 [Qt 导入规则](../cmake/PaeQt513.cmake)。
  通过本地 `PAE_QT_ROOT` 指定，不在通用仓库写入机器绝对路径，不升级 Qt、不获取新三方库。
- [ASCII 引擎契约](ascii-text-codec-minimal-contract.md)和
  [引擎验证报告](windows-msvc-2026-ascii-text-slice.md)仍是引擎语义与历史证据依据。
  本文不将已有引擎测试视为 UI 接入已通过。

## 2. 功能范围

首轮仅 Schema 0.10 Text-only 配置的离线 Encode / Inspect；旧 Binary 路径保持。
不同 Tab 可以分别加载旧 Binary 或 ASCII 配置，不允许包内混合布局。
每个 Tab 保持独立配置、输入草稿、动作选择、有效结果和修订号。

- Encode 显示所选 TX 模板，只要求该动作实际引用的输入字段。
- Inspect 在所选 Pipeline 中仅考虑具有 Decode 动作的消息，唯一性由 Core 决定；
  禁止使用 Encode Message 下拉选择替代 RX 结构匹配。
- 单向消息清晰标记可用动作。选中无 Encode 动作的消息禁用 Encode；Pipeline 无 Decode
  候选时禁用 Inspect，内部调用仍须返回 `OPERATION_NOT_SUPPORTED`，不能仅依靠按钮保护。
- 纯字面量模板支持零变量操作，零字段成功必须显示“成功，0 个字段”，不能显示为空白失败。
- 本轮不包含发送、接收、TCP、UDP、串口、流分帧、自动运行、定时发送、端点注册或配置编辑器。
- 不接普通 CLI、Result/Record/Event 持久化、Replay/Compare；不定义新 Evidence 版本或指纹。

## 3. ASCII / Hex 输入与显示语法

ASCII/Hex 是 Lab 的输入和显示表示，不是协议编码自动识别。线上编码仍由配置 `encoding=ascii`
决定；Hex 可以构造任意字节用于负例，但不得绕过 Core 的 ASCII 和字段约束。

### 3.1 ASCII escaped

文本模式明确标为 `ASCII (escaped)`，不含外层引号。规则用于 ASCII 字段草稿及 Inspect 完整记录：

| 输入 | 字节 |
| --- | --- |
| 直接输入 U+0020～U+007E，反斜杠除外 | 对应 ASCII 字节；空格保留，不 trim |
| `\\` | `5C` |
| `\r`、`\n`、`\t`、`\0` | `0D`、`0A`、`09`、`00` |
| `\xHH` | 恰好两个大写 Hex 数字，取值 `00`～`7F` |

拒绝孤立反斜杠、未知转义、不足两位的 `\x`、非 ASCII Unicode 字符、实际控制字符和 `\x80`～`\xFF`。
多行报文用可见 `\r\n` 输入，不将编辑框换行或剪贴板换行自动变成协议换行。
`\x414`按`41`与直接字符`4`解析；不使用变长十六进制转义。
显示时反斜杠转为`\\`，四种具名控制字节用具名形式，其余控制字节与 DEL 用`\xHH`；
非 ASCII 原始输入字节仅在标明为诊断预览时显示为`\xHH`，不得声称可在 ASCII 模式重新提交。

必须先验证 QString 字符，再生成字节；不得依赖会把不支持字符替换为问号的隐式转码。
转义解码成功不代表符合字段白名单、长度或终止边界；后者仍由 PAE 校验。
错误携带草稿位置：输入语法错误使用从0开始的 QString UTF-16 code-unit 下标，字节位置另行标注。

### 3.2 Hex 与模式切换

- ASCII 字段的 Hex 表示沿用 BYTES 的大写连续 Hex；Inspect Hex 沿用大小写及 SP/HT/CR/LF 规则。
  不改变旧 Binary 输入语法。
- 合法草稿切换表示时必须保持字节等价；切换不触发 Codec。为避免旧结果混淆，切换清空当前有效结果。
- 语法非法或包含非 ASCII 字节的草稿不能转换到 ASCII；保留原草稿及模式，显示原因，不替换、不丢弃。
- 编辑容量依据现有资源上限和最坏表示膨胀计算，检查乘加溢出；协议长度按解码后字节计。
  容量内保留超协议长度草稿并报错；超编辑容量的键入、粘贴、替换操作整次拒绝，不截取合法前缀。
  具体容量数值由实施记录说明，不得减少原合法输入集合。

## 4. 内部描述与执行适配

不把 ASCII 塞入旧 v06 Result 或旧 review_decode 事件。新增范围受限的非持久化内部适配路径，
旧 Binary 保留原桥。具体类型名和文件名可随现有组织确定，但必须满足以下所有权和字段合同：

- 已准备文档拥有 Plan、Workspace、作者描述。UI 自有 DTO（数据传输对象）复制所需描述，
  包含布局/编码、消息和字段身份、动作可用性、按动作排序的 literal/field、字段长度及控制字节约束。
- wire 与动作语义从 Frozen Plan 读取，作者元数据仍来自既有 Sidecar；不得在 QWidget 内重解析配置。
- 结果包含文档/Plan/选择/输入修订身份、操作、Core 状态、消息身份、自有帧字节和自有字段字节。
  不把借用 ByteView 或 Plan span 交给长期持有的 UI；先检查有效长度，再在 Plan/输入仍存活时复制。
- 零字段结果独立表达执行成功。失败不交付部分有效字段或有效 Encode 输出；原始 Inspect 输入可
  作为明确标注的失败定位材料保留，但不得命名为有效解码结果。
- Encode 只调用一次主 Encode，复用 Core 内部 TX 第二遍复核，不再用 RX 模板 Decode TX。
  显示复核种类 `TX_TEMPLATE`；不伪造独立 Review Decode 调用次数或无法观测的阶段成功。
- Inspect 由 Core 完成结构唯一性和 Decode，不在 Lab 另写一套匹配器或用“第一个成功”消歧。
- 异步配置完成、Tab 关闭、重载、模式/选择/草稿变化沿用修订门禁；过期完成不得覆盖新状态。

## 5. 物理范围与诊断

- 成功结果只显示本次实际字节范围，不把 max length 当成当前长度。零长度字段显示空范围，不高亮邻字节。
- Inspect 字段范围应从成功 Decode 的借用视图与本次原始输入关联，先验证范围再复制。
- Encode 可按成功 TX 模板与本次输入长度累加投影范围，须检查总长度等于有效输出长度；
  这只用于显示，不能代替 Core 的复核或自行宣称 Codec 成功。
- 失败后清除旧动态范围、高亮、Raw/Logical 和旧侧栏；静态约束若保留，必须标注为配置描述。
- Lab 本地输入错误使用 `UI_ASCII_INPUT_INVALID`，容量拒绝使用 `UI_ASCII_INPUT_CAPACITY_EXCEEDED`；
  Core 错误保留原状态名称及已有可用身份，不猜测未返回的字段、偏移或消息。
  `UNKNOWN_MESSAGE`、`AMBIGUOUS_MESSAGE`、`ASCII_CHARACTER_NOT_ALLOWED`、
  `ASCII_TERMINATOR_CONFLICT`、`FINAL_REVIEW_FAILED`、`OPERATION_NOT_SUPPORTED`必须可区分。

## 6. 构建隔离

V10 和 UI 默认关闭。仅当独立 ASCII 适配与 UI 接线完成后，精确允许 V10+UI 构建；
普通 Lab CLI/Evidence 仍须在执行前明确拒绝 0.10，不删除整体门禁后放任旧路径接受。
检查 UI 对旧 Lab 内部目标的传递依赖，确保共存不等于旧 Result 格式支持 ASCII。
所有共享布局宏在库与消费者间一致，避免再次出现结构大小不一致问题。
不得为了 UI 移动产品依赖方向或改变旧 Schema 接受域。

## 7. 验收与证据

一次 Windows Debug/Release 针对性测试覆盖：

1. 独立 RX/TX 字节向量、纯字面量、单向动作和零字段成功；不只测同实现往返。
2. literal、固定/有界/末尾变量、空变量、控制字节、反斜杠、非法转义/Unicode、长度越界与终止冲突。
3. ASCII/Hex 等价切换、非法草稿拒绝切换、非 ASCII Hex 负例、错误位置单位。
4. 实际 QTableView 编辑器 Enter/Tab/FocusOut、键盘/粘贴/替换容量门禁、失败清理与恢复。
5. Tab 隔离、切换/重载/关闭及过期完成拒绝、借用输入销毁后的自有结果。
6. 本次实际范围、零长度不高亮、单向动作禁用、旧 Binary Encode/Inspect 受影响回归。
7. 默认关闭、V10+UI 可用、普通 CLI/Evidence 不接入及相关链接依赖边界。

PAE 适配目标先做一次独立验证；UI 完成后做一次集中集成验证。相同源码、工具链、配置且证据完整
的成功检查不重复执行，扩大回归必须说明受影响原因。不将 Qt smoke、离线向量或人工点击称为网络、
Golden、Linux、硬件、现场或性能证据。

最后人工 UI 验收集中为：ASCII Encode 与 Hex 对照；ASCII Inspect 与字节高亮；非法输入后恢复；
单向/纯字面量和 Tab 隔离。提供准确程序/配置路径，由用户操作截图；除非用户另行接受代理验收，
不得自动宣称人工验收完成。保持用户既有 Qt 与屏幕操作约定。

## 8. 任务分工与顺序

| 顺序 | 负责人 | 交付与停点 |
| --- | --- | --- |
| 0 | 总控 | 冻结本文；在获授权后串行整合 `256c5b7` 与 `8bfe50f`，保留文档草稿和生成物；冲突不得覆盖 |
| 1 | 《子任务推进》 | 内部描述/执行适配、相关 Compiler Sidecar 最小接入、无界面测试与必要构建接线；回传接口清单及证据 |
| 2 | 总控 | 定向复核接口、旧代隔离与所有权后，确认 Lab 使用的实际代码基线 |
| 3 | 《Lab应用推进》 | Qt输入/显示、会话接线、实际范围、窗口测试、专属验证报告；不改Schema或Core语义 |
| 4 | 总控 | 集中检查与人工验收，准备交付；Stage/Commit/Merge/Push 按当次明确授权执行 |

共享顶层 CMake、Compiler/Plan/Core、本文、索引和路线由总控协调，禁止两任务并发写同一文件。
PAE 执行适配阶段 Lab 可只读检查现有入口并提出问题，不提前落盘接线。Lab 阶段不同时推进 PAE 新主线。
若要共享未提交适配实现，先由总控选择单写工作树顺序接管；不得私自 cherry-pick、复制同名修改或
提交中间产物。需要跨工作树移动或提交时单独说明。缺失实际范围能力、工具链不兼容、必须改变
Core/Schema语义或无法隔离旧Evidence时回报，不自行扩大范围。

## 9. 契约冻结时停点与当前进展

契约冻结时仅完成六项方向确认与文档细化，不等于源码已实施或已验证；当时下一步为申请
基线整合和检查点实施授权，再按上表串行派发。

后续已获独立授权并完成内部适配、UI接线、限定修复及验收。第1阶段历史证据见
[内部适配验证报告](lab-ascii-adapter-stage1-validation.md)，当前验收范围见
[第2阶段验证报告](lab-ascii-ui-stage2-validation.md)。当前待Git交付，未结束merge、Commit或Push；
保留原merge自动暂存内容，后续增量尚未主动Stage。网络、普通CLI和Evidence仍不在本检查点范围。
