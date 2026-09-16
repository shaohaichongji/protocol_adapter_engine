# PAE Windows x64 SDK 阶段 3 交付契约

日期：2026-09-15。状态：已确认并进入本地实施；本文不是正式发布、许可证授予、稳定 ABI 或生产可用承诺。

## 1. 范围与三种形态

本阶段只交付当前公开 C++17 API：Compiler/metadata、COMPLETE_RECORD Codec、StreamFramer 和 HostEndpoint。
三种形态均只向消费者暴露 `PAE::pae`，不包含 Lab、Qt、Socket、线程、Schema 扩展或新的协议行为：

- 源码包：白名单复制公开头、必要实现/CMake 闭包、yyjson 及许可证、Schema、公开合成配置和示例；
  从开发仓库外的展开目录可完全离线配置、构建和消费。
- 静态包：包含公开头、`pae.lib`及其私有 archive 闭包。生成的 `PAEConfig.cmake`在
  `PAE::pae`内部封装传递链接项，消费者不命名或手工链接内部模块。
- 动态包：包含公开头、`pae.dll`、import library 和可重定位 `PAEConfig.cmake`。内部模块静态吸收进
  DLL，不安装其 archive，不使用 blanket symbol export。

每个 Debug/Release、static/shared 组合使用独立包目录；不把多配置二进制混放成一个未经验证的包。

## 2. ABI、导出与 CRT

- Windows x64 使用现场 MSVC v142 工具集及 Windows SDK；公开接口不承诺跨编译器、跨工具集或稳定
  C++ ABI。消费者必须使用 ABI 兼容的 MSVC 工具链。
- SDK 构建和验证统一使用动态 CRT：Debug `/MDd`，Release `/MD`。包 provenance 和 CMake 配置公开
  `MultiThreaded$<$<CONFIG:Debug>:Debug>DLL`，消费者须保持匹配；该变量仅提供信息，不自动验证或
  改写任意消费者的 CRT/toolset。
- 静态包通过 `PAE_STATIC_DEFINE`关闭 import/export修饰；动态包仅在构建 `pae`时定义
  `PAE_BUILDING_LIBRARY`。所有公开非内联类、工厂和析构继续由 `PAE_API`导出；内部 Plan/Core/
  Compiler/Framer目标及yyjson不作公开导出。
- 产品包必须以 `BUILD_TESTING=OFF`、`PAE_BUILD_TESTING=OFF`构建，禁止公开 Codec/Host测试 hooks。
- Debug/Release 二进制专包在 `PAE::pae`上携带单一配置门禁。VS 多配置工程仍可正常生成；选择与
  专包不一致的配置时，链接阶段必须以稳定的 `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE`
  哨兵拒绝，不能静默链接另一配置的库。该门禁不扩展为 CRT 或 toolset 自动检测。
- class-level `PAE_API`可能生成 private constructor/helper 的 decorated export；它们不是公开头中可
  调用的 API，也不构成稳定导出面承诺。本阶段只保证同 toolset/arch/config/动态 CRT 的实验 C++ API。

## 3. 可重定位 CMake 消费

二进制包提供 `lib/cmake/PAE/PAEConfig.cmake`和版本文件。消费者只需：

```cmake
find_package(PAE CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE PAE::pae)
```

配置文件只基于自身 `PACKAGE_PREFIX_DIR`定位头、库和 DLL，不记录开发仓库或构建目录。动态消费者的
部署步骤需把 `$<TARGET_FILE:PAE::pae>`复制到可执行文件目录；不复制系统 DLL。

源码消费使用同一 `PAE::pae`：消费者把展开目录作为 `add_subdirectory`源，并显式关闭测试、开启
`PAE_BUILD_PUBLIC_API_STAGE1`。源码包不得联网取依赖，也不得回读开发仓库。

## 4. 白名单、来源与许可证

包内只允许公开头、必要私有实现/CMake闭包、yyjson源和原许可证、Schema、公开合成配置/示例及本
阶段使用说明。排除 `.git`、`out`、测试、spikes、Lab、Qt、内部工程历史和原始验收证据。

每个包必须包含：

- `MANIFEST.txt`：相对路径与文件大小；
- `SHA256SUMS.txt`：除自身外的包内容哈希；
- `PROVENANCE.json`：包形态、配置、架构、工具链、CRT、base HEAD、dirty状态和生成时间；
- `LICENSES/yyjson-LICENSE.txt`：第三方原许可证；
- `PAE-SDK-README.md`：构建/消费命令、兼容边界和许可证门槛。

仓库当前没有可用于对外分发的 PAE 项目许可证。本地验证包不授予 PAE 代码再分发许可；正式对外
发布必须先由权利人确定许可证。哈希用于内容复核，不是签名、来源认证或防篡改保证。

## 5. 验证门槛

三种形态均在开发仓库外的新目录使用同一 public-only consumer验证：

1. Compiler及公开metadata；
2. Codec Decode/Encode；
3. StreamFramer候选形成；
4. Host Decode以及同一Encode Handle依次选择多个Message；
5. Debug/Release分别构建运行，动态包同时检查 DLL/import lib及运行依赖；
6. 检查包内无开发仓库绝对路径、Qt、Lab、私有头或测试 hooks泄漏。

验证通过只说明指定 Windows x64 工具链下的本地包构建和消费闭合，不外推到 Linux、SDK安装器、
签名、网络、硬件、Golden、现场、正式性能或生产发布。
