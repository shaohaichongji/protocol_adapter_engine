# COMPLETE_RECORD Synthetic Engine Vectors

本目录保存 PAE（ProtocolAdapterEngine，协议适配引擎）首个
`COMPLETE_RECORD（完整记录）`编解码切片使用的机器向量。

## 权威边界

- 两条向量均为`SYNTHETIC_REVIEWED / INDEPENDENT_ENGINEERING_REVIEWED（人工构造 / 独立工程复核）`；
- `.frame.hex`、Decode期望和Encode输入在Codec（编解码器）实现前人工写定；
- 构建和测试不得使用PAE Encoder（编码器）生成或覆盖这些源文件；
- 两条向量的13/11字节布局、常量、Little Endian（小端）、三字节整数、BYTES和ENUM均为`SYNTHETIC_FROM_SCRATCH（从零人工设计）`；
- `record_length`只是由宿主输入的普通动态字段，不声明自动长度回填或其他生产协议语义；
- 因此这些向量只验证当前草案Plan的字段偏移、类型、字节序、Matcher（匹配器）、Decode（解码）、动态Encode（编码）和失败关闭行为；
- 向量不映射任何客户协议、生产代码、真实设备、硬件地址或现场报文。

## 文件关系

`manifest_v0.1.tsv`登记向量身份、方向、来源等级、文件位置和Frame文件SHA-256
（Secure Hash Algorithm 256-bit，256位安全散列算法）。CMake门禁另固定并校验全部六份源文件及运行副本的SHA-256。每条向量由三份独立输入组成：

```text
*.frame.hex
    人工期望的完整报文字节

*.decode_expected.tsv
    对上述字节的类型化Decode期望

*.encode_input.tsv
    宿主提交的动态Logical Value（逻辑值）；不包含constant字段
```

Round-trip（编码后再解码）只能作为辅助回归，不能替代`.frame.hex`的独立逐字节比较。

`synthetic_sum8/`另保存PAE-DEC-041从零设计的单条SUM8人工向量及其独立算术说明；它不修改
上述V0.1清单或两条历史向量，也不把新向量提升为真实协议Golden。
