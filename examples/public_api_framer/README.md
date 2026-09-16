# Public StreamFramer example

本示例是阶段 2A 的最小非 Lab 消费者，只 include `pae/compiler.h`、`pae/stream_framer.h`、
`pae/codec.h` 并链接 `PAE::pae`。`standalone/CMakeLists.txt` 以 `PAE_SOURCE_DIR` 引入源码树，关闭
Testing，构建 target `pae_public_framer_external_consumer`；它不使用 `src/**` 私有头。

## 最短调用链

1. `CompileProtocolJson()` 得到 `CompiledProtocol`；
2. `QueryStreamFramingCapability(compiled, pipeline_index)` 确认该 Pipeline 有 stream framing 能力；
3. `CreateStreamFramer()` 创建绑定该 Pipeline/逻辑流的实例，同时创建共享同一 compiled state 的
   `CompleteRecordCodec`；
4. 调用一次 `Push()`；每个同步 `FrameSink` 候选在 callback 内只调用一次 `Decode()`。

候选形成不等于 Decode 成功，示例分别统计 `candidates`、`decode_calls` 和成功数。候选字节只在同步
`noexcept` callback 内有效；如需跨回调保存，调用者必须复制。`StreamSubmitResult::bytes_consumed`
之后的输入后缀仍由调用者持有并决定何时重提，Framer 不隐式缓存或循环排空。返回 `STOP` 时当前候选
已经交付，但不预读后缀，也不重放当前候选。空 `Push` 与 `Continue` 等价，每次只推进一次内部工作，
不会刷新、提交或丢弃半帧；需要清空流状态时显式调用 `Reset()`。

## 已验证的 standalone 命令

工作目录为当前源码包或仓库根目录，并先记录该可搬移根目录：

```powershell
$PaeSourceRoot = (Resolve-Path .).Path
```

以下命令对应
[`阶段 2A Windows 验证报告`](../../docs/engineering/pae-public-framer-stage2a-validation.md)
中的独立 public-only consumer；`<CONFIG>` 依次替换为 `Debug`、`Release`：

```powershell
cmake -S examples/public_api_framer/standalone `
  -B out/build/windows-msvc-public-framer-stage2a-consumer `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DPAE_SOURCE_DIR=$PaeSourceRoot"

cmake --build out/build/windows-msvc-public-framer-stage2a-consumer `
  --config <CONFIG> --target pae_public_framer_external_consumer --parallel 4

& .\out\build\windows-msvc-public-framer-stage2a-consumer\<CONFIG>\pae_public_framer_external_consumer.exe `
  .\examples\config\synthetic_stream_framing_slice.pae.json
```

`main.cpp` 实际只接受上述一个 stream 配置参数，使用两个固定 3-byte 输入候选。Debug/Release 的既有
日志均以退出码 0 输出：

```text
PUBLIC_FRAMER_CONSUMER_PASS candidates=2 decode_calls=2
```

报告另记录公开 Framer 专项两配置各 37/37、受影响定向 CTest 各 7/7。这里引用既有验证，不表示本轮
重新执行，也不代表 Host 2B、独立 SDK/安装包、Lab 迁移、Linux、网络、设备、真实协议、Golden、
硬件、现场、生产或正式性能验证完成。
