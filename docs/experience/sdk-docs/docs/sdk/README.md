# PAE Windows SDK 中文首次运行指南

> 文档交付版投影：以固定产品提交 `9680cf90512f053962949cb55467d6af35959088` 的源码和示例为事实，
> 本文件为后续文档整理，包含公共头 UTF-8 接入说明，不是该提交的原始字节。
> 派生 identity/hash 由包内 provenance 的 `documentation_overlay` 独立记录。
## 1. 定位和边界

PAE 把协议规则编译为冻结 Plan，再由公开 Codec、StreamFramer 或 Host 接口消费。它不默认管理
Socket、串口、设备线程、重试、路由或业务状态。包内示例使用从零设计的公开合成配置，不代表真实
协议、设备、现场或生产验收。

本 SDK 是本地评审产物。Source、Static/Shared Debug/Release 的实际可用组合以所选包清单为准；
同基线体验目录计划包含五包，产品源码固定为 `9680cf90512f053962949cb55467d6af35959088`。
文档准备不表示这五包及 Lab 已归集或验证。先读包根 `PROVENANCE.json`，确认 package kind、
Debug/Release、x64、MSVC toolset、CRT、来源提交及 dirty 状态；再读 `PAE-SDK-README.md` 的许可和
ABI 边界。不要混用 Debug consumer 与 Release 二进制包，也不要把同工具链验证扩大为稳定 ABI。

## 2. 选择 source、static 或 shared

| 包 | 适合场景 | 配置入口 | 运行时特点 |
| --- | --- | --- | --- |
| source | 需要从包内源码构建 PAE | `PAE_SOURCE_DIR=$PackageRoot` | 构建时间更长；不应回读开发仓库 |
| static | 希望链接安装包中的静态库 | `CMAKE_PREFIX_PATH=$PackageRoot` | consumer 相邻目录没有 `pae.dll` |
| shared | 希望链接安装包中的 DLL | `CMAKE_PREFIX_PATH=$PackageRoot` | 示例规则复制包内 `pae.dll` 到程序旁 |

下文示范 `Visual Studio 18 2026 / x64 / v142 14.29.30133`；实际工具链版本须结合包
provenance 和同批构建记录核对，通用 `MSVC x64` 字段不包含完整 toolset 证明。带 YAML 的二进制包额外提供静态 `PAE::yaml_frontend`；JSON-only 包不提供该组件。
shared 和 Debug 是否在某一批包中实际验证，以该批独立验证记录为准，不能由包名推断。

## 3. 先运行最小程序

从 SDK 包根执行：

```powershell
$PackageRoot = (Resolve-Path .).Path
$Configuration = 'Release'
$PackageName = Split-Path $PackageRoot -Leaf
$BuildParent = [IO.Path]::GetTempPath()  # 包外目录
$BuildRoot = Join-Path $BuildParent "pae-getting-started-$PackageName-$Configuration"
if (Test-Path -LiteralPath $BuildRoot) { throw 'Choose a fresh BuildRoot' }
$Config = if (Test-Path -LiteralPath "$PackageRoot\share\pae\examples\config\synthetic_stream_framing_slice.pae.json") {
  "$PackageRoot\share\pae\examples\config\synthetic_stream_framing_slice.pae.json"
} else {
  "$PackageRoot\examples\config\synthetic_stream_framing_slice.pae.json"
}
```

source 包：

```powershell
cmake -S "$PackageRoot\examples\getting_started" -B $BuildRoot `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DPAE_SOURCE_DIR=$PackageRoot"
```

static/shared 安装包：

```powershell
cmake -S "$PackageRoot\examples\getting_started" -B $BuildRoot `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
```

二选一配置后，使用与包匹配的 Configuration；以上取 Release 包。这里是**独立运行包内示例**，不是把示例 CMake 作为已有宿主子目录：

```powershell
cmake --build $BuildRoot --config $Configuration --target pae_getting_started -- /m:1
& "$BuildRoot\$Configuration\pae_getting_started.exe" $Config
if ($LASTEXITCODE -ne 0) { throw 'PAE getting_started failed' }
```

预期最后一行是 `GETTING_STARTED_BINARY_PASS`。程序用手写字节 `AA 00 07`：配置中 `AA` 是
`fixed_message` 的固定 Matcher，`00 07` 是 `fixed_payload` 的双字节大端 `UINT64`，逻辑值为 7。
程序先 Decode，再以类型化 7 Encode，并对同一独立字节预期做检查。

需要受限 YAML 作者源时，选带 YAML 组件的包，用**另一个**新 BuildRoot 运行包内
`examples/yaml_sdk_consumer`，不要复用上方 JSON 的 CMake cache：

```powershell
$YamlBuildRoot = Join-Path $BuildParent "pae-yaml-$PackageName-$Configuration"
if (Test-Path -LiteralPath $YamlBuildRoot) { throw 'Choose a fresh BuildRoot' }
$YamlConfig = "$PackageRoot\examples\yaml_sdk_consumer\synthetic_fixed_message.pae.yaml"
```

source 包配置：

```powershell
cmake -S "$PackageRoot\examples\yaml_sdk_consumer" -B $YamlBuildRoot `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DPAE_SOURCE_DIR=$PackageRoot"
```

static/shared 安装包配置：

```powershell
cmake -S "$PackageRoot\examples\yaml_sdk_consumer" -B $YamlBuildRoot `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
```

二选一配置后执行：

```powershell
cmake --build $YamlBuildRoot --config $Configuration --target pae_yaml_sdk_consumer -- /m:1
& "$YamlBuildRoot\$Configuration\pae_yaml_sdk_consumer.exe" $YamlConfig
if ($LASTEXITCODE -ne 0) { throw 'PAE YAML SDK consumer failed' }
```

成功标志为 `PAE_YAML_SDK_CONSUMER_PASS`：YAML 转成严格 JSON 后仍走公开 Compiler/Codec。
JSON-only 包请求 YAML 组件会在 Configure 阶段拒绝；不要把这项预期拒绝当作运行失败。

已有自身 CTest 的宿主接入 Source 包时，直接受控加入包根；不要把包内示例 CMake 当成自己的宿主入口。示例的目录作用域选项服务于独立演示，宿主仍管理自身 BUILD_TESTING。以下是最小接入形状：

```cmake
include(CTest)  # 宿主管理自己的 BUILD_TESTING
set(PAE_BUILD_PUBLIC_API_STAGE1 ON)
set(PAE_BUILD_TESTING OFF)
set(PAE_BUILD_YAML_FRONTEND OFF)  # 需要 YAML 时按包内能力设 ON
add_subdirectory("${PAE_SOURCE_DIR}" pae-source)
add_executable(my_pae_consumer main.cpp)
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
```

目标源码 `main.cpp` 由宿主提供；需要 YAML 时按所选包能力开启前端并链接 `PAE::yaml_frontend`。这个片段不表示本轮对宿主 CTest 做了验证；应另查 Cache、测试注册及实际运行。

## 4. 再运行综合 consumer

### 公共头的源码编码

当前源码及由其新构建/安装的 SDK 公共头使用无 BOM 的 UTF-8。CMake 消费目标链接
`PAE::pae` 时，仅向 MSVC C++ 编译传递 `/source-charset:utf-8`，覆盖源码读取，不额外设置
execution charset，也不传播 PAE 内部警告选项。旧体验包未因此自动更新，须按各包来源判断。
MSVC v142 不允许同时传入 `/utf-8` 与 `/source-charset:utf-8`。宿主已有 `/utf-8` 时，须在
实际编译的消费目标上关闭 PAE 自动源码选项（源码和新安装 SDK 使用相同机制）：

```cmake
target_link_libraries(my_pae_consumer PRIVATE PAE::pae)
target_compile_options(my_pae_consumer PRIVATE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/utf-8>")
set_property(TARGET my_pae_consumer PROPERTY PAE_MSVC_SOURCE_CHARSET_SUPPLIED ON)
```

此属性是宿主“已提供 UTF-8 源码读取设置”的明确声明，不会自行添加编码选项。
应设置在每个实际编译目标，而不是 `PAE::pae` 或中间 INTERFACE 库；未提供等价源码设置就
关闭自动选项可能再次导致公共头编码错误。PAE 不猜测全局 flags、环境或其他目标中的选项。
默认消费无需设置该属性，继续只接收 source charset；不会强制宿主窄字符串采用 UTF-8。
仓库自身的 `pae::project_options` 改用等价的 `/source-charset:utf-8` 与
`/execution-charset:utf-8` 分列，保持内部源码及窄字符串均为 UTF-8，可与公共默认选项组合。
该选项作用于消费目标的整个 C++ 编译单元，不仅是 PAE 头；宿主自身源码也需使用 UTF-8，
不能在同一编译单元中把 GBK 源码与 UTF-8 公共头混作同一源码编码。
非 CMake 手工接入须在消费公共头的编译单元显式设置 MSVC `/source-charset:utf-8`，或使用
工具链对应的 UTF-8 源码读取设置；只有宿主自己也需要 UTF-8 窄字符串编码时才选择 `/utf-8`。
本次仅验证 MSVC v142，不能据此声明 clang-cl、Linux 或跨工具链 ABI 已验证。

`examples/sdk_consumer` 是包的综合可重复验证入口，覆盖 Compiler/metadata、Binary/ASCII Codec、
StreamFramer、Host 和多 Message Encode；它不是第一个程序的教学替代品。

source 包的配置位于 `examples/config/`，配置时使用 `PAE_SOURCE_DIR=$PackageRoot`。安装包配置位于
`share/pae/examples/config/`，配置时使用 `CMAKE_PREFIX_PATH=$PackageRoot`。具体完整命令见包根
`PAE-SDK-README.md`。成功标志为 `PAE_SDK_STAGE3_CONSUMER_PASS`。

## 5. 常见错误定位

- `Could not find PAE`：安装包检查 `CMAKE_PREFIX_PATH` 是否等于包根；source 包不要使用
  `find_package` 路径，改用 `PAE_SOURCE_DIR`。
- 切包后仍导入旧 Static/Shared 目标：检查 CMake cache 的 `PAE_DIR`；仅改 `CMAKE_PREFIX_PATH`
  不会清除旧值。不同包、Configuration 和示例首选不同的新 BuildRoot，再检查目标类型。
- `PAE_SDK_CONFIGURATION_MISMATCH`：consumer Configuration 与二进制包 Debug/Release 不一致。
- 配置文件打不开：始终从 `$PackageRoot` 形成绝对 `$Config`，不要依赖开发仓库当前目录。
- Compile 失败：读取 `CompileDiagnostic` 的 stage、code、json pointer；不要继续创建 Codec。
- Codec 失败：仅 `status==OK` 的 Decode record 或 Encode `bytes_written` 可交付；失败 buffer 不可发送。
- shared 程序缺少 DLL：确认 `pae.dll` 来自同一包且位于程序相邻目录，不复制系统 CRT DLL。
- 原生 VS/qmake 手工链接：当前 Static `PAE::pae` 还传递五个内部 `.lib` 和静态宏；
  一个 `pae.lib` 不是已验证闭包，优先使用 CMake target。非 CMake 链接未由当前实验验证。

## 6. 生命周期与线程边界

- 完整记录直接用 Codec；分块流先由 Framer 形成候选，再按需 Decode；需要端点/方向绑定和通道回调时才用 Host。Framer 候选不等于协议或业务成功。
- metadata 中的字符串 view 借用 `CompiledProtocol`；owner 移动、替换或销毁后重新查询或提前复制。Codec/Framer/Host 各自持有冻结执行状态，但不延长这些旧描述 view 的寿命。
- Decode record/field view 借用 Codec，并会在下一次受保护的 Decode/Encode、Codec 移动或销毁后
  失效；BYTES 还借用未修改的输入字节。
- Framer/Host callback 的候选或输出字节只在同步回调内借用；跨回调保存必须复制。
- Decode 失败不交付 record；Encode 失败即使修改了 Buffer 也不能发送。Host 只有交付状态 `OK` 才表示业务回调正常返回；成功 Reset 后旧 handle 失效，须重新 `Find`。
- 同一执行实例应串行使用，不在回调中重入；`WORKSPACE_BUSY` / `REENTRANT_CALL` 不是自动重试承诺。PAE 不替调用者管理通信线程。

## 7. 文件真实性与许可

`MANIFEST.txt` 记录文件和长度，`SHA256SUMS.txt` 记录包内文件 Hash，`PROVENANCE.json` 记录来源。
这些信息用于内部一致性与追溯，不提供签名、来源认证或防篡改保证。仓库当前没有项目级 PAE 许可；
`LICENSES/yyjson-LICENSE.txt` 只适用于 yyjson；启用 YAML 包内的 rapidyaml 通知只适用于
该依赖。二者都不代表 PAE 获准外部分发。

## 8. 可选 YAML 作者源组件（本地试用候选）

开启 `PAE_BUILD_YAML_FRONTEND` 的本地安装树可另外提供 `PAE::yaml_frontend` 静态
附加目标及 `<pae/yaml_frontend.h>`；默认 OFF/JSON-only 安装树不携带该头、目标、示例
和 rapidyaml 许可目录。`scripts/package_sdk_stage3.ps1` 的 source 包白名单现包含
可选 YAML 源码、公开头、CMake、固定 rapidyaml 原件与通知、Profile 和独立合成
consumer；source 包中该组件仍默认 OFF，不要求 JSON-only 消费者编译它。binary
包从已安装树识别 ON/OFF，ON 时必须具有附加库、头、Profile、许可和示例，缺文件或
混合状态拒绝；OFF 包仍可独立消费 JSON，显式请求 YAML 组件则在 `find_package`
阶段失败。shared PAE 包是 `pae.dll` 加静态 YAML 库，没有 YAML DLL。

本地打包候选的 `MANIFEST.txt`、`SHA256SUMS.txt` 与 `PROVENANCE.json` 可以由
`scripts/verify_yaml_sdk_package.ps1` 按实际包内容复核。复制既有安装树制包时，
元数据记录当前 HEAD/dirty 和既有安装树字节这一事实；当前 Git 状态本身不能证明
既有二进制由该提交构建。固定源码、构建/安装库的字节对照和同批验证记录共同建立来源链；
本指南不把历史工程报告当成当前包的新验证证据。
这些仍是本地试用候选，
不是正式发布、对外分发许可或稳定 ABI 承诺。

包内 `examples/yaml_sdk_consumer` 展示显式消费：`find_package(PAE CONFIG REQUIRED
COMPONENTS yaml_frontend)`，链接 `PAE::yaml_frontend` 与 `PAE::pae`；先把受限 YAML
转换为自有严格 JSON，再调用既有 `CompileProtocolJson` 和 Codec。前端不解释协议
Schema，不自动替换 JSON 编译入口；转换失败没有可用的 JSON 或来源映射。生成 JSON
的 view 借用 move-only 转换结果 owner，移动、赋值或销毁后须重新获取。来源查询返回
值类型的 YAML 行列；最近祖先回退有独立标志，生成 JSON offset 不能当成 YAML offset。

`TrialResourceLimitsV01()` 只读给出当前**试用组件资源约束 V0.1**：输入 16 KiB、
Parser/辅助各 128 KiB、JSON 32 KiB、512 节点、16 层、4 KiB 标量。它们是拒绝边界，
不是生产容量或全进程 RSS 上界；公开结果 owner 另有一次分配，失败返回
`ALLOCATION_FAILED`。公开接口不提供故障注入或任意预算调参。转换库的当前 C++ ABI
仅按同包 x64/MSVC/CRT 配对验证，不能跨编译器或混用 Debug/Release；项目级许可与
对外分发仍待另行审查。rapidyaml 相关通知仅随启用组件的安装树提供，不能替代
PAE 项目的分发许可。
