# 开发者最小宿主与公开接口核查（2026-09-26）

## Findings（优先）

1. **已实际复现：source 示例会强制关闭宿主测试。**
   `examples/getting_started/CMakeLists.txt:5-10` 在 source 模式以
   `set(BUILD_TESTING OFF CACHE BOOL "" FORCE)` 修改全局 Cache；
   `examples/yaml_sdk_consumer/CMakeLists.txt:5-10` 也采用同一写法。
   有自己 CTest 的最小 C++17 宿主先 `include(CTest)` 得到
   `HOST_BUILD_TESTING_BEFORE=ON`，再嵌入前一示例后变成 `AFTER=OFF`，
   `CMakeCache.txt` 为 `BUILD_TESTING:BOOL=OFF`，`ctest -N` 仅 **0 个测试**。
   触发条件是把示例 CMake 当作已有宿主的子目录，而非独立运行示例。
   这是示例嵌入风险，不能推断产品根构建默认吞宿主测试；根工程在非 top-level 时
   将 `PAE_BUILD_TESTING` 默认设为 OFF，见 `CMakeLists.txt:475-489`。
   使用隔离对照宿主直接 `add_subdirectory(<source 包>)`、用普通目录变量设置
   `PAE_BUILD_TESTING=OFF` 后，`BUILD_TESTING` 保持 ON，自有公开 API
   Decode/Encode 测试 **1/1** 通过。建议总控将“勿直接嵌入现有示例 CMake”列为
   上手必修说明；是否另派示例最小修复由总控决定，本片未改既有 CMake。
2. **已实际复现：不同 SDK 包复用同一 BuildRoot 可继续命中旧包。**
   隔离目录先以 static Release 包配置，`PAE_DIR` 被 Cache 到 static 包；
   在同一 BuildRoot 仅把 `CMAKE_PREFIX_PATH` 改成 shared 包重新 Configure，
   `HOST_PAE_DIR` 仍指 static，导入目标仍为 `STATIC_LIBRARY`，宿主的 shared
   类型断言使 Configure 退出 1。显式改 `PAE_DIR=<shared 包>/lib/cmake/PAE`
   后 Configure 恢复为 `SHARED_LIBRARY`；另用全新 shared BuildRoot 从开始即成功。
   这是 CMake 包发现缓存的接入风险，不是“shared 包构建失败”。指南必修规则应是
   每个包与配置使用独立的新 BuildRoot，并检查 `PAE_DIR`、目标类型；不以仅改
   `CMAKE_PREFIX_PATH` 代表包已切换。见 `cache-*.log`。

未发现需要在本片修改公共 API、ABI 或根构建策略的已证实产品缺陷。

## 隔离实验与实际结果

现场主仓库起点 `main@8eb5bb665b26efb99f86c8388c922c761ad62ed4`，暂存区空，
已有总控计划类文档变更；进行中 A 任务又修改指南和 `docs/sdk/README.md`，
本片未读其改动作为已验证规范、未写这些文件。实验只读现有
`b12ad809bdbc35589e8e5755f0e4ad04f386fa10` 干净候选的 source、
static YAML Release、shared YAML Release 包，先确认各包 provenance 为该 HEAD 且
`source_worktree_dirty=false`。所有实验源码/构建在忽略的
`out/build/developer-integration-audit-20260926`，日志在
`out/evidence/developer-integration-audit-20260926`；现用 SDK 包和旧构建根未写入。

使用 `Visual Studio 18 2026`、x64、MSVC `v142,version=14.29.30133`，
宿主源码 `host_probe.cpp` 只包含公开头，读取公开合成配置，执行
`CompileProtocolJson`、`CreateCompleteRecordCodec`、Decode/Encode 并检查
`AA 00 07`；binary 变体额外真实执行可选 YAML→严格 JSON。实验文件不是产品
接口或拟提交示例。

| 实验 | 命令要点与断言 | 结果/日志 |
| --- | --- | --- |
| 示例 FORCE | `cmake -S <隔离>/source_forced -B <隔离>/forced-build -DPAE_SOURCE_DIR=<b12ad80 source 包> -DBUILD_TESTING=ON`；随后 `ctest -N` | Configure 0；前 ON、后 OFF，Cache OFF；CTest **0**。`source-forced-configure.log`、`source-forced-ctest-list.log`。 |
| 不强制全局开关的对照 | `cmake -S <隔离>/source_controlled -B <隔离>/controlled-build ... -DBUILD_TESTING=ON`；`cmake --build ... --config Release --target host_own_test`；`ctest -C Release` | Configure/Build/CTest 均 0，宿主 **1/1**，Cache 的 `BUILD_TESTING=ON`；`source-controlled-{configure,build,ctest}.log`。该方式没有完全消除 PAE Cache 选项：公开 API 会按既有根规则启用自身必需能力；未见 Qt/Lab 目标进入所建宿主目标。 |
| static Release 包 | `cmake -S <隔离>/binary_host -B <隔离>/binary-static-build -DCMAKE_PREFIX_PATH=<static 包> -DHOST_EXPECT_STATIC=ON`；Build/CTest Release | 各退出 0，宿主 **1/1**。`PAE::pae` 为 `STATIC_LIBRARY`，`INTERFACE_LINK_LIBRARIES` 含五个内部 `.lib`，传播 `PAE_STATIC_DEFINE=1`；`binary-static-{configure,build,ctest}.log`。 |
| shared Release 包 | 同一实验源码但**独立** `binary-shared-build`、shared 包、`HOST_EXPECT_STATIC=OFF` | 各退出 0，宿主 **1/1**。`PAE::pae` 为 `SHARED_LIBRARY`，未传播静态宏；消费端 `pae.dll` 与该包 `bin/pae.dll` SHA-256 相同；`binary-shared-{configure,build,ctest,dll}.log`。 |
| 配置错配负例 | static **Release** 包配置新的消费 BuildRoot，`cmake --build --config Debug --target host_probe` | Configure 0，Build 退出 **1**，精确命中 `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_RELEASE_PACKAGE.lib`，不是源码编译错误；`mismatch-{configure,debug-build}.log`。 |
| 同根切包 | 先 static 配置，再于同一 `cache-reuse-build` 仅改 `CMAKE_PREFIX_PATH`/期望类型指向 shared；最后显式 `-DPAE_DIR=<shared 包>/lib/cmake/PAE` | 初次 Configure 0；第二次退出 **1**且仍是 static 目标；显式指定目录后 Configure 0、shared 目标；`cache-first-static-configure.log`、`cache-reuse-shared-configure.log`、`cache-explicit-pae-dir-reconfigure.log`。 |

宿主最小代码和 CMake 均只在上述隔离 out 内。未重跑 SDK 全矩阵、完整 Lab 或
非 Loopback 网络测试，也未声称直接 VS/qmake 手工链接已验证。

## 公开接口使用事实（静态审查，非新增运行测试）

| 分类 | 准确规则与源码定位 | 对中文指南的取舍 |
| --- | --- | --- |
| 选型 | 完整记录的一次 Decode/Encode 用 `CompleteRecordCodec`（`include/pae/codec.h:205-249`）；流式分段先查 Pipeline 输入/Framing 描述，再创建 `StreamFramer`，其 `Push/Continue` 只形成候选帧、不自动业务 Decode（`include/pae/stream_framer.h:51-86,137-185`）；需按端点/方向绑定、串起 Framer、Codec、回调时才用 `HostEndpoint`（`include/pae/host_endpoint.h:50-58,168-216`）。 | **必修**：先按输入形态与是否需要绑定选择，不把 Framer 候选当协议成功，也不默认所有宿主都要 Host。 |
| 编译 owner/view | `CompiledProtocol` 描述中的 `string_view` 借用 owner，移动/替换/销毁后重新查询（`include/pae/compiler.h:117-121`）。Codec/Framer/Host 创建时各自获取引用计数的冻结 `CompiledStateRef`（`src/public_api/compiled_state_internal.h:10-112`；三处 Create 分别见 `codec.cpp:597-601`、`stream_framer.cpp:345-350`、`host_endpoint.cpp:576-582`），不应把描述 view 的寿命与执行对象持有 Plan 的寿命混为一谈。 | **必修**：业务长期保存需复制描述值或重查，不持有旧 view。 |
| Codec 输入/输出 | `ByteView` 和 `MutableByteBuffer` 都不拥有字节（`include/pae/codec.h:59-67`）；Decode 记录/字段 view 借 Codec，下一次受保护 Decode/Encode、移动或销毁后过期；BYTES 还借未修改输入（同文件 `130-174,213-223`）。Decode 后匹配索引不是成功标志；失败不发布可用 record（`184-194`）。Encode 失败 `bytes_written=0`，Buffer 即使被修改也不能发送（`219-223`）。 | **必修**：同步消费/复制所需值，先判断 status，再发送成功长度；失败 Buffer 不用。容量与创建结果要显式检查。 |
| Framer 调用 | 回调是同步 `noexcept`，候选 bytes 只在回调期间借用；`Push` 返回 `bytes_consumed`，未消费后缀仍由调用者持有，空输入等同 `Continue`；`Observe` 是串行空闲查询，非并发快照（`include/pae/stream_framer.h:80-88,114-154`）。资源 override 有硬上限（同文件 `95-111`）。 | **必修**：回调外需复制候选，按消耗量续送、按 stop reason/`has_internal_work` 判断 Continue；不要在回调内移动/销毁或重入实例。 |
| Host 生命周期/恢复 | `HostChannelHandle` 非 owning；Host 销毁、跨实例或成功 Reset 后分别可能 `EXPIRED_HANDLE`、`FOREIGN_HANDLE`、`STALE_HANDLE`（`include/pae/host_endpoint.h:80-98,168-190`；`src/public_api/host_endpoint.cpp:210-220,371-390`）。回调异常转 `CALLBACK_FAILED` 并使通道 `reset_required`，成功 Reset 增 generation、清故障，随后须 `Find` 新 handle（`host_endpoint.cpp:224-267,371-390,419-431`）。`HostOperationResult` 的 `codec_attempted`/`codec_status` 与交付 status 不等同，只有 `HostStatus::OK` 表示业务回调正常完成（头文件 `136-153`）。 | **必修**：Handle 不持久化；检查返回状态与 reset_required，Reset 成功后重新 Find；回调中复制需跨调用保留的 Frame/Bytes/record 数据。 |
| 并发与边界 | 同一 Codec/Framer/Host 实例有非等待占用保护，冲突返回 `WORKSPACE_BUSY`，Framer/Host 回调重入返回 `REENTRANT_CALL`（`codec.cpp:321-335`、`stream_framer.cpp:180-247`、`host_endpoint.cpp:309-318,394-402`）。Host 不承担 Socket、线程或业务路由。 | **必修**：同实例串行调用；不要把这些状态当自动重试承诺。**可选**：在有多通道场景时再解释 Host 绑定与限额。 |
| YAML owner/位置 | `ConvertToStrictJson` 只在调用中借输入；成功结果自持 JSON/来源映射，失败没有可用 JSON；`Json()`/`SourceIdentity()` 的 view 在结果移动、赋值或销毁后失效，`SourceLocation` 是复制值，祖先回退带独立标志（`src/config_frontend_yaml/public/pae/yaml_frontend.h:36-78`）。生成 JSON 仍须再经 `CompileProtocolJson`；不可把 JSON offset 当原 YAML offset。 | **必修**：两阶段诊断、owner 存活和精确/祖先来源区分。 |
| 手工链接 | 当前 static `PAE::pae` 的接口传递五个内部 `.lib`，YAML 另有 `PAE::yaml_frontend` 静态目标；shared 是同包 DLL/导入库组合（`cmake/PAEConfig.cmake.in:29-114`；本次 `binary-{static,shared}-configure.log`）。 | **可选说明**：原生 VS/qmake 手工链接不是单个 `pae.lib` 的已验证闭包，建议优先 CMake；**暂缓**合并库、非 CMake 集成支持或 ABI 改造，不能从本实验推出承诺。 |

## 建议与交付状态

- **必修、待另派**：源示例作为子目录复用时避免全局 `BUILD_TESTING` FORCE；教程明确
  “示例独立运行”与“已有宿主受控接入”两种路径，以及每包/每配置新 BuildRoot。
  两个触发条件均已有反例/对照，不要求本轮修改根 CMake 或公开 API。
- **可选**：在有具体非 CMake 宿主需求后再写额外链接说明；当前 CMake target 已验证
  传递依赖/宏，不能据此称手工链接闭包已验。
- **暂缓**：公共 API/ABI、单库打包、更多 Host 门面、Linux/Qt/人工体验和跨工具链
  验证。这些均超出本轮授权与证据。

本轮只新增本报告；`git diff --check` 与新增报告格式/敏感路径检查另按收口现场记录。
未 Stage、Commit、Push、删除旧产物、覆盖部署或发布。状态为
**已完成派发范围，待总控复核**。
