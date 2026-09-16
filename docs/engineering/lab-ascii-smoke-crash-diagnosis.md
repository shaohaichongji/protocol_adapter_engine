# H2 后置 ASCII `qt_smoke_ascii` 偶发崩溃定向诊断

日期：2026-09-15。状态：原 H2 崩溃根因未定；独立诊断捕获的 ASCII 烟测编辑器焦点定位普通失败已有限定烟测路径修复，待总控复核。以下第 1～3 节为先前只读诊断记录，第 4 节为独立诊断，第 5 节为后续限定烟测修复。不覆盖已验收 H2 部署。

## 1. 已观察与未观察

- 历史 Release 失败：`out/stage4-h2-validation/flow-fix-release-ctest.log` 中 `pae.tools.protocol_lab_ui.qt_smoke_ascii` 于 2026-09-15 15:53:38 记录 `***Exception: SegFault`，其余 22 项通过。原 CTest 参数为随仓 H2 最终构建的 Release `out/build/windows-msvc-stage4-h2/out/protocol_lab_ui/Release/pae_protocol_lab_ui.exe --ui-smoke`，后接 `examples/config/synthetic_ascii_text_slice.pae.json`、`synthetic_ascii_literal_only.pae.json` 两路径；CTest 工作目录为 `out/build/windows-msvc-stage4-h2/tests/protocol_lab_ui`。当时同目录 EXE 最后写入 15:53:24，现 SHA256 `3D3F13C6A16A63E70B28463DEED395A7A7834B8D642427A3F85A91F888D0A240`，本轮未重建。
- 原有 Windows Application Error 1000 与 CTest 时间吻合：15:53:36，进程 `pae_protocol_lab_ui.exe`，故障模块是该目录随仓 `Qt5Core.dll` 5.13.0，异常码 `0xc0000005`（访问违例），模块偏移 `0x1cd1bc`，报告 ID `85526822-1c9e-4393-87b4-82d302c101aa`。证据 `out/stage4-ascii-crash-diagnosis/original-windows-application-error.txt`；Windows Error Reporting 1001 签名见同目录 `original-windows-wer.txt`。原 WER Archive 仅有 `Report.wer`，无 `.dmp/.mdmp` 栈。故障模块位置并不能证明 Qt 自身是根因；也不能由偏移单独确定调用者。
- 更早的 H2 Debug 定向总套曾输出 `UI_SMOKE_FAIL detail=ASCII document 1: failed to reopen ASCII table editor`，其后单项/总套复跑通过。该原始失败控制台输出未保留为独立文件；对应检查在 `tools/protocol_lab_ui/document_tab.cpp:1045-1051`。这是普通烟测检查失败，不能与上述访问违例预设为同根因。
- 本轮按现有最终 H2 构建串行运行同一专项：Release 20/20、Debug 10/10，失败 0；每次都是两文档 `UI_SMOKE_PASS detail=2 document(s)`，日志分别为 `out/stage4-ascii-crash-diagnosis/release-run-01.log` 至 `release-run-20.log`、`debug-run-01.log` 至 `debug-run-10.log`，逐次退出码和 EXE SHA256 在 `run-index.txt`。Release EXE SHA256 如上；Debug EXE SHA256 `9265ACE5D5EA506C084597F9DF64C2104B44D01FACA5D3F539861023C6F242E9`。使用 `ctest --test-dir out/build/windows-msvc-stage4-h2 -C <Release|Debug> -R '^pae\.tools\.protocol_lab_ui\.qt_smoke_ascii$' -V --output-on-failure -O <新日志>`；CTest 测试属性自身 timeout 180 秒。连续通过不构成修复或稳定性证明。

## 2. 源码路径与推断边界

现有 `--ui-smoke` 在 `tools/protocol_lab_ui/application_window.cpp:195-217` 加载最多两 Tab；15 ms timer 在 `:140-147` 拉取 worker completion 并执行 `AdvanceSmoke`，后者在 `:279-350` 先对两个文档各做 `VerifyAsciiForSmoke`、再做 `VerifyHostForSmoke`。`FinishSmoke` 在 `:581-588` 通过下一次 Qt 事件退出，窗口析构/关闭在 `:154-176` 关闭 DocumentTab；`DocumentTab::CloseDocument` 在 `document_tab.cpp:339-363` 先关闭 worker 文档再清模型/Session，worker 析构在 `compile_worker.cpp:76-88` 停止并 join 线程。此路径存在异步回调和关闭交错的理论窗口，但没有栈或失败阶段标记，不能说本次崩溃发生于关闭。

更具体的编辑器候选窗口在 `document_tab.cpp:983-1071`：ASCII 烟测重开同一表格单元编辑器、粘贴超协议/超容量文本、键盘提交，再取 `QApplication::focusWidget()` 为裸 `QLineEdit*`；辅助函数 `:272-295` 以 `sendEvent` 发送按键并手动 `processEvents`/`sendPostedEvents`。编辑器提交/关闭、模型刷新与 DeferredDelete 可在事件处理中改变焦点/对象寿命；若裸指针在后续事件中失效，可能诱发访问违例。这是源码风险推断，不是已证实根因，也不能以 Qt5Core 为故障模块反推就是编辑器。较早 Debug 的“未能重开编辑器”只提示该阶段值得观察，不证明它与 Release 崩溃相同。`exact_value_delegate.cpp:53-190` 的 editor 创建、输入拒绝和 model commit 回调亦需在有栈后核对。

普通用户路径会使用同一表格编辑器和 Host/关闭边界，但这次历史崩溃只在合成的两文档、强制 `sendEvent`/手动事件泵烟测中观察；没有普通用户手动操作、现场或持续复现证据。因此不能宣称只影响测试，也不能宣称普通使用必然崩溃。

## 3. 取证限制、最小下一步与回归建议

本轮上限耗尽而未复现，无法取得现场栈。宿主上 `cdb/windbg` 未发现；现有 ProcDump 的帮助调用显示首次许可证提示，未进行接受或修改注册表/系统 WER 策略；原 WER 没有转储。本轮未生成、上传或复制进程转储。若总控要继续定位，需另行授权独立诊断构建/局部取证方式：优先在 **新诊断产物** 加烟测阶段标记（编辑器首次/重开/粘贴/提交、Host Apply、退出/关闭）和 `QPointer` 寿命观察，复现时再将异常码、线程栈与最后阶段关联，不碰 H2 已验收目录或改变生产行为。

目前没有证据支持直接提交某个“修复”。若栈证实编辑器销毁后继续使用，最小修复候选是仅在烟测事件辅助函数中用受监护指针，并在每次事件泵后核验/重新获取 editor，避免对已关闭对象再发送键事件；同时核对真实 UI 编辑/提交是否也有同类裸指针跨事件问题。若栈落在 worker completion、CloseDocument 或窗口析构，需改按对应生命周期契约重新界定范围，不能套用编辑器修复。回归至少保留两文档 ASCII 烟测的 Debug/Release 专项、多次串行受限运行，并覆盖编辑器重开/容量拒绝、Host Apply/取消和正常关闭；只有稳定复现的失败前断言及修复后回归，才能宣称修复有效。

本轮只新增本报告和 `out/stage4-ascii-crash-diagnosis/` 本地诊断日志；未改代码、已验收部署、Qt/全局环境，未 Stage、Commit、Push、发布或删除目录。ASCII 崩溃仍为独立未解决项，等待总控决定下一片授权。

## 4. 独立诊断构建与第 6 次 Release 失败（后续授权）

总控限定复核：已读取第6次原始日志、诊断 helper 与调用点，并独立复算已验收部署哈希未变。注意 seq16 记录的是 qobject_cast<QLineEdit*>(QApplication::focusWidget()) 的结果；空值只能证明没有取得焦点 QLineEdit，不能区分 focusWidget 本身为空或焦点在其他类型控件。seq14 创建的 editor1 直到 seq28 清理才销毁，本次未观察到该编辑器提前销毁；没有访问违例或 EditorLost 证据。下一步建议修正烟测定位目标编辑器的方式并增加失焦负例，保留历史访问违例为独立未解决项；尚未授权修复，不以本次普通失败解释历史崩溃。

本次仅在根 `CMakeLists.txt`、`tools/protocol_lab_ui/CMakeLists.txt`、`tools/protocol_lab_ui/application_window.cpp`、`document_tab.cpp`、`exact_value_delegate.cpp` 和新增 `ascii_smoke_diagnostic.h` 加入默认 `OFF` 的 `PAE_BUILD_PROTOCOL_LAB_ASCII_SMOKE_DIAGNOSTIC` 门控。门控要求 UI 与 Schema 0.10 ASCII 均已启用，且只给 UI 可执行文件定义诊断宏；不进入 PAE Core、Host、旧后端或普通构建。运行期仅 `--ui-smoke` 启用序号化阶段日志、编辑器稳定 ID、销毁观察和 `QPointer` 保护；发现编辑器在合成按键中失效即输出 `ASCII_DIAG_EDITOR_LOST` 并停止继续使用，不做重试、重开或行为修复。诊断日志使用 `ASCII_SMOKE_DIAG` 前缀并立即刷新。事件泵、焦点查询及烟测判定本身未改；新增观察可能改变时序，不能把诊断包等同已验收产物。

使用 H2 preset 另配 `out/build/windows-msvc-stage4-ascii-diagnostic`，显式打开诊断门控，独立完成 Debug 和 Release UI 构建；另配但未构建 `out/build/windows-msvc-stage4-ascii-default-off`，其 cache 为 `OFF`，UI 工程未定义诊断宏。诊断包 Release EXE SHA256 为 `1683F19F65D1D7B1FEF90930E33FB37DB49F103388D634DAD14C6638937AF99D`；已验收 H2 Release EXE SHA256 仍为 `3D3F13C6A16A63E70B28463DEED395A7A7834B8D642427A3F85A91F888D0A240`。诊断包以非烟测 `--ui-performance` 负例执行，按现有规则报“不支持 ASCII UI 性能烟测”，日志中没有 `ASCII_SMOKE_DIAG`；这是门控观察负例，不是性能验证。日志见 `out/stage4-ascii-crash-instrumented/non-smoke-performance.log`。

以 `ctest --test-dir out/build/windows-msvc-stage4-ascii-diagnostic -C Release -R '^pae\.tools\.protocol_lab_ui\.qt_smoke_ascii$' -V --output-on-failure -O out/stage4-ascii-crash-instrumented/release-run-NN.log` 串行执行：Release 第 1～5 次通过，第 6 次普通烟测失败（CTest 退出码 8），立即停止；Debug **0 次，未启动**，未借后续通过覆盖失败。索引和逐次日志见 `out/stage4-ascii-crash-instrumented/run-index.txt`、`release-run-01.log` 至 `release-run-06.log`。第 1 次完整通过日志到 `smoke_finish_pass`，三个首文档编辑器依次创建/销毁，两个文档 Host 验证也完成。

第 6 次最后关键顺序是 `seq=13 editor_first_before_edit` → `seq=14 editor_create editor=1` → `seq=15 editor_first_before_pump` → `seq=16 editor_first_after_pump object=0` → `UI_SMOKE_FAIL detail=ASCII document 1: ASCII table editor did not expose expected escaped capacity=36`。编辑器 1 的 `editor_destroy` 到 `seq=28` 窗口/文档清理时才出现，且此轮没有 `ASCII_DIAG_EDITOR_LOST`、没有 `***Exception: SegFault`。现有检查直接从 `QApplication::focusWidget()` 取编辑器：本轮观测到编辑器已经创建，但事件泵后焦点查询为空，因此更接近焦点建立/维护的时序失败；并未观察到编辑器先销毁后被访问。不能从此失败反推历史 H2 Release 的 Qt5Core `0xc0000005` 崩溃与其同根因，也没有异常现场栈。历史 Debug 的“failed to reopen ASCII table editor”同样不能预设为同根因。

本次未修复失败、未迁移 ASCII/stream 公共路径，未改随仓 Qt、本机环境、已验收 H2 部署，未 Stage、Commit、Push 或清理目录。下一步需总控判断是否要对“创建后未取得焦点”的普通烟测失败与原访问违规分别授权调查；原崩溃根因仍未定。

## 5. ASCII 烟测目标编辑器定向修复（后续授权）

总控限定复核完成：已检查目标editor模型/行列/viewport/唯一性校验、跨事件QPointer与合法提交关闭处理、新测试完整代码、修复前失败及最终D/R专项日志，并独立复算原H2部署与新Release EXE哈希。限定接受本片烟测定位修复，不另做人工验收；总控未重复构建或测试。D/R各10次成功为中间版本证据，最终源码以final-focused各3/3及最终失焦用例D/R通过为准，不混称最终版本十连过。历史0xc0000005无现场栈，仍独立未解决；不再自动扩大重复测试。任务停止，无Git/发布授权。

本片仅修合成烟测寻找当前表格单元 editor 及跨事件使用的安全性。修复前在 `tests/protocol_lab_ui/ascii_smoke_editor_tests.cpp` 构造“目标 editor 存活，但模拟所查焦点是其他控件”的确定性负例；原 `qobject_cast<QLineEdit*>(focusWidget())` 型定位无法返回目标，`out/stage4-ascii-smoke-fix/before-focus-regression-debug-runtime-ready.log` 为预期失败。更早首次运行因新测试目标缺随仓 Qt DLL 而退出 `0xc0000135`，未到断言，原日志 `before-focus-regression-debug.log` 保留；随后只在测试目标接入随仓 Debug/Release Qt DLL 与 `qwindows` 平台插件，未安装或改变本机环境。最终负例使用实际 `QApplication::focusWidget()`，将目标 editor 保持存活、焦点移至其他控件/失焦，并验证旧焦点定位不返回目标；D/R 负例最终通过。

新增 `tools/protocol_lab_ui/smoke_editor_target.h`：delegate 在创建 `QLineEdit` 时被动标记其模型指针及行/列；烟测 `Find` 从**当前表格 viewport** 仅选唯一、非隐藏且标记与指定 `QModelIndex` 完全匹配的 editor，并要求指定索引有效、属于该表格模型。没有按 `findChildren` 任取首个 editor，也不通过全局焦点、抢桌面前台、固定 sleep 或直接写模型绕过被测编辑路径。目标索引仍明确传入，错模型、错单元 editor、同目标多个存活 editor、目标 editor 已销毁均在新用例中拒绝。`table.currentIndex()` 是导航状态，不作为 editor 身份：开发中最初误加等值前置条件，Debug 双文档首次失败于 `after-ascii-debug-run-01.log`；仅加诊断字段后的 `identity-detail-debug.log` 明确 `valid=1 model=1 current=-1,-1 target=0,4`。保留这两份失败日志后，移除**仅此导航条件**，保留 editor 自身模型/行/列、viewport、唯一性等身份校验；新用例亦验证导航状态改变时仍按 editor 身份定位，而错身份不被选中。

`document_tab.cpp` 的 ASCII 三次开 editor 均改用受监护目标定位，未降级原容量、键盘、粘贴超限、Tab/Return 提交、合法恢复、Encode、Inspect、Host 断言。共享烟测按键/粘贴辅助函数跨 `sendEvent`/`processEvents` 使用 `QPointer`；若编辑器在输入期间消失，则停止后续合成事件并明确失败。`SendIfLive` 负例证明已销毁 editor 不接收按键。提交键按下时 delegate **合法关闭** editor 可跳过后续 KeyRelease，但仍执行事件泵/DeferredDelete 并由既有模型和结果断言判断提交成功，不把合法销毁一律判失败。bounded BYTES 烟测复用的同一输入辅助函数也在 editor 消失时明确失败；正常用户 delegate 的编辑/提交/焦点/关闭逻辑未改，新增身份属性仅为被动观察。诊断开关 OFF 的包获得修复效果，ON 仍可观察，但不是正常烟测必需。

独立 H2 preset 构建目录 `out/build/windows-msvc-stage4-ascii-smoke-fix`，显式诊断 `OFF`，随仓 Qt 5.13.0 / MSVC v142 x64 的 UI 与新测试目标 Debug/Release 均构建成功。新 editor 用例 D/R 均通过。导航条件校正后，双文档 `qt_smoke_ascii` Debug `after-ascii-debug-run-02.log` 至 `run-11.log` **10 次通过**，Release `after-release-focused.log` 中首轮及 `after-ascii-release-run-02.log` 至 `run-10.log` **10 次通过**；均串行且逐次留日志。其后把“已销毁 editor 不再分发按键”加入同一 helper，最终源码 D/R 各重新跑一次新用例、ASCII 双文档与 bounded v0.8 烟测，`final-focused-debug.log` / `final-focused-release.log` 各 3/3；最后将失焦负例改为实际 `focusWidget()` 后，新用例 D/R 再次通过，见 `final-focus-negative-debug.log` / `final-focus-negative-release.log`。额外的最终一次是源码变化后的针对复核，不是无限压力复跑。受影响 UI 自动回归 `qt_smoke_ascii_one_way`、`qt_smoke_v08`、`qt_smoke_binary_stage1`、`qt_smoke` 在 D/R 各 4/4，日志 `affected-ui-debug.log` / `affected-ui-release.log`。

最终新诊断 OFF UI EXE SHA256：Debug `A186E8F5DB8681C47914FCA9DA934DBA52D73CEAF47203F857452C7BD40797C1`、Release `5172432573BF50F190683786F208A06F6156066DBBD0571C3B9BAF67F2E81F3A`；已验收 H2 Release EXE SHA256 开始/结束均为 `3D3F13C6A16A63E70B28463DEED395A7A7834B8D642427A3F85A91F888D0A240`，未在其目录重建或覆盖。`git diff --check` 对本片已跟踪文件通过，暂存差异为空。未人工复验普通 UI、未获取历史异常栈、未进行 Linux/硬件/现场验证；此片只说明普通烟测焦点定位缺陷有修复前负例和针对性回归，不证明历史 H2 Release Qt5Core `0xc0000005` 崩溃已修。未 Stage、Commit、Push、发布、删除或扩展 ASCII 公开迁移。
