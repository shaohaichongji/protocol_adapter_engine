# G2 Binary 流式公开消费最小契约

日期：2026-09-19。用户确认预检边界并授权契约落盘与G2-A非Qt实施。
基线main@04fa429；预检和总控文档未提交，须保留。

## 1. 支持域与职责

Schema 0.9 Binary STREAM_CHUNK，覆盖公开FIXED_LENGTH、SYNC_FIXED_LENGTH、SYNC_LENGTH_FIELD。
只消费公开framing查询和Host接口；一个CompiledProtocol、一个HostEndpoint，每(binding, Flow)独立handle与状态。
不扩PAE Core/API/Schema，不重复编译/Decode，不读private Plan或重解析算法配置。
G1 complete Decode/Encode和ASCII路径不变；不是Socket、监听、自动重试或自动drain功能。
最小展示仅策略、最大候选长度和公开运行状态；算法参数检查器后置。

## 2. 一步调用与输入所有权

- owner独占每Flow冻结输入及cursor；编辑器草稿不是执行输入。
- 新Submit只在无冻结后缀、无internal work且无需Reset时准入；检查输入/预算并准备冻结副本后调用一次Push。
- GUI继续：有后缀时以冻结的[cursor,end)调用一次Push；后缀为空且有internal work才调用Continue。
  公开Continue是空输入Push，不持有外部后缀。每次操作最多一步，无循环；仅按实际bytes_consumed推进cursor。
- 无后缀且无internal work为正常idle，不调用或返回明确无工作，不伪造候选、不因此强制Reset。
- 每候选observer返回STOP，保证Lab每步至多一个候选；这不是Host固有上限。成功候选仍进入business sink一次。
- 失败候选在observer内物化，成功字段在business回调内物化；同步借用必须在回调返回前深复制。
  不保存view，不从聚合status推断全部候选，不以成功Decode冒充业务回调正常完成。

## 3. 发布、错误与隔离

实际执行新一步前清旧候选/结果；无候选或失败不复用上次成功字段。保留本步Host/Codec/Framing事实及计数。
输入格式/长度等执行前拒绝不得改变Host、冻结cursor或另一Flow；返回本地诊断，不默认升级通道fault。
回调复制失败、消费越界等执行后本地契约失败须fail closed并要求Reset；保留已确认消费，不假装回滚。
零消费不等于无进展：联合候选、discard/malformed、work及前后Observe；正常NEED_MORE/idle不报fault。
Reset只作用目标Flow，成功后重新Find(endpoint, action, stream_index)；清其冻结输入、结果和计数。
Reset失败保留故障事实，不宣称已清零。切Flow/Tab不触发执行，其他Flow及complete结果不被清除。

## 4. 预算

分别表达C（单次输入上限）、M（最大完整候选长度）、work budget；运行有效值取公开Observe。
M超出物化能力则准备失败。计入冻结capacity、候选/字段/BYTES、旧新替换共存以及调用期输入副本峰值；
以后Qt Hex临时vector计在消费层，不遗漏或重复宣称为Host预算。不将逻辑计费称为RSS硬限制。
分配/预算拒绝须有针对性断言；测试注入不污染普通产品路径。

## 5. 分片与验证

G2-A：仅public_binary_decode.h/.cpp、tests/protocol_lab_binary/CMakeLists.txt、
新增public_binary_stream_tests.cpp及lab-binary-stream-g2-a-validation.md。
允许在新测试内构造合成输入，不改canonical示例。遇到其他文件/公开接口缺口先停报。
使用新根out/build/windows-msvc-lab-g2-a、out/validation/lab-binary-stream-g2-a。
独立D/R非Qt验证三策略split/glued、适用策略的垃圾/malformed恢复、精确suffix、预算暂停/internal work、
idle、callback复制/分配失败、Reset与双Flow隔离、每候选一次Decode及G1回归；Release断言不得关闭。
测试通过不等于Qt接线或人工验证；不跑全仓，不重打SDK，不覆盖旧部署。

G2-A复核后另派G2-B公开描述/自有DTO/adapter，再派G2-C Session/UI。
用户已采纳后续局部中立stream门禁与Binary-only（ASCII stream OFF）矩阵，复用canonical
examples/config/synthetic_stream_framing_slice.pae.json并限定扩cmake/PaeQt513.cmake部署白名单；
这些修改仅属于后续B/C，不在A片提前实施。布局保持左右工作台，最终最多一次简短体验。

## 6. 授权停点

### G2-C A2 组合配置拒绝授权（2026-09-20）

用户授权最小收尾：仅在根 `CMakeLists.txt` 为仓库构建的 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2=ON` 且 `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=OFF` 增加明确配置拒绝及说明，避免可生成却无法路由 Schema 0.10。此限制记录为当前 Lab 构建组合约束，不宣称 PAE 公共 API 本身依赖 private adapter；A2 彻底脱离旧 adapter 后续另议。

Lab 仅改根 CMake 和 G2-C validation 报告，证据仍在既定目录。验证上述精确组合因新门禁失败、common 和 Binary-only 仍配置生成成功；不重跑不变的 D/R 功能矩阵、不重建部署，不改运行源码/默认选项/standalone/PAE/API/Schema。记录新增根文件哈希与旧功能测试的证据边界。完成向总控反馈并停止；无 Stage/Commit/Push/发布/删除授权。

### G2-C 条件修正补充审查范围（2026-09-20）

用户允许将已发生的 `tools/protocol_lab_ui/schema_dispatch.cpp` 和 `description_mapping.h/.cpp` 最小条件修正纳入本轮审查：仅区分 PAE 编译能力与 Lab 实际 ASCII 消费者，保留既有 standalone public-only 行为。此前子任务未先报告即修改的范围偏差保留记录；本次允许审查不表示自动验收或允许进一步扩大实现。

总控已读取 Binary-only D/R 各3项、common 受影响 D/R 各4项通过日志及 ASCII stream UI 缺 observer 配置拒绝日志。仍需核对删除粗门禁后 A2 单独启用而 adapter 关闭等消费者组合是否明确拒绝或正确可用；整个 G2-C 尚未总控收口，无 Git 写操作或发布授权。

### G2-C 根门禁最小补充授权（2026-09-20）

用户追加授权：Lab 可修改根 `CMakeLists.txt` 中导致 Binary-only 被 ASCII stream observer 强制依赖的相关条件，区分 PAE 编译能力与实际 Lab ASCII 消费。仅收窄该依赖门禁；不改变默认开关、Stage1 强制能力、PAE/Core/API/Schema，不删除真正启用 ASCII stream UI/adapter 时的依赖检查，不引入新产品开关或 standalone 入口。本项为下文禁止根 CMake 的唯一例外。

补齐指定 Binary-only 目录 Debug/Release 定向构建与隐藏 Qt/Session 验证；记录实际开关，确认 ASCII adapter/stream observer/旧 Host observer 关闭且 Binary H2 启用。补配置正反例：Binary-only 可配置，实际 ASCII stream 消费缺少依赖仍拒绝；常用组合重新配置核对，不重复未受影响的整套测试。原阻塞日志保留，报告追加修复及验证证据，不把历史阻塞描述为未发生。若仍发现授权外缺口，停报总控。

其余 G2-C 文件范围、独占构建/证据目录、受影响回归、部署哈希及停止反馈规则不变。无 Stage/Commit/Push/发布/删除授权。

### G2-C Session/UI 实施授权（2026-09-20）

用户授权 G2-C；A/B 已限定复核，保留共享工作树全部未提交变更。Lab 独占：
`tools/protocol_lab_ui/document_session.h/.cpp`、`document_tab.h/.cpp`、该目录 `CMakeLists.txt`，
`tests/protocol_lab_ui/CMakeLists.txt`、`binary_public_h2_tests.cpp`、新增定向 Binary stream Qt smoke 文件，
`cmake/PaeQt513.cmake` 仅追加 canonical stream fixture 部署白名单；新增
`docs/engineering/lab-binary-stream-g2-c-validation.md`。
如接线需要，可在 `binary_host_adapter_public.h/.cpp` 中最小补齐 stream 当前结果复制上界、
草稿长度/计费查询及投影辅助；不得改变 A owner 执行契约或借此重构 B/G1。

保持左右中文工作台；按公开 input kind 选择完整解析或流式操作，Binary 输入只用 Hex。
接通提交输入块、继续、重置及 strategy/M/C/work、buffered/frozen cursor、candidate/Decode/business、
discard/malformed/reset 状态；不得把 idle/NEED_MORE 或无候选画成解析失败。
每次动作只执行一步，Continue 不读编辑器，切 Flow/Tab 不执行；结果与草稿按源/目标 Flow 正确保存恢复。
前置格式/预算拒绝不推进 owner，执行后失败清旧结果/高亮并保留事实；故障重读沿用 B 修复。
Qt 临时 Hex 输入与结果/缓存/旧新共存须按现有预算计费，不再保留第二份执行输入。

在 Lab 局部导出 protocol-neutral stream 门禁，ASCII 专有类型/路径仍由 ASCII 门禁控制；
支持 ASCII stream OFF、Binary ON，不新增根产品开关、不重构 ASCII DTO。
复用 `examples/config/synthetic_stream_framing_slice.pae.json`，不建立配置副本源。
不修改 PAE/Core/API/Schema、根 CMake、compile_worker/schema_dispatch、SDK或现有部署/Qt环境。
越界依赖、公开能力缺口或无法保持既有行为时停报总控。

新构建独占 `out/build/windows-msvc-lab-g2-c`，Binary-only 独占其 `binary-only` 子目录，
证据 `out/validation/lab-binary-stream-g2-c`；配置前明确使用已验证 v142/仓库 Qt，不试默认 v145。
常用组合与 Binary-only 均 D/R 构建及定向验证：三策略接线、split/glued/Continue、
候选成功/失败/无候选、冻结输入不随草稿改变、Flow/双Tab隔离、Reset与失败清空。
常用组合加 G1 Encode/complete Decode、ASCII stream 受影响回归，不重跑全仓/A片全部矩阵。
Release断言有效；Qt烟测使用隐藏/offscreen，不启动用户可见窗口或结束用户进程。
在新 Release 部署目录核对可执行文件、Qt运行依赖和 canonical fixture 来源/哈希，给出完整启动路径。
交付复核后最多一次三组简短人工体验，不在执行任务自行宣布人工通过。
总控独占契约/计划/AGENTS；PAE/工程整理不派并行写入。无Stage/Commit/Push/发布/删除授权。

### G2-B 后续实施授权（2026-09-19）

G2-A 已限定复核，用户授权推进下一片。Lab 独占以下文件：
`tools/protocol_lab_ui/owned_presentation_types.h`、`public_binary_description.cpp`、
`binary_host_adapter_public.h/.cpp`、`tests/protocol_lab_ui/binary_public_h2_tests.cpp`，
新增 `docs/engineering/lab-binary-stream-g2-b-validation.md`。后四个工具文件均位于同一 UI 目录。

公开查询映射 input kind/strategy/M；新增 Lab 自有中立观察 DTO 和 Binary step/view，
复用 A 片唯一 owner，通过 adapter 提供 Submit/Continue/Observe/Reset 与当前结果映射。
不另存第二份冻结输入，不重新 Decode，不重构 ASCII DTO。映射区分无候选、失败、成功与本地拒绝；
执行后复制/预算失败不发布半成品或旧成功，不丢已确认消费；按既有契约要求目标 Flow Reset。
保持 G1 complete 与 ASCII 行为，明确描述、物化、旧新结果共存及输入临时副本的计费边界。

独占新构建 `out/build/windows-msvc-lab-g2-b`、证据 `out/validation/lab-binary-stream-g2-b`。
沿用现场已验证工具链和配置，D/R 定向验证公开描述三策略、逐步映射、无旧结果残留、
前置拒绝不推进、结果复制/预算失败、Reset 与双 Flow 隔离，并运行受影响 G1/描述专项。
Release 断言须有效；不重复 A 全矩阵，不构建可见 Qt 应用、不新增人工验收。
如现有测试入口无法容纳，或需改 owner/CMake/Session/UI/公共接口，先报告总控，不自行扩范围。
G2-C 门禁、Session/UI 和 fixture 部署仍后置；无 Stage/Commit/Push/发布/删除授权。

Lab任务独占G2-A；PAE/工程整理不并发修改。总控独占本契约、综合计划及外层AGENTS。
禁止Git写操作、发布、删除、旧部署/SDK/本机Qt修改。交付包含文件、命令/日志、未验证及风险，
主动反馈总控01a04601-757d-7bb1-8254-61dde4954d74一次，停止写入待复核。
