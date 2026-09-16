# Windows MSVC 2026 ASCII 有界流式接收首片验证记录

日期：2026-09-12。范围：Schema 0.11 ASCII CRLF 有界流式接收的 PAE 内部首片；不包含
Protocol Lab、Evidence、Replay、UI、传输、网络或稳定公共 ABI。基线为
`main@407af0df6e7b77c05aa5afbc7f334822fe5ae205`。本记录只陈述本次实际实现与 Windows
离线验证，不替代总控审查或后续 Lab 集成验收。

## 1. 实现范围

- 新增默认关闭能力门 `PAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING`。它显式依赖 V10
  ASCII、V09 Framing、Loader 与 Core；与 Protocol Lab、Evidence 适配器或 UI 同时启用时
  在 CMake 配置阶段拒绝。
- Schema/Loader 仅为 0.11 接受 `stream_chunk` + `ascii_crlf`，要求解码后的
  `terminator_text` 恰为 CRLF，要求 `maximum_frame_length` 为至少 2 的整数，并拒绝其他
  Framing 联合成员。0.10 与 0.9 的接受范围保持原状。
- Compiler 对每个流式 RX action 执行 CRLF 首次出现位置的有限状态证明，覆盖 literal、字段
  允许字符集合、变长字段及零长度跨段边界；PlanBuilder 根据冻结前描述符独立复核终止符、
  长度、Decode 候选和边界不变量。
- Frozen Plan 新增 `ASCII_CRLF` 策略并复用既有受限资源模型。0.11 Plan Snapshot 使用独立
  `pae_plan_bundle_v0.11_ascii_stream_slice` 域，记录策略、CRLF、M、Pipeline/Message 绑定与
  资源事实；旧代快照路径未改。
- Framer 新增 ASCII 收集与超长丢弃状态。候选包含 CRLF；M 字节无结束符产生一次
  `RECORD_TOO_LONG`，随后有界扫描至下一 CRLF；跨 chunk 尾 CR、STOP、pending、预算续提、
  Reset 和工作区隔离沿用既有同步 Push 契约。
- Core 的 0.11 完整记录执行明确复用 0.10 ASCII 的 Decode/Encode、字段方向、缺失 action、
  输入要求和失败零交付语义。新增公开合成配置与无网络 host-push 示例。

## 2. 自动化覆盖

`pae.config_compiler.ascii_stream_schema_contract` 检查 0.11/0.10 正例，以及旧版本使用
ASCII stream、错误终止符、M 小于 2、外来联合成员和 0.11 使用 V09 Binary 策略的拒绝。

`pae.protocol_framing.ascii_stream_contract` 检查：

- 独立 Snapshot/资源事实和精确工作区内存边界；超提交上限在状态推进前原子拒绝；Push 路径
  分配观察为 0。
- literal 内部 CRLF、字段可产生 CRLF、零长度字段跨段形成 CRLF，以及 Decode 最大长度超过
  M 的准确 Compiler 诊断；损坏 Draft 的终止符、M、边界和无 Decode 候选由 Builder 拒绝，
  合法控制 Draft 可冻结。
- 单帧、逐字节、CR/LF 跨 chunk、两帧粘包、完整帧加半包、纯 CRLF 候选和恰 M 成功。
- M 无结束符、M 末字节 CR 后下一 chunk 为 LF、超长后合法记录、丢弃态 Reset；坏记录只计
  一次且不交付。
- 多次工作预算耗尽后的精确后缀续提、STOP 提交点、空 push 推进 pending、不重复交付、两个
  Workspace 共享 Plan 但状态隔离。
- Framer 候选数与 Core Decode 成功数分离；非法 ASCII、未知模板后仍能处理下一合法记录。
- 0.11 的缺失 Decode/Encode action，TX-only 字段仅供 Encode，RX-only 字段不要求 Values，
  错误方向字段输入被拒绝。

`pae.examples.ascii_stream_framing.host_push` 使用公开合成配置，将 CR 与 LF 分两次提交，复制
回调期借用字节后调用 Core Decode；不建立 socket 或其他传输。

## 3. 本次实际验证

主构建目录：`out/build/windows-msvc-ascii-stream`。实际配置使用 Visual Studio 18 2026、x64、
MSVC 19.51.36257.0、Windows SDK 10.0.22621.0 和 CMake 4.3.1-msvc1：

```powershell
cmake -S . -B out/build/windows-msvc-ascii-stream `
  -G "Visual Studio 18 2026" -A x64 `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON `
  -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON `
  -DPAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING=ON `
  -DPAE_BUILD_STREAM_FRAMING_SLICE=ON `
  -DPAE_BUILD_ASCII_STREAM_FRAMING_EXAMPLE=ON `
  -DPAE_BUILD_TESTING=ON
```

Debug 与 Release 串行执行：

```powershell
cmake --build out/build/windows-msvc-ascii-stream --config Debug
ctest --test-dir out/build/windows-msvc-ascii-stream -C Debug --output-on-failure -j 1
cmake --build out/build/windows-msvc-ascii-stream --config Release
ctest --test-dir out/build/windows-msvc-ascii-stream -C Release --output-on-failure -j 1
```

结果：Debug 构建退出 0、31/31 通过；Release 构建退出 0、31/31 通过。完整日志：
`out/ascii-stream-recovery-full.log`。其中 ASCII Stream 专项在两个配置均为 3/3 通过；格式化后
正确目标的独立重建日志为 `out/ascii-stream-recovery-targeted-rebuild.log`。

隔离与门禁：

- V11-off Debug 构建 `pae_protocol_conformance_runner` 退出 0；随后用它读取 0.11 合成配置，
  在 `/schema_version` 返回 `CONFIG_COMPILE_FAILED`，进程退出 2。日志分别为
  `out/ascii-stream-default-off-recovery-build.log` 和
  `out/ascii-stream-default-off-recovery-reject.log`。
- 仅打开 V11、未打开依赖时，CMake 配置退出 1，并命中 V10/V09/Loader/Core 显式依赖诊断。
- V11 与 `PAE_BUILD_PROTOCOL_LAB=ON` 同时启用时，CMake 配置退出 1，并在其他 Lab 能力检查
  前命中“未接入 Protocol Lab、Evidence 或 UI”诊断。日志：
  `out/ascii-stream-lab-gate-recovery.log`。
- Product-only、Testing-off 的 Release 构建退出 0，`ctest -N` 显示 `Total Tests: 0`。
  构建和门禁汇总日志：`out/ascii-stream-recovery-isolation.log`。
- Visual Studio 18 随附 clang-format 对本次 C/C++ 文件执行格式化后，`--dry-run --Werror`
  退出 0；`git diff --check` 退出 0。新增公开样例、测试和示例未检出本机绝对路径、身份、端点、
  密钥或私有协议词样。

断电恢复时发现一条真实的默认关闭编译缺口：Plan Snapshot 对 `ASCII_CRLF` 的引用未完全受
V11 宏保护，首次 V11-off 构建因此失败。补上编译期保护后，上述 V11-off 构建和拒绝检查均
通过。另将 V11 与 Lab/UI 的禁止组合检查前移，确保不先进入 Qt 或旧 Lab 专项门禁。较早的
失败门禁脚本及一次错误目标名的构建命令不作为通过证据。

### 3.1 Snapshot 定向纠错复核

总控复核后发现，初版 `pae_plan_bundle_v0.11_ascii_stream_slice` 虽已记录 CRLF、M 和独立格式
域，但 Message 的 `text_decode`/`text_encode` 序列化仍被 `v10` 条件独占。首次 31/31
矩阵没有直接断言 V0.11 的 action 内容，因此不能作为该项覆盖证据。

修复前先加入定向测试并实际运行。测试构建成功，但
`pae.protocol_framing.ascii_stream_contract` 失败，精确报告：

- V0.11 Snapshot 未保留 Decode/Encode 的 literal、field、长度、prefix table 和 null action；
- 仅将合法 RX literal `RX ` 等长改为 `RY `，Snapshot 未变化；
- 仅将合法 TX literal `TX ` 等长改为 `TY `，Snapshot 未变化。

修复前日志：`out/ascii-stream-snapshot-p2-before-fix.log`。产品修复仅将 action 序列化门禁从
`v10` 扩展为 `v10 || v11`；同文件其余 ASCII 执行事实门禁经静态审计，文本资源字段已经是
`v10 || v11`，字段 ASCII wire 事实按实际 `wire_codec` 序列化，未发现第二处 V0.11 遗漏。

修复后按 Debug、Release 串行重建并运行以下 6 项：Compiler contract、V0.11/V0.10 Schema、
V0.11/V0.9 Framer，以及 V0.10 ASCII Snapshot/Core contract。两个配置均构建退出 0、6/6
通过。新增断言确认 V0.11 action 的 literal bytes、field index、min/max length、prefix table、
缺失 action 的 null，以及同长度合法 RX/TX literal 差分均改变 Snapshot。V0.10 action Snapshot
与旧 Binary Framer 回归继续通过。格式化及最终精确关联断言后的日志：
`out/ascii-stream-snapshot-p2-final-regression.log`；Debug 首次修复确认日志为
`out/ascii-stream-snapshot-p2-after-fix-debug.log`。

## 4. 边界与未验证项

- 本次没有网络收发，也没有新增或运行 Protocol Lab、Evidence、Replay、Compare 或 UI。
- 未执行 Linux、多线程压力、长期运行、性能基准、真实协议 Golden、硬件或现场验证；Windows
  离线测试和分配观察不能提升为这些结论。
- 超长后的 CRLF 恢复只证明确定性、有界扫描，不证明业务边界真实性或数据认证。
- 本记录未修改已冻结契约、README 或路线文档；是否进入提交阶段仍待总控独立审查。
