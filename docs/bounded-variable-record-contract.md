# 有界变长完整记录：实施契约

日期：2026-09-09。已交付代码基线：`7d2ef4f`（包括长度实现`0179abc`，均已Push）。
状态：用户已确认六项范围及本文实施细化；当前工作树已按本文完成限定实现与Windows离线
Debug/Release验证，正在等待总控审查，尚未Stage/Commit/Push。初稿形成时仅同步Markdown、
新属性和版本尚未实施的历史状态保留于Git基线`7d2ef4f`，不再代表当前工作树。

本轮实际接入默认关闭的Schema 0.8 Compiler/Core与Lab开关，公开合成样例不对应真实协议。
执行证据见[Windows验证报告](windows-msvc-2026-bounded-variable-record-slice.md)；该证据不升级
Linux、真实协议Golden、设备、现场、正式性能或流式切帧结论。

## 1. 范围与六项决策映射

| 已确认项 | 本文落实 |
| --- | --- |
| 长度含义 | 第2节：一段BYTES，一个头部计算长度，整帧/载荷字节数 |
| 布局与校验 | 第2～3节：固定头部、变长载荷、可选紧邻校验尾部 |
| 空载荷与异常 | 第4节：显式空值、上下限、完整记录、不补齐截断 |
| 匹配与失败顺序 | 第5节：结构唯一性先行，长度/CRC不消歧 |
| 所有权与计费 | 第6节：输入借用、调用方输出、冻结上限及无热路径分配 |
| 兼容与验收 | 第7～9节：必要Lab闭环、版本隔离、集中验收 |

仅处理`COMPLETE_RECORD（完整记录）`。调用方提供一条完整记录；不承担串口/TCP拆包粘包、
持续监听、重试、线程、Runtime/Session注册、Qt或DEI。每条变长Message恰有一个输入BYTES载荷和
一个计算长度字段，其他字段均在固定头部。不同方向独立配置，不假设RX/TX格式相同。
不支持多段变长、数组、条件字段、嵌套、TLV、任意尾部字段、转义、长度表达式或单位换算。

## 2. 配置语法与布局推导

### 2.1 Message形态

新Schema `0.8`继承`0.7`的固定Message。Message必须且只能选一种形态：

- 固定：沿用`frame_length_bytes`及既有规则。
- 有界变长：禁止`frame_length_bytes`，使用下列`layout`对象。

```json
"layout": {
  "kind": "bounded_payload",
  "header_length_bytes": 4,
  "payload_field_id": "payload",
  "min_payload_bytes": 0,
  "max_payload_bytes": 32
}
```

上述是Message内片段，不是完整可加载配置。四个长度/尺寸相关作者值必须使用既有严格无符号整数
词法；`header_length_bytes > 0`，`0 <= min_payload_bytes <= max_payload_bytes`，上下限包含端点。
允许上下限相等（包括0），但仍按该布局语义执行，不隐式改写成固定Message。
`layout`及新增对象拒绝未知、缺失、重复属性、null及错误类型。

记头长为H，实际载荷长为P，尾长为T：无integrity时T=0，SUM8时T=1，CRC时T=width/8。
实际帧长F=H+P+T，最小/最大帧长分别为H+Pmin+T、H+Pmax+T；全部加减先做溢出/下溢检查。
T从既有算法参数推导，不增加重复作者属性。最大帧长不得超过现有resource_profile上限。

### 2.2 载荷字段

`payload_field_id`必须唯一解析到本Message中的一个字段，该字段只能是：

```json
"value_type": "BYTES",
"wire": {"codec": "bytes", "byte_offset": 4},
"encode": {"source": "input"}
```

完整字段仍需既有id及元数据。该字段`byte_offset`必须等于H，禁止`byte_length`、byte_order、
conversion、constant或computed属性。省略`byte_length`仅对被layout引用的载荷字段合法；
头部其他BYTES继续必须有固定`byte_length`。载荷上限只由layout声明，不在Wire重复维护。
不因载荷为空省略输出字段：成功Decode仍返回该BYTES字段，size为0。

### 2.3 计算长度与头部

变长Message恰有一个`encode.source=computed`的长度字段，必须完整位于`[0,H)`。
沿用字节对齐UINT64及1/2/4字节存储、1字节无byte_order、2/4字节显式大小端规则。
该Message中`computed`只允许以下两种之一，均禁止range：

```json
"computed": {"kind": "length", "scope": "frame"}
```

```json
"computed": {"kind": "length", "scope": "payload"}
```

`frame`期望为F，`payload`期望为P；最大可能期望必须在编译时证明可由长度存储表示。
不允许变长Message使用旧`region`计算长度；旧固定Message仍可使用frame/region，不能使用payload。
所有普通字段、位容器、常量和fixed_bytes Matcher都完整位于固定头部，继承原重叠和初始化规则。
长度存储不得与字段、位容器或Matcher存储重叠。载荷及尾部不得有固定偏移字段或Matcher。
未声明头部字节的初始化/匹配行为沿用原规则，不新增必须覆盖每个字节的限制。

## 3. 完整性校验

每Message至多一个integrity，沿用现有SUM8或参数化CRC，不修改CRC参数接受域或算法。
有界变长Message的校验存储必须紧接载荷，使用：

```json
"storage": {"anchor": "payload_end", "byte_order": "little_endian"}
```

SUM8禁止byte_order；CRC按既有16/32位规则必须指定byte_order。禁止同时提供byte_offset或其他
相对位移。存储起点为H+P，大小为T，末端恰为F。固定Message仍使用原绝对storage语法。

覆盖范围二选一，不增加离散区间：

- 固定：`{"byte_offset": S, "byte_length": N}`，N>0，必须证明S+N<=H+Pmin；
  因此每个合法载荷长度下都有效，且永不包含校验存储。
- 动态：`{"byte_offset": S, "end": "payload_end"}`，0<=S<=H；覆盖`[S,H+P)`，
  禁止byte_length。允许S=H、P=0形成空区间：SUM8结果为0；CRC为既有算法对空字节序列的结果
  （含其初始化、反射及最终异或规则），必须有独立预期测试。

范围可以包含计算长度的存储和其他头部字段；先写长度后生成校验，不产生计算依赖环。
固定Message禁止anchor/end新语法，以免本检查点改变原有布局接受域。

## 4. 完整输入与边界

Encode必须显式提供载荷BYTES，即使size=0；缺失返回MISSING_FIELD，提供长度不在上下限内
返回BYTES_LENGTH_MISMATCH。非空ByteView的空指针及输入/输出重叠仍按原参数、安全规则拒绝；
空视图允许null且不得解引用，对空地址不做无意义的指针算术。

Decode先以实际输入尺寸检验候选结构，在唯一合法候选上安全推导P=F-H-T。内部长度字段只负责
一致性检查，绝不决定本次读取边界、申请大小或补齐/截断。实际尺寸不属于任何候选区间时返回
UNKNOWN_MESSAGE；属于唯一结构但声明长度错误时返回LENGTH_MISMATCH。

不返回NeedMoreData，不等待更多字节，不跨调用重组；一条输入中多出的字节可能构成另一条合法
载荷，若所有语义均一致，引擎不能猜测“这些字节本来不该存在”。记录边界由宿主及未来Framer负责。

## 5. Matcher与执行语义

### 5.1 结构唯一性

变长Message隐式使用由layout推导的闭区间帧长条件，不提供新的作者帧长范围Matcher。
其matcher.all必须至少有一个fixed_bytes，且禁止frame_length_equals及重复声明长度范围。
Compiler把隐式长度条件纳入现有候选域分析；固定与变长、两个变长Message均需分析相交可能。
长度区间相交且fixed_bytes没有互斥证据，即属于可能相交，不依赖CRC或内部长度值证明不相交。
同Pipeline候选相交由Compiler拒绝，Builder独立防御；跨Pipeline按(Pipeline,Message)身份汇总，
多候选返回AMBIGUOUS_MESSAGE且不执行长度、CRC、字段Decode。唯一候选才进入主Codec。
这保留旧固定Matcher语义，并要求更新实际结构查询而非只更新主Decode。

### 5.2 操作顺序

Decode：入口参数/Plan/Workspace安全门禁 → 结构唯一性 → 实际布局及输出槽容量 → 长度一致性 →
SUM8/CRC → 字段转换 → 整体交付。失败时field_count=0，raw诊断失效，不交付部分字段。

Encode：入口门禁 → 引用/禁止覆盖/重复输入 → 必填 → 字段值与载荷长度（按冻结字段顺序） →
实际布局及输出容量/重叠安全 → 业务与常量写入 → 长度回填 → 校验 → 最终复核 → 整体交付。
布局算术安全先于任何依据该布局的指针运算；所有写入均在预检之后。
缺省构建不接受新代Plan；非法冻结描述不能退化为未知报文或尝试越界执行。

### 5.3 错误映射与身份

| 场景 | 结果 | 失败身份 |
| --- | --- | --- |
| 缺少载荷输入 | MISSING_FIELD | 按既有必填字段规则，无虚构输入索引 |
| 载荷输入类型错/尺寸超限 | TYPE_MISMATCH / BYTES_LENGTH_MISMATCH | 载荷字段及实际Values输入索引 |
| 业务覆盖计算长度 | COMPUTED_FIELD_OVERRIDE | 长度字段及实际输入索引，优先于该值类型检查 |
| 无合法实际尺寸/头部结构 | UNKNOWN_MESSAGE | 无虚构字段身份 |
| 跨Pipeline多候选 | AMBIGUOUS_MESSAGE | 不按内部长度/CRC消歧 |
| 声明长度不等于实际F或P | LENGTH_MISMATCH | 长度字段，无Decode输入索引 |
| 校验错误 | INTEGRITY_FAILED | 沿用现有非业务字段校验诊断 |
| 输出容量不足 | BUFFER_TOO_SMALL | required_size为本次实际F，而非最大帧长 |
| 实际输出复核不符/内部故障 | FINAL_REVIEW_FAILED / INTERNAL_ERROR | 沿用现有区别及失败交付 |

不新增CodecStatus枚举，仅扩展新Schema上的布局处理；旧枚举数值、旧代诊断优先级不变。
非Decimal失败conversion_error为空。除容量不足诊断可报告实际required_size外，其他失败尺寸输出
沿用现有约定，不伪造有效bytes_written；本次实际F未安全确定前required_size=0。
Compiler作者错误按结构/领域/资源既有阶段分类：错误指向实际layout属性、payload_field_id、
wire、computed、integrity.range或storage的JSON Pointer；实现测试应精确绑定阶段/代码/路径。
Builder损坏Draft走既有内部契约失败，不发布Plan；新增派生值不能信任Draft自报结果。

### 5.4 最终复核

复核本次实际F范围内的输出，从实际字节重新读取长度、载荷及校验；比对声明与F/P、业务输入
和独立复算校验。不能只复用写前长度缓存或将未使用的最大容量尾部当作报文。
失败Buffer可能被修改，调用方不得发送/缓存为有效Frame；不承诺擦除Buffer。
Lab Encode后的既有独立Decode复核必须使用实际F，而非输出capacity。

## 6. 冻结、计费与生命周期

冻结布局种类、H/Pmin/Pmax/T、推导帧长区间、载荷/长度字段索引、计数scope、校验锚点/范围。
Compiler执行领域及预算验证；Builder重新推导尺寸、引用唯一性、范围、存储关系、Matcher域及
完整资源报告。不得通过损坏Draft伪造更小计费、遗漏描述或越界索引。

新增Plan实际结构、数组和对齐均计费，包括既有结构因扩展增大的部分；Workspace按配置上限在
初始化阶段准备必要缓存，每次调用只写实际长度。无需载荷复制的部分不得机械预分配第二份最大
载荷；调用方输入/输出不冒充Workspace已分配内存，内部缓存也不得漏计。

沿用[执行语义](../schema/protocol_plan_execution_semantics_v0.1.md)的所有权：

- Decode BYTES为借用输入Frame的只读ByteView，不随Workspace自动拥有副本；宿主保证输入在
  消费期间存活且未被改写，需要异步/长期保存时自行复制。输出槽仍由调用方提供。
- Encode输入借用至同步调用结束，不保留到下一次调用；输出Buffer由调用方提供。
- Workspace绑定并借用Plan，Plan存活更久；并发/重入调用各自独占Workspace。
- Lab证据/结果需要自有副本时在工具层复制，不改变Core借用语义。

正常首次及重复Decode/Encode不得新增堆分配或读取JSON；不实现全局Session聚合准入、池化或
调度，不声称最大配置已通过目标板性能/容量实测。

## 7. 兼容版本与Lab接入

以下为本检查点实施目标。新增布局会改变Schema接受域及字节结果的解释，因此隔离确定性域；
不因沿用BYTES值类型而混用旧执行代次。固定Message置于Schema 0.8也统一进入新代。

| 项目 | 已交付长度链 | 有界变长目标 |
| --- | --- | --- |
| Schema | 0.7 | 0.8 |
| Result | 0.8 | 0.9 |
| 确定性指纹域 | 0.8 | 0.9 |
| Run Record | 0.9 | 0.10（版本字符串，不按浮点解析） |
| Event | 0.7 | 保持0.7 |
| Values | 0.1～0.4 | 0.1～0.4保持原代各自类型接受域；Schema 0.8可另用0.5 |
| CLI封装 | 0.1 | 保持0.1 |

复用现有指纹精确编码：实际Frame、实际BYTES值、已确定结构身份和状态参与，不把capacity或
未初始化缓存加入指纹；仅更新域及必要版本映射，不另造一套Reader/Writer。
新代底层Result不增加虚构CRC字段或专用载荷诊断字段，真实布局关联由运行资格层对冻结Plan验证。
后者必须检查实际F/P、BYTES输出长度、计算长度值和失败身份/输入索引；不能仍按固定field.width
检查载荷。失败结果不得由资格校验重新交付字段，完整性不用于结构身份选择。

接入现有离线inspect/encode/replay/compare；新代inspect/encode/replay强制record-root，Replay
使用原配置与原输入、不接受配置替换。父Record、历史Result、Schema和指纹域精确绑定；旧代
合法证据继续可读，新旧混代拒绝，跨指纹域Run Compare拒绝，原始Frame比较沿用现有规则。
错误Replay可比较EQUAL，但不改变本次失败；沿用退出码分类及expect-status封装，不伪造成功。
Event维持真实阶段及既有语法，不为内部载荷计算新增事件。

建议开关`PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER`、`PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V08_VARIABLE`
默认OFF，沿用前代依赖链并严格拒绝不完整组合。Core能力与Compiler新代一致，Lab开关不反向
引入UI/Transport；不升级普通UDP链、不新增命令、不开放新的公共API。

## 8. 独立预期与验收矩阵

至少一个公开合成向量：头部为`A5 LL`，LL是1字节整帧长度；载荷范围0～3，尾部SUM8覆盖
从偏移0至载荷末端。长度及校验可直接人工核算，固定预期如下；当前工作树已用独立常量验证：

| 载荷 | 独立预期完整Frame |
| --- | --- |
| 空 | `A5 03 A8` |
| `10 20` | `A5 05 10 20 DA` |
| `00 FF 01` | `A5 06 00 FF 01 AB` |

另设载荷计数及CRC向量，使用既有已核验CRC参考证据或独立固定预期；需要新的外部工具核算时
必须单独说明，不能未获授权运行或用被测Encode生成自身Golden预期。

| 验收组 | 必须证明 |
| --- | --- |
| 边界 | 显式空、最小/中间/最大、越上下限、空指针规则、超出长度Wire表示能力的配置拒绝 |
| 布局 | frame/payload、1/2/4字节及大小端代表、头部整数/bit/Decimal与载荷共存、有/无尾部 |
| 校验 | SUM8/CRC、固定/动态范围、空覆盖、长度计入校验、逐校验存储字节损坏及恢复 |
| 唯一性 | 固定/变长相交、变长/变长相交、合法互斥、跨Pipeline歧义且Codec未执行 |
| 错误顺序 | 缺失/空区分、覆盖早于类型、长度早于CRC、错误尺寸/多余字节边界、失败零交付 |
| 冻结与资源 | 损坏索引/计数/范围/帧长/预算单点负例、合法Draft对照、容量实际F、热路径无分配 |
| 最终复核 | 长度/载荷/尾部实际字节故障、不一致与内部错误区分、失败后有效操作恢复 |
| Lab与宿主 | 多种载荷业务Encode/Decode、成功/失败链式Replay、同代Compare、混代自洽篡改拒绝 |

Windows Debug/Release串行运行专项和受影响Compiler/Core/Lab离线回归，明确排除udp/loopback/
network；共享Matcher或版本域影响的旧代测试不能遗漏。按依赖改动复核Product-only及Lab-on/
Testing-off，保持不引入Qt或新Transport。验证目标、配置开关、命令和日志必须如实记录；不在契约
阶段提前写PASS或固定测试总数。无需无理由重跑无关成功矩阵。
本检查点不证明Linux、真实协议Golden、硬件、现场、正式性能或Decimal独立Oracle门禁。

集中审查补充明确：有界Message必须恰有一个计算长度字段；所有位容器必须完整落在固定头部；
只有`payload_field_id`引用的BYTES字段允许省略`byte_length`。公开配置入口在Domain Validator
失败关闭，Builder对损坏Budgeted Draft独立复核这些不变量。该纠错不改变Schema或证据版本。

后续集中复核进一步澄清“沿用原规则”的覆盖含义：固定头部`[0,H)`的每个字节仍须由普通字段、
位容器或`fixed_bytes` Matcher完整定义，不允许Encode从调用方输出Buffer继承空洞值；动态payload
及紧邻trailer则由冻结的载荷与完整性描述符在实际F内写入。Builder必须独立限制有界计算长度为
FRAME或PAYLOAD，并从完整性算法重算trailer宽度：无规则为0、SUM8为1、CRC为`width/8`。
这些是既有有界布局不变量的失败关闭，不增加填充值、REGION或其他配置语义。

总控后续确认空载荷证据表示：Result 0.9允许BYTES字段的`raw_value`/
`logical_value`同时为空串，并要求`enum_known=false`；Result 0.8及更旧代继续拒绝该组合。
Values 0.5仅继承0.4的既有类型与严格规则，只扩展显式BYTES `"hex":""`，且只能用于
Schema 0.8执行；缺失、`null`和空值仍是三种不同输入。载荷长度是否满足Plan下限仍由
Core判定，不在Values解析器中伪造成功。该纠错不升级Result/Record/Event版本。

## 9. 实施分工与授权

《子任务推进》在后续实施授权内负责完整PAE检查点（含必要Lab离线兼容、业务示例、测试和文档）。
《Lab应用推进》仅负责其另行授权的应用设计/实现，不修改本Schema、Core或证据版本；其描述
投影提案由总控协调，不为UI把控件、Tab、连接参数写入PAE配置。
共享CMake/Compiler/Lab文件同一时段只分配一个写入方；Lab未来写代码前另行协调worktree。
默认完整检查点一次集中审查、一次语义化提交；Stage/Commit/Push仍需明确授权。
本次实现仍不创建分支、不修改任何生产项目或清理既有工作树；Git交付另行授权。

相关基线：[固定长度契约](length-field-minimal-contract.md)、[CRC契约](crc-minimal-contract-draft.md)、
[Lab契约](protocol_lab_contract_v0.1.md)、[路线](post-dec040-roadmap.md)。
