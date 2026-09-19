# Qt Lab installed-SDK static 独立构建验证

## 1. 结论与边界

2026-09-19 在 `main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31` 加既有未提交 Lab/SDK 切片上，完成整个 Qt Lab 的 installed-SDK static Debug/Release 首片闭包：

- standalone 是仓库外顶层 CMake project；PAE 仅通过单个安装包根的 `find_package(PAE CONFIG REQUIRED ... NO_DEFAULT_PATH)` 和 `PAE::pae` 消费。
- Qt 仅来自仓库固定 Qt 5.13.0 副本的受控快照；yyjson 采用已批准方案 A，仅复制锁定的 0.12.0 source/license/lock，由一个 Lab-owned vendor target 编译。
- 最终源码快照 `F:/PersonalWorkspace/pae-lab-sdk-static-20260919-r8/` 的 static Debug/Release Testing-on 均 Configure、Build、32/32 Test 通过。
- 产品源码及 standalone CMake 相同的 r7 快照完成 static Debug/Release Testing-off、产品闭包检查、受控 Qt smoke 与 PAE/Qt/yyjson no-fallback 负向门禁。

这是本地 dirty-provenance static closure validation，不是 clean SDK、stable ABI、shared、Linux、正式发布或人工 UI 验收证据；本片未重打 SDK 五包。

## 2. 实现范围

### 2.1 Lab 产品闭包

- 新增 Lab-owned `lab_execution_observer.h`，standalone 不再依赖 private timing observer。
- `compile_worker`、`document_session`、`document_tab`、`description_mapping` 将 private payload、owner、mapping 和 fallback 限定在开发树 compatibility 路径；standalone 只走 public compile/adapter 路由。
- standalone 产品保留 0.5–0.8 complete Decode/Encode、0.9 public H2、0.10 public complete/Host、0.11 complete/stream 及既有 UI 行为；未新增 PAE public API，未把 private PAE/Core/Plan/Compiler 源带入产品。
- 测试注入与 allocation hook 只进入独立 test-only adapter/headless targets；产品 target 不携带测试宏。

### 2.2 独立入口与输入白名单

新增 `tools/protocol_lab_ui/standalone/`：

- `CMakeLists.txt`：独立 project、安装 SDK/Qt/yyjson 严格来源门禁、Testing-on/off 目标划分。
- `cmake/PaeLabQt513.cmake`：固定 Qt 5.13.0、D/R import library/DLL/plugin 和 MSVC toolset 检查。
- `cmake/DeployPaeLab.cmake`、`DeployPaeLabRun.cmake`：只向新建空目录部署匹配 EXE、Qt DLL/plugin 与配置白名单。
- `PrepareStandaloneInputs.ps1`：拒绝覆盖既有目标，复制显式 Lab 源/测试/config、static D/R SDK、固定 Qt 与锁定 yyjson，并生成 provenance/SHA-256 manifest。
- `CapturePaeLabModules.ps1`：只在脚本及其子进程内设置受控 PATH/Qt plugin 环境，按本次 PID 捕获实际 Qt 模块并比较加载/部署/输入三方哈希，最后只结束本次 PID。
- `CompareStandaloneSnapshot.ps1`：把仓库当前白名单文件重新计算 SHA-256，与指定外部快照逐项比较。
- `README.md`：记录 standalone 使用边界。

未修改根 CMake、`include/pae/**`、`src/**`、SDK 打包脚本或既有 `third_party` 内容。

## 3. 输入与来源

- 仓库基线：`c5b369298c79f27cf71dd2f73be9fbb4a4203a31`，工作树保留既有未提交变更。
- SDK 输入：`out/sdk-public-stream-description/candidate1-20260918/pae-sdk-static-{debug,release}`；包内 provenance 为 `source_head=dbf4798...`、`source_worktree_dirty=true`。
- 最终源码证据根：`F:/PersonalWorkspace/pae-lab-sdk-static-20260919-r8/`。
- 产品/负向证据根：`F:/PersonalWorkspace/pae-lab-sdk-static-20260919-r7/`。
- r7/r8 的 `inputs/lab` 逐文件 SHA-256 比较仅 `tests/protocol_lab_ui/description_mapping_tests.cpp` 不同；产品源、standalone CMake、SDK、Qt、yyjson 与配置输入相同。因此 r7 Testing-off/产品门禁只作为未变化产品闭包证据，最终测试源码的 Testing-on 结论仅取 r8。
- 交付前重新从仓库当前文件计算哈希，与 r8 中 97 个可直接映射回仓库路径的白名单文件比较，`mismatch_count=0`；证据为 `r8/logs/current-repository-whitelist-comparison.json`。`standalone-configs` 是生成文件或重复配置副本，不纳入直接路径比较。
- 当前 standalone 目录比 r8 快照多 `CapturePaeLabModules.ps1`、`CompareStandaloneSnapshot.ps1` 两个本轮补证脚本；它们不进入产品 target，也不改变 r8 已构建的产品源码/CMake 身份。除此以外，r8 白名单与仓库当前对应文件一致。

输入清单与哈希见各证据根的 `INPUT_PROVENANCE.json`、`INPUT_SHA256.json`。

## 4. 最终源码 Testing-on 验证

| 配置 | Configure | Build | CTest | 结果 |
| --- | --- | --- | --- | --- |
| Debug static | `r8/logs/debug-testing-on-configure.log` | `r8/logs/debug-testing-on-build.log` | `r8/logs/debug-testing-on-ctest.log` | 32/32 PASS |
| Release static | `r8/logs/release-testing-on-configure.log` | `r8/logs/release-testing-on-build.log` | `r8/logs/release-testing-on-ctest.log` | 32/32 PASS |

32 项覆盖 public Binary H1、ASCII A1/stream/noexcept、owned presentation/public façade、headless session/mapping/queue/ownership/dispatch、0.5–0.11 Qt smoke 和 close/cancel。Release build 日志对使用 `assert` 的测试 target 显示 `/DNDEBUG` 被 `/UNDEBUG` 覆盖，断言未被 Release 配置关闭。

开发树兼容性补充：在既有 `out/build/windows-msvc-lab-public-legacy-complete` Debug 构建中直接运行 `compile_queue`、`description_mapping`、`inspect_state`、`host_session` 四个可执行测试，均返回 0。该目录未向 CTest 注册这四项，因此没有用“未发现测试”冒充通过。

## 5. Testing-off 产品闭包

r7 的下列产品构建均成功：

- `logs/debug-testing-off-configure.log`、`debug-testing-off-build.log`
- `logs/release-testing-off-configure.log`、`release-testing-off-build.log`

生成工程复核结果：

- `PAE_LAB_BUILD_TESTING=OFF`，Debug/Release build tree 均无 `*TESTS.vcxproj`。
- 产品 `pae_protocol_lab_ui.vcxproj` 对测试宏、`ascii_*_compat`、`v06_execution`、private `binary_host_adapter.cpp` 的命中数为 0。
- `PAE_DIR` 分别严格位于当次 Debug/Release `PAE_SDK_ROOT` 内。

### 5.1 实际 Qt 模块来源补证

原 `*ui-smoke*.log` 只记录功能输出，不记录 Windows 实际加载模块，不能单独作为 Qt 来源证据。为此只启动 r7 Testing-off Debug/Release 部署各一次，不传 smoke 参数、不重复功能测试；新证据仅回答进程来源问题：

| 配置 | PID | 实际加载模块 | 结论 | 日志 |
| --- | ---: | --- | --- | --- |
| Debug | 18956 | `Qt5Cored.dll`、`Qt5Guid.dll`、`Qt5Widgetsd.dll`、`platforms/qwindowsd.dll` | 全部来自 `deploy/debug-testing-off/Debug`；加载/部署/`inputs/qt` 三方 SHA-256 一致 | `r7/logs/debug-testing-off-module-origin.json` |
| Release | 11072 | `Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`、`platforms/qwindows.dll` | 全部来自 `deploy/release-testing-off/Release`；加载/部署/`inputs/qt` 三方 SHA-256 一致 | `r7/logs/release-testing-off-module-origin.json` |

两次启动均将对应部署目录仅前置到脚本子进程 PATH，并把 `QT_PLUGIN_PATH`、`QT_QPA_PLATFORM_PLUGIN_PATH` 指向对应部署 `platforms`；没有修改全局/持久环境。日志记录 PID、EXE 绝对路径、环境值、每个模块的加载/部署/输入绝对路径及三份哈希。捕获完成后仅结束上述本次启动 PID，复核时两个 PID 均已不存在。

本补证是模块来源/哈希证据，不是新增功能通过。功能证据仍仅为前述 Testing-on CTest 与 Testing-off smoke；Debug 0.8 的一次偶发失败边界不变。

Testing-off 受控 Qt smoke：Release 六组全部通过；Debug 0.5/0.6、0.7、Binary/ASCII/stream 组通过。Debug 首次 0.8 运行曾出现一次 `focus-out did not commit corrected legal BYTES`，原日志保留于 `debug-testing-off-ui-smoke.log`；紧接的同目标重试及后续组通过，见 `debug-testing-off-ui-smoke-v08-retry.log`、`debug-testing-off-ui-smoke-rest.log`。本片未改该行为，不把一次重试通过写成已定位或已修复；r8 Debug Testing-on 的同一 0.8 smoke 也通过。

## 6. no-fallback 负向门禁

r7 以下门禁均按预期拒绝，保留日志：

- 缺失 SDK / SDK 根层级错误：`negative-sdk-missing.log`、`negative-sdk-wrong-root.log`。
- Qt 版本错误 / 必需 D/R binary 缺失：`negative-qt-wrong-version.log`、`negative-qt-missing-binaries.log`。
- yyjson 必需输入缺失 / hash 错误：`negative-yyjson-missing.log`、`negative-yyjson-wrong-hash.log`。
- Debug 包用于 Release、Release 包用于 Debug：`negative-debug-package-release-build.log`、`negative-release-package-debug-build.log`；均由配置错配 marker 在链接期 fail-closed。

这些检查没有命中开发 build tree、本机 Qt 5.11、package registry、系统 yyjson 或下载 fallback。

## 7. 未验证与剩余风险

- 未做 shared Debug/Release；没有 `pae.dll` 部署、实际加载来源或 DLL 缺失/错配证据。
- 未做 Linux、人工 UI、长期稳定性、Golden、硬件、现场或生产验证。
- Qt 正式外发形态、许可证/来源材料与发布包裁剪仍未闭合。
- 使用的是已知 dirty-provenance SDK 候选；当前结果不能升级为 clean release 或 stable ABI 声明。
- Debug 0.8 Testing-off smoke 的一次 focus-out 偶发现象尚未定位，应作为独立观察项，不阻塞本次静态包外闭包事实，也不能视为已解决。

## 8. Git 与停点

未 Stage、Commit、Push、发布或删除；未覆盖旧验证根或旧部署。r1–r8 均保留，其中 r8 是最终源码 Testing-on 证据，r7 提供未变化产品的 Testing-off、smoke 和负向门禁证据。

已完成派发范围，停止写入，待总控复核；shared 后片需另行授权。
