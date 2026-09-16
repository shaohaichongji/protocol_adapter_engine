# Binary 非Qt Host Encode检查点

2026-09-13，按用户“推进下一步”完成既定非QtEncode子片。不接UI、不改Qt、不Commit/Push。

## 实现与身份

- `prepared_binary.h`增加调用期类型化EncodeInput、自有ObservedEncode及EncodeIssue。
  输入仅接收稳定字段ID和枚举项ID，不接受调用者提供的FieldRef、EnumValueRef或Plan指针。
- `prepared_binary_encode.cpp`检查绑定方向、Message归属、必填INPUT字段、重复/非法字段、
  逻辑类型、枚举ID和BYTES指针；在当前Session内解析枚举引用，恰好调用一次Host Encode。
  常量和computed_length不接受外部输入；数值范围、Decimal转换、负载长度和校验由Core判定。
- 成功输出在回调内核验Plan、Message、generation、帧长，复制TX帧与逻辑输入，
  形成所有字段的实际byte/bit范围及实际校验区间。沿用当前结果指针交换发布，无结果历史。
  失败发布清除旧成功结果，不修改调用者输入，也不伪造Decode候选或转换raw整数。
- `prepared_binary.cpp`统一清理RX/TX暂存包络；`resource_budget.cpp`仅补计费注释。
  新源文件及`encode_tests.cpp`分别在内部库与测试CMake注册。

## 预算

输入复制前检查字段数、身份长度、累计BYTES/字符串、最大输出帧及全字段映射预估；
复制后按vector/string实际capacity复核。单份当前TX（输入、帧、映射）受B约束，
串行在途TX加Host NamedValue临时数组同受B约束，消费既有N×B+B预留。
原E×(B+C)仍保守保留给后续持久化Encode编辑器，不据此宣称Qt编辑器预算已实现。
owner包络按更新后的sizeof(ObservedOperation)计费；实例128MiB/新旧256MiB门禁不变。
外部显式副本、allocator/debug proxy及进程RSS不在该计费承诺内。

## 故障恢复边界

定向验证发现Host Reset仅接受Decode。Encode输出回调复制失败时，Core可能已成功，
但Host报告CALLBACK_FAILED、successful_outputs=0，且reset_required=true；本层清除未完成TX。
继续Encode被拒绝，Reset同样不能恢复Encode。必须通过现有重新准备Session路径恢复，
并使用新的instance身份；不得在UI上提示“点Reset即可恢复”，也不得自动重发。
本轮未扩大PAE Reset接口。UI重载/重绑事务及影响其他流的确认仍在下一阶段处理。

## 验证

新增`tests/protocol_lab_binary/encode_tests.cpp`：

- UINT64、负INT64、BOOL、已知ENUM、BYTES、DECIMAL64、位字段、常量与独立预期TX字节。
- 输入缺失、重复、常量被传入、枚举ID无效、逻辑类型错误、空指针、跨Pipeline Message和错误方向。
- Core数值超界失败；零输入常量消息；有界负载0/最大/超限、computed_length和动态SUM8区间。
- 输入/输出自有副本、原Session销毁后的外部副本；复制预算在Core前拒绝。
- 输出复制故障注入保留Core执行事实、清理半份TX、隔离Decode流、拒绝继续Encode，重建Session恢复。

首次Debug测试因错误假设Encode可Reset而失败，测试中未检查optional导致CRT断言等待并超时。
修正为验证真实Host边界，并让Debug断言输出stderr、检查optional后，定向用例通过。
没有改Host以迎合错误预期。

Windows x64/v142：Debug/Release联合回归各10/10通过（6个Binary非Qt测试及Host、
Binary Framer、ASCII Framer、Decimal Core）。PAE_BUILD_PROTOCOL_LAB_UI=OFF且
PAE_BUILD_TESTING=OFF的Release内部库构建通过。

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_encode_tests pae_binary_saved_state_tests pae_binary_stream_tests pae_binary_prepared_tests pae_binary_owned_description_tests pae_binary_candidate_materializer_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^(pae.protocol_lab_binary.(materializer|description|prepared|stream|saved_state|encode)|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.(contract|ascii_stream_contract))$' --output-on-failure
```

Release替换Debug。新适配层本轮独立预期校验向量为SUM8，不宣称CRC组合已逐一复验。
未做Qt人工验收、全局OOM穷举、Linux、硬件、Golden或正式性能验证。

## 下一停点

先统一复核非Qt生命周期、TX故障恢复、预算执行点与旧Binary桥移交契约，明确Qt编辑器、
Hex预览、跨Tab/Reload/关闭事务的剩余范围。复核闭合前不进入UI接线。
