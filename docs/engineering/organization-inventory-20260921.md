# PAE 用户入口、工程文档与生成产物盘点（2026-09-21）

## 1. 结论与边界

本轮在 `main@e2527cad500f928100b588fe433e891d460bbc6a` 上只做入口整理和只读盘点，没有移动工程文档、归集大包、删除产物、构建、测试或启动 UI。

结论：

1. 根 README 的 2026-09-06～20 历史过程已完整移入 `docs/archive/root-readme-history-20260921.md`，根入口只保留当前定位、阅读、构建、交付和边界。
2. 本报告写入前 `docs/engineering` 现场有 **171** 份 Markdown，写入后为 172 份；分类为 43 份现行入口/契约/最新证据（含本报告）与 129 份历史工程依据，后者中仅提出 30 份优先归档候选。本轮不批量移动。
3. `out/build` 有 8 个根。最新结构化诊断根继续保护；其余 7 个历史构建根共 10,096 个文件、4,579,982,839 bytes（4.265 GiB），可在归集最小配置/CTest 证据后形成精确删除申请，本轮不删除。
4. `out` 中另有 14 个没有跟踪 Markdown 精确路径引用的小型历史根，共 285 个文件、886,649 bytes；“没有引用”不等于“没有独有证据”，当前只列二级候选，不建议优先清理。
5. `src/core` 已按上一轮授权删除；`spikes/json_parser` 仍被根 `CMakeLists.txt` 引用并含 30 个 tracked 文件，不能按旧目录草率删除。
6. 当前 SDK 与 Lab 身份必须分开：SDK 是 clean `7b4205e` 五包；首选 Lab 是 `fe1683c` 加结构化诊断与流式示例打包未提交修补。`e2527ca` 是当前源码/文档检查点，不是旧二进制的新 provenance。

## 2. 工程文档分类

### 2.1 计数与分类原则

盘点输入时 `docs/engineering/*.md` 共 171 份、2,204,727 bytes。数量比任务描述的 170 多 1，是本轮总控新增 `onboarding-organization-20260921.md` 后的现场事实；本报告写入后目录总数为 172。

分类是导航和归档优先级，不是文档真假判定：

- **现行**：仍用于当前入口、稳定职责边界、当前公共契约或现用交付身份的文档；
- **历史依据**：结论仍可追溯，但“当前/下一步/待提交”只代表当时阶段；
- **优先归档候选**：已被后续契约、实现报告或执行结果覆盖的 preflight、review、plan、diagnosis、inventory；移动前必须更新所有链接并生成迁移映射。

### 2.2 现行集合：43 份

导航与状态 9 份：

```text
README.md
onboarding-organization-20260921.md
pae-execution-delivery-organization-plan.md
post-dec040-roadmap.md
document-migration-map.md
pae-lab-delivery-boundary.md
generated-artifact-cleanup-execution-20260920.md
lab-gui-capability-coverage-plan.md
organization-inventory-20260921.md
```

当前契约与语义 14 份：

```text
ascii-stream-framing-contract.md
ascii-text-codec-minimal-contract.md
bounded-stream-framing-contract.md
bounded-variable-record-contract.md
crc-minimal-contract-draft.md
host-endpoint-binding-contract.md
json_loader_diagnostic_contract_v0.1.md
length-field-minimal-contract.md
loader_compiler_boundary_v0.1.md
lab-binary-encode-g1-contract.md
lab-binary-stream-g2-contract.md
pae-public-ascii-consumption-contract.md
pae-public-host-stage2b-contract.md
protocol_lab_contract_v0.1.md
```

当前能力或交付仍依赖的验证证据 20 份：

```text
host-endpoint-binding-validation.md
lab-binary-encode-g1-validation.md
lab-binary-stream-g2-a-validation.md
lab-binary-stream-g2-b-validation.md
lab-binary-stream-g2-c-validation.md
lab-editor-commit-repair-20260920.md
lab-structured-compile-diagnostics-validation-20260921.md
lab-trial-sdk-7b4205e-validation.md
lab-stream-fixture-packaging-repair-20260920.md
pae-trial-sdk-7b4205e-validation.md
protocol-metadata-naming-validation-20260920.md
protocol-metadata-qt-smoke-repair-20260920.md
pae-public-ascii-facts-validation.md
pae-public-ascii-sdk-validation.md
pae-public-stream-framing-validation.md
pae-public-stream-sdk-validation.md
pae-public-codec-slice-validation.md
pae-public-framer-stage2a-validation.md
pae-public-host-stage2b-validation.md
pae-public-api-stage4-physical-query-validation.md
```

这不是排他的“唯一真相”列表。其余 129 份文档仍是历史证据，不应在没有替代关系和引用迁移的情况下删除。

### 2.3 优先归档候选：30 份

以下文件属于 129 份历史依据的子集，已有后续实现/验证/执行报告承接主要结论：

```text
lab-binary-stream-g2-preflight.md
lab-public-api-stage4-ascii-migration-review.md
lab-public-api-stage4-binary-migration-plan.md
lab-public-ascii-stream-preflight.md
lab-public-codec-consumer-review.md
lab-public-consumer-metadata-review.md
lab-public-consumption-residual-audit.md
lab-public-framer-host-consumer-review.md
lab-public-framer-stage2a-consumer-review.md
lab-public-host-stage2b-consumer-review.md
lab-qt-dependency-audit-plan.md
lab-sdk-external-build-preflight.md
lab-sdk-stage3-consumer-review.md
lab-sdk-standalone-implementation-scope.md
lab-trial-sdk-diagnostics-plan-20260920.md
lab-ui-usability-localization-plan.md
pae-binary-stream-g2-public-audit.md
pae-gui-consumption-capability-audit.md
pae-public-api-stage4-ascii-gap-review.md
pae-public-api-stage4-observation-gap-design.md
pae-public-stream-framing-preflight.md
pae-sdk-clean-checkpoint-preflight.md
pae-sdk-stage3-content-review.md
protocol-metadata-naming-plan-20260920.md
protocol-metadata-qt-smoke-diagnosis-20260920.md
repository-organization-inventory.md
generated-artifact-cleanup-inventory-20260919.md
pae-artifact-cleanup-inventory-20260920.md
lab-editor-commit-diagnosis-20260920.md
lab-g2-delivery-candidate-20260920.md
```

建议按“同一能力链的 preflight/review/plan → contract → validation”成组迁移，先更新 `docs/engineering/README.md`、正文反向链接和 `document-migration-map.md`，再移动文件；不要仅按文件日期批量归档。本轮没有执行这些移动。

## 3. `out/build` 精确盘点

### 3.1 当前保护根

| 绝对路径 | 文件 / bytes | 报告引用与保护原因 |
| --- | ---: | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-compile-diagnostics-20260921` | 1,195 / 575,095,706 | `lab-structured-compile-diagnostics-validation-20260921.md` 直接引用；对应当前首选 Lab 的结构化诊断源码验证，暂不列入清理 |

另一任务正在使用 `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\onboarding-20260921` 验证入门示例；该根不是本盘点的历史候选，必须等任务完成和总控回收后再判断。

### 3.2 七个历史构建候选

七根均位于当前主 worktree 内，现场未发现 reparse point。它们都是 CMake/VS 生成树，可以从保留源码、工具链和配置重建，但根内仍可能含 `CMakeCache.txt`、`CMakeConfigureLog.yaml` 和 `Testing` 状态，不能直接删除。

| 绝对路径 | 文件 | bytes | 当前报告引用 / 唯一性 | 可清理条件 |
| --- | ---: | ---: | --- | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-editor-commit-diagnosis-20260920` | 62 | 50,798,452 | `lab-editor-commit-diagnosis-20260920.md` 引用；诊断结论已由 repair 报告承接 | 归集 CMake 配置、CTest/诊断日志并映射 diagnosis/repair 报告 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-editor-commit-repair-20260920` | 991 | 296,806,972 | `lab-editor-commit-repair-20260920.md` 多处引用；最终 D/R smoke 日志在相邻 validation 根 | 先核对 validation 最终日志完整，再归集 build 内配置与 Testing 状态 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-a` | 641 | 117,586,939 | G2-A contract/validation 直接引用；源码已进入后续检查点 | 归集配置/CTest，保护 G2-A validation 与契约 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-b` | 1,036 | 207,922,854 | G2-B 报告记录父根失败缓存与 `v142` 有效子根 | 两类缓存都要记录，不能只保存成功子根；保护 G2-B validation |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-lab-g2-c` | 3,803 | 1,630,151,827 | G2-C common/Binary-only/config gate 报告及旧 `fa81329` 交付来源引用 | 先保护 `deliverables/lab/fa81329` 及 manifest，归集三类配置/CTest 和来源映射 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-protocol-metadata-20260920` | 2,572 | 1,983,992,868 | naming plan/validation 直接引用；含 D/R 及 Qt 目标生成物 | 归集配置、最终专项日志/CTest；保留 protocol-metadata validation |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-qt-smoke-repair-20260920` | 991 | 292,722,927 | Qt smoke repair 报告引用；最终 repair 日志在相邻 validation 根 | 归集配置与 Testing 状态，保护 repair/diagnosis 报告及 validation |

合计：7 根、10,096 个文件、4,579,982,839 bytes（4.265 GiB）。建议下一轮只对这 7 条绝对路径建立白名单：先归集并逐文件 Hash，再另请用户确认删除；不得使用 `out/build/windows-msvc-*` 通配符，以免包含当前结构化诊断根。

## 4. `out` 零散证据

### 4.1 二级候选：没有跟踪 Markdown 精确路径引用

下列 14 根不是立即删除项。它们体积合计仅 886,649 bytes，优先清理收益很低，且 manual/recovery/review/smoke 名称提示可能含独有失败过程：

| 绝对路径 | 文件 / bytes |
| --- | ---: |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\bench-memory` | 6 / 19,835 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\crc-recovery` | 46 / 49,695 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\dec042a-smoke` | 19 / 21,855 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\dec042b-c3-manual` | 30 / 40,717 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\dec042b-c3-manual-invalid-values` | 6 / 6,828 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\lab-sdk-standalone-preflight-configs` | 12 / 545,327 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\v08-recovery-runs` | 28 / 33,391 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\v08-review-bit-container` | 34 / 38,234 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\v08-review-missing-length` | 16 / 17,120 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\v08-tmp` | 6 / 5,561 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\dec040-debug-targeted` | 28 / 36,225 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\dec040-p2-smoke-20260906a` | 18 / 20,317 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\length-resume-debug` | 35 / 46,510 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\validation\schema-debug-inspect` | 1 / 5,034 |

若以后清理，应先按内容把失败复现、人工记录、输入、日志和纯生成副本分开；只有在对应报告已承接结论或最小证据已归集后，才按精确根申请删除。

### 4.2 必须保留或暂不判断

- `out\sdk-clean-checkpoint`、`out\sdk-clean-checkpoint-validation`：历史 clean SDK 身份和原始验证，保留。
- `out\validation` 中被当前报告直接引用的各根：原始日志很小，保留。
- `out\downloads`（12,322,603 bytes）与 `out\sources`（26,094,751 bytes）：影响离线依赖复现，策略未定，保留。
- `out\symlink-capability-check`：含当前唯一 reparse point，必须用单独链接策略处理，本轮保留且不跟随。
- `out` 根直属 605 个普通文件、16,047,535 bytes：来源混合，禁止按根级通配符删除；应先建立逐文件来源/引用清单。
- `F:\PersonalWorkspace\pae-trial-7b4205e-20260920`：当前推荐 SDK 与 Lab 试用根，35,144 个文件、7,084,028,424 bytes（6.598 GiB），不是本轮清理候选。
- 仓库 `deliverables`、`local_private`（若存在）、本机 Qt、旧 `98df5e0`/`fa81329` 对照交付均继续保护。

## 5. 其他仓库目录

| 路径 | 现场事实 | 结论 |
| --- | --- | --- |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\src\core` | 已在 2026-09-20 获批清理中删除；Git 原本不跟踪空目录 | 不重建，不再列候选 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\spikes\json_parser` | 54 个物理文件、386,074 bytes，其中 30 个 tracked；根 `CMakeLists.txt` 仍 `add_subdirectory(spikes/json_parser)` | 必须保留；如要退出需先修改构建入口、preset、测试和文档，属于独立重构 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\docs\engineering` | 171 份文档，现行与历史混排 | 先按第 2 节迁移 30 个高置信候选，不删除历史事实 |
| `F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\docs\archive\root-readme-history-20260921.md` | 根 README 入口降噪前完整快照，链接已按 archive 位置修正 | 长期保留 |

本轮没有发现可在不改变构建或历史追溯的前提下立即删除的其他 tracked 目录。

## 6. 后续建议顺序

1. 总控先复核根 README、`docs/README.md`、指南 01、Schema 入口和并行入门示例的一致性。
2. 单独派发文档归档：按第 2.3 节 30 个精确文件分组移动，并同时更新链接与迁移映射；不要一次移动全部 129 份历史依据。
3. 单独派发七根构建清理：归集 CMake/CTest/日志和报告引用、Hash 校验、保护当前交付，再向用户提交七条绝对路径确认。
4. 零散证据晚于大型构建根处理；其体积小，不值得在证据策略未明确时冒追溯风险。

## 7. 验证与限制

- 本轮使用精确路径统计普通文件逻辑长度；未跟随 reparse point，未用磁盘空闲空间估算。
- 现场只登记一个 Git worktree；盘点时未发现 Lab/CMake/CTest/MSBuild/Ninja 等相关进程。真正删除前仍需重新检查。
- 文档分类基于当前入口、后继契约/验证和引用关系；没有逐字审查 171 份文档的全部历史断言，因此本轮只移动根 README 快照，不批量移动工程文档。
- 未构建、测试、启动 UI、Stage、Commit、Push、发布或删除。

状态：完成入口降噪与清理盘点，待总控复核。
