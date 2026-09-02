# JSON Parser Spike

本实验使用同一套输入与统一结果模型比较：

- yyjson 0.12.0；
- nlohmann/json 3.12.0；
- RapidJSON 1.1.0。

比较维度包括严格 JSON 行为、重复 key、整数精度、资源上限、诊断、编译体量、峰值内存和解析耗时。候选依赖只存在于构建目录；最终选择必须另行记录，不能由本目录中的实验代码自动成为生产依赖。

当前状态：Windows x64/MSVC Release 下三个候选各 100 个文档级用例没有非预期变化，稳定 ID/来源 Inventory（清单）为 100/100；独立 Number Token（数字词法单元）语料已扩展到 77 项并闭合本轮数字转换契约，yyjson 另有 10 项 Raw Number（原始数字）到结构诊断的端到端自测。CTest（CMake Test，CMake 测试驱动）当前 20/20 通过。28 个文档用例已经成为机器清单权威，72 个仍由 C++ 生成；5 项独立负向变异门禁证明非法 offset（字节偏移）、目标锁和 Inventory 组合会在 Configure（配置生成）阶段失败。

这里的绿色结果是 Regression Gate（回归门禁）。三个文档级 Runner（运行器）仍包含 3 个`CHARACTERIZATION（现状表征）`，所以 Contract Closure State（契约闭合状态）保持`OPEN`；当前已经没有`OPEN_DECISION（待拍板）`。yyjson 使用`YYJSON_READ_NUMBER_AS_RAW`保留原始数字，已达到并锁定 3 个目标，摘要为`ready_to_promote=3 / open_contract_gaps=0 / target_lock_failures=0`；nlohmann/json 和 RapidJSON 仍各保留 3 个可见缺口。独立 77 项 Number Token 语料的`contract_closure_state=CLOSED`只表示该子契约闭合，不代表生产 Parser（解析器）选型或完整 Strict JSON Profile（严格 JSON 工作规范）已经闭合。

三档资源 Profile（资源档位）各执行 8 个维度的 exact/+1（恰好到上限/超限一级）输入，共 48 项 Parser-stage-only（仅解析阶段）生成型边界；每档还执行 Parser arena（解析器固定内存区）容量充分、硬边界和耗尽清理 3 项检查。当前结果均通过，但 Profile 状态仍是`CANDIDATE_UNVERIFIED（候选、尚未生产验证）`，`capacity_freeze_state=OPEN`、`hash_manifest_state=OPEN`；Loader/Compiler（加载器/编译器）峰值没有测量。最小SchemaIr、PlanBundle和Frozen Execution Plan（冻结执行计划）内部切片已在Windows构建测试；Runtime（运行时）注册仍未实现或运行。

yyjson 的固定内存池和 5 个解析期分配点逐点故障注入仍通过；其 Archive URL/Hash、MIT License 和两份候选源码现由锁文件驱动并在 Configure 阶段复核，所以继续作为唯一首选候选。它仍是`candidate（候选）`，不是已冻结的生产依赖。按当前推进顺序，Linux GCC/Clang 实测暂缓；源码和 CMake 仍维持平台无关边界，未执行前不得宣称 Linux 兼容。详见[第十三轮数字与 Parser 资源门禁报告](results/windows-msvc-2026-round13-number-resource-gate.md)。
