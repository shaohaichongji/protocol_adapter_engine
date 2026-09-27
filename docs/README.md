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
| 管理中间构建与清理 | [两批保留规则](engineering/build-artifact-retention.md)、[本轮清理记录](engineering/build-cleanup-20260927.md) |

## 当前交付身份

首次上手以 [完整体验包入口](../deliverables/README.md) 为准：当前首选为 `deliverables/sdk/adeae30-experience/PAE-Lab-Windows-x64-adeae30.zip`，内含五个 SDK、配套 Lab、说明和示例。产品源码固定为 `adeae30`，SDK 文档为独立投影身份；详见 [体验包验证](engineering/experience-bundle-validation-20260927.md)。以下 b12ad80 为上一批对照，不能替代当前首次体验入口。

本机统一入口为 [`deliverables/README.md`](../deliverables/README.md)。上一批本地初版候选来自固定干净提交 `b12ad809bdbc35589e8e5755f0e4ad04f386fa10`：七个 SDK 包在 `deliverables/sdk/b12ad80/`，消费同批 SDK 的 standalone Lab 在 `deliverables/lab/b12ad80/`，该批对照优先 static Release。包含可选受限 YAML 入口，JSON 仍为 canonical 编译输入。构建及包外消费证据、归集校验见 [同基线归集记录](engineering/yaml-clean-delivery-validation-20260926.md)。用户已确认 static Release JSON/YAML 正常解析通过，Lab 已关闭；配置错误诊断等未测项保持边界。本地产物被 Git 忽略，仅 clone 仓库不会自动取得。

此前 `7b4205e` SDK、`fe1683c-plus-patches` Lab 及提交前 dirty YAML 候选保留原构建身份，不因新版本归集而改变验证记录。

旧 `98df5e0` SDK/standalone 与 `fa81329/common-release` 继续作为历史对照。各身份不能互换来源证据；均不表示稳定 ABI、Linux、正式分发、许可闭合、真实协议或现场验收。

文档改名和拆分见 [迁移映射](engineering/document-migration-map.md)。验证记录只证明正文列明的范围；历史文档中的“当前”“下一步”不自动代表今天的状态或授权。
