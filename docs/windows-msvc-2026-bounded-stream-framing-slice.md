# Schema 0.9有界流式切帧Windows验证记录

日期：2026-09-10。源码基线：`main` / `dc5c4433b51961a9e2d24f425fba672a7121290e`；
本记录对应其上未提交的Schema 0.9 Framer检查点。未执行Stage、Commit或Push。

## 1. 结论与边界

Windows x64 MSVC Debug/Release全切片非UDP测试各`49/49`通过；Product-only与
Lab-on/Testing-off隔离构建通过，后者注册测试数为0。验证使用公开独立合成配置和手写字节向量，
未执行任何网络收发，也未执行Linux、真实协议Golden、设备、现场、人工Lab或性能验收。

Protocol Lab没有获得Schema 0.9执行能力。Inspect、Encode和UDP命令在合法0.9配置编译后、读取
Frame/Values、调用Codec、调用UDP Adapter或发布证据之前统一返回既有
`PAE_LAB_CONFIG_COMPILE_FAILED`；有无`--record-root`均不生成Bundle。

## 2. F01～F20证据映射

| 项 | 自动化/审查证据 | 本次结论 |
| --- | --- | --- |
| F01 | Compiler/Core/Lab旧代全切片回归；0.8携带stream成员精确拒绝；Builder损坏Draft代际门禁 | 通过；Builder门禁于2026-09-10专项纠错复核 |
| F02 | 三策略公开配置；未知成员、null同步头、联合外来成员、1/2/4字节序及准确Pointer | 通过；未运行官方Meta-Schema自验证 |
| F03 | 固定长度单字节分片、跨submit、单次双帧和尾部消费 | 通过 |
| F04 | 垃圾前缀、跨submit同步头、后缀保留、payload内同步字节不误切 | 通过 |
| F05 | 手写1/2/4字节大小端总长度帧 | 通过 |
| F06 | 最小/最大合法F、最大F半帧NEED_MORE及末字节恰好交付 | 通过 |
| F07 | F=0、非零小于min、F大于max立即拒绝并从候选下一字节恢复 | 通过；4字节整数极值由无符号安全读取静态审查覆盖 |
| F08 | 非法候选后下一合法帧；晚置长度字段下的重叠分段压缩、工作计数和有限empty续处理 | 通过；2026-09-10专项纠错复核 |
| F09 | 最大合法半帧不扫描payload同步头；显式reset | 通过；超时仍归宿主 |
| F10 | 宿主示例先切完整F，再调用既有Core | 通过 |
| F11 | 边界合法但Core返回UNKNOWN_MESSAGE的首帧不回扫；同submit后一帧仍成功且仅各回调一次 | 通过 |
| F12 | sink STOP后`bytes_consumed`停在已交付帧末尾，宿主重提后缀无重复 | 通过 |
| F13 | 帧数/工作预算、待交付帧、内部同步复制及分段搬移残留均可empty推进；搬移中Reset | 通过；2026-09-10专项纠错复核 |
| F14 | submit入口原子拒绝；含搬移游标的Workspace session exact/−1；同步头宿主限额；受检容量加法 | 通过；完整Runtime聚合准入不在范围内 |
| F15 | Builder代际、联合非适用字段、KMP表和Message长度损坏Draft负例；Compiler交叉校验 | 通过；代际/残留于2026-09-10专项纠错复核 |
| F16 | Compiler估算=Builder复算=Arena仍由既有合同覆盖；同步字节/KMP数组进入Plan；首次及重复push零分配 | 通过 |
| F17 | 回调期借用；同Workspace重入/并发拒绝；不同Workspace同Plan独立推进 | 通过；异步使用仍要求宿主复制 |
| F18 | 固定流只承诺显式reset，不宣称错位恢复 | 通过 |
| F19 | `pae_stream_framing_example`输出`decoded_frames=1 network_calls=0` | 通过 |
| F20 | Debug/Release全切片、Product-only、Testing-off和依赖边界 | 通过 |

主要专项用例位于`tests/protocol_framing/stream_framer_tests.cpp`、
`tests/config_compiler/config_compiler_contract_tests.cpp`、`tests/protocol_lab/`；公开样例为
`examples/config/synthetic_stream_framing_slice.pae.json`。

## 3. 实际命令与结果

所有MSVC命令均先执行：

```powershell
& 'D:\develop_env\Microsoft Visual Studio\18\Professional\Common7\Tools\Launch-VsDevShell.ps1' `
  -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
```

全切片使用Ninja独立目录，启用Loader、Core、Protocol Lab、Schema 0.5～0.9、旧Lab 0.5～0.8、
Framer和两个示例。历史独立`PAE_BUILD_LAB_V06_*`/`V07`测试开关不属于完整Lab组合，保持OFF。

```powershell
cmake --build out/v09-full-debug
chcp 65001
ctest --test-dir out/v09-full-debug -LE udp --output-on-failure

cmake --build out/v09-full-release
chcp 65001
ctest --test-dir out/v09-full-release -LE udp --output-on-failure
```

结果：Debug `49/49`，Release `49/49`；`-LE udp`明确排除
`pae.tools.protocol_lab.udp_exchange`。Debug第一次在默认控制台代码页运行时，旧Schema 0.8 CLI
用例把中文仓库路径误解码并报告文件缺失；`chcp 65001`后该用例单独及全矩阵均通过，未修改旧代码。

隔离构建：

```powershell
cmake -S . -B out/v09-product-only -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF <Loader/Core/Schema0.5-0.9/Framer options>
cmake --build out/v09-product-only

cmake -S . -B out/v09-lab-testing-off -G Ninja -DCMAKE_BUILD_TYPE=Release `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF <all compiler/Lab0.5-0.8/Framer options>
cmake --build out/v09-lab-testing-off
ctest --test-dir out/v09-lab-testing-off -N
```

结果：两者构建成功，Testing-off为`Total Tests: 0`。完整日志位于：

- `out/v09-full-debug-ctest-offline-utf8.log`
- `out/v09-full-release-ctest-offline-utf8.log`
- `out/v09-product-only-build.log`
- `out/v09-lab-testing-off-build.log`
- `out/v09-lab-testing-off-ctest-N.log`

### 3.1 COMPACT_INVALID进展纠错专项

全矩阵完成后，总控静态审查发现：合法晚置长度字段配置可能形成大于单次工作预算的保留区，而旧
实现要求一次性计费并搬移整个保留区。修复前隔离复现中，`max_work_units=5`、保留区6字节时，
连续17次empty提交均返回`WORK_BUDGET_REACHED`且`work_units_used=0`、缓存不变；预算6的对照
能够完成搬移。这是修复前新增证据，不将此前49项通过回溯解释为已覆盖该路径。

修复增加Workspace私有搬移游标，按每次剩余预算前向分段`memmove`，仅在整段完成后提交discard、
有效长度和状态迁移。正式专项使用`length_field.byte_offset=8`的公开合成夹具，验证预算5下有限
empty推进至`NEED_MORE`、补齐后缀只交付一次且字节不丢失、重叠分段搬移、搬移中Reset、每次
工作量不超预算、首次/重复push零分配，以及Workspace实际`sizeof`变化后的session exact/−1准入。

本次仅串行执行受影响的Framer专项，Debug与Release各`1/1`通过；F08、F13、F14相关断言位于同一
专项。没有重跑无变化的完整49项，也没有执行网络测试。日志：

- `out/v09-progress-repro/repro-budget5.log`（修复前预算5，进展停滞）
- `out/v09-progress-repro/control-budget6.log`（修复前预算6对照）
- `out/v09-progress-repro/fixed-budget5.log`（修复后预算5，有限推进至`NEED_MORE`）
- `out/v09-compaction-debug-build.log`
- `out/v09-compaction-debug-test.log`
- `out/v09-compaction-release-build.log`
- `out/v09-compaction-release-test.log`

### 3.2 Builder代际与严格联合纠错专项

在上述专项后，总控继续审查发现Builder启用Schema 0.9分支时，没有把非`COMPLETE_RECORD`的
stream描述限定为Schema 0.9；同时`COMPLETE_RECORD`、`fixed_length`和`sync_fixed_length`没有
复核不适用的长度字段offset/order零值。修复前使用真实Domain/Resource/Assembly能力链产生预算
自洽Draft，仅把合法0.9 fixed stream版本改为0.8，Builder仍成功发布Schema 0.8 stream Plan；分别
注入offset=7或BIG order时，三种非长度策略也会冻结成功。正常JSON Loader/Compiler仍在Structural
阶段以`UNKNOWN_PROPERTY`拒绝0.8 stream属性，因此版本问题限定为Builder发布前防御缺口；残留
字段在当前固定/完整记录执行及快照中不被读取，但违反严格联合不变量。

修复在Builder中要求所有非`COMPLETE_RECORD`描述必须属于Schema 0.9，并为三种非长度策略补齐
offset=0/order=`NOT_APPLICABLE`复核。正式test peer在预算自洽Draft上逐项注入：0.9 fixed降为0.8、
fixed/sync-fixed/complete-record各自的offset和order残留；全部精确断言
`INVALID_FRAMING_PLAN`及`framing_index=0`。合法0.9 fixed、0.9 sync-fixed和旧0.8 complete-record
作为独立成功对照。

本次只执行受影响的Compiler/Builder与Framer冻结集成专项，不把历史全矩阵改写为本轮结果。日志：

- `out/v09-builder-version-repro/repro.log`（修复前动态证据）
- `out/v09-builder-version-repro/fixed.log`（同一复现程序链接修复后Builder）
- `out/v09-builder-defense-debug-build-final.log`
- `out/v09-builder-defense-debug-test-final.log`
- `out/v09-builder-defense-release-build-final.log`
- `out/v09-builder-defense-release-test-final.log`

## 4. 资源、格式与剩余限制

Plan冻结strategy、边界、长度描述符、同步字节和KMP前缀表，Compiler/Builder按相同数组类别复算
Arena。`max_framing_buffer_bytes`只表示各Pipeline最大预分配缓存；创建器用
`sizeof(StreamFramingWorkspace)+pipeline_frame_capacity`报告完整Workspace，并验证exact/−1准入。
这不是完整Runtime活跃流内存聚合准入。

本切片不新增Lab Result/Record/Event/Values/指纹版本，不迁移历史证据。Schema 0.1～0.8行为由全
切片回归保持；Core仅允许0.9 Plan继续使用既有完整记录算法和签名。固定长度流无法可靠恢复插入或
丢字节；同步碰撞也不证明真实边界。Windows合成证据不代表生产可用或真实协议兼容。
