# PAE Validated/Budgeted能力链 Windows验证报告

## 1. 结论

截至2026-09-02，`PAE-DEC-032`在当前内部切片范围内完成实现并通过Windows x64、MSVC（Microsoft Visual C++，微软C/C++编译器）Release/Debug门禁，可标记为`CONFIRMED / VERIFIED（已确认 / 已验证）`。

本结论只证明配置编译期的不可伪造能力链、作者错误归属、失败原子性和现有`COMPLETE_RECORD（完整记录）`回归。它不证明`PAE-DEC-033`完整内存准入、稳定公共API（应用程序编程接口）、Linux、正式协议、硬件或现场行为。

## 2. 已实现契约

生产编译链为：

```text
SchemaIr
  → DomainValidator
ValidatedSchemaIr
  → ResourceBudgetValidator
BudgetedSchemaIr
  → PlanDraftAssembler
BudgetedPlanDraft
  → PlanBuilder::Freeze
PlanBundle
```

- 三种能力类型均为`final class`，禁止默认构造和复制，只允许移动；内部不使用公开布尔值、枚举Token或通用PassKey（通行密钥）；
- `ValidatedSchemaIr`和`BudgetedSchemaIr`使用独占私有载荷，`BudgetedPlanDraft`使用私有PIMPL（Pointer to Implementation，指向实现的指针）；移动后重复消费会得到内部契约失败，不能生成第二份有效能力；
- 原始草案降级为`plan_draft_internal.h`中的内部载荷；生产`PlanBuilder::Freeze`只接受`BudgetedPlanDraft`，不存在兼容的原始草案重载；
- 重复ID、引用、方向、字段边界/重叠、帧覆盖、Matcher（匹配器）、Enum（枚举）和Profile（资源档）超限由Domain/Resource Validator作为唯一作者错误权威报告；
- Builder保留的检查只作为内部不变量审计。合法Budgeted输入仍触发这些检查时，Config Compiler统一交付`INTERNAL_CONTRACT_VIOLATION（内部契约违规）`；分配失败单独映射为`COMPILER_ALLOCATION_FAILED（编译器分配失败）`；
- Domain、Budget、Assembly、Plan Build和最终Compile Result均通过受限构造保持“成功只有value、失败只有diagnostic（诊断）”，不交付部分能力或部分Plan；
- Core测试不再直接构造原始草案，统一通过测试专用JSON工厂进入正式配置编译链。

## 3. 编译期与故障注入门禁

CMake配置阶段实际执行1个正向与8个负向`try_compile（试编译）`：

| 类型 | 数量 | 结果 |
| --- | ---: | --- |
| 正向：能力类型头和允许的move-only类型特征可编译 | 1 | `expected=TRUE actual=TRUE` |
| 负向：Validated/Budgeted/Draft默认构造 | 3 | 均`expected=FALSE actual=FALSE` |
| 负向：Validated/Budgeted/Draft复制 | 3 | 均`expected=FALSE actual=FALSE` |
| 负向：原始`PlanDraft`调用Builder | 1 | `expected=FALSE actual=FALSE` |
| 负向：原始`SchemaIr`跳级构造Budgeted能力 | 1 | `expected=FALSE actual=FALSE` |

Config Compiler合同Runner新增两项：

1. 逐级消费有效能力，并验证`ValidatedSchemaIr`、`BudgetedSchemaIr`和`BudgetedPlanDraft`移动后均不能成功重复消费；
2. 仅测试Target（目标）定义私有Peer（测试同伴），构造损坏的`BudgetedPlanDraft`，验证最终只得到`PLAN_BUILD + INTERNAL_CONTRACT_VIOLATION`且没有部分Plan。该Peer源码没有进入产品静态库、安装/导出头文件或产品符号。

## 4. 实际环境与结果

| 项目 | 实际值 |
| --- | --- |
| CMake | 4.4.3 |
| Generator（生成器） | Visual Studio 18 2026，x64 |
| MSVC工具集 | 14.51.36231 |
| C/C++ Compiler（编译器） | MSVC 19.51.36256 |
| MSBuild | 18.9.1 |
| 平台 | Windows x64 |

主切片结果：

| 门禁 | Release | Debug |
| --- | --- | --- |
| Codec切片构建与CTest | 成功，`6/6` | 成功，`6/6` |
| Config Compiler合同Runner | `24/24` | `24/24` |
| COMPLETE_RECORD主合同Runner | `60/60` | `60/60` |
| 首次Decode/Encode分配 | 各`1/1` | CTest通过 |
| 操作计数 | `4/4` | CTest通过 |
| 共享Plan并发 | `2/2` | CTest通过 |

Parser/Loader/Plan/Codec共存构建目录`out/build/windows-msvc-all-slices-dec032`在Release和Debug下均构建成功，CTest各`26/26`。

产品边界另外取得以下实际证据：

- Product-only（仅产品）Release构建成功，生成目标只有`pae_protocol_plan`、`pae_protocol_core_slice`和CMake辅助目标；没有Config Compiler、yyjson、测试Peer、测试工厂、instrumented Core（带计数Core）或PAE测试Runner；
- 临时宿主以`BUILD_TESTING=ON`并通过`add_subdirectory()`嵌入PAE，但没有显式开启`PAE_BUILD_TESTING`；Release构建成功，PAE仍只生成产品Plan/Core，宿主可保留自身CTest辅助目标。临时宿主源文件验证后已删除；
- 变更C++文件执行`clang-format --dry-run --Werror`通过，`git diff --check`通过。

## 5. 执行命令摘要

```powershell
cmake --preset windows-msvc-codec-slice
cmake --build out/build/windows-msvc-codec-slice --config Release --parallel
ctest --test-dir out/build/windows-msvc-codec-slice -C Release --output-on-failure
cmake --build out/build/windows-msvc-codec-slice --config Debug --parallel
ctest --test-dir out/build/windows-msvc-codec-slice -C Debug --output-on-failure

cmake -S . -B out/build/windows-msvc-all-slices-dec032 -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_JSON_PARSER_SPIKE=ON `
  -DPAE_JSON_SPIKE_CANDIDATE=all -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON
cmake --build out/build/windows-msvc-all-slices-dec032 --config Release --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec032 -C Release --output-on-failure
cmake --build out/build/windows-msvc-all-slices-dec032 --config Debug --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec032 -C Debug --output-on-failure
```

实际执行使用`D:\develop_env\cmake-4.4.3\bin`下的`cmake.exe`和`ctest.exe`；上面以短命令展示便于复用。

## 6. 未验证边界

- `PAE-DEC-033`冷Plan真实字符串、Matcher、容器和Runtime全部活跃Plan/Session总内存准入；
- Loader/Compiler全阶段Allocator（分配器）故障注入和峰值内存；
- STREAM_CHUNK（流式字节块）、Integrity（完整性校验）、Receive Gate（接收门禁）、Mapping（映射）、Runtime、Session和C ABI（C应用二进制接口）；
- Linux GCC/Clang、Sanitizer（检测器）、其他CPU架构和嵌入式目标；
- 正式Benchmark（基准测试）、真实协议Golden Vector（黄金测试向量）、设备互通、硬件和现场验证。

本轮未Commit（提交）、未Push（推送）。
