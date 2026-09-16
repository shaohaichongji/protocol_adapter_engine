# Stage 4 H1：Lab public-only Binary complete Decode 验证

日期：2026-09-15。状态：已完成派发范围，待总控复核；已停止写入。此处只记录 H1 新 headless target，不宣称 Binary UI 已切换。

## 现场与变更

仓库 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`，接管时 `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`，暂存区为空，既有目录迁移/UI/PAE/SDK 变更均保留。H1 仅增加 `public_binary_decode.h/.cpp`、H1 测试和独立 static consumer、契约/验证文档；最小改 `tools/protocol_lab_binary/CMakeLists.txt`、`tests/protocol_lab_binary/CMakeLists.txt` 与根 CMake 默认 OFF 门禁。没有改 PAE 公开接口/Core/Plan/Schema、旧 Binary backend、UI/v06/ASCII、Qt、SDK 包/脚本或索引。

生产 H1 target `pae_protocol_lab_binary_public_h1` 只显式链接 `PAE::pae` 和通用编译选项，生产头仅标准库与 `pae/**`；没有私有 include、旧 materializer、test hook 或第二次 Decode。外部 consumer 的 VS `AdditionalIncludeDirectories` 仅有临时目录与 static SDK `include`；依赖库虽包含 static SDK Config 提供的内部 archive 传递闭包，consumer CMake 只声明 `PAE::pae`，未点名私有 target。

## Windows 自动验证

工具链：Visual Studio 18 2026 / MSBuild 18.10.1，MSVC 19.29.30159，v142 14.29.30133，Windows SDK 10.0.22621.0，x64，`PAE_MSVC_RUNTIME_LIBRARY` 默认 Debug `/MDd`、Release `/MD`。新构建目录均在 `out/build/windows-msvc-stage4-h1-*`，无共享构建目录并发。

| 检查 | 结果 | 证据 |
| --- | --- | --- |
| H1 定向 Debug/Release | 各 1/1，`PUBLIC_BINARY_H1_TEST_PASS` | `out/stage4-h1-validation/focused-debug.log`、`focused-release.log` |
| public Host / P physical query 定向回归 | D/R 各 2/2 | `public-regression-debug.log`、`public-regression-release.log` |
| 外部 static SDK Debug/Release | 各 Configure、Build、Run 退出 0；`PUBLIC_BINARY_H1_STATIC_CONSUMER_PASS` | `static-debug-consumer.log`、`static-release-consumer.log`；构建 Cache/VS 项目位于对应 `out/build/windows-msvc-stage4-h1-static-*` |
| Testing-off | H1 target D/R 构建成功，`ctest -N` D/R 各 `Total Tests: 0` | `testing-off-debug.log`、`testing-off-release.log`；构建目录 `out/build/windows-msvc-stage4-h1-testing-off` |
| 门禁 | 默认 H1 `OFF`；H1=ON、public=OFF 在 configure 明确 FATAL_ERROR，退出 1 | `default-off.log`、`missing-public.log` |

H1 专项使用独立预期而非旧 backend 作 oracle，覆盖 BOOL/ENUM known-unknown/UINT64/INT64/BYTES/DECIMAL64 与实际 signed conversion raw、跨 byte bit mask/普通 byte range；bounded payload 零/中/最大与实际 SUM8/computed 存储；fixed SUM8/CRC、失败清旧成功字段/高亮和恢复；两个 Flow 的草稿/结果隔离、成功 Reset 清本 Flow 并重取 Handle；本地输入拒绝保留当前结果；实例/替换/结果容量的 exact/minus-one；callback 复制失败 `CALLBACK_FAILED/reset_required`。测试入口的断言在 Debug/Release 都执行。失败 candidate 不推断 Message，成功结果只在一次 Host output callback 中物化。

外部 consumer 工程源位于 `C:\Users\Administrator\AppData\Local\Temp\pae-stage4-h1-consumer-20260915`，只有新 adapter `.h/.cpp` 与 consumer `CMakeLists.txt/main.cpp` 四文件；分别使用 `out/sdk-stage4-p/candidate2-20260915/static-debug`、`static-release` 安装包。没有重新打包或覆盖 P 候选；consumer 实际调用 H1 `Create` 与 `Decode`，而非 PAE SDK 原示例。

## 总控复核补齐（2026-09-15）

总控复核指出成功 DTO 最终容量守卫、草稿替换峰值和 Host/compiled owner 描述三处需闭合，H1 暂不进入 H2。只在 H1 adapter、专项测试与两份 H1 文档内补齐；根 CMake、PAE、UI 和 SDK 未改。

1. [Binary `ParseFields`](../../src/config_compiler/config_compiler.cpp) 在 `fields` 空数组时返回 `EMPTY_ARRAY`；[PlanBuilder](../../src/protocol_plan/plan_builder.cpp) 亦拒绝字段为空的 Binary Message。因此没有合法的零字段成功 Decode 向量，不以无效 JSON 伪装成功路径。专项把现有独立 CRC 向量的 `fields` 置空并断言公开 Compiler 返回 `EMPTY_ARRAY`，同时对合法单字段 CRC 成功 DTO 作实际 `accounted_bytes` exact/minus-one。尽管当前合法 Binary 至少一字段，`Output` 仍在进入 `call.pending` 前无条件检查最终 `Account(candidate) <= max_result_bytes`，让理论上字段循环零次时也不能绕过容量后置条件。
2. 修正前实例账本只覆盖两份保留草稿。新增帧上限 16→17 的准入差值应为 12 字节（三份草稿各多 4 字节）测试，修正前失败见 `out/stage4-h1-validation/controller-review-pre-fix-debug.log`。现按两份保留草稿加一份替换期 pending 草稿及 pending 字符串对象计准入峰值，并在 `SetDraft` 发布前核对实际字符串 capacity+终止符不越本片 reserve。双 Flow 各 32 字符满容量、Flow 0 再以 32 字符替换，Flow 1 不变；这组满容量状态下实例/替换峰值均验证 exact 可准入、minus-one 拒绝。仍是逻辑容量账本，不是 RSS 测量。
3. [compiled state](../../src/public_api/compiled_state_internal.h) 和 [public Host 实现](../../src/public_api/host_endpoint.cpp) 表明 Host 独立保留同一冻结状态引用，并非借用 `CompiledProtocol` 对象；H1 `Adapter` 声明 `compiled_` 在 `host_` 之前，成员反序析构使 Host 先销毁。契约已纠正。成功 callback 物理查询仍使用创建 Host 的本 Adapter `compiled_`，不需要或允许第二次 Decode。

补齐后的 H1 专项在原 `out/build/windows-msvc-stage4-h1-source` 串行构建，Debug/Release 各 1/1、退出 0：`out/stage4-h1-validation/controller-review-debug.log`、`controller-review-release.log`。新 `.h/.cpp` 重新拷至上述临时外部 consumer 源目录后，static Debug/Release 的 Build/Run 均退出 0，Run 输出 `PUBLIC_BINARY_H1_STATIC_CONSUMER_PASS`：`controller-review-static-debug-build.log`、`controller-review-static-debug-run.log`、`controller-review-static-release-build.log`、`controller-review-static-release-run.log`。原 H1、public 回归、门禁及 Testing-off 日志保留并标明来源；本补齐未改 PAE/根 CMake，未重复未变部分的回归/门禁。

## 证据界限和待总控复核

H1 仍为并列、非 Qt、非可见的 complete-record Decode 片；现有 Binary UI backend 没有切换。未运行 UI、未做新增人工烟测，未迁移 Encode/stream/ASCII、未删旧桥、未重跑整个旧 Binary/UI 矩阵或 shared SDK 消费；未验证 Linux、真实协议、Golden、硬件、现场、稳定 C++ ABI、性能或正式发布。资源数字是逻辑准入和容量上界，不是进程 RSS 上限；PAE compiled owner 与 Host 共享同一冻结状态，Lab 自有描述/Flow/result/callback 副本另计。H2 的 Qt Tab/reload/关闭生命周期仍需单独派发并验收。

未 Stage、Commit、Push；无发布或删除。结论仅为“已完成派发范围，待总控复核”。
