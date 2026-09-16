# Binary 非Qt每流草稿与当前结果保存门禁

2026-09-13，用户授权推进上一检查点的下一步。本轮不接UI、不改Qt、不Commit/Push。
保留此前PAE及非Qt子片未提交修改。

## 实现

修改`tools/protocol_lab_binary/prepared_binary.h/.cpp`：

- 每流持有一份当前ObservedOperation；Decode、Submit、Continue在同一内部暂存对象中执行，
  完成后交换所有权指针发布，立即释放被替换候选，没有结果历史、队列或第二次Decode。
  只保存真实调用结果，不开放外部DTO导入/覆盖接口。
- 执行入口由返回值改为返回const引用；Current提供当前结果的只读借用。
  目标流下次执行或Reset、实例销毁后借用失效；无效路由返回暂存结果，下次执行即失效。
  调用者显式复制仍可得到自有值，但任意外部副本不在本实例预算中。当前只有内部非Qt测试消费此API。
- 无候选、协议失败、输入早拒绝、回调失败各自保存本步事实，不保留上次成功字段/高亮。
  无效路由不发布到任何流。回调失败仍依Host实际消费推进冻结游标并要求Reset。
- 每流保存不解释的UTF-16草稿；SaveAndSelect先检查目的绑定/flow与源草稿长度，再暂存、发布。
  非法/未完成Hex可保存，解析仍属于提交边界。空草稿、同流保存、借用草稿作为输入均支持。
  切换只保存草稿和选择，不执行Host、不Reset、不复制候选，不改变其他流和冻结后缀。
- Reset成功后清目标流草稿、当前结果、冻结游标；不清其他流，也不自动改变选择。

## 预算与发布顺序

沿用128MiB实例、256MiB同Tab新旧准备门禁：

- Flow容器实际capacity×sizeof(Flow)已计入owner；新增N+1个ObservedOperation包络在owner中计费，
  并于预算通过后分配。内嵌候选对象与既有DTO账本有保守重叠，不宣称最紧内存估计。
- 当前候选动态存储受每份B上限约束；所有流最多N份，串行回调最多另1份，消费N×B+B预留。
  回调局部容量及临时峰值检查仍执行；切换不复制候选，因此没有新的候选副本峰值。
- 流式草稿上限为4C个UTF-16 code unit，C=min(65536,effective_max_submit_bytes)；
  完整记录及尚未接线的Encode通道采用4×65536上限。草稿原文空间/分隔符同样计入上限。
- 创建时分配每流上限+1的char16_t数组及一份全局4×65536+1暂存数组。
  分配前按实际数组大小（含终止符）累加核验utf16_draft_reserve，消费原有N份草稿加切换临时预留。
  不再将同一数组另加resident，不在保存时扩容。
- 保存阶段长度超限或目的路由无效在发布前拒绝；暂存后注入故障也不发布。
  发布阶段仅固定数组复制和标量赋值，无分配或回调；旧草稿、当前选择、候选与Host流保持一致。

因此非Qt“切换保存”已落地为容量检查与不复制结果的所有权约束，而非重新信任外部DTO自报预算。
尚不能据此宣布UI视图、Hex预览、Encode及旧Binary桥移交的总预算闭合；allocator/debug proxy、
任意外部副本、Compiler前置解析内存仍不是本计费模型的RSS承诺。

## 验证

新增`tests/protocol_lab_binary/saved_state_tests.cpp`及测试注册：

- 初始状态、非法Hex原文、空草稿、别名草稿、同流与跨绑定/flow切换。
- 128 code unit上限通过、加1拒绝；目的路由错误和暂存后/发布前故障注入保留旧状态。
- 保存过程不执行Host、不推进冻结后缀；各流当前结果独立、真实结果对象不复制。
- 成功后早拒绝/无候选清除旧成功；失败诊断及CALLBACK_FAILED保留正确事实。
- Reset只清目标；重绑预算拒绝保留旧草稿、半包和当前结果；新实例不继承旧结果。
- 原prepared测试继续核验完整记录、枚举/范围、精确预算及减1、关联失败等旧路径。

Windows x64/v142：Debug/Release各9/9通过，范围为saved_state及既有materializer、description、
prepared、stream、Host、Binary Framer、ASCII Framer、Decimal Core定向测试。
`PAE_BUILD_PROTOCOL_LAB_UI=OFF`、`PAE_BUILD_TESTING=OFF`的Release内部库构建通过。
故障测试仅模拟保存发布前失败，不等同于全局allocator OOM穷举。

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_saved_state_tests pae_binary_stream_tests pae_binary_prepared_tests pae_binary_owned_description_tests pae_binary_candidate_materializer_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^(pae.protocol_lab_binary.(materializer|description|prepared|stream|saved_state)|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.(contract|ascii_stream_contract))$' --output-on-failure
```

Release替换Debug。未做Qt UI人工验收、Linux、硬件、Golden或正式性能验证。

## 下一停点

下一步补非Qt Host Encode输入身份解析、输入/输出副本预算及一次执行结果映射。
后续状态：该非Qt子片已完成，见[Encode验证记录](lab-binary-encode-validation.md)；
Encode故障恢复需新Session，不能套用本节Decode Reset语义。
之后统一复核生命周期和旧Binary桥替换准备，再串行进入Qt UI接线与人工验收。
UI修订号、延迟编译发布、关闭/Reload确认及跨Tab视图事务仍须在接线阶段验证。
