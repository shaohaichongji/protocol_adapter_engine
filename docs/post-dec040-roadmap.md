# DEC-040 后续推进路线与当前状态

## 2026-09-10 当前入口

`main@4a04afb`已合并并Push：PAE有界流式切帧、Lab离线Inspect集成交付完成。
集成Windows Debug/Release各10/10通过，未升级网络、Linux、Golden或现场结论。
下一轮[三组八项确认契约](post-stream-host-and-ui-v08-checkpoint.md)：PAE完善流式宿主示例，
Lab开放Schema 0.8有界变长完整记录UI；整体授权后的两侧候选已完成，待提交与集成交付。
PAE初批受影响Debug/Release各14/14、字段增量后宿主各1/1；Lab初批UI各10/10，详情刷新
修复后专项各1/1。限定人工确认见Lab分支验证报告；共享产品接口未改，超长输入截断风险保留。
之后再评估离线流式观察、单连接UDP，最后TCP/串口；这些远期项未获实施授权。

## 历史推进记录（保留原时点）

2026-09-09当前推进：长度检查点`0179abc`、`7d2ef4f`均已Push；其后的
[有界变长完整记录实施契约](bounded-variable-record-contract.md)已在当前工作树完成限定实现与
Windows离线验证，正在等待总控审查，尚未Stage/Commit/Push。实现仍限定固定头部、一段有界
BYTES、可选SUM8/CRC尾部，不包含流式切帧。
空载荷证据收口在不升级Result/Record/Event的前提下增加Values 0.5：仅Schema 0.8可显式输入
空BYTES，Result 0.9 Reader/Writer/指纹统一接受空配对，旧代保持拒绝。修复后仍待总控复核。
《子任务推进》负责PAE主线和必要Lab离线兼容，《Lab应用推进》当前仅做UI方案修订及Qt官方
资料核验。两者不并发改共享文件，不为Lab修改PAE协议语义；本轮不创建worktree。
后续顺序仍为：有界变长审查收口 → 流式切帧 → 多实例/稳定接入；Lab应用支线独立。
每个完整检查点默认一次集中审查及一次提交，Git操作仍另行授权。
以下记录保留各批次时点，不代表当前交付状态。

2026-09-09收口更新：[长度字段校验与自动回填](length-field-minimal-contract.md)已在当前工作树
一次完成Compiler/Builder、Core、Lab、公开样例、业务嵌入和Windows离线验证；Schema 0.7使用
Result/指纹0.8、Record 0.9并保持Event 0.7。初次Debug/Release离线矩阵各39/39、业务嵌入各1/1、
两类0测试隔离构建通过；P2修复后受影响集合各4/4，最后CLI补测各1/1，完整矩阵未重跑。
总控定向复核关闭既有问题，51个候选（43修改、8新增、无删除）及115处本地相对文档链接已核对。
A组46文件已按批准信息本地提交为`0179abc`；B组5份入口Markdown已暂存并准备最终提交信息。
B组尚未Commit，全部尚未Push；本轮未联网。后续Commit/Push仍需另行授权。
后续依次评估有界变长、流式切帧、多实例/Session；Qt依赖核验可单独授权，不启动UI实施。
下述CRC暂存前及更早记录保留原时点，不作为当前未交付状态。

2026-09-09最新状态：最小业务嵌入检查点已审查并随`c905e33`提交、Push。
用户确认下一主线为[参数化CRC最小检查点](crc-minimal-contract-draft.md)，八项方向已确定，
参数精确编码、版本影响和验收表三组共八项亦已确认并获实施授权。实现及Windows离线自动化
已有回报，锁定pycrc外部参考11组/33次核算已通过；历史Result版本关联与CRC-32小端测试两项
P2已修复并经总控定向复核关闭。候选文件、敏感信息和文档链接集中检查完成，待分组Stage授权，
尚未Stage/Commit/Push。两次历史越界Loopback UDP执行保持单独记录，详见
[事实纠正、参考核算及审查报告](windows-msvc-2026-crc-minimal-slice.md)。下一步按批准范围交付本检查点。
后续顺序为长度校验/回填、有界变长、流式切帧、多实例/Session。Qt依赖核验作为可独立授权
支线，UI实现建议在CRC之后；ASCII/BCD按需求插入。下文保留早期批次时点。

2026-09-08最新决定：`df43165`已提交并Push，DEC-042B C3离线功能检查点按用户确认的代理验收
PASS收口，见[验收报告](agent-dec042b-c3-offline-acceptance.md)。用户人工仍NOT_EVALUATED，
不再阻塞本检查点。下文是早期批次路线快照，其未实现/未Push状态以本条及最新契约为准。

下一检查点的[最小业务嵌入验证八项契约](business-embedding-minimal-contract.md)已由用户确认并授权，
现已使用公开合成协议完成限定实施和Windows验证：宿主收到字节后调用Decode、业务产生动态值后
调用Encode，错误不交付。实际证据见[验证报告](windows-msvc-2026-business-embedding-minimal.md)，
当前待总控收口。
不自动扩展Runtime/Session、线程调度、网络、Qt UI、CRC或变长能力，也不自动发布稳定公共API。

日期：2026-09-06。更新：SUM8的11项范围原则及四项补充均已确认并登记为PAE-DEC-041；
实现、Windows验证与本轮已发现的P2纠错已完成，DEC-041已随`4d923e9`提交并Push。
具体接口、版本与测试组合见[SUM8契约](pae-dec-041-sum8-contract.md)。本路线不新增实现授权。

## 1. 已交付检查点

- C2第一段RUN Evidence 0.7实现及直接契约已通过总控限定复核，A组`d7b6e97`已本地提交，尚未Push。

- C1隔离执行桥接`115db10`及入口文档`ef350d5`已提交并Push。

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
C2第一段已按第18～20节实现默认关闭的RUN Evidence 0.7读写和真实阶段事件，验证见
[C2第一段报告](windows-msvc-2026-dec042b-lab-v07-run-evidence-stage-c2-first.md)。A组已本地提交，
下一步收口B组入口文档；Push及C2第二段Plan关联/Replay/Compare仍需独立授权，不能自动启动。
C阶段12项决策已确认，见[契约第15～17节](pae-dec-042b-decimal-conversion-contract-draft.md)。
第16节C1三项收口已完成限定实现和Windows复核：准备/Codec/Lab复核分层，保留Core错误顺序，
额外复核失败不篡改Encode结果。第17节历史草案已由第18～19节确认契约收口；C2第一段现已
完成限定实现，下一步收口提交检查点，C3最后开放专用新代CLI并验收。C1提交检查点已收口。

2026-09-08历史记录（C2实施授权前）：六项方向已确认，Record/Event 0.7与既有Result/指纹0.6分离，完整失败记录、
真实阶段事件、分层证据/Plan检查、有限Replay资格和两段实施边界见契约第18节。
第19节修订后的精确契约已确认，下一步按第20节待授权范围派发第一段证据读写；
第二段Plan关联/Replay另行授权。本轮只有Markdown变更，未派发实施任务，
没有C2代码、测试、网络、生成物清理或Git写操作。

第19节定向复核缺口已限定修订：补齐异常失败分支，结果映射与Review分开采集，比较绑定历史
Result及父Record快照。第一段只接受RUN，REPLAY第二段开放；完整枚举及状态组合已补至19.8，
CLI映射延至C3实施前冻结已在19.9确认，不将文档修订视为C2实现授权。独立只读复核进一步
拆分Reader执行前拒绝与Writer执行后发布失败，不抹除已发生Codec事实；同时明确Replay父子
输入长度/Hash一致，与独立Run Compare区分。本轮无运行验证。

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
