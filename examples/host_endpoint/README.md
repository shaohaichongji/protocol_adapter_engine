# Serialized host endpoint binding example

本例使用[公开ASCII流式配置](../config/synthetic_ascii_stream_slice.pae.json)，将同一逻辑端点
`device`分别绑定Decode与Encode，并为Decode预建两条独立逻辑流。端点不是网络地址；
名字不决定动作。示例提交两条`ONLY\r\n`，按STOP返回的bytes_consumed续提准确后缀，
再用独立Encode-only动作得到手算期望`SEND\r\n`。不打开Socket、不建立线程、不自动转发。

入口是内部非安装的`pae::host_endpoint::Session`，见
[契约](../../docs/host-endpoint-binding-contract.md)及[实施验证报告](../../docs/host-endpoint-binding-validation.md)。

## 调用与生命周期

1. `CompileJsonToPlan`得到PlanOwner，移动交给`Session::Create`；即使创建失败也消费该所有权。
2. 传入完整BindingSpec数组：endpoint、Action、pipeline、streams及可选Framer限额。
   创建成功前不发布任何可用绑定。首片只接受Schema 0.9/0.10/0.11。
3. `Find(endpoint, action, stream_index)`返回实例作用域Handle；不能跨Session复用。流索引
   只选择注册时预建的通道，不表示动态创建。Encode只有一个通道。
4. Decode完整记录使用`Decode`，流片段使用`Push`；输入种类不匹配直接拒绝。
   Encode传Message ID与NamedValue数组，NamedValue的Core field引用须留空，由Session解析。
5. 仅成功结果进入同步Sink。Decode字段保留Core类型、FieldRef及Plan内身份，ASCII字段为BYTES；
   Encode结果为完整字节。要保留数据必须在回调内复制，不能保存视图或跨Plan使用引用。
6. 所有操作串行，禁止同实例重入、并发销毁或回调中销毁Session。Reset仅作用于指定Decode通道，
   清流状态并递增generation；Encode回调故障后须重建Session。Plan晚于所有工作区销毁。

`Result.status`汇总本次失败，`codec_status`是最后一个候选的Core状态；混合坏帧和好帧时
可出现CODEC_FAILED与最后codec_status=OK，须结合decode_failures和successful_outputs判断。
`framing_attempted=false`表示Framer结果不可作已执行事实。frames_delivered仍为候选数。
回调抛异常后不计为成功回调，但宿主可能已产生部分业务副作用，不能由Session回滚或自动重试。

接入层每次只Push一次，输入所有权及未消费游标归宿主。空Push仅接受内部pending，不能刷新半包。
API失败保留消费事实并要求Reset；Core候选失败不阻断后续候选，不输出部分有效字段。

## 构建

使用Windows MSVC；所有能力显式打开，不依赖Qt：

```powershell
cmake -S . -B out/build/windows-msvc-host-endpoint -G "Visual Studio 18 2026" -A x64 `
  -DPAE_BUILD_HOST_ENDPOINT_SLICE=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_BUILD_STREAM_FRAMING_SLICE=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON -DPAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON `
  -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON -DPAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING=ON `
  -DPAE_BUILD_TESTING=ON
cmake --build out/build/windows-msvc-host-endpoint --config Release --target pae_host_endpoint_example
out/build/windows-msvc-host-endpoint/examples/host_endpoint/Release/pae_host_endpoint_example.exe `
  examples/config/synthetic_ascii_stream_slice.pae.json
```

`HOST_ENDPOINT_EXAMPLE=PASS`仅证明离线合成宿主调用，不是网络、Linux、Golden、硬件或现场验收。
