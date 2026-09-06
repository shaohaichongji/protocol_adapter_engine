# PAE-DEC-042A 字节对齐INT64 Windows验证报告

日期：2026-09-06。状态：实现与本轮Windows验证完成，待总控审查；未Stage、Commit或Push。

## 1. 验证结论与边界

Schema 0.4、INT64能力链、Core二进制补码编解码、Values 0.3及Lab证据0.5已按确认契约实现。
最终全切片Debug/Release各45/45通过；Protocol Lab专用Debug/Release各13/13通过；
Product-only及Lab-on/Testing-off两类隔离配置均完成Debug/Release构建并确认注册测试数为0。

网络测试仅为自动化本机Loopback。本文不证明Linux、真实协议Golden、真实设备、硬件、现场、
人工Lab、性能或生产可用；DEC-042B比例/偏置、DECIMAL64、有符号位字段及公共稳定API均未实现。

## 2. 实现摘要

- Schema/Compiler：Schema 0.4新增字节对齐INT64，严格区分旧代；配置constant使用精确JSON整数Token，
  编译期检查1～8字节有符号范围、字节序、Matcher冲突和损坏Draft。
- Plan/Core：Plan中保留独立`std::int64_t`常量与执行标签；Decode以无符号字节累积后安全解释补码，
  Encode先检查Wire范围再写低位，避免负数移位、移位64位及对INT64_MIN取负。
- Lab：Values 0.3使用规范有符号十进制字符串；Schema 0.4统一选择Result/Record/Event 0.5及
  `pae.lab.fingerprint/0.5`，RX Metadata仍为0.2；Reader严格校验INT64标签与值元组。
- 资源：INT64相关成员随`FieldPlan`、`FrozenFieldPlan`、`FieldExecutionPlan`的实际`sizeof`和对齐进入
  既有PlanMemoryLayout/Arena估算、发布前复算及最终报告；Workspace未增加INT64专用槽或堆分配。
  MSVC实测Debug/Release的`FrozenFieldPlan=128`、`FieldExecutionPlan=120`、`EncodeFieldValue=104`、
  `DecodedFieldSlot=120`字节；公开混合Plan计费3128字节、Workspace估算72字节。含标准库容器的
  可变`FieldPlan`受迭代器调试级别影响，Debug为168、Release为152字节，未假定两配置`sizeof`相同。

## 3. I01～I12映射

| 编号 | 自动化证据 | 本轮结果 |
| --- | --- | --- |
| I01 | `tests/protocol_core/int64_contract_tests.cpp`遍历1～8字节、0/1/-1/各宽度最小最大值及两种字节序 | PASS |
| I02 | 同测试覆盖24/40/48/56位边界、INT64_MIN/MAX；公开向量固化BE/LE24及64位最小值 | PASS |
| I03 | 同测试精确断言正负窄宽越界为`VALUE_NOT_REPRESENTABLE`，UINT64输入为`TYPE_MISMATCH` | PASS |
| I04 | 同测试覆盖动态输入、正负/INT64_MIN常量、常量覆盖及匹配/冲突Matcher；冲突定位到`encode/value` | PASS |
| I05 | `verify_int64_schema_contract.ps1`与Core测试覆盖旧代、未知转换属性、INT64位成员、小数/指数及范围错误 | PASS |
| I06 | `verify_int64_generation_contract.cmake`覆盖Values 0.3的`-0`、正号、前导零、空白、JSON Number、Wire越界；自洽长度/Hash链的未知kind由语义层拒绝 | PASS |
| I07 | `synthetic_int64_slice`独立25字节预期同时含INT64、UINT64、BOOL、BYTES、ENUM、位容器和SUM8；Core使用非零预填Buffer并断言失败交付长度为0 | PASS |
| I08 | 既有`sum8_contract_tests`和`inspect_phase_tests`覆盖零/单/多结构候选、容量及SUM8优先级；Schema 0.4无INT64对照证明沿用同一路径 | PASS（复用既有候选测试） |
| I09 | Lab测试覆盖Run→Replay A→Replay B、同代跨配置DIFFERENT及差异Bundle再Replay、失败Bundle比较；V0.5 Loopback覆盖DECODE_RX及Peer mismatch的两级NO_CODEC回放 | PASS |
| I10 | 全切片既有旧离线V0.1、UDP 0.2、位字段0.3、SUM8 0.4和固定指纹回归通过；新增无INT64 Schema 0.4与0.4/0.5跨代Compare拒绝；V0.5 Loopback断言历史Transport分离 | PASS |
| I11 | INT64测试核对字段/位容器/SUM8计数、Plan分类字节总和与独立Snapshot域；Builder损坏Draft、首次调用分配、容量/Buffer及共享Plan回归通过 | PASS（非性能结论） |
| I12 | 专项、Lab、全切片及两类隔离配置均完成Windows Debug/Release验证 | PASS |

## 4. 实际命令与结果

主要命令（Debug与Release串行执行）：

```powershell
cmake -S . -B out/build/windows-msvc-all-slices-dec042a -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_JSON_PARSER_SPIKE=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_BUILD_PROTOCOL_LAB=ON
cmake --build out/build/windows-msvc-all-slices-dec042a --config Debug
ctest --test-dir out/build/windows-msvc-all-slices-dec042a -C Debug --output-on-failure
cmake --build out/build/windows-msvc-all-slices-dec042a --config Release
ctest --test-dir out/build/windows-msvc-all-slices-dec042a -C Release --output-on-failure

cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-debug
ctest --preset windows-msvc-protocol-lab-debug
cmake --build --preset windows-msvc-protocol-lab-release
ctest --preset windows-msvc-protocol-lab-release
```

最终结果：

- 全切片Debug：45/45 PASS；Release：45/45 PASS。
- Protocol Lab专用Debug：13/13 PASS；Release：13/13 PASS。
- Product-only：`BUILD_TESTING=OFF`、Lab关闭，Debug/Release构建PASS，`ctest -N`均为0。
- Lab-on/Testing-off：Lab开启、Testing关闭，Debug/Release构建PASS，`ctest -N`均为0。
- 格式化使用VS 2026随附`clang-format`；`git diff --check`与候选文件检查结果见本轮交付报告。

日志位于被忽略目录`out/dec042a-logs/`，关键文件：

- `all-slices-debug-ctest-final.log`
- `all-slices-release-ctest-final.log`
- `protocol-lab-debug-ctest-final.log`
- `protocol-lab-release-ctest-final.log`
- `product-only-debug-build-final.log`、`product-only-release-build-final.log`
- `lab-no-tests-debug-build-final.log`、`lab-no-tests-release-build-final.log`
- `product-only-*-ctest-list.log`、`lab-no-tests-*-ctest-list.log`
- `int64-debug-resource-evidence.log`、`int64-release-resource-evidence.log`

## 5. 版本与兼容性

Schema 0.4可使用旧Values表示旧字段，BOOL仍至少Values 0.2；Values 0.3只配Schema 0.4。
Schema 0.4不论是否实际含INT64均输出0.5证据代际。旧Schema、Values、Result/Record/Event、
历史指纹与固定样例未迁移、未覆盖。Run跨代直接Compare及替换配置跨Schema Replay继续失败关闭，
原始Frame比较能力不变。

SUM8、Bundle Hash及证据来源真实性仍是不同层次：SUM8不是认证，Hash链也不提供签名或来源证明。

## 6. 已知限制

- 本轮只验证Windows/MSVC、公开合成向量与本机Loopback；未执行Linux、Sanitizer、真实协议、硬件或现场。
- 首次调用分配与操作上界测试只是回归门禁，不构成正式性能或全资源安全证明。
- DEC-042B及其他明确后置能力未进入实现。
- 本轮完成状态仅为待总控审查，不代表获准提交或生产发布。

## 7. P2独立复现、限定修复与复核（2026-09-06）

### 7.1 首次证据限制与实际复现

第3节为首次实现轮证据：I06当时只有UNKNOWN kind篡改，不等于INT64规范文本和元组专用分支
已被覆盖；I11当时的损坏Draft测试没有覆盖新增Schema代际及signed_constant残留不变量。
首次各13/13、45/45的通过结果不能关闭这两个缺口。

本轮先增加Builder测试，不修改产品：四类Schema 0.4合法样本经过DomainValidator、资源预算和
PlanDraftAssembler，原样Freeze均成功；仅修改版本为0.1/0.2/0.3，或仅在UINT64/BOOL/ENUM
位字段INPUT中注入signed_constant_value=-1。六例均实际输出
`Freeze published the corrupted legal-budget Draft`，而不是提前被计费差异拒绝。
修复前CTest报告失败（日志未单独保存退出码），内部case为41通过、6失败、预期47。普通JSON入口已拒绝相应非法输入，
这里确认的是内部损坏Draft防御缺口，不是外部配置绕过。

### 7.2 限定修复与断言

- `plan_builder.cpp`仅增加INT64的Schema 0.4检查及位字段编码的signed_constant排除条件。
- 测试专用Peer只在已预算Draft中注入单项异常；四个合法对照及六项精确拒绝均通过。
  拒绝必须为`INVALID_FIELD_PLAN`、message_index=0、field_index=1；内部case总数47/47。
- Lab保留UNKNOWN kind负例，另外保持kind=INT64测试`-0`、`01`、`+1`、正负INT64越界、
  raw/logical不一致及enum_known=true。各例更新并读取核对Record全部payload长度/Hash、清单
  全部Hash及Record/Result绑定后才调用Reader，退出码3且精确诊断为
  `stored INT64 field has an invalid raw/logical/enum_known tuple`。
- 追加Values正负INT64文本越界负例；保留24位Wire越界测试，二者不混用。
- 未修改Reader产品逻辑、格式版本、指纹及历史夹具，没有扩大DEC-042A范围。

### 7.3 实际命令和结果

使用`D:\develop_env\cmake-4.4.3\bin`中的CMake/CTest，Debug和Release分别执行：

```powershell
cmake --build out/build/windows-msvc-all-slices-dec042a --config Debug
ctest --test-dir out/build/windows-msvc-all-slices-dec042a -C Debug `
  -R 'config_compiler.contract|int64|first_call|inspect_phase' --output-on-failure
ctest --test-dir out/build/windows-msvc-all-slices-dec042a -C Debug --output-on-failure
cmake --build out/build/windows-msvc-protocol-lab --config Debug
ctest --test-dir out/build/windows-msvc-protocol-lab -C Debug --output-on-failure
cmake --build out/build/windows-msvc-product-only-dec042a --config Debug
ctest --test-dir out/build/windows-msvc-product-only-dec042a -C Debug -N
cmake --build out/build/windows-msvc-lab-no-tests-dec042a --config Debug
ctest --test-dir out/build/windows-msvc-lab-no-tests-dec042a -C Debug -N
```

Release使用相同命令替换配置名。修复后所有构建及CTest命令退出码0：针对性Debug/Release
各6/6、专用Lab各13/13、全切片各45/45；两类隔离Debug/Release构建成功且注册测试均为0。
另用Debug `ctest -R 'config_compiler.contract|int64_generation' -V`保留逐例诊断（2/2）。
四个本轮修改C++文件`clang-format --dry-run --Werror`及`git diff --check`通过。

日志目录为忽略目录`out/review/dec042a-p2/`：

- `pre-fix-build.log`、`pre-fix-ctest.log`：修复前真实错误发布复现。
- `post-fix-detail.log`：47个内部case、八种自洽篡改（含原UNKNOWN）精确诊断。
- `targeted-{Debug,Release}-ctest.log`、`all-slices-{Debug,Release}-{build,ctest}.log`。
- `protocol-lab-{Debug,Release}-{build,ctest}.log`。
- `{product-only,lab-no-tests}-{Debug,Release}-{build,ctest-list}.log`。

网络仍仅为既有本机Loopback自动化；未新增人工Lab、非Loopback、Linux、真实协议Golden、
硬件、现场或正式性能证据。既有工作区变更全部保留；未Stage、Commit、Push，待总控复核。
