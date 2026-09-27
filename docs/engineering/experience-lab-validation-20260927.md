# adeae30 纯净体验包配套 Lab 限定验证

## 身份与范围

- 产品源码：`adeae30d942ba42cc518c244c220730b7e462d2c`；本轮使用的 `out/build/experience-sdk-adeae30-20260927/clean-source` 同 HEAD 且干净。主仓库为同 HEAD 的共享工作树，原有总控、SDK、文档变更均未改动。
- 使用固定源码的 `tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1`，未改源码、CMake、脚本、安装 Qt 或原 SDK 包。新输入、构建和日志分别位于 `out/build/experience-lab-adeae30-20260927/`、`out/evidence/experience-lab-adeae30-20260927/`。
- 原脚本要求同时存在 `pae-sdk-static-debug` 和 `pae-sdk-static-release`。经总控补充授权，同轮 `static-yaml-Debug-install` **仅作准备阶段辅助输入**；`static-yaml-Release-install` 为实际 CMake、链接和运行来源。两包按原名复制到新输入根，未用 Release 伪装 Debug。Debug 包未进入最终运行目录，也未对其构建、测试或运行。

## 构建与定向验证

从准备输入的 `inputs/lab/tools/protocol_lab_ui/standalone` 独立配置，生成器为 Visual Studio 18 2026 x64，工具集 `v142,version=14.29.30133`。CMake 缓存核对 `PAE_LAB_ENABLE_YAML_ENTRY=ON`、`PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC`，`PAE_SDK_ROOT` 精确指向准备输入中的 `pae-sdk-static-release`，Qt 指向固定仓库 Qt 的准备副本。配置、构建日志：`configure-static-Release.log`、`build-static-Release.log`。

仅构建 Release Lab 与 `yaml_entry`、`yaml_ui_smoke`、`compile_queue`、`document_state`、`schema_dispatch` 五项测试目标；`ctest --test-dir out/build/experience-lab-adeae30-20260927/standalone-static-Release -C Release -R '^pae\.tools\.protocol_lab_ui\.(yaml_entry|yaml_ui_smoke|compile_queue|document_state|schema_dispatch)$' --output-on-failure` 为 **5/5 PASS**，见 `ctest-static-Release.log`。构建日志中这些测试编译均显示 `/UNDEBUG` 覆盖 `/DNDEBUG`，Release 断言有效。

在部署目录以 `--ui-smoke` 传入其 `configs/synthetic_ascii_literal_only.pae.json` 和同名 `.pae.yaml`，程序设置 `Qt::WA_DontShowOnScreen`，日志返回 `UI_SMOKE_PASS detail=2 document(s)`、退出码 0；未启动可见窗口。见 `ui-smoke-static-Release.log`。

## 可复制运行目录与来源

供后续归集的**纯运行目录**为 `out/build/experience-lab-adeae30-20260927/standalone-static-Release/deploy/Release/`，共 19 个文件：一个 `pae_protocol_lab_ui.exe`、`Qt5Core.dll`/`Qt5Gui.dll`/`Qt5Widgets.dll`、`platforms/qwindows.dll`、`configs/` 下 13 JSON + 1 YAML。运行目录无 `pae.dll`、Debug 输入、测试 exe、PDB、日志、库或构建缓存；它是候选，不是已归集或发布的最终体验包。

- 准备输入的 Release SDK 与原 `static-yaml-Release-install` **42/42 文件 SHA-256 一致**；准备副本的 `SHA256SUMS.txt` 中 **41/41 条通过**。CMake 由该 Release SDK 的 `PAE::pae`/`PAE::yaml_frontend` 导入目标链接；Lab 源码来自固定 clone 的独立白名单输入，输入内没有 `src/` 产品实现目录。
- 部署 EXE 与本轮 Release 构建 EXE SHA-256 一致：`1B2CF91E60AEA2D3B3B1935396867AB847A87D8D0F87AC9515D904B8D8600425`。四个 Qt 运行文件从固定 clone `third_party/qt/bin/release` 到准备副本、再到部署目录的 SHA-256 均一致。14 份配置与准备白名单逐文件 SHA-256 一致。部署目录逐文件审计零异常。
- `dumpbin /dependents` 对部署 EXE 只列 Qt 3 DLL 与 Windows/MSVC 运行库，不列 `pae.dll`；见 `dependents-static-Release.log`。这证明本 EXE 的直接静态 SDK 边界，未宣称对所有系统运行环境作全量验证。

## 未覆盖与停点

本片未重打 SDK 五包，未运行 Debug、Shared、JSON-only、全仓测试、可见 UI/人工体验、真实协议或设备长时运行，也未验证最终总包/ZIP 在新位置的解包消费。MSVC 系统运行库仍依赖目标 Windows 环境；其分发闭包由后续总包归集复核。没有 Stage、Commit、Push、发布、旧部署替换或删除。当前状态为**已完成派发范围，待总控复核**；此后停止写入。
