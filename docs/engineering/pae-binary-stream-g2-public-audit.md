# PAE Binary stream G2 公开能力只读审查

## 1. 结论与证据边界

本报告对应 G2 Binary 流式图形入口预检中的 PAE 公开能力审查。结论如下：

1. **最小 G2 首片不需要扩展 PAE 公开 API。** 现有 `QueryPipelineFramingDescription`、
   `HostEndpoint::Push/Continue/Observe/Reset`、候选观察器及业务接收器，已经足以支持
   Schema 0.9 下 fixed、sync-fixed、sync-length 三类 Binary stream 的一次有界投喂、
   候选观察、成功业务交付、精确后缀保留、显式继续和按 Flow 复位。
2. **公开元数据只够“可执行 + 最小展示”，不够算法细节检查器。** 当前公开描述可给出
   `strategy` 和跨 chunk 的 `maximum_candidate_bytes`，但不公开 sync bytes、长度字段位置/
   宽度/字节序、最小帧长或 framing profile 标识。G2 首片若只展示策略、最大候选长度和
   运行时状态，这不是阻断项；若验收要求图形化展示这些算法参数，则需要后续独立的公开
   只读描述契约，不能由 Lab 读取 private Plan、重解析配置或反推运行字节。
3. **公开执行语义已能避免第二套执行引擎。** 每个候选只应由 Host 内部执行一次 Decode；
   Lab 在同步回调内复制展示 DTO，不能保存借用的 frame/record/BYTES 视图，也不能再调用
   Codec 复核。
4. **现有公开测试对 Framer 原语覆盖较强，Host 组合层仍有证据缺口。** sync-fixed、
   sync-length、malformed length、观察状态等已在公开 Framer 测试覆盖，但公开 Host 测试
   主要使用 fixed-length。实施前/随实现应补 Host 层组合回归；这属于验证缺口，不构成
   当前 API 缺口。

本轮只读源码和既有测试/报告，没有构建、运行测试、启动 Lab、生成 SDK 或执行网络操作。
文中“已有测试”表示测试源码或历史验证记录存在，不表示本轮重新执行通过。

## 2. 实际支持域与准入门禁

### 2.1 Schema 和 framing 策略

- Schema 0.9 是当前 Binary stream 配置代际；Loader 只在该代启用 Binary stream framing
  解析，Schema 0.11 的 stream 分支是 ASCII CRLF，其他既有代际仍为 complete record。
  依据：[pae.schema.json](../../schema/pae.schema.json)、
  [config_compiler.cpp](../../src/config_compiler/config_compiler.cpp)。
- Binary stream 公开策略为 `FIXED_LENGTH`、`SYNC_FIXED_LENGTH`、
  `SYNC_LENGTH_FIELD`；公开查询还可区分 `COMPLETE_RECORD` 和 `ASCII_CRLF`，Lab 不应以
  “stream available”替代策略判断。依据：
  [stream_framer.h](../../include/pae/stream_framer.h#L51-L73)。
- `maximum_candidate_bytes` 表示跨 chunk 的最大完整候选长度；它不是单次 `Push` 输入上限，
  也不是工作预算。complete-record 没有该值。创建实例后的实际输入上限和工作预算应取
  `Observe()` 的 effective 值。依据：
  [stream_framer.h](../../include/pae/stream_framer.h#L172-L185)。

### 2.2 Pipeline、方向与 Host 身份

- `CreateHostEndpoint` 对每条 binding 校验 pipeline、action 和允许集合；Decode 绑定还要求
  对应 execution plan 的 `decode_available`。方向字符串只用于展示，不能代替 Host 的实际
  action gate。依据：[host_endpoint.cpp](../../src/public_api/host_endpoint.cpp#L600-L802)。
- `DECODE_STREAM` 每个 binding 拥有独立 Codec、Framer、输出区、generation 和 fault 状态；
  同一 Message 出现在不同 Pipeline 时仍是不同 Flow，不应由 GUI 合并身份。
- 同一 endpoint 可同时存在 Binary Encode binding，但 stream framing 只服务 Decode；G2
  不应借此扩展 Encode 或自动转发。

公开合成配置 [synthetic_stream_framing_slice.pae.json](../../examples/config/synthetic_stream_framing_slice.pae.json)
包含三种 Binary 策略，可作为 G2 首片的非私有输入基线。

## 3. 最小公开调用序列

建议 G2 非 Qt owner/Session 只组合公开对象，调用顺序如下：

1. 通过 `QueryPipelineFramingDescription(plan, pipeline_index)` 确认该 Pipeline 是
   `STREAM_CHUNK`，取得 strategy 和最大候选长度。
2. 以明确的 decode-stream binding、允许 Message 集合和有界 `HostLimits` 创建一个
   `HostEndpoint`；记录 `HostMemoryReport`，但不把 accounted bytes 宣称为 RSS 或性能结论。
3. 用 `Find(endpoint_key, action, stream_index)` 取得当前 generation 的 handle；每个可见 Flow 独立保存 handle、
   Lab 自有输入草稿、冻结后缀和复制后的展示 DTO。
4. 用户投喂时只调用一次 `Push(handle, chunk, sink, observer)`。在 observer 内同步复制候选
   frame、身份、decode status、匹配消息、字段/转换和 tainted 状态；在 business sink 内只
   复制成功交付。回调返回后不再使用任何借用视图。
5. 依据 `bytes_consumed` 精确冻结 `chunk[bytes_consumed, chunk.size)`。`SINK_STOP` 是正常暂停，
   不是失败；冻结后缀不得自动重放，也不得与下次新输入拼接后绕过用户操作。
6. 只有显式继续操作才提交冻结后缀；若没有外部后缀但 `Observe().has_internal_work` 为真，
   可用 `Continue()` 驱动一次有界内部工作。G2 首片不需要自动 drain 循环。
7. 展示 `HostOperationResult` 的 Host/Codec/Framing 三层状态、stop reason、消耗/候选/成功/
   失败/回调计数、discard/malformed/last issue/work units 以及 `Observe()` 的 phase、buffered、
   internal work 和 effective limits。
8. `reset_required` 或用户显式复位时，只对目标 Flow 调用 `Reset(handle)`；成功后旧 handle
   失效，必须重新 `Find()`。清空该 Flow 的冻结后缀和展示中的待继续状态，不影响其他 Flow。

该序列不读取 private Framer/Plan，不重复 Decode，不根据布局反算 framing，也不把 Qt 对象
跨入 PAE 回调。

## 4. 借用、候选和业务输出语义

### 4.1 同步借用边界

- `FrameCandidateView::bytes` 仅在同步 `noexcept` Framer sink 回调期间有效；Framer 不保留调用
  方未消费后缀。依据：[stream_framer.h](../../include/pae/stream_framer.h#L137-L154)。
- `HostCandidateView` 中 frame、decoded record 及字段内可能借用的 BYTES 同样只能在回调中
  读取。下一候选的 Decode 可能在同一次 `Push` 返回前复用工作区，因此不能先保存 view 再
  于 Push 返回后复制。依据：[host_endpoint.h](../../include/pae/host_endpoint.h#L101-L134)。
- Lab 应复制成自身 DTO/owned bytes/QString；复制产生的内存属于 Lab 预算，不属于
  `HostMemoryReport`。

### 4.2 Observer 与 business sink 分工

- 每个形成的候选都经过一次 Host 内部 Decode，并通知 observer，包括 Decode 失败候选。
- 只有 Decode 成功的候选进入 business sink；失败候选不得伪装成业务交付。
- 成功候选上 observer 返回 STOP 时，当前候选仍向 business sink 交付一次，随后停止；失败
  候选上 STOP 不会触发 business sink。
- callback 抛异常映射为 `CALLBACK_FAILED`，通道进入 faulted/reset-required；回调失败前的
  实际消耗和执行事实仍保留，但不能把该次业务交付标记为正常完成。依据：
  [host_endpoint.cpp](../../src/public_api/host_endpoint.cpp#L223-L301)。

### 4.3 聚合状态不能代替逐候选事实

同一次 `Push` 可先出现 Decode 失败候选、后出现成功候选。Host 的聚合 `status` 在首次
`CODEC_FAILED` 后不会因后续成功自动清除，而 `codec_status` 表示最后一次候选状态。因此 UI
必须以 observer 复制的逐候选事实和 operation 计数为准，不能仅用最终 `status` 或
`codec_status` 推断所有候选结果。

## 5. 暂停、进展、错误和复位

### 5.1 STOP、Continue 与精确后缀

- `SINK_STOP` 是 sink 请求的正常停点；`bytes_consumed` 是当前输入的精确已接收前缀。
- Framer 不保留未消费 suffix；Lab 必须冻结并在显式 Continue 时原样提交，不能丢弃、重复
  或偷偷合入新的草稿。
- `Continue()` 等价于空输入 `Push()`，用于 DELIVERY_PENDING 或其他内部工作；它不是重试
  失败候选的接口。依据：[stream_framer.cpp](../../src/public_api/stream_framer.cpp#L174-L225)。

### 5.2 “零消费”不等于“无进展”

公开 API 没有单独的 NO_PROGRESS 错误，也不需要为 G2 首片新增。`bytes_consumed == 0` 仍可能
交付 pending candidate 或推进内部状态。判断可见进展应联合：

- `candidates_delivered`、`bytes_discarded`、`malformed_candidates`；
- `work_units_used`；
- 调用前后 `Observe()` 的 phase、buffered bytes、has_internal_work；
- stop reason。

首片每个 UI 操作只执行一次有界 `Push/Continue`，天然避免无界空转。若未来引入自动 drain，
应先在 Lab 消费契约中定义有限循环和无进展停止条件，而不是扩展 PAE 或循环到“成功”。

### 5.3 错误分层

| 情况 | 当前公开行为 | G2 展示/处理 |
| --- | --- | --- |
| handle/action/input/调用状态非法 | Host 在 framing/codec 前拒绝 | 保留 Host status；不伪造候选 |
| Framer 参数、资源或内部失败 | `FRAMING_FAILED`，嵌套 framing 事实保留 | 不调用 Codec；展示 framing status/issue |
| 候选 Decode 失败 | observer 可见，business 不接收；聚合为 `CODEC_FAILED` | 展示该候选失败，可按 observer 动作继续或停 |
| observer/business callback 抛异常 | `CALLBACK_FAILED`、`reset_required=true` | 禁止继续；显式 Reset 后重新 Find |
| sink 返回 STOP | 正常 `SINK_STOP` | 冻结精确 suffix，等待用户继续 |
| resource/work budget 到达 | 有界返回及 runtime observation | 不自动放宽；用户可显式继续或复位 |

Reset 只作用目标通道，并推进 generation；旧 handle 之后应返回 stale-handle 语义。Host owner
本身是串行、非等待模型，跨线程并发返回 busy，回调内直接或 A→B→A 间接重入被拒绝。依据：
[host_endpoint.h](../../include/pae/host_endpoint.h#L167-L192)、
[stream_framer.cpp](../../src/public_api/stream_framer.cpp#L19-L45)。

## 6. 现有自动化证据与缺口

### 6.1 公开 Framer 测试源码已覆盖

[public_stream_framer_tests.cpp](../../tests/public_api/public_stream_framer_tests.cpp) 当前包含：

- capability 与 complete-record 拒绝；三种 Binary 策略、ASCII CRLF 的描述查询，且区分
  最大候选长度 M 与实例运行上限 C；
- 创建资源报告、单次 allocation、分配失败原子性、sync-length override；
- 输入上限失败零消费/不改状态、工作预算精确 suffix；
- fixed split/glued、sync-fixed 跨 chunk、sync-length 垃圾前缀与候选；
- STOP 精确前缀、pending candidate、空 Push/Continue、实例隔离、半帧 Reset、move；
- 直接重入和 A→B→A 间接重入、跨线程 busy；
- 每个候选恰好一次公开 Decode。

历史验证报告 [pae-public-framer-stage2a-validation.md](pae-public-framer-stage2a-validation.md)
记录最终公开 Framer Debug/Release 各 38/38、重入定向各 6/6。它是历史执行证据，本轮未复跑。

### 6.2 公开 Host 测试源码已覆盖

[public_host_endpoint_tests.cpp](../../tests/public_api/public_host_endpoint_tests.cpp) 当前包含：

- callback 内复制借用 frame/record、observer 与 business 分离；
- binding/预算/分配失败原子性、handle generation、Reset、owner 语义；
- fixed stream 两候选与 pending/Continue、空 Push 等价；
- observer STOP 后成功候选仍交付和精确 suffix；
- bad candidate 不进入 business 且不重放；callback fault/reset 隔离；
- Binary Encode 共存、热路径无分配、重入及跨线程 busy。

历史验证报告 [pae-public-host-stage2b-validation.md](pae-public-host-stage2b-validation.md)
记录最终公开 Host Debug/Release 各 56/56、定向各 10/10。它是历史执行证据，本轮未复跑。

### 6.3 建议在 G2 实施时补齐的公开组合证据

以下不是当前 API 缺陷，但仅凭现有 Host 测试不能完整证明 G2 组合层：

1. Host 级 sync-fixed 与 sync-length：跨 chunk、垃圾前缀、形成候选并一次 Decode。
2. Host 级 malformed length/discard/last issue 向 `HostOperationResult` 的映射。
3. 同一次 Push 内“失败候选后成功候选”的逐候选 DTO、业务交付和聚合状态/计数。
4. Host `Observe()` 的 buffered/internal-work/effective limits，以及 idle Continue 不伪造候选。
5. 两个 decode-stream Flow 的后缀、Reset、generation 和故障隔离。
6. Lab adapter 层 callback 内深复制与 callback 返回后不访问 borrowed view 的针对性测试。

private/internal Host 或现有 private Binary Lab 测试只能作为场景设计参考，不能替代上述公开
消费链路证据。G2 实施后仍应执行独立 Debug/Release 定向验证；是否需要完整 GUI 人工烟测由
Lab 预检收敛，PAE 本报告不把历史测试升级为新 UI 证据。

## 7. 最小实施边界建议

PAE 侧建议本轮保持零代码变更。Lab 后续首片可限定为：

- 一个仅消费公开 `HostEndpoint` 的非 Qt Binary stream adapter/owner；
- 自有 Flow 状态、冻结 suffix 和深复制候选 DTO；
- Qt 层只触发 Push/Continue/Reset 并展示公开状态；
- Schema 0.9 与三种 Binary stream 策略，不扩 ASCII、Socket、自动监听、自动重试或路由；
- strategy + maximum candidate + runtime observation 的最小展示。

下列诉求应另立契约，不在首片偷偷实现：

- 展示 sync bytes、fixed length、length-field offset/width/order、minimum length、profile/source ref；
- 自动 drain/retry、持续监听、网络接收、消息路由或跨 Flow 调度；
- 保存 public borrowed view、读取 private Plan、重复 Decode 或复制 private 执行引擎。

## 8. 审查状态

- 当前判断：**G2 最小首片可基于现有公开 API 实施，未发现必须先扩 PAE API 的阻断项。**
- 必要后置项：若产品要求算法细节展示，另行设计公开只读 metadata；不能把它混入最小执行片。
- 证据限制：本轮仅静态审查当前源码、测试源码及历史验证报告；未进行本次构建、测试、SDK、
  Lab、Loopback、真实设备、Linux、性能或人工 UI 验证。
- Git 边界：本报告是本轮唯一获准新增文件；未 Stage、Commit、Push。
