# 第八批：Schema IR、领域校验与资源预算中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不是提交批准。

## 1. 接管与增量

接管及本批核对基线为 `main@f30b29e6e76e8da1ae1057067fd1ac3bca2e3b15`，暂存区为空。
接管已有第七批四个源码修改：`src/config_compiler/config_compiler.h`、
`src/config_compiler/validation_pipeline_internal.h`、`src/public_api/compiled_state_internal.h`、
`src/public_api/compiler.cpp`；另有未跟踪第七批报告
`docs/engineering/chinese-comments-compiler-entry-validation-20261009.md`。全部保留。

本批只修改两份源码中的注释，并新增本报告：

- `src/config_compiler/schema_ir.h`：自有 IR、局部/全局索引、字节/位单位、默认值与准入边界、
  Validated/Budgeted 的构造权限和移动所有权。
- `src/config_compiler/config_compiler.cpp`：严格输入、DOM 审计、结构解析、领域校验、
  逻辑 Plan 预算及 `CompileJsonToBudgetedSchema()` 的寿命/编排说明。
- `docs/engineering/chinese-comments-schema-ir-validation-20261009.md`：本次独立证据。

没有改非注释 token、声明布局、运行逻辑、Schema、测试、CMake、公共 include、Lab/Qt/Adapter、
旧报告、索引或交付包。未扩展功能；没有继续历史 DEC 实施任务。
PlanDraftAssembler、FreezeBudgetedPlanDraft 和 CompileJsonToPlanImpl 起至文件尾的发布函数区段
与接管副本逐字符一致，留待后续批次。没有以此次专项通过替代这些实现的完整审查。

## 2. 注释依据与解释边界

### 自有 IR 与阶段凭证

完整读取 `schema_ir.h`，并核对实际解析与领域校验用法：string/vector 保存自有数据，
JSON Pointer 用于配置定位，不保存 yyjson 节点。默认值只是初始化，不自动赋予 Schema 能力。
Wire 的 byte_offset/byte_width 与 bit_offset/bit_width 单位不同；bit_container_index、
payload_field_index 和文本 field_index 引用 Message 内数组，未解析值为 size_t(-1)。
ResolvedPipelineIr 的 framing/message 索引则引用包级数组，保留配置中消息引用顺序。

ValidatedSchemaIr 正常链路由 DomainValidator 完整成功后发布，私有构造及 friend 限制入口；
move-only Payload 一起拥有 IR、解析引用和 ResourceRequirements。需求统计不等于预算批准。
BudgetedSchemaIr 再保留验证产物、PlanMemoryReport 和批准上限；此时尚未组装或冻结 Plan。
移动后的空凭证不能重复作为有效阶段输入，不将这种权限边界描述为对任意内部友元误用的安全防护。

### 严格输入、结构与领域

核对 `RunStrictPrecheck()`、`AuditJsonValue()`、字符串/精确整数读取器、`BuildSchemaIr()`、
`ValidateMessageDomain()`、`DomainValidator::Validate()` 及对应 ASCII 分支。

- 预检限制输入、UTF-8、嵌套及代理对，不替代 yyjson 的完整 JSON 语法解析。
- 数字保留 raw Token；unsigned 拒绝负号，包括 -0；signed 通过无符号幅值安全处理
  INT64_MIN，并将 -0 归一为 0，不先转浮点。
- DOM 审计限制节点/容器/字符串资源；解码后的对象键也计入字符串预算，重复键先拒绝。
- 属性白名单、必需属性、类型和版本属于结构门禁；多个未知属性按字典序选诊断位置。
- Schema 版本与当前编译开关共同决定准入。ASCII 使用独立消息分支，不是所有 Binary
  能力的机械并集；单字节位容器必须省略 byte_order，多字节必须显式声明。
- 领域层解析引用与方向，检查字节/位范围、重叠、完整覆盖、校验存储及常量与 Matcher 冲突。
  位容器只按常量成员和 base_value 保留位检查 Encode 确定位；动态覆盖位不按 base_value
  判冲突，也不把 base_value 变成 Decode 保留位约束。
- 同 Pipeline 的 Binary Matcher 交集检查不借完整性或字段语义消歧。
  `FixedMatchersCanIntersect()` 对 ASCII 返回 false，因此不能将这条编译检查泛化为
  所有 ASCII 消息均已证明结构唯一；运行时候选语义不在本批修改范围。
- ASCII 文本引用/长度、变长字段 literal 边界以及 CRLF 静态证明根据当前分支解释，
  不调用运行时 Decode 来证明配置。

`CompileJsonToBudgetedSchema()` 中解析池先构造、Document 后构造，离开局部作用域时
Document 先释放。字符串和数组已复制到 IR，之后领域/预算阶段不再借用 DOM 或输入字节。
这是源码寿命分析，不是所有内存分配失败点的动态证明。

### 资源预算

核对 `EstimateSchemaPlanMemory()`、`ResourceBudgetValidator::ValidateImpl()`，并只读核对
`src/protocol_plan/plan_memory.h/.cpp` 的乘法、加法和对齐检查。
估算按冻结类别、数量、sizeof 和对齐累计；包含容器、字段、枚举、执行描述、候选索引及
启用分支的扩展。Pipeline 允许位图按包级消息总数分配，固定长度候选按长度分组。
预算阶段先检查档位计数/尺寸，再估算需求并对照批准上限；测试替换预算仍受硬上限约束。

JSON parser pool 上限、解码字符串预算、不可变 Plan 预算和 metadata 独立计费不能混称。
估算过程中临时 map/set、可变 IR、Workspace 及进程 RSS 不属于这份冻结 Arena 报告；
不宣称已完成全链峰值计费或所有资源边界安全证明。

## 3. 等价、保护与格式

两文件接管副本位于忽略目录 `out/build/comments-public-20261009/schema-ir/baseline/`。
验证脚本保留字符串/字符字面量，仅忽略注释和空白比较 token；遇不支持的 raw string 会
失败关闭，本批两文件未含 raw string。非注释 token 与接管相同，宏续行文本相同，
没有注释尾反斜杠。结合实际 diff 审查确认未改实现。

初次等价脚本误把 CompileJsonToPlanImpl 定位锚点写成 static 函数，报
`Protected function anchor missing` 并失败关闭。只修正 out 中验证脚本后全部检查通过；
初次失败保留于 `comment-equivalence.log`，通过结果见 `comment-equivalence-corrected.log`。
没有把脚本定位错误当成产品缺陷或隐藏失败记录。

3241 个非本批 tracked 文件及第七批未跟踪报告，共 3242 个保护文件 SHA256 未变。
禁止修改的组装/冻结/发布区段逐字符一致。两个目标文件 clang-format dry-run 接管及最终
均退出 0；遵循仓库 Google/C++17/100 列配置，没有执行全文件 reformat。
源码及本报告严格 UTF-8 无 BOM；git diff --check 和新增候选便携路径检查通过。

## 4. 本次 Windows 专项

复用 `out/build/comments-public-20261009/default-repo`，核对当前仓库源根、
Visual Studio 18 2026、x64、v142 14.29.30133，testing/public API 开启，Schema 0.5～0.11
对应可选开关开启。Release 使用 `/O2 /Ob2 /DNDEBUG`。未修改或重新配置构建设置。

严格按 Debug build/test → Release build/test 执行，四条命令退出码均为 0。
每配置 CTest 2/2 通过：

| CTest | Debug 实际用例 | Release 实际用例 | 直接证据 |
| --- | --- | --- | --- |
| pae.config_compiler.contract | 122 passed / 0 failed，expected=122 | 同左 | 严格输入、结构/领域拒绝、阶段凭证、预算与固定快照 |
| pae.config_compiler.ui_description | 69 passed / 0 failed | 同左 | 解析资源、metadata 计费与完整产物失败关闭 |

代表性断言映射（测试源码未修改，实际输出保存在 cases 两配置日志）：

- `input_utf8_bom`、`input_invalid_utf8`、`json_duplicate_key`、`json_trailing_comma`：
  输入/资源/语法阶段及对应错误；`RunFailureCase()` 检查失败且没有部分 Plan。
- `structural_unknown_property`、`structural_missing_property`、`structural_type_mismatch`、
  `structural_integer_fraction`、`structural_unsigned_negative_zero`：检查结构错误及配置位置。
- `domain_duplicate_id`、`domain_missing_framing_reference`、`domain_direction_mismatch`、
  `domain_field_overlap`、`domain_frame_not_fully_defined`、`domain_constant_matcher_conflict`、
  `domain_ambiguous_matcher`：检查领域阶段、错误码和精确 JSON Pointer。
- `capability_single_consumption_internal_violation`：重复消费 moved-from 凭证拒绝；
  类型权限另有 static_assert，不依赖运行时布尔标记。
- `plan_memory_report_accounting`、`plan_memory_budget_exact_boundary`、
  `plan_memory_failure_injection_no_leak`、`plan_memory_estimate_mismatch`：分类求和、
  精确上限/减一、代表性冻结分配失败与批准报告不一致拒绝。
- metadata 专项的解析资源与独立描述预算、`probe_stages_once`、
  `plan_freeze_failure_no_partial_publish`、`audit_failure_no_partial_publish`：
  支持阶段和失败清理说明，不升级为所有分配点、所有 Schema 组合的覆盖证明。

Runner 通过运行时条件、失败计数和非零退出码检查；compiler contract 同时检查预期用例数。
static_assert 在 Release 编译也保留，未用仅普通 assert 的 Release 运行冒充有效验证。
用例数不等于覆盖率，未新增或改写历史期望快照。

实际命令（完整参数/退出码见 run-validation.ps1 与 validation-transcript.log）：

```powershell
$Build = 'out/build/comments-public-20261009/default-repo'
cmake --build $Build --config Debug --target pae_config_compiler_contract_tests pae_config_compiler_ui_description_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^pae\.config_compiler\.(contract|ui_description)$' --output-on-failure
# Debug 完成后，以相同目标和过滤器执行 Release。
```

证据均在 `out/build/comments-public-20261009/schema-ir/`：

- `takeover-status.log`、`baseline/`、`protected-hashes.json`：接管与文件保护。
- `verify-comments.ps1`、`comment-equivalence*.log`、`final-comment-equivalence.log`：等价与保护。
- `baseline-format-*.log`、`final-format-*.log`：格式前后结果。
- `run-validation.ps1`、`validation-transcript.log`、`build-{Debug,Release}.log`、
  `test-{Debug,Release}.log`、`cases-{Debug,Release}.log`：本次真实执行结果。
- `diff-check.log`、`candidate-portable.log`、`final-audit.log`：最终候选与 Git 核对。

## 5. 局限与停点

未发现需要在本批注释范围修复的新阻断；本批仍待总控独立复核。
未验证所有构建开关组合、全链峰值内存、任意 OOM/线程交错、所有损坏内部 IR、
完整 Schema 领域审查或全部能力矩阵。Plan 冻结实现留待后批，不因现有相关测试通过而提前收口。
未跑无关全仓、UI、网络、SDK 包外消费或重打包、Linux、硬件和真实协议；
不新增生产、稳定 ABI、性能或现场验收结论。

最终候选为累计六个 tracked 修改及两份 untracked 报告；第七批五文件原样保留。
未 Stage、Commit、Push、历史改写、清理、删除、发布或派发其他任务。
本次报告不改写第七批历史验证；完成一次总控交接后停止写入，等待复核。
