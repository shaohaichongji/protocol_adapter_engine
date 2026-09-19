# Lab UI U1 中文常用界面与稳定测试定位验证

## 1. 范围与基线

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 现场基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`
- 本片仅修改 `application_window`、`document_tab`、`field_table_model`、
  `exact_value_delegate`、`hex_view` 及本报告。
- 共享工作树中的总控文档、G1 Binary Encode、Session/Adapter 与既有未跟踪报告均保留；
  本片未修改 PAE public API、Schema、root CMake 或执行语义。

## 2. 实现结论

1. 主窗口、文件菜单、配置入口、模式与主操作、Host 草稿操作、字段表头、流状态、
   常用 Tooltip、确认提示和状态摘要改为中文；固定术语采用“处理管线（Pipeline）”、
   “流编号（Flow）”、“解析”和“组包”。
2. 配置提供的 `display_name` / `description`、全部 ID、Hex、`ASCII escaped`、状态枚举、
   原始数据和机器 smoke 日志保持原样。
3. 诊断显示为“中文摘要 + 稳定错误码 + 原技术 detail”，没有解析英文 detail 猜测状态。
4. 补齐窗口、Tab、路径、模式、Pipeline、Message、输入表示、主操作、结果、诊断、字段表、
   Hex 等稳定 `objectName`；模式、表示、Host 动作继续提供稳定 `itemData`。
5. `FieldTableModel` 增加 `ActionReferencedRole` 与 `PresentationActionRole`，ASCII action/source
   smoke 改用稳定 role，不再按英文可见文案定位；U1 smoke 同时断言控件锚点以及诊断中文摘要、
   稳定码和原 detail 共存。

## 3. 构建与自动验证

隔离构建根为 `out/build/windows-msvc-lab-ui-u1/v142`。最初直接在 U1 根配置时触发项目工具链
门禁（默认 `v145`，项目要求 `v142,version=14.29.30133`）；因本轮无删除授权，保留该失败缓存，
在 `v142/` 子目录重新配置并成功构建。

Debug/Release 均构建以下目标成功：

- `pae_protocol_lab_ui`
- `pae_protocol_lab_ui_ascii_smoke_editor_tests`
- `pae_protocol_lab_ui_bounded_v08_state_tests`
- `pae_protocol_lab_ui_binary_public_h2_tests`

Debug/Release 定向 CTest 均为 `3/3 PASS`：

- `pae.tools.protocol_lab_ui.ascii_smoke_editor`
- `pae.tools.protocol_lab_ui.bounded_v08_state`
- `pae.tools.protocol_lab_ui.binary_public_h2`

Debug/Release 均从各自隔离部署目录以隐藏窗口运行：

- Binary v0.5 + v0.6 UI smoke：`UI_SMOKE_PASS detail=2 document(s)`；
- ASCII stream UI smoke：`UI_SMOKE_PASS detail=1 document(s)`。

Release 构建日志明确记录测试目标以 `/UNDEBUG` 覆盖 `/DNDEBUG`，断言未关闭。终态证据位于：

- `out/validation/lab-ui-localization-u1/debug-build-final3.log`
- `out/validation/lab-ui-localization-u1/release-build.log`
- `out/validation/lab-ui-localization-u1/debug-headless-final.log`
- `out/validation/lab-ui-localization-u1/release-headless-final.log`
- `out/validation/lab-ui-localization-u1/{debug,release}-{binary,stream}-final.stdout.log`

## 4. 限制与未验证范围

- 未进行人工 UI 观察、DPI/缩放或真实用户配置验收；没有启动供用户交互的可见 Lab。
- 直接让 CTest 启动裸 `bin/<Config>` EXE 时，因该目录不含部署的 Qt DLL/plugin，Windows
  缺依赖对话导致 timeout；隔离部署目录中的同一 EXE/DLL/plugin 集合可正常运行。没有改全局 PATH。
- 以 `Start-Process -WindowStyle Hidden` 运行既有 ASCII complete-record 与 v0.8 GUI smoke 时，
  其依赖焦点切换的编辑提交步骤失败；对应 `ascii_smoke_editor` 和 `bounded_v08_state` D/R
  专项均通过。本片没有为隐藏窗口改变焦点/编辑语义，也未把这两项声明为 GUI smoke 通过。
- 未运行全仓测试、未重打 SDK 五包、未覆盖 `deliverables/lab/98df5e0` 或其他既有部署。
- 未执行 Stage、Commit、Push、发布或删除。

结论：U1 已完成授权范围并具备 D/R 构建、定向 headless/编辑器专项、Binary 与 stream 隐藏式
UI smoke 证据；待总控复核。U2 未提前实施。
