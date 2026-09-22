# Lab 结构化编译诊断验证（2026-09-21）

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 结论与边界

本片在 `main@fe1683c9d540d7f5180cad1d0368cef4ccf1136d` 上完成 Lab 结构化编译诊断接入，未修改 PAE 公共 API、Schema、Core、Host、stream 执行语义或根 CMake。编译失败现在保留原有 `diagnostic_id` / 原始 detail 决策，同时经 Lab 自有纯 C++ DTO 将下列字段传到当前文档的诊断页：

- `stage`、`code`、`json_pointer`；
- 可区分“未提供”和 `0` 的 `byte_offset`；
- `resource_kind`、`required_bytes`、`limit_bytes`、`resource_profile`；
- 原样 plain-text 展示的 `detail`。

空 `json_pointer` 显示为“根（空 pointer）”；`ResourceKind::NONE` 显示“无资源预算数据”，不伪造预算；数值全程使用整数类型，没有转为 `double`。诊断控件强制 `Qt::PlainText`，长文本及 HTML-like 字符不按富文本解释。

状态边界保持为当前 `DocumentId + load_revision`：`BeginLoad`、成功结果、普通非编译失败和关闭都会清除结构化编译诊断；过期 completion 在任何清理前被拒绝；Tab 间不共享诊断。既有 runtime 诊断仍沿用“操作失败”路径。

## 2. 实际变更

- `tools/protocol_lab_ui/compile_worker.h/.cpp`
  - 新增 `CompileDiagnosticView`、公开/私有诊断显式枚举 token 投影及中文格式化。
  - public legacy、Binary、ASCII 与 private compatibility 编译失败均在 worker completion 中携带结构化副本。
- `tools/protocol_lab_ui/document_session.h/.cpp`
  - 保存当前 revision 的结构化编译诊断，并在加载、成功、普通失败、关闭和 stale completion 场景维持隔离/清理语义。
- `tools/protocol_lab_ui/document_tab.h/.cpp`
  - 复用既有诊断页展示中文标签和稳定 raw token；detail 固定 plain text。
- `tests/protocol_lab_ui/compile_queue_tests.cpp`
  - 独立断言 root/non-root、`nullopt`/`0`/非零 offset、NONE/真实资源、`required=0`、64 位上界、长 detail 和特殊字符；覆盖 public 三类真实编译失败及 private compiler 真实类型投影。
- `tests/protocol_lab_ui/document_state_tests.cpp`
  - 覆盖 current/stale、BeginLoad/success/noncompile/close 清理及跨文档隔离。
- `tests/protocol_lab_ui/binary_stream_qt_smoke_tests.cpp`
  - 通过真实 worker→session→tab 编译失败和手工边界诊断验证 UI 字段、plain text、长文本、offset 0、资源 0 值及 Tab 隔离；原 Flow/stream/cancellation smoke 保留。

未新增或修改本片 CMake/standalone 白名单；既有 whitelist glob 已包含相关测试和源文件。

## 3. 仓内 common 验证

构建根：

`out/build/windows-msvc-lab-compile-diagnostics-20260921`

最终源码完成最小差异收敛后，Debug、Release 均构建以下目标成功：

- `pae_protocol_lab_ui_compile_queue_tests`
- `pae_protocol_lab_ui_document_state_tests`
- `pae_protocol_lab_ui_binary_stream_qt_smoke_tests`
- `pae_protocol_lab_ui`

最终定向结果：

| 配置 | 结果 |
| --- | --- |
| Debug | 3/3 PASS |
| Release | 3/3 PASS |

证据：

- `out/validation/lab-structured-compile-diagnostics-20260921/common-debug-verified-build.log`
- `out/validation/lab-structured-compile-diagnostics-20260921/common-debug-verified-tests.log`
- `out/validation/lab-structured-compile-diagnostics-20260921/common-release-verified-build.log`
- `out/validation/lab-structured-compile-diagnostics-20260921/common-release-verified-tests.log`

Release 构建日志对三项测试均明确记录 `/DNDEBUG` 被 `/UNDEBUG` 覆盖，断言没有因 Release 配置失效。

保留的过程证据：首次 Debug 测试中 `compile_queue` 失败，定位并修正了测试中的无效 private `0.4` 路由假设；初次构建还暴露并修正了测试命名空间限定。相关旧日志均保留在同一 validation 根，不作为最终 PASS 证据。格式噪声收敛后又完整重建并重跑上述最终矩阵。

## 4. installed-SDK 包外验证

最终逐字输入快照：

- static：`<TRIAL_ROOT>\lab-compile-diagnostics\static-verified`
- shared：`<TRIAL_ROOT>\lab-compile-diagnostics\shared-verified`

两份 snapshot 与仓库当前九个本片源码/测试文件现场 SHA-256 比对均为 `mismatch_count=0`。消费的 Debug/Release static/shared SDK 均来自固定 SDK 根，其 `PROVENANCE.json` 记录 `source_head=7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`、`source_worktree_dirty=false`；本轮未重打 SDK 五包。

定向结果：

| SDK 形态 | Debug | Release |
| --- | --- | --- |
| static | 3/3 PASS | 3/3 PASS |
| shared | 3/3 PASS | 3/3 PASS |

最终日志位于各 snapshot 的 `logs/verified-{debug,release}-{configure,build,tests}.log`。两组 Release build 均记录 `/UNDEBUG` 覆盖 `/DNDEBUG`。shared 仍出现既有 C4251 警告；这不阻止本次同工具链定向测试，也不升级为稳定 ABI 结论。

额外构建了 Testing-off static Release 产品闭包，未启动可见窗口：

`<TRIAL_ROOT>\lab-compile-diagnostics\static-verified\deploy\release-product\Release\pae_protocol_lab_ui.exe`

该部署包含 18 个文件；`configs/synthetic_ascii_stream_slice.pae.json` 存在，SHA-256 为 `5F6B871DFBFC099070EE8FB9881F02C8671496C61BFC028CC24DB332E7E8BE4C`。产品清单见 `lab-compile-diagnostics/evidence/verified-release-product.json`。

## 5. 子任务交接时的未验证与 Git 状态

- 未执行可见 UI 人工体验、全仓测试、全量 SDK consumer 矩阵、SDK 重打包、发布或旧部署替换。
- 未把静态/包外 smoke 结果扩大为人工可用性验收或正式发布结论。
- 共享树原有总控文档与 stream fixture packaging 变更均保留；本片未改写其内容。
- Stage 为空；未 Commit、未 Push、未发布、未删除仓库或既有证据。

当前状态为：已完成派发范围，待总控复核；本任务停止继续写入。

## 6. 总控复核与人工短体验收口

2026-09-21：总控抽查代码链路并核对六组最终定向测试成功日志，未自行重跑测试。用户使用第4节static Release入口加载独立的 `manual-compile-error.pae.json`，反馈“操作完成并已关闭”，并提供截图。

截图确认诊断页显示：`UI_PUBLIC_BINARY_COMPILE_FAILED`、阶段 `STRUCTURAL`、错误码 `MISSING_PROPERTY`、JSON位置 `/protocol_id`、字节偏移“未提供”、资源类型 `NONE`、需要/限制“无资源预算数据”、资源配置 `DESKTOP`，以及原始技术详情 `required property is missing`。该不完整配置的编译失败符合预期，中文标签和分项展示可见；Lab关闭状态按用户确认记录。

本次限定人工短体验完成，不要求重复复杂验收。截图不单独证明复制操作、长文本交互、全部错误类型或真实协议验证；相应自动测试边界保持原记录。下一步统一入口与提交候选收尾，尚未Stage/Commit/Push，不作正式发布声明。
