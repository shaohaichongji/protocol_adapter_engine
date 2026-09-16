# Schema

PAE V0.1 将使用 JSON Schema Draft 2020-12 描述基础结构，并使用独立的 ProtocolPlan Execution Semantics（协议执行计划语义规范）描述跨字段、跨引用、资源和执行语义。

当前 `pae.schema.json` 枚举 Schema 0.1～0.11。版本号表示逐步叠加的内部能力切片，不表示稳定公共格式或完整 PAE V0.1：

| Schema | 主要增量 |
| --- | --- |
| 0.1 | `COMPLETE_RECORD`、固定 Matcher、`UINT64`、固定 `BYTES`、`ENUM` |
| 0.2 | 位容器与 `BOOL` |
| 0.3 | `SUM8` 完整性校验 |
| 0.4 | 1～8 字节补码 `INT64` |
| 0.5 | 精确 Decimal64 比例/偏置 |
| 0.6 | 参数化 CRC-16/32 |
| 0.7 | 固定完整记录的 computed length |
| 0.8 | 单段有界变长 `BYTES` 与动态长度/校验范围 |
| 0.9 | Binary 有界流式 Framing |
| 0.10 | ASCII 完整记录编解码 |
| 0.11 | ASCII CRLF 有界流式 Framing |

- [PAE V0.1 Draft Slice JSON Schema](pae.schema.json)：Draft 2020-12结构草案；
- [ProtocolPlan Execution Semantics V0.1 — Draft Slice](protocol_plan_execution_semantics_v0.1.md)：首切片跨引用、布局、Matcher、Encode Source和Plan语义；
- [PAE Strict JSON Profile V0.1](strict_json_profile_v0.1.md)：定义`*.pae.json`输入字节、严格词法、Unicode、Number Token、重复 key 和资源边界。

不同宿主和工具并不自动支持全部版本：旧 Protocol Lab CLI/Evidence 链最高执行到 0.8；Qt Lab 的 ASCII 路径覆盖 0.10/0.11；Binary Host UI 当前只开放 0.9 完整记录 Decode，Binary Encode 与流式 UI 尚未接入。非 Qt 底层能力、UI 可见能力和人工验收必须分别判断。

JSON Parser Spike只验证候选Parser和Strict JSON Loader Profile，不构成正式Schema或协议正确性证据。当前仍未形成稳定公共 API、正式协议 Golden Vector、完整 Receive Gate/Mapping 产品闭环、Linux/目标板/硬件/现场或生产验收。

当前草案Schema已通过PowerShell 7 `Test-Json`的8项结构参考校验，合法样例、嵌套未知属性、stable ID和Unicode长度边界结果符合预期；PAE C++ Compiler另以原始Number Token拒绝Schema数学值层会接受的`2.0`和unsigned `-0`。本轮未执行官方Draft 2020-12 Meta-Schema自验证。
