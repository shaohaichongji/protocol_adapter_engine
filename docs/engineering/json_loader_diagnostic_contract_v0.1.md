# JSON Loader 诊断定位契约 V0.1（Spike Draft）

## 1. 状态与边界

本文定义 JSON Loader（JSON 加载器）技术探针当前采用的最小诊断定位契约。位置对象和公共 API（Application Programming Interface，应用程序编程接口）布局状态仍为`SPIKE DRAFT（技术探针草案）`；第十三轮确认的数字错误阶段、稳定错误码名称和原因属于`CONFIRMED（已确认）`产品语义。错误码数值、C ABI 布局和所有权尚未冻结。

本契约只覆盖配置文本的语法、结构和领域校验位置，不覆盖协议报文 Decode/Encode（解码/编码）诊断、Transport（传输层）或现场硬件状态。

## 2. 位置模型

单条诊断可以同时携带以下两个互相独立的位置：

- `byte_offset`：输入 UTF-8 字节序列中的零基偏移；
- `json_pointer`：符合 RFC（Request for Comments，请求评议） 6901 的 JSON Pointer（JSON 路径指针）。

任一位置无法稳定确定时必须显式缺省，不能用`0`、根节点或猜测值代替。

### 2.1 Byte Offset

`byte_offset`存在时必须同时携带`byte_offset_kind`：

| `byte_offset_kind` | 含义 |
|---|---|
| `EXACT_INPUT_BYTE` | PAE 公共预检器确定的违规输入字节，语义跨 Parser 稳定 |
| `PARSER_REPORTED_POSITION` | 第三方 Parser 报告的停止位置或错误位置；保持零基表示，但候选之间不要求数值相同 |
| `NONE` | 没有可声明的字节位置，此时`byte_offset`必须缺省 |

当前公共预检中的 UTF-8、UTF-8 BOM（Byte Order Mark，字节顺序标记）、Unicode代理项配对和容器深度错误使用`EXACT_INPUT_BYTE`。其他 JSON 语法错误使用`PARSER_REPORTED_POSITION`。正式 Loader 若需要把语法位置升级为完全跨 Parser 一致，必须另行实现并验证统一词法定位器，不能只重命名第三方 Parser 的位置。

### 2.2 JSON Pointer

结构错误、领域错误和解析树资源上限错误优先使用`json_pointer`：

- 根节点使用 RFC 6901 定义的空字符串；人类可读输出显示为`<root>`；
- 对象成员 token（路径片段）中的`~`编码为`~0`；
- 对象成员 token 中的`/`编码为`~1`；
- 数组元素使用十进制索引，例如`/messages/0/fields/2`；
- missing field（缺失字段）指向本应存在的位置，例如`/protocol_id`；
- cross-field constraint（跨字段约束）指向承担主要违规责任的字段，并由稳定错误码补充另一字段的关系；
- duplicate key（重复键）指向重复出现的逻辑键，而不是第二个键在源文本中的推测字节位置。

JSON Pointer 面向解析后的逻辑名称，因此`"protocol_id"`和`"\u0070rotocol_id"`都定位为`/protocol_id`。

## 3. 错误类别与位置要求

| 错误类别 | `byte_offset` | `json_pointer` |
|---|---|---|
| 输入字节、BOM、UTF-8、公共深度预检 | 必须，`EXACT_INPUT_BYTE` | 通常缺省 |
| JSON 语法错误 | Parser 可提供时携带，`PARSER_REPORTED_POSITION` | 缺省 |
| 重复键 | 不强制 | 能从逻辑树上下文稳定确定时携带 |
| 结构、类型、Number Token目标类型/精度/范围和领域约束错误 | 不强制 | 必须 |
| 解析树节点、Object成员、字符串、数组、Number Token或深度上限 | 不强制 | Parser 已生成逻辑树且路径稳定时必须 |
| Parser allocator（内存分配器）资源耗尽 | 必须缺省 | 必须缺省 |
| 内部契约错误 | 无稳定位置时缺省 | 无稳定位置时缺省 |

资源错误不得沿用 Parser 当时的扫描位置，因为该位置取决于具体分配策略，不是配置数据本身的稳定违规位置。

### 3.1 已确认数字错误

| 场景 | 阶段 | 稳定错误码 | 位置要求 |
| --- | --- | --- | --- |
| Number Token超过字节上限 | Loader Resource | `NUMBER_TOKEN_LIMIT` | byte offset缺省；解析树路径稳定时JSON Pointer必须 |
| REAL Token用于整数属性 | Structural | `INTEGER_NOT_EXACT` | JSON Pointer必须 |
| unsigned属性收到负号，包括`-0` | Structural | `INTEGER_OUT_OF_RANGE`，原因`NEGATIVE_TOKEN_FOR_UNSIGNED` | JSON Pointer必须 |
| REAL64溢出 | Structural | `REAL_OUT_OF_RANGE`，原因`OVERFLOW` | JSON Pointer必须 |
| 非零REAL64下溢为零 | Structural | `REAL_OUT_OF_RANGE`，原因`UNDERFLOW` | JSON Pointer必须 |
| 已转换数值违反字段或业务约束 | Domain | 对应领域约束错误 | JSON Pointer必须 |

`-0`是合法JSON，不能报告`SYNTAX_ERROR`；`1e999`用于REAL64时属于范围错误，不能报告Parser资源错误或内部错误。上述名称是稳定语义标识，不代表已经分配公共枚举数值。

## 4. 错误优先级

当前技术探针采用以下优先级：

1. 输入总字节数上限；
2. BOM、UTF-8 和容器深度公共预检；
3. Parser 语法与解析期 allocator；
4. 有界树审计，包括解析树节点、字符串、数组、深度和重复键；
5. 根结构、未知字段、必填字段和类型；
6. Number Token目标类型、精度与范围；
7. 引用和跨字段语义。

同一输入同时存在多个问题时，只保证返回上述执行顺序首先遇到的确定错误。配置作者修复该错误后，后续错误才可能显现。

当前 Spike 尚未证明树资源错误和重复键之间存在跨 Parser 统一的全局优先级。在正式复合错误语料冻结前，二者归入同一个`BOUNDED_TREE_AUDIT（有界树审计）`阶段；生产 Loader 必须使用确定遍历顺序，但不能把当前候选的检测先后静默升级为公共契约。

## 5. 当前验证证据

截至 2026-09-01，Windows x64、MSVC Release 实际运行结果为：

- yyjson 0.12.0、nlohmann/json 3.12.0 和 RapidJSON 1.1.0 各执行 100 个文档级用例，均为 0 个非预期失败；当前包含3个`CHARACTERIZATION（现状表征）`和0个`OPEN_DECISION（待拍板）`，unsigned `-0`已经成为带yyjson Target Lock（目标锁）的表征用例；
- 其中 24 个用例对 JSON Pointer 做精确字符串断言，覆盖根节点、缺失字段、类型、整数、引用、跨字段约束、重复根键、数组路径以及`~`和`/`转义；28项机器清单切片包含12个Pointer精确、16个Pointer缺省、10个公共预检精确byte offset、2个Parser位置种类和16个offset缺省断言；
- 公共预检补充Unicode代理项配对后，三个候选均拒绝孤立高/低代理项和反向代理项；该补充源于RapidJSON 1.1.0原生接受孤立低代理项的实测差异；
- yyjson 专属 allocator 自测覆盖固定池容量拒绝、第一次和第二次分配失败、包含 realloc（重新分配）的全部 5 个解析期分配点逐点注入，以及未命中注入点后的成功路径；
- 独立Number Token机器语料共77项，全部为`CONFORMANCE（符合性）`并通过，机器摘要为`contract_closure_state=CLOSED`；
- 5项清单生成器负向变异门禁证明非法offset组合、非法目标锁和缺失来源状态会在Configure阶段失败；
- yyjson另执行10项Raw Number（原始数字）到Structural Diagnostic（结构诊断）的端到端自测，覆盖 unsigned/signed `-0`、REAL64正零、溢出、下溢、最小subnormal、整数精度和Number Token边界，结果`failures=0 / gate=PASS`；
- Hard/Desktop/Constrained三档共执行48个Parser-stage-only（仅解析阶段）exact/+1生成输入，以及每档3项Parser arena检查，均通过；
- Parser Spike的CTest（CMake Test，CMake 测试驱动）当前20/20通过；另一个内部Config Compiler合同Runner当前22项C++用例已经通过，新增完整Frame覆盖负例，但不改变本节Parser探针统计口径。

三个表征用例为`UINT64_MAX + 1`、`INT64_MIN - 1`和 unsigned `-0`，目标类别均为`INTEGER_OUT_OF_RANGE`。yyjson使用`YYJSON_READ_NUMBER_AS_RAW`和PAE自有精确分类器后，当前摘要为`ready_to_promote=3 / open_contract_gaps=0`；nlohmann/json和RapidJSON仍各自报告`open_contract_gaps=3`。当前Runner允许表征用例命中旧行为或目标行为，但其他结果仍会失败，避免绿色CTest掩盖候选差异。

上述证明第十三轮数字语义和三档资源边界已经进入Windows yyjson技术探针并取得可执行证据。资源证据范围仅为公共预检、yyjson有界树审计和Parser arena：Profile仍是`CANDIDATE_UNVERIFIED（候选、尚未生产验证）`，容量和SHA清单保持`OPEN`，Loader/Compiler峰值未测。另一个内部切片已构建并测试最小SchemaIr和PlanBundle，但Runtime注册仍未执行，且该最小切片不能替代最大规模全链资源证据。Linux GCC/Clang已按Windows-first（Windows优先）实施顺序暂缓；最终Parser、完整V0.1 JSON Schema、稳定Loader API、恶意输入模糊测试和目标设备资源表现仍未验证。

## 6. 尚未冻结的事项

- 生产诊断对象的字段宽度、所有权和 C ABI（C Application Binary Interface，C 应用二进制接口）表示；
- 是否为所有嵌套 duplicate key 提供完整 JSON Pointer；当前跨候选硬门禁只覆盖根级重复键；
- 第三方 Parser 语法位置是否保留候选语义，或增加统一词法定位器；
- 一次 Loader 操作返回单条错误还是受上限约束的多条诊断；
- 五项容量候选的最终生产数值；输入、节点、全部解码字符串和Parser arena已有Parser阶段生成/容量证据，最小切片已有代表性失败原子性测试，但完整Schema最大配置、SHA清单、Loader/Compiler峰值和全阶段故障注入仍未验证；
- DECIMAL64作者文本规范化、JSON REAL Token是否允许直接初始化DECIMAL64及其舍入规则；
- 其余72个文档级用例和尚未覆盖的精确公共offset尚未完成机器清单迁移；Parser阶段生成型资源边界已经执行，最小Schema切片已经建立，但完整V0.1 Schema配置、SHA清单和Loader/Compiler最大规模语料尚未建立。

## 7. 修订记录

| 日期 | 说明 |
| --- | --- |
| 2026-09-02 | 更新Config Compiler当前22项合同证据；Parser探针20/20统计口径和历史21项记录保持不变 |
| 2026-09-01 | 同步最小Loader/SchemaIr/PlanBundle切片的21项内部用例，并保持Parser探针与生产容量证据边界分离 |
| 2026-09-01 | 记录77项数字语料、10项yyjson数字诊断、三档48项Parser阶段资源边界和20/20 CTest；明确Loader/Compiler峰值仍未测 |
| 2026-09-01 | 同步第十三轮稳定数字错误、关闭`-0`和超大REAL归属待定项，并明确现有16/16尚未按新契约复验 |
