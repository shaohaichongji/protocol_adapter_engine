# PAE-DEC-042 数值转换范围与DEC-042A确认契约

日期：2026-09-06。状态：第1节11项范围及第3～6节四组补充契约均为CONFIRMED（已确认）；
DEC-042A已实现并完成总控复核，已随`7807b9a`提交并Push，Windows验证证据见对应报告。
保留文件名中的draft以维持链接稳定，不代表A契约仍待拍板。
DEC-042B十项决策及四组补充已确认，见[独立契约](pae-dec-042b-decimal-conversion-contract-draft.md)。
固定256位候选已执行Windows隔离验证、只读审查及P2补测；生产接入A1～D4共16项已确认。
尚未接入生产Core；本轮仅同步Markdown，生产实施及验证另行授权。

## 1. 已确认范围

1. 目标为单字段双向转换：logical = raw × scale + bias，Encode使用反向公式；不加入脚本或跨字段公式。
2. 分为DEC-042A字节对齐INT64（64位有符号整数）与DEC-042B精确比例/偏置两个切片，分别冻结与授权。
3. A采用二进制补码，支持1～8字节及大小端，包括24/40/48/56 bit；有符号位字段、原码、反码后置。
4. B首切片仅覆盖字节对齐UINT64/INT64到DECIMAL64（精确十进制业务值）；无转换字段保留旧类型。
5. 比例与偏置用精确有理数，允许负比例，比例非零、分母为正；参数边界和溢出算法须另行冻结。
6. 不隐式舍入、截断或饱和；不可精确表示则失败。B首切片在编译期拒绝不能保证有限十进制表达的
   转换参数，具体接受域待B契约定义；有限小数不等于满足DECIMAL64的系数与0～18小数位边界。
7. 接口保持类型化；转换字段Encode明确接收业务值，不按数值外观猜测raw/logical；原始值诊断及资源另行契约化。
8. 多输入派生、模式选择和业务决策由宿主完成，PAE只承担字段转换。
9. Decode结构唯一性、完整性校验先于字段交付；Encode写入后生成SUM8并最终复核，失败不交付有效报文。
10. 新能力显式版本门禁，旧配置行为不变；Schema、Values、Lab与指纹的组合须按语义影响冻结。
11. 使用独立合成预期而非仅往返自洽；Windows Debug/Release、旧格式与失败Replay回归，不扩大硬件等证据。

## 2. 当前实现与边界

DEC-042A实施前基线为`4d923e9`；当前交付基线为`7807b9a`，DEC-041文档收口为`97da00e`，均已Push。
当前Schema 0.4增加字节对齐INT64；Core使用独立INT64标签和`std::int64_t`存储。DECIMAL64尚未实现。
`encode.source=input`已支持每次调用传入不同的INT64值；INT64的input与constant（常量）来源
均已实现。B不属于本轮A实施范围。

A仅扩展现有内部COMPLETE_RECORD（完整记录）能力链，不发布稳定公共API（应用编程接口）、
C ABI（C语言二进制接口）或Runtime（运行时调度）。没有新增传输、线程、浮点Wire编码或依赖库。

## 3. 补充拍板A：配置与输入来源（已确认）

- 沿用现有字段布局，新增`value_type: "INT64"`；Wire为1～8字节的字节对齐整数。
- 多字节必须显式big_endian或little_endian，单字节沿用UINT64规则；不增加重复的signed开关。
- INT64同时支持`encode.source=input`与`constant`，不增加default/computed或通用常量框架。
- constant使用精确JSON整数Token，不经double；必须满足INT64范围及实际Wire位宽。
  小数和指数Token拒绝；配置整数`-0`按现有signed Number规则归一为0，不改变unsigned规则。
- INT64位成员、ENUM有符号原始值、scale/bias及新constraints属性仍严格拒绝，不先接受后忽略。
- Matcher与常量字段的现有兼容性检查延伸到INT64，按最终补码字节比较，不新增重叠许可。

## 4. 补充拍板B：类型、算法与诊断（已确认）

Core增加独立INT64标签和`std::int64_t`值；禁止UINT64/INT64间隐式转换，即使值为正数。
Lab INT64的raw_value与logical_value均为规范有符号十进制字符串，A阶段二者数学值相同。
高位为1的原始位模式不作为正UINT64业务值暴露；需要字节诊断时使用原始Frame。

位宽w=8×byte_length，接受范围为[-2^(w-1), 2^(w-1)-1]。逐字节以无符号整数累积，
再安全解释补码；不使用非对齐指针转型，不对负数移位，不进行越界有符号运算，不移位64位。
特别避免对INT64_MIN直接取负、构造有符号2^63或对超INT64_MAX的uint64_t直接强转。
Encode先验证范围，再用标准定义的无符号转换提取低w位；最终复核按有符号数学值比较。

- 输入类型不符：TYPE_MISMATCH；INT64值超实际Wire范围：VALUE_NOT_REPRESENTABLE。
- 常量覆盖、缺字段、重复字段、容量、引用与Buffer重叠沿用既有诊断及前置次序。
- 配置常量越界在编译期失败，带属性位置，不延迟至运行期；Builder保留损坏Draft防御。
- 任意合法宽度的补码位模式都有一个INT64值，A不凭空增加“非法负数”Decode错误。
- 保持结构唯一性→输出容量→SUM8→字段语义的次序；Encode最终复核失败仍为FINAL_REVIEW_FAILED。
- 失败field_count/bytes_written为0不等于调用方内存未改写；不得消费失败后的字段或Buffer。

## 5. 补充拍板C：版本与Replay（已确认）

| 项目 | A已实现组合 |
| --- | --- |
| Schema | 新0.4接受INT64及既有能力；0.1～0.3拒绝INT64 |
| Values | 新pae.lab.values/0.3增加INT64；0.1/0.2不得承载INT64 |
| Result/Record/Event | Schema 0.4统一使用0.5，即使该配置没有INT64；独立确定性指纹域 |
| RX Metadata | 保持0.2，不改变来源与字节绑定 |
| 产品版本 | 不随内部格式代际升级 |

Values新增条目确认为`{"id":"signed_value","kind":"INT64","int64":"-123"}`。
字符串仅允许`0`或可选负号加无前导零的非零十进制整数；拒绝`-0`、`+1`、空白、指数、小数、
JSON Number代替字符串及越界值。配置Number与Lab规范文本是两个入口，不混用词法规则。

Schema 0.4可沿用旧Values承载旧类型（BOOL仍至少0.2）；Values 0.3限定Schema 0.4，
旧Schema保持原组合不变。严格拒绝未知版本与属性；旧指纹和历史夹具不得重写。
新代继续两阶段Inspect、明确Replay模式、历史Transport分离；同代失败可比较相等但仍失败。
跨Schema替换Replay与跨代Run Compare保持拒绝且比较未评估；Frame字节Compare仍可跨代。
Evidence读取须验证INT64标签、规范文本及合法版本组合，不仅验证Hash链。

## 6. 补充拍板D：资源与验证（已确认）

延伸SchemaIr、Validator、Budgeted Draft、Frozen Plan、Core和Lab完整链，不绕过能力链。
允许增加类型化存储，但实际sizeof、对齐、Arena和Workspace影响必须重新计费与验证，
不能仅因int64_t和uint64_t同宽就宣称零增量。热路径不新增堆分配，读取/写入最多8字节；
这只是设计上界，不是性能实测。不得借本次实施顺便重构全部Value存储。

| 编号 | 必须验证的行为 |
| --- | --- |
| I01 | 1～8字节，0、1、-1、各宽度最小/最大值；多字节两种字节序 |
| I02 | 24/40/48/56 bit符号边界，INT64_MIN/MAX，无移位64和取负最小值路径 |
| I03 | 各窄宽度越界正负输入拒绝；UINT64正数不能隐式当INT64 |
| I04 | input动态变化、constant正负及最小值、常量覆盖与Matcher兼容检查 |
| I05 | 未知属性/类型/版本、位成员INT64、浮点/指数常量、精确整数越界拒绝 |
| I06 | 新Values规范文本及边界；自洽Hash链下的非法INT64证据被语义读取层拒绝 |
| I07 | 独立Encode与Decode向量、非零预填Buffer、混合旧字段和SUM8；失败不交付 |
| I08 | 零/单/多结构候选及SUM8失败优先级，不以字段成功筛选消息 |
| I09 | 原Run→Replay A→Replay B、同代替换配置差异、失败比较及NO_CODEC模式 |
| I10 | 旧格式/旧指纹、新Schema无INT64、跨代门禁、历史Transport分离 |
| I11 | Plan精确计费、损坏Draft、首次调用分配检查、槽容量和Buffer边界 |
| I12 | Windows Debug/Release专项与全切片、Product-only及Lab-on/Testing-off隔离 |

独立算术预期包括：24位大端`FF FF FE`=-2，小端`FE FF FF`=-2；
24位大端`80 00 00`=-8388608、`7F FF FF`=8388607；64位大端`80 00 00 00 00 00 00 00`
=-9223372036854775808。上述预期已由独立公开向量和Core测试固化；实际执行结果见验证报告。
默认离线；若未来获准实现且共享Lab变化需要UDP回归，授权中须明确仅本机Loopback（回环）。

## 7. 实施与验证状态

Schema、能力链、Core和Lab 0.5已按A～D实现；公开样例和I01～I12映射见
[Windows验证报告](windows-msvc-2026-dec042a-int64-slice.md)。本次证据限Windows、公开合成向量和
既有Loopback自动化；Stage、Commit、Push仍分别受授权约束。
B的有理数接受域、DECIMAL64表示/溢出、原始值诊断、具体版本与测试矩阵仍待单独细化；
本次A版本确认不预先冻结B版本。Linux、真实协议Golden（权威预期向量）、硬件、现场与性能均未验证。

### 7.1 P2限定修复复核（2026-09-06）

I11补齐Builder对Schema代际和位字段INPUT有符号常量残留的发布前防御。从合法预算Draft仅注入
单项异常，修复前旧Schema的INT64及三类位字段共六例均被错误发布；修复后精确拒绝为
`INVALID_FIELD_PLAN`，合法对照仍通过。这不是普通JSON入口可绕过校验的证明。

I06首次测试只把INT64标签改成UNKNOWN，未覆盖INT64专用数值元组分支；本轮保留该历史限制，
新增七类保留INT64标签的负例，调用Reader前核验Record全部payload长度/Hash及清单全部Hash关联，
精确命中INT64元组诊断。Reader产品逻辑未改。专项各6/6、Lab各13/13、全切片各45/45及两类
隔离构建Debug/Release通过；详见验证报告第7节。状态仍为待总控复核，未Stage、Commit、Push。
