# 文档入口

这里保存 PAE / Lab 的通用文档，不包含真实协议资料或外部生产项目内容。新用户先走顺序指南；维护者需要精确事实时再进入工程依据，不要从历史阶段报告倒推当前行为。

## 面向使用者

| 目的 | 入口 |
| --- | --- |
| 从零理解项目 | [01 项目定位与能力边界](guides/01-项目定位与能力边界.md) |
| 直接启动本机 Lab | [02 首次运行与 Lab 体验](guides/02-首次运行与Lab体验.md) |
| 接入 Windows x64 SDK | [03 Windows SDK 集成](guides/03-Windows-SDK集成.md) |
| 编写 `*.pae.json` | [04 协议配置入门](guides/04-协议配置入门.md)、[Schema 索引](../schema/README.md) |

## 面向维护者

| 目的 | 入口 |
| --- | --- |
| 阅读架构和源码 | [05 架构与代码阅读](guides/05-架构与代码阅读.md) |
| 构建、测试和排错 | [06 构建测试与问题定位](guides/06-构建测试与问题定位.md) |
| 使用 installed SDK 构建完整 Qt Lab | [standalone README](../tools/protocol_lab_ui/standalone/README.md) |
| 查契约、设计和验证记录 | [工程依据索引](engineering/README.md) |
| 查已替代资料 | [历史归档](archive/README.md) |

## 当前交付身份

本机统一入口为 [`deliverables/README.md`](../deliverables/README.md)。当前推荐 SDK 五包来自 clean `7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`；首选 Lab 来自 `fe1683c` 加结构化编译诊断与 Binary 流式示例打包的未提交修补。Lab 使用上述 installed SDK，但 Lab 二进制不能据此冒充 clean `e2527ca` 重建。

旧 `98df5e0` SDK/standalone 与 `fa81329/common-release` 继续作为历史对照。各身份不能互换来源证据；均不表示稳定 ABI、Linux、正式分发、许可闭合、真实协议或现场验收。

文档改名和拆分见 [迁移映射](engineering/document-migration-map.md)。验证记录只证明正文列明的范围；历史文档中的“当前”“下一步”不自动代表今天的状态或授权。
