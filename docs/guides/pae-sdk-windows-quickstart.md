# PAE Windows x64 SDK 快速入口

本指南面向当前可定位的 Windows x64 本地 SDK 候选。它不是正式发布说明；每个包内的
`PAE-SDK-README.md` 和 `examples/sdk_consumer` 是对应包的首要、可复现入口，本页只负责选包和
说明共同边界。

## 1. 选择包目录

从仓库根目录进入当前 `candidate1-20260919` 候选：

| 形态 | 目录 | 配置 |
| --- | --- | --- |
| Source | `out/sdk-clean-checkpoint/candidate1-20260919/pae-sdk-source` | 同一源码包构建 Debug、Release |
| Static Debug | `out/sdk-clean-checkpoint/candidate1-20260919/pae-sdk-static-debug` | 仅 Debug |
| Static Release | `out/sdk-clean-checkpoint/candidate1-20260919/pae-sdk-static-release` | 仅 Release |
| Shared Debug | `out/sdk-clean-checkpoint/candidate1-20260919/pae-sdk-shared-debug` | 仅 Debug |
| Shared Release | `out/sdk-clean-checkpoint/candidate1-20260919/pae-sdk-shared-release` | 仅 Release |

进入所选目录后，先读包根 `PAE-SDK-README.md`，再从包根执行其中针对该形态和配置给出的
PowerShell 命令。构建目录应位于包外的新目录；不要回读开发仓库。
五包 `PROVENANCE.json` 均记录 `source_head=98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` 和
`source_worktree_dirty=false`。这些字段说明独立打包源树的身份，不能用来宣称当前共享工作树无文档变更。

## 2. 工具链与配置配对

当前验证组合固定为：

- 已验证工具链使用 CMake 4.3.1；工程声明最低 3.25 不表示 3.25 支持下面的 VS 2026 生成器，应使用实际支持该生成器的版本；
- `Visual Studio 18 2026`、x64；
- MSVC toolset `v142,version=14.29.30133`；
- Debug 使用 `/MDd`，Release 使用 `/MD`。

对应生成器参数为：

```powershell
-G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
```

Static/Shared 是单配置包，consumer 的 Debug/Release 必须与包名匹配。错配并非在 Configure
阶段强制失败；当前 CMake target 会把不匹配配置指向名为
`PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE` 的不存在输入，使其在链接期明确拒绝。
`PAE_MSVC_RUNTIME_LIBRARY` 只是包信息，尚未验证任意自定义配置、MSVC toolset 或 CRT 的自动检查；
也不承诺稳定或跨 toolset 的 C++ ABI。

## 3. 最小 CMake 集成

源码包 consumer 通过解压后的包根引入：

```cmake
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)

add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_features(my_pae_consumer PRIVATE cxx_std_17)
```

Configure 时令 `PAE_SOURCE_DIR` 指向 `pae-sdk-source` 包根。Static/Shared 包改为：

```cmake
find_package(PAE CONFIG REQUIRED)

add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_features(my_pae_consumer PRIVATE cxx_std_17)
```

Configure 时令 `CMAKE_PREFIX_PATH` 指向所选二进制包根。consumer 不应 include `src/**` 私有头，
也不应手工链接内部 PAE library。

Shared 包运行前需让 `pae.dll` 位于 consumer 可加载位置。包内示例已按 target 类型在构建后复制：

```cmake
get_target_property(pae_library_type PAE::pae TYPE)
if(pae_library_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(
        TARGET my_pae_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:PAE::pae>"
                "$<TARGET_FILE_DIR:my_pae_consumer>"
    )
endif()
```

不要从包中寻找或复制 Windows 系统 CRT DLL；使用与包匹配的已安装 MSVC Runtime。

## 4. 完整 Qt Lab 的 standalone 入口

本页上述 CMake 片段用于普通 PAE consumer，不代表完整 Qt Lab 构建。需要用所选 static 或 shared SDK
构建完整 Qt Lab 时，读 [`tools/protocol_lab_ui/standalone/README.md`](../../tools/protocol_lab_ui/standalone/README.md)。
`PrepareStandaloneInputs.ps1` 需要 `-RepositoryRoot`、`-DestinationRoot`、`-SdkCandidateRoot` 和
`-PackageKind static|shared`；Configure 需将 `PAE_SDK_ROOT` 指向单个安装包根，并显式配对
`PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED`。它还需要固定的 `PAE_QT_ROOT` 和
`PAE_LAB_DEPENDENCY_ROOT`；用 `PAE_LAB_BUILD_TESTING=OFF` 做产品闭包检查。准备后的 CMake 源目录为
`<DestinationRoot>/inputs/lab/tools/protocol_lab_ui/standalone`，Debug 和 Release 应使用分开的构建目录。

已验证的本地 Release 产物可直接从下列精确路径启动；普通本地使用优先选 static：

```powershell
& 'F:\PersonalWorkspace\pae-lab-clean-sdk-static-20260919\deploy\release-testing-off\Release\pae_protocol_lab_ui.exe'
```

static 配置根为
`F:\PersonalWorkspace\pae-lab-clean-sdk-static-20260919\deploy\release-testing-off\Release\configs`。
shared 对照 EXE 为
`F:\PersonalWorkspace\pae-lab-clean-sdk-shared-20260919\deploy\release-testing-off\Release\pae_protocol_lab_ui.exe`，
其配置根为同目录下的 `configs`，并必须连同已验证的 `pae.dll`、Qt DLL 和 `platforms\` 保持原目录结构。
两者均是本地验证产物，不是正式发布包也不替换旧部署。

## 5. 能力与证据边界

包内综合 consumer 覆盖当前公开 Compiler/metadata、Codec、StreamFramer、Host Decode，以及同一
Encode Handle 在调用期选择两个 Message。具体 Binary/ASCII 编解码支持域应以公开 API、当前契约
和配置能力查询为准，不能仅由 Schema 版本号推导“该版本全部可编解码”。PAE 不提供 Socket、串口、
线程、重试、设备生命周期、业务路由或 Lab/Qt UI。

当前证据关系如下：

- `candidate1-20260919` 的 Source/Static/Shared Debug/Release 六组仓库外 consumer 均完成构建与运行，见
  [clean-checkpoint SDK 验证](../engineering/pae-sdk-clean-checkpoint-validation.md)；
- 完整 Qt Lab 对同批 SDK 的 static/shared Debug/Release 已有本地仓库外闭包证据，见
  [Lab 同批 clean-checkpoint SDK 消费验证](../engineering/lab-clean-sdk-consumption-validation.md)；
- `candidate1-20260918` 是已验证的旧 dirty-provenance 候选，只作为
  [0.11 静态 Framing 查询 SDK 验证](../engineering/pae-public-stream-sdk-validation.md)、
  [Qt Lab static 验证](../engineering/lab-sdk-standalone-validation.md) 和
  [Qt Lab shared 验证](../engineering/lab-sdk-standalone-shared-validation.md) 的历史身份；
- Stage 3 `final5`/`final6` 是更早的历史候选身份，不是本页第 1 节的当前路径。
- 历史 `final5` 完成 Source/Static/Shared 的 Debug/Release 六组正向构建与运行，并完成四组二进制包
  Debug/Release 错配负向验证；
- 历史 `final6` 只规范化三份源码示例 README 的路径表达及相应包元数据；总控复核的 166 个功能输入与
  `final5` 一致，没有在 `final6` 重新运行上述功能验证；
- Debug/Release 公开 API 定向回归各 7/7 是此前既有证据，不是 `final6` 重跑结果。

这些结论只覆盖指定 Windows x64 工具链、`98df5e0` clean-checkpoint 本地候选与同批 Lab 消费。尚无
统一正式分发、稳定 ABI、任意自定义配置/toolset/CRT、Linux、真实协议、硬件、现场或生产证据。完整 Qt Lab standalone
闭包也不等于新人工 UI 验收。仓库尚无 PAE 项目级对外分发
许可证；这不阻断本地技术复核，但阻断正式对外发布。yyjson 的 MIT License 只适用于 yyjson，
不能替代 PAE 自身授权。
