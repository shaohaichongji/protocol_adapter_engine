# ASCII 公开消费最小契约

2026-09-18 更新：SDK/A1/A2 已在本地检查点 `dbf4798` 收口；用户授权推进 0.11。第 6 节现为静态 framing 查询的实施契约，具体派发以综合计划为准。下方初始授权描述为历史，不代表当前状态；无新的 Git/发布授权。

实施授权更新（2026-09-15）：用户已授权第 7 节公开事实首片实施及相称验证，由《子任务推进》执行；Lab 迁移、流式静态描述及 Git/发布仍未授权。具体文件范围和构建停点见综合计划当前派发。下文“仅文档”描述为契约落盘时点，不否认本次独立授权。

日期：2026-09-15。状态：用户已同意最小路线并授权契约落盘；**仅完成文档细化，代码实施、构建验证和 Lab 迁移尚未授权**。本文定义后续公开事实首片的行为边界，不宣称下列拟新增接口已经存在或 SDK 已支持。

## 1. 依据、取舍与范围

现场为 `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`，共享工作树已有大量变更，暂存为空。依据两份独立静态报告：[PAE 覆盖核对](../archive/engineering-20260921/pae-public-api-stage4-ascii-gap-review.md)、[Lab 消费需求](../archive/engineering-20260921/lab-public-api-stage4-ascii-migration-review.md)。它们是调研输入；关于实际范围、控制字符列表和切片顺序的不同建议以本文合并决定为准，不回改历史报告。

沿用 [ASCII 引擎契约](ascii-text-codec-minimal-contract.md)、[Lab 离线契约](lab-ascii-offline-integration-contract.md)、[公开 Codec 设计](pae-public-codec-slice-design.md)和[公开 Host 契约](pae-public-host-stage2b-contract.md)的执行语义。本片不改变 Schema、冻结 Plan 接受域、匹配/编解码算法、指纹或默认能力开关。

| 决定 | 本片边界 |
| --- | --- |
| 冻结 ASCII 描述 | 公开 RX/TX 有序片段、按动作字段引用及字段/记录长度界限；只读、应用无关 |
| RX 实际位置 | 明确成功 ASCII BYTES 是本次输入连续子区间，包括零长度位置；暂不新增 range accessor |
| TX 实际位置 | Lab 按公开 TX 片段和本次输入长度投影，逐段核对成功输出；不增加 Encode range 结果 |
| 失败消息身份 | Codec 与 Host candidate 同源传递已唯一确定的可选 Message 身份；失败不发布有效 record |
| 控制字节列表 | 暂不公开；字符校验仍由 Core 最终负责，不把缺失列表解释为不允许控制字节 |
| 流式静态描述 | input kind、strategy、最大帧长 M 在 stream 接线前另片补齐，不并入首片 |

最后两项控制首片规模。旧 Lab 离线契约第 4 节“DTO 包含控制字节约束”在本次 public-only 迁移中调整为可缺省的未知描述：不要求复制列表、不伪造空白名单，不缩减引擎接受域；现有未迁移分支不必删除该成员。将来若 UI 实际需要展示或预检白名单，再单独补公开描述。

PAE 不依赖 Qt/Lab，不提供表格、高亮、Tab 或草稿接口；Lab 不重解析 Schema、不成为第二个协议执行引擎。Binary 物理查询继续仅支持 Binary，不借本片扩大其语义。

## 2. 只读描述的最小公共形状

本节冻结逻辑字段和查询行为。具体 C++ 名称可沿用现场命名风格，以下名称是实施建议而非当前可调用 API；改变所有权、失败语义或字段内容需先报告总控。

- `AsciiAction`：明确 Decode / Encode，不从方向字符串推断。
- Message action 查询（例如 `CompiledProtocol::AsciiActionDescription(message_index, action)`）：返回所属 Message、动作、记录 `ByteLengthBounds`、片段数。动作不存在与动作存在但零字段不同；literal-only 动作仍有非空片段。
- Segment 按 action 内 ordinal 查询：返回 `LITERAL` 或 `FIELD`。LITERAL 给有长度的借用字节视图，允许嵌入 NUL；FIELD 给当前 compiled owner 下的 Message/Field 身份。两者是互斥有效负载，不用魔法空字符串表示字段。
- ASCII Field 查询：返回字段身份、BYTES 最小/最大字节长度、Decode/Encode 是否引用。引用必须与有序动作片段一致；同一字段在一个动作至多出现一次，保留既有规则。
- 作者名称、说明和枚举等继续用已有公开描述，不在新描述中重复复制。Pipeline 的允许消息和动作可用性继续以 `PipelineMessageExecution` 为准；Message 模板存在不等于任意 Pipeline 均可执行。

所有事实从已经严格编译的冻结状态读取，不重新解析 JSON、不执行 Decode/Encode。片段顺序及字面量按冻结动作的实际顺序返回，不承诺 JSON 原始排版或源文本位置。记录 min/max 是配置界限，不是本次实际长度。

查询须明确区分成功、无效 compiled owner、越界/非法 selector、非 ASCII 表示、动作不存在及内部契约错误。失败不返回貌似有效描述；不能把动作不存在编码成“成功且空模板”。动作/字段不存在均不得回退到另一个动作。类型可以复用已有合适的查询结果机制，但不得修改现有 Binary 查询状态的含义。

返回的索引和值结构可复制；借用 literal 的寿命依附冻结 compiled state，与现有作者 `string_view` 一致：消费者保留对应 `CompiledProtocol` 所有者或复制字节，不因一个数值索引/描述结构存在就假定所有者仍存活。跨 owner 的相同索引不具同一身份。Lab UI 长期存储只保留自己的有界 DTO。

优先按索引读取已有冻结描述，查询不做逐帧分配、不暴露私有 Plan/span 类型。如 facade 新增缓存/容器，必须计入编译元数据和分配报告，溢出/预算不足在发布前拒绝，不隐藏费用。不得默认增加 Core Workspace；若现有表示无法满足此边界，停止并报告。

## 3. 成功 Decode 的输入切片与实际位置

仅对**成功 ASCII Decode** 固定以下公开合同：`DecodedFieldView::Bytes()` 返回原调用输入/本次 Host candidate frame 的连续子区间，值不经过转码、重排或独立复制。对于字段偏移 `o`、长度 `n`、输入长度 `N`，满足 `0 <= o <= N`、`n <= N-o`，非空输入上的指针为输入首地址加 `o`。

允许空字段时，视图仍携带其真实起点和 `size=0`；尾端空字段可指向输入尾后位置，不得用 null 丢失位置。空字段不是缺少字段，Lab 显示 `{o,0}`，不得高亮相邻字节。现有 ASCII 合法记录非空，此条不新增空记录接受域。

视图同时受两种寿命约束：record/field 依现有 Codec epoch 规则失效；BYTES 还要求原输入存活且未被改变。Host 中 frame 和字段视图仅在对应同步回调有效，必须在回调内完成范围核对和有界复制，不把 borrowed 指针排队到 UI。未改变并发、重入和 move 规则。

Lab 先核对 owner/Message/Field，再检查地址及长度是否落在本次帧内；检查采用有溢出保护的地址区间方式，不先对未经验证的任意指针作相减。复制字段与帧后，以核定 offset 关联 UI 自有帧。不得通过字段内容搜索、Logical 反算或再次 Decode 获取位置。

此决定将当前实现事实升级为待实现头注释及专项验证支持的合同，不以目前一个非零切片测试证明全部边界已覆盖。若零长度等边界无法在现有实现下可靠满足，应报告并重新讨论同次 range accessor，不私自采用错误偏移或扩大 Core。

## 4. 成功 Encode 的 Lab 展示投影

PAE 仍只调用一次 Encode，内部 TX 独立复核保持不变。仅在返回 OK 且有效输出长度确定后，Lab 使用同一 compiled owner、同一所选 Message 的公开 TX 片段和本次原始 BYTES 输入：

1. 游标从 0 开始，所有加法和区间检查有界。
2. LITERAL：核对该区间与 literal 字节逐字节一致，再前移。
3. FIELD：按明确字段身份取本次输入及长度，核对长度界限和输出对应区间的字节一致，再记录实际 `{offset,length}`。不按内容搜索位置，不借用上次输入。
4. 最终游标必须等于有效输出长度；未引用字段不生成动态范围，零长度字段保留空范围。

这只是展示映射自洽校核，不是第二套 TX parser、终止匹配器或字符校验器，也不替代 Core TX reviewer。不得 RX Decode TX，不伪造独立 Review Decode 调用或计时。映射不一致/复制失败时，Lab 报本地物化失败、清除有效展示与动态高亮；保留“Codec 已返回 OK”的诊断事实，但不宣称展示有效，也不篡改原 Codec 状态。

## 5. 唯一匹配身份与失败零交付

拟在公开 `DecodeResult` 和 `HostCandidateView` 增加同源可选 `matched_message_index`（最终命名可按风格调整）。含义是**本次 Core 已唯一选择的消息**，不是解码成功标记。

- 成功时应与 `record.MessageIndex()` 一致，包括 literal-only 零字段成功。
- 唯一匹配后发生字符、容量或后续校验失败时，可以保留 Core 已知身份；不得据失败字段猜身份。
- 无匹配、匹配歧义及尚未产生唯一匹配的参数/资源早拒绝时为空。以本次 Core 匹配进展为准，不能仅按错误枚举猜有无身份（后续失败可能复用错误状态）。
- 不新增匹配尝试、不扫描其他消息、不读取前一次结果；发布前校验身份属于本 compiled owner 和本次候选范围。
- Host 直接 Decode 与流式候选使用同一 public Codec 结果透传，失败不进入成功业务 sink。早拒绝没有候选时不额外制造 observer 回调。无需在 HostOperationResult 再加入可能混淆多候选的“最后消息”。
- 失败的 `record.HasValue()` 仍为 false，不交付部分字段；可选身份和原候选帧仅作诊断。Lab 清掉旧成功字段、Raw/Logical、动态范围、高亮及侧栏，再使用本次明确返回的诊断身份。

## 6. 后续流式片边界

2026-09-18 总控根据两份预检定稿，采用独立查询，不扩展旧 `StreamFramingCapability` 布局：

- 在 `pae/stream_framer.h` 声明 `QueryPipelineFramingDescription(const CompiledProtocol&, std::size_t pipeline_index) noexcept`，返回 `PipelineFramingQueryResult`，函数导出为 `PAE_API`。
- `PipelineInputKind`：`COMPLETE_RECORD`、`STREAM_CHUNK`。
- `PipelineFramingStrategy`：`COMPLETE_RECORD`、`FIXED_LENGTH`、`SYNC_FIXED_LENGTH`、`SYNC_LENGTH_FIELD`、`ASCII_CRLF`。
- `PipelineFramingQueryStatus`：`OK`、`INVALID_COMPILED_PROTOCOL`、`PIPELINE_OUT_OF_RANGE`、`INTERNAL_CONTRACT_VIOLATION`。
- `PipelineFramingDescription`：`pipeline_index`、`input_kind`、`strategy` 和 `std::optional<std::size_t> maximum_candidate_frame_bytes`（M）。
- `PipelineFramingQueryResult`：默认 invalid 的 `status` 与默认空的 `std::optional<PipelineFramingDescription> value`；仅 OK 返回 value。

完整记录查询成功，返回 COMPLETE_RECORD/COMPLETE_RECORD、M=nullopt；非 stream 的拒绝属于执行器创建或 Lab stream 准入，不属于静态描述查询失败。此决定统一 Lab 预检中“非 stream 必须失败”的笼统表述。

stream 的 M 映射：FIXED_LENGTH / SYNC_FIXED_LENGTH 取冻结 `frame_length_bytes`；SYNC_LENGTH_FIELD / ASCII_CRLF 取冻结 `maximum_frame_length`。ASCII M 包含终止 CRLF。未知枚举、非法 input/strategy 组合、坏 profile 索引、零 stream M、超 hard limit 或不可表示的 size_t 容量返回 INTERNAL_CONTRACT_VIOLATION，value 为空；无效/moved owner 与 Pipeline 越界分别返回对应状态。非法 Schema 仍由编译器拒绝，不以查询扩大接受域。

返回值完全自有，无借用指针；owner 销毁后值仍安全，但 Pipeline 索引只对原 owner 有身份意义。只读同一冻结 Plan，不解析 JSON、不创建 Workspace、不做堆分配或缓存、不新增计费。公共枚举不随 feature macro 改变形状；未启用能力不得因此可执行。头文件直接包含 optional。

保留旧 capability 函数、导出、布局及既有状态含义；若内部复用新查询，内部契约失败映射旧 INVALID_COMPILED_PROTOCOL，不新增旧枚举或改变合法输入结果。允许最小私有无分配容量 helper 供查询与 Workspace 共用，但不得改变原执行准入、内存计费、策略算法或 Core/Plan/Schema。无分配测试使用真实分配观察，不能只比较未变的报告计数；无法安全构造内部损坏测试时说明限制，不增加生产测试接口或使用未定义行为。

本片不公开 profile id、sync 字节或长度字段细节，不修改 Host 生命周期。新增 C++ API 不宣称稳定 ABI 或任意旧二进制兼容。

运行有效 submit/work 限和 phase 继续来自实例 Observe；chunk 容量 C 不等于跨 chunk 帧上限 M。现有 Push/Continue/Reset、精确 consumed、候选与业务结果分离保持不变。Lab 自有冻结后缀、游标、Flow/Tab/revision、取消/替换事务，不移入 PAE。静态查询实施后先完成定向验证，再验证新 SDK 消费；Lab 非 Qt 及 UI 接线仍须串行派发，不包含在本片实现中。

## 7. 实施顺序、职责与验收出口

1. **公开事实首片**：后续独立授权后由《子任务推进》负责 `include/pae`、`src/public_api`、相关公开测试及验证记录；涵盖第 2、3、5 节，不改 Core/Plan/Schema。总控复核后才交 Lab。
2. **ASCII 0.10 无 Qt 适配**：由《Lab应用推进》消费公开接口和自有 DTO，实现一次 Decode/Encode 与第 4 节投影；不先建同时承担全部 UI/流生命周期的大 adapter。
3. **0.10 UI/显式 Host 接线**：保留单次严格编译、动作 selector、草稿、失败恢复和取消/替换门禁。只有离线路径通过而 Apply 仍私有，不计完整迁移完成。
4. **0.11 stream 片**：先补第 6 节静态事实，再复用完整记录结果物化并接流状态。

共享 CMake、公共接口和主线文档串行协调。各任务完成后按外层 AGENTS 规则向总控反馈一次并停止；本文不是任务派发，也不授权 Stage/Commit/Push、打包发布或目录删除。

公开事实首片未来最低验证：Windows Debug/Release 定向测试双向/单向/literal-only 描述，固定/变长/末尾/零长度 RX 切片，嵌入 NUL literal，错误索引/动作与非 ASCII 查询拒绝；成功、唯一匹配后失败、未知/歧义身份及 Host 同源透传，失败零交付和旧值不泄漏；借用失效、预算边界及受影响 Binary 回归。公开头自包含及受影响 shared 导出/外部链接需核查。未知/歧义测试使用真实可达的已有测试方式，不为覆盖强造 Schema 接受域。

新增 API 的 SDK 消费在消费迁移交接前以新候选验证，旧 final6/candidate2 不视作含新 API；源码/静态/动态交付验证可集中在该接口片稳定后做，不要求每次文档或小修都重打五包。确切构建目录与命令由后续派发限定，成功且未变的检查不重复。Lab 后续专项覆盖 TX 重复内容不误定位、失败清理和 Flow 隔离；本契约片不安排人工点击。

## 8. 本轮落盘与未验证范围

本轮只新增本契约并同步工程入口、综合计划、路线和外层协作索引；验证限文档内容、链接目标与修改范围检查。没有源码修改、构建、测试、SDK 生成、部署覆盖或 Git 写操作。静态判断不等于实现或验证完成。

历史 ASCII Qt5Core 访问违例无调用栈、根因未定，继续独立保留；不因本契约或烟测焦点修复而关闭。Linux、真实协议 Golden、硬件/现场、稳定 ABI 和正式发布均不在本轮。
