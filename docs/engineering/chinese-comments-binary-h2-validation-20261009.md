# 第六批 Binary H2 展示适配层中文注释验证

## 身份与范围

2026-10-09，接管及最终核对基线为 `main@5765457a78951b019e8d9bad0e79d32f16e7716d`。
本轮正式续派是 H2 中文注释，不重做历史 A1 预算修复。
状态为已完成派发范围，待总控复核；无 Stage/Commit/Push、删除、发布或重打包。

仅修改以下四文件的注释，另新增本报告：

- `tools/protocol_lab_ui/binary_host_adapter_public.h`
- `tools/protocol_lab_ui/binary_host_adapter_public.cpp`
- `tools/protocol_lab_ui/public_binary_description.h`
- `tools/protocol_lab_ui/public_binary_description.cpp`

接管时的 ASCII 四源码、Binary H1 两源码及两份未跟踪注释报告原样保护。
未修改运行行为、非注释代码、声明布局、宏续行、CMake、测试、公共契约、PAE、Qt、
DocumentSession/Tab、通用 DTO、legacy adapter、索引、教程、旧报告或成品。

## 注释依据与内容

通读四文件，并只读核对 H1 接口、展示 DTO、DocumentSession 的描述接收与 Flow 发布调用、
对应测试和构建配置。保留正确英文，本批未发现需要纠正的矛盾英文。

- H2 上接 DocumentSession，下接公开 H1/Host；不另建执行引擎。H1 当前状态是内部引用，
  H2 返回展示 DTO 持有自己的字符串和容器；Current、Draft、TypedDrafts 及元数据 getter 是借用。
- Description 保留内部描述；TakeDescription 一次性移交独立发布副本，移交不扣减创建时的保守计费。
- CreatePublic 检查准备身份，替换须同 document/load/hash 且 session 为下一代；request 只要求非零。
  描述复制、旧实例及外部共存、UI/Hex/UTF-16 预留属于逻辑计费，不是 RSS 硬上限。
- MapCurrent/MapCurrentEncode 不执行 Codec；MapCurrentStream 不推进 Host，但首次展示复制失败
  会设置此 Flow 的 H2 映射故障。保留实际消费与计数，后续推进要求 Reset，H1 Reset 成功才解除标记。
- Decode raw 来自实际 H1 结果；Decimal logical 使用 coefficient@scale，不反算 raw。
  Encode 不补做 RX Decode，raw 显示未观察；实际高亮范围来自本次执行，不替代为描述最大范围。
- NO_CANDIDATE 不复用上一候选，失败 DTO 不冒用旧成功字段；全局失败字段索引不能单独确定 Message。
- 保存的是当前源 Flow 的 Inspect/类型化草稿与 Message 选择，完成复制和预算检查后先保存再切换目标。
  改变 Encode Message 时清空旧类型化草稿和 TX 结果，避免错用字段索引。
- 公开描述从 Schema 0.9 Binary 元数据、物理查询和关联执行能力构造；局部完整构造后才替换输出。
  Message 级方向能力汇总所有 Pipeline，Pipeline 列表按各自关联能力生成。

本批没有实施代码缺陷修复；未据局部阅读宣称全面预算体系审计或线程安全。

## 静态与保护验证

证据根：`out/build/comments-public-20261009/binary-h2/`。
复用上轮已保存的 baseline、protected-before.csv、untracked-before.csv 和格式前置日志，
续派时先复算四源码与 baseline、全部受保护文件的 Hash，一致后才编辑；未覆盖旧日志。

`verify.ps1` 保留普通字符串/字符字面量，移除行及块注释，比较剩余完整非空代码行；
不只比较词序，同时检查宏续行逐字一致。遇 raw literal token 拒绝，不静默忽略。
初版用全字符串搜索 R 引号，误将普通字符串末尾 ERROR 引号判为 raw，初次验证中止；
修正为在扫描器匹配字面量/注释时识别 raw token，保留失败日志 `verify-initial.log`，
重试记录为 `verify-initial-retry.log`，最终为 `verify-source-final.log`、`verify-final.log`。

最终四源码非注释内容/布局/续行等价、严格 UTF-8 无 BOM、`git diff --check`、空暂存区通过；
其余 3236 个已跟踪文件 Hash 未变（包括既有六个 dirty 源码），两个既有未跟踪报告 Hash 未变。
最终报告编码、文件 Hash 与状态见 `final-files.csv`、`git-status-final.log`。

四文件 `clang-format --dry-run --Werror` 修改前、修改后及最终源码分别保存独立日志。
`binary_host_adapter_public.cpp` 前后退出 1，均为原有三处诊断，代码行/caret 上下文逐项相同；
其余三文件前后退出 0。见 `format-before-*`、`format-after-*`、`format-final-*` 及
`format-comparison-final.log`。未全文件 format，未新增格式差异。

## Debug / Release 专项

核对并复用 `out/build/comments-public-20261009/lab-lifecycle/build`：源根为本仓库，
H1/H2/Testing 开关 ON，MSVC v142 `14.29.30133`，未修改构建配置。
本轮开工未发现正在运行的 cmake/ctest/MSBuild/cl/ninja 主构建；严格串行 D/R。

从仓库根执行：

```powershell
cmake --build out/build/comments-public-20261009/lab-lifecycle/build --config Debug --target pae_protocol_lab_ui_binary_public_h2_tests pae_protocol_lab_ui_binary_public_header_tests --parallel 4
ctest --test-dir out/build/comments-public-20261009/lab-lifecycle/build -C Debug -R '^pae\.tools\.protocol_lab_ui\.(binary_public_h2|binary_public_header)$' --no-tests=error --output-on-failure -j 1
```

Release 使用相同命令，将 config 和 `-C` 改为 Release。
实际封装执行为 `run.ps1 -Phase Debug`、`run.ps1 -Phase Release`。
最后收紧两处注释措辞并补元数据借用与物理范围说明后，补跑
`run.ps1 -Phase Debug -LogPrefix final-`；该 Debug 与 Release 均对应最终四源码。

构建退出码均 0；Debug 初跑、最终 Debug 与 Release 均为两项 2/2 PASS，CTest 退出 0。
证据：`build-Debug.log`、`test-Debug.log`、`build-Release.log`、`test-Release.log`、
`final-build-Debug.log`、`final-test-Debug.log`。开工 `test-inventory.log` 中未构建的 exe 提示
仅是 CTest -N 预检，不当作执行成功；以后述实际构建测试为准。

Release 两目标生成项目均有 NDEBUG undefine，源/执行编码均为 UTF-8，见 `release-options.log`。
H2 测试源码有 `#if defined(NDEBUG) #error`，实际构建成功证明断言未被关闭。
header 测试是公开头编译及默认 DTO 探针，没有运行断言，不能当作丰富行为覆盖。
Debug（含最终补跑）warning 行数 0；Release C4819 为 0，有两条 D9025（/UNDEBUG 覆盖 /DNDEBUG），
未屏蔽，见 `warning-summary.log`；不宣称 Release 零警告。

## 未验证与停点

仅两个直接相关 headless 专项及必要依赖闭包；其中 headless 依赖包含兼容模块，
不表示扩大 legacy/ASCII/H1 修改或对它们做全面验收。
未启动 Lab/Qt smoke，未做人工 UI、Linux、全仓、全开关组合、installed SDK 包外、
SDK/体验包重制、硬件或真实协议现场验证。
既有三处格式问题仅记录，未修复；本轮无需要新增实施授权的已确认代码缺陷。
完成后向总控发送一次交接，停止写入等待复核，不自行宣称总控验收通过。
