# Lab UI U2B 左右工作台验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

> 2026-09-19 限定返修更正：初版名为 `workbench-1280x820.png` 的文件实际为
> **1420×820**，初版日志只打印请求尺寸，不能作为 1280×820 证据。该原文件与日志保留为错误历史
> 证据；本报告第 4 节以带 `-refinement` 后缀的严格尺寸证据替代，不删除或覆盖原证据。

## 1. 范围与基线

- 仓库：`<REPO_ROOT>`
- 现场基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`
- 保留现场全部 G1、U1、U2 与总控未提交改动；未覆盖 U2 构建目录、
  `deliverables/lab/98df5e0`、SDK 或 `local_private`。
- U2B 实际修改：`tools/protocol_lab_ui/application_window.{h,cpp}`、`main.cpp`、
  `document_tab.{h,cpp}` 与本报告。未修改 Session、Adapter、Core、public API、Schema、
  root CMake、依赖、主题、Qt 或全局 DPI 策略。

## 2. 实现结论

1. 顶部“文档与编译”压缩为文件名、加载/编译状态及三个自然宽度按钮；完整路径由
   `configPathToggle` 展开，原 `configPath` 仍可编辑、选择和复制。
2. 主区使用 `workbenchSplitter` 左右分区，初始尺寸按约 30/70 设置且允许用户拖动。
   左侧 `controlTabs` 含“操作”和“绑定设置”两页，各自使用局部 `QScrollArea`；没有新增
   整窗滚动容器。
3. 操作页使用表单行配对“当前动作 / Pipeline / Message / 输入表示”，主操作按钮保持自然宽度；
   当前 Host 绑定/动作、Flow 和状态固定在操作页，不随绑定设置草稿折叠或切页消失。
4. 绑定设置页复用唯一 `hostBindingDraft`、增删和 Apply 控件；没有复制绑定模型、Session 或编辑器。
5. 右侧结果摘要下方以 `resultWorkspaceSplitter` 上下分区：上部是字段表，下部是“字段详情 / 报文字节 /
   诊断与计时 / 流状态”Tab。字段表保留 8 个原始列并设置常用列初始宽度；宽度不足时由字段表自身
   横向滚动，未删除、合并或伪造技术数据。
6. 字段选择继续调用原 `RefreshFieldDetails`，同步 `HexView` 高亮；测试明确在“报文字节”页选字段后
   仍停留该页，不强制抢回“字段详情”。
7. 展示 Tab 往返复用原控件树，不触发编译、Decode、Encode、Flow 切换或 model 重建。Binary Host
   真实 Encode 控件路径在已有有效结果上切换左右 Tab 后，草稿文本、结果字节、状态签名和成功状态均不变。
8. 稳定 `objectName`、`itemData`、中文术语、ID、配置文本和原技术 detail 保留；新增布局锚点为
   `configPathToggle`、`workbenchSplitter`、`controlTabs`、`operationScroll`、`bindingScroll` 和
   `resultWorkspaceSplitter`。

## 3. 构建与自动验证

隔离构建根：`out/build/windows-msvc-lab-ui-u2b`。配置显式使用 Visual Studio 18 2026、x64、
`v142,version=14.29.30133` 与仓内 Qt 5.13；配置日志：
`out/validation/lab-ui-workbench-u2b/configure.log`。

Debug/Release 均成功构建：

- `pae_protocol_lab_ui`
- `pae_protocol_lab_ui_ascii_smoke_editor_tests`
- `pae_protocol_lab_ui_bounded_v08_state_tests`
- `pae_protocol_lab_ui_binary_public_h2_tests`

构建日志：`out/validation/lab-ui-workbench-u2b/{debug,release}-build-final.log`。三个既有专项目标继续
保留其原有 `/UNDEBUG` 编译设置；本轮没有修改测试 CMake。

Debug/Release 定向 CTest 均为 `3/3 PASS`：

- `pae.tools.protocol_lab_ui.ascii_smoke_editor`
- `pae.tools.protocol_lab_ui.bounded_v08_state`
- `pae.tools.protocol_lab_ui.binary_public_h2`

证据：`out/validation/lab-ui-workbench-u2b/{debug,release}-headless-final.log`。

Debug/Release 从各自新部署目录以隐藏窗口执行以下五组 smoke，全部 PASS：

- Binary Host 0.9 双文档：真实 Decode/Encode 绑定入口、Flow、布局锚点及有效 Encode 结果上的 Tab 往返。
- Binary v0.5 + v0.6：字段选择、Hex 高亮、全部字段列和失败诊断。
- ASCII stream 单文档：流状态页与局部滚动。
- ASCII stream 双文档：close-cancel 原枚举与状态保持。
- ASCII literal-only：零字段 `TX_TEMPLATE` 展示语义。

证据：`out/validation/lab-ui-workbench-u2b/{debug,release}-{binary-host,binary-basic,stream,stream-close,ascii-literal}-final.{stdout,stderr}.log`。

尺寸返修后仅按改动范围增量构建 D/R `pae_protocol_lab_ui`，并重跑 Binary Host 双文档真实控件路径，
D/R 均 PASS；未无理由重复整套已通过矩阵：

- `out/validation/lab-ui-workbench-u2b/refinement-{debug,release}-build.log`
- `out/validation/lab-ui-workbench-u2b/refinement-{debug,release}-binary-host.{stdout,stderr}.log`

## 4. 隐藏渲染截图

仅当设置 `PAE_LAB_UI_SNAPSHOT_PATH/WIDTH/HEIGHT` 时，进程使用 `WA_DontShowOnScreen`，在 smoke
已有结果阶段将同一主窗口 `render` 为 PNG；普通启动和普通 smoke 不改变窗口行为。

初版错误定位不是推测。`minimum-width-before-fix.stdout.log` 实测：

- window `minimumSizeHint=1420×439`
- `documentConfigGroup minimumSizeHint=1400×66`
- `workbenchSplitter minimumSizeHint=623×256`

因此实际最小宽度来自顶部同一水平行叠加完整文件名、完整 Schema/协议状态与三个按钮，不是字段表或
工作台 Splitter。返修将状态移到摘要第二行，使文字可换行/压缩并保留完整 tooltip；文件名占剩余空间，
完整路径仍由原控件展开编辑和复制。操作输入说明允许换行，长 Pipeline/Message/Host binding ComboBox
允许在窄工作台中缩小，完整当前值和下拉项未删除。返修后 window `minimumSizeHint=643×469`，
`documentConfigGroup minimumSizeHint=428×96`。

捕获现在在 `resize` 和事件处理后比较实际窗口尺寸；不一致即以
`snapshot size mismatch: requested=... actual=...` 失败，不写 `UI_SNAPSHOT_PASS`。成功日志明确同时打印
`requested` 与 `actual`。严格尺寸终态证据：

- `workbench-1280x820-refinement.png`：PNG 像素实测 **1280×820**；日志
  `refinement-snapshot-1280x820.{stdout,stderr}.log` 记录 `requested=1280x820 actual=1280x820`。
- `workbench-1920x1080-refinement.png`：PNG 像素实测 **1920×1080**；日志
  `refinement-snapshot-1920x1080.{stdout,stderr}.log` 记录 `requested=1920x1080 actual=1920x1080`。

目视结果：真实 1280×820 下文件名与状态可见，左右分区、局部纵向滚动和下部结果 Tab 可用，字段表
通过自身横滚访问尾部技术列；1920×1080 下 8 个字段列可同时显示，左侧操作与 Host 当前上下文保持
固定，右侧字段表和下部结果区没有互相挤压成窄栏。该截图是 Qt 隐藏渲染，不等于真实显示器 DPI、
系统字体或人工交互验收。

## 5. 未验证范围与边界

- 未启动可见 Lab，未关闭或操作用户进程；未进行真实鼠标拖动、键盘编辑、150% DPI、系统字体、
  多显示器或用户真实配置人工验收。
- 未无限复跑已知依赖隐藏焦点的完整 ASCII/v0.8 GUI 路径；对应定向专项和本轮受影响 smoke 已通过。
- 未运行全仓测试、SDK/包外消费或硬件/现场验证；U3 未启动。
- 未执行 Stage、Commit、Push、发布或删除。

结论：U2B 已完成派发范围，具备 D/R 构建、定向专项、五组隐藏 UI smoke、两种尺寸隐藏渲染与
目视检查证据；待总控复核及后续短时真实体验。
