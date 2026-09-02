# JSON Parser Spike 机器可读语料 V0.1

本目录是 JSON Parser Spike（JSON 解析器技术探针）的独立机器可读语料入口。它只服务于可丢弃的候选比较代码，不是正式 `pae.schema.json`，也不是协议报文 Golden Vector（黄金测试向量）。

## 1. 当前文件

- `limits_v0.1.tsv`：独立保存 Spike 的资源限制，不从被测 Adapter（适配器）默认值反推边界；
- `resource_profiles_v0.1.tsv`：保存 Hard/Desktop/Constrained 三档 Parser 阶段候选容量和未测的 Loader/Compiler 峰值目标；
- `strict_json_corpus_v0.1.tsv`：28 个文档集成用例，覆盖 Number Token（数字词法单元）、空输入/空白、编码预检和首批重复 key；
- `strict_json_inventory_v0.1.tsv`：冻结全部 100 个文档级 `case_id`及其语料来源状态，防止迁移时静默漏项；
- `number_token_corpus_v0.1.tsv`：77 个单 Token 词法分类、精确整数、REAL64 位模式和稳定原因用例；
- `number_token_inventory_v0.1.tsv`：冻结 77 个单 Token `case_id`，防止语料被静默删减。

TSV（Tab-Separated Values，制表符分隔值）只允许固定列、ASCII 元数据和 LF 换行。原始输入、JSON Pointer（JSON 路径指针）及 Token 使用大写 Hex（十六进制）保存，避免文本模式、编辑器编码和 NUL 字节改变测试输入。

## 2. 信任边界

候选 yyjson、nlohmann/json 和 RapidJSON 不读取这些清单。CMake 在 Configure（配置）阶段使用独立的固定列解析逻辑校验它直接读取的 TSV，再生成只读 C++ 记录。该阶段覆盖：

- 表头和列数；
- ID、枚举、十六进制和十进制语法；
- 重复 `case_id`；
- TSV 内部 ID 集，以及 `MANIFEST_AUTHORITY`文档用例与 Inventory（清单）的双向一致性；
- 输入字节数；
- `limits_id` 引用；
- `CONFORMANCE`、`CHARACTERIZATION`和`OPEN_DECISION`状态约束；
- Pointer 与 byte offset（字节偏移）断言组合。

完整的 100 个文档级用例还包含 72 个由 C++ 生成的 `LEGACY_EMBEDDED`用例。Runner（运行器）启动时把生成后的完整用例集与`strict_json_inventory_v0.1.tsv`逐项核对，校验总量、稳定 ID、唯一性和`LEGACY_EMBEDDED / MANIFEST_AUTHORITY`来源状态。因而，完整 Inventory 门禁是 CMake 静态校验与 Runner 运行期校验共同完成的，不应表述为仅由 Configure 阶段覆盖。

Configure 阶段另有 5 项独立 Negative Mutation Gate（负向变异门禁），证明以下非法清单不会被静默接受：`EXACT + PARSER_REPORTED_POSITION`、越界精确 offset、`KIND_ONLY + NONE`、非`CHARACTERIZATION`目标锁，以及缺失`MANIFEST_AUTHORITY`来源状态。

预期结果不得由三个候选的多数结果自动生成。运行结果只写入 `results/`报告，不回写语料。

## 3. 状态语义

- `CONFORMANCE（符合性）`：实际 code、Pointer、offset、词法类别和精确值必须满足清单；不匹配使 Regression Gate（回归门禁）失败。
- `CHARACTERIZATION（现状表征）`：`expected_code`记录尚未满足契约的候选现状，`target_code`记录目标。命中目标输出 `ready_to_promote`，命中旧行为继续计入 `open_contract_gaps`，二者之外视为非预期变化并失败。
- `OPEN_DECISION（待拍板）`：记录当前稳定行为，但不把它升格为生产契约。第十三轮数字决策落实后，当前清单中已没有该状态；状态类型仍保留给后续真正未决事项。

Runner（运行器）把两种门禁明确分开：

- Regression Gate：`CONFORMANCE`满足唯一预期，`CHARACTERIZATION`命中旧行为或目标行为，`OPEN_DECISION`保持当前记录，均可通过；
- Contract Closure State（契约闭合状态）：只要回归门禁失败，或仍存在`CHARACTERIZATION`或`OPEN_DECISION`，就保持`OPEN`，不得把绿色 CTest 解释为生产契约已闭合。当前它是机器可读摘要状态，不单独决定 CTest 退出码。

逐用例结果使用`CONFORMANT`、`OBSERVED_STABLE`、`TARGET_REACHED`、`OPEN_STABLE`或`UNEXPECTED_CHANGE`，并单独输出`regression_gate_passed`，不再使用含义模糊的通用`pass`字段。

`target_lock_candidate`只允许用于`CHARACTERIZATION`。当某个候选已达到目标后，可锁定该候选必须继续命中`target_code`，同时让其他候选继续保留旧行为表征；当前 yyjson 的两个整数越界目标和 unsigned `-0`拒绝目标均已锁定。yyjson 主用例仍执行 Raw Number 逐字节保真自测，并另有 10 项数字结构诊断自测；独立 77 项 Number Token 语料锁定词法类别、状态、稳定原因、整数值和 REAL64 IEEE 754（浮点数表示标准）位模式。

## 4. 当前边界

现有 100 个文档级用例中，28 个已从 `BuildFixtures()`迁入 TSV；其余 72 个仍由 C++ 生成。新增切片包含 2 个 Parser 位置种类断言、10 个公共预检精确字节偏移断言，以及 2 个根级重复 key 的精确 JSON Pointer 和 offset 缺省断言。Inventory（清单）只防止用例或来源状态在迁移中静默丢失，不使 72 个内嵌用例自动获得独立语料权威。

资源门禁由稳定 Generator ID（生成器标识）`pae-json-resource-v0.1/<profile>/<dimension>/<boundary>`确定生成，每档覆盖 depth、object members、array elements、single string、number token、nodes、total decoded strings 和 input bytes 的 exact/+1，共 48 项。当前尚未建立生成结果 SHA-256（Secure Hash Algorithm 256-bit，256 位安全散列算法）清单，而且这些输入是裸 JSON Parser 审计语料，不是正式 PAE Schema 配置；因此`hash_manifest_state`和容量冻结状态都保持`OPEN`。嵌套重复 key、PoC（Proof of Concept，概念验证）形态和正式 Loader/Compiler 组合仍未完成，不能把本目录称为完整生产 Conformance Corpus（一致性语料）。

后续迁移静态二进制输入时，应使用 `.bin`文件并在 `.gitattributes`中标记为 `-text`；生成型边界在升级为生产权威前还需冻结参数、输出字节数和 SHA-256，并改用正式 PAE Schema 配置覆盖 Loader/Compiler 全链。
