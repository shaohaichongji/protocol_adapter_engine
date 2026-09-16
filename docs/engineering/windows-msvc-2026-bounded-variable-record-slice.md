# 有界变长完整记录 Windows 验证报告

日期：2026-09-09。代码基线：`7d2ef4f`，验证对象为其后的未提交工作树。
状态：`IMPLEMENTED / WINDOWS OFFLINE VERIFIED / PENDING REVIEW`。本报告不构成提交批准。

## 实现范围

- 默认关闭Schema 0.8 Compiler/Core能力与独立Lab开关；固定Message继续可用，新布局只接受固定
  头部、一个动态输入BYTES载荷和可选紧邻SUM8/CRC尾部。
- Loader、Domain Validator、Resource Budget、Budgeted Draft、Builder和Frozen Plan贯通实际
  载荷上下限、字段索引、头/尾/帧范围；Builder对损坏索引、范围、派生帧长和动态锚点失败关闭。
- Core按实际F执行结构唯一性、容量、长度、完整性和字段顺序；Encode输出及最终复核只使用实际F，
  Decode BYTES继续借用输入Frame。首次变长Encode/Decode的可替换`new/new[]`分配为0。
- Lab使用Result/指纹0.9和Record 0.10，Event 0.7及Values保持原代；成功/失败Bundle、Replay A/B、
  自Compare、跨文件绑定和自洽语义篡改拒绝均通过实际读取验证。
- 示例宿主同步处理空/中间载荷，回调方显式复制借用载荷；长度或载荷边界失败不触发成功回调。

## 独立向量与专项覆盖

公开A5样例的独立常量为：空载荷`A5 03 A8`，载荷`10 20`为`A5 05 10 20 DA`，最大载荷
`00 FF 01`为`A5 06 00 FF 01 AB`。另验证payload scope、无完整性尾部，以及独立计算的
CRC-16/CCITT-FALSE动态尾部`A5 06 10 20 47 42`。补充固定SUM8范围、SUM8/CRC真正空动态范围，
以及2/4字节大小端实际frame长度独立常量。收口复核新增`logical = raw × 2 + 1`的非恒等
Decimal头字段：空载荷为`A5 05 00 03 AD`，载荷`10 20`为
`A5 07 00 03 10 20 DF`；均由常量手算后分别检查Encode和Decode，不是仅用Encode结果回喂。

专项自动化覆盖：上下限及Wire表示能力、payload引用、禁止frame matcher、动态范围边界；实际
required_size、非零预填输出、长度先于完整性、失败零字段、过短结构、过长Encode零写入；冻结
描述、变量候选索引、Plan资源、损坏Draft及实际范围操作计数。Lab覆盖Result/Record/Event版本、
精确JSON路径、原Bundle不变、成功链式Replay、失败链式Replay和低层Hash自洽后的语义拒绝；
非恒等Decimal与动态载荷的Result 0.9、Record 0.10及Replay A/B也实际读取通过。

## 实际命令与结果

使用Visual Studio 18 2026、MSVC 19.51、Windows SDK 10.0.22621.0。Debug/Release共享目录串行。

```powershell
cmake --build out/build/v08-lab --config Debug
ctest --test-dir out/build/v08-lab -C Debug -E "udp|loopback|network|exchange" --output-on-failure
cmake --build out/build/v08-lab --config Release
ctest --test-dir out/build/v08-lab -C Release -E "udp|loopback|network|exchange" --output-on-failure
```

第一次集中审查修复后两配置各43/43通过；当时配置编译器内部合同94/94。明确排除UDP及Exchange测试，
没有网络收发。

```powershell
cmake --build out/build/v08-business --config Debug --target pae_business_embedding_tests
ctest --test-dir out/build/v08-business -C Debug -R pae.examples.business_embedding.contract
cmake --build out/build/v08-business --config Release --target pae_business_embedding_tests
ctest --test-dir out/build/v08-business -C Release -R pae.examples.business_embedding.contract
```

业务宿主Debug/Release各1/1通过。

Product-only与Lab-on/Testing-off分别在`out/build/v08-product-only`和
`out/build/v08-lab-no-tests`完成Debug/Release构建，四次`ctest -N`均为`Total Tests: 0`。
单独打开Schema 0.8 Compiler但缺少Loader/Core/0.7依赖、或单独打开Schema 0.8 Lab时，CMake均
按预期以`FATAL_ERROR`拒绝（退出1）。
实现阶段日志位于`out/v08-final-debug.log`、`out/v08-final-release.log`、
`out/v08-business-tests.log`、`out/v08-isolation-configure.log`和
`out/v08-isolation-build.log`；负向门禁见`out/v08-negative-gates.log`。中断后新增跨Pipeline
断言的两配置复核见`out/v08-post-matrix-focused.log`，中断前完整Debug日志保留为
`out/v08-debug-offline.log`。Schema 0.8固定Message与扩展独立向量的两配置复核分别见
`out/v08-fixed-v08-focused.log`和`out/v08-expanded-vectors.log`。

## 集中审查纠错

总控集中审查前的执行侧复核独立确认并关闭三项合同缺口：

- 位容器原先可越过有界记录固定头部，直到Builder才以内部合同错误拒绝；Domain Validator现按
  容器完整范围在公开配置阶段拒绝，Builder保留损坏Draft防御。
- 有界Message原先只限制“至多一个”计算长度，未要求“恰好一个”；Domain Validator和Builder
  现均要求唯一计算长度，避免以输出缓冲区默认零值冒充长度字段。
- Schema 0.8 Loader为识别载荷允许BYTES省略`byte_length`，但原先没有把例外收窄到
  `payload_field_id`所指字段；Domain Validator及Builder现拒绝其他零宽BYTES字段。

前两项修复前CLI复现与修复后精确JSON Pointer诊断保存在
`out/v08-review-before-after.log`；专项Debug/Release 2/2见`out/v08-review-focused.log`。
修复后完整离线矩阵见`out/v08-review-final-debug.log`和
`out/v08-review-final-release.log`，两配置均43/43；业务宿主及两类隔离构建复核见
`out/v08-review-business-isolation.log`，编译器94例逐项输出见
`out/v08-review-compiler-internal.log`，格式、JSON及diff门禁见`out/v08-review-quality.log`。
上述是本次集中审查后的新增证据，不回溯改变早期92例编译器合同及实现阶段日志的历史事实。

第二次集中复核又独立确认并关闭三项P2：固定头部覆盖检查曾被bounded早退跳过；Builder曾接受
bounded的REGION计算长度；Builder曾信任作者自报trailer宽度而未从无校验/SUM8/CRC规则重算。
修复不采用整帧清零，而是在Compiler和Builder只对固定头部执行完整覆盖门禁；动态payload和
trailer仍由既有执行描述符写入。合法Encode使用`0xCC`与`0x5A`两种预填Buffer得到相同字节。
Builder损坏Draft分别命中`FRAME_NOT_FULLY_DEFINED`或`INVALID_MESSAGE_PLAN`，并保持真实
Compiler→Builder链上的无校验、SUM8、CRC合法独立向量通过。

修复前专项失败（Compiler 94/101、Core空洞负例未命中）见
`out/v08-review-round2-before.log`；其中Compiler的5项失败是目标Builder缺口，另2项是手工合法
Draft没有携带Compiler批准的精确内存报告而被末端计费门禁拒绝，已改由真实Compiler能力链中的
无校验/CRC合法向量承担对照，不把测试构造问题记为产品缺陷。公开空洞配置修复前成功输出
`A506001020DB`、修复后配置阶段
退出4及精确header诊断，见`out/v08-review-round2-header-gap/before-fix.json`和
`after-fix.json`。修复后Debug专项2/2见`out/v08-review-round2-focused-debug.log`，Compiler
内部Debug/Release各99/99见`out/v08-review-round2-compiler-internal.log`；完整离线矩阵
Debug/Release各43/43见`out/v08-review-round2-final-debug.log`和
`out/v08-review-round2-final-release.log`，业务及隔离复核见
`out/v08-review-round2-business-isolation.log`。本批次仍未执行网络。

提交前只读总审查又实际复现Schema 0.8继承遗漏：合法有界Message包含既有Decimal conversion时，
Core的转换Plan门禁未接受0.8，Encode退出5并报告`PAE_LAB_CODEC_INVALID_PLAN`；原始失败证据保留于
`out/v08-readonly-precommit-decimal/result.json`。同轮扫描确认确定性Plan Snapshot也将0.8误标为
0.1并省略conversion、integrity、computed length及bounded layout；专项修复前失败见
`out/v08-decimal-fix-snapshot-before.log`。

限定修复只在既有V08编译宏下补齐0.8接受域，并为0.8 Snapshot记录继承描述符和有界布局；不改变
旧代接受域、Result/Record/Event、指纹或Decimal算术。修复后专项Debug 2/2见
`out/v08-decimal-fix-focused-debug.log`；完整离线Debug/Release各43/43见
`out/v08-decimal-fix-full-debug.log`和`out/v08-decimal-fix-full-release.log`。另在未启用V08的
Schema 0.7构建中复核Compiler、Decimal Core及Length Core，Debug/Release各3/3，见
`out/v08-decimal-fix-v07-compat.log`。本次小整数手算向量只证明有界布局与既有Decimal能力正确共存，
不替代DEC-042B的高精度独立Oracle边界，也未执行网络或重新运行无关隔离矩阵。

随后针对空载荷的总控诊断实际复现两条失败：Inspect `A5 03 A8`在Core成功后因
Result共享校验拒绝空BYTES，终端退出10/`PAE_LAB_C3_INTERNAL_ERROR`；Values 0.4的
`"hex":""`在Codec前以退出5/`PAE_LAB_C1_VALUES_INVALID`拒绝。修复前隔离输出保留于
`out/v08-empty-payload-diagnostic`，该路径被Git忽略。

按确认的版本边界，新增Values 0.5仅为Schema 0.8接受显式空BYTES，并使Result 0.9
Writer、Reader和指纹共用同一空值配对规则；Values 0.4、Result 0.8及旧Schema仍拒绝。
修复后空Encode得到`A503A8`，空Inspect解析出BYTES空值，两者Bundle均完整读取、
自Compare且Replay A/B为EQUAL；`min_payload_length=1`仍在Core返回
`PAE_LAB_CODEC_BYTES_LENGTH_MISMATCH`及退出5，失败Bundle可读且Replay保留当前失败。

本次针对性CLI Debug/Release各1/1，格式与执行隔离单测Debug/Release各2/2；全离线
标签矩阵各21/21，排除UDP的全切片Debug/Release各43/43。日志分别见
`out/v08-empty-closure-focused-debug.log`、`out/v08-empty-closure-focused-release.log`、
`out/v08-empty-closure-final-isolated-test-debug.log`、`out/v08-empty-closure-final-isolated-test-release.log`、
`out/v08-empty-closure-full-test-debug.log`、`out/v08-empty-closure-full-test-release.log`、
`out/v08-empty-closure-final-all-slices-debug.log`和`out/v08-empty-closure-final-all-slices-release.log`。断电后还发现
隔离format测试目标未继承V08编译宏，修正CMake后重新编译才确认新用例真正执行。

## 验证边界

本轮未运行UDP/Loopback、人工NetAssist、Linux、真实协议Golden、硬件、现场或正式性能测试；
公开样例为`SYNTHETIC_FROM_SCRATCH`，不代表任何真实协议。未实现流式切帧、多段载荷、TLV、
Runtime/Session、GUI或稳定公共API。Windows通过只证明本报告列出的离线配置与自动化边界。
