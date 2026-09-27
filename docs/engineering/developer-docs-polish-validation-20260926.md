# 开发者文档主线打磨验证记录

## 总控补充（2026-09-27）

下文“并行修复尚未由本片验证”是文档任务交接时的证据边界。随后 C 片已完成，
总控抽查差异、宿主断言、Cache 和运行日志，与
[示例修复验证](developer-integration-example-fix-validation-20260926.md) 一致；
指南 03 已同步仓库修复与旧包未更新的区别。没有据此宣称全部教程命令重新执行，
Mermaid 渲染、Debug/全量/UI 仍未新增验证。

## 范围与基线

- 本片按 [开发者上手体验与接入核查计划](developer-onboarding-polish-plan-20260926.md) 的 A 片执行。起点为 `main@8eb5bb6`；`b12ad80` SDK/Lab 是既有本地评审产物，本片没有改动包内字节。
- 仅改 `docs/guides/README.md`、指南 02/03/04/05、`docs/sdk/README.md`，并新增本记录。总控维护的工程索引和路线文件虽在同一工作树显示修改，但不属于本片写入。
- 用户此前确认同版 static Release Lab 的 JSON/YAML 正常解析；错误配置诊断没有人工确认。本片没有新增构建、运行或人工验收证据。

## 已整理的开发者路线

1. 指南入口以当前 `b12ad80` 为主，首次 Lab 体验并列给出 JSON 与 YAML 的实际配置、绑定、输入和预期观察；旧候选的有用向量保留在附录。
2. SDK 指南先选择精确包，再从包内 `getting_started` 或 `yaml_sdk_consumer` 独立入门。包内 Source 与 installed 配置路径分开，包、配置和示例分别使用新 BuildRoot；JSON-only 包不声称提供 YAML 组件。包内 README 源稿同步上述用法，但已生成的 `b12ad80` 包内说明未更新。
3. 配置指南以 `synthetic_ascii_literal_only` 的 JSON/YAML 同协议文件对照，区分 PAE Schema 0.10、YAML 1.2 语法与 YAML Profile V0.1；前端诊断、生成 JSON 的编译诊断和执行失败分别处理。
4. 架构指南画出可选 YAML 前端到严格 JSON、再到原编译器的配置流，并补充公开头、owner/view 与来源映射的阅读入口。图中的其他箭头仍按代码依赖解释。

## 静态核对

- `git diff --check`：通过。
- 六份被改文档的相对 Markdown 链接逐一按所在目录解析：均存在。
- 当前 Static Release SDK 的两个示例 CMake、YAML 示例配置、installed JSON 示例配置，以及当前 Lab 同协议 JSON/YAML 配置：路径均存在；`examples/config/` 的 JSON 与测试夹具 YAML、`src/config_frontend_yaml/frontend.cpp` 也存在。
- 三处 Mermaid 块均有 `flowchart` 起始与对应代码围栏；检查了新增节点和连线的文字结构。未调用渲染器，视觉排版未验证。
- 没有执行教程中的 CMake 命令、SDK consumer 或 Lab；因此命令的本轮可重复性仍待使用者或后续专项验证。

## 保留的边界

- 初次 A 片完成时，Source 示例的 `BUILD_TESTING` / `PAE_BUILD_TESTING` `FORCE` 风险尚待 B 片核查；后续已按下节的复现实验同步文档，但 `b12ad80` 包内旧示例仍不当作已有宿主的通用模板。
- `b12ad80` 当前包内 README、manifest、provenance 未就地修改；如需把这份源稿带入包，应另建候选并重新核验。
- 文档不建立跨工具链 ABI、Linux、真实设备、现场、生产或错误诊断人工验收结论。
- 无 Stage、Commit、Push、删除、部署覆盖或发布。

## B 片回收后的限定证据同步

总控回收 [最小宿主与接口审计](developer-integration-audit-20260926.md) 后，限定更新指南 03/05、`docs/sdk/README.md` 与本记录，不触及其他指南、产品代码或已生成的 SDK 包：

- 已复现：把旧 Source 示例 CMake 嵌入有自身 CTest 的宿主，会以 `FORCE` 使 `BUILD_TESTING` 从 ON 变 OFF，`ctest -N` 只见 0 项。文档改为区分“独立运行旧包示例”和“宿主直接受控 `add_subdirectory` 包根”；后者以普通目录变量配置 PAE 选项，隔离宿主自有测试 1/1。并行的仓库示例修复尚未由本片验证，不写成已通过。
- 已复现：同 BuildRoot 从 Static 切 Shared 时，仅改 `CMAKE_PREFIX_PATH` 仍可能保留旧 `PAE_DIR`。文档以每包、每配置、每示例新 BuildRoot 为首选，并提示检查缓存路径和目标类型。
- B 片 Static/Shared Release 最小宿主各 1/1；Static `PAE::pae` 的传递链接包含五个内部 `.lib` 与静态宏。文档优先建议 CMake target，不承诺原生 VS/qmake 单库手工链接。
- 使用者入口补齐 Codec/Framer/Host 选型、描述 view 与冻结执行状态的不同寿命、同步回调借用、失败输出不可交付及 Host 成功 Reset 后重新 `Find`。这些是公开头和 B 片源码审计支持的边界，不是本片新运行测试。

本次仍只做文档静态检查；B 片的实验结果按其报告范围引用，不扩展为 SDK 全矩阵、UI、硬件或现场通过。旧包内 README、manifest、provenance 没有就地改动；无 Stage、Commit、Push、删除或发布。

本次四份限定文件的相对 Markdown 链接均解析到现有路径；`git diff --check` 通过；
`scripts/check-portable-paths.ps1 -ChangedOnly` 连同本记录扫描 9 个当前变更候选，
`PORTABLE_PATH_CHECK_PASS files=9 hits=0`。未渲染 Mermaid、未执行文档命令或新增构建测试。
