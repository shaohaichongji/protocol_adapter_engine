# Schema

PAE V0.1 将使用 JSON Schema Draft 2020-12 描述基础结构，并使用独立的 ProtocolPlan Execution Semantics（协议执行计划语义规范）描述跨字段、跨引用、资源和执行语义。

当前Schema 0.1～0.4内部切片覆盖COMPLETE_RECORD固定Matcher、UINT64/INT64/BYTES/ENUM、位容器BOOL和SUM8；Schema 0.4新增1～8字节补码INT64。它仍不是完整PAE V0.1 Schema。

- [PAE V0.1 Draft Slice JSON Schema](pae.schema.json)：Draft 2020-12结构草案；
- [ProtocolPlan Execution Semantics V0.1 — Draft Slice](protocol_plan_execution_semantics_v0.1.md)：首切片跨引用、布局、Matcher、Encode Source和Plan语义；
- [PAE Strict JSON Profile V0.1](strict_json_profile_v0.1.md)：定义`*.pae.json`输入字节、严格词法、Unicode、Number Token、重复 key 和资源边界。

JSON Parser Spike只验证候选Parser和Strict JSON Loader Profile，不构成正式Schema或协议正确性证据。完整V0.1仍需补齐三个PoC需要的流式Framing、其余字段类型、Integrity、Receive Gate、Mapping、正式长度字段语义和Golden Vector。

当前草案Schema已通过PowerShell 7 `Test-Json`的8项结构参考校验，合法样例、嵌套未知属性、stable ID和Unicode长度边界结果符合预期；PAE C++ Compiler另以原始Number Token拒绝Schema数学值层会接受的`2.0`和unsigned `-0`。本轮未执行官方Draft 2020-12 Meta-Schema自验证。
