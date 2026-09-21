# Lab 对 Windows x64 SDK 阶段 3 的后置消费复核

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-15。状态：final3 补充复核已完成，待总控复核；本报告先审查冻结的
`out/sdk-stage3/final2-20260915/`，再按总控补充派发审查最终
`out/sdk-stage3/final3-20260915/` 的消费入口及既有验证证据。未迁移 Lab，未修改源码、CMake、
脚本、README 或索引，未运行构建、测试或 UI，未获 Stage、Commit、Push 授权。

## 0. final3 补充复核后的当前结论

final2 的已知入口缺口已经在 final3 关闭：五个包均实际包含
`examples/sdk_consumer/CMakeLists.txt` 与 `main.cpp`，各自 Manifest 也收录这两个文件；包内 README
从包根给出完整的 PowerShell Configure/Build/Run 路径，固定 Visual Studio 18 2026、x64、
v142 14.29.30133，分别传入源码包的 `PAE_SOURCE_DIR` 或二进制包的 `CMAKE_PREFIX_PATH`，并把
Static/Shared 的 Debug、Release 包与唯一允许的 `--config` 明确配对。README 同时说明 Shared DLL
自动复制、系统 CRT 不随包复制、toolset/CRT 配置匹配和实验性 C++ ABI 边界。

既有 final3 日志直接证明这套入口可执行：从五个包自身的 consumer 按 README 路径完成六次运行，
均输出完整 `PAE_SDK_STAGE3_CONSUMER_PASS`；五个仓库外 build tree 均为 `Total Tests: 0`，Shared
Debug/Release 均确认 `pae.dll` 已复制到 consumer 运行目录。包完整性复核为五包通过，final2 与
final3 的 16 个 Static/Shared lib/dll 逐项 SHA-256 相同。因此，下文对 final2 二进制链接闭包、
DLL 导出/C4251 和运行依赖的代码及二进制审查结论可以限定继承到 final3；该继承基于二进制哈希
相等，不表示本轮重新构建或重做了源码审查。

`PAEConfig.cmake` 不会 fail-fast 拒绝 Debug/Release 错配的风险仍存在，但 final3 已把独立 build
tree、精确 toolset 和配置专包配对写入主入口并按正确组合验证。按当前已确认契约“消费者必须保持
配置与 CRT 匹配”，这项风险降为消费健壮性与未来集成注意项，不再阻断阶段 3 本地 SDK 技术收口；
若以后要求 package 自身抵御误用，再单独增加配置约束和负向验证，不应把该增强误作当前 Core/API
能力缺口。

## 1. final2 首次复核结论

final2 已在限定矩阵内形成可消费的技术闭包：源码包能够离线 `add_subdirectory`，Static
Debug/Release 通过单一 `PAE::pae` 封装公开 facade 与五个内部 archive，Shared Debug/Release
提供同一公开 target、import library 与运行时 DLL；五包公开头逐文件一致，公开 include 未泄漏
Plan/Core、Lab、Qt、测试 support 或开发仓库路径。既有仓库外 consumer 在 MSVC v142、x64、匹配
`/MDd` 或 `/MD` 的六个组合中均通过 Compiler/metadata、Codec、StreamFramer、Host Decode 和同一
Encode Handle 多 Message。未发现 DLL 缺失必要导出、静态依赖未闭合、共享包仍要求消费者链接内部
archive，或公共头无法独立形成消费边界的新增确定阻断。

首次复核时，final2 仍不能作为阶段 3 的最终消费入口直接交给 Lab：其 README 缺少可执行命令，
四个二进制包也没有随包 consumer。这是总控当时已识别并单独派发到 final3 的既知入口缺口，不是
本报告的新发现；其当前关闭状态以第 0 节 final3 补充复核为准。

此外有一项应由总控在阶段 3 收口时明确处置的 **配置专包误用风险**：Debug/Release 包中的
`PAEConfig.cmake` 都用通用 `IMPORTED_LOCATION` / `IMPORTED_IMPLIB`，没有
`IMPORTED_CONFIGURATIONS`、配置映射或错配检查。Visual Studio 多配置消费者配置一次后，会让所有
配置都指向同一份 Debug 或 Release 库；`PAE_MSVC_RUNTIME_LIBRARY` 也只是信息变量，不会改变或验证
消费 target 的 CRT。现有通过证据只覆盖“Debug 包构建 Debug、Release 包构建 Release”，不能外推为
错误组合会在 configure 阶段安全失败。按当前契约，消费者本来就必须选择匹配包、工具集和 CRT，
因此这不是 PAE 编解码能力缺口。final3 已把“独立 build tree + 精确 `--config` + 对应包目录”写成
不可省略的主入口前置条件并验证正确组合；package 内增加 fail-fast 与错配负向验证属于可选健壮性
增强，不再作为当前阶段 3 阻断。

## 2. 冻结快照与证据边界

- 复核基线为 `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`；接管时暂存区为空，保留共享
  工作树全部既有修改。
- 首次审查 final2 五个目录、[阶段 3 契约](../../engineering/pae-sdk-stage3-contract.md)、生成后的 CMake package、
  公开头、provenance/manifest/hash及既有日志；补充轮审查更新后的
  [阶段 3 验证记录](../../engineering/pae-sdk-stage3-windows-validation.md)、final3 五包 README、随包 consumer、
  Manifest及 final3 入口/完整性/二进制比对日志，没有重做未变的 final2 源码检查。
- 仓库外验证根分别记录为 `F:\PersonalWorkspace\pae-sdk-stage3-validation-final2-20260915` 与
  `F:\PersonalWorkspace\pae-sdk-stage3-validation-final3-20260915`。本任务只读取其已生成项目和日志，
  没有重新配置、编译或运行；六次 final3 README PASS、7/7 回归、包哈希与 DLL 检查均属于 PAE
  交付证据，不是本任务新增动态验证。
- final2/final3 的 `source_worktree_dirty=true` 与 dirty snapshot 说明一致；这些本地产物位于 ignored
  `out` 下，不是提交物、签名包、正式发行物或可授权再分发物。

## 3. 三种形态的消费闭包

| 形态 | 消费侧结论 | 证据与限制 |
| --- | --- | --- |
| Source | 限定通过 | 包含公开头、必要实现/CMake、yyjson 源码及许可证、Schema/合成配置和 consumer；既有 D/R 仓库外构建运行 PASS，测试列表为 0。final2 README 缺命令的历史问题已由 final3 README 和随包 consumer 关闭。 |
| Static Debug/Release | 限定通过 | `PAE::pae` 传播 `PAE_STATIC_DEFINE=1`，并在 `INTERFACE_LINK_LIBRARIES` 内封装 Compiler/Core/Framing/Plan/yyjson 五个 archive；已生成 consumer 工程只命名 `PAE::pae`，最终不依赖 `pae.dll`。闭包只验证了与包配置匹配的 `/MDd` 或 `/MD`。 |
| Shared Debug/Release | 限定通过 | package 以自身 prefix 定位 `lib/pae.lib`、`bin/pae.dll` 和公开 include；consumer 的 POST_BUILD 复制 `$<TARGET_FILE:PAE::pae>`，运行时确实依赖 `pae.dll`。DLL 自身 Debug 依赖 Debug CRT，Release 不依赖 Debug CRT。 |
| Relocation | 通过既有证据 | 五包复制到仓库外新根后重新配置并运行，生成目标路径均指向复制后的 prefix；Config 与公开头未发现开发仓库绝对路径。未验证含特殊字符/只读安装位置或安装器。 |
| 内容边界 | 通过 | 五包七个公开头的 SHA-256 逐文件相同；公开头只依赖标准库及 `pae/**`。二进制包不安装内部头，Shared 不安装内部 archive；源码白名单未含 tests、tools、spikes、Qt 或 test support。 |

静态 target 在生成的 Visual Studio consumer 工程中展开为 `pae.lib` 加五个内部 archive，说明消费者
无需认识内部 target；这是链接闭包证据，不表示这些内部 archive 已成为公共 API。Shared 的 consumer
只链接 import library，DLL 将内部实现静态吸收；既有 export 扫描未出现 Plan/Core/Compiler/
Framer/yyjson 内部 target 标识。

## 4. DLL 导出、C4251 与生命周期边界

C4251 不能单凭“出现 warning”判定动态包失败，也不能因同矩阵 consumer 已运行就视为没有风险。
本次把具体成员及调用位置拆开核对：

1. `CompiledProtocol`、`CompleteRecordCodec`、`StreamFramer`、`HostEndpoint` 的 pImpl 成员是
   `std::unique_ptr`，其公开析构和 move 操作均由导出的非内联成员实现。final2 DLL 导出对应析构，
   consumer import 表也实际引用这些析构，常规 owner 销毁在 DLL 边界内执行。
2. `CompileResult` 内含 `std::optional<CompileDiagnostic>`；其 move、析构和访问函数同样由 DLL 导出。
   `HostChannelHandle` 内含 `std::weak_ptr<HostScopeToken>`，虽然头中 default constructor 写成
   `= default`，MSVC 的 class export 实际生成并导出了 ctor/copy/move/assignment/dtor；既有 Shared
   consumer import 表明确引用其析构。因此不能把这两个成员简单描述成“必然由消费者自行析构”。
3. 风险仍真实存在：工厂返回的普通 aggregate 中含 `std::unique_ptr`，诊断与多处返回/参数类型含
   `std::string`、`std::string_view`、`std::optional`，这些模板类型、对象布局、分配与释放跨越 C++
   DLL ABI。即便 pImpl 析构在 DLL 侧，同工具集和动态 CRT 一致仍是必要条件。final2 只证明 v142
   14.29、x64、Debug `/MDd` 与 Release `/MD` 的匹配组合，没有证明跨 toolset、`/MT[d]`、错配
   Debug/Release 或稳定 ABI。
4. class-level `PAE_API` 还导出了公开类的若干 private constructor 和 private helper decorated
   symbol，例如 `CompleteRecordCodec::AdvanceViewEpoch`。它们不在公开头的可调用接口中，且未泄漏
   私有 Plan/Core 类型，故在当前“实验性 C++ API、无稳定 ABI”契约下不是单独阻断；但 export surface
   比四个工厂函数更宽，后续不能仅凭工厂存在就宣称导出面已精确冻结。

因此，本报告不因 C4251 单独否决 Shared 包；接受它的前提严格限于当前同 toolset、同 arch、同配置、
同动态 CRT 的本地 SDK。若未来要求跨工具集、插件式长期 ABI 或第三方不可控编译环境，应另行设计
C ABI/opaque handle 或 DLL 内分配释放成对接口，而不是把该目标扩进阶段 3。

## 5. CMake 与 Lab 后续接入约束

后续 Lab 若消费二进制包，最小安全入口应保持：

1. Debug 与 Release 使用不同 build tree，并把对应 `pae-sdk-*-debug` 或
   `pae-sdk-*-release` 作为唯一 PAE package prefix；每次构建显式给出匹配的 `--config`。
2. 继续只链接 `PAE::pae`；Static 不手写内部 archive，Shared 用 `$<TARGET_FILE:PAE::pae>` 部署 DLL，
   不把 SDK `bin` 永久加入全局 PATH，也不复制系统 CRT DLL。
3. Lab toolchain 必须现场确认 x64、MSVC v142 ABI 兼容和动态 CRT；不能只读取
   `PAE_MSVC_RUNTIME_LIBRARY` 变量后假定 CMake 已替消费者强制匹配。`PAEConfigVersion.cmake` 能拒绝
   32 位消费者，但不检查 MSVC toolset 或 Debug/Release 包错配。
4. 配置、Plan、Codec/Framer/Host owner 及 borrowed view 生命周期仍按公开 API 契约处理。替换 DLL、
   重建配置或切换 package 时必须重建 owner，不能跨 DLL 版本保留 Handle、view、STL 返回对象或冻结
   状态。

这些是未来接入约束，不构成本轮 Lab 迁移授权，也不要求现在修改 Qt kit、本机 Qt、UI 或现有 Lab
构建路径。

## 6. 阶段 3 阻断、阶段 4 缺口与停点

阶段 3 当前消费侧结论：

- final2 已知 README/随包 consumer 入口缺口已由 final3 关闭；在当前已确认的配置匹配契约下，未
  发现剩余的阶段 3 本地 SDK 技术包装阻断。配置专包 fail-fast 与错配负向验证保留为健壮性增强。
- 项目级 PAE 对外许可证仍缺失；它不阻断本地技术复核，但阻断正式外部分发。

以下不是阶段 3 包装阻断，继续留在阶段 4 或更后续：Lab 从私有实现迁移到公开 API、Qt UI 与人工
验收、网络/线程/设备生命周期、完整安装器与签名、Linux、跨 toolset ABI、硬件、Golden、现场和
正式性能验证。Stage 3 的 public-only consumer PASS 不能替代这些证据。

本任务已停止写入；唯一新增文件为本报告。未修改 Lab、源码、CMake、脚本、README、计划、验证
记录或其他报告，未 Stage、Commit、Push。
