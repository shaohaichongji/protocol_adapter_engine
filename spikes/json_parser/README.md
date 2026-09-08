# JSON Parser Spike

本实验使用同一套输入与统一结果模型比较：

- yyjson 0.12.0；
- nlohmann/json 3.12.0；
- RapidJSON 1.1.0。

比较维度包括严格 JSON 行为、重复 key、整数精度、资源上限、诊断、编译体量、峰值内存和解析耗时。
本目录保留三候选选型过程及`dependencies.lock.json`历史快照。yyjson后续已经另行确认为正式内部
依赖，当前Spike与Compiler/Lab统一消费`third_party/yyjson`；nlohmann/json和RapidJSON仍只存在于
实验构建目录，不自动成为产品依赖。

当前状态：Windows x64/MSVC Release 下三个候选各 100 个文档级用例没有非预期变化，稳定 ID/来源 Inventory（清单）为 100/100；独立 Number Token（数字词法单元）语料已扩展到 77 项并闭合本轮数字转换契约，yyjson 另有 10 项 Raw Number（原始数字）到结构诊断的端到端自测。CTest（CMake Test，CMake 测试驱动）当前 20/20 通过。28 个文档用例已经成为机器清单权威，72 个仍由 C++ 生成；5 项独立负向变异门禁证明非法 offset（字节偏移）、目标锁和 Inventory 组合会在 Configure（配置生成）阶段失败。

这里的绿色结果是 Regression Gate（回归门禁）。三个文档级 Runner（运行器）仍包含 3 个`CHARACTERIZATION（现状表征）`，所以 Contract Closure State（契约闭合状态）保持`OPEN`；当前已经没有`OPEN_DECISION（待拍板）`。yyjson 使用`YYJSON_READ_NUMBER_AS_RAW`保留原始数字，已达到并锁定 3 个目标，摘要为`ready_to_promote=3 / open_contract_gaps=0 / target_lock_failures=0`；nlohmann/json 和 RapidJSON 仍各保留 3 个可见缺口。独立 77 项 Number Token 语料的`contract_closure_state=CLOSED`只表示该子契约闭合，不代表生产 Parser（解析器）选型或完整 Strict JSON Profile（严格 JSON 工作规范）已经闭合。

三档资源 Profile（资源档位）各执行 8 个维度的 exact/+1（恰好到上限/超限一级）输入，共 48 项 Parser-stage-only（仅解析阶段）生成型边界；每档还执行 Parser arena（解析器固定内存区）容量充分、硬边界和耗尽清理 3 项检查。当前结果均通过，但 Profile 状态仍是`CANDIDATE_UNVERIFIED（候选、尚未生产验证）`，`capacity_freeze_state=OPEN`、`hash_manifest_state=OPEN`；Loader/Compiler（加载器/编译器）峰值没有测量。最小SchemaIr、PlanBundle和Frozen Execution Plan（冻结执行计划）内部切片已在Windows构建测试；Runtime（运行时）注册仍未实现或运行。

yyjson 的固定内存池和5个解析期分配点逐点故障注入属于选型阶段既有证据。当前正式版本、
License和三份最小随仓文件改由`third_party/yyjson/dependency.lock.json`唯一驱动并在Configure阶段
复核；本目录锁保留为历史选型快照。Linux GCC/Clang实测仍暂缓，未执行前不得宣称Linux兼容。
详见[第十三轮数字与Parser资源门禁报告](results/windows-msvc-2026-round13-number-resource-gate.md)。
