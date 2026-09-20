# Protocol Metadata Qt Smoke 启动与诊断修复记录（2026-09-20）

## 1. 结论

- 本轮已修复仓库构建下 Qt window smoke 的启动入口：CTest 现在运行对应配置的完整部署目录，而不是缺少 Qt DLL/plugin 的 `bin/<Config>` 可执行文件。
- `--ui-smoke` 在构造 `QApplication` 前启用仅限当前进程的 Windows/MSVC 诊断设置，将 CRT 报告导向 stderr，并关闭系统错误对话框路径；普通启动和 performance 路径不进入该设置。
- Qt Windows platform plugin 的文件名是 `qwindows[d].dll`，但 platform key 是 `windows`。首次定向运行使用错误 key `qwindows`，日志明确报告可用 key 为 `windows`；修正后最小 Debug Binary smoke 通过并输出 `UI_SMOKE_PASS`。
- 扩展到 Debug `window` 标签定向组后，8 项中 6 项通过、2 项返回 UI smoke 行为失败。失败分别是 ASCII Enter commit 未形成独立 TX template bytes，以及 Schema 0.8 focus-out 未提交修正后的合法 BYTES。两项根因未归属，隐藏窗口的焦点行为和测试驱动方式也在待核对范围；本轮不修改 ASCII/v08 业务，因此没有运行 Release smoke。
- 后续定向补证确认：仅设置 `_CRTDBG_MODE_FILE` 不能证明 Debug CRT `_ASSERTE` 必然终止。smoke-only 诊断现对 `_CRT_ASSERT/_CRT_ERROR` 报告执行“输出原报告后非零 fail-fast”，隔离真实 `_ASSERTE` probe 已证明输出、非零退出、未继续执行且无残留进程。

当前状态是“Qt smoke 启动/诊断基础设施已修复；Debug 定向验证发现两个既有 UI 行为失败，待总控决定后续范围”，不是 Qt window smoke 全部通过，也不是 UI 验收完成。

## 2. 基线与范围

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 分支与 HEAD：`main@fa81329563dd3aea9bb167ef9bd34526a2606161`
- 新构建根：`out/build/windows-msvc-qt-smoke-repair-20260920`
- 新证据根：`out/validation/qt-smoke-repair-20260920`
- 工具链：MSVC v142 `14.29.30133`，`cl 19.29.30159.0`，仓库 Qt `5.13.0`
- 实际修改：
  - `tests/protocol_lab_ui/CMakeLists.txt`
  - `tests/protocol_lab_ui/verify_crt_assert_probe.cmake`
  - `tools/protocol_lab_ui/main.cpp`
  - 本记录
- 未修改 PAE API、Schema、SDK、Host、stream、业务/UI 行为实现；未覆盖旧 G2-C 部署。
- 既有 metadata naming、文档和并行变更全部保留；本轮未 Stage、Commit、Push、发布、替换部署或删除文件。

## 3. 最小修复

### 3.1 CTest 使用 matching-config 部署闭包

仓库构建下 `pae_lab_ui_smoke_executable` 改为：

```cmake
${PROJECT_BINARY_DIR}/out/protocol_lab_ui/$<CONFIG>/pae_protocol_lab_ui.exe
```

因此 Debug/Release 测试分别使用自身配置的 EXE、Qt DLL 和 `platforms/qwindows[d].dll`。Standalone 分支仍优先使用既有 `PAE_LAB_UI_SMOKE_EXECUTABLE` override，本轮没有改变其入口契约。

所有 window smoke 显式设置 `QT_QPA_PLATFORM=windows`。这里的 `windows` 是 Qt platform key；实际加载文件仍是 `qwindows.dll` 或 `qwindowsd.dll`。

### 3.2 仅 `--ui-smoke` 的进程内诊断

`main.cpp` 在创建 `QApplication` 前扫描原始 argv。仅当存在明确的 `--ui-smoke` 时：

- 设置当前进程的 `SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX`；
- 将 MSVC error mode 定向到 stderr；
- 保留 abort 文本、关闭 ReportFault 路径；
- Debug CRT 的 warn/error/assert 报告定向到 stderr；
- Debug CRT `_CRT_ASSERT/_CRT_ERROR` 通过 smoke-only report hook 输出实际报告文本后调用 `_Exit(EXIT_FAILURE)`，保证不会从报告点继续；
- 输出并刷新 `UI_SMOKE_DIAGNOSTICS stderr=enabled dialogs=disabled` 标记。

该路径没有禁用或吞掉断言，没有将异常或失败转换为 PASS，也没有修改注册表、系统 PATH、全局 Qt 或 dump 配置。`--ui-smoke` 窗口同时使用 `Qt::WA_DontShowOnScreen`；普通启动仍保持原行为。测试专用参数 `--ui-smoke-crt-assert-probe` 只有与 `--ui-smoke` 同时出现、且仅在 Windows MSVC Debug 构建中才触发真实 `_ASSERTE`，不会进入 `QApplication`。

本机 Windows SDK `10.0.22621.0` 的 `ucrt/crtdbg.h` 显示，Debug `_ASSERTE` 展开后仅在 `_CrtDbgReportW(...)` 返回 `1` 时调用 `_CrtDbgBreak()`；因此原先单独设置 FILE report mode 只确定输出路由，不能作为非零终止证明。本轮 report hook 补齐了这一控制流缺口。对应头文件路径和宏展开摘录固化在 `crt-header-control-flow.log`。

## 4. 验证结果与证据

### 4.1 配置与构建

以下步骤成功：

```powershell
cmake --preset windows-msvc-stage4-h2 `
  -B out/build/windows-msvc-qt-smoke-repair-20260920 `
  -T v142,version=14.29.30133

cmake --build out/build/windows-msvc-qt-smoke-repair-20260920 `
  --config Debug --target pae_protocol_lab_ui

cmake --build out/build/windows-msvc-qt-smoke-repair-20260920 `
  --config Release --target pae_protocol_lab_ui
```

证据：

- `configure.log`
- `reconfigure-platform-key-fix.log`
- `debug-build.log`
- `release-build.log`
- `runtime-identity.log`（Debug/Release EXE、Qt DLL 和 Windows platform plugin 的尺寸与 SHA-256）

### 4.2 启动诊断与最小 Debug gate

初次最小运行保留在 `debug-single-qt-smoke.log`。它使用错误的 `QT_QPA_PLATFORM=qwindows`，退出 `0xc0000409`；stderr 明确报告：

```text
Available platform plugins are: windows.
```

这证明 plugin 目录已被发现，同时定位了 platform key 错误。修正为 `windows` 后，CTest 命令记录在 `debug-test-command-platform-key-fix.log`，最小 Binary smoke 记录在 `debug-single-qt-smoke-platform-key-fix.log`：

```text
UI_SMOKE_DIAGNOSTICS stderr=enabled dialogs=disabled
UI_BINARY_HOST_SMOKE_DOCUMENT index=1 fields=9
UI_BINARY_HOST_SMOKE_DOCUMENT index=2 fields=9
UI_SMOKE_PASS detail=2 document(s)
```

结果：1/1 PASS，CTest exit code 0；日志 2395 bytes；运行后无 `pae_protocol_lab_ui` 残留进程。

### 4.3 Debug window 定向组与停止条件

命令：

```powershell
ctest --test-dir out/build/windows-msvc-qt-smoke-repair-20260920 `
  -C Debug -V --output-on-failure -L '^window$'
```

结果记录在 `debug-window-smoke.log`：8 项中 6 PASS、2 FAIL，CTest exit code 8；全部测试均输出诊断标记，运行后无残留进程。

失败项：

1. `pae.tools.protocol_lab_ui.qt_smoke_ascii`

   ```text
   UI_SMOKE_FAIL detail=ASCII document 1: ASCII Enter commit did not produce independent TX template bytes
   ```

2. `pae.tools.protocol_lab_ui.qt_smoke_v08`

   ```text
   UI_SMOKE_FAIL detail=document 1: bounded Schema 0.8 UI state differs: focus-out did not commit corrected legal BYTES
   ```

这些失败已进入应用自有 smoke 判定，未出现 Qt plugin 初始化错误或进程挂留，但当前证据不足以把它们直接归因为产品缺陷。隐藏窗口的 focus/commit 行为与测试驱动方式仍需独立核对；本轮保持未归因且不修改 ASCII/v08 业务。

### 4.4 Debug CRT 真实断言隔离 probe

新增测试 `pae.tools.protocol_lab_ui.ui_smoke_crt_assert_probe` 仅对 MSVC Debug 配置生效。CMake 包装器启动 matching-config 部署 EXE，并要求同时满足：

- 出现 `UI_SMOKE_DIAGNOSTICS stderr=enabled dialogs=disabled`；
- 出现 `UI_SMOKE_CRT_FAIL_FAST`；
- stderr 包含真实 `_ASSERTE` 表达式 `PAE_UI_SMOKE_CRT_ASSERT_PROBE`；
- 子进程结果非零；
- 不出现断言之后的 `UI_SMOKE_CRT_ASSERT_CONTINUED`。

定向结果记录在 `crt-assert-probe-debug.log`：子进程 result 为 `1`，包装测试 1/1 PASS，耗时 0.04 秒；断言报告已写入 stderr，未执行 continuation 标记，运行后无 `pae_protocol_lab_ui` 残留进程。配置与构建记录为 `crt-assert-probe-reconfigure.log`、`crt-assert-probe-debug-build.log`。

补证后的普通 Debug Binary smoke 记录在 `crt-assert-probe-debug-normal-smoke.log`：1/1 PASS，仍输出两个 Binary document 结果和 `UI_SMOKE_PASS`，运行后无残留进程。这说明 fail-fast hook 在没有 CRT assert/error 时不改变该正常 smoke 路径。

## 5. 未验证与边界

- Release 部署闭包曾在 fail-fast 补证前构建并记录身份；新增 hook/probe 均受 `_DEBUG`/Debug test 配置约束，本轮没有重建或运行 Release smoke，也没有运行 Release 全矩阵。
- Standalone override 由 CMake 差异确认仍保留，未新建或重跑 standalone SDK 消费构建。
- 旧 G2-C 二进制没有本轮 pre-`QApplication` 诊断设置，无法保证运行时不出现可见 CRT 对话框，因此没有重跑；旧部署对比保持未验证。
- 未运行 UI 全仓、全量 CTest、人工可见窗口、offscreen、SDK 五包重打、发布或部署替换。
- Debug 两项 UI smoke 行为失败保持未归因；产品行为、隐藏窗口焦点语义和测试驱动方式均未在本轮核对或修改。

## 6. 交付状态

已完成本轮允许的启动入口、smoke-only CRT fail-fast 诊断及真实断言定向补证，并停止写入。当前为“已完成派发范围，待总控复核”。
