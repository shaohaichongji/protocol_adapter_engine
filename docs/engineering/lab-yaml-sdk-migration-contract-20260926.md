# Lab YAML 公开 SDK 消费迁移

## 基线与边界

用户授权继续推进。总控已读取打包脚本差异与校验脚本，现场复算七包清单/长度/Hash/
组件声明均通过，读取九次消费通过日志；未重跑编译或消费。包身份仍为带 dirty 来源的
本地候选，不是正式发布。当前 main@4da54b9，保留全部未提交 Lab/组件/打包/文档。
人工体验延期至统一成品；本片不发新版 SDK，不提交推送，不替换旧部署。

## 目标与写入范围

《Lab应用推进》独占必要的 `tools/protocol_lab_ui/`（含 standalone CMake/README）、
`tests/protocol_lab_ui/`、新报告 `docs/engineering/lab-yaml-sdk-migration-validation-20260926.md`。
若 standalone 白名单准备在独立脚本中定义，可最小修改该 Lab 专用脚本，先报告精确路径；
禁止修改 SDK 打包脚本、Core、公开接口、第三方或全局构建默认值。
根 CMake 如需调整，仅限 YAML Lab 对公开目标的门禁，不改已完成 SDK 语义。

1. 仓库 Lab 与 standalone 均只消费 `pae/yaml_frontend.h`、`PAE::yaml_frontend`、
   `pae::yaml` 公开 owner/状态/值类型位置/只读 V0.1 限额。不直接 include 内部 frontend.h，
   不使用内部 Result、Limits、SourceEntry 或 fault 参数，不另写转换实现。
2. 保持原 YAML 源身份、生成 JSON 哈希、Host 重准备、文档 ID/revision、取消/重载/关闭
   生命周期与输入上限；精确/祖先近似/未映射如实呈现，失败无部分结果，语法错误无位置不猜。
   保持中文左右工作台、原 JSON 路径与执行规则。
3. standalone 增加显式默认 OFF 的 YAML 入口开关。ON 明确要求 installed SDK 的
   yaml_frontend 组件，OFF 接受 JSON-only SDK；无下载/仓库内前端回退。
   imported include/lib 路径限制在指定 SDK 根，配置/CRT匹配与同包 DLL 验证保留。
4. 测试与必要示例纳入 Lab 自有闭包，不让 standalone 测试依赖 spikes 或 PAE src。
   只新增 YAML 配置到本片隔离候选，不能覆盖现用配置和部署。

## 验证及交付

独占新根 `out/build/lab-yaml-sdk-migration-20260926`、
`out/evidence/lab-yaml-sdk-migration-20260926`；采用仓库有效 v142/Qt，保护本机环境。
SDK 输入为 `out/build/yaml-sdk-packaging-20260926` 下已校验的
`static-debug-final-package`、`static-release-final-package`、
`shared-debug-final-package`、`shared-release-final-package`；JSON-only 使用相应
`json-only-static-final-package`、`json-only-shared-final-package`。不可修改这些包。

- 仓库构建 D/R 定向回归 YAML worker/UI/queue/session/dispatch 与原 JSON 行为。
- standalone static/shared D/R 构建及同组定向自动测试，覆盖真实 YAML 编译、Codec、
  Host 生成 JSON 复用、来源位置和旧 completion 隔离；既有测试驱动若不适用于
  standalone，可提供等价专项目标，不削弱断言。
- standalone YAML OFF + JSON-only Release 正例及 ON + 缺组件的明确负例；
  检查生成工程无内部前端路径，shared/Qt DLL 来源与同包 Hash；静态 YAML 无新 DLL。
- 生成新隔离可运行 Release Lab 候选及一份简单 YAML 示例，不启动可见窗口。
  候选构建身份、SDK身份与日志分开记录，不标为当前 HEAD clean 成品。

无变化全仓矩阵不重复；发现超范围缺口先停报。完成向总控
`01a04601-757d-7bb1-8254-61dde4954d74` 主动交接一次文件、命令/结果/证据、
未验证/风险、Git状态并停止写入。PAE和工程整理不并发修改；总控独占计划索引。
