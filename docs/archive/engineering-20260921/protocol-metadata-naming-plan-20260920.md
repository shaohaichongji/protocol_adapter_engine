# 协议元数据内部命名整理

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 授权与目标

2026-09-20 用户同意规划并整理 `src/config_compiler/ui_description.*` 的职责命名。它们实际提供无 Qt 的协议元数据存储、编译与索引核对，不是废弃 UI 代码。本轮保留能力、预算、所有权、执行顺序及公开 API/Schema，仅整理内部名称和直接引用。

基线 `main@fa81329`；此前入口和清理报告的未提交修改必须保留。本轮无 Stage/Commit/Push、SDK 重打包、覆盖部署或额外清理授权。

## 固定映射

| 原名 | 新名 |
| --- | --- |
| ui_description.h/.cpp | protocol_metadata.h/.cpp |
| ui_description_internal.h | protocol_metadata_internal.h |
| UiDescriptionSidecar | ProtocolMetadataStorage |
| UiDescriptionBuilder | ProtocolMetadataBuilder |
| CompiledUiArtifacts | CompiledProtocolArtifacts |
| CompileUiArtifactsResult | CompileProtocolArtifactsResult |
| CompileJsonToPlanWithUiDescription | CompileJsonToPlanWithMetadata |
| DerivedUiDescriptionMemoryLimit | DerivedProtocolMetadataMemoryLimit |
| UiDescriptionTestProbe / ScopedUiDescriptionTestProbe | ProtocolMetadataTestProbe / ScopedProtocolMetadataTestProbe |
| UiDescriptionLayoutTestInput | ProtocolMetadataLayoutTestInput |
| EstimateUiDescriptionLayoutForTest | EstimateProtocolMetadataLayoutForTest |
| UiDescriptionPlanFreezeAllowedForTest | ProtocolMetadataPlanFreezeAllowedForTest |

同步测试源名 `ui_description_tests.cpp` 为 `protocol_metadata_tests.cpp`。已有测试 target/CTest 名若被脚本依赖可保留并注明，不为命名统一扩改历史证据。

## 职责与禁止项

- 元数据仍归 config_compiler；公开 `include/pae` 名称、布局、签名和语义不变。
- 更新直接依赖的 src/public_api、Lab 私有兼容消费者、测试、CMake 源列表和 SDK 源包白名单，仅机械引用迁移。
- Lab 自有的 `UiDescriptionBytes`、UI DTO、控件、展示预算等名称不属于本次改名，不按 `Ui` 全局替换。
- 检查测试探针当前隔离事实并在报告记载；不趁机改变注入机制、条件编译、结构布局或生产行为。
- 允许将内部诊断中的误导性 UI compiler 表述改为 protocol metadata/compiler；错误码、阶段和失败行为不变。若存在逐字诊断契约冲突先停报。
- 历史报告保留原文件名和原命令，当前阅读入口由总控同步，不修改旧包 provenance/hash。

## 分工与依赖

《子任务推进》独占上述内部重命名、直接消费者引用、专项验证和新报告 `protocol-metadata-naming-validation-20260920.md`；《Lab应用推进》和《PAE工程整理》不并发写代码。总控维护本计划、综合计划、外层 AGENTS 和必要当前指南。实施回收后总控审差异与证据，不增加人工 UI 验收。

## 验证及停点

使用全新 `out/build/windows-msvc-protocol-metadata-20260920` 和 `out/validation/protocol-metadata-20260920`，不覆盖已保护 G2 构建。沿用项目已验证 v142/仓库 Qt，不改全局环境。

1. 扫描新旧引用，核对公开头无差异、非 Qt 依赖方向不变、SDK 白名单全部存在且无旧路径。
2. Debug/Release 编译元数据专项、公开 Compiler/Codec 查询及实际受影响 Lab 消费者；先构建所有选中测试 target，再运行对应 CTest，Release 保留断言。
3. Lab common 兼容引用与公开 Binary 引用均需编译覆盖；只做相关隐藏/无窗口专项，不启动可见 UI。
4. 若新配置、工具链或源码缺口要求功能修复、额外 SDK 包或门禁变更，停报，不扩大范围。
5. 完成后记录精确开关、命令、结果、未验证边界与实际改动，主动向总控反馈一次并停止写入。
