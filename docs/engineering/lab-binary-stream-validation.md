# Binary 非Qt双流冻结驱动验证

后续状态：[每流保存门禁](lab-binary-saved-state-validation.md)已接入不解释的草稿和当前结果；
执行入口改为返回当前结果的const借用，下面保留流驱动检查点当时的范围。

2026-09-13，用户同意下一步。本轮保留此前未提交修改，不接UI、不修改Qt、不Commit/Push。

## 实现与边界

`tools/protocol_lab_binary/prepared_binary.h/.cpp`增加Submit、Continue、Stream。
输入是已解析的字节；Hex校验、草稿、当前结果保存及UI视图尚不属于本入口。

- 创建时为每个流式Decode通道分配C=min(65536,effective_max_submit_bytes)的自有数组。
  数组在总预算准入后分配，消费已有D×65536冻结预留，不再次计入resident；
  Flow容器实际capacity×sizeof(Flow)额外计入owner。没有运行时扩容或第二份冻结临时数组。
- Submit先核对路由、Reset状态、输入指针/非空及容量，再冻结输入，最多调用一次Host Push。
  存在后缀或内部待处理工作时拒绝新Submit，不覆盖旧后缀。
- 成功和失败候选均STOP；依据Host实际bytes_consumed推进游标。输入全部消费后清逻辑游标，
  数组容量仍归当前实例拥有。Continue只推进未消费后缀；只有has_internal_work才允许空Push。
  buffered_bytes非零的普通半包不允许空Continue，不自动循环、不强制刷新。
- 回调物化沿用实际调用绑定、flow、generation、operation及当前Plan身份；不执行第二次Decode。
  物化失败不发布候选、不交付业务输出，仍保留实际消费进度，要求Reset。
- Reset成功后只清目标流的冻结游标；其他流半包不变。接口保持串行约定，不增加线程安全承诺。

返回的ObservedOperation属于调用者，当前结果/草稿保存门禁尚待实现；任意外部保留副本不在实例预算内。
创建中分配失败不会发布新对象，但未做全局allocator故障注入；不是进程RSS或任意OOM可恢复承诺。

## 验证

新增`tests/protocol_lab_binary/stream_tests.cpp`及CTest目标`pae.protocol_lab_binary.stream`：

- 三种Binary策略、两条交错逻辑流、半包不可空刷新。
- 成功→失败→成功逐候选STOP、精确冻结游标、新Submit拒绝覆盖待处理后缀。
- 同步头噪声丢弃、定长失败不额外重同步、声明长度策略后缀续推。
- 32字节容量的33字节早拒绝、无效指针及路由拒绝。
- 工作预算截断、无输入但同步头内部复制尚待完成的空Continue。
- 限制候选复制预算触发CALLBACK_FAILED：不半发布、不交付业务、保留实际消费、故障不污染另一流。
- Reset清冻结后缀，保留另一流半包及generation；真实回调flow身份检查。

Windows x64/v142：Debug与Release各8/8通过，范围为新增stream及既有materializer、description、
prepared、Host、Binary Framer、ASCII Framer、Decimal Core定向回归。
`PAE_BUILD_PROTOCOL_LAB_UI=OFF`、`PAE_BUILD_TESTING=OFF`的Release内部库构建通过。

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_stream_tests pae_binary_prepared_tests pae_binary_owned_description_tests pae_binary_candidate_materializer_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^(pae.protocol_lab_binary.(materializer|description|prepared|stream)|pae.host_endpoint.contract|pae.protocol_core.decimal_conversion.contract|pae.protocol_framing.(contract|ascii_stream_contract))$' --output-on-failure
```

Release替换Debug。未进行UI人工验收、Linux、Golden或硬件验证。

## 下一停点

先闭合非Qt每流草稿/当前结果保存及切换预算门禁，再补Host Encode输入输出路径与预算。
随后评审整体生命周期与旧Binary桥替换准备，最后才进入Qt UI接线和人工验收。
本报告不把冻结门禁完成等同于契约要求的四个生命周期预算点全部闭合。
