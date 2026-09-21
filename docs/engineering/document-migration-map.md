# docs 三分区迁移映射

日期：2026-09-14。该映射只记录路径整理，不改变各文档正文中的历史结论、验证范围或 Git 状态。

## 映射规则

| 原路径 | 新路径 | 数量 | 原则 |
| --- | --- | ---: | --- |
| `docs/manual-dec042b-c3-offline-acceptance.md` | `docs/guides/专项验收/DEC-042B-C3-人工离线验收.md` | 1 | 面向操作者的人工验收步骤；现归入专项验收 |
| `docs/<有效契约、设计或验证文件>` | `docs/engineering/<同名文件>` | 77 | 当前仍需用于实现、复核或证据边界判断；拿不准时保守留在 engineering |
| `docs/<已替代历史文件>` | `docs/archive/<同名文件>` | 7 | 已被后续阶段替代但仍有追溯价值 |
| `docs/README.md` | `docs/README.md` | 1 | 保留总入口 |

迁移前 docs 顶层 86 个文件；迁移后 85 个文件进入三分区，总入口留在原位。全部使用原文件名，具体 engineering 文件见[工程依据索引](README.md)。

## archive 精确清单

- `checkpoint-delivery-plan.md`
- `lab-ascii-adapter-stage1-validation.md`
- `generated-artifact-inventory-20260914.md`
- `generated-artifact-worktree-retirement-20260914.md`
- `generated-artifact-cleanup-execution-20260914.json`
- `generated-artifact-cleanup-manifest-20260914.json`
- `generated-artifact-evidence-index-20260914.json`

其中生成物与 worktree 整理记录内的外层 `Lab验证`、`工程整理归档` 路径已因用户后续永久删除而失效；保留路径只用于解释当时操作，不作为当前可访问证据入口。

## 2026-09-19 人读指南顺序化

本次只调整面向人的阅读入口，不批量改名 `engineering/` 或 `archive/` 中的契约、验证与历史证据。旧内容按主题拆分复用，映射如下：

| 原路径 | 新路径 | 说明 |
| --- | --- | --- |
| `docs/guides/pae-first-use.md` | `docs/guides/01-项目定位与能力边界.md` | 提取项目职责、能力和证据边界 |
| `docs/guides/pae-first-use.md` | `docs/guides/02-首次运行与Lab体验.md` | 提取首次启动和 Lab 离线体验 |
| `docs/guides/pae-first-use.md` | `docs/guides/04-协议配置入门.md` | 提取真实配置起步和 Compiler 流程 |
| `docs/guides/pae-sdk-windows-quickstart.md` | `docs/guides/03-Windows-SDK集成.md` | 按 Source/Static/Shared 重组 SDK 集成 |
| `docs/guides/pae-maintainer-reading-map.md` | `docs/guides/05-架构与代码阅读.md` | 保留五站阅读法并补架构图 |
| 根 `README.md`、旧三指南中的构建/排错内容 | `docs/guides/06-构建测试与问题定位.md` | 新增面向维护者的分层排错入口 |
| `docs/guides/manual-dec042b-c3-offline-acceptance.md` | `docs/guides/专项验收/DEC-042B-C3-人工离线验收.md` | 移出主线；保留 `NOT_EVALUATED` 和历史证据边界 |

`docs/guides/README.md` 保留惯例英文文件名，正文提供 01～06 的唯一推荐顺序。旧文件名不保留重定向副本，仓库内版本化 Markdown 引用已机械更新到新路径。

## 2026-09-21 工程历史归档

以下 30 份文档从 `docs/engineering/` 迁入 `docs/archive/engineering-20260921/`。迁移保持原文件名和历史正文，只增加统一归档说明并按新位置校正相对链接。

| 原路径 | 新路径 |
| --- | --- |
| `docs/engineering/lab-binary-stream-g2-preflight.md` | `docs/archive/engineering-20260921/lab-binary-stream-g2-preflight.md` |
| `docs/engineering/lab-public-api-stage4-ascii-migration-review.md` | `docs/archive/engineering-20260921/lab-public-api-stage4-ascii-migration-review.md` |
| `docs/engineering/lab-public-api-stage4-binary-migration-plan.md` | `docs/archive/engineering-20260921/lab-public-api-stage4-binary-migration-plan.md` |
| `docs/engineering/lab-public-ascii-stream-preflight.md` | `docs/archive/engineering-20260921/lab-public-ascii-stream-preflight.md` |
| `docs/engineering/lab-public-codec-consumer-review.md` | `docs/archive/engineering-20260921/lab-public-codec-consumer-review.md` |
| `docs/engineering/lab-public-consumer-metadata-review.md` | `docs/archive/engineering-20260921/lab-public-consumer-metadata-review.md` |
| `docs/engineering/lab-public-consumption-residual-audit.md` | `docs/archive/engineering-20260921/lab-public-consumption-residual-audit.md` |
| `docs/engineering/lab-public-framer-host-consumer-review.md` | `docs/archive/engineering-20260921/lab-public-framer-host-consumer-review.md` |
| `docs/engineering/lab-public-framer-stage2a-consumer-review.md` | `docs/archive/engineering-20260921/lab-public-framer-stage2a-consumer-review.md` |
| `docs/engineering/lab-public-host-stage2b-consumer-review.md` | `docs/archive/engineering-20260921/lab-public-host-stage2b-consumer-review.md` |
| `docs/engineering/lab-qt-dependency-audit-plan.md` | `docs/archive/engineering-20260921/lab-qt-dependency-audit-plan.md` |
| `docs/engineering/lab-sdk-external-build-preflight.md` | `docs/archive/engineering-20260921/lab-sdk-external-build-preflight.md` |
| `docs/engineering/lab-sdk-stage3-consumer-review.md` | `docs/archive/engineering-20260921/lab-sdk-stage3-consumer-review.md` |
| `docs/engineering/lab-sdk-standalone-implementation-scope.md` | `docs/archive/engineering-20260921/lab-sdk-standalone-implementation-scope.md` |
| `docs/engineering/lab-trial-sdk-diagnostics-plan-20260920.md` | `docs/archive/engineering-20260921/lab-trial-sdk-diagnostics-plan-20260920.md` |
| `docs/engineering/lab-ui-usability-localization-plan.md` | `docs/archive/engineering-20260921/lab-ui-usability-localization-plan.md` |
| `docs/engineering/pae-binary-stream-g2-public-audit.md` | `docs/archive/engineering-20260921/pae-binary-stream-g2-public-audit.md` |
| `docs/engineering/pae-gui-consumption-capability-audit.md` | `docs/archive/engineering-20260921/pae-gui-consumption-capability-audit.md` |
| `docs/engineering/pae-public-api-stage4-ascii-gap-review.md` | `docs/archive/engineering-20260921/pae-public-api-stage4-ascii-gap-review.md` |
| `docs/engineering/pae-public-api-stage4-observation-gap-design.md` | `docs/archive/engineering-20260921/pae-public-api-stage4-observation-gap-design.md` |
| `docs/engineering/pae-public-stream-framing-preflight.md` | `docs/archive/engineering-20260921/pae-public-stream-framing-preflight.md` |
| `docs/engineering/pae-sdk-clean-checkpoint-preflight.md` | `docs/archive/engineering-20260921/pae-sdk-clean-checkpoint-preflight.md` |
| `docs/engineering/pae-sdk-stage3-content-review.md` | `docs/archive/engineering-20260921/pae-sdk-stage3-content-review.md` |
| `docs/engineering/protocol-metadata-naming-plan-20260920.md` | `docs/archive/engineering-20260921/protocol-metadata-naming-plan-20260920.md` |
| `docs/engineering/protocol-metadata-qt-smoke-diagnosis-20260920.md` | `docs/archive/engineering-20260921/protocol-metadata-qt-smoke-diagnosis-20260920.md` |
| `docs/engineering/repository-organization-inventory.md` | `docs/archive/engineering-20260921/repository-organization-inventory.md` |
| `docs/engineering/generated-artifact-cleanup-inventory-20260919.md` | `docs/archive/engineering-20260921/generated-artifact-cleanup-inventory-20260919.md` |
| `docs/engineering/pae-artifact-cleanup-inventory-20260920.md` | `docs/archive/engineering-20260921/pae-artifact-cleanup-inventory-20260920.md` |
| `docs/engineering/lab-editor-commit-diagnosis-20260920.md` | `docs/archive/engineering-20260921/lab-editor-commit-diagnosis-20260920.md` |
| `docs/engineering/lab-g2-delivery-candidate-20260920.md` | `docs/archive/engineering-20260921/lab-g2-delivery-candidate-20260920.md` |

能力链索引见[归档目录 README](../archive/engineering-20260921/README.md)。总控维护的 `onboarding-organization-20260921.md` 与 `pae-execution-delivery-organization-plan.md` 未在本任务中修改，其中旧引用待总控回收时统一修正。
