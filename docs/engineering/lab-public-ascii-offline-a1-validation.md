# Lab ASCII 0.10 非 Qt public-only A1 验证记录

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

总控状态更新（2026-09-18）：A1 含预算一致性返修已限定收口，总控核对源码及 budget-fix 证据，未重复构建测试。已纳入用户授权的 SDK/A1/A2 本地提交检查点范围，未授权 Push/正式发布。下方“待总控复核/未 Commit”为执行任务交付时点。

日期：2026-09-18。状态：**已完成派发范围，待总控复核**。本记录只覆盖完整记录 ASCII 0.10 的非 Qt adapter、自有 DTO 和 Windows x64 定向验证；不是 UI、显式 Host、0.11 stream、正式 SDK 发布或历史 Qt 访问违例修复结论。未 Stage、Commit、Push、删除或发布。

### 2026-09-18 总控限定预算一致性返修

总控复核发现：描述预检只按 Pipeline 中实际 Decode/Encode 可用 Message 数计费，但初版复制时两个方向集合都按 `message_count` 预留，单向/混合 Pipeline 会产生未计入逻辑描述预算的无用 capacity。返修先增加与 `DescriptionAccountedBytes()` 无关的 Testing-only capacity 观察，以及 Decode-only、Encode-only、两个 Message 分属单向的混合 Pipeline 独立期望；修复前 Debug 运行在 Decode-only 的空 Encode 集合 `capacity==0` 断言失败，保留于：

- `out/stage4-ascii-public-a1/budget-fix-before-Debug-build.log`
- `out/stage4-ascii-public-a1/budget-fix-before-Debug-direct.log`

最小修正是在复制每个 Pipeline 前通过公开 `PipelineMessageExecution` 统计实际 Decode/Encode 数，`message_indices` 仍预留完整关联数，两个方向集合分别只预留实际计数。修正后单向预留为 `1/0` 或 `0/1`，混合两 Message 预留为 `1/1`，专项 D/R 直接运行均 `PUBLIC_ASCII_A1_TEST_PASS`、CTest 各 1/1；candidate1 static 包外 D/R 均 `PUBLIC_ASCII_A1_PACKAGE_CONSUMER_PASS`。**以下 `budget-fix-*` 日志是对应当前源码的最终证据；旧 `final-*` / `sdk-static-*` 日志只保留为返修前历史：**

- `out/stage4-ascii-public-a1/budget-fix-{Debug,Release}-{build,direct,ctest}.log`
- `out/stage4-ascii-public-a1/budget-fix-sdk-static-{Debug,Release}-{build,run}.log`

同步核对了本片其他显式 `reserve`：描述的 Pipeline、Message、字段、action segment、Pipeline 全关联集合均与相同公开 count 的预检一致；Decode/Encode 已发布字段集合均与预检使用的实际发布 field count 一致。EncodeValue 与 Codec 输出 buffer 是单次执行的瞬时 scratch，分别受 `max_fields/max_field_bytes` 与 `max_frame_bytes` 限制，不属于持久描述或发布结果的逻辑字节计费；本次没有借机扩建资源预算体系。检索位置保存在 `out/stage4-ascii-public-a1/budget-fix-reserve-audit.log`。

## 1. 基线、范围与实现

- 基线：`main@481d51ae2d62e2e28754fd84e5df4bb64309a1e2`，开始及结束时暂存区为空。既有 `pae-execution-delivery-organization-plan.md`、`post-dec040-roadmap.md`、`examples/public_api_sdk_consumer/main.cpp` 和 `pae-public-ascii-sdk-validation.md` 修改全部保留，不属于本片。
- 新目标由默认 OFF 的 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1` 控制，并要求 `PAE_BUILD_PUBLIC_API_STAGE1=ON`。它只包含 `public_ascii_offline_adapter.*`，链接 `PAE::pae` 和标准库；没有引用 Qt、旧 ASCII adapter 或 private Compiler/Plan/Core/Host 头。
- `Adapter::AdoptCompiled` 移入一次公开编译所得 `CompiledProtocol`，同一 owner 同时支撑公开只读描述和一个 `CompleteRecordCodec`。Adapter 不接收 JSON，不重新编译；单次 `Decode` / `Encode` 各只调用一次公开 Codec。
- 自有描述复制协议、Pipeline/Message/Field 作者信息、Pipeline 允许 Message 及 Decode/Encode 可用集合、ASCII RX/TX 有序片段、字段 byte bounds 与动作引用。控制字节列表保持 `nullopt`，表示公开描述未知，不解释为空白名单。
- 成功 RX 在借用 view 有效期内，以整数地址和长度做有溢出保护的本帧区间核验，然后原子复制 frame、字段 bytes、Message/Field 身份和实际 `ByteRange`；零长字段保留真实 offset。失败只保留本次 Codec 状态、可选 matched Message、失败字段和标记后的诊断输入，不发布有效 record/字段。
- 成功 TX 用同一 Message 的公开 TX segments、本次原始字段输入和一次 Codec 成功输出逐段比较并推进游标；不搜索内容、不 RX Decode TX、不使用上次输入。Codec 已 OK 后若投影、预算或复制失败，结果保留 Codec OK 事实但本地状态为物化/分配失败，且不发布 frame/字段。
- 描述、结果、实例和 replacement 使用饱和加法/乘法进行逻辑预算门禁；描述及结果在分配/发布前完成所需字节预检。Codec 自身分配由公开创建接口负责，创建失败或随后实例 admission 失败均不发布 Adapter。测试钩子只在 `PAE_BUILD_TESTING` 下编译，用于确定性覆盖 Codec OK 后本地物化与 `bad_alloc` 原子失败，不进入 Testing-off 或包外 consumer。

最初先接入测试/CMake 而没有实现源文件，Configure 按预期失败并明确报告缺少 `public_ascii_offline_adapter.cpp`，证据为 `out/stage4-ascii-public-a1/initial-configure-missing-implementation.log`；随后才新增实现。这里的初始失败只证明新专项确实依赖本片实现，不是功能缺陷复现或 TDD 覆盖率证明。

## 2. 源码专项 Debug / Release

工具链为 Visual Studio 18 2026、x64、MSVC v142 `14.29.30133`。独立构建目录：

`<REPO_ROOT>\out\build\windows-msvc-stage4-ascii-public-a1`

配置使用：

```powershell
cmake -S . -B out/build/windows-msvc-stage4-ascii-public-a1 -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A1=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_JSON_PARSER_SPIKE=OFF
cmake --build out/build/windows-msvc-stage4-ascii-public-a1 --config Debug --target pae_protocol_lab_ascii_public_a1_tests --parallel 4
cmake --build out/build/windows-msvc-stage4-ascii-public-a1 --config Release --target pae_protocol_lab_ascii_public_a1_tests --parallel 4
ctest --test-dir out/build/windows-msvc-stage4-ascii-public-a1 -C Debug -R '^pae\.tools\.protocol_lab_ascii\.public_a1$' --output-on-failure
ctest --test-dir out/build/windows-msvc-stage4-ascii-public-a1 -C Release -R '^pae\.tools\.protocol_lab_ascii\.public_a1$' --output-on-failure
```

Configure、D/R 定向构建、直接运行和 CTest 均退出 0；直接运行均输出 `PUBLIC_ASCII_A1_TEST_PASS`，CTest 各 **1/1**。日志为：

- `out/stage4-ascii-public-a1/final-configure.log`
- `out/stage4-ascii-public-a1/final-{Debug,Release}-build.log`
- `out/stage4-ascii-public-a1/final-{Debug,Release}-direct.log`
- `out/stage4-ascii-public-a1/final-{Debug,Release}-ctest.log`

专项断言覆盖：双向及 Decode-only/Encode-only/literal-only；Pipeline 的 Decode/Encode 可用 Message 集合；两个 Message 的调用期 Encode selector；not referenced 输入拒绝；成功 RX 变长范围和输入修改后的结果所有权；嵌入 NUL literal 与零长 RX/TX 范围；TX 字段内容与前缀重复时仍按片段得到 offset 3；非法 Pipeline/Message selector；未知、唯一匹配后字符失败及成功恢复；失败不泄漏旧 frame/字段；描述、结果、实例和 replacement exact / minus-one 预算；Codec OK 后 Decode/Encode 本地物化失败；Codec OK 后确定性分配失败；Adapter 销毁后已发布结果仍自有。

真实公共 Codec 不会正常生成“Encode OK 但输出与冻结 TX segment 不一致”的状态，不能在不破坏 PAE 契约的前提下构造该输入。因此专项以 Testing-only 物化失败钩子验证 Codec OK 与本地失败分层及零发布，以正常重复内容用例验证实际逐段算法；它不是一次真实 PAE 契约违例复现。确定性 `bad_alloc` 同样是测试钩子证据，不是操作系统内存耗尽压力试验。

## 3. candidate1 static 包外消费

外部消费根为：

`<LOCAL_WORK_ROOT>\pae-lab-ascii-a1-static-consumer-20260918\`

两个全新构建分别只设置对应安装包 `CMAKE_PREFIX_PATH`，通过 `find_package(PAE CONFIG REQUIRED)` / `PAE::pae` 编译同一 adapter 源码和最小 consumer：

- Debug：`out/sdk-stage4-ascii/candidate1-20260918/pae-sdk-static-debug`
- Release：`out/sdk-stage4-ascii/candidate1-20260918/pae-sdk-static-release`

两组 Configure、Build、Run 均退出 0，运行均输出 `PUBLIC_ASCII_A1_PACKAGE_CONSUMER_PASS`。日志：

- `out/stage4-ascii-public-a1/sdk-static-{Debug,Release}-configure.log`
- `out/stage4-ascii-public-a1/sdk-static-{Debug,Release}-build.log`
- `out/stage4-ascii-public-a1/sdk-static-{Debug,Release}-run.log`
- `out/stage4-ascii-public-a1/sdk-static-input-paths.log`

路径证据显示公开 include 与 `pae.lib` / internal static transitive libraries 全部来自所选 candidate1 包；仓库路径只提供本片 adapter 自身的 `.h/.cpp`，没有仓库 `PAE::pae`、private 头或源码 target fallback。consumer 实际完成一次公开编译、Adopt、RX Decode 切片复制和 TX Encode 投影。

## 4. 门禁、文件与边界

默认 OFF、`BUILD_TESTING=OFF` / `PAE_BUILD_TESTING=OFF` 的隔离 Configure 退出 0，目标帮助中 A1 命中数为 0，见 `isolation-configure.log` 和 `isolation-targets.log`。A1 显式 ON、Testing-off 的独立 Release adapter 构建退出 0，`ctest -N` 为 `Total Tests: 0`，见 `notesting-configure.log`、`notesting-Release-build.log`、`notesting-ctest-list.log`。`source-boundary.log` 记录 adapter 仅含标准库、`pae/codec.h`、`pae/compiler.h`，private/Qt/旧 adapter 禁止模式命中为 0。
新 C++ 文件经仓库 `.clang-format` 机械格式化后，以 VS bundled `clang-format --dry-run --Werror` 复核退出 0，见 `clang-format-check.log`；上述最终 D/R 与包外消费均在格式化后的源码上重新执行。

本片实际源码/测试/CMake 增量：

- `CMakeLists.txt`
- `tools/protocol_lab_ascii/CMakeLists.txt`
- `tools/protocol_lab_ascii/public_ascii_offline_adapter.h`
- `tools/protocol_lab_ascii/public_ascii_offline_adapter.cpp`
- `tests/protocol_lab_ascii/CMakeLists.txt`
- `tests/protocol_lab_ascii/public_ascii_offline_adapter_tests.cpp`
- `tests/protocol_lab_ascii/public_ascii_a1_consumer/CMakeLists.txt`
- `tests/protocol_lab_ascii/public_ascii_a1_consumer/main.cpp`
- 本验证记录

没有修改旧 adapter、UI、公共 API、Core、Plan、Schema、SDK 安装白名单、CLI/Evidence 拒绝门、现有 SDK/H2 部署或本机 Qt；没有运行 Qt/UI、Host/Flow、stream、完整 PAE/全仓矩阵、动态/源码包消费、Linux、真实协议 Golden、硬件或现场验证。candidate1 static 验证不升级为 SDK 正式发布或稳定 ABI 结论。历史 Qt5Core 访问违例仍独立未解决。

本片达到的停点是：A1 非 Qt public-only 完整记录 adapter、源码专项 D/R、candidate1 static 包外 D/R 及接线隔离完成；**已完成派发范围，待总控复核**，现停止写入。
