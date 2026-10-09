# 第三批 Lab 主学习链中文注释验证（2026-10-09）

## 状态与范围

状态：已完成派发范围，待总控复核。12 文件注释及静态一致性检查已完成；编码兼容修正后，Lab app 与四个专项均在 Debug/Release 成功构建，四类 CTest 各 4/4 PASS。首轮 D8016 失败证据保留，不改写为成功；本结论不是总控验收、人工 UI 或发布批准。

接管基线：`main@3facda71a53f57d97b2d1b497e19c7624b846676`，暂存区为空。以下 12 文件接管时无差异，本批仅新增 101 行中文注释，未改变声明布局、非注释 token、宏续行、断言或执行逻辑。正确英文全部保留，无英文纠错。

| 文件 | 注释重点 |
| --- | --- |
| `tools/protocol_lab_ui/application_window.h` | 窗口协调职责、Qt parent 所有权、worker 生命周期及借用 Tab |
| `tools/protocol_lab_ui/application_window.cpp` | GUI 轮询、关闭确认顺序、业务关闭后延迟销毁、ticket 到 owner 的路由 |
| `tools/protocol_lab_ui/compile_worker.h` | 自持请求/结果、pending 上限、active 非取消、Submit 与编译成功区别、邮箱移交 |
| `tools/protocol_lab_ui/compile_worker.cpp` | YAML 生成 JSON 身份、公开路径单次编译、初始化后启动线程、锁外编译、停止后 join |
| `tools/protocol_lab_ui/document_session.h` | 无 Qt 串行 Session、状态含义、revision 身份、借用视图、Binary publication、同步操作 |
| `tools/protocol_lab_ui/document_session.cpp` | Binary Prepare/Publish 边界、Flow 不执行、晚到成功/错误拒绝、关闭与状态刷新 |
| `tools/protocol_lab_ui/document_tab.h` | worker 借用与 Session 自有、GUI 更新标记、Host 高位请求、YAML 来源映射 |
| `tools/protocol_lab_ui/document_tab.cpp` | 结果分流、解绑模型先于释放 owner、Host 候选、Flow UI 恢复、加载与同步执行顺序 |
| `tests/protocol_lab_ui/compile_queue_tests.cpp` | 诊断投影、门闩驱动的替换/排队合同及未覆盖范围 |
| `tests/protocol_lab_ui/mailbox_ownership_tests.cpp` | 关闭后晚到 owner 丢弃，不误称已验证 active 析构 join 或二次取结果 |
| `tests/protocol_lab_ui/document_state_tests.cpp` | 结果失效、晚到成功/错误、诊断隔离、关闭及未覆盖 GUI 边界 |
| `tests/protocol_lab_ui/dual_tab_isolation_tests.cpp` | 两 Session 隔离，不误称真实 Qt Tab 或并发验证 |

没有修改两个 ASCII adapter、CMake、公共 API、教程/索引/既有报告或成品包。逻辑预算未称为 RSS 硬上限；Binary publication 的保证未泛化到 ASCII；不同 adapter 的 review 未统一解释为无独立 Decode。

## 接管及静态证据

证据根：`out/build/comments-public-20261009/lab-lifecycle/`，本批新建，未覆盖前两批证据。以下路径均相对此根：

- `baseline/`：编辑前 12 文件原字节副本，保持目录结构。
- `targets-before.csv`：原目标文件 SHA256；验证时再次核对副本与该清单。
- `protected-before.csv`：36 个既有已修改/未跟踪文件的 SHA256（未跟踪目录展开为文件）。
- `verify.ps1`、`verify-initial.log`、`verify-final.log`：严格 UTF-8 无 BOM、非注释 token、预处理指令/续行、行拼接数量、行注释尾部反斜杠检查；12/12 通过。支持字符串、字符及带 delimiter 的 raw string，行拼接先于 token 比较。原有 36 文件哈希保持不变。
- `format-check.ps1`、`format-summary.csv`、`format-{baseline,current}-*.log`：`clang-format --dry-run --Werror --style=file:<仓库 .clang-format>` 分别检查接管副本和当前源码，未执行格式化写入。
- `compare-format.ps1`、`format-comparison-fixed.log`：按诊断所指源行文本及列号比较，12 文件诊断序列一致，没有新增格式诊断。
- `format-comparison.log` 保留验证辅助脚本首次绝对路径拼接错误；脚本修正后生成独立 `format-comparison-fixed.log`，未覆盖失败证据。该错误不涉及目标源码。
- `verify-final.log` 另含目标范围与全工作树 `git diff --check` 结果。

格式检查不是全量 PASS：接管前已有 7 文件共 388 条诊断，本批保留，不进行越界重排。

| 文件名 | 接管前/当前诊断数 | 两次退出码 |
| --- | ---: | --- |
| `application_window.cpp` | 5 / 5 | 1 / 1 |
| `document_tab.cpp` | 217 / 217 | 1 / 1 |
| `document_session.h` | 5 / 5 | 1 / 1 |
| `document_session.cpp` | 138 / 138 | 1 / 1 |
| `compile_worker.h` | 4 / 4 | 1 / 1 |
| `compile_worker.cpp` | 16 / 16 | 1 / 1 |
| `dual_tab_isolation_tests.cpp` | 3 / 3 | 1 / 1 |

其余五文件两次均退出 0，无诊断。

## 首轮构建尝试及阻塞（历史证据）

使用仓库当前推荐 `windows-msvc-pae-lab` preset，独立 BuildRoot 为证据根下的 `build/`。工具链为 Visual Studio 18 2026 / v142 14.29.30133 / x64，Qt 使用仓库固定 5.13 副本。为覆盖已注释分支，额外开启现有 Binary H1/H2、ASCII A1/A2/stream/UI、public legacy complete 和 YAML frontend/entry；未修改 preset/CMake 或全局环境。

实际入口命令（仓库根执行）：

```powershell
& ./out/build/comments-public-20261009/lab-lifecycle/run-validation.ps1 -Phase Configure
& ./out/build/comments-public-20261009/lab-lifecycle/run-validation.ps1 -Phase Debug
& ./out/build/comments-public-20261009/lab-lifecycle/run-validation.ps1 -Phase Release
```

脚本及每份日志第一行保留展开后的完整 cmake 命令。Debug 完全结束后才启动 Release，同一 BuildRoot 未并发构建。

| 阶段 | 日志（相对证据根） | 实际结果 |
| --- | --- | --- |
| Configure | `configure.log` | cmake 退出 0 |
| Debug 构建 | `build-Debug.log` | cmake 退出 1，MSVC D8016 |
| Release 构建 | `build-Release.log` | cmake 退出 1，同一 MSVC D8016 |
| Debug/Release 四类 CTest | 无执行日志 | 构建失败后脚本停止，未执行，不填造测试数量或 PASS |

构建命令限定目标为 `pae_protocol_lab_ui` 及 `pae_protocol_lab_ui_{compile_queue,mailbox_ownership,document_state,dual_tab_isolation}_tests`。没有执行全仓测试、UI smoke 或人工 UI。预定 CTest 正则为 `^pae\.tools\.protocol_lab_ui\.(compile_queue|mailbox_ownership|document_state|dual_tab_isolation)$`，`-j 1`，未实际运行。

首轮阻塞来源（当时源码的只读核对，下列原行号不作为修正后源码定位）：

1. 既有变更 `src/public_api/CMakeLists.txt:22` 向 MSVC C++ consumers 传递 `/source-charset:utf-8`。
2. 根 `CMakeLists.txt:558` 的 `pae::project_options` 已传递 `/utf-8`。
3. 新生成 `build/tools/protocol_lab_ascii/pae_protocol_lab_ascii_public_a1.vcxproj` 的 `AdditionalOptions` 同时包含 `/source-charset:utf-8 /utf-8 /Zc:__cplusplus`；Binary H1 和 UI owned presentation 同样失败。Debug/Release 日志均有原始 D8016。
4. 当前进程 `CL` 和 `_CL_` 环境变量均不存在；没有通过环境删改或添加编译选项来绕过。

Release 测试项目生成文件仍设置 `UndefinePreprocessorDefinitions=NDEBUG`（来自既有 `/UNDEBUG`），但没有成功构建/运行测试，不能据此宣称本批 Release 断言动态验证通过。

当时所需决定：由总控协调负责 UTF-8/CMake 变更的范围，解决公开 source charset 与内部 `/utf-8` 的兼容性后，授权恢复本批独立构建及四类 D/R 测试。本片当时停止，没有擅自回退已有变更或改编译选项。该决定现已落实，恢复证据如下。

## 编码修正后恢复验证

总控明确恢复本批验证；编码修正由《子任务推进》完成并停止写入，详见只读参考 [编码兼容修正验证](source-charset-compatibility-validation-20261009.md)。恢复时仍为上述 `main@3facda7`，暂存区为空。

恢复阶段仅调整本批 `run-validation.ps1` 的日志前缀与 `--no-tests=error` 检查、`verify.ps1` 的恢复保护清单，并更新本报告。12 源码文件未继续修改；未修改 CMake、目标属性、SDK 或 Qt 环境。本仓内部 project_options 已由授权修正拆为 source/execution UTF-8 选项，Lab 无需设置 `PAE_MSVC_SOURCE_CHARSET_SUPPLIED`。

保护与来源证据（相对证据根）：

- `post-charset-report-before.md` 保留本报告恢复前版本。
- `post-charset-authorized-changes.csv` 对比原 `protected-before.csv`，仅 6 个重叠文件哈希变化：`cmake/PAEConfig.cmake.in`、`docs/sdk/README.md`、`src/public_api/CMakeLists.txt`、`tests/public_api/CMakeLists.txt`、`tests/public_api/source_charset_consumer.cpp`、`tests/public_api/verify_source_charset.cmake`。均属于此次获准编码修正；原清单其他 30 文件未变。
- 编码修正另外新增根 `CMakeLists.txt` 的工作树差异及 `docs/engineering/source-charset-compatibility-validation-20261009.md`，共 8 个获准编码修改文件；它们不是本片写入。
- `post-charset-protected-before.csv` 固定恢复时 50 个文件的哈希，包括上述 8 文件、12 注释源码及其他既有变更，排除获准更新的本报告。恢复结束全部保持一致，没有通过简单忽略编码文件来放弃保护。
- `post-charset-verify-initial.log`、`post-charset-verify-final.log`：再次对原 `baseline/` 核对 12 文件的非注释 token、预处理指令/宏续行、行拼接及严格 UTF-8 无 BOM，12/12 PASS；并对 `targets-final.csv` 核对注释源码整文件哈希，12/12 与上次交接一致。格式检查沿用前节证据，源码哈希未变，未重复格式化。
- `post-charset-old-log-hashes.csv`、`post-charset-old-log-protection.log`：首轮配置、D/R 失败与静态/格式日志均保留，哈希未变。

复用同一个独立 `build/`，开关和工具链仍与首轮一致；Debug 完全结束后才启动 Release。恢复命令（仓库根执行）：

```powershell
& ./out/build/comments-public-20261009/lab-lifecycle/run-validation.ps1 -Phase Configure -LogPrefix post-charset-
& ./out/build/comments-public-20261009/lab-lifecycle/run-validation.ps1 -Phase Debug -LogPrefix post-charset-
& ./out/build/comments-public-20261009/lab-lifecycle/run-validation.ps1 -Phase Release -LogPrefix post-charset-
& ./out/build/comments-public-20261009/lab-lifecycle/verify.ps1 -Final -PostCharset
```

每份新日志首行记录展开后的实际命令及绝对 BuildRoot，末行记录实际退出码，脚本拒绝覆盖已有日志。构建仍只选择 Lab app 和四个专项目标；CTest 使用前述锚定正则、`-j 1` 和 `--no-tests=error`，未运行全仓测试。

| 阶段 | 新日志（相对证据根） | 结果/退出码 |
| --- | --- | --- |
| 重新配置 | `post-charset-configure.log` | 0 |
| Lab app＋四专项 Debug 构建 | `post-charset-build-Debug.log` | 0 |
| 四专项 Debug CTest | `post-charset-ctest-Debug.log` | 4/4 PASS，0 |
| Lab app＋四专项 Release 构建 | `post-charset-build-Release.log` | 0 |
| 四专项 Release CTest | `post-charset-ctest-Release.log` | 4/4 PASS，0 |

实际四个 CTest 名称为：

- `pae.tools.protocol_lab_ui.compile_queue`
- `pae.tools.protocol_lab_ui.mailbox_ownership`
- `pae.tools.protocol_lab_ui.document_state`
- `pae.tools.protocol_lab_ui.dual_tab_isolation`

`post-charset-release-options.log` 记录四个生成 Release 测试工程的选项，均含分列的 `/source-charset:utf-8`、`/execution-charset:utf-8`，没有额外 `/utf-8`，且 `UndefinePreprocessorDefinitions=NDEBUG`。实际 Release 构建日志四次 D9025 明确以 `/UNDEBUG` 覆盖 `/DNDEBUG`；随后四专项实际运行通过，不把仅生成工程当成动态断言验证。

构建成功不是零警告：Debug/Release 的 `document_tab.cpp` 各记录 32 条 C4819（日志称当前代码页 0），对应现有 Qt 字符串相关源码位置。目标源码经严格 UTF-8 检查且整文件哈希与上次注释交接一致，生成工程已采用分列 UTF-8 选项。未确定该警告的进一步原因，也未修改源码、Qt/CMake 或屏蔽警告；作为非阻塞残留交总控，不宣称全部字符显示已人工验证。

## 未验证与停点

未验证：人工 UI、UI smoke、全部开关组合、installed SDK 包外、Linux、真实协议/硬件/现场、长期并发或生产可用性。Lab app 成功构建不能代替窗口关闭、事件重入及字符显示的人工运行验收；四类 headless 专项不是全部 Flow/adapter 行为验证。未发现本批需要修复的执行逻辑缺陷；原 D8016 已不再阻塞所测路径，C4819 残留如上。

未 Stage/Commit/Push、发布、删除或重新打包。保留既有文件与全部新旧日志/构建产物；当前报告、保护和 Git 状态见 `post-charset-verify-final.log`、`post-charset-git-status-final.log`。恢复范围已完成，向总控主动反馈一次新证据后停止写入，等待复核；本片不自行宣称总控验收通过。

## 后续 Unicode 修复交叉链接（2026-10-09）

后续独立诊断确认此前 C4819 不能仅视为非阻塞残留：所测 v142/Qt 与分列编码设置存在真实 Unicode 损坏，且可能没有警告。上述记录保留为当时证据，不改写其历史结果；当前修复与新证据见 [Lab Unicode 字面量限定修复验证](lab-unicode-literal-fix-validation-20261009.md)。

本次单独授权为 `document_tab.cpp` 的 44 个非 ASCII `QStringLiteral` 调用、45 个字面量段添加显式 `u` 前缀，并新增真实产品字面量回归。它是 token 修复，不属于本报告原注释批的纯注释等价声明。Debug/Release 新专项与原四项各 5/5 通过，不代表人工 UI、全仓、SDK 或发布验收；原日志全部保留。
