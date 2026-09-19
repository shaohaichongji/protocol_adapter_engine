# Lab ASCII 私有兼容闭包隔离验证

## 1. 结论

本片已在 `main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31` 的既有未提交工作树上完成限定实现与验证：

- Qt Lab ASCII façade 的公开头、公开实现和独立 CMake target 不再包含或链接旧
  `host_observer_adapter`、`protocol_lab::ascii::*` DTO、`host_endpoint::Action` 或
  `pae_protocol_lab_ascii_adapter_internal`。
- binding、identity、operation、execution result、stream observation/step 已由
  `ascii_host_types.h` 提供 Lab 自有类型；公开 0.10/0.11 路径仅消费 public ASCII adapter
  与 `PAE::pae`。
- 旧私有 complete-record/Host/stream 兼容路径收敛到独立的 types conversion 与 backend
  compatibility source；未新增第二套 stream 状态，Session 仍持有同一套 Lab 自有展示状态。
- 完整公开组合、A2 关闭旧兼容组合、Host observer 关闭公开组合均完成针对性编译验证；
  Debug/Release 定向测试通过。

本片不表示整个 Qt Lab 包外构建完成，也不新增人工 UI、SDK、Linux、硬件或现场证据。

## 2. 实际变更边界

### 2.1 Lab 自有类型与公开 façade

- `tools/protocol_lab_ui/ascii_host_types.h`
- `tools/protocol_lab_ui/ascii_host_types.cpp`
- `tools/protocol_lab_ui/ascii_host_adapter.h`
- `tools/protocol_lab_ui/ascii_host_adapter.cpp`

`AsciiHostAdapter` 通过 Lab 自有 `AsciiHostBackend` 接口持有 backend；公开实现只做 public
ASCII description/result/stream observation 到 Lab 自有类型的映射。公开 target
`pae_protocol_lab_ui_ascii_public_facade` 只链接：

- `pae_protocol_lab_ui_owned_presentation`
- `pae_protocol_lab_ascii_public_a1`
- `PAE::pae`

### 2.2 私有兼容闭包

- `tools/protocol_lab_ui/ascii_host_types_compat.h/.cpp`：旧私有 DTO 与 Lab 自有类型之间的
  单向边界转换；A2 关闭时仍可供既有 Session 路径复用。
- `tools/protocol_lab_ui/ascii_host_adapter_compat.h/.cpp`：旧 `HostObserverAdapter` 的 backend
  包装和私有 description 映射，仅由 private compatibility target 消费。
- `document_session.*`、`document_tab.*`：公开路径只传递 Lab 自有类型；A2 关闭分支在调用旧
  backend 前后显式转换。既有 flow、失败清空、selection/identity 与 stream ownership 状态未拆分。

### 2.3 构建与测试

- `tools/protocol_lab_ui/CMakeLists.txt` 将 public façade、private types compatibility、private
  backend compatibility 分为独立 target；headless 对私有兼容 target 仅为 `PRIVATE` 依赖。
- 新增 `ascii_public_facade_isolation_tests.cpp`，测试目标只链接 public façade，不链接
  headless 或 private compatibility。总控首轮复核指出原版测试只消费 DTO 与
  `AsciiCodecStatusName`，静态 archive 可能未拉入 `ascii_host_adapter.cpp`，因此不能作为真实
  façade 行为/链接证据；限定返修后该测试会读取最小公开 ASCII 配置，依次执行
  `CompileProtocolJson`、`CreatePublicDirect` 和真实 `Inspect`。
- `ascii_public_a2` Host 集成测试只在 A2 与 Host observer 同时开启时生成；A2 开启但 Host
  observer 关闭的合法组合不再错误生成依赖 Host Session API 的测试目标。

## 3. 验证结果

### 3.1 完整公开组合（A2、0.11 public stream UI、Host observer 均开启）

构建目录：`out/build/windows-msvc-lab-ascii-compat-isolation`

Debug 与 Release 均构建以下目标成功：

- `pae_protocol_lab_ui_ascii_public_facade_isolation_tests`
- `pae_protocol_lab_ui_ascii_public_a2_tests`
- `pae_protocol_lab_ui_ascii_stream_session_tests`
- `pae_protocol_lab_ui_host_session_tests`
- `pae_protocol_lab_ui`

Debug、Release 各 4/4 PASS：

- `pae.tools.protocol_lab_ui.ascii_public_facade_isolation`
- `pae.tools.protocol_lab_ui.ascii_public_a2`
- `pae.tools.protocol_lab_ui.ascii_stream_session`
- `pae.tools.protocol_lab_ui.host_session`

这组回归覆盖 0.10 A2、0.11 stream、mixed Pipeline、Host Flow
隔离、失败清空及既有 ownership/budget 断言。Release 工程明确包含 `/UNDEBUG`（生成工程中为
`UndefinePreprocessorDefinitions=NDEBUG`），断言未被 `NDEBUG` 关闭。

证据：

- `out/lab-ascii-compat-isolation/debug-build-final2.log`
- `out/lab-ascii-compat-isolation/debug-tests-final2.log`
- `out/lab-ascii-compat-isolation/release-build-final2.log`
- `out/lab-ascii-compat-isolation/release-tests-final2.log`
- `out/lab-ascii-compat-isolation/release-build-final.log`（独立 façade Release 首次实际编译，含 `/UNDEBUG`）

### 3.2 A2 关闭、旧私有兼容路径

构建目录：`out/build/windows-msvc-lab-ascii-compat-isolation-a2-off`

Debug 构建 UI、Host session、ASCII stream session 成功；2/2 PASS：

- `pae.tools.protocol_lab_ui.ascii_stream_session`
- `pae.tools.protocol_lab_ui.host_session`

证据：

- `out/lab-ascii-compat-isolation/a2-off-debug-build-final.log`
- `out/lab-ascii-compat-isolation/a2-off-debug-tests-final.log`

### 3.3 Host observer 关闭、公开 complete-record 路径

构建目录：`out/b/ascii-host-off`

Debug 构建 public façade 独立测试、ASCII stream session 与 UI 成功；2/2 PASS：

- `pae.tools.protocol_lab_ui.ascii_public_facade_isolation`
- `pae.tools.protocol_lab_ui.ascii_stream_session`

证据：

- `out/lab-ascii-compat-isolation/host-off-short-configure.log`
- `out/lab-ascii-compat-isolation/host-off-short-debug-build-final.log`
- `out/lab-ascii-compat-isolation/host-off-short-debug-tests.log`

### 3.4 静态边界与差异检查

- `git diff --check` 对本片源码/CMake/tests 范围通过。
- 生成的 `pae_protocol_lab_ui_ascii_public_facade.vcxproj`、公开 header、公开 source 中未发现
  `host_observer_adapter`、private compatibility target/source、
  `pae_protocol_lab_ascii_adapter_internal`、`protocol_lab::ascii::*` 或
  `host_endpoint::Action`。
- 初次 Host-off 验证使用过长构建目录时 linker 报 `LNK1104`，改用短目录后同一源码组合构建
  通过；原失败日志保留于 `host-off-debug-build.log` / `host-off-facade-retry.log`，未删除目录。

### 3.5 总控限定返修：真实 façade 行为与链接闭包

本轮未改执行代码，也未重复 4 项矩阵；只重建并运行独立 façade 测试。

增强后的测试执行：

1. 读取 `examples/config/synthetic_ascii_text_slice.pae.json`；
2. `CompileProtocolJson` 成功并将 `CompiledProtocol` 交给 `CreatePublicDirect`；
3. 从 façade 查找 Pipeline 0 的 Decode binding；
4. 通过 `AsciiHostAdapter::Inspect` 解码 `RX ALICE!OK\r\n`；
5. 断言 `CodecStatus::OK`、Message `greeting`、原始帧保持，以及 `name=ALICE`、
   `rx_code=OK` 两个字段结果。

Debug/Release 各 1/1 PASS。Release 实际编译命令同时出现 `/DNDEBUG` 与 `/UNDEBUG`，因此上述
断言确实执行。两种配置的实际 linker 输入均包含：

- `pae_protocol_lab_ui_ascii_public_facade.lib`
- `pae_protocol_lab_ui_owned_presentation.lib`
- `pae_protocol_lab_ascii_public_a1.lib`
- `pae.lib` 及其 public implementation 依赖

linker 输入不包含 `pae_protocol_lab_ui_headless_internal`、
`pae_protocol_lab_ui_ascii_private_compat`、`pae_protocol_lab_ui_ascii_types_compat` 或
`pae_protocol_lab_ascii_adapter_internal`。由于测试真实调用 `CreatePublicDirect`、
`FindBinding` 和 `Inspect` 等定义于 `ascii_host_adapter.cpp` 的非内联符号，当前证据可证明
façade archive 对象已被拉入并完成行为链接；原版测试只能证明 header/DTO/status-name 可消费，
不能证明这一点。

新增证据：

- `out/lab-ascii-compat-isolation/facade-behavior-debug-build.log`
- `out/lab-ascii-compat-isolation/facade-behavior-debug-test.log`
- `out/lab-ascii-compat-isolation/facade-behavior-release-build.log`
- `out/lab-ascii-compat-isolation/facade-behavior-release-test.log`
- `out/lab-ascii-compat-isolation/facade-behavior-link-audit.log`

## 4. 未验证与状态

- 未运行全仓测试、SDK 五包或包外 Qt consumer；未覆盖旧部署。
- 未进行人工 UI 烟测；本片不新增人工验收要求。
- 未验证 Linux、硬件或现场环境。
- 未执行 Stage、Commit、Push、发布或删除；工作树中的总控、SDK 与前序 Lab 变更均保留。
- 当前状态：已完成派发范围，待总控复核；已停止写入。
