# ASCII完整记录 Windows 验证报告

日期：2026-09-10。代码基线：`1a5467c648f6973c427c2ef0ae970fbb545839c2`，验证对象为其后的
工作树（原始验证时未提交）。状态：`IMPLEMENTED / WINDOWS OFFLINE VERIFIED / REVIEWED`。
2026-09-12总控限定复核完成，用户已授权提交和推送；交付结果以Git历史为准，不构成生产可用声明。

## 实现范围

- 新增默认关闭的`PAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC`。Schema 0.10配置包只接受
  `layout.kind=text`与`encoding=ascii`；0.1～0.9 Binary接受域和旧Plan Snapshot保持不变。
- Decode和Encode动作独立，均由非空literal与可选ASCII BYTES field组成；支持纯字面量完整记录、
  固定长度、紧随非空literal终止的有界变长字段及末尾剩余字段。缺失动作返回
  `OPERATION_NOT_SUPPORTED`。
- Decode先做无字段交付的结构唯一性判断，唯一后校验字符并交付借用输入的BYTES视图；Encode使用
  TX模板组装，并以TX模板重新解析输出、逐字段对照原输入。失败字段数或有效输出长度为0。
- 有界变量使用KMP前缀表查找第一个终止序列。终止`ABA`与值`AB`产生`ABABA`时，第一个匹配位于0，
  Encode返回`ASCII_TERMINATOR_CONFLICT`，不会回溯到位置2。
- Segment、literal、前缀表、字段约束及Pipeline文本候选索引均冻结到Plan Arena并逐类计费；
  Compiler估算、Builder复算和Arena实际使用一致。首个ASCII Encode/Decode未出现可替换
  `new/new[]`分配。
- 首轮不接Protocol Lab、UI、网络或Evidence格式。V10与Lab/UI同时构建在CMake配置期明确拒绝；
  V10关闭的既有Lab对0.10在配置阶段拒绝。

## 专项覆盖

`tests/protocol_core/ascii_text_contract_tests.cpp`使用独立明文常量验证`TX ALICE!Z\r\n`与
`RX ALICE!OK\r\n`，不是只做同实现往返。专项同时覆盖：

- RX/TX模板和字段集合独立、Decode-only与Encode-only字段、单向动作；
- 固定/变长/末尾剩余字段、空值允许边界、首次终止、无回溯、尾随垃圾；
- 非ASCII字节、显式TAB控制字节、长度、缺失/重复/未知字段和非零预填Buffer；
- 输出槽/缓冲区不足、失败零交付、TX最终复核故障注入及随后恢复；
- 零/单/多文本候选，结构歧义不以字符校验消歧；
- Schema/Compiler边界、派生帧资源上限、V10确定性Plan Snapshot；
- Builder对损坏前缀表、资源计数、旧Schema残留和字段类型的发布前拒绝。

Schema正反对照测试还证明公开样例可通过`Test-Json`，而旧0.9声明ASCII、Binary Message混入0.10、
以及第二套literal字节语法均失败。Config Compiler内部合同本轮为117/117，其中新增5项Builder
ASCII控制/损坏Draft用例。

## 2026-09-12纯字面量限定纠错

总控审查后独立复现：Schema 0.10纯字面量消息`fields:[]`在JSON Schema阶段返回false；绕过Schema
进入Compiler时，Debug/Release均在`/messages/0/fields`报告“text fields must contain at least
one field”并退出2。修复前命令输出保存在`out/ascii-pure-literal-repro/before-fix.log`，该证据不
回溯记为原31项矩阵已覆盖。

限定修复仅允许Schema 0.10 Text Message使用空`fields`数组，并同步移除Compiler空数组拒绝、将
Builder字段非空门禁限定到旧Binary Message。动作至少一个、动作segments非空、literal非空、
非空记录、旧Binary字段非空、V10默认关闭和Lab隔离规则均保持不变。公开独立样例为
`examples/config/synthetic_ascii_literal_only.pae.json`，Decode预期`PING\r\n`、Encode预期
`PONG\r\n`。

新增精确自动化覆盖纯字面量Decode-only、Encode-only、独立双动作、零Values Encode、零字段槽
Decode且`field_count=0`、缺失动作、字面量不匹配/截断/尾随垃圾、额外输入拒绝、TX二次复核故障
注入，以及旧Schema 0.1 Binary空字段仍在`/messages/0/fields`以`EMPTY_ARRAY`拒绝。Plan与
Workspace断言字段计费及容量均为0，不削弱非零计数指针检查。

本次只串行重建受影响的Config Compiler与ASCII Core目标，并执行Compiler合同、ASCII Schema合同、
ASCII Core合同：Debug 3/3、Release 3/3通过，日志分别为
`out/ascii-pure-literal-repro/after-fix-debug.log`和
`out/ascii-pure-literal-repro/after-fix-release.log`。改动未触及Lab、Evidence、网络、公共接口或
旧Binary执行实现，因此未无理由重跑历史31项完整矩阵；其结果仍仅作为2026-09-10历史证据。

## 2026-09-12 Schema负例证据纠错

总控只读复现发现，原Binary属性混入负例通过单引号替换串插入了字面反引号，生成的JSON语法无效。
因此原测试结果不能证明目标Schema属性规则生效。现改为对象级添加`frame_length_bytes`，序列化后
先用`ConvertFrom-Json`确认JSON可解析，再断言Schema拒绝；未修改产品源码或Schema接受域。
修复后仅重跑`pae.config_compiler.ascii_text_schema_contract`，Debug/Release各1/1通过，日志为
`out/ascii-schema-negative-debug.log`和`out/ascii-schema-negative-release.log`。未重跑其他矩阵。

## 2026-09-10实际命令与结果（历史）

工具链为Visual Studio 18 2026、MSVC 19.51、Windows SDK 10.0.22621.0；同一构建目录的
Debug/Release严格串行。

```powershell
cmake -S . -B out/build/windows-msvc-ascii-all-slices -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON -DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON `
  -DPAE_BUILD_STREAM_FRAMING_SLICE=ON -DPAE_BUILD_STREAM_FRAMING_EXAMPLE=ON `
  -DPAE_BUILD_BUSINESS_EMBEDDING_EXAMPLE=ON -DPAE_BUILD_ASCII_TEXT_EXAMPLE=ON
cmake --build out/build/windows-msvc-ascii-all-slices --config Debug
ctest --test-dir out/build/windows-msvc-ascii-all-slices -C Debug --output-on-failure
cmake --build out/build/windows-msvc-ascii-all-slices --config Release
ctest --test-dir out/build/windows-msvc-ascii-all-slices -C Release --output-on-failure
```

最终Debug/Release均31/31通过；其中ASCII标签4/4，包括Schema、最小宿主、Core合同和首次调用
无分配。日志为`out/ascii-v10-final-debug.log`和`out/ascii-v10-final-release.log`。

Product-only在`out/build/windows-msvc-ascii-product`完成Debug/Release构建，两配置`ctest -N`
均为0 tests，日志为`out/ascii-v10-product-only.log`。V10+Lab组合按预期配置失败，见
`out/ascii-v10-negative-gates.log`；V10关闭的Lab-on/Testing-off在
`out/build/windows-msvc-ascii-lab-no-tests-v08`完成Debug/Release构建且均为0 tests，随后对公开
0.10样例执行离线Inspect，Debug/Release均退出4并报告`CONFIG_COMPILE_FAILED`，见
`out/ascii-v10-lab-early-rejection.log`。最后一轮Plan Snapshot修改后又串行重建该V10关闭配置，
Debug/Release仍构建通过且均为0 tests，见`out/ascii-v10-lab-no-tests-v08-final.log`。

## 修复过程中的证据边界

首次完整Debug矩阵曾因两个Config Compiler测试目标未继承V10结构布局宏而出现枚举索引损坏：
Compiler合同超时、UI Description失败。按既有V05～V09目标组织补齐V10编译定义后，二者分别在
0.06秒和0.02秒通过；该失败不是旧Schema接受域变化。另一次有意V10+Lab共构建暴露Lab会落入旧
Result执行路径，因此增加了配置期隔离门，未修改Lab Result/Record/Event或指纹。

早期实现期还曾出现Builder常量名编译错误、Encode-only消息候选索引导致的Plan计费不一致，以及
测试预期诊断分类错误；均在最终矩阵前纠正。最终结论只依据上述最终日志，不把早期失败回溯记为通过。

## 验证边界

本轮未执行任何网络收发（包括Loopback）、人工NetAssist、Linux、真实协议Golden、硬件、现场或
正式性能测试。公开样例全部为`SYNTHETIC_FROM_SCRATCH`，不代表任何客户或生产协议。未实现UTF-8、
字符集转换、文本数字转换、CR/LF流分帧、Binary/Text混合Message、Runtime、稳定公共API、Lab/UI
ASCII接入或新Evidence格式。Plan计费和首调用分配测试证明列出的确定性边界，不构成性能结论。
