# JSON Parser Spike：Windows/MSVC 首轮证据报告

> 本文是首轮历史快照。解析期内存和诊断门禁的当前状态以[第二轮门禁报告](windows-msvc-2026-parser-gate.md)为准。

## 1. 结论与状态

本轮在同一套严格 JSON 包装层、定向语料和 Windows x64/MSVC Release 构建下，对 yyjson 0.12.0、nlohmann/json 3.12.0 和 RapidJSON 1.1.0 完成了首轮比较。

当前结论是：

- 三个候选各执行 53 个合法、非法、资源边界和合成协议形态用例，均为 0 失败；
- CTest（CMake 测试驱动）同时验证每个候选拒绝负数和超量 Benchmark（基准测试）迭代参数，最终 9/9 通过；
- yyjson 在本轮两个输入 Profile（档位）中均最快，并保持 C 源码集成边界，继续作为首选候选；
- RapidJSON 的可执行文件最小，性能居中，可保留为备选；
- nlohmann/json 的接口易用，但本轮字段密集型输入的包装层耗时明显更高，单头文件源码范围也最大；
- 最终 Parser（解析器）选型仍为 `OPEN（未锁定）`。解析期内存 Hard Limit（硬上限）、稳定 JSON Pointer（JSON 路径指针）诊断和 Linux GCC/Clang 验证尚未完成。

本报告只证明可丢弃 Spike 在当前 Windows 软件环境中的行为，不证明 PAE Core（协议适配引擎核心）、正式 Schema（配置结构规范）、PoC（Proof of Concept，概念验证）协议、目标板、硬件或现场正确性。

## 2. 实验环境

| 项目 | 实测值 |
| --- | --- |
| 日期 | 2026-09-01 |
| OS（Operating System，操作系统） | Windows 10 专业版 10.0.19045，Build 19045 |
| CPU（Central Processing Unit，中央处理器） | AMD Ryzen 7 5800H，8 Core（核心）/16 Logical Processor（逻辑处理器），MaxClockSpeed 3201 MHz |
| 物理内存 | 16,487,866,368 B |
| CMake | 4.4.3 |
| Generator（生成器） | Visual Studio 18 2026，x64 |
| MSVC | 19.51.36256.0，Toolset 14.51.36231 |
| Windows SDK | 10.0.22621.0 |
| clang-format | 22.1.3 |
| Git | 2.55.0.windows.5 |
| 构建类型 | Release |

本机当前没有可用的 WSL（Windows Subsystem for Linux，适用于 Linux 的 Windows 子系统）发行版，因此本轮没有伪造 Linux 构建结论。

## 3. 候选依赖锁定输入

候选版本、Commit、归档 SHA-256、License、候选源码范围和文件 SHA-256 记录在`../dependencies.lock.json`。候选源码由 CMake FetchContent 下载到被 Git 忽略的`out/`，本轮没有把三方候选直接 Vendor（随仓库管理）到`third_party/`。

| 候选 | Commit | 归档 SHA-256 | 首轮候选源码范围 |
| --- | --- | --- | --- |
| yyjson 0.12.0 | `8b4a38dc994a110abaec8a400615567bd996105f` | `a3bc9626ec0ba8bcc0644cc5355b3cf6eeda3188caf72bd7caa0c305acc78e79` | `src/yyjson.h` + `src/yyjson.c`，合计 733,220 B；MIT License |
| nlohmann/json 3.12.0 | `55f93686c01528224f448c19128836e7df245f72` | `13ef31d691947940a08909f8e0772f1d7d68e5da1678ee812a49c4bb0c996b2f` | `single_include/nlohmann/json.hpp`，953,436 B；MIT License |
| RapidJSON 1.1.0 | `f54b0e47a08782a6131cc3d60f94d038fa6e0a51` | `4a76453d36770c9628d7d175a2e9baccbfbd2169ced44f0cb72e86c5f5f2f7cd` | `include/rapidjson`，35 个文件、590,075 B；所需范围按 MIT 审查，归档内其他三方材料仍需单独排除 |

上述是 Spike 候选输入，不是生产依赖锁定结果。最终若选择 yyjson，仍需单独确认 Vendor 文件清单、NOTICE/License 交付和更新流程。

## 4. 测试方法

三个候选使用相同的公共预检、Fixture（固定测试输入）、标准化错误码和统计模型：

```text
输入字节
→ 文件大小/BOM/UTF-8/容器深度预检
→ 候选 Parser 构造 DOM（Document Object Model，文档对象模型）
→ 重复 key 与节点/String/Array/深度审计
→ 最小结构和领域探针
→ 标准化结果
```

默认实验上限：

| 限制项 | 值 |
| --- | ---: |
| 输入大小 | 256 KiB |
| 最大深度 | 64 |
| 最大节点数 | 4,096 |
| 最大 String | 4,096 B |
| 最大 Array | 1,024 项 |

字段密集型合成输入仅为本用例提高节点数至 20,000、Array 至 4,096，以容纳 1,708 个字段形态。该调整是实验参数，不是未来 Loader（加载器）正式契约。

53 个用例覆盖：

- 最小合法 JSON、空输入、BOM、多个非法 UTF-8 形态；
- 注释、尾随逗号、`NaN`、无穷大、第二文档、孤立 surrogate（代理项）、单引号、未加引号 key、十六进制、前导正号和前导零；
- 根级、嵌套、转义等价和内嵌 NUL 的重复 key；
- `INT64_MIN/MAX`、`UINT64_MAX`、正向溢出、负数误入无符号字段、小数/指数误入整数字段；
- 未知字段、缺失字段、类型错误、错误引用和跨字段语义约束的正反用例；
- 文件大小、深度、节点数、String 和 Array 的边界值及超限一级；
- COMPLETE_RECORD（完整记录）、固定 9 字节形态和 1,708 字段密集型 Stream（字节流）三个合成配置骨架。

这些是面向 PAE 风险的定向样本，不是完整 RFC 8259 或 JSON Conformance Suite（JSON 符合性套件）。三个 PoC 骨架目前位于探针`payload`中，只验证 Parser 和资源形态，不是正式`pae.schema.json`，也不是 Golden Vector（黄金测试向量）。

## 5. 功能结果

最终实际命令：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release --parallel
ctest --preset windows-msvc-spike-release --output-on-failure
```

最终结果：

| 项目 | yyjson | nlohmann/json | RapidJSON |
| --- | ---: | ---: | ---: |
| 定向用例 | 53/53 | 53/53 | 53/53 |
| 失败数 | 0 | 0 | 0 |
| 负数迭代参数拒绝 | 通过 | 通过 | 通过 |
| 超过 10,000,000 次迭代参数拒绝 | 通过 | 通过 | 通过 |

CTest 最终为 9/9 通过，总耗时 0.28 s。

实验早期曾因`depth_exact`把“容器深度”和“叶值深度”混为同一个边界，导致三个候选同时失败。该问题属于 Fixture 定义错误；修正边界定义后才重新执行所有候选。本报告只采用修正后的最终结果。

## 6. Benchmark 结果

Benchmark 包含公共预检、Parser、重复 key/资源审计、最小结构校验和结果统计，不是候选库裸解析微基准。

- `small`：76 B，1,000,000 次；
- `field_dense`：66,533 B、1,708 字段形态，200 次；
- 每个候选独立运行 3 次，表中为三次中位数；
- Working Set（进程工作集）由另一次相同负载进程采样并结合 OS Peak 值取得，只能用于当前进程级近似比较。

| 候选 | small ns/parse | small MiB/s | field_dense ns/parse | field_dense MiB/s | 采样/OS Peak Working Set | EXE 体积 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| yyjson | 675.04 | 107.37 | 547,626.0 | 115.87 | 4,640,768 B | 146,432 B |
| nlohmann/json | 3,565.04 | 20.33 | 16,975,255.5 | 3.74 | 5,267,456 B | 145,920 B |
| RapidJSON | 1,419.36 | 51.06 | 778,047.5 | 81.55 | 4,837,376 B | 77,312 B |

相对本轮 yyjson 中位数：RapidJSON 的 small 耗时约 2.10 倍、field_dense 约 1.42 倍；nlohmann/json 分别约 5.28 倍和 31.00 倍。这个结果只适用于当前包装层、编译器、机器和合成输入，不能外推为正式 PAE 吞吐。

最终可执行文件 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| `pae_json_spike_yyjson.exe` | `9b969ad12d52ecd8c25b078b49d8ee51eef56a58e9f17887d575e2f7ea48a93b` |
| `pae_json_spike_nlohmann.exe` | `12606d0e54a73e7a30d1b7edd786dd50b05abd1423733b1b555f9dc0a3c85cf8` |
| `pae_json_spike_rapidjson.exe` | `43b60fe8d4f55e8a78deb6878859c34e2f87a355b37c74857f2aa05a6ed641ed` |

## 7. Clean Build 单次样本

每个候选使用新的独立 Build Tree（构建树），`BUILD_TESTING=OFF`，并通过 FetchContent 从锁定归档获取源码。Configure（配置）时间包含编译器探测、网络下载和解压，因此不是纯 Parser 配置成本；以下只是一轮样本。

| 候选 | Configure | Release Build |
| --- | ---: | ---: |
| yyjson | 45.218 s | 12.559 s |
| nlohmann/json | 33.400 s | 6.417 s |
| RapidJSON | 30.530 s | 4.184 s |

yyjson Build 还包含`yyjson.c`的独立 C 静态库编译。上述时间没有重复统计，不应作为稳定 CI（Continuous Integration，持续集成）预算。

## 8. 已知缺口和选型门禁

以下缺口阻止本轮直接锁定最终 Parser：

1. 当前节点、String 和 Array 上限在 DOM 构造完成后审计，尚未给 Parser 分配器施加硬上限；只能证明“解析后拒绝超限结构”，不能证明解析峰值内存有界。
2. 已把`std::bad_alloc`及 yyjson 的内存分配失败映射为`RESOURCE_EXHAUSTED`，但尚未做故障注入，不能宣称该路径已验证。
3. 结构/领域错误尚无稳定 JSON Pointer；当前`byte_offset`主要来自语法错误，三个候选对个别错误的 offset 含义存在差异，需先冻结“违规首字节/停止位置”等语义。
4. Linux x86_64/GCC 和 Clang preset 已定义，但当前没有 Linux 环境实际 Configure、Build、CTest 和诊断一致性证据。
5. 负向`INT64_MIN - 1`的统一分类、更多非法 UTF-8/数字词法和独立 JSON 符合性语料仍可补强。
6. 通用 Draft 2020-12 参考 Validator 尚未选定；当前领域探针不是正式 PAE Structural/Domain Validator。
7. Working Set 包含进程基线和标准库/运行库，不是 Parser allocator 的精确 Peak Bytes（峰值字节数）。
8. 本轮没有 PAE Core 热路径、正式协议 Decode/Encode、Golden Vector、Sanitizer（运行期检测器）、ARM、目标板、硬件或现场数据。

## 9. 首轮建议

yyjson 继续作为 V0.1 Loader 的首选候选，推荐下一步只围绕 yyjson 补齐以下门禁，同时保留 RapidJSON 作为对照：

1. 设计受限 allocator 或等效解析期内存上限，并执行分配失败注入；
2. 冻结稳定错误路径和 byte offset 契约，至少为结构/领域错误提供 JSON Pointer；
3. 在真实 Linux GCC/Clang 环境执行同一套用例和诊断对照；
4. 引入许可证清晰的独立严格 JSON 语料子集；
5. 门禁通过后，再单独拍板最终版本、`yyjson.h/yyjson.c/LICENSE`文件范围和 Vendor 方式。

在这些事项完成前，状态保持：`INITIAL WINDOWS PASS / FINAL SELECTION OPEN（Windows 首轮通过／最终选型未锁定）`。
