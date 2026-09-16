# Tests

本目录同时包含 PAE 内部验证、experimental 公开 API 验证以及 Lab/消费侧回归，不是公开 SDK
的一部分。建议按边界定位：

- `public_api/`：公开头自包含/私有路径边界、配置编译与 metadata、`COMPLETE_RECORD` Codec、
  消费准备 metadata、`StreamFramer`、`HostEndpoint` 及公开消费示例的专项验证。阶段 1B 最终记录为
  Debug/Release 的 public API CTest 各 5/5、metadata 直测各 21/21、Codec facade 直测各 57/57；
  阶段 2A 修正后 Framer 专项各 38/38、public 定向 CTest 各 6/6；阶段 2B selector 修正后的 Host
  专项各 56/56、受影响定向 CTest 各 10/10。Host 原 52/52 只对应创建期固定 Message 的修正前模型。
- `config_compiler/`、`protocol_core/`、`protocol_framing/`、`host_endpoint/`：PAE 内部契约和实现验证；
  内部 target 通过测试不自动形成稳定公共 API。
- `protocol_lab*`、`business_embedding/`：Lab、旧桥、UI 或业务消费侧验证；不能替代 PAE 公开接口验证，
  也不能用 PAE Core 通过替代 UI 生命周期、真实网络、硬件或现场验收。

需要先理解公开入口时读 [`../include/README.md`](../include/README.md)；需要确认分阶段交付边界时读
[`../docs/engineering/pae-execution-delivery-organization-plan.md`](../docs/engineering/pae-execution-delivery-organization-plan.md)。

`tests/config_compiler`覆盖首个Loader/SchemaIr（加载器/类型化中间表示）草案切片，包括合法Plan、稳定ID Golden Snapshot（黄金快照）、严格JSON代表边界、Structural/Domain Validation（结构/领域校验）诊断和失败时无部分`PlanBundle`。

`tests/config_compiler` 还覆盖`PAE-DEC-033A` Plan计费内存合同：分类和总量守恒、重复编译报告确定性、测试Limit的exact/limit−1、批准估算不一致拒绝，以及Storage Block与全部Arena逻辑分配点的逐序号故障注入和零残留。

`tests/protocol_core`保存PAE（Protocol Adapter Engine，协议适配引擎）首个`COMPLETE_RECORD（完整记录）`Codec（编解码器）内部切片的契约用例和Synthetic Engine Vector（合成引擎向量）。每条向量由人工写定的完整报文字节、独立Decode（解码）期望和动态Encode（编码）输入组成；测试侧只读取这些源文件，不能用PAE Encoder（编码器）生成或覆盖期望字节。`manifest_v0.1.tsv`记录向量身份、证据等级和文件关系；CMake在Configure（配置）阶段及CTest执行前校验六份向量文件的SHA-256（Secure Hash Algorithm 256-bit，256位安全散列算法）。

当前两条向量的证据边界如下：

- `lab_command`和`lab_report`的13/11字节布局、常量、Little Endian（小端）与动态值均为`SYNTHETIC_FROM_SCRATCH（从零人工设计）`；
- 两条向量均为`SYNTHETIC_REVIEWED / INDEPENDENT_ENGINEERING_REVIEWED（人工构造 / 独立工程复核）`，只用于引擎契约检查；
- 向量不包含或映射任何客户协议、生产代码、现场报文、硬件地址或业务标识。

Codec测试由五个CTest入口组成：主合同、首次Decode分配、首次Encode分配、操作计数和共享Plan并发。它们检查`PlanBundle + ExecutionWorkspace（执行工作区）+ pipeline_index`入口、冻结失败不交付部分Plan、确定性Matcher（匹配器）、Plan-scoped Field/Enum Reference（计划作用域字段/枚举引用）、Big Endian/Little Endian（大端/小端）、未知Enum（枚举）拒绝或保留、动态输入与常量、调用方槽位/Buffer（缓冲区）、失败关闭以及Workspace归属/独占边界。Round-trip（往返测试）只能作为辅助回归，不能替代`.frame.hex`的独立逐字节比较。

分配门禁在Plan和Workspace创建后，从第一次Codec调用开始统计replaceable `operator new/new[]`（可替换全局new/new[]）；CMake配置阶段另静态拒绝Core源码直接调用`malloc/calloc/realloc/free`。操作计数只存在于测试专用instrumented目标，用于观察字段数、候选组、固定/BYTES字节数和Enum项增长时的访问次数。并发测试允许多个线程共享只读Plan但要求各自使用独立Workspace和输出区，并验证同一Workspace并发复用会被拒绝。这些证据不等于跨Allocator、跨平台、ThreadSanitizer（线程数据竞争检测器）或正式性能结论。

当前公开层的实际命令、工具链、结果和日志边界见
[`公开 Codec 验证报告`](../docs/engineering/pae-public-codec-slice-validation.md)及
[`消费准备 metadata 验证报告`](../docs/engineering/pae-public-consumer-metadata-validation.md)，
公开 Framer 的候选、consumed/STOP、空推进、Reset、guard、资源和 external consumer 证据见
[`StreamFramer 2A 验证报告`](../docs/engineering/pae-public-framer-stage2a-validation.md)；公开 Host 的
binding/Handle、同 Handle 多 Message、callback 异常隔离、Reset、资源和新版 external consumer
证据见[`Host 2B 最终验证报告`](../docs/engineering/pae-public-host-stage2b-validation.md)。
本说明不把报告中的限定 Windows 结果扩展为本轮重跑、Lab/UI、网络、Linux、硬件、现场或生产验证。
