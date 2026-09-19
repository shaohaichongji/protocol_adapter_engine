# PAE 全能力图形入口：公开消费能力审计

## 1. 审计状态与边界

- 审计日期：2026-09-19。
- 现场基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`，相对 `origin/main` ahead 1；审计开始时暂存区、工作树和普通未跟踪文件均为空。
- 本轮是源码、公开头和测试源码的只读能力审计。没有 Configure、Build、CTest、启动 Lab、网络收发、安装或修改交付目录；下文“测试覆盖”表示当前测试源码中存在相应断言，不表示本轮重新执行。
- 保护范围未改动：`deliverables/sdk/98df5e0`、`deliverables/lab/98df5e0`、既有部署、本机 Qt 和仓库外产物。
- 唯一写入为本报告。没有修改 public API、Schema、Core、Lab 源码或 CMake，也没有 Stage、Commit、Push。
- 公开接口仍是 `0.experimental.1`；当前构建覆盖 Schema 0.1～0.11，但头文件明确不承诺未来 source/binary ABI 兼容，见
  [version.h](../../include/pae/version.h#L7-L15)。因此“可供当前 GUI 消费”不能写成“稳定 SDK/ABI 已冻结”。

## 2. 结论先行

1. **最小完整记录 Encode 图形入口没有新的 PAE public API 阻断。** 当前公开层已经能给出实际
   Pipeline/Message 关联、Decode/Encode 可用性、调用方输入/常量/计算字段来源、逻辑值类型、枚举项、
   Encode 精确长度或上界，并能执行 typed Encode。GUI 可以只为 `CALLER_INPUT` 字段创建编辑器，按
   `message_index` 调用 Codec 或 Host，并只在 `status == OK` 时展示有效输出字节。
2. **现有 Binary Host GUI 的 Encode 缺失是 Lab 接线缺口，不是 PAE 执行能力缺失。** 当前 public
   Binary adapter 只接收 Decode binding，Session 明确返回 Binary Host Encode 不可用，界面也隐藏
   Encode 按钮；对应位置为
   [binary_host_adapter_public.cpp](../../tools/protocol_lab_ui/binary_host_adapter_public.cpp#L175-L188)、
   [document_session.cpp](../../tools/protocol_lab_ui/document_session.cpp#L1024-L1039) 和
   [document_tab.cpp](../../tools/protocol_lab_ui/document_tab.cpp#L2932-L2946)。
3. **“能 Encode”不等于“能完整解释本次 Encode 的每个生成事实”。** `EncodeResult` 只发布状态、
   `bytes_written/required_size`、失败 value/field 和 conversion error；它没有成功结果的逐字段 logical/raw、
   constant/computed/integrity 实际值观察。GUI 不得从 logical Decimal 反算 raw，不得靠第二次 Decode
   冒充同一次 Encode 观察，也不得把静态物理位置写成“本次已验证”。这是后续若要求完整字段级 TX
   观察时的真实接口缺口，不阻断只显示最终字节、状态和静态/按长度解析物理范围的首片。
4. **现有 metadata 足够选择编辑器种类，但不是完整输入约束模型。** 公共描述提供 logical kind、
   source、枚举和 Binary/ASCII 字节长度边界；没有统一公开整数 wire 范围、conversion scale/bias、
   全部字符约束、constant 值或 matcher 内容。GUI 可做 canonical 语法、类型、枚举和已公开长度校验，
   其余必须由 Codec fail-closed，不应复制 Schema/Core 规则成为第二套执行器。
5. **Binary、ASCII、流式切帧和 Host 都有公开可执行入口，但展示深度不同。** Binary 有 byte range/
   bit mask/动态长度解析；ASCII 有动作、ordered segment、字段长度和 CRLF framing；Framer 只公开策略
   类别及最大候选长度，不公开 sync token、length-field offset 等全部算法参数。Host 是逻辑 endpoint/
   channel owner，不是 Socket、串口或设备线程。

## 3. 公开能力矩阵

| GUI 所需能力 | 当前公开事实或执行入口 | 结论 | GUI 使用边界 |
| --- | --- | --- | --- |
| 严格编译与诊断 | `CompileProtocolJson`、`CompileDiagnostic`、move-only `CompiledProtocol`，见 [compiler.h](../../include/pae/compiler.h#L118-L203) | 可直接消费 | 编译失败时没有合法 Plan，不创建执行页或伪造 Message |
| Protocol/Pipeline/Message/Field/Enum 选择 | `Protocol/Pipeline/PipelineMessageIndex/Message/Field/Enum`；字段含 `value_kind`、`encode_value_source`，见 [protocol_description.h](../../include/pae/protocol_description.h#L172-L232) | 可直接消费 | 所有 `string_view` 与 ASCII literal 借用 compiled owner；跨线程/跨 reload 前复制到 Lab-owned DTO |
| 实际动作与 Message selector | `PipelineMessageExecution` 只对真实关联且 allowed 的 Message 返回 `decode_available/encode_available`，见 [compiler.cpp](../../src/public_api/compiler.cpp#L513-L560) | 可直接消费 | 不从 direction 文本或 Schema 版本猜动作；Encode 每次显式携带 Message selector |
| typed Encode 输入 | `EncodeValue::UInt64/Int64/Bool/Bytes/Enum/Decimal`，见 [codec.h](../../include/pae/codec.h#L85-L104) | 可直接消费 | UINT64、INT64、BOOL、BYTES、ENUM、DECIMAL64 必须保持独立类型，不用字符串或整数互相代替 |
| 输入字段/生成字段区分 | `CALLER_INPUT/CONSTANT/COMPUTED/NOT_REFERENCED`，见 [protocol_description.h](../../include/pae/protocol_description.h#L11-L15) | 可直接消费 | 只为 `CALLER_INPUT` 创建可编辑草稿；其余只读。`NOT_REFERENCED` 不能映射成可编辑 INPUT |
| Encode 输出容量 | `EXACT/UPPER_BOUND/NOT_AVAILABLE` 与 `encode_output_size`；Binary 固定、bounded record、ASCII 均由冻结 Plan 映射，见 [compiler.cpp](../../src/public_api/compiler.cpp#L527-L560) | 可直接消费 | `EXACT/UPPER_BOUND` 可预分配；仍检查 `EncodeResult.required_size`。`NOT_AVAILABLE` 不允许调用 |
| 完整记录 Encode/Decode | `CreateCompleteRecordCodec`、`Decode`、`Encode`，见 [codec.h](../../include/pae/codec.h#L205-L249) | 可直接消费 | 失败时 `bytes_written == 0`；缓冲区可能已改但不是可交付结果。借用 Decode view 在下一次受保护调用后失效 |
| Decode typed/raw | `DecodedFieldView` 发布 typed logical、enum raw/known 和 conversion raw integer，见 [codec.h](../../include/pae/codec.h#L130-L180) | 可直接消费 | raw 来自本次 Decode 的真实 workspace；不得由 logical 反算 |
| Encode 成功逐字段观察 | `EncodeResult` 没有逐字段成功 view，见 [codec.h](../../include/pae/codec.h#L196-L203) | **缺口（非首片阻断）** | 首片只展示最终 bytes、状态及可证明的物理布局；若产品要求 actual raw/generated 值，需另立应用无关接口契约 |
| Binary 静态/动态物理位置 | `MessagePhysical/FieldPhysical/Resolve*` 提供长度、integrity/computed storage、byte range、bit masks，见 [protocol_description.h](../../include/pae/protocol_description.h#L107-L145) | 可直接消费 | 这些是冻结 Plan/给定 frame size 的布局，不证明 Frame 已匹配、完整性通过或 Decode 成功 |
| ASCII 动作与布局事实 | `AsciiAction/AsciiSegment/AsciiField` 提供动作长度、ordered literal/field segment、字段长度和引用方向，见 [protocol_description.h](../../include/pae/protocol_description.h#L52-L98) | 可直接消费 | literal 借用 owner；ASCII 不使用 Binary `FieldPhysical` 冒充物理布局 |
| Binary/ASCII stream framing | `QueryPipelineFramingDescription`、`CreateStreamFramer`；策略含 fixed、sync-fixed、sync-length、ASCII CRLF，见 [stream_framer.h](../../include/pae/stream_framer.h#L46-L100) | 可执行 | 可展示 strategy 与最大候选长度、投喂 chunk、STOP/Continue/Reset；不能展示未公开的 sync/length 算法细节 |
| 逻辑 Host binding | `CreateHostEndpoint`、`Find/Observe/Reset/Decode/Push/Continue/Encode`，见 [host_endpoint.h](../../include/pae/host_endpoint.h#L165-L215) | 可直接消费 | Host 串行、non-waiting；handle Reset 后需重新 Find。它不拥有网络、设备或重试 |
| 候选失败与业务交付分离 | Host candidate observer 可见每个候选 Codec 状态，business sink 仅接成功输出；operation result 区分 codec/framing/callback，见 [host_endpoint.h](../../include/pae/host_endpoint.h#L101-L153) | 可直接消费 | `CALLBACK_FAILED` 可保留 Codec 已产出事实，但不表示业务交付成功；`status == OK` 才确认回调正常返回 |
| 资源展示/准入 | Compile、Codec、Framer、Host 都有 memory report；Host 预分配每 Pipeline 最大 Encode 输出，见 [host_endpoint.cpp](../../src/public_api/host_endpoint.cpp#L56-L89) | 可直接消费 | 是组件 accounted bytes，不是进程 RSS 或性能保证；GUI 自有 DTO/文本/草稿预算仍由 Lab 单独计费 |

## 4. 首片 Encode 图形入口的事实充分性

### 4.1 已足够的部分

首片可以不扩 public API 地完成以下闭环：

1. 编译一次配置并由单一 owner 持有冻结状态。
2. 遍历 Pipeline 的 Message 关联；只列出 `encode_available == true` 的 Message。
3. 对选中 Message 遍历 Field；根据 `value_kind` 选择编辑器，根据 `encode_value_source` 决定可编辑性：
   - UINT64/INT64：canonical 十进制输入；
   - BOOL：原生布尔控件；
   - BYTES：Binary Hex 或 ASCII escaped，长度仅按公开 bounds 预检；
   - ENUM：使用公开 enum identity/display/raw，但提交 `EnumSelector` 而不是 raw 数字；
   - DECIMAL64：精确 `{coefficient, scale}`，不经 `double`。
4. 依据 `EXACT/UPPER_BOUND` 分配 caller-owned buffer，调用 `CompleteRecordCodec::Encode`；需要 endpoint/
   channel 生命周期时调用 `HostEndpoint::Encode`，Host 已按整个 Pipeline 的最大输出预留内部 buffer。
5. 只在 Codec/Host 成功时发布输出；失败展示 status、failed value/field、conversion error 和 required size，
   清除旧成功结果。
6. 对成功 Binary 输出，以实际 `bytes_written` 调用 `ResolveMessagePhysical/ResolveFieldPhysical` 显示
   byte/bit 高亮及 integrity/computed storage；这仍是布局展示，不冒充 actual raw/generated value。

现有公开测试源码已经提供可复用的独立预期和失败断言：

- Binary typed Encode、独立期望字节、type mismatch、enum selector、buffer-too-small、失败零交付：
  [public_codec_tests.cpp](../../tests/public_api/public_codec_tests.cpp#L137-L217)。
- INT64/BOOL/BYTES/ENUM 组合和 Decimal logical/raw Decode、Decimal Encode 越界：
  [public_codec_tests.cpp](../../tests/public_api/public_codec_tests.cpp#L290-L372)。
- caller/constant/computed、Binary exact、bounded/ASCII upper bound、单向动作：
  [public_consumer_metadata_tests.cpp](../../tests/public_api/public_consumer_metadata_tests.cpp#L73-L151)。
- Host Binary Encode 预分配输出、回调故障/Reset、热路径无 engine allocation：
  [public_host_endpoint_tests.cpp](../../tests/public_api/public_host_endpoint_tests.cpp#L451-L485)。
- ASCII typed/literal-only Encode、同一 Handle 的调用期 Message selector、跨 Pipeline selector 预拒绝：
  [public_host_endpoint_tests.cpp](../../tests/public_api/public_host_endpoint_tests.cpp#L530-L595)。

### 4.2 当前 Lab 接线必须先纠正的映射边界

这两项是 **Lab 实施缺口**，不是 public API 缺口；本轮没有修改：

1. `public_binary_description.cpp` 当前把 `ValueKind::DECIMAL64` 的基础 `FieldValueType` 映射成
   `INT64`，仅设置 `decode_decimal64=true`，见
   [public_binary_description.cpp](../../tools/protocol_lab_ui/public_binary_description.cpp#L9-L17) 和
   [public_binary_description.cpp](../../tools/protocol_lab_ui/public_binary_description.cpp#L93-L105)。
   但 Encode 编辑器只有 `decimal_conversion=true` 才解析 `coefficient@scale`，见
   [field_table_model.cpp](../../tools/protocol_lab_ui/field_table_model.cpp#L473-L489)。Binary Encode 首片必须
   让公开 `DECIMAL64` 形成 Decimal draft，不能降级为 INT64。
2. 同一映射把 `EncodeValueSource::NOT_REFERENCED` 转成 `FieldEncodeSource::INPUT`。即使当前 Binary
   complete 配置通常不会产生该组合，统一图形能力入口也不得把公共四态压成可编辑三态。可以在
   Lab-owned DTO 增加明确 `NOT_REFERENCED`，或至少以独立 participation flag 保留事实；不能仅靠
   direction 或默认值猜测。

## 5. 不能从现有公开事实推导的内容

### 5.1 不得从 logical 反推 raw

- Decode 的 conversion raw 由 `DecodedFieldView::ConversionRaw*` 明确发布；这是唯一可直接展示的实际
  raw 事实。
- Encode 成功没有对应 field view。尤其 Decimal conversion 可能涉及比例、偏置、rounding/范围规则；
  GUI 不能根据 logical 值复制 Core 算法反算 raw，也不能从最终字节结合大小端和位宽自行解释。
- 对普通 UINT64/INT64/BOOL，GUI可以把提交的 caller input 标为“输入值”，但仍不能把它写成所有生成
  字段的实际输出观察；constant/computed/integrity 不来自调用方输入。
- 如果后续验收要求“Encode 后表格显示每个字段实际 logical/raw/generated value”，应单独设计
  `Encode observation`，事实必须来自同一次成功 Core 执行；本报告不建议为 GUI 暴露 private Plan。

### 5.2 不得把布局查询当作执行证明

`MessagePhysical/FieldPhysical` 注释已经明确：结果来自不可变 Plan，不证明任何 Frame 匹配、通过
integrity 或成功 Decode。`Resolve*` 只把合法 frame size 代入有界布局。因此：

- 成功 Encode 后可以据 `bytes_written` 展示物理范围；
- Encode 失败时不能展示失败 buffer 中的“有效字段”；
- Decode 未得到唯一 matched Message 前不能选一个 Message 的布局冒充候选；
- integrity storage 的 byte range 不等于公开了 algorithm、coverage range、expected/actual 或通过状态。

### 5.3 不得把公开 metadata 扩写成完整约束模型

当前 Field 描述没有统一发布：普通整数 wire byte width/byte order 的语义约束、转换 scale/bias、常量值、
matcher 内容、全部 numeric min/max、全部 ASCII allowed-control 列表。物理查询可给 byte range/mask 和
BYTES 长度，不能据此推导所有可表示逻辑值。GUI 首片应：

- 自己只做 canonical 文本、类型、枚举选择及已公开长度边界检查；
- 把 `TYPE_MISMATCH`、`VALUE_NOT_REPRESENTABLE`、`BYTES_LENGTH_MISMATCH`、
  `CONVERSION_*`、`FINAL_REVIEW_FAILED` 等执行失败准确展示；
- 不复制 Schema/Validator/Core 规则来追求“提交前全预测”。

## 6. Stream、ASCII 与 Host 的展示边界

1. `PipelineFramingDescription` 足以让 GUI 区分 complete record 与 stream chunk，以及 fixed、sync-fixed、
   sync-length、ASCII CRLF；公开测试覆盖三种 Binary 策略、ASCII CRLF、完整记录无最大候选长度和查询
   无分配，见 [public_stream_framer_tests.cpp](../../tests/public_api/public_stream_framer_tests.cpp#L224-L297)。
2. 该描述没有 sync bytes、length-field offset/width/order 等完整 framing 参数。首片可以显示策略名、
   最大候选字节、当前 phase/stop reason 和候选；若要求“图形化展示每个 framing 参数”，存在新的
   public description 缺口，不能解析 Schema 或 private Plan 绕过。
3. ASCII 的 ordered segments 和字段 min/max 已公开，可按本次输入长度投影 Encode range；Binary
   `FieldPhysical` 对 ASCII 返回 `REPRESENTATION_NOT_SUPPORTED`，两套展示不得混用。
4. Host 已覆盖 complete Decode、stream Push/Continue、Encode、observer/business callback、channel
   fault isolation 和 Reset。它提供逻辑 binding 生命周期，不提供 Socket/串口配置、连接、重试或设备
   线程；“全能力图形入口”不应借此扩成通信框架。

## 7. 资源、所有权与线程约束

- `CompiledProtocol` move-only；metadata 的 `string_view`、ASCII literal 及查询索引依赖来源 owner。
  GUI worker 必须在 owner 有效期间复制为 Lab-owned DTO，再跨线程发布。
- `CompleteRecordCodec` 的 Record/Field view 在下一次成功 guarded Decode/Encode、owner move 或析构时失效；
  BYTES 还借用本次输入，见 [codec.h](../../include/pae/codec.h#L213-L223)。不能把 view 直接放进 Qt model。
- Host 同一实例操作是 serialized/non-waiting；Reset 后旧 handle stale，见
  [host_endpoint.h](../../include/pae/host_endpoint.h#L167-L192)。GUI 的 Tab/Flow owner 应保持现有隔离，
  不共享同一 workspace 做并发调用。
- Codec、Framer、Host 的 memory report 可作为 admission 依据；Lab-owned description、typed draft、Hex/
  Unicode 文本、结果副本仍须单独有界计费。测试中的“engine hot operation 无分配”不是 GUI 无分配或
  进程 RSS 上界。

## 8. 建议的最小实施顺序

### G1：public Binary complete Encode 图形入口

这是当前最小可见缺口：0.9 public Binary Host 文档已有完整 Decode/物理展示，但 Encode 被显式隐藏。
建议只做以下内容：

1. 在现有 public Binary owner 上增加 Encode binding/channel 或等价直接 Codec owner；继续单次编译，
   不重新读 Schema、不双编译、不 fallback private 执行。
2. 复制 `PipelineMessageExecution` 和 Field/Enum facts，保留四态 source；修正 DECIMAL64 typed draft。
3. 复用现有 typed editor、Message selector、草稿/Flow/失败清旧和预算机制。
4. 依据 `EXACT/UPPER_BOUND` 有界分配，成功只发布最终 bytes；失败不得发布 buffer。
5. 成功后只用实际 `bytes_written` 解析 physical layout；generated raw 保持“未观察”，不要增加第二次
   Decode 来填表。
6. 先加非 Qt adapter 行为测试，再接 Qt 可见按钮；覆盖所有六种 logical kind、constant/computed、
   bitfield、length/integrity/bounded record、错误定位、切换 Message、重载/关闭和预算。

### G2：Binary stream GUI

在 G1 owner/DTO 生命周期稳定后，再把公开 Framer/Host 的 Push/Continue/Reset、phase、stop reason、
候选与 Decode 结果接到 GUI。只展示公开 strategy/max candidate；详细 framing 参数另行决定是否值得补 API。

### G3：统一 Schema 0.1～0.11 的公共入口

合并当前 0.5～0.8 public legacy complete、0.9 Binary Host、0.10 complete ASCII、0.11 ASCII stream 的
选择/草稿/结果模型，但保留表示差异和历史兼容分支。先做行为等价，不以删除旧路径或压缩展示制造统一。

### G4：可选的 Encode 同调用观察

只有当实际 GUI 验收确认必须显示成功 Encode 的 generated/raw 细节时，再冻结应用无关观察契约。
该片应来自同一次 Core 执行、无第二次 Decode、无 logical 反算、失败零交付，并单独评估 public API、
DLL export、预算和 SDK 重验影响。当前首片不需要为此提前扩大接口。

## 9. 可复用测试与后续验收重点

| 目标 | 可复用测试源码 | 首片应新增的精确断言 |
| --- | --- | --- |
| typed Encode 与失败关闭 | `tests/public_api/public_codec_tests.cpp` | Qt/adapter draft 到六种 `EncodeValue` 的精确映射；失败清旧 frame |
| 动作/source/output size | `tests/public_api/public_consumer_metadata_tests.cpp` | Message 下拉只列实际可 Encode 关联；四态 source 不丢失 |
| Binary 物理高亮 | `tests/public_api/public_physical_query_tests.cpp` | 使用本次 bytes_written 解析，bit mask/动态 payload/integrity/length storage 不越界 |
| Host selector/回调/Reset | `tests/public_api/public_host_endpoint_tests.cpp` | 同一 GUI Flow 切 Message、callback 失败不发布、Reset 后重新 Find |
| Stream framing | `tests/public_api/public_stream_framer_tests.cpp` | chunk/Continue/STOP/Reset 与 UI 草稿、候选、结果隔离 |
| 现有 0.5～0.8 public complete | `tests/protocol_lab_ui/public_legacy_complete_tests.cpp` | 迁移/统一后保留 length/integrity/Decimal/review 与 Pipeline 限制 |
| 当前 0.9 public Decode UI | `tests/protocol_lab_ui/binary_public_h2_tests.cpp` | 新 Encode 不退化 Apply、双 binding/Flow、typed/raw Decode、失败清旧 |
| 当前 0.10/0.11 public ASCII | `tests/protocol_lab_ui/ascii_public_a2_tests.cpp`、`tests/protocol_lab_ascii/public_ascii_stream_adapter_tests.cpp` | Binary 首片不改变 ASCII selector、ordered segments、range、stream 生命周期 |

后续验证应先跑新增 adapter/Qt 专项，再按实际共享改动决定 Lab 完整矩阵和 static/shared installed-SDK
消费回归。当前 deliverables 是受保护的 98df5e0 本地交付，任何新实现必须使用新构建/部署目录；不能
覆盖旧目录后把旧人工结果当成新能力证据。

## 10. 本轮未验证和剩余决定

- 本轮没有动态执行任何测试；测试结果需在后续实施片重新取得，不能引用本报告冒充运行证据。
- 未验证 Linux、不同 MSVC toolset ABI、真实协议 Golden、硬件、现场、非 Loopback 网络或长期稳定性。
- 未审查或批准 Qt/第三方组件的正式外发许可；现有 installed-SDK/Lab 仍是本地候选，不是正式发布。
- 若 G1 只要求“选择 Message、编辑 caller input、生成并显示最终 Frame”，无需新 PAE API。
- 若 G1 同时要求“逐字段展示本次 Encode 的 actual raw/generated/integrity 计算值”，则存在明确接口
  决策点；应由总控先冻结观察契约，不能在 Lab 中读取 private Plan、重放 Decode 或自行反算。

## 11. 审计停点

本轮已完成公开能力、当前 Lab 接线和可复用测试源码的只读核对，只新增本报告。建议总控先确认 G1
采用“最终字节 + 失败定位 + 可证明物理布局，generated raw 未观察”的最小范围，再派 Lab 单独实施；
本报告不构成实现、构建、人工验收、提交或发布授权。
