# PAE-DEC-040 位字段最小切片确认契约与实施清单

## 1. 状态与权威

2026-09-06用户确认本轮12项推荐原则及四组补充拍板；随后另行授权按本文范围实现和验证。
编号`PAE-DEC-040`承接DEC-039，用于本轮追踪。
状态：12项原则及四组配置／格式补充`CONFIRMED（已确认）`；
最小源码、公开人工配置和自动化测试`IMPLEMENTED（已实现）`；Windows x64 MSVC
Debug/Release证据`VERIFIED（已验证）`，边界见第11节及专项报告。
保留现有文件名用于链接稳定；名称中的draft不再表示四组补充仍待拍板。

接管基线`a418ac9`的Core仅有UINT64、BYTES、ENUM及完整记录输入；本轮在该基线上增加
Schema 0.2位字段能力。历史已验证用例不等于本切片验证，原有人工验收文档仍为独立变更组。

## 2. 已确认的12项原则

| 编号 | 已确认内容 |
|---|---|
| 1 | 在COMPLETE_RECORD（完整记录）内实现位字段双向编解码，不扩展通讯、线程、流式切帧或注册路由 |
| 2 | 本地原始协议作为需求参考；公开独立人工样例；歧义标记OPEN，通用测试不能冒充真实协议兼容 |
| 3 | 首批BOOL严格1 bit、UINT64及无符号原始值ENUM；排除有符号位字段、比例／偏移、浮点、BCD |
| 4 | 容器1/2/4/8字节；先解释大小端再解释lsb0/msb0；允许容器内跨字节，不允许跨容器；支持满宽 |
| 5 | 拒绝成员重叠、容器重叠、普通字段与容器重叠、越界、重复id和非法类型／位宽；无别名或联合体 |
| 6 | 必填base_value作为组包基础；动态及常量成员显式写入；未覆盖位保留基础值；缺失／越界失败，不截断 |
| 7 | Decode输出成员类型化值；base_value不隐含接收保留位约束；枚举沿用现有未知值策略 |
| 8 | 业务主要访问成员，无需同时提供容器原始整数；不冻结稳定公共API（应用程序编程接口） |
| 9 | 编译期预计算布局与掩码，冻结并计费；集中容器读写；不新增逐帧堆分配，避免满宽移位未定义行为 |
| 10 | Lab仅做新字段必要支持；先审查BOOL、Schema、Values、Replay、Compare及指纹版本影响，不默认版本不变 |
| 11 | 独立预期字节、布局／边界／非法输入、内存计费及Windows Debug/Release回归；Linux实测暂缓 |
| 12 | 先闭合契约和兼容性，再单独授权实现及验证；不授予Stage、Commit或Push权限 |

## 3. 配置结构（原OPEN-01，已确认）

Message新增`bit_containers`；业务成员继续位于`fields`，通过`container_id`引用容器。
这能沿用现有Message字段id空间及业务查找方式；容器不是额外业务字段。
以下仅为布局片段，不是完整ProtocolPackage，也不是当前Schema接受的配置：

```json
{
  "bit_containers": [{
    "id": "status_word",
    "container_offset": 2,
    "container_width": 2,
    "byte_order": "little_endian",
    "bit_numbering": "lsb0",
    "base_value": 40960
  }],
  "fields": [
    {"id":"enabled","value_type":"BOOL",
     "wire":{"codec":"bitfield","container_id":"status_word","bit_offset":0,"bit_width":1},
     "encode":{"source":"input"}},
    {"id":"mode","value_type":"ENUM",
     "wire":{"codec":"bitfield","container_id":"status_word","bit_offset":1,"bit_width":3},
     "encode":{"source":"input"},"unknown_enum_policy":"reject",
     "enum_entries":[{"id":"mode_idle","raw_value":0},{"id":"mode_active","raw_value":3}]},
    {"id":"counter","value_type":"UINT64",
     "wire":{"codec":"bitfield","container_id":"status_word","bit_offset":7,"bit_width":5},
     "encode":{"source":"input"}}
  ]
}
```

为简明省略display_name、description、source_ref等既有元数据，正式样例应补齐。
容器id在Message内唯一，成员id与普通字段共用唯一空间；container_width以字节为单位，
bit_offset/bit_width以bit为单位；container_offset从完整帧第0字节计算。
单字节容器省略byte_order，多字节必须显式给出；
容器统一bit_numbering，不允许成员覆写。拒绝空容器；容器及字段分属独立引用空间，
允许同名，但示例避免同名以减少误读。

## 4. 位编号计算与独立字节预期

设容器字节数B、位数N=8B、成员offset=o、width=w。
先验证`0 <= o < N`、`1 <= w <= N-o`，再计算右移量s：

```text
lsb0: s = o
msb0: s = N - o - w
raw  = (container >> s) & low_mask(w)
```

msb0的offset指成员最靠近容器最高有效位的一端，不反转成员数值内部的bit。
`low_mask(64)=UINT64_MAX`，其他合法宽度使用`(uint64(1)<<w)-1`。
64位满宽必有offset=0、shift=0，不执行任何移位64位的操作。
成员掩码为`low_mask(w)<<s`；Encode按`(C & ~mask) | (raw<<s)`写成员，
先确认raw可表示；容器外高位不得参与写出。BOOL只接收布尔值，不把非零整数自动转true。

人工向量：base_value=0xA000，enabled=true，mode_active=3，counter=21。
原始贡献为`0x0001 | 0x0006 | 0x0A80`，所以容器整数为`0xAA87`。

| 字节序 | 位编号 | enabled/mode/counter的offset（宽度分别1/3/5） | 期望容器字节 |
|---|---|---|---|
| little_endian | lsb0 | 0 / 1 / 7 | 87 AA |
| big_endian | lsb0 | 0 / 1 / 7 | AA 87 |
| little_endian | msb0 | 15 / 12 / 4 | 87 AA |
| big_endian | msb0 | 15 / 12 / 4 | AA 87 |

counter跨越字节边界；bit4～6及bit12～15未覆盖。改变接收端保留位不应仅因base_value不同而拒绝。
这里的预期由掩码算式给出，不来源于被测Codec的输出；本轮公开Golden文件继续以独立算式为权威。

## 5. 编译和执行契约

结构校验负责属性类型、未知属性和结构分支；领域校验负责容器引用、位布局、区间冲突、
常量／枚举原始值能否装入成员位宽。禁止错误配置进入PlanBuilder。
区间运算采用先比较后减法的防溢出写法，不直接依赖offset+width不会溢出。
base_value必须在容器宽度范围内；它的成员覆盖位不必为零，因为Encode会清除后写入。

建议BitContainerPlan保存字节位置、宽度、基础值及成员索引；FieldPlan保存已归一化的shift、
mask和容器索引。冷数据／热数据分离、PlanArena计费和Workspace容量同步更新。
Decode每容器读取一次；Encode先完成输入合法性检查，再组装容器、写出并完成既有最终复核。
失败不交付有效完整报文，输出Buffer在失败时的修改保证不得超出现有契约承诺。

常量成员沿用现有常量字段的Encode约束；接收常量是否作为强约束须核对既有实现后统一，
本轮不偷偷扩大普通常量的接收语义。位字段不加入Message Matcher作为新匹配原语。
BOOL内部需要独立LogicalValueKind及布尔存储，不能复用UINT64并要求业务端猜测。

## 6. Lab与版本演进（原OPEN-02～04，已确认）

### 当前事实

- `schema/pae.schema.json`固定schema_version为0.1，拒绝未知属性，当前无位字段分支。
- Values 0.1只接受UINT64、BYTES、ENUM，整数以规范十进制字符串保真。
- Result字段目前以kind加raw_value/logical_value字符串表示，StoredRun逐字段严格读取。
- UDP Result/Record/Event为0.2，离线为0.1；Values为0.1。它们不是一个共享版本号。
- Lab契约第7节明确格式变化必须升级format_version；不能把新BOOL偷塞进旧格式并宣称兼容。

### 已确认方案

1. 配置规范升为0.2以表达位容器，继续接受0.1旧配置；不改protocol_version及产品版本。
   0.1配置维持原允许集合，新特性不得使用0.1；具体Schema文件组织留待实现清单细化。
2. 新Values 0.2增加`{"id":"enabled","kind":"BOOL","bool":true}`，只接受原生JSON
   true/false，拒绝0/1及字符串；新程序保留Values 0.1读取。其他类型编码不改。
   Values版本不与配置版本机械绑定，以字段类型和消息契约匹配为准；无BOOL输入的
   Schema 0.2配置可以使用Values 0.1，Schema 0.1也可接收只使用旧类型的Values 0.2。
3. Schema 0.2配置执行使用Lab Result/Record/Event 0.3，统一离线和UDP的新能力表示；
   历史0.1/0.2继续读写原能力路径，不自动迁移旧证据。RX Metadata未改语义时保持0.2。
   输出代际由执行配置决定，不因本次是否出现BOOL而变化。
   BOOL输出kind=BOOL、raw_value为"0"/"1"、logical_value为"false"/"true"、enum_known=false，
   只允许0/false及1/true配对，不接受不一致值。
   保留当前字段形状；类型安全由内部Value和kind保障，并非把业务接口改成字符串列表。
4. 0.3指纹使用显式版本域和规范BOOL表示，包含类型、稳定字段id及确定性执行结果，
   不含网络地址、墙钟等历史传输因素。旧算法及既有Golden Snapshot不得重算覆盖。
   首批拒绝跨Schema代际替换配置Replay，也拒绝旧格式与0.3 Run直接Compare，
   明确报告不支持，不能以Hash不同伪报协议差异。现有旧格式组合保持已有支持边界。
   同格式代际替换配置回放延续既有差异判断与失败关闭；原始Frame字节比较仍允许跨来源。

以上四组已由用户确认。新增能力声明但继续旧版本的替代方案不采用。
错误诊断的具体命名和新格式序列化夹具须在实施时显式列出、评审并断言；不得改变已确认的
拒绝／兼容语义。未能编译配置或未取得有效配置版本时的失败输出，继续沿用既有诊断路径，
如实现发现必须改变该路径，应报告契约缺口而非自行变更版本规则。

## 7. 测试与实施顺序

| 测试组 | 最低覆盖 |
|---|---|
| 布局 | 1/2/4/8字节、大小端、两种编号、首末位、跨字节、64位满宽及边界掩码 |
| 值 | BOOL真假、UINT最小最大及溢出、枚举已知／未知、常量和动态输入 |
| 配置拒绝 | 成员／容器／普通字段重叠、越界、零宽、重复id、错误引用、错误类型、基础值溢出 |
| 输出 | 非零预填Buffer、保留位稳定、缺字段及非法值不交付完整报文、输入顺序不影响结果 |
| 接收 | 保留位变化不隐式拒绝、成员值正确、错误帧长度及缓冲容量失败 |
| 资源 | 单Plan计费、Workspace容量、首次调用分配、同Plan多Workspace执行及防误共享 |
| 兼容 | 旧配置／Values／离线0.1／UDP0.2回放与指纹不变，新BOOL表示严格校验，未知版本失败关闭 |
| 构建 | Windows Debug/Release合同及共存矩阵、Product-only隔离、格式检查；Linux不宣称实测 |

实施顺序：先准备公开输入／独立期望和格式夹具，再扩展Schema/Loader/Validator，
随后Plan/预算/Codec，再接通Lab表示与版本读取，最后共存回归。不为了引入BOOL新增网络功能。
本轮实现授权已按上述目录与验证范围执行；Stage、Commit和Push仍未授权。

## 8. 原始协议需求核对边界

本地原始Word字段表的只读结构核对确认存在单bit状态和多bit编号；私有来源、表索引、
合并单元格及待确认项记录在仓库外技术文档中。本公开草案不复制真实字段布局。
当前未完成版面视觉核对或真实抓包验证，不宣称真实协议配置已闭合。

## 9. 最终实施清单（本轮执行状态）

| 顺序 | 目标及预期范围 | 完成条件 |
|---|---|---|
| 1 | schema：组织0.1/0.2结构规则，公开人工配置／Values及独立期望 | 旧规则不放宽；正式样例含完整元数据，非仅复制本文片段 |
| 2 | src/config_compiler：结构、领域、资源校验和中间表示 | 引用／区间／位宽／类型／版本错误有稳定定位，错误配置不能发布Plan |
| 3 | src/protocol_plan：位容器及成员布局冻结、Plan计费 | 精确计费和批准预算复核，不绕过Validated/Budgeted能力链 |
| 4 | src/protocol_core：BOOL及位容器双向执行、Workspace | 独立字节预期通过，无新逐帧分配，保留位及失败输出符合现有契约 |
| 5 | tools/protocol_lab：Values 0.2、Result/Record/Event 0.3及读取、比较、回放 | 类型严格、版本选择确定、旧格式与指纹不变；不新增网络行为 |
| 6 | tests及必要CMake：合同、版本、资源、负向用例 | 以下矩阵有自动化断言，旧样本固定哈希且不得由新Writer重建替代 |
| 7 | Windows验证及文档 | Debug/Release专项及共存回归、Product-only隔离、格式检查；实际命令及结果留证 |

上述七项已在本轮授权范围内实现和验证；没有顺带重构Runtime、Session或公共API。
构建、测试、Stage、Commit和Push继续是独立门禁，本轮只完成前两项。

## 10. 兼容性验收矩阵

| 编号 | 输入／操作 | 必须断言的结果 |
|---|---|---|
| C01 | Schema 0.1，旧类型，Values 0.1 | 原离线0.1／UDP0.2行为及已有指纹不变 |
| C02 | Schema 0.1配置出现位容器或BOOL | 拒绝，不悄悄按0.2解释 |
| C03 | Schema 0.2，无BOOL动态输入，Values 0.1 | 类型匹配时允许，执行产物仍为0.3 |
| C04 | Schema 0.1，Values 0.2仅旧类型 | 类型匹配时允许，产物仍按旧配置选择 |
| C05 | Schema 0.2，Values 0.2，BOOL真假 | 原生布尔接收；正确位值及0.3类型化结果 |
| C06 | Values 0.1带BOOL；BOOL值为0、1或字符串 | 拒绝，不进行隐式类型转换 |
| C07 | BOOL Result的raw/logical不一致或enum_known=true | 即使清单哈希自洽也拒绝读取 |
| C08 | 原始离线0.1／UDP0.2普通Replay | 原有语义、事件、指纹和历史Transport边界不变 |
| C09 | 新0.3普通及链式Replay | 0.3保持，确定性比较相等，既有三种Replay模式语义不变 |
| C10 | 同Schema代际替换配置，字节变化或Codec失败 | 延续既有差异与失败证据规则，不伪造成功 |
| C11 | Schema 0.1与0.2跨代替换配置Replay，双向 | 明确不支持，不隐式迁移；不得仅返回协议DIFFERENT |
| C12 | 旧Run与0.3 Run直接Compare，双向 | 明确不支持，不能直接用不同指纹域判断协议差异 |
| C13 | 上述两代产物仅比较原始Frame | 允许，按实际字节给出相等或差异 |
| C14 | 未知版本、混合代际Record/Result/Event | 失败关闭，不按最近版本猜测读取 |
| C15 | Schema 0.2结果有／无BOOL或无Codec重新执行 | 都按新执行配置输出0.3；无Codec仍NOT_EVALUATED，不伪造比较通过 |
| C16 | 新UDP0.3带RX Metadata 0.2 | 版本组合明确允许，来源／事件／路径／字节绑定仍严格校验 |

网络相关版本路径可先用现有注入Adapter或历史证据验证；实际Loopback回归若需执行，
在实施授权中单列。该矩阵不要求非Loopback主动发送，也不升级人工Lab或协议Golden结论。

## 11. 实施与验证证据

提交后补记（2026-09-06）：DEC-040实现与范围内审查纠错已随`3b23db9`提交并Push；
人工Lab文档检查点为独立提交`263f514`。本轮已发现的审查项已关闭，不扩大下文验证边界。

本轮公开人工样例位于`examples/config/synthetic_bitfield_slice.*`，独立字节预期位于
`tests/protocol_core/golden/synthetic_bitfield/`。Schema、Loader/Validator、冻结Plan、
精确Plan/Workspace计费、Core Codec及Lab多版本读取/写入均已按第2～10节实现。

Windows x64 MSVC Debug/Release首次实现轮已分别完成位字段专项、Protocol Lab 5项测试、含三种
Parser候选的全切片33项共存回归和Product-only隔离构建。随后P2审查纠错收紧单字节容器
`byte_order`规则，并确保合法Schema 0.2 Plan建立后的读取／解析／资源／Replay准备失败仍采用
Result/Record/Event 0.3；本次Protocol Lab各6/6、全切片各34/34通过。C01～C16均有自动化映射；
自洽Hash篡改覆盖BOOL配对、未知版本和混合代际Event。实际命令、日志和未验证边界见
[PAE-DEC-040 Windows验证报告](windows-msvc-2026-dec040-bitfield-slice.md)。

后续P2审查纠错进一步补齐三项既有契约落实：Inspect及其Replay统一按当前Plan执行Frame资源
门禁；位容器固定Matcher在DomainValidator和PlanBuilder均按确定位检测冲突；Stored Field读取
覆盖全部受支持kind和值元组，并保证`comparison_equal`与确定性差异类别一致。PlanBuilder发布前
另防御单字节容器非`NOT_APPLICABLE`字节序、非法`bit_numbering`枚举，以及损坏Draft中的单Message
和全局容器数量；上限复用字段数量预算，与ResourceBudgetValidator保持一致并在辅助数组分配前拒绝。
正常配置入口原已执行相同预算，本项不增加Schema能力。该结论不改变格式版本、退出码或指纹算法。

本轮没有执行Linux、非Loopback网络、真实设备、硬件、现场或真实协议Golden验证；仓库外私有
需求证据仍存在合并单元格映射等缺口，因此不得据此声明真实协议完整兼容。
