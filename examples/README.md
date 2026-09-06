# Examples

只允许放置脱敏、人工构造或已确认可分发的协议示例。真实客户协议和现场数据默认禁止进入 Git。

- [`config/synthetic_lab_exchange_slice.pae.json`](config/synthetic_lab_exchange_slice.pae.json)：从零设计的人工实验台双向配置切片；状态为`V0.1 DRAFT SLICE / INCOMPLETE`。
- [`config/synthetic_sum8_slice.pae.json`](config/synthetic_sum8_slice.pae.json)：从零设计的Schema 0.3
  SUM8完整记录样例，配套Values与独立计算Frame；不来自真实协议或现场报文。

该样例只用于当前及后续 Loader/Compiler 和 Plan快照验证：

- 所有`source_ref`均为`SYNTHETIC_FROM_SCRATCH（从零人工设计）`；
- 标识、长度、常量、字段值、大小端和完整字节布局均不来自客户协议、生产代码或现场报文；
- 两个方向故意使用13/11字节记录、Little Endian（小端）、三字节整数、BYTES和ENUM组合，以覆盖当前引擎能力；
- `record_length`使用`encode.source=input`，只验证动态字段写入，不声明自动长度回填语义。

该样例由C++ Loader/Compiler生成与机器Golden Snapshot逐字节一致的Plan，并配套独立Engine Vector；这只验证人工协议范围内的引擎行为，不代表完整PAE V0.1、任何生产协议、硬件或现场验证通过。
