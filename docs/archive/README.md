# 历史归档

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

本目录保存已被后续阶段替代、但对决策或验证追溯仍有价值的文档。归档不表示内容无效，也不表示其中历史状态仍是当前状态；需要判断当前能力时返回[工程依据](../engineering/README.md)和[文档总入口](../README.md)。

## 历史推进与阶段记录

- [2026-09-21 工程历史归档](engineering-20260921/README.md)：30 份 preflight、review、plan、diagnosis 与 inventory，按能力链组织。
- [旧检查点交付计划](checkpoint-delivery-plan.md)：已被后续路线、契约和实际交付记录替代。
- [Lab ASCII 内部适配 Stage 1 验证](lab-ascii-adapter-stage1-validation.md)：已由后续 UI Stage 2 与完整观察验证继续推进。

## 生成物与旧工作区整理记录

- [生成产物盘点](generated-artifact-inventory-20260914.md)
- [旧 Lab worktree 退役记录](generated-artifact-worktree-retirement-20260914.md)
- [清理执行快照](generated-artifact-cleanup-execution-20260914.json)
- [清理授权清单](generated-artifact-cleanup-manifest-20260914.json)
- [证据索引](generated-artifact-evidence-index-20260914.json)

这些文件保留 2026-09-14 当时的路径、Hash 和边界。用户随后已永久删除外层 `<PROJECT_WORKSPACE_ROOT>\Lab验证` 与 `<PROJECT_WORKSPACE_ROOT>\工程整理归档`；JSON 和历史正文中指向这两个目录的路径现已失效，不得据此宣称外层归档仍可访问或可恢复。
