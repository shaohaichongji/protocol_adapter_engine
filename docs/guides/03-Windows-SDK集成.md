# 03 Windows SDK 集成

## 适用读者

需要在 Windows x64 C++17 工程中消费 PAE Source、Static 或 Shared SDK 的开发者。

## 前置条件

- 已理解 [01 项目定位与能力边界](01-项目定位与能力边界.md)；
- 使用 Visual Studio 18 2026 x64 与已验证的 MSVC `v142,version=14.29.30133`；
- 明确 consumer 要使用 Debug 还是 Release；
- 已在仓库根目录打开终端；当前本机候选归集在 `deliverables/sdk/7b4205e`。该目录是本地产物、不随 clone 获取，也不是正式发布；不存在时应按 [06 构建测试与问题定位](06-构建测试与问题定位.md) 的 SDK 路线重新生成。统一导航见 [交付入口](../../deliverables/README.md)。

## 读完能做什么

读完后可以正确选择 SDK 形态，使用 `PAE_SOURCE_DIR` 或 `CMAKE_PREFIX_PATH` 接入 `PAE::pae`，并避开 Debug/Release、Static/Shared 和 DLL 部署错配。

## 1. 选择包

| 形态 | 路径 | 可用配置 |
| --- | --- | --- |
| Source | `deliverables/sdk/7b4205e/pae-sdk-source` | Debug、Release |
| Static Debug | `deliverables/sdk/7b4205e/pae-sdk-static-debug` | Debug |
| Static Release | `deliverables/sdk/7b4205e/pae-sdk-static-release` | Release |
| Shared Debug | `deliverables/sdk/7b4205e/pae-sdk-shared-debug` | Debug |
| Shared Release | `deliverables/sdk/7b4205e/pae-sdk-shared-release` | Release |

进入包后先读 `PAE-SDK-README.md`。五包 `PROVENANCE.json` 记录相同 `source_head=7b4205ea899cf16b9c73ba6ebc4c64382b71dc63` 与 `source_worktree_dirty=false`；这只说明独立打包源树身份，不随仓库后续提交改变。旧 `deliverables/sdk/98df5e0` 保留作历史对照。

## 2. Source 包接入

```cmake
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(PAE_BUILD_PUBLIC_API_STAGE1 ON CACHE BOOL "" FORCE)
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)

add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_features(my_pae_consumer PRIVATE cxx_std_17)
```

Configure 时令 `PAE_SOURCE_DIR` 指向 Source 包根。Source 包是白名单源码包，不是整个开发仓库；consumer 不应回读本仓库的 `src/**`。

## 3. Static/Shared 包接入

```cmake
find_package(PAE CONFIG REQUIRED)

add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_features(my_pae_consumer PRIVATE cxx_std_17)
```

Configure 时让 `CMAKE_PREFIX_PATH` 指向一个精确二进制包根。不要手工拼接内部 `.lib`，也不要 include 私有头。

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

## 4. 配置与二进制必须配对

| 包 | consumer 配置 | CRT |
| --- | --- | --- |
| `*-debug` | Debug | `/MDd` |
| `*-release` | Release | `/MD` |

当前错配门禁可能在链接阶段才失败，并出现 `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_*_PACKAGE` 名称。它是有意拒绝，不应通过复制另一配置的库来绕过。`PAE_MSVC_RUNTIME_LIBRARY` 是包信息，不代表任意自定义配置、toolset 或 CRT 已自动验证。

## 5. 最小公开调用链

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
5. 根据输入形态创建 Codec、Framer 或 Host；
6. 在同步 callback 内复制需要长期保留的 borrowed view。

完整示例：

- `examples/public_api_codec/main.cpp`：完整记录 Codec；
- `examples/public_api_framer/main.cpp`：Framer callback + Codec；
- `examples/public_api_host/main.cpp`：Host binding/handle/callback；
- `examples/public_api_sdk_consumer/main.cpp`：安装包综合消费。

## 6. installed SDK 构建完整 Qt Lab

普通 consumer 与完整 Qt Lab 是两条路线。Qt Lab 必须从 [`tools/protocol_lab_ui/standalone/README.md`](../../tools/protocol_lab_ui/standalone/README.md) 进入：

- `PrepareStandaloneInputs.ps1` 显式接收仓库、目标、SDK 候选和 `static|shared`；
- `PAE_SDK_ROOT` 指向单个安装包根；
- `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED` 与包匹配；
- `PAE_QT_ROOT`、`PAE_LAB_DEPENDENCY_ROOT` 指向固定输入；
- Debug/Release 使用不同构建目录；产品闭包检查使用 `PAE_LAB_BUILD_TESTING=OFF`。

## 7. 当前证据边界

当前候选已有 Windows x64 的 Source/Static/Shared Debug/Release 包外消费证据，但不是正式发布。shared 仍有 C4251，不承诺稳定 ABI；Linux、真实协议、硬件、现场和生产未由此验证。包内 README 与 manifest/provenance 是具体包的首要依据。

下一篇：配置编写见 [04 协议配置入门](04-协议配置入门.md)；失败定位见 [06 构建测试与问题定位](06-构建测试与问题定位.md)。
