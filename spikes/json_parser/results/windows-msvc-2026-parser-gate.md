# JSON Parser Spike：Windows/MSVC 第二轮门禁报告

> 本文是 56 用例阶段的历史证据快照，不回写后续结果。当前状态和 100 用例证据见[Strict JSON Profile 门禁报告](windows-msvc-2026-strict-profile.md)。

## 1. 结论与状态

本轮在[首轮证据报告](windows-msvc-2026-initial.md)基础上，补充了 yyjson 解析期内存硬上限、逐分配点故障注入和稳定诊断路径，并在相同 Windows x64/MSVC（Microsoft Visual C++ Compiler，Microsoft Visual C++ 编译器）Release 环境重新执行三个 Parser（解析器）候选。

实际结果：

- yyjson 0.12.0、nlohmann/json 3.12.0 和 RapidJSON 1.1.0 各执行 56 个定向用例，均为 0 失败；
- 19 个用例对 RFC（Request for Comments，请求评议） 6901 JSON Pointer（JSON 路径指针）进行精确断言，三个候选均通过；
- yyjson 固定容量 pool（内存池）和无堆代理 allocator（内存分配器）通过容量拒绝及 fail-on-N（第 N 次分配失败）自测；
- CTest（CMake Test，CMake 测试驱动）新增 yyjson allocator 专属门禁后为 10/10 通过；
- yyjson 加入硬上限和分配跟踪后，仍是本轮两个输入 Profile（档位）中最快的候选；
- 当前状态为`WINDOWS PARSER GATE PASS / LINUX GATE OPEN / FINAL SELECTION OPEN（Windows 解析器门禁通过／Linux 门禁未完成／最终选型未锁定）`。

最终选择仍保持`OPEN`，原因不是 Windows 结果不足，而是当前主机没有可用 Linux 环境，尚未取得 Linux GCC/Clang 的实际 Configure（配置）、Build（构建）、CTest 和诊断一致性证据。生产 Vendor（源码随仓库管理）范围和 License 文件交付也尚未单独拍板。

本报告只覆盖可丢弃的 JSON Parser Spike（JSON 解析器技术探针）。它不证明正式 PAE Core、Schema（配置结构规范）、PoC（Proof of Concept，概念验证）协议、协议编解码、目标板、硬件或现场正确性。

## 2. 本轮实现边界

### 2.1 yyjson 解析期内存门禁

当前 yyjson 包装层采用：

```text
Limits.max_parser_memory_bytes
→ 分配固定容量 backing buffer（后备缓冲区）
→ yyjson_alc_pool_init()
→ 不分配堆内存的 proxy allocator（代理分配器）
→ yyjson_read_opts()
→ 文档销毁
→ 校验 live blocks/live requested bytes 均回到 0
```

硬上限由固定 backing buffer 的实际容量兑现。代理层记录 malloc（分配）、realloc（重新分配）、free（释放）、活跃请求字节和故障原因，并允许指定第 N 次分配失败。

对`YYJSON_READ_NOFLAG`，包装层调用 yyjson 自带的`yyjson_read_max_memory_usage()`计算输入对应的保守上界，再取该上界与实验策略上限的较小值作为固定池容量。当前实验参数为：

| 项目 | 值 |
|---|---:|
| 最大输入 | 256 KiB |
| yyjson 实验解析池上限 | 4 MiB |
| 256 KiB 输入的 yyjson API 上界 | 3,408,128 B |
| 76 B 最小合法输入的实际固定池 | 1,244 B |
| 76 B 最小合法输入的代理请求峰值 | 400 B |
| 66,533 B 字段密集输入的实际固定池 | 865,185 B |
| 66,533 B 字段密集输入的代理请求峰值 | 244,073 B |

“代理请求峰值”只统计 yyjson 向 allocator 请求的逻辑字节，不包含 pool 对齐、chunk 元数据和碎片；真正的不可突破边界是固定池容量。该边界只覆盖 yyjson 的输入副本和 DOM（Document Object Model，文档对象模型），不覆盖宿主输入、Fixture 构造、JSON Pointer 字符串或解析后的领域审计容器。因此它是`Parser arena hard limit（解析器内存区硬上限）`证据，不是整个 Loader 进程内存的全局硬上限。

### 2.2 故障注入

yyjson 专属自测实际观察到：

| 场景 | 结果 |
|---|---|
| 策略上限为 0 B | `RESOURCE_EXHAUSTED` |
| 固定池为 64 B | `RESOURCE_EXHAUSTED` |
| 最小合法输入正常解析 | `OK`，2 个分配点 |
| 第 1 个分配点失败 | `RESOURCE_EXHAUSTED`，释放后活跃块为 0 |
| 第 2 个分配点失败 | `RESOURCE_EXHAUSTED`，释放后活跃块为 0 |
| 2,048 元素压力输入正常解析 | `OK`，5 个分配点，其中 3 次 realloc |
| 压力输入第 1～5 个分配点逐点失败 | 5/5 均为`RESOURCE_EXHAUSTED`，释放后活跃块为 0 |
| 注入点设为第 6 次而实际只有 5 次 | `OK`，未误触发 |
| 输入预检先失败 | 保持`INPUT_LIMIT`，allocator 尝试数为 0 |
| 容量充足的非法 JSON | 保持`SYNTAX_ERROR`，未命中的注入点不改变分类 |

allocator ledger（分配记录表）采用固定 8 个槽位，自身不在回调中申请堆内存。未知指针、旧大小不匹配或记录槽耗尽被归类为`INTERNAL_ERROR`，不能伪装为资源耗尽。

### 2.3 诊断定位

本轮新增[JSON Loader 诊断定位契约 V0.1](../../../docs/json_loader_diagnostic_contract_v0.1.md)：

- byte offset（字节偏移）统一为零基；
- 公共 BOM、UTF-8 和深度预检使用`EXACT_INPUT_BYTE`；
- 第三方 Parser 的语法位置标记为`PARSER_REPORTED_POSITION`，不伪称三个候选数值完全相同；
- 结构、整数和领域错误使用 RFC 6901 JSON Pointer；
- 根节点使用空字符串，文本报告显示为`<root>`；
- 明确验证`~ → ~0`和`/ → ~1`；
- Parser 资源耗尽不携带不稳定的 byte offset 或 JSON Pointer。

56 个用例中有 19 个精确路径断言，覆盖根节点、重复根键、未知/缺失字段、类型、整数精度与范围、引用、跨字段约束、数组路径、字符串/数组上限和 token 转义。

当前跨候选门禁尚未要求所有嵌套 duplicate key（重复键）都返回完整路径；这是诊断草案继续保留的明确缺口。

## 3. 实验环境与命令

环境沿用首轮：Windows 10 10.0.19045、AMD Ryzen 7 5800H、CMake 4.4.3、Visual Studio 18 2026 x64 Generator（生成器）、MSVC 19.51.36256.0、Toolset 14.51.36231、Windows SDK 10.0.22621.0、Release 构建。

实际执行入口：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release
ctest --preset windows-msvc-spike-release --output-on-failure
```

最终 CTest 为 10/10 通过。三个候选的公共 CTest 分别执行 56 个定向用例和 Benchmark 参数边界；yyjson 另执行 allocator 专属自测。

## 4. 更新后的 Benchmark

本轮 Benchmark 包含公共预检、Parser、重复键/资源审计、最小结构校验、诊断对象，以及 yyjson 每次解析时的固定池创建和代理分配统计。它不是候选库裸解析微基准，也不是正式 PAE 吞吐。

- `small`：76 B，1,000,000 次；
- `field_dense`：66,533 B、1,708 字段形态，200 次；
- 每个候选独立运行 3 次，表中为按`ns/parse`排序后的中位样本；
- 同一中位样本的 MiB/s（Mebibytes per second，每秒二进制兆字节）一并报告。

| 候选 | small ns/parse | small MiB/s | field_dense ns/parse | field_dense MiB/s |
|---|---:|---:|---:|---:|
| yyjson | 795.08 | 91.16 | 607,201.0 | 104.50 |
| nlohmann/json | 3,729.30 | 19.44 | 19,560,109.5 | 3.24 |
| RapidJSON | 1,540.03 | 47.06 | 863,963.5 | 73.44 |

相对当前 yyjson 中位数：RapidJSON 的 small 耗时约 1.94 倍、field_dense 约 1.42 倍；nlohmann/json 分别约 4.69 倍和 32.21 倍。

相对首轮 yyjson 中位数，加入固定池、代理统计和 JSON Pointer 路径维护后，small 从 675.04 ns 增至 795.08 ns，field_dense 从 547,626.0 ns 增至 607,201.0 ns。两轮包装层已经不同，因此该变化只能作为实现成本样本，不能解释为纯 allocator 单项开销。

本轮没有重新采样进程 Working Set（工作集）；首轮 Working Set 数据不能代表当前二进制。yyjson 的固定池容量和代理请求峰值是更窄、更可解释的 Parser 内存证据，但不等于整个进程峰值。

## 5. 当前二进制证据

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `pae_json_spike_yyjson.exe` | 166,400 B | `35a0c42e7e6739b37b1008f89c415d5ff3412e3488ea5896f51bc95d2a0f18be` |
| `pae_json_spike_nlohmann.exe` | 156,160 B | `f177d2f3fe84ad2fd2ac7fbcafedb2b9dc5d3e7fda8fc0348eff880bb68bfbfa` |
| `pae_json_spike_rapidjson.exe` | 88,576 B | `fe2a7ab3a4545c1209dde631c91e2ba55e785a62ee70e326a5190e21bd18730d` |

SHA-256 只标识本轮 Release 输出，不是可复现构建声明。

## 6. Linux 环境核查

本轮只做只读环境核查，没有安装 WSL、Docker、Podman 或编译器：

- `wsl.exe`存在，但 WSL（Windows Subsystem for Linux，适用于 Linux 的 Windows 子系统）没有已安装发行版；`wsl --list --verbose`返回退出码 1 和安装提示；
- Docker、Podman、Bash、Clang 和 Clang++当前不可用；
- 当时PATH中的`gcc/g++`来自Strawberry Perl环境，不能视为Linux或已验证MinGW工具链；
- Ninja 可用，但没有 Linux 编译器和运行环境，不能据此生成 Linux 构建证据。

因此 Linux preset 虽已定义，本轮没有执行 Linux Configure、Build 或 CTest，也没有以 Windows 编译器结果冒充跨平台验证。

## 7. 门禁判断与下一步

yyjson 继续作为唯一首选候选，Windows 侧以下原有缺口已关闭：

- 解析期固定内存池硬上限；
- 分配失败注入和释放后活跃块检查；
- 结构/领域错误的稳定 JSON Pointer；
- byte offset 来源类型区分；
- 加入上述机制后的重新构建、测试和 Benchmark。

仍阻止最终依赖锁定的事项：

1. 在真实 Linux x86_64/GCC 和 Clang 环境执行相同用例、allocator 自测和诊断断言；
2. 明确正式 Vendor 文件范围、License/NOTICE 交付和更新流程；
3. 使用许可证清晰的独立严格 JSON 语料补强当前定向用例；
4. 在正式 Loader 设计中冻结输入、节点、深度、字符串、数组和 Parser arena 的产品级数值；
5. 决定是否为所有嵌套重复键补齐完整 JSON Pointer，以及是否增加跨 Parser 的统一语法定位器。

在 Linux 环境具备前，不建议通过安装行为扩大本轮授权；下一步可以先在当前 Windows 仓库中整理严格 JSON Profile（严格 JSON 子集规范）和 Loader/Validator 边界，或由用户明确提供可用 Linux 执行环境后继续关闭跨平台门禁。
