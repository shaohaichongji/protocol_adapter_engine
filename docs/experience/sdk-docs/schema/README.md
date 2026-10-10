# Schema

> 交付版投影：据固定 `513f6cc9bd46b64b3b6d1d4283f13c806039eebf` 的
> `schema/README.md` 制作；本文件不是该提交的原始字节。Schema 能力表和
> 下列规范入口保留，具体 SDK/Lab 可用路径以所选包及其验证为准。

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
- [PAE 受限 YAML Profile V0.1](pae_yaml_profile_v0.1.md)：定义可选作者源的语法、标量、来源及试用资源边界；转换后仍进入严格 JSON 编译链。

不同宿主和工具并不自动支持全部版本。旧 Protocol Lab CLI/Evidence 链最高执行到 0.8；
具体构建开关和 installed-SDK 来源须按**所选包**的 provenance、公开头及同批验证核对，
不能仅由 Schema 版本号推断 UI 可见能力。非 Qt 底层能力、UI 可见能力、自动验证和
人工验收必须分别判断。

JSON Parser Spike只验证候选Parser和Strict JSON Loader Profile，不构成正式Schema或协议正确性证据。当前仍未形成稳定公共 API、正式协议 Golden Vector、完整 Receive Gate/Mapping 产品闭环、Linux/目标板/硬件/现场或生产验收。

固定源码仓库的历史结构参考曾用 PowerShell 7 `Test-Json` 检查8项输入；
PAE C++ Compiler 另以原始 Number Token 拒绝 Schema 数学值层会接受的 `2.0`
和 unsigned `-0`。这不是所选 SDK 包的新测试；官方 Draft 2020-12
Meta-Schema 自验证仍不由本交付版声明完成。
