# Lab UI U2 页面分区与诊断可读性验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 范围与基线

- 仓库：`<REPO_ROOT>`
- 现场基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`
- U2 实际修改：`tools/protocol_lab_ui/document_tab.{h,cpp}` 与本报告。
- G1、U1、总控文档和共享工作树中的全部既有改动保留；未修改 Session、Adapter、Core、
  public API、Schema、root CMake 或执行/失败语义。

## 2. 实现结论

1. 页面按“文档与编译”“绑定与上下文”“操作与输入”“结果”分区；Host 绑定草稿表及其
   增删/应用按钮默认收起，当前绑定、Flow 与当前状态保留在折叠区外。独立
   `hostPanelToggle` 只控制草稿设置可见性，不改变子控件 enabled 状态或 Session。
2. Host 标题按已知 UI 状态显示“绑定草稿·尚未应用”“正在准备候选会话…”“绑定已生效”或
   “绑定已生效·草稿尚未应用”。草稿 dirty 仅来自表格增删/编辑，候选与 active 仍以现有公开状态为准。
3. 字段表与 Hex 原有选择/高亮链路保持不变；右侧改为 `resultDetailsTabs`，包含“字段详情”、
   “诊断与计时”“流状态”。诊断与流状态位于可滚动、可复制的容器，不再占用页面底部高度。
4. 顶部结果状态使用明确中文文本和符号，并提供稳定 `paeResultState` 属性；流状态页签明确标注
   “可继续处理”或“需要重置”，不只依赖颜色。
5. QMessageBox 的“是/否”文本固定为中文，同时保持 `QMessageBox::Yes/No` 枚举和 `No` 默认决策。
6. 恢复并加强 ASCII `TX_TEMPLATE` timing 覆盖：`paeReviewKind=TX_TEMPLATE` 与实际 timing 文本
   必须同时一致；零字段 literal-only 路径也执行同一断言。
7. U1 稳定锚点验证扩展到折叠按钮、右侧页签和滚动诊断；折叠/展开自动核对 DocumentState、
   plan generation 和预览大小不变，并以 `isVisibleTo(hostPanel)` 核对当前绑定、Flow 与状态在
   收起后仍可见。长诊断继续逐字核对中文摘要、稳定码和原技术 detail。
8. Binary public H2 文档的真实 `hostBindingDraft` 动作下拉框提供“解析/组包”；旧版未启用
   public H2 的 Binary 分支仍仅提供原有 Decode 能力。Binary Host smoke 从草稿表选择 Encode、
   Apply 后切到该绑定，选择 Message、填充 typed input 并执行 Encode，再回切 Decode，未注入
   Adapter 或绕过 UI 控件。

## 3. 测试优先与修正

首次 Debug Binary Host 双文档 smoke 失败于：

```text
UI_SMOKE_FAIL detail=Binary Host document 1: Schema 0.9 did not start unbound
```

原因是最初采用 checkable `QGroupBox` 实现折叠，未选中会连带禁用 `hostApply`。修正为独立折叠按钮后，
按钮仅改变 `host_content_` 可见性；同一 Binary Host 双文档 smoke 随后通过。失败证据保留于：

- `out/validation/lab-ui-layout-u2/debug-binary-host.stderr.log`
- `out/validation/lab-ui-layout-u2/debug-binary-host-fix.stdout.log`

总控限定复核后新增真实控件路径断言。临时保留修复前 Binary 动作下拉框不提供 Encode 的源码形态，
同一 Debug Binary Host smoke 以退出码 2 失败：

```text
UI_SMOKE_FAIL detail=Binary Host document 1: Binary Host Encode action is not exposed by hostBindingDraft
```

证据为 `out/validation/lab-ui-layout-u2/refinement-pre-fix-debug-binary-host.stderr.log`；随后仅在
public H2 Binary 分支加入 Encode 选项。另将当前绑定、Flow、状态移出 `host_content_`，新增
`isVisibleTo` 与执行状态不变断言。修正后的同路径 D/R 均通过。

## 4. 终态构建与验证

隔离构建根：`out/build/windows-msvc-lab-ui-u2`，配置显式使用
`v142,version=14.29.30133` 与仓内 Qt 5.13 输入。

Debug/Release 均成功构建：

- `pae_protocol_lab_ui`
- `pae_protocol_lab_ui_ascii_smoke_editor_tests`
- `pae_protocol_lab_ui_bounded_v08_state_tests`
- `pae_protocol_lab_ui_binary_public_h2_tests`

Debug/Release 定向 CTest 均为 `3/3 PASS`：

- `pae.tools.protocol_lab_ui.ascii_smoke_editor`
- `pae.tools.protocol_lab_ui.bounded_v08_state`
- `pae.tools.protocol_lab_ui.binary_public_h2`

Debug/Release 均从各自隔离部署目录以隐藏窗口执行并通过以下 UI smoke：

- Binary Host 0.9 双文档：`2 document(s)`；覆盖折叠锚点、Host publication、Flow 隔离、
  真实 Host 草稿 Encode/Apply/绑定选择/typed input/Encode、回切 Decode、G1 Encode/Decode 与
  中文确认框原枚举决策。
- Binary v0.5 + v0.6：`2 document(s)`；覆盖字段表—Hex 联动、失败诊断完整内容与状态属性。
- ASCII stream 单文档：`1 document(s)`；覆盖流状态分区、继续/重置可达性。
- ASCII stream 双文档 close-cancel：`2 document(s)`；覆盖中文 Yes/No 下的原取消语义。
- ASCII literal-only：`1 document(s)`；实际执行零字段 `TX_TEMPLATE` 属性与展示一致性断言。

Release 构建日志记录测试目标以 `/UNDEBUG` 覆盖 `/DNDEBUG`，断言未关闭。终态证据：

- `out/validation/lab-ui-layout-u2/{debug,release}-headless-final.log`
- `out/validation/lab-ui-layout-u2/{debug,release}-{binary-host,binary-basic,stream,stream-close,ascii-literal}-final.stdout.log`
- `out/validation/lab-ui-layout-u2/release-build.log`

本轮限定纠偏的终态证据使用 `refinement-` 前缀，旧日志未覆盖：

- `out/validation/lab-ui-layout-u2/refinement-{debug,release}-build-all.log`
- `out/validation/lab-ui-layout-u2/refinement-{debug,release}-headless.log`
- `out/validation/lab-ui-layout-u2/refinement-{debug,release}-{binary-host,binary-basic,stream,stream-close,ascii-literal}.stdout.log`

## 5. 未验证范围与边界

- 未启动供用户交互的可见 Lab；未进行 150% DPI、系统字体、人工布局或真实用户配置观察。
- 按派发要求未无限复跑已知依赖隐藏窗口焦点的完整 ASCII/v0.8 GUI smoke；对应
  `ascii_smoke_editor`、`bounded_v08_state` D/R 专项通过，literal-only timing UI 路径通过。
- 未搭建主题/翻译平台，未改变进程或全局 DPI 策略，未增加依赖。
- 未运行全仓测试、未覆盖 `deliverables/lab/98df5e0`、SDK 或 `local_private`。
- 未执行 Stage、Commit、Push、发布或删除。

结论：U2 已完成派发范围并具备 D/R 构建、定向专项、G1/Binary/stream/ASCII timing 隐藏式
UI smoke 证据；待总控复核。U3 未提前实施。
