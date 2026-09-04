# Protocol Conformance Runner

`pae_protocol_conformance_runner`是仅在`PAE_BUILD_TESTING=ON`且启用
`PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON`时构建的内部一致性验证工具。

它读取一份严格JSON协议配置和一个外部Corpus（语料）目录，执行：

- 配置编译与Protocol/Pipeline/Message/Direction稳定标识绑定检查；
- Manifest登记的Frame SHA-256复核；
- 每条向量的精确Decode与Encode逐字节比较；
- 重复Encode确定性检查；
- 长度、固定字节、未知枚举及常见Encode错误的失败关闭检查。

命令格式：

```text
pae_protocol_conformance_runner \
  --config <protocol.pae.json> \
  --corpus-root <directory-containing-manifest_v0.1.tsv>
```

Runner输出两个不同门禁：

- `ENGINE_POC_PASS`：配置驱动引擎在当前语料上的行为是否通过；
- `PROTOCOL_GOLDEN_PASS`：只有所有向量均具有`CONTRACT_FACT`或
  `OBSERVED_CAPTURE`证据，并通过独立复核时才允许通过。

`DOCUMENT_DERIVED_REVIEWED`、`IMPLEMENTATION_CORROBORATED`和
`SYNTHETIC_REVIEWED`可以通过引擎PoC，但不能被Runner自动升级为协议Golden Vector。

该目标不安装、不导出，不属于稳定公共API，也不持有Transport、线程、Route或设备生命周期。
