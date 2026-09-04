# Tests

`tests/config_compiler`覆盖首个Loader/SchemaIr（加载器/类型化中间表示）草案切片，包括合法Plan、稳定ID Golden Snapshot（黄金快照）、严格JSON代表边界、Structural/Domain Validation（结构/领域校验）诊断和失败时无部分`PlanBundle`。

`tests/config_compiler` 还覆盖`PAE-DEC-033A` Plan计费内存合同：分类和总量守恒、重复编译报告确定性、测试Limit的exact/limit−1、批准估算不一致拒绝，以及Storage Block与全部Arena逻辑分配点的逐序号故障注入和零残留。

`tests/protocol_core`保存PAE（Protocol Adapter Engine，协议适配引擎）首个`COMPLETE_RECORD（完整记录）`Codec（编解码器）内部切片的契约用例和Synthetic Engine Vector（合成引擎向量）。每条向量由人工写定的完整报文字节、独立Decode（解码）期望和动态Encode（编码）输入组成；测试侧只读取这些源文件，不能用PAE Encoder（编码器）生成或覆盖期望字节。`manifest_v0.1.tsv`记录向量身份、证据等级和文件关系；CMake在Configure（配置）阶段及CTest执行前校验六份向量文件的SHA-256（Secure Hash Algorithm 256-bit，256位安全散列算法）。

当前两条向量的证据边界如下：

- `lab_command`和`lab_report`的13/11字节布局、常量、Little Endian（小端）与动态值均为`SYNTHETIC_FROM_SCRATCH（从零人工设计）`；
- 两条向量均为`SYNTHETIC_REVIEWED / INDEPENDENT_ENGINEERING_REVIEWED（人工构造 / 独立工程复核）`，只用于引擎契约检查；
- 向量不包含或映射任何客户协议、生产代码、现场报文、硬件地址或业务标识。

Codec测试由五个CTest入口组成：主合同、首次Decode分配、首次Encode分配、操作计数和共享Plan并发。它们检查`PlanBundle + ExecutionWorkspace（执行工作区）+ pipeline_index`入口、冻结失败不交付部分Plan、确定性Matcher（匹配器）、Plan-scoped Field/Enum Reference（计划作用域字段/枚举引用）、Big Endian/Little Endian（大端/小端）、未知Enum（枚举）拒绝或保留、动态输入与常量、调用方槽位/Buffer（缓冲区）、失败关闭以及Workspace归属/独占边界。Round-trip（往返测试）只能作为辅助回归，不能替代`.frame.hex`的独立逐字节比较。

分配门禁在Plan和Workspace创建后，从第一次Codec调用开始统计replaceable `operator new/new[]`（可替换全局new/new[]）；CMake配置阶段另静态拒绝Core源码直接调用`malloc/calloc/realloc/free`。操作计数只存在于测试专用instrumented目标，用于观察字段数、候选组、固定/BYTES字节数和Enum项增长时的访问次数。并发测试允许多个线程共享只读Plan但要求各自使用独立Workspace和输出区，并验证同一Workspace并发复用会被拒绝。这些证据不等于跨Allocator、跨平台、ThreadSanitizer（线程数据竞争检测器）或正式性能结论。

本说明只描述测试资产和证据边界，不声明当前Codec已经构建、测试或运行通过；执行结论必须来自单独记录的实际命令和结果。
