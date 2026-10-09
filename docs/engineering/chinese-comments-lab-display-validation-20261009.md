# 第十六批 Lab 展示链路中文注释验证

## 当前状态：已完成派发范围，待总控复核

2026-10-09，接管基线为 `main@0e40741cebb283bf2a3438b940584eeaf68606d9`。
已完成六源码的限定注释、静态检查及 Debug/Release 五项限定验证，各 5/5 PASS。
本批先按同步要求完成静态检查并停止等待，第十五批结束验证后才由总控正式放行。
随后确认 ready Hash 与构建占用再串行构建、测试；没有并发占用或扩大配置矩阵。
本结论不是总控验收、人工 UI、Linux、全仓或发布批准。

仅六源码新增注释，并新增本报告：

- `tools/protocol_lab_ui/description_mapping.h`
- `tools/protocol_lab_ui/description_mapping.cpp`
- `tools/protocol_lab_ui/field_table_model.h`
- `tools/protocol_lab_ui/field_table_model.cpp`
- `tools/protocol_lab_ui/document_tab.cpp`
- `tools/protocol_lab_ui/hex_view.cpp`

保留全部正确英文；未改非注释 token、字面量、声明布局、预处理/宏续行、接口、执行行为、
测试、CMake 或依赖。不补 UI 搭建，不重复前批生命周期/H2 注释，不增加 stream 专属范围。
无 Stage/Commit/Push、删除、重打包、外部网络访问、全局环境或本机 Qt 修改。

## 注释语义与依据

| 文件 | 本批限定说明 |
| --- | --- |
| `description_mapping.h/.cpp` | 兼容 Plan/legacy ASCII 与公开 ASCII 入口受宏隔离；描述自有复制，不执行 Codec。容器数值位按执行字节序映射到物理字节，掩码按同字节合并；显示 byte、LSB0 和 global_bits 的区别。bounded payload 按实际帧长减固定头尾并检查边界，合法空载荷不是错误；动态尾校验存储位置随实际载荷移动。缺当前范围只显示边界，不高亮最大占位。 |
| `field_table_model.h/.cpp` | 模型借用描述和结果容器，自有行编辑状态；Reset 清旧结果/失败/实际长度。结果按 field_index 与稳定 id 双重匹配，不按向量同行位置。VALUE 是草稿或只读说明，RAW/LOGICAL 直接展示已有结果；物理位置优先 result.actual_range，ASCII 不猜 token 位置。编辑解析及上层提交通过后才发布合法行值，拒绝保留可恢复输入与错误。 |
| `document_tab.cpp` | 描述高亮与实际结果高亮的不同来源；Inspect 成功借结果给模型，失败只展示诊断帧，未知 Message 不借当前选择。没有有效 preview 清结果、帧长和字节而不清草稿；详情取 DisplayedMessage，refresh_frame=false 防止递归重绘。实际布局只取成功帧长，失败定位不冒用有效布局。 |
| `hex_view.cpp` | 帧和掩码是自有复制；每帧从零重建，同字节 OR，越界标记忽略。背景是整 byte 单元格，tooltip 保留位掩码；清内容仍保留 capacity。计费是两个 byte 容器容量，不是完整 Qt 对象或 RSS。 |

`ResolveActualFieldRange` 只解析匹配的 bounded payload；其他字段直接返回描述范围，
不宣称它能推导任意公开动态布局。公开 Binary/ASCII 的实际结果范围仍由上层投影选用。
Binary 公开描述构造已在前批 `public_binary_description` 注释，本批不重复。
未确认需要新增授权修复的代码缺陷，也未据本批阅读宣称全面行为或安全审计。

## 接管、并行排除与保护

独立证据根：`out/build/comments-public-20261009/lab-display/`，本批新建，不覆盖旧日志。

- `baseline/`、`targets-before.csv`：六源码接管原字节副本及 SHA256。
- `protected-before.csv`：3243 个非目标、非并行排除的已跟踪文件 SHA256，包括既有 Decimal 两源码。
- `untracked-before.csv`：既有 `chinese-comments-decimal-conversion-validation-20261009.md` SHA256。
- `parallel-exclusions.csv`：明确排除第十五批 `src/config_compiler/protocol_metadata.h/.cpp` 与
  `docs/engineering/chinese-comments-metadata-validation-20261009.md` 的变化；记录初始 Hash/是否存在。
  本批不写这些文件，也不以本批保护校验宣称它们保持不变；其授权变化不当作污染。
- `status-before.log`：接管时仅 Decimal 两源码和报告未提交，暂存区为空。

保护检查通过：3243 个受保护已跟踪文件及既有 Decimal 报告 Hash 未变。
范围检查只允许本批七文件、既有 Decimal 三文件和已列明的第十五批三文件。

## 已执行静态检查

从仓库根执行，命令和证据路径均相对仓库：

```powershell
& ./out/build/comments-public-20261009/lab-display/verify.ps1
& ./out/build/comments-public-20261009/lab-display/compare-format.ps1
& ./out/build/comments-public-20261009/lab-display/verify.ps1 -Snapshot ready
```

前两项退出 0，日志为 `verify-initial.log`、`format-comparison.log`。
最后一项退出 0；就绪快照证据为 `verify-ready.log`、`files-ready.csv`、`status-ready.log`。
放行前七文件 ready Hash 再核对一致；构建后六源码仍与 ready Hash 一致，
仅按授权补充本报告的验证结果。最终 `verify.ps1 -Snapshot final` 证据为
`verify-final.log`、`files-final.csv`、`status-final.log`。
校验器保留普通/带前缀字符串、字符及带 delimiter 的 raw string；行拼接先于 token 比较。
六文件非注释 token/字面量序列、预处理指令、宏续行原行及非注释完整代码行布局一致。
另检查新增行注释没有尾部反斜杠，严格 UTF-8 无 BOM（含报告）、Hash 保护、范围、
目标及全工作树 `git diff --check`、空暂存区。以上是静态检查，不是构建或运行证明。

六文件各自执行修改前和修改后的 `clang-format --dry-run --Werror`，指定仓库 `.clang-format`，
未执行格式化写入。`format-{baseline,current}-*.log` 保存诊断，两个 summary CSV 保存退出码及数量；
按诊断源行文本及列号逐项比较，序列一致，没有新增格式差异。

| 文件 | 修改前/后诊断数 | 两次退出码 |
| --- | ---: | --- |
| `description_mapping.h` | 0 / 0 | 0 / 0 |
| `description_mapping.cpp` | 19 / 19 | 1 / 1 |
| `field_table_model.h` | 0 / 0 | 0 / 0 |
| `field_table_model.cpp` | 13 / 13 | 1 / 1 |
| `document_tab.cpp` | 217 / 217 | 1 / 1 |
| `hex_view.cpp` | 0 / 0 | 0 / 0 |

保留原有共 249 处格式诊断，不称全量格式 PASS，不顺手重排。

## 已完成的限定运行验证

只读核对 `tests/protocol_lab_ui/CMakeLists.txt` 的真实目标与五项注册，按正式放行运行：

- headless：`pae.tools.protocol_lab_ui.description_mapping`、`pae.tools.protocol_lab_ui.bounded_v08_state`。
- Qt smoke：`pae.tools.protocol_lab_ui.qt_smoke_v08`、`pae.tools.protocol_lab_ui.qt_smoke_ascii_one_way`、
  `pae.tools.protocol_lab_ui.qt_smoke_binary_stage1`。

复用 `out/build/comments-public-20261009/lab-lifecycle/build`，源根为本仓库，
UI、Binary UI/H2、ASCII A2、Testing 已启用，MSVC v142 `14.29.30133`，使用随仓 Qt。
没有重新配置、修改开关或扩大矩阵。放行核对时没有活动 cmake/ctest/cl/ninja/Lab 主进程；
一个 MSBuild 是 `/nodemode:1 /nodeReuse:true` 节点，父进程已退出，未终止它。
记录为 `build-occupancy-release-gate.log`，放行保护检查为 `verify-release-gate.log`。

`test-inventory-Debug.json`、`test-inventory-Release.json` 各恰好五项；三项 Qt smoke
既有注册使用 `QT_QPA_PLATFORM=windows`，没有临时换 offscreen 或修改本机环境。
预检时两个 headless exe 尚未构建，JSON 无已解析 command；首次辅助摘要直接索引空 command
产生 `Cannot index into a null array` 提示，非 CTest 运行失败。随后按未生成状态输出
`test-inventory-summary.log`，保留原 JSON，不将预检当作 PASS。

从仓库根实际串行执行 `run.ps1 -Phase Debug`，完成后再执行 `run.ps1 -Phase Release`。
两阶段封装的真实命令如下，Release 将 Debug 配置名换为 Release：

```powershell
cmake --build out/build/comments-public-20261009/lab-lifecycle/build --config Debug --target pae_protocol_lab_ui pae_protocol_lab_ui_description_mapping_tests pae_protocol_lab_ui_bounded_v08_state_tests --parallel 4
ctest --test-dir out/build/comments-public-20261009/lab-lifecycle/build -C Debug -R '^pae\.tools\.protocol_lab_ui\.(description_mapping|bounded_v08_state|qt_smoke_v08|qt_smoke_ascii_one_way|qt_smoke_binary_stage1)$' --no-tests=error --output-on-failure -V -j 1
```

| 配置 | 构建退出 | CTest 退出 | 两 headless + 三 Qt smoke | Qt 成功标记 |
| --- | ---: | ---: | --- | ---: |
| Debug | 0 | 0 | 5/5 PASS | 3 条 UI_SMOKE_PASS |
| Release | 0 | 0 | 5/5 PASS | 3 条 UI_SMOKE_PASS |

原始命令、标准输出/错误与退出记录见 `build-Debug.log`、`test-Debug.log`、
`build-Release.log`、`test-Release.log`，摘要为 `result-summary.log`。
Qt smoke 是实际 QApplication/qwindows 展示消费验证，不是仅 headless，也不是人工 UI 验收。
v08 日志含编辑提交 model/output accepted 及帧/高亮观察；两配置全部烟测均正常退出。
没有新增或屏蔽测试、启动额外 stream/performance/CRT probe 或运行全仓。

Release 两项 headless 项目均有 NDEBUG undefine，源/执行编码分列 UTF-8；Lab 同样保持
既有分列 UTF-8，见 `release-options.log`。headless 使用 assert 的设置与检查不被 NDEBUG 关闭。
Qt smoke 以检查结果和退出状态判断，不把 app 的 Release 宏状态当作 headless 断言证明。
Debug warning 行数 0；Release 有两条 D9025（/UNDEBUG 覆盖 /DNDEBUG），两配置 C4819 均为 0。
未屏蔽警告，不宣称 Release 零警告。

## 未验证范围与停点

description_mapping 测试在 public legacy 开关下经 Session 公开路径取描述，不能据此宣称
私有 Plan 的 BuildPhysicalMapping 所有分支都已运行覆盖。

未做人工 UI、Linux、全仓、包外、SDK/体验包重制、其他开关矩阵、硬件或真实协议现场。
必要依赖闭包包含第十五批/Decimal 和兼容模块，不代表本批修改或扩大这些模块验收。
范围外保护仍排除第十五批明确授权的三候选；其余受保护文件及既有 Decimal 报告 Hash 未变。
六源码新增 42 行注释，测试阶段没有再次改源码；原 249 处格式诊断保留。
完成后发送一次完整交接，停止写入等待总控复核；未 Stage/Commit/Push、删除或发布。
