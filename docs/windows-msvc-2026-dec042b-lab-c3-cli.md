# DEC-042B Lab C3 CLI Windows验证报告

日期：2026-09-08

状态：`IMPLEMENTED / AUTOMATED OFFLINE VERIFIED / MANUAL NOT_EVALUATED`

当前复核基线：`main` / `386a8bb7a1b0e21453f6b80faa3283e513f4a02e`

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

本次路径P2独立复现确认：修复前工具可以消费PowerShell传入的中文绝对路径并成功发布Bundle，
但`generic_string()`把Windows本地代码页字节写入JSON stdout；严格UTF-8解码在首个中文字节失败，
返回路径也不能作为UTF-8原样串联。原始失败证据保留在
`out/agent-c3-acceptance-20260908-205122`，独立复现保留在
`out/dec042b-c3-unicode-pre-fix-中文 路径/pre-fix-evidence.json`。后者同时证明按当前ACP解码可恢复
真实存在的Bundle，因此问题位于CLI窄字符串路径边界，不是文件系统不支持中文路径。

最小修复在Lab内部增加单一路径桥：输出始终采用`generic_u8string()`；Windows输入先按严格UTF-8
解码，失败时保留CRT/PowerShell本地代码页兼容。配置、Values、Frame、record-root、Bundle及
Compare两侧路径统一经过该桥。未改全局代码页、C3封装版本、Evidence格式、指纹、Core或网络。
曾评估`wmain`强制UTF-8入口，但CMake 3.25子进程参数会对当前中文仓库绝对路径再次转码，导致
既有Replay回归；该路线未保留，日志仅作为开发期排除证据。

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
- Windows专用Unicode串联测试通过PowerShell `Start-Process`传入中文及空格绝对路径，按原始字节
  严格UTF-8解码stdout，并将返回的`published_bundle`不经转码地用于Inspect、Replay和Compare；
  精确核对Decimal、父子Replay均EQUAL、Compare EQUAL及路径实际存在。

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
| Debug C3专项（当前，恢复后复跑） | 3/3 PASS | `out/dec042b-c3-debug-recovery-targeted.log` |
| Debug Protocol Lab离线（当前，`-L protocol_lab -LE udp`） | 19/19 PASS | `out/dec042b-c3-debug-protocol-lab-offline-unicode-final.log` |
| Release C3专项（当前，恢复后复跑） | 3/3 PASS | `out/dec042b-c3-release-msvc-recovery-targeted.log` |
| Release Protocol Lab离线（当前，`-L protocol_lab -LE udp`） | 19/19 PASS | `out/dec042b-c3-release-msvc-protocol-lab-offline-unicode-final.log` |
| 修复前Debug/Release完整离线历史基线 | 各35/35 PASS | `out/dec042b-c3-*-offline-full-p2-optional-final.log` |
| C3 Testing-off Release（当前） | 构建PASS，0测试；Unicode串联PASS | `out/dec042b-c3-testing-off-release-*-unicode-final.log` |
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
ctest --test-dir out/dec042b-c3-<config> -R '^pae\.tools\.protocol_lab\.c3_(cli|policy|unicode_paths)$' --output-on-failure
ctest --test-dir out/dec042b-c3-<config> -L protocol_lab -LE udp --output-on-failure
```

恢复后的第一次Release配置未进入编译：未初始化Developer Shell时CMake误选Strawberry Perl附带
的GCC 4.9.2，见`out/dec042b-c3-release-configure-final.log`。随后使用MSVC新隔离目录完成上述
有效验证；该日志属于环境命令失败，不计为产品测试失败，也未清理其目录。

## 4. 边界与待验收

- 代理已在修复后使用中文及空格绝对路径重跑七项离线验收，35项精确检查PASS，证据位于
  `out/agent-c3-acceptance-20260908-212535`。它不替代第22.8节用户人工验收；人工状态仍为
  `NOT_EVALUATED`，执行命令见[人工验收单](manual-dec042b-c3-offline-acceptance.md)。
- 输入桥采用“严格UTF-8优先、否则Windows本地代码页”的有限兼容策略；已验证当前中文Windows
  环境和ASCII路径，不宣称覆盖所有系统代码页组合或CMake对任意Unicode命令参数的行为。
- 退出10只有纯策略和既有内部组件测试证据；非Testing工具没有故障注入入口。
- 退出8/9继续只属于旧Transport，新离线链不使用。
- P2后Testing-off Release可执行文件的COFF符号扫描未发现`ExecutionTestHooks`、
  `BindExecutionForTesting`或相关测试标记，且注册0测试，见
  `out/dec042b-c3-testing-off-release-symbols-p2-optional.log`和对应注册日志。
- 本轮未运行UDP或其他网络测试，未验证Linux、Golden、真实协议、设备、现场、独立Oracle、
  性能或Qt集成，也不作生产可用声明。
- Evidence Hash只证明Bundle内部一致性，不提供签名、身份认证或来源真实性。
