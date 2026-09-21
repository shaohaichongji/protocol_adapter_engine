# PAE 生成产物清理盘点（2026-09-20）

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

> 后续已获用户精确授权并完成第 6 节九个目录的证据归集与删除；实际结果、保护项复核和可恢复性边界见 [2026-09-20 清理执行记录](../../engineering/generated-artifact-cleanup-execution-20260920.md)。

## 1. 结论

本轮在 `main@fa81329563dd3aea9bb167ef9bd34526a2606161` 上只读盘点了：

- 仓库 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out`；
- `F:\PersonalWorkspace` 直属、当前仍存在的两套明确 PAE clean SDK Lab 根。

未递归检查 `DEI`、`NexForgeVisionHost` 等无关项目，未跟随 reparse point，未删除、移动、复制、压缩或构建任何内容。`deliverables`、`local_private`、本机 Qt、源码/Schema，以及全部 G2 构建和验证根均排除在删除候选之外。

现场主要结论：

1. `out` 当前有 48,652 个普通文件、8,459,275,757 bytes（7.878 GiB）。唯一 reparse point 是既有 `out\symlink-capability-check\link.txt`，未跟随。
2. 历史 G1/UI 的 5 个构建根和 G1 大型独立消费根合计 37,519 个文件、6,390,488,445 bytes（5.952 GiB），是本轮主要可回收对象；但其中仍有输入身份、配置、CTest 与执行日志，必须先归集最小证据，因此归为“2：归集最小证据后再删”。
3. 4 个 UI 小型验证根合计仅 1,016,076 bytes，包含文档直接引用的日志和截图；原地保留比再搬移更稳妥。
4. 两个空的历史生成根可直接进入后续删除申请，但本轮未删除：
   - `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\embedding-boundary-host`
   - `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\host-dec032`
5. G2 仍是当前回收、交付与复核链的一部分；其 3 个构建根和 3 个验证根本轮一律保留，不用“已提交/已推送”替代当前证据与复核需要。
6. 两套外部 clean SDK Lab 根已在 2026-09-19 清理中收缩为 `deploy`、`logs` 和根清单，当前分别为 140,413,180 bytes 与 142,209,314 bytes；它们只做盘点，继续保留。

处置分类：

- `1`：可立即申请删除；仍需用户对精确绝对路径另行授权。
- `2`：先归集最小证据并校验，再申请删除。
- `3`：必须保留。
- `4`：用途或长期保留策略仍不确定，默认保留。

## 2. 计量、安全与占用检查

- 计量使用普通文件逻辑长度；`1 GiB = 2^30 bytes`，不代表实际占用簇或可释放空间。
- 遍历时遇到 reparse point 只记录对象，不进入目标。两套外部根、`out\build` 和 `out\validation` 的根及祖先均不是 reparse point。
- `git worktree list --porcelain` 只登记主工作树 `F:/PersonalWorkspace/协议解析拼接工具/protocol_adapter_engine`；下述候选均不在其他 worktree 中。
- 盘点时未发现 `pae_protocol_lab_ui`、CMake、CTest、MSBuild、Ninja、`cl`、`link`、JOM 或 qmake 进程。该进程快照不能证明不存在任意第三方句柄；真正删除前必须重查进程、路径、reparse point 和工作树。
- `out` 被仓库根 `.gitignore` 忽略，但“被忽略”不等于“无独有证据”。本报告的分类同时核对了验证报告引用、文件内容类别和复建来源。
- 本轮没有创建可选的 `out\validation\cleanup-inventory-20260920`，避免清单任务本身再制造待清理产物。

## 3. 本轮精确候选

### 3.1 分类 1：可立即申请删除

| 绝对路径 | 文件 / bytes | 用途与复建来源 | 当前引用 / 独有证据 | 安全状态 |
| --- | ---: | --- | --- | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\embedding-boundary-host` | 0 / 0 | 历史 Host 嵌入边界生成根；当前为空 | 未检出跟踪文档或脚本的精确路径引用；无文件可构成独有证据 | 非 reparse、非 worktree、无运行占用 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\host-dec032` | 0 / 0 | 历史 DEC-032 Host 生成根；当前为空 | 未检出跟踪文档或脚本的精确路径引用；无文件可构成独有证据 | 非 reparse、非 worktree、无运行占用 |

这两项只是后续删除申请候选，不是本报告授予删除权限。

### 3.2 分类 2：先归集最小证据后再删

| 绝对路径 | 文件 | bytes | 用途 / 复建来源 | 当前引用与独有证据 |
| --- | ---: | ---: | --- | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-gui-g1` | 1,044 | 380,012,313 | G1 Binary Encode D/R 构建树；可由 `fa81329` 源码、CMake、v142 与现有 Qt 重建 | G1 契约/验证报告引用；根内仍有 CMake 配置与 CTest 状态 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u1` | 1,124 | 450,287,941 | U1 中文界面构建树；含一次失败的外层配置和有效 `v142` 子构建 | U1 验证报告引用；两套 CMake 配置记录与 CTest 状态仍在根内 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u2` | 1,079 | 455,957,881 | U2 布局构建树；可由当前源码重建 | U2/U2B 报告引用；根内仍有配置与 CTest 状态 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u2b` | 1,079 | 461,014,382 | U2B 左右工作台构建树；其历史体验入口已被后续源码/交付候选取代 | U2B 报告与综合计划引用；根内仍有配置与 CTest 状态 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-polish` | 1,042 | 383,785,947 | UI 可读性收尾构建树；可由当前源码重建 | 可读性验证报告与综合计划引用；根内仍有配置与 CTest 状态 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-gui-g1` | 32,151 | 4,259,429,981 | G1 static/shared 多轮包外消费、部署与构建副本；源码已进入 `fa81329`，生成树可重建 | G1 报告直接引用 `refinement-logs` 与 `static/shared-atomic2-final`；各轮 `logs`、`INPUT_PROVENANCE.json`、`INPUT_SHA256.json` 是独有原始证据 |

六根合计 37,519 个文件、6,390,488,445 bytes（5.952 GiB）。它们及其祖先均非 reparse point，不属于其他 worktree，盘点时未发现相关运行进程。

#### 最小证据归集建议

后续若获准执行证据归集，应在被保护的 `deliverables\evidence` 下建立新的独立日期根；不得写回待删根。至少保留：

1. 5 个构建根中的全部：
   - `CMakeCache.txt`；
   - `CMakeFiles\CMakeConfigureLog.yaml`；
   - `Testing\**`；
   - 其他 `*.log`。
2. G1 大型验证根中的全部：
   - `refinement-logs\**`；
   - 每个 static/shared 迭代根的 `logs\**`；
   - 每个迭代根的 `INPUT_PROVENANCE.json` 与 `INPUT_SHA256.json`。
3. 另生成清单，逐文件记录来源绝对根、相对路径、长度、mtime、SHA-256、reparse 标记和对应验证报告引用；复制后逐文件复算 Hash，再允许进入删除复核。

按当前规则静态命中：构建根 28 个文件、2,417,081 bytes；G1 验证根 121 个文件、6,015,199 bytes；合计 149 个文件、8,432,280 bytes。此数量只用于规划，归集执行前仍要重新扫描，避免遗漏本轮之后新增的证据。

## 4. 必须保留的当前根

### 4.1 G2 构建与验证

| 绝对路径 | 文件 / bytes | 用途 / 复建来源 | 当前引用 / 独有证据 | 分类 |
| --- | ---: | --- | --- | ---: |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-a` | 641 / 117,586,939 | G2-A 非 Qt owner 构建根 | G2 契约和 A 验证报告引用 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-b` | 1,036 / 207,922,854 | G2-B DTO/adapter D/R 构建根 | G2 契约和 B 验证报告引用 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-c` | 3,803 / 1,630,155,872 | G2-C Session/UI/common/Binary-only 与配置门禁构建根 | G2-C 报告、契约、综合计划及当前交付回收均在使用 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-binary-stream-g2-a` | 14 / 21,658 | G2-A 原始专项日志 | 契约和 A 报告直接引用 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-binary-stream-g2-b` | 10 / 36,457 | G2-B D/R 与故障重读日志 | 契约和 B 报告直接引用 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-binary-stream-g2-c` | 40 / 163,781 | G2-C D/R、配置门禁、部署与体验依据 | G2-C 报告及当前交付候选报告直接引用 | 3 |

以上六根全部非 reparse、非其他 worktree；“源码已提交并推送”不等于这些当前复核证据可以删除。

### 4.2 UI 小型验证证据

| 绝对路径 | 文件 / bytes | 当前引用 / 独有证据 | 分类 |
| --- | ---: | --- | ---: |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-ui-localization-u1` | 40 / 137,018 | U1 报告直接引用 D/R build、headless 和 Binary/stream 日志 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-ui-layout-u2` | 61 / 69,782 | U2 报告直接引用前后修复与 D/R 烟测日志 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-ui-workbench-u2b` | 53 / 483,451 | U2B 报告直接引用日志及四张布局截图 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-ui-readability-polish` | 18 / 325,825 | 可读性报告直接引用 D/R 日志及两张截图 | 3 |

这四根体积合计不足 1 MiB，原地保留可避免文档引用失效；不建议为节省这部分空间另做搬移或删除。

### 4.3 当前 SDK、外部 clean Lab 与交付根

| 绝对路径 | 文件 / bytes | 用途 / 复建来源 | 当前引用 / 独有证据 | 分类 |
| --- | ---: | --- | --- | ---: |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-clean-checkpoint` | 189 / 29,946,570 | `98df5e0` 五套 clean SDK 原始候选 | SDK/Lab/G1 报告与交付归集报告引用；包内 provenance/manifest/hash 必须保留 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-clean-checkpoint-validation` | 63 / 122,473 | clean SDK 六组包外消费原始摘要 | clean SDK 报告直接引用 | 3 |
| `F:\PersonalWorkspace\pae-lab-clean-sdk-static-20260919` | 92 / 140,413,180 | static SDK Lab 已验证 D/R 部署；当前只剩 `deploy`、`logs` 与根清单 | `deploy` 68 文件、139,880,508 bytes；`logs` 22 文件；两份输入身份清单 | 3 |
| `F:\PersonalWorkspace\pae-lab-clean-sdk-shared-20260919` | 98 / 142,209,314 | shared SDK Lab 已验证 D/R 部署；当前只剩 `deploy`、`logs` 与根清单 | `deploy` 72 文件、141,248,572 bytes；`logs` 24 文件；两份输入身份清单 | 3 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables` | 本轮不计量 | 本机统一交付与历史清理证据根 | 保护 `sdk\98df5e0`、`lab\98df5e0`、`evidence\cleanup-20260919`，以及已完成限定归集的 `lab\fa81329`；后者共 19 文件、21,485,398 bytes，身份见 G2 交付候选报告 | 3 |

`deliverables` 本轮明确排除在清理范围之外，不能因为存在其他副本而删除。`local_private`（如存在）、`src`、`schema`/Schema 相关源码和配置、`tests`、`third_party` 以及 `D:\develop_env\Qt\Qt5.11.3\5.11.3` 同样不进入任何候选。

## 5. 其余 `out` 根的保守分类

这些根是 2026-09-19 已授权清理后特意留下的原始证据、SDK 身份、离线依赖或混合用途内容。本轮没有得到新的长期证据迁移策略，因此不因“体积小”“时间旧”或“无当前引用”改判为可删。

### 5.1 分类 3：原始验证/诊断证据继续保留

下表的路径均以
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\`
为基准；均非 reparse、非其他 worktree，盘点时无相关运行占用。

| 相对根 | 文件 / MiB | 当前引用状态 / 用途 |
| --- | ---: | --- |
| `agent-c3-acceptance-20260908-205122` | 109 / 0.143 | 两份 C3 验收/CLI 文档引用 |
| `agent-c3-acceptance-20260908-212535` | 97 / 0.133 | 两份 C3 验收/CLI 文档引用 |
| `ascii-pure-literal-repro` | 4 / 0.009 | ASCII text slice 报告引用 |
| `bench-memory` | 6 / 0.019 | 无精确路径引用，但内容为 benchmark 原始输出，证据策略未统一 |
| `crc-recovery` | 46 / 0.047 | CRC 恢复过程证据，当前无精确路径引用 |
| `crc-reference` | 9 / 0.068 | CRC slice 报告引用 |
| `crc-validation` | 37 / 0.112 | CRC slice 报告引用 |
| `dec042a-logs` | 52 / 0.131 | DEC-042A 报告引用 |
| `dec042a-smoke` | 19 / 0.021 | DEC-042A smoke 原始输出，当前无精确路径引用 |
| `dec042b-c3-manual` | 30 / 0.039 | 人工验证原始输出，当前无精确路径引用 |
| `dec042b-c3-manual-invalid-values` | 6 / 0.007 | 人工负例原始输出，当前无精确路径引用 |
| `dec042b-c3-unicode-pre-fix-中文 路径` | 11 / 0.012 | 两份 C3 文档引用 |
| `host-dec033a` | 2 / <0.001 | Plan memory 报告引用 |
| `lab-ascii-compat-isolation` | 37 / 0.574 | 对应验证报告与综合计划引用 |
| `lab-owned-presentation-types` | 18 / 0.118 | 对应验证报告与综合计划引用 |
| `lab-public-ascii-stream` | 60 / 0.101 | 非 Qt/UI 流式报告与综合计划引用 |
| `lab-public-ascii-stream-ui` | 44 / 0.294 | UI 流式报告与综合计划引用 |
| `lab-public-legacy-complete` | 38 / 8.333 | complete-record 报告与综合计划引用 |
| `lab-sdk-standalone-preflight-configs` | 12 / 0.520 | standalone preflight 配置快照，当前无精确路径引用 |
| `manual-lab` | 51 / 0.078 | Loopback 人工验收文档引用 |
| `public-api-stage1` | 20 / 0.042 | public compile metadata 文档引用 |
| `public-codec-stage1` | 82 / 0.220 | public codec/Lab 报告引用 |
| `public-consumer-metadata-stage1` | 32 / 0.045 | consumer metadata 报告引用 |
| `public-framer-stage2a` | 43 / 0.111 | public framer 报告与综合计划引用 |
| `public-host-stage2b` | 51 / 0.137 | Host 报告与综合计划引用 |
| `public-stream-description` | 74 / 2.943 | public stream framing 报告与综合计划引用 |
| `public-stream-sdk` | 39 / 0.076 | public stream SDK 报告与综合计划引用 |
| `review` | 234 / 0.393 | 多份历史 slice 报告引用的 review 证据集合 |
| `sdk-stage3-validation` | 218 / 0.432 | Stage 3 SDK 报告与综合计划引用 |
| `stage4-ascii-crash-diagnosis` | 33 / 0.070 | crash diagnosis 报告引用 |
| `stage4-ascii-crash-instrumented` | 8 / 0.064 | crash diagnosis 报告引用 |
| `stage4-ascii-facts` | 25 / 0.164 | ASCII facts 报告引用 |
| `stage4-ascii-public-a1` | 39 / 0.058 | A1/A2 报告与综合计划引用 |
| `stage4-ascii-public-a2` | 6 / 0.069 | A2 报告与综合计划引用 |
| `stage4-ascii-sdk` | 49 / 0.109 | 综合计划引用 |
| `stage4-ascii-smoke-fix` | 32 / 0.075 | crash diagnosis 报告引用 |
| `stage4-h1-validation` | 17 / 0.008 | H1 报告与综合计划引用 |
| `stage4-h2-validation` | 18 / 0.034 | H2/crash 报告与综合计划引用 |
| `stage4-p-validation` | 59 / 0.286 | physical query 契约/验证与综合计划引用 |
| `v08-empty-payload-diagnostic` | 18 / 0.013 | v0.8 报告引用 |
| `v08-readonly-precommit-decimal` | 10 / 0.009 | v0.8 报告引用 |
| `v08-recovery-runs` | 28 / 0.032 | 恢复运行原始证据，当前无精确路径引用 |
| `v08-review-bit-container` | 34 / 0.036 | review 原始证据，当前无精确路径引用 |
| `v08-review-missing-length` | 16 / 0.016 | review 原始证据，当前无精确路径引用 |
| `v08-review-round2-header-gap` | 17 / 0.018 | v0.8 报告引用 |
| `v08-tmp` | 6 / 0.005 | 名称虽为 tmp，但内容归属未逐文件证明，默认保留 |
| `v09-builder-version-repro` | 7 / 2.249 | v0.9 framing 报告引用 |
| `v09-progress-repro` | 8 / 1.024 | v0.9 framing 报告引用 |
| `yv-eol` | 127 / 6.295 | yyjson 依赖与仓库组织文档引用 |
| `validation\dec040` | 19 / 0.048 | DEC-040 报告引用 |
| `validation\dec040-debug-targeted` | 28 / 0.035 | 定向 Debug 原始证据，当前无精确路径引用 |
| `validation\dec040-p2-smoke-20260906a` | 18 / 0.019 | P2 smoke 原始证据，当前无精确路径引用 |
| `validation\length-field-20260909` | 41 / 0.114 | length-field 报告引用 |
| `validation\length-resume-debug` | 35 / 0.044 | resume Debug 原始证据，当前无精确路径引用 |
| `validation\schema-debug-inspect` | 1 / 0.005 | Schema Debug 检查输出，当前无精确路径引用 |

### 5.2 分类 4：用途/离线复现策略待确认

| 绝对路径或范围 | 文件 / bytes | 不确定点与当前处理 |
| --- | ---: | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\downloads` | 3 / 12,322,603 | 早期依赖下载缓存；删除会影响离线复现能力，继续保留 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sources` | 2,356 / 26,094,751 | yyjson 等依赖源码缓存；文档有引用，是否由包管理重新获取未形成统一策略 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\qt-checkpoint-closeout` | 12 / 6,772 | `third_party/qt-package.md` 引用的 Qt 关账证据；与本机 Qt 不同，不能按 Qt 缓存直接删除 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\symlink-capability-check` | 1 普通文件 / 4 bytes，另 1 个 SymbolicLink | 唯一 reparse point；能力检查用途已历史化，但删除必须先精确处理 link object，暂不列候选 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out` 根直属 605 个普通文件 | 16,047,535 bytes | 来源混合，不能用根级通配符处理；本轮未逐文件建立删除白名单 |

## 6. 后续删除申请清单

若总控和用户认可本盘点，建议分两批、分别授权：

### 批次 A：空根

```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\embedding-boundary-host
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\host-dec032
```

### 批次 B：完成第 3.2 节证据归集与 Hash 复核后

```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-gui-g1
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u1
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u2
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u2b
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-polish
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-gui-g1
```

不得把上述清单简写成 `out\build\windows-msvc-lab-*`、`out\validation\lab-*` 或其他通配符；这些模式会误包含必须保留的 G2 和 UI 小型证据根。

## 7. 验证边界与停止状态

- 本轮只做文件系统、Git/worktree、进程、reparse point、文档引用和逻辑长度检查；未重新构建、测试、启动 Lab 或验证产物功能。
- 没有检查 Alternate Data Streams，也没有使用内核级句柄枚举；“无占用”仅是进程级快照。
- 并行 Lab 任务随后完成 `deliverables\lab\fa81329` 限定归集和静态 Hash 复核；本报告仅记录其完成后的文件数/逻辑长度并整体保护，不把它升级为正式发布或 installed-SDK standalone 交付。
- 本报告没有改变任何产物、环境、源码、CMake 或验证证据；未 Stage、Commit、Push、发布或删除。
- 达到本任务停点后停止写入，等待总控复核和用户对精确删除批次另行授权。
