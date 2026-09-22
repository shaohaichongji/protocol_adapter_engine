# 公共仓库文档路径可移植性治理验证

日期：2026-09-22。状态：**已完成派发范围，待总控复核**。

本记录只覆盖已跟踪 `docs` 中含机器绝对路径的 Markdown/JSON，以及 `third_party/qt-package.md` 文本。未修改其他任务拥有的 `portable-*.md`、`deliverables/README.md`、examples/tools README、scripts、Qt 副本、清单或二进制；未构建、运行 UI、删除目录、Stage、Commit、Push 或发布。

## 1. 修改前原始证据

修改前将 75 个命中文件逐字复制到 Git 忽略的 `deliverables/evidence/portable-paths/documents/originals/`，并在同目录 `manifest.json` 中记录相对路径、长度和 SHA-256。逐项复核原始副本与清单 Hash 一致，差异数为 0；最终清单自身 SHA-256 为：

`2C1F2D86D96F8EC1F6BB6A1578ABCD7F898F0BBCB1EE0CDA1BFEDABD9F322852`

该目录是保真原始证据，不属于公开仓库内容；公开文档中的别名不能替代原文取证。

## 2. 治理方式

- 66 份历史/验证 Markdown 使用有序语义别名，例如 `<REPO_ROOT>`、`<PROJECT_WORKSPACE_ROOT>`、`<TRIAL_ROOT>`、`<TOOLCHAIN_ROOT>` 和 `<LOCAL_WORK_ROOT>`；每份文档均增加路径可移植性说明，明确别名描述历史或本地位置，不是可直接执行的当前命令。
- `docs/guides/02-首次运行与Lab体验.md`、`03-Windows-SDK集成.md`、`04-协议配置入门.md`、`06-构建测试与问题定位.md` 与 `docs/sdk/README.md` 单独审查命令语义。仓库指南从仓库根执行，SDK 使用 `deliverables/sdk/7b4205e`，首选 Lab 使用 `deliverables/lab/fe1683c-plus-patches/static-release`；均说明这些本地产物不随 clone 获得，缺失时应从构建/standalone 入口生成。
- 三份 2026-09-14 清理 JSON 从 Git 忽略的原始副本派生。公开副本仅替换 JSON string 中的机器路径，并增加 `portable_derivation.kind=redacted-derived-copy`、原始证据相对路径、原始 SHA-256、别名说明和验证边界。历史 Hash、计数、字节数和长度值保持原值；没有把路径脱敏表述为重新验证当前磁盘。
- `third_party/qt-package.md` 仅移除本地来源路径和故意缺失根的机器写法；保留 Qt 5.13.0 快照身份、文件数/字节/Hash 事实以及许可和来源材料尚未闭合的边界。

三份派生 JSON：

- `docs/archive/generated-artifact-cleanup-execution-20260914.json`
- `docs/archive/generated-artifact-cleanup-manifest-20260914.json`
- `docs/archive/generated-artifact-evidence-index-20260914.json`

## 3. 验证结果

- 范围扫描：排除其他任务拥有的 `docs/engineering/portable-*.md` 后，已跟踪 `docs` Markdown/JSON 与 `third_party/qt-package.md` 的真实盘符路径命中为 0；HTTP/HTTPS URL 保持不变。
- JSON：三份公开派生副本均可由 PowerShell JSON parser 读取；从原始证据递归比较名称含 Hash、count、bytes、length 或 total 的 1810 个指标字段，差异为 0。
- Markdown 链接：去除 fenced code 后解析全部已跟踪 `docs` Markdown 的本地链接，缺失目标为 0；没有生成指向语义别名的 Markdown 链接。
- PowerShell：当前指南和 SDK 指南共 10 个 `powershell` fenced code block 均通过 PowerShell parser，语法错误为 0。历史文档中的别名命令已明确标为描述性记录，不冒充可直接执行模板。
- 当前本机核对时，`deliverables/sdk/7b4205e` 与 `deliverables/lab/fe1683c-plus-patches/static-release` 均存在；这只是本机存在性检查，不代表 clone、重建、运行或发布验证。
- 最终执行 `git diff --check`；结果见本轮交接，不以静态文档检查替代构建或运行验证。

## 4. 未验证与边界

未执行 SDK/Lab 构建、测试或 UI；未重新验证历史外部目录、清理结果、旧 Hash 的来源真实性或当前可访问性；未审查或选择 Qt 许可证，也未下载依赖。三份 JSON 是公开脱敏派生版本，只有 Git 忽略目录中的副本保持逐字原文。

本片完成后停止写入，等待总控复核。
