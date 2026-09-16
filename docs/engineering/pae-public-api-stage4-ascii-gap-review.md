# 阶段 4 ASCII/流式公开接口覆盖与最小缺口核对

日期：2026-09-15。状态：已完成派发范围，待总控复核。本文是静态审查，不是实施契约或新的构建/测试结果；本批未修改源码、配置、测试或既有文档，未运行构建/测试。历史 ASCII 烟测访问违例仍是独立未定问题，不从本报告推断根因或修复。

## 1. 现场、依据与结论先行

现场仓库为 `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`，暂存区为空；共享工作树有大量已有修改、移动与未跟踪文件，均保持原样。适用外层 `AGENTS.md`，仓库内没有更深层文件。本报告是本批唯一新增文件；没有 Stage、Commit、Push、删除、部署或网络操作。

对照依据：[综合计划当前 ASCII/流式准备段](pae-execution-delivery-organization-plan.md)、[ASCII 0.10 完整记录契约](ascii-text-codec-minimal-contract.md)、[ASCII 0.11 流式契约](ascii-stream-framing-contract.md)、[Lab ASCII 完整记录契约](lab-ascii-offline-integration-contract.md)与[流式观察契约](lab-ascii-stream-observer-contract.md)。实际公开头与实现为 `include/pae/{protocol_description,compiler,codec,stream_framer,host_endpoint}.h`、`src/public_api/`。既有验证记录只作历史证据，未在本轮重跑。

结论：**执行与流状态公开接口已足以构成一次 Decode/Encode、分块候选、STOP/精确消费的调用主干；硬缺口集中在只读 ASCII 动作模板/字段约束/完整记录长度事实与成功执行的本次字段字节范围。** 另有流式配置的 `M`、策略等静态描述缺口；`QueryStreamFramingCapability` 的 `available` 不能替代它们。应先拍板一个最小、非 UI 专用的 ASCII 描述/实际范围出口，再接 Lab 完整记录；流式 Host/Framer 算法本身无需因为迁移重做。若总控选择不保留既有按字段约束展示与精确高亮，接口门槛可以缩小，但这会改变既有 Lab 验收语义，不应在执行任务中默许。

## 2. 能力映射：已有 API、硬缺口与证据缺口

| 需求 | 现有公开接口 / 源码 / 历史证据 | 判定与所属层 |
| --- | --- | --- |
| Text/Binary 表示、Message/Field 身份、作者元数据 | `CompiledProtocol::{Protocol,Pipeline,Message,Field,MessageRepresentation}`；`protocol_description.h`。P 查询只支持 Binary physical，ASCII 返回 `REPRESENTATION_NOT_SUPPORTED`；[P 验证](pae-public-api-stage4-physical-query-validation.md)。 | 已有。Lab 自有字符串复制、Tab/Flow 身份；不能把 P 的 Binary byte range 用于 ASCII。 |
| RX/TX 独立动作、allowed Message、输入/生成/不引用、Encode 上界 | `PipelineMessageExecution` 从 allowed/candidate 集合和 `text_decode/text_encode` 得出动作，ASCII `text_encode` 的最大记录长决定 `EXACT/UPPER_BOUND`；`Field` 从 `text_encode_input` 映射 `CALLER_INPUT/NOT_REFERENCED`，见 `src/public_api/compiler.cpp:450-498,536-583`、[metadata 验证](pae-public-consumer-metadata-validation.md)。 | 已有动作**可用性**与容量上界；不是 RX/TX segment 顺序、字段长度/控制字符约束或实际输出位置。Lab 不得由 direction 字符串猜动作，也不得要求 RX/TX 字段集合对称。 |
| 完整记录 Decode、typed BYTES、零字段成功、失败零字段交付 | `CompleteRecordCodec::Decode` 只在 Core OK 时发布 `DecodedRecordView`，BYTES `ByteView` 取实际 slot，见 `src/public_api/codec.cpp:321-353,503-507`；ASCII 公开专项 `public_codec_tests.cpp:376-478`。 | 执行主干已有。`record.HasValue()` 与 `FieldCount()==0` 可区分纯字面量成功和失败；`UNKNOWN_MESSAGE/AMBIGUOUS_MESSAGE/ASCII_CHARACTER_NOT_ALLOWED/OPERATION_NOT_SUPPORTED` 已有状态。Lab 自有 escaped/Hex 格式和自有结果复制。 |
| Decode 本次字段 range | Core 扫描 `WalkTextAction` 时确定 `(field_index,offset,length)`，成功 slot 保存 `input.data+offset`，见 `src/protocol_core/complete_record_codec.cpp:279-332,1496-1506`；旧 Lab 用同一输入的地址有界核对借用 range，见 `tools/protocol_lab_ascii/ascii_offline_adapter.cpp:258-268,452-471`。公开专项只断言一个非零 BYTES 指针 `reply.data+3`。 | **接口/契约缺口。** 当前 `DecodedFieldView` 无明确 offset/range。调用方可在当前实现中做有界地址关联，但公开注释只说明 BYTES 借输入，没有冻结每字段零长度指针位置或正式 range 结果；不宜让跨包 Lab 的物理定位依赖指针反推。需公开同调用成功 range，零长返回存在的 `{offset,0}`，失败不返回。 |
| Encode BYTES 与本次实际 TX 字节/range | `Encode(pipeline,message,values,output)` 返回真实 `bytes_written`，Host 也在回调中提供 `bytes/bytes_produced`；`src/public_api/codec.cpp:356-431`、`src/public_api/host_endpoint.cpp:535-570`。旧 Lab 读取私有 TX segment，按 literal 与输入长度累加并核对最终长度，见 `ascii_offline_adapter.cpp:545-579`；[Lab 契约第 4/5 节](lab-ascii-offline-integration-contract.md)。 | 有输出与有效长度，**无实际字段 range / TX 模板公开事实**。按输入 BYTES 内容搜索会与重复内容/literal 碰撞；以 RX Decode TX 则违反独立模板契约。成功 TX range 应来自 PAE 的本次输出事实，或由公开冻结 TX 模板作有界展示投影并与输出字节/有效长度核对；两种出口需拍板，均不得替代 Codec 内部复核。 |
| ASCII 字段长度/控制字节约束、动作 segment 顺序与字面量、完整 RX/TX 长度界限 | 私有 adapter 从冻结 `text_min_length/text_max_length`、允许控制字节、RX/TX segment 拷贝 UI DTO，见 `ascii_offline_adapter.cpp:39-65,106-108,123-181`。公开 `FieldDescription` 只有 kind/source；`MessageExecutionDescription` 只含 Encode 最大值。 | **硬缺口。** 现有 Lab 按字段约束准备编辑/容量并展示 RX/TX 模板；不能把 QWidget 变成 Schema 解释器，亦不能只依赖最大 output 伪称字段约束。推荐公开只读通用 ASCII action/field 约束描述，带 min/max record、按动作 ordered segment、literal bytes、Field index、BYTES min/max、allowed control mask；借用值或纯值复制的寿命要与 compiled owner 明示。 |
| literal-only、单向动作 | `PipelineMessageExecution` 和 `Field.encode_value_source` 区分未引用/不可用；公开 Codec/Host 以空 Values Encode `SEND\r\n`、零字段 Decode `ONLY\r\n`、反向 `OPERATION_NOT_SUPPORTED` 精确断言，见 `public_codec_tests.cpp:426-478`、`public_host_endpoint_tests.cpp:530-549`。 | 已有执行与动作门禁；literal 片段本身仍属于上行描述缺口。Lab 的按钮/草稿可自有，不可用动作仍由 PAE 最终拒绝。 |
| 候选成功/失败与业务结果分离 | `HostCandidateObserver` 可见原始 frame、decode_status、required count、失败 Field、tainted；仅 Core OK 进入 `HostOutputSink`；`HostOperationResult` 区分 candidates/decode attempts/successes/failures 和回调返回，见 `host_endpoint.h`、`src/public_api/host_endpoint.cpp:270-301`。历史 Host 测试含零字段成功、失败候选不进入业务、observer/business STOP；[2B 验证](pae-public-host-stage2b-validation.md)。 | 已有。失败候选的 Message index 不可用时不能猜测；API 早拒绝/超长 discard 不伪造候选；Lab 自有失败材料标注和“本步无候选”。无新候选 API 必要。 |
| 流式 Push/Continue/Reset、精确 consumed、预算、状态观察、借用 | `StreamFramer::{Push,Continue,Reset,Observe}` 和 `HostEndpoint::{Push,Continue,Reset,Observe}`；`StreamSubmitResult` 有 stop reason、consumed、candidate/discard/malformed/issue/work；`HostOperationResult` 有 framing 与 reset_required。见 `src/public_api/stream_framer.cpp:174-255`、`src/public_api/host_endpoint.cpp:434-503`；[2A](pae-public-framer-stage2a-validation.md)/[2B](pae-public-host-stage2b-validation.md)。 | 执行主干已有，Framer 候选帧与 Host 借用视图只在同步回调有效；Lab 必须在回调内有界复制，并只续提冻结后缀，STOP 当前候选不可重放。无需新增网络或第二套 Matcher/Framer。 |
| 流式策略、帧上限 `M`、完整记录/stream 配置描述 | `QueryStreamFramingCapability` 只提供 `available`，实际从冻结 profile 读 `STREAM_CHUNK`，见 `src/public_api/stream_framer.cpp:262-281`；`Framer/Host Observe` 给 phase/buffer/internal work/effective submit/work，但不提供策略和 `M`。旧 Lab 契约要求 `input_kind,strategy,M` 来自 Plan，见 [流式观察第 2/5 节](lab-ascii-stream-observer-contract.md)。 | **静态描述缺口。** 能安全选择 Push 和显示 phase，但不能按现有 UI 契约显示 CRLF 策略/计算帧上限、区分 chunk C 与跨 chunk M。建议作为通用 `PipelineFramingDescription` 查询最小扩充，不提供 Socket/路由/监听功能。运行有效 submit/work 仍以实例 `Observe` 为准。 |
| Flow、Tab、冻结后缀、输入表示、取消/替换原子性 | [Lab 完整记录](lab-ascii-offline-integration-contract.md)与[流式观察](lab-ascii-stream-observer-contract.md)规定 revision、copy、状态清理。公开 owner/view 有借用与同步防重入边界。 | **Lab 自有**，不是 PAE 公共接口缺口。P 查询无 identity token，不能用索引跨 owner 关联。 |

历史专项证明代表配置的已知行为，不证明全部字段约束/零长 range/多候选的迁移链已覆盖。本轮没有动态复现；不能将“源码可见”写为新 D/R 测试通过。

## 3. 最小调用顺序与依赖门槛

完整记录优先，分两条独立动作，不做 TX 后再 RX review：

1. Lab 严格准备配置并持有唯一 compiled owner；复制必要作者描述；从公开 `Pipeline/Message/Field`、`PipelineMessageExecution` 与拟补的 ASCII RX/TX 描述得到动作、约束及输入/输出界限。
2. Decode：Lab 解析/冻结 Hex 或 escaped 输入 → 按资源准入 → 一次 `HostEndpoint::Decode`（或 H1 同样的一次 public Codec）→ candidate observer 复制诊断帧 → 仅 success sink 在借用期复制 Field BYTES、身份、**本次 RX range** → 由 Lab revision 门禁原子发布。零字段成功用成功状态而非字段数判定。
3. Encode：Lab 按当前 Message `CALLER_INPUT` 字段和 public ASCII 约束准备原生 BYTES → 同一个 ENCODE Handle 的调用期 `message_index` 选择 → 一次 `HostEndpoint::Encode` → success sink 内复制 `output.bytes` 的 `bytes_produced` 有效区以及**本次 TX range**，失败清除旧成功/有效输出。literal-only 提交零 Values；不以 RX action Decode TX。
4. 当前 UI 的输入表示、Field escaped/Hex 格式、草稿、Flow/Tab、失败旧状态清理与 revision 属 Lab；PAE 只提供配置/执行物理事实。

流式在完整记录公开消费与范围出口收口后复用同一个适配 DTO，另建立每流 Host channel：

1. Lab 从拟补的 Pipeline framing 描述确认 stream/ASCII CRLF/`M`，从 `Host Observe` 取实际 submit/work 限；完整解析并冻结 chunk，不把整条 chunk 误作完整记录。
2. 一次 `Push`，observer 对每个候选（包括失败）在回调内复制原始候选与可用诊断；success sink 复制同一次 Decode 的有效字段/range；sink 对本步候选 `STOP`。只以 `bytes_consumed` 推进冻结 chunk 游标，候选数与 Decode 成功数分开。
3. 仅在后缀或 `has_internal_work` 存在时显式 `Continue`（有后缀则 Push 剩余后缀，只有内部工作则空 Continue）；不因半包/丢弃等待边界而空忙循环。`Reset` 丢弃本流半包、pending、后缀并重取 Handle，Lab 单独清 revision/generation 与显示；不重放已消费前缀。

无需先切“共用 adapter”：完整记录已可复用 Compiler/Codec/Host 主干，stream 在此基础上添加状态而不应强迫提前迁移全部 Flow/Qt 生命周期。若为共用 DTO 把 H2 UI 和流式状态一次性切换，耦合与验证面明显更大；推荐公开事实补齐 → 非 Qt ASCII complete adapter → UI complete → 非 Qt stream adapter → UI stream 的串行依赖。具体 Lab 写入范围由其独立报告和总控合并决定。

## 4. 建议最小公开补齐（只供拍板，不实施）

**A，ASCII 冻结执行描述。** 在 `include/pae/protocol_description.h`/`compiler.h` 及 `src/public_api/compiler.cpp` 增加按 Message/动作查询的通用只读描述：动作可用性、RX/TX min/max record、ordered literal/Field segment、BYTES min/max 与控制字节 mask。非 ASCII/坏 owner/非法索引显式失败，无部分有效 DTO；borrowed literal 的 compiled owner 寿命与既有 string_view 一致。单独测试 RX/TX 独立、decode-only/encode-only、literal-only、零长 Field、末尾变量及未知索引。若仅暴露模板而让 Lab 据输入长度投影 TX display range，须明确这是展示推导，不是执行结果/Codec review，并与成功输出的所有 literal/字段位置和有效长度自洽核对。

**B，本次成功范围。** 优先考虑从同一次 Codec 执行提供 RX/TX Field 实际 byte range（Decode 仅在 OK view 可读，Encode 仅在 OK output callback/结果可读），或经冻结 action 对成功输出提供通用有界 `Resolve`；禁止重 Decode、字段内容搜索、由 logical 反推 raw。是否将 Encode 成功范围随 `EncodeResult` 提供，还是采用公开 TX action 展示投影，**是需总控确认的最小接口形状选择**；两者都有资源/borrowed lifetime 影响，不能在本报告静默冻结。RX 的旧地址反推可做短期对照负例，但不应代替稳定范围契约。

**C，流式配置描述。** 在 `include/pae/stream_framer.h`/`src/public_api/stream_framer.cpp` 最小扩 `Pipeline` framing query：完整记录/stream、策略类别、`M`（含 CRLF），保持与 frozen profile 同源、纯值、失败关闭。已有 `Observe` 继续回答有效运行限与 phase，不给 UI 重新解析 Schema 或拿 `available` 猜策略。

门槛：A/B 应先于完整记录 UI 接线；C 应先于流式 UI 接线，但可在完整记录之后独立完成。若总控确认只需要 bytes 与状态、不保留 Field 编辑约束及高亮，A/B 可部分后置，不过须对既有 Lab 合同/人工验收影响明确拍板。无需改变 Schema/Plan 算法、指纹/历史证据、Runtime/C ABI；如实施需要 Core 或 Workspace 新槽，须另计资源并在后续授权中精确列入，不能从本报告推定零成本。

## 5. 建议验收及本轮边界

后续授权实施时建议先测试缺失事实，再做 Windows Debug/Release 串行 public 专项/头边界、Codec/metadata/Framer/Host 定向回归，Testing-off 零测试门禁；用新的 SDK 候选完成源码、static D/R、shared D/R 包外 consumer、manifest/hash 与 DLL 导出/依赖，既有 Stage3 final6 和 P candidate2 不覆盖。重点断言：RX/TX 模板不同、固定/变长/零长实际 range、重复 BYTES/literal 不靠搜索、literal-only/单向、原始候选失败与成功隔离、STOP 只消费当前候选、Continue 不重放、Reset 多流隔离、回调内复制后借用失效、编译/资源早拒绝不伪造候选。Lab complete 与 stream 各自定向 D/R 及必要一次简短人工烟测由总控另派；本报告未提出重跑整套旧矩阵。

本轮只执行读取与 Git 诊断，未构建/测试/包外消费，未查询或修改设备/网络/生产数据；不宣称 Linux、Golden、硬件、现场、正式 SDK 或稳定 ABI。未对历史 ASCII 访问违例作新诊断。报告结论待与 Lab 独立需求报告合并；未合并前不启动公共 API 实施或 Lab 迁移。
