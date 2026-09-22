# PAE 历史文档迁移与构建根清理准备收口（2026-09-21）

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

最新状态（2026-09-22）：用户已手动删除七根，总控核对不存在及归集证据Hash通过，见第6节。第1至5节保留准备和受阻时点事实。

## 1. 结论与边界

本轮在 `main@e2527cad500f928100b588fe433e891d460bbc6a` 加共享工作树既有变更上，完成两项限定工作：

1. 将 `organization-inventory-20260921.md` 第 2.3 节精确列出的 30 份历史工程文档迁入 `docs/archive/engineering-20260921/`，保持原文件名和历史正文，只增加归档说明并校正链接；
2. 为七个精确旧 `out/build` 根归集最小配置、CTest 状态、失败与最终验证证据，形成删除准备，但没有删除任何构建根。

本轮未构建、测试、启动 UI、修改产品/包、Stage、Commit、Push、发布或删除。总控维护的 `onboarding-organization-20260921.md` 与 `pae-execution-delivery-organization-plan.md` 保持未改。

## 2. 三十份历史文档迁移

### 2.1 实际结果

- 迁移前精确确认目标目录不存在、30 个源文件存在，且非 Markdown 构建/脚本/源码没有直接依赖这些文件。
- 迁移后旧位置 0 份、新位置 30 份；没有覆盖同名目标。
- 新增[能力链归档索引](../archive/engineering-20260921/README.md)，按公开 API/Lab、Binary G2/UI、SDK/Qt、命名/编辑诊断、仓库/生成物五组组织。
- [文档迁移映射](document-migration-map.md)逐文件列出 30 条原路径与新路径；[归档总入口](../archive/README.md)和[工程依据入口](README.md)已接入。
- 仓库中有效的 Markdown 链接已机械改到新路径；承担当前定位作用的三个纯文本路径也已修正：`third_party/qt-package.md`、`protocol-metadata-naming-validation-20260920.md`、`lab-editor-commit-repair-20260920.md`。
- 历史正文中记录“当时新增文件”“当时唯一写入路径”或旧 Git 状态的纯文本路径继续原样保留，避免把历史事实改写成今天的状态。

### 2.2 正文与链接验证

逐份从 `HEAD:docs/engineering/<文件名>` 取原文，与归档文件移除新增说明后的内容比较；仅将 Markdown 链接目标归一化为占位符：

```text
Files=30
OldPresent=0
NewPresent=30
NonLinkContentMismatches=0
```

全仓排除 `out/` 后检查 234 份 Markdown，剩余 7 个断链均未由普通迁移文件产生：

- 6 个位于本轮明确禁止修改的 `pae-execution-delivery-organization-plan.md`，目标分别为：
  - `protocol-metadata-naming-plan-20260920.md`
  - `lab-ui-usability-localization-plan.md`
  - `lab-public-consumption-residual-audit.md`
  - `lab-sdk-external-build-preflight.md`
  - `pae-sdk-stage3-content-review.md`
  - `lab-public-framer-host-consumer-review.md`
- 另 1 个是 `pae-public-host-stage2b-validation.md` 中既有的 `contract|ascii_stream_contract` 非文件目标，与本轮迁移无关。

因此，除待总控机械修正的 6 个受限链接和 1 个既有无关目标外，本轮受影响链接为 0 断链。

## 3. 七个旧构建根的删除准备

### 3.1 删除前现场

七根均解析在仓库 `out/build` 下，根和子项未发现 reparse point；检查时未发现 CMake、CTest、MSBuild、Ninja、Visual Studio 或 Protocol Lab 相关进程。原始统计仍为 10,096 个文件、4,579,982,839 bytes（4.265 GiB）。

### 3.2 归集结果

新证据根为 [`deliverables/evidence/cleanup-preparation-20260921`](../../deliverables/evidence/cleanup-preparation-20260921/README.md)：

| 类别 | 文件 | bytes | 内容 |
| --- | ---: | ---: | --- |
| 构建配置与 CTest 状态 | 39 | 4,117,741 | 多配置 `CMakeCache.txt`、`CMakeConfigureLog.yaml`、已有 `LastTest*.log` |
| validation 证据 | 109 | 441,062 | 七个报告引用根内的日志、文本和诊断最小复现源码 |
| 合计 | 148 | 4,558,803 | 不含二进制、对象文件或完整构建树 |

[`evidence-manifest.tsv`](../../deliverables/evidence/cleanup-preparation-20260921/evidence-manifest.tsv)逐文件记录角色、证据组、绝对源/目标路径、bytes 和 SHA-256。复制后逐一重算目标 Hash，148/148 一致。

归集特意同时保留：

- G2-B 父目录默认工具集失败缓存与 `v142/` 成功配置；
- G2-C common、binary-only、A2 boundary 与 negative gate 多配置；
- 编辑提交修复前失败/停点与 `annotations-fix` 最终 D/R 通过；
- protocol-metadata 定向 31/31 通过与完整 Qt smoke 失败；
- Qt platform key 初始失败、修正后最小 PASS、window 组 2 FAIL 与 CRT fail-fast probe。

`LastTest*.log` 只代表构建根最后一次 CTest 状态，不能取代报告引用的失败/最终 validation 日志。

### 3.3 保护集合

以下对象未读取私有输入、未复制、未修改、未移动或删除：

- `<TRIAL_ROOT>`
- `deliverables/sdk/`、`deliverables/lab/` 和本机 Qt
- `out/build/windows-msvc-lab-compile-diagnostics-20260921`
- `out/onboarding-20260921`
- `out/sdk-onboarding-20260921`
- 所有不在第 3.4 节精确清单中的构建根

### 3.4 待用户确认的精确删除候选

以下七条绝对路径仍然存在，本轮没有删除授权：

1. `<REPO_ROOT>\out\build\windows-msvc-lab-editor-commit-diagnosis-20260920`
2. `<REPO_ROOT>\out\build\windows-msvc-lab-editor-commit-repair-20260920`
3. `<REPO_ROOT>\out\build\windows-msvc-lab-g2-a`
4. `<REPO_ROOT>\out\build\windows-msvc-lab-g2-b`
5. `<REPO_ROOT>\out\build\windows-msvc-lab-g2-c`
6. `<REPO_ROOT>\out\build\windows-msvc-protocol-metadata-20260920`
7. `<REPO_ROOT>\out\build\windows-msvc-qt-smoke-repair-20260920`

真正删除前须重新检查进程、绝对路径范围、reparse point、文件数/bytes、保护集合和 manifest Hash；必须逐根使用精确路径，不得使用 `out/build/windows-msvc-*` 通配符。

## 4. 验证与未验证范围

已验证：30 文件新旧映射、非链接正文保留、归档与仓库链接、148 个证据源/目标 Hash、七根范围/reparse/进程现场及 Git diff 格式。

未验证：没有重新构建、运行 CTest、启动 UI、执行包外 consumer、人工验收或重新证明历史日志结论；证据归集只证明文件复制一致，不把历史失败或通过升级为今天重新执行的结果。

状态：已完成派发范围，待总控复核；停止写入。

## 5. 精确删除执行尝试（2026-09-22）

用户已明确授权删除第 3.4 节七条绝对路径。执行前重新完成全部门禁：

- 七根仍为 10,096 个文件、4,579,982,839 bytes，路径均在仓库 `out/build` 内；
- 根与子项 reparse point 为 0，相关构建/Lab 进程为 0；
- 10,096 个文件均可取得短暂独占只读句柄；
- 148 条归集 manifest 的源/目标长度与 SHA-256 全部匹配；
- 外部 trial、现用 SDK/Lab、当前 compile-diagnostics、onboarding、sdk-onboarding 的元数据摘要已记录，本机 Qt 只确认存在且与删除路径不相交。

随后按授权尝试执行 PowerShell `Remove-Item -LiteralPath`。组合逐根调用与第一条单根显式绝对路径调用均在 PowerShell 进程创建前被当前执行策略以 `blocked by policy` 拒绝；没有任何删除命令开始运行。为避免绕过安全策略，没有改用 .NET、`cmd`、通配符或其他删除手段。

阻止后只读复核结果：七根 7/7 仍存在，文件数/bytes 差异 0；148 条源/目标证据 Hash 差异 0；7 个保护记录前后差异 0。实际删除为 **0 根、0 文件、0 bytes**。

新增证据：

- `deliverables/evidence/cleanup-preparation-20260921/deletion-preflight.tsv`
- `deliverables/evidence/cleanup-preparation-20260921/protected-before.tsv`
- `deliverables/evidence/cleanup-preparation-20260921/protected-after-blocked.tsv`
- `deliverables/evidence/cleanup-preparation-20260921/deletion-attempt-blocked-20260922.md`

该证据目录不是完整构建树备份。后续若在允许破坏性操作的环境中执行删除，必须重新运行同等门禁；删除后的构建树不可从当前最小证据直接恢复。

当前状态：删除授权存在，但受执行策略阻止，七根未删除；已停止写入，等待总控决定可执行环境或由用户在本机执行。

## 6. 用户手动清理完成（2026-09-22）

用户在PowerShell 7.6.6先执行 scripts/cleanup-seven-builds-20260922.ps1 的只检查模式，
取得CHECK_ONLY_PASS，再以-Execute运行并输入DELETE-7。用户粘贴输出逐根列出DELETED，
最终为CLEANUP_PASS deleted_roots=7 bytes=4579982839，删除后EVIDENCE_OK files=148。
总控随后只读确认第3.4节七条路径全部不存在，148个归集目标的长度与SHA256全部匹配；
out/build内仍有当前windows-msvc-lab-compile-diagnostics-20260921。

实际清除10,096文件，逻辑长度4,579,982,839 bytes（约4.265 GiB）。未测量磁盘空闲空间变化。
删除由用户执行，不是代理成功绕过执行策略；未重新构建/测试或提交推送。
本次删除后的总控核对未重新全量Hash现用SDK/Lab/Qt，不把目标范围隔离当作全量完整性验证。
原构建树无法从最小日志直接恢复；可从源码重新构建，但不保证再现所有历史临时状态。
