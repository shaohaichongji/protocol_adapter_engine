# PAE-DEC-042B 精确比例与偏置转换确认契约

创建日期：2026-09-06。确认更新：2026-09-07。第1节十项决策及第3～6节四组补充方案
均为CONFIRMED（已确认）。用户随后授权隔离算术验证，固定256位候选已完成Windows
Debug/Release隔离测试；只读审查未发现明确实现缺陷，已补齐P2测试证据缺口。
Core双向转换第二段已实现并完成限定Windows验证，不Stage/Commit/Push。
Values及Lab新版本仍是目标契约，不是当前可执行能力。
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
提交并Push。当前工作树已接入专用开关下的DECIMAL64 Core双向转换，第二段尚未提交。
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
生产接入A1～D4已确认，实施必须遵循第8节；不代表新Schema、Core或Lab已实现。
独立高精度Oracle的具体工具亦需单独批准，不因本轮确认而允许引入或执行。
该次审查的P2测试缺口已补齐；三段实施中的首段编译冻结与计费、第二段Core双向转换均已实现
并完成限定Windows验证；首段已审查提交，第二段raw诊断失败调用生命周期补测已完成，详见
Core报告。Lab证据段仍未开始。
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
