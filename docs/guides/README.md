# 开发者指南

本目录面向使用者和验收操作者。消费当前 Windows x64 本地 SDK 候选先读
[SDK 快速入口](pae-sdk-windows-quickstart.md)；项目能力、仓库构建与 Lab 启动仍以
[仓库 README](../../README.md)为入口，Schema 增量和工具边界见 [Schema 索引](../../schema/README.md)。

## 建议阅读顺序

1. 使用当前 Windows x64 SDK 本地候选时，读 [SDK 快速入口](pae-sdk-windows-quickstart.md)，再进入所选包的 `PAE-SDK-README.md` 和 `examples/sdk_consumer`。
2. 从开发仓库构建或启动 Lab 时，读[仓库 README](../../README.md)的当前能力和推荐入口。
3. 学习配置时，读 [Schema 索引](../../schema/README.md)，再对照 [ASCII 完整记录示例](../../examples/ascii_text/README.md)或[宿主端点示例](../../examples/host_endpoint/README.md)。示例覆盖范围不同，不代表全部能力。
4. 理解历史内部嵌入方式时，可读[业务嵌入示例](../../examples/business_embedding/README.md)；SDK 消费应以包内综合 consumer 为准。
5. 修改实现或查精确限制时，再进入[工程依据](../engineering/README.md)，不用先通读历史验证报告。

## 专项人工清单（不是新用户必做项）

- [DEC-042B C3 人工离线验收单](manual-dec042b-c3-offline-acceptance.md)：公开合成配置下的人工操作步骤，状态仍为 `NOT_EVALUATED`；代理执行结果不能替代用户人工验收。

仓库开发、配置和 Lab 排错内容仍分布在仓库 README、`examples/*/README.md`、`tools/*/README.md`
及工程契约中。SDK 快速入口只汇总 final6 本地候选的既有事实，不把历史验证扩展成新的能力或发布承诺。
