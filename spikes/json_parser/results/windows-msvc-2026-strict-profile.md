# JSON Parser Windows/MSVC Strict JSON Profile 门禁报告

> 历史证据说明：本文记录启用原始 Number Token 前的阶段结果。最新的 Raw Number、机器可读语料和 11/11 CTest 证据见[Number Token 与机器可读语料门禁报告](windows-msvc-2026-number-token-corpus.md)；本文中的`known_contract_gaps=2`和10项CTest不得作为当前状态引用。

## 1. 结论

本轮在既有 Parser（解析器）Spike（技术探针）上扩展 Strict JSON Profile（严格 JSON 子集规范）门禁，并重新执行 Windows x64/MSVC Release 构建和测试。

截至 2026-09-01 的实际结果为：

- yyjson 0.12.0、nlohmann/json 3.12.0、RapidJSON 1.1.0 各执行 100 个定向用例，测试期望 0 失败；
- 每个候选均通过 19 个 JSON Pointer（JSON 路径指针）精确断言和 14 个公共精确 byte offset（字节偏移）路径；
- CTest（CMake Test，CMake测试驱动）10/10 通过；
- yyjson 固定内存池、解析期 5 个分配点逐点故障注入和释放归零门禁继续通过；
- 每个候选均显式报告`known_contract_gaps=2`，对应原始 Number Token（数字词法单元）未保留导致的越界分类缺口；
- yyjson继续是唯一首选候选，但最终 Parser 选择仍为`OPEN（待定）`。

“测试期望0失败”只说明当前已声明行为没有发生非预期回归。两个已知契约缺口没有被改写为合规结论，也不能被绿色CTest掩盖。

## 2. 本轮范围

本轮新增或细化：

- 合法 JSON 前后空白、标准escape（转义）、合法代理项对和最高Unicode标量值；
- 不做Unicode Normalization（Unicode规范化）的对象key语义；
- UTF-8非法续字节、UTF-8编码代理区、超出`U+10FFFF`、历史5字节形式和UTF-16输入拒绝；
- Vertical Tab（垂直制表符）、Form Feed（换页符）和Non-Breaking Space（不换行空格）拒绝；
- 原始控制字符、原始NUL、行/块注释、对象/数组尾随逗号和尾随字符拒绝；
- 单独高代理项、单独低代理项和反向代理项拒绝；
- JSON Number完整词法边界，以及小数/指数误入整数属性；
- 所有合法JSON根值的语法接受与PAE根Object（对象）结构门禁分层；
- 独立的[Strict JSON Profile V0.1](../../../schema/strict_json_profile_v0.1.md)工作规范；
- [Loader与Compiler架构边界](../../../docs/loader_compiler_boundary_v0.1.md)草案。

本轮没有实现生产Loader、StructuralValidator（结构校验器）、DomainValidator（领域校验器）、ProtocolPlan、协议Core或PoC（Proof of Concept，概念验证）。

## 3. 环境

| 项目 | 实际值 |
| --- | --- |
| 操作系统 | Microsoft Windows 10 专业版，10.0.19045 |
| CPU | AMD Ryzen 7 5800H with Radeon Graphics，16 logical processors（逻辑处理器） |
| CMake | 4.4.3 |
| Generator（生成器） | Visual Studio 18 2026，x64 |
| MSVC | 19.51.36256.0 |
| Toolset（工具集） | 14.51.36231 |
| 构建配置 | Release |

本轮实际执行了重新Configure（配置）、增量Release Build（发布构建）和完整CTest。该结果不是Clean Build（干净构建）时间证据。

## 4. 严格性门禁结果

### 4.1 汇总

| 候选 | 用例 | 非预期失败 | JSON Pointer精确断言 | `EXACT_INPUT_BYTE`用例 | 已知契约缺口 |
| --- | ---: | ---: | ---: | ---: | ---: |
| yyjson 0.12.0 | 100 | 0 | 19 | 14 | 2 |
| nlohmann/json 3.12.0 | 100 | 0 | 19 | 14 | 2 |
| RapidJSON 1.1.0 | 100 | 0 | 19 | 14 | 2 |

最终Runner摘要：

```text
SUMMARY candidate=yyjson cases=100 failures=0 known_contract_gaps=2
SUMMARY candidate=nlohmann_json cases=100 failures=0 known_contract_gaps=2
SUMMARY candidate=rapidjson cases=100 failures=0 known_contract_gaps=2
```

### 4.2 Unicode代理项预检

扩展用例后，RapidJSON 1.1.0原生路径接受了单独低代理项`\uDC00`，而另外两个候选拒绝。PAE不能因此把“严格JSON”完全委托给候选Parser。

公共预检新增一个只读、无堆、窄范围的Unicode代理项转义检查：

- 高代理项必须紧跟低代理项；
- 单独低代理项拒绝；
- 反向组合拒绝；
- 被`\\`转义为普通文本的`uDC00`不得误判；
- 合法代理项对和`U+10FFFF`继续接受。

补足后，三个候选对相关用例给出相同类别；这证明当前PAE Adapter边界行为对齐，不代表RapidJSON原生严格性发生变化。

### 4.3 Number Token已知缺口

本轮新增`INT64_MIN - 1`后，三个候选首次运行都返回：

```text
expected=INTEGER_OUT_OF_RANGE
actual=INTEGER_NOT_EXACT
```

结合既有`UINT64_MAX + 1`用例，可以确认当前Adapter主要依据Parser DOM（Document Object Model，文档对象模型）数值类型反推整数类别；越界整数被候选转成浮点或“大数字”类别后，已经无法与`1.0`、`1e0`可靠区分。

生产契约目标为：

- 小数或指数误入整数属性：`INTEGER_NOT_EXACT`；
- 词法上是整数、但超出目标整数范围：`INTEGER_OUT_OF_RANGE`。

因此Runner将两项改为明确的characterization case（现状表征用例），同时输出目标类别和`known_contract_gaps=2`。这不是降低契约要求；正式Loader必须保留原始数字Token类别或等价信息后，删除缺口标记并把目标类别变为普通通过条件。

## 5. CTest与Allocator证据

实际命令：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release
ctest --preset windows-msvc-spike-release --output-on-failure
```

最终结果：

```text
100% tests passed out of 10
Total Test time (real) = 0.28 sec
```

yyjson allocator（内存分配器）自测最终摘要：

```text
zero=RESOURCE_EXHAUSTED
exhausted=RESOURCE_EXHAUSTED
normal=OK
normal_attempts=2
first_failure=RESOURCE_EXHAUSTED
second_failure=RESOURCE_EXHAUSTED
preflight=INPUT_LIMIT
syntax=SYNTAX_ERROR
stress_attempts=5
stress_reallocs=3
injection_cases=5
injection_not_reached=OK
pass=true
```

这仍只约束yyjson reader使用的固定pool（内存池），不覆盖后续SchemaIr、路径、重复键集合或Plan staging（计划暂存区）的总临时内存。

## 6. Benchmark

### 6.1 方法

- 每个候选顺序执行3轮；
- small：76 B输入，每轮1,000,000次；
- field_dense：66,533 B、5,309 nodes（节点）、最大Array 1,708，每轮200次；
- 表中取3轮中位数；
- 没有固定CPU亲和性、关闭后台任务或建立统计置信区间。

因此以下只用于同机同构建下的候选相对比较，不是PAE Core吞吐、SLA（Service Level Agreement，服务级别协议）或实时性结论。

### 6.2 中位数

| 候选 | small ns/parse | small MiB/s | field_dense ns/parse | field_dense MiB/s | dense相对yyjson耗时 |
| --- | ---: | ---: | ---: | ---: | ---: |
| yyjson | 868.26 | 83.48 | 655,491.00 | 96.80 | 1.00× |
| nlohmann/json | 3,594.87 | 20.16 | 17,478,514.00 | 3.63 | 26.66× |
| RapidJSON | 1,579.49 | 45.89 | 937,609.50 | 67.67 | 1.43× |

公共Unicode预检改变了实际解析路径，因此第二轮报告中的旧耗时不再作为当前实现数值。单次非隔离Benchmark的细小变化不能解释为稳定性能回归或提升。

## 7. Release产物

| 文件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `pae_json_spike_yyjson.exe` | 179,200 B | `48d2852d63c19e6ace8f266ad076aa46af5278115a40d414904167b50ab26929` |
| `pae_json_spike_nlohmann.exe` | 168,448 B | `00342943e2162f17ded31e2d8548e3da6c5a6a9cff64bbb8ab3d069de16b3f81` |
| `pae_json_spike_rapidjson.exe` | 101,376 B | `80152083f1079dd4cd28caba1240fbcd6c22daa5168cfc281c9456b89985d619` |

SHA-256只标识本轮本机Release输出，不构成可复现构建声明。

## 8. 当前选择判断

yyjson继续作为唯一首选候选，理由没有变化：

- 严格行为可由PAE公共预检和Adapter边界补齐；
- 当前解析性能和字段密集型性能优于两个对照候选；
- C源码集成、固定pool和故障注入已经形成Windows证据；
- 第三方类型可以保持在Loader私有边界。

本轮不锁定最终Parser，原因是以下门禁仍未完成：

1. Linux GCC和Clang同语料实测；
2. 原始Number Token保留及两个越界分类缺口关闭；
3. 正式Vendor源文件范围、License文件和本地修改记录冻结；
4. 与正式Schema容量自洽的生产Loader资源数值；
5. 独立机器可读Strict JSON Conformance Corpus（一致性语料）；
6. 所有层级重复key的稳定完整路径；
7. Loader/SchemaIr/PlanBuilder整体内存失败和事务性交付验证。

## 9. 证据边界

本轮可以证明：

- 当前Windows/MSVC Release工程能够重新配置、构建并运行10项CTest；
- 三个候选在100个当前定向用例上没有非预期行为差异；
- PAE公共预检能够补足已观察到的孤立低代理项差异；
- 两个Number Token契约缺口被机器摘要持续暴露；
- yyjson解析期固定pool和故障注入门禁保持通过。

本轮不能证明：

- Linux、目标板或其他编译器行为；
- 正式生产Loader、Schema、ProtocolPlan或协议Core已经实现；
- 任一真实协议、PoC Golden Vector、硬件或现场正确性；
- Core性能、热路径零分配或资源Profile已经达到目标；
- 最终Parser和第三方源码再分发范围已经获批。
