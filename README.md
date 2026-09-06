# ProtocolAdapterEngine

ProtocolAdapterEngine（PAE，协议适配引擎）当前处于 V0.1 Internal MVP（内部最小可用版本）的工程骨架、技术探针和首个内部可执行纵向切片阶段。

PAE 的目标是通过严格配置完成工业二进制协议的有方向 Decode（解析）、Encode（组包）、Framing（切帧）、Integrity（完整性校验）和 Receive Gate（接收门禁）。它不拥有串口、Socket、CAN、IPC、线程、设备生命周期、重试恢复、UI 或业务状态机。

## 当前状态

- 已创建本地工程骨架；
- 已完成 JSON Parser Spike（JSON 解析器技术探针）的 Windows/MSVC（Microsoft Visual C++，微软C/C++编译器）Strict JSON Profile（严格 JSON 子集规范）、Number Token（数字词法单元）、两批机器语料及三档Parser阶段资源门禁；28项文档清单、77项Token语料、10项yyjson数字诊断、48项资源边界、精确offset/Pointer、5项清单负向门禁，以及yyjson原始数字、解析期内存硬上限、故障注入和候选来源/License复核已有实测证据；
- 已实现内部 `StrictJsonLoader → StructuralValidator / SchemaIrBuilder → DomainValidator → ResourceBudgetValidator → PlanDraftAssembler → PlanBuilder` 最小纵向切片，可把受限 `*.pae.json` 编译为不可变 `PlanBundle`；该切片不安装、不导出，不属于稳定公共 API（应用程序编程接口）；
- 已形成草案 `pae.schema.json`、ProtocolPlan Execution Semantics（协议计划执行语义）、从零设计的人工实验台双向样例和稳定 ID Golden Snapshot（黄金快照）；
- 已把 yyjson-free（不依赖yyjson）的`pae_protocol_plan`从配置编译器抽离，并实现首个内部`COMPLETE_RECORD（完整记录）`Codec（编解码器）切片：支持确定性Matcher（匹配器）、`UINT64`、固定`BYTES`、`ENUM`、1～8字节大小端、Plan作用域引用、调用方Buffer及失败关闭；
- 已完成`PAE-DEC-031`首个Frozen Execution Plan（冻结执行计划）内部实现切片：`PlanBundle`只能由`PlanBuilder`冻结构造且不可复制/移动，Plan同时携带候选组、字段、固定字节、Enum辅助索引及Workspace资源布局；Decode/Encode显式接收与Plan绑定的`ExecutionWorkspace（执行工作区）`，正常逐帧路径不再执行静态Plan深度校验或重复字段线性查找；
- 已完成`PAE-DEC-032`内部能力链：`ValidatedSchemaIr`、`BudgetedSchemaIr`和`BudgetedPlanDraft`均为私有载荷、move-only（仅移动）的不可默认构造能力类型，移动后重复消费会失败关闭；原始`PlanDraft`已退出生产Builder入口，作者错误只由Domain/Resource Validator（领域/资源校验器）报告，Builder异常统一映射为内部契约违规或分配失败；
- 已完成`PAE-DEC-033A`内部切片：不可变Plan的`FrozenString/FrozenArray`全部存入单一`PlanStorageBlock`，`PlanArena`精确分类计费，`PlanOwner`负责move-only（仅移动）所有权；ResourceBudget（资源预算）在产生Budgeted能力前完成单Plan准入，Builder发布前复算并逐字段匹配批准报告；`PAE-DEC-033B` Runtime全部活跃Plan/Session聚合准入仍未实现；
- 已建立两条`SYNTHETIC_REVIEWED / INDEPENDENT_ENGINEERING_REVIEWED（人工构造 / 独立工程复核）`Engine Vector（引擎向量），其Frame、Decode期望和Encode输入均为独立文件并受SHA-256门禁保护；它们不是正式协议Golden Vector（黄金测试向量）；
- 已实现仅Testing构建的`Protocol Conformance Runner（协议一致性验证运行器）`，覆盖配置与语料绑定、Frame SHA-256、精确双向编解码、重复编码确定性和失败关闭；公开Synthetic CTest及仓库外私有协议语料的Engine PoC（引擎概念验证）已有Windows Release/Debug执行证据，但正式协议Golden门禁仍未满足；
- 已实现并加固`PAE Protocol Lab（PAE协议实验与复现工具）`：独立CLI提供`inspect/encode/replay/compare`和Windows `udp-exchange`，支持严格类型化Values、收发前原始Frame记录、V0.2阶段事件与RX来源Metadata、显式UDP Replay模式、历史Transport/当前执行分离、可校验Evidence Bundle和差异分类；Windows UDP通过内部Winsock Adapter完成同步有界单次请求/响应，自动化仅在Loopback（本机回环）执行；
- 已完成2026-09-06 NetAssist人工Loopback收发、字段核对和关闭工具或端口后的离线回放；本次模拟场景Lab门禁通过，不升级真实协议Golden或硬件结论，详见[人工验收报告](docs/windows-protocol-lab-manual-loopback-acceptance-20260906.md)；
- 尚未形成稳定公共 API、完整 ProtocolPlan、完整 Decode/Encode Runtime（运行时）或生产可用协议引擎；
- Linux门禁按当前Windows-first（Windows优先）顺序暂缓；正式 Core 性能、协议Golden Vector、目标板、硬件或现场验证尚未完成；
- 当前仓库不得直接替换任何生产协议代码。

当前实现证据见[Protocol Lab Windows UDP Exchange验证报告](docs/windows-msvc-2026-protocol-lab-udp-exchange-slice.md)、[Protocol Lab Offline Slice Windows验证报告](docs/windows-msvc-2026-protocol-lab-offline-slice.md)、[Protocol Conformance Runner Windows验证报告](docs/windows-msvc-2026-protocol-conformance-runner.md)、[PAE-DEC-033A Windows验证报告](docs/windows-msvc-2026-accounted-plan-memory-slice.md)、[Validated/Budgeted能力链Windows验证报告](docs/windows-msvc-2026-validated-budgeted-capability-chain.md)和[Frozen Execution Plan Windows内部切片验证报告](docs/windows-msvc-2026-frozen-execution-plan-slice.md)；此前的[COMPLETE_RECORD Codec历史基线报告](docs/windows-msvc-2026-complete-record-codec-slice.md)、[Loader/SchemaIr Windows可执行切片报告](docs/windows-msvc-2026-loader-schema-ir-slice.md)和[第十三轮数字与Parser资源门禁报告](spikes/json_parser/results/windows-msvc-2026-round13-number-resource-gate.md)继续保留。最新Windows x64、MSVC Release/Debug下，配置编译器Runner均`28/28`，Codec合同Runner均`60/60`，首次Decode/Encode分配门禁各`1/1`，操作计数门禁`4/4`，共享Plan并发门禁`2/2`；Parser、Loader、Plan、Codec、Conformance Runner和Protocol Lab共存CTest均`30/30`。上述证据仍不等于完整V0.1、Runtime总量准入、最终生产Parser、正式协议字节正确性、正式性能、Linux兼容、目标板、硬件或现场验证。

## 目录

```text
include/                 未来的公共 C++ 头文件；当前没有稳定公共 API
src/                     当前内部 protocol_plan、config_compiler 与 COMPLETE_RECORD Codec切片
schema/                  JSON Schema 与协议执行语义
tools/                   配置检查和诊断工具
tests/                   正式单元与集成测试
examples/                脱敏示例
third_party/             最终锁定并经审计的源码依赖
spikes/json_parser/      可丢弃的 JSON Parser 对比实验
cmake/                   项目 CMake 辅助模块
docs/                    仓库内通用技术文档
```

真实协议文档、客户/线路信息、生产代码和现场报文默认不进入本仓库 Git 历史。

## 构建 JSON Parser Spike

Windows：

```powershell
cmake --preset windows-msvc-spike
cmake --build --preset windows-msvc-spike-release
ctest --preset windows-msvc-spike-release
```

## 构建 Loader/SchemaIr 纵向切片

Windows：

```powershell
cmake --preset windows-msvc-loader-slice
cmake --build --preset windows-msvc-loader-slice-release
ctest --preset windows-msvc-loader-slice-release
```

## 构建 COMPLETE_RECORD Codec 内部切片

Windows：

```powershell
cmake --preset windows-msvc-codec-slice
cmake --build --preset windows-msvc-codec-slice-release
ctest --preset windows-msvc-codec-slice-release
```

Linux（预留命令；按当前推进顺序暂不执行）：

```bash
cmake --preset linux-gcc-spike
cmake --build --preset linux-gcc-spike-release
ctest --preset linux-gcc-spike-release

cmake --preset linux-clang-spike
cmake --build --preset linux-clang-spike-release
ctest --preset linux-clang-spike-release
```

构建目录和下载的候选依赖位于`out/`，不进入 Git。远端发布必须先通过协议资料保密、仓库可见性、第三方License和提交内容审计门禁。

## License 状态

当前仓库尚未选择或授予开源License（许可证）。仓库公开可见不等于允许复制、修改、分发或用于商业项目；正式开源前将单独完成License拍板及第三方Notice（声明）整理。
