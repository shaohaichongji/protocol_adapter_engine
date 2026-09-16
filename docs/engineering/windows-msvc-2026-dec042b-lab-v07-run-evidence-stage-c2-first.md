# PAE-DEC-042B Lab C2 第一段 RUN Evidence 0.7 Windows 验证

## 1. 范围和结论

本检查点只实现默认关闭、Testing-only 的 C 执行 RUN Evidence 0.7：真实 C1 阶段观察、内部
RUN 编排、事务 Writer、严格 Reader 和隔离自动化。Values 仍为 0.4，Result 与确定性指纹仍为
0.6，B 合成 Evidence 0.6 保持原格式。未接普通 Protocol Lab CLI，未实现 Replay、Compare、
Plan 关联、网络或 C3。

Windows MSVC Debug/Release 当前源码专项各 2/2、相关 v06+v07 离线测试各 5/5 通过；第一段
P2 修复前的完整离线切片各 22/22 是历史证据，本轮未将其冒充修复后的完整重跑。Product-only
与 Lab-on/Testing-off 的 Debug/Release 隔离构建通过且均注册 0 个测试。该结论待总控复核，
不等于提交批准、Linux/Oracle/真实协议 Golden、设备或现场验证。

## 2. 实现对应

- `v06_execution` 增加仅供内部桥接使用的阶段观察接口，在准备、结构查询、主 Codec、Encode
  Review Decode 和 Result 映射的实际边界采集事件；测试接缝只在 operation counters 构建存在。
- `v07_run` 只接受 RUN 的 Inspect/Encode 输入，保留原配置、Values 或 Frame；Writer 失败仍保留
  已发生的 Codec/Review 次数，不返回正式发布路径，也不自动重试执行。
- `v07_run_evidence` 写入 `run_record_v0.7.json`、`events_v0.7.jsonl`、原始输入及可选 Result/
  Frame；采用 `.inprogress`、逐文件写闭后重读、完整自读预检、无覆盖重命名。
- Reader 在读取内容前拒绝链接/重解析点和不安全相对路径，随后严格校验文件集合、Manifest、
  payload 长度/Hash、未知/重复属性、终态、阶段事件、调用计数、Result 0.6/指纹/Frame绑定。
- 第一段强制 `invocation_kind=RUN`，`parent_run_id`、`comparison`、`historical_baseline`、
  `requested_pipeline_id` 均为 null；B 合成 0.6 仍由 B Reader 读取，C Reader 不猜代际。

恢复后源码审阅补强了两项既有契约不变量：Inspect 只接受适用于其入口的准备诊断；主 Codec
对象与调用计数双向一致，Review Decode 必须建立在成功主 Encode 上。补测时先暴露并修复了
合法 Inspect 配置准备失败 Bundle 被 Reader 错拒的问题；修复前日志为
`out/dec042b-c2-debug-targeted-after-review.log`，修复后两配置均通过。

总控后续 P2 审查指出的两个 Reader 关联缺口均已独立动态复现并做最小收紧：成功 Inspect 的
主 Decode 现在必须由 `structural_query.status=OK` 与 `candidate_class=ONE` 明确授权；
`RAW_ASSOCIATION_FAILED` 必须保留两次映射且严格为 `OK` 后 `FAILED`。此外 Result 0.6 的
`replay_mode/replay_subject` 必须与 Record operation 对应，成功 Result 禁止携带失败诊断，
Codec 失败的外层与当前执行诊断必须同时精确对应主 Codec 状态。未改变机器格式、版本或指纹算法。

## 3. 自动化覆盖

专项 `pae.tools.protocol_lab.v07_reader_isolation` 与
`pae.tools.protocol_lab.v07_run_evidence` 覆盖：

- Encode/Inspect 成功、Schema 0.5 无 conversion 且使用旧 Values 0.1；
- 配置和 Values 准备失败、零/多结构候选、结构查询 INVALID_ARGUMENT；
- Decode INTEGRITY_FAILED、Encode conversion/Codec 失败；
- Review Decode 错误、Review Message 不一致、raw 关联失败、Result 物化内部失败，以及主 Codec
  非 OK 后基础 Result 映射再次失败；
- Event 顺序/状态/次数、原始输入、Result/指纹有无及 Frame 角色；
- 写闭、写后重读、最终重命名失败，正式或 `.inprogress` 目标已存在时拒绝覆盖；
- Record/Event 未知和重复属性、REPLAY、非 null 历史字段、自洽终态/阶段/计数篡改、缺失/
  额外文件、Hash 不一致、成功后失败输出清空、目录链接门禁；
- B 合成 0.6 的 B Reader 正对照和 C Reader 拒绝对照。

P2 专项新增 9 个跨文件自洽变异：Inspect 的 UNKNOWN_MESSAGE/ZERO、AMBIGUOUS_MESSAGE/MULTIPLE、
INVALID_ARGUMENT/UNDETERMINED 三种结构事实，RAW 仅保留单次 FAILED 映射，Encode/Inspect 的
Result mode/subject 互换，成功 Result 注入诊断，以及失败 Result 的诊断错配或缺失。辅助函数会
同步 Result 指纹、Record payload 长度/Hash、Record 指纹和 Manifest，因此拒绝来自目标语义
校验，而不是低层文件关联失败；每条拒绝同时断言`stored.record.run_id`为空，
不将该检查扩大表述为对输出对象所有成员的逐项清空断言。

Reader 隔离目标的最终链接命令不含 Config Compiler、Protocol Core、Protocol Plan、普通
`protocol_operations` 或 Winsock；执行专项目标按预期链接 Compiler、instrumented Core 和 Plan，
但不含普通 Lab operations/Winsock。

## 4. 当前执行命令和证据

开发环境由 `Launch-VsDevShell.ps1 -Arch amd64 -HostArch amd64 -SkipAutomaticLocation` 初始化。
核心命令如下，Debug 与 Release 串行：

```powershell
cmake -S . -B out/dec042b-c2-run-<config> -G Ninja -DCMAKE_BUILD_TYPE=<Debug|Release> `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_BUILD_LAB_V06_FORMAT_TESTS=ON `
  -DPAE_BUILD_LAB_V06_EVIDENCE_TESTS=ON `
  -DPAE_BUILD_LAB_V06_EXECUTION_TESTS=ON `
  -DPAE_BUILD_LAB_V07_RUN_EVIDENCE_TESTS=ON
cmake --build out/dec042b-c2-run-<config>
ctest --test-dir out/dec042b-c2-run-<config> -N
ctest --test-dir out/dec042b-c2-run-<config> -R '^pae\.tools\.protocol_lab\.v07_' --output-on-failure
ctest --test-dir out/dec042b-c2-run-<config> --output-on-failure
```

第一段 P2 修复前，Debug 专项为 1/2，新增的 9 条语义拒绝均被 Reader 错误接受；证据见
`out/dec042b-c2-p2-prefx-debug-targeted.log`。修复后当前结果：

| 配置 | 专项 | 相关 v06+v07 离线测试 | 日志 |
| --- | ---: | ---: | --- |
| Debug | 2/2 PASS | 5/5 PASS | `out/dec042b-c2-p2-recovery-debug-targeted.log`、`out/dec042b-c2-p2-recovery-debug-lab-offline.log` |
| Release | 2/2 PASS | 5/5 PASS | `out/dec042b-c2-p2-recovery-release-targeted.log`、`out/dec042b-c2-p2-recovery-release-lab-offline.log` |

对应构建日志为 `out/dec042b-c2-p2-postfix-debug-build.log` 和
`out/dec042b-c2-p2-postfix-release-build.log`。P2 修改前、但第一段源码审阅修复后的完整离线切片
Debug/Release 各 22/22 通过，历史日志为 `out/dec042b-c2-debug-full-review-fix.log` 和
`out/dec042b-c2-release-full-review-fix.log`；本轮按关联范围复核 v06+v07，没有重复全量矩阵。

隔离构建日志为 `out/dec042b-c2-product-only-{configure,build,registered}.log`、
`out/dec042b-c2-product-only-debug-{configure,build,registered}.log`、
`out/dec042b-c2-lab-on-testing-off-{configure,build,registered}.log` 和对应的 `-debug-` 日志；
四种配置均构建成功且 `Total Tests: 0`。错误开关门禁日志为
`out/dec042b-c2-gate-testing-off.log`、
`out/dec042b-c2-gate-missing-prereqs.log`、`out/dec042b-c2-gate-ordinary-lab-v05.log`，三者均按
预期在配置阶段失败。链接证据见 `out/dec042b-c2-release-*-link-commands.log`。

## 5. 边界

- 第一段 Reader 只证明 Evidence 0.7 格式和包内关联，不验证冻结 Plan 中的 Pipeline/Message/
  Field 身份，因此不授予重执行资格。
- 未实现 C2 第二段 Replay/Compare/Plan 关联、C3 普通 CLI 或退出码映射。
- 未执行任何 UDP/网络测试；未运行 Linux、Oracle、真实协议 Golden、人工 Lab、硬件或现场验收。
- Hash 用于包内一致性，不提供来源认证、签名或防伪保证。

当前状态：待总控复核。
