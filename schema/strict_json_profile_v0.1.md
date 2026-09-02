# PAE Strict JSON Profile V0.1

## 1. 文档状态

| 项目 | 内容 |
| --- | --- |
| 文档版本 | 0.1.7 |
| 当前状态 | `CONFIRMED BASELINE + WORKING PROFILE + WINDOWS SPIKE AND LOADER SLICE PARTIALLY VERIFIED（已确认基线 + 工作规范 + Windows 技术探针及Loader切片部分验证）` |
| 适用范围 | `*.pae.json`的字节输入、JSON Token（词法单元）和 Parser（解析器）边界 |
| 不覆盖 | PAE 对象结构、协议字段语义、跨引用、五项容量候选的最终实测数值和 ProtocolPlan 执行语义 |
| 当前验证环境 | Windows x64、MSVC Release；按当前 Windows-first（Windows 优先）顺序，Linux GCC/Clang 实测暂缓 |

本文把《PAE V0.1 技术细节拍板方案》中的`PAE-DEC-008`、`PAE-DEC-025`、`PAE-DEC-029`和`PAE-DEC-030`细化为可执行的 Strict JSON Profile（严格 JSON 子集规范）。已确认条款可以作为实现约束；包含`OPEN`项的完整 Profile 尚未最终冻结。Windows Spike 已覆盖第十三轮数字语义和 Parser-stage（解析阶段）资源边界；内部 Loader/Compiler（加载器/编译器）切片已覆盖受限Schema、SchemaIr和PlanBundle链路，但仍不覆盖全链峰值、完整生产Schema或最终Parser选型。本文不是完整V0.1 `pae.schema.json`，也不把技术探针中的临时对象结构固化为完整生产 Schema（配置结构规范）。

规则状态按以下方式理解：

- 标记为“必须”或“拒绝”的规则来自已经确认的 PAE 基线；
- “当前 Spike 行为”只描述实验实现和证据，不自动成为生产公共 API（Application Programming Interface，应用程序编程接口）；
- 标记为`OPEN（待定）`的内容必须在生产 Loader（加载器）实现前另行冻结；标记为`CANDIDATE / UNVERIFIED（候选、未验证）`的容量必须经过生成型边界和峰值内存实测后才能升级为生产上限。

## 2. 分层边界

PAE 配置编译必须遵循以下职责顺序：

```text
输入字节
→ StrictJsonLoader
→ StructuralValidator / SchemaIrBuilder
→ DomainValidator
→ ResourceBudgetValidator
→ PlanBuilder
→ immutable ProtocolPlan / PipelinePlan
```

Strict JSON Profile 只决定“输入是否是 PAE 接受的严格 JSON 文档，以及 JSON Number（数字）Token 的原始形态是什么”。后续规则分别归属：

- StructuralValidator（结构校验器）：根对象、属性名称、必填项、JSON 类型、局部枚举和局部范围；
- DomainValidator（领域校验器）：引用、方向、字段布局、Matcher（消息匹配器）歧义、computed（计算字段）依赖和 Integrity（完整性校验）范围；
- ResourceBudgetValidator（资源预算校验器）：Profile（资源档位）容量和编译后资源准入；
- PlanBuilder（执行计划构建器）：正式Config Compiler入口的目标职责是消费已经验证的类型化中间表示并构建不可变Plan，不重新解释原始JSON；当前内部`PlanDraft`能力边界尚不可伪造，Builder仍保留Domain安全规则的防御复核，不能把目标类型状态误写成已经完全落盘。

## 3. 输入字节和编码

### 3.1 单一文档

- 输入必须只包含一个完整 JSON 文档；
- 文档前后允许 JSON 标准空白；
- 完整文档后的第二个值或其他非空白内容必须拒绝；
- 空输入和纯空白输入必须拒绝；
- 文件名不参与协议身份和配置语义。

JSON 语法层允许任意合法的顶层 JSON 值；`*.pae.json`的 PAE StructuralValidator 必须进一步要求顶层值为 Object（对象）。该分层避免 StrictJsonLoader 认识具体 PAE Schema。

### 3.2 UTF-8

输入必须是合法 UTF-8（Unicode Transformation Format 8-bit，8 位 Unicode 转换格式）：

- 拒绝截断序列、孤立 continuation byte（续字节）、overlong encoding（过长编码）和历史 5/6 字节形式；
- 拒绝用 UTF-8 直接编码的 Unicode surrogate（代理项）码点；
- 拒绝大于`U+10FFFF`的码点；
- 拒绝 UTF-8 BOM（Byte Order Mark，字节顺序标记），包括文件起始位置的`EF BB BF`；
- 输入字节不得依赖当前 Windows Code Page（代码页）或系统 Locale（区域设置）解释。

输入不执行 Unicode Normalization（Unicode 规范化）。例如预组字符`U+00E9`与`U+0065 U+0301`是两个不同的字符串和两个不同的对象 key。

### 3.3 JSON 空白

字符串外只接受 JSON 定义的四种空白字节：

```text
U+0020 SPACE
U+0009 HORIZONTAL TAB
U+000A LINE FEED
U+000D CARRIAGE RETURN
```

Vertical Tab（垂直制表符）、Form Feed（换页符）、Non-Breaking Space（不换行空格）及其他 Unicode 空白不得被当作 JSON 空白接受。

## 4. 语法范围

PAE 接受 RFC 8259 所定义的标准 JSON 语法，并明确拒绝：

- JSONC、JSON5 和注释；
- 尾随逗号；
- 单引号字符串；
- 未加双引号的对象 key；
- 十六进制、八进制或带前导`+`的 JSON Number；
- `NaN`、正负 Infinity（无穷大）和 Parser 私有扩展数字；
- 前导零、多余小数点和不完整指数；
- 多文档拼接；
- Parser 的宽松、原位改写或自动修复模式。

Loader Backend（加载器后端）必须显式关闭候选 Parser 的非标准扩展，不能依赖其默认值长期不变。

## 5. String 和 Object key

### 5.1 字符串

- 字符串必须使用双引号；
- 只接受标准 JSON escape（转义）；
- 字符串中的`U+0000`至`U+001F`控制字符必须使用转义形式，原始控制字节必须拒绝；
- `\uXXXX`中的单个高代理项必须紧跟一个低代理项；
- 单独高代理项、单独低代理项和反向代理项必须拒绝；
- 合法代理项对解码为对应的 Unicode scalar value（Unicode 标量值）；
- `\/`与`/`语义相同；
- `\u0000`在 JSON Token 层是合法字符串内容，但稳定 ASCII ID、文件路径、算法名等具体属性可以由 StructuralValidator 施加更严格限制。

当前 Windows Spike 发现 RapidJSON 1.1.0 原生接受单独低代理项。PAE 因此在候选 Parser 之前执行无堆、只读的代理项转义配对预检；该预检只补足确定的 Strict JSON 规则，不替代完整 JSON Parser。

### 5.2 对象 key

- 所有层级的重复 key 必须在发生覆盖之前拒绝；
- 重复判断基于 JSON 转义解码后的完整字符串，而不是源文本字节拼写；
- 因此`"protocol_id"`和`"\u0070rotocol_id"`属于同一个逻辑 key；
- 包含转义 NUL（空字符）的 key 也必须按完整长度比较，不能使用以 NUL 结尾的 C 字符串比较；
- 对象成员书写顺序不表达覆盖、优先级或执行顺序；
- Unicode 规范等价但码点序列不同的 key 不视为重复；具体 PAE 属性名和稳定 ID 将由 StructuralValidator 限制为约定字符集。

Unknown Field（未知字段）不是 JSON 语法错误，由 StructuralValidator 按版本化 Schema 拒绝。

## 6. Number Token

### 6.1 词法分类

StrictJsonLoader 必须保留足够信息，使 StructuralValidator 至少能够区分：

```text
SIGNED_INTEGER_TOKEN
UNSIGNED_INTEGER_TOKEN
REAL_TOKEN
```

还必须保留原始 Token 字节、正负号、`negative_zero`、`has_fraction`和`has_exponent`，不得先统一转换为`double`再判断 Token 类型。至少必须保证`INT64_MIN`、`INT64_MAX`和`UINT64_MAX`在 Loader、类型化中间表示和校验之间不发生精度丢失。

### 6.2 整数语义属性

offset、width、length、count、bitmask 和 Enum Raw Value（枚举原始值）等整数语义属性：

- 必须使用没有 fraction（小数部分）和 exponent（指数部分）的 JSON Number Token；
- `1.0`和`1e0`即使数学值等于 1，也必须拒绝；
- unsigned（无符号）属性只接受无负号整数 Token；词法`-0`必须由 StructuralValidator 以`INTEGER_OUT_OF_RANGE / NEGATIVE_TOKEN_FOR_UNSIGNED`拒绝，不得自动修复为`0`；
- signed integer（有符号整数）属性允许`-0`，进入 SchemaIr 后规范化为整数`0`；
- 符合 JSON 语法但超出目标属性位宽或数值范围的 Token，由 StructuralValidator 或 DomainValidator 按属性职责拒绝；
- 任何实现都不得通过二进制浮点中转整数；
- 十六进制参数、bitmask 展示和 byte pattern（字节模式）使用规范化 String，例如`"0x1021"`和`"A5 5A"`。

### 6.3 实数语义属性

只有 Schema 明确声明为实数语义的属性才能接受 fraction 或 exponent 形式。Token 语法合法不等于对应 PAE 属性有效。

REAL64 作者值遵循：

- 只接受能够转换为有限 IEEE 754 binary64 的 JSON Number；`NaN`和`Infinity`不是合法 JSON Number；
- 有限非零转换必须使用 locale-independent round-to-nearest-ties-to-even（不依赖区域设置的最近值、正中取偶）；
- `0e999`、`0e-4000`、`-0.0`和其他数学值精确为零的实数 Token 可以接受，但进入 SchemaIr 后统一规范化为正零；
- 非零十进制数不得因下溢静默变为零，返回`REAL_OUT_OF_RANGE / UNDERFLOW`；
- 超出有限 binary64 范围的值不得变为 Infinity，返回`REAL_OUT_OF_RANGE / OVERFLOW`；
- 能够转换为有限非零 subnormal（次正规数）的值允许进入 REAL64；
- 该作者值转换不放宽运行期 Encode 的精确反算约束；精确十进制继续使用`DECIMAL64{coefficient, scale}`。

不设置脱离目标类型的全局指数绝对值上限；资源防护由 Number Token 字节上限承担。对整数属性，`0e999`和`1e999`仍属于 REAL Token，均以`INTEGER_NOT_EXACT`拒绝。

### 6.4 尚未冻结的 DECIMAL64 作者细节

以下内容仍不由本 Profile 静默决定：

- DECIMAL64 的文本规范化与舍入策略。
- JSON REAL Token 是否允许直接初始化 DECIMAL64；若未来允许，必须精确落入`int64_t coefficient + scale 0..18`，不得通过 binary64 中转。

### 6.5 稳定数字诊断

| 场景 | 阶段 | 稳定错误码 |
| --- | --- | --- |
| Number Token 超过字节上限 | Loader Resource（加载资源） | `NUMBER_TOKEN_LIMIT` |
| REAL Token 用于整数属性 | Structural | `INTEGER_NOT_EXACT` |
| unsigned 属性收到负号 | Structural | `INTEGER_OUT_OF_RANGE`，原因`NEGATIVE_TOKEN_FOR_UNSIGNED` |
| REAL64 溢出或非零下溢 | Structural | `REAL_OUT_OF_RANGE`，原因`OVERFLOW`或`UNDERFLOW` |
| 已转换数值违反字段或业务约束 | Domain | 对应领域约束错误 |

Structural 和 Domain 数字错误必须携带准确 RFC 6901 JSON Pointer；没有稳定 byte offset 时显式缺省。Parser 私有错误、Infinity 或下溢后的零不得泄漏为 PAE 结果。

## 7. 确定性和外部状态

同一输入、同一 PAE 版本、同一资源 Profile 和同一 Extension Registry（扩展注册表）必须得到相同的接受/拒绝结论。V0.1 权威 JSON 不支持：

- include/import（包含/导入）指令；
- 环境变量替换；
- 文件路径自动搜索；
- 模板表达式或脚本求值；
- 依赖对象成员书写顺序的覆盖；
- 根据操作系统、Locale 或当前工作目录改变配置语义。

未来如增加 Loader 前置转换，转换结果必须成为单一 Authoring Authority（编写权威源）的确定产物，并经过相同 Strict JSON 与 Compiler 门禁。

## 8. 资源上限

生产 StrictJsonLoader 必须具有有限且可查询的：

- 输入总字节数；
- 嵌套深度；
- 解析树节点数；
- 单 Object 成员数；
- 单字符串字节数；
- 单数组元素数；
- Number Token 字节数；
- 全部解码字符串字节数；
- Parser arena（解析器固定内存区）字节数；
- Loader/Compiler 峰值临时内存。

### 8.1 已确认结构上限

| 项目 | Hard Limit（硬性安全上限） | Desktop Baseline（桌面基线） | Constrained（受限资源档） |
| --- | ---: | ---: | ---: |
| 嵌套深度 | 64 | 32 | 16 |
| 单 Object（对象）成员数 | 4,096 | 2,048 | 256 |
| 单 Array（数组）元素数 | 16,384 | 8,192 | 1,024 |
| 单字符串解码后字节 | 64 KiB | 16 KiB | 4 KiB |
| Number Token 字节 | 128 B | 64 B | 64 B |

正式 Schema 可以针对具体属性设置更小限制，不得突破本表。PAE 必须定义稳定计数语义，不能直接使用第三方 Parser 的内部容量或默认限制作为公共契约。

### 8.2 待生产全链实测与冻结的容量候选

以下数值状态为`CANDIDATE / UNVERIFIED`，只作为最大规模配置生成和测量目标，不是已经验证的生产上限；Parser阶段已完成的边界探针不改变这一生产状态：

| 项目 | Hard 候选 | Desktop 候选 | Constrained 候选 |
| --- | ---: | ---: | ---: |
| 配置输入字节 | 4 MiB | 2 MiB | 512 KiB |
| PAE 逻辑 DOM 节点数 | 524,288 | 262,144 | 32,768 |
| 全部解码字符串字节 | 2 MiB | 1 MiB | 256 KiB |
| Parser arena | 64 MiB | 32 MiB | 8 MiB |
| Loader/Compiler 峰值临时内存 | 128 MiB | 64 MiB | 16 MiB |

候选节点预算采用`24 × 总字段数 + 128 × Message 数 + 全局余量`作为首轮保守估算。当前 yyjson Windows Spike 的固定内存 API 上界约为输入的 13 倍，因此 Parser arena 候选按输入上限的 16 倍设置。两项都必须由正式 Schema 骨架和实际分配计数复核。

不存在“0 表示无限”或回退到第三方默认上限的模式。输入大小和 Parser arena 必须在解析前准入；解析后的树资源必须在进入 SchemaIr（类型化中间表示）前完成审计。

当前 Spike 使用的默认实验值为 256 KiB 输入、64 层、4,096 节点、4,096 B 单字符串、1,024 数组元素和 4 MiB Parser 内存；字段密集型合成用例另行放宽节点和数组限制。这些数值只用于候选对比，不是生产 Profile。尤其是默认 1,024 数组元素和 4,096 节点可能与已经确认的 PAE Message/Field 容量不自洽，生产数值必须在正式 Schema 骨架和大配置测试形成后单独冻结。

### 8.3 生命周期与失败原子性

- Loader 完成 Structural/SchemaIr 构建后必须立即释放 LoadedJsonDocument、Parser DOM 和 Parser arena，再进入 Domain、ResourceBudget 和 PlanBuilder；
- 峰值临时内存预算覆盖 Parser/DOM、重复键集合、JSON Pointer 路径、SchemaIr、校验索引和 Plan staging（计划暂存区）；
- 宿主同步借出的只读源 Buffer 不计入预算；Parser 或 Loader 创建的任何副本必须计入；
- 任一阶段失败时整体释放临时资源，不生成部分 Plan、不产生可查询 Handle，也不替换已经生效的 Plan；
- 超过默认 Profile 的设置必须显式标记为`CUSTOM`，不得超过最终 Hard Limit，并重新通过全部资源门禁；
- 所有长度、节点和内存累计必须检查整数溢出。

五项容量候选最终冻结前，必须为三档分别建立“恰好到上限”和“超限一级”的确定生成配置，固定 Generator ID、实际字节数和 SHA-256，并验证最大 Field/Message 组合、yyjson `yyjson_read_max_memory_usage()`、全链峰值内存、稳定资源诊断、故障注入后零活跃分配以及 Runtime 无部分注册。

## 9. 诊断要求

诊断位置遵循[JSON Loader 诊断定位契约](../docs/json_loader_diagnostic_contract_v0.1.md)：

- BOM、非法 UTF-8、公共深度和代理项配对错误应给出零基、`EXACT_INPUT_BYTE`的 byte offset（字节偏移）；
- 第三方 Parser 语法位置必须标记为`PARSER_REPORTED_POSITION`，不能伪装成跨 Parser 的精确位置；
- Structural 和 Domain 错误使用 RFC 6901 JSON Pointer（JSON 路径指针）；
- Parser 资源耗尽不携带伪造的数据位置；
- 第一版允许只返回一条确定的 Fatal Diagnostic（致命诊断），但错误优先级必须稳定。

对象书写顺序不能改变 Validator 的规则优先级。需要多条错误时必须另行冻结数量上限、排序和截断策略。

## 10. 当前可执行证据

截至 2026-09-01，Windows x64/MSVC Release 的当前技术探针包括：

- yyjson 0.12.0、nlohmann/json 3.12.0 和 RapidJSON 1.1.0 各执行 100 个文档级用例，稳定 ID/来源 Inventory（清单）为 100/100，均为`regression_failures=0 / regression_gate=PASS`；每个候选的清单由 97 个`CONFORMANCE（符合性）`和 3 个`CHARACTERIZATION（现状表征）`组成，当前没有`OPEN_DECISION（待拍板）`，但由于仍有表征用例，文档级`contract_closure_state=OPEN`；
- 新增覆盖合法顶层空白、标准 escape、代理项对、Unicode key 非规范化、JSON Number 完整语法，以及非法 UTF-8、非法空白、原始控制字符、孤立/反向代理项和非标准数字；
- 28个文档级用例已迁入独立TSV（Tab-Separated Values，制表符分隔值）清单，72个仍由C++生成；另有77个单Token词法、元数据、精确值、范围、稳定原因和REAL64位模式用例由独立CTest执行，两类用例均有独立稳定 ID Inventory 防静默删减；独立数字语料摘要为`regression_failures=0 / contract_closure_state=CLOSED`；
- 24 个 JSON Pointer 精确断言继续通过；28项机器清单切片包含12个Pointer精确断言、16个Pointer缺省断言、10个公共预检精确byte offset断言、2个Parser位置种类断言和16个offset缺省断言；
- 5项CMake清单生成器负向变异门禁证明非法offset组合、非法目标锁和缺失`MANIFEST_AUTHORITY`来源状态会在Configure阶段失败；
- yyjson使用`YYJSON_READ_NUMBER_AS_RAW`逐字节保留原始数字，并以PAE自有分类器区分`SIGNED_INTEGER`、`UNSIGNED_INTEGER`、`REAL`和`INVALID`；
- yyjson 固定内存池和解析期 5 个分配点逐点故障注入继续由独立 CTest 覆盖；端到端 Raw Number（原始数字）结构诊断自测覆盖 unsigned `-0`、signed `-0`、REAL64正零、溢出、下溢、最小subnormal、整数精度和Number Token边界共10项，摘要为`failures=0 / gate=PASS`；
- Hard/Desktop/Constrained 三档各执行 depth、Object成员、Array元素、单字符串、Number Token、逻辑节点、全部解码字符串和输入字节 8 个维度的 exact/+1，共48个确定生成输入；三档均为`structural_profile_gate=PASS / candidate_capacity_probe=PASS / failures=0`，并通过 Parser arena容量充分、硬边界和耗尽清理检查；
- yyjson候选的Archive URL/Hash、MIT License、`yyjson.h`和`yyjson.c`范围由锁文件驱动并在Configure阶段复核；Windows Release CTest当前为20/20通过。
- 独立的内部Loader/SchemaIr切片在Windows x64、MSVC下完成Release和Debug构建/CTest；当前Config Compiler合同Runner的22项内部用例通过，包含原始Number Token拒绝`2.0`和unsigned `-0`、稳定ID Golden Snapshot、完整Frame覆盖负例及失败时无部分Plan。

三个表征用例记录`UINT64_MAX + 1`、`INT64_MIN - 1`和 unsigned `-0`的候选旧行为及目标行为。yyjson当前结果为`ready_to_promote=3 / open_contract_gaps=0 / target_lock_failures=0`，已经满足并锁定三个目标，任一回退会使其 CTest 失败；nlohmann/json和RapidJSON当前均为`ready_to_promote=0 / open_contract_gaps=3`。因此绿色CTest只表示 Regression Gate（回归门禁）通过，不能隐藏另外两个 DOM Adapter（文档对象模型适配器）的原始数字保真缺口。

第十三轮数字语义已在 Windows yyjson 推荐路径形成可执行证据：unsigned（无符号）属性拒绝词法`-0`并返回`INTEGER_OUT_OF_RANGE / NEGATIVE_TOKEN_FOR_UNSIGNED`，signed integer 的`-0`规范化为整数零；精确零、REAL64溢出/下溢、有限非零subnormal和round-to-nearest-ties-to-even（舍入到最近值、正中时取偶数）代表边界已进入77项机器语料。该证据基于本机MSVC标准库转换实现，尚不是Linux或最终生产Parser跨平台证据。

三档资源门禁只审计裸JSON输入的公共预检、yyjson有界树和Parser arena。它们仍标记为`CANDIDATE_UNVERIFIED（候选、尚未生产验证）`，`capacity_freeze_state=OPEN / hash_manifest_state=OPEN`；Loader/Compiler峰值为`NOT_MEASURED`，最小SchemaIr和PlanBundle为`BUILT_AND_TESTED_SLICE（最小切片已构建并测试）`，Runtime注册为`NOT_RUN`。因此不能把48项边界通过或最小切片通过写成五项容量已经最终冻结。

上述证据只证明当前 Windows Spike 中三个 Adapter（适配器）在 PAE 公共预检和统一结果模型下对齐。它不证明：

- 任一候选已经成为最终生产 Parser；
- Linux GCC/Clang 行为；该实测已按当前推进顺序暂缓，而不是已经通过；
- 五项容量候选已在正式 PAE Schema 与 Loader/Compiler 全链完成最大规模实测并最终冻结；当前仅有 Parser 阶段生成型探针；
- 完整V0.1 `pae.schema.json`、完整Execution Semantics或协议正确性；当前只有显式标记为不完整的草案切片；
- 模糊测试、目标板、硬件或现场验证。

## 11. 生产实现门禁

在 Windows-first 阶段把本 Profile 转为生产 StrictJsonLoader 前至少还需完成：

1. 冻结最终 Parser、版本、License、Vendor 源文件范围和 SHA-256；
2. 将当前三档 Parser 阶段 exact/+1 生成器迁移到正式 PAE Schema 配置，固定参数、实际字节数和 SHA-256，并实测 Loader/Compiler峰值、最大Field/Message组合、SchemaIr、PlanBundle及失败原子性后最终冻结五项容量；
3. 把剩余72个文档级用例迁入独立于Spike C++源码的合法/非法Strict JSON Conformance Corpus（一致性语料）；当前28个文档用例及77个单Token用例只是阶段性切片；
4. 为所有层级 duplicate key（重复键）、代理项、精确整数边界和资源超限建立稳定诊断断言；
5. 完成 Parser 文档生命周期、故障注入、无泄漏和 Parser 类型隔离门禁；
6. 扩展并冻结完整V0.1 `pae.schema.json`和 ProtocolPlan Execution Semantics；最小草案切片已经形成并进入C++契约测试，但不覆盖完整字段、Framer、Integrity和Runtime能力。

Linux GCC 和 Clang 同语料测试暂不阻塞 Windows 阶段的设计与实现，但在宣称 Linux 支持、跨平台完成或执行既有跨平台发布退出门槛前必须补齐。暂缓执行不放宽设计约束：Portable Core、Loader 公共边界和 CMake 不得引入 Windows 专属 API、平台宽度假设或隐式宿主字节序。

在这些门禁完成前，Parser Spike仍是可丢弃技术探针，内部Loader切片也不得作为稳定公共API、生产协议Core或现场替换依据。

## 12. 修订记录

| 文档版本 | 日期 | 说明 |
| --- | --- | --- |
| 0.1.7 | 2026-09-02 | 更新Config Compiler当前22项合同证据，并明确PlanBuilder目标类型状态与当前防御复核之间的边界 |
| 0.1.6 | 2026-09-01 | 记录最小Schema、SchemaIr和PlanBundle切片及21项内部用例；保持完整V0.1 Schema、峰值和生产容量冻结为OPEN |
| 0.1.5 | 2026-09-01 | 记录77项数字语料、10项yyjson数字诊断、三档48项Parser阶段资源边界和20/20 CTest；保持生产Parser、容量Hash与Loader/Compiler峰值为OPEN |
| 0.1.4 | 2026-09-01 | 记录第十三轮确认的`-0`、REAL64、稳定数字诊断、五项结构上限、五项容量候选及失败原子性；明确现有机器清单尚未按新契约复验 |
| 0.1.3 | 2026-09-01 | 记录第二批Corpus、Number Token、offset、候选来源和Windows-first边界 |
