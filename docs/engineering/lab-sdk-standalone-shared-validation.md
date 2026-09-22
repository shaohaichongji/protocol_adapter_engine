# Qt Lab installed-SDK shared Debug/Release 验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 结论与证据边界

2026-09-19 在 `main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31` 加全部既有未提交实现上，完成 Qt Lab installed-SDK shared Debug/Release 的本地包外闭包验证：

- standalone 显式接受 `STATIC` 或 `SHARED`，shared 仍只通过安装包 `find_package(PAE CONFIG REQUIRED ... NO_DEFAULT_PATH)` 与 `PAE::pae` 获取同配置 import library/runtime；失败不回退 static、开发树或系统路径。
- shared Debug/Release Testing-on 均 Configure、Build、原 32 项 CTest 通过；Release 断言保持有效。
- shared Debug/Release Testing-off 均 Configure、Build 通过，产品无测试注入/private compatibility；部署包含匹配 `pae.dll`。
- `dumpbin /DEPENDENTS` 与进程模块取证均确认产品依赖并实际加载 `pae.dll`；PAE/Qt/plugin 的输入、部署、加载文件哈希一致。
- 缺/错 SDK、包类型、D/R 包错配、runtime 缺失、另一配置 runtime 替换均 fail-closed。
- shared 改动后的 static Debug Testing-off 最小回归通过，部署仍无 `pae.dll` 依赖。

这是 dirty-provenance 候选的本地 closure validation，不是 clean release、stable ABI、正式分发、Linux 或长期稳定性证据。

## 2. 实现范围

本轮只修改 `tools/protocol_lab_ui/standalone/`：

- `CMakeLists.txt`
  - 新增 `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED`，默认保持 `STATIC`。
  - 校验 `PAE_LIBRARY_KIND`、`PAE_PACKAGE_CONFIGURATION` 和 `PAE::pae` 当前配置的 import library/runtime 绝对路径均位于精确 `PAE_SDK_ROOT`。
  - shared `pae.dll` 必须与包内 `SHA256SUMS.txt` 的 `bin/pae.dll` 条目一致。
  - shared 测试可执行文件统一输出到独立目录，并在配置期放置同一已校验 SDK runtime；不依赖开发树或全局 PATH。
- `PrepareStandaloneInputs.ps1`
  - 新增显式 `-PackageKind static|shared`；仅复制相应 D/R 两包，static 默认行为保持。
  - provenance 记录 package kind 和对应两包原始 provenance。
- `cmake/DeployPaeLab.cmake`、`DeployPaeLabRun.cmake`
  - shared 部署从 `PAE::pae` 解析的 runtime 路径复制 `pae.dll`，复制前后校验 SHA-256；static 不部署 PAE DLL。
- `CapturePaeLabModules.ps1`
  - 可选纳入预期 `pae.dll`；启动前先比较 SDK 输入与部署 runtime 哈希，再记录实际加载路径和输入/部署/加载三方哈希。
- `README.md`
  - 增补 static/shared 显式选择和 shared runtime 边界。

未修改任何协议执行、展示或测试业务源码；未修改根 CMake、公共 PAE/Core、SDK 包、Qt、yyjson、旧部署或总控文档。

## 3. 输入身份与外部根

- SDK 候选：`out/sdk-public-stream-description/candidate1-20260918/pae-sdk-shared-{debug,release}`。
- 两包 provenance：`source_head=dbf4798f96a45b8d36a4754ad5a01e274680337e`、`source_worktree_dirty=true`，Debug `/MDd`、Release `/MD`。
- 最终证据根：`<LOCAL_WORK_ROOT>/pae-lab-sdk-shared-20260919-r2/`。
- 首次根 `<LOCAL_WORK_ROOT>/pae-lab-sdk-shared-20260919/` 保留一次 Configure 失败证据：尝试从父目录给子目录测试设置属性违反 CMake 测试目录作用域；未进入 Build。最终实现改为同源 DLL 邻接测试可执行文件，未覆盖失败根。
- `r2/INPUT_PROVENANCE.json`、`INPUT_SHA256.json` 记录输入身份与哈希。
- 交付前从当前仓库重新计算 99 个可直接映射白名单文件的 SHA-256，与 r2 快照比较，`mismatch_count=0`、无额外 standalone 文件；见 `logs/current-repository-whitelist-comparison.json`。`standalone-configs` 为生成/重复配置副本，按脚本说明不作直接路径比较。

## 4. shared Testing-on 功能验证

| 配置 | Configure | Build | CTest | 结果 |
| --- | --- | --- | --- | --- |
| Debug | `logs/debug-testing-on-configure.log` | `logs/debug-testing-on-build.log` | `logs/debug-testing-on-ctest.log` | 32/32 PASS |
| Release | `logs/release-testing-on-configure.log` | `logs/release-testing-on-build.log` | `logs/release-testing-on-ctest.log` | 32/32 PASS |

32 项沿用 static 首片的 public adapter、headless、0.5–0.11 Qt smoke 与 close/cancel 矩阵。Release build 日志含 19 处 `/UNDEBUG` 覆盖 `/DNDEBUG`，使用 `assert` 的测试未被 Release 配置关闭。Debug 0.8 smoke 本轮一次通过；没有复现历史 AV 或 focus-out 偶发，因此未追加重跑、修复或稳定性声明。

测试 runtime 位于各 build tree 的 `test-bin/<Config>/pae.dll`。`logs/product-isolation-and-runtime-audit.json` 证明其 SHA-256 与对应 SDK 输入、Testing-off 部署三方一致：

- Debug：`C46478EE0783FC415ABB83BFB469177E04A1C36CC95CD0E96E6BF9DCAC70AF8C`
- Release：`6171E3920CAC0874E802E69C3EF24DCED3CF00FD7BA6B18951674F315743B169`

## 5. Testing-off、链接与产品隔离

Debug/Release Testing-off 的 Configure/Build 日志分别为：

- `logs/debug-testing-off-configure.log`、`debug-testing-off-build.log`
- `logs/release-testing-off-configure.log`、`release-testing-off-build.log`

`logs/product-isolation-and-runtime-audit.json` 记录：

- D/R 均 `PAE_LAB_BUILD_TESTING=OFF`、0 个 `*TESTS.vcxproj`。
- 产品工程对测试宏、private ASCII compatibility、`v06_execution`、private `binary_host_adapter.cpp` 的命中均为 0。
- `PAE_DIR` 位于对应 shared SDK 根，`PAE_LAB_EXPECTED_LIBRARY_KIND=SHARED`。
- SDK runtime、测试 runtime、部署 runtime 哈希一致。

`logs/import-library-audit.json` 证明生成的 D/R 产品工程分别含对应 SDK 根下的 `lib/pae.lib` 绝对路径；没有点名仓库开发库。D/R import library SHA-256 均为 `3B0FCA016FDAF30B9CDC72F33C08E71C36C1E56558AD581567558E6D96959BF4`。

`dumpbin /DEPENDENTS`：

- Debug 产品依赖 `Qt5Widgetsd.dll`、`pae.dll`、`Qt5Guid.dll`、`Qt5Cored.dll`；见 `logs/debug-testing-off-dumpbin-dependents.log`。
- Release 产品依赖 `Qt5Widgets.dll`、`pae.dll`、`Qt5Gui.dll`、`Qt5Core.dll`；见 `logs/release-testing-off-dumpbin-dependents.log`。

## 6. 实际模块来源与局部环境

只启动 Testing-off Debug/Release 部署各一次，不传 smoke 参数；模块捕获完成后只结束本次 PID：

| 配置 | PID | 实际模块 | 结果 | 日志 |
| --- | ---: | --- | --- | --- |
| Debug | 4612 | `pae.dll`、`Qt5Cored.dll`、`Qt5Guid.dll`、`Qt5Widgetsd.dll`、`platforms/qwindowsd.dll` | 全部从 Debug 部署目录加载，输入/部署/加载三方哈希一致 | `logs/debug-testing-off-module-origin.json` |
| Release | 12984 | `pae.dll`、`Qt5Core.dll`、`Qt5Gui.dll`、`Qt5Widgets.dll`、`platforms/qwindows.dll` | 全部从 Release 部署目录加载，三方哈希一致 | `logs/release-testing-off-module-origin.json` |

捕获脚本只在自身及子进程内把部署目录前置 PATH，并设置 `QT_PLUGIN_PATH`、`QT_QPA_PLATFORM_PLUGIN_PATH` 为对应部署 `platforms`；没有修改全局/持久环境。两个 PID 复核时均已不存在。此处是来源证据，功能证据仍以第 4 节 CTest 为准。

## 7. no-fallback 负向门禁

全部使用新替代输入、build 或 deployment；没有删除、覆盖原 SDK/runtime/部署：

| 门禁 | 预期拒绝点 | 证据 |
| --- | --- | --- |
| SDK 不存在 / 传 SDK 父目录 | 精确安装包根检查 | `negative-sdk-missing.log`、`negative-sdk-wrong-root.log` |
| 期望 SHARED 却传真实 STATIC 包 | package kind 不匹配 | `negative-static-kind-mismatch-real.log` |
| Debug/Release runtime 缺失 | `PAE::pae` runtime 必须存在 | `negative-{debug,release}-runtime-missing.log` |
| SDK 内 `pae.dll` 换成另一配置 DLL | 实际 DLL 与包内 `SHA256SUMS.txt` 不一致 | `negative-{debug,release}-other-runtime.log` |
| Debug 包构建 Release / Release 包构建 Debug | SDK configuration guard marker 链接失败 | `negative-debug-package-release-build.log`、`negative-release-package-debug-build.log` |
| 已部署 `pae.dll` 换成另一配置 DLL | 启动前 SDK 输入/部署来源哈希不一致 | `negative-debug-other-runtime-capture-2.log`、`negative-release-other-runtime-capture-1.log` |

另一配置 DLL 替换测试没有依赖 Windows loader 自行拒绝：SDK 输入替换由 manifest hash 拒绝，部署替换在创建进程前由来源 hash 拒绝。

Qt/yyjson 检查逻辑未变，沿用 static 首片的错版本、缺 binary、缺输入及 hash 不符 fail-closed 证据；本轮未无意义重跑这些不变门禁。

## 8. static 最小回归

因 standalone CMake/部署共享逻辑发生变化，在新根 `<LOCAL_WORK_ROOT>/pae-lab-sdk-static-regression-20260919/` 执行 static Debug Testing-off 最小回归：

- Configure/Build PASS：`logs/debug-testing-off-configure.log`、`debug-testing-off-build.log`。
- Qt 模块来源/哈希捕获 PASS：`logs/debug-testing-off-module-origin.json`。
- `dumpbin` 仅含 Qt/CRT/system 依赖，不含 `pae.dll`：`logs/debug-testing-off-dumpbin-dependents.log`。

未重跑 static 32 项完整矩阵；其功能证据仍属于已收口 static 首片。

## 9. 未验证与风险

- shared 消费编译对 SDK 公共头出现 MSVC C4251 警告，涉及导出类持有 STL/PImpl 成员；本轮 D/R 构建和运行通过不能把该警告升级为稳定 ABI 证明。
- 未做 Linux、人工 UI、长期稳定性、Golden、硬件、现场或正式发布验证。
- Qt 正式外发许可与打包形态仍未闭合。
- SDK 为 dirty provenance；没有重打包、修改或发布。
- 历史 AV 与 Debug 0.8 focus-out 偶发仍是独立观察边界；本轮未复现不等于解决。

## 10. Git 与停止点

未 Stage、Commit、Push、发布或删除；未修改全局环境；未覆盖 static/shared 旧根或旧部署。已完成派发范围，停止写入，待总控复核。
