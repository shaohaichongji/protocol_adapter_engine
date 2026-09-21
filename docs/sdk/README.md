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
$BuildRoot = 'C:\path\to\fresh\pae-getting-started-build'
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
