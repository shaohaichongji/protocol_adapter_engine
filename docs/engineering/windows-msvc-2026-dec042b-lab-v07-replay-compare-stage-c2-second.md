# PAE-DEC-042B Lab C2 第二段 Replay/Compare Windows 验证

## 1. 范围与结论

本检查点在默认关闭、Testing-only 的 C 执行 0.7 隔离目标中实现 Plan 关联、指定 Pipeline
Decode、Replay 历史快照及独立 Compare。Values 保持 0.4，Result 与确定性指纹保持 0.6，
Record/Event 保持 0.7；未开放普通 CLI、C3、网络或新的 Core 算法。

Windows MSVC Debug/Release 当前源码专项各 2/2、完整注册离线矩阵各 22/22 通过。底层 Reader
仍无 Compiler/Core/Plan 链接依赖。该结果待总控复核，不等于提交批准，也不升级 Linux、Oracle、
真实协议 Golden、性能、设备或现场证据。

## 2. 实现

- 文件、格式、Hash 与跨文件检查继续由底层 Reader 完成；上层 `QualifyRunEvidence` 才编译原配置，
  核对 Schema、配置、Protocol、Pipeline、Message、方向、字段顺序、类型、conversion raw 标签及
  失败字段/Values 作者索引。准入失败发生在执行与发布之前。
- `InspectPipeline` 只查询父 Result 指定的 Pipeline 一次；仅 `OK/ONE` 进入一次主 Decode。
  全局初次 Inspect 路径未改，测试专用接缝只用于形成指定 Pipeline 的 ZERO/MULTIPLE/查询错误。
- Replay 保留原 operation，写入父 Record 与历史 Result 的逐字节快照，绑定父子配置和 Values/Frame
  的长度与 Hash；Inspect 另绑定父 Result Pipeline。当前结果按既有 0.6 指纹记录 EQUAL/DIFFERENT，
  无 Result 记录 NOT_EVALUATED/CURRENT_RESULT_UNAVAILABLE。
- Replay Bundle 可独立读取，不要求父目录仍存在；合格有 Result 的子 Run 可继续 Replay。无 Result
  子 Run 可读但不能成为下一次基准。
- 独立 Compare 只读双方 C 执行证据并执行 Plan 准入，不调用 Codec、不发布 Bundle；不同 operation
  明确拒绝，不冒充协议差异。数学等价 Decimal 使用既有 A20 规范指纹，因此原 Values 文本可不同。

## 3. 第 21.4 节验收映射

| 验收组 | 当前自动化证据 |
| --- | --- |
| Plan 身份 | 合法 Encode/Inspect 与合法失败通过；自洽非法 Pipeline、Message、字段顺序、conversion raw 标签、失败 Values 索引在 Plan 层拒绝；拒绝后无子目录 |
| 指定 Pipeline | Inspect Replay 查询计数固定 1、Decode 固定 1；ZERO/MULTIPLE/INVALID_ARGUMENT 均为 1 次查询、0 次 Decode、无 Result；初次全局 RUN 既有断言保持 |
| 执行与比较 | Encode/Inspect Replay 成功；相同 Codec 失败为 EQUAL 但仍为 CODEC_ERROR；等价 Decimal 独立 Compare 为 EQUAL；合法不同值为 DIFFERENT；不同 operation 拒绝 |
| 历史链 | Inspect Run→Replay A→Replay B 均可完整读取及 Plan 准入；父 Record 不变；父子材料、父身份及比较声明的自洽篡改拒绝；无 Result 不可再次 Replay |
| 失败边界 | Plan 准入拒绝不发布；指定 Pipeline 结构失败不 Decode；第一段 Review/映射/Writer 故障及真实计数断言继续运行 |
| 回归隔离 | A/B/C1、Core、第一段与第二段共同组成当前 22 项离线矩阵；B 合成 0.6 不能作为 C Compare 输入；Reader 隔离链接复核通过 |

Hash 只证明包内一致性，不提供签名、来源认证或防止全面自洽重写的能力。测试中的结构状态强制
接缝是内部防御路径，不宣称由合法父材料自然产生。

## 4. Windows 命令与日志

开发环境使用：

```powershell
& 'D:\develop_env\Microsoft Visual Studio\18\Professional\Common7\Tools\Launch-VsDevShell.ps1' `
  -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
cmake --build out/dec042b-c2-run-<debug|release>
ctest --test-dir out/dec042b-c2-run-<debug|release> -N
ctest --test-dir out/dec042b-c2-run-<debug|release> `
  -R '^pae\.tools\.protocol_lab\.v07_' --output-on-failure
ctest --test-dir out/dec042b-c2-run-<debug|release> --output-on-failure
```

| 配置 | 专项 | 完整离线矩阵 | 日志 |
| --- | ---: | ---: | --- |
| Debug | 2/2 PASS | 22/22 PASS | `out/dec042b-c2-second-debug-targeted-final2.log`、`out/dec042b-c2-second-debug-full-final2.log` |
| Release | 2/2 PASS | 22/22 PASS | `out/dec042b-c2-second-release-targeted-final2.log`、`out/dec042b-c2-second-release-full-final2.log` |

对应构建和注册清单为 `out/dec042b-c2-second-{debug,release}-build-final2.log` 与
`out/dec042b-c2-second-{debug,release}-registered.log`。首次恢复编译未初始化 Developer Shell，
因 MSVC 标准库路径缺失而失败，保留为 `out/dec042b-c2-second-debug-build-initial.log`；初始化后
编译通过。一次增量运行现象与旧测试对象未随结构体头文件重编译导致的 ABI 不一致相符；两个测试
曾崩溃，相关对象重新编译后消失。失败日志为 `out/dec042b-c2-second-debug-existing-tests.log`，最终
双配置结果如上。

本次没有改变 CMake、目标依赖或普通 Lab 开关，故未重复 Product-only、Testing-off 与既有错误
开关配置门禁；第一段相同目标的历史隔离构建不冒充本轮执行结果。Reader 当前链接命令复核日志为
`out/dec042b-c2-second-release-reader-link.log`。

## 5. 总控复核P2纠错

修复前Debug专项使用完全重算长度、Record描述符、historical_baseline、指纹、比较声明及清单Hash
的副本，实际确认：

- Encode父历史Result保留`operation_kind=encode`却声明`DECODE_RX/RX`时，A模型可读且C Reader曾
  接受；
- 父Record descriptor集合将Event角色改为未知路径时，C Reader曾接受；父`values_file=null`
  已被既有`ValidateTerminal`角色门禁拒绝，不属于新增产品缺陷；
- INT64字段和合法INT64 Values自洽声明`BYTES_LENGTH_MISMATCH`时，Plan准入曾接受。

修复前日志为`out/dec042b-c2-second-p2-prefx-debug-build.log`和
`out/dec042b-c2-second-p2-prefx-debug-targeted.log`，专项`0/1`，三个目标断言失败。修复后共享
校验同时约束当前及历史Result的operation、Replay模式/主体、终态和诊断；父Record descriptor
集合按其角色精确校验，历史Frame只用父descriptor的长度与Hash绑定；BYTES长度失败要求目标Plan
字段及输入均为BYTES。未递归读取父路径、未重执行历史Codec，也未改变格式或A20指纹。

最终验证：

| 配置 | v0.7专项 | 完整离线矩阵 | 日志 |
| --- | ---: | ---: | --- |
| Debug | 2/2 PASS | 22/22 PASS | `out/dec042b-c2-second-p2-debug-targeted-final2.log`、`out/dec042b-c2-second-p2-debug-full-final2.log` |
| Release | 2/2 PASS | 22/22 PASS | `out/dec042b-c2-second-p2-release-targeted-final2.log`、`out/dec042b-c2-second-p2-release-full-final2.log` |

最终构建日志为`out/dec042b-c2-second-p2-{debug,release}-build-final2.log`；修复过程中的Debug
单用例复核日志另保留为`out/dec042b-c2-second-p2-fixed-debug-targeted-final2.log`。

## 6. 边界

- C2 第二段仍为内部隔离 API，不定义普通 CLI 参数、退出码或 `--expect-status`。
- 未执行 UDP 或其他网络收发，未运行 Linux、Oracle、真实协议 Golden、人工 Lab、硬件、现场或
  性能测试。
- 未改变 A/B 接受域、A20 指纹、Values/Result/Record/Event 版本或 Core 算法。
- C3、普通 Protocol Lab 集成和人工离线验收仍待后续独立授权。

当前状态：待总控复核。
