# PAE-DEC-042B 精确比例与偏置转换确认契约

2026-09-08当前检查点：C1实现`115db10`及入口文档`ef350d5`已提交并Push。
C2六项方向决策及第19节修订后精确契约均已确认；CLI退出码映射延至C3实施前冻结。
第20节第一段已获授权并完成限定实现与Windows验证。A/B/C1已形成提交检查点；第21节C2第二段
已在未提交工作树完成限定实现与Windows验证，待总控复核；C3未实施；
下文早期批次记录保留其历史时点，旧建议称谓以第19节最终确认状态为准。

创建日期：2026-09-06。确认更新：2026-09-07。第1节十项决策及第3～6节四组补充方案
均为CONFIRMED（已确认）。用户随后授权隔离算术验证，固定256位候选已完成Windows
Debug/Release隔离测试；只读审查未发现明确实现缺陷，已补齐P2测试证据缺口。
Core双向转换第二段已完成审查及限定Windows验证，随`90c5165`提交并Push。
Lab第三段六项补充决策已确认，见第10节；A纯格式模块随后已获授权并完成限定实现，B/C仍未开始。
Values 0.4仅由隔离测试解析，普通Lab及0.6证据仍不是当前可执行能力。
文件名保留draft以维持链接稳定；生产接入A1～D4共16项已确认，见第8节。
2026-09-07追加确认首段实施临时隔离门：本轮仅接入Schema、SchemaIr、编译冻结、
PlanBuilder防御复核与资源计费，当时不接入Core或Protocol Lab执行路径。首段已完成
总控审查并随`4d26d42`提交、Push；证据见
[首段验证报告](windows-msvc-2026-dec042b-compiler-slice.md)。
2026-09-07续：第二段已接入Core私有256位算术、Decimal64、Workspace诊断、失败顺序及
最终重读复核；Schema 0.5仍禁止与Protocol Lab共存，证据见
[Core验证报告](windows-msvc-2026-dec042b-core-slice.md)。

## 1. 十项已确认决策

1. DECIMAL64为`coefficient:int64_t, scale:0..18`，值为coefficient×10^(-scale)。
   Encode接受等价表示；Decode、Lab输出与确定性比较规范化，去除可消除的小数尾零，零为{0,0}。
   不隐式转换double。DECIMAL64的scale为小数位数，不是转换比例。
2. 保留value_type=UINT64/INT64定义原始整数，通过可选conversion声明DECIMAL64输出；
   scale和bias显式填写。无conversion保持既有行为；位字段、BOOL、ENUM等不接受conversion。
3. 比例/偏置分子为INT64，分母整数范围1..10^18；比例非零、允许负比例，偏置可零。
   编译期先约分，约分后分母只能含2和5，且各参数最多18位小数；不接受1/3近似值。
4. 编译期检查参数，运行期检查具体结果；不要求合法配置覆盖整个Wire整数域都成功。
   有限十进制仍可能超出DECIMAL64系数范围，不隐式截断、饱和或舍入。
5. Encode精确反算`raw=(logical-bias)/scale`，非整数或超Wire范围均拒绝；转换字段只接收DECIMAL64，
   不允许传入UINT64/INT64绕过转换。
6. 转换字段只支持encode.source=input；constant+conversion编译期拒绝。无转换整数常量保持原行为。
7. 全程精确整数运算，不依赖MSVC专属类型、__int128或long double；先做独立小型算术验证，
   优先评估固定容量、无堆分配宽整数，证明中间位宽上界，不引入通用任意精度表达式引擎。
   不能因窄中间乘法先溢出而拒绝最终可表示的结果，须支持抵消。
8. Decode交付类型化DECIMAL64，另以显式INT64/UINT64标签保留原始值诊断，额外存储准确计费。
   区分类型、非法十进制表示、反算非整数、Wire越界及业务值越界，精确枚举另行冻结。
9. 保持结构唯一性→容量→SUM8→字段解释/转换→整体交付；Encode预检→写入→SUM8→最终复核。
   最终复核比较数学值，不要求非规范输入与规范输出逐成员相同。
10. 新Schema 0.5、Values 0.4、Result/Record/Event 0.6；RX Metadata保持0.2，产品版本不升级。
    旧行为/指纹保持不变，新Schema无转换亦使用新代证据；验证等价表示、精确算术、历史兼容、
    SUM8、失败Replay、资源及Windows Debug/Release和隔离构建。

已确认的字段配置片段（不是完整可加载配置）：

```json
{
  "value_type": "INT64",
  "conversion": {
    "kind": "linear",
    "output_type": "DECIMAL64",
    "scale": {"numerator": 1, "denominator": 10},
    "bias": {"numerator": -40, "denominator": 1}
  }
}
```

## 2. 实现基线与证据边界

DEC-042A及审查修复已随`7807b9a`提交并Push；DEC-041文档收口为`97da00e`，亦已Push。
缺省构建仍只支持Schema 0.1～0.4；DEC-042B隔离算术与编译首段已分别随`8fd2019`、`4d26d42`
提交并Push。专用开关下的DECIMAL64 Core双向转换第二段已随`90c5165`提交并Push。
第二段限定Windows合成向量证据见Core报告；它不证明完整输入域、性能或Lab证据链正确性。
真实Golden、Linux、硬件及现场仍未验证。
本文不扩展Runtime、C ABI、传输、线程、业务派生、有符号位字段或完整constraints能力。

## 3. 补充A：类型接口与作者输入（已确认）

确认新增内部Decimal64值结构，成员coefficient为int64_t、scale为int32_t；先检查scale范围，
再规范化。不能先消除零而接受{0,-1}或{0,19}这类非法表示。
只去除小数位范围内的尾零，不允许产生负scale；正负INT64极值不直接取绝对值。
新增RawIntegerValue诊断对象：显式INT64/UINT64标签及对应值，仅在转换字段成功交付时有效。
未转换字段保持现有槽位语义，不把所有字段强制扩展成两份业务值；存储与生命周期按第8节A组执行。

Values 0.4确认条目：

```json
{"id":"temperature","kind":"DECIMAL64","decimal64":{"coefficient":"1230","scale":2}}
```

coefficient采用规范INT64十进制字符串，拒绝-0、正号、前导零、小数、指数和越界；
scale采用0..18精确JSON整数Token。表示可以非规范，但数值Token必须规范，二者不能混淆。
配置numerator沿用signed整数Token规则（-0归一），denominator沿用unsigned规则（拒绝-0），
拒绝小数/指数Token、null、未知属性；分母上限按作者输入检查后再约分。
比例分子零拒绝，偏置零规范为0/1；结果规范化必须先于最终coefficient范围检查。

## 4. 补充B：诊断与失败顺序（已确认）

确认复用现有CodecStatus，不把每个数值原因都升级为全局状态；增加受限的转换失败原因：

| 情况 | 已确认状态/原因 |
| --- | --- |
| 输入标签不是DECIMAL64 | TYPE_MISMATCH |
| scale不在0..18 | INVALID_ARGUMENT / DECIMAL_SCALE_OUT_OF_RANGE |
| Encode反算非整数 | VALUE_NOT_REPRESENTABLE / RAW_NOT_INTEGRAL |
| Encode整数超实际Wire范围 | VALUE_NOT_REPRESENTABLE / RAW_OUT_OF_RANGE |
| Decode规范化后仍不能用DECIMAL64表示 | VALUE_NOT_REPRESENTABLE / LOGICAL_OUT_OF_RANGE |
| 最终复核不一致 | FINAL_REVIEW_FAILED，沿用既有输出失效规则 |

配置诊断仍由结构/领域层产生，指出numerator、denominator或encode.source等位置。
Encode保留现有引用、重复输入、常量覆盖等前置检查，随后按字段稳定顺序作表示、反算、Wire检查；
成功预检才写报文。旧Schema与Schema 0.5的精确检查顺序按第8节B组执行，不由实现任意选择。
任何失败不得交付部分字段；有效数量为零不承诺底层槽位或Buffer完全未改写。
固定宽整数内部溢出若突破已证明边界属于实现/内部错误，不得伪装成合法用户值不可表示。

## 5. 补充C：精确算术验证清单（已验证、已审查并补测）

候选推导路径：将约分后的比例和偏置表示为A/10^t与B/10^t，t取两者所需小数位最大值，t≤18。
Decode计算N=raw×A+B，再规范化N/10^t；Encode对logical=C/10^s采用u=max(s,t)，
计算`(C×10^(u-s)-B×10^(u-t))/(A×10^(u-t))`，仅余数为零且商在Wire范围内时成功。
负分母在内部归一，零比较、符号与除法余数规则必须明确。

上述推导已在隔离报告展开并经只读审查：|分子|≤2^63、10^18<2^60，故公共十进制系数幅值<2^123；
raw幅值<2^64，因此Decode乘加幅值<2^188。Encode中的缩放项可用同样保守界检查。
确认以固定256位幅值+显式符号作为生产内部算术方案，不将隔离结果当成形式证明或生产实现验收。
需验证约分、负最小值、乘加进位/借位、除法、规范化、输出转换的全部中间步骤；
不能只证明主公式而忽略辅助运算。不得把宽整数暴露为稳定公共API。

算术验证阶段只产生隔离测试辅助代码，尚不接入生产Core或更改Schema；该阶段已获单独授权。
未来可使用宿主侧独立高精度Oracle（参考计算器）交叉检查，不作为产品依赖；具体工具先批准。
测试集至少包含：

- {123,1}与{1230,2}、不同零表示、负数尾零、非法scale；
- 1/10与-3/20、3/6约分接受、1/3拒绝、需超过18小数位的分母；
- 比例1/10、偏置-40：12.3→523，12.35反算非整数失败；
- UINT64_MAX、比例1、偏置INT64_MIN：结果INT64_MAX，必须成功，证明中间宽值可抵消；
- UINT64_MAX、比例1、偏置0：Decode业务值越界；
- INT64_MIN负比例、零附近符号变化、整除/非整除、最小/最大Wire边界；
- 结果先去除尾零后系数可表示的情况，不能提前按未规范系数拒绝；
- 固定容量上界、随机与边界向量、无动态分配、Windows Debug/Release一致性。

以上为验收预期；实际测试、上界推导及证据限制见
[隔离验证报告](windows-msvc-2026-dec042b-arithmetic-spike.md)。Windows Debug/Release各1/1，
P2补测后各78386断言、失败0、普通new/new[]计数增量0；尚无独立高精度Oracle证据。
算术阶段通过不代表完整B能力链通过；只读审查未发现明确算术实现或容量推导错误。

## 6. 补充D：证据与接入矩阵（已确认）

Values 0.4仅配Schema 0.5；Schema 0.5可用旧Values表达旧类型，沿用其原有类型门禁。
DECIMAL64证据采用结构化系数/小数位，并单独记录raw_kind和原始整数，不挤进含糊的字符串元组。
Result字段名称、允许属性及Reader合法组合按第8节C组执行。
保存原始Values用于追溯，但执行比较及确定性指纹使用规范化数学值；原文件Hash可不同而执行指纹相同。
规范化不忽略配置身份、raw类型、错误原因或其他既有确定性输入；不通过全局剥除Hash绕过历史契约。
同代失败Replay可比较相等但当前仍失败；无Codec模式比较未评估。跨代Run比较继续拒绝，Frame比较保留。

| 编号 | 完整B接入验收目标 |
| --- | --- |
| D01 | 参数接受域、约分、0/负分母、比例零、未知属性和旧Schema拒绝 |
| D02 | 类型化DECIMAL64、非法表示、等价表示、规范零及原始整数诊断 |
| D03 | 独立Decode/Encode预期及反算非整数、两侧Wire与业务值越界 |
| D04 | 中间大数抵消、规范化后范围、全部符号与极值 |
| D05 | 无转换字段/常量不变，转换常量与后置类型严格拒绝 |
| D06 | 结构唯一性、容量、SUM8和转换失败次序，最终复核与失败不交付 |
| D07 | 编译冻结、损坏Draft防御、实际资源计费、首次调用无新增分配 |
| D08 | 新版本无转换、旧格式/指纹不变，Values代际矩阵 |
| D09 | 等价表示同执行指纹、不同数学值差异、原Run→Replay→Replay失败链 |
| D10 | 自洽长度/Hash链下非法DECIMAL64、原始类型与元组的精确语义拒绝 |
| D11 | Windows Debug/Release专项/全切片及两类隔离构建 |

## 7. 已确认后的剩余门禁

四组补充方案已确认；固定256位候选及中间上界已有隔离测试、书面推导和只读审查，
不是完整B或生产Core验收结论。
若算术验证不通过，须回到契约讨论，不得静默缩小已确认的参数接受范围。
生产接入A1～D4已确认，实施必须遵循第8节；不代表普通Lab 0.6运行链已实现。
独立高精度Oracle的具体工具亦需单独批准，不因本轮确认而允许引入或执行。
该次审查的P2测试缺口已补齐；三段实施中的首段编译冻结与计费、第二段Core双向转换均已实现
并完成限定Windows验证；首段已审查提交，第二段raw诊断失败调用生命周期补测已完成，详见
Core报告。Lab证据段仅完成A纯格式模块，B证据读写和C运行链仍未开始。
算术验证、完整实现、Stage、Commit、Push分别授权。Linux、Golden、硬件、现场和性能不升级。

## 8. 生产接入四组拍板（A1～D4，CONFIRMED）

2026-09-07用户确认以下16项并授权同步契约。它们细化前述契约；本轮只修改Markdown，
不修改源码/CMake，不运行验证程序，不Stage/Commit/Push。不得将生产目标写为当前能力。

### A组：存储与计费

**A1 私有固定256位算术。** 沿用已审查候选算法和接受范围，整理为Core内部模块，
生产代码不直接引用spikes，不暴露宽整数公共接口，不引入浮点或第三方大整数产品依赖。

**A2 独立冻结转换表。** 每个转换字段关联一个描述，保存原始整数类型、公共小数位和带符号A/B。
字段保存转换索引，不为每个普通字段内嵌两份256位系数；首版不跨字段去重。
Loader、Domain Validator、PlanBuilder分别落实对应校验；Builder重新计算并核对派生描述，
不信任损坏Draft（编译草稿）。

**A3 类型值与原始诊断分离。** EncodeFieldValue增加类型化Decimal64；Decode转换字段输出规范Decimal64。
RawIntegerValue放在Workspace（执行工作区）专用连续槽中，只为转换字段预分配，通过索引关联结果。
原始诊断仅在整次Decode成功时有效，下一次使用该Workspace或销毁时失效；长期保存由调用方复制。
普通字段不新增原始整数副本。值结构允许因Decimal64成员增大，不承诺旧二进制布局不变，
不借本轮把全部值结构改为union。

**A4 缓存与实际计费。** Encode预检反算成功后缓存原始整数，写入复用缓存；最终复核仍重新读取字节
并执行转换，不能只比缓存。分别记录Plan转换描述/索引、Workspace诊断/Encode缓存、调用方值槽大小
及算术固定局部存储。按实际sizeof、容量和对齐计算，不写死Windows字节数，不把局部栈计为Plan堆内存。
运行期不得首次懒分配。

### B组：失败优先级

**B1 旧代不变。** Schema 0.1～0.4保留原字段检查顺序、状态、错误索引、失败输出和历史指纹。

**B2 Schema 0.5 Encode分阶段。** 即使没有转换，也按以下新代顺序执行：

1. Workspace占用、归属、参数描述范围、Pipeline/Message合法性。
2. 按输入顺序检查字段引用、常量覆盖、重复字段，建立索引。
3. 按配置字段顺序检查必填项。
4. 按配置字段顺序检查类型和数值；转换字段依次为类型→Decimal表示→精确反算→Wire范围。
5. 输出指针→容量→地址范围及输入输出重叠。
6. 字段写入→SUM8生成→最终复核。

转换非整数与容量不足并存时先返回转换错误。预检失败bytes_written=0；字段输入阶段失败
required_size沿用清零原则，输出容量阶段报告完整报文长度。

**B3 Decode完整性优先。** 合法调用前提下：结构唯一性→输出槽容量及安全检查→SUM8→
按字段顺序解释/转换→整体交付。SUM8失败不得执行转换；任一字段失败field_count=0，
原始诊断不可读取。保留已确认Message身份及失败字段索引，不保证底层槽位清零。

**B4 错误分类。** 数值原因沿用DECIMAL_SCALE_OUT_OF_RANGE、RAW_NOT_INTEGRAL、
RAW_OUT_OF_RANGE、LOGICAL_OUT_OF_RANGE。增加内部状态INTERNAL_ERROR，专用于合法冻结Plan下
突破算术上界等实现异常；损坏Plan描述仍为INVALID_PLAN，最终复核不一致仍为FINAL_REVIEW_FAILED。
不得把内部故障伪装成用户值不可表示；Decimal输入scale超出0..18属于输入表示非法，映射
INVALID_ARGUMENT并保留DECIMAL_SCALE_OUT_OF_RANGE；该检查不因coefficient为0而省略。
最终重读转换本身发生内部算术故障时仍返回INTERNAL_ERROR，仅实际字节或数学值不一致返回
FINAL_REVIEW_FAILED。使用测试专用故障注入验证，不扩展正常配置路径。

### C组：证据字段与兼容矩阵

**C1 严格结构化转换结果。** 转换字段条目固定为：

```json
{
  "id": "temperature",
  "kind": "DECIMAL64",
  "decimal64": {"coefficient": "123", "scale": 1},
  "raw_kind": "INT64",
  "raw_value": "523"
}
```

coefficient与raw_value为对应类型的规范整数文本；输出Decimal必须规范化，Values输入可为等价表示。
DECIMAL64条目不得带旧logical_value或enum_known。非转换字段维持原条目形状。
Reader按kind检查允许属性，拒绝缺失、混搭、未知属性和错误原始类型。

**C2 独立转换原因。** 新代Codec结果增加conversion_error：无转换失败为null，否则为固定原因字符串。
继续使用既有失败字段身份；内部错误和最终复核失败不借用数值原因。
失败时字段结果列表为空，不输出部分转换结果。

**C3 版本组合。**

| 输入/记录 | 已确认规则 |
| --- | --- |
| Schema 0.1～0.4 | 原格式、行为、指纹不变 |
| Schema 0.5 | Result/Record/Event统一0.6，无转换亦如此 |
| Values 0.4 | 仅Schema 0.5 |
| 旧Values + Schema 0.5 | 仅原来支持的类型，不可输入DECIMAL64 |
| UDP RX Metadata | 保持0.2 |

Reader联合操作类型、Replay模式及Schema检查合法组合，不能只检查各文件版本分别可识别。

**C4 追溯与执行比较分离。** 原始Values原样保存并校验Hash，执行指纹使用规范化Decimal数学值。
其他确定性条件相同时等价输入指纹一致；配置身份、raw类型、失败原因等仍参与比较。
不对旧代全局剥除Hash或修改历史指纹。Reader验证结构和关联一致性；只有重执行Codec的Replay
证明当前转换结果。NO_CODEC_REEXECUTION仅验证证据，不产生转换成功结论。

### D组：接入验收

**D1 三段实施。** 依次为Schema/IR（中间表示）/编译冻结与计费，Core双向转换/失败顺序/最终复核，
Lab新代证据/Reader/Replay/兼容性。各段独立复核，不合并Runtime、C ABI（C语言二进制接口）、
GUI、网络监听或其他数值类型。

**D2 组合错误与失败事务。** 在D01～D11矩阵上补充：非整数与容量不足同时发生；SUM8错误与转换越界
同时发生；多个字段同时错误及输入顺序翻转；中途失败不交付字段和原始诊断；损坏Draft、无转换Plan、
混合字段Plan计费；宽数抵消向量从Schema加载至Lab Replay全链验证。

**D3 精确拒绝位置。** 使用长度、Hash、清单关联自洽的非法证据，验证非规范输出Decimal、错误raw类型、
版本混搭、未知属性确实被语义门禁拒绝。覆盖原Run→Replay→再Replay以及当前执行失败但历史比较相等。

**D4 Windows与Oracle门禁。** 执行专项、Lab、全切片Debug/Release及Product-only、
Lab-on/Testing-off两类隔离构建；新转换路径检查首次调用无动态分配。
独立高精度Oracle具体工具另行批准；在未完成前保持无独立高精度交叉验证限制，
不宣称任意合法输入均已全面验证。本轮确认不授权运行Oracle或生产实现。

## 9. 首段实施临时隔离门（已确认）

1. 新增Schema 0.5编译能力开关，缺省为关闭。缺省构建中的Loader与PlanBuilder继续拒绝
   所有Schema 0.5配置，包括没有`conversion`的0.5配置，保持当前生产入口边界。
2. 首段仅专用Loader-only构建可显式开启该开关；第二段起专用Loader+Core构建也可开启。
   开关通过相关CMake目标的编译定义传递，
   不使用目录级或全局宏；开启时允许Schema 0.5完成结构加载、领域校验、编译冻结、
   PlanBuilder复核、快照及资源计费验证。
3. 首段曾禁止开关与Core或Protocol Lab共存；第二段Core语义实现后仅继续禁止Protocol Lab。
   相关开关组合纳入自动化或独立配置验证。
4. 首段生成的Plan只用于编译期验证；第二段专用Core构建现在可以执行Schema 0.5 Plan。
   Protocol Lab混合链接仍不受支持；本段不实现Schema 0.6、Values 0.4或证据格式0.6。
5. Core执行入口按构建能力显式拒绝未知Schema代际；0.5只有在专用开关开启时可执行。
   不同编译选项产生的C++二进制不得混合链接，现阶段不承诺跨该开关的ABI兼容。

实施结果：`PAE_ENABLE_SCHEMA_V05_COMPILER`已按上述规则落地。总控审查后补齐诊断定位和
Builder防御证据；专用Loader构建的Windows Debug/Release合同Runner各80/80、Schema正反对照
各7/7，缺省关闭路径各49/49。先前13/13正确归类为Compiler/Lab共存证据；新建分离目录、
显式注册独立Core测试的完整Loader/Codec/Lab矩阵Debug/Release各25/25。两类隔离构建均通过且
注册0测试。该结论只覆盖首段，详见首段验证报告。

过程边界：初次实施曾未经具体工具授权一次性使用宿主`System.Numerics.BigInteger`核算两组固定
测试常数，磁盘无可追溯命令日志；本轮未再次运行，不追认为已授权Oracle或完整交叉验证证据。

## 10. Lab第三段六项补充拍板（已确认，A已交付，B已限定实现及验证，C未实施）

2026-09-07用户确认以下六项，随后仅授权Markdown同步。它们细化第8节C1～C4，
当时确认不代表实施。当前A已交付Values 0.4及Result 0.6纯格式能力，B已获单独授权并完成
Evidence 0.6隔离读写及限定Windows验证，见第14节；C运行链仍未实施，不改变旧代文件和指纹。

### 10.1 转换错误与诊断ID

`conversion_error`仅允许`DECIMAL_SCALE_OUT_OF_RANGE`、`RAW_NOT_INTEGRAL`、
`RAW_OUT_OF_RANGE`、`LOGICAL_OUT_OF_RANGE`或JSON `null`。沿用`PAE_LAB_CODEC_<STATUS>`
诊断ID，不为每种转换原因再建立一套ID。内部故障、最终复核失败、未执行Codec时原因均为null。
Reader必须联合检查状态与原因：非法scale对应INVALID_ARGUMENT，其他三种数值原因对应
VALUE_NOT_REPRESENTABLE；不得只校验原因字符串属于枚举集合。沿用Core实际失败，不猜测原因。

### 10.2 Result 0.6失败字段身份

| 字段 | 类型和空值规则 |
| --- | --- |
| `failed_field_id` | 字符串或null；仅能在已确定Message中定位字段时填写 |
| `failed_field_index` | 非负整数或null；配置字段索引 |
| `failed_value_index` | 非负整数或null；仅适用于已定位的Encode输入项 |

成功或相应身份无法确定时使用null，不输出内部无效索引哨兵值。身份来自实际失败结果，
不根据输入猜测。Reader校验同一已确定Message下ID/索引关联；失败字段列表仍为空。

### 10.3 Result、Record、Event的信息归属

Result负责转换结果和失败细节。Record通过既有文件长度、Hash及关联检查绑定Result，
不重复上述新增转换字段或失败身份。Event继续表达阶段事件，不新增逐字段转换明细，
使用0.6并遵守合法版本组合；RX Metadata仍为0.2。
完整同代三件套为`result_summary_v0.6.json`、`run_record_v0.6.json`、`events_v0.6.jsonl`；
不得忽略未知文件或借用旧文件名绕过原子读写门禁。

### 10.4 指纹0.6规范化规则

使用独立`pae.lab.fingerprint/0.6`域。保留配置身份、方向、消息、操作、报文和既有确定性执行信息；
Decimal使用规范化coefficient/scale，并绑定raw类型及原始值。转换原因、失败字段身份参与指纹，
即使失败没有输出字段也不能丢失。原始Values文件原样保存并校验Hash，不把该文件Hash作为
新代执行等价比较依据；不得改变旧代Hash或指纹算法。时间、历史网络端点及超时等非确定性信息
不混入执行指纹。
Canonical payload（规范化指纹输入）的固定顺序、null及字符串编码已追加确认，见第12节；
第12～13节的A纯格式实现已随`8d4c7c4`提交并Push；不表示B证据读写或C运行链完成。

### 10.5 等价输入的Run Compare

其余确定性条件相同、两个Bundle分别通过完整性检查、Decimal数学执行结果相同时，
即使原始Values文本及Hash不同，Run Compare返回EQUAL。EQUAL表示执行等价而非原文件相同。
失败结果可比较相等，但当前执行仍失败；不同配置身份不因输出相同就成为同一执行。
跨代Run Compare继续拒绝，原始Frame比较维持既有规则。NO_CODEC_REEXECUTION不产生转换成功结论。

### 10.6 差异分类

不新增顶层CONVERSION_ERROR类别。转换原因或失败字段身份不同归入既有STABLE_DIAGNOSTIC；
Decimal数学值、raw类型或raw值不同归入既有字段结果差异类别，明细指出具体字段。
类别顺序须确定，旧代类别及行为不变。

## 11. Lab第三段实施检查点（顺序已确认，实施另行授权）

| 检查点 | 范围 | 放行条件 |
| --- | --- | --- |
| A：内存模型与纯格式 | Values 0.4、Decimal结果结构、转换错误/失败身份、规范化指纹及隔离单元测试 | 普通Lab不接通Schema 0.5，不发布0.6 Bundle |
| B：证据读写 | 完整0.6三件套、严格Reader、跨文件绑定、长度/Hash自洽篡改负例 | 读写闭环，不向普通CLI发布半成品证据 |
| C：运行链闭环 | Core结果复制、结构优先Inspect、Replay/Compare、版本矩阵及Windows回归 | 全链验证后才开放专用Schema 0.5 Lab构建 |

A使用隔离测试验证纯格式；实际Core输出与raw绑定在C接入。Core Encode成功不发布raw诊断，
Lab应从其单独Decode输出报文的成功结果及同一Workspace复制Decimal和raw；失败不交付部分字段。
Inspect的Schema 0.5走结构唯一性优先路径，零/多候选不转换，唯一候选才Decode。
保持Schema 0.5默认关闭；A/B不得提前移除现有Schema 0.5与Protocol Lab组合拒绝。
隔离测试接入方式及指纹精确编码按第12～13节，不通过ABI不兼容混链测试绕过门禁。

验收覆盖等价输入不同原文件Hash、不同数学值、无转换0.5、旧Values类型限制、宽数抵消、
成功及失败Run→Replay→再Replay、自洽非法证据的精确拒绝与旧代固定夹具不变。
最终Windows专项/全切片Debug及Release、两类隔离构建及必要既有Loopback回归按实施授权执行。
不新增网络功能、不展开Runtime/Session、流式切帧、GUI或新校验算法；Oracle、Linux、
真实Golden、硬件、现场及性能证据不升级。上述实施顺序拍板时仅同步文档，未运行构建或测试；
后续A阶段实际实施及验证见第13节，不以该历史边界替代当前状态。

## 12. 指纹0.6精确编码（已确认、A阶段已限定实现）

指纹表示确定性协议执行结果，不表示原始请求文件或全部运行事实相同。不同输入若失败于同一
字段及原因、且其他确定性结果相同，可以同指纹；原始Values与其Hash保留追溯职责。
NO_CODEC_REEXECUTION不证明转换成功。不新增独立输入请求指纹。

### 12.1 基础字节语法

```text
null       N;
string     S<UTF-8字节数>:<原始UTF-8字节>
integer    I<规范十进制文本字节数>:<规范十进制文本>
boolean    B0; 或 B1;
array      A<元素数量>:<各元素依次编码>
```

长度及数量为ASCII十进制，无正号、无前导零，零写0。字符串长度按UTF-8字节计算，
不做Unicode归一化、去空格或换行替换。整数无小数、指数、-0、千分位或本地化格式。
null与空串分别为`N;`和`S0:`。完整编码不添加BOM、平台换行或结束字符；SHA-256结果
沿用大写十六进制。字符串`a|b`编码为`S3:a|b`，内容中的分隔字符没有语法作用。

### 12.2 顶层固定20项数组

| 顺序 | 内容 | 编码类型 |
| --- | --- | --- |
| 1 | 固定域pae.lab.fingerprint/0.6 | string |
| 2 | Schema版本0.5 | string |
| 3 | 原操作operation_kind | string |
| 4～5 | replay_mode、replay_subject | string |
| 6～7 | 当前执行状态、当前执行稳定诊断ID | string；无诊断为null |
| 8 | 配置SHA-256 | string；尚无配置身份为null |
| 9～12 | protocol、pipeline、message、direction身份 | string或null |
| 13～15 | 主报文、TX报文、RX报文 | 大写无分隔HEX string或null |
| 16 | conversion_error | string或null |
| 17～19 | failed_field_id、failed_field_index、failed_value_index | string或null、integer或null、integer或null |
| 20 | 配置字段顺序的结果 | array |

无适用报文为null，实际存在的零长度报文为空字符串；未确定身份为null，不把内部空串当作有效身份。
Replay保持被重执行的原操作类型，不用CLI命令名replay替代。离线采用当前Codec执行状态/诊断，
UDP采用既有当前执行口径，不混入历史Transport终态。合法组合需校验，不凭空补造缺失身份。
退出码、比较结论、Bundle路径、诊断长文本、时间、端点、超时、事件时间偏移及原始Values Hash
不进入编码。无Codec时保留未执行语义，不制造成功状态。

### 12.3 字段数组

Decimal字段是固定6项数组`[id, kind, coefficient, scale, raw_kind, raw_value]`：
id/kind/raw_kind为string，kind固定DECIMAL64；coefficient/scale/raw_value为integer，
raw_value按raw_kind精确解释。先检查表示合法再规范化，非法scale不能因零值而通过。
其他类型保留既有五项语义`[id, kind, raw_value, logical_value, enum_known]`，前四项为string，
最后为boolean；只在0.6改用本节编码，不改旧代字节。字段按配置顺序，不按Encode输入顺序排列。
失败字段数组为空，顶层失败原因及身份仍参与指纹。FieldsCanonical与指纹复用同一0.6字段编码。

## 13. A阶段隔离接入（已限定实现、复核、提交并Push）

交付记录：A实现`8d4c7c4`、入口文档`0c4bc48`、yyjson依赖整理`1c0617c`均已提交并Push。
本节历史测试批次保留，不作为本次Markdown同步重跑证据。

新增内部纯格式模块，A阶段只由测试目标编译，B/C复用同一源码。范围为Values 0.4解析、
Decimal及失败模型、纯结果序列化、规范化编码和指纹；不包含配置编译、Core执行、Bundle文件操作
或UDP。不大范围抽拆旧Lab，不迁移旧Values解析及指纹函数。

计划开关`PAE_BUILD_LAB_V06_FORMAT_TESTS`默认OFF；必须同时开启PAE_BUILD_TESTING，否则
CMake配置失败。不要求Schema 0.5编译开关或完整Lab，不链接Core执行库或Winsock。
复用现有JSON解析依赖和SHA-256，不新增第三方依赖；必要时仅最小整理CMake依赖，
不能为了获得依赖而加载完整Compiler/Core。原Schema 0.5+Lab拒绝条件保持不变。
A可生成内存格式样本，但不发布0.6 Bundle、不让普通CLI接受Values 0.4。

2026-09-07实施补记：A已按本节落地为默认关闭的内部纯格式模块和隔离测试目标。实现覆盖
Values 0.4严格解析、Decimal规范化结果、失败原因/身份模型、Result 0.6内存序列化样本、
FieldsCanonical及固定20项指纹编码；Debug/Release隔离测试各1/1通过。普通Lab、Evidence
Writer/Reader、Core结果复制、Inspect、Replay/Compare和UDP均未接入，Schema 0.5+普通Lab组合
继续配置失败关闭。执行证据及限制见
[A阶段Windows验证报告](windows-msvc-2026-dec042b-lab-v06-format-stage-a.md)。

验收计划包括：null/空串、UTF-8与特殊字符无歧义、数量和整数极值、独立固定预期编码字节；
Decimal等价/差异/规范零/非法表示、raw类型和值变化、字段顺序；状态原因组合、失败身份、
内部错误无数值原因及无部分字段；默认无新增目标、Testing关闭拒绝、格式目标独立构建、
原Lab组合门禁保留。Windows Debug/Release及旧代必要回归须在后续实施授权下执行，
不执行网络或Oracle。A初次实现及本轮P2纠错均已取得隔离Debug/Release各1/1结果；初次实现时
旧Lab离线回归各12/12，本轮未重复无关旧矩阵。P2纠错补充模式/主体/当前状态及失败身份的
纯模型不变量，仍不接入B/C。

### 13.1 A阶段P2纠错（2026-09-08，已总控复核）

- `NO_CODEC_REEXECUTION`仅允许`RX`或`RX_INCOMPLETE`，当前执行必须`NOT_EVALUATED`，且无当前
  诊断、转换原因、失败身份或字段结果；`ENCODE_TX/TX`、`DECODE_RX/RX`及默认`NONE/NONE`
  分别检查主体和执行状态，未知或矛盾组合失败关闭。
- 已知失败字段身份要求非空Message、非空字段ID，并与字段索引成对；`failed_value_index`仅用于
  Encode路径。未知字段等输入错误仍可只保留输入索引，不强迫伪造字段身份。A无Plan依赖，
  ID与配置索引的真实对应留待B/C联合校验。

## 14. B阶段四组补充（已限定实现、复核、提交并Push）

收口记录：实现`70cf4ff`、入口文档`761ff7f`已提交并Push。下述测试和纠错保留原批次记录，
不作为本轮C契约同步重跑证据。

用户先确认本节四组方案并同步契约，后续已单独授权B阶段实现。接管基线为`1c0617c`。
当前已实现默认关闭的Evidence 0.6隔离读写目标；未接普通CLI、Core、Replay/Compare或网络。
清理候选继续保留，没有删除授权。第10～13节的格式及指纹决策未重新打开。

### 14.1 构建隔离与交付边界

- 新增默认关闭、要求开启Testing的B证据专项测试入口；具体目标名在实施时按现有风格确定。
- 允许文件读写，复用A格式模块、本地yyjson和SHA-256；不链接Core、Config Compiler、Winsock，
  不接入普通CLI（命令行工具）。不得为复用文件操作而链接完整旧Lab目标。
- 测试目录可生成并读取完整0.6 Bundle（证据包），但普通Lab不开放新代格式。
- Schema 0.5与普通Lab组合拒绝保持。B完成仅证明证据保存/读取，不证明协议执行正确。

### 14.2 模块复用与写入事务

- 只抽取必要文件操作和通用文件描述，不复制整套旧证据实现，不全面重构Lab。
- 0.6格式规则独立，旧代序列化、指纹和读取行为保持不变。
- 沿用`.inprogress`及正式发布流程；必要文件全部写入、关闭、重读复核及关联检查成功后，
  才发布完整Bundle。任一步失败不得留下正式成功的证据包，不覆盖已有正式Bundle。
- 原配置、Values和报文字节原样保存，不因Decimal规范化改写原文件。
- 不增加断电持久性承诺；文件操作故障验证不等于`fsync/FlushFileBuffers`保证。

### 14.3 Reader校验顺序与语义边界

Reader（读取器）依次完成：

1. 目录/文件边界：拒绝路径越界、重复记录、缺失文件、非法版本组合及既有规则禁止的额外文件。
2. 内容完整性：校验长度、Hash、Result/Record/Event同代关系及适用的RX Metadata 0.2。
3. 格式/内部语义：严格属性、Decimal表示、状态与转换原因、NO_CODEC、失败身份及事件关联。
4. 指纹复算：复用A规范编码计算，不信任文件声明。
5. 整体交付：全部检查成功才返回完整读取结果，失败不交付部分成功数据。

B检查字段ID非空、索引成对等模型内部关系；同一冻结Plan中的实际ID/索引对应在C接入时验证。
这细化第10.2节的最终Reader要求，不将未完成的Plan语义校验表述为已通过。B不调用Decode，
不产生Replay/Compare成功结论。Hash不是签名认证：能检测不一致，不能证明自洽重写的证据真实。

### 14.4 验收矩阵与回归范围

| 类别 | 必须验证 |
| --- | --- |
| 正常往返 | 合成成功、失败、NO_CODEC结果写入后完整读取 |
| 表示差异 | 等价Decimal原文Hash不同而规范指纹相同；无报文与实际零长度报文不同 |
| 写入故障 | 创建、写入/关闭、重读、最终重命名失败；不覆盖已有正式Bundle |
| 文件关系 | 缺失、重复、混代、路径越界、长度或Hash错误 |
| 自洽非法证据 | 同步长度及所有关联Hash后，精确拒绝非法属性、状态、身份等语义 |
| 兼容隔离 | 旧代行为不变，普通Lab不提前接受0.6，产品目标不引入新依赖 |

授权实施后，Windows Debug/Release执行B专项、A专项及共享改动影响的旧代离线回归。
独立固定预期与精确拒绝诊断不可仅由Writer生成，避免Writer/Reader同错互证。不执行网络收发，
不升级Oracle、Linux、Golden、硬件或现场证据。以上是验收计划，不是已执行结果。

### 14.5 B阶段实施和验证记录（2026-09-08）

- 新增`PAE_BUILD_LAB_V06_EVIDENCE_TESTS`，默认关闭且要求`PAE_BUILD_TESTING=ON`；目标仅编译
  `v06_evidence`、A阶段`v06_format`、SHA-256和本地yyjson，不链接Compiler、Core、Winsock或CLI。
- Writer保存原配置、可选原Values、主/TX/RX字节及适用的RX Metadata 0.2；每个文件经临时写入、
  关闭、重读、长度/内容/Hash复核后发布，在完整Reader预检成功后才把`.inprogress`目录最终改名。
  已存在正式目录或中间目录均拒绝覆盖；未增加断电持久性承诺。
- Reader严格校验唯一0.6文件集合、路径与符号链接边界、排序清单、Record长度/Hash、Result/Event/
  RX Metadata关联、Result内部模型以及A阶段确定性指纹复算；失败时不交付部分`StoredBundle`。
  B只检查纯模型身份，不声称已验证冻结Plan中的字段ID/索引对应。
- 隔离测试覆盖成功、失败、NO_CODEC往返，Decimal原文Hash差异/规范指纹相等，无Frame/零长度Frame，
  创建、写闭、重读、最终改名故障，不覆盖正式目录，缺失/重复/混代/越界路径/Hash，以及同步
  Record长度和全部清单Hash后的Result/Event未知属性语义拒绝。Debug/Release A+B各3/3通过；旧Lab
  明确排除UDP后的离线回归各13/13通过。详细命令与边界见B阶段Windows报告。

### 14.6 B阶段Reader P2纠错（2026-09-08，已复核并随70cf4ff交付）

- 独立动态复现Reader失败后残留已读配置、Frame、Result或指纹；现改为局部候选完成全部校验，
  成功后一次性交付。早期、中途、晚期以及同一输出对象先成功后失败均检查全部可观察字段为空。
- 独立动态复现清单、负载和中间目录符号链接在拒绝前发生内容读取；现将完整目录对象及Windows
  重解析点检查前移至首个内容读取前，保留后续清单文件集合、长度、Hash和语义验证。
- Windows A+B隔离Debug/Release各3/3通过。本轮未改格式、指纹、旧代、普通CLI、Core或CMake，
  未运行网络。读前检查不承诺抵抗检查与打开之间的并发路径替换；目录联接由Windows重解析属性
  规则覆盖，本轮动态样例为文件及目录符号链接。

## 15. C阶段四组十二项决策（CONFIRMED / C1 IMPLEMENTED）

用户已确认以下12项。随后已单独授权C1实施和Windows验证；接管代码基线`761ff7f`。
C1实际状态见16.8；C2/C3仍未实施，第17节事件细节仍待C2前冻结。

### 15.1 分段与开放边界

1. C1执行映射、C2证据复现、C3入口与回归依次实施，各段独立复核和授权。
2. C1/C2只提供隔离测试入口；Schema 0.5与普通Lab组合拒绝继续保留。C3全链通过后才开放
   显式启用Schema 0.5的专用Lab构建，默认开关仍关闭。
3. 主线仅离线；不扩展UDP新代执行、持续监听、Qt UI、Runtime调度或其他协议字段。
   旧UDP实现不变，C3是否执行既有Loopback回归须在验证授权中明确。

### 15.2 执行结果与生命周期

4. 按Schema分派：0.1～0.4保留旧格式、错误顺序与指纹；0.5统一0.6结果，包括无转换配置。
   Values 0.4仅用于0.5；旧Values与0.5沿用第8节C3兼容规则。不复制整套Lab或全面重构旧链。
5. Lab在Workspace失效前复制结果为自有数据；失败无部分字段，原因和身份来自实际结果，
   不猜测、不解析错误文字，无法确定的身份为null。
6. 全局Inspect先汇总结构身份：零候选未知、多候选歧义，均不Decode；唯一候选才Decode一次。
   SUM8和数值转换的失败优先级由Core执行，不用校验或转换结果筛选消息。

### 15.3 绑定与复现

7. C新增Plan关联校验层，B不引入Compiler/Core依赖。检查记录配置对应的Pipeline、Message、
   方向、字段ID/索引/类型/转换属性、结果顺序和Encode输入索引范围。身份合法不证明数值正确。
8. ENCODE_TX按记录Values重新Encode；DECODE_RX按明确Pipeline和报文重新Decode；
   NO_CODEC_REEXECUTION只检查证据，不执行Codec、不产生转换通过结论。不猜Pipeline或静默降级模式。
9. 等价Decimal可执行等价，原文Hash仍保留；失败比较相等不等于成功；跨代Run Compare拒绝。
   B合成Bundle不能冒充Core执行证据。C2前冻结合成/真实事件接受矩阵；若需改变0.6文件契约，
   先报告拍板，不修改历史Bundle。

### 15.4 验收与收口

10. 覆盖等价Decimal、负比例、宽数抵消/边界、SUM8与转换错误并存、非整数与容量不足并存、
    中途失败整体交付、自洽非法身份/类型/版本，以及Run→Replay→再Replay、失败但历史比较相等。
11. C1专项及受影响Core、C2追加A/B与复现、C3新旧Lab/Core及隔离矩阵分层验证；Windows
    Debug/Release实际注册清单为准。Oracle、Linux、硬件和现场另行授权，不累加历史测试数。
12. 关闭条件为专用0.5 Lab离线转换、记录、读取、复现及比较闭环；不等于产品生产化。
    公共接口、Session、切帧、CRC、Qt和生产接入均后置；Commit/Push另行授权。

## 16. C1接口映射与验收细化（限定实现完成，待总控复核）

用户同意准备失败、桥接校验职责、Encode展示复核三项收口建议；本节已据现有Core和A模型作
静态一致性复核。C1采用内部分层结果，不强迫所有分支生成Result 0.6；完整证据状态在C2前冻结。
本记录不是运行验证或源码实施授权。

### 16.1 模块及隔离入口

建议新增内部执行桥接模块，依赖Config Compiler、ProtocolPlan、Core与A格式，不依赖B文件IO、
普通Lab整库或Winsock；不把这些依赖反向加入A/B。不新建稳定公共API。
测试入口建议为默认关闭的`PAE_BUILD_LAB_V06_EXECUTION_TESTS`，要求Testing、Loader及Schema 0.5
能力开关开启；具体目标名实施前核对既有命名。不修改普通Lab组合拒绝条件。
文件内容由测试调用方提供，C1不负责读写Bundle、CLI参数、Replay或Compare。

### 16.2 数据映射

| 来源 | C1桥接要求 | 输出或失败边界 |
| --- | --- | --- |
| 原配置文本 | 编译为PlanOwner，并对原字节计算配置Hash | 不用格式化配置替代原文；编译失败不调用Codec |
| Values版本与Schema | 先执行兼容矩阵，再调用对应解析器 | 0.4仅0.5；不得让旧入口默默接受DECIMAL64 |
| ParsedValues的字段序列 | 保持作者输入顺序，解析Plan作用域引用 | 不预先排序/去重，以免改变Core输入索引和错误优先级 |
| UINT64/INT64/BOOL/BYTES/ENUM | 映射相应LogicalValueKind；Bytes保持自有存储，Enum绑定同Plan | 不经浮点，不统一转字符串后再计算 |
| DECIMAL64 | coefficient/scale逐成员复制为Core类型 | 禁止reinterpret_cast依赖两个结构布局相同 |
| Decode成功槽 | 按冻结字段顺序复制为FieldResult | Bytes、ID和Enum文本均归Lab所有 |
| 转换字段raw | 从成功Workspace诊断读取并核对FieldRef、raw kind | 不假定raw诊断序号就是配置字段索引，不反算伪造raw |
| Codec失败 | status及ConversionError枚举显式映射 | NONE为null；字段数组为空，不读取失败raw |
| failed_field_index | 检查已知Message和索引范围后取真实ID | 无效哨兵转null；越界内部关联不能猜测修补 |
| failed_value_index | 仅Encode且Core确实定位输入时保存 | 未知字段的桥接输入错误保持来源区分，不伪装为Core诊断 |

输入在Values严格解析阶段被拒绝时，记录为准备/输入错误，不能声称发生了Core执行。
非法Decimal表示的Core映射测试可直接用类型模型测试；不能绕过严格Values解析器并声称端到端输入合法。
C1状态对照见16.5：准备错误和Lab复核错误保存在独立内部结果中，不强行填入0.6执行结果。
本阶段无CLI，不为内部结果新增退出码；需要生成的0.6结果仅限16.5列出的实际执行分支。

### 16.3 Encode成功结果与生命周期

现有旧Lab在Encode成功后用独立Workspace再次Decode实际输出，生成展示字段。建议C1沿用此编排：
Core内部最终复核仍保留；Lab额外Decode用于生成自有字段/raw结果，不把输入Values当实际输出。
该额外Decode须在测试调用计数中明确区分，不与“Inspect唯一候选只Decode一次”混淆。
若额外Decode失败或Message不一致，禁止交付成功结果；INTERNAL_ERROR不得降格为普通比较不等。
额外复核失败按16.5独立保存阶段和原因，不修改原Encode状态，也不修改Core算法来迎合展示需求。

Plan必须晚于Workspace及全部FieldRef销毁；调用期间Values的Bytes存储不得移动失效。
复制成功结果和raw后才允许复用Workspace；复用、失败调用或销毁不得改变已返回的Lab结果。
结果映射采用局部候选，映射缺失或内部关联错误时不交付部分字段。
Lab自有容器允许分配，不将Core已有无新增分配门禁扩大为Lab全链零分配或正式性能结论。

### 16.4 C1最低验收清单（计划，非已执行）

- 经真实配置编译完成0.5 Decode/Encode映射：转换/非转换混合字段、无转换0.5、INT64/UINT64边界。
- 等价Decimal规范值、负比例、宽数抵消使用独立固定报文与预期，不仅靠Encode/Decode互证。
- 输入顺序翻转、重复/缺失/错误类型，精确检查失败身份及输入索引。
- 全局零候选/跨Pipeline歧义为0次Decode，唯一Inspect为1次；SUM8失败无转换执行。
- 非整数与容量不足并存，类型化非法scale，内部故障和最终复核不一致保持不同状态。
- 成功后Workspace复用、失败后复用及销毁，Lab自有结果不变；失败不保留旧字段。
- Encode展示的额外Decode及失败分支可观察；不产生虚假OK结果。
- 新旧版本拒绝矩阵、A纯格式回归、受影响Core Debug/Release；普通Lab门禁保留。

### 16.5 三层结果与交付对照

内部桥接结果区分准备、主Codec调用、结果物化/额外复核三个阶段；内部枚举及成员名可在授权实施时
按项目风格确定，以下语义必须固定。保存实际Core结果时同时复制可定位身份，不能保存悬空引用。

| 分支 | 内部事实 | 可选Result 0.6 | 交付及诊断 |
| --- | --- | --- | --- |
| 配置/Values解析、版本、绑定或引用准备失败 | 准备失败；主Codec未调用 | 无 | 保留准备诊断及可确定的作者输入索引，不填Core原因；无成功字段/输出 |
| Inspect结构查询零候选或歧义 | 保存真实结构查询状态；Decode为0次 | C1不强制生成，留待C2统一编排语义 | 不把查询状态冒充Decode返回；不交付字段 |
| 主Codec返回非OK | 保存原DecodeResult/EncodeResult及确定身份 | 有；operation_status=CODEC_ERROR，exit_code=5，current_execution_status为真实Core状态 | PAE_LAB_CODEC_<STATUS>；转换原因严格按Core枚举，字段为空 |
| 主Codec成功且物化/复核成功 | 主调用及适用的复核均真实成功 | 有；operation_status=OK，exit_code=0，current_execution_status=OK | 字段按配置顺序自有复制；成功Encode仅此时交付完整输出 |
| 主Codec成功但Lab物化/复核失败 | 原主Codec状态仍为OK；额外Decode状态单独保存（若有） | 无，不发布部分或虚假成功Result | 归属Lab复核失败；内部故障与普通不一致分开，清空交付字段和Encode输出 |

有Result的Encode使用ENCODE_TX/TX，Decode使用DECODE_RX/RX；非OK结果的current诊断及外层
diagnostic.id均对应真实Codec状态。NONE转换原因转null，非法scale及三个数值原因沿用第8/10节。
输入Frame在Decode失败时仍可作为原始输入保留，不等于成功输出；Encode失败不交付半成品字节。
主调用失败不得触发展示Decode；展示Decode的INTERNAL_ERROR不得变成不一致，也不得回写为原Encode
返回的INTERNAL_ERROR。主Codec自身FINAL_REVIEW_FAILED与Lab额外复核失败始终区分来源。
Lab物化失败中的raw缺失/错关联属于内部一致性故障，Message不一致单列；无需新增Core状态或改A模型。

准备层索引仅属于内部准备诊断，不使用Result.failed_value_index绕过A的NONE模式约束。
因此不改变A允许Encode路径保存输入索引的已有接受域，也不声称C1覆盖了完整0.6失败证据。
C2前必须统一准备/结构查询/Lab复核失败的事件、Result及CLI退出码映射；不得默认降级NO_CODEC。

### 16.6 桥接与Core校验职责

准备顺序为：配置编译→Values严格解析及版本兼容→Pipeline/Message名称绑定→按作者顺序解析
必要Field/Enum引用。此层遇到无法解析引用时返回准备失败，主Codec调用数为0。
它不与Core内部错误共享一个全局优先级，后部未知引用可能先于前部可表示的类型错误被报告。

能够构造的输入按作者kind建立LogicalValueKind，不因其与字段类型不符而提前返回；已知字段的常量
覆盖、重复、必填、类型和数值判定交给Core。不得照搬旧EncodeValues中的常量/类型提前拒绝。
Enum名称只有在目标具有相应Enum表时才解析；其他类型错配应使用类型不匹配输入交给Core，
不解引用不存在的Enum表、不伪造有效Enum引用。不新增“去重后调用Core”或按配置重排输入逻辑。

Core仍负责已确认的0.5顺序：输入引用/常量/重复→必填→按字段顺序类型/数值→输出容量→写入/复核。
旧0.1～0.4路径不改；字符串引用解析所需的最小工具若需抽取，不得因此链接普通Lab或Winsock。

### 16.7 收口新增断言及验证归属

- 配置、Values版本/语法、未知Field/Enum准备失败：内部准备诊断准确，Codec为0次，可选Result为空。
- 前项类型错、后项重复且引用均可解析：由Core报告重复；缺失必填与类型错误组合保持Core顺序。
- 前项类型错、后项未知引用：明确为准备失败，不标为Core优先级回归；常量覆盖不在桥接层截断。
- 主Encode失败为0次展示Decode；正常成功为1次Encode加1次展示Decode；唯一Inspect为1次Decode。
- 注入展示Decode失败、Message不一致、raw关联异常：原Encode仍OK，Lab结果失败，无成功Result/字节。
- 注入展示内部故障与普通不一致：内部失败原因不同，均不产生转换成功或比较结论。
- 只有实际执行结果的0.6映射调用A校验并检查状态/诊断/索引/null；准备分支不调用A序列化伪造结果。
- 非整数与容量不足的Core组合测试沿用Core测试入口；若C1自有输出始终按Plan分配完整容量，
  不声称该桥接入口动态覆盖容量不足，不为测试擅增生产参数。

C1实施授权包含上述内部结果与专项测试、CMake隔离入口及相关文档；仍不包含B文件读写、C2/C3、
普通CLI或A模型接受域变更。实际落实及验证见16.8。

### 16.8 C1实施与Windows验证记录（2026-09-08）

- 新增默认关闭的`PAE_BUILD_LAB_V06_EXECUTION_TESTS`隔离入口，仅在Testing、Loader、Schema 0.5
  Compiler和Complete Record Core同时启用时允许配置。目标只链接Compiler、Plan、带内部观察器的
  Core、A阶段0.6格式、SHA-256及既有yyjson；不链接B Evidence、普通Lab、Winsock或网络实现。
- C1桥接从原配置文本编译并持有Plan和两个独立Workspace。Values 0.4继续由A阶段严格解析器负责；
  C1另以隔离版本分派严格解析0.1～0.3，且只接受各代原有类型，不通过改写版本借0.4语法解析。
  随后绑定Pipeline、Message、Field和Enum。无法绑定的引用是准备失败且Codec调用为0；可构造输入
  的常量、重复、必填、类型和数值顺序仍由Core决定。
- Inspect逐Pipeline调用Core内部结构匹配，零/多候选不Decode、不生成0.6 Result；唯一候选只Decode
  一次。Encode成功后只用第二Workspace额外Decode一次实际输出；复核或物化失败保留主Encode为OK，
  但不交付Result或Frame。主Encode失败不触发展示Decode。
- 成功字段按冻结顺序复制；Decimal采用规范值，转换raw通过`FieldRef`与实际Plan/Message/Field关联，
  不按诊断序号猜字段。Result及Frame完全自有，Workspace复用和桥接销毁后仍有效。
- 公开合成测试覆盖转换/非转换混合、新代无转换、全部继承类型、等价Decimal、负比例与宽数抵消结果、
  SUM8优先级、逻辑/反算范围、非整数、非法scale、输入顺序、准备/Core优先级、零/多/单候选、四类
  展示/物化失败和恢复复用。Core容量不足及首次调用无分配仍由受影响Core测试覆盖；C1自身始终按
  Plan分配完整容量，不冒充动态覆盖该分支。
- Windows Debug/Release专项各1/1、含Compiler/Core/A/C1的隔离矩阵各19/19通过；Product-only和
  Lab-on/Testing-off均构建通过且注册0测试。三项错误开关组合按预期Configure失败。详细命令与日志
  见[C1验证报告](windows-msvc-2026-dec042b-lab-v06-execution-stage-c1.md)。
- 本阶段未读写Evidence Bundle、未实现Replay/Compare、未修改A模型接受域、未开放普通CLI、未运行
  UDP或其他网络。C2/C3、事件语法、Linux、Oracle、Golden、硬件及现场均保持未验证。

### 16.9 C1两项P2审查纠错（2026-09-08，待总控复核）

- P2-1实际复现：合法Schema 0.5无转换消息配Values 0.1～0.3时，原C1直接调用仅接受0.4的A解析器，
  三个正例均在准备阶段被拒绝且Codec为0次；旧测试还将0.3拒绝写成正向断言。修复不改变A解析器，
  新增C1内部兼容分派，0.1支持UINT64/BYTES/ENUM，0.2增加BOOL，0.3增加INT64；各代均拒绝
  DECIMAL64，0.1拒绝BOOL，0.2拒绝INT64，未知版本、JSON Number和重复ID继续失败关闭。
- P2-2静态确认为测试证据缺口，未发现产品状态映射缺陷。现通过既有Core测试专用算术故障机制，
  在主Encode返回OK之后、展示Decode之前精确安排下一次Decimal转换失败。断言展示Decode实际返回
  INTERNAL_ERROR，区别于非法Pipeline产生的INVALID_ARGUMENT；两者均保留主Encode为OK、复核
  Decode恰好1次且不交付Result或Frame。未修改Core产品实现或公开接口。
- 修复后Windows Debug/Release C1专项各1/1，含Compiler/Core/A/C1的隔离矩阵各19/19。本轮未改
  旧Lab解析器，因此没有重跑其不受影响的离线矩阵；A解析器未修改，既有A纯格式测试随19项矩阵
  实际通过，不将有限测试扩大为完整接受域证明。链接检查不含旧Protocol Operations、B Evidence或
  Winsock。详见更新后的C1验证报告。

## 17. B合成证据与C执行证据接受矩阵（历史草案，方向由第18节确认）

当前B固定单条Event，event_id=1，frame_origin为LAB_B_SYNTHETIC（有Frame时），
tool_version固定为0.1.0-dec042b-lab-b。它是隔离合成数据格式，不是未来真实阶段事件的已实现接口。
来源标记只描述内部语义，不认证来源；无Frame时origin=null也不能据此推断真实执行。

| 输入类别 | B隔离Reader | C证据/Plan检查 | C Codec重执行及比较 |
| --- | --- | --- | --- |
| 已发布B合成Bundle | 保持现有严格读取 | 可识别为合成；Plan可编译时检查身份，不能授予执行资格 | 建议拒绝作为历史真实执行基准，不静默转NO_CODEC或给EQUAL |
| C真实执行Bundle | 不放宽B旧接受域迁就C | 按经批准的C事件语法及版本矩阵检查，再核对Plan | 仅证据齐全且模式明确时执行；结果与比较分离 |
| 旧代Bundle | 仍走原读取实现，不借B读取 | 保持旧代身份/版本语义 | 原Replay/Compare不变，跨代Run Compare拒绝 |
| 未知/混搭/伪装来源 | 拒绝不合法文件关系 | 失败关闭，不信任仅改origin/tool_version的声明 | 不执行Codec、不产生等价结论 |

C事件名称、字段、数量/顺序、缺失执行材料的模式表达及版本策略仍待C2前专项复核。
不直接把LAB_B_SYNTHETIC替换为新字符串并沿用固定单事件假设；不把C执行过程塞回A模型。
优先保持已确认0.6格式目标；若承载真实阶段信息需要改变第10节或现有文件接受域，须先提出
精确兼容差异并取得拍板，不能以“已授权契约细化”为由自行升级/复用版本并实施。
B向量继续留作隔离测试，C另建真实Codec执行向量，不覆盖历史文件。此矩阵不授权网络重执行。

## 18. C2六项方向决策（CONFIRMED，第一段已限定实现）

1. Values保持0.4，Result及执行指纹保持0.6；C执行Record/Event采用0.7，B合成0.6不变。
   这是对第10/14节统一Evidence 0.6目标的显式修订，不升级Schema或产品版本。
2. Record独立保存准备、结构查询、主Codec、额外复核和物化事实；Result按分支可缺席。
   不伪造Codec返回，不把缺失结果自动降级为NO_CODEC。
3. 事件在实际阶段采集，结束时集中写出；编号连续、同一单调时钟、顺序和Record相互校验。
4. 先检查证据一致性，再执行分支适用的Plan检查。配置编译失败允许成为失败证据，
   无Plan不授予其他分支跳过身份检查的资格。
5. 首批重执行仅材料完整的ENCODE_TX/DECODE_RX。准备、零/多结构候选和物化失败先只读诊断，
   不自动复现或给执行EQUAL。B合成证据无真实执行比较资格，缺失材料必须拒绝。
6. C2分为证据写入/读取、Plan关联及Replay/Compare两个独立收口点，分别授权。
   仍为隔离入口，不接普通CLI、C3、网络或Qt。

无Result则无0.6执行指纹；文件Hash不冒充执行指纹。RUN_FINISHED只表示运行终态，
正式目录重命名成功才表示发布成功；Bundle内不得预先记录自身发布成功。

## 19. C2精确格式及验收契约（CONFIRMED，第一段已限定实现）

用户已确认冻结本节修订后的完整精确契约，以及19.9的CLI映射冻结时点调整。
本节细化第18节；确认契约不等于授权源码实施，第一段授权范围见第20节。

### 19.1 文件和版本分派

- 新执行文件名：`run_record_v0.7.json`、`events_v0.7.jsonl`；有Result时仍用
  `result_summary_v0.6.json`。原配置、Values原文、Frame及SHA256SUMS继续按字节绑定。
- Record格式为`pae.lab.record/0.7`，Event为`pae.lab.event/0.7`；不能只按tool_version或文件名判代。
- B的0.6 Reader、Result 0.6接受域、A20指纹和旧代文件完全不放宽、不迁写。
- C Reader只解析明确0.7执行结构；识别到B时返回合成证据不具执行资格，不猜成新代。
  混代Record/Event、未知版本、未知/重复属性和重复文件角色一律拒绝。
- 新离线执行无received_from，不凭空生成RX Metadata。DECODE_RX中的RX指解析输入，
  不宣称发生网络接收；既有B RX Metadata 0.2仍只按其原关系读取。

### 19.2 Record精确字段建议

所有列出的属性必须出现，可选值用null，不能混用缺失、空字符串及零长度Frame。
以下为0.7顶层允许集；未列属性拒绝。

| 属性 | 类型及约束 |
| --- | --- |
| format_version / run_id / tool_version | 非空字符串；版本固定0.7标识；run_id与目录身份一致；tool_version只记生产者版本，不授予信任 |
| evidence_origin | 固定`LAB_C_EXECUTION`，仅语义分类，不是来源认证 |
| operation_kind | `encode`或`inspect`，与C1及Result逐字一致；Replay保持原种类，不将REPLAY混入指纹 |
| invocation_kind / parent_run_id | `RUN`或`REPLAY`；原Run父身份null，Replay记录父身份但不能依赖父目录存在才能读本Bundle |
| config_file / values_file / frame_file / tx_frame_file / rx_frame_file | 固定相对路径或null；config必须存在；Encode有原Values；Inspect有原Frame；成功Encode才有TX输出 |
| result_file / event_file | Result路径或null；Event路径必须存在 |
| deterministic_fingerprint | 有Result时复用0.6大写Hash，否则null |
| requested_pipeline_id | 明确Pipeline重执行时必填；初次全局Inspect为null，不借Result倒推原请求 |
| execution | 见下方分层对象 |
| comparison | 原Run为null；Replay为独立比较对象，不进入0.6执行指纹 |
| historical_baseline | 原Run为null；Replay保存19.5规定的父Record及历史Result内容绑定 |
| hash_manifest / hash_manifest_excludes_self / recorded_payload_files | 沿用B的清单约束；清单覆盖Record和全部负载，Record的负载列表不包含自身及清单，避免循环Hash |

`execution`固定属性：`terminal_stage`、`terminal_status`、`terminal_reason`、`preparation`、
`structural_query`、`main_codec`、`review_decode`、`result_mapping`、`counts`。
未执行阶段对象为null；终止原因独立保存，不能据原因伪造曾执行的阶段。

- preparation：`status`、`diagnostic_id`、`detail`、`value_index`；索引仅为准备诊断，
  不写入Result的failed_value_index绕过A规则。
- structural_query：`status`、`candidate_class=ZERO|ONE|MULTIPLE|UNDETERMINED`；不记录伪精确数量。
  MULTIPLE表示至少两个；查询异常为UNDETERMINED，不将C1提前结束时的2说成精确总数。
- main_codec / review_decode：`status`；main另有`kind=ENCODE|DECODE`，只保存实际返回值。
- result_mapping：`status=OK|FAILED`；包括Result基础映射、失败Result构造及成功字段复制/校验，
  不包括Review Decode。内部多次进入按19.3分别采集，最终status为全部映射的汇总。
- terminal_reason：`NONE|PREPARATION_REJECTED|STRUCTURAL_REJECTED|STRUCTURAL_QUERY_ERROR|`
  `REVIEW_DECODE_FAILED|REVIEW_MESSAGE_MISMATCH|RAW_ASSOCIATION_FAILED|INTERNAL_ERROR`。
  主Codec非OK但映射成功时原因为NONE，具体错误保留main_codec.status及Result。
  Review失败原因独立保存，此时未执行的result_mapping仍为null。
- counts：C1四项调用数；与事件、阶段对象、结果存在性一致。零候选和歧义的主Decode必须为0。
- 枚举和状态关系以19.8已确认表为准，不接受任意字符串扩展状态。

### 19.3 阶段事件语法建议

每行固定属性：`format_version`、`run_id`、`event_id`、`offset_us`、`event_kind`、`phase`、
`status`。编号从1连续；offset_us为非负整数、同一Run单调原点、非递减，不进入执行指纹。
诊断及字段只保存在Record/Result；事件通过run_id和Record指定的唯一event_file关联，不重复整份结果。

| event_kind | phase / status | 次数及条件 |
| --- | --- | --- |
| RUN_STARTED | NONE / null | 第一条且一次 |
| PHASE_STARTED | PREPARATION、STRUCTURAL_QUERY、MAIN_CODEC、REVIEW_DECODE、RESULT_MAPPING / null | 只在实际进入阶段时采集 |
| PHASE_FINISHED | 与对应开始一致 / 该阶段真实状态 | 每个开始恰有一个结束；阶段失败后不生成未执行阶段 |
| RUN_FINISHED | 真实终止阶段 / 整体终态 | 最后一条且一次；不是PUBLISHED |

阶段不重叠，允许路径如下，M表示RESULT_MAPPING，每个M均对应实际映射调用而非文件序列化：

- 准备失败：PREPARATION后结束；结构拒绝/错误：PREPARATION→STRUCTURAL_QUERY后结束。
- Inspect：PREPARATION→STRUCTURAL_QUERY→MAIN_CODEC→M；主成功才继续第二个M复制字段。
- Encode主失败：PREPARATION→MAIN_CODEC→M（构造失败Result），不做Review。
- Encode主成功：PREPARATION→MAIN_CODEC→REVIEW_DECODE→M→M；两个M分别为基础映射与字段复制。
- 任一M失败立即结束；Review返回失败或返回OK但Message不一致时，Review结束后直接终止，
  status仍为真实Decode返回，终止原因单独说明Message不一致，不生成M。

每个开始与紧随的同phase结束配对，因此重复M无须伪造一个跨阶段计时区间。
结构查询事件覆盖整次候选汇总，counts记录其中实际逐Pipeline调用数。
Encode额外Review Decode与主调用明确分开；Inspect不生成Review Decode事件。
事件上限建议256，超限拒绝发布而非截断；完成Bundle不得含未配对阶段，强杀中间状态不承诺可读。
实现需要在C1真实调用边界增加内部观察接缝，不能只从最终Outcome反造时间和事件；不改变Core/A接受域。

### 19.4 全部分支的结果及诊断映射

| 运行分支 | Record事实 | Result / 指纹 | 重执行及比较 |
| --- | --- | --- | --- |
| 配置、Values、版本、引用准备失败 | PREPARATION_FAILED；准备诊断及适用输入索引；Codec未调用 | 无 / null | 只读诊断，NOT_EVALUATED |
| 全局结构零候选或多候选 | STRUCTURAL_REJECTED；保留UNKNOWN_MESSAGE/AMBIGUOUS_MESSAGE | 无 / null | 只读诊断，NOT_EVALUATED |
| 结构查询其他非OK | STRUCTURAL_ERROR；保留真实状态，candidate_class为UNDETERMINED | 无 / null | 只读诊断，NOT_EVALUATED |
| 主Codec非OK且失败Result映射成功 | CODEC_ERROR；主返回原状态，未Review | 有 / 0.6 | 材料齐全时可重执行；同失败可EQUAL，但不是转换成功 |
| 主Codec非OK且失败Result映射也失败 | LAB_RESULT_FAILED；主仍保留原非OK状态，映射失败原因为INTERNAL_ERROR | 无 / null | 只读诊断，NOT_EVALUATED，不覆盖原Codec错误 |
| 主Codec与物化成功 | OK；Encode适用Review也为OK | 有 / 0.6 | 可重执行和比较 |
| 主Codec成功但Review/物化失败 | LAB_RESULT_FAILED；主仍OK，保留实际Review和物化原因 | 无 / null | 只读诊断，NOT_EVALUATED；不发布成功字段或Encode字节 |
| Reader路径、Hash、语义或重执行资格失败 | 重执行前拒绝，不伪造执行终态 | 不交付Bundle | 当前Codec为0，不产生EQUAL |
| 执行后Writer写入、重读或发布失败 | Writer操作错误；保留已发生的Codec及事件事实 | 不发布正式Bundle，返回的发布路径为空 | 不自动重执行、不发布比较通过结论；不能声称先前Codec未调用 |

准备失败的配置文件仍保留原字节。Reader可确认文件一致性，不因此宣称编译诊断已经独立复现。
无Result分支不得调用A指纹器生成默认对象指纹。对16.5“C2前冻结CLI退出码”的时点已确认
显式修订：C2仅冻结内部终态，CLI映射延至C3实施前单独冻结，见19.9。
不由内部枚举自动推导进程退出码，也不修改Result既有exit_code。

### 19.5 Plan、Replay与比较建议

先完成路径门禁、Hash、严格格式及跨文件关联，再执行C层Plan检查；底层文件Reader不链接Core。
配置无效且记录为配置准备失败时允许只读报告；声称进入结构查询/Codec的Bundle则必须可编译。
已知身份核对Pipeline、Message、方向、Field ID/索引/类型/转换属性及顺序；Encode输入索引按
记录Values的作者顺序检查。Plan身份合法不证明字段数值正确。

重执行资格必须同时满足：C执行0.7、有效Result 0.6、明确模式、完整对应原始材料、通过Plan关联。
ENCODE_TX使用原Values；DECODE_RX使用记录的明确Pipeline与原Frame，禁止重新全局猜测。
初次全局Inspect唯一候选后需记录获选Pipeline供后续明确重执行，但requested_pipeline_id仍为null。
NO_CODEC不作为本次无Result分支的替代；既有B合成NO_CODEC只能按B证据检查，不授予C执行比较资格。

comparison固定建议为`status=EQUAL|DIFFERENT|NOT_EVALUATED`及`reason`，只在双方有合格执行结果
时使用前两者。当前执行发生物化失败时比较NOT_EVALUATED，不能仅因历史成功而填EQUAL。
不同Record来源、格式或不合格分支返回明确不支持/证据不足，不静默降级。
historical_baseline固定属性：`parent_record_file`、`parent_record_sha256`、`result_file`、
`result_sha256`、`fingerprint_domain`、`deterministic_fingerprint`。两个文件分别为
`history/parent_record_v0.7.json`和`history/result_summary_v0.6.json`，逐字节复制原父Record和
其Result；原Run不得存在这些文件。它们均进入本Bundle负载清单和Hash检查。
Reader校验父Record.run_id等于parent_run_id、父Record绑定的Result长度/Hash匹配历史快照、
其声明指纹与快照复算的0.6指纹一致，再据当前指纹验证comparison。历史Record中的祖先路径
只作记录，不递归打开；快照不冒充完整父Bundle验证或来源认证。全量自洽重写仍不能靠Hash识别。
父Record本身先按0.7严格结构解析并校验角色唯一性；不得仅提取几个字符串后忽略其非法状态。
Replay还必须校验父子输入：当前原配置长度/Hash等于父Record对应描述符；encode当前Values
长度/Hash等于父Values描述符；inspect当前原Frame长度/Hash等于父Frame描述符，且当前明确
requested_pipeline_id等于父Result.pipeline_id。模式、operation_kind亦须一致。
任何缺失或不一致均在重执行前拒绝。离线Reader读取新Replay时也检查同样的父子绑定，不能仅
校验本Bundle自洽后接受一个替换输入的EQUAL。encode新输出不要求等于父输出，否则会误拒DIFFERENT。
上述“相同原材料”约束仅针对Replay父子关系；两个独立Run的Compare仍允许等价Decimal的
原Values字节/Hash不同。不把独立Run Compare的宽松原文规则搬到Replay。
缺失或非法历史基准拒绝Replay证据，不仅将自报EQUAL改成NOT_EVALUATED后接受。
comparison.reason限定`FINGERPRINT_EQUAL|FINGERPRINT_DIFFERENT|CURRENT_RESULT_UNAVAILABLE`，
分别对应三种status；原Run的comparison和historical_baseline均为null。
新Replay发布自有完整材料和当前结果，可再次Replay；父Run身份只用于追溯，不改变数学值比较。
当前无Result的Replay可读但不可作为下一次执行基准；“可再次Replay”只适用于合格有Result分支。
原配置Hash仍绑定执行；等价Decimal原Values Hash不同不导致执行不等，时间/父身份不进入A20。

### 19.6 分段授权及最低验收

第一段（C2证据）：新增默认关闭隔离入口，要求Testing；复用A和C1，自有0.7读写与事件采集，
不放宽B Reader，不接普通Lab。验收全部19.4分支、真实事件顺序、Result缺席规则、原始材料绑定、
同对象成功后失败清空、写闭/重读/重命名故障、链接读前拒绝、自洽语义篡改、旧B不变。
其中“无Result/无指纹”是执行分支规定；Writer失败不能追溯抹除已经返回的内存执行结果，
但发布结果必须失败关闭、不返回正式Bundle路径。日志只如实区分执行结果与发布结果。

第二段（C2复现）：另行授权Plan关联、明确Pipeline Decode入口、Replay/Compare；验收自洽非法
身份/顺序/输入索引、材料缺失拒绝、合成证据拒绝、等价Decimal、失败比较相等、Run→Replay→Replay，
以及重执行内部失败不产生成功比较。不得为无Result失败链擅自新增复现算法。

各段Windows Debug/Release先列实际注册清单，再执行针对性及受影响A/B/C1/Core矩阵；隔离构建
和普通Lab拒绝门禁按依赖变化复核。没有运行即不写通过；网络一律不执行，Linux/Oracle/硬件另行授权。
Qt、清理生成物、Stage、Commit、Push不在以上任何实现范围内。

### 19.7 定向复核补充（已纳入确认契约）

- terminal_stage为PREPARATION、STRUCTURAL_QUERY、MAIN_CODEC、REVIEW_DECODE、RESULT_MAPPING；
  terminal_status为PREPARATION_FAILED、STRUCTURAL_REJECTED、STRUCTURAL_ERROR、CODEC_ERROR、
  OK、LAB_RESULT_FAILED。终止阶段取最后实际阶段，不直接照搬C1的Outcome.stage；C1成功映射后
  stage仍可能为CODEC，因此事件观察接缝必须独立采集真实边界。
- preparation.status为OK/FAILED；成功诊断和value_index为null，detail为空字符串。
  结构和Codec状态保留实际枚举，完整允许集见19.8，不因使用字符串而允许未知状态。
- 四项counts采用C1成员名structural_query_calls、encode_calls、decode_calls、review_decode_calls；
  后三项各为0或1；结构次数为非负整数，Plan检查阶段核对不超过Pipeline数。
- 所有长度、索引、计数及offset_us采用严格无符号JSON整数，范围0..UINT64_MAX；禁止负数、
  浮点和指数写法。event_id为1..256；进入本机size_t前必须检查可表示性，不能截断。
- 原配置固定inputs/protocol.pae.json，Values固定inputs/values.pae-lab.json，主Frame固定
  frames/000001_frame.bin。新C离线仅使用主Frame，tx_frame_file/rx_frame_file固定null；
  Result.tx_frame_hex/rx_frame_hex亦保持C1现状null，不为了记录方向修改A20指纹。
  Encode成功主Frame为实际输出；失败无Frame；Inspect主Frame总是原输入，含零长度输入。
  第19.2“成功Encode才有TX输出”指逻辑方向，不要求额外TX文件。禁止一个路径同时承担多个角色。
- Review失败不保存成功Encode字节，但保留主Codec真实状态；异常结果映射属于Lab错误。
  系统分配异常、强杀、断电不在完整Bundle保证范围内，不虚构已完成的阶段事件。
- 第二段验收追加：父Record/历史Result/比较声明的自洽篡改、缺失基准、无Result再Replay拒绝、
  主非OK且映射失败、结构异常、Review失败未进入映射及Inspect两段映射的真实事件顺序。
  第一段只生成并接受invocation_kind=RUN；comparison、historical_baseline和parent_run_id为null，
  REPLAY证据明确拒绝，其实现和校验在第二段授权。

本轮三项缺口已完成文档修订；完整状态及CLI映射时点确认结果见19.8～19.9。
不据此自动启动C2，不声称运行验证通过。

### 19.8 完整状态允许集与关联表（CONFIRMED，第一段已实现）

以下按当前`complete_record_codec.h`、`CodecStatusName()`、结构查询及C1准备分支静态核对。
允许集是可序列化的明确状态集合，不表示每个值都能由合法配置触发，也不表示已动态覆盖。
只在C证据层收紧关系，不更改A/B现有接受域；未知字符串拒绝，不沿用内部switch的默认兜底。

**准备诊断。** `preparation`必须存在；status为OK/FAILED。FAILED的diagnostic_id只允许下表；
detail为诊断说明字符串，不解析detail决定分支或优先级。value_index约束如下：

| diagnostic_id | value_index | 当前来源 |
| --- | --- | --- |
| PAE_LAB_C1_CONFIG_INVALID | null | CompileJsonToPlan失败 |
| PAE_LAB_C1_SCHEMA_UNSUPPORTED | null | 配置编译成功，但不是Schema 0.5 |
| PAE_LAB_C1_VALUES_INVALID | null | Values严格解析或版本分派失败 |
| PAE_LAB_C1_UNKNOWN_PIPELINE | null | Values Pipeline绑定失败 |
| PAE_LAB_C1_UNKNOWN_MESSAGE | null | Values Message绑定失败 |
| PAE_LAB_C1_MESSAGE_NOT_ALLOWED | null | Pipeline不允许该Message |
| PAE_LAB_C1_UNKNOWN_FIELD | 必有非负索引 | 作者顺序Field绑定失败 |
| PAE_LAB_C1_UNKNOWN_ENUM_ENTRY | 必有非负索引 | 作者顺序Enum名称绑定失败 |
| PAE_LAB_C1_UNSUPPORTED_VALUE_KIND | 必有非负索引 | C1类型模型防御分支，不冒充严格Values原文可达分支 |

后七项仅适用于encode。第一段RUN输入必须来自原配置/Values/Frame文本或字节；直接构造
ParsedValues的防御测试不能发布为原Values已通过严格解析的真实证据。
第一段Reader可检查诊断/索引结构，原Values索引真实对应与诊断适用性由第二段Plan检查完成。
未实施Plan检查前只能声明“证据格式和内部关联通过”，不能授予重执行资格。

**结构查询。** status只允许以下五种；全局Inspect才有该对象，encode固定null。

| status | candidate_class | 整体含义 |
| --- | --- | --- |
| OK | ONE | 结构唯一，可进入一次主Decode |
| UNKNOWN_MESSAGE | ZERO | 无匹配，主Decode为0 |
| AMBIGUOUS_MESSAGE | MULTIPLE | 至少两个身份，主Decode为0 |
| INVALID_ARGUMENT / INVALID_PLAN | UNDETERMINED | 查询错误，主Decode为0，不伪造候选数量 |

UNKNOWN_MESSAGE既可为单Pipeline结果也可为最终汇总；Record保存最终汇总状态，
counts保存实际查询次数。第二段明确Pipeline重执行的结构查询约束必须在其实施前补充，
不得默默沿用第一段的全局汇总事件路径；本表不授权新增该接口。

**Codec状态。** 定义集合C（共同项）、D（Decode追加项）、E（Encode追加项）：

- C：`OK`、`INVALID_ARGUMENT`、`INVALID_PLAN`、`WORKSPACE_PLAN_MISMATCH`、`WORKSPACE_BUSY`、
  `INPUT_OUTPUT_OVERLAP`、`VALUE_NOT_REPRESENTABLE`、`INTERNAL_ERROR`。
- D：`UNKNOWN_MESSAGE`、`AMBIGUOUS_MESSAGE`、`OUTPUT_SLOTS_TOO_SMALL`、`INTEGRITY_FAILED`、
  `UNKNOWN_ENUM_VALUE`。
- E：`MESSAGE_NOT_ALLOWED`、`FIELD_REFERENCE_MISMATCH`、`DUPLICATE_FIELD`、`MISSING_FIELD`、
  `TYPE_MISMATCH`、`BYTES_LENGTH_MISMATCH`、`ENUM_REFERENCE_MISMATCH`、`CONSTANT_FIELD_OVERRIDE`、
  `BUFFER_TOO_SMALL`、`FINAL_REVIEW_FAILED`。

main_codec.kind=ENCODE仅接受C∪E，kind=DECODE及review_decode仅接受C∪D。
共23个不同Core状态，数量只为文本核对，不是测试数。未调用用null对象，不写NONE或NOT_EVALUATED。
Review只有在主Encode为OK时可存在；Inspect禁止Review。main_codec.kind与operation_kind对应。
conversion_error不新增Record副本，仍由Result 0.6及其既有四种原因/status关联表达；
无Result时不从诊断文字推导数值原因，不要求结果映射失败分支保留一份伪Result。

**阶段、事件和终态。** RUN_STARTED.offset_us固定0；后续时间非递减。
PHASE_FINISHED.status与该阶段对象对应；RESULT_MAPPING每次仅OK/FAILED，汇总为最后一次状态。
同一Run的准备/结构/主/Review阶段最多各一次；映射最多两次，任一次FAILED后终止。
Record存在阶段对象，当且仅当有对应配对事件；counts与主/Review事件次数精确相等。

| terminal_status | terminal_stage | terminal_reason | Result条件 |
| --- | --- | --- | --- |
| PREPARATION_FAILED | PREPARATION | PREPARATION_REJECTED | 无；准备FAILED，其余阶段null |
| STRUCTURAL_REJECTED | STRUCTURAL_QUERY | STRUCTURAL_REJECTED | 无；状态为UNKNOWN_MESSAGE或AMBIGUOUS_MESSAGE |
| STRUCTURAL_ERROR | STRUCTURAL_QUERY | STRUCTURAL_QUERY_ERROR | 无；状态为INVALID_ARGUMENT或INVALID_PLAN |
| CODEC_ERROR | RESULT_MAPPING | NONE | 有；main非OK，映射一次且OK，Review为null |
| OK | RESULT_MAPPING | NONE | 有；main为OK，两个映射均OK；Encode的Review也OK |
| LAB_RESULT_FAILED | REVIEW_DECODE | REVIEW_DECODE_FAILED | 无；main Encode OK，Review非OK，映射null |
| LAB_RESULT_FAILED | REVIEW_DECODE | REVIEW_MESSAGE_MISMATCH | 无；main Encode及Review均OK，映射null；不将Message检查说成Decode返回错误 |
| LAB_RESULT_FAILED | RESULT_MAPPING | INTERNAL_ERROR | 无；最后映射FAILED，main原状态保持，Review按实际保存 |
| LAB_RESULT_FAILED | RESULT_MAPPING | RAW_ASSOCIATION_FAILED | 无；main OK，首映射OK，第二映射FAILED；Encode的Review须OK |

MAIN_CODEC保留为phase允许值，但在当前正常返回路径中不直接作为完整Run终止阶段：
非OK后仍须映射失败Result。RUN_FINISHED.phase/status必须等于Record的terminal_stage/status。
UNKNOWN_MESSAGE等词不能出现在preparation.status；NOT_EVALUATED属于比较语义，不是实际Codec状态。
有Result时其operation_kind、current_execution_status及配置Hash与Record/原文件逐项绑定；
OK结果无失败身份/诊断，非OK结果operation_status=CODEC_ERROR、exit_code=5且两个diagnostic_id
均为`PAE_LAB_CODEC_<main status>`。错误字段/raw/索引继续遵循A规则和第二段Plan校验。

### 19.9 CLI冻结时点的单独拍板（CONFIRMED）

用户已确认：将16.5要求的CLI退出码映射冻结后移至C3实施前，C2仅交付内部Run/Reader/Writer/
Replay状态。原因是C2没有普通CLI，也不实现`--expect-status`、进程异常出口或Compare进程退出。
这不更改既有CLI的0、2～10含义，也不更改C1 Result的0/5；尤其不能把新增终止枚举的序号
当退出码，或为了进程退出10而将原主Codec OK篡改为INTERNAL_ERROR。

C3前必须单独确认准备诊断分派、Reader输入错误、Writer失败、Lab复核内部错误、Compare差异、
`--expect-status`及同时失败优先级。未冻结不得开启C3；同意本轮文档收口不等于授权C2或C3源码。

### 19.10 本轮只读复核记录（2026-09-08）

本轮仅对照C1/Core源码补齐推荐枚举及状态关系；另经独立只读复核，把Reader重执行前拒绝与
Writer执行后发布失败拆开。后者不交付正式Bundle，但不能将已发生Codec次数改写为0。
第一段失败发布验收须断言该区别，且不得为重试发布自动重新执行Codec。
另补齐第二段父子输入的长度/Hash、模式及明确Pipeline绑定；替换输入但保持失败指纹相等的
负例须在执行前拒绝。独立只读复核未发现其余实质冲突；该结论不是实现正确性或完整输入域证明。
未运行构建、测试或网络；这是契约静态一致性核对，不是新机器格式已实现的证据。

## 20. C2第一段执行任务范围（已授权并完成限定实现，待总控复核）

执行对象为既有《子任务推进》。接管时必须重核HEAD、工作树和适用规则，保留当前全部Markdown
变更。20.1～20.3保留实施授权前的任务草案和权限边界，第一段后续已单独授权并完成限定实施，
证据见20.4～20.5；旧草案不代表当前仍未授权，也不授权第二段。权威为第18～19节，不另行放宽接受域。

### 20.1 授权后交付范围

- 仅RUN证据0.7：真实C1阶段观察、内部执行编排、Writer、严格Reader、独立测试和验证文档。
- 允许范围拟为Lab内部源码、必要C1观察接缝、CMake、隔离测试及相关Markdown；不修改Core
  算法/公共接口、Schema、A接受域/指纹或B旧Reader语义。通用IO复用不得将Compiler/Core反向
  引入底层Reader。新增开关默认关闭且依赖Testing，普通Lab/Schema 0.5组合拒绝保持。
- 第一段仅接受invocation_kind=RUN，父身份/历史基准/比较字段为null；遇REPLAY明确拒绝。
  第19节第二段的Plan关联、明确Pipeline重执行、历史快照、Replay/Compare均不提前实现。
- 记录Codec真实阶段和失败整体交付；不得从最终Outcome反造时间，不把Writer失败说成未执行Codec。
  Writer发布失败不自动重跑Codec，不交付正式路径；无Result分支不生成执行指纹。
- 如发现冻结契约无法由当前C1无语义变更地实现，先提交具体冲突与最小修订建议，暂停相关部分，
  不静默改变版本、状态、事件路径或测试预期。

### 20.2 验收及Windows验证要求

1. RUN成功Encode/Inspect、无转换0.5、转换失败、准备失败、结构拒绝/异常、Review/映射失败，
   包括主非OK且Result映射失败；逐项核对事件、调用数、原始输入和Result/指纹有无。
2. 事务故障：写闭、重读、内容/Hash不一致、重命名失败、已有正式/中间目录拒绝覆盖；Reader
   缺失/额外/重复/未知属性、自洽语义篡改、路径/链接门禁、成功后失败输出清空。
3. 第一段拒绝REPLAY及非null历史/比较字段；B合成0.6和旧代不被当作C执行0.7；不授予Plan
   验证或重执行资格。防御分支可用隔离接缝核实，不伪称合法配置和严格Values入口实际可达。
4. Windows Debug/Release先列注册清单，再跑专项及受影响A/B/C1/Core回归；按依赖变化复核
   Product-only、Lab-on/Testing-off和错误开关门禁。全部串行使用明确构建目录，避免共享产物污染。
   排除UDP及任何网络测试；不能把历史成功数字当本轮重跑。
5. 回报实际命令、日志、结果、文件清单和契约映射；未执行项明确标注。格式、敏感内容和
   diff空白检查仅覆盖本轮候选；第三方源码不格式化，历史out不清理。

### 20.3 不在授权草案中的内容

C2第二段、C3、普通CLI、网络/UDP、Qt及依赖复制、生成物清理、Linux、Oracle、真实协议Golden、
硬件和现场验证均不包含。Stage、Commit、Push、Git配置修改亦不包含。
完成后仅报告“待总控复核”，不能自行进入第二段或提交。源码实施及Windows验证须用户另行授权。

### 20.4 第一段实施与Windows验证记录（2026-09-08）

本轮在默认关闭且要求Testing的`PAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS`下完成：

- C1内部执行桥接增加阶段观察接缝，在准备、结构查询、主Codec、Encode Review Decode及Result
  映射的实际边界采集事件；普通C1调用保持兼容，未修改Core公共接口或算法。
- 新增RUN编排、Record/Event 0.7事务Writer及严格Reader；绑定原配置、Values/Frame、可选Result
  0.6、执行指纹、文件长度/Hash和Manifest，发布前完成完整自读，正式及中间目录均不覆盖。
- 严格Reader拒绝未知/重复属性、非法终态/事件/调用计数、REPLAY及非null父/历史/比较字段；
  B合成0.6继续由B Reader读取，C Reader不猜代际。Inspect准备诊断适用性、主Codec对象/调用数
  双向关系及Review必须建立在成功主Encode上的关联也已失败关闭。
- 写闭、写后重读、重命名和目标占用失败均不返回正式路径，不抹除已发生的Encode/Review次数，
  不自动重试执行。

恢复后当前源码的Windows Debug/Release专项各`2/2 PASS`，受影响离线切片各`22/22 PASS`；
Product-only和Lab-on/Testing-off的Debug/Release均构建成功且注册0个测试，三类错误开关按预期
配置失败。完整命令、日志和边界见
[第一段验证报告](windows-msvc-2026-dec042b-lab-v07-run-evidence-stage-c2-first.md)。

该证据仅证明第一段格式、执行记录及包内关联；未实现或验证第二段Plan关联、Replay/Compare、
C3普通CLI/退出码、UDP/网络、Linux、Oracle、真实协议Golden、硬件或现场。当前状态为待总控复核，
未Stage、Commit或Push。

### 20.5 第一段Reader两项P2纠错记录（2026-09-08）

总控复核指出并经自洽证据变异动态确认：成功Inspect的主Decode此前没有显式绑定结构查询
`OK/ONE`；`RAW_ASSOCIATION_FAILED`此前允许只保留一次FAILED映射。Reader现要求前者由唯一
结构候选授权，后者严格保存`OK`后`FAILED`两次映射，避免终态与实际阶段轨迹脱节。

同时补齐Record 0.7与Result 0.6的执行语义绑定：Encode只接受`ENCODE_TX/TX`，Inspect只接受
`DECODE_RX/RX`；成功Result不得携带失败诊断，Codec失败的外层和当前执行诊断必须同时精确对应
主Codec状态。修复只收紧既有第19.8节关系，没有改变格式字段、版本、指纹算法、Core、A接受域
或B语义。

修复前Debug专项为`1/2`，9条已同步Result指纹、payload长度/Hash、Record指纹及Manifest的
语义变异被错误接受；修复后Windows Debug/Release专项各`2/2 PASS`，相关v06+v07离线测试各
`5/5 PASS`。第一段P2前完整离线切片各`22/22 PASS`保留为历史证据，本次未追溯改写为修复后
全量结果。详细命令、日志和边界见第一段验证报告；当前仍为待总控复核。

## 21. C2第二段集中实施契约（CONFIRMED，已授权实施）

2026-09-08：用户已确认本节四组方案并授权整个C2第二段检查点连续实施。第18～19节已确认规则
保持权威；本节指定Pipeline事件语义、检查层职责和验收组合均已生效，不回写第一段历史事实。
推进流程见[功能检查点计划](checkpoint-delivery-plan.md)。

### 21.1 Plan关联与重执行准入

- 底层Reader继续无Compiler/Core/Plan依赖；完成文件、格式和跨文件校验后，由C执行层编译原配置并检查Plan关联。
- 无Result记录保留只读能力，不授予重执行或执行等价比较资格；配置准备失败不因无法编译而失去只读诊断能力。
- 有Result者核对Schema、配置Hash、Pipeline、Message及成员关系；方向按实际Encode/Decode能力核对，不增加配置中不存在的方向属性。
- 成功字段按冻结Plan的实际交付集合检查数量、顺序、ID、类型和转换属性；转换字段raw标签核对原始线类型。失败字段数组仍为空，不要求失败结果伪造成功字段。
- 已知失败字段的ID/索引必须指向同一冻结字段；Encode输入索引必须对应原Values作者顺序中的真实条目。未知字段失败允许仅有输入索引，但须核实该条目确实无法绑定。
- 核对失败原因与相关字段/输入类型的适用性，不重新执行Codec来验证历史错误是否真实发生；Plan合法不是数值正确或来源真实的证明。
- 文件/Plan/资格拒绝发生在任何重执行Codec调用之前：不发布子Run，不伪造阶段事件。内部诊断采用独立枚举或类型化状态，不依赖解析错误文字；CLI编码仍留给C3。

### 21.2 明确Pipeline重执行与实际事件

- 仅新增内部指定Pipeline Decode入口；Pipeline来自合格父Result，禁止回退到全局Inspect。
- 指定Pipeline必须在重执行准备完成前存在且具备所需能力；无效引用由前置准入拒绝，不新增第一段准备诊断字符串。
- Inspect重执行仍有一次STRUCTURAL_QUERY阶段，但只查询指定Pipeline一次：structural_query_calls=1。
  允许状态沿用19.8五种组合；只有OK/ONE进入一次主Decode；零/多候选或查询故障仍走既有无Result终态。
- 查询后的防御性Matcher复核属于主Decode内部，不额外记成结构查询调用。全局初次RUN路径及其真实计数不改变。
- 事件保持0.7既有名称与阶段集合；RUN_STARTED/RUN_FINISHED表示一次实际执行生命周期，同样用于invocation_kind=REPLAY，不另增REPLAY_STARTED事件。
- Encode重执行沿用原Values指定的Pipeline/Message，结构查询仍为空、计数0；Review及结果映射沿用既有真实阶段规则。
- 前置证据/Plan核验在当前运行事件时钟开始前完成；通过后新的准备、查询、Codec、Review和映射按实际发生记录。发布失败不修改已发生调用次数。
- 新Reader对RUN保持第一段规则，对REPLAY按本节及19.5绑定requested_pipeline_id：Inspect为父Result.pipeline_id；Encode仍为null。

### 21.3 Replay、历史快照与Compare

- 仅合格C执行0.7、有Result 0.6且完整材料通过准入的记录允许Replay；B合成0.6和无Result分支明确拒绝执行，不猜版本、不降级。
- 严格复用19.5父Record/历史Result固定路径、逐字节快照、长度/Hash及父子输入绑定，不递归读取祖先Bundle。
- 当前结果独立生成；operation_kind保持encode/inspect，不改为replay。A20编码、数学等价规范化及0.6指纹域均不改变。
- 当前有合格Result时按既有指纹比较EQUAL/DIFFERENT；相同失败允许EQUAL，但当前Codec仍失败。
- 当前无Result时保存真实失败阶段及NOT_EVALUATED/CURRENT_RESULT_UNAVAILABLE；该子记录可读但不可作为下一次Replay基准。
- 独立Run Compare只读双方证据并执行适用Plan准入，不调用Codec，不发布新的执行Bundle；不适用的输入返回明确拒绝，不伪造EQUAL或DIFFERENT。
- 独立Compare允许数学等价Decimal原Values文本/Hash不同；Replay父子仍必须满足原材料长度/Hash相同。Encode新输出允许不同，不能预先要求等于父输出。
- 历史快照非法或比较声明与复算结果矛盾时整个Reader拒绝，不通过改写为NOT_EVALUATED来容忍损坏证据。

### 21.4 一次性验收与交付边界

以下作为一个检查点交付，允许实现授权范围内连续修复，不逐条拆成新的审批轮次：

| 验收组 | 必须证明 |
| --- | --- |
| Plan身份 | 自洽非法Pipeline/Message/字段顺序、类型、raw标签及输入索引均在Codec前拒绝；合法失败身份正对照通过 |
| 指定Pipeline | 不遍历其他Pipeline；查询计数1；OK/ONE才Decode；零/多/故障无Result；原全局RUN不变 |
| 执行与比较 | Encode/Inspect成功；相同失败EQUAL但非成功；等价Decimal独立Compare；合法不同结果DIFFERENT |
| 历史链 | Run→Replay→Replay；父子材料/快照/比较声明自洽篡改拒绝；无Result可读但不能再次Replay |
| 失败边界 | 准入拒绝零Codec且无发布；执行后Review/映射失败保留真实计数；Writer失败不重执行、不返回正式路径 |
| 回归隔离 | A/B/C1及第一段受影响离线矩阵；Reader链接隔离；普通Lab与Schema 0.5组合仍拒绝 |

开发期运行受影响专项，收口对最终代码集中执行Windows Debug/Release相关离线矩阵；只有依赖或
构建边界变化才重跑对应Product-only/Testing-off门禁。失败日志与最终日志区分，不用历史批次充数。
测试中无法由合法Plan触发的防御分支明确标注测试接缝，不制造非法Plan冒充合法路径。

不展开C3、普通CLI、网络、Qt、Core新算法、A接受域变更、生成物清理或Git写操作。
授权前文档细化曾静态核对现有C1接口、0.7记录模型与19.5/19.8规则；该历史记录不作为本次
实现或运行验证证据。当前检查点的实际实施与验证另行追加记录。

### 21.5 C2第二段实施与Windows验证记录（2026-09-08）

本轮在既有默认关闭、Testing-only的0.7目标内完成Plan关联、指定Pipeline Decode、Replay历史
快照和独立Compare。底层Reader仍无Compiler/Core/Plan依赖；上层准入编译原配置并核对Protocol、
Pipeline、Message、方向、字段顺序/类型/conversion raw标签及失败字段/Values作者索引，拒绝发生
在重执行与发布之前。

Inspect Replay只查询父Result指定Pipeline一次，`OK/ONE`才执行一次主Decode；ZERO、MULTIPLE及
查询故障均记录真实0.7事件、0次Decode和无Result。Replay逐字节保存父Record/历史Result快照，
绑定父子配置和Values/Frame；有Result按0.6指纹比较，无Result保存NOT_EVALUATED且不能成为下一
次基准。独立Compare只读、零Codec、零发布，不同operation明确拒绝；等价Decimal原文仍按A20
规范指纹比较。

第21.4节六组验收已落入自动化。Windows Debug/Release专项各`2/2 PASS`，当前注册完整离线矩阵
各`22/22 PASS`；Reader链接隔离复核通过。本次未修改CMake或目标依赖，未重复Product-only与
Testing-off配置门禁。详细命令、日志和边界见
[第二段验证报告](windows-msvc-2026-dec042b-lab-v07-replay-compare-stage-c2-second.md)。当前状态为
待总控复核，未Stage、Commit或Push。

### 21.6 C2第二段历史快照与失败适用性纠错（2026-09-08）

总控复核后补齐两处P2。第一，历史父Record/Result现在复用当前记录的文件角色集合及Result执行
关系门禁：父快照仍只作低层严格解析，不递归读取祖先路径，但必须满足operation与Replay模式、
主体、终态、诊断的对应关系；父Record描述符集合必须与其RUN/REPLAY及Values/Frame/Result角色
一致。历史`frame_hex`仅与父Record已有Frame描述符的字节长度和Hash绑定，不重新执行Codec。

第二，Plan准入不再仅以Values kind相等判断`BYTES_LENGTH_MISMATCH`；该状态必须对应BYTES字段及
BYTES输入。同类型INT64输入自洽声明该状态会在重执行前拒绝。修复前Debug专项动态证明历史模式、
父descriptor集合和INT64错误状态可被放行；父`values_file=null`已由既有终态角色门禁拒绝，未将
其误报为新产品缺陷。所有篡改用例在目标语义检查前显式断言清单、长度及Hash关联自洽。

修复后Windows Debug/Release专项各`2/2 PASS`、完整离线矩阵各`22/22 PASS`。格式、A接受域、
A20指纹、底层Reader依赖边界及C3范围均未改变，详细日志见第二段验证报告。
