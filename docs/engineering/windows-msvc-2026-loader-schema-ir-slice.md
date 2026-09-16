# PAE Loader/SchemaIr Windows 可执行切片验证报告

> 公开分发说明：本文保留2026-09-01的Loader/SchemaIr执行数量和环境事实。首次公开推送前，旧实现证据衍生配置的具体标识与布局已移除；当前仓库改用完全独立的`SYNTHETIC_FROM_SCRATCH（从零人工设计）`样例，并以后续回归结果作为当前证据。

## 1. 结论

截至 2026-09-01，PAE（ProtocolAdapterEngine，协议适配引擎）已经形成首个可执行的 Loader/SchemaIr（加载器/类型化中间表示）纵向切片，并在 Windows x64、MSVC（Microsoft Visual C++，微软C/C++编译器）下完成实际配置、构建和测试。

该结果只证明以下链路在当前草案切片内可工作：

```text
严格 JSON 字节
→ 有界 yyjson 候选解析
→ Structural Validation（结构校验）
→ Domain Validation（领域校验）
→ Resource Budget（资源预算）
→ 不可变 PlanBundle
```

它不是完整 PAE V0.1，也不证明 Decode（解码）、Encode（编码）、Runtime（运行时）、C ABI（C 应用二进制接口）、真实协议、Linux、性能、硬件或现场行为。

## 2. 本切片范围

当前只支持：

- `COMPLETE_RECORD（完整记录）`输入形态；
- `frame_length_equals`和`fixed_bytes` Matcher（消息匹配器）；
- `UINT64`、`BYTES`和`ENUM`；
- `input`及 UINT64 `constant` Encode Source（编码来源）；
- Pipeline、Message、FramingProfile和Field稳定ID引用；
- 字段越界、重叠、引用、方向、常量位宽、Matcher冲突和静态歧义校验；
- Desktop与Constrained的首批Plan容量门禁；
- 成功时完整`PlanBundle`，失败时无部分Plan。

内部实现位于`src/config_compiler`，没有安装、导出或写入`include/`公共接口。yyjson只出现在私有`.cpp`和PRIVATE CMake依赖中，第三方类型没有进入SchemaIr或PlanBundle。

## 3. 配置与协议证据边界

仓库当前样例是从零设计、可公开分发的实验台双向切片：

- 所有`source_ref`均为`SYNTHETIC_FROM_SCRATCH:`；
- 标识、长度、常量、字段值、Little Endian（小端）和完整字节布局均不来自客户协议、生产代码或现场报文；
- 13/11字节记录以及三字节整数、BYTES和ENUM组合只用于覆盖当前引擎能力；
- `record_length`仍为`encode.source=input`，不声明computed（计算字段）或自动长度回填语义；
- 配套独立Golden Vector（黄金测试向量）只证明人工协议范围内的引擎行为。

正式协议来源、实现证据和待确认项保存在仓库之外，本仓库不复制敏感原始资料。

## 4. 实际验证环境

| 项目 | 实际值 |
| --- | --- |
| CMake | 4.4.3 |
| Generator（生成器） | Visual Studio 18 2026，x64 |
| C/C++ Compiler | MSVC 19.51.36256.0 |
| Toolset（工具集） | 14.51.36231 |
| Windows SDK | 10.0.22621.0 |
| 目标系统 | Windows 10.0.19045，x64 |

## 5. 执行结果

### 5.1 Loader/SchemaIr

Release：

```text
pae.config_compiler.contract: 1/1 passed
内部契约用例: 21 passed, 0 failed
```

Debug：

```text
pae.config_compiler.contract: 1/1 passed
```

21项内部用例覆盖：合法人工Plan、最小合法配置、稳定ID Golden Snapshot（黄金快照）逐字节比较、重复编译等价、Unicode码点长度、BOM（Byte Order Mark，字节顺序标记）、非法UTF-8（8位Unicode转换格式）、重复key、尾随逗号、未知/缺失/类型错误、整数`2.0`、unsigned `-0`、重复ID、常量与Matcher冲突、静态Matcher歧义、缺失引用、方向不匹配、字段重叠/越界和4 MiB输入Hard Limit（硬上限）。所有负例都检查无部分Plan。

Runner同时断言`expected=21`；用例数量减少或增加但未同步契约时，门禁同样失败，避免测试被静默删除。

### 5.2 旧 Spike 回归与共存

```text
JSON Parser Spike Release: 20/20 passed
Spike + Loader同时启用 Release: 21/21 passed
```

共存配置还会重新读取并核对每个调用方的yyjson候选锁；当前两个锁的Archive URL和SHA-256一致。

### 5.3 JSON Schema参考校验

使用 PowerShell 7 `Test-Json -SchemaFile schema/pae.schema.json`实际执行8项结构参考校验，8/8符合预期：

- 两份合法配置通过；
- 嵌套未知属性、129字符stable ID和257个Unicode码点的`display_name`被拒绝；
- 100个Unicode码点的`display_name`通过；
- `2.0`和unsigned `-0`在JSON Schema数学值层通过，再由PAE保留原始Number Token并拒绝，符合Structural职责分工。

本轮没有单独把`pae.schema.json`提交给官方Draft 2020-12 Meta-Schema（元Schema）做自验证，因此不能把上述8项写成完整Schema标准符合性认证。

## 6. 本轮关闭的问题

- Windows SDK的`DOMAIN`宏与内部枚举冲突，改为`DOMAIN_VALIDATION`；
- yyjson默认分配改为最大64 MiB的pool allocator（池式分配器）；
- Schema `maxLength`与Compiler统一按解码后的Unicode码点计数，stable ID草案上限统一为128；
- 移除内部编译函数不真实的`noexcept`内存耗尽承诺；
- Golden Snapshot只表达稳定ID，不冻结内部数组索引；
- 增加常量与Matcher冲突、同Pipeline静态Matcher歧义门禁及直接负例；
- Spike与Loader共用yyjson目标时，不再静默跳过第二份候选锁校验。

## 7. 未验证与下一步门禁

以下仍为`OPEN / UNVERIFIED（待定/未验证）`：

- yyjson最终生产选型和正式Vendor（随仓库源码）边界；
- 完整PAE V0.1 Schema、全部字段类型、流式Framing、Integrity和Receive Gate；
- 可信外部LoaderLimits（加载器上限）输入；当前内部切片解析前使用Hard Limit，配置内`constrained`只参与后续Plan预算；
- Loader/Compiler全链峰值内存、最大Field/Message组合、全链allocator故障注入和生产容量冻结；
- 完整Strict JSON Corpus（一致性语料）及Meta-Schema自验证；
- Decode/Encode、动态业务值、Runtime、Session、回调、C API和公共API；
- 三个PoC（Proof of Concept，概念验证）的独立Golden Vector；
- Linux GCC/Clang、Sanitizer、吞吐、低占用、目标板、硬件和现场验证。

因此，本切片仍是内部、可替换、未导出的实现基础，不得直接替换生产协议代码。
