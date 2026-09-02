# Windows MSVC 第十三轮数字与 Parser 资源门禁报告

## 1. 结论

截至 2026-09-01，JSON Parser（JSON 解析器）Spike（技术探针）已在 Windows x64/MSVC Release 下把第十三轮确认的 Number Token（数字词法单元）语义和三档资源边界转成可执行门禁。

实际结果：

- 独立 Number Token 语料 77/77 通过，全部为`CONFORMANCE（符合性）`，`contract_closure_state=CLOSED`；
- yyjson Raw Number（原始数字）到 Structural Diagnostic（结构诊断）的 10/10 项端到端自测通过；
- Hard、Desktop、Constrained 三档各执行 8 个维度的 exact/+1（恰好到上限/超限一级）输入，共 48 项，全部通过；
- 每档 Parser arena（解析器固定内存区）的容量充分、硬边界和耗尽清理 3 项检查全部通过；
- 三个 Parser 候选各 100 个文档级用例均无回归；yyjson 达到并锁定 3 个数字目标，另外两个 DOM（Document Object Model，文档对象模型）Adapter 继续显式保留 3 个原始数字保真缺口；
- CTest（CMake Test，CMake 测试驱动）20/20 通过。

本轮证据范围是`PARSER_PREFLIGHT_YYJSON_BOUNDED_TREE_AUDIT（公共预检 + yyjson 有界树审计）`。资源 Profile（资源档位）继续标记为`CANDIDATE_UNVERIFIED（候选、尚未生产验证）`，不是生产容量冻结，也不是 Loader/Compiler（加载器/编译器）全链内存证据。

## 2. 数字契约

### 2.1 独立 Token 语料

77 项语料覆盖：

- `UINT64`和`INT64`的 JSON 数字语法、边界、溢出和精确值；
- unsigned（无符号）整数拒绝所有带负号 Token，包括`-0`，稳定原因为`NEGATIVE_TOKEN_FOR_UNSIGNED`；
- signed integer（有符号整数）的`-0`接受并规范化为整数零；
- `REAL64`仅接受有限 IEEE 754 binary64（二进制 64 位浮点数）；
- 精确零 Token，包括`0e999`、`0e-4000`和`-0.0`，规范化为正零；
- 非零 overflow（上溢）与 underflow（下溢）分别稳定报告`OVERFLOW`和`UNDERFLOW`；
- 最大有限值、最小 normal（正规数）、最小非零 subnormal（次正规数）及 round-to-nearest-ties-to-even（舍入到最近值、正中时取偶数）的代表位模式；
- REAL Token 用于整数属性时返回`INTEGER_NOT_EXACT`；
- Token 长度超限返回`NUMBER_TOKEN_LIMIT`。

机器摘要：

```text
NUMBER_TOKEN_SUMMARY cases=77 conformance_cases=77 open_decision_cases=0
  regression_failures=0 regression_gate=PASS closure_gate=PASS
  contract_closure_state=CLOSED
```

这里的`CLOSED`只属于独立数字 Token 子契约，不能解释为完整 Strict JSON Profile（严格 JSON 工作规范）、生产 Parser 或 Loader/Compiler 已闭合。

### 2.2 yyjson 结构诊断

yyjson 的 10 项端到端自测验证 Raw Number 经过 PAE 自有分类和转换后，能够形成稳定 code、reason（原因）和 JSON Pointer（JSON 路径指针）。机器摘要：

```text
NUMBER_CONTRACT_SUMMARY cases=10 failures=0 gate=PASS
  scope=YYJSON_RAW_NUMBER_TO_STRUCTURAL_DIAGNOSTIC
  production_parser_selection=OPEN linux_gate=DEFERRED
```

三个文档级候选的最终摘要为：

```text
yyjson        ready_to_promote=3 open_contract_gaps=0 target_lock_failures=0
nlohmann/json ready_to_promote=0 open_contract_gaps=3
RapidJSON     ready_to_promote=0 open_contract_gaps=3
```

三个 Runner 均为`regression_gate=PASS`，但由于仍有`CHARACTERIZATION（现状表征）`，文档级`contract_closure_state=OPEN`。

## 3. 三档 Parser 阶段资源门禁

### 3.1 候选容量

| 项目 | Hard | Desktop | Constrained | 当前证据 |
| --- | ---: | ---: | ---: | --- |
| 输入字节 | 4 MiB | 2 MiB | 512 KiB | exact/+1 通过，候选未冻结 |
| 最大深度 | 64 | 32 | 16 | exact/+1 通过，结构上限已确认 |
| 逻辑 DOM 节点 | 524,288 | 262,144 | 32,768 | exact/+1 通过，候选未冻结 |
| 单 Object 成员 | 4,096 | 2,048 | 256 | exact/+1 通过，结构上限已确认 |
| 单字符串解码字节 | 64 KiB | 16 KiB | 4 KiB | exact/+1 通过，结构上限已确认 |
| 单 Array 元素 | 16,384 | 8,192 | 1,024 | exact/+1 通过，结构上限已确认 |
| Number Token 字节 | 128 B | 64 B | 64 B | exact/+1 通过，结构上限已确认 |
| 全部解码字符串 | 2 MiB | 1 MiB | 256 KiB | exact/+1 通过，候选未冻结 |
| Parser arena | 64 MiB | 32 MiB | 8 MiB | 容量/硬边界/清理通过，候选未冻结 |
| Loader/Compiler 峰值 | 128 MiB | 64 MiB | 16 MiB | `NOT_MEASURED` |

生成器使用稳定标识：

```text
pae-json-resource-v0.1/<profile>/<dimension>/<boundary>
```

每档 8 个维度各有 exact 和 over 两项，共`8 × 2 × 3 = 48`个生成输入。树审计超限用例同时断言稳定 JSON Pointer 存在，输入预检超限断言 Pointer 缺省；Number Token 根级超限另断言 Pointer 精确为根。当前 Generator ID（生成器标识）、参数和实际字节数由运行输出记录，但尚未冻结生成结果 SHA-256（Secure Hash Algorithm 256-bit，256 位安全散列算法），所以`hash_manifest_state=OPEN`。

### 3.2 Parser arena 证据

对每档最大输入，yyjson API（Application Programming Interface，应用程序编程接口）报告的保守内存上界均低于候选 arena：

| Profile | 最大输入 | yyjson API 上界/实际预留 | 候选 arena |
| --- | ---: | ---: | ---: |
| Hard | 4,194,304 B | 54,526,208 B | 67,108,864 B |
| Desktop | 2,097,152 B | 27,263,232 B | 33,554,432 B |
| Constrained | 524,288 B | 6,816,000 B | 8,388,608 B |

三档分别通过：

```text
ARENA_CAP_SUFFICIENT
ARENA_HARD_BOUND_ENFORCED
ARENA_EXHAUSTION_CLEANUP_PASS
```

这只统计 yyjson allocator（内存分配器）的逻辑请求和固定池预留，不包含输入生成器、`std::string`、重复 key 集合、JSON Pointer、SchemaIr、校验索引或 Plan staging（计划暂存区）。

### 3.3 机器摘要边界

三档摘要均为：

```text
status=CANDIDATE_UNVERIFIED
scope=PARSER_PREFLIGHT_YYJSON_BOUNDED_TREE_AUDIT
structural_profile_gate=PASS
candidate_capacity_probe=PASS
capacity_freeze_state=OPEN
hash_manifest_state=OPEN
loader_compiler_peak_bytes=NOT_MEASURED
schema_ir=NOT_BUILT
plan_bundle=NOT_BUILT
runtime_registration=NOT_RUN
linux_gate=DEFERRED
failures=0
```

## 4. Windows 实测证据

### 4.1 工具链和命令

```text
CMake 4.4.3
Visual Studio 18 2026 / MSBuild 18.9.1
Windows SDK 10.0.22621.0
x64 Release
```

最终实际执行：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release --clean-first --parallel
ctest --preset windows-msvc-spike-release --output-on-failure
```

结果：

```text
100% tests passed, 0 tests failed out of 20
Total Test time (real) = 0.73 sec
```

最终证据来自 Clean Build（干净构建）。Windows 验收 Preset 显式固定`PAE_JSON_SPIKE_CANDIDATE=all`，避免既有 CMake Cache（配置缓存）把 yyjson 数字/资源门禁整体省略；独立 Number Token Runner 同时把`closure_gate=PASS`作为退出条件。格式化后首次增量构建曾暴露两个`.h.in`模板的 CMake 占位符被拆分；补加局部`clang-format off/on`保护后，最终重新 Configure、干净 Build 和 CTest 均通过。

## 5. 未关闭边界和下一步

仍未关闭：

- 最终生产 Parser 选型及正式 Vendor（随仓库管理）边界；
- 正式`pae.schema.json`和 ProtocolPlan Execution Semantics（执行语义）；
- 正式 PAE Schema 最大 Field/Message 组合及生成输入 SHA-256；
- Loader/Compiler 全链峰值、SchemaIr、ResourceBudget、PlanBundle和失败原子性；
- 72 个源码内嵌文档用例、嵌套重复 key 和完整共享 Conformance Corpus（一致性语料）；
- Linux GCC/Clang、Sanitizer（运行期检测器）、Core、PoC（Proof of Concept，概念验证）、目标板、真实硬件和现场协议验证。

下一步推荐进入最小`pae.schema.json + Loader/SchemaIr`垂直切片：先用从零设计的最小双向人工消息证明正式配置可以严格加载、形成可检查的不可变 Plan，并测量全链临时内存；在此之前不应把本报告中的候选容量升级为生产上限。
