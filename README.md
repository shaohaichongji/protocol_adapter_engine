# ProtocolAdapterEngine

ProtocolAdapterEngine（PAE，协议适配引擎）当前处于 V0.1 Internal MVP（内部最小可用版本）的工程骨架、技术探针和首个内部可执行纵向切片阶段。

PAE 的目标是通过严格配置完成工业二进制协议的有方向 Decode（解析）、Encode（组包）、Framing（切帧）、Integrity（完整性校验）和 Receive Gate（接收门禁）。它不拥有串口、Socket、CAN、IPC、线程、设备生命周期、重试恢复、UI 或业务状态机。

## 当前状态

2026-09-09总控收口：[固定完整记录长度字段](docs/length-field-minimal-contract.md)已在当前
工作树完成Schema 0.7、Compiler/Builder、Core、Lab 0.8/Record 0.9、公开向量及业务嵌入实现。
初次Windows Debug/Release离线矩阵各39/39、业务嵌入各1/1及两类0测试隔离构建通过；
P2修复后受影响离线集合各4/4，最后CLI补测各1/1，未重跑完整矩阵。总控定向复核已关闭
P2及验收缺口，51个候选与文档链接检查完成；A组已本地提交为`0179abc`，B组入口文档待提交，尚未Push。
未执行网络；详见[验证报告](docs/windows-msvc-2026-length-field-slice.md)。
更早批次“最新/待提交”叙述保留其历史时点。

2026-09-08最新收口：C2复现/比较、C3专用Schema 0.5离线CLI及Windows中文路径修复已提交并Push，
代码基线`df43165`。用户确认以代理七项离线验收、35项精确检查PASS收口DEC-042B C3离线功能
检查点；用户人工验收仍为NOT_EVALUATED，不阻塞本检查点。见
[代理验收报告](docs/agent-dec042b-c3-offline-acceptance.md)。当前工作树已按确认契约完成默认关闭的
[最小业务嵌入示例](examples/business_embedding/README.md)及Windows离线验证，待总控审查；它不扩展
Core/Lab，不升级完整V0.1、Oracle、Linux、Golden、硬件、现场或生产准入状态。
以下批次记录保留历史时点，其“尚未实现/提交/Push”不代表最新状态。

2026-09-08 C2第一段实施：默认关闭的C执行RUN Evidence 0.7隔离入口已实现真实阶段事件、
事务Writer、严格Reader及完整失败记录；P2修复后Windows Debug/Release专项各2/2、相关离线集
各5/5通过。完整离线切片各22/22及两类0测试隔离构建属于P2前验证批次，本次未重跑。
详见[C2第一段报告](docs/windows-msvc-2026-dec042b-lab-v07-run-evidence-stage-c2-first.md)。
实现及直接契约已通过总控限定复核，并随A组`d7b6e97`本地提交，尚未Push。
本阶段未实现Replay/Compare/Plan关联、普通CLI或网络。以下记录保留其历史时点。

2026-09-08历史记录（C2实施授权前）：C1实现`115db10`及入口文档`ef350d5`已提交并Push。
C2六项方向决策已确认：C执行Record/Event使用0.7，Values 0.4、Result/指纹0.6及B合成0.6保持不变。
完整失败记录、真实阶段事件、分层Reader与两段实施边界见
[契约第18～19节](docs/pae-dec-042b-decimal-conversion-contract-draft.md)。修订后的精确契约已确认，C2/C3未实施。
CLI退出码映射已确认延至C3实施前冻结；第20节已整理第一段执行范围，待单独授权后派发。
本次仅同步Markdown，未运行测试或执行Git写操作。下列实施回报保留当时状态。

2026-09-08 C1实施补记：B实现`70cf4ff`及入口文档`761ff7f`已提交并Push。
C阶段12项决策已确认；默认关闭的Schema 0.5执行桥接C1现已完成限定实现及Windows
Debug/Release隔离验证，见[C1报告](docs/windows-msvc-2026-dec042b-lab-v06-execution-stage-c1.md)。
C1审查纠错后，Schema 0.5可使用0.1～0.3 Values表达各代原有类型，0.4仍由A阶段严格解析；
展示Decode的真实`INTERNAL_ERROR`也已通过Core测试故障接缝覆盖。
C1不读写Evidence、不提供CLI、不执行Replay/Compare或网络；C2/C3仍未实施，事件细节仍待
C2前冻结。当前变更待总控复核，未Stage、Commit或Push。下述早期批次状态保留。

2026-09-08补记：A实现`8d4c7c4`、入口文档`0c4bc48`及yyjson整理`1c0617c`已提交并Push。
B阶段已在默认关闭的隔离目标中实现Evidence 0.6事务写入与严格读取，并完成Windows
Debug/Release限定验证，见[B阶段报告](docs/windows-msvc-2026-dec042b-lab-v06-evidence-stage-b.md)；
尚未接入普通CLI、Core或Replay/Compare。历史生成物清理暂缓，未删除。

配套工具方向已确认：保留CLI并增加独立Qt UI，Qt不进入Core依赖链；UI尚未实现，
安排在当前Lab第三段之后。暂不复制现有Qt包，先核验依赖并选择版本。
Lab指纹0.6精确编码与A阶段纯格式隔离测试方案已完成限定实现和Windows隔离验证，见
[DEC-042B契约第12～13节](docs/pae-dec-042b-decimal-conversion-contract-draft.md)。
Config Compiler、Protocol Lab及yyjson Spike现统一使用随仓的yyjson 0.12.0最小源码与唯一锁；
正常Configure不再通过网络获取yyjson，见[依赖验证报告](docs/windows-msvc-2026-yyjson-vendor-dependency.md)。

2026-09-07状态快照：公开代码基线为`90c5165`，DEC-042B Core第二段已审查、提交并Push。
Schema 0.2位字段、0.3 SUM8、0.4 INT64已实现；0.5精确Decimal64比例/偏置仅在专用Loader+Core
开关下执行，默认关闭，仍不能与Protocol Lab共存。Lab第三段A纯格式模块已在默认关闭的独立测试
目标中实现；B证据读写和C运行链仍未实现，普通Lab仍只支持Values 0.3及Result/Record/Event 0.5，
见[DEC-042B契约第10～13节](docs/pae-dec-042b-decimal-conversion-contract-draft.md)。
Core纠错后Windows专用矩阵Debug/Release各16/16、旧代全切片各25/25；最后仅补raw生命周期测试，
专项各1/1。它们是不同批次和构建组合的既有证据，本次Markdown同步没有重跑，详见
[Core报告](docs/windows-msvc-2026-dec042b-core-slice.md)。下方早期切片叙述和30/30等数字保留为
历史证据，不表示当前最新全量注册数。Values最高0.3、Lab结果/记录/事件最高0.5；新代仍未接通。

- 已创建本地工程骨架；
- 已完成 JSON Parser Spike（JSON 解析器技术探针）的 Windows/MSVC（Microsoft Visual C++，微软C/C++编译器）Strict JSON Profile（严格 JSON 子集规范）、Number Token（数字词法单元）、两批机器语料及三档Parser阶段资源门禁；28项文档清单、77项Token语料、10项yyjson数字诊断、48项资源边界、精确offset/Pointer、5项清单负向门禁，以及yyjson原始数字、解析期内存硬上限、故障注入和候选来源/License复核已有实测证据；
- 已正式采用yyjson 0.12.0固定Commit作为Compiler和Lab内部依赖，仅随仓`LICENSE`、`yyjson.h`和`yyjson.c`；Config阶段逐文件复核长度及SHA-256，缺失或漂移失败关闭，Core和ProtocolPlan仍不依赖JSON Parser；
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

构建目录及nlohmann/json、RapidJSON实验候选下载位于`out/`，不进入Git。正式yyjson最小源码和
MIT License位于`third_party/yyjson`。远端发布必须先通过协议资料保密、仓库可见性、第三方
License和提交内容审计门禁。

## License 状态

当前仓库尚未选择或授予开源License（许可证）。仓库公开可见不等于允许复制、修改、分发或用于商业项目；正式开源前将单独完成License拍板及第三方Notice（声明）整理。
