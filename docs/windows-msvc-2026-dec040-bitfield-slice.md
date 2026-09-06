# PAE-DEC-040 位字段最小切片 Windows 验证报告

## 1. 结论与证据边界

2026-09-06在`a418ac99b94a1e29b3f3476712ea7d55fd815404`基线上完成
PAE-DEC-040授权范围内的实现和Windows x64 MSVC验证。Schema/Loader/Validator、Frozen Plan、
精确Plan与Workspace计费、COMPLETE_RECORD Codec及Protocol Lab多版本路径已形成可执行闭环。

本报告结论限于公开`SYNTHETIC_FROM_SCRATCH`人工向量、自动化离线测试和本机Loopback。
没有执行Linux、非Loopback主动发送、真实设备、硬件、现场或真实协议Golden验证；仓库外私有
需求证据存在映射缺口，不能据本报告声明真实协议完整兼容。此前6份人工Lab验收Markdown属于
独立检查点，本轮没有改写原始人工Evidence Bundle。

## 2. 实现范围

- Schema 0.2增加Message `bit_containers`、`bitfield` Wire和BOOL；Schema 0.1保持原拒绝规则；
- 公开容器字段使用确认名称`container_offset/container_width`，支持1/2/4/8字节、大小端、
  `lsb0/msb0`、跨字节成员与64位满宽；
- Validator拒绝空容器、未知引用、非法类型/位宽/基础值、重复ID、成员/容器/普通字段重叠和越界；
- Frozen Plan保存容器及预计算mask/shift，Plan Arena、ResourceRequirements和Workspace精确计费；
- Core使用原生BOOL；Encode从`base_value`初始化且不依赖输出Buffer旧值，Decode不以基础值约束保留位；
- Lab增加Values 0.2及Result/Record/Event 0.3，RX Metadata保持0.2；0.3指纹使用独立版本域；
- 同代Replay、跨配置差异/失败、链式Replay及三种模式保留；跨Schema配置Replay和旧Run/0.3 Run
  Compare失败关闭，原始Frame比较继续允许。

公开配置、Values和独立字节预期分别位于：

- `examples/config/synthetic_bitfield_slice.pae.json`；
- `examples/config/synthetic_bitfield_slice.values.pae-lab.json`；
- `tests/protocol_core/golden/synthetic_bitfield/bitfield_record_001.frame.hex`。

期望18字节为`AD 85 A0 43 40 80 BE EF 01 EF CD AB 89 67 45 23 01 54`，由文档化掩码、
字节序和成员值独立计算，不由被测Codec生成。

## 3. 位字段与资源专项

`pae.protocol_core.bitfield.contract`覆盖：

- 1/2/4/8字节容器、大小端和两种位编号；
- BOOL真假、ENUM、UINT64、首末位、跨字节、64位满宽和常量成员；
- 非零预填Buffer、输入顺序无关、独立Encode/Decode预期和接收保留位变化；
- 缺失值、运行期值溢出、成员重叠、容器重叠、普通字段/容器重叠、空容器、未知引用、
  重复ID、非法BOOL宽度、基础值越界、非法容器宽度和未知属性失败关闭；
- Frozen Plan容器数、ResourceRequirements及Workspace容器槽精确匹配。

`pae.protocol_core.complete_record_codec.first_bitfield_allocation`验证构造Workspace后首次Encode和
Decode均无可替换`operator new`分配。同一Plan使用两个独立Workspace得到同一结果；既有共享Plan
并发测试继续通过，Workspace误共享防护未回退。

## 4. C01～C16兼容性矩阵

| 编号 | 既有自动化证据 | 本轮强化与剩余限制 |
| --- | --- | --- |
| C01 | 旧Schema/Values离线0.1、真实旧离线夹具、旧指纹及UDP0.2回归继续通过 | 完整矩阵继续覆盖；未改写旧夹具或指纹算法 |
| C02 | Schema 0.1出现位容器/BOOL返回`PAE_LAB_CONFIG_COMPILE_FAILED` | 现有断言已覆盖，本轮无新增缺口 |
| C03 | Schema 0.2无BOOL输入可使用Values 0.1，Result仍为0.3 | 本轮Frame资源提前失败及Replay链继续精确断言0.3 |
| C04 | Schema 0.1可使用只含旧类型的Values 0.2，Result仍为0.1 | 现有断言已覆盖，旧代失败路径未回退 |
| C05 | Values 0.2原生BOOL真假编码及0.3 BOOL字段精确路径断言通过 | 合法BOOL证据作为Stored Field负例的对照样例 |
| C06 | Values 0.1 BOOL及数字BOOL均以`PAE_LAB_VALUES_INVALID`拒绝；合法Schema 0.2 Plan建立后的失败结果仍为0.3 | 新增数字`1`和字符串`"true"`负例；仍按精确JSON路径核对格式与诊断 |
| C07 | BOOL raw/logical自洽Hash篡改仍被读取端拒绝 | 新增未知kind、BOOL改标UINT64、UINT64非规范十进制及`enum_known`矛盾的自洽Hash负例 |
| C08 | 真实旧离线0.1 Load/Compare/Replay、当前0.1及UDP0.2 Replay回归通过；旧UDP草案仍拒绝 | 完整矩阵继续覆盖；没有重新生成历史样例 |
| C09 | 0.3原始Run→Replay A→Replay B保持0.3且比较EQUAL | 新增资源超限失败Run→Replay A→Replay B；每次按当前Plan重判且Codec调用为零 |
| C10 | 同代新配置改变Encode字节得到DIFFERENT且产物可读/可再Replay；Values失败产物同样可读/可再Replay | 新增替换配置放宽Frame上限后执行Decode并产生可读DIFFERENT；后续Replay与上次当前输出比较 |
| C11 | Schema 0.1/0.2双向替换配置Replay均返回`PAE_LAB_CROSS_SCHEMA_REPLAY_UNSUPPORTED` | 强化双向退出码、诊断、拒绝阶段及无完成Bundle断言 |
| C12 | 旧Run/0.3 Run双向Compare均返回`PAE_LAB_CROSS_FORMAT_COMPARE_UNSUPPORTED` | 强化双向退出码和诊断精确断言，不将代际不兼容归为协议差异 |
| C13 | 跨来源原始Frame比较按字节返回EQUAL | 强化`comparison_status`、`comparison_equal`和分类精确断言 |
| C14 | 未知Result版本及混合0.2 Event/0.3 Record/Result在Hash全部更新后仍拒绝 | Stored Field语义负例已补齐；未穷举所有字段排列及所有JSON数值边界 |
| C15 | Schema 0.2普通、链式和Peer mismatch `NO_CODEC_REEXECUTION`均输出0.3；后者保持NOT_EVALUATED且零Socket调用 | 完整矩阵继续覆盖，历史Transport隔离未回退 |
| C16 | 新UDP0.3本机Loopback与RX Metadata 0.2绑定、读取及DECODE_RX链式Replay通过 | 完整矩阵继续覆盖；仅证明本机Loopback，不代表真实设备或现场 |

JSON断言通过CMake `string(JSON ... GET)`按精确字段路径执行，不用包含匹配穿透
`historical_transport`中的同名字段。Replay测试还断言零Socket调用、Codec观察器调用、本次顶层状态、
历史Transport不变及原Bundle清单不变。

## 5. 实际命令与结果

### 5.1 Protocol Lab独立矩阵

```powershell
cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-debug --parallel
ctest --preset windows-msvc-protocol-lab-debug --output-on-failure
cmake --build --preset windows-msvc-protocol-lab-release --parallel
ctest --preset windows-msvc-protocol-lab-release --output-on-failure
```

首次实现轮Debug和Release均为`5/5`通过。本次P2纠错加入Schema/Compiler对照测试后，完整Lab
矩阵Debug和Release均为`6/6`通过；专项测试均为`2/2`通过。

### 5.2 全切片共存矩阵

```powershell
cmake -S . -B out/build/windows-msvc-all-slices-dec040 -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_JSON_PARSER_SPIKE=ON -DPAE_JSON_SPIKE_CANDIDATE=all `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON -DPAE_BUILD_PROTOCOL_LAB=ON
cmake --build out/build/windows-msvc-all-slices-dec040 --config Debug --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec040 -C Debug --output-on-failure
cmake --build out/build/windows-msvc-all-slices-dec040 --config Release --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec040 -C Release --output-on-failure
```

首次实现轮Debug和Release均为`33/33`通过。本次P2纠错后的Debug和Release均为`34/34`通过，
包含三种Parser候选、Config Compiler、Core、首次调用分配、操作计数、共享Plan并发、
Conformance Runner和Protocol Lab共存回归。

### 5.3 Product-only隔离

```powershell
cmake -S . -B out/build/windows-msvc-codec-product-only-dec040 -G "Visual Studio 18 2026" -A x64 `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_JSON_PARSER_SPIKE=OFF `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=OFF `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON -DPAE_BUILD_PROTOCOL_LAB=OFF
cmake --build out/build/windows-msvc-codec-product-only-dec040 --config Debug --parallel
cmake --build out/build/windows-msvc-codec-product-only-dec040 --config Release --parallel
ctest --test-dir out/build/windows-msvc-codec-product-only-dec040 -C Debug -N
```

两种配置构建通过，CTest为0；PAE工程仅生成`pae_protocol_plan`和
`pae_protocol_core_slice`，未生成Config Compiler、yyjson、Lab或测试目标。

### 5.4 Schema与格式

PowerShell 7 `Test-Json`使用更新后的`schema/pae.schema.json`验证Schema 0.2位字段样例和现有
Schema 0.1 Lab样例，二者均为`True`。实际修改的17个C++文件经VS随附`clang-format --dry-run
--Werror --style=file`通过；`git diff --check`及新增公开文件敏感信息检查见最终复核日志。

### 5.5 P2审查纠错（2026-09-06）

本次独立复现并关闭两项P2：

1. Schema 0.2配置编译成功后，Values解析失败原输出`pae.lab.result/0.1`。根因是Plan代际只在
   Codec操作结果中回填，读取、解析、资源限制、UDP Pipeline及Replay准备的提前返回未绑定
   已编译Plan。现于Plan成功后立即绑定Schema／操作身份，并在提前失败时写入0.3的Replay、
   Current Execution、指纹状态；未调用Codec时不产生成功字段。输入内容完整的失败Bundle可
   Load／Compare并进入后续Replay拒绝；缺失Values或非法Frame输入不发布`COMPLETE`。
2. 单字节位容器显式`byte_order`原先同时被JSON Schema和Loader接受。现JSON Schema通过
   条件`else`禁止该属性，Loader在STRUCTURAL阶段以精确JSON Pointer拒绝；显式大端、小端均
   拒绝，省略继续有效，多字节缺失或非法值继续拒绝。普通整数字段规则未修改。

针对性命令：

```powershell
ctest --test-dir out/build/windows-msvc-protocol-lab -C Debug -R "pae.tools.protocol_lab.bitfield_(generation|schema)_contract" --output-on-failure
ctest --test-dir out/build/windows-msvc-protocol-lab -C Release -R "pae.tools.protocol_lab.bitfield_(generation|schema)_contract" --output-on-failure
```

两种配置均`2/2 PASS`。失败路径按精确JSON字段断言`format_version`、`operation_status`、
`diagnostic.id`、`replay_mode/replay_subject`、`current_execution_status`、
`comparison_status/comparison_equal`、Frame状态及`send_attempted=false`；没有以嵌套同名字段或
宽泛字符串匹配替代。Schema正反例同时运行`Test-Json`和真实Lab编译器，避免规则分叉。

### 5.6 三项P2与PlanBuilder防御复核（2026-09-06）

本轮独立复现并关闭：

1. Schema 0.2 Inspect的19字节输入首次为`INPUT_ERROR/PAE_LAB_FRAME_LIMIT_EXCEEDED`，旧Replay
   绕过Plan上限进入Codec并变为`UNKNOWN_MESSAGE`。现Inspect与Replay共用当前Plan资源门禁；
   同配置Replay A/B均重新得到相同失败且Codec调用为零，放宽到19字节的替换配置才执行Decode，
   产生可读取、可再次Replay的`DIFFERENT`。当前执行失败时即使比较`EQUAL`仍返回执行失败退出码5。
2. 位容器常量／保留位与固定字节Matcher冲突原可通过编译，在Encode最终复核才失败。现按容器
   `byte_order`、`bit_numbering`和成员位区间计算确定掩码：常量成员写入位与未覆盖base位参与冲突，
   动态成员覆盖位不按base误拒绝。DomainValidator给出`MATCHER_CONFLICT`及常量值或base_value定位，
   PlanBuilder发布前执行同等防御检查。
3. 自洽Hash篡改把BOOL改为未知kind或UINT64后，旧Reader接受，Replay曾同时输出
   `comparison_status=EQUAL`、`comparison_equal=true`和`TYPED_FIELDS`。现Reader严格校验全部受支持
   kind及Raw/Logical/enum_known组合；Replay相等必须同时满足指纹相等且确定性差异分类为空。

PlanBuilder另在不增加公开接口的测试Peer中验证：单字节位容器内部字节序必须为
`NOT_APPLICABLE`，`bit_numbering`必须为有效枚举；损坏Draft中的单Message和全局容器数量须复用
字段数量预算并在辅助数组分配前拒绝。正常配置入口原已由ResourceBudgetValidator执行相同限制。

最终实际验证：Protocol Lab Debug/Release各`7/7`，全切片Debug/Release各`35/35`；因本轮修改
PlanBuilder，重新执行Product-only Debug/Release构建并确认CTest为0；Lab-on/Testing-off两种配置
也重新构建并确认CTest为0。仅UDP完整回归使用既有本机Loopback，未执行人工或非Loopback网络。

## 6. 日志

日志目录为`out/validation/dec040/`，关键文件：

- `test-targeted-core-lab-debug.log`、`test-targeted-core-lab-release.log`；
- `test-protocol-lab-debug.log`、`test-protocol-lab-release.log`；
- `test-all-slices-debug.log`、`test-all-slices-release.log`；
- `build-product-only-debug.log`、`build-product-only-release.log`；
- `final-static-checks.log`。

本次P2纠错日志位于`out/validation/`：

- `dec040-p2-targeted-debug.log`、`dec040-p2-targeted-release.log`；
- `dec040-p2-lab-debug.log`、`dec040-p2-lab-release.log`；
- `dec040-p2-all-slices-debug.log`、`dec040-p2-all-slices-release.log`。

本轮复核日志位于`out/review/dec040-p2-repro-20260906/`：

- `pre-fix-artifact-summary.log`；
- `fix-focused-debug.log`；
- `fix-protocol-lab-debug.log`、`fix-protocol-lab-release.log`；
- `fix-all-slices-debug.log`、`fix-all-slices-release.log`。

上一轮P2纠错未修改Plan/Core，因而当时未重跑Product-only。本轮触及PlanBuilder内部防御后，
已按5.6重新执行Product-only；两轮证据不混用。

## 7. 未验证项与提交边界

- 未执行Linux GCC/Clang，不声明跨平台实测；
- 未执行非Loopback主动发送、真实设备、硬件或现场门禁；
- 未把公开人工向量升级为真实协议Golden；`PROTOCOL_GOLDEN_PASS`不升级；
- 原有人工Lab验收不自动扩展到位字段能力；
- 未扩展Runtime/Session、注册路由、线程、持续监听、重试、GUI、公共API或C ABI；
- 本轮未Stage、Commit或Push；三项P2及PlanBuilder防御已完成自动化纠错，DEC-040整体仍处于
  待总控审查状态。
