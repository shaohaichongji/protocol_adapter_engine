# Lab Unicode 字面量限定修复验证（2026-10-09）

状态：已完成派发范围，待总控复核。本片是产品 token 修复，不是纯注释等价变更；不代表人工 UI、完整 PAE 或发布验收。

## 基线与实际修改

现场为 `main@3facda71a53f57d97b2d1b497e19c7624b846676`，暂存区为空。既有注释、编码配置、教程、SDK 与其他任务变更保留。

本片文件：

- `tools/protocol_lab_ui/document_tab.cpp`：只为非 ASCII `QStringLiteral` 参数增加显式 `u` 前缀。
- `tests/protocol_lab_ui/generate_unicode_literal_fixture.ps1`：构建时抽取真实产品调用，生成数值期望。
- `tests/protocol_lab_ui/unicode_literal_tests.cpp`：检查 UTF-16 单元和 UTF-8 字节，并含三个独立手写数值锚点。
- `tests/protocol_lab_ui/CMakeLists.txt`：仅新增 MSVC 字面量专项、生成规则与所需 QtCore DLL 复制。
- 本报告；第三批注释报告仅追加交叉链接和修复边界，不修改旧结果。

完整扫描解析了本文件全部 **442** 个 `QStringLiteral` 调用，其中非 ASCII 为 **44 调用、45 字面量段**。初步 43 处统计仅识别单段形式；一个含换行转义的双段 tooltip 调用增加为第 44 个调用，须为其两段分别加前缀。最终移除恰好 45 个新增 `u` 后，源码恢复为接管副本的完整原始 UTF-8 字节；原中文、注释、转义、占位符及拼接顺序不变。Git 对 HEAD 的累计差异还包含此前注释，不能把它当作本片增量。

## 根因与回归方法

保留的 `c4819-diagnosis/findings.md` 记录：当前 v142/随仓 Qt 的宏展开配合分列 `/source-charset:utf-8 /execution-charset:utf-8` 时，窄参数可能生成错误 Unicode；存在无 C4819 的静默损坏。显式 UTF-16 参数解决所测路径，不改变公共编码配置或 Qt。

新增测试依赖实际 `document_tab.cpp` 与生成脚本，构建时重新抽取并编译原样 `QStringLiteral(...)` 表达式，不是只复制几个正确 probe。期望由 .NET 严格解码产品源 UTF-8 后生成十六进制整数，绕开 MSVC 字面量编译路径；另手写“解析结果”“实际字节范围：2 + 0”“<b>报文</b>：%1”的码元/字节数值锚点，覆盖静默中文、范围、HTML 和占位符。

所有 44 个调用都动态比较 UTF-16 与 UTF-8，不只是静态覆盖。生成器支持普通/u 前缀的连续字符串及本文件所用 `\n`、`\r`、`\t`、引号、反斜杠转义；无法解析的调用形状或未支持转义会失败，要求显式更新扫描器。它不是通用 C++ 解析器，也不验证 `.arg()` 最终替换、富文本渲染、字体或 QWidget 实际展示。

## 失败对照与 Debug/Release 证据

证据根：`out/build/comments-public-20261009/lab-lifecycle/unicode-fix`。沿用同一 Lab 构建树及 MSVC v142 `14.29.30133`、随仓 Qt；不改全局 PATH/CL，不重打包。先完成 Debug，再完成 Release，无并发 D/R 构建。

| 证据日志（相对证据根） | 结果 |
| --- | --- |
| `unicode-fix-configure.log` | 配置退出 0 |
| `unicode-fix-before-build.log` | 修复前真实产品抽取专项构建退出 0 |
| `unicode-fix-before-test.log` | CTest 退出 8；44 调用均失败，加三个锚点共 `FAILURES=47` |
| `unicode-fix-build-Debug.log` | Lab app＋新专项＋原四专项构建退出 0；C4819 为 0 |
| `unicode-fix-test-Debug.log` | 5/5 PASS，退出 0 |
| `unicode-fix-literal-details-Debug.log` | `PRODUCT_CHECKED=44 ANCHORS=3 FAILURES=0`，退出 0 |
| `unicode-fix-build-Release.log` | 相同目标构建退出 0；C4819 为 0 |
| `unicode-fix-test-Release.log` | 5/5 PASS，退出 0 |
| `unicode-fix-literal-details-Release.log` | `PRODUCT_CHECKED=44 ANCHORS=3 FAILURES=0`，退出 0 |

修复前生成片段另外保留为 `before-product-fixture.inc`。每份执行日志记录实际命令与退出码，脚本拒绝覆盖日志。展开命令见日志；执行入口为：

```powershell
& ./out/build/comments-public-20261009/lab-lifecycle/unicode-fix/run.ps1 -Phase Configure
& ./out/build/comments-public-20261009/lab-lifecycle/unicode-fix/run.ps1 -Phase BeforeFix
# 保存失败后才修改产品字面量。
& ./out/build/comments-public-20261009/lab-lifecycle/unicode-fix/run.ps1 -Phase Debug
& ./out/build/comments-public-20261009/lab-lifecycle/unicode-fix/run.ps1 -Phase Release
```

五项 CTest 为 `pae.tools.protocol_lab_ui.{unicode_literal,compile_queue,mailbox_ownership,document_state,dual_tab_isolation}`，锚定正则、`-j 1 --no-tests=error --output-on-failure`，没有执行全仓测试。新 C++ fixture 在 Debug 验证后只做格式化，产品/生成器/CMake 未变化；Release 验证最终格式化文件，格式检查退出 0。收尾未重复构建测试。

`release-options.log` 确认新专项沿用分列编码选项、`UndefinePreprocessorDefinitions=NDEBUG`；注册含 `/UNDEBUG`，源码若定义 NDEBUG 则 `#error`，检查失败也明确返回非零，不依赖可被关闭的 assert。Release 构建有一条 D9025（`/UNDEBUG` 覆盖 `/DNDEBUG`），不宣称零警告。原四专项 Release 断言依据保留的 `post-charset-release-options.log` 和此前实际运行证据。

## 保护核对与限制

`protected-before.csv` 针对接管时 51 个既有改动/未跟踪文件保存 Hash：排除产品源码及允许追加的旧报告两个授权重叠，剩余 **49 文件逐项未变**。本片测试 CMake 的接管副本另存在 `baseline/CMakeLists.txt`，只增注册块。旧报告的最终原始字节前缀与接管副本一致，保证仅追加。`old-evidence-hashes.csv` 针对保留的 76 个旧日志/诊断文件复算未变；这是保留证据校验，不冒称此前未保存文件的完整历史快照。

最终核对保存为 `unicode-fix-verify-final.log`，包括 45 前缀原始字节还原、上述保护、严格 UTF-8/no BOM、全工作树 `git diff --check`、空暂存区。最终相关文件 Hash 及 Git 状态保存在 `unicode-fix-final-files.csv`、`unicode-fix-git-status-final.log`。

只读扫描其余顶层 `tools/protocol_lab_ui/*.cpp/*.h`，在同一受支持形状中发现非 ASCII 窄 `QStringLiteral` 为 0；结果为 `outside-scope-scan.json`（空结果）。这不是全仓、转发宏或任意字符串编码审计，未扩修其他文件。

未验证：人工 UI/UI smoke、实际窗口渲染、Linux、全开关组合、standalone/installed SDK 包外、真实协议/硬件/现场、SDK 或体验包。新增测试在 MSVC 测试配置中要求可找到 PowerShell，其他平台未启用/验证该专项。此次不修改 A1/PAE/公共编码契约/Qt/其他 UI/教程/包或原四测试实现；不屏蔽 warning。

未 Stage/Commit/Push、删除或发布。交接用语为“已完成派发范围，待总控复核”；向总控一次反馈后停止写入。
