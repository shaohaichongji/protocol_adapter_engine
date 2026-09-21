# PAE 生成产物清理执行记录（2026-09-20）

## 1. 结论

依据用户对 [2026-09-20 清理盘点](../archive/engineering-20260921/pae-artifact-cleanup-inventory-20260920.md) 第 6 节精确路径的授权，已完成：

1. 六个历史 G1/UI 根的最小证据归集、逐文件 Hash 校验和删除；
2. 两个空生成根及现场仍为空的 `src\core` 删除；
3. SDK/Lab 交付保护集合逐文件 Hash 前后核对；
4. 六个 G2 构建/验证根的文件数、目录数、逻辑长度和树元数据签名前后核对。

执行结果：

- 删除精确目录根：**9 个**；
- 删除普通文件：**37,519 个**；
- 删除目录：**9,902 个**（含 9 个根）；
- 删除前逻辑长度：**6,390,488,445 bytes（5.952 GiB）**；
- 证据 payload：**149 个文件、8,432,280 bytes（8.042 MiB）**；
- 删除后 9 个根均不存在；
- 保护集合 501 个文件、124,344,387 bytes，路径/长度/SHA-256 集合签名前后一致；
- G2 六根前后统计及树元数据签名一致。

6,390,488,445 bytes 是删除前按普通文件逻辑长度求和，不是磁盘自由空间增量，也不宣称等于实际释放簇空间。

## 2. 基线与执行门禁

执行基线为 `main@fa81329563dd3aea9bb167ef9bd34526a2606161`。开始前保留了总控和其他执行任务的全部既有未提交文档修改，暂存区为空。

正式执行前门禁均通过：

- 九个候选均解析为清单中的精确绝对路径，严格位于仓库根内；
- `out\embedding-boundary-host`、`out\host-dec032`、`src\core` 现场仍为空；
- 候选根、候选递归项及从候选到仓库根的祖先均无 reparse point；
- Git 只登记当前主工作树，无其他 worktree；
- 未发现 `pae_protocol_lab_ui`、CMake、CTest、MSBuild、Ninja、`cl`、`link`、JOM 或 qmake 进程；
- `deliverables\sdk`、`deliverables\lab`、两组 `out` SDK 根、六个 G2 根、两套外部 clean SDK Lab 及本机 Qt 根均存在；
- dry-run 精确命中 9 根、37,519 文件、6,390,488,445 bytes，以及计划归集的 149 文件、8,432,280 bytes。

dry-run 命令：

```powershell
& 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\evidence\cleanup-20260920\cleanup-generated-artifacts-20260920.ps1'
```

正式执行命令：

```powershell
& 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\evidence\cleanup-20260920\cleanup-generated-artifacts-20260920.ps1' -Execute
```

脚本使用 PowerShell `-LiteralPath` 对每个候选逐项处理；未使用通配符或跨 shell 拼接删除。

## 3. 证据归集

证据根：

`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\evidence\cleanup-20260920`

归集规则：

- 五个 build 根：`CMakeCache.txt`、`CMakeFiles\CMakeConfigureLog.yaml`、`Testing\**`、其他 `*.log`；
- G1 验证根：`refinement-logs\**`、各迭代 `logs\**`、`INPUT_PROVENANCE.json`、`INPUT_SHA256.json`。

归集完成后，脚本逐文件比较来源和 payload 的长度、SHA-256；149/149 一致，才进入删除阶段。删除完成后的独立复算再次确认：

- manifest 中 `source_sha256 == target_sha256`：149/149；
- payload 当前文件存在、长度和 SHA-256 与 manifest 一致：149/149；
- 不一致：0。

根级机器证据：

| 文件 | 内容 |
| --- | --- |
| `cleanup-generated-artifacts-20260920.ps1` | 本次固定白名单、门禁、归集、删除和后置核对脚本 |
| `candidate-roots-before.json` | 9 根删除前文件数、目录数、逻辑长度和树元数据签名 |
| `evidence-plan.json` | 每根证据选择规则、预计文件/长度和报告引用 |
| `evidence-manifest.json` | 149 个 payload 的来源绝对根/相对路径、目标路径、长度、mtime、双向 SHA-256、reparse 标记和报告引用 |
| `report-references.json` | 候选根到历史 Markdown 行号的引用映射 |
| `protected-before.json` / `protected-after.json` | SDK/Lab 保护集合逐文件路径、长度和 SHA-256 |
| `g2-roots-before.json` / `g2-roots-after.json` | G2 六根前后统计与树元数据签名 |
| `execution-log.json` | 逐根删除时间、计数、长度和最终状态 |

一次执行后人工汇总查询曾把 `Where-Object source_sha256 -ne target_sha256` 的右侧字段名误当作字符串，产生 149 个假不一致；随即改用脚本块 `$_.source_sha256 -ne $_.target_sha256` 并对 payload 逐文件重新计算，确认两类不一致均为 0。删除门禁使用的是脚本内部显式变量比较，不受该汇总查询错误影响。

## 4. 删除清单与结果

| 绝对路径 | 文件 | 目录 | 逻辑 bytes | 结果 |
| --- | ---: | ---: | ---: | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\embedding-boundary-host` | 0 | 1 | 0 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\host-dec032` | 0 | 1 | 0 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\src\core` | 0 | 1 | 0 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-gui-g1` | 1,044 | 813 | 380,012,313 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u1` | 1,124 | 833 | 450,287,941 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u2` | 1,079 | 815 | 455,957,881 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-u2b` | 1,079 | 815 | 461,014,382 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-ui-polish` | 1,042 | 811 | 383,785,947 | 已删除，不存在 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\lab-gui-g1` | 32,151 | 5,812 | 4,259,429,981 | 已删除，不存在 |

删除前、证据复制后和每个根删除前均重新核对树元数据签名；执行中未发现候选变化。没有删除清单外路径。

## 5. 保护项复核

### 5.1 SDK 与 Lab 交付包

逐文件 Hash 保护范围：

- `deliverables\sdk`；
- `deliverables\lab`，包含 `98df5e0` 与 `fa81329`；
- `out\sdk-clean-checkpoint`；
- `out\sdk-clean-checkpoint-validation`。

删除前后均为 501 个文件、124,344,387 bytes，集合签名均为：

`5F8E511105391DA879245DE0DF119E1AC137F671D23C785017F0BBF1CF4312E9`

前后 JSON 文件本身逐字节相同。

### 5.2 G2 构建与验证根

| 根 | 文件 | 目录 | bytes | 前后结果 |
| --- | ---: | ---: | ---: | --- |
| `out\build\windows-msvc-lab-g2-a` | 641 | 534 | 117,586,939 | 一致 |
| `out\build\windows-msvc-lab-g2-b` | 1,036 | 820 | 207,922,854 | 一致 |
| `out\build\windows-msvc-lab-g2-c` | 3,803 | 3,137 | 1,630,155,872 | 一致 |
| `out\validation\lab-binary-stream-g2-a` | 14 | 1 | 21,658 | 一致 |
| `out\validation\lab-binary-stream-g2-b` | 10 | 1 | 36,457 | 一致 |
| `out\validation\lab-binary-stream-g2-c` | 42 | 1 | 166,725 | 一致 |

六根的树元数据签名也分别前后一致。G2-C 验证根相较盘点报告中的 40 文件增加到执行前的 42 文件，是并行 Lab 归集任务补充交付候选证据后的现场状态；本次以执行前 42 文件为保护基线，删除后仍为 42 文件。

### 5.3 其他保护根

删除后仍确认存在：

- `F:\PersonalWorkspace\pae-lab-clean-sdk-static-20260919`；
- `F:\PersonalWorkspace\pae-lab-clean-sdk-shared-20260919`；
- `D:\develop_env\Qt\Qt5.11.3\5.11.3`；
- `deliverables\evidence\cleanup-20260919` 与本次新证据根；
- 其他源码、Schema、`local_private`（若存在）、测试、第三方依赖及未列入白名单的 `out` 内容。

删除后的 `out` 快照为 11,133 个普通文件、2,068,787,312 bytes（1.927 GiB），仍有既有 `out\symlink-capability-check\link.txt` 一个 reparse point；本次未处理或跟随它。

## 6. 验证边界与 Git 状态

- 本轮没有构建、测试或启动 UI；验证仅针对文件系统操作、归集 Hash、保护集合和精确路径不存在性。
- 删除不可恢复；149 个最小证据文件可通过 manifest 校验，但不是六个生成树的完整镜像。若重新需要构建树，须从保留源码、依赖和配置重新构建。
- 没有执行内核级句柄或 Alternate Data Streams 检查；执行前进程门禁通过，不代表对所有可能句柄的形式化证明。
- 结束时分支与 HEAD 仍为 `main@fa81329563dd3aea9bb167ef9bd34526a2606161`，暂存区为空。
- 未 Stage、Commit、Push 或发布；总控和其他执行任务的既有未提交文档修改均保留。

总控后置复核：独立确认9根均不存在，现场重算149个归集文件和501个保护文件的长度/SHA-256全部匹配；protected与G2前后JSON哈希一致，git diff --check通过、暂存区为空。未重复构建测试，G2前后元数据记录不等于逐文件内容哈希验证。

状态：精确清理已完成限定总控复核；执行任务停止写入，未提交或推送。
