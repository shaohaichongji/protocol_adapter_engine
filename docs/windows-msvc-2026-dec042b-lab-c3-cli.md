# DEC-042B Lab C3 CLI Windows验证报告

日期：2026-09-08

状态：`IMPLEMENTED / AUTOMATED OFFLINE VERIFIED / MANUAL NOT_EVALUATED`

基线：`main` / `aa1d66393ff2aafbf8be353c8a352a8f6fd5486b`

## 1. 实施结论

在默认关闭的`PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05`开关下，现有`pae_protocol_lab`接通Schema
0.5的C1执行与C2 Evidence 0.7能力。新链提供inspect、encode、replay、compare，前三者强制
`--record-root`；Replay拒绝配置替换，Compare只读且不发布。终端新增独立
`pae.lab.cli/0.1`封装，进程退出和`--expect-status`不改变Result 0.6或确定性指纹。

旧Schema 0.1～0.4继续走原入口。本轮没有修改Core、Compiler、Result 0.6、Record/Event 0.7、
旧指纹算法或网络实现，也未运行UDP测试。

收口自审补齐两项入口证据：显式record-root下的未知Schema版本由C3封装按配置失败/退出4处理，
已知0.1～0.4仍走旧链；Record 0.7缺失但Event 0.7仍在的损坏Bundle继续由C3 Reader拒绝，
退出3且不发布子Run。

## 2. 自动化覆盖

`pae.tools.protocol_lab.c3_cli`执行命令级断言：

- Encode/Inspect精确Frame与Decimal路径；
- Run→Replay A→Replay B均EQUAL；
- 等价Decimal独立Compare为EQUAL，改值为DIFFERENT/退出6；
- Compare前后Run目录计数不变，证明命令级零发布；
- 普通转换失败默认退出5、期望匹配退出0，但Result状态、`exit_code=5`及指纹不变；
- 失败Run Replay默认退出5，期望匹配退出0，历史比较仍EQUAL；
- SUM8失败及期望匹配保持原Result失败；
- Values内容失败发布可读无Result完整记录，进程退出5；该记录不可Replay且不发布子Run；
- Values文件读取失败退出3且不发布；
- 参数、缺record-root、Replay替换、Compare期望、跨代Compare、未知Schema/配置准备、Writer失败、损坏
  Evidence等退出和诊断；损坏Evidence不发布子Run；
- Schema 0.4仍输出Result 0.5，help/version仍成功。
- 缺省text输出包含CLI 0.1终态封装及完整Result 0.6。

`pae.tools.protocol_lab.c3_policy`直接覆盖纯退出策略，包括Writer 7、内部10、普通失败5、差异6、
期望匹配0，以及Writer/内部优先级。合法命令难以触发的内部分支只有策略级证据，不伪造成
命令级动态故障证据。

总控复核后关闭一项绑定层P2：`SelectProcessExit`对内部退出10有意保留
`expectation.matched=null`，原`BindExecution`却在仅判断期望参数及主Codec存在后解引用该空
optional。该问题由静态控制流确认；修复前没有合法CLI故障接缝，未声称本机实际崩溃。修复仅在
`matched.has_value()`时处理期望诊断。Testing-only绑定接缝直接构造并序列化两条真实封装路径：
期望OK加主Codec INTERNAL_ERROR时退出10、保留原失败Result；主Encode OK加Review/物化失败时
退出10且Result为null。两者JSON中的`expectation.matched`均为null，稳定内部诊断不被
EXPECTATION_MISMATCH覆盖；Writer覆盖内部失败仍由纯策略断言为7。

## 3. 实际命令与结果

专用测试构建共同参数为：Lab、Loader、Core、Schema 0.5 Compiler及C3开关开启，Testing和
v06/v07相关测试开启；Debug与Release使用独立Ninja目录，并由Visual Studio 2026 Developer
PowerShell提供MSVC环境。

| 验证 | 结果 | 日志 |
| --- | --- | --- |
| Debug C3专项 | 2/2 PASS | `out/dec042b-c3-debug-targeted-p2-optional-final.log` |
| Debug完整离线（`ctest -LE udp`） | 35/35 PASS | `out/dec042b-c3-debug-offline-full-p2-optional-final.log` |
| Release C3专项 | 2/2 PASS | `out/dec042b-c3-release-msvc-targeted-p2-optional-final.log` |
| Release完整离线（`ctest -LE udp`） | 35/35 PASS | `out/dec042b-c3-release-msvc-offline-full-p2-optional-final.log` |
| C3 Testing-off Debug/Release | 构建PASS，各0测试 | `out/dec042b-c3-testing-off-*-build-final-current.log`及对应注册日志 |
| Product-only Debug/Release | 构建PASS，各0测试 | `out/dec042b-c3-product-only-*-build-final*.log`及对应注册日志 |
| 默认旧Lab Testing-off | 构建PASS，0测试 | `out/dec042b-c3-legacy-lab-default-debug-*-final.log` |
| 缺显式C3开关、缺依赖 | 均按预期配置失败 | `out/dec042b-c3-gate-*-configure-expected-failure-final.log` |

主要命令：

```powershell
cmake -S . -B out/dec042b-c3-<config> -G Ninja -DCMAKE_BUILD_TYPE=<Debug|Release> `
  -DPAE_BUILD_PROTOCOL_LAB=ON -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05=ON -DPAE_BUILD_TESTING=ON -DBUILD_TESTING=ON `
  -DPAE_BUILD_LAB_V06_FORMAT_TESTS=ON -DPAE_BUILD_LAB_V06_EVIDENCE_TESTS=ON `
  -DPAE_BUILD_LAB_V06_EXECUTION_TESTS=ON -DPAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS=ON
cmake --build out/dec042b-c3-<config>
ctest --test-dir out/dec042b-c3-<config> -R '^pae\.tools\.protocol_lab\.c3_(cli|policy)$' --output-on-failure
ctest --test-dir out/dec042b-c3-<config> -LE udp --output-on-failure
```

恢复后的第一次Release配置未进入编译：未初始化Developer Shell时CMake误选Strawberry Perl附带
的GCC 4.9.2，见`out/dec042b-c3-release-configure-final.log`。随后使用MSVC新隔离目录完成上述
有效验证；该日志属于环境命令失败，不计为产品测试失败，也未清理其目录。

## 4. 边界与待验收

- 自动化离线证据不替代第22.8节七项人工离线验收；当前为`NOT_EVALUATED`，执行命令见
  [人工验收单](manual-dec042b-c3-offline-acceptance.md)。
- 退出10只有纯策略和既有内部组件测试证据；非Testing工具没有故障注入入口。
- 退出8/9继续只属于旧Transport，新离线链不使用。
- P2后Testing-off Release可执行文件的COFF符号扫描未发现`ExecutionTestHooks`、
  `BindExecutionForTesting`或相关测试标记，且注册0测试，见
  `out/dec042b-c3-testing-off-release-symbols-p2-optional.log`和对应注册日志。
- 本轮未运行UDP或其他网络测试，未验证Linux、Golden、真实协议、设备、现场、独立Oracle、
  性能或Qt集成，也不作生产可用声明。
- Evidence Hash只证明Bundle内部一致性，不提供签名、身份认证或来源真实性。
