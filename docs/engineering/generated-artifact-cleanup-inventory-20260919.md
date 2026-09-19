# PAE 生成产物清理盘点（2026-09-19）

## 1. 结论与边界

2026-09-19 在 `main@0b89beded03b39d8ffe019bfec659b5a5885a8f1` 上只读盘点了：

- `F:\PersonalWorkspace` 直属、名称以 `pae-` 开头的 24 个生成根；
- 仓库 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out`。

未遍历 `F:\PersonalWorkspace` 中其他项目。本轮未删除、移动、复制或压缩任何产物，未构建或启动 Lab。

范围内总逻辑体积约 **33.895 GiB**：仓库 `out` 约 20.772 GiB，外部 `pae-*` 根约 13.123 GiB。下表的 **22.780 GiB** 和 **33.522 GiB** 均是“归集最小原始证据之前”的估计上限，不是无条件可删除量；49 个构建根和 8 个旧外部 SDK 根中的日志、CTest 状态及手工证据必须先按第 5.3 节归集：

| 阶段 | 不重叠范围 | 预计释放 |
| --- | --- | ---: |
| A：归集前候选 | `out` 构建根、四份旧 preflight 部署、clean clone、当前外部 SDK consumer 根、当前 Lab 的 build/inputs/负例根 | **上限 22.780 GiB** |
| B：先归集后可删 | 旧 Lab 根除 `logs`/`INPUT_*.json` 外的内容、旧外部 SDK consumer 根、仓库内旧 SDK 包根 | 10.741 GiB |
| 合计 | A + B，路径互斥 | **归集前上限 33.522 GiB** |

上述是后续获得删除授权并再次核对后的上限，不是本轮已释放空间。数字使用逻辑文件长度，`1 GiB = 2^30 bytes`，不等于文件系统实际占用簇。

## 2. 计量与安全核对

- 用 `robocopy /L /E /BYTES /XJ` 做只列表计量，不写入目标，不跟随 junction；一次独立检查确认虚拟目标目录未被创建。
- 外部 `pae-*` 根未发现重解析点。仓库 `out` 有 27 个重解析点，全部为 SymbolicLink：12 个目录链接、15 个文件链接。分布为 7 组 v06 Evidence 负例（每组 1 个目录链接 + 2 个文件链接，共 21 个）、5 个 v07 Run Reader 目录链接，以及 `symlink-capability-check\link.txt` 1 个文件链接。只按链接保存的目标文本做语法归一化后，27 个目标都位于本仓库 `out` 内，未发现指向范围外的目标；这不代表目标当前存在，也不是跟随目标检查的结果。
- 其中 21 个 v06 链接的保存目标为相对路径 `v06-evidence-runs\controlled_external_*`，归一化后仍在各自生成证据根下；5 个 v07 目录链接保存绝对路径，分别指向同一构建根内的 `run_reader_baseline`；最后 1 个能力检查链接保存绝对路径，指向同目录 `target.txt`。
- 后续清理不得让递归命令穿越上述链接。必须先按精确路径删除链接对象本身，再处理其父构建根；不得对链接目标执行递归删除。当前计量使用 `/XJ`，且证据扫描排除了链接对象。
- 未发现运行中的 `pae_protocol_lab_ui`、CMake、CTest、MSBuild 或 Ninja 进程。首次统计遗留的一个本轮只读 PowerShell helper 经精确命令行核对后已终止，未写盘。
- 主仓库只登记一个 worktree：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`。
- 外部根只有 `pae-clean-checkpoint-98df5e0-source\.git`。它是独立 detached clone，HEAD 为 `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`，`git status --short` 为空，未发现独有修改。
- `98df5e0..0b89bed` 只有 10 个 Markdown 入口/计划/验证报告变化，无功能源码变化；因此删除干净 clone 不会丢失未合并代码，但仍需独立删除授权。
- 仓库 `out` 由根 `.gitignore:1:/out/` 忽略；其中内容不是 Git 跟踪变更。非 Git 生成根仍可能含独有原始日志，因此本报告把这些日志单独归为“需先归集”，不只凭目录名判定可删。

## 3. 根级盘点

处置标记：`K` 保留，`D` 可再生且可纳入后续删除候选，`P` 先归集最小证据再决定是否删除，`K/D/P` 表示同一根中分子目录处理。文件数/目录数不跟随重解析点。

| 根 | GiB | 文件 | 目录 | 重解析点 | 用途 / 处置 |
| --- | ---: | ---: | ---: | ---: | --- |
| `...\protocol_adapter_engine\out` | 20.772 | 99,454 | 47,695 | 27 | 仓库内构建、SDK、部署和证据混合根；`K/D/P` 分区，禁止整根删除；49 个构建根先按 `P` 处理 |
| `pae-lab-clean-sdk-static-20260919` | 1.219 | 4,746 | 1,003 | 0 | 当前 static Lab；保留 deploy/logs/根清单，其余 `D` |
| `pae-lab-sdk-static-20260919-r6` | 1.218 | 4,774 | 945 | 0 | 旧 static 中间根；`P` |
| `pae-lab-sdk-static-20260919-r7` | 1.218 | 4,770 | 937 | 0 | 旧 static 产品闭包证据根；`P` |
| `pae-lab-sdk-static-20260919-r5` | 1.141 | 4,782 | 1,073 | 0 | 旧 static 中间根；`P` |
| `pae-lab-sdk-shared-20260919-r2` | 1.068 | 5,131 | 1,183 | 0 | 旧 shared 最终证据根；`P` |
| `pae-lab-sdk-static-20260919-r2` | 1.004 | 4,103 | 752 | 0 | 旧 static 中间根；`P` |
| `pae-lab-clean-sdk-shared-20260919` | 0.978 | 4,855 | 1,062 | 0 | 当前 shared Lab；保留 deploy/logs/根清单，其余 `D` |
| `pae-lab-sdk-static-20260919-r8` | 0.943 | 3,839 | 630 | 0 | 旧 static 最终 Testing-on 根；`P` |
| `pae-lab-sdk-static-20260919-r3` | 0.943 | 3,834 | 630 | 0 | 旧 static 中间根；`P` |
| `pae-lab-sdk-static-20260919-r4` | 0.917 | 3,831 | 628 | 0 | 旧 static 中间根；`P` |
| `pae-lab-sdk-static-20260919` | 0.708 | 3,109 | 327 | 0 | 旧 static 初始根；`P` |
| `pae-clean-checkpoint-98df5e0-source` | 0.351 | 3,233 | 247 | 0 | 干净 detached clone；无独有修改，`D` |
| `pae-lab-sdk-static-regression-20260919` | 0.278 | 2,761 | 184 | 0 | 旧 shared 改动后 static 回归根；`P` |
| `pae-sdk-clean-checkpoint-validation-candidate1-20260919` | 0.130 | 1,063 | 579 | 0 | 当前 SDK 外部 consumer/包副本；仓库候选及证据保留后 `D` |
| `pae-sdk-stage4-ascii-validation-candidate1-20260918` | 0.124 | 962 | 615 | 0 | 旧 ASCII SDK 消费根；`P` |
| `pae-sdk-stage3-validation-final5-20260915` | 0.122 | 899 | 468 | 0 | 旧 Stage 3 SDK 消费根；`P` |
| `pae-sdk-stage3-validation-final4-20260915` | 0.120 | 845 | 443 | 0 | 旧 Stage 3 中间根；`P` |
| `pae-sdk-stage3-validation-final3-20260915` | 0.120 | 828 | 446 | 0 | 旧 Stage 3 中间根；`P` |
| `pae-sdk-stage3-validation-recovery-20260915` | 0.120 | 817 | 430 | 0 | 旧 Stage 3 recovery 根；`P` |
| `pae-sdk-stage3-validation-final2-20260915` | 0.120 | 822 | 439 | 0 | 旧 Stage 3 中间根；`P` |
| `pae-sdk-stage3-validation-final-20260915` | 0.120 | 822 | 439 | 0 | 旧 Stage 3 中间根；`P` |
| `pae-lab-sdk-shared-20260919` | 0.109 | 2,566 | 115 | 0 | 旧 shared 失败/初始根；`P` |
| `pae-sdk-public-stream-validation-candidate1-20260918` | 0.028 | 189 | 89 | 0 | 旧 public-stream SDK 消费根；`P` |
| `pae-lab-ascii-a1-static-consumer-20260918` | 0.024 | 125 | 73 | 0 | 旧 A1 static consumer 根；无根清单/logs，报告复核后 `D` |

上表中外部根的绝对基准均为 `F:\PersonalWorkspace\<根名>`。

## 4. 关键二级目录

### 4.1 仓库 `out`

| 子范围 | GiB | 文件 | 结论 |
| --- | ---: | ---: | --- |
| `out\build` | 14.130 | 47,517 | CMake/VS 构建树，但夹有日志、CTest 状态和手工复现证据；先归集再删 |
| `out` 下其他含 `CMakeCache.txt` 的一级根 | 6.054 | 见第 7.1 节 | 构建/门禁根，但夹有日志和 CTest 状态；先归集再删 |
| 四份 `lab-sdk-standalone-preflight-deploy*` | 0.181 | 68 | 重复的旧 preflight Qt 部署，可删 |
| 四组旧 SDK 包根 | 0.302 | 2,054 | 保留报告/身份摘要后可删 |
| `out\sdk-clean-checkpoint` | 0.028 | 189 | **保留**：当前 `candidate1-20260919` 五包 |
| `out\sdk-clean-checkpoint-validation` | <0.001 | 63 | **保留**：当前 SDK 原始验证摘要 |
| `out` 根文件 | 0.015 | 605 | 用途混合，不纳入批量删除 |

`out` 中检测出的构建型一级根共 20.184 GiB，与 preflight 部署、SDK 包根互不重叠。这里的“构建型”只描述根的主要用途，不证明其中每个文件都可由当前源码重建。

### 4.2 当前 clean-checkpoint Lab

| 根 / 二级分类 | GiB | 处置 |
| --- | ---: | --- |
| static 全根 | 1.219 | 分区处理 |
| static `deploy` | 0.130 | **保留** D/R Testing-on/off 已验证部署 |
| static `logs` + 根 `INPUT_*.json` | <0.001 | **保留**最小原始证据 |
| static 其余 build/inputs/负例 | 1.088 | 可再生，可删 |
| shared 全根 | 0.978 | 分区处理 |
| shared `deploy` | 0.132 | **保留** D/R Testing-on/off 已验证部署 |
| shared `logs` + 根 `INPUT_*.json` | <0.001 | **保留**最小原始证据 |
| shared 其余 build/inputs/负例 | 0.846 | 可再生，可删 |

普通本地使用的最小保留路径是：

- static Release：`F:\PersonalWorkspace\pae-lab-clean-sdk-static-20260919\deploy\release-testing-off\Release`，20.35 MiB / 17 文件；
- shared Release：`F:\PersonalWorkspace\pae-lab-clean-sdk-shared-20260919\deploy\release-testing-off\Release`，20.49 MiB / 18 文件，其中包含 `pae.dll`。

本报告保守建议保留两根的完整 `deploy`，不只保留 Release，以免丢失已验证的 Debug 和 Testing-on/off 对照身份。

### 4.3 当前 SDK 五包和外部 consumer

| 包 | MiB | 文件 | 处置 |
| --- | ---: | ---: | --- |
| `pae-sdk-source` | 1.66 | 81 | **保留** |
| `pae-sdk-static-debug` | 16.58 | 29 | **保留** |
| `pae-sdk-static-release` | 7.47 | 29 | **保留** |
| `pae-sdk-shared-debug` | 2.09 | 25 | **保留** |
| `pae-sdk-shared-release` | 0.75 | 25 | **保留** |

五包均位于 `out\sdk-clean-checkpoint\candidate1-20260919`。外部
`pae-sdk-clean-checkpoint-validation-candidate1-20260919` 中的 `build` 为 0.102 GiB，其余是五包外部副本和负例输入；在仓库候选、跟踪报告和 `out\sdk-clean-checkpoint-validation` 保留时，该 0.130 GiB 外部根可再生。

### 4.4 重复 Qt 输入

13 个含 `INPUT_SHA256.json` 的 Lab 根都记录同一组 Qt 清单：2,358 文件、110,957,060 bytes（105.82 MiB），对按 `path|bytes|sha256` 排序后的清单内容计算得到同一指纹
`BD19B3A1ECF24A987761C7006B8E40A2937C873F5C51184DE408811E55EA6304`。这是清单级一致性，本轮未重新哈希 1.343 GiB 的所有 Qt 副本。

当前 static/shared 根各保留了一份 Qt inputs，但实际启动仅依赖 `deploy` 中的 Qt DLL/plugin。因此：

- 当前两份 `inputs\qt` 可随整个 `inputs` 在清单保留后删除；
- 旧 11 份 `inputs\qt` 则在其根 `INPUT_SHA256.json` 先归集后删除；
- 不得删除当前 `deploy` 中的 Qt DLL 和 `platforms\qwindows*.dll`。

### 4.5 仓库本机 `deliverables`（排除清理）

当前 SDK 和 Lab Release 产品已经另行归集到仓库内被 Git 忽略的 `deliverables`，该目录从未计入本报告的 33.895 GiB，也**永不进入本报告任何删除候选或白名单**：

| 路径 | 文件 | bytes | 已核对 |
| --- | ---: | ---: | --- |
| `deliverables\sdk\98df5e0` | 189 | 29,946,570 | 总控复核五包 source/target 文件集合及逐文件 SHA-256，0 mismatch |
| `deliverables\lab\98df5e0\static-release` | 17 | 21,337,615 | 总控复核来源/目标集合及逐文件 SHA-256，0 mismatch |
| `deliverables\lab\98df5e0\shared-release` | 18 | 21,490,191 | 总控复核来源/目标集合及逐文件 SHA-256，0 mismatch |

对应证据为 [SDK 本地归集验证](local-deliverables-sdk-validation-20260919.md) 和 [Lab 本地归集验证](local-deliverables-lab-validation-20260919.md)。这些副本降低了误删产品的风险，但不产生删除授权：本报告仍保留原始 `out\sdk-clean-checkpoint\candidate1-20260919` 五包，以及当前两个 clean Lab 根的完整 `deploy`。

## 5. 处置分类

### 5.1 必须保留

1. `out\sdk-clean-checkpoint\candidate1-20260919` 五包。
2. `deliverables\sdk\98df5e0`、`deliverables\lab\98df5e0` 全部内容；它们不属于本盘点范围，也不因存在副本而替代原始保留项。
3. `out\sdk-clean-checkpoint-validation` 及跟踪文档
   [clean-checkpoint SDK 验证](pae-sdk-clean-checkpoint-validation.md)、
   [Lab 同批 SDK 消费验证](lab-clean-sdk-consumption-validation.md)。
4. 当前 clean static/shared 两根的完整 `deploy`、`logs`、`INPUT_PROVENANCE.json`、`INPUT_SHA256.json`。
5. 仓库内跟踪的契约、验证报告和使用入口。
6. `out` 中名称为 `*-validation`、`review`、`manual-*`、`agent-*-acceptance-*` 的小型原始证据，在建立单独证据保留策略前不删。

### 5.2 可再生候选（仍需另行删除授权）

1. 第 7.2 节四份旧 preflight 部署：0.181 GiB。
2. `F:\PersonalWorkspace\pae-clean-checkpoint-98df5e0-source`：0.351 GiB；它是 clean detached clone，不是主仓 worktree。
3. `F:\PersonalWorkspace\pae-sdk-clean-checkpoint-validation-candidate1-20260919`：0.130 GiB；仓库候选与证据保留后可重建。
4. 第 7.3 节当前 static/shared 根内精确子目录：1.934 GiB；不包含 `deploy`、`logs` 或根清单。

### 5.3 需先归集最小证据

1. 第 7.1 节 49 个构建根和 8 个旧外部 SDK consumer 根先统一执行保守证据归集。排除 reparse 对象后，按下述规则扫描到 **337 个文件、1,342,277 bytes（约 1.28 MiB）**：其中 49 个构建根 310 个文件、1,338,443 bytes，8 个旧 SDK 根 27 个文件、3,834 bytes。
   - 全部 `*.log`：241 个、1,200,560 bytes；
   - `Testing` 目录下其他 CTest 状态（包括 `TAG`、`CTestCostData.txt` 等）：65 个、61,196 bytes；
   - 路径段匹配 `manual*`、`acceptance*`、`crash*`、`diagnos*`、`review*` 且扩展名为 `.txt/.json/.xml/.md/.csv/.trace/.etl/.bin/.dat`：31 个、80,521 bytes；
   - 未发现扩展名为 `.dmp/.mdmp/.dump/.core` 或名称含 `dump` 的文件。
2. 上述 337 个文件应按原相对路径整体保留，不按相同 Hash 去重；SHA-256 只有 280 种、首份内容合计 1,285,069 bytes，但路径、时间和多次运行归属本身也是证据。归集时另生成含来源根、相对路径、长度、mtime、SHA-256 和 reparse 标记的清单。
3. 12 个旧 Lab 根：全部共 9.571 GiB。先保留各根 `logs` 和 `INPUT_*.json`，共约 5.73 MiB；其余预计可释放 9.565 GiB。
4. 第 7.4 节的四组仓库内旧 SDK 包根：0.302 GiB。先保留包身份、manifest/hash 摘要和对应历史报告。

“归集”是下一个独立写入/复制任务，本轮没有执行。

### 5.4 不明或待用户确认

- `out\downloads`（0.011 GiB）与 `out\sources`（0.024 GiB）：可能是早期依赖下载/源码缓存，删除前需确认是否要保留离线复现能力。
- `out\manual-lab`、`out\agent-c3-acceptance-*`、`out\review`、`out\validation` 及其他小型证据目录：体积不大，在“历史原始日志是否必须长期保留”拍板前默认保留。
- `out` 根下 605 个直接文件（0.015 GiB）：来源混合，不应用根级通配符删除。

## 6. 风险与最小证据建议

1. **不删整个 `out`**：当前五包和原始证据与构建树共存。
2. **不使用 `pae-*` 通配符整批删除**：当前 clean Lab 部署也匹配该前缀。
3. **归集旧证据时保留身份链**：至少保留根名、`INPUT_PROVENANCE.json`、`INPUT_SHA256.json`、第 5.3 节的路径保留证据集、对应跟踪报告路径和一份归集清单。
4. **部署保留必须以目录为单位**：shared Release 不能只保留 EXE，必须同时保留 `pae.dll`、Qt DLL、`platforms\` 和 `configs\`。
5. **删除执行前重查进程和路径**：本轮的“无占用”只是 2026-09-19 盘点时快照，不能代替实际删除前检查。
6. **每批都先归集、再复核、后删除**：阶段 A 中的 49 个构建根也必须先完成第 5.3 节归集；每批删除后重算大小与保留路径，不把本报告的预计值写成已释放值。
7. **链接对象单独先处理**：27 个链接目标虽都语法归一化到 `out` 内，删除程序仍须关闭跟随链接行为；逐个核对并先删除 link object，绝不递归链接目标，再处理父根。
8. **扩展名策略仍有盲区**：可能遗漏已改名或无扩展名的手工输出、未知 dump 扩展、Alternate Data Streams、保存在已失效链接目标中的内容，以及未落盘的进程状态。真正删除前还要按根复核目录名和最近修改项，并搜索跟踪报告对候选路径的引用；发现无法分类的文件时默认保留，不扩大删除范围。

## 7. 后续删除候选（本轮未执行）

下列都是精确目录名，不授权删除。后续执行必须使用解析后的绝对路径，逐项核对仍在预期根下，不得使用广泛通配符。

### 7.1 `out` 构建型一级根（归集前候选上限 20.184 GiB）

基准：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\`

```text
build
b
v09-full-debug
integrated-ui-v09
dec042b-c3-debug
yv
dec042b-c2-run-debug
dec042b-lab-c1-debug
recovery-v09-lab-tests
recovery-v09-tests
recovery-v09-lab
dec042b-c3-testing-off-debug
be-debug
dec042b-c3-release-msvc
v09-full-release
dec042b-c2-lab-on-testing-off-debug
dec042b-c3-legacy-lab-default-debug
dec042b-c2-run-release
b42bd
v09-lab-testing-off
dec042b-c3-testing-off-release-msvc
dec042b-lab-c1-release
recovery-v09-compiler
b042b
dec042b-c3-product-only-debug-msvc
dec042b-c2-product-only-debug
dec042b-c1-lab-on-testing-off
dec042b-c2-lab-on-testing-off-release
be-release
v09-product-only
be-testing-off-release
dec042b-c3-product-only-release-msvc
b42br
dec042b-c2-product-only-release
host-endpoint-gates
dec042b-c1-product-only
b42product
dec042b-c3-release
dec042b-c2-gate-missing-prereqs
dec042b-c1-gate-missing-slices
dec042b-c1-gate-testing-off
b42gate-testoff
be-default-off-release
dec042b-c2-gate-testing-off
dec042b-c3-gate-missing-dependencies
dec042b-c3-gate-missing-explicit
dec042b-c2-gate-ordinary-lab-v05
dec042b-c1-gate-ordinary-lab-schema05
b42gate-lab
```

### 7.2 旧 preflight 部署（0.181 GiB）

```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\lab-sdk-standalone-preflight-deploy
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\lab-sdk-standalone-preflight-deploy-2
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\lab-sdk-standalone-preflight-deploy-3
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\lab-sdk-standalone-preflight-deploy-4
```

### 7.3 当前 Lab 根内可再生子目录（1.934 GiB）

static 基准：`F:\PersonalWorkspace\pae-lab-clean-sdk-static-20260919\`

```text
build
build-debug-testing-off
build-debug-testing-on
build-release-testing-off
build-release-testing-on
inputs
negative-static-debug-sdk-release-build
negative-static-kind-mismatch
negative-static-release-sdk-debug-build
```

shared 基准：`F:\PersonalWorkspace\pae-lab-clean-sdk-shared-20260919\`

```text
build-debug-testing-off
build-debug-testing-on
build-release-testing-off
build-release-testing-on
derived-negative-inputs
inputs
negative-shared-debug-sdk-release-build
negative-shared-kind-mismatch
negative-shared-missing-runtime-dll
negative-shared-release-sdk-debug-build
negative-shared-wrong-runtime-dll
```

### 7.4 旧仓库 SDK 包根（0.302 GiB）

```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-public-stream-description
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-stage3
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-stage4-ascii
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-stage4-p
```

### 7.5 旧外部 Lab/SDK 根

第 3 节中标为 `P` 的 12 个 `pae-lab-*` 根和 8 个旧 `pae-sdk-*` 根是精确候选；必须完成第 5.3 节的归集、清单复核和另行删除授权后才可进入删除执行。`pae-lab-clean-sdk-*`、`pae-sdk-clean-checkpoint-*` 以及全部 `deliverables` 不属于该批整根删除对象。

## 8. 盘点时 Git 与停止状态

本轮仓库内只新增本报告。接管时已有的 `docs/README.md`、综合计划、`docs/guides/README.md`、上手指南和维护者阅读指南变更全部保留，未被本任务编辑。
最终核对时又出现其他并行任务维护的 `.gitignore`、`deliverables/`、
`local-deliverables-{sdk,lab}-validation-20260919.md` 和
`repository-organization-audit-20260919.md`；它们不属于本报告的仓库 `out`/外部直属 `pae-*` 统计范围，未计入 33.895 GiB，也未进入任何删除白名单。

分支仍为 `main`，HEAD 仍为 `0b89beded03b39d8ffe019bfec659b5a5885a8f1`，暂存区为空。盘点轮未 Stage、Commit、Push、正式发布或删除。

## 9. 后续执行结果

用户后续已授权按本报告第 5、7 节固定候选执行“先归集校验，再删除”。实际执行结果见 [生成产物清理执行记录](generated-artifact-cleanup-execution-20260919.md)：

- 证据 payload 已归集到 `deliverables\evidence\cleanup-20260919`，共 7,912 个文件、26,654,536 bytes，复制和删除前复算 Hash 100% 一致；
- 已删除 99 个精确目录根和其中 26 个链接对象，无跳过、无目录删除失败；
- 删除前逻辑长度合计 35,999,609,815 bytes（33.527 GiB），不以磁盘自由空间变化代替；
- 6,963 个保护文件删除前后路径、长度和 SHA-256 一致，范围外链接仍保留；
- `deliverables`、当前 SDK 五包、当前 clean Lab 完整 `deploy`/`logs`/根清单、源码/测试/`third_party` 和本机 Qt 均未删除。

执行未构建、测试或启动 Lab，未 Stage、Commit、Push 或发布。删除不可恢复；归集证据可校验，但已删除构建树如需使用必须重新构建。
