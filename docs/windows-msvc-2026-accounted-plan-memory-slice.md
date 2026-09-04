# PAE-DEC-033A Accounted Plan Memory Windows验证报告

## 1. 结论

截至2026-09-02，`PAE-DEC-033A`不可变Plan长期内存计量与单Plan准入内部切片已在Windows x64、MSVC Release/Debug下实现并通过当前阶段门禁，状态为`PARTIALLY VERIFIED（部分已验证）`。

本结论只覆盖当前内部`COMPLETE_RECORD（完整记录）`Schema/Plan/Codec切片。它不证明`PAE-DEC-033B` Runtime（运行时）全部活跃Plan/Session（计划/会话）聚合准入、最终生产容量、Linux、嵌入式目标、正式性能、真实协议、硬件或现场已经验证。

## 2. 已实现范围

- 最终`PlanBundle`、稳定字符串、冷元数据、Matcher字节、热执行描述符和索引由`FrozenString/FrozenArray`保存在一个`PlanStorageBlock（计划存储块）`中；
- `PlanArena（计划内存区）`按对象、字符串、Matcher、元数据容器、执行描述符、索引、扩展和对齐分类计费，上游堆分配成功Plan固定为一次；
- `PlanOwner（计划所有者）`是move-only（仅移动）所有权对象，先销毁Plan及嵌套Frozen对象，再释放Storage Block；Codec仍只借用`const PlanBundle&`；
- `ResourceBudgetValidator（资源预算校验器）`在产生`BudgetedSchemaIr`前计算精确`PlanMemoryReport（计划内存报告）`，按Profile执行单Plan准入；
- `BudgetedSchemaIr → BudgetedPlanDraft → PlanBuilder`私有载荷携带批准报告和有效Limit；Builder基于Prepared Plan复算，最终Arena报告必须与批准报告逐字段完全相等；
- 内部候选单Plan限制为Hard `256 MiB`、Desktop `128 MiB`、Constrained `8 MiB`，继续标记为`CANDIDATE / UNVERIFIED（候选 / 未经生产规模验证）`；
- 超限返回`RESOURCE_BUDGET / RESOURCE_LIMIT_EXCEEDED`，并携带`PLAN_ACCOUNTED_MEMORY + required_bytes + limit_bytes + resource_profile`类型化上下文；
- 任意Storage Block或逻辑分配点失败均不交付Plan，RAII（资源获取即初始化）负责释放已构造Frozen对象和唯一Storage Block。

当前没有单独的`PlanMemoryTransaction`或`PlanStorageShape`公开类型；事务语义由`Budgeted`能力、批准报告、`PlanOwner`和局部RAII对象共同实现。ResourceBudget和Builder分别枚举同一最终布局，并复用`PlanMemoryLayout`受检布局原语；Builder发布前的逐字段相等检查是两侧漂移门禁。这些名称和C++布局均为内部实现，不属于稳定API（应用程序编程接口）或ABI（应用二进制接口）。

## 3. 新增契约证据

Config Compiler（配置编译器）Runner由`24/24`增至`28/28`，新增：

1. 分类字节之和、对齐、逻辑分配数、一次上游分配及重复编译报告确定性；
2. 测试注入Limit的`exact（恰好等于）`成功与`limit - 1`类型化拒绝；
3. 从上游Storage Block到最后一个Arena逻辑分配点的全序号故障注入，每次失败后活动字节和活动上游分配均归零，成功Plan销毁后同样归零；
4. Builder批准报告被测试Peer篡改后返回`PLAN_MEMORY_ESTIMATE_MISMATCH`且不交付Plan。

现有Codec主合同、首次调用零分配、操作计数和共享Plan并发门禁保持通过，说明Frozen Storage迁移没有改变当前字节语义和热路径分配合同。

## 4. 实际环境与命令

实际工具：

- CMake/CTest：`D:\develop_env\cmake-4.4.3\bin`，版本4.4.3；
- Generator（生成器）：`Visual Studio 18 2026`，x64；
- MSVC：19.51.36256.0；
- Windows SDK：10.0.22621.0。

全切片使用全新目录`out/build/windows-msvc-all-slices-dec033a`配置；Configure阶段实际通过1个正向和8个负向`try_compile（试编译）`能力门禁。随后执行：

```powershell
cmake --build out/build/windows-msvc-all-slices-dec033a --config Release --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec033a -C Release --output-on-failure
cmake --build out/build/windows-msvc-all-slices-dec033a --config Debug --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec033a -C Debug --output-on-failure
```

Release和Debug均构建成功，共存CTest均`26/26`通过。两种配置的专项Runner结果均为：

- Config Compiler：`28/28`；
- Codec主合同：`60/60`；
- 首次Decode/Encode分配：各`1/1`；
- 操作计数：`4/4`；
- 共享Plan并发：`2/2`。

## 5. 产品和宿主嵌入边界

`out/build/windows-msvc-codec-product-only-dec033a`以`BUILD_TESTING=OFF`、`PAE_BUILD_TESTING=OFF`进行Release构建并通过。生成的PAE产品目标只有：

- `pae_protocol_plan`；
- `pae_protocol_core_slice`。

没有Config Compiler、yyjson、测试Peer、测试工厂、instrumented Core（带计数Core）或PAE测试Runner。

临时宿主`out/host-dec033a`设置宿主`BUILD_TESTING=ON`，通过`add_subdirectory()`嵌入PAE但不显式开启`PAE_BUILD_TESTING`。Release构建成功，宿主可执行文件实际退出码为0；PAE仍只生成上述两个产品目标。宿主自身的CTest辅助目标不代表PAE测试目标泄漏。

## 6. 尚未验证和剩余风险

- `PAE-DEC-033B` Runtime/Session聚合预算、并发原子预留、共享Plan只计一次和销毁退款尚未实现；
- `128/8/256 MiB`只是内部候选上界，尚未用正式最大规模Schema、Loader/Compiler峰值和目标设备数据冻结；
- 当前Estimator与Builder依靠共同布局原语和最终相等门禁保持一致，尚未抽取单独Storage Shape对象；新增Final Plan字段时必须同时扩展两侧枚举和分类覆盖测试；
- 尚未覆盖稳定公共Allocator注入、C ABI、Binary Plan、动态插件、Linux GCC/Clang、Sanitizer（运行期检查器）和正式Benchmark（基准测试）；
- 当前Synthetic Engine Vector（合成引擎向量）不构成真实协议、硬件或现场证据。

本轮没有Commit（提交）或Push（推送）。

## 7. 第十八轮提交前复核

2026-09-04在全新构建目录`out/build/windows-msvc-all-slices-dec018-audit`重新执行Configure、Release/Debug构建与CTest：

- Configure阶段1个正向和8个负向`try_compile`结果均符合预期；
- Release和Debug全切片CTest分别为`26/26`；
- 两种配置的Config Compiler、Codec主合同、首次Decode/Encode分配、操作计数和共享Plan并发Runner分别保持`28/28`、`60/60`、`1/1 + 1/1`、`4/4`和`2/2`；
- 变更C++文件的`clang-format --dry-run --Werror`与`git diff --check`通过；
- Product-only Release只生成`pae_protocol_plan`和`pae_protocol_core_slice`两个PAE产品目标；宿主`add_subdirectory()` Release构建及可执行退出码0，PAE测试、Config Compiler和yyjson未泄漏；
- 对全部已跟踪变更及21个未跟踪文件执行凭据、私钥、账号口令、IP地址和真实协议身份扫描，未发现需要阻断公开提交的内容。

本次复核修正了Strict JSON Profile与Loader/Compiler文档版本号，以及本Frozen Execution Plan历史报告对DEC-033A状态的滞后描述；没有修改源码或测试语义。仍未执行Linux、Sanitizer、正式协议、目标设备、硬件或现场验证，也未Stage、Commit或Push。
