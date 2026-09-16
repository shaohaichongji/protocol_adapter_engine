# PAE Windows x64 SDK 阶段 3 验证记录

日期：2026-09-15。状态：已完成派发范围，待总控复核。

本记录只覆盖当前公开 C++17 Compiler/metadata、COMPLETE_RECORD Codec、StreamFramer 与
HostEndpoint 的 Windows x64 本地源码包、静态包和动态包。它不是正式发布、稳定 ABI、许可证授予
或生产可用声明。

## 1. 基线与产物

- 现场分支/HEAD：main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc。
- 构建工具：Visual Studio 18 2026，MSVC v142 14.29.30133，Windows SDK
  10.0.22621.0，x64。
- 最终本地包：out/sdk-stage3/final6-20260915/。
- 功能与配置门禁验证根：F:\PersonalWorkspace\pae-sdk-stage3-validation-final5-20260915。
- 日志根：out/sdk-stage3-validation/recovery/。

五个最终目录分别为源码包、Static Debug/Release、Shared Debug/Release。每个目录都有
PAE-SDK-README.md、PROVENANCE.json、带字节数的 MANIFEST.txt、SHA256SUMS.txt 和
LICENSES/yyjson-LICENSE.txt；源码包另保留锁定的 yyjson 源码与原许可证。Provenance 明确记录
基线 HEAD 与 dirty snapshot，没有把未提交工作树描述为干净发布。

总控初核后补齐包可用入口：五个包均包含 examples/sdk_consumer/CMakeLists.txt 与 main.cpp；
PAE-SDK-README.md 给出从包根执行的完整 PowerShell Configure/Build/Run 命令，固定说明
Visual Studio 18 2026、x64、v142 14.29.30133 三项生成器参数，以及源码 PAE_SOURCE_DIR 或二进制
CMAKE_PREFIX_PATH。README 同时说明配置匹配、DLL 自动复制、系统 CRT 不随包复制及实验性 C++
ABI 边界。

二进制包是严格单配置包。`PAEConfig.cmake` 为 VS 多配置生成器声明完整配置集合，但只有包自身
配置指向真实 `pae.lib`/import library；其他配置指向带稳定
`PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE` 名称的不存在输入，使错配在链接前停止，
而不修改消费者的 `CMAKE_CONFIGURATION_TYPES`、`CMAKE_BUILD_TYPE` 或 CRT。包内
`PAE_MSVC_RUNTIME_LIBRARY` 仅为构建信息，不自动验证任意消费者的 CRT 或工具集。

## 2. 实现与首次失败证据

阶段 3 新增/调整：

1. 根 CMake 固定 Windows SDK 包的动态 CRT 策略：Debug /MDd、Release /MD。
2. pae_public_api 按 BUILD_SHARED_LIBS 生成 Static 或 Shared；Static 传播
   PAE_STATIC_DEFINE，Shared 仅构建 DLL 时定义 PAE_BUILDING_LIBRARY。
3. 安装规则生成可重定位 PAEConfig.cmake。两种二进制包均只暴露 PAE::pae；Static 的私有
   archive 闭包由目标内部封装，Shared 不安装内部 archive；配置专用 imported location 保证
   Debug/Release 二进制包不会被另一配置静默消费。
4. 白名单打包脚本生成源码闭包、清单、Hash、provenance 与许可证边界；排除 tests、Lab、Qt、
   spikes、test-support 头、.git 和 out。
5. 同一综合 Consumer 覆盖 Compiler/metadata、Codec、Framer、Host Decode，以及同一
   Encode Handle 选择两个 Message。

恢复后的首次 Shared Debug 构建实际失败：公开工厂函数先由未修饰的 friend 声明，再由
PAE_API 声明，MSVC 报 C2375“重定义；不同的链接”。原始日志为
build-shared-debug.log。修复是让四组 friend 工厂声明使用同一 PAE_API，未改变函数签名；
build-shared-debug-after-export-fix.log 与 Release 构建随后通过。

Shared 构建仍报告 MSVC C4251：公开 C++ 类的私有 unique_ptr、weak_ptr 或 optional
成员没有独立 DLL 接口。公开析构均为 out-of-line 导出，实际同工具集、同 CRT Consumer 已通过；
本阶段明确不承诺稳定或跨工具集 C++ ABI，因此将其保留为兼容边界而非静默抑制警告。

另有一次源码 Consumer 运行退出 4，原因是验证命令误把 Lab Exchange 配置传给明确要求
3-Pipeline Stream Framing 向量的 Consumer；日志为 run-consumer-source-debug.log。改用
synthetic_stream_framing_slice.pae.json 后 D/R 均通过，这不是产品实现失败。

## 3. 实际验证

### 3.1 产品包构建与安装

Static 与 Shared 使用独立构建目录，均以 BUILD_TESTING=OFF、PAE_BUILD_TESTING=OFF、
PAE_BUILD_PUBLIC_API_STAGE1=ON 配置。Debug/Release 串行构建并分别安装；四个组合全部退出 0。
生成的两个产品 vcxproj 未出现 PAE_ENABLE_PUBLIC_CODEC_TEST_HOOKS 或
PAE_ENABLE_PUBLIC_HOST_TEST_HOOKS。

相关日志：

- configure-static.log、configure-shared.log
- build-static-debug.log、build-static-release.log
- build-shared-debug-after-export-fix.log、build-shared-release.log
- install-final6-*.log、package-final6-*.log

### 3.2 仓库外 Consumer

最终五个包复制到仓库外新目录后重新配置。源码包不读取开发仓库；二进制包只执行
find_package(PAE CONFIG REQUIRED) 并链接 PAE::pae。实际结果：

| 形态 | Debug | Release |
| --- | --- | --- |
| Source | PASS | PASS |
| Static | PASS | PASS |
| Shared | PASS | PASS |

六次运行均输出：

    PAE_SDK_STAGE3_CONSUMER_PASS compiler=1 metadata=1 codec=1 framer=1 host=1 multi_message_encode=1

最终补齐入口后，再从各包自身的 examples/sdk_consumer 按 README 命令执行。`final5` 的六组
正向构建与运行均通过，对应 `build-final5-*.log`、`run-final5-*.log` 与
`final5-positive-summary.log`。

配置错配负例覆盖 Static Debug→Release、Static Release→Debug、Shared Debug→Release、
Shared Release→Debug。四组均以构建退出码 1 拒绝，并分别命中包期望配置的稳定标识；证据为
`build-final5-negative-*.log` 与 `final5-configuration-guard-negative-summary.log`。修复前
`final4` 的首次负例虽因 CRT 符号不匹配而失败，但没有命中门禁标识，故不把它计作有效门禁证据；
原始日志保留为 `build-final4-negative-static-debug-wrong-release.log`。

### 3.3 CMake、DLL 与内容边界

- Source、Static D/R、Shared D/R 五个 `final5` 仓库外构建目录执行 ctest -N，均为
  Total Tests: 0；见 `final5-ctest-inventory.log`。
- Shared D/R 的 dumpbin /exports 均包含 CompileProtocolJson、CreateCompleteRecordCodec、
  CreateStreamFramer、CreateHostEndpoint，未匹配内部 Plan/Core/Compiler/Framer/yyjson 标识；
  未启用 blanket export。
- Shared Consumer 依赖 pae.dll；Debug 匹配 Debug CRT，Release 未依赖 Debug CRT。
  Static Consumer 不依赖 pae.dll。
- 最终包逐项复算全部 SHA-256、Manifest 字节数与许可证文件，五包均通过。
- 包内容未发现开发仓库绝对路径；源码白名单未包含 tests、tools、spikes、Qt 或
  *_test_support。

final5 与 final4 的 16 个 Static/Shared lib/dll 文件逐项 SHA-256 相等，因此配置门禁修正没有
重复构建 PAE 本体；`final5` 完成上述 6 个正向和 4 个错配负向验证。随后仅将三个源码示例 README
中的开发机绝对路径改为可搬移包根表达，生成 `final6` 最终候选；功能构建输入未改变。最终包继续
逐项复算 Manifest 与 SHA-256，并执行开发路径扫描。证据日志：`final5-binary-hash-compare.log`、
`final6-package-integrity.log`、`final6-content-delta.log`、`final6-package-path-scan.log`，以及既有
final2-testing-off-and-dll.log、
dumpbin-final2-exports-*.log、dumpbin-final2-dependents-shared-*.log。

### 3.4 受影响源码回归

独立 Testing-on 目录 out/build/windows-msvc-sdk-stage3-regression 仅运行
^pae\.public_api\.：

- Debug：7/7 通过；
- Release：7/7 通过。

覆盖 compile metadata、header self-contained/boundary、Codec、consumer metadata、
StreamFramer 和 HostEndpoint。最终日志为 ctest-public-api-final-debug.log 与
ctest-public-api-final-release.log。

## 4. 未覆盖与发布阻断

- 未执行完整仓库、Protocol Lab、Qt UI、人工验收、网络、Linux、硬件、Golden、现场或性能测试；
  Stage 3 未修改这些行为。
- 未验证跨 MSVC 工具集或跨 CRT 的 C++ ABI；契约要求消费者使用 ABI 兼容工具链和匹配配置。
- Hash 只用于内容复核，不是签名、来源认证或防篡改保证。
- 仓库没有 PAE 项目级对外分发许可证。该事项不阻断本地技术复核，但阻断正式对外发布。
- 本轮没有 Stage、Commit 或 Push；最终产物仍是 ignored out 下的本地审查材料。
