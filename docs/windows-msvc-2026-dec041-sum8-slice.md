# PAE-DEC-041 SUM8最小切片Windows验证报告

日期：2026-09-06。状态：实现与本轮Windows验证完成，待总控复核；未Stage、Commit或Push。

## 1. 范围和证据边界

本轮只实现`COMPLETE_RECORD`中每Message最多一个、单段、单字节存储的SUM8规则。公开配置、
Values、Frame及歧义样例均为`SYNTHETIC_FROM_SCRATCH`，不包含私有协议、客户信息、生产端点或
现场报文。自动化网络只使用本机Loopback；未执行Linux、真实协议Golden、硬件、现场或新的
人工NetAssist验收。SUM8只能发现部分意外损坏，不提供认证或全面防篡改保证。

## 2. 实现摘要

- Schema 0.3增加Message可选严格`integrity`对象；0.1/0.2继续拒绝该属性。
- Loader、SchemaIr、Domain Validator和Resource Budget验证算法、非空连续范围、受检边界、
  自包含及存储与字段、位容器、固定Matcher的所有权冲突。
- Budgeted Draft和Frozen Plan保存算法枚举、范围及存储偏移；Builder发布前复算规则数量并防御
  损坏Draft。描述符计入现有Arena对象与执行描述符精确布局，执行使用局部累加器，不增加
  Workspace槽。
- Decode在唯一结构候选和槽容量检查后验证SUM8，失败返回`INTEGRITY_FAILED`且字段数为0；
  校验通过后仍执行Enum等字段语义。P2纠正后Lab的Schema 0.3全局Inspect先通过
  Core内部Matcher查询汇总Pipeline/Message结构身份，只在唯一候选时调用一次Decode；
  不以SUM8或字段语义结果消歧。该能力保持为Core/Lab内部接口，未新增稳定公共API。
- Encode写完既有内容后生成SUM8，再从最终Frame复算；任何最终复核失败均保持
  `FINAL_REVIEW_FAILED`和有效长度0。
- Schema 0.3统一使用Result/Record/Event 0.4及独立指纹域，RX Metadata保持0.2。历史格式、
  指纹和夹具不迁移、不重写。

## 3. S01～S14自动化映射

| 编号 | 自动化证据 | 本轮结果 |
| --- | --- | --- |
| S01 | `pae.protocol_core.sum8.contract`独立Frame逐字节Encode/Decode，零值、`0xFF`、模256溢出、动态字段和16位位容器 | PASS |
| S02 | 同一Core用例以非零Buffer和反序Values验证确定性；Lab以额外`integrity`输入命中未知字段 | PASS |
| S03 | 覆盖位翻转、存储字节翻转均为`INTEGRITY_FAILED`；`+1/-1`抵消变化被接受并解码为变化后的字段 | PASS |
| S04 | 范围外固定Matcher字节变化为`UNKNOWN_MESSAGE`，没有误报SUM8失败 | PASS |
| S05 | Core与Schema双层负例覆盖空范围、越界、UINT64边界、自包含、未知算法/属性、字段/位容器/fixed matcher冲突 | PASS |
| S06 | `pae.tools.protocol_lab.inspect_phase`在实际Core调用点计数：零候选和“一条SUM8通过、一条失败”的跨Pipeline多候选均`decode_calls=0`；唯一候选为1；Pipeline反序结果不变；同一Message出现在两Pipeline仍为歧义。同Pipeline相交Matcher仍在Compiler/Builder阶段拒绝 | PASS |
| S07 | 校验失败字段数0、槽容量优先；SUM8通过后的未知Enum仍为`UNKNOWN_ENUM_VALUE` | PASS |
| S08 | instrumented Core一次性损坏存储字节，断言`FINAL_REVIEW_FAILED`且`bytes_written=0` | PASS |
| S09 | 失败Run→Replay A→Replay B均退出5、诊断及比较精确；源清单不变；Fake Adapter调用0且每次仅观察`DECODE_RX` | PASS |
| S10 | 同代替换配置改变覆盖范围，产生可读`DIFFERENT` Frame并再次Replay为`EQUAL` | PASS |
| S11 | 0.3无integrity仍输出0.4；旧0.1/0.2及真实旧离线夹具随完整回归通过；跨代Replay/Run Compare拒绝，Frame比较允许 | PASS |
| S12 | P2纠正后Event未知字段负例先断言目标文件长度/Hash、Record Hash和清单Hash自洽，再精确拒绝`V0.2 event contains unknown property unexpected`；合法Bundle仍可读。原NO_CODEC与历史Transport隔离回归仍通过 | PASS（首次报告中该Event负例只命中了低层长度不一致，不追溯改写） |
| S13 | 规则数进入ResourceRequirements，Builder复算与损坏Draft拒绝；首次SUM8 Encode/Decode无可替换`new`分配；Decode N、Encode 2N计数精确 | PASS（操作上界，不是性能结论） |
| S14 | Protocol Lab及全切片Debug/Release串行通过；Product-only和Lab-on/Testing-off两配置构建通过且CTest为0 | PASS |

PowerShell `Test-Json`按JSON Schema数学整数语义可能接受`1.0`；PAE Loader仍依据原始Token以
`INTEGER_NOT_EXACT`拒绝。Schema正反例使用`1.5`验证双方共同边界，完整Compiler语料继续覆盖
词法浮点拒绝。该差异是既有书面契约，不是本轮放宽。

## 4. 实际命令与结果

专项回归：

```powershell
ctest --test-dir out/build/windows-msvc-all-slices-dec041 -C Debug `
  -R "sum8|dec041|config_compiler.contract" --output-on-failure
```

结果为`6/6 PASS`。首次新增Builder防御后，36个Config Compiler子用例均通过，但旧总数门槛仍为
33，测试门禁按预期失败；修正门槛为实际36后通过。另一次Enum测试最初缺少Schema要求的
`display_name`，因此准备阶段失败；补齐合法Enum样例后才证明SUM8通过后的语义失败顺序。

Protocol Lab（首次实现验证，历史记录）：

```powershell
cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-debug --parallel
ctest --preset windows-msvc-protocol-lab-debug
cmake --build --preset windows-msvc-protocol-lab-release --parallel
ctest --preset windows-msvc-protocol-lab-release
```

Debug和Release均为`10/10 PASS`。其中UDP完整回归只访问本机Loopback。该记录中
unknown-Event-field负例存在上述长度未同步的证据限制，不将其追溯解释为语义校验覆盖。

全切片共存：

```powershell
cmake --build out/build/windows-msvc-all-slices-dec041 --config Debug --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec041 -C Debug --output-on-failure
cmake --build out/build/windows-msvc-all-slices-dec041 --config Release --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec041 -C Release --output-on-failure
```

Debug和Release均为`40/40 PASS`，两种配置在同一目录中串行执行。

隔离构建：

```powershell
cmake -S . -B out/build/windows-msvc-product-only-dec041 -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_JSON_PARSER_SPIKE=OFF `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=OFF -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_BUILD_PROTOCOL_LAB=OFF
cmake --build out/build/windows-msvc-product-only-dec041 --config Debug --parallel
cmake --build out/build/windows-msvc-product-only-dec041 --config Release --parallel

cmake -S . -B out/build/windows-msvc-lab-no-tests-dec041 -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_JSON_PARSER_SPIKE=OFF `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=OFF -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=OFF `
  -DPAE_BUILD_PROTOCOL_LAB=ON
cmake --build out/build/windows-msvc-lab-no-tests-dec041 --config Debug --parallel
cmake --build out/build/windows-msvc-lab-no-tests-dec041 --config Release --parallel
```

两类Debug/Release构建均成功，CTest列举均为0。Product-only只生成Plan/Core产品目标；
Lab-on/Testing-off生成Lab但不生成测试目标。

日志位于`out/review/dec041-final/`：

- `protocol-lab-debug-ctest.log`、`protocol-lab-release-ctest.log`；
- `all-slices-debug-ctest.log`、`all-slices-release-ctest.log`；
- `product-only-*-build.log`、`lab-no-tests-*-build.log`及对应配置/CTest清单日志。

## 5. P2纠正与复核（2026-09-06）

独立复现证明原`unknown-event-field`样例的Event实际为448字节，Run Record仍记录
432字节，Reader因`Run Record payload does not match the recorded file`退出3。修正
`copy_mutate_v4_file`后，Record的长度为448，文件Hash与Record一致，Record自身Hash也已同步
到`SHA256SUMS`；此时Reader精确拒绝未知Event属性。因此P2-1是测试证据缺口，
未复现Reader产品缺陷。修复前后原始输出分别保存于
`out/review/dec041-p2/pre-fix-*`和`post-fix-*`。

Schema 0.3的全局Inspect原实现在候选汇总前逐Pipeline调用`DecodeCompleteRecord`，
静态确认可提前执行SUM8及字段解析。现改为两阶段：第一阶段只调用Core内部
结构Matcher，第二阶段仅对唯一Pipeline/Message身份调用一次Decode。内部计数用例实际
断言零候选和跨Pipeline多候选`decode_calls=0`，唯一成功、唯一完整性失败及无integrity
新代消息均`decode_calls=1`；后者仍保留既有失败优先级。
结构查询不分配Workspace或堆内存，其遍历上界受已计费的Pipeline、candidate group与Matcher
数量限制。唯一候选进入Decode时，Core仍保留一次防御性结构复核；现有Workspace操作计数只表示
单次Codec调用，不将Lab编排层的前置查询冒充为Codec操作。本轮未新增Plan或Workspace资源槽。

实际串行执行：

```powershell
ctest --test-dir out/build/windows-msvc-protocol-lab -C Debug `
  -R "pae.tools.protocol_lab.(inspect_phase|sum8_generation_contract)" --output-on-failure
ctest --test-dir out/build/windows-msvc-protocol-lab -C Release `
  -R "pae.tools.protocol_lab.(inspect_phase|sum8_generation_contract)" --output-on-failure
ctest --preset windows-msvc-protocol-lab-debug --output-on-failure
ctest --preset windows-msvc-protocol-lab-release --output-on-failure
ctest --test-dir out/build/windows-msvc-all-slices-dec041 -C Debug --output-on-failure
ctest --test-dir out/build/windows-msvc-all-slices-dec041 -C Release --output-on-failure
```

针对性Debug/Release均`2/2 PASS`；完整Protocol Lab Debug/Release均`11/11 PASS`；全切片
Debug/Release均`41/41 PASS`。Product-only和Lab-on/Testing-off均重新配置并完成Debug/Release
构建，CTest列举均为0。本轮日志位于`out/review/dec041-p2/`。完整Lab中的Socket仅为
既有本机Loopback自动化；未重复人工NetAssist验收。

## 6. 未验证与结论边界

- 未执行Linux、非Loopback网络、真实设备、现场、真实协议Golden或新的人工Lab验收。
- 没有吞吐、时延或嵌入式性能测量；N/2N和零分配只是当前实现的操作/分配边界。
- 真实协议仍缺完整可追溯接收算法及合并单元格映射证据；公开人工样例不能关闭该门禁。
- Runtime/Session聚合资源准入、完整Receive Gate、CRC和其他完整性算法仍不在本切片。

本轮实现和验证完成，但是否可Stage/Commit仍由总控独立审查决定。
