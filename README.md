# ProtocolAdapterEngine

ProtocolAdapterEngine（PAE，协议适配引擎）当前工程版本为 `0.1.0`，仓库内已形成多组可执行的内部能力切片；这不等于完整 V0.1、稳定 SDK/API 或生产可用版本。

PAE 的目标是通过严格配置完成工业二进制协议的有方向 Decode（解析）、Encode（组包）、Framing（切帧）、Integrity（完整性校验）和 Receive Gate（接收门禁）。它不拥有串口、Socket、CAN、IPC、线程、设备生命周期、重试恢复、UI 或业务状态机。

## 当前入口（2026-09-19）

- 直接使用现有 SDK 或 Lab：先打开 [统一本地交付入口](deliverables/README.md)；人类阅读从 [01 项目定位与能力边界](docs/guides/01-项目定位与能力边界.md) 开始，首次体验见 [02 首次运行与 Lab 体验](docs/guides/02-首次运行与Lab体验.md)，源码阅读见 [05 架构与代码阅读](docs/guides/05-架构与代码阅读.md)。交付二进制被 Git 忽略，不随 clone 自动获取。
- 权威 JSON Schema 当前枚举 `0.1`～`0.11`；各版本增量及工具支持边界见 [Schema 导航](schema/README.md)。
- PAE 非 Qt 层已有配置编译/冻结 Plan、完整记录 Decode/Encode、Binary/ASCII 有界 Framing、Integrity 以及 Host 绑定等内部切片；具体契约和验证入口见 [仓库文档索引](docs/README.md)。
- Qt Lab 可观察 ASCII `0.10/0.11` 路径；Binary Host UI 当前只开放 Schema `0.9` 完整记录 Decode。Binary Encode、Submit/Continue 和流式能力已存在于非 Qt 底层，但尚未接入 Binary UI。
- 当前成果定位为限定内部试用。Windows 自动化、人工 Lab、Loopback、Golden、硬件、现场和生产验收是不同证据层级，不能相互替代。
- 仓库内开发构建仍以 [`windows-msvc-pae-lab`](#1-开发树构建)为入口；现有 SDK 候选消费和完整 Qt Lab standalone 构建分别见下方第 2、3 条路径。
- 完整 Qt Lab 已完成基于同一 `98df5e0` clean-checkpoint SDK 的 static/shared Debug/Release 本地仓库外闭包验证；这不是稳定 ABI、Linux、正式分发或人工 UI 验收证据。
- 当前本地检查点与后续授权边界见 [公开执行、独立交付与仓库整理推进计划](docs/engineering/pae-execution-delivery-organization-plan.md)。

## 历史状态快照（保留）

以下内容按当时的提交、验证和授权状态保留，用于追溯；其中“当前”“下一轮”“待提交”等措辞不再代表 2026-09-14 的实时状态。

2026-09-10当前交付基线：`main@4a04afb`已合并并Push，包含有界流式切帧及Lab离线Inspect。
集成Windows Debug/Release各10/10通过；Lab UI仍只接受Schema 0.5～0.7，不支持流式UI。
下一轮[流式宿主接入与Lab Schema 0.8 UI三组八项](docs/engineering/post-stream-host-and-ui-v08-checkpoint.md)
已获整体授权并完成候选实现：PAE宿主示例位于当前工作树，Lab 0.8 UI位于独立工作分支，
均待提交及集成交付；实际自动化与限定人工证据见该契约。尚未执行本轮Git交付。
以下“当前工作树/待提交”等叙述均保留其历史时点，不覆盖上述当前状态。

2026-09-09当前工作树：在已交付基线`7d2ef4f`上完成
[有界变长完整记录](docs/engineering/bounded-variable-record-contract.md)限定实现，新增默认关闭Schema 0.8、
固定头部加单段有界BYTES、frame/payload计算长度、动态SUM8/CRC尾部、Core双向执行、
Result/指纹0.9及Record 0.10离线证据链。公开A5向量、业务宿主和Windows Debug/Release离线
验证见[专项报告](docs/engineering/windows-msvc-2026-bounded-variable-record-slice.md)。空载荷收口新增仅Schema 0.8
可用的Values 0.5显式空BYTES，不放宽旧Values/Result。本工作树尚待总控审查，
未Stage/Commit/Push；不代表流式切帧、Linux、真实协议Golden、设备或现场通过。

2026-09-09总控收口：[固定完整记录长度字段](docs/engineering/length-field-minimal-contract.md)已在当前
工作树完成Schema 0.7、Compiler/Builder、Core、Lab 0.8/Record 0.9、公开向量及业务嵌入实现。
初次Windows Debug/Release离线矩阵各39/39、业务嵌入各1/1及两类0测试隔离构建通过；
P2修复后受影响离线集合各4/4，最后CLI补测各1/1，未重跑完整矩阵。总控定向复核已关闭
P2及验收缺口，51个候选与文档链接检查完成；A组已本地提交为`0179abc`，B组入口文档待提交，尚未Push。
未执行网络；详见[验证报告](docs/engineering/windows-msvc-2026-length-field-slice.md)。
更早批次“最新/待提交”叙述保留其历史时点。

2026-09-08最新收口：C2复现/比较、C3专用Schema 0.5离线CLI及Windows中文路径修复已提交并Push，
代码基线`df43165`。用户确认以代理七项离线验收、35项精确检查PASS收口DEC-042B C3离线功能
检查点；用户人工验收仍为NOT_EVALUATED，不阻塞本检查点。见
[代理验收报告](docs/engineering/agent-dec042b-c3-offline-acceptance.md)。当前工作树已按确认契约完成默认关闭的
[最小业务嵌入示例](examples/business_embedding/README.md)及Windows离线验证，待总控审查；它不扩展
Core/Lab，不升级完整V0.1、Oracle、Linux、Golden、硬件、现场或生产准入状态。
以下批次记录保留历史时点，其“尚未实现/提交/Push”不代表最新状态。

2026-09-08 C2第一段实施：默认关闭的C执行RUN Evidence 0.7隔离入口已实现真实阶段事件、
事务Writer、严格Reader及完整失败记录；P2修复后Windows Debug/Release专项各2/2、相关离线集
各5/5通过。完整离线切片各22/22及两类0测试隔离构建属于P2前验证批次，本次未重跑。
详见[C2第一段报告](docs/engineering/windows-msvc-2026-dec042b-lab-v07-run-evidence-stage-c2-first.md)。
实现及直接契约已通过总控限定复核，并随A组`d7b6e97`本地提交，尚未Push。
本阶段未实现Replay/Compare/Plan关联、普通CLI或网络。以下记录保留其历史时点。

2026-09-08历史记录（C2实施授权前）：C1实现`115db10`及入口文档`ef350d5`已提交并Push。
C2六项方向决策已确认：C执行Record/Event使用0.7，Values 0.4、Result/指纹0.6及B合成0.6保持不变。
完整失败记录、真实阶段事件、分层Reader与两段实施边界见
[契约第18～19节](docs/engineering/pae-dec-042b-decimal-conversion-contract-draft.md)。修订后的精确契约已确认，C2/C3未实施。
CLI退出码映射已确认延至C3实施前冻结；第20节已整理第一段执行范围，待单独授权后派发。
本次仅同步Markdown，未运行测试或执行Git写操作。下列实施回报保留当时状态。

2026-09-08 C1实施补记：B实现`70cf4ff`及入口文档`761ff7f`已提交并Push。
C阶段12项决策已确认；默认关闭的Schema 0.5执行桥接C1现已完成限定实现及Windows
Debug/Release隔离验证，见[C1报告](docs/engineering/windows-msvc-2026-dec042b-lab-v06-execution-stage-c1.md)。
C1审查纠错后，Schema 0.5可使用0.1～0.3 Values表达各代原有类型，0.4仍由A阶段严格解析；
展示Decode的真实`INTERNAL_ERROR`也已通过Core测试故障接缝覆盖。
C1不读写Evidence、不提供CLI、不执行Replay/Compare或网络；C2/C3仍未实施，事件细节仍待
C2前冻结。当前变更待总控复核，未Stage、Commit或Push。下述早期批次状态保留。

2026-09-08补记：A实现`8d4c7c4`、入口文档`0c4bc48`及yyjson整理`1c0617c`已提交并Push。
B阶段已在默认关闭的隔离目标中实现Evidence 0.6事务写入与严格读取，并完成Windows
Debug/Release限定验证，见[B阶段报告](docs/engineering/windows-msvc-2026-dec042b-lab-v06-evidence-stage-b.md)；
尚未接入普通CLI、Core或Replay/Compare。历史生成物清理暂缓，未删除。

配套工具保留CLI并增加独立Qt UI，Qt不进入Core依赖链；PAE-UI-C1已在独立worktree完成
限定实现和Windows验证，已完成Release基础人工离线操作核对，当前进入提交前收口；精确bit掩码人工显示、
编译中关闭竞态及性能阈值仍未获人工验收结论，尚未Stage、
Commit或Push。Qt仅部署到构建目录，不复制进源码树，见[实施契约](docs/engineering/lab-ui-offline-encode-inspector-contract.md)
和[验证报告](docs/engineering/lab-ui-offline-encode-inspector-validation.md)。
Lab指纹0.6精确编码与A阶段纯格式隔离测试方案已完成限定实现和Windows隔离验证，见
[DEC-042B契约第12～13节](docs/engineering/pae-dec-042b-decimal-conversion-contract-draft.md)。
Config Compiler、Protocol Lab及yyjson Spike现统一使用随仓的yyjson 0.12.0最小源码与唯一锁；
正常Configure不再通过网络获取yyjson，见[依赖验证报告](docs/engineering/windows-msvc-2026-yyjson-vendor-dependency.md)。

2026-09-07状态快照：公开代码基线为`90c5165`，DEC-042B Core第二段已审查、提交并Push。
Schema 0.2位字段、0.3 SUM8、0.4 INT64已实现；0.5精确Decimal64比例/偏置仅在专用Loader+Core
开关下执行，默认关闭，仍不能与Protocol Lab共存。Lab第三段A纯格式模块已在默认关闭的独立测试
目标中实现；B证据读写和C运行链仍未实现，普通Lab仍只支持Values 0.3及Result/Record/Event 0.5，
见[DEC-042B契约第10～13节](docs/engineering/pae-dec-042b-decimal-conversion-contract-draft.md)。
Core纠错后Windows专用矩阵Debug/Release各16/16、旧代全切片各25/25；最后仅补raw生命周期测试，
专项各1/1。它们是不同批次和构建组合的既有证据，本次Markdown同步没有重跑，详见
[Core报告](docs/engineering/windows-msvc-2026-dec042b-core-slice.md)。下方早期切片叙述和30/30等数字保留为
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
- 已完成2026-09-06 NetAssist人工Loopback收发、字段核对和关闭工具或端口后的离线回放；本次模拟场景Lab门禁通过，不升级真实协议Golden或硬件结论，详见[人工验收报告](docs/engineering/windows-protocol-lab-manual-loopback-acceptance-20260906.md)；
- 尚未形成稳定公共 API、完整 ProtocolPlan、完整 Decode/Encode Runtime（运行时）或生产可用协议引擎；
- Linux门禁按当前Windows-first（Windows优先）顺序暂缓；正式 Core 性能、协议Golden Vector、目标板、硬件或现场验证尚未完成；
- 当前仓库不得直接替换任何生产协议代码。

当前实现证据见[Protocol Lab Windows UDP Exchange验证报告](docs/engineering/windows-msvc-2026-protocol-lab-udp-exchange-slice.md)、[Protocol Lab Offline Slice Windows验证报告](docs/engineering/windows-msvc-2026-protocol-lab-offline-slice.md)、[Protocol Conformance Runner Windows验证报告](docs/engineering/windows-msvc-2026-protocol-conformance-runner.md)、[PAE-DEC-033A Windows验证报告](docs/engineering/windows-msvc-2026-accounted-plan-memory-slice.md)、[Validated/Budgeted能力链Windows验证报告](docs/engineering/windows-msvc-2026-validated-budgeted-capability-chain.md)和[Frozen Execution Plan Windows内部切片验证报告](docs/engineering/windows-msvc-2026-frozen-execution-plan-slice.md)；此前的[COMPLETE_RECORD Codec历史基线报告](docs/engineering/windows-msvc-2026-complete-record-codec-slice.md)、[Loader/SchemaIr Windows可执行切片报告](docs/engineering/windows-msvc-2026-loader-schema-ir-slice.md)和[第十三轮数字与Parser资源门禁报告](spikes/json_parser/results/windows-msvc-2026-round13-number-resource-gate.md)继续保留。最新Windows x64、MSVC Release/Debug下，配置编译器Runner均`28/28`，Codec合同Runner均`60/60`，首次Decode/Encode分配门禁各`1/1`，操作计数门禁`4/4`，共享Plan并发门禁`2/2`；Parser、Loader、Plan、Codec、Conformance Runner和Protocol Lab共存CTest均`30/30`。上述证据仍不等于完整V0.1、Runtime总量准入、最终生产Parser、正式协议字节正确性、正式性能、Linux兼容、目标板、硬件或现场验证。

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

## 推荐构建和启动

### 1. 开发树构建

在仓库根目录执行。推荐入口是显式的完整 PAE + Qt Lab 开发组合；它不会构建旧 Protocol Lab CLI，因为 Schema 0.11 与该 CLI/Evidence 链按当前契约不能共存。

```powershell
cmake --preset windows-msvc-pae-lab
cmake --build --preset windows-msvc-pae-lab-debug
ctest --preset windows-msvc-pae-lab-debug

cmake --build --preset windows-msvc-pae-lab-release
ctest --preset windows-msvc-pae-lab-release
```

构建后从完整部署目录启动，而不是直接运行 `bin/<Config>` 中的裸 EXE：

```powershell
& .\out\build\windows-msvc-pae-lab\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe
```

完整部署目录包含 EXE、Qt DLL、`platforms/qwindows.dll` 和公开合成配置；`bin/Release/pae_protocol_lab_ui.exe` 只是链接产物，不能视为可独立分发或启动入口。Debug 对应目录为 `out/protocol_lab_ui/Debug`。

早期 Spike、Loader、Codec 与 CLI 预设继续保留，服务历史切片复现，不作为当前 Qt Lab 的推荐入口。构建目录和本地证据均位于忽略的 `out/`；2026-09-14 当时的盘点及清理边界见 [历史生成产物盘点](docs/archive/generated-artifact-inventory-20260914.md)，其中外层归档路径现已失效。

### 2. 现有 SDK 候选消费

当前可定位的五包候选根为 `deliverables/sdk/98df5e0/`；请先读
[03 Windows SDK 集成](docs/guides/03-Windows-SDK集成.md)，再读所选包的 `PAE-SDK-README.md`。
Source 包通过 `PAE_SOURCE_DIR` 引入，Static/Shared 包通过 `CMAKE_PREFIX_PATH` 和
`find_package(PAE CONFIG REQUIRED)` 消费 `PAE::pae`。该候选五包均记录
`source_head=98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` 和 `source_worktree_dirty=false`；这仅说明打包输入来自干净检查点，不表示当前共享工作树无文档变更，也不是正式发布包。

### 3. 完整 Qt Lab standalone 构建

需验证“Lab 白名单源码 + 单个 installed PAE SDK + 固定 Qt/yyjson”时，从
[`tools/protocol_lab_ui/standalone/README.md`](tools/protocol_lab_ui/standalone/README.md) 进入。输入准备脚本
`tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1` 要求显式传入 `-RepositoryRoot`、
`-DestinationRoot`、`-SdkCandidateRoot` 和 `-PackageKind static|shared`，且拒绝覆盖已有目标。
Configure 时至少指定 `PAE_SDK_ROOT`、`PAE_QT_ROOT`、`PAE_LAB_DEPENDENCY_ROOT` 与
`PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED`；产品闭包检查再设 `PAE_LAB_BUILD_TESTING=OFF`。
该入口只是本地 closure validation，不是打包器，也没有生成统一部署物。

当前普通本地使用优先启动 static Release 闭包：

```powershell
& 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\98df5e0\static-release\pae_protocol_lab_ui.exe'
```

配置文件位于同一目录的 `configs\`。shared Release 作为 DLL 消费对照入口，位于
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\98df5e0\shared-release\pae_protocol_lab_ui.exe`，
其同目录必须保留本批验证过的 `pae.dll`、Qt DLL、`platforms\` 和 `configs\`。这两个目录是本地验证产物，不替换旧部署也不构成正式分发。

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
