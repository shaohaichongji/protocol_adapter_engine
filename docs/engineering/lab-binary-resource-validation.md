# Binary 实例与重绑预算基础门禁

后续状态：冻结预留已在[双流驱动子片](lab-binary-stream-validation.md)落实为准入后固定容量分配与运行时检查；
随后[非Qt每流保存门禁](lab-binary-saved-state-validation.md)已落实草稿和当前结果预留。
下文保留本预算基础检查点的历史范围，UI副本及Encode预留仍待落实。

2026-09-13，用户同意继续推进整体内存预算。保留此前未提交修改；本轮不改Qt、不接UI、不提交推送。

## 实现

- 新增`tools/protocol_lab_binary/resource_budget.h/.cpp`，所有乘加受检。
- `PreparedBinary::Create`增加可收紧的ResourceLimits（128MiB实例、256MiB重绑）和可选previous。
  previous必须是同Tab的存活实例，旧实例费用读取其自身报告，不接受调用者直接填旧费用作为生产入口。
- Plan、Session、自有描述、绑定容器及身份串采用现有计费报告或实际capacity；PreparedBinary对象
  扣除已在描述中计费的内嵌OwnedDescription，避免重复计算该对象。Plan不在Host Session账本中，单独计入。
- 源sidecar只计准备共存峰值，正常返回后不计常驻；旧实例按含容量预留的instance admission计费。
- 复制描述及绑定后、创建Host工作区前先核验基底与预留，再把剩余实例/重绑额度作为Host预算上限。
  Host返回后按其报告再次核验；超限不发布新对象，旧对象不修改。

## 预留模型（不代表已分配或已完成运行时门禁）

设N为总通道数，D为Decode通道数，E为Encode通道数，B为单候选复制上限，C=65536。

| 项目 | 容量预留 |
| --- | --- |
| 每通道保存DTO | N×B；已有raw、enum、范围均在DTO预算内 |
| 串行回调临时DTO | B |
| Decode冻结后缀 | D×C；未分配Framer缓冲的第二份，这里预留的是未来Lab自有后缀 |
| UTF16草稿及切换临时 | N×2×(4C+1)×2字节 |
| 当前UI视图及替换临时 | 2×B |
| Hex预览及临时 | 2×(3C+1)×2字节 |
| Encode输入副本及输出 | E×(B+C) |

常驻计费加上述预留为instance admission，再加新源sidecar为preparation peak，
再加旧instance admission为replacement peak。前两项均不得超过实例限额，最后一项不得超过重绑限额。
模型有意保守；数量允许64通道，不保证64通道在任意B下都能通过容量门禁。

## 验证

沿用无Qt的Windows x64/v142配置。扩充`pae.protocol_lab_binary.prepared`：
资源分项非零、通道DTO预留、精确上限通过、减1字节失败、算术溢出拒绝、
旧新费用相加、同Tab限制、重绑拒绝后旧实例仍可Observe且generation不变、64通道容量超限拒绝。
该目标也继续执行上一轮完整记录身份/范围/准入测试；Debug与Release各1/1通过。
TESTING=OFF、UI=OFF的Release内部库构建通过。`git diff --check`通过。

```powershell
cmake --build out/build/windows-msvc-binary-materializer-noqt --config Debug --target pae_binary_prepared_tests --parallel 4
ctest --test-dir out/build/windows-msvc-binary-materializer-noqt -C Debug -R '^pae.protocol_lab_binary.prepared$' --output-on-failure
```

Release替换命令中的Debug。本轮未重复整配置测试，未做UI或硬件验证。

## 仍未闭合

本API接收已编译Artifacts，因此不约束Compiler解析时的临时内存；预算检查前的描述/绑定构造
仍受原本局部硬限额控制，不声称任意收紧总额度都能阻止检查之前的所有分配。
计费不包含allocator/debug proxy、全部调用栈、任意外部保留的DTO副本，不是RSS承诺。

当前只消费现有PreparedBinary实例报告；旧Binary桥的Plan/Core/描述跨路径移交未实现，
不能传null previous来宣称完成旧桥重绑计费。UI修订号仍由调用者提供，不是跨进程身份。
后续必须把预留变为冻结后缀、草稿保存、UI复制及Encode的实际容量检查；创建与重绑两点通过
不代表契约要求的创建/回调/切换保存/重绑四点均完成。现有回调已有局部B限制，但尚无UI保存入口。
因此本轮不继续混入流驱动或UI；下一步优先实现双流状态与预算预留的运行时消费门禁。
