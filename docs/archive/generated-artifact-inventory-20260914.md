# 生成产物与旧工作区精确盘点（2026-09-14）

> 历史状态提示：本文记录整理当时的现场。用户随后已永久删除外层 `Lab验证` 与 `工程整理归档`；下述外层归档路径现已失效，仅保留路径和 Hash 供历史追溯。

本报告先保留第二轮“逐对象授权”盘点快照，再记录第三轮经用户授权后的本地证据归档与限定清理结果。没有退役 worktree、删除分支、修改源代码、构建、测试、Stage、Commit 或 Push。盘点大小均为删除前的文件逻辑大小，不等同于磁盘物理可用空间。

## 实际执行结果

- 删除前 allowlist 见 [generated-artifact-cleanup-execution-20260914.json](generated-artifact-cleanup-execution-20260914.json)：114 个历史 build 候选中，113 个通过实时 preflight；它们均为 `out/build` 的直接子目录、ignored 且无 tracked 文件、无 reparse point、无活动构建进程，声明的 CMake 源目录仍存在。`windows-msvc-protocol-lab` 因含 4 个 reparse point 被排除。
- 当时的本地归档根为 `F:\PersonalWorkspace\协议解析拼接工具\工程整理归档\2026-09-14`。基线 344 项加 `v06-gates` 其余 2 个日志，共复制并复算 SHA-256 346/346 项、5,911,057 逻辑字节，无冲突；当时映射文件为 `archive-copy-result.json`，其 SHA-256 为 `7DE178820FEF94EA30C10C205A937F1B41B23CEA6A90876555C6E6640C0A9102`。该外层路径现已失效。
- 按 allowlist 逐项永久删除 113/113 个历史 build 目录，无失败或部分残留；删除前合计 104166 个文件、14,866,416,694 逻辑字节（13.845 GiB）。当时结果文件为 `cleanup-delete-result.json`，其 SHA-256 为 `81FB04EC88218E1A91D980C370B84F72C267989A174A4EA2A57E708AC79519C4`。该外层路径现已失效。
- 删除是 `Remove-Item -LiteralPath -Recurse -Force` 的永久操作，不在回收站。当时归档证据可从归档根取回；该外层归档后来已由用户永久删除，现不能再据此路径恢复。构建树本身需按源码、CMake preset/Cache 和验证文档重新生成。
- 主仓 `out/build` 现仅保留 `windows-msvc-pae-lab`、`windows-msvc-binary-ui-stage1`、`windows-msvc-protocol-lab`。80 个主仓非 build 直接目录与 `out` 根文件、旧 worktree、外层 `Lab验证`、随仓 Qt、本机 Qt 和所有分支均未删除。
- 第四轮在补充归档并复算旧 worktree 证据后，通过 Git 退役 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine_lab_ui`；详情见 [旧 Lab worktree 退役记录](generated-artifact-worktree-retirement-20260914.md)。当时本地与远端 `feat/lab-ui-c1` 分支仍保留，外层 `Lab验证`、随仓 Qt、本机 Qt 和主仓产物未受该轮影响；外层 `Lab验证` 已在后续由用户永久删除。

## 结论

- 精确清单共有 211 个对象：主仓库 `out/build` 116 个直接子目录、115855 个文件、17.110 GiB；主仓库 `out` 非 build 80 个直接子目录、47167 个文件、5.880 GiB；`out` 根文件集合 1 个对象、598 个文件、0.010 GiB；旧 Lab worktree `out` 12 个直接子目录、4746 个文件、2.270 GiB；外层 `Lab验证` 2 个目录、715 个文件、0.180 GiB。
- 机器可读的逐对象授权基线是 [generated-artifact-cleanup-manifest-20260914.json](generated-artifact-cleanup-manifest-20260914.json)。每个对象均记录绝对路径、大小、文件数、用途判断依据、Git/worktree 关系、重建依据、文档日志引用数、reparse 风险、建议动作和待确认状态；本报告只提供分组视图，不能替代该全量清单。
- 删除前的文档日志引用扫描得到 286 次出现、284 个唯一引用，其中 267 个 literal、17 个 pattern。267 个 literal 在删除前全部可解析：主仓库 build 25 个、主仓库非 build 210 个、旧 worktree 32 个；被删 build 对象中的 literal 日志已按原相对映射归档。
- 先前“261 个唯一引用、229 个存在、32 个缺失”是旧正则与仅按主仓库解析路径造成的中间结果。32 个并非缺失，而是位于旧 worktree；本报告以改进扫描为准，并保留 261 作为方法变更对照。
- 证据索引见 [generated-artifact-evidence-index-20260914.json](generated-artifact-evidence-index-20260914.json)：基线 344 项已全部归档并验证，另完整保存 `v06-gates` 的 2 个补充日志；源路径、归档路径、大小、SHA-256 与实际状态由归档结果清单记录。
- 主仓库中 8 个对象含 27 个 reparse point。任何后续动作都必须先解析目标并将链接与链接目标分别处理，不能直接递归删除这些对象。

## 清单范围与排除项

本次逐对象清单覆盖：

1. `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build` 的全部直接子目录；
2. 主仓库 `out` 除 `build` 外的全部直接子目录，以及 `out` 根文件集合；
3. `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine_lab_ui\out` 的全部直接子目录；
4. `F:\PersonalWorkspace\协议解析拼接工具\Lab验证` 的两个直接子目录。

明确排除私有协议材料、`EIK`、随仓 Qt、本机 Qt、源代码、tracked 工作树内容以及其他仓库外目录。原始对象 JSON 中的 `pendingConfirmation: true` 保留的是第二轮盘点快照；第三轮实际删除权限只来自独立 allowlist，不能从旧推荐标签反推。

## 清理前分批建议（历史快照）

下表保留删除前的决策依据；实际执行范围与结果以本报告开头和执行清单为准。

| 批次 | 对象数 | 约 GiB | 建议 | 授权前必须完成 |
| --- | ---: | ---: | --- | --- |
| 当前保留构建 | 2 | 2.47 | 保留 `windows-msvc-pae-lab`；`windows-msvc-binary-ui-stage1` 至少保留到当前交付复核结束 | 核对当前脏工作树和交付状态 |
| build 内先归档日志的候选 | 4 | 1.19 | 归档引用日志后再决定删除 | 校验 23 个引用日志的 SHA-256 与拟归档相对路径 |
| 其余历史 build 候选 | 110 | 13.46 | 逐对象 preflight 后可作为删除候选 | 重算大小/最近写入、确认无新增引用、排除 reparse 风险 |
| 主仓库非 build 与根文件 | 81 | 5.90 | 全部保留，等待 Evidence Bundle/复现材料分诊 | 不能对 `out` 根目录做通配符清理 |
| 旧 worktree `final-ui-v142` | 1 | 0.57 | 先归档 9 个直接日志，再决定删除 | 核对是否存在未被文档引用的唯一人工证据 |
| 旧 worktree 其余目录 | 11 | 1.70 | 唯一证据审查后再决定删除 | 重新确认 ignored 内容与 tracked clean 状态 |
| 外层 `Lab验证` | 2 | 0.18 | 保留报告、日志、构建说明和 tar 后选择性清理 | 先做内容/保密审查，再按 Hash 验证复制结果 |

### build 内需先归档引用日志的四个对象

- `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\v06-a`：7 个 literal 日志引用。
- `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\v06-gates`：6 个 literal 日志引用。
- `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-all-slices-dec039`：4 个 literal 日志引用。
- `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-protocol-lab`：6 个 literal 日志引用，且包含 4 个 reparse point。

其余 2 个 build literal 引用位于当前保留的 `windows-msvc-binary-ui-stage1`，因此 25 个 build literal 引用与清单统计一致。

## 证据索引与 Hash 策略

规划的归档根目录曾在第三轮实际创建，后续已由用户永久删除：

`F:\PersonalWorkspace\协议解析拼接工具\工程整理归档\2026-09-14`

证据类别如下：

| 类别 | 文件数 | 约 MiB | 说明 |
| --- | ---: | ---: | --- |
| `referenced_log` | 267 | 1.74 | 仓库 Markdown 明确引用且现场存在的 literal 日志 |
| `old_worktree_final_log` | 9 | 0.01 | 旧 worktree `final-ui-v142/logs` 的直接文件 |
| `lab_validation_log` | 63 | 0.14 | 两个外层 Lab 对象的直接日志 |
| `lab_validation_record` | 4 | 0.03 | `REPORT.md` 与 `CMakeLists.txt` |
| `lab_validation_snapshot` | 1 | 3.71 | 历史源码 tar |

本轮已保持类别下的相对路径，复制前核对源大小与 SHA-256，复制后在目标端逐项复算；346 项均通过。Hash 只能证明字节一致，不能证明内容语义正确、无敏感信息，也不能替代构建、运行、硬件、Golden、人工 UI 或现场验收。

静态提取只扫描仓库内、排除 `out` 与 `third_party` 的 Markdown，并排除本盘点文件避免自引用；它不是完整的 Evidence Bundle 语义扫描。17 个 pattern 未做全面展开；preflight 确认没有 pattern 与本轮 113 个删除对象相交，因此不阻塞本轮限定清理，也不能据此宣称 pattern 证据已全面归档。

## Reparse 风险

以下 8 个对象共含 27 个 reparse point，当前一律排除于递归清理动作之外：

- `out\dec042b-c2-run-debug`：4；
- `out\dec042b-c2-run-release`：4；
- `out\dec042b-c3-debug`：4；
- `out\build\windows-msvc-protocol-lab`：4；
- `out\b42bd`：3；
- `out\b42br`：3；
- `out\dec042b-c3-release-msvc`：4；
- `out\symlink-capability-check`：1。

实际清理前应以只读方式解析每个链接的最终绝对目标，确认目标仍在授权范围内；不得把未解析的链接交给递归删除命令。

## 旧 worktree 与分支边界

第二轮盘点时，旧 Lab worktree 的 tracked 工作树干净，只有 ignored `out/`；`feat/lab-ui-c1@8bfe50f` 是 `main` 的祖先。第四轮重新核对同一状态后，先复核已有 41 项归档证据并补充保存 18 项，再使用 `git worktree remove --force` 退役确切路径。退役共永久移除 4747 个 ignored 生成文件、2,443,134,288 逻辑字节；没有手工删除目录或移动 `.git`。

本地 `feat/lab-ui-c1` 与远端 `origin/feat/lab-ui-c1` 均未删除，仍可在新的明确路径重新建立源码 worktree。构建树不在回收站且需要重新生成；退役记录仍保留当时映射，但外层归档已删除，不能再据该路径直接恢复归档文件。

## 清理前建议顺序（历史快照）

1. 总控先复核当前脏工作树与本次三份清单，确认保留对象和归档根目录。
2. 对 344 个拟保留文件进行保密/语义审查；17 个 pattern 与未覆盖的 Evidence Bundle 另行分诊。
3. 获得归档授权后复制证据并逐项复算 Hash；保留实际归档清单和失败项，不以“计划路径”冒充已归档。
4. 先处理 110 个可重建历史 build 候选，再处理 4 个已完成日志归档的 build 对象；含 reparse point 的对象保持排除，直至目标解析完成。
5. 主仓库非 build、旧 worktree、外层 `Lab验证` 分别授权和执行；不得用一个根目录级通配符跨批次清理。
6. 旧 worktree 生成物清理完成后，再单独决定 worktree 与分支是否退役。

## 本轮验证边界

第三轮当时验证包括：allowlist 113 个目标全部消失；三个排除 build 目录、80 个主仓非 build 目录、外层 `Lab验证`、随仓 Qt 和本机 Qt 仍存在；346 个归档文件目标端大小与 SHA-256 再核对无误。第四轮进一步确认 Git worktree 列表只移除旧 Lab 目标，本地/远端分支仍在，59 项旧 worktree 归档证据退役后复算无误；推荐 `windows-msvc-pae-lab` preset、构建目录和 README 启动命令仍存在。外层 `Lab验证` 与归档根在后续由用户永久删除。Git staged 当时仍为空，既有源文件差异未被整理动作改变。没有重新构建、启动 Lab 或运行测试，也没有执行人工 UI、硬件、Golden、现场或生产验收。
