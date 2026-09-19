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
