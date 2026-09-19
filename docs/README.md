# 文档入口

这里保存通用 PAE / Lab 文档，不包含真实协议资料或外部生产项目内容。先按阅读目的选择入口，不必从历史迭代记录开始。

| 阅读目的 | 入口 |
| --- | --- |
| 从开发树构建并启动 Lab | [仓库 README](../README.md#1-开发树构建) |
| 使用现有 Windows x64 SDK 候选 | [SDK 快速入口](guides/pae-sdk-windows-quickstart.md)、包内 `PAE-SDK-README.md` 与 `examples/sdk_consumer` |
| 使用 installed SDK 构建完整 Qt Lab | [standalone 入口](../tools/protocol_lab_ui/standalone/README.md)、[同批 clean-checkpoint SDK 的 Lab 验证](engineering/lab-clean-sdk-consumption-validation.md) |
| 学习配置和已有示例 | [Schema 索引](../schema/README.md)、[开发者阅读路线](guides/README.md) |
| 修改实现、核对契约或验证边界 | [工程依据](engineering/README.md) |
| 查阅已替代的历史资料 | [历史归档](archive/README.md) |

当前工程整理方向以 [PAE / Lab 职责与独立交付边界](engineering/pae-lab-delivery-boundary.md)为准。
当前 `candidate1-20260919` 五包候选来自 `98df5e0`、`source_worktree_dirty=false` 的独立源树，已有六组仓库外消费证据；
完整 Qt Lab 也已完成同批 SDK 的 static/shared Debug/Release 本地 standalone 闭包验证。
这不表示当前共享工作树无文档变更，也尚未形成统一正式分发；稳定 ABI、Linux、生产、许可复核和新人工 UI 验收结论均未由此升级。

完整契约与验证记录按主题收录在工程依据索引；旧路径迁移规则见[迁移映射](engineering/document-migration-map.md)。验证记录只证明其列明范围，历史文档中的“当前”“下一步”不自动代表现在的状态或授权。
