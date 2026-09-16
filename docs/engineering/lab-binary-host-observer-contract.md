# Lab Binary Host 观察首片：已确认契约

状态：2026-09-13用户授权“提交推送，再推进Binary Host契约细化”。
UX检查点已随`84644338b06be378e542b72f5eed03f48a2e2bd5`提交并推送。
用户随后确认本文六项，契约已冻结。2026-09-13又独立授权PAE最小raw候选观察接口及
Debug/Release验证；该PAE首片已完成，见[验证记录](host-raw-candidate-validation.md)。
用户随后授权规划并推进下一阶段。本轮完成独立非Qt候选物化子片：自有类型化DTO、raw关联、
局部复制预算和测试门禁，见[物化子片验证](lab-binary-materializer-validation.md)。
随后继续完成非Qt自有描述/枚举显示与物理byte/bit映射基础件，见
[描述子片验证](lab-binary-description-validation.md)。随后完成绑定前全Plan接受域扫描、
转换说明元数据及最小完整记录观察的候选/描述身份关联，见[准备与关联验证](lab-binary-prepared-validation.md)。
另已补[实例及重绑预算基础门禁](lab-binary-resource-validation.md)和
[非Qt双流Submit/Continue及冻结容量门禁](lab-binary-stream-validation.md)，随后完成
[非Qt每流草稿/当前结果及切换保存门禁](lab-binary-saved-state-validation.md)及
[非Qt Host Encode与TX保存](lab-binary-encode-validation.md)。Encode回调故障需重新准备Session，
现有Host Reset仅支持Decode；后续UI必须明确恢复路径且不自动重发。
UI接线、副本预算执行点及旧Binary桥替换仍未完成；局部证据不代表Lab验收。
随后完成[非Qt总体复核](lab-binary-nonqt-audit-validation.md)：修复错误方向发布与拒绝代次丢失，
补CRC/Decimal/双流隔离组合回归；下一步细化UI及旧桥移交，不直接进入UI实施。
Qt依赖检查点已随`c0da00f9e41a6edbe3311c4fcc66508a11b59d9d`提交推送，后续消费仓库内
`third_party/qt`固定副本；本契约的Git交付和代码实施分别授权。
2026-09-13用户随后确认UI四项细则，并授权落盘后实施UI第一片；最新范围以第7节为准。
第一片尚待实现和验证，不包含后续Encode/流式UI或Git交付授权。

## 0. 已核对事实与必须处理的差异

| 来源 | 当前事实 | 对本契约的约束 |
| --- | --- | --- |
| `src/host_endpoint/host_endpoint.cpp::Session::Create` | 仅接受0.9、0.10、0.11 | Binary首片限定0.9，不顺带扩大Host历史Schema接受域 |
| `tools/protocol_lab/v06_execution.cpp::ExecutionBridge::AdoptCompiledPlan` | 旧Binary桥只接受启用的0.5–0.8 | 不能靠放宽版本判断或再次执行旧桥实现Host显示 |
| `src/config_compiler/config_compiler.cpp`的0.9解析分支 | 接续Binary转换、长度和有界负载能力 | 不能把现有简单UINT64示例当成完整类型覆盖 |
| `src/host_endpoint/host_endpoint.h::Candidate` | 已补RawIntegerCount/GetRawInteger，成功回调内按索引拷出raw整数 | 只含转换整数字段；Lab仍需按FieldRef关联，不能将raw索引当作所有字段索引 |
| `v06_execution.cpp::MaterializeFields` | 从workspace逐项读取raw整数，关联转换字段 | 不允许由逻辑小数反算raw，也不能另做Decode取得raw |
| `tools/protocol_lab_ui/description_mapping.cpp` | 固定位mask按执行Plan解析；有界负载按实际帧长解析 | 可复用映射算法，但须核对新身份、成功前提及范围 |
| `tests/host_endpoint/host_endpoint_tests.cpp::Binary` | 已有三种Framer、UINT64、完整记录及Encode用例 | 属于已有PAE证据，不是新Lab Binary验收 |

与此前“优先不扩PAE”的初步评估相比，发现raw整数观察缺口。推荐保留DECIMAL64能力，
只补宿主候选的最小raw读取接口；若选择完全不改PAE，则必须明确排除转换字段并在加载时拒绝。
本草案推荐前者，不用空Raw列掩盖能力缺失。

## 1. 接受域与默认关闭门禁

推荐首片：Schema 0.9 Binary；Decode完整记录、流片段和Encode完整记录。
流策略仅`fixed_length`、`sync_fixed_length`、`sync_length_field`，参数仍由Compiler/Framer校验。
支持已启用能力的UINT64、INT64、BOOL、ENUM、BYTES，以及转换后的DECIMAL64；
覆盖大小端、位字段、现有SUM8/CRC、计算长度及有界BYTES负载，不新增算法或布局语义。
DECIMAL64是逻辑结果类型，不新增名为DECIMAL64的wire codec。

不是任意Schema×策略×布局笛卡尔积：仅接受Compiler、Core与Framer共同支持的组合；
在注册前遍历准备显示的全部Message/字段，未支持组合明确拒绝整次准备，不半加载、不回退执行。
0.5–0.8保留旧Lab路径，0.10/0.11保留现有ASCII Host路径；不将ASCII改为Binary，不自动改写JSON。
同一文档按已编译Plan选择适配路径，不引入混合Schema文档。

新Binary Lab能力单独默认OFF，显式依赖Host、V09和所需Binary编译能力，缺依赖配置失败。
不自动扩大CLI/Evidence或旧ExecutionBridge的接受域，不升级Qt，不改变Core/Framer依赖方向。

## 2. 候选观察、类型化DTO及所有权

先在PAE宿主层补可选raw整数只读访问：仅本次成功Candidate回调有效，提供count及按index拷出
RawIntegerValue的受检读取；具体C++命名由实现确定，不暴露可变workspace，也不将其指针交给UI。
PAE已实现命名为`RawIntegerCount()`及`GetRawInteger(index, output)`；无转换整数字段和
ASCII字段不产生raw条目，越界读取返回false且不改output。FieldRef依旧借用Plan。
失败Candidate的字段和raw均为空；无observer时不新增raw拷贝或热路径分配。
读取必须检查边界，不允许跨回调保存访问器；枚举/字段的Plan引用同样只在回调内有效。
这只是内部诊断接口，不承诺并发读取或公共ABI；Core与Framer不改协议语义。

Lab非Qt适配层在回调内形成自有结果：
- 身份：Tab/load/session修订、绑定、flow、generation、operation、Message ID及索引。
- 帧：自有完整候选字节；失败原帧单独标为诊断材料。
- 字段：稳定ID/索引、明确类型及对应整数/布尔/字节/小数值，不全部降为BYTES。
- ENUM：复制raw、known、已知项ID/显示文本；未知枚举保留unknown，不伪造已知项。
- DECIMAL64：复制coefficient/scale；raw按同Plan/Message/Field关联并校验有且仅有一项。
  缺失、重复或类型不匹配为结果物化失败，不展示部分成功字段，不从逻辑值反算raw。

不得把借用FieldRef、EnumValueRef、ByteView或Plan指针保存在显示DTO；销毁Session后旧DTO仍可读，
但不能以旧DTO身份执行新Session。Encode输入在活动Plan作用域重新解析枚举ID及字段身份。
正常观察STOP仍交付当前成功业务结果一次；复制/关联异常停止且抑制当前业务交付，保留实际消费并要求Reset。

## 3. 字节、位范围与执行层复用

固定范围从当前Message冻结执行Plan取得；位容器遵守大小端与数值位mask到物理字节mask的映射，
禁止以field序号或逻辑值猜物理位置。范围均为当前帧内零基偏移，不声称全流绝对偏移。
有界负载仅在本次Core成功且实际帧长通过范围检查后，按header/trailer及实际帧长推导范围，
再与本次BYTES长度交叉验证。payload末尾的校验存储位置使用动态位置；零长度不高亮任何字节。
范围越界或关联不一致按物化失败处理，不裁剪出貌似有效的高亮。

失败Candidate只展示原帧、Core错误及可确认的失败定位；不得残留上一次成功字段或成功高亮。
无候选、输入早拒绝与协议失败分别显示。API OK、候选数、业务成功数不能互相替代。

复用现有非Qt描述映射及格式化算法，可作限定提取，但不调用旧ExecutionBridge::Inspect/Encode
获取第二份“复核”结果。Encode只执行Host Encode一次，展示TX输出与本次输入/执行Plan对应关系，
不经RX Decode回环证明成功。旧完整记录路径继续独立回归，不做全项目适配器重构。

## 4. 流驱动、切换与生命周期

沿用ASCII Host的显式endpoint/action/pipeline绑定；每Decode绑定两条串行逻辑流，Encode一条。
Submit先整次校验Hex和容量，冻结chunk后最多一次Push；单个成功或失败候选均STOP。
Continue严格提交未消费后缀；只有has_internal_work才能空Push，不强行刷新半包。
候选、decode成功、观察完成、业务输出、Framer丢弃/畸形计数分离；丢弃字节不伪造Core候选。
fixed_length失败不额外找同步头；sync策略保持既有重同步及预算语义。

每流保存当前草稿、冻结游标和一份当前结果，无结果历史/队列；切绑定、Flow、Tab不提交或Reset。
Reset只清目标流并更新generation。重绑先准备独立Plan/Session及显示资源，成功且确认后原子发布；
准备失败或取消保留所有旧流、当前选择和草稿。Reload保留既有不同语义，确认丢弃后加载失败不恢复旧半包。
关闭、重载和重绑检查所有受影响流，包括非选中流、pending及冻结后缀。
延迟编译结果按完整修订身份拒绝，旧结果不能覆盖新视图。禁止同Session回调重入。

## 5. 有界资源与构建顺序

建议沿用明确上限而重新核算Binary副本：最多64绑定/64总通道、身份256字节，
单帧最大65536字节、描述字段总数1024、最大字段BYTES长度求和1MiB、描述报告4MiB。
单chunk容量C=min(65536,effective_max_submit_bytes)，不是单帧长度M。
单适配实例准入128MiB、同Tab新旧准备峰值256MiB；这些是计费上限，不是进程RSS承诺。

准入报告必须逐项列出：Plan、Session工作区、sidecar/自有描述、每流冻结字节、输入UTF16/Hex字符串、
当前及临时DTO、raw整数与枚举复制、UI视图/预览副本和Encode输入输出。共享只计一次，真实共存副本逐份计费。
乘加先检查溢出，字符串/容器采用可审计最大容量；复制临时峰值计入预算，禁止借用ASCII公式宣称已覆盖。
至少在创建、回调复制、切换保存、重绑准备四个点验证预算；边界故障注入证明不半发布、不污染其他流。
旧桥移交指实现接线的职责分离，不新增跨Schema原子迁移事务。普通Reload确认后释放旧实例，
不要求旧桥保留到新Host发布；只有实际共存的对象才逐份计费，同一Plan不得出现第二个owner。
同Tab Binary Rebind则必须核算旧实例及新候选完整峰值，具体收口见第7节。

先验证PAE raw观察扩展，再实现非Qt Binary适配/物化与描述，最后串行修改document_session、UI和CMake。
采用现有Lab Qt 5.13.x x64/v142同套全链编译；不混用其他MSVC产物。
预算明细及数据结构须在非Qt阶段提交复核，未闭合前不得进入UI接线。

## 6. 验收矩阵与停点

以下为分阶段验收要求；PAE及非Qt各子片已有本文顶部链接的专项记录，
资源预留与非Qt验证不代表实际UI副本、发布事务或人工验收已完成：
1. PAE raw观察：正/负转换、无转换、早拒绝、失败、零字段、越界读取、raw关联及observer遗漏；
   证明一次Core调用，正常STOP及异常消费/业务计数保持原契约，无observer旧路径不新增分配。
2. 非Qt DTO：所有类型、已知/未知枚举、大小端跨字节位mask、整数极值、Decimal精度及有界负载零/最大值；
   析构Session后自有DTO存活，错Plan/字段关联和复制失败明确拒绝。
3. 三种Framer：任意切分、粘包、同步头跨块、坏候选后好候选、非法长度重同步、工作预算、
   STOP精确后缀和内部pending；一个动作不自动耗尽所有输入，零进展不忙循环。
4. 状态：双流交错、Reset隔离、Encode不清半包、跨Tab、重绑失败/取消/成功、未选流关闭及Reload保护。
5. Windows Debug/Release：新增针对性测试及受影响旧Binary/ASCII/Host回归；默认OFF及缺依赖门禁。
   保留独立手算期望，不把Encode→Decode自循环作为唯一向量，不累加不同批次测试冒充最终全量。
6. 人工UI：三种策略各一条分片与恢复链、类型/Raw/Logical和位高亮、绑定切换/资源拒绝；
   验收说明必须给出最终EXE和所有合成JSON完整路径。

现成入口为`examples/config/synthetic_stream_framing_slice.pae.json`（0.9，三种策略，主要UINT64）；
它不足以覆盖上述类型/转换/有界负载，实施阶段另补从零设计的公开合成向量，不使用客户协议。
本文六项已获用户确认；随后另行实施授权。自动验证完成后交人工验收，Git交付单独授权。
不包含网络、串口、自动转发、UTF-8、持久化、历史记录、Evidence、Linux/硬件/现场或性能认证。

## 7. UI细则确认与第一片实施授权（2026-09-13）

用户已确认下述四项细则，并授权先落盘契约、再完成第一片实现。此节收口此前旧桥移交
表述及UI初始/恢复行为，不改变PAE协议语义，不授权后续UI子片或Stage/Commit/Push。

### 7.1 已确认四项细则

1. 不新增旧桥原子迁移功能。Schema 0.5–0.8保留旧桥，0.9使用PreparedBinary，
   0.10/0.11保留ASCII路径。Reload确认丢弃后销毁旧实例再加载，失败不恢复旧状态；
   Rebind独立准备候选并计入真实新旧共存峰值，确认后原子发布。不得放宽旧桥版本判断。
2. 0.9初次加载进入“Binary Host未绑定”：展示描述和Binding Draft，用户Apply后才创建
   执行Session，不自动激活默认device绑定。首次Apply失败保留可重试的配置/草稿；
   不得先消耗唯一配置owner再把失败留成无法重试的未绑定状态。可独立编译候选，
   但其Plan/sidecar/描述与未绑定对象的实际共存必须计费。
3. Raw仅展示实际提供的数据：BOOL没有独立raw时显示“未单独提供”；Encode Raw显示
   “未观察”。转换字段使用实际raw_integer，禁止logical反算或二次Decode补展示。
   conversion逻辑类型显示DECIMAL64，原wire type保留在详情；旧路径展示不顺带改写。
4. Encode callback/copy故障仅禁用故障TX绑定，其他RX流允许继续；用户重新Apply时
   先准备新Session，再确认整体替换，失败/取消保留旧状态，不自动重发。
   本项为后续Encode UI冻结行为，第一片不实现Encode UI或扩大Host Reset。

### 7.2 第一片范围与最低安全基础

- 新增独立默认OFF的Binary UI接线门禁，显式检查UI、Binary materializer、Host及所需
  Schema/Core/Framer依赖；不隐式打开缺失能力，不改变UI OFF隔离。
- 实现0.9路由、明确backend身份及唯一执行owner、未绑定描述、显式Apply和完整记录Decode。
  第一片只开放完整记录Decode操作；不暴露尚未接好的Encode、Submit/Continue或流式状态操作。
  不可执行的方向/模式应明确禁用或提示未实施，不回退到旧桥。方向数据可保留，但不能误执行。
- 描述及成功结果覆盖UINT/INT/BOOL/ENUM/BYTES/DECIMAL64、实际raw、枚举known/unknown、
  大小端byte/bit映射及有界负载；只调用一次Host Decode。失败清除旧成功字段和高亮，
  只显示接口确实提供的诊断帧/失败定位；本地输入失败不调用Host。
- 从第一片建立有界UI description、active view及临时替换、Hex frame/mask、输入草稿的
  实际复制门禁；按已声明计费模型检查容量与乘加溢出，不将预留公式当作执行点证据。
  未开放编辑器不要求实现，但所有实际分配及候选共存均须纳入对应预算，避免重复计费。
- 首次Apply及重复Apply均须安全：候选准备失败/取消不改变旧选择、草稿或当前结果，
  通过所有准备与预算检查后才发布。采用不逃逸借用引用的owner移交，发布阶段不再做可能失败的复制。
- 在第一片建立请求身份校验（Tab/document、load、session、request及backend/config身份），
  防止迟到结果覆盖当前状态。Reload和Close必须安全取消/失效化pending；关闭沿用多Tab
  两阶段只读确认骨架，取消保留用户可见草稿与结果。完整流状态组合留后续片验证，
  但不能以“生命周期最后实施”为理由交付当前已开放操作的不安全版本。

### 7.3 执行分工、验证与停止条件

- 《Lab应用推进》为唯一实现写入方：允许tools/protocol_lab_ui/及tests/protocol_lab_ui/
  的必要文件、根CMakeLists.txt的限定门禁与链接接线、从零设计的公开合成JSON，
  以及专属docs/engineering/lab-binary-ui-stage1-validation.md。不得覆盖既有CMake修改。
- 《子任务推进》只读复核PAE/非Qt接口消费、owner/预算与验收断言；不并发修改PAE、
  非Qt模块、共享文档或构建目录。若存在无法绕开的接口缺口，报告总控再定向授权。
- 总控负责本契约、路线图及文档索引，实施方不同时改这些文件；各方不创建临时子智能体。
- 第一片先补能暴露0.9未接线/发布缺口的针对性测试，再实施；Windows x64/v142
  Debug/Release分别构建并跑新增headless及Qt smoke，保留受影响0.5–0.8和0.10/0.11回归。
- 必测：初次未绑定、Apply失败后重试、准备/复制/预算失败及取消不半发布、成功Decode后
  协议失败/输入失败不残留成功展示、所有支持类型与实际范围、旧completion拒绝、
  Reload失败不恢复、多Tab关闭Yes→No保留、唯一执行路径；默认OFF、缺依赖拒绝、
  项目测试开关关闭的Release UI构建分别提供证据。不混淆BUILD_TESTING与PAE_BUILD_TESTING。
- 本轮不重复无变化的全部非Qt验证，不把模拟/故障接缝称为全局OOM或RSS验证。
  交付人工验收步骤时列出最终EXE、所有JSON绝对路径及SHA-256；尚未人工验收明确标记。
- 完成后向总控01a04601-757d-7bb1-8254-61dde4954d74主动反馈一次交接摘要，
  包含范围、文件、命令/日志、未验证项、风险和Git状态；停止写入，等待总控复核。
  后续Encode、流式保存及完整生命周期组合验收分别派发，不自动连续实施。

相关：[路线图](post-dec040-roadmap.md)、[ASCII Host契约](lab-host-endpoint-observer-contract.md)、
[UX验收](lab-representation-ux-validation.md)。
