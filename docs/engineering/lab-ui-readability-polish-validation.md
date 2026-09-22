# Lab 工作台可读性短收尾验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 范围与基线

- 仓库：`<REPO_ROOT>`
- 现场基线：`main@19c524758190dffd2395c87dd10fbe3e8e2d053d`
- 本片只修改 `tools/protocol_lab_ui/document_tab.cpp`、
  `tools/protocol_lab_ui/field_table_model.cpp` 与本报告。
- 保留工作树中既有 G1/U1/U2/U2B、总控与 SDK 相关变更；未修改 Session、Adapter、PAE API、
  Schema、根 CMake、旧构建或旧部署。

## 2. 实现结论

1. 字段详情中的固定展示标签和固定解释改为中文；配置提供的英文 `display_name`、`description`、
   `source_ref`、ID 及物理位置技术文本保持原文，不解析英文详情推断状态。
2. `类型` 列默认宽度改为至少 140 像素，并受当前字体下 `DECIMAL64` 实测宽度加 24 像素留白约束；
   smoke 直接校验该约束。
3. 字段详情允许鼠标及键盘选择；配置多行说明按原换行展示。表格 tooltip 补充当前单元格完整内容，
   `值` 列只读时补充中文“只读原因”，同时保留配置说明和原始只读注释。
4. Binary Host smoke 使用 `Temperature` 与 `Marker` 建立独立锚点：确认 `DECIMAL64`、中文固定详情标题、
   中文只读原因、英文配置说明原文及物理位置标签同时成立。

## 3. 构建与自动验证

隔离构建根：`out/build/windows-msvc-lab-ui-polish`。配置使用 Visual Studio 18 2026、x64、
`v142,version=14.29.30133` 与仓内 Qt 5.13。

Debug/Release 均成功构建：

- `pae_protocol_lab_ui`
- `pae_protocol_lab_ui_bounded_v08_state_tests`
- `pae_protocol_lab_ui_binary_public_h2_tests`

最终增量构建证据：

- `out/validation/lab-ui-readability-polish/debug-build-final2.log`
- `out/validation/lab-ui-readability-polish/release-build-final2.log`

Debug/Release 定向 CTest 均为 `2/2 PASS`：

- `pae.tools.protocol_lab_ui.bounded_v08_state`
- `pae.tools.protocol_lab_ui.binary_public_h2`

证据：`out/validation/lab-ui-readability-polish/{debug,release}-ctest.log`。Release 两个专项目标仍以
`/UNDEBUG` 覆盖 `/DNDEBUG`，断言未被关闭。

Debug/Release 从各自新部署目录隐藏执行 Binary Host 0.9 smoke，均输出
`UI_BINARY_HOST_SMOKE_DOCUMENT ... fields=9` 与 `UI_SMOKE_PASS`。最终证据：

- `out/validation/lab-ui-readability-polish/debug-binary-host-snapshot-final.stdout.log`
- `out/validation/lab-ui-readability-polish/release-binary-host-snapshot-final.stdout.log`

首轮 smoke 曾按新增门禁失败，暴露固定 116 像素在运行时字体下小于 127 像素要求；失败日志保留为
`diagnostic.stderr.log`。修正为 140 像素与动态字体下限后才取得上述 PASS，未把失败结果写成通过证据。

## 4. 隐藏渲染与目视检查

- `out/validation/lab-ui-readability-polish/readability-1280x820.png`：日志记录
  `requested=1280x820 actual=1280x820`。
- `out/validation/lab-ui-readability-polish/readability-1920x1080.png`：日志记录
  `requested=1920x1080 actual=1920x1080`。

目视检查确认：

- 两种尺寸下 `DECIMAL64` 均完整显示；1280 宽度通过字段表自身横向滚动访问尾部技术列，1920 宽度
  可同时看到完整字段结果列。
- 选中的 `Temperature` 详情中，`报文`、`报文来源`、`字段`、`来源`、`转换`、`字节范围`、
  `物理字节 / 位 / 掩码` 为中文固定标签；`Typed record`、英文配置说明和技术位置原文未被改写。
- 长物理位置文本在详情区可滚动；详情控件的鼠标/键盘可选择属性由 smoke 断言覆盖。

用户先前已确认 U2B Binary 九字段/详情及左右布局符合使用习惯；本片仅做其后的可读性短收尾。
上述截图为隐藏 Qt 渲染和本轮目视复核，不冒充新的完整人工交互验收。

## 5. 未验证范围与状态

- 未启动可见 Lab，未操作用户进程，未进行真实鼠标复制、键盘选择、150% DPI、多显示器或系统字体
  人工验收。
- 未运行 UI 全矩阵、全仓测试、SDK/包外消费、硬件或现场验证；未重打 SDK 五包。
- 未执行 Stage、Commit、Push、发布或删除。

结论：已完成本次工作台可读性短收尾的实现、D/R 定向构建、D/R 两项专项、D/R Binary Host
隐藏 smoke、两种严格尺寸截图及目视检查；已停止写入，待总控复核。
