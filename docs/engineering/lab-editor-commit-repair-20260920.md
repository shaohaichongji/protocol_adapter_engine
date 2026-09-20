# Lab 编辑提交 smoke 驱动修复记录（2026-09-20）

## 1. 结论

本轮按授权修复了 `DocumentTab` 中两处 smoke 驱动：

- ASCII 首次 `ALICE` 编辑现在通过既有 `CommitEditorByKey(..., Qt::Key_Return)` 发送真实 Return，并分别核对 field model、`DocumentSession::drafts()`、Encode preview、报文字节与 `tx_template_review`。
- Schema 0.8 FocusOut 场景现在先激活隐藏顶层窗口、让 editor 真实获得 focus 并核对 `QApplication::focusWidget()`，再把 focus 转给按钮，经 Qt delegate 真实事件链提交；没有直接调用 `setModelData`，也没有把 FocusOut 改成 Enter。

Debug 定向结果证明两处授权修复均到达预期阶段：

- ASCII：model/session 接收 `ALICE`，Encode preview、预期 TX 字节及 `TX_TEMPLATE` 身份均通过；随后在更晚的既有 Inspect action/source/result annotations 检查失败。
- v0.8：focus 前置、FocusOut model 提交和 Encode 恢复均通过，整项 smoke PASS。

由于 ASCII 在授权两处之后的新阶段仍失败，本轮按停点停止，没有运行 Release 定向测试，也没有扩修 Inspect annotations。当前不是 D/R 两项全通过。

## 2. 基线与范围

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 分支与 HEAD：`main@fa81329563dd3aea9bb167ef9bd34526a2606161`
- 输入诊断：`docs/engineering/lab-editor-commit-diagnosis-20260920.md`
- 实际修改：`tools/protocol_lab_ui/document_tab.cpp`
- 唯一新增报告：本文件
- 新构建根：`out/build/windows-msvc-lab-editor-commit-repair-20260920`
- 新证据根：`out/validation/lab-editor-commit-repair-20260920`
- 工具链：MSVC v142 `14.29.30133`、`cl 19.29.30159.0`、仓库 Qt `5.13.0`

未修改 production delegate、model、session、`EncodeCurrent()`、公共 PAE/API/Schema、跟踪 CMake、普通 UI 交互或旧部署。既有命名、清理和 Qt smoke 诊断变更全部保留。

## 3. 实际修改

### 3.1 ASCII Return 提交

原首段输入 `ALICE` 后调用按钮 `setFocus()`，但错误文案和测试意图均声称 Enter commit。现改为：

1. 使用 `CommitEditorByKey(*editor, Qt::Key_Return)`，仍走 Qt delegate 键盘事件路径；
2. editor 可能在 KeyPress 期间关闭，因此 Return 后不再访问原 editor；
3. 先检查 `FieldTableModel` 的 `Qt::EditRole` 等于 `ALICE`；
4. 通过现有 `DocumentSession::drafts()` 只读接口核对对应 field draft 是字节 `ALICE`；
5. 再执行 Encode，并把原 compound condition 拆成：preview 存在、encoded frame 精确匹配、`tx_template_review` 为真；
6. 输出 smoke-only 阶段标记：

   ```text
   UI_SMOKE_EDITOR_COMMIT case=ascii method=return model=accepted session=accepted
   UI_SMOKE_EDITOR_COMMIT case=ascii method=return output=accepted
   ```

ASCII diagnostic trace 名称同步从 `before_focus_commit` 改为 `before_return_commit`，避免诊断与实际动作冲突。

### 3.2 v0.8 真实 FocusOut

在重新输入合法 `010203` 后：

1. 用 `QPointer<QLineEdit>` 守护 editor 生命周期；
2. 取得当前顶层窗口并调用 `activateWindow()`；
3. pump 后让 editor `setFocus(Qt::OtherFocusReason)`；
4. 再 pump 并严格检查 editor 仍存活且 `QApplication::focusWidget() == editor`；无法建立 focus 时明确报测试前置失败；
5. 调用按钮 `setFocus()` 产生真实 FocusOut，并在事件/DeferredDelete pump 后只检查 model，不解引用可能已经关闭的 editor；
6. model 接受 `010203` 后继续验证 Encode preview；
7. 输出 smoke-only 阶段标记：

   ```text
   UI_SMOKE_EDITOR_COMMIT case=v08 method=focus_out precondition=focused
   UI_SMOKE_EDITOR_COMMIT case=v08 method=focus_out model=accepted
   UI_SMOKE_EDITOR_COMMIT case=v08 method=focus_out output=accepted
   ```

这些动作只存在于 `VerifyBoundedV08ForSmoke()`，不会改变普通启动或用户编辑流程，也没有创建可见窗口。

## 4. 构建与测试

### 4.1 配置和构建

```powershell
cmake --preset windows-msvc-stage4-h2 `
  -B out/build/windows-msvc-lab-editor-commit-repair-20260920 `
  -T v142,version=14.29.30133

cmake --build out/build/windows-msvc-lab-editor-commit-repair-20260920 `
  --config Debug --target pae_protocol_lab_ui

cmake --build out/build/windows-msvc-lab-editor-commit-repair-20260920 `
  --config Release --target pae_protocol_lab_ui
```

结果：配置、Debug 应用构建、Release 应用构建均成功。证据：

- `configure.log`
- `debug-build.log`
- `release-build.log`

### 4.2 精确测试清单

使用正则：

```text
^pae\.tools\.protocol_lab_ui\.(qt_smoke_ascii|qt_smoke_v08)$
```

D/R `ctest -N -V` 均精确列出 2 项：

- `pae.tools.protocol_lab_ui.qt_smoke_ascii`
- `pae.tools.protocol_lab_ui.qt_smoke_v08`

没有包含 `ascii_stream`、`ascii_one_way` 或其他 window smoke。两项均指向 matching-config 完整部署 EXE，并设置 `QT_QPA_PLATFORM=windows`。证据：

- `debug-test-inventory.log`
- `release-test-inventory.log`

### 4.3 Debug 定向执行与停点

```powershell
ctest --test-dir out/build/windows-msvc-lab-editor-commit-repair-20260920 `
  -C Debug -V --output-on-failure `
  -R '^pae\.tools\.protocol_lab_ui\.(qt_smoke_ascii|qt_smoke_v08)$'
```

结果：1 PASS / 1 FAIL，CTest exit code 8；运行后无 `pae_protocol_lab_ui` 残留进程。完整证据：`debug-targeted-tests.log`。

#### ASCII

日志先出现：

```text
UI_SMOKE_DIAGNOSTICS stderr=enabled dialogs=disabled
UI_SMOKE_EDITOR_COMMIT case=ascii method=return model=accepted session=accepted
UI_SMOKE_EDITOR_COMMIT case=ascii method=return output=accepted
```

这证明本轮授权的 Return → model/session → Encode 输出链已通过。之后失败为：

```text
UI_SMOKE_FAIL detail=ASCII document 1: ASCII Inspect action/source/result annotations are inconsistent
```

该失败位于 `document_tab.cpp` 约 1427–1445 行的既有 compound Inspect annotations 检查，晚于本轮修改点。现有日志不能区分 `rx_source/rx_value/rx_raw/rx_logical/rx_physical/tx_source/tx_value/tx_raw` 中哪一项不符；本轮没有插桩或修改该范围。

#### Schema 0.8

日志出现全部三项阶段标记，随后：

```text
UI_SMOKE_DOCUMENT index=1 frame_bytes=6 highlighted_cells=1 ...
UI_SMOKE_PASS detail=1 document(s)
```

结果为 PASS，证明隐藏窗口已建立真实 editor focus，FocusOut 提交更新 model，并恢复有效 Encode 输出。

### 4.4 Release 停止边界

Release 应用已构建，Release 精确测试清单已核对，但因为 Debug ASCII 在后续 Inspect annotations 阶段失败，按授权没有运行 Release 两项测试。没有通过重复运行或扩大修改尝试刷绿。

## 5. 未验证与剩余事项

- ASCII Inspect annotations 失败的具体子谓词、测试驱动责任或产品展示责任尚未定位；需另行授权只读诊断或局部修复。
- Release 两项定向 smoke 未运行，因此不能声明 D/R 通过。
- 未运行 window 全组、全量 CTest、人工可见窗口、旧部署对比或 SDK 消费验证。
- 没有验证其他平台插件、Linux、人工交互或正式发布条件。

## 6. Git 与交付状态

- 未 Stage、Commit、Push、删除、发布或替换部署。
- 本轮只修改 `tools/protocol_lab_ui/document_tab.cpp` 并新增本报告。
- 已到达失败停点并停止写入；状态为“已完成授权修复及 Debug 定向验证，发现后续独立失败，待总控复核/决策”。

## 7. Inspect 展示断言同步与 D/R 补验

在后续限定授权内核对 `FieldTableModel` 当前展示规则后，确认第 4.3 节记录的失败来自 smoke 中三个已过期的英文期望，而非产品展示异常：Inspect 下被引用字段来源为“解析结果”，未引用字段来源为“未引用”，未引用字段值为“未被 Decode 动作引用”。

仅在 `VerifyAsciiForSmoke()` 内完成以下调整：

- 将上述三个固定期望同步为当前中文展示契约；期望值仍独立写定，没有从实际输出反推。
- 保留 RX value 为空、raw 为 `4F4B`、logical 为 `OK`、physical 为 `9 + 2`，以及 TX raw 为空等原有隔离断言。
- 将原 compound 失败改为逐项检查；失败信息明确给出 `field`、`column`、`expected`、`actual`，空字符串显示为 `<empty>`。
- 未改动 production delegate/model/session、`EncodeCurrent()`、PAE/API/Schema/CMake，也保留前述真实 Return 与真实 FocusOut 提交路径。

沿用构建根 `out/build/windows-msvc-lab-editor-commit-repair-20260920`。Debug 应用在本轮修改后已增量构建；Release 应用也在执行 Release 测试前基于当前源码重新增量构建。使用与第 4.2 节相同的严格正则，D/R 均精确执行两项：

```text
pae.tools.protocol_lab_ui.qt_smoke_ascii
pae.tools.protocol_lab_ui.qt_smoke_v08
```

结果：

- Debug：2/2 PASS，CTest exit code 0；ASCII Return 的 model/session/output 标记、v0.8 FocusOut 的 precondition/model/output 标记均存在；运行后残留进程数 0。
- Release：当前源码增量构建成功；2/2 PASS，CTest exit code 0；同样保留上述提交与输出标记；运行后残留进程数 0。

新增证据位于 `out/validation/lab-editor-commit-repair-20260920/annotations-fix/`：

- `debug-build.log`
- `debug-test-inventory.log`
- `debug-targeted-tests.log`
- `release-build.log`
- `release-test-inventory.log`
- `release-targeted-tests.log`

本轮没有运行 window 全组、全量 CTest、人工可见窗口、SDK 消费、其他平台或正式发布验证；第 4.3、4.4 节保留修复前失败与当时停止边界，不能脱离本节把历史中间状态当作最终结果。未 Stage、Commit、Push、删除、发布或替换部署。
