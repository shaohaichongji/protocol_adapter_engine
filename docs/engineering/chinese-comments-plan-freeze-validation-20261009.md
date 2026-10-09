# 第九批：Plan 草稿组装、冻结与存储中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不表示提交批准。

## 1. 接管与增量

接管及最终核对为 `main@f30b29e6e76e8da1ae1057067fd1ac3bca2e3b15`，暂存区为空。
已有第七、八批六个 tracked 修改和两份 untracked 报告，均保留；
`config_compiler.cpp` 是本批唯一与前批重叠文件，只在授权两函数内新增注释。
现场没有 cmake/ctest 活动进程；可见 MSBuild 为 `/nodemode:1 /nodeReuse:true` 复用节点，
不把节点存在误写成正在执行另一条构建，也未终止它。

本批七个源码增量及独立报告：

| 文件 | 注释内容 |
| --- | --- |
| src/protocol_plan/plan_builder.h | Draft 能力入口、诊断索引、结果借用/Take 与 noexcept 映射 |
| src/protocol_plan/plan_builder.cpp | 防御复核、预计算、布局报告核对、Arena 构造及失败释放 |
| src/protocol_plan/plan_draft_internal.h | 原始草稿、批准报告及临时 Prepared 执行材料 |
| src/protocol_plan/frozen_storage.h | 字符借用、数组元素析构责任、Adopt 和未检查访问前置条件 |
| src/protocol_plan/plan_bundle.h | 可变/冻结/执行三层、单位与索引、Workspace 计费及 owner 寿命 |
| src/protocol_plan/plan_bundle.cpp | 已构造数组责任移交、查询的借用边界 |
| src/config_compiler/config_compiler.cpp | 仅 PlanDraftAssembler::Assemble、FreezeBudgetedPlanDraft 内 |
| docs/engineering/chinese-comments-plan-freeze-validation-20261009.md | 本次独立验证报告 |

未修改非注释 token、声明布局、宏续行、运行语义、CMake、测试或公开头。
plan_memory.*、plan_types.h、decimal_conversion_internal.* 及 metadata/public API 仅作只读依据；
前批报告、索引、Lab/Adapter、SDK/体验包和其他源码没有改动。
保留原正确英文注释，没有借本批更改契约或修复产品实现。

## 2. 实际语义与注释依据

### 草稿与 Prepared

`PlanDraftAssembler::Assemble()` 检查预算凭证未被消费后，读取自有 IR/解析引用，
按实际路径拷贝或移动为可变 Plan 描述，生成同步字节及文本 literal 的前缀表。
ResourceRequirements、批准的 PlanMemoryReport 与上限一起移交到自有 PlanDraftData，
再由私有构造封装为 BudgetedPlanDraft。组装完成不表示已冻结，亦不表示已发布 metadata。
生产入口只消费该 move-only 包装；测试友元能够构造或破坏原始材料，所以 Builder 仍复核。

PreparedMessageExecutionPlan / PreparedPipelineExecutionPlan 持有临时 vector 等自有数据；
其预计算结果随后复制到 Arena 的执行描述，不是每帧业务值或已发布的 FrozenArray。
Builder 按当前实现检查版本、档位、需求统计、引用/方向、布局、类型、Matcher 和各启用分支。
评论解释的是现有防御链，不声称此次已完整审计所有损坏 Draft 组合。

字段的机器尺寸、动态输入序号、位掩码/移位、枚举 lookup 等在冻结前计算。
64 位满宽掩码单独处理，合法满宽布局的移位为 0；枚举 lookup 排序保留 entry_index，
映射原声明顺序。固定 Matcher 字节按偏移组织，固定长度候选按长度分组；
allowed 位图仍使用包级消息索引，变长和文本候选单独保存。
候选存在不是完整性/字段校验成功，也不因此合并 Pipeline 身份。

### 计费与发布

核对 `EstimatePreparedPlanMemory()`、`FreezeImpl()` 构造段及只读的 PlanMemoryLayout/PlanArena：

1. 按实际冻结类型、分配顺序、数量、sizeof、对齐复算 Prepared 需求。
2. 检查有效上限，并逐类别/对齐/分配计数对照上游批准报告。
3. 取得冻结存储块，在 Arena 内按对应类别构造字符、元素数组和执行索引。
4. 核对 Arena 实际报告与复算报告、storage.Size()，才 placement 构造 PlanBundle。
5. PlanOwner 一起接管块及 Bundle 地址，成功结果发布完整 owner。

Layout/数组分配对乘法、加法、对齐和容量做检查；注释没有把逻辑计费扩展为编译全链峰值或 RSS。
Workspace 槽按消息最大需求推导，estimated_workspace_bytes 是该布局的计费项，
不等于所有宿主对象、输出槽和进程分配的总和。
“Arena 内不逐描述向堆申请”只解释冻结存储构造，不承诺整个编译过程无堆分配。
Prepared vector/map、原始草稿和其他临时对象仍可分配。

### 存储和寿命

- FrozenString 只描述显式长度字符区，不拥有/释放字节，也不保证 NUL 结尾。
- FrozenArray 不 delete Arena 内存，但接管已构造元素的析构责任；移动清空源，
  Reset 逆序析构，包含嵌套数组。它不是普通可复制的 span。
- Adopt 要求对应元素已构造、存储覆盖析构期，不能为同批元素重复接管。
  数组访问器不检查边界，front/back 要求非空。
- PlanBundle 不能复制或移动；PlanOwner 移动转移存储而不搬动 Bundle。
  查询 view/引用仍是借用，不增加引用。PlanOwner::Reset 先析构 Bundle，再释放存储块。
- PlanBuildResult::TakePlan() 移走 owner 后源 Succeeded() 为 false，但 Plan 地址不变，
  已有 Plan 借用可在新 owner 保障下继续使用；Diagnostic 借用仍依赖结果对象，不依赖新 owner。

注释初稿曾把 TakePlan 后的旧借用说明写得过严，本批自审发现后纠正为上述实际边界。
第一次 Debug/Release 四专项均通过；注释纠正后重新构建并串行测试最终状态。
前后全部非注释 token 相同，两轮日志独立保留。没有将评论问题表述为产品实现缺陷。

FreezeObjectArray 在 factory 返回失败时逆序析构已完成元素，成功才 Adopt；不回退 Arena 偏移。
外层 storage 构造早于局部 FrozenArray，失败返回先析构数组，再释放底层字节。
这条说明限定实际返回失败路径，不宣称任意新增 factory 抛异常也具有同样的局部回滚保证。
Freeze noexcept 捕获 bad_alloc 为 ALLOCATION_FAILED，其他异常为 INTERNAL_ERROR；
编译边界保留既有分配、预算超限、报告不一致和内部构造违规映射，不重新报告作者配置错误。

## 3. 注释等价与保护

接管副本、Hash 和日志在忽略目录 `out/build/comments-public-20261009/plan-freeze/`。
七文件的验证保留字符串/字符字面量，只忽略注释和空白；raw string 不支持时失败关闭，
本批目标未包含 raw string。结合 diff 复核，非注释 token 完全一致，宏续行文本相同，
没有注释尾反斜杠。

`config_compiler.cpp` 以授权两函数的起止锚点保护前缀/后缀，UTF-8 字节完全一致，
包括第八批所有已写注释。3236 个范围外 tracked 文件及两份前批 untracked 报告，
共 3238 文件 SHA256 未变；包含前批另外五个 dirty 源码，以及所有只读依赖。

七目标接管/最终 clang-format dry-run 均退出 0，遵守仓库 Google/C++17/100 列，
未执行全文件重排。源码与本报告严格 UTF-8、无 BOM；git diff --check 和便携候选检查通过。
新增候选只有本报告，out 内证据被忽略；未纳入原始人工证据或私有协议。

## 4. 本次 Windows 验证

现有 `default-repo` 源根核对为当前仓库；VS18 2026、x64、v142 14.29.30133，
testing/public API 及当前可选 Schema 开关开启。Release 为 `/O2 /Ob2 /DNDEBUG`。
不重新配置 CMake，不运行其他构建目录或共享目录并行 Debug/Release。

选取四个现有专项：编译器契约内已有 Builder 防御、精确计费和分配注入；
metadata 支持完整编译发布链；Core/公开 Codec 支持实际冻结材料、容量和保留 owner 的消费。
没有独立 tests/protocol_plan 目录，因此未臆造 Plan 专项目标。

最终顺序 Debug build/test → Release build/test，四命令退出码均为 0，各配置 CTest 4/4。

| CTest | Debug | Release | 证据范围 |
| --- | --- | --- | --- |
| pae.config_compiler.contract | 122 passed / 0 failed，expected=122 | 同左 | 能力凭证、损坏 Draft、计费复核及固定快照 |
| pae.config_compiler.ui_description | 69 passed / 0 failed | 同左 | 完整产物与发布失败清理 |
| pae.protocol_core.complete_record_codec.contract | 61 passed / 0 failed，expected=61 | 同左 | 冻结执行材料、字段/Frame 容量、Workspace 归属及合成向量 |
| pae.public_api.complete_record_codec | 57 passed / 0 failed | 同左 | owner 保留、移动、独立执行状态及资源门禁 |

编译器 `plan_memory_report_accounting` 检查分类求和和确定性报告，
`plan_memory_budget_exact_boundary` 检查精确上限/减一与类型化诊断，
`plan_memory_estimate_mismatch` 拒绝批准报告破坏。
`plan_memory_failure_injection_no_leak` 在既有合成 Plan 的分配序号范围内检查失败诊断、
live upstream bytes/allocations 回零及成功 owner 析构；不泛化到所有 Schema 和所有异常点。
`capability_single_consumption_internal_violation`、损坏 Draft、位容器/CRC/变长/ASCII 等
现有 Builder 防御用例和 static_assert 也实际执行/编译，测试源码未修改。

Core 的既有 verify_golden_files.cmake 先核验六份运行副本的固定 SHA256，随后执行
61 用例；没有重写原始合成期望。此处是 Synthetic Engine Vector，不是新真实协议 Golden。
Core Runner 检查预期 ID、重复/遗漏和失败计数；compiler Runner 检查预期数量，
其他专项运行时条件与失败计数决定退出码，Release 不依赖被 NDEBUG 移除的普通 assert。
用例数不是覆盖率，也没有据局部无分配断言生成性能结论。

实际命令形状（完整参数与退出码在脚本/transcript）：

```powershell
$Build = 'out/build/comments-public-20261009/default-repo'
cmake --build $Build --config Debug --target pae_config_compiler_contract_tests pae_config_compiler_ui_description_tests pae_complete_record_codec_contract_tests pae_public_codec_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^(pae\.config_compiler\.(contract|ui_description)|pae\.protocol_core\.complete_record_codec\.contract|pae\.public_api\.complete_record_codec)$' --output-on-failure
# Debug 完成后，以相同目标和过滤器执行 Release。
```

证据均在本批 plan-freeze 子目录，不覆盖前批：

- `baseline/`、`protected-hashes.json`、`takeover-status.log`、`build-cache-audit.log`：接管与配置。
- `verify-comments.ps1`、`comment-equivalence.log`、`corrected-comment-equivalence.log`、
  `final-comment-equivalence.log`、`baseline-format-*.log`、`final-format-*.log`：等价/保护/格式。
- `run-validation.ps1`、`final-validation-transcript.log`、`final-build-{Debug,Release}.log`、
  `final-test-{Debug,Release}.log`、`final-cases-{Debug,Release}.log`：最终实际命令及用例输出。
- 无 final 前缀的 build/test/cases 和 `validation-transcript.log`：本批注释初稿的首次通过记录，
  不是最终状态，也不是前批历史验证。
- `diff-check.log`、`candidate-portable.log`、`final-audit.log`：最终候选与 Git 核对。

## 5. 停点与局限

未发现需要在本批注释范围修复的新产品阻断；仍待总控独立复核，不自行批准提交。
未动态穷尽 FrozenArray 错误 Adopt/越界、所有坏 Draft、任意异常 factory、全链峰值内存、
所有构建开关或线程交错。只读分析与代表性专项不等于完整内存安全证明。
未运行全仓、UI、网络、SDK 包外/重打包、Linux、硬件或真实协议；
不新增稳定 ABI、生产、性能或现场验收结论。

最终累计 12 个 tracked 源码修改和 3 份 untracked 报告；第七、八批全文件或非授权区域保真。
未 Stage、Commit、Push、历史改写、清理删除、发布或派发其他任务。
完成一次总控反馈后停止写入，等待复核。

## 6. 总控复核措辞修正（2026-10-09）

总控指出 PlanBundle 类前“所有查询返回借用”过宽：GetResourceProfile() 按值返回枚举。
仅修正该句为视图/引用借用且不增加引用或延长寿命，枚举返回独立值；未改变接口或实现。
本段追加记录，不覆盖前面的执行历史。第 4 节 final 前缀的 Debug/Release 各 4/4
是在本次措辞修正之前执行；此次不重跑不受影响的测试，非注释 token、声明与布局完全不变。

本次修正前两文件副本保存于 plan-freeze/review-wording-baseline/；
其余 3242 个 tracked 文件及两份前批报告，共 3244 个文件 SHA256 不变。
plan_bundle.h 与本次修正前副本精确对照，仅替换目标注释；报告原文保留并追加本段。
最终实际复核通过：注释等价、宏续行、严格 UTF-8 无 BOM、clang-format dry-run、git diff --check。
首次新增注释行过长，格式检查失败关闭；随后仅拆分目标注释的显示行，未改变周围注释。
首次失败保留在 review-wording-verification.log、review-wording-format.log；
第二次验证因脚本预期 CRLF 与新增行实际 LF 不一致而中断，
保留在 review-wording-corrected-verification.log，不作为通过证据。
逐字节确认新增行分隔符为 LF 后，仅纠正脚本的精确预期，不归一化源码或放宽比较。
通过记录为 review-wording-final-verification.log（报告状态更新前）及
review-wording-closeout-verification.log（报告最终状态），格式及 diff 检查退出码均为 0。
另有 review-wording-final-format.log、review-wording-final-diff-check.log、review-wording-verify.ps1、
review-wording-protected-hashes.json 及 review-wording-git-status.log；旧验证日志没有覆盖。
分支与 HEAD 不变，暂存区仍为空，未 Stage、Commit、Push；再次反馈后停止写入待总控复核。
