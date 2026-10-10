# Public COMPLETE_RECORD Codec example

> 交付版投影：据固定 `9680cf90512f053962949cb55467d6af35959088` 的
> `examples/public_api_codec/README.md` 制作；仅将未随 SDK 附带的工程验证链接
> 标为固定源码仓库历史参考，本文件不是该提交的原始字节。

本示例是 Stage 1/1B 的最小公开消费者，只 include `pae/compiler.h`、`pae/codec.h`，并链接
`PAE::pae`。它不引用 `src/` 私有头，也不包含 Lab、Qt、Transport、Framer 或 Host 语义。

`main.cpp` 接受两个位置参数，顺序固定：

1. Binary 配置：`examples/config/synthetic_lab_exchange_slice.pae.json`；
2. ASCII 配置：`examples/config/synthetic_ascii_text_slice.pae.json`。

Binary 路径先从公开 metadata 取得 Pipeline/Message 关联，核对 Decode/Encode 可用性、字段逻辑类型、
`CALLER_INPUT` 来源和 `EXACT` 输出长度，再构造 typed values，执行 Encode 后 Decode。ASCII 路径同样
根据公开 metadata 准备 caller input 和输出 Buffer，不从 direction 字符串或私有 Plan 推断行为。

## Source 包独立运行（本轮未执行）

从 Source 包根执行，BuildRoot 放在包外并使用新目录；此例需要包根 CMake，SDK 不携带开发仓库 preset：

```powershell
$PackageRoot = (Resolve-Path .).Path
$BuildRoot = Join-Path ([IO.Path]::GetTempPath()) 'pae-source-codec-release'
if (Test-Path -LiteralPath $BuildRoot) { throw 'Choose a fresh BuildRoot' }
cmake -S $PackageRoot -B $BuildRoot -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF `
  -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_PROTOCOL_LAB_UI=OFF
cmake --build $BuildRoot --config Release --target pae_public_codec_example -- /m:1
& (Join-Path $BuildRoot 'Release/pae_public_codec_example.exe') `
  (Join-Path $PackageRoot 'examples/config/synthetic_lab_exchange_slice.pae.json') `
  (Join-Path $PackageRoot 'examples/config/synthetic_ascii_text_slice.pae.json')
```

成功输出为 `PUBLIC_CODEC_EXAMPLE gate=PASS`，失败退出码为 1。

阶段 1B 的既有验证报告记录该 metadata 驱动示例在 Windows x64 Debug/Release 均通过。
固定 `9680cf9` 源码仓库历史参考（未随 PAE SDK 附带）：
`docs/engineering/pae-public-codec-slice-validation.md`、
`docs/engineering/pae-public-consumer-metadata-validation.md`。
这是限定工具链和合成配置范围内的结果，不代表独立 SDK/安装包、Lab 迁移、Linux、真实协议、硬件、
现场或生产验证完成。
