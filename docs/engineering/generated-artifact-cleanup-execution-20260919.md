# PAE 生成产物清理执行记录（2026-09-19）

## 1. 结论

依据 [生成产物清理盘点](../archive/engineering-20260921/generated-artifact-cleanup-inventory-20260919.md) 第 5、7 节的固定候选清单，已完成“dry-run → 证据归集与逐文件校验 → 链接对象先移除 → 精确根删除 → 保护项复核”。

- 删除目录根：**99 个**；跳过 0 个，目录删除失败 0 个；
- 删除获批根内 SymbolicLink 对象：**26 个**；
- 保留获批根外 `out\symlink-capability-check\link.txt`：1 个；
- 删除前候选文件：157,540 个；
- 删除前候选逻辑长度：**35,999,609,815 bytes（33.527 GiB）**；
- 证据 payload：**7,912 个文件、26,654,536 bytes（25.420 MiB）**；
- 证据根含 7 份机器清单/前后快照后：7,919 个文件、38,517,545 bytes；
- 保护集合：删除前后均为 6,963 个文件，逐文件长度和 SHA-256 一致。

这里的 35,999,609,815 bytes 是 99 个根在删除前按不跟随 reparse point 的逻辑文件长度求和；所有根删除后均不存在，因此也是本次精确清理量。它不是文件系统空闲空间变化，未用磁盘自由空间差值代替清理量。

## 2. 固定范围与执行门禁

执行脚本为 `scripts/cleanup-generated-artifacts-20260919.ps1`。默认只 dry-run，只有显式 `-Execute` 才允许删除；本次固定白名单及实际结果如下：

| 分类 | 根数 | 文件 | 删除前逻辑 bytes |
| --- | ---: | ---: | ---: |
| 第 7.1 节 `out` 构建根 | 49 | 91,902 | 21,672,871,230 |
| 第 7.2 节旧 preflight 部署 | 4 | 68 | 194,416,700 |
| 第 7.3 节当前 clean Lab 精确子目录 | 20 | 9,411 | 2,076,670,082 |
| 第 7.4 节旧仓库 SDK 根 | 4 | 2,054 | 324,324,395 |
| 第 7.5 节旧外部 Lab 根 | 12 | 43,625 | 10,276,517,569 |
| 第 7.5 节旧外部 SDK 根 | 8 | 6,184 | 937,979,282 |
| clean detached clone | 1 | 3,233 | 377,253,599 |
| 当前外部 SDK consumer | 1 | 1,063 | 139,576,958 |
| **合计** | **99** | **157,540** | **35,999,609,815** |

执行前门禁实际通过：

1. 分支/基线为 `main@0b89beded03b39d8ffe019bfec659b5a5885a8f1`，暂存区为空。
2. 99 个绝对路径全部存在，均严格位于各自 `out`、当前 clean Lab 根或 `F:\PersonalWorkspace` scope 内；没有重复、父子候选或与保护路径重叠。
3. 候选根及其祖先没有意外 reparse point；`out` 的 27 个已知 SymbolicLink 与盘点完全一致。
4. `pae-clean-checkpoint-98df5e0-source` 仍为干净 detached `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`，仅一个 worktree，且该提交是当前 HEAD 的祖先，没有独有提交。
5. 未发现 `pae_protocol_lab_ui`、CMake、CTest、MSBuild 或 Ninja 进程；没有终止用户进程。
6. 两次 dry-run 都通过；第二次精确确认原 337 文件、1,342,277 bytes 核心证据集完整包含在扩展归集集合中，证据目标相对路径无碰撞。

实际命令顺序：

```powershell
& 'scripts\cleanup-generated-artifacts-20260919.ps1'
& 'scripts\cleanup-generated-artifacts-20260919.ps1' -Execute
& 'scripts\cleanup-generated-artifacts-20260919.ps1' -VerifyAfter
```

## 3. 证据归集与校验

证据位于被 Git 忽略的：

`deliverables\evidence\cleanup-20260919`

归集范围覆盖：

- 原盘点的 337 个日志、CTest 状态及手工/诊断证据；
- 12 个旧 Lab 根的 `logs`、`INPUT_*.json` 及其他匹配证据；
- 旧 SDK/包中的 `PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS[.txt]`、README；
- 所有候选中的负例、Evidence、Run Reader、崩溃/dump 命名及报告引用匹配项。

复制保持“候选根编号 + 原相对路径”。每个 payload 都记录来源绝对路径、来源根、相对路径、长度、mtime、来源 SHA-256、目标 SHA-256 和 reparse 标记；复制后逐文件比较长度和 SHA-256，删除前又重算全部来源 SHA-256，**7,912/7,912 一致**。

主要机器证据：

| 文件 | 内容 |
| --- | --- |
| `evidence-manifest.json` | 7,912 个归集文件的来源、目标和双向 Hash |
| `candidate-roots-before.json` | 99 个根的文件数、目录数、逻辑长度和树签名 |
| `reparse-points.json` | 27 个链接对象、保存目标文本和处置 |
| `report-references.json` | 249 条候选根到 Markdown 报告行的引用映射 |
| `protected-before.json` / `protected-after.json` | 保护集合逐文件长度和 SHA-256 |
| `execution-log.json` | 26 个链接和 99 个根的逐项删除结果 |

## 4. Reparse point 处理

没有跟随链接枚举或删除目标。27 个链接中：

- 26 个位于获批删除根内；逐项确认仍为 reparse point 后，先用非递归的 .NET 文件/目录删除 API 移除 link object；
- 每个候选根在递归删除前再次确认已无 reparse point；
- 范围外 `out\symlink-capability-check\link.txt` 未处理，删除后复核仍是 reparse point。

## 5. 删除后保护项复核

以下路径均仍存在，并纳入路径或 Hash 复核：

- `deliverables\sdk\98df5e0`、`deliverables\lab\98df5e0`；
- `out\sdk-clean-checkpoint\candidate1-20260919` 五包；
- `out\sdk-clean-checkpoint-validation`；
- 当前 clean static/shared Lab 的完整 `deploy`、`logs`、`INPUT_PROVENANCE.json`、`INPUT_SHA256.json`；
- 仓库 `src`、`tests`、`third_party`；
- `out\downloads`、`out\sources`、`out\manual-lab`、`out\review`、`out\validation`、两份 `agent-c3-acceptance-*` 和 `out` 根散文件；
- 本机 Qt `D:\develop_env\Qt\Qt5.11.3\5.11.3`。

保护快照共有 6,963 个普通文件；删除前后相对同一绝对路径的文件集合、长度、SHA-256 全部一致。保护快照本身没有 reparse point。

## 6. 执行中的异常与恢复

99 个根全部删除后，脚本第一次进行最终保护集合对比时，在 PowerShell 严格模式下对空的 `Compare-Object` 结果直接访问 `.Count`，触发了只读后置检查错误。该错误发生在所有精确删除完成之后，没有扩大删除范围，也不是路径、权限、长路径或文件删除失败。

已用 `apply_patch` 将比较改为数组化计数，并增加只读 `-VerifyAfter` 恢复入口。该入口不执行删除，只读取既有执行日志和删除前快照，确认：

- 执行日志已记录 99 根、26 链接；
- 99 根全部不存在；
- 保护集合 Hash 一致；
- 范围外链接和本机 Qt 仍存在。

恢复复核通过后，`execution-log.json` 最终状态为 `complete-after-postcheck-retry`。本次没有跳过项，也没有未解决的删除失败。

## 7. 可恢复性与验证边界

- `deliverables\evidence\cleanup-20260919` 中的归集证据可按清单读取和校验。
- 删除是不可恢复删除；证据归集不是 99 个根的完整镜像，不能原样恢复构建树、旧部署或旧 consumer 根。
- 已删除构建树如再需要，必须从保留源码、依赖和配置重新构建；本轮没有进行构建、测试或启动 Lab。
- 当前 SDK 五包、当前 clean Lab 部署及仓库内 `deliverables` 均被保留，不依赖已删除旧根恢复。
- 未 Stage、Commit、Push 或发布。共享树既有文档修改全部保留。

状态：已完成授权清理范围，待总控复核。
