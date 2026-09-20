# Lab 编辑提交失败限定诊断（2026-09-20）

## 1. 结论

本轮分别定位了 ASCII 首次编辑提交失败和 Schema 0.8 BYTES 失焦提交失败。当前证据支持将两项**现有自动 smoke 失败**归到测试驱动前置条件，而不是已经证明的产品编辑器或业务缺陷：

- ASCII 失败文案称“Enter commit”，但对应代码没有发送 Enter；它输入 `ALICE` 后只调用 `encode_button_->setFocus()`。在 `Qt::WA_DontShowOnScreen` 且窗口未激活时，editor 从未拥有 focus，因此不会产生 FocusOut 或 delegate commit。该项最小修复应让测试发送真实 Enter，并继续经 delegate 的键盘提交路径验证结果。
- v0.8 明确要验证 FocusOut，但测试同样没有先证明隐藏窗口和 editor 已经取得 focus；随后 `setFocus()` 不产生焦点迁移。隔离复现证明：保持隐藏属性不变、先激活窗口并让 editor 实际获得 focus，再把 focus 转给按钮，会产生完整的 FocusOut → `commitData` → `closeEditor` → `setModelData` 链。

两项共享“隐藏且未激活窗口没有实际 focus”这一环境事实，但测试意图不同，不能用同一个替代动作糊平：ASCII 应走真实 Enter；v0.8 应建立并核验真实 focus 前置条件后再走 FocusOut。直接调用 `setModelData` 只能绕过待测链路，本轮没有采用，也不建议作为修复。

本轮未修改任何产品、业务、PAE/API/Schema、跟踪测试源码或跟踪 CMake；唯一仓库新增文件为本报告。

## 2. 基线、范围与证据

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 分支与 HEAD：`main@fa81329563dd3aea9bb167ef9bd34526a2606161`
- 既有未提交命名、清理和 Qt smoke 诊断变更全部保留。
- 输入报告：`docs/engineering/protocol-metadata-qt-smoke-repair-20260920.md`
- 既有失败日志：`out/validation/qt-smoke-repair-20260920/debug-window-smoke.log`
- 隔离源码：`out/validation/lab-editor-commit-diagnosis-20260920/repro-src/`
- 隔离构建根：`out/build/windows-msvc-lab-editor-commit-diagnosis-20260920`
- 隔离证据根：`out/validation/lab-editor-commit-diagnosis-20260920`
- 工具链：MSVC v142 `14.29.30133`、`cl 19.29.30159.0`、仓库 Qt `5.13.0`、Windows platform key `windows`。

隔离程序只使用 `QTableView`、`QStandardItemModel`、`QStyledItemDelegate`、`QLineEdit` 和按钮复现 Qt 编辑链；窗口设置 `Qt::WA_DontShowOnScreen`，没有可见 UI。它记录：

- `QApplication::activeWindow()` 与 `focusWidget()`；
- editor/button 的 `hasFocus()`；
- FocusIn、FocusOut 和 KeyPress 数量；
- delegate 的 `commitData`、`closeEditor`、`setModelData` 数量；
- action 前后的模型值。

程序没有直接调用 `setModelData`，所有提交均由真实 Enter 或真实 FocusOut 事件触发。启动时设置进程内 Windows/MSVC stderr 诊断与无系统错误对话框路径；两次隔离运行均 exit 0、无残留进程。

## 3. 公共编辑链事实

### 3.1 Delegate 与 model/session 边界

`ExactValueDelegate` 继承 `QStyledItemDelegate`，没有覆写 `eventFilter`。因此 Enter/FocusOut 是否触发提交，仍由 Qt delegate 的标准编辑事件路径决定。实际写入发生在：

1. delegate `setModelData()` 读取 `QLineEdit::text()`；
2. `FieldTableModel::setData()` 解析并校验 draft；
3. `draft_changed_` 回调调用 `DocumentSession::SetDraft()`；
4. 后续 `DocumentSession::Encode()` 使用 session 中已提交的 typed drafts。

editor 的 `textChanged` 回调并不提交新值，而是调用 `DocumentTab::InvalidateEditedDraft()` → `DocumentSession::InvalidateDraft()`；该路径会擦除对应已保存 draft、invalid draft 和 preview。这保证“正在编辑但尚未提交”的内容不会被 Encode 当作有效输入。

`DocumentTab::EncodeCurrent()` 会执行 `field_table_->clearFocus()` 并 pump `ExcludeUserInputEvents`，但如果 editor 从未持有 focus，这两个动作仍不会产生 FocusOut，也不会补做 delegate commit。

### 3.2 隐藏窗口的实测行为

`hidden-editor-commit-repro-activated-focus.log` 记录了四个对照：

| 场景 | active/focus 前置 | FocusOut | commit/close/setModelData | 最终模型 |
| --- | --- | ---: | --- | --- |
| `set_focus` | activeWindow=null，focusWidget=null，editor 无 focus | 0 | 0 / 0 / 0 | `OLD` |
| `clear_then_set_focus` | activeWindow=null，focusWidget=null，editor 无 focus | 0 | 0 / 0 / 0 | `OLD` |
| `activate_then_set_focus` | 隐藏窗口已激活，editor 有 focus | 1 | 1 / 1 / 1 | `NEW` |
| `enter` | activeWindow=null，editor 无 focus；直接发送真实 Enter | 0 | 1 / 1 / 1 | `NEW` |

异步处理均执行了 `QApplication::processEvents(QEventLoop::ExcludeUserInputEvents)` 和 `sendPostedEvents(..., DeferredDelete)`。未激活的两个 focus 场景在完整 pump 后仍没有提交，说明缺失的不是“再多 pump 一轮”，而是根本没有发生焦点迁移。Enter 和已建立 focus 的 FocusOut 都在相同 pump 边界内完成提交并关闭 editor。

## 4. ASCII 首次提交

### 4.1 已证明事实

1. `VerifyAsciiForSmoke()` 在 `document_tab.cpp` 约 1193–1238 行打开第一行 editor，通过逐字符 `QKeyEvent::KeyPress` 输入 `ALICE`。
2. 输入后代码没有调用 `CommitEditorByKey(..., Qt::Key_Return)`；它调用的是 `encode_button_->setFocus(Qt::OtherFocusReason)`，pump 后立即 `EncodeCurrent()`。
3. 失败文案“ASCII Enter commit did not produce independent TX template bytes”与实际驱动不一致；这一段没有 Enter。
4. `main.cpp` 的 smoke 路径在 `window.show()` 前设置 `Qt::WA_DontShowOnScreen`。隔离复现证明，同样条件下 editor 没有实际 focus，按钮 `setFocus()` 不产生 FocusOut 或任何 delegate/model 提交。
5. 既有 `debug-window-smoke.log` 只记录 compound condition 失败，不能区分 `preview` 缺失、frame 不匹配或 `tx_template_review` 为 false；本轮没有把其中某一项伪装成已直接观测事实。

### 4.2 推断

- 源码控制流与隔离 Qt 行为共同表明：ASCII 第一段输入触发 `InvalidateDraft` 后，没有 delegate commit 将 `ALICE` 写回 model/session；紧接的 Encode 因而无法使用该新值。这足以解释当前 smoke 失败。
- 该结论定位的是自动测试驱动错误，不证明真实可见窗口中的 Enter 或正常焦点切换存在产品缺陷。

### 4.3 未知

- 现有 compound failure 中，实际失败的子谓词组合没有独立日志；若要精确记录 session diagnostic/frame，需要在跟踪 smoke 源码增加只读诊断输出。本轮授权禁止跟踪源码插桩，因此保持未知。
- 未运行真实可见窗口或人工输入；不能据隐藏 smoke 推断最终用户交互验收状态。

### 4.4 最小修复建议与验证范围

- 将第一段的 `encode_button_->setFocus()` 提交动作改为既有 `CommitEditorByKey(*editor, Qt::Key_Return)`，使代码与“Enter commit”测试意图一致；继续由 delegate 产生 `commitData/closeEditor/setModelData`，不得直接调用 `setModelData`。
- 在 Encode 前独立断言 model/session 已接受 `ALICE`，再断言预期 frame 和 `TX_TEMPLATE`，避免 compound failure 混淆提交失败与 Encode/展示失败。
- 修复后只需 Debug/Release 各运行 ASCII 定向 window smoke，并保留真实 Enter、model/session 与 frame 证据；不需要重跑全 UI 矩阵。

## 5. Schema 0.8 FocusOut 提交

### 5.1 已证明事实

1. `VerifyBoundedV08ForSmoke()` 的早期步骤通过 `CommitEditorByKey()` 发送真实 Enter/Tab；对应 model 检查在本次失败点之前均已通过。因此同一隐藏 smoke 中，editor 创建、键盘输入、delegate `setModelData`、model/session 校验链在真实按键提交时可以工作。
2. 失败段重新打开 editor、通过键盘输入 `010203`，随后仅调用 `encode_button_->setFocus()`，pump 后直接检查 model 是否更新；检查失败于约 860–861 行，尚未进入后续 Encode。
3. 隔离复现的未激活隐藏窗口中，editor 无 focus，按钮 `setFocus()` 和显式 `editor->clearFocus()` 均不产生 FocusOut/commit，model 保持旧值。
4. 同一个隐藏窗口若先 `activateWindow()` 并确认 editor 实际取得 focus，再转移到按钮，会记录一次 FocusOut、一次 `commitData`、一次 `closeEditor`、一次 `setModelData`，model 更新为新值。

### 5.2 推断

- 当前 v0.8 失败由 smoke 在没有 focus 的 editor 上试图测试 FocusOut 所解释；它首先是测试前置条件缺失。
- 当前证据不支持修改 `ExactValueDelegate::setModelData()`、`FieldTableModel` 或 `DocumentSession`，因为真实 Enter 和已建立 focus 的 FocusOut 均能完成标准提交链。

### 5.3 未知

- 尚未在完整应用内部记录 v0.8 editor 的 `focusWidget`、`commitData` 和 session draft；做到这一点需要跟踪源码插桩，超出本轮授权。
- 隔离结果只证明当前 Windows/qwindows/Qt 5.13 环境；不能自动推广到其他平台插件或可见窗口管理器。

### 5.4 最小修复建议与验证范围

- 保留 FocusOut 测试意图：在隐藏 smoke 中先激活顶层窗口，让 editor 获得 focus，并显式断言 `QApplication::focusWidget() == editor`；然后用按钮 `setFocus()` 触发真实迁移。若无法建立 focus，应以明确的“测试前置条件失败”停止，而不是继续检查 model。
- 不建议把 v0.8 改成 Enter，也不建议直接调用 `setModelData`，因为两者都不能验证 FocusOut 路径。
- 修复后只需 Debug/Release 各运行 v0.8 定向 window smoke，并记录 focus 前置、FocusOut 后 model 值和 Encode 恢复；不需要重跑 window 全组。

## 6. 验证记录

配置与构建：

```powershell
cmake -S out/validation/lab-editor-commit-diagnosis-20260920/repro-src `
  -B out/build/windows-msvc-lab-editor-commit-diagnosis-20260920 `
  -G "Visual Studio 18 2026" -A x64 `
  -T v142,version=14.29.30133

cmake --build out/build/windows-msvc-lab-editor-commit-diagnosis-20260920 `
  --config Debug --target lab_editor_commit_diagnosis
```

隐藏运行使用 `QT_QPA_PLATFORM=windows`。证据：

- `configure.log`：工具链和仓库 Qt 输入通过；
- `build-debug.log`：初始隔离程序构建通过；
- `hidden-editor-commit-repro.log`：未激活 focus 与 Enter 首轮对照；
- `build-debug-activated-focus.log`：增加激活 focus 对照后构建通过；
- `hidden-editor-commit-repro-activated-focus.log`：四场景最终证据，exit 0、`REPRO_PASS`、无残留进程。

没有重建完整应用，没有重跑两项应用 smoke 或 window 全组；应用失败事实直接使用既有 `debug-window-smoke.log`，避免重复运行。

## 7. 交付状态

已完成两项失败的测试驱动/产品责任限定定位，未实施修复。仓库跟踪源码与 CMake 未修改；未 Stage、Commit、Push、删除、发布或替换部署。当前为“已完成派发范围，待总控复核”，已停止写入。
