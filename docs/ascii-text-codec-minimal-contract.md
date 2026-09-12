# ASCII 文本编解码与 Lab 输入可靠性检查点

日期：2026-09-10。
状态：三组八项及后续六项接口决策已由用户确认；Compiler/Plan/Core、隔离测试与最小宿主
示例已实现，Windows验证状态见专属报告；2026-09-12完成纯字面量与Schema负例限定纠错及总控复核。
用户已授权本引擎检查点提交和推送，交付结果以Git历史为准；原引擎批次不包含ASCII UI、网络及Lab证据格式。

原引擎批次接口进展：后续六项接口决策已确认并作为实现依据；该批次授权不扩展到Lab或UI接入。
Lab 输入修复已在 `feat/lab-ui-c1@8bfe50f` 提交并推送；当前已在主工作树启动合并，尚未生成合并提交。
第8、9节原修复授权边界保留，其“不执行Git”只约束原实施批次，不否认后续单独交付授权。

2026-09-12交付更新：本引擎检查点已在`main@256c5b7`提交并推送。后续
[Lab ASCII离线接入契约](lab-ascii-offline-integration-contract.md)的六项方向已确认，
后续独立授权的内部适配和UI接线已实现，并完成Windows Debug/Release自动验证及限定人工复核，
详见[第2阶段验证报告](lab-ascii-ui-stage2-validation.md)。当前保留未提交合并状态，
本批次增量尚未主动Stage、Commit或Push；原merge自动暂存内容保留。本文原批次边界按历史保留。

## 1. 基线与职责

基线为 main `1a5467c648f6973c427c2ef0ae970fbb545839c2`，已集成流式宿主示例与
Schema 0.8 Lab UI。此基线不包含本契约的 ASCII 实现。
Lab 工作分支已包含相应 UI 提交；本次工作必须先核对分支和工作树，保留既有修改。
Core 被动处理完整记录，不拥有 TCP、UDP、串口、线程或重连。
Lab 依赖 PAE，不为 UI 便利修改协议语义。

## 2. 第一项：布局和编码

概念维度为 Binary/Text、Decode/Encode；Text 显式声明 ASCII。
Hex 是字节显示方式，不是线上字符编码。第一版拒绝 UTF-8 等未实现编码，不自动探测。
JSON 配置自身的 UTF-8 编码不等于线上文本编码；JSON 解码后的明文也必须满足 ASCII 约束。
不加入 Qt、通用转码或新第三方依赖。消息内 Binary/Text 混合布局和混合字符编码不在首版。

## 3. 第二项：组装规则

采用有序字面量和命名变量片段。例如下列是概念表示，不是可加载 JSON：

```text
literal("Hello ") -> field(name) -> literal("!")
name bytes = 41 6C 69 63 65
output = 48 65 6C 6C 6F 20 41 6C 69 63 65 21
```

变量复用 BYTES 值加 ASCII 约束。字段定义具有唯一身份，长度以字节计，最小/最大长度明确。
默认内容限可打印 ASCII（0x20..0x7E）；控制字符只按显式规则开放，不隐式附加 NUL 或 CR/LF。
全角感叹号不是 ASCII，必须拒绝。宿主提供目标编码字节，不由 Core 转换 QString 或 UTF-16。
缺失、重复、未知输入字段、非法字符和长度越界均明确拒绝，不静默截断、补空格或规范化。
不支持脚本、表达式、数值格式化、大小写转换或 Unicode 规范化。

## 4. 第三项：解析规则

在完整输入字节上匹配，不要求分配中间字符串。首版支持：

- 固定字节长度变量；
- 由紧随的非空字面量终止的有界变量；内容不得包含完整终止序列，Encode 同样检查；
- 末尾变量取剩余完整记录，并受最小/最大长度限制。

禁止无法确定边界的相邻变长变量；不做回溯猜测。终止规则按确定性首次匹配执行，
超出上限或缺少终止字面量失败，不越界搜索。必须消费匹配整个完整记录，拒绝尾随垃圾。
空变量只在 min=0 时允许。多消息匹配必须保持唯一性，不能用“第一个成功”掩盖歧义。
编译阶段拒绝结构性歧义；运行期无法唯一匹配时失败，不交付字段。

## 5. 第四项：独立动作与统一交付

Decode 和 Encode 可以使用不同模板，字段定义可共享，不强制 RX/TX 对称。
Decode 输出沿用字段身份、类型和值；文本变量交付 BYTES 视图并可从 Plan 获知编码约束。
视图借用输入，跨调用保存由宿主复制；不得交付指向局部临时缓冲区的数据。
任意 Decode 失败不交付部分有效字段。Encode 写入调用方缓冲区，返回有效长度；
失败不得将中间缓冲内容标记为有效输出，也不承诺失败时物理缓冲区从未被触碰。
业务显示与解释仍归宿主。

## 6. 第五项：范围边界

本检查点只面向完整记录，不新增 CR/LF 分帧、转义、Runtime、端点注册、跨协议字段映射、
网络、UTF-8、文本数字转换或稳定公共 ABI。未来端点 RX/TX 绑定不属于当前实现授权。
报文中可有显式 CR/LF 字面量，但不意味着可从任意分片流中建立记录边界。

## 7. 第六项：编译、资源、兼容与复核

Compiler 预编译片段、字段约束和确定性边界；Plan 计量新增数组和字面量资源。
最大报文长度计算须检查加法溢出，使用既有资源上限机制；不引入无界增长或回溯。
执行期使用预分配 Workspace 和调用方缓冲区，不按模板执行动态脚本。
新增实现必须保持旧 Binary 接受域、诊断和执行语义；已有 Lab 未接入时提前明确拒绝 Text。

现有 Encode 后独立 Decode 复核不能直接使用不同格式的 RX 规则。
Text 如需反向复核，使用 TX 规则确定的内部校验规则，不自动声称任意模板可逆。
测试同时使用人工明确的预期字节和字段，不能只用同实现往返作为正确性证据。

### 实施前接口冻结门

初次八项确认时仅冻结行为；后续六项确认已确定下文Schema版本与属性方案。
PAE 源码授权前要求具备：确切 JSON 语法及版本门禁、控制字符声明方式、
Plan/IR/Workspace 增量及预算映射、诊断标识、共享描述与执行桥差异、TX 复核接线。
示例伪语法不得作为已支持配置发布。上述细节不得改变八项已确认的接受域；
如需扩大范围，先回报总控。冻结与源码实施授权是两个不同事实。

### 已实施接口细化（六项已确认）

本节依据当前 Compiler、IR、Plan、Core 整理，并按总控复核后的六项用户确认修订。
第2～7节的行为边界继续有效。当前实现由默认关闭的V10能力门保护；只有启用该门的
Compiler/Plan/Core构建可以加载和执行0.10，文档确认本身仍不等于执行验证。

#### 版本与能力门禁

- 以 `schema_version: "0.10"` 承载首个 ASCII 完整记录能力，保持 0.1～0.9 的
  Binary 接受域、结果格式和指纹域不变。
- 新增默认关闭的编译能力门禁 `PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC`，依赖现有
  Schema 0.9、Loader 和 Core 能力链；未启用时按现有 `UNSUPPORTED_FEATURE` 路径拒绝，
  不部分接受 0.10。
- Schema 0.10 首版配置包只允许 Text Message；Binary 保持在旧版本配置包。
  Text Message 不允许出现 `frame_length_bytes`、`matcher`、`bit_containers`、
  `integrity`、`computed` 等 Binary 布局属性和 ASCII 模板属性。
- Schema 0.10 的 Lab Result/Record/Event 版本及指纹域不在本次八项行为确认内；不得仅因
  采用 0.10 就推断编号。首轮不接 Lab，现有 Lab 提前拒绝0.10；格式版本留到接入时确认。

首轮不支持包内 Binary/Text 混用，也不支持单 Message 混合布局。

#### JSON 语法

ASCII Message 建议保留既有 Message 元数据与 `fields`，增加下列互斥布局对象：

```json
"layout": {
  "kind": "text",
  "encoding": "ascii",
  "decode": {
    "segments": []
  },
  "encode": {
    "segments": []
  }
}
```

`layout`、`decode`、`encode` 和各 segment 均拒绝未知属性；模板片段保持数组顺序。
decode/encode 至少存在一个，允许只读或只写消息；存在的动作必须有非空 segments。
`fields`属性仍为必需数组，但Schema 0.10允许其为空：此时每个动作只能由非空literal segment
组成，用于`PING\r\n`一类没有宿主变量的完整记录。该许可不扩展到旧Binary Message，不允许省略
`fields`、空动作、空literal或整条空报文，也不引入虚构占位字段。
首版片段严格为以下两种对象之一：

```json
{"kind": "literal", "text": "RX \r\n"}
{"kind": "field", "field_id": "name"}
```

- `text` 是非空 JSON String；JSON 解码后每个字符必须在0x00～0x7F内。
- 控制字符通过合法JSON转义显式书写，如 `\r\n`、`\u0000`；按指针和长度处理，不用strlen。
- 不提供第二套Hex字面量语法。不隐式追加 NUL、CR、LF 或空格。
- field segment 必须且只能给出 `field_id`；引用当前 Message 的 BYTES 字段。

ASCII BYTES 字段建议使用下列 wire 形状；`min_byte_length` 和 `max_byte_length` 均按字节，
满足 `0 <= min <= max`，且最大值受 Message 最大长度和现有资源配置共同约束：

```json
{
  "id": "name",
  "display_name": "Name",
  "description": "Synthetic printable ASCII value.",
  "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_INTERFACE_CANDIDATE",
  "value_type": "BYTES",
  "wire": {
    "codec": "ascii_text",
    "min_byte_length": 1,
    "max_byte_length": 16,
    "allowed_control_bytes": "09"
  },
  "encode": {
    "source": "input"
  }
}
```

`allowed_control_bytes` 省略时仅允许 0x20～0x7E；存在时只增加显式列出的
0x00～0x1F 或 0x7F，不削减可打印集合。使用单空格分隔、两位大写Hex、按值递增的非空集合，
拒绝重复字节、可打印字节和未知属性；空集合以省略属性表达。字段内容是否允许完整包含其紧随终止
literal，不能由该白名单放宽。

同一字段在每个动作模板中至多出现一次；Decode 和 Encode 可以引用不同字段集合，
但字段至少被一个动作引用。Decode 输出集合由 Decode 模板确定，Encode 必需输入集合由
Encode 模板确定。现有未知、缺失、重复输入字段检查仍适用于 Encode，不允许模板外 Values。
字段仅被Decode引用时省略字段级encode属性；被Encode引用时必须为 `encode.source=input`。
缺失动作的显式执行返回 `OPERATION_NOT_SUPPORTED`，不自动用另一个动作替代。
Pipeline自动Decode仅考察有Decode动作的消息；没有任何Decode动作时返回该状态。

#### 完整合成配置示例

以下示例与公开独立样例使用相同语法。它刻意使用不同 RX/TX 前缀并显式写出 CR/LF，
以证明动作模板独立；启用V10能力门后可加载，未启用时仍按不支持版本拒绝。

```json
{
  "schema_version": "0.10",
  "protocol_id": "synthetic_ascii_interface_candidate",
  "protocol_version": "1.0",
  "display_name": "Synthetic ASCII interface candidate",
  "description": "Non-loadable contract example for interface review.",
  "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_INTERFACE_CANDIDATE",
  "resource_profile": "desktop",
  "framing_profiles": [
    {
      "id": "record",
      "display_name": "Complete record",
      "description": "The host supplies one complete ASCII record.",
      "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_INTERFACE_CANDIDATE",
      "input_kind": "complete_record"
    }
  ],
  "pipelines": [
    {
      "id": "ascii_pipeline",
      "display_name": "ASCII pipeline",
      "description": "Synthetic complete-record pipeline.",
      "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_INTERFACE_CANDIDATE",
      "direction_id": "synthetic",
      "input_framing_profile_id": "record",
      "message_ids": ["greeting"]
    }
  ],
  "messages": [
    {
      "id": "greeting",
      "display_name": "Greeting",
      "description": "Independent decode and encode templates.",
      "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_INTERFACE_CANDIDATE",
      "direction_id": "synthetic",
      "layout": {
        "kind": "text",
        "encoding": "ascii",
        "decode": {
          "segments": [
            {"kind": "literal", "text": "RX "},
            {"kind": "field", "field_id": "name"},
            {"kind": "literal", "text": "!\r\n"}
          ]
        },
        "encode": {
          "segments": [
            {"kind": "literal", "text": "TX "},
            {"kind": "field", "field_id": "name"},
            {"kind": "literal", "text": "!\r\n"}
          ]
        }
      },
      "fields": [
        {
          "id": "name",
          "display_name": "Name",
          "description": "Printable ASCII name.",
          "source_ref": "SYNTHETIC_FROM_SCRATCH:ASCII_INTERFACE_CANDIDATE",
          "value_type": "BYTES",
          "wire": {
            "codec": "ascii_text",
            "min_byte_length": 1,
            "max_byte_length": 16
          },
          "encode": {"source": "input"}
        }
      ]
    }
  ]
}
```

#### 控制字符与字段边界

Compiler 按动作分别把模板编译为有序 segment，不在热路径解释属性名或字符串：

1. `min_byte_length == max_byte_length` 的 field 是固定长度，可与另一个固定长度 field 相邻。
2. 变长 field 后紧随非空 literal 时，该 literal 的完整字节序列是终止序列；按从左到右
   第一次完整匹配确定字段末尾，终止 literal 不属于字段数据。
3. 变长 field 位于模板末尾时取得完整记录剩余字节。
4. 变长 field 后紧随 field，或已声明动作模板为空，均在编译期拒绝；动作省略不是空模板。
5. Decode 必须使最后一个 segment 恰好结束于完整记录末尾；Encode 结果同样必须被 TX
   模板完整消费。合法前缀后存在额外字节不是成功。
6. 字段长度、字符集合和“内容不得含完整终止序列”三项分别检查；`min=0` 只允许空内容，
   不免除终止 literal 本身的存在性。

7. Encode还必须验证跨边界终止冲突：在“变量字节+紧随终止字面量”上首次完整匹配的位置
   必须等于变量长度。例：终止 `ABA`、值 `AB` 会产生 `ABABA`，首次匹配位置为0而非2，
   必须返回 `ASCII_TERMINATOR_CONFLICT`；不能只检查变量内部是否包含完整终止序列。
   Decode按首次匹配并检查剩余模板和完整消费，不为适配Encode输入而回溯。
   TX第二遍复核应重新提取边界并与原输入长度/字节比较，不能只按输入长度跳过字段。

多字节终止 literal 建议在 Plan 中预计算有界的前缀表，执行时线性查找首次匹配，避免
回溯和逐帧分配。单字节终止 literal 可走同一描述符，不增加语义分支。

#### IR、Frozen Plan、Workspace 与预算映射

拟新增数据均保持 SchemaIr → Budgeted Draft → Frozen Plan → ExecutionWorkspace 能力链：

| 层 | 候选增量 | 所有权和预算 |
| --- | --- | --- |
| SchemaIr | `AsciiTextLayoutIr`、动作模板、segment、字段长度和控制字节集合及各自 `JsonOrigin` | 仅 Compiler 可变对象；所有加法先做溢出检查 |
| Budgeted Draft | 动作 segment 数组、literal 字节、终止查找前缀表、字段索引和动作所需字段集合 | 与 Compiler 估算使用相同元素数量、`sizeof` 和对齐 |
| Frozen Plan | 只读 RX/TX segment span、ASCII 字段执行描述符、literal/prefix span、最小/最大记录长度 | 全部进入 Plan Arena；不得保留 IR 字符串或 JSON 节点引用 |
| Workspace | 首选不新增永久槽：唯一候选后按 Plan 做两遍有界扫描，Decode BYTES 直接借用输入，Encode 使用既有输入索引和 presence 位图 | 不按调用分配；若实现证明必须缓存字段边界，需另行确认并按每 Message 最大文本字段数预分配 offset/length 槽 |

Plan 资源分类建议如下：segment 与字段执行描述符计入 `EXECUTION_DESCRIPTOR`；literal 和终止
前缀表按匹配用途计入 `MATCHER`；动作字段索引计入 `INDEX`；容器头和 span 索引计入
`METADATA_CONTAINER`；新增标识文本仍计入 `STRING`。Compiler 估算、Builder 发布前复算和
Arena 实际使用必须逐类一致，不能只比较总数。

不新增未经确认的公开数值上限。记录最小/最大长度使用 checked add 汇总 literal 长度与字段
min/max，并受现有 `max_frame_bytes` 约束；Plan 与 Workspace 分别受现有
`max_plan_memory_bytes`、`max_session_memory_bytes` 约束；字段数沿用现有 Message/Plan 字段
限制。新增独立segment计数，不占用matcher上限。基于每字段每动作最多一次推导：

| 独立上限 | 推导 | Desktop | Constrained |
| --- | --- | --- | --- |
| 每动作segment数 | 2 × max_fields_per_message + 1 | 4097 | 513 |
| 每Plan总segment数，合计RX/TX | 4 × max_total_fields + 2 × max_messages | 32896 | 4128 |

该上限限制原始segments数组（不靠合并片段绕过），同时受既有JSON数组、节点、字节和Plan预算
约束，取更严者，不提升现有JSON限制。上述数值已由Resource Budget和Builder发布前复算共同执行。
literal字节和前缀表另按实际数量计费；计数未超限不代表内存准入成功。

#### 诊断候选

结构阶段继续复用 `UNKNOWN_PROPERTY`、`MISSING_PROPERTY`、`TYPE_MISMATCH`、
`INVALID_ENUM_VALUE`、`INVALID_HEX_BYTES` 和 `UNSUPPORTED_FEATURE`，并把 JSON Pointer 定位到
具体动作、segment 或字段属性。领域阶段建议冻结以下新标识：

| 标识候选 | 阶段 | 含义 |
| --- | --- | --- |
| `ASCII_LITERAL_INVALID` | `DOMAIN_VALIDATION` | literal 为空、非 ASCII 或表示不规范 |
| `ASCII_CONTROL_BYTE_INVALID` | `DOMAIN_VALIDATION` | 控制字节集合含可打印值、重复值或非 ASCII 值 |
| `ASCII_FIELD_LENGTH_INVALID` | `DOMAIN_VALIDATION` | min/max 关系非法或记录长度汇总越界 |
| `ASCII_FIELD_REFERENCE_INVALID` | `DOMAIN_VALIDATION` | 字段类型、动作引用次数或引用集合非法 |
| `ASCII_FIELD_BOUNDARY_AMBIGUOUS` | `DOMAIN_VALIDATION` | 变长字段没有可确定边界或相邻变长字段 |
| `ASCII_TEMPLATE_EMPTY` | `DOMAIN_VALIDATION` | Decode 或 Encode 模板没有 segment |

运行期新增 Core 状态 `ASCII_CHARACTER_NOT_ALLOWED`、`ASCII_TERMINATOR_CONFLICT` 与
`OPERATION_NOT_SUPPORTED`；长度错误复用 `BYTES_LENGTH_MISMATCH`，消息零/多候选继续复用
`UNKNOWN_MESSAGE`/`AMBIGUOUS_MESSAGE`，容量错误复用 `OUTPUT_SLOTS_TOO_SMALL`/
`BUFFER_TOO_SMALL`，TX 独立复核失败继续复用 `FINAL_REVIEW_FAILED`。所有失败保持
Decode 字段数为 0 或 Encode 有效输出长度为 0；不把被修改的缓冲区解释为有效结果。
具体 Lab 诊断映射和新证据格式版本必须在 Lab 接入授权时另行冻结。

#### 共享描述接口与 TX 独立复核

当前 `UiDescriptionSidecar` 只承载作者元数据，不宜把 wire 约束和动作模板塞入该对象。
建议 ASCII 约束以 Frozen Plan 中的只读执行描述符为唯一事实源；需要供 UI 显示时，由拥有
Plan 生命周期的共享执行桥物化自有 DTO，不把 Plan 内 span 或输入借用视图跨生命周期交给 UI。
首轮不新增完整 `ExecutionDescription` DTO，留到 Lab 接入时确定，不提前占用证据字段。

TX 复核必须是对**本次已组装输出**的第二遍独立检查，并使用 TX 模板，而不是调用现有
RX `DecodeCompleteRecord`：

1. Encode 先按 TX Frozen segment 顺序写 literal 和输入字段，并记录候选长度。
2. 独立 TX reviewer 从字节 0 开始重新核对每个 literal、字段长度、字符白名单、终止序列
   冲突和记录完全消费；字段字节与规范化前的原始 BYTES 输入逐字节一致。
3. 任一不一致返回 `FINAL_REVIEW_FAILED` 且有效输出长度为 0；缓冲区可能已被写入。
4. Binary Message 保持现有 Encode 后 Decode 复核路径。ASCII Message 的 Lab 执行桥不得
   再以 RX 模板 Decode TX 字节，也不得伪造 `review_decode` 成功。

首轮只做内部TX复核及隔离故障测试，不修改现有桥复核枚举和Result/Record/Event持久化。
后续Lab接入须区分RX_DECODE与TX_TEMPLATE，不把两者冒充同一类review_decode事件。

#### 兼容与零交付复核

- Schema 0.1～0.9 的 Binary JSON、Plan 内存快照、执行顺序和指纹算法不得因 ASCII 分支改变。
- 未启用 0.10 能力时在配置阶段失败；配置尚未形成合法 Plan 时不得生成貌似执行完成的结果。
- ASCII Decode 在结构唯一性、输出槽容量或字段检查任一失败时均不交付部分 BYTES 视图。
- ASCII Encode 在输入、容量、字符、终止冲突或最终复核任一失败时有效长度为 0。
- 完整记录长度始终受现有 `max_frame_bytes` 约束；本候选不建立流分片边界，也不改变
  `bytes_consumed`、Framer 或 Runtime 的职责。

#### 六项确认记录与实施边界

1. Schema0.10及默认关闭能力门；首轮仅Compiler/Plan/Core、隔离测试和宿主示例。
2. 首版0.10配置包仅Text；旧版本Binary不变。
3. `layout.kind=text`、`encoding=ascii`和literal `text`统一明文及JSON转义；字段wire仍为
   `ascii_text`以描述ASCII执行约束，不提供UTF-8接受域。
4. 支持单向动作和RX-only/TX-only字段，禁用动作明确拒绝。
5. 确定性边界、跨边界终止冲突和独立segment上限，使用上文预算推导。
6. TX-template复核与旧Binary复核隔离；UI DTO和Evidence格式延后。

当前实现未改变旧Plan快照、动作诊断优先级或预算模型；后续如需改变这些契约仍须先回报总控，
不得以文档数值允许为由跳过现有更严格资源门禁。

### 验收集合

覆盖明文成功、独立收发格式、空/最小/最大变量、边界字面量冲突、歧义拒绝、
非法 ASCII、截断输入、尾随字节、缺失/重复字段、输出不足、失败零交付与随后恢复。
覆盖资源溢出/上限和借用生命周期，回归受影响 Binary 路径。
Windows Debug/Release 针对性执行；未运行的 Linux、网络、Golden、现场和性能不作结论。

实现与验证证据见[ASCII完整记录Windows验证报告](windows-msvc-2026-ascii-text-slice.md)。

## 8. 第七项：Lab 限定修复（本轮已授权）

当前 BYTES 编辑器使用协议长度作为 QLineEdit maxLength，会将超长输入截短。
修复必须区分协议上限与有界编辑容量：

- 编辑容量内保留超协议长度草稿，由既有模型明确报错，不缩短为合法值；
- 超编辑容量的输入操作整体明确拒绝，不静默接受截短前缀；
- 拒绝操作有可见反馈，不把拒绝后的旧值冒充新输入的成功；
- 输入变更或失败清理旧结果及高亮，修正后正常恢复；
- 编辑容量有界、计算无溢出，不通过无限制编辑器规避问题；
- 保持 Binary 值语法、协议长度、computed 只读和旧版本接受域不变。

测试必须通过实际编辑器覆盖键盘、粘贴、替换/提交和恢复，包含 3 字节上限下输入
01020304 的精确案例；不能仅调用模型 setData 冒充输入行为覆盖。
编辑容量具体值及策略由 Lab 根据现有资源约束给出理由并记录；若会限制原合法输入则回报。
只修改 UI、相关测试与专属验证记录，不修改 Core、Compiler、Plan、Schema 或共享执行桥。
继续使用既有 Qt，不复制、升级依赖，不开启网络，不清理生成物。

## 9. 第八项：调度与交付

总控拥有本契约、索引、路线与共享文件。Lab 拥有第八节限定修复和专属验证记录。
Lab 从当前工作树安全开始；本轮不要求 Git 快进或切换分支，必要基线同步先回报总控。
主仓库未提交的本契约可从绝对路径只读参考，不自行复制到 Lab 分支造成双份修改。
PAE ASCII Compiler/Plan/Core已获单独授权并在当前工作树实施；Lab ASCII UI仍等待单独授权。
每侧一次针对性 Debug/Release、一次集中复核；后续合并一次必要集成验证。
本轮禁止 Stage、Commit、Merge、Push、Reset、Clean、Stash，不改变其他任务工作树。

依据：[业务嵌入契约](business-embedding-minimal-contract.md)、
[流式分帧契约](bounded-stream-framing-contract.md)、
[上轮确认契约](post-stream-host-and-ui-v08-checkpoint.md)。
