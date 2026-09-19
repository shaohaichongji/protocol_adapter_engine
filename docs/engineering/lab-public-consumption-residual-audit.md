# Lab 剩余 private 依赖与公开消费边界盘点

## 1. 结论与证据边界

本报告对应 `main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31`，只读核对源码和
构建配置后形成。未 Configure、Build、Test、运行 Lab 或核对包外链接结果；因此下文的“已走公开
路径”是当前源码在相应公开选项开启时的静态运行路径结论，不追加新的动态验证声明。

结论如下：

1. Qt Lab 的 Schema 0.9、0.10、0.11 已有严格的顶层版本分流；在
   `PAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H2`、`PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2`、
   `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM_UI` 对应开启时，这三类首次加载都只调用一次
   `pae::CompileProtocolJson`，没有失败后 private fallback。
2. 0.9 当前产品路径仅迁移了 **complete-record Decode**。Binary Encode 和 Binary stream
   不是 PAE 公开 API 不支持：`pae::HostEndpoint` 已公开 `Encode`、`Push`、`Continue`、`Reset`
   与 `Observe`；缺口在 Lab 的 public adapter、DTO 物化、状态所有权和 UI 接线。当前 H2 明确拒绝
   Encode binding，`DocumentSession::EncodeAvailable` 对 0.9 返回 false，不能把历史非 Qt
   `PreparedBinary` Encode 测试当成当前 Qt UI 已迁移或已提供该功能。
3. 0.10 complete Decode/Encode、0.11 direct/Host 的 complete/stream 路径均已有 public owner；
   但 `AsciiHostAdapter` 和 `DocumentSession` 仍用旧 ASCII adapter 的 DTO/状态类型作为统一表面，
   并保留 `CreatePrivate` 分支。对当前公开路由而言，这主要是**类型/兼容分支和构建耦合**，不是
   0.10/0.11 正常执行仍调用 private Core。
4. 0.5–0.8 是剩余的真实 private 执行主干：private compiler 产生
   `CompiledUiArtifacts`，`ExecutionBridge` 直接持有 private Plan，并调用 private Core 完成
   Decode/Encode 及 Encode 后独立 Decode review。禁止通过禁用这些 Schema、隐藏入口或删除
   回归来声称 public-only。
5. 即使先迁移 0.5–0.8 执行，整个 Qt Lab 仍不会自动成为包外可构建目标：共享
   `DocumentDescription`、`TypedDraft`、诊断/状态和解析工具还暴露
   `protocol_plan`、`config_compiler`、`protocol_core` 或旧 adapter 类型。应先建立 Lab 自有 DTO，
   再逐条替换运行依赖，最后才收紧 target/include/link 边界。
6. 命令行 `pae_protocol_lab` 是另一条仍完全依赖 private compiler/Core/Plan、且还包含 Evidence、
   Replay、UDP 的产品路径。它没有参与 0.9/0.10/0.11 Qt 公开接线；若“整个 Lab 包外消费”包含
   CLI，必须单独定范围，不能借 Qt UI 收口顺带宣称完成。

## 2. 用户入口到执行层的真实路径

### 2.1 Qt Lab：加载与 Schema 分流

入口链为：

`application_window.cpp / DocumentTab::LoadPath` →
`DocumentTab::BeginLoadFromPath` → `CompileWorker::Submit` →
`CompileRequest` → `ClassifySchemaVersion` → `DocumentSession::ApplyCompileCompletion`。

关键事实：

| Schema | 分类与编译 | 首次发布 |
| --- | --- | --- |
| 0.9 | `schema_dispatch.cpp::ClassifySchemaVersion` 返回 `BINARY_PUBLIC`；`compile_worker.cpp::CompileRequest` 调用 `pae::CompileProtocolJson` | `BuildPublicBinaryDescription` 只建立公开描述；初始 unbound，显式 Apply 再由 `BinaryHostAdapter::CreatePublic` 接管同一次公开编译产生的 owner |
| 0.10 | 公开 A2 开启时返回 `ASCII_PUBLIC`，只调用 `pae::CompileProtocolJson` | `public_offline::Adapter::AdoptCompiled` 成为 direct complete owner；显式 Apply 由 `AsciiHostAdapter::CreatePublic` 创建 public Host owner |
| 0.11 | public stream UI 开启时返回 `ASCII_PUBLIC`，只调用 `pae::CompileProtocolJson` | `AsciiHostAdapter::CreatePublicDirect` 建立 public stream owner；显式 Apply 同样使用 `CreatePublic` |
| 0.5–0.8 | 返回 `PRIVATE_LEGACY` | `CompileJsonToPlanWithUiDescription` → `CompiledUiArtifacts` → `ExecutionBridge::AdoptCompiledPlan` |

`schema_dispatch.cpp` 只读根对象中唯一的 `schema_version`，重复键、缺失、非字符串或未知版本均失败；
`CompileRequest` 对 public route 成功或失败都会直接返回，不再执行 private compiler。这是当前三条公开
Schema 的单编译保证。

### 2.2 Decode / Inspect

| 能力 | 当前真实调用链 | private 性质 |
| --- | --- | --- |
| 0.9 complete Decode | `DocumentTab::InspectCurrent` → `DocumentSession::Inspect` → `BinaryHostAdapter::DecodeComplete` → `public_decode::Adapter::Decode` → `pae::HostEndpoint::Decode` | 执行是 public；`public_binary_decode.*` 与 `binary_host_adapter_public.*` 是 Lab 自有复制/状态层 |
| 0.10 complete Decode | `DocumentSession::Inspect` → `public_offline::Adapter::Decode`，或显式 Host 后经 `AsciiHostAdapter::Inspect` → `public_offline::HostAdapter::Decode` | 执行是 public；结果先被转换成旧 `protocol_lab::ascii::ExecutionResult`，属于 DTO 耦合 |
| 0.11 complete 分支 | 仅当公开观察表明该 binding 是 complete 时，`AsciiHostAdapter::Inspect` → public Host Decode | 执行是 public；与 stream 的可用性由 `Observe` 区分，不按 `available` 猜测 |
| 0.5–0.8 complete Inspect | `DocumentSession::Inspect` → `ExecutionBridge::InspectPipeline` → `protocol_core::DecodeCompleteRecord` | 真实 private 执行 |

0.9 的描述和结果物化还使用公开 `CompiledProtocol` 的 metadata、physical query、Host candidate view，
没有为了展示再调用一次 Decode。0.5–0.8 则仍由 `CopyLegacyFields` 复制 `v06::FieldResult`。

### 2.3 Encode

| 能力 | 实际状态 | 证据 |
| --- | --- | --- |
| 0.10 complete Encode | 已公开迁移 | `DocumentSession::Encode` 调用 `public_ascii_adapter->Encode`；显式 Host 调用 `AsciiHostAdapter::Encode` → public Host adapter |
| 0.11 Encode | 若所选 Pipeline 有公开 Encode binding，则复用同一 `AsciiHostAdapter` public owner；没有另编译 | `FindBinding(..., ENCODE)` 与 `AsciiHostAdapter::Encode` |
| 0.9 Binary Encode | **当前 Qt UI 未提供、未迁移** | `DocumentSession::EncodeAvailable` 对 `IsBinaryHostDocument()` 直接返回 false；`DocumentTab::ApplyHostDraft` 的 0.9 分支只接受 Decode；`binary_host_adapter_public.cpp::CreatePublic` 要求 `pae::HostAction::DECODE` |
| 0.5–0.8 Encode | 仍真实可用，但走 private | `DocumentSession::Encode` 构造 `v06::ParsedValues` → `ExecutionBridge::EncodeParsed` → private `EncodeCompleteRecord`，随后 private Decode review |
| 历史非 Qt Binary Encode | 有实现和专项证据，但不是当前 H2 产品路径 | `PreparedBinary` / `prepared_binary_encode.cpp` 与 `tests/protocol_lab_binary/encode_tests.cpp` 仅在 `PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER` 分支构建；H2 CMake 改用 `public_binary_decode.*` |

因此 Binary Encode 的准确结论是：PAE 公开 `CompleteRecordCodec::Encode` 和
`HostEndpoint::Encode` 已存在，旧 Lab 非 Qt 适配也证明过所需行为；当前缺少的是 public H2 的
Encode binding、输入 DTO → `pae::EncodeValue` 映射、输出/失败物化、TX 保存与故障后的重建/UI
事务。不能写成“PAE 不支持”，也不能写成“已由旧 PreparedBinary 迁移完成”。

### 2.4 Stream 与 Host

- 0.11 direct stream：`DocumentSession::{SubmitStream,ContinueStream,ResetStream}` →
  `prepared_->public_ascii_stream_adapter` → `AsciiHostAdapter` →
  `public_offline::HostAdapter` → public `HostEndpoint::{Push,Continue,Reset,Observe}`。
- 0.11 显式 Host：Apply 后同一组 `DocumentSession` 入口改用
  `prepared_->host_adapter`；当前 public route 仍由 `AsciiHostAdapter` 包装 public Host owner。
- 0.10 显式 Host：`AsciiHostAdapter::CreatePublic` 建立 complete binding，Decode/Encode 都由
  public Host 执行；stream 操作由公开观察结果拒绝。
- 0.9 H2：只有 complete Decode Host；`public_binary_decode::Adapter::AdoptCompiled` 明确拒绝
  stream observation。PAE public Host 已有 Binary stream/Encode 能力，但 Lab 尚无相应 public
  owner 和 UI 状态机。
- 0.5–0.8 Qt UI 没有 Host/stream 接线，继续执行 complete record；不要为追求统一表面擅自给
  legacy 增加 Host/stream 产品能力。

### 2.5 CLI 是独立边界

`tools/protocol_lab/main.cpp` → `RunApplication` → `RunInspectOrEncode` / `RunReplay` /
`RunUdpExchange`；`CompileConfig` 调用 private `config_compiler::CompileJsonToPlan`，
`protocol_operations.cpp` 直接调用 private Core，C3/Evidence 路径复用 `ExecutionBridge`。
根 CMake 还明确拒绝 CLI 与 Schema 0.11 同开。Qt public migration 没有改变这条链路。

## 3. private 依赖分类

### 3.1 真实执行依赖

| 依赖 | 文件 / 符号 | 仍服务的行为 |
| --- | --- | --- |
| private compiler + UI sidecar | `compile_worker.cpp::CompileRequest` 的 private 分支 | 0.5–0.8 编译、owned UI 描述来源 |
| private Plan/Core bridge | `v06_execution.*::ExecutionBridge`；`DocumentSession::{Encode,Inspect}` legacy 分支 | 0.5–0.8 Decode、Encode、Encode 后独立 review、诊断和结果物化 |
| CLI private execution | `protocol_operations.cpp::{CompileConfig,InspectFrame,EncodeValues}`、`v07_run.cpp` | CLI Inspect/Encode/Replay/UDP/Evidence；不属于 Qt H2/A2/stream 已迁移范围 |

这些依赖删除后会直接损失现有行为，必须先有等价 public 路径和针对回归。

### 3.2 类型、DTO 与展示耦合

| 耦合 | 文件 / 符号 | 判断 |
| --- | --- | --- |
| UI 描述使用 private enum/descriptor | `description_mapping.h::FieldDescriptor` 使用 `protocol_plan::ValueType`、`WireCodec`、`ByteOrder`、`EncodeSource`、`LinearConversionDescriptor` | 多数是 UI 自有语义可改为 Lab-owned enum/flag；当前会迫使公开路径也包含 private Plan 头 |
| 编译 completion 同时容纳 public/private | `compile_worker.h::CompileCompletion` 含 `CompiledUiArtifacts`/private diagnostic 与 `pae::CompiledProtocol`/public diagnostic | private branch 仍真实需要；应在迁移时用 route-specific payload 隔离，而不是先删字段 |
| Session 统一结果 DTO | `document_session.h`、`ui_field_result.h`、`TypedDraft`、`CopyLegacyFields`/`CopyAsciiFields` | UI 需要 owned DTO，但不应以 private PAE/Core 类型作为公共路径接口 |
| ASCII public 结果转旧 DTO | `ascii_host_adapter.h::ConvertPublicAsciiResult`，以及 `protocol_lab::ascii::{ExecutionResult,ExecutionIdentity,HostBinding,StreamStepResult}` | 当前 0.10/0.11 正常执行 public；这里是 Lab 内部 DTO 复用造成的源依赖 |
| 文本解析与 Decimal64 | `field_table_model.cpp` 引用 `v06::internal::ParseCanonical*`、`v06::Decimal64` | 是通用 UI 输入解析和 owned value 需要；可提取为 Lab-owned utility/type，不应继续把整条 `v06_execution` 带入 public route |
| 状态名称/高亮 | `document_tab.cpp` 的 private framing/Core enum 映射 | public 路径已进行状态转换；应最终映射到 Lab-owned presentation enum，不能让 Qt 直接持有 private engine enum |

### 3.3 仅旧分支或当前公开配置不可达的兼容分支

- `AsciiHostAdapter::{CreatePrivate,private_}` 以及其 `Inspect/Encode/Submit/Continue/Reset/Observe`
  private 分派：在 0.10 public A2 与 0.11 public stream UI 同时开启的已接线路径中不可达，但仍为
  未开启公开选项的兼容构建服务。
- `DocumentSession::PreparedDocument::ascii_adapter`、`BuildDocumentDescription(const
  protocol_lab::ascii::DocumentDescription&)`：同样保留旧 ASCII direct 路径。
- `binary_host_adapter.cpp`、`PreparedBinary` 分支：H2 开启时 CMake 改为编译
  `binary_host_adapter_public.cpp`；private Binary materializer 仍是独立旧选项和历史专项，不是 H2
  正常运行依赖。
- 0.9 private compile/neutral description兼容代码只在未开启 H2 的构建矩阵有意义；H2 route 在
  `CompileRequest` 已提前返回 public owner。

这些分支可在 public-only target 形成后从该 target 的 source closure 中排除，但是否继续保留仓库内
兼容 target 是产品/维护决策；不能先删仓库功能来制造包外成功。

### 3.4 测试耦合

- `tests/protocol_lab_ui/test_support.h`、`binary_stage1_tests.cpp`、旧
  `ascii_session_tests.cpp` / `ascii_stream_session_tests.cpp` / `host_session_tests.cpp` 直接制造
  `CompiledUiArtifacts` 或 private adapter，用于旧路径和迁移前基线。
- `tests/protocol_lab_binary/{prepared_binary,stream,saved_state,encode,integration_audit}_tests.cpp`
  属于 private materializer 历史能力；`public_binary_decode_tests.cpp` 才是当前 H1 public Decode
  专项。
- `tests/protocol_lab_ascii/{ascii_offline_adapter,host_observer_adapter}_tests.cpp` 是 private ASCII
  adapter 专项；`public_ascii_offline_adapter_tests.cpp` 与
  `public_ascii_stream_adapter_tests.cpp` 是 public A1/stream 专项。
- `binary_public_h2_tests.cpp`、`ascii_public_a2_tests.cpp` 及公开 stream UI 下的 session/queue
  断言是当前 public Qt 路径回归入口。迁移时应新增 route-specific public fixture helper，不能通过
  删除旧测试或让所有测试链接开发仓库 private target 来“通过”包外验证。

## 4. 已有公开替代点与真实缺口

| 需求 | 现有公开能力 | Lab 仍需补齐 | 是否观察到必须新增 PAE API |
| --- | --- | --- | --- |
| Schema 编译与 owned owner | `CompileProtocolJson`、`CompiledProtocol` | legacy route-specific completion、owner 发布事务 | 否 |
| 协议/Pipeline/Message/Field/Enum 描述 | `CompiledProtocol::{Protocol,Pipeline,PipelineMessageIndex,PipelineMessageExecution,Message,Field,Enum}` | 建立完全 Lab-owned 的 `DocumentDescription`，移除 `protocol_plan` enum/type | 否 |
| Binary 物理范围 | `MessagePhysical`、`FieldPhysical` 及 Resolve 查询 | 把 0.5–0.8 描述和成功结果映射到同一 Lab DTO；保持动态帧范围语义 | 否 |
| complete Decode/Encode | `CreateCompleteRecordCodec`、`Decode`、`Encode` | legacy 行为等价 adapter；保留 Encode 后独立 review、一次主执行、诊断与 raw/logical 映射 | 否 |
| Host Decode/Encode | `CreateHostEndpoint`、`Find`、`Decode`、`Encode`、`Observe`、`Reset` | 0.9 Encode public adapter、TX state、故障重建和 UI 事务 | 否，按当前代码可由 Lab 侧完成 |
| stream | framing query、`HostEndpoint::{Push,Continue,Observe,Reset}` | 0.9 public stream adapter、冻结后缀/每流状态与 UI；是否要做属于后续产品范围 | 否；但本盘点不建议自动实施 |
| ASCII complete/stream | 已有 `public_offline::{Adapter,HostAdapter}` | 去掉对旧 ASCII DTO/compat branch 的编译耦合，形成 public-only source surface | 否 |
| UI 精确展示 | 公开 metadata、physical、`ValueKind`、`EncodeValueSource` | 某些 legacy 展示目前直接复用 private `LinearConversionDescriptor`/Plan 类型；需先定义行为保持清单和 Lab-owned 表示 | 暂未证明需要；若要求展示 public metadata 未提供的 authored conversion 参数，再单独报契约缺口，不能预设扩 API |

这里的“否”仅表示静态源码已存在组成能力，不是实施、包外构建或行为等价已验证。

## 5. 最小行为保持迁移切片与先后

### 切片 1：Lab-owned DTO / utility 解耦（建议下一片）

目标只替换公共与旧路径共享的类型边界，不改变执行 owner：

1. 将 `DocumentDescription`、字段 value/source/physical presentation enum、Decimal64、canonical
   数字解析、operation/result/stream presentation 状态改为 Lab-owned 类型。
2. public 0.9/0.10/0.11 和现有 private 0.5–0.8 分别映射到同一 owned DTO；旧运行链保持不变。
3. 针对每个 Schema 比较选择、编辑器类型、只读字段、描述/source、物理高亮、Decode/Encode
   可用性和错误清旧结果，证明仅换 DTO 未删行为。

这是最小且可单独验证的切片；它能先消除“公开执行却因 DTO 引入 private 头”的主要混淆，也为
后续包外 target 划界。若与工程整理报告发现的 target/CMake 修改交叉，由总控串行派发。

### 切片 2：0.5–0.8 public complete-record 执行适配

在 DTO 稳定后，用一个 public compiled owner 和 public complete-record codec 替代
`ExecutionBridge`：

- Decode 保持 Pipeline 限定、Message identity、所有已有值类型、raw/logical/范围及失败字段语义；
- Encode 保持 typed input、constant/computed、可变长度、CRC/长度、Decimal64 转换错误，以及成功后
  一次独立 Decode review；
- 保持异步 load/reload/close 的 document/load/plan/request 身份和旧成功清理；
- 不给 legacy 新增 Host/stream，不删除 CLI/Evidence。

完成后再判断 Qt headless target 是否仍需 `v06_execution`；不得在适配完成前移除旧桥。

### 切片 3：ASCII facade 去 private source closure

只在 0.10/0.11 public 回归稳定后：

- 用 Lab-owned binding/result/stream DTO 替换 `protocol_lab::ascii::*` 共享表面；
- 将 `CreatePrivate/private_` 放回独立兼容 target，public-only Qt target 只编译 public facade；
- 保持 direct 与显式 Host 的同 owner、Flow 隔离、Reset、complete/stream 分流、Encode 和现有人工路径。

这一步不删除 private adapter 源码或旧测试，只隔离目标闭包。

### 切片 4：Binary 0.9 能力另行拍板

Binary Encode 和 Binary stream 是功能接线，不是清理依赖的必要前置，建议分开决策：

- 若要求保持**当前 H2 产品行为**，public-only 收口仍可保持 0.9 Decode-only；这不是禁用既有 Qt
  能力，因为当前 Qt H2 从未开放 Encode/stream。
- 若要求把历史非 Qt `PreparedBinary` Encode/stream 能力提升到 Qt 产品入口，应作为独立功能片，
  先冻结 TX/流状态、故障恢复、预算与人工验收，不与 DTO/CMake 解耦混做。

### 切片 5：包外构建与 CLI

Qt 运行依赖完成后，再由工程整理盘点落地 installed SDK + Lab source + 随仓 Qt 的 build target 和
禁止开发仓库 fallback 检查。CLI 是否一并迁移需单独决定；若包含 CLI，还要独立处理
Evidence/Replay/UDP 的契约和数据兼容，不能复用 Qt UI 结论。

## 6. 需要总控 / 用户拍板的项目

1. “整个 Lab 包外消费”是否只指 Qt Lab，还是还包括 `pae_protocol_lab` CLI 与
   Evidence/Replay/UDP。两者成本和验收完全不同。
2. public-only 目标是否允许继续在仓库内保留 private compatibility targets。建议允许：包外目标
   闭包干净即可，不以删除历史能力为验收条件。
3. Binary 0.9 的目标是保持当前 Decode-only H2，还是把历史非 Qt Encode/stream 提升为 Qt
   产品能力。建议先保持现状，分别立项，避免把依赖迁移扩大成功能开发。
4. legacy 0.5–0.8 展示是否要求保留 private Plan 中未由当前 public metadata 显式暴露的 authored
   conversion 参数。当前 UI 实际只显示“logical Decimal64 / Core 检查”的概括文本；若未来要求
   展示公式参数，再评估最小公开查询，不应提前扩 API。

## 7. 未验证与停止点

- 未 Configure、Build、Test、运行程序、重打 SDK、复制部署或核对 DLL/import/include 来源。
- 未证明整个 Qt Lab 已可仅凭已安装 SDK 构建；该项由并行构建预检报告独立盘点。
- 未复验 0.11 三组人工烟测、旧 0.5–0.8 UI、Binary H2 或 CLI；沿用历史证据但不升级结论。
- 未诊断历史 Qt 访问违例，也不将本报告视作该问题修复或稳定性证据。
- 未修改实现、CMake、测试、索引或综合计划；没有 Stage、Commit、Push、发布或删除。

最小下一片建议停在“Lab-owned DTO / utility 解耦的契约与定向实现”，先由总控合并本报告与构建
预检，划定共享文件和 target 改动，再串行派发。
