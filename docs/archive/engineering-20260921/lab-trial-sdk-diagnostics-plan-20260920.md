# 同批 installed-SDK Lab 消费与结构化编译诊断计划

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-20

仓库：`<REPO_ROOT>`

静态核对基线：`main@7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`

## 1. 结论与本轮边界

本轮仅检查当前源码、CMake、standalone 白名单和既有验证记录，未读取或消费尚未交付的
`<TRIAL_ROOT>` 五包，未构建、测试、启动 UI、复制产物或修改
功能代码。

建议顺序保持为：

1. PAE 任务先完成同提交五包及六组普通 SDK 包外消费，并由总控复核包身份；
2. 再修整/冻结本报告列出的 standalone 输入闭包，使用同批 SDK 串行完成 static/shared、Debug/Release
   的 Testing-on、Testing-off 与模块来源验证；
3. 以该同批 SDK + Lab 结果形成新的可回退基线；
4. 最后另行实施结构化编译诊断首片，避免把未提交诊断实现混入本轮 SDK/Lab 候选。

现有 `98df5e0` standalone 证据只能说明旧快照。`7b4205e` 相对它已经包含 G1 Binary Encode、G2
Binary stream、中文左右工作台和 smoke 启动/编辑提交修复，不能复用旧 `32/32` 数量或旧部署来源
结论冒充本批验证。

## 2. 同批 SDK standalone 的当前闭包

### 2.1 已具备的产品边界

当前 `tools/protocol_lab_ui/standalone/CMakeLists.txt` 已保持以下隔离：

- 顶层独立工程，不把 PAE 源树 `add_subdirectory` 进来；
- 只从精确 `PAE_SDK_ROOT` 执行 `find_package(PAE CONFIG REQUIRED ... NO_DEFAULT_PATH)`，产品链接
  `PAE::pae`；
- 校验 package kind、configuration 以及 imported library/runtime 均位于精确 SDK 根；
- shared 包额外校验 `pae.dll` 与 `SHA256SUMS.txt`，并将同一 DLL 放到测试和产品部署闭包；
- 固定 MSVC v142 `14.29.30133`、Qt 5.13 Core/Gui/Widgets/qwindows，以及锁定的 yyjson 0.12.0；
- standalone 定义 public-only 路由：legacy complete、Binary public H1/H2、ASCII public A1/A2/stream；
  private ASCII adapter 与 private Binary materializer 保持关闭；
- 产品 UI 继续复用当前中文左右工作台，不另建第二套执行或展示逻辑。

G1 complete Encode 的非 Qt 主路径已进入现有白名单文件
`public_binary_decode.cpp/.h`，G1 UI 及 G2 描述/流式映射也主要落在现有白名单内的
`binary_host_adapter_public.*`、`public_binary_description.*`、`DocumentSession` 和
`DocumentTab`。因此本批不应把 private `prepared_binary*`/materializer 重新带入 standalone。

### 2.2 预检发现的实际缺口

以下缺口需在后续实施片先用“缺文件/缺测试门禁”失败证据固定，再最小修正；本轮不修改：

| 缺口 | 当前事实 | 影响 | 最小方向 |
| --- | --- | --- | --- |
| G2 非 Qt 新测试源未进入白名单 | `tests/protocol_lab_binary/CMakeLists.txt` 已引用 `public_binary_stream_tests.cpp`，但 `PrepareStandaloneInputs.ps1` 只显式复制 `public_binary_decode_tests.cpp` | Testing-on 的 copied tree 缺源，无法证明 G2 owner | 将该 `.cpp` 加入显式白名单 |
| G2 canonical 配置未复制 | tests/H2/UI 使用 `examples/config/synthetic_stream_framing_slice.pae.json`，但准备脚本的 `$configs` 未列入 | G2 test、Qt hidden smoke 与部署配置闭包不完整 | 加入源路径复制及 `standalone-configs` 副本 |
| CRT probe 脚本未复制 | UI test CMake 已注册 Debug-only `verify_crt_assert_probe.cmake`，准备脚本只自动复制该目录的 `*.cpp` | copied tree 中 probe 到执行时缺脚本，不能声称新 smoke fail-fast 受验 | 显式加入 `.cmake` 白名单，并核对 Debug-only inventory |
| public-only ASCII window smoke 被旧开关屏蔽 | standalone 为 `ASCII_PUBLIC_A2=ON`、private `ASCII_ADAPTER=OFF`；但 `qt_smoke_ascii`、one-way、editor 及 qwindows 环境列表仍只看 private adapter | 产品具备 public ASCII 路由，但 Testing-on 不一定覆盖完整记录窗口提交/中文展示修复 | 引入清晰的“ASCII complete UI consumer available”局部门禁，至少让 public A2 注册完整记录及 one-way 产品 smoke；不得为刷测试开启 private adapter |
| 测试数量已漂移 | 旧报告固定为 32 项，当前已增加 G2 public stream、Binary stream Qt smoke 和 CRT probe | 固定数量会漏测或误判 Release 中 Debug-only probe | 每个配置先 `ctest -N -V` 保存 inventory，再以名称集合和配置属性校验，不在计划中硬编码旧总数 |

`standalone/README.md` 中 `98df5e0` 与其本地绝对路径是已验证旧交付身份，不是 CMake/脚本回退路径。
在本批验证完成前应保留为历史入口；新路径应来自后续明确的输出根和生成 provenance，不能先把 README
改成尚未通过的 `7b4205e` 产品。

### 2.3 建议的最小 standalone 写入范围

后续实施若无新增阻塞，预计只需：

- `tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1`：补三项输入白名单；
- `tests/protocol_lab_ui/CMakeLists.txt`：把 public A2 纳入 ASCII complete UI smoke 的真实消费者条件，
  同时保持 repo private-adapter 组合原行为；
- 必要时 `tools/protocol_lab_ui/standalone/CMakeLists.txt`：只同步测试门禁/变量，不新增执行实现；
- `tools/protocol_lab_ui/standalone/README.md`：仅在验证收口后更新新身份与入口；
- 新的 validation 报告记录最终源码、命令、日志和未验证项。

若 copied tree 还暴露其他缺失源，先列出引用方、期望路径和缺失文件，停报总控；不要改为递归复制整个
仓库，也不要把 private PAE/Lab 源码带入闭包绕过缺口。

## 3. 同批 SDK + Lab 验证矩阵

### 3.1 输入门禁

开始 Lab 消费前必须从 PAE 交接中确认：

- 五包均来自精确 `7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`，各自 `PROVENANCE.json`、
  `SHA256SUMS.txt` 和 package config 自洽；
- source、static Debug/Release、shared Debug/Release 五包已完整交付，PAE 六组普通包外消费已由总控
  限定复核；
- 本批 static/shared D/R 使用同一提交、同一打包批次，禁止混用旧 `98df5e0` 包；
- `PrepareStandaloneInputs.ps1` 只在全新外部根执行，记录仓库 HEAD/status、SDK provenance 和全部输入
  SHA-256；不覆盖现用 `deliverables/lab/98df5e0` 或 `fa81329/common-release`。

### 3.2 配置、构建与功能矩阵

static 后 shared 严格串行；每种包各自 Debug/Release 独立 build/deploy 根：

| 包形态 | 配置 | Testing-on | Testing-off | 重点 |
| --- | --- | --- | --- | --- |
| STATIC | Debug | configure/build + inventory + 全部注册测试 | configure/build 产品闭包 | G1/G2/ASCII/legacy、Debug CRT probe |
| STATIC | Release | configure/build + inventory + 全部注册测试 | configure/build 产品闭包 | `/UNDEBUG` 生效、无 `pae.dll` 依赖 |
| SHARED | Debug | configure/build + inventory + 全部注册测试 | configure/build 产品闭包 | 同配置 import lib/runtime、Debug CRT probe |
| SHARED | Release | configure/build + inventory + 全部注册测试 | configure/build 产品闭包 | `/UNDEBUG`、`pae.dll` 来源与哈希 |

Testing-on 至少要在 inventory 中出现并通过以下能力组；最终以当前 CTest 清单为准：

- Binary G1：`pae.protocol_lab_binary.public_h1` 及 `binary_public_h2`，覆盖 complete
  Decode/Encode、失败清旧结果、多消息/Flow 身份与 UI 投影；
- Binary G2：`pae.protocol_lab_binary.public_stream`、`binary_public_h2`、
  `binary_stream_qt_smoke`，覆盖三策略、Submit/Continue/Reset、Flow/文档隔离和物化失败；
- ASCII public：A1、public stream/noexcept、public façade/A2、stream session/Host session；
- ASCII complete 产品 smoke：text/literal 和 decode-only/encode-only，经 public A2 路由执行；
- legacy public complete：0.5--0.8 headless 与 Qt smoke；
- 通用所有权/queue/mapping/session/关闭流程，以及 Debug-only CRT assertion probe。

不要仅凭总项数判断闭包。应保存 D/R 四份 `ctest -N -V`，比较测试名称、可执行文件、配置路径、
`QT_QPA_PLATFORM=windows` 和 Debug-only probe 属性；再运行完整 Testing-on。若历史 ASCII focus/AV 再现，
保留首次日志并停报，不反复刷绿。

### 3.3 Qt、部署与模块来源

每个 Testing-off 部署必须是新空目录，至少包含：

- `pae_protocol_lab_ui.exe`；
- 匹配 D/R 的 Qt5Core、Qt5Gui、Qt5Widgets 与 `platforms/qwindows*.dll`；
- legacy 0.5--0.8、Binary complete、Binary stream canonical、ASCII complete/one-way/stream 及生成的
  max fixture；
- shared 额外包含同 SDK 根的 `pae.dll`，static 明确不含且不依赖 `pae.dll`。

使用 `CapturePaeLabModules.ps1` 在隐藏进程内取证，且只结束本次 PID：

| 包形态 | 预期模块 | 判定 |
| --- | --- | --- |
| STATIC D/R | Qt Core/Gui/Widgets/qwindows 共 4 项 | 全部从本次部署加载；Qt 输入=部署=已加载 SHA-256；进程不得加载 `pae.dll` |
| SHARED D/R | 上述 4 项 + `pae.dll` | 五项均从本次部署加载；Qt 与 PAE 各自输入=部署=已加载 SHA-256 |

同时记录产品工程实际链接路径：static library 或 shared import library 必须位于对应
`PAE_SDK_ROOT`；shared 用 `dumpbin /DEPENDENTS` 命中 `pae.dll` 和匹配配置 Qt，static 不命中
`pae.dll`。旧 Qt/yyjson 负例若实现与输入未变可引用历史；包类型、D/R 错配、shared 缺失/错误
runtime 必须对本批新包使用派生副本验证 fail-closed，不能修改原包。

## 4. 结构化编译诊断首片

### 4.1 当前丢失点

公开 `pae::CompileDiagnostic` 已提供：

- `stage`、`code`；
- `json_pointer`；
- `std::optional<std::size_t> byte_offset`；
- `detail`；
- `resource_kind`、`required_bytes`、`limit_bytes`、`resource_profile`。

当前 worker 在 public route 失败时把完整对象放入 `CompileCompletion::public_diagnostic`，但
`DocumentSession::ApplyCompileCompletion()` 对 legacy/Binary/ASCII 三条 public 路由都只把
`detail` 传给 `SetDiagnostic()`。Session 仅保存 `diagnostic_id_` 和 `diagnostic_detail_`，
`DocumentTab::RefreshState()` 最终也只显示“操作失败（稳定 UI 码）+ detail”。private route 的
`CompileCompletion::Diagnostic` 更早就只保留了 detail。

因此缺口是 Lab 展示链丢失结构化字段，不是 PAE public API 缺字段。首片不需要扩公开接口。

### 4.2 最小数据链

建议新增一个 Lab-owned、只读的编译诊断投影，例如 `CompileDiagnosticView`，不把 Qt 类型放入 worker
或 Session：

```text
CompileProtocolJson / private compiler
  -> CompileWorker 显式枚举映射为 CompileDiagnosticView
  -> CompileCompletion::structured_compile_diagnostic
  -> DocumentSession::compile_diagnostic（仅当前 load revision）
  -> DocumentTab::RefreshState
  -> 既有右侧“诊断与计时”可滚动区域
```

建议字段语义：

| 展示字段 | DTO 表示 | 缺失/零值规则 |
| --- | --- | --- |
| stage | 稳定英文枚举 token | 编译器诊断中始终存在，不从 detail 猜测 |
| code | 稳定英文枚举 token | 始终存在，保留原错误码 |
| JSON pointer | 原始字符串 | 空字符串是 JSON Pointer 根位置，显示“根（空 pointer）”，不是“缺失” |
| byte offset | `optional<size_t>` | `nullopt` 显示“未提供”；数值 `0` 必须显示 `0` |
| resource kind | 稳定英文枚举 token | `NONE` 是明确值，不等同字段丢失 |
| required/limit | 资源信息子对象或显式 presence | kind 非 `NONE` 时原样显示，包括真实 `0`；kind 为 `NONE` 时显示“无资源预算数据”，不要把默认零伪装成预算值 |
| detail | 原始技术详情 | 原样保留，不解析英文文本推断状态 |

`resource_profile` 已在 public DTO 中存在，可作为同一资源区的补充字段；本轮用户指定的七个核心字段
不得因是否显示 profile 而被省略。

### 4.3 建议改动范围

最小实现预计限于：

- `tools/protocol_lab_ui/compile_worker.h/.cpp`：定义 Lab-owned DTO；对 public/private 诊断使用显式
  `switch` 映射枚举 token；在 completion 中旁路保存结构化投影；
- `tools/protocol_lab_ui/document_session.h/.cpp`：保存当前 load revision 的可选编译诊断并提供只读
  accessor；BeginLoad、成功发布、关闭及非编译失败按状态清理，拒绝的陈旧 completion 不得覆盖当前值；
- `tools/protocol_lab_ui/document_tab.cpp`：在既有右侧诊断区域追加中文标签和原始稳定 token，保持左右
  工作台、Tab 顺序及现有运行期诊断文案；
- `tests/protocol_lab_ui/compile_queue_tests.cpp`、`document_state_tests.cpp` 及必要的局部 UI smoke：覆盖传递、
  清理、陈旧 revision 与显示格式；若需要 private/public 两套映射专项，只在现有 test target 内最小补充。

为降低回归风险，首片可保留现有 `public_diagnostic`、`diagnostic_id_`、`diagnostic_detail_` 作为执行决策
和兼容显示来源；结构化 DTO 只是附加观察数据。待测试证明所有 public/private 路由一致后，再评估是否
去重，不能在首片顺带重构 completion/session 错误体系。

### 4.4 展示建议

继续使用现有右侧“诊断与计时”Tab 和 `diagnosticScroll`，不新增弹窗或重排左右布局。编译失败时建议
按以下顺序显示：

```text
配置编译失败（UI_PUBLIC_ASCII_COMPILE_FAILED）
阶段：DOMAIN_VALIDATION
错误码：UNKNOWN_REFERENCE
JSON 位置：/pipelines/0/messages/0/fields/1
字节偏移：未提供
资源类型：NONE
需要 / 限制：无资源预算数据
技术详情：<原 detail>
```

资源失败时 required/limit 即使为 `0` 也显示数字；offset 为 `0` 时显示数字 0。分类失败、调度拒绝、
文件过大和运行期 Encode/Decode/stream 错误没有 PAE `CompileDiagnostic` 时，只保留现有 UI 码和技术
详情，结构化区明确不出现，不能制造 stage/code。

### 4.5 首片验证建议

- worker：至少覆盖 pointer 根/非根、offset 缺失/0/非零、resource `NONE`、resource kind 有效且
  required/limit 含零值；public/private enum token 使用独立期望；
- session：三条 public compile route 的结构化失败、成功后清理、重新加载清理、旧 revision completion
  不覆盖、普通 UI 错误没有伪造结构化诊断；
- UI：诊断 Tab 同时包含中文标签、稳定 stage/code token、原 pointer/offset/resource 数字和 detail；
  长文本仍可滚动/复制，Tab 文案仍用“有内容”状态；
- D/R 定向验证保持 Release 断言有效；随后只跑受影响的 compile queue/document state/ASCII/Binary/legacy
  编译失败 smoke，不借机运行全仓或改执行逻辑；
- 待结构化诊断实现完成后，必须重新做其自身 standalone static/shared D/R 消费验证；不能把本报告规划
  或此前同批 SDK/Lab 基线当作实现后的验证。

## 5. 停止条件与未验证项

遇到以下情况先停报总控：

- 五包身份、配置或 package manifest 不是同一 `7b4205e` 批次；
- standalone 需要 PAE private header/library、回退开发树或全局 PATH 才能构建；
- 新缺失文件超出已列白名单，或 public-only ASCII 必须启用 private adapter 才能工作；
- structured diagnostic 需要修改 `include/pae/**`、Core/Schema、编译执行语义或重新解析 detail；
- 历史 AV/focus-out 再现，或模块来源无法证明来自本次部署。

本报告未验证新五包、Lab 构建、CTest、Qt 模块加载、可见 UI、Linux、异工具链/异 CRT、稳定 ABI、
Qt 正式外发许可、真实协议、硬件、现场或正式发布。没有 Stage、Commit、Push、删除或替换现用部署。

## 6. 本轮 Git 与交付状态

- 实际仓库写入仅本报告；总控既有 `pae-execution-delivery-organization-plan.md` 修改已保护。
- 未修改源码、CMake、测试、脚本、SDK、Qt、yyjson 或部署。
- 状态为“已完成只读准备范围，待总控复核”；达到停点后停止写入。
