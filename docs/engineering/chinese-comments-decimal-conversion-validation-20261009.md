# 第十四批 Decimal 转换实现中文注释验证（2026-10-09）

## 1. 接管、范围与依据

接管现场为干净 main@0e40741cebb283bf2a3438b940584eeaf68606d9，暂存区为空。
第十三批 YAML 注释及独立测试入口修复已在该提交中，本批不修改它们及其报告。
本次仅向以下两文件新增中文说明，共 37 行注释，另新增本报告：

- src/protocol_core/decimal_conversion_internal.h：内部前提、数学表示、双向契约及比较语义。
- src/protocol_core/decimal_conversion_internal.cpp：定容表示、算术步骤、极值、规范化与失败边界。

没有修改非注释 token、预处理指令、宏续行、接口、布局或执行实现；未修改 Codec、
编译器、公开头、测试、CMake、YAML、Lab 或旧报告。不启动后续批次。

依据为 [DEC-042B 契约](pae-dec-042b-decimal-conversion-contract-draft.md) 第1、3～5、8节，
[Core 历史报告](windows-msvc-2026-dec042b-core-slice.md)，以及现场
src/protocol_plan/plan_types.h、decimal_conversion_internal.cpp 的冻结描述推导，
plan_builder.cpp 的派生描述复算，Core 类型、转换调用、状态映射和最终重读复核。
本批未用早期文档中的下一阶段状态替代当前代码，也未重做 DEC-042B 产品实施。

## 2. 注释表达的实际语义

Decimal64 的值为 coefficient * 10^(-scale)，scale 是 0..18 小数位数，不是转换比例。
配置比例/偏置先在 Plan 层约分，并统一为 A/10^t、B/10^t；A/B 带符号，t<=18。
运行期 Decode 计算 N=raw*A+B，再规范化 N/10^t；Encode 对 logical=C/10^s，
取 u=max(s,t)，计算：

```text
raw = (C*10^(u-s) - B*10^(u-t)) / (A*10^(u-t))
```

对齐整数尺度后做精确除法，先拒绝非零余数，再检查原始整数类型与实际 Wire 宽度。
不通过浮点近似、舍入或截断扩大接受域，也不承诺每个合法配置覆盖整个 Wire 域。
编译期参数约分与运行期 Decimal 尾零规范化是不同步骤，没有新增通用约分算法。

Wide 使用低位在前的 8 个 uint32_t limb，SignedWide 分离符号和幅值，固定局部存储。
这不是宿主或 Wire 字节序；调用方已按字段字节序取出 raw_bits。本层不直接读写 Frame。
逐 limb 加减、双宽乘积及高半部检查、256 位长除法的商/余数与最高移出位均有针对性说明。
INT64_MIN 的幅值在无符号域处理，收窄先检查高 limb 和符号边界；全宽 mask 单独处理，
不求 1<<64。合法冻结描述及 byte_width=1..8 是内部调用前提，不声称这些 helper 是
可直接接受任意损坏描述的公开安全接口。

规范化仅消除小数位范围内可整除的尾零，零为 {0,0}；先规范化再检查 int64_t 系数。
Encode 即使系数为零也先检查输入 scale。固定容量内部异常与用户值不可表示区分；
Codec 负责映射状态和错误字段定位。raw_bits 仅在 EncodeDecimal 成功末尾写入；
Decode 输出仅在 OK 有效。整次 Codec 失败不交付，不等于底层 Buffer/槽位从未被改写。
DecimalEqual 检查合法 scale 后规范化数学值，允许 {123,1} 与 {1230,2} 相等；
最终复核重读实际字节再转换，不只比较缓存或原始结构成员。

这些是对现有实现与契约的注释，不是新的性能、完整输入域或线程安全保证。

## 3. 实际构建与专项测试

复用经核对的 out/build/comments-public-20261009/default-repo，不重配或扩大矩阵。
缓存为当前仓库、Visual Studio 18 2026、x64、v142 14.29.30133；公共 API、测试与
Schema 0.5 开关开启，PAE_BUILD_HOST_ENDPOINT_SLICE 已为 ON（前批留下的实际配置，
不是声称本次验证 Host）。本次没有执行网络、设置 CL/_CL_ 或改全局环境。

三个构建目标从实际 tests/protocol_core/CMakeLists.txt 和 tests/public_api/CMakeLists.txt 核对。
从仓库根运行，下方 B 表示上述实际构建目录；完整参数、退出及工具输出在本批日志：

```powershell
ctest --test-dir $B -C Debug -N -R '^(pae\.protocol_core\.decimal_conversion\.contract|pae\.protocol_core\.complete_record_codec\.first_decimal_allocation|pae\.public_api\.complete_record_codec)$'
cmake --build $B --config Debug --target pae_decimal_conversion_contract_tests pae_complete_record_first_call_allocation_tests pae_public_codec_tests -- /m:2
ctest --test-dir $B -C Debug -R '^(pae\.protocol_core\.decimal_conversion\.contract|pae\.protocol_core\.complete_record_codec\.first_decimal_allocation|pae\.public_api\.complete_record_codec)$' -V
cmake --build $B --config Release --target pae_decimal_conversion_contract_tests pae_complete_record_first_call_allocation_tests pae_public_codec_tests -- /m:2
ctest --test-dir $B -C Release -R '^(pae\.protocol_core\.decimal_conversion\.contract|pae\.protocol_core\.complete_record_codec\.first_decimal_allocation|pae\.public_api\.complete_record_codec)$' -V
```

实际严格先 Debug Build/CTest、再 Release Build/CTest；各命令退出 0，两配置各 3/3。
测试清单只用于确认注册；构建前清单提示部分目标程序尚不存在，不将清单当执行成功证据。
随后的实际构建和三项执行均成功，完整原始输出保留。

| 测试 | Debug / Release 实际输出 | 相关证据与限制 |
| --- | --- | --- |
| pae.protocol_core.decimal_conversion.contract | 每配置整套 checks passed，退出 0 | 独立字节向量、宽中间抵消、INT64_MIN/负比例、非法 scale、反算非整数、Wire/业务范围、SUM8 优先级、raw 诊断失效和内部/最终复核故障注入；程序未输出逐项成功计数，不能编造数量 |
| pae.protocol_core.complete_record_codec.first_decimal_allocation | 每配置 passed=1 failed=0 | 已构建 Plan/Workspace 后的首次 Encode/Decode，无可替换 new 计数增长；不是进程全部分配或完整输入域证明 |
| pae.public_api.complete_record_codec | 每配置 passed=57 failed=0 | 现有公开 Codec 运行时检查，含 Decimal 独立编码、规范业务值与记录 raw、转换失败定位；57 为该整套公开用例，不是 57 个 Decimal 算术用例 |

源码中的 Expect/普通条件返回、Runner::Check 和分配计数比较均为运行时检查，不用 assert
维持门禁；Release 生成项目为 MaxSpeed/NDEBUG，但这些检查不因 NDEBUG 消失。
本次没有新增或改写断言，没有用新生成的期望替换旧向量。
Build 日志未发现 warning/error；限定专项通过不等于全链资源、算术形式证明或产品验收。

## 4. 等价、格式、保护与证据

证据根 out/build/comments-public-20261009/decimal-conversion/，未覆盖旧批目录。

- takeover.ps1、takeover-status.log、baseline/、protected-hashes.json：原路径源码副本和接管。
- baseline-format 日志、initial-equivalence.log：修改前格式、首次 token/指令/续行对照。
- build-cache-audit.log、test-inventory.log、run-validation.ps1、validation-transcript.log：
  实际构建根、开关、目标清单和串行命令退出。
- build-{Debug,Release}.log、test-{Debug,Release}.log、cases-{Debug,Release}.log：
  本次构建、详细 CTest 和保存的 LastTest 输出。
- verify-comments.ps1、final-equivalence.log、final-format 日志与 final-diff-check.log：
  最终非注释 token、字面量、预处理指令和宏续行一致性，以及候选与保护核对。

校验器保留字符串/字符字面量及全部非注释 token；遇到不支持的 raw string 语法失败关闭，
本批两源码不含该语法。没有新增注释尾反斜杠；原宏/指令和续行不变。
两源码原 baseline 与最终 clang-format dry-run 均退出 0，没有全文件格式化。
两源码和报告严格 UTF-8 无 BOM；git diff --check 退出 0。
原 3249 个范围外 tracked 文件 SHA256 全部不变，包含前批源码、测试和报告。
候选严格限两源码与本报告，out 被忽略；新增候选定向路径/凭据检查通过，不冒充完整保密审计。

## 5. 停点与未验证边界

本批没有发现需要越界修复的明确实现问题；这不是独立全域算术审计结论。
未重跑隔离算术 spike/高精度 Oracle、全切片、所有容量组合、所有整数输入域、
SDK 包外/搬迁/打包、开关隔离、YAML/Lab/UI、网络、Linux、真实协议 Golden、硬件或现场。
不改变原有行为、资源布局、格式、指纹或既有验证结论，不引入性能或线程安全保证。

最终 main/HEAD 不变，两 tracked 注释修改、一 untracked 报告，暂存区为空。
未 Stage、Commit、Push、清理、下载、重打包或启动下一批。
已完成派发范围，向总控发送一次交接后停止写入，待总控复核。
