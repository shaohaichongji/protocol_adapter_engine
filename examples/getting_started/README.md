# 最小公开 API 入门程序

这是首次接触 PAE 时建议运行的最小 C++17 consumer。它只 include `pae/compiler.h`、
`pae/codec.h`，只链接已安装 SDK 的 `PAE::pae`，不引用仓库 `src/**` 私有接口，也不把通信、
重试或设备生命周期放进 PAE。

程序复用公开 canonical 配置
[`synthetic_stream_framing_slice.pae.json`](../config/synthetic_stream_framing_slice.pae.json) 的第一个
Pipeline：`fixed_rx`。它完成一条很小的闭环：

1. 读取严格 JSON 配置并调用 `CompileProtocolJson()`；
2. 从冻结 metadata 核对 `fixed_rx`、`fixed_message`、`fixed_payload`；
3. Decode 手写字节 `AA 00 07`，得到 `fixed_payload=7`；
4. 把类型化 `UINT64 7` 交给 Encode，得到同一手写预期 `AA 00 07`。

这里的 `0.9` 是所用配置的 Schema 能力版本，不是学习路线编号。示例没有用 Encode 的输出生成
Decode 预期，因此不是只做往返自洽。

## Windows：从 SDK 包根目录运行

先进入解压后的 SDK 包根目录；构建目录必须在包外且尚不存在：

```powershell
$PackageRoot = (Resolve-Path .).Path
$Build = 'C:\path\to\fresh\pae-getting-started-build'
$Config = "$PackageRoot\examples\config\synthetic_stream_framing_slice.pae.json"
```

source 包使用自身源码：

```powershell
cmake -S "$PackageRoot\examples\getting_started" -B $Build `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DPAE_SOURCE_DIR=$PackageRoot"
```

static/shared 安装包改用 `find_package`：

```powershell
cmake -S "$PackageRoot\examples\getting_started" -B $Build `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
```

二选一完成配置后构建并运行；`Release` 必须与当前二进制包配置一致：

```powershell
cmake --build $Build --config Release --target pae_getting_started -- /m:1
& "$Build\Release\pae_getting_started.exe" $Config
```

shared 包的 CMake 规则会把包内 `pae.dll` 复制到程序相邻目录；该规则本轮未执行验证。工具链、
Debug/Release 和 CRT 必须先读包根 `PROVENANCE.json` 与 `PAE-SDK-README.md`，不能把上面的已验证
Release 命令机械套到另一种包。

预期最后五行：

```text
CONFIG schema=0.9 protocol=synthetic_stream_framing
PIPELINE id=fixed_rx message=fixed_message
DECODE frame=AA 00 07 field=fixed_payload value=7
ENCODE value=7 frame=AA 00 07
GETTING_STARTED_BINARY_PASS
```

## 生命周期边界

- `ProtocolDescription`、`PipelineDescription` 等字符串视图借用 `CompiledProtocol`，移动、替换或销毁
  owner 后重新查询，不保存旧 view。
- `DecodedRecordView`/`DecodedFieldView` 借用 `CompleteRecordCodec`；下一次受保护的 Decode/Encode、
  移动或销毁 Codec 会使旧 view 失效。BYTES 字段还会借用未修改的输入字节。
- Encode 的 output buffer 归调用者所有；仅在 `status==OK` 时使用 `bytes_written` 范围。失败时即使
  buffer 被改写也不是可交付报文。
- Pipeline 的 `direction_id` 是配置的协议方向。本例的 `fixed_rx` 名称不意味着所有 Pipeline 都只能
  Decode；是否可执行 Decode/Encode 应查询 `PipelineMessageExecution()`。

更完整的可重复包验证位于包内 `examples/sdk_consumer/`。源码包还包含聚焦 Codec、Framer 和 Host
的示例；它们不应被整段复制进第一个程序。中文选包、错误定位及包内路径见
`docs/sdk/README.md`。
