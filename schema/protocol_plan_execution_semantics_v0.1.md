# PAE ProtocolPlan Execution Semantics V0.1 — Draft Slice

## 1. 文档状态

| 项目 | 内容 |
| --- | --- |
| 当前状态 | `V0.1 DRAFT SLICE / INCOMPLETE（V0.1 草案切片 / 不完整）` |
| 对应 Schema | [`pae.schema.json`](pae.schema.json) |
| 当前切片 | 严格配置编译、yyjson-free（不依赖yyjson）的Frozen Execution Plan（冻结执行计划）、显式ExecutionWorkspace、COMPLETE_RECORD内部Decode/Encode、固定长度/固定字节Matcher、UINT64/INT64/BYTES/ENUM，Schema 0.2中的BOOL与位容器、Schema 0.3中的SUM8、Schema 0.4中的字节对齐INT64，以及专用开关下Schema 0.5比例/偏置的编译冻结和Core双向转换；`input`与整数`constant` Encode Source |
| 不覆盖 | STREAM_CHUNK流式Framing、CRC及其他Integrity算法、Receive Gate、Mapping、Session、Runtime注册、公共API及完整V0.1字段类型 |

本文描述PAE（Protocol Adapter Engine，协议适配引擎）首个Loader/Compiler（加载器/编译器）垂直切片及其后的`COMPLETE_RECORD（完整记录）`Codec（编解码器）内部切片。它没有完成《PAE V0.1 技术细节拍板方案》中`PAE-DEC-027`要求的完整Schema V0.1语义覆盖，也没有冻结公共API（Application Programming Interface，应用程序接口），不能作为完整V0.1配置语言或生产协议正确性声明。

本文使用以下状态：

- `CONFIRMED PRINCIPLE（已确认原则）`：来自现有技术拍板，首切片必须遵守；
- `CONFIRMED TARGET / UNVERIFIED（已确认目标 / 未验证）`：职责与不变量已经拍板，但当前源码和测试尚未闭合；
- `DRAFT SLICE RULE（草案切片规则）`：为形成首个可执行切片采用的暂定作者格式和语义；
- `OPEN（待定）`：证据或作者格式尚不足，当前实现不得静默补全。

## 2. 证据与分发边界

仓库中的实验台双向样例是从零设计的人工协议：

- 所有`source_ref`使用`SYNTHETIC_FROM_SCRATCH:`前缀；
- 标识、长度、常量、字段值、大小端和完整字节布局均不来自客户协议、生产代码或现场报文；
- 样例不建立到任何真实设备、接口、项目或业务概念的映射；
- 样例通过Schema、生成预期Plan或参与Synthetic Engine Vector（合成引擎向量）契约检查，只能为对应的引擎行为提供证据，不能证明真实协议、硬件或现场行为。

当前样例和向量的三个重要证据边界必须保留：

1. `lab_command`和`lab_report`的13/11字节记录、固定标识`A7 31`/`5C E2`以及全部字段值均为独立人工选择；
2. 多字节整数故意使用`little_endian`，并加入三字节整数、固定BYTES和ENUM以覆盖当前引擎能力；
3. 两条向量在Codec实现前人工写定并完成独立工程复核，权威等级为`SYNTHETIC_REVIEWED / INDEPENDENT_ENGINEERING_REVIEWED（人工构造 / 独立工程复核）`。它们是引擎契约输入，不是协议权威、设备、硬件或现场证据。

样例中的`record_length`使用`encode.source=input`，只验证动态UINT64字段写入。当前切片不声明它具有自动长度回填语义，也不在Decode时自动把它与`frame_length_bytes`比较。

## 3. 冻结的执行链原则

`CONFIRMED PRINCIPLE`：

```text
Strict JSON bytes
→ StrictJsonLoader
→ StructuralValidator / SchemaIrBuilder
→ DomainValidator
→ ResourceBudgetValidator
→ PlanBuilder
→ complete PlanBundle or no PlanBundle
```

- 任一阶段失败时不得执行后续阶段；
- 第三方 Parser 类型不得进入 SchemaIr、诊断跨层接口或 Plan；
- Parser DOM 必须在 SchemaIr 拥有全部所需数据后释放；
- `PAE-DEC-032`要求的不可伪造Validated/Budgeted（已验证/已预算）能力状态已经在当前内部切片落地；原始`PlanDraft`已退出生产Builder入口，具体边界见11.2节；
- `pae_protocol_plan`是从`config_compiler`抽离出的内部、不可安装、不可导出目标；它不得依赖yyjson或其他JSON Parser；
- `pae_config_compiler`和`pae_protocol_core_slice`都依赖`pae_protocol_plan`，Codec不得反向依赖配置编译器；
- 同一输入、同一 Profile 和同一 Extension Registry 快照必须产生等价 Plan 语义。

首个运行切片使用以下两条入口链：

```text
PlanBundle + bound ExecutionWorkspace + pipeline_index + COMPLETE_RECORD bytes
→ deterministic Matcher（确定性匹配）
→ typed Decode fields or no delivered fields

PlanBundle + bound ExecutionWorkspace + pipeline_index + message_index + typed input values
→ full preflight（完整预检）
→ write caller buffer
→ final Matcher / field review
→ complete bytes or no delivered bytes
```

两个入口均为内部`noexcept`函数。逐帧调用不读取JSON、不创建或扩容容器，Decode输出槽位和Encode输出Buffer（缓冲区）由调用方提供；Plan的容器、字符串和执行描述符已经在配置编译/冻结阶段建立。`ExecutionWorkspace（执行工作区）`在调用前按Plan资源布局一次性分配，永久绑定并借用该Plan；Plan必须比Workspace存活更久，每个并发或重入调用必须独占一个Workspace。该切片不创建Session（会话）、线程或Runtime注册对象。

## 4. 根对象语义

### 4.1 身份

`CONFIRMED PRINCIPLE`：

- `schema_version`表示 PAE 配置语言版本；
- `protocol_version`表示被描述协议的版本；
- `protocol_id`是稳定 ASCII ID，文件名不参与身份；
- `display_name`面向人员显示；
- `source_ref`记录配置声明的来源定位。

`DRAFT SLICE RULE`：

- 缺省生产入口的`schema_version`接受字符串`"0.1"`至`"0.4"`；0.1保持原属性与类型集合，0.2允许
  `bit_containers`、`bitfield` Wire和BOOL，0.3增加Message的单对象`integrity`，0.4增加字节对齐INT64；
- 仅当专用构建显式开启`PAE_ENABLE_SCHEMA_V05_COMPILER`时，Schema 0.5才允许字节对齐、
  `encode.source=input`的UINT64/INT64字段声明`linear`到DECIMAL64的转换；Loader可完成编译冻结，
  Complete Record Core可执行双向转换；该门与Protocol Lab组合时仍配置失败；
- stable ID匹配`^[a-z][a-z0-9_]*$`，本草案切片最多128个字符；
- `resource_profile`只接受`desktop`和`constrained`；
- 根对象及所有子对象的未知属性必须拒绝。

Stable ID完整V0.1的最终最大长度、`source_ref`的正式结构以及`CUSTOM` Profile作者格式仍为`OPEN`；128只是当前Schema与实现一致的草案切片上限。

### 4.2 集合与顺序

根对象包含`framing_profiles`、`pipelines`和`messages`三个数组。

- 数组书写顺序不得成为 Matcher 优先级；
- FramingProfile、Pipeline、Message以及同一 Message内Field的ID必须唯一；
- Pipeline中的`message_ids`必须唯一且全部能够解析；
- ID唯一和跨引用属于 Domain Validation，不由 JSON Schema 的`uniqueItems`替代；
- 当前预期 Plan 快照按稳定 ID表达语义，不把内部索引数值冻结为公共契约。

## 5. COMPLETE_RECORD 与 Pipeline

### 5.1 FramingProfile

当前切片只接受：

```json
{
  "input_kind": "complete_record"
}
```

每次宿主提交的一个完整输入记录形成一个候选帧。内部Codec直接接受该记录和显式长度，不创建Session，也不监听Transport（传输层），不执行缓存、粘包、拆包或流式重组。

### 5.2 有方向 Pipeline

- `direction_id`使用协议绝对方向，不使用观察方相关的`rx/tx`；
- `input_framing_profile_id`必须引用存在的 FramingProfile；
- `message_ids`确定该 Pipeline允许的候选 Message集合；
- Pipeline与其 Message的`direction_id`必须相同；
- 同一 ProtocolPackage可以同时保存两个方向，但两个方向形成独立 PipelinePlan。

## 6. Message 与 Matcher

### 6.1 帧长度

`frame_length_bytes`表示本 Message首切片所采用的完整候选记录长度，最大值受 PAE Hard Limit（硬上限）约束。

- Matcher中的`frame_length_equals.length_bytes`必须等于所属 Message的`frame_length_bytes`；
- `frame_length_bytes`必须能够容纳全部 Field和固定字节 Matcher；
- 当前人工样例的13/11字节记录长度完全由测试设计确定，只构成Synthetic引擎契约；
- 该配置值不自动证明报文内“消息总长度”字段的单位或取值。

### 6.2 Matcher

当前切片只支持 AND 组合：

- `frame_length_equals`：候选记录长度必须相等；
- `fixed_bytes`：从`byte_offset`开始的原始字节必须与规范化大写十六进制字节串相等。

规范化字节串示例：`"A7 31"`。不接受`0xA731`、小写、连续无空格或双空格形式。

DomainValidator必须拒绝：

- Matcher范围超出`frame_length_bytes`；
- 同一 Pipeline候选 Message之间静态可发现的重叠；
- Matcher与相同 Wire区间的常量字段在字节层面冲突；
- Matcher与位容器的确定值冲突。确定值包括常量成员写入位，以及未被任何成员覆盖、由
  `base_value`确定的位；动态成员覆盖位不得仅按`base_value`判为冲突。判断须先按容器
  `byte_order`映射字节，再按`bit_numbering`和成员offset/width映射位。

Matcher数组顺序不构成优先级。运行期只接受唯一匹配：零匹配返回内部`UNKNOWN_MESSAGE`状态，多匹配返回内部`AMBIGUOUS_MESSAGE`状态，二者都不交付Decoded Field（已解码字段）。这些状态名尚未冻结为公共错误码。

## 7. Field 与 Wire 布局

### 7.1 公共属性

每个 Field必须包含：

```text
id
display_name
description
source_ref
value_type
wire
encode
```

`byte_offset`从完整记录首字节的0开始。字段存储区间为：

```text
[byte_offset, byte_offset + byte_width_or_length)
```

所有加法必须检查整数溢出。Field不得越过`frame_length_bytes`，两个 Field区间不得重叠。

### 7.2 UINT64

当前切片支持1至8字节、字节对齐的无符号整数：

- 多字节整数必须显式配置`big_endian`或`little_endian`；
- 单字节整数不要求 Byte Order；
- 读取和写入不依赖宿主CPU（Central Processing Unit，中央处理器）字节序；
- 当前人工样例的`little_endian`只属于`SYNTHETIC_FROM_SCRATCH`测试设计，不是正式协议或现场证据。

### 7.3 BYTES

当前切片只支持显式固定`byte_length`的BYTES。它在运行期保持二进制类型，不自动转换为IP（Internet Protocol，网际协议）字符串、Hex String或文本；Decode返回的Byte View（字节视图）借用输入记录，宿主必须保证输入生命周期覆盖消费过程。

### 7.4 ENUM

- Enum Wire存储使用本切片的无符号整数规则；
- 每项包含`id`、`display_name`和`raw_value`；
- Entry ID和Raw Value必须在所属 Field内唯一；
- `unknown_enum_policy`必须显式为`reject`或`preserve`；
- JSON Schema只能检查局部结构，重复ID或Raw Value由 DomainValidator拒绝；
- Decode遇到未知Raw Value时，`reject`失败且不交付任何字段；`preserve`保留Raw Value并把结果标记为`tainted（已污染）`；
- Encode只接受与当前Message、Field和Entry都一致的类型化Enum引用，不按字符串查找。

## 8. Encode Source 草案

当前切片只实现以下作者形态：

```json
{ "source": "input" }
```

以及适用于 UINT64/INT64 的：

```json
{ "source": "constant", "value": 1 }
```

- `input`表示宿主执行Encode时必须提交对应Logical Value（逻辑值）；
- `constant`表示宿主不得覆盖，值仍必须满足字段Wire宽度和领域约束；
- BYTES和ENUM在当前切片只接受`input`；
- `default`、通用typed constant以及`computed`作者格式仍为`OPEN`，不在本Schema中先接受后忽略；
- Message Matcher固定字节和常量字段覆盖同一区域时，DomainValidator必须验证二者编码结果一致。

人工样例的两个`record_length`字段使用`input`，只验证普通动态字段；在完整长度字段语义另行冻结前，不得把该字段改写为引擎自动`computed`或长度回填证据。

## 9. 首个内部 Decode / Encode 执行切片

### 9.1 Decode

`DecodeCompleteRecord`接收不可变`PlanBundle`、与该Plan绑定的`ExecutionWorkspace&`、`pipeline_index`、完整输入记录和调用方Field Slot（字段槽位）数组：

- Pipeline必须引用`COMPLETE_RECORD` FramingProfile（切帧配置），且候选Message、方向、Matcher和字段布局必须自洽；
- Matcher必须得到唯一Message后才开始字段解码；
- Slot容量必须覆盖该Message全部Field；Enum `reject`预检必须完成后才写入任何Slot；
- `UINT64`和`ENUM`按显式Big Endian或Little Endian逐字节读取，不依赖宿主CPU字节序或非对齐整数读取；
- `BYTES`返回借用输入记录的只读视图；
- 成功时一次性交付全部字段；任一失败结果的`field_count`保持为零。

### 9.2 Encode

`EncodeCompleteRecord`接收不可变`PlanBundle`、与该Plan绑定的`ExecutionWorkspace&`、`pipeline_index`、目标`message_index`、类型化输入值和调用方Buffer：

- 目标Message必须属于所选Pipeline，且Plan中的Field和固定Matcher必须定义完整Frame（帧）；没有规则覆盖的字节不得隐式补零；
- 每个`input`字段必须且只能提交一次，`constant`字段不得由宿主覆盖；
- 输入类型、整数Wire宽度、固定BYTES长度和Enum引用范围必须在写入前全部验证；
- BYTES输入与输出Frame区间重叠时失败，避免边写边破坏仍待读取的数据；
- 通过预检后才写固定Matcher、动态字段和UINT64常量，随后重新执行Matcher和字段复核；
- 只有最终复核成功才把`bytes_written`设为完整Frame长度，其他结果均保持零。

Fail-closed（失败关闭）约束的是“是否交付完整结果”。如果错误发生在写入后的最终复核阶段，调用方Buffer可能已经被修改，但不得读取、发送或缓存为有效报文。

### 9.3 当前接口边界

Field和Enum使用Plan-scoped Reference（计划作用域引用）：引用同时绑定原`PlanBundle`地址和Message/Field/Entry索引，不能跨Plan复用；宿主必须保证同一`PlanBundle`在引用使用期间存活且地址不变。当前入口不提供`"字符串key：值"`列表，不属于稳定公共API，也没有冻结Handle（二进制句柄）、Allocator（分配器）、C ABI（Application Binary Interface，应用二进制接口）或长期兼容的状态码数值。

## 10. Structural 与 Domain 分工

### 10.1 StructuralValidator

负责：

- 根对象、必填项和未知属性；
- JSON基础类型和局部枚举；
- Stable ID局部词法；
- 十六进制字节串局部词法；
- 整数Token精确性、范围和目标UINT64转换；
- 构建拥有自身数据的SchemaIr和ConfigOrigin。

JSON Schema按数学值可能接受`1.0`作为integer；PAE StructuralValidator仍必须根据原始Token返回`INTEGER_NOT_EXACT`。unsigned属性收到`-0`必须返回`INTEGER_OUT_OF_RANGE / NEGATIVE_TOKEN_FOR_UNSIGNED`。

### 10.2 DomainValidator

负责：

- ID唯一和引用解析；
- Pipeline/Message方向兼容；
- Field越界、重叠和算术溢出；
- Matcher越界、冲突和静态歧义；
- Enum Entry ID和Raw Value唯一；
- UINT64 constant是否能够由Wire宽度表示；
- Matcher固定字节与常量字段的一致性。

Domain诊断具体数值编码仍未冻结。测试语料中的`DRAFT_*`名称只表示当前预期类别。

## 11. PlanBuilder 与 Frozen Execution Plan 最小可观察结果

### 11.1 冷元数据结果

合法人工实验协议切片的预期语义快照至少包含：

- 1个ProtocolPackage；
- 1个COMPLETE_RECORD FramingProfile；
- 2个有方向Pipeline；
- 2个Message；
- 10个Field；
- 每条Message的长度、Matcher和Field半开区间；
- Field Value类型、Wire布局和Encode Source；
- 0个Integrity Rule。

PlanBuilder不得读取原始JSON、保留Parser DOM或修改Runtime。失败时不交付部分PlanBundle。生成后的Plan类型位于yyjson-free的`pae_protocol_plan`内部目标中，可由配置编译器和Codec共同依赖；这不把其当前C++结构升级为稳定公共布局。

### 11.2 当前冻结执行描述符

当前实现由`PlanBuilder::Freeze`消费不可伪造、move-only（仅移动）的内部`BudgetedPlanDraft`并一次性交付`PlanOwner`所有的`const PlanBundle`：

- `PlanBundle`构造器私有化，并删除复制和移动；
- 冻结前重新计算`ResourceRequirements（资源需求）`并与草案声明比较，再按Desktop/Constrained Profile检查当前帧、Message、Field、Matcher、Enum和Workspace上限；
- 每个Message生成固定字节、Field执行项、Enum稳定值表和按Raw Value排序的辅助查询表；
- 每个Pipeline生成允许Message位图，并按Frame Length建立有序Candidate Group（候选组）；
- `ExecutionResourceLayout（执行资源布局）`记录单Message最大字段/输入字段、Encode值索引数、存在位图字数和当前Workspace估算字节数；
- Codec只消费上述描述符，不再调用`ValidatePipeline`、`ValidateMessageLayout`、`MessageDefinesWholeFrame`或`FindEncodeValue`等旧逐帧静态检查/重复线性查找路径；
- Encode最终Matcher和字段复核、调用方输入、引用归属、Buffer容量、alias、Matcher唯一性及Workspace独占检查继续在每次调用中执行。

当前Config Compiler已经形成`SchemaIr → ValidatedSchemaIr → BudgetedSchemaIr → BudgetedPlanDraft → PlanBundle`单向能力链。作者配置错误与稳定JSON Pointer由Domain/Resource Validator（领域/资源校验器）作为单一语义权威报告；ResourceBudget在产生Budgeted能力前完成精确单Plan内存准入；Builder只接受Budgeted能力，并把尾部防御审计命中统一映射为内部契约违规。能力类型禁止默认构造和复制，移动后重复消费失败关闭；原始Draft不再是生产入口。Windows专项证据见`docs/windows-msvc-2026-validated-budgeted-capability-chain.md`和`docs/windows-msvc-2026-accounted-plan-memory-slice.md`。

当前`PAE-DEC-033A`已实现：冷/热Plan长期对象使用单一Storage Block的`FrozenString/FrozenArray`，ResourceBudget完成单Plan精确准入，Builder复算与最终Arena报告必须匹配批准报告。`PlanMemoryReport`分类覆盖对象、字符串、Matcher、元数据容器、执行描述符、索引、扩展和对齐；当前候选单Plan上限Hard/Desktop/Constrained为`256/128/8 MiB`，仍是`CANDIDATE / UNVERIFIED`。`PAE-DEC-033B` Runtime活跃Plan/Session聚合准入仍未实现。实施与Windows证据分别见`docs/pae-dec-033a-accounted-plan-memory-implementation-slice.md`和`docs/windows-msvc-2026-accounted-plan-memory-slice.md`。

### 11.3 Schema 0.2位容器执行规则

`PAE-DEC-040`只扩展`COMPLETE_RECORD`。Message可声明`bit_containers`，容器以
`container_offset/container_width`定位1/2/4/8字节区域，多字节必须显式声明`byte_order`，
并统一声明`lsb0`或`msb0`。成员仍在`fields`中，通过`container_id`引用；BOOL严格1 bit，
UINT64和无符号原始值ENUM可占1～64 bit，允许容器内跨字节和64位满宽，不允许跨容器。

Structural/Domain Validator拒绝空容器、未知引用、非法宽度或类型、成员重叠、容器重叠、普通
字段与容器重叠及越界。Builder预计算容器索引、shift和mask并纳入Frozen Plan精确计费；
Workspace按单Message最大容器数预留`uint64_t`槽。Encode从`base_value`初始化每个容器，清除成员
位后写入，未覆盖位保留基础值且不读取输出Buffer旧内容；Decode不把`base_value`当作接收约束。

Core内部BOOL使用独立`LogicalValueKind::BOOL`和真正的`bool`值。位字段没有扩展Matcher、普通字段
接收规则、Runtime/Session、公共API或C ABI。公开独立向量及Windows证据见
[`windows-msvc-2026-dec040-bitfield-slice.md`](../docs/windows-msvc-2026-dec040-bitfield-slice.md)。

### 11.4 Schema 0.3 SUM8完整记录校验规则

`PAE-DEC-041`只扩展`COMPLETE_RECORD`。Message可选一个`integrity`对象，首版算法固定为
`sum8`，对非空连续范围逐字节无符号累加并取低8位，结果写入范围外的独占单字节存储位置。
存储位置参与Frame完整覆盖，但不是Field，不能由Values输入，也不能与普通字段、位容器或
`fixed_bytes` Matcher重叠。范围、存储和所有权在Domain Validator中用受检减法验证，Builder
发布前复核算法枚举、边界、自包含和冲突不变量。

Decode先按长度和固定字节选择结构候选；零候选为`UNKNOWN_MESSAGE`，多候选为
`AMBIGUOUS_MESSAGE`。仅在唯一候选且输出槽容量满足后验证SUM8，失败返回
`INTEGRITY_FAILED`且`field_count=0`，校验通过后仍执行Enum等字段语义。跨Pipeline选择不得用
SUM8成败消除结构歧义。Encode先写既有字段、常量、固定字节和位容器，再生成并复算SUM8，
最终Matcher/字段复核成功才交付完整长度；复核失败沿用`FINAL_REVIEW_FAILED`且有效长度为0。

规则以冻结枚举和偏移保存在现有单Arena Plan对象/执行描述符中，资源需求单独记录实际规则数；
执行使用局部`uint8_t`累加器，不增加Workspace槽或逐帧分配。Decode校验计数为覆盖长度N，
Encode生成和复算为2N；这是操作上界证据，不是性能结论。公开向量和Windows验证见
[`windows-msvc-2026-dec041-sum8-slice.md`](../docs/windows-msvc-2026-dec041-sum8-slice.md)。

### 11.5 Schema 0.4字节对齐INT64规则

`PAE-DEC-042A`在COMPLETE_RECORD字段链增加1～8字节二进制补码INT64，多字节沿用显式大小端，
位字段INT64不在本切片。配置常量由原始JSON整数Token精确解析，signed `-0`归一为0；常量与
动态输入都必须位于实际Wire宽度的有符号范围。Builder冻结独立signed constant并在发布前复核。

Core使用`LogicalValueKind::INT64`和`std::int64_t`。Decode先以uint64_t逐字节累积，再通过掩码和
无符号幅值安全解释符号；Encode先检查范围，再按标准无符号转换提取低位。UINT64和INT64不可
隐式互换。Schema 0.4统一选择Lab 0.5代际，旧Schema、旧指纹与历史证据保持原行为。

### 11.6 Schema 0.5比例/偏置编译冻结与Core执行

`PAE-DEC-042B`首段在临时编译门下增加严格`linear`转换作者结构。比例与偏置的分子为精确
INT64 JSON整数Token，分母为1..10^18的精确UINT64 Token；参数先约分，约分后的分母只能含
2和5且最多需要18位十进制。比例不能为零，转换只适用于字节对齐、动态输入的UINT64/INT64字段。

编译器把比例和偏置转换为公共十进制尺度下的固定256位有符号A/B，冻结在独立转换表中，字段
只保存索引。Builder重新推导并核对描述符、索引、引用计数和原始类型，不信任损坏Draft。
转换数量和表的实际大小/对齐进入Plan `EXTENSION`计费，ResourceBudget、Builder复算与Arena报告
必须一致。专用Core构建增加类型化`Decimal64`，Decode在结构唯一、容量和SUM8之后按字段顺序精确
换算，整条记录成功后才交付字段及Workspace raw诊断；Encode在输出检查前按配置顺序精确反算并
缓存raw，写入后从实际字节重读、再次转换并比较数学值。scale非法映射INVALID_ARGUMENT并保留
DECIMAL_SCALE_OUT_OF_RANGE；反算非整数、Wire越界和
逻辑值越界分别保留原因；合法Plan内部算术异常为`INTERNAL_ERROR`，最终不一致为
`FINAL_REVIEW_FAILED`；最终重读转换的内部算术异常不降级为最终不一致。Workspace槽按每Message
最大转换数预分配并计入执行内存估算，热路径不分配。Core入口同时按编译能力检查Schema代际，
未知代际及缺省构建中的0.5执行均失败关闭。

默认Schema 0.5及其Lab开关仍关闭；显式开启专用C3链时使用Values 0.4、Result 0.6、指纹0.6和
Record 0.7。详见[编译首段报告](../docs/windows-msvc-2026-dec042b-compiler-slice.md)、
[Core第二段报告](../docs/windows-msvc-2026-dec042b-core-slice.md)和
[C3 CLI报告](../docs/windows-msvc-2026-dec042b-lab-c3-cli.md)。

### 11.7 Schema 0.6参数化CRC规则

Schema 0.6继承0.5并为COMPLETE_RECORD增加CRC-16/CRC-32。每条Message仍至多一个
`integrity`规则，可选无校验、SUM8或CRC。CRC显式冻结位宽、正常非反射多项式、初值、
`refin/refout`、最终异或、连续非空覆盖范围和独立存储字节序；2/4字节存储区不得进入覆盖区，
也不得与字段、位容器或`fixed_bytes` Matcher重叠。

Core使用有界直接左移寄存器实现：可选逐字节位反转、逐位最高位反馈、位宽截断、可选最终寄存器
反转及`xorout`，不追加零字节、不构建动态表、不逐帧分配。Decode在唯一结构候选和容量检查后
校验CRC，失败返回`INTEGRITY_FAILED`且不交付字段；Encode写完业务内容后生成CRC并从最终Frame
复算，最终不一致保持`FINAL_REVIEW_FAILED`且有效输出长度为0。覆盖字节操作计数为Decode N、
Encode 2N，这只是有界操作证据。

Schema 0.6使用独立Result 0.7、确定性指纹0.7及Run Record 0.8；Event语法保持0.7，CLI保持0.1。
旧Schema 0.1至0.5、旧指纹及历史Bundle不重写，跨0.5/0.6执行域Compare失败关闭，原始Frame比较
仍可跨来源。公开合成向量与Windows证据见
[`windows-msvc-2026-crc-minimal-slice.md`](../docs/windows-msvc-2026-crc-minimal-slice.md)。

### 11.8 Schema 0.7固定完整记录长度字段

Schema 0.7继承0.6，并允许每条COMPLETE_RECORD Message至多一个字节对齐UINT64计算长度字段。
字段使用`encode.source=computed`与`computed.kind=length`成对声明；`frame`取固定整帧字节数，
`region`取帧内非空连续区域的固定`byte_length`。存储宽度只允许1/2/4字节；1字节省略字节序，
2/4字节显式大端或小端。存储不得与其他字段、位容器、完整性存储或固定字节Matcher重叠。

Compiler推导期望值并纳入资源报告，Builder独立重算范围、值、字段关联和数量。Core Encode拒绝
业务覆盖，写完普通内容后写长度、再生成SUM8/CRC并最终重读；Decode在结构唯一和容量检查后先
验证长度、再验证完整性和业务字段。失败分别为`COMPUTED_FIELD_OVERRIDE`、`LENGTH_MISMATCH`，
不交付有效输出长度或字段。长度描述不新增Workspace槽，首次调用不新增可替换`new/new[]`分配。

Schema 0.7统一使用Result及指纹0.8、Run Record 0.9；Event 0.7、Values 0.1～0.4和CLI 0.1不变。
旧代格式、指纹及证据不重写，跨代Run Compare失败关闭。公开合成证据见
[`windows-msvc-2026-length-field-slice.md`](../docs/windows-msvc-2026-length-field-slice.md)。

### 11.9 Schema 0.8有界变长完整记录

Schema 0.8继承固定Message，并增加固定头部、一段输入BYTES载荷和可选紧邻SUM8/CRC尾部的
`bounded_payload`布局。实际帧长由完整输入或BYTES长度安全推导；一个头部计算长度字段记录
frame或payload字节数。结构候选只使用实际长度闭区间及fixed_bytes，长度值和完整性不消歧。

Core Decode在唯一结构及容量检查后依次验证计算长度、动态完整性和字段；Encode预检载荷后按
实际尺寸写业务字段、长度和尾部，并只对实际帧范围最终复核。载荷Decode视图借用输入Frame；
正常首调及重复调用不新增堆分配。Builder重新推导载荷索引、尺寸区间、尾部锚点和资源计费。

Schema 0.8使用Result及指纹0.9、Run Record 0.10；Event 0.7和CLI 0.1不变。Values 0.1～0.4
保持原接受域；Values 0.5仅为Schema 0.8扩展显式空BYTES，缺失、null及旧代空BYTES仍拒绝。
Result 0.9允许空BYTES的raw/logical同为空串且enum_known为false，旧Result接受域不改。
旧格式及指纹不重写，跨代Run Compare失败关闭。公开合成证据见
[`windows-msvc-2026-bounded-variable-record-slice.md`](../docs/windows-msvc-2026-bounded-variable-record-slice.md)。

## 12. Schema 0.9有界流式切帧内部切片

Schema 0.9在既有完整记录Codec之前增加内部`protocol_framing`层，仅接受
`input_kind=stream_chunk`的`fixed_length`、`sync_fixed_length`和`sync_length_field`三种严格联合。
长度字段为1/2/4字节无符号总帧长度；1字节省略`byte_order`，2/4字节必须显式指定大小端。
同步头锚定帧偏移0，Compiler与Builder共同复核Pipeline、Message、Matcher、长度字段和同步搜索表。

每逻辑流使用独立`StreamFramingWorkspace`。Workspace按所绑定Pipeline的最大帧一次性分配缓存，
创建结果精确报告对象本体加缓存容量；Plan资源报告另以`max_framing_buffer_bytes`明确表示最大可变
缓存，不冒充完整对象大小。首次及后续push不分配，submit、回调数、同步头、单流内存和工作单位
均在冻结Profile及Hard Limit内失败关闭。`bytes_consumed`只表示本次输入中已处理或已复制的前缀；
sink返回STOP后未消费后缀仍归宿主，工作/回调预算留下的内部状态可由empty submit继续。

Framer只交付完整候选帧，随后宿主同步调用既有`DecodeCompleteRecord`。下游UNKNOWN、AMBIGUOUS、
长度、完整性或字段错误不触发Framer回扫。Protocol Lab不执行Schema 0.9，也不新增Result/Record/
Event格式；它在配置成功编译后、输入/Codec/网络/证据动作前以既有配置编译失败类别统一拒绝。
公开实现及Windows离线证据见`docs/bounded-stream-framing-contract.md`和
`docs/windows-msvc-2026-bounded-stream-framing-slice.md`。

## 13. 当前不覆盖的完整 V0.1 能力

完成本切片不能宣称完成完整Schema V0.1。至少仍缺少：

- REAL64、STRING/ASCII和Packed BCD；有符号位字段仍未实现；
- 比例/偏置的Values/Lab证据、raw/value constraints及稳定公共接口；
- `default`及长度以外的通用`computed`作者格式；
- SUM、XOR、LRC、CRC之外的自定义Checksum及多段/动态完整性规则；
- Receive Gate、Mapping、Session、资源自定义和Runtime注册；
- 稳定公共API、C ABI和字符串键值适配层；
- 三个PoC（Proof of Concept，概念验证）的正式协议独立Golden Vector；当前只有人工实验协议的两条Synthetic引擎向量。

## 14. 验证边界

截至2026-09-02，Loader/Compiler与Frozen Execution Plan内部切片已有以下执行证据：

- PowerShell 7 `Test-Json`对8项结构参考用例的结果符合预期；尚未执行官方Draft 2020-12 Meta-Schema（元Schema）自验证；
- Windows x64、MSVC（Microsoft Visual C++，微软C++编译器）Release/Debug下的Config Compiler合同Runner各28项内部用例通过；
- 合法人工实验样例生成的Canonical Plan（规范化计划）与稳定ID Golden Snapshot逐字节一致；
- 代表性Structural/Domain负例均验证失败时不交付部分`PlanBundle`。

仓库当前另保存了内部Codec源码和两条预先写定的Synthetic Engine Vector。最新Windows MSVC Release/Debug证据为：Config Compiler各`28/28`，Codec CTest各`6/6`，主合同Runner各`60/60`，首次Decode/Encode分配各`1/1`，操作计数`4/4`，共享Plan并发`2/2`；Parser、Loader、Plan和Codec共存CTest各`26/26`。详细命令和边界记录在`docs/windows-msvc-2026-accounted-plan-memory-slice.md`、`docs/windows-msvc-2026-validated-budgeted-capability-chain.md`与`docs/windows-msvc-2026-frozen-execution-plan-slice.md`；此前`52/52`报告只作为历史基线保留。

上述Loader/Compiler执行证据及Synthetic资产仍不证明：

- 任一JSON Parser已成为生产依赖；
- 完整V0.1 Decode/Encode、正式性能目标或生产运行时已经验证；
- Linux、目标板、硬件或现场行为；
- 人工样例与任何生产协议、真实报文或设备行为之间存在等价关系。

## 15. 修订记录

| 文档版本 | 日期 | 说明 |
| --- | --- | --- |
| 0.1.16 | 2026-09-10 | 同步Schema 0.9有界流式切帧、三策略、精确消费/背压、Plan与Workspace资源边界、Lab早拒绝及Windows离线验证边界 |
| 0.1.15 | 2026-09-09 | 收口Schema 0.8空BYTES：增加仅新代可用的Values 0.5，Result 0.9 Reader/Writer/指纹统一接受空配对，旧代不变 |
| 0.1.14 | 2026-09-09 | 同步Schema 0.8有界变长完整记录、实际尺寸执行、动态完整性、Lab 0.9/Record 0.10及Windows离线边界 |
| 0.1.13 | 2026-09-09 | 同步Schema 0.7固定完整记录长度字段、Builder重算、Core执行顺序、Lab 0.8/Record 0.9及Windows离线边界 |
| 0.1.12 | 2026-09-09 | 同步Schema 0.6参数化CRC-16/32、冻结参数、Core双向执行、Result 0.7/Record 0.8隔离及Windows离线验证边界 |
| 0.1.11 | 2026-09-07 | 同步DEC-042B Core审查纠错：非法Decimal scale归类为INVALID_ARGUMENT，最终重读算术内部故障保持INTERNAL_ERROR，并补充运行时代际门禁与Workspace计费边界 |
| 0.1.10 | 2026-09-07 | 同步PAE-DEC-042B Core精确双向转换、Decimal64、Workspace raw诊断、失败顺序、最终重读复核及运行槽计费；Values/Lab证据段仍未实现 |
| 0.1.9 | 2026-09-07 | 同步PAE-DEC-042B临时门隔离的Schema 0.5编译冻结、独立转换表、Builder复核和Plan计费首段；Core/Lab运行语义仍未实现 |
| 0.1.8 | 2026-09-06 | 同步PAE-DEC-042A Schema 0.4字节对齐INT64、独立Core类型、安全补码算法及Lab 0.5版本边界 |
| 0.1.6 | 2026-09-06 | 同步PAE-DEC-040 Schema 0.2位容器、BOOL、冻结布局、Workspace计费及Windows验证边界；Schema 0.1保持原能力 |
| 0.1.5 | 2026-09-02 | 同步PAE-DEC-033A单Storage Block、Frozen Storage、PlanOwner、精确ResourceBudget准入、报告复核和Windows部分验证；033B仍未实现 |
| 0.1.4 | 2026-09-02 | 同步PAE-DEC-032已实现能力链、第十七轮PAE-DEC-033A/033B计量合同和033A推荐实施方案；保持033A/033B源码与测试为UNVERIFIED |
| 0.1.3 | 2026-09-02 | 用完全独立的SYNTHETIC_FROM_SCRATCH实验台双向样例替换实现证据衍生布局；保留相同引擎能力与验证边界，不新增生产协议结论 |
| 0.1.2 | 2026-09-02 | 同步PAE-DEC-032/033，确认不可伪造的Validated/Budgeted能力状态、Validator单一规则权威及完整Plan/Runtime内存准入目标；当前实现继续标记UNVERIFIED |
| 0.1.1 | 2026-09-02 | 记录Frozen Execution Plan、显式ExecutionWorkspace、Windows操作计数/并发/首次调用门禁，以及Validated Draft类型状态和完整Plan/Runtime内存准入仍未闭合的边界 |
| 0.1.0 | 2026-09-01 | 建立首个Loader/Compiler与COMPLETE_RECORD Codec草案执行语义 |
