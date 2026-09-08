# DEC-040 后续推进路线与当前状态

日期：2026-09-06。更新：SUM8的11项范围原则及四项补充均已确认并登记为PAE-DEC-041；
实现、Windows验证与本轮已发现的P2纠错已完成，DEC-041已随`4d923e9`提交并Push。
具体接口、版本与测试组合见[SUM8契约](pae-dec-041-sum8-contract.md)。本路线不新增实现授权。

## 1. 已交付检查点

- Lab B隔离证据实现`70cf4ff`及入口文档`761ff7f`已复核、提交并Push。

- Lab A纯格式`8d4c7c4`、入口文档`0c4bc48`和yyjson依赖整理`1c0617c`均已提交并Push。
- DEC-042B隔离算术`8fd2019`、编译冻结`4d26d42`、Core双向转换`90c5165`均已提交并Push。

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
   第二段已补齐成功Decode后失败Encode/Decode使raw诊断失效的两个独立状态迁移测试，随`90c5165`
   提交并Push。Lab第三段六项补充已确认：转换原因、失败身份、文件职责、指纹、执行等价及差异分类。
   指纹长度前缀编码、固定20项顺序和A隔离测试入口已确认；A纯格式模块已完成限定实现及Windows
   Debug/Release隔离验证及总控限定复核，A提交检查点已收口。B四组补充已确认，并已完成
   Evidence 0.6隔离读写、事务故障和严格Reader限定实现及Windows验证，详见
   [B报告](windows-msvc-2026-dec042b-lab-v06-evidence-stage-b.md)。C1执行桥接已在默认关闭的隔离
   目标中完成Schema 0.5准备、Core调用及Result 0.6物化，并通过Windows Debug/Release限定验证，
   见[C1报告](windows-msvc-2026-dec042b-lab-v06-execution-stage-c1.md)。C1不接B文件IO、普通CLI、
   Replay/Compare或网络。审查纠错已恢复旧Values到Schema 0.5的原类型兼容，并补展示Decode真实
   INTERNAL_ERROR证据。无系统性高精度Oracle证据；C2/C3仍未实施，本轮不Stage/Commit/Push。
3. 参数化CRC（循环冗余校验）、长度字段与变长能力分别评估，不合并成一次实现。

生成物清理暂缓：首批9个历史中间目录已只读审核，未批准删除，不阻塞后续契约工作。
C阶段12项决策已确认，见[契约第15～17节](pae-dec-042b-decimal-conversion-contract-draft.md)。
第16节C1三项收口已完成限定实现和Windows复核：准备/Codec/Lab复核分层，保留Core错误顺序，
额外复核失败不篡改Encode结果。下一步先冻结第17节及准备失败和合成/真实事件的完整证据映射，
再单独授权实施C2；C3最后开放专用新代CLI并验收。C1变更待总控复核，未Stage、Commit或Push。

配套工具方向已确认：保留CLI，增加独立Qt UI，复用Lab执行层；Qt不进入Core依赖链。
先完成当前Lab第三段，再单独开展Qt来源/完整性/工具链及版本核验和最小UI切片。
暂不复制现有Qt包，UI版本、依赖获取及实现另行确认授权；Qt方向拍板时仅同步Markdown，
该历史授权边界不表示后续A纯格式模块尚未实施。

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
