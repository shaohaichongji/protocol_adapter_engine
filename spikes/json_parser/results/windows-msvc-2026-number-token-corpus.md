# Windows MSVC Number Token 与机器可读语料门禁报告

## 1. 结论

截至 2026-09-01，本轮在可丢弃 JSON Parser（JSON 解析器）Spike（技术探针）中完成了原始 Number Token（数字词法单元）验证和第一批机器可读语料迁移。

实际结果为：

- 新增的单 Token 语料共 55 项，其中 54 项 `CONFORMANCE（符合性）`、1 项 `OPEN_DECISION（待拍板）`；55 项的词法类别、`negative`、`negative_zero`、fraction、exponent、转换状态和精确值回归门禁均通过；
- 三个候选继续各执行 100 个文档级用例，稳定 ID 与来源清单门禁为 100/100，均为 `regression_failures=0 / regression_gate=PASS`；
- yyjson 0.12.0 使用 `YYJSON_READ_NUMBER_AS_RAW`后，`UINT64_MAX + 1`和`INT64_MIN - 1`均返回目标类别 `INTEGER_OUT_OF_RANGE`，摘要为 `ready_to_promote=2 / open_contract_gaps=0`；
- 上述两个 yyjson 目标行为已经增加 Candidate Target Lock（候选目标锁），任一行为回退到旧分类都会使 yyjson CTest 失败；
- yyjson 端到端 Raw Number 自测覆盖 1 个稳定结构 Token 和 7 个代表性 Token；结构字段`1`及`-0`、`1.0`、`1e0`、`1E+007`、`UINT64_MAX + 1`、`0e999`和`1e999`共45个原始词法字节，经7个NUL分隔后形成52字节比较串并逐字节一致；
- nlohmann/json 3.12.0 和 RapidJSON 1.1.0 保持既有表征结果，均为 `ready_to_promote=0 / open_contract_gaps=2`；
- yyjson 固定内存池及全部 5 个解析期分配点逐点故障注入继续通过；
- Windows x64、MSVC Release 的 CTest（CMake Test，CMake 测试驱动）回归门禁为 11/11 通过；由于仍有`CHARACTERIZATION`和`OPEN_DECISION`，Contract Closure State（契约闭合状态）保持`OPEN`。该状态当前只用于摘要和人工审查，不单独决定 CTest 退出码。

这使 yyjson 的两个 Number Token 分类缺口在推荐路径上得到关闭，但不等于最终生产 Parser 已经锁定，也不等于完整 Strict JSON Conformance Corpus（一致性语料）、正式 Loader（加载器）或协议 Core 已经完成。

## 2. 实现范围

### 2.1 原始数字保留

yyjson Adapter 使用同一个 `kReadFlags = YYJSON_READ_NUMBER_AS_RAW`同时调用：

```text
yyjson_read_max_memory_usage()
yyjson_read_opts()
```

每个数字节点通过 `yyjson_get_raw() + yyjson_get_len()`取得 Parser 文档生命周期内的原始字节，再交给 PAE 自有的单 Token 分类器。分类器：

- 按 RFC 8259 数字语法完整消费 Token；
- 区分 `UNSIGNED_INTEGER`、`SIGNED_INTEGER`、`REAL`和`INVALID`；
- 保留 `negative_zero`、fraction（小数部分）和 exponent（指数部分）信息；
- 使用受检查的 `uint64_t`逐位累积，不经 `double`中转；
- 对 `INT64_MIN`单独处理 magnitude（绝对值）为 `2^63`的边界；
- 先验证完整词法，再判范围，因此溢出数字后带非法后缀不会误报为范围错误。

端到端逐字节自测只在定向门禁中开启`capture_number_lexemes`，把1个稳定结构Token和7个代表性Token的45个原始词法字节以7个NUL分隔后，与显式的52字节期望串比较；普通文档用例和Benchmark不捕获该字符串，避免把自测分配与复制混入候选相对性能结果。这个捕获字段属于Spike私有诊断，不是生产Loader接口。

分类器位于 Spike 私有目录，当前接口使用 C++17 `std::string_view`。它不是生产公共 API，也没有替代后续 C++17 Core 的正式内部类型设计。

### 2.2 机器可读语料

当前新增：

- `fixtures/limits_v0.1.tsv`；
- `fixtures/strict_json_corpus_v0.1.tsv`；
- `fixtures/strict_json_inventory_v0.1.tsv`；
- `fixtures/number_token_corpus_v0.1.tsv`；
- `fixtures/number_token_inventory_v0.1.tsv`。

清单使用固定列 TSV（Tab-Separated Values，制表符分隔值），原始输入和 JSON Pointer（JSON 路径指针）使用大写 Hex（十六进制）。候选 Parser 不读取清单；CMake 4.4.3 在 Configure（配置）阶段以独立逻辑校验表头、列数、ID、枚举、引用、Hex、规范十进制、输入长度、offset 组合、状态组合及 Characterization 目标差异，再生成只读 C++ 记录。这避免让被测 JSON Parser 解析自己的测试清单。

100 个文档级用例保持原有 `case_id`总量，其中 14 个 Number Token 用例已从 C++ `BuildFixtures()`迁入 TSV；其余 86 个仍由 C++ 生成。独立 Inventory（清单）冻结了 100 个稳定 ID 及其`LEGACY_EMBEDDED / MANIFEST_AUTHORITY`来源状态，防止迁移时静默漏项；它不把 86 个内嵌用例升格为独立语料权威。55 个单 Token 用例也由独立 ID 清单防删减。当前仍只能称为“首批机器可读语料切片”，不能称为完整生产 Corpus。

状态分开统计：

```text
CONFORMANCE      规范性结果，不匹配即失败
CHARACTERIZATION 当前候选行为与契约目标并列记录
OPEN_DECISION    稳定记录当前行为，但不替用户拍板生产语义
```

CTest 当前执行 Regression Gate（回归门禁）：表征用例命中旧行为或目标行为、待拍板用例保持记录值时均可通过。逐用例另输出`CONFORMANT / OBSERVED_STABLE / TARGET_REACHED / OPEN_STABLE / UNEXPECTED_CHANGE`；只要回归门禁失败，或仍有`CHARACTERIZATION`或`OPEN_DECISION`，`contract_closure_state=OPEN`。yyjson 已达到的两个目标另有专属 Target Lock，不允许回退。

`-0`作为 unsigned（无符号）属性输入时，词法分类明确为 `SIGNED_INTEGER + negative_zero=true`；当前 Spike 转换结果仍为 0，但语料状态是 `OPEN_DECISION`，生产是否接受它仍未冻结。

## 3. Windows 实测证据

### 3.1 工具链

```text
CMake 4.4.3
Visual Studio 18 2026 / MSBuild 18.9.1.35102
Windows SDK 10.0.22621.0
x64 Release
```

执行入口：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release --parallel
ctest --preset windows-msvc-spike-release --output-on-failure
```

CTest 实际结果：

```text
100% tests passed, 0 tests failed out of 11
```

11 项包括：独立 Number Token 语料、三个候选主用例、三个负 benchmark 参数拒绝、三个过大 benchmark 参数拒绝，以及 yyjson allocator（内存分配器）门禁。yyjson 主用例内同时执行端到端 Raw Number 逐字节自测和两个 Target Lock。

### 3.2 结果摘要

```text
NUMBER_TOKEN_SUMMARY cases=55 conformance_cases=54 open_decision_cases=1
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN

RAW_NUMBER_SELF_TEST candidate=yyjson expected_tokens=8 actual_tokens=8
  expected_bytes=52 actual_bytes=52 parser_code=OK
  lexemes_preserved=true byte_for_byte=true regression_gate_passed=true

SUMMARY candidate=yyjson cases=100 inventory_cases=100 inventory_gate=PASS
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN
  conformance_cases=97 conformance_failures=0
  characterization_cases=2 characterization_changes=0
  ready_to_promote=2 open_contract_gaps=0
  target_locked_cases=2 target_lock_failures=0
  raw_number_self_test_cases=1 raw_number_self_test_failures=0
  open_decision_cases=1 open_decision_changes=0

SUMMARY candidate=nlohmann_json cases=100 inventory_cases=100 inventory_gate=PASS
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN
  conformance_cases=97 conformance_failures=0
  characterization_cases=2 characterization_changes=0
  ready_to_promote=0 open_contract_gaps=2
  target_locked_cases=0 target_lock_failures=0
  open_decision_cases=1 open_decision_changes=0

SUMMARY candidate=rapidjson cases=100 inventory_cases=100 inventory_gate=PASS
  regression_failures=0 regression_gate=PASS contract_closure_state=OPEN
  conformance_cases=97 conformance_failures=0
  characterization_cases=2 characterization_changes=0
  ready_to_promote=0 open_contract_gaps=2
  target_locked_cases=0 target_lock_failures=0
  open_decision_cases=1 open_decision_changes=0
```

yyjson 文档审计同时输出 `number_lexemes_preserved=true`；另外两个现有 DOM（Document Object Model，文档对象模型）Adapter 输出 `false`。

### 3.3 yyjson allocator 门禁

启用 Raw Number 后实际结果仍为：

```text
zero=RESOURCE_EXHAUSTED
exhausted=RESOURCE_EXHAUSTED
exhausted_reserved=64
normal=OK
normal_reserved=1244
normal_upper_bound=1244
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

这证明当前 Spike 固定池容量拒绝、未分配预检、第一次/第二次分配失败、含 realloc（重新分配）的 5 个解析期分配点逐点注入及释放归零门禁没有被 Raw Number 模式破坏。它不证明正式 Loader 的资源上限已经冻结。

## 4. 相对 Benchmark

同一增量 Release 构建、同一机器、每候选顺序执行 5 轮；每轮 small profile 200,000 次，field-dense profile 40 次。以下为中位数：

| 候选 | small ns/parse | small MiB/s | field-dense ns/parse | field-dense MiB/s |
| --- | ---: | ---: | ---: | ---: |
| yyjson 0.12.0 Raw Number | 1,005.01 | 72.12 | 750,860.00 | 84.50 |
| nlohmann/json 3.12.0 | 3,576.81 | 20.26 | 16,761,110.00 | 3.79 |
| RapidJSON 1.1.0 | 1,629.23 | 44.49 | 947,462.50 | 66.97 |

本轮数据只用于候选相对比较。运行中出现过单轮系统噪声，采用中位数降低影响；没有隔离 CPU、没有做冷/热缓存分离，也没有采用最终生产 Loader、Schema 或 ProtocolPlan，因此不能作为 PAE Core 性能承诺。

## 5. 构建产物快照

| 文件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `pae_json_number_token_corpus.exe` | 40,448 B | `d2125c1aecbf508d84cc41fa5538054789dfd33458d742e137379f2354199dea` |
| `pae_json_spike_yyjson.exe` | 211,456 B | `2f79fa93c1877dca95431bf9141d5e21eacad71d837e638696fbd7d386da0176` |
| `pae_json_spike_nlohmann.exe` | 199,168 B | `d2ee27fdd254bdf5994121df76b41486831f143a475d034ed0d699c0fc034338` |
| `pae_json_spike_rapidjson.exe` | 131,584 B | `11231bd4775e6778c13975ce69c37148ffe6a11830e8e1ff0ebf21ce10b44007` |

这些 Hash 只对应当前本机构建产物，不是依赖源码 Hash 或发布制品签名。

## 6. 尚未关闭的边界

- 最终 JSON Parser、正式 Vendor 源文件范围和 License 交付尚未锁定；
- Linux GCC/Clang、Sanitizer（运行期检测器）和目标设备尚未执行；
- `-0`的 unsigned 属性生产语义仍为 `OPEN`；
- REAL64、`0e999`、`1e999`等超大实数的 Loader/Structural 归属和公共表示仍为 `OPEN`；
- Number Token 最大字节数和其他正式 Loader 资源数值尚未冻结；
- 其余 86 个文档级用例尚未迁入独立语料；生成型资源边界尚未固定 Generator ID、输出大小和 SHA-256；
- 既有公共预检的精确 byte offset 尚未全部迁入独立清单断言；
- 所有层级 duplicate key（重复键）的稳定完整 JSON Pointer 仍未完成；
- 当前 Probe Schema 校验仍位于候选 Adapter，正式 `pae.schema.json`、StructuralValidator、DomainValidator 和 ProtocolPlan 尚未实现；
- 没有执行协议 PoC、Golden Vector、硬件或现场验证。

## 7. 下一阶段建议

推荐下一步不进入报文 Core，而是完成 Parser 决策前的最后一组高价值门禁：

1. 在 Linux GCC 和 Clang 上运行同一 11 项 CTest；
2. 把静态 Strict JSON、UTF-8、重复 key 和精确 offset 用例继续迁入机器可读 Corpus；
3. 冻结 Number Token 最大字节数及 Loader 输入、深度、节点、字符串、数组和 arena 数值；
4. 决定 `-0`和超大 REAL Token 的生产语义；
5. 完成 yyjson 正式 Vendor/License 边界和 Parser 私有类型隔离门禁；
6. 再单独拍板最终 Parser，随后进入最小 `pae.schema.json + ProtocolPlan Execution Semantics`。

在这些事项完成前，yyjson 可以继续表述为“唯一首选候选”，不能表述为“生产依赖已经最终冻结”。
