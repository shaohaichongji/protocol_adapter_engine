# PAE Lab UI 离线 Inspect 验证记录

日期：2026-09-10

基线：`dc5c4433b51961a9e2d24f425fba672a7121290e`，分支 `feat/lab-ui-c1`

状态：**实现完成；自动化验证通过；人工验收已执行部分步骤，仍未完成**

本文只记录 Schema 0.5/0.6/0.7 离线 complete-record Inspect checkpoint。它不是网络、硬件、
Evidence、Golden、现场、长稳或部署验收记录。

## 实现结果

- 单 Tab 提供显式 Encode/Inspect 模式；typed drafts 与 Inspect Hex draft 独立，模式切换保留，
  Pipeline 切换、重新 Load 和 Close 清空。
- Encode 非法 edit 作为独立 invalid draft state 保留原文本和校验原因，不生成无效 typed value；
  Encode/Inspect/Encode 往返后仍可见。Encode 失败诊断按模式保留，并明确包含 field id 与原因；
  修正字段后清除旧失败和旧结果，再产生新的有效结果。
- Inspect 仅接收一条完整 record，调用现有 `ExecutionBridge::InspectPipeline`；UI 没有把 Encode
  Message selector 传入 Inspect，Message 只来自选定 Pipeline 内的 Core 唯一结构匹配。
- 新增 UI-owned Hex parser：接受大小写 hex 与 SP/HT/CR/LF，拒绝其他字符；按 zero-based UTF-8
  byte offset 报告 empty、invalid character、odd nibble、raw-text budget 和 decoded-frame budget。
- frame budget 为 `min(Pipeline maximum frame, Plan max_frame_bytes, 65536)`；raw text 为 `3*B`。
  parser 单次扫描：decoded byte vector 的逻辑大小/reserve 请求不超过 B，canonical Hex string 的
  逻辑大小/reserve 请求不超过 2B，payload 请求合计不超过 3B；标准库 capacity 舍入和 allocator
  overhead 未计入，不能表述为 parser 总分配不超过 B。Qt 编辑层把保留文本限制在至多
  budget+1 个 UTF-16 code unit，以便保留一次明确的超限诊断而不长期持有无界文本。
- Inspect 成功才发布有效字段；失败清除旧成功结果，不显示 partial fields。codec 失败校验成对的
  field id/index 与 UI sidecar，并显示 status、conversion reason（若有）和 field id。
- Inspect 失败没有当前 field identity 时显式清空字段选择和右侧详情，不依赖
  model reset 触发 `currentRowChanged`；失败有合法 field index 时选中并重建该字段详情。
  详情清理不会改写 failure input frame 或 integrity storage 高亮；后续点击字段也
  复用同一 failure-region 优先级，不会用字段配置位置覆盖 CRC storage 高亮。
- UI 以文字标签区分 Raw input、Valid encoded output、Valid decoded result 和 Failure location。
- 字段表分列显示 raw/logical，并显示 zero-based `byte[n]`、`0xNN mask`、LSB0 bit 与 global bit；
  Hex 高亮直接使用已解析的 physical mapping。CRC integrity failure 使用 integrity storage 区域，
  不伪造 field identity。
- 未修改共享 `tools/protocol_lab/v06_execution.*`、canonical Values Hex parser、Core、Compiler、
  Plan、根 CMake、README 或共享索引。

## Windows 构建环境

- Generator：Visual Studio 18 2026，x64。
- Toolset：`v142,version=14.29.30133`；CMake 观察到 `cl` 19.29.30159.0。
- Qt input：5.13.0，沿用既有依赖；没有把 Qt 复制进源码目录。
- Build tree：`out/final-ui-v142`。

## 自动化结果

Debug 与 Release 均完成以下定向目标构建：UI executable、description mapping、compile queue、
document state、dual-tab isolation、mailbox ownership 和新增 inspect state 测试。

| Configuration | Protocol Lab UI tests | Headless | Shown-window Qt smoke | Result |
|---|---:|---:|---:|---|
| Debug | 8/8 | 6/6 | 2/2 | PASS |
| Release | 8/8 | 6/6 | 2/2 | PASS |

新增或扩展的自动覆盖包括：

- Hex 大小写、四种空白、canonical uppercase 输出、空输入、非法字符、`0x`、非 ASCII、奇数
  nibble、`3*B` 文本预算、frame byte 预算及精确 offset。
- Schema 0.7 success 与 `LENGTH_MISMATCH`，失败身份为 `record_length`，失败不保留成功结果。
- 两 Message fixture 中 Encode selector 停在 `message_a`，Inspect `B2 07` 仍匹配 `message_b`。
- UI structural failure mapper 对 `AMBIGUOUS_MESSAGE` 保持 failure-only，不生成 Message/field 身份。
- 模式切换保留两类草稿和各自结果，Pipeline 切换清空两类草稿与失败状态。
- 实际 widget/model 路径输入 `Count=01` 后保留文本和 canonical UINT64 原因，模式往返不丢失；
  Encode 诊断包含 `field=count` 与具体原因，改为 `1` 后旧失败不复用并产生新有效结果。
- public Schema 0.5 fixture 含两个真实 Pipeline；先形成 `AA:00` Inspect failure，再切换下拉选择，
  断言 raw input、result、failure identity、Hex 和底部 diagnostic 全部清空。
- lexical/budget 失败通过 `ExecutionObserver` 精确断言零 phase callback，即 Bridge/Core 未被调用；
  observer 在 structural phase 改写 Inspect draft 时，旧 result key 被判 stale 且 completion 不发布。
- Inspect success/failure 在重新 Load 和 Close 后均清除。
- ordinary byte、跨 byte bit field 的 exact `0xNN` mask、LSB0 bit 与 global bit 文本。
- shown-window Schema 0.5/0.6/0.7 success；lexical offset=2 后恢复；Schema 0.5 structural
  `UNKNOWN_MESSAGE`；Schema 0.6 `INTEGRITY_FAILED` 两 byte 区域，且后续点击字段后 byte 9/10
  `mask=0xFF` 仍保持；Schema 0.7 字段级 length failure。
- shown-window 在成功 Inspect 选中字段后进入 `AA:` 和 `UNKNOWN_MESSAGE` 等无字段失败，
  断言 current index、selected rows 和右侧详情均清空；`LENGTH_MISMATCH` 则断言当前字段
  为 `record_length` 且详情由当前 model 重建。

shown-window smoke 会创建并显示 Qt window、运行事件循环并程序化驱动 model/widget。它不是人工
鼠标、键盘、焦点、可读性或视觉审美验收。

## 保留日志

- `out/final-ui-v142/logs/pae-ui-inspect-debug-build.log`
- `out/final-ui-v142/logs/pae-ui-inspect-debug-registration.log`
- `out/final-ui-v142/logs/pae-ui-inspect-debug-tests.log`
- `out/final-ui-v142/logs/pae-ui-inspect-debug-smoke.log`
- `out/final-ui-v142/logs/pae-ui-inspect-release-build.log`
- `out/final-ui-v142/logs/pae-ui-inspect-release-registration.log`
- `out/final-ui-v142/logs/pae-ui-inspect-release-tests.log`
- `out/final-ui-v142/logs/pae-ui-inspect-release-smoke.log`
- `out/final-ui-v142/logs/pae-ui-inspect-pre-fix-debug-tests.log`（修复前失败复现）
- `out/final-ui-v142/logs/pae-ui-inspect-details-pre-fix-debug-smoke.log`（详情残留修复前窗口复现）
- `out/final-ui-v142/logs/pae-ui-inspect-details-debug-smoke.log`（详情清理 Debug 定向回归）
- `out/final-ui-v142/logs/pae-ui-inspect-details-release-smoke.log`（详情清理 Release 定向回归）
- `out/final-ui-v142/logs/pae-ui-inspect-crc-click-pre-fix-debug-build.log`（CRC 点击回归修复前构建）
- `out/final-ui-v142/logs/pae-ui-inspect-crc-click-pre-fix-debug-smoke.log`（CRC 点击回归修复前失败）
- `out/final-ui-v142/logs/pae-ui-inspect-crc-click-debug-build.log`（CRC 点击回归 Debug 构建）
- `out/final-ui-v142/logs/pae-ui-inspect-crc-click-debug-smoke.log`（CRC 点击回归 Debug 通过）
- `out/final-ui-v142/logs/pae-ui-inspect-crc-click-release-build.log`（CRC 点击回归 Release 构建）
- `out/final-ui-v142/logs/pae-ui-inspect-crc-click-release-smoke.log`（CRC 点击回归 Release 通过）

这些日志位于忽略的 build tree，不是源码交付文件。

修复前 shown-window 复现中，Schema 0.5 明确失败为：
`Count=01 or its reason was lost across Encode/Inspect/Encode`。Pipeline 清理遗漏同时由修复前源码
确认：`SelectPipeline` 清除 Inspect outcome 后没有清除共享 `diagnostic_id/detail`；最终测试新增真实
双 Pipeline widget 切换断言，不以再次 Inspect 覆盖旧状态。

右侧详情修复前的 shown-window 断言复现为：
`Inspect failure without field identity retained stale selection/details`。修复后 Debug/Release
定向 shown-window 回归同时保留 Schema 0.6 integrity storage 高亮与 Schema 0.7 已知失败字段详情。

CRC 字段点击修复前的 shown-window 断言复现为：
`integrity storage highlight changed after field selection`。本轮将初次失败与字段详情刷新改为
共用同一 failure-region 优先级；Debug/Release 各 2/2 shown-window PASS。该结果是自动回归，
不改写下文既有人工记录。

## 已完成的人工观察（修复前构建）

用户已在 Schema 0.7 UI 完成以下操作与观察；其中详情残留是本轮修复的人工复现，
不能写为修复后人工 PASS：

- `aa 00 06 05 7e 55` 成功 Inspect，观察到 record_length=6、value=5、payload=`7E`。
- `AA` 返回 `UNKNOWN_MESSAGE`，无 field identity，原始 Hex 保留 `AA`。
- `AA:` 返回 `UI_INSPECT_HEX_INVALID_CHARACTER`、`input_offset=2`，无 field identity、
  无可视 Hex；修复前右侧仍残留旧 `Record length`/物理位置详情。
- `AA 00 05 00 00 55` 返回 `LENGTH_MISMATCH`，field=`record_length`，对应行红色、
  byte 1/2 高亮，raw/logical 不伪造结果。
- 恢复 `AA 00 06 05 7E 55` 后正常解码并清除旧错误；点击 payload 时仅 byte 4
  高亮，详情为 mask `0xFF`、global bits 32..39。
- Inspect -> Encode -> Inspect 往返后，用户确认 Inspect 文字与结果保留。

人工截图证据由用户提供，附件 ID：`af02bce6-50a6-4354-8659-7295fb164684`、
`47349f85-0096-4594-903b-2af9cc45bc16`、`136ac686-3fd3-4604-ad81-dde570621a71`、
`73a3ee37-52c6-4621-a5b4-a95acd829da9`、`dc1a174e-78a1-48e6-b479-4e7eeab59d74`、
`f1ec7ded-be0b-4bf9-9055-e01293f715a6`。

## 修复后针对性人工复验

用户已针对“Inspect 失败无 field identity 时右侧残留旧字段详情”完成修复后复验：

- Schema 0.7 Inspect 输入 `AA:`，显示 `UI_INSPECT_HEX_INVALID_CHARACTER`、
  `input_offset=2`。
- 字段表和 Hex 区域为空，右侧只显示灰色占位提示，不再残留旧字段详情。
- 截图附件 ID：`8d907a9a-7e7c-4f7f-87b7-1f712aaa040c`。

结论：**该具体详情残留修复项人工 PASS**。该结论不扩展为整个 Inspect checkpoint
的完整人工验收，也不替代下列尚未执行的人工项。

## 待总控安排的人工验收

已完成项见上述两节；尚未完成的原有人工项如下，建议使用 Release executable
和对应 public fixture 逐项记录实际观察：

1. 打开 Schema 0.5，分别建立 Encode/Inspect 草稿，确认两份草稿独立；切换 Pipeline 或
   Reload 后确认两类草稿清空。
2. 在 Inspect 粘贴 lowercase 且混合空格/Tab/换行的完整帧，确认匹配 Message、raw/logical
   列及“Valid decoded result”标签；确认 Encode Message 控件禁用且不能决定 Inspect Message。
3. 选择 Schema 0.5 的 `enabled`、`cross_bits`、`msb_bits`，逐项核对详情与表格中的 zero-based
   byte、`0xNN` mask、LSB0 bits、global bits 以及 Hex 高亮。
4. Schema 0.6 输入 CRC 被改坏的 12-byte record，确认显示 `INTEGRITY_FAILED`、只高亮 CRC
   storage 两 byte、不出现伪造 field id、不显示旧成功字段。
5. 检查最大允许输入及刚超限输入的响应性和诊断；该步骤只形成当前机器上的人工观察，除非
   另行批准阈值与测量方法，否则不形成性能 PASS。

## 未验证与剩余风险

- 已完成上述部分 Schema 0.7 人工步骤，且右侧详情残留修复项已针对性人工 PASS；
  该结论不外推。Schema 0.5/0.6 步骤和完整精确 mask 可读性仍为
  `MANUAL_UNVERIFIED`。
- 未定义或验证最大输入的性能阈值；自动 smoke 的耗时不是性能验收。
- active compilation 期间关闭 Tab 仍没有人工演示；既有自动生命周期测试不能替代该人工场景。
- 未运行网络、外部真实协议、硬件、Evidence、Schema 0.8、stream 或全项目测试矩阵。
- Schema 0.5/0.6/0.7 Compiler 会拒绝同一 Pipeline 内 matcher 相交的 Plan，因此无法用合法 UI
  fixture 动态构造 Pipeline-local `AMBIGUOUS_MESSAGE`；本轮直接覆盖了 UI failure-only 映射，但
  没有为此修改共享 Bridge 或启用其 test hook，不能声称完成 Bridge 到 UI 的端到端动态覆盖。
- 本轮没有 Stage、Commit、Push、Pull、Fetch、Reset、Clean、Stash、切分支或清理生成物。
