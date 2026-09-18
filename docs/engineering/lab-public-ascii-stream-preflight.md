# Lab 0.11 流式公开消费迁移预检

日期：2026-09-18。状态：**已完成派发范围，待总控复核**。本报告基于
`main@dbf4798f96a45b8d36a4754ad5a01e274680337e` 静态核对 A2 后现状，只定义 Lab
消费侧的最小分片、依赖门槛和后续验证计划；没有修改源码、CMake、公共头或其他文档，没有构建、
测试、部署、Stage、Commit、Push、发布或删除。历史验证记录仅用于理解既有行为，不作为本轮运行证据。

## 1. 结论与实施门槛

0.11 不能在当前检查点上直接把路由从 private 改成 public。运行接口已经能表达有界流处理：
`StreamFramer` 提供同步借用候选、精确 `bytes_consumed`、STOP、`Continue`、`Reset` 和空闲态观察
（`include/pae/stream_framer.h:50-61,84-124`）；`HostEndpoint` 进一步把每个候选的一次 Decode、
候选观察和成功业务回调分开，并明确 Reset 后旧 Handle 失效、必须重新 `Find`
（`include/pae/host_endpoint.h:101-168,177-194`）。但是公开 capability 当前只有 `available` 布尔值，
实现也只判断 `STREAM_CHUNK`（`include/pae/stream_framer.h:45-48,142-149`、
`src/public_api/stream_framer.cpp:262-281`），尚不能让 Lab 从冻结状态确认 input kind、framing strategy
和最大帧长 M。

因此必须先满足以下依赖门：

1. **PAE 静态事实门**：公开、只读、owner 绑定的 Pipeline framing 描述，至少区分完整记录与 stream
   input kind，给出明确 strategy，并仅对相应策略给出 M。0.11 ASCII CRLF 的 M 包含终止符；完整记录
   不得伪造 M。越界 Pipeline、无效 owner、非 stream、未知/不支持策略和内部不一致必须有明确失败，
   失败不返回貌似有效的默认值。
2. **事实来源门**：上述数据必须来自严格编译后的冻结状态。Lab 不得私读 JSON、正则扫描
   `schema_version` 以外的协议内容、从 `available=true` 猜 CRLF，也不得由 Message 长度或运行观察值
   反推 M。`C = min(64 KiB, effective_max_submit_bytes)` 是实例有效 submit 容量；C、M 和
   `effective_max_work_units` 是三个不同边界。
3. **所有权门**：0.11 public 路径必须持有一次公开编译产生的同一 `CompiledProtocol` owner，并复用
   A1/A2 已有 ASCII 描述和 Decode 结果物化。一次 worker 请求只调用一个目标编译器；不得同时保留
   private Plan 作为 public 缺口的回退。当前 0.11 仍明确路由 `PRIVATE_ASCII`
   （`tools/protocol_lab_ui/schema_dispatch.cpp:80-95`），这是待实施事实，不是缺陷绕过许可。
4. **行为保持门**：0.10 A2、Binary H2、旧 legacy 路由和 CLI/Evidence 对 0.11 的拒绝保持不变。
   0.11 改路由、非 Qt adapter 和 UI 接线必须分别获得精确实施派发，不能在静态事实片中顺带完成。

最小顺序为：**PAE 静态 framing 描述 → 非 Qt public stream adapter → Qt Session/UI 接线**。
非 Qt 片先闭合候选、消费、冻结后缀、Reset、预算和自有 DTO；Qt 片只负责现有修订、草稿、Flow/Tab、
Apply/取消/reload 与展示发布，不在 UI 中重新实现 Framer 或 Decode。

## 2. A2 后现场事实

### 2.1 已可复用部分

- A2 的 `public_ascii_offline_adapter.*` 已拥有同一公开 compiled owner、复制后的 ASCII 描述、直接
  Decode/Encode 物化、输入切片核对和逻辑预算。0.11 候选成功或失败结果应复用这一套 DTO/物化规则，
  不再保留另一套字段复制算法。
- `public_ascii_host_adapter.*` 已在同一个 A1 owner 上创建 `HostEndpoint`，持有 binding/channel/Handle，
  对 complete-record Decode 的当前分工是：失败候选在 observer 复制，成功结果在 business sink 复制
  （`tools/protocol_lab_ascii/public_ascii_host_adapter.cpp:210-255`）；Host、owner 与本地 retained storage
  已进入 A2 instance/replacement 准入。这是 stream 片应延伸的所有权与预算基础，不应另建第二份
  public compiled owner。
- Qt facade 目前有稳定的 complete/stream 表面，但 public 分支只支持 complete record；Submit/Continue
  在 public 分支直接拒绝，Observe/DiscardableState 只转发 private adapter
  （`tools/protocol_lab_ui/ascii_host_adapter.h:17-57`、
  `tools/protocol_lab_ui/ascii_host_adapter.cpp:191-220`）。因此后续可以保持 Session 调用形状，替换其
  0.11 backend，而不要求先重写 UI。
- 现有 Session 已维护 `document/load/plan/selection/input/request` 修订、流步骤、已提交输入修订和
  成功/失败展示所有权（`tools/protocol_lab_ui/document_session.h:67-134,350-374`）。Submit/Continue
  会先清旧结果，再仅按本次候选发布；Reset 清草稿、递增 input revision 并清流结果
  （`tools/protocol_lab_ui/document_session.cpp:1665-1754,1757-1829`）。这些是迁移必须保持的 Lab
  生命周期，不属于 PAE 公共 API。

### 2.2 当前仍是 private 的部分

- 0.11 编译 completion 仍携带 `CompiledUiArtifacts`，Session 仍创建 private `OfflineAdapter`
  （`tools/protocol_lab_ui/compile_worker.cpp:17-68`、
  `tools/protocol_lab_ui/document_session.cpp:598-681`）。A2 public completion 只处理当前
  `ASCII_PUBLIC` 即 0.10（`document_session.cpp:557-595`）。
- private description 直接读取 Plan 的 input kind、`ASCII_CRLF` strategy 和
  `maximum_frame_length`（`tools/protocol_lab_ascii/ascii_offline_adapter.cpp:203-220`）；该访问不能搬到
  新 public adapter 或 UI。
- private stream context 自有冻结输入/cursor、reset-required、步/候选/Decode/丢弃计数；只在无后缀
  且无 internal work 时接收新 chunk，并按实际 `bytes_consumed` 推进
  （`ascii_offline_adapter.cpp:588-628,648-750`）。这些是可复用行为，不是可继续依赖的私有类型。
- 显式 Host private adapter 已把候选数、Decode 成功、observer 返回和业务输出分别累计，并在复制或
  回调故障后保留消费事实、要求 Reset（`tools/protocol_lab_ascii/host_observer_adapter.cpp:273-315,
  316-417`）。迁移不能把 `candidates_delivered` 或 `decode_successes` 当作业务回调成功。

## 3. 非 Qt public stream adapter 最小分片

建议在一个独立、默认 OFF 的非 Qt 分片内完成，先不接 Qt。优先复用现有 A1/A2 target 与 owner，
不新增 PAE 公共 DTO。

### 3.1 建议文件范围

允许实施时的最小候选范围如下；确切名称由总控实施派发冻结：

- 新增 `tools/protocol_lab_ascii/public_ascii_stream_adapter.h/.cpp`：只保存 A1 owner、公开
  `HostEndpoint`/Handle、每 Flow 自有冻结输入、cursor、计数、故障态及静态 framing DTO；对外返回
  A1/A2 已有 `OperationResult` 和 stream step/observation 自有 DTO。
- 最小调整 `tools/protocol_lab_ascii/public_ascii_offline_adapter.h/.cpp`：仅暴露同一 owner 下已有描述、
  limits 和同次 Decode 物化所需的内部组合入口；不得复制一套描述或 Decode 映射，不改公开 PAE。
- 如采用 A2 Host owner 直接承载 stream，最小调整
  `tools/protocol_lab_ascii/public_ascii_host_adapter.h/.cpp`：Host binding 依据公开静态事实区分
  complete/stream，增加 Submit/Continue/Observe/Reset 与 flow storage；0.10 complete Decode/Encode
  行为原样保留。若新 stream adapter 已封装此职责，不再并行实现第二条 Host 流路径。
- `tools/protocol_lab_ascii/CMakeLists.txt` 与根 `CMakeLists.txt`：新增独立默认 OFF 门禁，要求 public A1、
  public API Stage 1、Host endpoint、V11/Framing；不得继续以 private
  `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER` 或 private `pae::protocol_framing` 链接作为 public adapter
  的隐式依赖。现有门禁见根 `CMakeLists.txt:186-194,229-253` 和
  `tools/protocol_lab_ascii/CMakeLists.txt:48-84`。
- 新增 `tests/protocol_lab_ascii/public_ascii_stream_adapter_tests.cpp` 并在
  `tests/protocol_lab_ascii/CMakeLists.txt` 独立注册；不覆盖现有 private stream 测试。
- 本片验证记录另行指定；不回写本预检报告作为运行结果。

若复核证明仅扩展 `public_ascii_host_adapter.*` 即可同时满足直接 stream 与显式 Host 复用，则应删除
候选新 adapter 文件，而不是保留两套状态机。判断标准不是文件少，而是**同一个 compiled owner、每个
候选一次 Decode、一个冻结后缀真源和一套结果物化**。

### 3.2 单步调用与回调契约

公开 Host 的真实顺序是：Framer 形成候选 → Codec Decode 一次 → candidate observer → 成功时 business
sink；observer 请求 STOP 仍会让当前成功候选完成一次 business sink，然后停止后续候选
（`src/public_api/host_endpoint.cpp:223-301,435-499`；已有公开测试亦覆盖
`tests/public_api/public_host_endpoint_tests.cpp:400-420,502-528`）。据此固定 Lab adapter 规则：

1. 每次 Submit/Continue 最多向 Host 发起一次 Push/Continue；每个同步调用最多接受一个候选。
2. **失败候选**仅在 candidate observer 中调用一次已有 Decode 结果物化并返回 STOP；business sink
   不会被调用。
3. **成功候选**的 observer 只记录候选/Decode事实并返回 STOP，不复制字段；business sink 只调用一次
   已有成功结果物化并返回 STOP。不得 observer 和 business 各复制一份，也不得为了 UI 再 Decode。
4. 完成后交叉验证 `candidates/decode_attempts/decode_successes/decode_failures`、
   `observer_callbacks_returned/business_callbacks_returned`、framing candidate 数与本地回调次数。
   任何不一致都是本地 materialization failure：不发布候选 DTO，保留 Host/Framer 消费事实并将该 Flow
   标为 Reset required。
5. business sink 未正常返回时，即使 Codec 已成功也不发布业务成功；保留 `codec_attempted/status`、
   generation、consumed 和 callback failure 事实。此边界避免重复 A2 初版的 sink 路由/结果来源问题。

### 3.3 冻结后缀、C/M、STOP 与 Continue

- 接收新 chunk 前先以公开静态事实确认选中 Pipeline 是受支持 stream strategy；再从 idle Observe
  获取 C/work limit。输入必须 `0 < size <= C`；M 只约束 Framer 内一条候选记录，不替代 C。
- 新 chunk 先完整复制到 Flow 自有 `frozen_input`，cursor 置 0。一次动作只提交
  `[cursor, frozen_input.size)`；返回后必须先验证 `bytes_consumed <= submitted_size`、最多一个候选、
  回调计数一致，再以饱和检查推进 cursor。只要 cursor 未到尾，后续只能重提**尚未消费后缀**，不能
  从 chunk 开头重放。
- STOP 后 `bytes_consumed` 是已提交切片的精确前缀。当前候选无论 Decode 成败都已被消费；失败候选
  后的合法记录仍留在 frozen suffix，下一次 Continue 才处理。
- cursor 已到尾时释放 frozen input；只有 `Observe.has_internal_work=true` 才调用空 Continue。
  `WORK_BUDGET_REACHED` 不等于错误：有后缀就 Push 后缀，只有内部 pending 就 Continue。无后缀且无
  internal work 时 Continue 必须本地拒绝，避免空调用造成无进展循环。
- Framer `status != OK`、callback failure、计数溢出/不一致、物化异常或连续无进展进入 Reset-required。
  已报告的 consumed 不回滚、不重放；候选 DTO 原子清空。`CODEC_FAILED` 本身不是 Framer 故障：若
  observer 正常复制失败诊断且 Host 未要求 Reset，可保留后缀并允许下一步。

### 3.4 Reset、Flow 和预算

- Reset 只作用于指定 binding/Flow。先调用 `HostEndpoint::Reset(old_handle)`；成功会推进 generation 并
  使旧 Handle stale（`include/pae/host_endpoint.h:167-180`）。随后必须用原 endpoint/action/flow
  重新 `Find`，校验新 Observe 成功后，才原子替换本地 Handle 并清 frozen input、cursor、计数、
  no-progress 和 fault。Reset 或 Find 任一步失败时保持 faulted，不把旧 Handle 当新句柄继续使用。
- 两个 Decode Flow 各有独立 Handle、冻结输入、cursor、generation 和计数；一次 Reset 不得改变另一个
  Flow。Tab/文档身份不进入 PAE，但必须进入 Lab operation identity，防止跨 Session 发布。
- adapter 建立前独立计算 owner、Host、Handle/Flow、冻结 capacity、静态描述和结果 DTO 的逻辑准入；
  使用饱和加乘，instance/replacement 在发布前原子拒绝。显式 `reserve(C)` 必须与预检数量一致；C 与 M
  不能重复计费或遗漏。候选/诊断/result 复制继续受 A1 `max_result_bytes` 等限额；逻辑计费不宣称
  allocator 或 RSS 硬上限。

## 4. Qt 接线最小分片

非 Qt D/R 通过并经总控复核后，Qt 片才可开始。建议只改以下既有分支：

- `schema_dispatch.*`、`compile_worker.*`：把明确的 0.11 路由到 public compiler；仍只读顶层
  `schema_version` 作 dispatch，一请求一个编译器，失败不回退 private。0.10/Binary/legacy 分类保持。
- `document_session.h/.cpp`：`PreparedDocument` 为 0.11 持有同一 public owner/stream adapter；沿用现有
  stream Session 表面和 A2 `DocumentDescription`/结果 DTO。把目前 `ascii_adapter` private 调用点替换
  为 public stream adapter，但不改 Encode/complete Decode 的 A2 路径。
- `ascii_host_adapter.h/.cpp`：仅在显式 Host Apply 路径复用非 Qt public stream owner；去掉 0.11 对
  private `HostObserverAdapter` 的运行依赖。候选观察、成功业务回调和复制仍由非 Qt adapter 完成，Qt
  facade 不接 borrowed view。
- `document_tab.h/.cpp`：保留现有 Submit/Continue/Reset 按钮、冻结状态提示、表示切换确认、Flow/Tab
  草稿和 Apply 候选发布；只消费自有 DTO。不得在 UI 中查询私有 Plan、拼接 CRLF、重新 Decode 或
  根据文本猜 strategy/M。
- `description_mapping.*` 原则上只消费新 adapter 已映射的静态 stream 字段；无差异则不改。
- 对应 `tools/protocol_lab_ui/CMakeLists.txt`、`tests/protocol_lab_ui/CMakeLists.txt`、
  `ascii_stream_session_tests.cpp` 和新的 0.11 public 专项；保留旧测试名或新增清晰 public 测试名由实施
  派发决定，不能让 private 测试静默变成 public 证据。

### 4.1 发布与修订边界

- 首次打开：completion 的 `document_id/load_revision` 与当前 Session 匹配后，先完整准备 public owner、
  描述、stream adapter 和预算；全部成功才递增 `plan_generation` 并发布。迟到/过期 completion 销毁，
  不触碰当前文档。
- 流操作：operation identity 至少绑定 document、load、plan generation、pipeline selection、input 和
  request revision、binding/Flow、Host generation。本次结果回到 Session 时再核当前键；过期 DTO
  丢弃，不能覆盖新草稿或新结果。
- Apply：候选重新公开编译一次并完整准备 adapter；准备失败、资源拒绝、revision/config hash 不匹配
  或用户取消都保留旧 owner、Handle、Flow frozen suffix/cursor、草稿和结果。只有成功 publication
  才切换 generation、销毁旧 owner，并清理被替换状态。
- reload/close：用户取消保留当前 owner 和全部流状态；确认后才开始新 load/关闭。新 load 发布前旧
  completion 依 revision 拒绝。不得因为 0.11 public 化改变 0.10 或 Binary 的取消语义。
- 表示切换：存在 frozen suffix、buffered bytes、internal work、discarding phase、reset-required、结果
  或失败时都属于 discardable state；取消切换必须原样保留 UTF-16 草稿、cursor、Flow 和展示。

## 5. 后续自动回归计划（本批未执行）

### 5.1 非 Qt Debug/Release

1. 静态事实门：0.11 ASCII CRLF 返回明确 input kind/strategy/M（M 含 CRLF）；0.10 complete record
   不伪造 M；错误 owner/index/strategy 原子拒绝；没有 JSON 私读。
2. CR/LF 跨 chunk、半包、粘包两个记录：每步至多一个候选；STOP 精确 consumed；只续提 suffix，
   第二条不重放。
3. work budget 到达、`DELIVERY_PENDING`、仅 internal work 的 Continue；无 suffix/internal work 时本地拒绝；
   C 小于/等于 chunk 边界，验证 C、M、work limit 相互独立。
4. 成功、失败、零字段候选：每候选 Decode 恰好一次；observer 与 business 计数分离；失败没有 business
   输出，成功只复制一次；失败后合法 suffix 可继续。
5. 超长候选进入 discard、跨 chunk 找到 CRLF 后恢复；malformed/discarded/candidate/Decode/business
   计数分别正确。
6. candidate/result 复制 allocation failure、回调异常、计数溢出/不一致和无进展：保留 consumed 与
   Codec/Host 事实，不发布半成品，指定 Flow 要求 Reset；另一 Flow 不受影响。
7. Reset 使旧 Handle stale，重新 Find 后 generation 更新且可恢复；仅清目标 Flow；直接 stream 与显式
   Host 走同一物化，A1/A2 complete record 行为保持。
8. instance/replacement、冻结 capacity 和结果预算独立 exact/minus-one；Testing-off 不含测试钩子。

### 5.2 Qt/受影响回归 Debug/Release

- 0.11 首次打开与 Apply 都只调用 public compiler 一次；无 private Plan/Core/Framer 运行依赖；无
  fallback。0.10 public A2、Binary H2、legacy 和 Testing-off 门禁回归。
- direct stream 与显式 Host：Submit/Continue/Reset、两个 Flow、两个 Tab、pipeline 切换；不同草稿和
  结果不串写，revision 过期结果不发布。
- Apply 无效候选、资源拒绝、取消、成功替换；reload/close 取消；表示切换取消；全部核对 frozen
  suffix/cursor、草稿、结果和 owner 是否按事务边界保留或替换。
- CR/LF 分片、粘包、未知/失败候选后合法候选、超长丢弃恢复、callback/copy failure Reset、成功字段
  Raw/Logical/range 与失败清旧。确认候选观察不等于业务成功，不增加第二次 Decode。

实施后建议最多三组简短人工烟测，不在本批执行：

1. 一个 Tab 以 CR/LF 分片提交，再粘连两条记录，观察 STOP 后 Continue 只出现下一条，最后 Reset。
2. 同一显式 Host 的 Flow 0/1 使用不同半包/后缀往返切换，确认草稿、cursor、结果隔离。
3. 保留半包时尝试 Apply/reload/表示切换并取消，再确认原流可继续；随后成功 Apply，确认旧流被替换。

## 6. 未验证、风险与停止点

- 本轮没有运行编译器、Framer、Host、Qt 或测试；所有行号是当前检查点的静态证据。历史 D/R/人工
  通过不等于 0.11 public 迁移已实现。
- PAE 并行预检尚未在本报告中假定具体公共类型名、枚举值或错误码。若其证明静态事实无法在现有
  frozen representation 下无分配提供，需由总控先收敛公共契约和计费，不得由 Lab 私读 Plan 补洞。
- 最大风险是同时保留 direct Framer 路径和 Host stream 路径后形成两套冻结状态/物化。实施前应选定
  一个非 Qt owner 为真源；若必须支持两种入口，共享物化和状态不变量，分别测试，不共享 borrowed view。
- 历史 Qt 访问违例不因本预检或未来 public 迁移自动关闭；真实通信、Linux、硬件、Golden、发布包和
  包外 Qt UI 也不在本片范围。

本报告落盘后停止写入。0.11 改路由、非 Qt adapter、Qt/UI 接线和任何构建测试均等待总控后续精确派发。
