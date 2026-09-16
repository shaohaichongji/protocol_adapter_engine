# PAE-DEC-041 SUM8完整记录校验契约

日期：2026-09-06。状态：11项范围原则及第3～7节四组补充方案均为CONFIRMED（已确认）；
实现与范围内审查已完成，已随`4d923e9`提交并Push；以下实施轮记录保留当时证据边界。

## 1. 已确认范围

用户已确认：

1. 仅固定COMPLETE_RECORD（完整记录），每Message最多一个SUM8校验规则。
2. 无符号字节求和，初值0、模256；不依赖char符号性。
3. 单个非空连续范围，Frame零基偏移，校验字节位于范围之外。
4. 校验规则独占1字节，不与普通字段、位容器或fixed_bytes Matcher重叠，不作为业务输入字段。
5. 收发Message分别配置，不要求镜像；真实接收算法证据缺失时只用人工接收样例。
6. Encode先写字段与位容器，再生成校验，最终复核成功才交付有效Frame。
7. Decode在唯一结构候选上校验后交付字段；失败字段数为0，不以checksum消除结构歧义。
8. 区分完整性失败、未知消息及结构问题；当前执行与历史比较分离。
9. 编译期冻结规则，热路径不解释字符串、不新增分配；计费与操作量有界。
10. Schema、Lab状态和指纹影响须显式评估并冻结版本组合；保留旧夹具回归。
11. 独立向量、异常路径、失败Replay链及Windows Debug/Release和隔离构建验证。

排除CRC、XOR、LRC、多段/动态范围、多校验依赖、完整Receive Gate（接收门禁）、线程、路由、
传输管理及生产替换。SUM8只提供有限的意外损坏检测，不提供认证、防篡改保证；某些多字节
变化可抵消，测试不能宣称检测所有报文损坏。

## 2. 实施前基线（历史）

`3b23db9`提供位字段和Lab多版本证据；Core当前没有完整性失败状态或SUM8规则。
现有Schema 0.1/0.2及Lab旧离线0.1、UDP0.2、位字段0.3是兼容基线。
本文不修改上述已实现契约；用户已确认下列补充选择并授权本轮实现与验证。

## 3. 配置契约（补充拍板A，已确认）

Message使用可选单对象`integrity`，而非预留任意算法数组；缺省表示无校验，拒绝null。
已确认且可加载的形态如下：

```json
"integrity": {
  "algorithm": "sum8",
  "range": { "byte_offset": 1, "byte_length": 4 },
  "storage": { "byte_offset": 5 }
}
```

存储宽度固定1字节，省略width与byte_order（字节序），不允许用户覆盖初值或输出异或值。
对象严格拒绝未知属性、浮点偏移、负数及超范围整数。范围表示为半开区间
`[byte_offset, byte_offset+byte_length)`，用减法边界判断避免加法溢出。

校验存储字节参与Frame完整覆盖检查，输入字段枚举、FieldRef及字段数量均不包含它。
范围可覆盖普通字段、常量、固定Matcher字节、位容器及其保留位；不能覆盖自身存储位置。
不允许存储位置与任何已有字节所有者重叠。Builder发布前复核这些不变量。

稳定诊断分类：结构层沿用缺属性/未知属性/类型错误；领域层分别提供范围越界、存储越界、
自包含及存储冲突诊断，JSON Pointer（JSON位置指针）指向range、storage或冲突属性。
具体C++枚举名可以随实现保持现有风格，但不得将这些错误全部混成算法执行失败。

## 4. 执行与错误顺序（补充拍板B，已确认）

### Decode

保留既有参数、Workspace归属/占用等前置检查。新能力代际顺序为：

1. 验证Frame结构及候选所需读取范围，不访问越界输入。
2. 按既有长度/fixed_bytes规则选结构候选：零个为UNKNOWN_MESSAGE，多个为AMBIGUOUS_MESSAGE。
3. 唯一候选下验证输出槽容量，容量不足沿用既有错误，不计算或交付部分字段。
4. 执行SUM8；不一致返回新增`INTEGRITY_FAILED`，交付字段数为0。
5. 执行字段语义校验及Decode；校验通过不掩盖未知枚举等既有失败。

Lab跨Pipeline选择也必须遵守结构唯一性：不能循环Decode后只计“成功的候选”，从而吞掉
完整性失败或把歧义误报为成功。若需要内部候选查询辅助接口，不扩大为稳定公共API。
旧Schema路径维持既有行为，混合或跨代配置按版本门禁处理。

### Encode

沿用输入、类型、容量、引用、重叠等前置检查；写入固定字节、普通字段与位容器后，计算SUM8，
写校验字节，再从已写Frame重新计算核对，并完成既有Matcher/字段最终复核。
校验最终复核失败仍归`FINAL_REVIEW_FAILED`，与接收`INTEGRITY_FAILED`区分。
最终失败时有效输出长度为0；Buffer可能已改写，调用方不得发送或使用失败Buffer。

不向业务fields中追加校验伪字段。首版通过原始Frame和稳定失败诊断定位，不新增expected/actual
数值诊断对象。若后续需要该对象，再独立评估机器格式。

## 5. 版本与Lab（补充拍板C，已确认）

Schema 0.3承载integrity，新代允许已有字节字段和位字段；旧0.1/0.2继续拒绝integrity。
使用Lab Result/Record/Event 0.4及独立fingerprint域，不扩大0.3的状态与匹配语义。
理由：新增完整性失败和结构候选顺序会影响确定性执行结果，独立版本便于历史Replay隔离。

| 配置 | Values | Result/Record/Event | RX Metadata |
| --- | --- | --- | --- |
| 0.1 | 原规则不变 | 原离线0.1/UDP0.2 | UDP仍0.2 |
| 0.2 | 原规则不变 | 0.3 | UDP仍0.2 |
| 0.3 | 0.1或0.2，BOOL仍要求0.2 | 统一0.4，含无integrity消息 | UDP仍0.2 |

Values不新增校验输入，所以不升级。新状态映射为`PAE_LAB_CODEC_INTEGRITY_FAILED`，
当前执行失败退出5；读取证据本身失败继续走既有证据错误路径，不能混为协议校验失败。
同代失败Replay可比较EQUAL但仍退出5；成功执行的差异沿用退出6。
跨Schema替换Replay、跨代Run Compare建议明确拒绝且比较未评估；原始Frame Compare仍可跨代。
保留NO_CODEC_REEXECUTION（不重执行编解码）模式，比较为null/NOT_EVALUATED，不伪造校验通过。
RX来源证据持久化门禁仍在协议校验之前，哈希校验与SUM8互不替代。

## 6. 资源契约

可选规则的冻结描述符采用现有Arena分类计费，包含算法枚举、范围与存储偏移；规则数量至多
为Message数量，无需单独引入用户资源配置。Builder按实际布局复算，不能忽略对齐或可选存储。
执行使用局部无符号累加器，不新增Workspace数组。
Decode校验工作量为覆盖长度N；Encode生成与重算为2N，另加既有字段/匹配开销。
这只是操作上界，不是吞吐、延迟或嵌入式性能实测结论。

## 7. 验证矩阵（补充拍板D，已确认）

公开人工向量为Frame `70 F0 20 01 02 13 7E`，覆盖字节[1,5)，存储偏移5。
独立算术为`0xF0+0x20+0x01+0x02=0x113`，输出`0x13`。
偏移0和6在校验范围外，测试其翻转时须避开Matcher或字段语义约束，不能一概期待Decode成功。

| 编号 | 必须验证的行为 |
| --- | --- |
| S01 | 独立Encode/Decode预期，零值、最大字节、溢出、动态字段与位容器 |
| S02 | 非零预填Buffer、输入顺序变化、校验存储无业务输入 |
| S03 | 覆盖字节单比特翻转、校验字节翻转被拒绝；互相抵消变化展示SUM8局限 |
| S04 | 非覆盖字节变化只按其他结构/字段规则处理，不误称SUM8保护全帧 |
| S05 | 空范围、越界、自包含、整数边界、未知算法/属性、所有权冲突编译期拒绝 |
| S06 | 零/单/多结构候选，尤其“一条校验通过另一条失败”仍为歧义 |
| S07 | 校验失败字段数0；校验通过但字段语义失败不被掩盖；容量等优先级固定 |
| S08 | 最终复核失败不能交付有效Frame；故障注入沿用内部测试机制 |
| S09 | 原Run→Replay A→Replay B：失败诊断、比较、退出码、原证据不变及零Socket调用 |
| S10 | 同代替换配置改变覆盖范围/预期结果，可读差异及链式Replay |
| S11 | 旧代夹具及指纹不改写；新代无integrity；跨代拒绝；Frame跨代比较 |
| S12 | NO_CODEC模式、损坏Bundle、当前执行与历史Transport隔离 |
| S13 | 精确Plan计费、损坏Draft拒绝、首次调用无分配与覆盖长度操作上界 |
| S14 | Windows Debug/Release串行、全切片及Product-only/Lab-on-Testing-off隔离 |

2026-09-06 P2复核补证：S06已由Lab内部执行阶段计数证明零候选和跨Pipeline
多候选的`Decode`/SUM8/字段交付次数均为0，唯一候选只执行一次`Decode`；Pipeline
顺序翻转不改变歧义结果，同一Message同时出现在两个Pipeline中仍按两个Pipeline身份判定
歧义。同Pipeline的相交Matcher仍由Domain Validator和PlanBuilder在
发布前拒绝，Core保留多候选防御分支，本轮没有放宽该不变量。S12首次实现时的
unknown-Event-field负例未同步Run Record文件长度，实际只证明了低层payload不一致
拒绝；本轮修正辅助函数后，先精确断言文件长度、文件Hash、Record Hash及清单Hash
自洽，再精确命中`V0.2 event contains unknown property unexpected`。这是测试证据
缺口纠正，未发现Event Reader产品实现缺陷。

默认离线。若Lab接口变化必须复跑UDP，后续实施授权中只允许Loopback；不进行人工网络验收、
非Loopback或硬件访问。私有文档推导语料不是正式Golden，接收算法证据缺口不阻断通用人工切片。

## 8. 实施与验证状态

用户已确认补充A～D并授权实现。当前实现覆盖Schema/Loader、SchemaIr、Domain/Resource、
Budgeted Draft、Frozen Plan、Core Encode/Decode、Lab 0.4和Evidence Bundle读取链；公开样例来自
`SYNTHETIC_FROM_SCRATCH`，不包含私有协议或现场数据。两项P2纠正后的实际Windows命令、
S01～S14映射及未验证边界见`windows-msvc-2026-dec041-sum8-slice.md`。本状态仍为待
总控复核，不代表获准提交。

交付补记（2026-09-06）：上述待复核状态为实施轮历史记录。总控已复核并按用户授权完成
分组Stage、Commit和Push，DEC-041提交为`4d923e9`，DEC-040文档收口为`57743bf`。
本次仅补记交付状态，没有重跑测试，不升级真实协议Golden、Linux、硬件或现场结论。
