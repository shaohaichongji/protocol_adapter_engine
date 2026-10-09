# 第十批：运行时完整记录 Codec 主线中文注释验证

日期：2026-10-09。状态：已完成派发范围，待总控复核；不表示提交批准。

## 1. 接管与修改范围

现场为 `main@829632d7aece20a677d0ffbee9f65d9479863966`，接管工作树和暂存区均为空。
第七至九批已在此基线提交，相关源码、报告及旧 out 日志完整保留。
读取外层 AGENTS.md；仓库内未发现更深层 AGENTS.md。接管时未见活动 cmake/ctest/MSBuild。

仅修改下列三个源码中的注释，并新增本报告：

| 文件 | 新增注释行 | 内容 |
| --- | ---: | --- |
| src/public_api/codec.cpp | 15 | facade 职责、保留状态、占用、发布/失效、类型映射和 raw 来源 |
| src/protocol_core/complete_record_codec.h | 9 | 内部执行表示、引用归属、槽类型、交付长度、Workspace 与计数边界 |
| src/protocol_core/complete_record_codec.cpp | 23 | 匹配、准入、物化、位容器、完整性生成和最终复核 |

不改非注释 token、声明、布局、宏续行、接口、测试、CMake、Schema 或运行行为。
正确英文注释全部保留；未纠正或删除原英文，也未逐行翻译全部 helper。
公开 include 头、decimal_conversion 实现、Host、Framer、YAML、Lab 只读或不涉及。

## 2. 注释依据与实际执行语义

依据当前三个目标源码、include/pae/codec.h、CompiledState 所有权链、冻结执行描述，
以及 schema/protocol_plan_execution_semantics_v0.1.md 的完整记录、位字段和完整性章节。

### 所有权与失效

Core ExecutionWorkspace 永久绑定并借用同地址 Plan，构造时按冻结布局准备可变槽；
Plan 必须覆盖 Workspace 寿命。公开 Codec 的 Impl 保留 CompiledStateRef，拥有独立的
Workspace、decoded_slots 和 mapped_values；成员逆序析构使 Workspace 早于 state 释放。
输入/输出字节不因此归 Codec 所有，返回 BYTES 仍借用本次未被改写的输入。

FacadeLease 与 ExecutionWorkspaceLease 仅防止同执行对象的重叠调用；未取得占用的调用
不清除别人的标记。这不是视图读取、owner 移动/销毁或外部 Buffer 的任意线程安全保证。
BeginGuardedCall 在取得保护后撤销上一代发布，即使后续失败也使旧成功视图失效。
移动的 epoch 与调用的 generation 共同检查旧视图，但不延长 owner 寿命；owner 销毁后
不能再用 HasValue 探测悬空。此批不增加任何生命周期或并发能力。

### Decode

前置占用、归属、代际、参数和 Pipeline 检查保留。FindPipelineMatch 使用冻结的固定长度
分组、有界变长及文本候选；完整性和业务枚举语义不参与结构消歧。
内部 MatchCompleteRecordStructure 复用同一 Matcher，不占用 Workspace，不交付字段。

Binary 在唯一候选后检查完整槽容量和内存别名，随后按启用分支检查计算长度、完整性，
预检转换及未知枚举策略，再物化槽。preserve 可成功并 tainted，不能强行映射已知枚举。
仅整条记录 OK 才发布 field_count 和转换 raw 诊断。失败不交付槽内容，不作 Buffer 回滚保证。
文本分支在容量/别名/字符检查后物化输入切片，不把它描述成 Binary 整数流程。

公开 facade 核对结构身份与 Pipeline 关联后映射诊断；matched_message_index 可以在
唯一匹配后的失败中存在，但只有 OK 才发布可用 record。raw 来自 Wire 读取/提取或
Workspace 留存，Decimal logical 单独存储，不从显示值反算 raw。

### Encode 与条件编译

公开层保留类型标签和值，数值不隐式互换，BYTES 仍借用。Core 建立 input_ordinal 到
values 序号的映射与存在位图，拒绝引用、重复、常量/计算字段覆盖及缺失。
转换代际按配置字段顺序检查值；实际长度、容量和别名在写入前检查，不由输入顺序决定布局。
位容器从 base_value 初始化，清成员位后写入，不依赖输出旧内容。

写完固定字节、业务字段/位容器及计算长度后生成完整性字节并复算，再执行最终 Matcher
和字段复核，不通过调用 Decode 绕过复核。只有成功交付 bytes_written，失败保持 0；
后期失败 Buffer 可已改写，required_size 不是可发送长度。校验通过不等于设备业务成功或认证。

SupportsCompleteRecordSchema 与可选编译开关共同限制执行代际；操作计数/故障注入
只在插桩目标存在。读取生产计数返回零，不把访问计数作为耗时或吞吐结论。

## 3. 本次实际验证

复用 `out/build/comments-public-20261009/default-repo`，未创建全量构建根或重新配置。
核对源根为当前仓库，VS18 2026/x64/v142，MSVC 工具目录版本 14.29.30133；
Schema 0.5 至 0.11 开关开启，Release 为 `/O2 /Ob2 /DNDEBUG`。
CMake/CTest 4.3.1-msvc1、clang-format 22.1.3；未修改 Qt 或全局工具环境。

实际执行顺序 Debug build/test → Release build/test，四条命令退出码均为 0。
每个配置 CTest 2/2；本次两项专项内部输出如下，不把用例数当覆盖率：

| 专项 | Debug | Release |
| --- | --- | --- |
| pae.protocol_core.complete_record_codec.contract | 61 passed / 0 failed，expected=61 | 同左 |
| pae.public_api.complete_record_codec | 57 passed / 0 failed | 同左 |

```powershell
$Build = 'out/build/comments-public-20261009/default-repo'
cmake --build $Build --config Debug --target pae_complete_record_codec_contract_tests pae_public_codec_tests -- /m:2
ctest --test-dir $Build -C Debug -R '^(pae\.protocol_core\.complete_record_codec\.contract|pae\.public_api\.complete_record_codec)$' --output-on-failure
# Debug 完成后，相同目标与过滤器执行 Release。
```

Core Runner 校验预期 ID、重复/遗漏、实际失败数及退出码；public Runner 的 Check 是运行时
条件判断而非 assert，Finish 据失败数返回非零。Release 不因 NDEBUG 取消这些检查。
Core 既有 CTest wrapper 核验六份合成向量固定 Hash 后运行，未重新生成旧期望。

实际代表性断言包括：独立 Encode/Decode 向量、输入顺序、未知枚举 preserve/reject、容量、
引用/Workspace 归属、别名、最终复核失败、首次调用和成功/失败热路径 replaceable-new 计数；
public 还检查保留编译状态、逻辑计费精确上限/减一、忙调用不改缓存、移动和再次调用使旧视图
失效、BYTES 借用真实输入、Decimal logical 与记录 raw、独立 Workspace、ASCII 单向能力。
这些是已有有限向量/代表性路径，不是所有新代或所有故障组合的完整审计。

三个目标的接管/修改后 clang-format dry-run 均退出 0，未全文件格式化。
保留字面量的非注释 token 对照一致，宏续行文本一致，没有注释尾反斜杠。
校验器遇不支持的 raw string 失败关闭；目标未含该语法。
范围外 3243 个 tracked 文件 SHA256 未变，包含第七至九批内容、测试和公共头。
源码/报告严格 UTF-8、无 BOM，git diff --check 及候选白名单检查通过。
新增候选仅本报告，四候选未发现私有资料、生产端点或机器身份路径；out 证据不纳入候选。

本批证据根：`out/build/comments-public-20261009/runtime-codec/`，旧批日志未覆盖。

- baseline/、protected-hashes.json、takeover-status.log、build-cache-audit.log：接管依据。
- verify-comments.ps1、initial-equivalence.log、final-equivalence.log、
  baseline-format-*.log、initial-format-*.log、final-format-*.log：等价/Hash/UTF-8/格式。
- run-validation.ps1、validation-transcript.log、build-{Debug,Release}.log、
  test-{Debug,Release}.log、cases-{Debug,Release}.log：实际命令、退出码和用例输出。
- final-diff-check.log、candidate-check.log、final-git-status.log：最终候选及 Git 核对。

## 4. 停点与未验证边界

本批注释范围未发现必须修复的实现阻断，待总控独立复核，不表示项目提交或生产批准。
未重跑完整全仓矩阵、独立 SUM8/CRC/长度/位字段/Decimal专项或所有关闭开关组合；
未动态穷尽线程交错、生命周期误用、epoch/generation 整数绕回或损坏 Plan 的所有路径。
未运行网络、UI、SDK 包外、重打包、Linux、真实协议、硬件或现场验收。
局部无分配/逻辑计费不升级为性能结论、进程 RSS 上限或完整内存安全证明。

最终累计三个 tracked 注释修改和一份 untracked 报告，暂存区为空，分支/HEAD 不变。
未 Stage、Commit、Push、清理、发布、新建任务或子智能体。完成一次总控交接后停止写入。
