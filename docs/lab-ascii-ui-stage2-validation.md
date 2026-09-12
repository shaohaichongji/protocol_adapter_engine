# Lab ASCII 离线 UI 接入第2阶段验证记录

日期：2026-09-12。实施工作树：主仓库 `main@256c5b7318f73effe7834800dc2c14f4a9466f7b`，
保留 `MERGE_HEAD=8bfe50fcb71a2e572bdc907b7392803038e6afdb` 的未提交合并状态。
状态：Lab ASCII 离线 Encode / Inspect UI 已接线并完成 Windows 自动化验证；用户已完成首轮
人工检查、四项限定 UI 修复后的针对性人工复核，并确认合法输入恢复正常后关闭软件。保留原merge
自动暂存内容，本阶段增量未主动Stage、Commit、Push；不代表Linux、性能、网络、硬件、Golden、
现场或生产部署验收。

## 实施边界

- `DocumentSession` 按 Schema 分流：0.5～0.8 保留 `v06::ExecutionBridge`，0.10 独占
  `ascii::OfflineAdapter`。`CompiledUiArtifacts` 只被一个后端移动，UI 保留自有描述和结果。
- 新增 `ASCII (escaped)` 纯输入辅助层：先验证 UTF-16 草稿，再生成字节；支持
  `\\`、`\r`、`\n`、`\t`、`\0`、`\xHH`，拒绝实际控制字符、非 ASCII、非法转义和
  `\x80`～`\xFF`，错误位置为从 0 开始的 QString UTF-16 code-unit 下标。
- ASCII / Hex 表示切换只对语法有效且字节等价的草稿生效，并清除旧结果；
  非 ASCII Hex 或语法无效草稿保持原文和原模式。
- 字段编辑容量按最坏表示计算：Hex 为 `2 * max_bytes + 2`，ASCII escaped 为
  `4 * max_bytes + 4`；Inspect Hex 为 `3 * max_record_bytes + 1`，ASCII escaped 为
  `4 * max_record_bytes + 4`。超编辑容量操作整次拒绝，不截断合法前缀；超协议长度但仍在
  编辑容量内的草稿完整保留并报错。
- Encode 只物化当前 TX action 引用字段；Inspect 不使用 Encode Message 选择来消歧。
  按钮可用性和 session 内部拒绝同时实现，单向动作的内部调用仍返回
  `OPERATION_NOT_SUPPORTED`。
- 成功结果使用 adapter 验证后的实际字节范围；零长度范围不高亮。纯字面量
  Encode / Inspect 显式显示“成功，0 个字段”。`TX_TEMPLATE` 只在 Core Encode 成功结果中
  显示，未将早期输入失败呈现为已完成复核。

## 工具链与构建

- Generator：Visual Studio 18 2026，x64。
- Toolset：`v142,version=14.29.30133`；`cl.exe 19.29.30159.0`。
- Qt：通过本地 `PAE_QT_ROOT` 指向已验证的 DEI Qt 根目录，导入检查为 Qt 5.13.0。
- V10+UI 构建目录：`out/build/windows-msvc-lab-ascii-ui`。Debug / Release 均从源码串行
  重建 adapter、Compiler / Plan / Core 消费者、UI 和定向测试，未链接前阶段 MSVC 19.51 产物。

配置核心选项：

```powershell
$RepoRoot = (Resolve-Path '.').Path
$BuildDir = Join-Path $RepoRoot 'out/build/windows-msvc-lab-ascii-ui'
if ([string]::IsNullOrWhiteSpace($env:PAE_QT_ROOT)) {
  throw 'Set PAE_QT_ROOT to the validated DEI Qt root before configuring.'
}
$PaeQtRoot = (Resolve-Path -LiteralPath $env:PAE_QT_ROOT -ErrorAction Stop).Path

cmake -S $RepoRoot -B $BuildDir -G "Visual Studio 18 2026" -A x64 `
  -T v142,version=14.29.30133 `
  -DPAE_QT_ROOT="$PaeQtRoot" `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_PROTOCOL_LAB_UI=ON `
  -DPAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=ON `
  -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON `
  -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON
```

`$RepoRoot` 必须在仓库根目录解析，`$PaeQtRoot` 必须指向已验证的 DEI Qt 根目录；后续
`cmake --build` 和 `ctest` 命令复用上述完整配置生成的 CMake cache，不应把另一构建树的
开关或编译器产物视为可复现前提。

## 自动化结果

Debug 和 Release 分别执行：

```powershell
ctest --test-dir out/build/windows-msvc-lab-ascii-ui -C <Debug|Release> `
  --output-on-failure `
  -R '^pae\.tools\.(protocol_lab_ascii\.adapter|protocol_lab_ui\.)'
```

最终 Debug `15/15 PASS`，Release `15/15 PASS`。覆盖：

- adapter 独立向量、ASCII escaped parser / UTF-16 位置、session schema dispatch、字节等价切换、
  纯字面量零字段、单向动作内部拒绝、实际范围和自有结果。
- 真实 Qt `QTableView` editor 的键盘输入、粘贴、Enter、Tab 和 FocusOut，以及容量整次
  拒绝、失败清理和恢复。
- ASCII 双 Tab（变量模板+纯字面量）、单向动作按钮状态（Decode-only+Encode-only），
  以及旧 Binary Schema 0.5 / 0.7 / 0.8 Encode / Inspect 窗口回归。

日志：

- `out/lab-ascii-ui-configure.log`
- `out/lab-ascii-ui-debug.log`
- `out/lab-ascii-ui-release.log`

V10-off 兼容性：使用同一 Qt / v142 工具链在
`out/build/windows-msvc-lab-ui-v10-off` 构建 Debug UI，未构建 ASCII adapter，手动执行
`--ui-smoke` 的 Schema 0.5 + 0.8 双文档路径，返回 `UI_SMOKE_PASS`。

## 限定 UI 修复与复核

修复前，用户在 Release 可见 UI 中完成了以下人工检查：ASCII Encode 得到
`TX ALICE!Z\r\n`；Inspect `RX ALICE!OK\r\n` 得到 `ALICE` / `OK`；非法 `\q` 被拒绝且
字段、Hex、侧栏清空，恢复合法输入成功；纯字面量 Encode / Inspect 均显示成功 0 字段；
单向按钮禁用和跨 Tab 隔离符合预期。表述切换只确认了最终 ASCII 页已清空结果、侧栏和 Hex，
中间 Hex 状态没有单独截图，本记录不扩大该证据。

上述人工检查同时暴露了四项展示缺陷，随后进行了限定修复：

- ASCII 表格按当前 action 显示 `input`、`decoded` 或 `not referenced`；Decode-only 字段不再
  被 UI 的只读占位值误标为 `constant`，Inspect 的有效 Raw / Logical 结果也不再与 Value 列
  的 Encode 注释冲突。配置中的真实字段定义和 action 引用集合未改变。
- ASCII 侧栏直接复用 adapter 返回并已验证的实际半开字节范围，例如 `[9, 11)`；不再追加
  Binary 静态 bit / mask 格式器产生的 `current range unavailable`。失败仍清除旧动态事实。
- Encode 的 `TX_TEMPLATE` 和计时只在本次有效 Encode 结果存在时显示；Inspect、输入编辑、
  action 模式、消息、Pipeline 或表示切换会清除旧 Encode 计时，不冒称独立 RX Decode 复核。
- ASCII escaped 输入错误显示解析器的真实位置，例如
  `input_utf16_code_unit_offset=11 (zero-based)`；该值是 QString UTF-16 code-unit 下标，不是
  解码后 byte offset。

修复后使用既有 `out/build/windows-msvc-lab-ascii-ui` cache，串行重建 Debug / Release UI 与
ASCII session 测试，并在两个配置下分别运行 5 项针对性回归：`ascii_session`、ASCII 双向和
单向 Qt smoke、旧 Binary Qt smoke、v08 Qt smoke。最终 Debug `5/5 PASS`、Release
`5/5 PASS`，日志为：

- `out/lab-ascii-ui-fix-debug.log`
- `out/lab-ascii-ui-fix-release.log`

两类现有上限只读核对保持契约既定值，未扩容：变量 Tab 的 `name` 最大 8 bytes，对应
ASCII escaped 字段编辑容量 `4 * 8 + 4 = 36`；其完整记录最大 16 bytes，对应 Inspect 编辑容量
`4 * 16 + 4 = 68`。纯字面量 Tab 的完整记录为 6 bytes，对应 Inspect 编辑容量
`4 * 6 + 4 = 28`。协议长度仍按解码后字节计。

## 能力门禁

- 默认配置中 `PAE_BUILD_PROTOCOL_LAB_UI=OFF`、
  `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=OFF`、
  `PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=OFF`。
- V10+UI 但未显式启用 ASCII adapter 时配置失败，要求
  `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=ON`。
- V10+普通 Protocol Lab CLI 仍配置失败，提示 Schema 0.10 未接入 Protocol Lab / Evidence。
- 未修改旧 Result / Record / Event、Evidence、Replay / Compare、协议指纹或普通 CLI 实现。

## 人工复核结果与边界

可执行文件：

- Debug：`out/build/windows-msvc-lab-ascii-ui/out/protocol_lab_ui/Debug/pae_protocol_lab_ui.exe`
- Release：`out/build/windows-msvc-lab-ascii-ui/out/protocol_lab_ui/Release/pae_protocol_lab_ui.exe`

同目录 `configs` 中已部署变量 ASCII、纯字面量、Decode-only、Encode-only 及旧
Binary 公开合成配置。原定最少人工检查为：

1. 加载 `synthetic_ascii_text_slice.pae.json`，在 `ASCII (escaped)` 下输入 name / tx_tag，
   Encode 后对照 Hex，确认结果标记 `TX_TEMPLATE`。
2. Inspect 输入 `RX ALICE!OK\r\n`，确认匹配 `greeting`、字段值和实际字节高亮；
   切到 Hex 再切回，确认字节等价。
3. 输入实际换行、非法转义或超长值，确认错误可见、旧结果清除，改正后恢复。
4. 另开 Tab 加载纯字面量或单向配置，确认“成功，0 个字段”、动作按钮禁用和
   两个 Tab 的草稿/结果隔离。

修复后针对性人工复核状态为 `TARGETED_MANUAL_FIX_CONFIRMATION_PASS`，证据边界如下：

1. 成功 Inspect `RX ALICE!OK\r\n` 显示匹配 `greeting`、成功 2 个字段；`name` 和
   `rx_code` 的 Source 均为 `decoded`，Raw / Logical 分别为 `414C494345` / `ALICE` 和
   `4F4B` / `OK`；`tx_tag` 明确为 `not referenced` 且无 Raw / Logical 结果；底部没有残留
   Encode 的 `TX_TEMPLATE` 或计时。
2. 成功截图实际选中 `name`：侧栏 `Actual byte range: 3 + 5` 与
   `Actual physical bytes (zero-based): [3, 8)` 一致，Hex 03～07 高亮。`rx_code` 表格显示
   `9 + 2`，但没有选中它并截图侧栏，因此 `[9, 11)` 侧栏只保留自动化证据，不记为人工确认。
3. 非法输入 `RX ALICE!OK\q` 显示 `UI_ASCII_INPUT_INVALID` 和
   `input_utf16_code_unit_offset=11 (zero-based)`；字段、Hex、侧栏和旧 Encode 计时均清空。
4. 随后合法输入恢复正常及关闭软件来自用户文字确认，没有第三张截图；该确认不扩展为未展示的
   中间 Hex 表述状态。首次人工检查中的 Encode、纯字面量、单向按钮和跨 Tab 隔离结论保持不变。

应用同时最多打开两个文档 Tab 是既定 UI 契约；两个 Tab 后 New / Open 禁用、关闭一个后恢复，
不属于本次容量问题，也未授权或扩大该上限。
