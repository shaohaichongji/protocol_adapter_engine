# Protocol Lab UI Schema 0.8 有界变长检查点验证记录

日期：2026-09-10

基线：`4a04afb13d8dc38ea59bd7e7aa3cc3bba370c301`，分支 `feat/lab-ui-c1`

构建目录：`out/ui-v08-v142-check`

## 1. 构建配置

Windows x64、MSVC v142 `14.29.30133`、Qt `5.13.0`。Release 与 Debug 使用同一
multi-config 构建树和以下关键开关：

```text
PAE_BUILD_PROTOCOL_LAB_UI=ON
PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON
PAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=OFF
PAE_BUILD_PROTOCOL_LAB=OFF
```

Release 原批次和 Debug 本批次均成功构建 7 个 UI headless 测试目标与
`pae_protocol_lab_ui`。Debug 部署输出位于
`out/ui-v08-v142-check/out/protocol_lab_ui/Debug`。

## 2. 自动验证结果

Release 原批次命令：

```powershell
ctest --test-dir out/ui-v08-v142-check -C Release --output-on-failure -R "^pae\.tools\.protocol_lab_ui\."
```

结果：10/10 通过，其中 7 个 headless 测试、3 个真实 shown-window Qt smoke。

Debug 本批次命令：

```powershell
ctest --test-dir out/ui-v08-v142-check -C Debug --output-on-failure -R "^pae\.tools\.protocol_lab_ui\."
```

结果：10/10 通过，其中 7 个 headless 测试、3 个真实 shown-window Qt smoke；本批次
CTest 日志保存于 `out/ui-v08-v142-check/Testing/Temporary/LastTest.log`。Debug 验证在
Release 原批次之后按序执行，没有因无代码变化重复运行 Release。

Schema 0.8 专项覆盖：

- 空 payload：Core 输出 `A5 03 A8`，computed length 为 3，payload 结果为空；
- 最大三字节 payload：Core 输出 `A5 06 00 FF 01 AB`；
- 两字节 payload：Core 输出 `A5 05 10 20 DA`，payload 高亮为 byte 2、3，动态
  integrity storage 为 byte 4；
- 直接调用 Field model 时，四字节输入被长度边界拒绝；非法 Hex 被 canonical 词法规则拒绝，
  二者错误原因不同；
- 动态 integrity 失败不显示未经有效结果证明的 trailer 位置，随后合法 Inspect 可恢复实际
  payload 高亮；
- 描述映射验证复制后的 0..3 payload 边界、动态 range/storage 锚点和实际范围解析；
- Schema 0.5、0.6、0.7 的既有 headless 与 shown-window smoke 全部通过。

人工发现详情陈旧问题后，只重建受影响的 UI 目标并运行 Schema 0.8 shown-window 专项：

```powershell
ctest --test-dir out/ui-v08-v142-check -C Release --output-on-failure -R "^pae\.tools\.protocol_lab_ui\.qt_smoke_v08$"
ctest --test-dir out/ui-v08-v142-check -C Debug --output-on-failure -R "^pae\.tools\.protocol_lab_ui\.qt_smoke_v08$"
```

Release 1/1、Debug 1/1 通过。专项保持 payload 选择不变，依次验证空载荷、两字节、三字节
Encode 及 `GG` 失效后的右侧详情、表格、实际 Frame 和高亮同步。日志分别保存为：

- `out/ui-v08-v142-check/Testing/Temporary/v08-detail-refresh-release.log`
- `out/ui-v08-v142-check/Testing/Temporary/v08-detail-refresh-debug.log`

## 3. 证据边界

已验证的是离线合成配置下的 Debug/Release 编译、状态模型、Core 调用结果、窗口控件路径
和 Hex 高亮；限定人工检查见第4、5节。未验证完整视觉布局、真实协议配置、网络、硬件、
长稳、部署或现场验收；没有把 shown-window 自动 smoke 记为人工通过。

## 4. 人工实际检查记录

状态：`PARTIAL / DEFECT_FOUND_AND_FIXED_IN_CANDIDATE`。以下是用户修复前实际检查的文字记录；
外部临时截图仅用于核对，没有复制进仓库。

1. 空 payload Encode 实际得到 `A5 03 A8`、长度 3、表格 `length=0` 且无 payload
   高亮；右侧详情错误地保留 `current range unavailable`。
2. `1020` Encode 实际得到 `A5 05 10 20 DA`；重新选择 payload 后，右侧正确显示 payload
   `2 + 2`、integrity `4 + 1`。
3. 保持 payload 选择，改为 `00FF01` 后实际得到 `A5 06 00 FF 01 AB`、长度 6、高亮
   byte 2..4，但右侧错误地保留旧的 `2 + 2` / `4 + 1`。
4. 随后提交 `GG`，`UI_INPUT_INVALID` 正确报告 canonical Hex 错误，Frame、Raw、Logical、
   高亮和表格实际范围均清空；右侧仍错误地保留旧实际范围。
5. Inspect `A5 05 10 20 DB` 正确进入 `INTEGRITY_FAILED` 且无动态尾高亮；改为
   `A5 05 10 20 DA` 后恢复长度 5、payload `1020`、byte 2..3 高亮和 `2 + 2` / `4 + 1`
   详情。该 Inspect 人工路径通过，无需重复。

根因是 Encode 结果变化只刷新表格与 Hex，右侧详情仅在选择变化时重算。候选修复让每次
Encode 成功或失效都基于当前选择重算详情，同时避免详情与预览刷新互相递归。

四字节越界没有由人工编辑器路径验证。BYTES 编辑器按最大三字节设置 `maxLength=6`，因此
键入或粘贴 `01020304` 时，超过六个字符的部分不会进入 Field model；自动化中的四字节
负例属于 model 层验证，不能记为人工 UI 验证。该静默截断存在误解输入已完整提交的风险；
建议后续单独决策是否增加显式超长提示或整次粘贴拒绝，本检查点不改变输入策略。

## 5. 修复后人工确认（2026-09-10）

状态：`TARGETED_MANUAL_FIX_CONFIRMATION_PASS`，仅覆盖下述截图可核实结果，不升级完整人工矩阵。

1. 用户按不重新选行的检查步骤回传 `00FF01` Encode 截图：Frame为
   `A5 06 00 FF 01 AB`，长度6，payload高亮byte 2..4；右侧已同步为 actual
   `2 + 3`、integrity `5 + 1`，不再保留旧范围。
2. 随后 `GG` 截图显示 `UI_INPUT_INVALID`；Frame、Raw、Logical、高亮和右侧旧actual
   范围均清空，右侧显示 `current range unavailable`。失败清理修复获人工确认。
3. 修复后空载荷详情没有单独截图确认，保留专项自动化证据；此前Inspect人工通过记录不变。

用户已关闭Lab。本次只记录实际人工证据，未重跑测试、未更改源码，未执行Git写操作。
第4节超长输入截断风险仍保留，四字节越界未由人工编辑器路径验证。

## 6. 提交前格式收口

总控只读格式检查发现部分候选C++排版不符合仓库clang-format配置，随后仅对候选C++文件
执行格式化，`clang-format --dry-run --Werror`及`git diff --check`通过。未修改业务逻辑，
未重跑测试；上述Debug/Release证据属于格式整理前的功能验证批次。未Stage/Commit/Push。

## 7. BYTES 编辑器超长输入可靠性修复（2026-09-10）

实施起点为分支 `feat/lab-ui-c1`、HEAD
`111916b23ecd655e8ee104c490468d7a5e048e1b`，工作树起始状态干净。只读参考总控工作树
ASCII 最小契约第8、9节；没有复制或修改该契约，也没有执行Git写操作。

### 7.1 修复前证据与根因

用户实际验证确认：三字节 payload 的 `QLineEdit::maxLength` 为6，键入或粘贴
`01020304` 时无法保留完整八字符草稿。实现检查确认旧代码直接以
`protocol_byte_width * 2` 设置 `maxLength`，Qt会在 Field model 看到输入前截短，因此已有
model四字节负例不能覆盖真实编辑器行为。

### 7.2 编辑容量与拒绝策略

- 协议最大长度仍由既有 Field model 校验，不改变 Binary BYTES 语法或接受域。
- 编辑容量取 `protocol_byte_width * 2 + 2` 个 Hex 字符，即在全部合法输入之外至少容纳一个
  完整超长字节。三字节 payload 的容量因此为8，`01020304`会完整保留并在提交时得到既有
  `Payload length must be 0..3 bytes` 错误。
- 容量计算以既有 desktop `max_frame_bytes=65536` 为硬依据，最大编辑容量为131074字符；
  计算先核对协议宽度和 `int` 可表示性，不允许溢出或无界编辑状态。
- `QValidator`针对整次候选文本判定容量。超过容量的键入、粘贴或替换不会接受合法前缀；
  编辑器保留操作前文本，并通过红色背景、即时tooltip和模型诊断显示
  `entire edit rejected`。
- 容量拒绝同时把当前session草稿置为无效，清除旧Frame、Raw、Logical和高亮；带拒绝标记
  的编辑器不能把旧文本重新提交为本次成功。后续合法编辑清除反馈并可正常恢复。

### 7.3 首轮编辑器输入验证的边界

首轮Schema 0.8 shown-window smoke通过委托创建了实际 `QLineEdit` 并发送键盘、粘贴事件，
但随后直接调用 `setModelData` 提交，绕过 `QTableView` / `QStyledItemDelegate` 的标准
Enter、Tab和FocusOut门禁。因此下列首轮通过结果能证明输入保留、容量拒绝、模型校验和恢复，
不能证明标准编辑生命周期能够提交；该证据缺口和后续修复见第8节。

1. 从旧成功Preview开始，选择全部文本并逐键输入 `01020304`；编辑器保留完整八字符，旧结果
   立即清理，提交后model报告0..3字节越界。
2. 通过键盘替换为 `010203`、提交并Encode，验证恢复成功。
3. 选择全部文本后粘贴 `01020304`；完整草稿保留，提交后得到同一协议长度错误。
4. 恢复合法三字节和成功Preview后，粘贴十字符 `0102030405`；整次操作因超过八字符容量被
   拒绝，编辑器仍是旧的 `010203`，容量属性、tooltip和模型诊断可见，旧Preview已清理，
   提交不会产生新成功。
5. 再通过键盘替换为 `1020`、提交并Encode，验证Frame恢复为 `A5 05 10 20 DA`。
6. 既有空载荷、两/三字节动态详情、`GG`失败清理和Inspect恢复断言继续通过。

最终源状态的针对性Windows结果：

- Release：重建 `pae_protocol_lab_ui` 成功；`qt_smoke`、`qt_smoke_v07`、
  `qt_smoke_v08` 为3/3通过。
- Debug：重建 `pae_protocol_lab_ui` 成功；同一集合3/3通过。
- 格式：候选C++文件 `clang-format --dry-run --Werror` 通过；`git diff --check` 通过。

最终日志：

- `out/ui-v08-v142-check/Testing/Temporary/bytes-editor-final-release.log`
- `out/ui-v08-v142-check/Testing/Temporary/bytes-editor-final-debug.log`

本批次只证明离线Windows Qt编辑器、旧成功状态清理、提交与恢复行为。未运行完整产品矩阵、
Linux、网络、真实协议、硬件、长稳、性能或部署验收；没有实施ASCII codec或ASCII UI。

## 8. BYTES 标准提交门禁修复（2026-09-10）

### 8.1 修复前动态证据

总控先通过Qt 5.13源码静态指出：容量Validator对全部容量内输入返回 `Intermediate`，标准
delegate在Enter、Tab或FocusOut提交前会检查 `hasAcceptableInput()`，因此合法值也可能无法
提交。该发现最初只有静态证据，没有冒充动态复现。

随后将专项改为由实际 `QTableView` 打开和管理编辑器，并用真实按键触发Enter提交；保持
Validator为 `Intermediate` 的修复前Release运行明确失败：

```text
UI_SMOKE_FAIL ... Enter did not commit legal BYTES through the table delegate
```

修复前日志为
`out/ui-v08-v142-check/Testing/Temporary/bytes-editor-submit-prefix-release.log`。这次运行才构成
动态复现证据。

### 8.2 限定修复

容量Validator现在对容量内候选文本返回 `Acceptable`，只表达“允许进入标准提交链”；超过
编辑容量仍返回 `Invalid` 并整次拒绝。协议Hex语法和payload长度继续由既有Field model
判断，因此 `01020304`仍能完整进入model并得到0..3字节错误，没有重新变成编辑器级拒绝。

剪贴板测试辅助从“只保存文本”改为复制并恢复原剪贴板的全部MIME formats，避免测试结束时
丢失用户原有非文本数据。测试不激活或操作其他应用窗口。

### 8.3 标准编辑生命周期覆盖

最终shown-window专项不再手调 `setModelData`，覆盖：

1. `QTableView`实际编辑器逐键替换为合法 `1020`，按Enter提交；model值为 `1020`，Encode
   Frame为 `A5 05 10 20 DA`。
2. 重新打开编辑器，逐键替换为容量内超协议值 `01020304`，按Tab提交；model完整保留八字符
   并报告0..3字节错误，旧成功状态已清理。
3. 重新打开编辑器，粘贴 `01020304` 后按Enter；model再次收到完整草稿并报告长度错误。
4. 替换为合法 `010203`，将焦点移到Encode按钮触发FocusOut提交；model值更新并可恢复
   Encode。
5. 从合法成功状态粘贴超容量 `0102030405`；整次拒绝，编辑器仍为旧 `010203`，容量反馈和
   model无效状态可见，随后Enter不能把旧文本提交为新成功。
6. 重新打开编辑器，替换为 `1020`并Enter提交，验证model值和预期Frame恢复。

### 8.4 最终Windows结果

最终源状态串行重建并执行受影响窗口集合：

- Release：`pae_protocol_lab_ui`构建成功；`qt_smoke`、`qt_smoke_v07`、
  `qt_smoke_v08`为3/3通过。
- Debug：`pae_protocol_lab_ui`构建成功；同一集合3/3通过。
- `clang-format --dry-run --Werror`和`git diff --check`通过。

最终日志：

- `out/ui-v08-v142-check/Testing/Temporary/bytes-editor-submit-final-release.log`
- `out/ui-v08-v142-check/Testing/Temporary/bytes-editor-submit-final-debug.log`

本轮只修复Lab BYTES编辑器的有界输入和标准提交可靠性。未修改Compiler、Plan、Core、Schema、
共享执行桥或根CMake，未实施ASCII codec、ASCII UI或网络，未运行完整产品矩阵、Linux、
真实协议、硬件、长稳、性能或部署验收。
