# 第七批：配置编译主线中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核，不是提交批准。

## 1. 接管与本批文件

接管时及交付时均为 `main@f30b29e6e76e8da1ae1057067fd1ac3bca2e3b15`，暂存区为空。
接管工作树干净，未发现 cmake/MSBuild/ctest 构建占用；未继续此前编码修复任务。
本批只修改四个实现侧文件的注释并新增本报告：

- `src/public_api/compiler.cpp`：文件职责、诊断映射、查询投影、公开编译及结果发布主线。
- `src/public_api/compiled_state_internal.h`：共享冻结产物、RAII 引用、Adopt/Acquire 与线程边界。
- `src/config_compiler/config_compiler.h`：Plan-only 与带 metadata 结果、借用与 Take 前置条件。
- `src/config_compiler/validation_pipeline_internal.h`：move-only 阶段凭证与顺序约束。
- `docs/engineering/chinese-comments-compiler-entry-validation-20261009.md`：独立证据报告。

没有修改公开 include 头、config_compiler.cpp、Schema IR、metadata/Plan 实现、CMake、测试、
Lab/Qt/Adapter、索引、旧报告或交付包。未逐行机械注释所有访问器，未全文件 format。
接管四文件副本保留在忽略的 `out/build/comments-public-20261009/compiler-entry/baseline/`；
其余 3239 个 tracked 文件逐一 SHA256 核对未变化。out 不纳入源码候选。

## 2. 实际链路及注释依据

公开 `CompileProtocolJson()` 调用 `CompileJsonToPlanWithMetadata()`，实际顺序为：

1. 严格输入预检；有界 JSON parser、JSON 资源审计及结构 SchemaIr。
2. DomainValidator 接管 IR，发布 ValidatedSchemaIr；ResourceBudgetValidator 批准 Plan 预算。
3. 基于 BudgetedSchemaIr 先构造 metadata（包括描述内存门禁）。
4. PlanDraftAssembler 接管预算凭证，组装 BudgetedPlanDraft。
5. FreezeBudgetedPlanDraft 调用 PlanBuilder::Freeze，发布冻结 Plan。
6. ProtocolMetadataBuilder::Audit 核验 metadata 与 Plan，再组合完整自有 artifacts。
7. 公开层一次性 TakeArtifacts，构造 Impl/CompiledState，发布 CompiledProtocol。

依据：只读核对 `src/config_compiler/config_compiler.cpp` 的 `CompileJsonToBudgetedSchema()`、
`CompileJsonToPlanImpl()`、`CompileJsonToPlanWithMetadata()` 与 `FreezeBudgetedPlanDraft()`。
内部 `CompileJsonToPlan()` 是 Plan-only 路径，不构造或发布 metadata，不与公开完整产物混称。
只纠正该内部入口原英文“V0.1 vertical slice”的陈旧范围说明为 internal compiler entry points；
当前实现已有多代条件能力，但本批不扩展任何能力或接口。

公开默认 metadata 上限在入口按 DESKTOP 推导，不能注释成自动按输入配置 resource profile
选择该默认值。物理与 metadata 查询是冻结事实投影，不读取 Frame、不执行 Codec，
不把索引/方向能力解释为实际匹配或业务成功。

### 结果与异常

工厂正常构造时，内部 Plan-only 结果为 Plan 或诊断二选一，完整结果为 artifacts 或诊断二选一。
`TakeArtifacts()`、`TakeCapability()`、`TakeDiagnostic()` 解引用 optional，必须按成功/失败状态
选择对应提取路径；Take 只移动 contained value，并不 reset optional 标记。
因此提取后再次检查内部 `Succeeded()` 不能证明仍有完整产物，不能重复消费。
公开 CompileResult 的自定义移动/TakeCompiled 则清除源成功标记，与内部行为明确区分。
`CompiledProtocolArtifacts` 的描述计费是构造时快照，拆取 Plan/描述后不代表仍拥有同量存储。

内部顶层捕获 bad_alloc/其他阶段异常并尝试生成失败诊断；诊断构造仍可能分配。
公开 MapDiagnostic 的字符串复制和 facade/state 分配也可能抛异常，入口并非 noexcept。
公开声明与实现的该边界一致，本批未发现需修复的公开异常契约冲突，未承诺 OOM 下永不抛出。

### 拥有、借用与线程

CompiledState 一起拥有冻结 Plan 和 metadata，不保留原 JSON 字符串。
CompiledStateRef 复制增加引用，移动转移指针，析构释放；Adopt 接管已有一次引用，
不额外 Retain，不能重复接管同一次引用或接管栈对象。Acquire 从有效 facade 复制强引用，
执行对象可保留冻结状态，但不延长外部 facade 对象寿命。
atomic 引用计数不等于同一 Ref/CompiledProtocol/Workspace 的任意操作线程安全，
不允许 Acquire 与同一个外部 owner 的移动/销毁并发。
metadata 字符串、内部 artifacts 引用和 get() 裸指针仍是借用，不因结构复制而拥有数据。
这些限制为源码静态核对，不声称测试穷尽了所有寿命误用或引用计数溢出边界。

## 3. 注释等价与格式

四个文件与实际接管副本的非注释 token 序列完全相同；保留字符串/字符字面量，
宏续行数量不变且无新增注释尾反斜杠。脚本遇不支持的 raw string 语法会失败关闭；
本批四文件未含 raw string。结合 diff 审查确认只改注释，未改声明、布局或执行语义。
四文件及报告严格 UTF-8、无 BOM，git diff --check 与候选便携路径检查通过。

clang-format dry-run 前后对照：

| 文件 | 接管退出码 | 本批退出码 | 差异 |
| --- | --- | --- | --- |
| compiler.cpp | 0 | 0 | 无 |
| compiled_state_internal.h | 0 | 0 | 无 |
| validation_pipeline_internal.h | 0 | 0 | 无 |
| config_compiler.h | 1 | 1 | 原四处诊断保持一致 |

原四处分别是 include 排列、artifacts 构造声明、Success 参数续行及返回构造长行。
移除诊断路径/行号后，前后输出完全一致；不将这些原差异追溯改写为通过，也不借注释任务
改代码排列。未出现本批新增格式差异，剩余历史格式问题保留供总控决定。

## 4. 本次 Windows 验证

复用 `out/build/comments-public-20261009/default-repo`，先核对 CMakeCache：源根为当前仓库，
Visual Studio 18 2026、x64、v142 14.29.30133，PAE testing/public API 开关开启。
没有新增或修改构建配置源码，没有重跑配置/全仓测试或重新打包。
单进程按 Debug build/test → Release build/test 串行执行，四条命令退出码均为 0。

所选 7 个 CTest 均通过，Debug 7/7、Release 7/7：

| CTest | 每配置运行时通过计数 | 直接相关证据 |
| --- | --- | --- |
| pae.config_compiler.contract | 122，failed=0、expected=122 | Plan-only、阶段凭证、领域/预算与冻结失败 |
| pae.config_compiler.ui_description | 69，failed=0 | 描述计费、阶段执行、发布前失败清理与核验 |
| pae.public_api.compile_metadata | 41，failed=0 | 诊断映射、描述上限、公开移动/Take 与空 owner |
| pae.public_api.complete_record_codec | 57，failed=0 | 执行对象保留 compiled 状态、移动与结果寿命 |
| pae.public_api.consumer_metadata | 21，failed=0 | 查询类型/方向、移动及索引错误 |
| pae.public_api.physical_query | 31，failed=0 | 布局投影、长度边界与回调中不重复 Decode |
| pae.public_api.ascii_facts | 34，failed=0 | literal 借用、NUL、模板方向及 owner 移动 |

计数为现有用例实际日志，不是覆盖率；这些测试未修改。
`protocol_metadata_tests.cpp` 的 `probe_stages_once`、`plan_freeze_failure_no_partial_publish`、
`plan_freeze_failure_releases_sidecar`、`audit_failure_no_partial_publish`、
`audit_failure_reached_final_gate` 等断言验证代表性的阶段与失败清理。
公开测试的 `metadata_limit_exact`、`metadata_limit_minus_one`、移动/Take 断言及
Codec owner 释放后调用断言支持拥有与借用说明，但不是所有 OOM/竞态/非法 Take 的证明。

Release 有 `/O2 /Ob2 /DNDEBUG`，用例通过自定义 Runner 的运行时条件、失败计数及非零退出门禁
检查行为，不依赖普通 assert。类型能力限制使用 static_assert，Release 编译仍检查。
独立保存两配置 LastTest.log 中实际 case 输出和上述计数，未以只注册测试代替执行。

实际命令形状（完整目标数组、参数与退出码见独立脚本/transcript）：

```powershell
cmake --build $Build --config Debug --target pae_config_compiler_contract_tests pae_config_compiler_ui_description_tests pae_public_api_tests pae_public_codec_tests pae_public_consumer_metadata_tests pae_public_physical_query_tests pae_public_ascii_facts_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^(pae\.config_compiler\.(contract|ui_description)|pae\.public_api\.(compile_metadata|complete_record_codec|consumer_metadata|physical_query|ascii_facts))$' --output-on-failure
# Debug 完成后，以相同目标及过滤器执行 Release。
```

## 5. 证据与停点

证据均相对 `out/build/comments-public-20261009/compiler-entry/`，未覆盖前批日志：

- `baseline/`、`takeover-status.log`、`non-authorized-tracked-hashes.json`：接管与保真。
- `verify-comments.ps1`、`comment-equivalence.log`、`final-comment-equivalence.log`：
  token、宏续行、UTF-8、格式前后比较及 3239 文件保真。
- `baseline-format-*.log`、`final-format-*.log`：原格式差异原样保留。
- `run-validation.ps1`、`validation-transcript.log`、`build-{Debug,Release}.log`、
  `test-{Debug,Release}.log`、`cases-{Debug,Release}.log`：本次真实命令和 case 输出。
- `diff-check.log`、`candidate-portable.log`、`final-audit.log`：候选及最终状态检查。

未发现本批注释与已执行路径的新阻断，仍待总控独立复核；历史四处格式差异未修。
未验证全链峰值内存、任意异常分配点、引用计数上限、所有线程交错、完整 Schema/领域/Plan
实现审查、全仓、Linux、UI、网络、硬件或真实协议；不把专项通过升级为生产/稳定 ABI 结论。
未执行 Stage、Commit、Push、历史改写、清理、删除、SDK/体验包重打或发布。
向总控完成交接后停止写入，等待复核。
