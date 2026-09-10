# 有界流式切帧：已确认最小实施契约

日期：2026-09-10。当前实施准备基线：`dc5c4433b51961a9e2d24f425fba672a7121290e`，
已合入并Push主线，包含有界变长完整记录`3ae4df7`、UI契约`51db6d2`和离线Lab UI实现。
`51db6d2`保留为本契约最初编写时的历史代码基线，不再作为后续源码任务起点。

状态：`CONFIRMED / IMPLEMENTED / WINDOWS_OFFLINE_VERIFIED（已确认 / 已实现 /
Windows离线已验证）`。本文件最初只记录确认契约；2026-09-10实施检查点已增加Schema 0.9、
Compiler/Plan、内部Framer、公开合成样例、宿主示例、Lab早拒绝门禁及自动化测试。验证边界见
[`windows-msvc-2026-bounded-stream-framing-slice.md`](windows-msvc-2026-bounded-stream-framing-slice.md)。

2026-09-10实施授权：用户已授权整个Framer检查点、第10节Lab版本拒绝门禁及Windows
Debug/Release离线验证。允许范围内源码、测试和验证文档变更；不执行网络、不Stage、Commit、
Push或清理生成物。授权不代表已经实现或验证通过；后文“待源码授权”为此前准备阶段记录，
当前以本段授权及总控任务包为准。

2026-09-10补充确认：同步头长度采用Desktop 16字节、Constrained 8字节、Hard Limit 64字节；
宿主或配置可在Hard Limit内显式调整，值0沿用对应Profile默认值。该确认关闭原同步头上限候选项，
不改变`sync_bytes`必须非空的语法规则。

本文只定义`STREAM_CHUNK（流式字节片段）`到完整候选帧的有界切分。它不引入TCP、串口、
设备、线程、轮询、持续监听、重试、Runtime注册、稳定公共API或C ABI；不支持分隔符、转义、
TLV、数组、条件字段、跨帧业务状态或通用自动恢复。私有协议和现场报文不属于本文证据。

## 1. 已确认决策与边界

| 已确认项 | 本文落实 |
| --- | --- |
| 版本与分层 | Schema 0.9；独立内部`protocol_framing`；每逻辑流一个`StreamFramingWorkspace` |
| 三种策略 | `fixed_length`、`sync_fixed_length`、`sync_length_field` |
| 长度含义 | 无符号1/2/4字节；值直接等于总帧字节数`F`；同步头锚定帧偏移0 |
| 恢复规则 | 非法`F`从候选起点下一字节重同步；合法`F`等待恰好`F`；不扫描其payload内部 |
| 调用与所有权 | 宿主同步push；帧回调期借用；准确`bytes_consumed`；STOP后的当前帧已经交付 |
| 资源与首片 | 沿用既有帧/submit/回调/单流上限；同步头16/8/64字节；增加已确认工作预算；Lab只增加0.9执行早拒绝门禁，UI不接入 |

当前`COMPLETE_RECORD（完整记录）`入口和Codec语义保持不变：宿主提交的一段输入已经是一条完整
记录，实际`input.size`就是该记录的`F`。`STREAM_CHUNK`中“当前已经收到或缓存的字节数”不是
可信`F`；Framer必须先按冻结的FramingProfile确定一条完整候选帧，之后上层才可调用现有
`MatchCompleteRecordStructure`或`DecodeCompleteRecord`。Schema 0.8的有界变长Message只有
`minimum/maximum`区间，该区间本身不能从无边界字节流推导帧界。

FramingProfile由Pipeline唯一引用，并在Message识别之前执行。Message不选择Framer，CRC、长度、
Matcher或字段语义也不得用来反向选择FramingProfile。完整候选帧形成后，既有Core仍负责结构唯一性、
长度、完整性和字段语义；Framer不复制第二套Message Matcher。

## 2. 分层与对象模型

### 2.1 推荐内部形态

新增内部目标`protocol_framing`，依赖冻结的`protocol_plan`，不依赖Transport、Lab、Qt或业务层：

```text
host byte source
  -> StreamFramingWorkspace::Push(chunk, sink)
  -> zero or more complete candidate frames
  -> existing MatchCompleteRecordStructure / DecodeCompleteRecord
```

- `ProtocolPlan`及`PipelinePlan`继续不可变并可被多个流只读共享。
- 一个`StreamFramingWorkspace`只绑定一个Plan实例和一个有方向的Pipeline，只保存该逻辑流的
  半包、同步匹配进度、待交付状态、丢弃计数及统计。
- 不同连接或逻辑流不得共享Workspace；一个流的坏数据不得污染另一个流。
- Workspace创建时一次性预留经预算批准的存储；push热路径不分配、扩容或首次建立缓存。
- 上层在帧回调内执行Decode时，为该流使用独占的现有`ExecutionWorkspace`。
- 本对象是首片内部有状态工作区，不等同于完整Runtime/Session注册实现，也不冻结稳定API。

不把流式入口合并进`DecodeCompleteRecord`。后者继续是无缓存、单完整记录、显式Workspace的Codec
入口；否则会把不可信边界、缓存、重同步、背压和生命周期混入当前已验证语义。

### 2.2 生命周期和并发

- Plan必须比FramingWorkspace、ExecutionWorkspace及回调中使用的Plan引用存活更久。
- 同一FramingWorkspace的`push/reset`不允许并发，返回`WORKSPACE_BUSY`语义。
- 同一FramingWorkspace发起的同步回调栈内，不允许再次`push/reset`，返回
  `REENTRANT_CALL`语义。
- destroy没有“在执行时返回错误”的公共承诺；首片由宿主保证destroy前调用已经结束且无并发访问。
- 不同FramingWorkspace可在不同线程独立执行；共享Plan只读。本文不据此宣称完整PAE无条件
  Thread-safe（线程安全）。
- 回调期间不持有会导致业务回调自锁的内部互斥锁；实现可以使用原子占用标志或等价有界机制拒绝
  并发/重入，但不得用Runtime全局锁串行化所有流。

## 3. Schema 0.9严格配置

Schema 0.9继承0.8已实现能力，只新增流式FramingProfile。旧Schema 0.1～0.8继续只接受
`input_kind: "complete_record"`，不得静默接受或忽略新属性。

所有对象沿用Strict JSON规则：拒绝未知、重复、缺失的必需属性，拒绝`null`、错误JSON类型、
小数或指数冒充整数。下列片段省略现有必需元数据`id/display_name/description/source_ref`。

### 3.1 `fixed_length`

```json
{
  "input_kind": "stream_chunk",
  "strategy": "fixed_length",
  "frame_length_bytes": 6
}
```

- 只允许`input_kind`、`strategy`和`frame_length_bytes`之外的既有公共元数据。
- `frame_length_bytes > 0`，且不超过绑定resource profile的`max_frame_bytes`。
- 禁止`sync_bytes`、`length_field`、`minimum_frame_length`和`maximum_frame_length`。
- Pipeline内每个Message都必须是相同精确帧长；有界变长Message不得仅凭上下限接入该策略。

### 3.2 `sync_fixed_length`

```json
{
  "input_kind": "stream_chunk",
  "strategy": "sync_fixed_length",
  "sync_bytes": "A5 5A",
  "frame_length_bytes": 6
}
```

- `sync_bytes`为非空规范十六进制字节串，语义固定为帧偏移0，不另配offset。
- `sync_bytes`长度不得超过`frame_length_bytes`及同步头资源上限。
- Pipeline内每个Message都必须具有相同精确帧长，且其既有`fixed_bytes` Matcher必须在偏移0
  完整、逐字节包含相同同步头；不能只靠其他字段或校验间接判定。
- 禁止`length_field`、`minimum_frame_length`和`maximum_frame_length`。

### 3.3 `sync_length_field`

```json
{
  "input_kind": "stream_chunk",
  "strategy": "sync_length_field",
  "sync_bytes": "A5 5A",
  "length_field": {
    "byte_offset": 2,
    "byte_width": 2,
    "byte_order": "little_endian"
  },
  "minimum_frame_length": 4,
  "maximum_frame_length": 64
}
```

- `sync_bytes`沿用第3.2节规则。
- `length_field`表示字节对齐无符号整数，值直接等于总帧字节数`F`；首片不支持payload长度、
  word单位、固定偏置、比例、表达式或其他换算。
- `byte_width`只允许1、2、4。1字节必须省略`byte_order`；2/4字节必须显式为
  `big_endian`或`little_endian`。
- `byte_offset + byte_width`必须受检无溢出，且不超过`minimum_frame_length`。
- 同步头区间`[0,sync_size)`与长度存储区间不得重叠。
- `0 < minimum_frame_length <= maximum_frame_length <= resource.max_frame_bytes`。
- 最大长度字段值必须能够表示`maximum_frame_length`；配置不允许依赖运行时截断。
- Pipeline内每个Message的最小/最大实际帧长必须落在Profile区间内；每个Message都必须有一个
  `encode.source=computed`、`computed.kind=length`、`scope=frame`的UINT64长度字段，其
  offset/width/order与Profile物理描述完全一致。
- 每个Message的`fixed_bytes` Matcher必须在偏移0完整包含相同同步头。字段id可因Message而异，
  因为Framer在Message身份确定前只依赖Profile物理位置，不引用Message字段id。

### 3.4 编译失败和Builder复核

Structural阶段报告对象形状、属性、JSON类型、局部枚举和局部整数范围，使用准确JSON Pointer。
Domain阶段报告Pipeline/Profile/Message交叉引用、长度/同步判别不一致及所有权冲突。Resource阶段
报告已推导存储和上限超额。Builder只消费Budgeted Draft，并在发布前独立复核：

- strategy与其联合成员严格配对；
- 任何`STREAM_CHUNK`描述只允许Schema 0.9；旧代Budgeted Draft即使被内部损坏为同形态stream，
  Builder也必须以`INVALID_FRAMING_PLAN`失败关闭；
- `COMPLETE_RECORD`、`fixed_length`及`sync_fixed_length`的非适用长度字段offset必须为0、byte order
  必须为`NOT_APPLICABLE`，不能把Loader不会生成的冗余状态冻结进Plan；
- 固定长度或min/max、同步头、长度字段范围及表示能力；
- Pipeline中每个Message的长度形态、同步Matcher及computed长度描述；
- 冻结索引、字符串/同步字节、预计算搜索表和资源报告自洽；
- 所有`size_t/uint64_t`转换、相加、相乘和对齐无溢出。

任一失败都不得发布部分Plan。损坏Draft进入现有内部`INVALID_PLAN/INTERNAL_ERROR`职责，不伪装成
普通作者错误。Plan内不保留作者JSON字符串供热路径解释。

### 3.5 已确认的同步头资源规则

同步头长度上限已于2026-09-10确认：Hard Limit 64字节、Desktop 16字节、Constrained 8字节。
`sync_bytes`本身仍必须非空，不能用0长度表达“关闭同步”。有效资源限制遵守：

- 未显式调整或调整值为0时，使用所选Profile默认值；0不表示无限，也不是同步头长度；
- 宿主或配置可以在Hard Limit内显式调低或调高有效上限；超过64字节在资源准入阶段失败；
- 实际`sync_bytes`长度必须小于等于有效上限，同时满足第3.2/3.3节的帧内范围约束；
- 内部资源字段推荐命名为`max_sync_bytes`，具体C++成员和配置承载位置按既有资源限制机制落实，
  不作为稳定公共API冻结，也不需要再次进行产品语义微审批；
- 同步字节和搜索预计算表按实际长度计入Plan，不能只按有效上限粗略计费。

## 4. Framer状态机

概念状态如下；内部名称可以按现有风格调整，但行为不可改变：

| 状态 | 含义 | 允许的下一步 |
| --- | --- | --- |
| `COLLECT_FIXED` | 收集纯固定长度帧 | 收满后`DELIVER_PENDING`；否则`NEED_MORE` |
| `SEARCH_SYNC` | 线性寻找offset-0同步头 | 命中后进入固定收集或`READ_LENGTH`；否则保留同步头最长可能后缀 |
| `READ_LENGTH` | 同步头已命中，等待长度字段完整 | 合法`F`进入`COLLECT_DECLARED`；非法`F`执行重同步 |
| `COLLECT_DECLARED` | 等待合法声明长度`F`的剩余字节 | 恰好收满后`DELIVER_PENDING`；否则`NEED_MORE` |
| `DELIVER_PENDING` | 一条完整候选帧已在Workspace中但尚未回调 | 预算允许时回调；STOP或继续下一候选 |

Workspace在任意返回点必须处于上述可恢复状态之一。不得保留指向已返回调用输入的裸指针；只有
当前同步回调栈内可以使用当前输入的直接视图。

### 4.1 `fixed_length`

- 每连续`N`字节形成一帧；半帧跨push保存，单次push可以形成多帧。
- 没有同步头时，插入、丢失或从错误位置开始都会使后续固定分块错位，PAE无法可靠识别。
- Framer不因下游`UNKNOWN_MESSAGE`、长度、CRC或字段错误而移动一字节重试；已交付的`N`字节
  仍是一条已消费候选帧。恢复错位需要宿主显式reset或使用带同步头的Profile。

### 4.2 同步头扫描

- 同步头锚定候选帧偏移0。使用KMP或等价持久线性自动机，跨chunk保存匹配进度。
- 未命中的字节作为垃圾丢弃；输入耗尽时只保留“可能成为下一同步头前缀”的最长后缀。
- 已经判定为垃圾的字节不得在同一状态下无界回退重扫；每次比较和失败回退都计工作单位。
- 连续垃圾通过汇总计数/诊断表达，不为每个字节分配对象或触发无界回调。
- 一旦接受一个合法边界和`F`，payload内即使出现相同同步序列也不提前重同步。

### 4.3 长度解析、非法候选和恢复

同步头命中后，只等待到`byte_offset + byte_width`足以读取长度；按冻结字节序无符号解析。
解析值不是`size_t`前先与冻结范围比较，避免窄平台转换和相加溢出。

若`F < minimum_frame_length`、`F > maximum_frame_length`或内部转换失败：

1. 记录一个`MALFORMED_LENGTH`语义和该候选值（若可表示）；
2. 不等待声明的`F`，不扩容；
3. 只确定丢弃候选同步头起点的第一个字节；
4. 将此前已消费但仍可能构成下一同步头的其余候选前缀重新送入持久搜索状态；
5. 继续扫描，全部比较、回退和搬移均重新计工作单位。

因此“从下一字节继续”不表示一次性丢弃整个header，也不表示未计费重扫。一个损坏长度后的payload
可能偶然包含同步头；此规则只承诺确定性、有界前进，不承诺找回物理世界中的真实帧界或提供认证。

合法`F`则等待恰好`F`字节。即使后续长时间不完整，也不扫描其内部同步头；Workspace缓存仍受
`maximum_frame_length`约束。超时、断链和是否reset由宿主Transport策略决定，Core不创建定时器。

### 4.4 下游Codec错误不是重同步信号

完整候选帧一旦进入帧回调，即从Framer状态移除。下游结构歧义、未知报文、长度不匹配、SUM/CRC
失败或字段语义失败都不使Framer回扫该帧，也不改变下一候选起点。这样避免用CRC是否通过消歧或
把payload中的同步字节解释两次。

既有CRC文档中“失败后恢复成功”“顺序恢复”只表示下一次独立完整记录调用能够成功且失败状态
不残留，不表示当前COMPLETE_RECORD Core已经实现流式重同步。本切片的重同步只由本节带同步头
Framer规则定义，CRC既不替代同步头，也不决定候选边界。

## 5. 内部push契约

### 5.1 概念接口

以下名称用于冻结语义，不是稳定公共C++ API：

```cpp
SubmitResult PushStreamChunk(
    const PlanBundle& plan,
    StreamFramingWorkspace& workspace,
    std::size_t pipeline_index,
    ByteView input,
    FrameSink sink) noexcept;
```

`FrameSink`同步接收一条`ByteView`并返回`CONTINUE`或`STOP`。帧ByteView可能借用当前input，也可能
借用Workspace缓存；无论来源，只在该次sink回调期间有效。宿主异步保留时必须复制。回调异常不得
穿过内部边界；C++首片应捕获并映射为内部错误或要求noexcept sink，具体机制是普通实现选择。

### 5.2 返回结构和语义

`SubmitResult`至少表达下列信息；字段名可按现有类型风格调整：

| 字段 | 语义 |
| --- | --- |
| `api_status` | 调用/Plan/Workspace层状态；正常切帧为`OK` |
| `stop_reason` | `INPUT_EXHAUSTED`、`NEED_MORE`、`WORK_BUDGET_REACHED`或`SINK_STOP` |
| `bytes_consumed` | 本次input已接受前缀，范围`[0,input.size]` |
| `frames_delivered` | 本次已经进入sink的完整帧数 |
| `bytes_discarded` | 本次处理期间确定为垃圾的字节数，包括此前缓存中本次才判废的字节 |
| `malformed_candidates` | 本次确认的非法长度候选数量 |
| `last_framing_issue` | 最近一个聚合Framing问题；没有则`NONE` |
| `work_units_used` | 本次实际工作计数，不超过有效预算 |

建议的内部`api_status`工作名称为`OK`、`INVALID_ARGUMENT`、`INVALID_PLAN`、
`WORKSPACE_PLAN_MISMATCH`、`WORKSPACE_BUSY`、`REENTRANT_CALL`、`LIMIT_EXCEEDED`和
`INTERNAL_ERROR`。具体枚举名称和数值不是本内部切片需要人工确认的产品决策，由实现按既有内部
风格落地；上述职责和失败原子性不可改变。未来若发布稳定API或C ABI，再单独冻结数值。

`MALFORMED_LENGTH`是可恢复Framing问题，不必把整次submit的`api_status`改为失败：一次调用可以
先丢弃非法候选，随后交付合法帧。结果通过计数和`last_framing_issue`保留事实。

### 5.3 `bytes_consumed`

- 只计本次input中已处理、已交付、已丢弃或已复制进半包缓存的连续前缀。
- 以前调用已经缓存的字节不重复计入本次`bytes_consumed`，但本次再次比较/搬移仍计工作单位。
- 返回后PAE不再引用本次已消费前缀；`input[bytes_consumed, input.size)`不得被复制、保留或排队。
- 宿主只重提未消费后缀；已复制进Workspace但尚未形成帧的字节不得重提。
- 实现不得预先复制整块input，再以预算或STOP为由返回较小`bytes_consumed`。
- `input.size > max_submit_bytes`在任何状态推进和回调前返回`LIMIT_EXCEEDED`，
  `bytes_consumed=0`、`frames_delivered=0`，既有Workspace状态不变。

### 5.4 STOP和不可重复交付

sink收到帧时，该帧已经进入“交付提交点”。sink返回`STOP`后：

- 当前帧计入`frames_delivered`并从Workspace移除，不得在下次push重复回调；
- `stop_reason=SINK_STOP`；
- 本次input中直到该帧末尾的字节属于已消费；之后的字节不预取、不复制；
- 如果STOP与预算恰在同一提交点发生，显式宿主背压优先，返回`SINK_STOP`；预算事实仍可由
  `work_units_used`观察，但不覆盖STOP。

### 5.5 空input续处理

`push(empty)`允许，但不轮询Transport、不等待数据：

- 若Workspace因上一调用工作预算而留有`DELIVER_PENDING`或待继续的内部扫描/搬移，使用新一轮
  工作预算继续处理，`bytes_consumed`恒为0，并可交付已完整缓存的帧。
- 若只有真正的半帧，返回`NEED_MORE`，不产生回调。
- 若无缓存、无待处理状态，返回`INPUT_EXHAUSTED`，计数均为0。
- 若上次是`SINK_STOP`，已交付帧不会因empty调用重现；只有确实已消费并缓存的后续状态才可继续。

### 5.6 返回优先级和同时发生

入口优先级：空指针/结构参数 → Plan及Pipeline有效性 → Workspace归属 → 并发/重入 →
`max_submit_bytes` → 正常状态推进。入口失败不消费输入、不改变Workspace、不调用sink。

正常推进结束时按以下规则选择唯一`stop_reason`：

1. sink显式返回STOP：`SINK_STOP`；
2. 仍有输入或Workspace内部可执行工作，但帧数或工作预算已尽：`WORK_BUDGET_REACHED`；
3. 输入已耗尽且只差未来新字节：`NEED_MORE`；
4. 输入已耗尽、无待处理或半帧：`INPUT_EXHAUSTED`。

如果“最后一个输入字节耗尽”和“预算恰好耗尽”同时发生，只有仍存在未执行状态迁移、内部扫描、
待交付帧或其他当前无需新字节即可推进的工作时才返回`WORK_BUDGET_REACHED`；否则按3或4返回。
这防止每次预算刚好归零却没有剩余工作时要求无意义empty调用。

## 6. 工作预算、线性进展和资源

### 6.1 已确认有效限额

| 项目 | Desktop | Constrained | Hard Limit |
| --- | ---: | ---: | ---: |
| 最大完整帧 | 64 KiB | 4 KiB | 1 MiB |
| 单次submit输入 | 1 MiB | 64 KiB | 4 MiB |
| 单次完整帧回调 | 1,024 | 64 | 4,096 |
| 单流PAE内存 | 512 KiB | 128 KiB | 2 MiB |
| 同步头字节 | 16 | 8 | 64 |
| 单次Framer工作单位 | 4,194,304 | 262,144 | 16,777,216 |

配置值0沿用Profile默认，不表示无限。首片没有完整Runtime活跃内存聚合准入；验证
FramingWorkspace自身计费不能被表述为整个Session/Runtime准入已经实现。宿主或配置可以在
Hard Limit内显式调整同步头、帧、submit、回调和工作预算的有效限制；所有调整必须在分配或状态
推进前完成准入，且不能放宽`sync_bytes`非空、单流总内存或其他结构规则。

### 6.2 工作单位

以下每项各计1个工作单位：

- 扫描或推进一个输入/缓存字节，即使实现采用直接视图而未复制；
- 将一个字节与同步头字节比较；
- 同步匹配自动机执行一次失败回退步骤；
- 复制、搬移或压缩一个字节；
- 读取长度字段的一个字节。

同一字节因非法候选重扫、失败回退或搬移而重复工作时重复计数。实现可以消除复制或搬移，但不能
因此免除实际发生的扫描/比较。固定时间的整数比较、状态赋值和一次回调调度不单独计字节工作单位；
sink及其内部Codec/业务耗时不计入Framer工作单位，由帧回调上限及各自模块预算约束。

达到预算只在状态可恢复的安全点返回。不得在一个未记录的多字节复制、未完成长度读取或未保存
同步自动机状态的中间位置返回。已消费前缀和Workspace状态必须足以让下一次push（包括empty）
继续且不重复交付。

非法候选恢复需要压缩保留区时，搬移进度也是必须持久化的状态。一次调用只搬移剩余工作预算允许
的字节；未完成时保留搬移游标并返回`WORK_BUDGET_REACHED`，全部搬移完成后才提交丢弃计数、
有效缓存长度和下一状态。不得要求整段保留区一次性小于单次工作预算，也不得据此提高最低预算或
收紧合法长度字段位置。

### 6.3 有界进展和无死循环证明义务

主循环的每次迭代必须至少满足一项：

1. 增加`bytes_consumed`；
2. 增加确定丢弃字节数或缩短缓存待处理区；
3. 推进/回退同步匹配状态并扣减至少一个工作单位；
4. 交付并移除一条完整帧；
5. 因需要新字节、STOP、预算或入口错误立即返回。

不得存在既不改变状态/输入位置、也不扣工作单位、也不返回的分支。带同步头策略的每次比较和回退
均受工作预算限制；垃圾扫描近似O(n)，但本文只承诺“每次调用工作量不超过冻结预算”，不把复杂度
静态判断写成正式性能结论。

### 6.4 Plan和Workspace计费

Plan实际存储至少计入：strategy、固定/min/max长度、length descriptor、同步字节、同步搜索
预计算表、Pipeline引用/索引、对象及数组对齐。Compiler估算、Builder发布前复算、Arena实际使用
必须按相同类别和数量一致。

FramingWorkspace实际存储至少计入：对象本体、最大单帧半包缓存、搜索/长度/待交付状态、统计和
对齐。缓存容量按绑定Pipeline实际`maximum_frame_length`一次性分配，不按1 MiB Hard Limit
无条件分配；COMPLETE_RECORD继续默认不分配半包缓存。

创建前完成全部受检加法、乘法、对齐和`uint64_t`到`size_t`转换，并同时满足帧上限及单流内存
上限。分配或准入失败时不返回部分可用Workspace，不改变宿主原有对象。创建成功后的首次及后续
push不得调用Allocator、扩容或延迟创建搜索表。

合法`F <= maximum_frame_length`时缓存溢出在正确Plan/Workspace中不应发生；若冻结描述损坏或
容量不一致，应fail-closed为内部Plan/Workspace错误，不把它伪装成不可信输入的普通MALFORMED。

## 7. 独立手写向量

这些是契约预期，不是本轮运行证据。测试不得用被测Framer生成预期。

### 7.1 固定长度分片和多帧

Profile：`fixed_length=3`。

| 调用 | input | 预期 |
| --- | --- | --- |
| 1 | `AA` | consumed=1，frames=0，`NEED_MORE` |
| 2 | `01 02 BB 03 04` | consumed=5，依次交付`AA 01 02`、`BB 03 04`，`INPUT_EXHAUSTED` |

### 7.2 同步固定长度、跨块同步头和垃圾

Profile：sync=`A5 5A`，frame length=4。

| 调用 | input | 预期 |
| --- | --- | --- |
| 1 | `00 A5` | consumed=2，discarded=1，保留`A5`，`NEED_MORE` |
| 2 | `5A 01 02 FF A5 5A 03 04` | consumed=8，依次交付`A5 5A 01 02`、`A5 5A 03 04`，discarded=1 |

第二帧payload若包含`A5 5A`仍按固定4字节边界，不在帧内另起候选。

### 7.3 小端总帧长度

Profile：sync=`A5 5A`，length offset=2/width=2/little，min=4，max=8。

| 调用 | input | 预期 |
| --- | --- | --- |
| 1 | `00 A5` | consumed=2，discarded=1，`NEED_MORE` |
| 2 | `5A 06 00 11` | consumed=4，已知F=6但缺1字节，`NEED_MORE` |
| 3 | `22` | consumed=1，交付`A5 5A 06 00 11 22` |

大端独立对照把长度字节写作`00 06`；不得用小端实现的往返结果作为唯一预期。

### 7.4 非法长度后下一字节恢复

同上Profile，一次输入：

```text
A5 5A 09 00 A5 5A 04 00
```

第一个候选F=9超过max，应记录1个malformed；从第一个`A5`的下一字节恢复，最终丢弃前4字节并
交付`A5 5A 04 00`。全部8个input字节只计一次`bytes_consumed`，但对已缓存header的重扫/搬移
按实际发生次数再次计工作单位。

### 7.5 payload内同步头不提前恢复

同上Profile，输入：

```text
A5 5A 08 00 A5 5A 10 20
```

必须交付一条8字节帧，不能在offset 4处拆成第二候选。

### 7.6 STOP背压和重提后缀

Profile：`fixed_length=2`，input=`01 02 03 04`，sink对第一帧返回STOP。

- 第一次：交付`01 02`，frames=1，bytes_consumed=2，`SINK_STOP`；不得复制`03 04`。
- 宿主重提`03 04`：交付第二帧；第一帧不得重复。

### 7.7 预算残留和empty续处理

以内部可精确设置的小工作预算制造以下状态：非法长度候选的已消费header尚有搜索回退或分段压缩
待处理，或完整候选已经缓存为`DELIVER_PENDING`但帧回调预算未允许提交。第一次必须返回
`WORK_BUDGET_REACHED`及准确已消费前缀；随后`push(empty)`使用新预算继续，bytes_consumed=0，
最终只交付一次。分段压缩测试必须覆盖保留区大于单次预算、源/目标重叠、搬移中显式reset及补齐
后缀后的完整帧字节。测试不得靠sleep、网络或后台线程推进状态。

## 8. 一次检查点测试矩阵

| ID | 层次 | 必须精确断言 |
| --- | --- | --- |
| F01 | 兼容 | Schema 0.1～0.8 COMPLETE_RECORD配置、快照、Codec和计费不变；旧Schema拒绝stream属性 |
| F02 | Schema/Loader | 三策略正例；未知/重复/null/错误类型/非法联合成员及准确JSON Pointer |
| F03 | 固定 | 单字节分片、跨chunk、多帧+尾包、输入顺序不变 |
| F04 | 同步固定 | 垃圾前缀、同步头跨chunk、同步头最长后缀、payload内相同字节不误切 |
| F05 | 长度读取 | 1/2/4字节、大小端、header逐字节分片及独立手写F |
| F06 | 长度帧 | 合法最小/最大F、分片、多帧、最后半帧及合法F恰好交付 |
| F07 | 非法长度 | 0/过小/过大/整数边界立即拒绝，不等待、不扩容，下一字节恢复 |
| F08 | 恢复进展 | 连续垃圾、连续假sync、失败回退与分段压缩工作计数、下一合法帧及无死循环 |
| F09 | 合法未完成 | 最大合法F返回NEED_MORE；不扫描payload sync；reset/超时不由Core伪造 |
| F10 | 分层 | Framer先定F，随后现有结构零/单/多候选；bounded min/max不冒充帧界 |
| F11 | Codec隔离 | UNKNOWN/AMBIGUOUS/LENGTH/INTEGRITY/字段错误不回扫、不重复交付 |
| F12 | 背压 | STOP提交点、bytes_consumed、未消费后缀重提、无丢失/重复/内部队列 |
| F13 | Partial submit | 帧数/工作预算、安全停止；输入耗尽与预算同时发生；扫描/搬移游标的empty续处理 |
| F14 | 资源 | frame/submit/work/session exact/+1、Workspace状态成员、整数溢出、同步头上限及失败原子性 |
| F15 | Builder | Schema代际、strategy联合及非适用字段零值、索引、长度/同步/字段交叉关系、搜索表及损坏Draft拒绝 |
| F16 | 分配 | Compiler估算=Builder复算=Arena；首次和重复push无Allocator调用 |
| F17 | 生命周期 | 回调期借用、异步复制、同workspace并发/重入拒绝、不同流状态隔离 |
| F18 | 固定错位边界 | 插入/丢字节不宣称恢复；显式reset后新输入独立处理 |
| F19 | 业务示例 | 宿主push碎片，无轮询；帧内同步Decode；失败不触发业务成功交付 |
| F20 | 构建 | Windows Debug/Release专项、全切片、Product-only、Testing-off；目标依赖扫描 |

F20完成也不代表Linux、真实协议Golden、网络、设备、现场或正式性能通过。操作计数和无分配测试只
证明既定负载/路径，不升级为所有平台、所有Allocator或完整Runtime的性能保证。

## 9. 首片文件和实现边界

确认后的首片预计只涉及：

- `schema/pae.schema.json`及执行语义文档；
- `src/config_compiler`的SchemaIr、严格Loader/Validator、资源预算；
- `src/protocol_plan`的类型、FrozenPlan、Builder、Arena计费；
- 新建`src/protocol_framing`内部目标和单元测试；
- 独立公开合成配置及流式业务宿主示例/测试；
- 必要的顶层构建接入和本切片Windows验证文档。

不修改现有`DecodeCompleteRecord`输入语义，不新增网络、线程、Runtime/Session注册、C ABI、GUI、
Lab Result/Record/Event/Replay/Compare格式或稳定公共API。首片可调用现有Core，不把Framer逻辑
复制到Core或业务示例。

## 10. 与独立Lab UI工作树的冲突规避

Lab UI首检查点已合入主线；下一轮Lab仅在独立worktree细化离线Inspect契约，尚未授权源码。
Framer首片除下述已确认版本门禁例外外，明确零修改：

- `tools/protocol_lab/**`及所有Lab UI文件；
- Lab Result/Record/Event/Replay/Compare版本与证据格式；
- Lab UI契约和验证文档。

2026-09-10集中确认的唯一Lab源码例外：保留Schema 0.9 Compiler与完整Lab同一构建共存，
在旧Lab共享配置编译成功后、任何绑定/Codec/发送或证据执行之前，统一拒绝Lab不支持的0.9 Plan。
该改动只负责fail-closed，不让Lab执行Framer，不新增0.9 Result、Record、Event或指纹域。
已有C3/ExecutionBridge拒绝路径保留；不能只修无record-root路径而漏掉其他入口。

门禁实现及必要测试由PAE任务唯一负责，Lab UI任务不得同时修改。覆盖Inspect、Encode、UDP及
适用的有/无record-root路径，断言准确失败类别、无Codec调用、无网络调用、不产生伪成功证据。
UDP负例必须在任何网络调用前失败；证据错误分类复用既有合法语义，不把拒绝伪装成执行成功。
为实现门禁所需的最小Lab测试接线允许纳入，其他Lab重构和功能扩展仍禁止。
本段为已确认实施范围，不构成源码开工授权；实现前仍需总控派发完整任务包。

当前集成所有权安排（不构成源码授权）：Framer实施获授权后，Compiler、Plan及根CMake的
Framer相关增量由PAE任务唯一负责；Lab不得并行修改这些文件。下列共享入口文档由总控统一更新：

- `docs/README.md`、仓库根`README.md`、`docs/post-dec040-roadmap.md`；
- `tools/protocol_lab/README.md`。

Framer实现分支优先新增独立目录和测试，并让最终集成者一次性处理共享索引/CMake hunk；不得通过
复制UI改动、回退对方工作树或同时编辑同一hunk规避冲突。

## 11. 明确未覆盖与非阻断实现选择

- 内部C++类型、枚举名称、错误码数值和回调异常承载方式由实现按既有风格选择；它们不是需要
  再次人工确认的产品项，语义必须满足第5节。未来稳定API/C ABI冻结不属于本切片。
- 当前长度字段只表示总帧字节数。payload长度、word单位、偏置、比例和其他协议后置。
- 合法但迟迟不完整的声明长度没有Core超时；断链/reset由宿主决定。
- 同步头偶然碰撞不能证明恢复到真实边界；Bundle Hash、CRC或SUM也不提供来源认证。
- 纯固定长度流不能可靠恢复插入/丢字节错位。
- 完整Runtime活跃内存聚合准入、Transport生命周期和稳定C ABI均未实现。
- Lab/UI首片不接入Framer；Lab仅在配置成功编译后统一早拒绝0.9 Plan，因此本检查点没有Lab人工
  交互或Evidence Bundle结论。
- 本文手写向量已作为自动化预期的来源之一，但F01～F20的逐项证据和局限以Windows验证报告为准；
  文档示意本身不构成验证。

已确认六项及同步头资源数值可以在不改变现有COMPLETE_RECORD、Core Codec或Lab格式的前提下
同时落实，当前没有阻断首片源码任务包的产品语义未决项。若实现发现必须改变帧身份、长度含义、
回扫规则、STOP提交点或`bytes_consumed`定义，应停止对应部分并交总控，而不是静默调整本文。

## 12. 最小可执行源码任务包与Lab sidecar依赖

### 12.1 源码任务包

总控授权源码后，建议按一个可审查检查点顺序执行，不拆成功能微审批或多个产品版本：

1. **Compiler与Plan**：Schema 0.9三种严格联合、SchemaIr、Domain/Resource校验、Plan执行描述、
   Pipeline/Message交叉校验、Builder防御复核、同步搜索预计算和精确Arena计费；先完成F01、F02、
   F14、F15、F16的配置/冻结部分。
2. **内部Framer**：新增`src/protocol_framing`和`StreamFramingWorkspace`，实现第4～6节状态机、
   push、STOP/empty/partial submit、三种策略和零热路径分配；完成F03～F13、F17、F18。
3. **宿主示例与集成验证**：使用公开手写chunk序列同步调用现有Core，证明宿主主动push、无轮询、
   帧回调期Decode和失败不交付；完成F19及Windows Debug/Release、全切片、Product-only、
   Testing-off的F20。

首个任务包仅允许第10节Lab版本拒绝门禁例外，不修改UI；共享根CMake按第10节由PAE任务单一写入，新增模块与独立测试
统一接入。实现完成前不得把文档状态改为`IMPLEMENTED/VERIFIED`。

### 12.2 与当前Lab description sidecar稳定接口的依赖清单

当前Lab UI契约的description sidecar是同次Compiler事务产生、按最终Plan索引绑定的显示元数据，
旧`CompileJsonToPlan`保持no-sidecar零额外成本。流式首片对它的依赖严格限定为：

- Compiler新增Schema 0.9、FramingPlan和资源报告时，不改变旧Plan-only入口行为及旧Schema快照；
- Framer不读取、不拥有、不计费description sidecar，也不依赖Qt、UI model或Lab执行桥；
- 首UI allowlist仍只包含Schema 0.5/0.6/0.7；共享Compiler可编译0.9不表示UI支持0.9，UI应在自身
  `DocumentSession`边界按既有`UI_SCHEMA_UNSUPPORTED`职责拒绝并释放临时产物；
- sidecar首检查点明确不展示FramingProfile metadata，因此Schema 0.9新增FramingProfile作者描述
  不得被Framer任务顺带塞入sidecar；未来展示需另立UI扩展检查点；
- Framer完整帧回调不经过Lab C1 `ExecutionBridge`，也不要求拆分、重构或链接
  `pae_protocol_lab_internal`；
- 共享Compiler的可选sidecar事务已随`dc5c443`合入，Schema 0.9接入必须保持Plan和sidecar最终
  索引审计及失败不部分发布，但Framer执行描述和同步搜索表只属于Plan，不复制进sidecar；
- Lab Result/Record/Event/Replay/Compare、Values和Evidence版本均不因本首片升级。

### 12.3 真正未决事项

当前没有阻断源码首片的产品语义未决项。以下事项是后续范围或集成调度，不要求在源码开工前再次
向用户微审批：

- 内部符号名称、枚举数值和具体无锁/占用标志实现；
- 根CMake和共享索引由哪个worktree/集成者最终落hunk；
- 未来是否为Lab/UI增加stream人工观察能力；
- 未来稳定API/C ABI、完整Session/Runtime、Transport超时及非总帧长度表达。

只有实现必须改变已确认的Schema结构、资源数值、恢复/消费/交付语义或首片边界时，才构成新的
人工关键点。
