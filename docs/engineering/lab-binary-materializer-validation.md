# Lab Binary非Qt候选物化子片

日期：2026-09-13；承接独立PAE raw接口实现，用户授权“规划并推进，必要时派发子任务”。
本轮拆分为独立实现、只读预算审查和主任务测试集成；不同时修改ASCII、旧Binary桥或UI。

## 已完成与边界

新增`tools/protocol_lab_binary/candidate_materializer.{h,cpp}`，入口为`MaterializeCandidate`。
只消费本次Schema 0.9 Host候选回调，不执行Decode/Encode，不接管Plan或Session。
返回自有帧、诊断帧、Message/Field ID和索引，以及UINT64、INT64、BOOL、BYTES、ENUM、DECIMAL64。
Decimal拷贝coefficient/scale及独立raw标量，按Plan/Message/Field完整身份关联，不由逻辑值反算。
ENUM复制raw/known/item_id，未知值保留unknown及Core tainted事实；不把tainted等同Codec失败。
冻结Plan没有描述sidecar，`display_text`暂为空，不用ID冒充显示名。

错误关联、重复/缺失raw、错误类型、越界、预算不足或受控复制异常抛出；局部DTO不发布。
在Host回调内调用时，异常交由Host转换为CALLBACK_FAILED，抑制业务交付并要求Reset。
失败候选仅保存诊断帧和DecodeResult，不带上一条成功字段。
BYTES精确校验执行Plan offset/length；bounded payload结合本次frame/header/trailer推导。
DTO无Plan/FieldRef/ByteView指针，Session销毁后仍可读。

**未完成**：Tab/load/session/binding/flow/operation完整身份、自有sidecar/显示文本、物理位mask/高亮、
全Message接受域扫描、双流冻结/Continue/切换/重绑、Encode、UI及人工验收。
这是物化组件，不是已经可供用户操作的Binary Host适配器。

## 局部预算

调用者只能收紧Limits：frame 65536字节、fields 1024、字段BYTES合计1MiB、字符串计费4MiB、
单身份256字节、当前DTO+局部关联scratch总计8MiB。
乘加先查溢出，复制前保守预检，复制后核对实际容器容量。

| CopyBudget项 | 计量 |
| --- | --- |
| object_bytes | CandidateResult自身 |
| frame_bytes | 成功/诊断帧vector capacity之和 |
| field_slot_bytes | fields.capacity × sizeof(FieldValue)，含内联raw/枚举标量 |
| field_bytes | 各字段BYTES capacity之和 |
| string_bytes | Message ID和每字段ID/枚举ID/显示文本capacity+终止符；SSO仍保守计入 |
| temporary_bytes | 固定raw索引表和字段去重表，x64共9216字节 |
| materialization_peak_bytes | 以上当前DTO容量与scratch之和 |

字符串预检为每项`2*长度+32`的保守准入额度，后检按真实capacity；允许保守拒绝。
这不是进程RSS/所有分配器内部内存承诺，也不包括编译器栈帧、MSVC容器调试代理、allocator元数据。
**不包含调用方旧DTO、Plan/Session、sidecar、冻结chunk、输入文本、UI或新旧适配器共存**。
不能用本报告宣称128MiB整实例或256MiB重绑峰值已闭合；整适配须逐份计真实共存副本。

## 检查与回归

新增默认OFF门禁`PAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER`，需显式Host和V05–V09能力。
没有依赖Qt、ASCII适配器、旧ExecutionBridge、UI headless目标或CLI/Evidence。
测试宏`PAE_PROTOCOL_LAB_BINARY_TEST_HOOKS`仅随PAE_BUILD_TESTING启用。

测试使用既有公开合成decimal/int64/bounded配置，在内存中改为0.9；不修改原配置，不扩大运行时Schema。
输入为独立固定字节与SUM8，不以Encode→Decode循环构造唯一期望。
覆盖六种类型、INT64_MIN/UINT64_MAX、正负转换、raw关联、Decimal尾零/零值规范化、已知/未知枚举、
BYTES帧外及帧内错误offset拒绝、bounded零/最大载荷、错误Plan/重复字段/缺raw/缺字段、
逐项限额拒绝、失败诊断与成功结果分离、Session销毁后DTO存活、受控复制异常不半发布。

独立审查检出Decimal scale相等检查错误，新增用例先以
`FAIL: normalized decimal trailing zeros and zero remain valid`失败，修复为允许规范化精度后通过。
运行回归又检出合法unknown enum因tainted误拒绝，保留标记并验证后通过。

初次全局operator new故障注入命中v142 Debug STL的noexcept代理分配，出现CRT弹窗及60秒超时。
已删除该注入方式；未改Qt、运行库或iterator debug等级。
改为物化函数非noexcept边界上的受控bad_alloc注入（结果构造、帧、字段容器、字段复制、最终发布前），
遍历检查点验证CALLBACK_FAILED、零业务交付、旧DTO保留及Reset要求。
这是受控复制异常证据，**不是任意OOM或全部allocator失败点可恢复证明**。

最终独立目录：`out/build/windows-msvc-binary-materializer-noqt`。
Visual Studio 18 2026生成器，x64，v142 14.29.30133，MSVC 19.29.30159.0；UI=OFF。

| 针对性测试 | Debug | Release |
| --- | --- | --- |
| pae.protocol_lab_binary.materializer | PASS | PASS |
| pae.host_endpoint.contract | PASS | PASS |
| pae.protocol_core.decimal_conversion.contract | PASS | PASS |
| pae.protocol_framing.contract | PASS | PASS |
| pae.protocol_framing.ascii_stream_contract | PASS | PASS |

每种配置5/5，不与此前批次累加为全量测试。
`binary-materializer-default-off`默认配置成功且新开关OFF；
`binary-materializer-missing-dependencies`仅开新能力，按预期在依赖门禁配置失败。
另在`windows-msvc-binary-materializer-production`以同套v142、UI=OFF、TESTING=OFF配置，
Release构建`pae_protocol_lab_binary_materializer`通过，验证无测试钩子的编译路径。

复现（仓库根目录，将CONFIG替换为Debug或Release）：

```powershell
cmake -S . -B out/build/windows-msvc-binary-materializer-noqt -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' -DPAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER=ON -DPAE_BUILD_HOST_ENDPOINT_SLICE=ON -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON -DPAE_BUILD_STREAM_FRAMING_SLICE=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON -DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON -DPAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON -DPAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING=ON -DPAE_BUILD_TESTING=ON
cmake --build out/build/windows-msvc-binary-materializer-noqt --config CONFIG --target pae_binary_candidate_materializer_tests pae_host_endpoint_tests pae_decimal_conversion_contract_tests pae_protocol_framing_contract_tests pae_ascii_stream_framing_contract_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C CONFIG -R '^(pae.protocol_lab_binary.materializer|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.contract|pae.protocol_framing.ascii_stream_contract)$' --output-on-failure
```

## 后续顺序与停点

1. 自有描述sidecar、ENUM显示文本、完整身份DTO及physical byte/bit映射；加入跨字节大小端mask向量。
2. 全Message接受域扫描及分项资源报告：Plan/Session分别只计一次，旧当前DTO+临时DTO双份计费。
3. 非Qt Host驱动接入Inspect/双流Submit/Continue/Reset/Encode，加入切换保存和重绑准备预算故障注入。
4. 复核完整实例/准备峰值报告后，再进入DocumentSession/UI及最终人工验收。

当前停止于独立物化检查点，未启动Lab、未改Qt、未Commit/Push，不代表Linux/硬件/现场验证。
