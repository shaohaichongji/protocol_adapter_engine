# PAE Frozen Execution Plan Windows 内部切片验证报告

## 1. 结论

截至2026-09-02，PAE（Protocol Adapter Engine，协议适配引擎）已经完成`PAE-DEC-031`所定义范围内的首个Frozen Execution Plan（冻结执行计划）内部实现切片，并在Windows x64、VS（Visual Studio）18 2026与MSVC（Microsoft Visual C++，微软C/C++编译器）下完成实际配置、构建和测试。

本次实现证明当前`COMPLETE_RECORD（完整记录）`子集可以在初始化阶段冻结Plan并预分配Workspace，Decode/Encode逐帧只消费执行描述符，同时保留输入检查、Matcher唯一性、alias（内存重叠）、跨Plan引用、最终复核和fail-closed（失败关闭）语义。它不是完整Runtime（运行时）、Session（会话）、稳定公共API（应用程序接口）或正式性能结论。

## 2. 实现范围

- `PlanBundle`构造器私有化，并明确删除复制和移动；第十六轮后`PlanBuilder::Freeze`只消费不可伪造的`BudgetedPlanDraft（已预算计划草案）`，原始草案载荷不再属于生产入口；
- 冻结前重新计算`ResourceRequirements（资源需求）`，不信任草案汇总值，并按Desktop/Constrained Profile（桌面/受限资源档）检查帧、Message、Field、Matcher、Enum和当前Workspace估算；失败不交付部分Plan；
- 生成按Message组织的Field/固定字节/Enum辅助索引、按Pipeline组织的允许Message位图与Frame Length候选组，以及`ExecutionResourceLayout（执行资源布局）`；
- `ExecutionWorkspace`永久绑定并借用一个Plan，初始化时分配Encode稠密值索引与存在位图；Plan必须比Workspace存活更久；
- Decode按Frame Length二分定位候选组，Enum按Raw Value排序辅助表二分查找；Encode一次扫描输入并按Field ordinal（字段序号）索引，不再重复线性查找；
- 同一Workspace并发或重入复用以一次非自旋原子门禁返回`WORKSPACE_BUSY`；不同Workspace可以并发共享同一只读Plan；
- 产品Core不保存或清零操作计数；只有`PAE_BUILD_TESTING=ON`时生成测试专用instrumented（带计数）Core；
- PAE作为CMake子项目嵌入时，`PAE_BUILD_TESTING`默认关闭，不继承宿主的`BUILD_TESTING=ON`。

本切片没有新增`STREAM_CHUNK（流式字节块）`、CRC（Cyclic Redundancy Check，循环冗余校验）、BCD（Binary-Coded Decimal，二进制编码十进制）、Mapping（映射）、C ABI（C应用二进制接口）、完整Runtime/Session或生产协议字段。

## 3. 实际验证环境

| 项目 | 实际值 |
| --- | --- |
| CMake | 4.4.3 |
| Generator（生成器） | Visual Studio 18 2026，x64 |
| MSVC工具集 | 14.51.36231 |
| C/C++ Compiler（编译器） | MSVC 19.51.36256 |
| MSBuild | 18.9.1 |
| 验证平台 | Windows x64 |

## 4. 实际执行与结果

以下命令均在仓库根目录执行：

```powershell
$cmakeExe = 'cmake'
$ctestExe = 'ctest'

& $cmakeExe --preset windows-msvc-codec-slice
& $cmakeExe --build out/build/windows-msvc-codec-slice --config Release --parallel
& $ctestExe --test-dir out/build/windows-msvc-codec-slice -C Release --output-on-failure
& $cmakeExe --build out/build/windows-msvc-codec-slice --config Debug --parallel
& $ctestExe --test-dir out/build/windows-msvc-codec-slice -C Debug --output-on-failure
```

| 门禁 | Release | Debug |
| --- | --- | --- |
| Codec配置构建与CTest | 构建成功，`6/6` | 构建成功，`6/6` |
| Config Compiler合同Runner | `28/28` | `28/28` |
| COMPLETE_RECORD主合同Runner | `60/60` | `60/60` |
| 首次Decode分配门禁 | `1/1` | `1/1` |
| 首次Encode分配门禁 | `1/1` | `1/1` |
| 操作计数规模门禁 | `4/4` | `4/4` |
| 共享Plan并发门禁 | `2/2` | `2/2` |

首次公开推送前，仓库将实现证据衍生向量替换为完全独立的`synthetic_lab_exchange`人工协议；13/11字节记录、固定标识、Little Endian（小端）、三字节整数、BYTES、ENUM和全部动态值均为`SYNTHETIC_FROM_SCRATCH（从零人工设计）`。替换后重新执行Release/Debug构建与CTest以及两个直接Runner，仍分别得到`6/6`、Config Compiler `22/22`和Codec主合同`60/60`；这些结果只继承引擎能力结论，不继承任何旧协议布局证据。

另以所有Parser候选、Loader和Codec同时启用的全新Release构建目录执行：

```powershell
& $cmakeExe -S . -B out/build/windows-msvc-all-slices-frozen -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_JSON_PARSER_SPIKE=ON -DPAE_JSON_SPIKE_CANDIDATE=all -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON
& $cmakeExe --build out/build/windows-msvc-all-slices-frozen --config Release --parallel
& $ctestExe --test-dir out/build/windows-msvc-all-slices-frozen -C Release --output-on-failure
```

结果为配置和构建成功，CTest `26/26`通过；公开人工向量替换后在`out/build/windows-msvc-all-slices`再次干净构建并复验，仍为`26/26`。第十六轮能力链实现后在`out/build/windows-msvc-all-slices-dec032`执行Release和Debug共存回归，两种配置均为`26/26`；DEC-033A落地及第十八轮提交前复核后，Config Compiler增至`28/28`，全切片Release/Debug CTest仍分别为`26/26`。最新内存准入证据见`docs/windows-msvc-2026-accounted-plan-memory-slice.md`。

## 5. 专项门禁

### 5.1 首次调用与逐帧分配

首次Decode/Encode用例在Plan和Workspace构造完成后、第一次调用Codec前开启replaceable `operator new/new[]`（可替换全局new/new[]）计数，并分别得到`1/1`通过。CMake配置阶段另扫描`src/protocol_core`，拒绝直接调用`malloc/calloc/realloc/free`；本次静态扫描零命中。

该结果只说明覆盖路径未观察到上述C++分配入口，且当前Core源码没有直接C堆调用；它不覆盖自定义Allocator、第三方间接分配、宿主回调、未来扩展或所有平台的全部分配通道。

### 5.2 操作计数

测试专用instrumented Core实际覆盖：

- 16、64、256字段时的输入、字段、写入/复核和存在位图访问；
- 8、32个Frame Length候选组的二分查找步数；
- 8、128字节固定Matcher和BYTES写入/复核；
- Enum Raw Value二分查找及稳定配置Entry索引。

四项门禁`4/4`通过，且`static_plan_validation_visits`保持为零。这是算法访问次数随输入规模增长的结构性证据，不是吞吐、延迟、CPU占用、WCET（Worst-Case Execution Time，最坏情况执行时间）或正式Benchmark（基准测试）结果。

### 5.3 并发

Windows压力用例以4个线程、每线程4096次Decode/Encode共享同一`const PlanBundle`，每个线程使用独立Workspace、Slot和输出Buffer，结果通过；另一个用例确定性验证同一Workspace被占用时返回`WORKSPACE_BUSY`。这属于当前Windows观察证据，不替代TSan（ThreadSanitizer，线程数据竞争检测器）、形式化证明、其他编译器或目标设备验证。

### 5.4 产品和宿主CMake边界

实际创建全新`out/build/windows-msvc-codec-product-only`，以`BUILD_TESTING=OFF`、`PAE_BUILD_TESTING=OFF`和Codec切片开启进行Release构建；生成工程只有产品Plan/Core及CMake自身辅助项目，没有Config Compiler、yyjson、instrumented Core或PAE测试Runner。

另创建临时宿主工程，设置宿主`BUILD_TESTING=ON`后通过`add_subdirectory`嵌入PAE，但不显式设置`PAE_BUILD_TESTING`。配置确认PAE测试开关默认关闭，Release产品Core构建成功，工程图没有Config Compiler、yyjson、instrumented Core或PAE测试Runner；宿主自身仍可生成`RUN_TESTS`等通用CTest辅助目标。临时宿主`CMakeLists.txt`验证后已删除，忽略目录`out/`内只保留构建产物。

### 5.5 格式与静态扫描

- 对本切片涉及的`protocol_plan`、`protocol_core`、`config_compiler`和对应测试C++文件执行`clang-format --dry-run --Werror`，通过；
- `ValidatePipeline`、`ValidateMessageLayout`、`MessageDefinesWholeFrame`和`FindEncodeValue`在`src/protocol_core`零命中；
- `malloc/calloc/realloc/free`直接调用在`src/protocol_core`零命中。

## 6. 尚未闭合的DEC-031边界

当前实现是`PARTIALLY VERIFIED（部分已验证）`，不能写成DEC-031全部完成：

1. `PAE-DEC-032`不可伪造的`Validated/Budgeted Draft（已验证/已预算草案）`能力边界已经实现并通过Windows门禁；Builder保留的重复检查现在只作为内部不变量审计，任何命中均映射为`INTERNAL_CONTRACT_VIOLATION（内部契约违规）`，不再形成第二套作者错误入口。专项证据见`docs/windows-msvc-2026-validated-budgeted-capability-chain.md`。
2. `PAE-DEC-033A`已经实现冷Plan中对象、字符串、Matcher、元数据容器、执行描述符、索引和对齐的精确计费与单Plan准入；`PAE-DEC-033B`仍未闭合，尚没有Runtime全部活跃Plan/Session的聚合准入、并发预留和销毁退款。4 MiB JSON入口限制仍不能替代未来Runtime总内存预算。
3. 稳定Runtime `slot + generation（槽位+代次）`Handle、ExtensionCatalog身份、Allocator注入、完整Session所有权和公共状态/API仍未实现。

## 7. 验证边界

本轮未执行或不能证明：

- Linux GCC/Clang、ASan/UBSan/TSan（地址、未定义行为、线程检测器）和其他CPU架构；
- 正式吞吐、p50/p95/p99/max延迟、CPU占用、峰值内存或目标设备低占用；
- 完整V0.1字段、Framing、Integrity、Gate、Mapping、Runtime、Session和C ABI；
- 由协议权威或真实抓包支撑的正式Golden Vector（黄金测试向量）；
- 真实串口/TCP/UDP设备、硬件和现场行为。

两条现有Synthetic Engine Vector只证明当前人工构造的引擎合同，不得据此修改生产协议实现或声明设备互通。当前仓库仍是不可安装、不可导出的Windows-first（Windows优先）内部API切片。本报告中的验证执行发生在首次Commit（提交）和Push（推送）之前；实际发布状态以Git历史和远端分支为准。
