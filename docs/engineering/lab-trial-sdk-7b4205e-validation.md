# Lab 统一试用交付验证记录（7b4205e）

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 当前结论（2026-09-20，提交候选整理）

本报告按实施顺序保留失败、补充授权和续验记录；第1～8节描述各次执行当时的状态，不代表当前仍阻塞。最新证据与边界见第9节：static/shared 的 Debug 各33/33、Release各32/32测试通过，四组Testing-off新构建及模块来源验证完成。总控已核对四组CTest成功汇总，并现场复算18个模块的输入/部署/加载文件哈希一致，未重跑构建测试或逐份复核全部负例日志。

最后一次static Debug Testing-on增量构建在重复部署阶段触发禁止覆盖非空目录的保护，保留为操作停点，不写成该次构建成功；不影响此前已取得的实际测试证据。SDK为clean 7b4205e，Lab为该基线加本报告明确列出的未提交包装/测试/特性定义修补。尚未正式发布、替换现用交付或完成真实协议人工验证。

## 1. 首次执行结论（历史）

本轮在获批范围内完成了 standalone 输入闭包和 public A2 测试注册条件的最小修正，并建立了 static/shared 两份隔离输入快照。static Debug Testing-on 配置成功，33 项测试均已注册；但首次构建在测试执行前失败，因此本轮按停点停止，未形成统一试用交付通过结论。

失败原因是 standalone 同时启用了 ASCII stream observer 与 Binary public H2，却没有像仓内 UI CMake 一样派生中性的 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1`。结果 `document_session.h` 裁掉了 `OperationMode::STREAM_INSPECT`，而对应实现路径仍引用该枚举值。修复需要修改本轮未授权的 `tools/protocol_lab_ui/standalone/CMakeLists.txt`；本轮未越权修改，也未继续 Release、shared、Testing-off、模块来源或负向门禁。

## 2. 基线与边界

- 仓库：`<REPO_ROOT>`
- 分支与 HEAD：`main@7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`
- 固定 SDK 输入：`<TRIAL_ROOT>\sdk`
- Lab 隔离根：`<TRIAL_ROOT>\lab`
- 五类 SDK 包的 `PROVENANCE.json` 均记录 `source_head=7b4205e...`、`source_worktree_dirty=false`；本轮只消费，不重打包。
- 保留总控计划与 PAE 任务报告等既有改动；无 Stage、Commit、Push、发布、删除或旧部署替换。

## 3. 本轮实际修改

### 3.1 standalone 输入闭包

`tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1` 新增复制以下三个已存在的仓库输入：

- `tests/protocol_lab_ui/verify_crt_assert_probe.cmake`
- `tests/protocol_lab_binary/public_binary_stream_tests.cpp`
- `examples/config/synthetic_stream_framing_slice.pae.json`

static/shared 快照均已生成，三项输入均存在。

### 3.2 public A2 测试注册条件

`tests/protocol_lab_ui/CMakeLists.txt` 保留私有 ASCII adapter 的产品源条件不变，只将以下 public complete-record 消费测试改为在 `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER` 或 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2` 任一开启时注册：

- ASCII editor smoke
- ASCII complete smoke
- ASCII one-way smoke
- 上述 UI 测试所需的 qwindows platform 环境

未启用私有 adapter，未修改执行语义或诊断函数。

## 4. 输入一致性证据

执行两次 `PrepareStandaloneInputs.ps1` 均成功：

- `<TRIAL_ROOT>\lab\evidence\prepare-static.log`
- `<TRIAL_ROOT>\lab\evidence\prepare-shared.log`

两份快照分别与当前获批白名单比较：

- static：103 个直接映射文件，`mismatch_count=0`，无当前 standalone 白名单缺项。
- shared：103 个直接映射文件，`mismatch_count=0`，无当前 standalone 白名单缺项。

证据：

- `<TRIAL_ROOT>\lab\evidence\static-whitelist-comparison.json`
- `<TRIAL_ROOT>\lab\evidence\shared-whitelist-comparison.json`
- `<TRIAL_ROOT>\lab\static\INPUT_PROVENANCE.json`
- `<TRIAL_ROOT>\lab\shared\INPUT_PROVENANCE.json`

## 5. static Debug Testing-on 结果

### 5.1 配置与测试清单

配置成功：

- Visual Studio 18 2026
- x64
- v142 `14.29.30133`
- MSVC `19.29.30159`
- Windows SDK `10.0.22621.0`

配置日志：

- `<TRIAL_ROOT>\lab\static\logs\debug-testing-on-configure.log`

构建前 `ctest -N -V` 共列出 33 项测试，包含 Binary public/stream、ASCII public A1/A2、ASCII editor、ASCII complete/one-way、CRT probe、Qt smoke、headless session 等目标。此清单只证明测试已生成和注册；由于尚未构建，可执行文件缺失提示是预期现象，不是测试结果。

清单日志：

- `<TRIAL_ROOT>\lab\static\logs\debug-testing-on-inventory.log`

### 5.2 首次构建失败

构建退出码为 1。代表性错误：

```text
document_session.cpp(350,32): error C2838: “STREAM_INSPECT”: 成员声明中的限定名称非法
document_session.cpp(351,15): error C2065: “STREAM_INSPECT”: 未声明的标识符
```

相同问题出现在产品 headless 目标和多个测试 headless 目标。完整日志：

- `<TRIAL_ROOT>\lab\static\logs\debug-testing-on-build.log`

源码与构建条件对应关系：

- `tools/protocol_lab_ui/document_session.h:56-57` 仅在 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI` 定义时声明 `OperationMode::STREAM_INSPECT`。
- `tools/protocol_lab_ui/document_session.cpp:350,397,662,724` 的已启用 ASCII/Binary stream 路径引用该枚举值。
- 仓内 `tools/protocol_lab_ui/CMakeLists.txt:253-257` 在 ASCII stream observer 或 Binary public H2 开启时定义 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1`。
- standalone `tools/protocol_lab_ui/standalone/CMakeLists.txt` 定义了 `PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2=1` 和 `PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER=1`，但没有派生 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1`。

因此，这是 Lab standalone 的 feature definition 不闭合；当前证据不能把它归因于固定 SDK 包，也不能据此宣称任何测试通过或失败。

## 6. 停止点与待决策

最小候选修正是：在 standalone 的 `pae_lab_features` 上，按与仓内 UI CMake 相同的条件，在 ASCII stream observer 或 Binary public H2 开启时定义 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1`。该文件不在本轮授权修改范围内，故本轮只报告，不实施。

以下项目均未执行或未完成：

- static Debug Testing-on 的测试执行
- static Release Testing-on
- shared Debug/Release Testing-on
- static/shared Testing-off 构建
- DLL/Qt plugin 模块来源捕获
- 缺失/错配负向门禁
- UI 人工体验与全仓测试

本轮已停止写入；等待总控决定是否授权上述 standalone CMake 最小闭包修正后，再从首次失败构建继续串行验证。

## 7. `STREAM_UI` 增补授权与续跑结果

总控随后明确授权仅修改 `tools/protocol_lab_ui/standalone/CMakeLists.txt`：在 standalone 已设置的 feature 变量之后，当 `PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER` 或 `PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2` 开启时，只对 `pae_lab_features` 增加 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1`。本次补丁未修改产品执行源码、根 CMake、PAE/API/Schema，也未启用 private adapter。

static/shared 两份既有隔离输入只同步了该自有 CMake 副本，并更新对应 `INPUT_SHA256.json` 条目：

- 新文件长度：14797 bytes
- 新 SHA-256：`1A7CD9066919D5200B14884494B2D3BACB80D7406EE47420F31AC7428EEC2E2D`
- static 清单：2539 项，重新计算后零缺失、零长度或哈希差异。
- shared 清单：2531 项，重新计算后零缺失、零长度或哈希差异。
- 两份快照与现场仓库直接映射的 103 个 Lab 文件均零哈希差异。

增量核对日志：

- `<TRIAL_ROOT>\lab\static\logs\stream-ui-fix-input-verification.log`
- `<TRIAL_ROOT>\lab\shared\logs\stream-ui-fix-input-verification.log`

static Debug Testing-on 重新配置成功，生成的 VS 工程已包含 `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1`；原 `STREAM_INSPECT` 编译错误不再出现。证据：

- `<TRIAL_ROOT>\lab\static\logs\stream-ui-fix-debug-testing-on-configure.log`
- `<TRIAL_ROOT>\lab\static\logs\stream-ui-fix-debug-testing-on-build.log`

但同一次续跑构建在链接阶段发现新的独立 feature definition 缺口并退出 1：

```text
binary_stream_qt_smoke_tests.obj : error LNK2019: unresolved DocumentTab::BinaryStateSignatureForSmoke
binary_stream_qt_smoke_tests.obj : error LNK2019: unresolved DocumentTab::VerifyBinaryStreamForSmoke
application_window.obj : error LNK2019: unresolved DocumentTab::VerifyHostForSmoke
application_window.obj : error LNK2019: unresolved DocumentTab::VerifyBinaryHostStage1ForSmoke
application_window.obj : error LNK2019: unresolved DocumentTab::BinaryStateSignatureForSmoke
application_window.obj : error LNK2019: unresolved DocumentTab::VerifyBinaryReloadFailureForSmoke
```

源码条件核对表明：

- `document_tab.h` 会因 standalone 已有的 `HOST_OBSERVER`、`BINARY_UI` 和 `BINARY_PUBLIC_H2` 宏声明上述 smoke 方法。
- `application_window.cpp` 与 Binary stream smoke 测试也会在相同已启用 feature 下引用这些方法。
- `document_tab.cpp:1549-2254` 则把这些方法定义整体置于 `PAE_BUILD_PROTOCOL_LAB_BINDING_UI` 条件内。
- 仓内 UI CMake 在 Host observer 或 Binary public H2 开启时派生 `PAE_BUILD_PROTOCOL_LAB_BINDING_UI=1`；standalone 当前没有对应派生。

因此第二次失败不是原 `STREAM_UI` 补丁失效，而是随后暴露出的另一个 standalone feature definition 闭包缺口。修正它需要新增本轮未授权的 `PAE_BUILD_PROTOCOL_LAB_BINDING_UI` 派生；本轮未自行扩大修改。

按“再次出现范围外缺口或失败即准确停报”的停点要求，未执行任何测试，也未继续 static Release、shared Debug/Release、Testing-off、模块来源三方哈希或负向门禁。首次失败日志和本次续跑日志均保留，未覆盖。当前仍是 `7b4205e + 未提交闭包/测试补丁`，不构成 clean SDK 或统一试用交付通过结论。

## 8. feature definition 一致性收尾与第三次续跑

总控进一步授权对 standalone 已启用功能做一次完整的特性定义一致性检查，只允许补齐有仓内规则依据的遗漏派生定义。对照结论如下：

| 类别 | CMake 功能变量条件 | 仓内传播目标/预处理宏 | standalone 状态与处理 |
| --- | --- | --- | --- |
| 产品直接能力 | `PUBLIC_LEGACY_COMPLETE`、`BINARY_UI`、`BINARY_PUBLIC_H2`、`ASCII_PUBLIC_A1/A2/STREAM/STREAM_UI`、`ASCII_STREAM_OBSERVER`、`HOST_OBSERVER` | 仓内按具体 target 传播同名宏；standalone 由 `pae_lab_features` 传播 | 当前能力均已显式启用；未机械增加其他功能 |
| stream UI 派生 | `ASCII_STREAM_OBSERVER OR BINARY_PUBLIC_H2` | `PAE_BUILD_PROTOCOL_LAB_STREAM_UI=1` | 已在第7节补齐并保留 |
| binding UI 派生 | `HOST_OBSERVER OR BINARY_PUBLIC_H2` | `PAE_BUILD_PROTOCOL_LAB_BINDING_UI=1` | 本节确认是唯一剩余遗漏，按相同条件补齐 |
| private adapter | `ASCII_ADAPTER` | 私有 adapter target、兼容层及同名宏 | standalone 保持 `OFF`，未用于规避公开路径问题 |
| 测试专用 | `PAE_LAB_BUILD_TESTING` | `PAE_PROTOCOL_LAB_ASCII_PUBLIC_A1_TEST_HOOKS`、`PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION` | 继续只定义于测试 target，未传播到产品能力选择 |
| smoke diagnostic | `ASCII_SMOKE_DIAGNOSTIC` | 仅产品 target 的诊断宏/源 | standalone 未启用，也没有因本轮检查而新增 |

这里的 CMake 变量决定是否构建/接线能力，预处理宏决定对应源码条件；二者不能互相替代。standalone 使用 `pae_lab_features` 统一向其自有产品和测试闭包传播当前已启用能力，但没有因此改变 SDK、PAE 公共契约或 private adapter 边界。

本节唯一新增修正位于 `tools/protocol_lab_ui/standalone/CMakeLists.txt`：

```cmake
if(PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER OR PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2)
  target_compile_definitions(pae_lab_features INTERFACE PAE_BUILD_PROTOCOL_LAB_BINDING_UI=1)
endif()
```

两份隔离输入同步后的 standalone CMake 身份：

- 长度：14982 bytes
- SHA-256：`D68ADCD269C4F6E5C1ACC718258CD7091B3022B6EA5E8574753F2C73A8BDD885`
- static 2539 项、shared 2531 项 manifest 全量重算均零差异。
- static/shared 各自与现场仓库直接映射的 103 个 Lab 文件均零差异。

输入核对日志：

- `<TRIAL_ROOT>\lab\static\logs\feature-consistency-input-verification.log`
- `<TRIAL_ROOT>\lab\shared\logs\feature-consistency-input-verification.log`

### 8.1 static Debug Testing-on

- 配置：通过。
- 构建：通过；此前 `STREAM_INSPECT` 编译错误和 `DocumentTab` 未解析符号均不再出现。
- 测试清单：33 项。
- 实际执行：33/33 通过。

证据：

- `static/logs/feature-consistency-debug-testing-on-configure.log`
- `static/logs/feature-consistency-debug-testing-on-build.log`
- `static/logs/feature-consistency-debug-testing-on-inventory.log`
- `static/logs/feature-consistency-debug-testing-on-ctest.log`

### 8.2 static Release Testing-on 新停点

- 配置：通过。
- 构建：通过；测试 target 的 `/UNDEBUG` 警告明确显示 Release 断言测试仍启用，未通过关闭断言刷绿。
- 测试清单：33 项。
- 实际执行：32/33 通过；唯一失败为 `pae.tools.protocol_lab_ui.ui_smoke_crt_assert_probe`。

失败输出显示 Release 产品返回 2，并把 `--ui-smoke-crt-assert-probe` 当作配置路径处理，缺少 `UI_SMOKE_CRT_FAIL_FAST`。只读核对表明：

- `main.cpp` 中 CRT report hook 与 probe 分支均只在 `_WIN32 && _MSC_VER && _DEBUG` 下编译。
- Release 产品 VS 工程明确包含 `NDEBUG`，不包含 `_DEBUG`，因此不会进入该 Debug-only 探针。
- `tests/protocol_lab_ui/CMakeLists.txt` 为测试设置了 `CONFIGURATIONS Debug`，但生成的多配置 `CTestTestfile.cmake` 仍在 Release 分支注册并实际执行该测试。

这不是产品 feature definition 漏传，也不是关闭 Release 断言导致普通测试失效；它是 Debug-only CRT probe 的 Release 注册/执行语义问题。修正需要更改测试注册方式或探针契约，属于本轮允许的 standalone 特性定义一致性修正之外，因此未修改、未略过失败后继续刷绿。

证据：

- `static/logs/feature-consistency-release-testing-on-configure.log`
- `static/logs/feature-consistency-release-testing-on-build.log`
- `static/logs/feature-consistency-release-testing-on-inventory.log`
- `static/logs/feature-consistency-release-testing-on-ctest.log`

按新停点要求，shared Debug/Release Testing-on、全部 Testing-off、模块输入/部署/加载三方哈希和负例矩阵均未继续。本轮仍是 `7b4205e + 未提交闭包/测试补丁`，SDK 只读且未重打，不构成完整矩阵通过或发布结论。

## 9. CRT probe 注册修正与矩阵续跑

总控随后授权只修正 CRT probe 的配置注册方式：将 `CONFIGURATIONS Debug` 从 `set_tests_properties(... PROPERTIES ...)` 移入 `add_test(NAME ...)` 命名参数，保留探针命令、断言、fail-fast marker 与失败判断，不修改 `main.cpp` 或产品逻辑。

修改后的 `tests/protocol_lab_ui/CMakeLists.txt` 身份：

- 长度：18292 bytes
- SHA-256：`59CAE5685F7A4C75CBF83BCEAC51EB8D5EF43F9A95A47E11F7A26CB99E55A85B`

static/shared 隔离输入同步后：

- static 2539 项、shared 2531 项 manifest 全量重算零差异。
- 两份快照各有 103 个可直接映射 Lab 文件与现场仓库零差异。
- 证据：两根 `logs/crt-registration-input-verification.log`。

### 9.1 实际测试名称与 Testing-on

重新配置后逐项提取了真实测试名称，而非只比较数量：

- Debug：33 项，明确包含 `pae.tools.protocol_lab_ui.ui_smoke_crt_assert_probe`。
- Release：32 项，明确不包含该 probe；其余名称等于 Debug 清单去除 probe 后的集合。

正向执行结果：

| SDK 形态 | Debug | Release |
| --- | --- | --- |
| static | 33/33 PASS | 32/32 PASS |
| shared | 33/33 PASS | 32/32 PASS |

shared Debug/Release Testing-on 均在本次修正后重新配置和完整构建；构建仍出现已知 C4251（每份日志162处），不阻止同 MSVC/v142 工具链测试，但不证明稳定 ABI。shared Release 测试 target 仍以 `/UNDEBUG` 覆盖 `/DNDEBUG`（18处 warning），没有通过关闭断言刷绿。

证据位于两根 `logs/` 下的：

- `crt-registration-{debug,release}-testing-on-configure.log`
- `crt-registration-{debug,release}-testing-on-build.log`（shared 正向构建；static 见下述增量重建停点）
- `crt-registration-{debug,release}-testing-on-inventory.log`
- `crt-registration-{debug,release}-testing-on-ctest.log`

### 9.2 Testing-off 与运行模块来源

四组产品态均在本次修正后使用独立 Testing-off build/deploy 目录完成配置和构建：

| SDK 形态 | Debug | Release |
| --- | --- | --- |
| static | Configure/Build PASS | Configure/Build PASS |
| shared | Configure/Build PASS | Configure/Build PASS |

`CapturePaeLabModules.ps1` 隐藏启动产品并捕获实际模块：

- static D/R：各4个 Qt 模块，全部从各自部署目录加载，Qt 输入/部署/加载三方哈希一致；部署和 `dumpbin /dependents` 均不含 `pae.dll`。
- shared D/R：各5个模块（`pae.dll` + Qt Core/Gui/Widgets/qwindows），全部从各自部署目录加载；SDK 输入、部署副本、进程实际加载文件三方哈希一致；`dumpbin /dependents` 明确包含 `pae.dll`。
- 四个捕获 PID 均已终止，无残留 Lab 进程。

证据：

- `crt-registration-{debug,release}-testing-off-{configure,build}.log`
- `crt-registration-{debug,release}-testing-off-module-origin.json`
- `crt-registration-{debug,release}-testing-off-dumpbin-dependents.log`

### 9.3 no-fallback 负例

所有负例均使用独立 build/deploy 或新派生副本，不修改固定 SDK 输入或正向部署：

| 负例 | 实际结果 |
| --- | --- |
| static 包却期望 SHARED | Configure 以 package kind mismatch 拒绝 |
| shared 包却期望 STATIC | Configure 以 package kind mismatch 拒绝 |
| static Debug 包构建 Release | 链接期命中 `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED` 并失败 |
| static Release 包构建 Debug | 同上 |
| shared Debug 包构建 Release | 同上 |
| shared Release 包构建 Debug | 同上 |
| 派生 shared Debug 包缺少 `bin/pae.dll` | Configure 以 imported runtime 不存在拒绝 |
| 派生 shared Release 包放入 Debug `pae.dll` | Configure 以 SDK manifest hash mismatch 拒绝 |
| 派生 Release 部署放入 Debug `pae.dll` | 模块捕获在创建进程前以 runtime source hash mismatch 拒绝；未生成模块 JSON |

对应证据均使用 `logs/crt-registration-negative-*` 前缀。负例完成后固定 SDK 仍只读，未重打包。

### 9.4 最终增量重建停点

在上述矩阵与负例完成后，为给 static Testing-on 补同前缀 build 日志，又对已有 static Debug Testing-on build/deploy 执行了一次增量构建。编译/链接目标均已完成，但部署脚本按其不可覆盖契约拒绝非空的既有部署目录：

```text
Refusing to overwrite non-empty deployment directory: .../static/deploy/debug-testing-on/Debug
```

该失败发生在部署 post-build 阶段，不是源码编译、链接、测试或 SDK 失败；此前同一产品闭包在 feature-consistency 阶段已构建成功，本次注册修正只改变 CTest 注册，重新配置后的 Debug 33/33 与 Release 32/32 也已实际通过。不过，按照“其他类型失败立即停报”要求，本轮没有改写部署脚本、清理/覆盖旧部署、切换新部署根或继续补 static Release 增量 build 日志。

因此本节的限定结论是：CRT 注册语义、四组 Testing-on 实测、四组 Testing-off 新构建、模块三方来源与既定负例均已有正向证据；但最后一次 static Debug Testing-on 重复部署触发了预期的 non-overwrite 门禁，保留为操作停点，不能把本轮称为无条件完整重跑。当前仍为 `7b4205e + 未提交包装/测试/特性定义修补`，不是 clean 同提交成品或正式发布。
