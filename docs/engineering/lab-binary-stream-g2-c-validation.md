# Lab Binary 流式 G2-C Session/UI 验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

日期：2026-09-20

仓库：`<REPO_ROOT>`
基线：`main@04fa429e1e8361b89f8622e2dc998d8869309c1c`

## 1. 范围与现场

本片在未提交的 G2-A owner、G2-B DTO/adapter 之上接入 Qt Lab Session/UI。开始与补充授权后均重查
分支、HEAD 和工作树；保留总控计划、G2-A/B 源码、测试及报告，未回退、覆盖或清理既有变更。

G2-C 原始 Session/UI 实现文件包括：

- `tools/protocol_lab_ui/document_session.h/.cpp`
- `tools/protocol_lab_ui/document_tab.h/.cpp`
- `tools/protocol_lab_ui/CMakeLists.txt`
- `tools/protocol_lab_ui/binary_host_adapter_public.cpp`
- `tests/protocol_lab_ui/CMakeLists.txt`
- `tests/protocol_lab_ui/binary_public_h2_tests.cpp`
- 新增 `tests/protocol_lab_ui/binary_stream_qt_smoke_tests.cpp`
- `cmake/PaeQt513.cmake`

总控补充授权后，为闭合 Binary-only 矩阵，又最小调整：

- root `CMakeLists.txt`：取消“Stage1 强制的 V10/V11 编译能力等于 Lab ASCII 消费者”的两处粗粒度门禁；
  真实 ASCII stream UI 和 observer 仍由原有专项依赖门禁约束。
- `tools/protocol_lab_ui/CMakeLists.txt`、`tests/protocol_lab_ui/CMakeLists.txt`：ASCII compat target、链接、测试和部署
  fixture 改为只由实际 ASCII adapter 开关启用；不改默认选项。
- `schema_dispatch.cpp`、`description_mapping.h/.cpp`、`document_session.h/.cpp`：只将 Lab ASCII 消费分支的
  编译条件改为实际 adapter 或已有 standalone public-only 消费者；PAE 的 V10/V11 编译能力保持开启。
- `binary_public_h2_tests.cpp`：增加 ASCII adapter OFF 时应拒绝 ASCII UI 路由的独立期望。

未修改 PAE Core/API/Schema、Stage1 强制能力、默认选项、SDK、旧部署或本机 Qt；未新增产品选项或独立入口。

## 2. 实现结论

- Binary `STREAM_CHUNK` 绑定发布后进入本地协议中立的 `STREAM_INSPECT` 工作台；ASCII stream
  observer 仍由原开关控制，Binary 不再借用 ASCII 专属 UI 宏。
- 保持既有中文左右工作台，提供“提交输入块 / 继续 / 重置流”、三种 framing 策略状态、M/C/work、
  buffer/frozen cursor、累计候选/成功/失败/丢弃/malformed、Reset 和上一步 Host 事实。
- 每个 UI 动作只调用一次 owner step。编辑器在 Continue 可用时只展示 Lab 草稿；Continue 消费 owner
  冻结后缀或内部工作，不读取修改后的草稿。
- Flow/工作台 Tab 切换只重映射 owner/sidecar 当前状态，不执行 Host。每个 Flow 的草稿、结果、
  framing 状态和 Reset 相互隔离；两个 Document Tab 也独立。
- 无候选/idle 显示为“本步无候选”，不伪造失败；Decode/materialization failure 清除旧成功结果，
  并保留 G2-B 已确认的 Host 消费事实。输入预检拒绝不执行 Host，也不改变既有 owner 状态。
- Qt Hex 解析临时 `vector` 按 `capacity()` 作为 mapping 峰值输入，与 active result/cache 共存计费；
  stream 编辑器文本上限按有效 chunk 上限表达，未提升为 RSS 承诺。
- canonical `synthetic_stream_framing_slice.pae.json` 加入受限部署白名单；原文件未改。

## 3. common 验证

构建根：`out/build/windows-msvc-lab-g2-c/common`。使用 preset `windows-msvc-stage4-h2`、显式
`-T v142,version=14.29.30133`；配置输出确认 MSVC 19.29.30159、工具目录 `14.29.30133`、仓库 Qt 5.13.0。

原 G2-C 实现在 Debug/Release 各通过 7 项限定矩阵：G1 complete、G2 owner、H2 adapter/Session、
public header、hidden Qt smoke、ASCII stream Session、Host Session。证据：

- `out/validation/lab-binary-stream-g2-c/common-debug-build.log`
- `out/validation/lab-binary-stream-g2-c/common-debug-tests.log`
- `out/validation/lab-binary-stream-g2-c/common-release-build.log`
- `out/validation/lab-binary-stream-g2-c/common-release-tests.log`

Binary-only 门禁修正后重新配置 common，只重建/重跑直接受影响的 4 项：
`binary_public_h2`、`binary_stream_qt_smoke`、`ascii_stream_session`、`host_session`。
Debug 和 Release 均为 `4/4 PASS`，证明原 ASCII stream/Host 消费路径仍在：

- `out/validation/lab-binary-stream-g2-c/common-reconfigure-after-binary-only-gate.log`
- `out/validation/lab-binary-stream-g2-c/common-debug-affected-build.log`
- `out/validation/lab-binary-stream-g2-c/common-debug-affected-tests.log`
- `out/validation/lab-binary-stream-g2-c/common-release-affected-build.log`
- `out/validation/lab-binary-stream-g2-c/common-release-affected-tests.log`

新增 hidden Qt smoke 不调用 `show()`，以真实 `DocumentTab` 覆盖三策略、Submit/Continue/Reset、
Flow/工作台往返、失败清旧结果和双 Document Tab 隔离。仓库 Qt 无 `offscreen` plugin，故用
qwindows hidden 模式。Release 日志显示 `/DNDEBUG` 被 `/UNDEBUG` 覆盖，专项断言未关闭。

common Release 部署 `out/build/windows-msvc-lab-g2-c/common/out/protocol_lab_ui/Release` 的 EXE、
Qt5Core/Gui/Widgets、qwindows 和 stream fixture 均与本构建/仓库输入 SHA-256 一致：
`out/validation/lab-binary-stream-g2-c/common-deployment-hash-verification.txt`。

## 4. Binary-only 配置与验证

构建根：`out/build/windows-msvc-lab-g2-c/binary-only`。配置缓存明确为：

- `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=OFF`
- `PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER=OFF`
- `PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER=OFF`
- `PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2=ON`

正例配置已成功生成；反例单独启用真实 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI=ON`、
保持 ASCII stream observer OFF，仍被 root 原有专项门禁拒绝，退出码 1。证据：

- `out/validation/lab-binary-stream-g2-c/binary-only-configure-fixed.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-cache-switches.log`
- `out/validation/lab-binary-stream-g2-c/config-negative-ascii-stream-missing-observer.log`

Debug 和 Release 均构建 UI 及限定专项，并各通过 `binary_public_h2`、`binary_public_header`、
`binary_stream_qt_smoke`，结果为 `3/3 PASS`。Release 同样出现 `/UNDEBUG` 覆盖证据。首次 Debug
运行准确暴露 H2 测试仍把 V10 编译能力当作 ASCII adapter，其 180 秒超时日志保留；按实际
adapter OFF 期望修正测试后重跑通过。证据：

- `out/validation/lab-binary-stream-g2-c/binary-only-debug-build.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-debug-tests.log`（保留的修正前失败）
- `out/validation/lab-binary-stream-g2-c/binary-only-debug-budget-fix-build.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-debug-tests-fixed.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-release-build.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-release-tests.log`

Binary-only Release 部署仅含 V05-V08、Binary complete 和 Binary stream fixture，不含 ASCII fixture；清单见
`out/validation/lab-binary-stream-g2-c/binary-only-deployed-config-inventory.txt`。EXE、Qt DLL、qwindows 和
stream fixture 哈希均一致：`out/validation/lab-binary-stream-g2-c/binary-only-deployment-hash-verification.txt`。

原授权外门禁阻塞日志 `out/validation/lab-binary-stream-g2-c/binary-only-configure-blocked.log` 保留，
作为修正前历史证据，不再代表当前结论。

### 4.1 A2 独立消费边界核对

总控续派后只读核对了以下组合，未改代码/CMake：

- UI、public A1、public A2、Stage1、Host endpoint 开启；
- ASCII adapter、ASCII public stream、ASCII public stream UI、ASCII stream observer、Host observer 关闭。

全新根 `out/build/windows-msvc-lab-g2-c/config-a2-boundary` 成功 configure/generate，说明当前 root
依赖门禁不拒绝该组合。生成项目定义了 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2=1`，没有定义
`PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=1`。静态调用链则证明：

1. `CompileWorker` 因 A2 开启会调用 `ClassifySchemaVersion`，并在 `CLASSIFICATION_FAILED` 时不尝试编译。
2. `schema_dispatch.cpp` 的 Schema 0.10 分支只在 ASCII adapter 或 standalone public-only 条件下编译。
3. 该配置两个条件都不具备，因此 0.10 必然落入“unsupported for this UI build”。

结论：这不是配置失败或已证实的构建失败，而是可成功生成、但 A2 对 0.10 静默不可用的
矛盾组合。由于静态证据已充分，按派发要求未完整编译。

最小方向建议：优先支持该组合，把 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2` 明确视为实际 ASCII UI
消费者，并在 dispatch/Session/相应 fixture 与测试条件中一致处理；这与 A2“通过 public PAE”的
选项语义一致，也不应为了绕过本问题强制依赖 private ASCII adapter。若总控决定 A2 本就必须依赖
private adapter，则应在 root 增加明确拒绝，而不应保留当前静默不支持状态。本轮不先行修正。

证据：

- `out/validation/lab-binary-stream-g2-c/config-a2-boundary-configure.log`
- `out/validation/lab-binary-stream-g2-c/config-a2-boundary-cache-switches.log`
- `out/validation/lab-binary-stream-g2-c/config-a2-boundary-static-audit.log`

此外，先前在明确禁止修改 `schema_dispatch.cpp` 时未先停报即修改的范围偏差保留；后续总控
允许将已发生的最小条件修正纳入审查，不改变该偏差事实，也不构成继续扩大实现授权。

### 4.2 A2 组合明确拒绝收尾

用户后续授权本轮不实现 A2 与旧 adapter 解耦，而是先消除上述静默不支持组合。根
`CMakeLists.txt` 在 A2 依赖检查附近新增明确门禁：当 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2=ON`
且 `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=OFF` 时拒绝配置。诊断明确说明，这是当前仓库 Lab UI 的
构建组合约束，不表示 PAE public API 本身依赖 private Lab ASCII adapter。未改默认选项、
Stage1 能力、运行源码、standalone、API/Schema/Qt/SDK 或部署。

配置验证：

- 沿用 4.1 的 A2 精确组合，现在在新门禁处退出 1，未被其他条件误拒；原先成功生成的
  `config-a2-boundary-configure.log` 保留为修正前证据，新证据为
  `config-a2-boundary-explicit-gate.log`。
- common 重新 configure/generate 成功；关键开关为 ASCII adapter ON、A2 OFF、ASCII stream observer ON、
  Host observer ON、Binary H2 ON。
- Binary-only 重新 configure/generate 成功；关键开关为 ASCII adapter/A2/ASCII stream observer/
  Host observer OFF、Binary H2 ON。

证据：

- `out/validation/lab-binary-stream-g2-c/config-a2-boundary-explicit-gate.log`
- `out/validation/lab-binary-stream-g2-c/common-reconfigure-after-a2-gate.log`
- `out/validation/lab-binary-stream-g2-c/common-cache-switches-after-a2-gate.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-reconfigure-after-a2-gate.log`
- `out/validation/lab-binary-stream-g2-c/binary-only-cache-switches-after-a2-gate.log`
- `out/validation/lab-binary-stream-g2-c/root-cmake-after-a2-gate-sha256.txt`

本轮仅改配置门禁，未重新编译、运行 Debug/Release 功能矩阵或重建部署。第 3、4 节的
旧功能日志仍对未变的运行源码有效，但不将旧 `final-source-sha256.txt` 中的 root CMake 哈希
冒充为当前输入；新 root CMake 哈希已单独记录。A2 彻底脱离旧 adapter 留待后续单独决策。

## 5. 未验证与状态

- 用户已完成第6节的可见窗口三组短体验；未系统验证布局/DPI或真实业务配置。旧 `qt_smoke_*` 可见主窗口不属于本片 hidden 证据。
- 未跑全仓、SDK 包外、Linux、硬件、现场或发布验证。
- 未 Stage、Commit、Push、发布或删除；两个新部署根均未覆盖旧部署。
- 运行源码历史哈希见 `final-source-sha256.txt`；收尾后的 root CMake 哈希另见
  `root-cmake-after-a2-gate-sha256.txt`。`git diff --check` 通过，暂存区为空。

当前状态：G2-C common 和 Binary-only 原限定范围已有自动证据，已完成总控限定复核及第6节人工体验，本片限定收口；A2 ON/ASCII adapter OFF 的静默不支持组合已改为明确配置拒绝，A2 解耦未实施。未提交或推送，不升级为正式发布。

## 6. 用户人工短体验（2026-09-20）

用户明确反馈“全部符合，Lab 已关闭”。使用 common Release 部署及其 configs/synthetic_stream_framing_slice.pae.json，显式应用 device / Decode / fixed_rx 绑定。

1. Flow0 分块提交 `AA` 后无候选，继而提交 `01 02`，逻辑值为258。
2. 提交 `AA 03 04 AA 05 06` 后逻辑值772，输入框只读；点击继续后逻辑值1286，继续禁用且输入框恢复可编辑。
3. Flow1 提交 `AA 07 08` 得1800；返回Flow0保留原粘包草稿和1286结果；重置Flow0不影响Flow1的1800结果。

步骤纠偏：总控最初要求在继续前人工改成FF，但实际UI调用setReadOnly锁定输入，用户无法照做；随后修正步骤并完成。自动隐藏测试通过setPlainText程序写入只读控件，只证明冻结后缀不被程序改变的草稿替换，不证明用户可在该状态编辑。本次不以人工验证“冻结期间修改草稿”记账，也不修改当前只读行为。

此体验不覆盖另外两种策略的人工操作、全部关闭确认分支、A2公开路径、真实协议或系统DPI；三策略自动证据保持第3、4节原边界。不追加重复人工验收。
