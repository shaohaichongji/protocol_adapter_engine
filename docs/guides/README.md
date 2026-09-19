# 开发者指南

本目录面向使用者和验收操作者。请先区分开发树、现有 SDK 候选与完整 Qt Lab standalone 三条路径；
Schema 增量和工具边界见 [Schema 索引](../../schema/README.md)。

## 建议阅读顺序

1. 从开发树构建或启动 Lab 时，读[仓库 README](../../README.md#1-开发树构建)，使用 `windows-msvc-pae-lab` 预设。
2. 消费现有 Windows x64 SDK 候选时，读 [SDK 快速入口](pae-sdk-windows-quickstart.md)，再进入所选包的 `PAE-SDK-README.md` 和 `examples/sdk_consumer`。
3. 用 installed SDK 构建完整 Qt Lab 时，直接读 [standalone 入口](../../tools/protocol_lab_ui/standalone/README.md)；该入口要求显式选择 `static|shared` 并使用包外新目录。已验证的本地产物优先从 static Release 路径启动，精确 EXE、配置目录和 shared 对照路径见该入口。
4. 学习配置时，读 [Schema 索引](../../schema/README.md)，再对照 [ASCII 完整记录示例](../../examples/ascii_text/README.md)或[宿主端点示例](../../examples/host_endpoint/README.md)。示例覆盖范围不同，不代表全部能力。
5. 理解历史内部嵌入方式时，可读[业务嵌入示例](../../examples/business_embedding/README.md)；SDK 消费应以包内综合 consumer 为准。
6. 修改实现或查精确限制时，再进入[工程依据](../engineering/README.md)，不用先通读历史验证报告。

## 专项人工清单（不是新用户必做项）

- [DEC-042B C3 人工离线验收单](manual-dec042b-c3-offline-acceptance.md)：公开合成配置下的人工操作步骤，状态仍为 `NOT_EVALUATED`；代理执行结果不能替代用户人工验收。

仓库开发、配置和 Lab 排错内容仍分布在仓库 README、`examples/*/README.md`、`tools/*/README.md`
及工程契约中。SDK 快速入口以当前 `98df5e0` clean-checkpoint 候选为主，旧 dirty 候选和 `final6` 只保留历史身份；不把历史验证扩展成新的能力或发布承诺。
