# DEC-040 后续推进路线与当前状态

日期：2026-09-06。更新：SUM8的11项范围原则及四项补充均已确认并登记为PAE-DEC-041；
实现、Windows验证与本轮已发现的P2纠错已完成，DEC-041已随`4d923e9`提交并Push。
具体接口、版本与测试组合见[SUM8契约](pae-dec-041-sum8-contract.md)。本路线不新增实现授权。

## 1. 已交付检查点

- `263f51456f9f78012a897cdc354ea1116233a45d`：人工Lab验收文档。
- `3b23db9ea160fbb967c6c3597461ece19117f599`：DEC-040位字段和版本化Lab证据。
- 上述两个提交已Push；DEC-040文档收口`57743bf`和DEC-041实现`4d923e9`亦已Push。
- DEC-041文档收口`97da00e`及DEC-042A实现/修复`7807b9a`已提交并Push；不升级既有验证边界。
- DEC-040既有最终Windows证据：Lab Debug/Release各7/7，全切片各35/35；Product-only与
  Lab-on/Testing-off隔离构建通过。总控另复跑离线失败链各1/1。此次路线文档更新没有重新执行测试。

## 2. 推荐顺序

1. 固定完整记录的SUM8（逐字节求和取低8位）完整性生成/验证最小切片：已实现、提交并Push。
2. [DEC-042数值转换](pae-dec-042-numeric-conversion-contract-draft.md)：11项范围已确认；
   A字节对齐INT64已审查、提交并Push；[B精确比例/偏置](pae-dec-042b-decimal-conversion-contract-draft.md)
   十项决策及四组补充方案均已确认（2026-09-07）；固定256位候选已完成授权隔离验证，
   经只读审查与P2补测，Windows Debug/Release各78386断言通过，见[报告](windows-msvc-2026-dec042b-arithmetic-spike.md)。
   生产接入A1～D4共16项已确认并同步契约；三段实施中的编译冻结与计费首段已实现并完成限定
   Windows验证，并在总控审查后补齐诊断、Builder防御和完整Core矩阵证据，见
   [首段报告](windows-msvc-2026-dec042b-compiler-slice.md)。Core双向转换第二段已实现并完成限定
   Windows验证，见[Core报告](windows-msvc-2026-dec042b-core-slice.md)。首段已随`4d26d42`提交并Push；
   第二段已补齐成功Decode后失败Encode/Decode使raw诊断失效的两个独立状态迁移测试，Lab证据段后置。
   无系统性高精度Oracle证据，本轮不授权Stage/Commit/Push。
3. 参数化CRC（循环冗余校验）、长度字段与变长能力分别评估，不合并成一次实现。

本地资料支持校验需求，但不同方向算法证据不等价；公开测试只采用从零设计的向量。
原始来源、客户文件名和未闭合字段映射仅保留在仓库外私有矩阵中。

## 3. DEC-041已确认范围与验证

已确认仅支持每Message一个SUM8规则：初值0、模256、非空连续字节范围、1字节独占存储位置，
校验字节不包含在计算范围中。输入输出方向独立，不引入通信、线程、重试或业务路由。

Encode在普通字段及位容器写完后生成校验并最终复核；Decode在唯一结构候选上校验后才交付字段。
Schema 0.3全局Inspect先汇总结构候选，多候选返回歧义且不执行Decode，唯一候选才执行一次Decode；
完整性失败返回INTEGRITY_FAILED。Lab结果、记录和Event使用0.4格式及独立指纹域，历史格式保持兼容。
不得靠校验通过来改变消息匹配契约。失败Replay可以与历史相等，但不变成当前执行成功。

热路径保持有界、无新增分配；增加冻结Plan计费及独立正反向向量。已有历史格式回归、失败链、
隔离构建继续保留。CRC、多段/动态范围、多校验依赖及宽松接收门禁不进入本切片。

最新执行任务证据为Lab Debug/Release各11/11、全切片各41/41及两类隔离构建通过；总控另复跑
结构匹配阶段与SUM8生成契约针对性测试，Debug/Release各2/2。详见[验证报告](windows-msvc-2026-dec041-sum8-slice.md)。
配置属性名、精确错误顺序与验收矩阵以DEC-041确认契约为准，不据此扩大后续能力范围。

## 4. 未完成边界

真实协议完整位映射、权威Golden、Linux、真实硬件、现场、正式性能和Runtime聚合准入仍未完成。
人工Lab验收不自动覆盖DEC-040或下一切片。历史接收端的仅诊断策略也不等价于新引擎严格拒绝。
