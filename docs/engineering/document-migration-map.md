# docs 三分区迁移映射

日期：2026-09-14。该映射只记录路径整理，不改变各文档正文中的历史结论、验证范围或 Git 状态。

## 映射规则

| 原路径 | 新路径 | 数量 | 原则 |
| --- | --- | ---: | --- |
| `docs/manual-dec042b-c3-offline-acceptance.md` | `docs/guides/manual-dec042b-c3-offline-acceptance.md` | 1 | 面向操作者的人工验收步骤 |
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
