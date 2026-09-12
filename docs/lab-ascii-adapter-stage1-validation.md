# Lab ASCII 离线适配第1阶段验证记录

日期：2026-09-12。基线：`main@256c5b7318f73effe7834800dc2c14f4a9466f7b`，并保留
`MERGE_HEAD=8bfe50fcb71a2e572bdc907b7392803038e6afdb`的未提交合并状态。
第1阶段结束时状态（历史）：内部非持久化适配已实现并完成Windows针对性验证，待总控复核和后续UI接线；不构成Commit、
Push、人工UI验收或生产可用声明。

后续UI接线及限定验收已完成，当前进展见[第2阶段验证报告](lab-ascii-ui-stage2-validation.md)。
本文保留第1阶段原工具链、结果与未验证边界，不将后续UI证据倒写为本阶段结果。

## 接口与所有权

- 头文件：`tools/protocol_lab_ascii/ascii_offline_adapter.h`。
- 命名空间：`pae::protocol_lab::ascii`。
- CMake目标：`pae_protocol_lab_ascii_adapter_internal`；默认关闭选项为
  `PAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER`，并要求`PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON`。
- 调用方先以`OfflineAdapter::Supports`按Schema分流，再由
  `OfflineAdapter::AdoptCompiledArtifacts`按值接收并唯一转移`CompiledUiArtifacts`，避免旧Binary
  Artifacts被提前move或同一Plan被double-adopt。适配器按声明顺序
  拥有Plan、Compiler UI Sidecar、自有Description DTO和借用Plan的Workspace；析构时Workspace先于
  Sidecar与Plan销毁，不存在第二次Adopt。
- `DocumentDescription`复制协议、Pipeline、Message和Field作者元数据；wire与动作事实从Frozen Plan
  读取。每个动作包含可用性、长度边界、有序literal/field segment与引用字段顺序；Field包含长度、
  控制字节及RX/TX引用属性；Pipeline分别保留作者Message集合和实际Decode候选集合。
- `ExecutionIdentity`携带document/load/plan/selection/input修订及Pipeline/Message身份。`Inspect`不接受
  Message选择，继续由Core执行结构唯一性；`Encode`要求精确Pipeline和Message身份。
- `ExecutionResult`拥有有效Frame、Field字节和已验证实际范围。失败清空有效Frame/Field；Inspect原始
  输入只存放在明确命名的`diagnostic_input_frame`。可用的Message、failed input/field身份才会复制，
  不猜测Core未返回的偏移或字段。
- Encode只调用一次`EncodeCompleteRecord`并标记复核种类`TX_TEMPLATE`，不调用RX Decode、不生成
  `review_decode`事件。缺失Encode动作或Pipeline无Decode候选仍实际调用Core并返回
  `OPERATION_NOT_SUPPORTED`；纯字面量零字段通过独立成功状态和空Field集合表达。

旧Binary继续使用既有v06桥；ASCII适配器明确拒绝非0.10的Compiled UI Artifacts。未修改v06 Result、
普通CLI、Evidence、Replay/Compare、QWidget、DocumentSession或输入编辑器。

## 自动化覆盖

`tests/protocol_lab_ascii/ascii_offline_adapter_tests.cpp`使用公开合成配置验证：

- 独立`RX ALICE!OK\r\n`与`TX ALICE!Z\r\n`字节，不依赖同实现往返；
- Sidecar作者元数据、Frozen Plan动作/segment、TX输入字段顺序、字段长度和控制字节、Pipeline Decode
  候选及最大记录容量；
- Inspect的Core唯一性/Decode、借用ByteView边界验证、输入销毁后的自有Frame/Field；
- Encode实际长度投影、结果自有、`TX_TEMPLATE`，以及终止`ABA`与值`AB`的
  `ASCII_TERMINATOR_CONFLICT`透传；
- 纯字面量零输入Encode、零槽/零字段Inspect、Decode-only、Encode-only及
  `OPERATION_NOT_SUPPORTED`；
- 零长度字段的`offset=0,length=0`空范围；非法Pipeline身份在Core前拒绝；非ASCII失败只保留诊断
  输入；旧Schema 0.1 Binary仍编译并由ASCII适配器拒绝，以便交给旧桥。

## Windows针对性验证

使用Visual Studio 18 2026生成器、x64，在独立目录
`out/build/windows-msvc-lab-ascii-adapter`配置。Debug/Release串行执行：

```powershell
cmake -S . -B out/build/windows-msvc-lab-ascii-adapter -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON -DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON `
  -DPAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=ON
cmake --build out/build/windows-msvc-lab-ascii-adapter --config Debug `
  --target pae_protocol_lab_ascii_adapter_tests
ctest --test-dir out/build/windows-msvc-lab-ascii-adapter -C Debug --output-on-failure `
  -R '^pae\.tools\.protocol_lab_ascii\.adapter$'
cmake --build out/build/windows-msvc-lab-ascii-adapter --config Release `
  --target pae_protocol_lab_ascii_adapter_tests
ctest --test-dir out/build/windows-msvc-lab-ascii-adapter -C Release --output-on-failure `
  -R '^pae\.tools\.protocol_lab_ascii\.adapter$'
```

Debug 1/1、Release 1/1通过，日志为`out/lab-ascii-adapter-debug.log`和
`out/lab-ascii-adapter-release.log`。负门禁还确认适配选项未启用V10时配置退出1，V10与普通
Protocol Lab CLI同时启用时仍配置退出1，日志为`out/lab-ascii-adapter-gates.log`。

本阶段未修改Core/Schema执行语义，也未接UI、Qt、普通Lab或Evidence，因此没有重跑引擎历史31项或
完整UI矩阵。后续UI必须在同一Qt 5.13.x/v142构建树中从源码重建本目标，禁止复用本次MSVC 19.51
二进制以混合ABI；共享V10布局宏由`pae::protocol_plan`公开依赖传播。

## 边界

尚未实现ASCII escaped/Hex编辑与切换、UTF-16错误位置、输入容量整次拒绝、DocumentSession路由、
Tab修订门禁、QTableView范围高亮、UI动作禁用或人工UI验收；这些属于后续Lab阶段。本阶段未运行网络、
Linux、Golden、硬件、现场或性能验证，也未将旧Result/Record/Event扩展到Schema 0.10。
