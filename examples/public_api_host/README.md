# Public HostEndpoint example

本示例是阶段 2B 的最小非 Lab Host 消费者，只 include `pae/compiler.h`、
`pae/host_endpoint.h` 并链接 `PAE::pae`。`standalone/CMakeLists.txt` 通过 `PAE_SOURCE_DIR` 引入源码树，
关闭 Testing，构建 target `pae_public_host_external_consumer`；它不使用 `src/**` 私有头，也不提供
Socket、串口、线程、重试或业务路由。

## 最短调用链

1. `CompileProtocolJson()` 得到 `CompiledProtocol`；
2. 以 `HostBindingSpec` 创建两个不可变 binding：相同 `endpoint_key="device"`，分别绑定
   `DECODE + pipeline 0` 和 `ENCODE + pipeline 0`；binding 不固定 Message；
3. `Find()` 分别取得 Decode 与 Encode `HostChannelHandle`；
4. 两次 `Push()` 组合 CRLF 分块，在同步 candidate observer 后向 business sink 交付一次成功 Decode；
5. `Reset(receive_handle)` 后旧 Handle 变为 stale，调用者重新 `Find()` 取得新代 Handle；
6. 使用同一 Encode Handle 调用两次 `Encode()`，分别以调用期 `message_index` 选择 Message 0 和 2。

Handle 是非拥有的 scope/channel/generation 身份，不能保活 Host。candidate、Decode record/field 和
Encode bytes 只在当前同步 callback 内借用，需要持久化时必须在 callback 内复制。observer 或 business
callback 抛出异常时，Host 捕获异常并保留真实 `bytes_consumed` 与已发生回调计数，目标通道进入
`RESET_REQUIRED`；调用方副作用不会回滚。Encode callback 收到字节只表示本次同步输出交付，示例没有
Transport，因此不等于数据已通过网络或串口发送。

## 已验证的 standalone 命令

工作目录为当前源码包或仓库根目录，并先记录该可搬移根目录：

```powershell
$PaeSourceRoot = (Resolve-Path .).Path
```

以下命令对应 selector 修正后的
[`阶段 2B Windows 验证报告`](../../docs/engineering/pae-public-host-stage2b-validation.md)；
`<CONFIG>` 依次替换为 `Debug`、`Release`：

```powershell
cmake -S examples/public_api_host/standalone `
  -B out/build/windows-msvc-public-host-stage2b-consumer `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  "-DPAE_SOURCE_DIR=$PaeSourceRoot"

cmake --build out/build/windows-msvc-public-host-stage2b-consumer `
  --config <CONFIG> --parallel 4 --target pae_public_host_external_consumer

& .\out\build\windows-msvc-public-host-stage2b-consumer\<CONFIG>\pae_public_host_external_consumer.exe `
  .\examples\config\synthetic_ascii_stream_slice.pae.json
```

`main.cpp` 实际只接受上述一个 ASCII stream 配置参数。Debug/Release 新版 consumer 日志均以退出码 0
输出：

```text
PUBLIC_HOST_CONSUMER_PASS candidates=1 successes=3 greeting_bytes=8 literal_bytes=6
```

最终报告另记录 selector 修正后 Host 专项两配置各 56/56、受影响定向 CTest 各 10/10。原 52/52
只证明创建期固定 Encode Message 的修正前模型，不能作为当前 API 的最终证据。这里引用既有验证，
不表示本轮重新执行，也不代表独立 SDK/安装包、Lab 迁移、稳定 ABI、Linux、网络、设备、真实协议、
Golden、硬件、现场、生产或正式性能验证完成。
