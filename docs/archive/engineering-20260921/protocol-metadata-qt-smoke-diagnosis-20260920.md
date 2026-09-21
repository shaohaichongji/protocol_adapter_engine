# 协议元数据命名整理 Qt smoke 证据缺口诊断（2026-09-20）

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 1. 范围与现场

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 基线：`main@fa81329563dd3aea9bb167ef9bd34526a2606161`
- 当前工作树包含已授权的协议元数据命名修改、文档和其他并行变更，本任务全部保留。
- 本片只做静态闭包/命令核对和一次受控启动尝试，未修改功能源码、CMake、Qt、
  系统环境、旧部署或既有日志。

证据根：
`out/validation/protocol-metadata-qt-smoke-diagnosis-20260920`。

## 2. CTest 命令与 Qt 闭包事实

### 2.1 CTest 实际命令

Debug/Release 的 `pae.tools.protocol_lab_ui.qt_smoke_binary_stage1` 都直接启动当前命名构建根
`bin/<Config>/pae_protocol_lab_ui.exe`，参数均为：

```text
--ui-smoke <synthetic_binary_ui_stage1.pae.json> <synthetic_binary_ui_stage1.pae.json>
```

CTest 工作目录为当前构建根的 `tests/protocol_lab_ui`。`bin/Debug` 和 `bin/Release`
只有 EXE（Debug 另有 PDB），没有 Qt DLL 或 platform plugin。完整闭包位于各自
`out/protocol_lab_ui/<Config>`，其中有 Qt DLL 及 `platforms/qwindows[d].dll`。

### 2.2 plugin 能力

仓库 Qt 及当前/旧 G2-C common 部署都只有：

- Debug：`qwindowsd.dll`
- Release：`qwindows.dll`

找不到 `qoffscreen.dll` 或其他 offscreen platform plugin。因此原先在
`QT_QPA_PLATFORM=offscreen` 下的异常不能直接当作应用 smoke 结果。

项目已有的隐藏机制是：进程环境存在 `PAE_LAB_UI_SNAPSHOT_PATH` 时，`main.cpp` 在
`show()` 前设置 `Qt::WA_DontShowOnScreen`。本次因此选择 qwindows + 该机制，而不是继续盲跑
不存在的 offscreen plugin。

## 3. 当前与 G2-C common 可比性

当前命名构建根与旧 G2-C common 的相关 CMake cache 逐项比较，差异数为 0。两者都是：

- UI/Stage1/Host endpoint ON。
- Binary H1/H2/UI ON，Binary materializer OFF。
- private ASCII adapter/stream observer/Host observer ON。
- ASCII public A1/A2/public stream/public stream UI OFF。
- Schema V05-V11 ON。

同一配置下当前与旧构建的 Qt5Core/Gui/Widgets 和 qwindows plugin SHA-256 分别一致。
每个构建的 `bin` EXE 与自己的部署 EXE 哈希一致，但当前与旧构建的 EXE 哈希不同：

- current Debug：`7C2771DD...E717AA`
- baseline Debug：`5835FE60...CDBB8`
- current Release：`6F93D7F8...A78F8D`
- baseline Release：`54BBA2D4...E8E402`

这些事实表明可以在相同开关和相同 Qt 输入下设计对照，但不能因为“退出码相同”
就自动得出同一因果；EXE 本身不同，仍需要有效 smoke 输出或更精确证据。

## 4. 原有失败的证据分类

1. `out/validation/protocol-metadata-20260920/ctest-debug-targeted.log` 非空，其中完整应用测试以
   `0xc0000135` 失败。结合 CTest 直接运行无 Qt DLL 的 `bin/Debug` 事实，这是环境启动/
   DLL 闭包失败，不是已执行的 UI smoke 失败。
2. `ctest-debug-qt-stage1-rerun.log` 非空，记录 offscreen 环境下 `SEGFAULT`，但仓库与部署
   没有 offscreen plugin。该结果不足以证明应用逻辑失败，也不能用来与 G2-C common
   做因果对照。
3. 原先声称的 4 份 current/baseline D/R 对照日志仍不存在，本轮不改写这一事实。

## 5. 受控 qwindows 隐藏尝试

新增诊断脚本 `run-hidden-qt-smoke-comparison.ps1`，原计划在同一工作目录、同一参数、
同一进程级环境策略下按 current Debug / baseline Debug / current Release / baseline Release
各运行一次。各 case 只计划改变匹配自身的 EXE/DLL/plugin 根和快照文件名；系统
`PATH` 未修改，只在子进程 PATH 前缀中加入对应闭包。

实际只启动了第一个 `current-debug` case：

- 完整命令、cwd、进程环境、EXE/Qt/plugin/fixture 哈希已写入
  `current-debug-metadata.log`。
- 进程启动后观察到 PID 14828 的窗口标题为 `Microsoft Visual C++ Runtime Library`，
  说明 `WA_DontShowOnScreen` 与 hidden process 只能隐藏 Qt 主窗，不能保证 CRT 对话框不可见。
- 为遵守“无用户可见窗口”和失败停报要求，随即外部终止该进程与诊断脚本；
  baseline Debug 和两个 Release case 均未启动。
- 记录的退出值为十进制 `-1`、十六进制 `0xFFFFFFFF`，这是外部强制终止结果，
  不是应用 smoke 退出码。
- `current-debug-stdout.log` 和 `current-debug-stderr.log` 实际落盘但均为 0 bytes，快照也未生成。
  这不满足“日志非空”验收条件，所以本轮没有任何可记账的实际 smoke 结果。

该尝试没有获得 CRT 断言正文，因此不能确定对话框的源头，也不能将它归因于命名整理。

## 6. 结论

### 6.1 环境启动失败

- 原 CTest `0xc0000135` 可归类为 `bin/Debug` 缺 Qt DLL 闭包的启动失败。
- offscreen 环境不受当前仓库 Qt plugin 集支持，对应 SegFault 不能当作有效应用 smoke。
- qwindows 闭包尝试避免了第一层 DLL 缺失，但 Debug CRT 对话框又使“全程无可见窗口”
  的对照方案失效。

### 6.2 实际 smoke 结果

本轮没有获得 `UI_SMOKE_PASS`、`UI_SMOKE_FAIL` 或应用自主退出码。current Debug 是外部
终止，其余三个 case 未启动。因此完整应用 Qt smoke 仍未验证通过，也没有新增
功能失败证据。

### 6.3 基线比较能与不能证明的内容

当前和 G2-C common 的 cache 一致，Qt/plugin 输入哈希一致，这些静态事实排除了“两个
构建使用不同 Qt 闭包或功能开关”这种简单差异。但由于 baseline 未实际运行、current 也没有
有效 smoke 结果，仍不能证明命名整理引入或未引入 Qt 应用回归。原“旧产物同错所以不是
本次引入”的结论继续保持撤回状态。

## 7. 证据与未验证边界

新证据：

- `ctest-command-inventory.log`
- `qt-closure-cache-and-hash-inventory.log`
- `run-hidden-qt-smoke-comparison.ps1`
- `current-debug-metadata.log`
- `current-debug-interruption-observation.log`
- `current-debug-stdout.log`（0 bytes，明确为无效运行日志）
- `current-debug-stderr.log`（0 bytes，明确为无效运行日志）

未验证：

- baseline Debug、current Release、baseline Release 未启动。
- current Debug CRT 断言正文、源头和命名整理关系未知。
- 未新建构建、未改代码、未开启 dump、未修改系统 PATH/Qt/旧部署。
- 未 Stage、Commit、Push、SDK 打包、部署替换或删除。

当前状态：证据缺口已分解为“原 CTest DLL 闭包失败”、“不受支持的 offscreen 环境”和
“qwindows Debug CRT 对话框且无有效输出”三层；完整应用 smoke 与基线对照仍未闭合，需要新的
无对话框诊断方案或明确修复授权后再继续。
