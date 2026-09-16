# Public COMPLETE_RECORD Codec example

本示例是 Stage 1/1B 的最小公开消费者，只 include `pae/compiler.h`、`pae/codec.h`，并链接
`PAE::pae`。它不引用 `src/` 私有头，也不包含 Lab、Qt、Transport、Framer 或 Host 语义。

`main.cpp` 接受两个位置参数，顺序固定：

1. Binary 配置：`examples/config/synthetic_lab_exchange_slice.pae.json`；
2. ASCII 配置：`examples/config/synthetic_ascii_text_slice.pae.json`。

Binary 路径先从公开 metadata 取得 Pipeline/Message 关联，核对 Decode/Encode 可用性、字段逻辑类型、
`CALLER_INPUT` 来源和 `EXACT` 输出长度，再构造 typed values，执行 Encode 后 Decode。ASCII 路径同样
根据公开 metadata 准备 caller input 和输出 Buffer，不从 direction 字符串或私有 Plan 推断行为。

## Windows Debug 运行

工作目录必须是当前源码包或仓库根目录；以下命令均使用该目录下的相对路径。

```powershell
cmake --preset windows-msvc-public-api-stage1
cmake --build --preset windows-msvc-public-api-stage1-debug `
  --target pae_public_codec_example -- /m:1
& .\out\build\windows-msvc-public-api-stage1\Debug\pae_public_codec_example.exe `
  .\examples\config\synthetic_lab_exchange_slice.pae.json `
  .\examples\config\synthetic_ascii_text_slice.pae.json
```

Release 使用现有 `windows-msvc-public-api-stage1-release` build preset，并运行
`out\build\windows-msvc-public-api-stage1\Release\pae_public_codec_example.exe`，输入参数和顺序不变。
成功输出为 `PUBLIC_CODEC_EXAMPLE gate=PASS`，失败退出码为 1。

阶段 1B 的既有验证报告记录该 metadata 驱动示例在 Windows x64 Debug/Release 均通过：
[公开 Codec 验证](../../docs/engineering/pae-public-codec-slice-validation.md)、
[消费准备 metadata 验证](../../docs/engineering/pae-public-consumer-metadata-validation.md)。
这是限定工具链和合成配置范围内的结果，不代表独立 SDK/安装包、Lab 迁移、Linux、真实协议、硬件、
现场或生产验证完成。
