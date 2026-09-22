# Stage 4 H2：Binary 0.9 UI 切换验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

最新人工反馈（2026-09-15）：用户明确确认“Flow 复验通过，Lab 已关闭”。本次确认覆盖返修版 Flow0=count1、Flow1=count2 的输入草稿/结果往返隔离，关闭该人工失败项；不扩展为完整 UI、ASCII、Golden 或包外 Qt 消费验收。结合总控限定代码/自动证据复核，H2 Binary 0.9 complete Decode 切换本片限定收口。ASCII 自动烟测崩溃根因仍未确认、独立保留；无 Stage/Commit/Push 或发布。本段覆盖下文历史待验状态。

日期：2026-09-15。状态：已完成派发范围，待总控复核。本记录只覆盖 Windows x64、随仓 Qt 的本地构建、自动测试与静态 SDK 消费侧；不等于人工 UI、Golden、硬件、Linux 或正式发布验收。契约见 `lab-public-api-stage4-h2-contract.md`。

## 1. 本次实现与边界

- `windows-msvc-stage4-h2` 在原 Lab UI 上开启 public API Stage 1、H1 与 H2，关闭旧 Binary materializer。Schema 0.9 complete-record Binary 初次打开与显式 Apply 由 worker 的 dispatch-only 根版本分类进入一次 `CompileProtocolJson`，同一 move-only compiled owner 进入公开描述映射、H1/public Host Decode；无 private Binary fallback。0.5–0.8 与 ASCII 仍在旧 private 编译/执行支路。
- Binary 专属 H2 adapter/description 源和头独立，链接 H1/PAE；共享 Qt DTO 仍需为旧分支保留内部展示 enum，但 Binary 执行不借用 private Plan/Core/Host 或旧 materializer。公开 logical Decimal64/本次 raw 与实际 byte range 承接可见 TYPE、通用转换提示、Raw/Logical、高亮；不凭旧内部 DTO 猜造 wire/order/scale/bias。
- 路由分类失败、目标编译失败分开报告；失败清旧成功字段。测试覆盖 duplicate/escaped/nested schema key、非法/缺失/超限、一次目标编译、无 fallback、未绑定/Apply、两 binding/两 Flow、typed/raw、物理位置、预算/替换/close/双 Tab；旧 session/ASCII/UI 回归保持。

## 2. 自动验证结果

| 层级 | 命令或构建 | 结果与证据 |
| --- | --- | --- |
| H2 Debug/Release | `cmake --build out/build/windows-msvc-stage4-h2 --config <Debug\|Release> --target pae_protocol_lab_ui pae_protocol_lab_ui_binary_public_h2_tests pae_protocol_lab_ui_binary_public_header_tests pae_binary_public_h1_tests`；`ctest --test-dir out/build/windows-msvc-stage4-h2 -C <Debug\|Release> -R 'pae\.protocol_lab_binary\.public_h1\|pae\.tools\.protocol_lab_ui\.' --output-on-failure` | 两配置分别 23/23；`out/stage4-h2-validation/focused-debug-ctest.log`、`focused-release-ctest.log`。一次 Debug ASCII 重开编辑器窗口烟测失败，单独复跑通过，随后全套复跑 23/23；暂记时序波动，未宣称修复。 |
| 旧 Binary backend | H2 OFF/materializer ON 的独立 `out/build/windows-msvc-stage4-h2-legacy`，Debug/Release 旧 Binary headless 与 Qt smoke | 两配置各 2/2；`out/stage4-h2-validation/legacy-debug-ctest.log`、`legacy-release-ctest.log`。H2 末尾的动态高亮补丁在宏分支内，旧支路未重新构建。 |
| H1 静态 SDK 包外消费 | 临时源码 `<USER_PROFILE>\AppData\Local\Temp\pae-stage4-h2-h1-consumer-20260915` 含与当前 H1 `.cpp` 同 SHA256 的副本；对候选 2 静态 SDK 分别构建 `out/build/windows-msvc-stage4-h2-static-debug/-release`，运行 `h1_consumer.exe tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json` | Debug/Release 均 `PUBLIC_BINARY_H1_STATIC_CONSUMER_PASS`；`out/stage4-h2-validation/h1-static-debug-run.log`、`h1-static-release-run.log`。只证明静态包消费者，不证明 H2 Qt UI 作为包外消费。 |
| 开关/缺依赖 | H2 推荐缓存：H2 ON/materializer OFF；独立旧缓存：H2 OFF/materializer ON；Testing-off 缓存 `BUILD_TESTING=OFF`、`PAE_BUILD_TESTING=OFF`，两配置 UI 构建；缺 H1 配置尝试 | Testing-off `ctest -N` 为 0 test，见 `out/stage4-h2-validation/testing-off-ctest-n.log`；H2 ON/H1 OFF 配置拒绝，报 `Binary public H2 requires UI, Binary UI, public H1 and public API Stage 1.`；默认 H2 OFF。无外部发布动作。 |

## 3. Release 本地部署与待用户人工烟测

初始 H2 交付时的部署目录：`out/build/windows-msvc-stage4-h2/out/protocol_lab_ui/Release`，包含 15 个白名单文件（EXE、随仓 Qt DLL/plugin、synthetic configs）。当时 `pae_protocol_lab_ui.exe` 与 `bin/Release` 的 SHA256 同为 `5E26ABF5AAD100AAB00A4133E548E080312D8008627ABB5A5C6CE29ECE8F719D`。当时最终目录双文档 `--ui-smoke` 输出两次 `fields=9` 与 `UI_SMOKE_PASS`，证据 `out/stage4-h2-validation/deployment-release-smoke.log`。返修后当前 EXE/证据见第 5 节；此初始自动结果没有通过后续人工 Flow 隔离项。

建议用户在此目录进行最多三个动作组的人工烟测（本任务未代替用户执行）：

1. 打开目录内 synthetic Binary 配置，确认初次打开未绑定、选择 `rx/ui_pipeline` 后显式 Apply；输入合法整帧 `80 0D 03 00 01 00 CA FE 05 5A`，核对 9 字段、BOOL bit 高亮及 temperature 的 Decimal64、Raw `5`、Logical `5@0`，切换 Flow 0/1 看草稿和结果隔离。
2. 在成功结果后输入非法十六进制 `AA:`，确认旧字段、Raw/Logical 和高亮清除；再输入合法帧恢复。修改配置显式 Apply，检查失败时旧已发布 Session 保持、成功后才替换。
3. 打开第二个 Tab 重复解码，切换/关闭 Tab，并在重载/取消附近操作，确认两 Tab 不串结果、关闭后无迟到结果回填。

## 4. 未验证与 Git 状态

初始自动交付时未进行人工 UI 烟测、完整 PAE 全量测试、真实协议 Golden、Linux、硬件或现场验收；也未进行动态 SDK 的包外 H2 消费。后续人工 Flow 反馈及返修见第 5 节。共享工作树中的其他阶段文档、SDK 和公共 API 改动属于并行任务，本片不为其宣称验证。本任务未 Stage、Commit、Push，未删除旧桥或修改本机 Qt/全局环境；达到停点后停止写入，等待总控核对差异与证据。

## 5. Flow 草稿隔离定向返修（2026-09-15）

用户人工烟测发现 Flow0 使用 count1 帧、Flow1 使用 count2 帧反复切换时，Flow1 显示结果 count2/帧02，而输入框却显示 count1/帧01；用户已关闭 Lab，授权本项定向返修，人工隔离项此前未通过。源码根因：`DocumentSession::PrepareBinaryHostFlow` 正确读取目标草稿，但 `PublishBinaryHostFlow` 传当前草稿与目标编号，H2 adapter 把当前草稿写进目标槽；旧 backend 同名调用保存的是当前选中槽。修复仅在 H2 adapter 增加当前 binding/Flow 选择跟踪，完成草稿复制/预算确认后保存源槽再切换目标；Session 事务、旧 backend、H1/PAE 均未改。

- 修复前增加新 Flow 空草稿断言，Debug 构建后 `ctest --test-dir out/build/windows-msvc-stage4-h2 -C Debug -R '^pae\.tools\.protocol_lab_ui\.binary_public_h2$' --timeout 10 --output-on-failure` 按预期 0/1，输出 `H2_FLOW_DRAFT_ISOLATION_FAIL first empty Flow inherited source draft`；证据 `out/stage4-h2-validation/flow-fix-before-debug-ctest.log`。早期测试复用了先前已编辑的 adapter，造成一次前置断言弹窗；换成全新 Session adapter 后取得上述有效失败证据，未据弹窗宣称复现。
- 修复后 H2 `cmake --build out/build/windows-msvc-stage4-h2 --config <Debug|Release> --target pae_protocol_lab_ui pae_protocol_lab_ui_binary_public_h2_tests` 均成功；沿用本记录第 2 节定向 CTest 表达式，两配置最终各 23/23，见 `flow-fix-debug-ctest.log`、`flow-fix-release-rerun-ctest.log`。新增断言检查首次空 Flow、count1/count2 的不同帧三轮 Qt/Session/结果往返、未 Inspect 草稿、跨 binding、超限保存失败、复制 hook 失败及视图预算回滚。Flow0 未 Inspect 草稿切回可见同槽上次结果，测试明确按“上次结果”观察，不把它当作新 Decode。旧 Binary 独立 H2 OFF Debug/Release 重建及 headless/Qt smoke 各 2/2，见 `flow-fix-legacy-debug-ctest.log`、`flow-fix-legacy-release-ctest.log`。
- Release 首轮 H2 定向总套 22/23，旧 ASCII `qt_smoke_ascii` 一次 SEGFAULT；原始 `flow-fix-release-ctest.log` 保留。该项单独复跑 1/1（`flow-fix-release-ascii-rerun.log`），随后总套 23/23。根因未确认，未归因于 Flow，也未顺手修改 ASCII。
- 最新 Release 最终目录仍为 `out/build/windows-msvc-stage4-h2/out/protocol_lab_ui/Release`，15 文件；部署与 `bin/Release` EXE 的 SHA256 同为 `3D3F13C6A16A63E70B28463DEED395A7A7834B8D642427A3F85A91F888D0A240`。从最终目录 EXE 与目录内 synthetic Binary 配置双文档执行 `--ui-smoke`，输出两次 `fields=9`、`UI_SMOKE_PASS`，见 `out/stage4-h2-validation/flow-fix-deployment-release-smoke.log`。原 `deployment-release-smoke.log` 与初始哈希保留为返修前历史证据。

返修未启动用户交互窗口；用户只需在该目录按第 3 节动作组复验 Flow0/Flow1 使用不同 count 帧往返（count1 输入 `80 0D 03 00 01 00 CA FE 05 5A`，count2 输入 `80 0D 03 00 02 00 CA FE 05 5A`），核对输入、选中 Flow、结果与帧分别稳定。人工复验仍未完成；不宣称 Golden、Linux、硬件、现场或发布通过。未 Stage、Commit、Push，未删除目录或复制系统 DLL；本项已到总控复核停点。
