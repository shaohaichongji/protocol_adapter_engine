# Stage 4 Binary Lab 公开 API 迁移首片范围核对

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-15。状态：已完成派发范围，待总控复核。本报告只做现场源码与既有证据的消费侧分析，
不实现迁移，不修改代码、CMake、Schema、README、SDK 包或索引，不构建、测试或运行 UI，不删除目录，
不修改 Qt 环境，未获 Stage、Commit、Push 或发布授权。TLV、数组、嵌套及其他新协议能力不进入本轮。

## 1. 结论

当前公开 Compiler/metadata、Codec、StreamFramer 与 Host 已能替代 Binary Lab 的基础执行主链：配置编译、
稳定 ID 到 index 解析、完整记录 Decode、typed logical 值、转换字段实际 raw integer、流式候选、
endpoint/action/flow/generation、同 Handle 多 Message Encode、错误状态、Reset 和执行资源报告均不再要求
Lab 访问私有 Plan/Core/Host 类型。

但现有已验收 Binary 结果不能直接无损切换到这些公开接口。首个用户可见迁移片至少还依赖应用无关的
物理观察事实：固定及有界记录的实际 byte range、位字段物理 byte/mask、动态 integrity/computed
位置，以及这些事实与本次 Decode 成功结果的同调用身份。现有私有路径从 `PlanBundle` 与
`host_endpoint::Candidate` 取得这些事实；当前公开 metadata/Host callback 没有等价信息。若不先补齐，
就只能删掉已验收高亮、让 UI 解析 Schema/Plan、重复 Decode，或从 logical 值猜 raw/range，均不可接受。

Encode 需分层判断：`PreparedBinary::Encode` 及其非 Qt 测试已经存在并验证，不能写成“Binary 尚无
Encode”；但 Qt `BinaryHostAdapter`、`DocumentSession` 与 `DocumentTab` 尚未接入该入口，Binary 文档
只允许 Decode binding，且隐藏 Encode 按钮。因此 **Encode UI 不是首个可见迁移片的必需能力**；
后续迁移现有 headless Encode 时，仍需补 Encode 同调用的生成字段/raw/物理范围观察，不能用
Encode 后 Decode 回环伪造。流式 Submit/Continue 同样已有非 Qt 能力、尚未接入 Binary UI，按总控
“先 Binary、后 ASCII/流式”顺序不进入首个 UI 切换片。

推荐以一个新的、并列且可独立验证的 public-only headless Binary adapter 起步，在它达到完整记录
Decode 等价前不替换现有 UI backend；随后再切换 Binary UI。这样既能证明 `PAE::pae` 的真实仓库外
消费边界，也不会让共享 `CompileWorker`、`description_mapping` 和 UI headless target 对 0.5–0.8
旧桥及 0.10/0.11 ASCII 的私有依赖阻碍 Binary 首片。并列 target 只是过渡停点，不得形成两个可见
执行路径或长期保留第二套 Decode 引擎。

## 2. 现场、既有能力与证据边界

- 现场仓库为 `<REPO_ROOT>`，基线
  `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`；接管时暂存区为空，完整保留共享工作树既有修改。
- 现有 Binary 非 Qt backend 位于 `tools/protocol_lab_binary/`；Qt 适配位于
  `tools/protocol_lab_ui/binary_host_adapter.*`，状态/Tab/窗口接线位于 `document_session.*`、
  `document_tab.*`、`application_window.*`。
- 0.5–0.8 仍通过 `tools/protocol_lab/v06_execution.*` 直接消费私有 Plan/Core；0.10/0.11 仍通过
  `tools/protocol_lab_ascii/`。Stage 4 Binary 首片不迁移、删除或放宽这些路径。
- 既有 Binary UI Stage 1 自动证据为 Windows x64/v142 Debug/Release 各 28/28；限定人工试用覆盖
  0.9 未绑定、显式 Apply、完整记录 Decode、9 字段 typed/raw/logical、payload/cross-bit 高亮、失败
  清旧结果、非法 Hex、Flow 恢复和取消 Apply/Reload/关闭。Reload 到无效配置未获人工执行证据，
  两 Tab Yes→No 的“数据不变”只有自动证据，不能升级表述。
- 既有非 Qt 总体回归记录为 D/R 各 11/11，覆盖完整 Decode、三种 stream、Encode、CRC/Decimal、
  双流、保存、预算和故障隔离。它证明旧 backend 行为，不证明公开 API 迁移已完成。
- 本轮只阅读源码、契约和历史验证记录；没有重新运行任何命令。因此下文“已验证”均注明为既有
  证据，不是本轮新增动态证据。

## 3. 当前实际能力分层

| 层次 | 当前实际状态 | 本次迁移要求 |
| --- | --- | --- |
| Binary Qt 可见能力 | Schema 0.9 初始未绑定；显式 Apply；只接受 complete-record Decode binding；两个 Flow 保存草稿/当前结果；显示 typed/raw/logical、byte/bit 高亮和失败诊断；Encode 按钮隐藏 | 首片必须无损保持，不能用隐藏字段、空高亮或第二次 Decode 制造“已迁移” |
| Binary 非 Qt 已实现但 UI 未接入 | `Submit/Continue/Stream/Reset`、Host Encode、TX 自有输出/映射、per-flow state、128 MiB/256 MiB 逻辑准入 | 不冒称尚未实现；首个 UI 片可延期启用，但删除旧 backend 前必须有公开路径等价承接或明确保留范围 |
| 旧 Binary 0.5–0.8 | `ExecutionBridge` 直接持有 Plan/workspace，支持原 Encode/Inspect | 本批完全保留；不因 0.9 公开迁移放宽或删除旧桥 |
| ASCII 0.10/0.11 | 独立 Offline/Host/stream adapter，和 Binary 共用若干 UI 类型及 worker | 本批保留功能、构建门禁和回归；不要求其同时迁移公开 API |
| 未实现/未授权能力 | Binary stream UI、Binary Encode UI、网络/设备、持久化、历史队列、TLV | 不列为首片验收必需，不借迁移扩大功能 |

## 4. 消费矩阵

| 消费事实 | 当前路径与旧私有 API | 可用公开替代 | 确定缺口或兼容点 | 影响 |
| --- | --- | --- | --- | --- |
| 编译与诊断 | `compile_worker.*` 调 `CompileJsonToPlanWithUiDescription`，完成对象持有 `CompiledUiArtifacts` | `CompileProtocolJson`、`CompileResult`、`CompileDiagnostic`、`CompiledProtocol` | 基础编译/诊断已足够；通用 worker 仍为旧桥/ASCII提供私有 artifacts，不能整体换 target | Binary public adapter应有独立编译入口或接收 `CompiledProtocol`；不强迫 ASCII 同批迁移 |
| 协议/Pipeline/Message/Field/Enum 描述 | `owned_description.*`、`description_mapping.*` 读取 `PlanBundle` 与 UI sidecar | `Protocol/Pipeline/Message/Field/Enum` metadata、`PipelineMessageExecution` | ID、文本、logical `ValueKind`、Encode source、动作及输出上界已有；wire/raw类型、转换说明、帧布局/范围和完整字段约束不全 | 基础选择器可迁；物理详情和准确 Type/Source 展示必须等观察契约，不能解析 JSON补齐 |
| Schema 0.9 全 Plan 准入 | `binary_admission.*` 遍历 private Plan、codec/framing/layout | public metadata、`QueryStreamFramingCapability`、`CreateHostEndpoint` 的失败关闭 | Host 可最终拒绝非法 binding；当前公开信息不足以复制全部“准备前可展示的原因”和物理资源上界 | 首片只保留有用户价值的明确失败；不复制 private 算法到 Lab，也不降低整次准备原子性 |
| Binding/Flow/Handle | `PreparedBinary` 持有 private `host_endpoint::Session/BindingSpec` | `HostBindingSpec`、`CreateHostEndpoint`、`Find`、opaque `HostChannelHandle` | 基础语义已闭合；public binding 直接使用 pipeline index，Lab仍负责 endpoint文本及两 Flow策略 | 可直接替代；Handle不得跨 Host/reload/rebind保存，Reset后必须重新Find |
| 完整记录 Decode | `PreparedBinary::DecodeOnce` → private Session → `Candidate` → `MaterializeCandidate` | public `HostEndpoint::Decode`、`HostCandidateView`、`HostOutputView`、`DecodedRecordView/FieldView` | typed值、enum raw/known、Decimal和实际 conversion raw已提供；borrowed view只在同步callback内有效 | 在一次callback内复制自有DTO；失败发布清旧成功，不保存任何 public view/ByteView |
| 失败观察 | private Candidate含帧、message/field/Codec事实 | public candidate frame、Codec status、failed flat field、counts/reset_required | 失败时不一定直接给 message index；可由已确认 field映射时推回，其他情况只能标未知 | 不伪造message；只显示接口确证的诊断帧/位置，旧成功字段和高亮必须清空 |
| raw / logical | private Candidate raw整数 + Core field slot | public typed field + `ConversionRaw*`、enum raw/known | BOOL等无独立raw仍无raw；Encode public输出当前没有字段观察 | Decode首片可保持“未单独提供”；禁止logical反算raw。已记录的 DECIMAL64 Type显示债务应按logical kind校正 |
| 固定 byte 与 bit高亮 | `BuildOwnedDescription` 从 `FrozenFieldPlan/FieldExecutionPlan` 计算大小端物理mask | 当前无等价公开事实 | **首个可见迁移片阻断** | 不得按field序号、value或BYTES指针猜范围；未补接口前不能切UI backend |
| 有界payload/动态integrity/computed位置 | private MessageExecutionPlan + actual frame length，成功后交叉验证BYTES长度 | 当前无等价公开实际布局观察 | **首个可见迁移片阻断** | 零payload不高亮；范围不一致整次物化失败，不能裁剪或保留旧范围 |
| Stream Submit/Continue | private Session Push/Continue/Observe + Lab冻结数组/游标 | public Host `Push/Continue/Observe/Reset` 或 Framer+Codec | 执行语义已足够；UI未启用。Lab仍须拥有未消费后缀，候选callback内复制 | 后续流式片迁移；一次动作不循环排空、不重提已提交候选、不重复Decode |
| Encode输入/执行 | `PreparedBinary::Encode` 解析稳定field/enum ID并调 private Host一次 | public metadata selector + `EncodeValue` + public Host `Encode(message_index,...)` | 基础Encode可替代；当前 UI未接入 | headless能力不能写成未实现；首个Decode UI片不新增Encode控件 |
| Encode TX观察 | private路径成功后生成所有字段range/integrity映射，保存逻辑输入与TX | public Host output只给message index与bytes | constant/computed/integrity、转换raw及物理范围缺少同调用观察 | 后续headless Encode迁移的阻断；禁止Encode→Decode回环或失败buffer展示 |
| Encode故障恢复 | private Host只允许Decode Reset，复制/callback故障后要求重建Session | public Host契约允许Decode和Encode Handle Reset | **行为差异，需总控决定** | 首片不涉及Encode UI；后续不得静默从“重新Apply”改为通道Reset，也不得自动重发 |
| 资源准入 | private Plan/Session report + Lab DTO/草稿/UI reserve，128/256 MiB门禁 | public Compile/Codec/Framer/Host memory report；Lab自算DTO/UI副本 | PAE报告不是RSS；新观察DTO及新旧backend过渡共存必须计费 | 保留创建、callback复制、Flow切换、rebind四个门禁；发布阶段不得新增可失败复制 |
| Tab/Flow/关闭/Reload/rebind | `DocumentSession/DocumentTab/ApplicationWindow` 管revision、pending、两阶段确认和发布 | 仍是Lab职责，不应进入PAE | public Host只提供实例/Handle/generation，不替代document/load/request身份 | 保留完整六维身份；迟到结果拒绝；取消保持草稿/结果；确认Reload失败不恢复旧owner |

## 5. 具体耦合与最小拆分路径

### 5.1 不能整体替换的共享耦合

1. `pae_protocol_lab_ui_headless_internal` 当前 PUBLIC链接旧 `pae_protocol_lab_c1_execution_internal`，
   PRIVATE链接 `pae_config_compiler`、`pae_protocol_core_slice`、`pae::protocol_plan`，并按门禁链接
   ASCII和Binary adapter。只检查该聚合target是否含私有库，无法证明Binary是否完成公开迁移。
2. `CompileCompletion` 直接持有 `CompiledUiArtifacts`；`DocumentSession::ApplyCompileCompletion` 用同一
   private Plan决定0.5–0.8、0.9和0.10/0.11路由。贸然把 worker 改成只返回
   `CompiledProtocol` 会同时破坏旧桥和ASCII。
3. `DocumentDescription/FieldDescriptor` 仍直接使用 `protocol_plan::ValueType/WireCodec/ByteOrder/
   EncodeSource/LinearConversionDescriptor`；`FieldTableModel` 和 `DocumentTab` 也读取这些私有enum。
4. `BinaryHostAdapter::Create` 接收 `CompiledUiArtifacts`，内部再交给 `PreparedBinary::Create`；
   `PreparedBinary` 的头本身暴露 private compiler/Plan/Core/Host类型。因此只把CMake改为链接
   `PAE::pae`会立刻失败，也不代表行为已迁移。

### 5.2 推荐最小路径

1. **前置门槛**：总控合并本报告与PAE观察缺口报告，冻结一套应用无关物理/同调用观察契约；若PAE
   需增量，先由PAE独占修改公开头/实现/SDK并交付验证，Lab不并发猜接口。
2. **H1 public-only headless complete Decode（推荐首个实施片）**：在
   `tools/protocol_lab_binary/` 内新增边界清晰的public adapter/DTO文件及独立CMake target，target
   只链接 `PAE::pae` 和项目通用编译选项；头文件只包含标准库与 `pae/**`。它用 public Compiler
   构造自有description/binding/Host，在一次Decode callback内复制typed/raw/physical observation，
   保留Lab revision、预算和失败清旧语义。现有private PreparedBinary仍只服务当前UI，H1不接UI、
   不形成第二个可见执行入口。
3. **H1停点**：public-only target及针对性D/R测试通过、无private include/link后停止，交总控复核。
   此时只能宣称“公开headless完整Decode等价片完成”，不能宣称Binary UI已迁移，也不能删除旧target。
4. **H2 Binary UI切换**：复用H1 DTO，改 `binary_host_adapter.*` 及Binary分支的worker/completion接线；
   保持旧桥/ASCII分支原样。Binary Apply/rebind请求必须只编译一次，不可为取得private sidecar再执行
   public第二次编译；若共享worker无法在不双编译的情况下返回public owner，应增加明确的Binary
   request/result variant，而不是让UI解析`schema_version`决定后端。
5. **H2发布与生命周期**：保留 `DocumentSession` 已有prepare→预算→确认→move-only publish顺序及
   `DocumentTab/ApplicationWindow` 的pending取消、旧completion拒绝和两Tab关闭事务。Binary UI切换
   后才允许移除其对private adapter的使用；聚合UI target仍可因0.5–0.8/ASCII链接私有库，验收应按
   Binary专属target和源依赖判断。
6. **后续而非首片**：按明确派发迁移headless Encode、Binary stream，再讨论ASCII；全部现有消费者
   不再使用private backend后，才另行授权删除失效文件/门禁。旧 `v06_execution` 在0.5–0.8仍有
   真实消费者，不能在Binary 0.9切换时删除。

## 6. H1建议文件范围

允许写入应限定为以下类别，具体新文件名由实现轮按本地命名统一，不在本报告预造接口：

- `tools/protocol_lab_binary/`：新增public-only adapter、Lab-owned DTO及必要的独立预算/映射实现；
  不修改旧private执行语义，不复用会泄漏private类型的 `candidate_materializer.h`、
  `owned_description.h` 或 `prepared_binary.h` 作为public target头。
- `tools/protocol_lab_binary/CMakeLists.txt`：新增独立target，仅链接 `PAE::pae`；旧target在H1保留。
- `tests/protocol_lab_binary/`及其CMake：新增public adapter针对性测试；从既有独立向量复用测试意图，
  不以调用旧backend作为唯一oracle，也不让新旧实现互相生成期望。
- 根 `CMakeLists.txt`：仅增加H1明确门禁与目录/测试接线，依赖公开API选项；不得让H1重新要求
  `pae_host_endpoint`、`pae_config_compiler`、`pae_protocol_core_slice`或`pae::protocol_plan`。
- 若PAE观察接口需改动，其文件范围由PAE任务单独冻结并先行交付；Lab H1不修改
  `include/pae`、`src/public_api`、Plan/Core/Host或SDK脚本。

H1不修改 `tools/protocol_lab_ui/`、`tools/protocol_lab/`、`tools/protocol_lab_ascii/`、Qt、Schema、
fixture、README/index或发布包。H2 UI切换必须另行派发并重新列确切文件。

## 7. 构建依赖切断与消费形态

### 7.1 “不再私有 include/链接”的判据

H1通过必须同时满足：

1. public adapter生产头/源不出现 `../../src/`、`config_compiler::`、`protocol_plan::`、
   `protocol_core::`、`host_endpoint::`或`protocol_lab::v06::ExecutionBridge`；只使用 `pae::*`及Lab
   自有类型。
2. public adapter target的消费侧声明只含`PAE::pae`及项目通用options；PAE自身所需的内部静态闭包
   只能由`PAE::pae`封装传递，Binary target/CMake不得点名任何private target。不能因最终链接器恰好
   从其他UI/ASCII target带入私有库而宣称关闭。
3. 头自包含测试只提供SDK `include`路径即可编译；生产配置无test hook。
4. 可用源码包/安装后的静态或动态SDK构建同一最小consumer；至少有一种仓库外binary形态验证。
5. Binary结果形成期间每个候选只发生一次公开Host/Codec执行；代码和测试不得调用旧bridge作
   复核，也不得由logical反算raw/physical位置。

聚合 `pae_protocol_lab_ui_headless_internal` 因旧桥/ASCII仍含私有依赖不构成H1失败；必须检查新的
Binary专属target。H2切换后则应证明Binary专属源不再include旧private backend，且Binary CMake门禁
不再要求 `PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER` 和private Host/Core/Plan选项。

### 7.2 推荐消费形态

- **首选源码形态**：仓库内开发和H1定向测试直接链接当前源码树的 `PAE::pae`。这便于在公开观察
  契约稳定后同步迭代，又不把Lab重新耦合到internal targets。
- **静态SDK**：作为首个仓库外headless闭包复验，依赖部署最少，仍只链接 `PAE::pae`，严格匹配
  Debug/Release、x64、v142和动态CRT。
- **动态SDK**：在源码/静态闭包后补D/R运行复验，确认`pae.dll`随consumer部署及C++ ABI/CRT边界；
  不建议让仓库内Qt Lab日常构建直接依赖ignored `out/sdk-stage3/final6-*`路径。

三种形态是同一公开API的交付验证，不是要求H1同时形成三套实现。Stage 3 final6是冻结基线；若
Stage 4公开头发生增量，必须生成新的SDK证据，不能继续把final6称为包含新接口。

## 8. 最小验证与停止条件

### 8.1 H1自动验证

1. Windows x64/v142 Debug/Release分别构建public-only target及测试；默认OFF、缺public API依赖明确
   configure失败、`PAE_BUILD_TESTING=OFF`生产库构建通过且0测试注册。
2. public header self-contained和源依赖扫描；CMake target link graph证明无private Compiler/Plan/Core/
   Host/旧bridge。
3. complete-record Decode覆盖：UINT64/INT64/BOOL/ENUM/BYTES/DECIMAL64、known/unknown enum、
   实际conversion raw、大小端byte range、跨字节bit mask、有界payload零/最大、动态integrity位置。
4. 独立帧覆盖SUM8/CRC成功、损坏后只保留诊断帧、无成功字段/高亮，再次正确输入恢复；不使用
   Encode生成唯一Decode期望。
5. 本地Hex/容量拒绝不调用Host；Host协议失败发布本步失败并清旧成功；物化/预算失败不半发布且
   `reset_required`/消费事实不丢失。owner销毁后自有DTO仍可读，borrowed public view不逃逸callback。
6. 两个逻辑Flow的身份、Current、草稿、Reset/错误隔离保持；H1即使只开放完整记录，也不得让
   一个flow的失败或重建覆盖另一flow。
7. 资源报告区分PAE owner/workspace与Lab DTO/草稿/临时复制；exact和minus-one在复制前拒绝，
   不把128/256 MiB逻辑账本宣称为RSS或全局OOM证明。
8. 仓库外至少Static D/R同consumer通过；Shared如进入本片则同时核对DLL复制。没有硬件、网络、
   Golden、Linux、性能或人工UI结论。

### 8.2 H1停点

H1达到上述headless complete Decode闭包后立即停止。不得继续修改Qt UI、启用Binary Encode/stream
控件、删除旧backend、迁移ASCII、重打正式发布包或自动进入H2。总控先合并PAE缺口交付和H1证据，
再决定UI切换文件范围。

### 8.3 H2最低回归（后续派发输入）

UI切换时至少保留现有Binary Stage 1自动矩阵：未绑定/Apply、所有支持类型、实际raw/logical与物理
高亮、成功后协议失败/本地失败清旧、Flow切换、prepare/copy/budget失败不半发布、旧completion拒绝、
Reload失败、取消Apply/Reload/Close、两Tab关闭Yes→No、唯一执行路径。0.5–0.8和0.10/0.11做受影响
回归；是否新增人工复验由总控另行授权，不能用headless PASS替代UI生命周期验收。

## 9. 需总控合并的决定与剩余风险

1. **物理及同调用观察契约**：等待总控合并独立PAE报告；本报告只确认消费缺口和不可绕过方式，
   不预设接口名称或擅自要求Plan/Core改造。
2. **Encode故障恢复**：旧private backend要求重新准备Session，public Host允许Encode Handle Reset。
   推荐迁移时先保持现有“故障TX禁用、显式重新Apply、绝不自动重发”行为；如要改为通道Reset，
   这是可观察行为变化，应单独确认并补隔离/代次/提示测试。
3. **共享编译worker**：H1用独立public target绕开；H2必须避免同一次Binary Apply同时private/public
   双编译。若采用completion variant，需证明旧桥/ASCII时序、取消和预算不变。
4. **过渡双backend**：仅允许旧backend继续服务当前UI、新backend仅服务H1测试。不得在同一用户动作
   同时调用两者比较，也不得长期保留双路由；H2切换完成后再列删除候选请用户授权。
5. **已知展示债务**：内部试用记录中的DECIMAL64 Type显示、Decode Source/编辑外观及初始未绑定
   Mode疑点不能因迁移被掩盖。首片至少保证logical/raw不倒退；是否同时修正纯展示项由H2范围决定。

本任务唯一新增文件为本报告；未修改代码、CMake、Schema、README、SDK包、索引、Qt或其他报告，
未执行构建、测试、UI、格式化、清理、Stage、Commit、Push或发布。已停止写入，等待总控复核。
