# Config Compiler Fixtures

这些文件是`V0.1 DRAFT SLICE / INCOMPLETE`配置编译语料，不是完整Schema V0.1验收集。

- `valid/minimal_complete_record.pae.json`：最小Structural/Domain合法配置；
- `invalid/structural_unknown_property.pae.json`：根对象未知属性；
- `invalid/structural_integer_fraction.pae.json`：整数语义属性使用`2.0`；
- `invalid/structural_unsigned_negative_zero.pae.json`：无符号属性使用词法`-0`；
- `invalid/domain_missing_framing_reference.pae.json`：Pipeline引用不存在的FramingProfile；
- `invalid/domain_direction_mismatch.pae.json`：Pipeline与Message方向不一致；
- `invalid/domain_field_overlap.pae.json`：两个Field Wire区间重叠。
- `invalid/domain_frame_not_fully_defined.pae.json`：Field与Fixed Matcher区间并集未覆盖完整帧。

所有来源均使用`SYNTHETIC:`，不包含真实客户、现场IP或原始报文。期望阶段、错误类别和JSON Pointer见[`../expected/diagnostics_v0.1.tsv`](../expected/diagnostics_v0.1.tsv)。

其中`2.0`和`-0`用例依赖原始Number Token，测试工具不得先把Fixture解析并重新序列化后再交给PAE，否则会破坏需要验证的词法形态。
