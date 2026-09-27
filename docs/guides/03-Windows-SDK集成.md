# 03 Windows SDK 集成

## 适用读者

需要在 Windows x64 C++17 工程中消费 PAE Source、Static 或 Shared SDK 的开发者。

## 前置条件

- 已理解 [01 项目定位与能力边界](01-项目定位与能力边界.md)；
- 使用 Visual Studio 18 2026 x64 与已验证的 MSVC `v142,version=14.29.30133`；
- 明确 consumer 要使用 Debug 还是 Release；
- 已在仓库根目录打开终端；当前同基线候选位于 `deliverables/sdk/b12ad80/`。本地产物不随 clone 获取，也不是正式发布；不存在时按 [06 构建测试与问题定位](06-构建测试与问题定位.md) 的 SDK 路线重新生成。统一导航见 [交付入口](../../deliverables/README.md)。

## 读完能做什么

读完后可以正确选择 JSON-only 或受限 YAML 候选，使用 `PAE_SOURCE_DIR` 或 `CMAKE_PREFIX_PATH` 接入 `PAE::pae`，并避开 Debug/Release、Static/Shared 和 DLL 部署错配。

## 1. 选一个当前包

首次集成选择 `deliverables/sdk/b12ad80/pae-sdk-static-release/`。下表路径相对于仓库根；同一个 consumer 只选一个精确包根，Debug/Release 与所选包配对：

| 形态 | 当前包根 | YAML 作者源 |
| --- | --- | --- |
| Source | `deliverables/sdk/b12ad80/pae-sdk-source/` | 源码可选，默认 OFF |
| Static Debug / Release | `deliverables/sdk/b12ad80/pae-sdk-static-{debug,release}/` | 含静态附加组件 |
| Shared Debug / Release | `deliverables/sdk/b12ad80/pae-sdk-shared-{debug,release}/` | 含静态附加组件；运行时需同包 `pae.dll` |
| JSON-only Static / Shared Release | `deliverables/sdk/b12ad80/pae-sdk-json-only-{static,shared}-release/` | 不提供 YAML 组件 |

七包由干净固定提交 `b12ad80` 构建，包内 `PROVENANCE.json` 与哈希保持原样。[SDK 构建验证](../engineering/yaml-clean-sdk-validation-20260926.md)记录 Source 和六个二进制包的消费，[归集验证](../engineering/yaml-clean-delivery-validation-20260926.md)记录本机新位置一致性。它们都是 Windows x64 本地候选。

JSON 继续直接调用 `pae::CompileProtocolJson()`。选择 YAML 作者源时，先按 [Profile V0.1](../../schema/pae_yaml_profile_v0.1.md) 限定语法转换成严格 JSON，再交给同一编译器；YAML 不增加新的协议执行语义。包内 `examples/yaml_sdk_consumer/` 是最小完整示例：

```cmake
find_package(PAE CONFIG REQUIRED COMPONENTS yaml_frontend)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae PAE::yaml_frontend)
```

上例用于带 YAML 组件的二进制包，`CMAKE_PREFIX_PATH` 必须指向选定包根。`PAE::yaml_frontend` 是静态附加库；Shared PAE 仍须把同包 `pae.dll` 放在程序可加载目录，没有单独 YAML DLL。JSON-only 包请求该组件会在 Configure 阶段失败。Source 包含可选源码但默认关闭，使用 `PAE_SOURCE_DIR` 接入时需显式设置 `PAE_BUILD_YAML_FRONTEND=ON`；详见包内示例 CMake。Debug/Release、`v142,version=14.29.30133` 与 CRT 配置应与对应候选匹配。本地同工具链验证不等于跨工具链稳定 ABI 或生产容量保证。

## 2. 从包内示例走通 JSON 或 YAML

在 Visual Studio Developer Shell 中，从仓库根目录选择当前 Static Release 包。每次换包、切 Debug/Release 或更换示例，都使用新的 `$BuildRoot`，不复用已有 CMake cache。已复现：同根仅改 `CMAKE_PREFIX_PATH` 时，缓存的 `PAE_DIR` 仍可能指向旧包；新目录是首选，排查旧目录时同时检查 `PAE_DIR` 与 `PAE::pae` 的 Static/Shared 类型：

```powershell
$PackageRoot = (Resolve-Path 'deliverables/sdk/b12ad80/pae-sdk-static-release').Path
$BuildRoot = Join-Path (Get-Location).Path 'out/build/first-use-b12ad80-static-release-json'
if (Test-Path -LiteralPath $BuildRoot) { throw 'Choose a fresh BuildRoot' }
cmake -S "$PackageRoot\examples\getting_started" -B $BuildRoot `
  -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
cmake --build $BuildRoot --config Release --target pae_getting_started
& "$BuildRoot\Release\pae_getting_started.exe" `
  "$PackageRoot\share\pae\examples\config\synthetic_stream_framing_slice.pae.json"
```

预期 `GETTING_STARTED_BINARY_PASS`，表示手写 `AA 00 07` 的 Decode 与 Encode 断言通过。要试 YAML，保持同一包，但换一个新目录和包内另一个示例：

```powershell
$BuildRoot = Join-Path (Get-Location).Path 'out/build/first-use-b12ad80-static-release-yaml'
if (Test-Path -LiteralPath $BuildRoot) { throw 'Choose a fresh BuildRoot' }
cmake -S "$PackageRoot\examples\yaml_sdk_consumer" -B $BuildRoot `
  -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
cmake --build $BuildRoot --config Release --target pae_yaml_sdk_consumer
& "$BuildRoot\Release\pae_yaml_sdk_consumer.exe" `
  "$PackageRoot\examples\yaml_sdk_consumer\synthetic_fixed_message.pae.yaml"
```

预期 `PAE_YAML_SDK_CONSUMER_PASS`；示例先将 YAML 转成严格 JSON，再使用公开 Compiler/Codec。命令列出的是现有包的使用方式，本轮文档打磨没有重新执行它们。Source 包中的配置与代码在 `examples/` 内；二进制包也有示例源码，但安装配置通常位于 `share/pae/examples/config/`。宿主不要回读开发仓库的私有 `src/**`。

## 3. Source 包接入

进入包后先读 `PAE-SDK-README.md` 与 `PROVENANCE.json`。本节区分两种用途：上节命令是**独立运行 b12ad80 包内旧示例**；已有自身 CTest 的宿主不要把该示例 CMake 当作子目录嵌入，因为它使用 `FORCE` 改写全局 `BUILD_TESTING`，隔离实验中宿主测试从 ON 变成 OFF、`ctest -N` 只见 0 项。宿主可直接受控加入 Source 包：

```cmake
include(CTest)  # 已有宿主按自身规则管理 BUILD_TESTING
set(PAE_BUILD_PUBLIC_API_STAGE1 ON)
set(PAE_BUILD_TESTING OFF)
set(PAE_BUILD_YAML_FRONTEND OFF)  # 确需 YAML 时设 ON
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)

add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_features(my_pae_consumer PRIVATE cxx_std_17)
```

Configure 时令 `PAE_SOURCE_DIR` 指向 Source 包根。普通目录变量约束 PAE 选项，不用 `FORCE BUILD_TESTING` 改写宿主 Cache；B 片在独立最小宿主中验证自有测试 1/1，详见 [接入审计](../engineering/developer-integration-audit-20260926.md)。仓库两个入门示例已采用局部选项，JSON/YAML 嵌入宿主 Release 各 2/2 通过，见 [示例修复验证](../engineering/developer-integration-example-fix-validation-20260926.md)；现有 `b12ad80` 包内旧示例未更新，不混用这一结论。Source 包是白名单源码包，不是整个开发仓库；consumer 不应回读本仓库的 `src/**`。

## 4. Static/Shared 包接入

```cmake
find_package(PAE CONFIG REQUIRED)

add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_features(my_pae_consumer PRIVATE cxx_std_17)
```

Configure 时让 `CMAKE_PREFIX_PATH` 指向一个精确二进制包根，并使用新 BuildRoot；它不会自动清除旧缓存 `PAE_DIR`。当前 Static `PAE::pae` 会传递五个内部 `.lib` 及静态编译宏，不是手工链接单个 `pae.lib` 的已验证闭包；优先消费 CMake target，不手工拼库或 include 私有头。原生 VS/qmake 手工链接尚无本轮验证承诺。

Shared consumer 还需把同包 `pae.dll` 放到 EXE 可加载目录。推荐由 target 自动复制：

```cmake
get_target_property(pae_library_type PAE::pae TYPE)
if(pae_library_type STREQUAL "SHARED_LIBRARY")
  add_custom_command(TARGET my_pae_consumer POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:PAE::pae>"
            "$<TARGET_FILE_DIR:my_pae_consumer>")
endif()
```

不要从 SDK 包复制 Windows 系统 CRT DLL；使用与候选匹配的已安装 MSVC Runtime。

## 5. 配置与二进制必须配对

| 包 | consumer 配置 | CRT |
| --- | --- | --- |
| `*-debug` | Debug | `/MDd` |
| `*-release` | Release | `/MD` |

当前错配门禁可能在链接阶段才失败，并出现 `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE` 名称。它是有意拒绝，不应通过复制另一配置的库来绕过。`PAE_MSVC_RUNTIME_LIBRARY` 是包信息，不代表任意自定义配置、toolset 或 CRT 已自动验证。

## 6. 最小公开调用链

consumer 只需要公开头：

```cpp
#include <pae/codec.h>
#include <pae/compiler.h>
```

基本顺序是：

1. 读取完整 JSON 字节，调用 `pae::CompileProtocolJson()`；
2. 失败时记录 `CompileDiagnostic.stage/code/json_pointer/byte_offset/detail`；
3. 成功后移动取得 `CompiledProtocol`；
4. 查询 Pipeline/Message/字段事实，不从字符串或私有 Plan 猜测；
5. 完整记录用 Codec；分块流先用 Framer 形成候选，再按需交 Codec；只有需要端点/方向绑定及通道回调时才用 Host。创建结果与输出容量都须检查，候选不等于 Decode 成功；
6. `CompiledProtocol` 描述的 `string_view` 在 owner 移动、替换或销毁后重查；Codec 的 record/field view 在下一次受保护操作等事件后失效，BYTES 还借用未修改的输入。执行实例自身持有冻结状态，不等于这些描述 view 会延寿；
7. Framer/Host 的候选和输出仅在同步回调中借用，跨回调先复制。同一实例串行调用，不在回调内重入；
8. 先判状态再使用输出：Decode 失败不交付 record，Encode 失败的 Buffer 即使改写也不可发送；Host 成功 Reset 后旧 handle 失效，须重新 `Find`。Host 的 Codec 尝试事实不等于业务交付成功。

完整示例：

- `examples/public_api_codec/main.cpp`：完整记录 Codec；
- `examples/public_api_framer/main.cpp`：Framer callback + Codec；
- `examples/public_api_host/main.cpp`：Host binding/handle/callback；
- `examples/public_api_sdk_consumer/main.cpp`：安装包综合消费。

## 7. installed SDK 构建完整 Qt Lab

普通 consumer 与完整 Qt Lab 是两条路线。Qt Lab 必须从 [`tools/protocol_lab_ui/standalone/README.md`](../../tools/protocol_lab_ui/standalone/README.md) 进入：

- `PrepareStandaloneInputs.ps1` 显式接收仓库、目标、SDK 候选和 `static|shared`；
- `PAE_SDK_ROOT` 指向单个安装包根；
- `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED` 与包匹配；
- `PAE_QT_ROOT`、`PAE_LAB_DEPENDENCY_ROOT` 指向固定输入；
- Debug/Release 使用不同构建目录；产品闭包检查使用 `PAE_LAB_BUILD_TESTING=OFF`。

## 8. 当前证据与历史身份

`b12ad80` 的包外消费范围见上方专项验证，用户对同版 static Release Lab 的 JSON/YAML 正常解析反馈见 [人工记录](../engineering/yaml-clean-delivery-validation-20260926.md)。此前 `deliverables/sdk/7b4205e/` 五包与 `out/build/yaml-sdk-packaging-20260926/` dirty 包保留旧身份，不进入当前命令；需要对照时看 [交付入口](../../deliverables/README.md)。所有批次均不是正式发布。shared 仍有 C4251，不承诺稳定 ABI；Linux、真实协议、硬件、现场和生产未由此验证。包内 README 与 manifest/provenance 是具体包的首要依据。

下一篇：配置编写见 [04 协议配置入门](04-协议配置入门.md)；失败定位见 [06 构建测试与问题定位](06-构建测试与问题定位.md)。
