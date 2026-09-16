# PAE 公开 Host 阶段 2B 契约

日期：2026-09-14。状态：已确认并进入实现；文件落盘不等于实现、验证、提交或正式发布完成。

## 1. 范围

本阶段在公开 `CompiledProtocol`、`CompleteRecordCodec` 和 `StreamFramer` 之上增加应用无关的同步
Host 组合层。它提供不可变 endpoint/action 绑定、完整记录 Decode、流式 Push/Continue、Encode、
Find、Observe、Reset 和资源报告，不提供 Socket、串口、线程、重试、路由、热替换、Qt/Lab 或业务
状态机，不公开内部 Plan/Core/Framer/Host 类型，也不复制 Matcher 或切帧算法。

## 2. 绑定与身份

- `HostBindingSpec::endpoint_key`是调用方提供的非空不透明字节串；Host 创建时精确复制，Find 按
  `std::string_view`字节和长度比较，不做大小写或 Unicode 归一化。最大长度默认256字节。
- 唯一键是`endpoint_key + action`。同一endpoint可各有一个Decode和Encode绑定，同action重复失败。
- Pipeline使用公开index，binding不固定Message。Decode Pipeline至少有一条公开可Decode Message；
  Encode Pipeline至少有一条公开`encode_available`且存在可靠输出上界的Message。
- Decode绑定显式给出`decode_stream_count`且至少为1；每条逻辑流形成独立通道。Encode固定一个通道。
  COMPLETE_RECORD Decode通道持有Codec；STREAM_CHUNK Decode通道持有Codec和StreamFramer；Encode
  通道持有Codec及按公开输出上界预分配的buffer。
- 创建先校验全部绑定、数量、能力、index、重复项和预算，再原子发布。失败不返回部分Host。

`HostChannelHandle`是不透明、可复制、非拥有身份，包含实例scope、通道和generation。默认Handle无效；
其他Host的Handle为foreign；Host销毁后既有Handle为expired；Reset成功后旧Handle为stale。旧Host销毁
后，其Handle不能因地址复用而命中新Host。Host自身不可复制、不可移动；销毁不得与操作或回调并发。

## 3. 操作与优先级

Host单实例串行、非等待。每个公开操作按以下顺序处理：

1. 当前线程活动Host callback链包含本实例时返回`REENTRANT_CALL`，包括A→B→A；
2. 无法取得Host guard时返回`WORKSPACE_BUSY`；
3. 校验Handle scope、通道和generation，依次区分invalid、expired、foreign、stale；
4. 校验action与input kind；
5. 若目标通道已故障，返回`RESET_REQUIRED`；
6. Encode再校验调用期Message selector属于绑定Pipeline且`encode_available`，然后校验值和回调；
7. 执行一次对应的public Codec/Framer操作。

因此callback链与并发分类先于会准备或修改调用context的检查；入口失败的消费、候选和回调计数均为
0。`Decode`只接受COMPLETE_RECORD Decode通道；`Push/Continue`只接受STREAM_CHUNK Decode通道；
`Encode`只接受Encode通道。`Push(empty)`与`Continue`完全同路，不flush、不等待、不循环排空。

## 4. Decode、观察与成功交付

- COMPLETE_RECORD输入是一个候选；一旦进入Codec，`bytes_consumed`等于完整输入长度。流式输入的
  `bytes_consumed`、停止原因、候选数量和未消费后缀严格来自一次public StreamFramer调用。
- 每个候选最多调用一次Codec Decode。同一次结果先提供给可选`CandidateObserver`，再在成功时提供给
  business sink；observer和成功Decode Output的raw frame、typed字段及状态都只在同步callback内借用。
- 失败候选也可被observer观察，但`record.HasValue()==false`且不得暴露成功字段。业务sink只接收Codec
  成功，零字段成功也必须交付。
- observer正常返回STOP时，当前Codec成功仍进入business sink一次；失败候选永不进入business。
  business STOP提交当前成功后停止。observer/business正常返回次数、Decode成功/失败和业务正常返回
  次数分别计数，不能把已发生的用户副作用描述为可回滚。
- Decode失败使该次`HostOperationResult.status=CODEC_FAILED`，但不会要求Framer回扫或把候选重新拼接；
  若同一stream调用中还有后续候选，正常CONTINUE仍可继续，结果保留失败计数和最后Codec状态。

## 5. 回调异常、Reset与代次

observer和business callback是可抛异常的同步函数指针。Host在边界内捕获所有异常：observer异常抑制
当前业务，business异常不重试已调用callback；两者均停止本次后续候选，保留真实消费与已发生计数，
并且只将目标通道标记为`RESET_REQUIRED`。Encode callback异常执行相同隔离规则。其他通道不受影响。

`Reset`可用于Decode和Encode通道。成功时丢弃目标Framer半包/pending、使目标Codec既有借用view失效、
清除fault并递增generation；旧Handle立即stale，调用方必须用`Find`取得新Handle。Reset失败不改变
generation或fault状态。generation达到`uint64_t`最大值时返回`GENERATION_EXHAUSTED`并失败关闭。

## 6. Encode

Encode绑定固定Pipeline但不固定Message；同一Handle每次调用通过公开`message_index`选择该Pipeline内
公开为`encode_available`的Message。Host创建时遍历全部可Encode Message，以其中最大可靠公开输出
上界预分配单一channel buffer；selector越界、跨Pipeline或指向Decode-only Message均在Codec前拒绝。
Codec成功后以借用`ByteView`交付预分配buffer，Codec失败不调用sink且`bytes_produced=0`。callback正常
STOP仍表示当前成功已交付；callback异常令该Encode通道RESET_REQUIRED，并可保留Codec已实际生成的
`bytes_produced`事实，但`CALLBACK_FAILED`及零正常返回回调明确表示输出未成功交付。失败buffer不是可
交付输出。

## 7. 资源与分配

默认上限：64个binding、64个channel、单identity 256字节、Host聚合64 MiB；调用方只能收紧或在这些
hard limit内选择。所有数量和字节乘加均检查溢出。

`HostMemoryReport`分别记录：Host/Impl facade、scope payload、binding数组及其可Encode selector索引、
精确identity副本、channel数组、所有Codec accounted totals、所有Framer accounted totals、Encode预分配buffer，以及只列一次的
retained compiled-state facade。`host_accounted_total_bytes`为前述除共享compiled-state外各项之和；
不重复累加每个子对象报告中的共享compiled-state项。数组和字节副本按实际分配capacity计费。

创建可以暂持尚未发布的子对象，但只有全量成功且聚合不超过`max_accounted_bytes`才发布Host。报告的
`allocation_count`对应本实现的引擎侧创建分配点；allocator元数据、调用方输入/回调复制、进程RSS及
新旧Host共存峰值不属于该逻辑数值。已创建Host的Find/Observe/Reset/Decode/Push/Continue/Encode
热路径不得新增引擎堆分配。

## 8. 借用与兼容边界

Candidate frame、Decode record/field和Encode bytes只在当前同步callback内有效；callback返回后必须视为
失效，需要持久化时由宿主在callback内复制。Host保活同一compiled state，外部`CompiledProtocol`
移动、替换或销毁后仍可执行。绑定不可变；配置、Pipeline或绑定替换通过创建新Host并由应用切换。

本阶段不改变Schema、Codec、Framer、历史证据格式或产品版本，不引入稳定ABI承诺。Windows专项通过
不能外推为Linux、SDK包、网络、硬件、Golden、现场、正式性能或生产可用证明。
