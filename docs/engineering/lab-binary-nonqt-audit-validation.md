# Binary 非Qt总体复核与最小修复

2026-09-13，用户授权启动第1项总体复核，并按适合程度派发子任务。
保留当前工作树已有修改；不接Qt UI、不改变PAE接口、不Stage/Commit/Push。

## 审查分工与结论

三个子任务分别审查生命周期/故障、资源账本、准入/映射/覆盖。初轮均为只读；
随后覆盖子任务仅新增integration_audit_tests.cpp。主任务统一修改生产文件、注册测试、构建验证。
生命周期子任务对修复做了第二次只读定向复核，没有将其静态结论计作运行证据。

发现并关闭两项P2：

1. 错误动作但索引有效的调用仍进入Publish，替换另一个方向的Current。
   Decode/Submit/Continue与Encode四入口现先核对绑定方向和flow；无效路由返回临时错误，
   不发布、不执行Host、不修改目标当前结果。有效路由上的输入失败仍发布本步失败并清旧成功。
2. Decode前置拒绝没有保存Observe得到的generation；Reset后错误返回默认0。
   现于早退前保存generation，覆盖WRONG_INPUT_KIND及RESET_REQUIRED。

主任务先加入对应断言，Debug两个测试分别以
`wrong-direction Decode preserves TX current`、
`Decode early rejection preserves current generation`失败；修复后通过。
后续补四入口方向保护、两种generation=1早拒绝、冻结消费保持及Encode预算精确边界/减1。

## 资源复核边界

没有发现本轮声明范围内可确认的漏计或副本峰值突破：owner/Flow/N+1包络、描述/绑定、
N×B+B当前及在途结果、冻结数组、草稿及scratch、Encode临时NamedValue都有相应计费/容量门禁。
结果指针交换不复制DTO，未发现别流Current或借用被正常跨流执行释放。

不能统一描述为“全部Session实际capacity计费”：适配自有副本采用capacity，
Host继承既有请求存储量/工作区估计报告，排除STL余量、控制块、allocator/debug proxy等。
128MiB实例与256MiB新旧共存是声明范围的计费门禁，不是RSS/物理内存硬上限。
UI、Hex预览、持久化类型编辑器仍只有预算预留，尚无UI执行点证据。

Encode回调失败仍需新Session恢复；不改变Decode-only Reset，不自动重发。
UI侧如何确认放弃其他流状态、准备失败保留旧实例，属于第2项移交细则。

## 新增组合验证

`tests/protocol_lab_binary/integration_audit_tests.cpp`覆盖：

- CRC16固定帧双存储序：123456789独立校验值29B1。
- CRC16动态尾部独立帧A5 06 10 20 47 42；零负载/空覆盖范围A5 04 FF FF。
- 各CRC案执行Host Encode并核验自有TX/实际范围；直接输入独立预期帧Decode，
  损坏CRC后只有诊断帧、无成功字段及mapping；再次正确输入恢复。
- 同Session两条RX流分别保留半包与冻结后缀，Encode成功、输入早拒绝、输出复制失败
  均保持RX观察、Current身份、草稿与选择；Encode故障后两条RX仍能各自完成。
- 非单位Decimal逻辑式raw/2+1：输入3反算raw4；输入3.25拒绝不可精确反算，
  不进入回调故障、不伪造raw、不残留旧TX。

CRC参考向量来自已有Core测试，非本适配自身Encode生成预期；仅在测试内修改Schema/参数，
未修改原fixture。新增覆盖未穷举CRC8/32、动态最大负载CRC组合；原SUM8边界测试保留。

## 执行证据

Windows x64/v142，Debug/Release联合定向回归各11/11通过：7个非Qt Binary测试，
以及Host、Binary Framer、ASCII Framer、Decimal Core。不是仓库全量测试。

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_encode_tests pae_binary_integration_audit_tests pae_binary_saved_state_tests pae_binary_stream_tests pae_binary_prepared_tests pae_binary_owned_description_tests pae_binary_candidate_materializer_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^(pae.protocol_lab_binary.(materializer|description|prepared|stream|saved_state|encode|integration_audit)|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.(contract|ascii_stream_contract))$' --output-on-failure
```

Release替换Debug。PAE_BUILD_TESTING=OFF、PAE_BUILD_PROTOCOL_LAB_UI=OFF的Release内部库构建通过。
重新Configure确认默认新能力OFF且无对应目标；仅开启新能力时按预期在明确依赖门禁失败。
未运行Qt UI、网络、Linux、硬件、Golden、全局OOM穷举或正式性能测试。

## 变更与下一停点

生产改动仅prepared_binary.cpp和prepared_binary_encode.cpp；测试改动为encode_tests.cpp、
stream_tests.cpp、新integration_audit_tests.cpp及测试CMake注册；另更新本报告和文档入口。
没有改Core/Host协议语义、Qt、历史fixture或旧Binary桥。

结论：本次总体复核发现的两项确定缺陷已关闭，可以进入第2项“Binary UI接线与旧桥移交细则”。
这不是UI实施/人工验收通过，也不是完整PAE Runtime或生产准入结论。
下一步明确Schema路径、编辑器与展示预算、Reload/重绑/关闭事务和Encode故障重建提示，
行为变化先确认，再串行实施UI。
