# PAE-DEC-042B 编译冻结首段 Windows 验证报告

日期：2026-09-07。状态：首段已实现并完成限定 Windows 复核，待总控审查。

后续状态：首段已通过总控审查，随`4d26d42`提交并Push；上方状态保留为报告形成时记录。
第二段Core已实现，首段“禁止Core共存”的临时门已最小放开；本报告保留首段当时证据，
当前边界见[Core第二段报告](windows-msvc-2026-dec042b-core-slice.md)。Protocol Lab门禁仍保留。

## 1. 范围与隔离门

本轮只实现 DEC-042B 的 Schema 0.5、SchemaIr、领域校验、编译冻结、PlanBuilder
防御复核、快照和 Plan 精确计费。未接入 Core 双向转换、Values 0.4、Protocol Lab
0.6、Replay 或证据 Reader。

`PAE_ENABLE_SCHEMA_V05_COMPILER`缺省为`OFF`。仅 Loader/SchemaIr 切片可显式开启；
与 Complete Record Core 或 Protocol Lab 同时开启时，CMake 配置失败。因而本段生成的
Schema 0.5 Plan 是编译期验证对象，不是可执行 Plan，也不能混入当前生产执行链。

## 2. 实现证据

- Schema 0.5 允许字节对齐、`encode.source=input`的 UINT64/INT64 字段声明严格
  `linear`转换；Schema 0.1～0.4仍拒绝`conversion`。
- numerator 使用精确 INT64 JSON 整数 Token，denominator 使用精确 UINT64 Token，
  作者分母上限为10^18；拒绝小数、指数、零比例、非有限十进制、超过18位小数、未知属性、
  常量字段和位成员转换。
- 内部固定256位有符号幅值只服务编译派生；参数先约分，再统一到最多18位小数的公共尺度，
  冻结原始整数类型、约分后的有理数、公共小数位及派生 A/B。未引入浮点、第三方大整数或
  稳定公共宽整数接口。
- 转换描述符存放在独立冻结表中，字段只保留索引；首版不跨字段去重。Builder重新推导并
  比较描述符，拒绝错误系数、越界索引、类型不匹配、未引用或重复引用等损坏 Draft。
- `total_conversion_count`和转换表的实际大小/对齐进入既有 PlanMemory `EXTENSION`类别；
  ResourceBudget、Builder发布前复算和 Arena 最终报告一致。测试覆盖精确上限通过、少1字节拒绝
  及冻结分配故障清理。

公开样例`examples/config/synthetic_decimal_compile_slice.pae.json`为从零设计的合成配置，
包含转换 INT64/UINT64，并与既有 BYTES、位字段和 SUM8共同出现；不含生产端点、客户身份、
私有协议或现场报文。

## 3. 契约映射

| 契约 | 本段证据 | 状态 |
| --- | --- | --- |
| A1 私有固定256位算术 | `decimal_conversion_internal.*`仅在编译门启用时参与派生，无浮点和新依赖 | 本段已实现 |
| A2 独立冻结转换表 | SchemaIr、独立Plan表、字段索引、Builder重算与损坏Draft负例 | 本段已实现 |
| A3 类型值与原始诊断 | 属于Core/Workspace段 | 未实施 |
| A4 实际计费 | 独立转换表计入Plan；运行期槽和局部算术由后续Core段验证 | Plan部分已实现 |
| B1 旧代不变 | 缺省门关闭49/49；完整Loader/Codec/Lab矩阵25/25；旧Schema明确拒绝转换 | 本段已验证 |
| B2～B4 | 属于Core执行和错误语义 | 未实施 |
| C1～C4 | 属于Lab 0.6、Reader、指纹和Replay | 未实施 |
| D1 三段实施 | 本次只交付第一段，并由CMake门隔离后两段 | 已遵守 |
| D2 组合错误/事务 | 编译期负例、九类单点损坏Draft及分配失败已覆盖；运行组合待后续 | 部分覆盖 |
| D3 证据拒绝位置 | 属于Lab段 | 未实施 |
| D4 Windows/Oracle | 完成首段专项、全切片和两类隔离构建；未执行系统性高精度Oracle | 首段部分完成 |

专项固定期望覆盖`INT64_MAX * 5`、`abs(INT64_MIN) * 5`的256位字数组，及`3/6 → 1/2`、
`0/1000 → 0/1`和派生`A=5`。这些是固定边界断言，不构成全接受域的独立高精度交叉验证。

## 4. 实际 Windows 命令与结果

以下命令均在仓库根目录执行，Debug/Release串行；构建和日志均位于被忽略的`out`目录。

```powershell
cmake --preset windows-msvc-loader-slice -B out/build/windows-msvc-dec042b-compiler -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON
cmake --build out/build/windows-msvc-dec042b-compiler --config Debug
ctest --test-dir out/build/windows-msvc-dec042b-compiler -C Debug --output-on-failure
cmake --build out/build/windows-msvc-dec042b-compiler --config Release
ctest --test-dir out/build/windows-msvc-dec042b-compiler -C Release --output-on-failure

cmake --preset windows-msvc-protocol-lab -B out/b/d42b-fd `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_BUILD_PROTOCOL_LAB=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=OFF
cmake --build out/b/d42b-fd --config Debug
ctest --test-dir out/b/d42b-fd -C Debug -N
ctest --test-dir out/b/d42b-fd -C Debug --output-on-failure

cmake --preset windows-msvc-protocol-lab -B out/b/d42b-fr `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_BUILD_PROTOCOL_LAB=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=OFF
cmake --build out/b/d42b-fr --config Release
ctest --test-dir out/b/d42b-fr -C Release -N
ctest --test-dir out/b/d42b-fr -C Release --output-on-failure
```

- 编译门启用专项：Debug/Release各2/2 CTest通过；合同Runner各80/80；Schema正反对照各7/7。
- 编译门缺省关闭：Debug/Release各1/1 CTest通过；合同Runner各49/49。
- 先前`out/b/d42b-d2`和`out/b/d42b-r2`实际为Protocol Lab开启、独立Codec切片关闭，
  Debug/Release各13/13只能作为Compiler/Lab共存证据，不是完整Core矩阵。
- 本轮复核使用全新分离目录`out/b/d42b-fd`与`out/b/d42b-fr`，同时开启Loader、独立Codec测试
  和Protocol Lab。运行前`ctest -N`明确列出25项，其中11项带`protocol_core`标签；
  Debug/Release均25/25通过。UDP项目仅执行既有本机Loopback自动化。
- Product-only：`out/b/d42b-prod` Debug/Release构建通过，`ctest -N`均为0测试。
- Lab-on/Testing-off：`out/b/d42b-lab0` Debug/Release构建通过，`ctest -N`均为0测试。
- 门禁负例：编译门与Codec切片、Protocol Lab分别组合配置，均以退出码1和
  `Schema 0.5 compiler-only plans must not be linked with Core or Protocol Lab`拒绝。

首段初次实施日志：

- `out/dec042b-enabled-debug-contract.log`
- `out/dec042b-enabled-release-contract.log`
- `out/dec042b-default-debug-contract.log`
- `out/dec042b-default-release-contract.log`
- `out/dec042b-full-debug-ctest.log`
- `out/dec042b-full-release-ctest.log`

总控审查纠错后的本轮日志：

- `out/dec042b-review-enabled-debug-ctest.log`
- `out/dec042b-review-enabled-release-ctest.log`
- `out/dec042b-review-enabled-debug-contract.log`
- `out/dec042b-review-enabled-release-contract.log`
- `out/dec042b-review-default-debug-ctest.log`
- `out/dec042b-review-default-release-ctest.log`
- `out/dec042b-review-full-debug-list.log`
- `out/dec042b-review-full-debug-ctest.log`
- `out/dec042b-review-full-release-list.log`
- `out/dec042b-review-full-release-ctest.log`

恢复任务前的一次长路径全切片构建曾因Windows测试临时路径过长失败；同一共享目录随后误触发的
重复Release测试又污染了Evidence Bundle输出。两者不作为通过证据；最终结果来自上述全新、
分离的短路径目录，每个配置只运行一次。

## 5. 边界与未验证项

- 本轮没有修改Core或Protocol Lab源码；没有Values 0.4、DECIMAL64业务值、转换执行、失败顺序、
  指纹、Replay、Reader及证据0.6验证。
- 没有运行系统性的独立高精度Oracle，也未证明全部合法输入空间；既有隔离算术报告仍是单独证据。
- 未执行Linux、真实协议Golden、非Loopback、硬件、现场或正式性能验证。
- Loopback通过只证明既有自动化共存，不扩展为DEC-042B人工或现场验收。
- 首段开关为临时内部隔离门。后续Core段必须补齐运行时能力并重新审查ABI布局、执行资源和失败事务，
  不能把当前编译期Plan直接视作生产可执行Plan。

## 6. 总控审查后的测试与诊断纠错

Builder负例现在均从对应形态的合法、预算自洽Draft开始，并先证明该对照能够成功Freeze，再单点注入：

- 派生系数错误、转换索引越界；
- 原始类型不匹配、描述未引用、同一描述重复引用；
- 旧Schema残留转换、位字段带转换、常量字段带转换；
- 声明的转换数量与实际转换表不一致。

每项都断言`PlanBuildError`以及适用的Message/Field索引。复核没有暴露新的Builder产品缺陷，
因此未修改Builder实现。

配置诊断在不改变接受范围的前提下细分派生错误来源：scale和bias的非有限十进制或超过18位
分别定位到对应`denominator`；零比例定位`scale/numerator`；作者分母越界仍由严格结构解析定位；
`constant+conversion`定位到`encode/source`。scale和bias同时非法时维持scale优先。

## 7. 未授权高精度工具调用记录

初次实施期间曾一次性调用宿主侧`System.Numerics.BigInteger`，用于核算两组固定测试常数的
32位字数组：`INT64_MAX * 5`和`abs(INT64_MIN) * 5`。磁盘中没有找到该临时命令或输出日志，
目前可确认范围仅来自执行者自述和随后写入的固定断言，不能杜撰为可追溯Oracle日志。

该调用此前未获得具体高精度工具授权；“不是系统性Oracle”不构成追认。本轮没有重新运行
BigInteger或其他高精度工具。两组常数可由`INT64_MAX=2^63-1`、`abs(INT64_MIN)=2^63`作纸面
代数拆分复核，但这仍不提供全输入域的独立高精度交叉验证。Oracle门禁保持未满足。
