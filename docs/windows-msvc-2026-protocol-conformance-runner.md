# Protocol Conformance Runner Windows验证报告

## 1. 范围

本文记录`PAE-DEC-034`下通用`Protocol Conformance Runner（协议一致性验证运行器）`
的实现和Windows执行证据。

Runner是内部Testing工具，不是稳定公共API。它不持有Transport、Socket、线程、Route、
Session或设备生命周期，也不随产品静态库安装或导出。

## 2. 输入与证据门禁

Runner接收：

```text
--config <protocol.pae.json>
--corpus-root <directory-containing-manifest_v0.1.tsv>
```

Corpus沿用现有TSV/Hex结构：每条向量包含Frame、Decode期望和Encode输入。Manifest绑定
`protocol_id`、`protocol_version`、Pipeline、Message、Direction、证据等级、来源、相对路径
和Frame SHA-256。

允许的证据等级为：

- `CONTRACT_FACT`；
- `OBSERVED_CAPTURE`；
- `IMPLEMENTATION_CORROBORATED`；
- `DOCUMENT_DERIVED_REVIEWED`；
- `SYNTHETIC_REVIEWED`；
- `OPEN`。

Runner分别输出：

- `ENGINE_POC_PASS`：全部配置、绑定、哈希、精确编解码、确定性和失败路径是否通过；
- `PROTOCOL_GOLDEN_PASS`：只有每条向量都属于`CONTRACT_FACT`或`OBSERVED_CAPTURE`，
  且`review_status`包含独立复核标记时才允许通过。

Runner不会把实现佐证、文档推导或人工语料自动升级为协议Golden Vector（黄金样本）。

## 3. 覆盖范围

每条适用向量执行：

- Manifest与冻结Plan稳定标识、方向和Pipeline归属绑定；
- Frame SHA-256；
- 精确Decode字段、类型、逻辑值和raw值比较；
- 动态Encode逐字节比较；
- 重复Encode确定性；
- 截断、额外字节、固定Matcher错误和未知ENUM拒绝；
- 缺字段、重复字段、类型错误、整数宽度溢出和输出Buffer过小；
- Decode槽位与Encode输出Buffer的失败原子性/失败关闭检查。

Corpus级Coverage（覆盖）门禁要求至少有一条向量实际执行所有必需的负向类别。没有ENUM的
Message可以跳过其局部未知ENUM检查，但整个Corpus仍必须由其他Message闭合该能力。

## 4. Windows执行证据

环境：

```text
CMake 4.4.3
Generator: Visual Studio 18 2026
MSVC: 19.51.36256.0
Windows SDK: 10.0.22621.0
Architecture: x64
Build directory: out/build/windows-msvc-all-slices-dec034
```

全切片配置同时启用JSON Parser Spike、Loader/SchemaIr、ProtocolPlan、COMPLETE_RECORD Codec
和Conformance Runner。Configure阶段的1项正向、8项负向能力`try_compile`均符合预期。

结果：

| 验证项 | Release | Debug |
| --- | ---: | ---: |
| 全切片CTest | `27/27 PASS` | `27/27 PASS` |
| 公开Synthetic Runner CTest | `1/1 PASS` | `1/1 PASS` |
| 外部私有Corpus向量 | `4` | `4` |
| 外部私有Corpus检查 | `61 PASS / 0 FAIL / 2 SKIP` | `61 PASS / 0 FAIL / 2 SKIP` |
| `ENGINE_POC_PASS` | `PASS` | `PASS` |
| `PROTOCOL_GOLDEN_PASS` | `NOT_SATISFIED` | `NOT_SATISFIED` |

两个`SKIP`来自外部Corpus中两条同类Message没有ENUM字段；Corpus级未知ENUM拒绝覆盖已由
另一个方向的Message闭合。这是按能力适用性跳过，不是失败或未运行整个错误类别。

## 5. 构建边界

- Runner只在`PAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON`且`PAE_BUILD_TESTING=ON`时加入；
- 产品Only构建路径不加入Runner、Config Compiler或测试语料；
- 公开仓库CTest只使用从零设计的`synthetic_lab_exchange`；
- 外部私有Corpus路径和内容不写入CMake、源码、公开样例或Git跟踪文件；
- Runner当前只覆盖`COMPLETE_RECORD`及V0.1已实现的`UINT64/BYTES/ENUM`能力。

上述产品边界已在全新`out/build/windows-msvc-product-dec034`目录实际Configure并完成Release
构建。除CMake编译器识别程序外，只生成`pae_protocol_plan.lib`和
`pae_protocol_core_slice.lib`；生成工程中没有Conformance Runner、Config Compiler、
yyjson、instrumented Core或测试Runner目标。

## 6. 未验证边界

- `PROTOCOL_GOLDEN_PASS`仍未取得独立协议证据；
- 未执行真实设备、硬件、现场网络或生产替换验证；
- Linux GCC/Clang、Sanitizer和正式Benchmark仍未执行；
- STREAM_CHUNK、bitfield、BCD、ASCII、Checksum/CRC、Receive Gate、Route、Session和C ABI不在本切片。
