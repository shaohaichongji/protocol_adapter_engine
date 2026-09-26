# PAE Windows SDK 中文首次运行指南

## 1. 定位和边界

PAE 把协议规则编译为冻结 Plan，再由公开 Codec、StreamFramer 或 Host 接口消费。它不默认管理
Socket、串口、设备线程、重试、路由或业务状态。包内示例使用从零设计的公开合成配置，不代表真实
协议、设备、现场或生产验收。

本 SDK 是本地评审产物，不是正式发布。先读包根 `PROVENANCE.json`，确认 package kind、
Debug/Release、x64、MSVC toolset、CRT、来源提交及 dirty 状态；再读 `PAE-SDK-README.md` 的许可和
ABI 边界。不要混用 Debug consumer 与 Release 二进制包，也不要把同工具链验证扩大为稳定 ABI。

## 2. 选择 source、static 或 shared

| 包 | 适合场景 | 配置入口 | 运行时特点 |
| --- | --- | --- | --- |
| source | 需要从包内源码构建 PAE | `PAE_SOURCE_DIR=$PackageRoot` | 构建时间更长；不应回读开发仓库 |
| static | 希望链接安装包中的静态库 | `CMAKE_PREFIX_PATH=$PackageRoot` | consumer 相邻目录没有 `pae.dll` |
| shared | 希望链接安装包中的 DLL | `CMAKE_PREFIX_PATH=$PackageRoot` | 示例规则复制包内 `pae.dll` 到程序旁 |

本指南的命令以包 provenance 所列 `Visual Studio 18 2026 / x64 / v142 14.29.30133` 为当前工具链
边界。shared 和 Debug 的包装规则存在，但是否在某一批包中实际验证，必须以该包随附证据为准。

## 3. 先运行最小程序

从 SDK 包根执行：

```powershell
$PackageRoot = (Resolve-Path .).Path
$BuildRoot = Join-Path (Split-Path $PackageRoot -Parent) 'pae-getting-started-build'
$Config = "$PackageRoot\examples\config\synthetic_stream_framing_slice.pae.json"
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

二选一配置后，使用与包匹配的 Configuration。以下以 Release 为例：

```powershell
cmake --build $BuildRoot --config Release --target pae_getting_started -- /m:1
& "$BuildRoot\Release\pae_getting_started.exe" $Config
if ($LASTEXITCODE -ne 0) { throw 'PAE getting_started failed' }
```

预期最后一行是 `GETTING_STARTED_BINARY_PASS`。程序用手写字节 `AA 00 07`：配置中 `AA` 是
`fixed_message` 的固定 Matcher，`00 07` 是 `fixed_payload` 的双字节大端 `UINT64`，逻辑值为 7。
程序先 Decode，再以类型化 7 Encode，并对同一独立字节预期做检查。

## 4. 再运行综合 consumer

`examples/sdk_consumer` 是包的综合可重复验证入口，覆盖 Compiler/metadata、Binary/ASCII Codec、
StreamFramer、Host 和多 Message Encode；它不是第一个程序的教学替代品。

source 包的配置位于 `examples/config/`，配置时使用 `PAE_SOURCE_DIR=$PackageRoot`。安装包配置位于
`share/pae/examples/config/`，配置时使用 `CMAKE_PREFIX_PATH=$PackageRoot`。具体完整命令见包根
`PAE-SDK-README.md`。成功标志为 `PAE_SDK_STAGE3_CONSUMER_PASS`。

## 5. 常见错误定位

- `Could not find PAE`：安装包检查 `CMAKE_PREFIX_PATH` 是否等于包根；source 包不要使用
  `find_package` 路径，改用 `PAE_SOURCE_DIR`。
- `PAE_SDK_CONFIGURATION_MISMATCH`：consumer Configuration 与二进制包 Debug/Release 不一致。
- 配置文件打不开：始终从 `$PackageRoot` 形成绝对 `$Config`，不要依赖开发仓库当前目录。
- Compile 失败：读取 `CompileDiagnostic` 的 stage、code、json pointer；不要继续创建 Codec。
- Codec 失败：仅 `status==OK` 的 Decode record 或 Encode `bytes_written` 可交付；失败 buffer 不可发送。
- shared 程序缺少 DLL：确认 `pae.dll` 来自同一包且位于程序相邻目录，不复制系统 CRT DLL。

## 6. 生命周期与线程边界

- metadata 中的字符串 view 借用 `CompiledProtocol`；owner 移动、替换或销毁后重新查询。
- Decode record/field view 借用 Codec，并会在下一次受保护的 Decode/Encode、Codec 移动或销毁后
  失效；BYTES 还借用未修改的输入字节。
- Framer callback 的候选字节只在同步回调内有效；跨回调保存必须复制。
- 同一执行实例的并发、生命周期和 workspace 使用必须遵守公开头注释；PAE 不替调用者管理通信线程。

## 7. 文件真实性与许可

`MANIFEST.txt` 记录文件和长度，`SHA256SUMS.txt` 记录包内文件 Hash，`PROVENANCE.json` 记录来源。
这些信息用于内部一致性与追溯，不提供签名、来源认证或防篡改保证。仓库当前没有项目级 PAE 许可；
`LICENSES/yyjson-LICENSE.txt` 只适用于 yyjson，不代表 PAE 获准外部分发。

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
此前二进制是由当前脏树重建。仓库侧 2026-09-26 的实际打包与消费记录见
`docs/engineering/yaml-sdk-packaging-validation-20260926.md`（不随 SDK 包安装）。
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
