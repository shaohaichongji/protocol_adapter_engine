# Lab 0.11 Qt 流式公开接线验证

## 1. 结论与边界

- 基线：`main@dbf4798f96a45b8d36a4754ad5a01e274680337e`；实施前工作树已存在总控、PAE public stream、SDK 消费验证及非 Qt Lab stream 的未提交变更，本批保留且未回退、未覆盖。
- 新增默认 `OFF` 的 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI`。开启后，Schema 0.11 首次加载、直接 stream 和显式 Host Apply 均走一次 public compiler，不再路由到 private ASCII compiler。
- 直接 stream 由一个 `AsciiHostAdapter` 持有同源 public `CompiledProtocol`/Host 状态；Decode/Encode binding 只根据 public Pipeline/Message execution 事实生成，没有 private Plan 解读、private fallback 或第二份冻结执行状态。
- `Submit/Continue/Reset`、Flow/Tab 草稿隔离、失败清理旧结果、zero-field success、Apply 替换和 close/cancel 既有语义均由公开 Host 路径通过现有 UI DTO 继续呈现。public `StreamDiagnostic` 到文本的转换只在 UI facade 中完成，非 Qt `noexcept` 层未增加字符串分配。
- 本批没有修改 `tools/protocol_lab_ascii/public_ascii_host_adapter.*`，因此未重跑非 Qt/static 包外消费；未重打 SDK 包，未改 PAE 公共接口、Schema/Core/Plan/Host 契约。

## 2. 测试先行证据

- 修复前独立期望：`schema_dispatch_tests` 在 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI` 下期望 0.11 为 `ASCII_PUBLIC`。
- 修复前红灯：实际为 `PRIVATE_ASCII`，`actual=3 expected=1`。日志：`out/lab-public-ascii-stream-ui/before-fix-r2-test.log`。
- 首次隔离配置缺少项目要求的 v142 参数，在 CMake 阶段被门禁拒绝；保留于 `before-fix-configure.log`，未当作上述红灯。正式构建目录为 `out/build/windows-msvc-lab-public-ascii-stream-ui-r1`。

## 3. 自动验证

### 3.1 Debug

- 7 项 headless 串行通过：`compile_queue`、`schema_dispatch`、`ascii_stream_session`、`host_session`、`ascii_public_a2`、`binary_public_h2`、`binary_public_header`。
- 5 项 Qt smoke 串行通过：0.11 stream、0.11 close/cancel、0.10 ASCII、0.10 one-way、H2 Binary。
- 日志：`debug-targeted-build.log`、`debug-targeted-tests.log`、`debug-qt-smoke.log`。

### 3.2 Release

- 最终 12/12 串行通过：上述 7 项 headless + 5 项 Qt smoke。
- Release headless target 保留 `/UNDEBUG`，构建日志显示 `/DNDEBUG` 被 `/UNDEBUG` 覆盖，测试中的 `assert` 实际执行。
- 日志：`release-targeted-build.log`、`final-release-tests.log`。

### 3.3 构建门禁

- 默认配置：`PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI:BOOL=OFF`，见 `default-off-cache.log`。
- Testing-off：`BUILD_TESTING=OFF` 且 `PAE_BUILD_TESTING=OFF` 时，Release `pae_protocol_lab_ui` 构建和部署通过，见 `testing-off-final-build.log`。
- 本批源文件 SHA-256 清单：`out/lab-public-ascii-stream-ui/source-sha256.txt`。

## 4. Release 部署候选

- 可运行目录：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-public-ascii-stream-ui-r1\out\protocol_lab_ui\Release`
- EXE：`pae_protocol_lab_ui.exe`
- SHA-256：`F2A39850FD947C29E8ADDBA4265A7750D9DCC3AEA6888EE40C9E8B972D97CF5E`
- 目录已包含仓库 Qt DLL/platform plugin，以及 `configs/synthetic_ascii_stream_slice.pae.json`。验证见 `deployment-stream-config-check-2.log` 和 `mixed-pipeline-fix-deployment-sha256.txt`。

## 5. 人工烟测与未验证边界

2026-09-19 用户确认“三组烟测通过，Lab 已关闭”。实际执行的是总控复核后给出的以下简化流程（替代初始建议，不将未执行的额外场景记为人工通过）：

1. 加载第 4 节部署目录的 `configs/synthetic_ascii_stream_slice.pae.json`，ASCII escaped 下先提交 `ON` 再 `LY\r\n`，以及 `ONLY\r\nONLY\r\n` 后 Continue，验证分片、后缀续传和零字段成功。
2. Apply binding table 后，Decode Flow 0 提交 `ON`，Flow 1 提交 `RX A!`，往返确认草稿；分别续交 `LY\r\n`、`OK\r\n`，得到 decode_only 与 greeting（A、OK）。
3. Flow 0 保留 `ON`，点击程序窗口关闭按钮并选 No，续交 `LY\r\n` 成功；随后关闭整个 Lab。

结合此前限定源码/自动证据复核，本片 0.11 Qt 公开接线限定收口。人工证据为用户反馈，未由总控再次运行。未重打/正式发布 SDK，未验证 Linux、硬件或现场；历史 Qt 访问违例及整个 Lab 包外消费不因此关闭。本轮未 Stage、未 Commit、未 Push、未正式发布、未删除文件。

## 6. mixed Pipeline 限定返修（2026-09-19）

### 6.1 问题与红灯

- 原 direct public 分支只检查 `FindBinding(DECODE).has_value()`，因此把存在 Decode binding 的 complete-record `decode_only_pipeline` 误报为 stream 可用；显式 Host 分支已使用 public `Observe`，不存在该误判。
- 新增 mixed Pipeline 独立断言：`ascii_pipeline` 必须为 stream；`decode_only_pipeline` 必须仅允许 complete-record Inspect，并能把 `ONLY\r\n` 解析为 zero-field success；`encode_only_pipeline` 必须仅保留 Encode。direct 与显式 Host 均覆盖。
- 对两个 complete-record Pipeline 同时断言 `StreamObservation` 为空，`Submit/Continue/Reset` 均不进入 stream 执行，不解引空 optional。
- 修复前 Release 中 `ascii_stream_session` 和 `host_session` 均按预期失败，见 `mixed-pipeline-fix-before-tests.log`。

### 6.2 最小修正

- 仅修改 `DocumentSession::StreamInspectAvailable()` 的 direct public 分支：先定位 Decode binding，再以该 binding 的 public `Observe(...).has_value()` 判定 stream 可用性。
- 没有修改 PAE、非 Qt Host adapter 或执行语义；显式 Host 分支未改。

### 6.3 返修验证

- Debug：6/6 PASS，包含 `ascii_stream_session`、`host_session`、`document_state`、`inspect_state` 及两项 0.11 Qt smoke；见 `mixed-pipeline-fix-debug-tests.log`。
- Release：同一组 6/6 PASS；见 `mixed-pipeline-fix-release-tests.log`。Release 构建继续以 `/UNDEBUG` 执行断言。
- 返修源码/测试 SHA-256：`mixed-pipeline-fix-source-sha256.txt`。
- 未重跑 SDK、非 Qt/static 包外消费或全仓测试；未增加人工步骤。人工验收继续暂停，等待总控复核。
