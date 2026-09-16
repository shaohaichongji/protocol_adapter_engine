# PAE 公开 Framer / Host 最小切片设计

日期：2026-09-14。状态：阶段 2 设计已完成；阶段 2A 已完成限定复核，阶段 2B 已按确认契约实施并
完成Windows定向验证，仍待总控复核。本文不是稳定 ABI、正式发布或提交授权。现场基线为
`main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`，暂存区为空。本文只依据当前内部Framer、Host、
已公开 COMPLETE_RECORD Codec 和既有契约提出最小公开边界，不改变协议、切帧、Decode、资源或
错误语义。实际2A接口和证据见[阶段2A验证报告](pae-public-framer-stage2a-validation.md)。

## 阶段 2A 实施补充（优先于下文候选命名）

- 实际公开 `StreamFramerStatus`、`StreamFramingCapability`、`StreamFramerOptions`、
  `StreamFramingMemoryReport`、`StreamSubmitResult`、`StreamFramerObservation`和`StreamFramer`；
  提供`QueryStreamFramingCapability`、`CreateStreamFramer`、`Push`、`Continue`、`Reset`、`Observe`
  和`MemoryReport`。
- `Push(empty)`与`Continue`严格调用同一路径；公开Observation包含phase、buffered bytes、
  `has_internal_work`及有效submit/work上限。能力查询只报告冻结Pipeline事实，不承诺预算准入。
- `max_session_memory_bytes`沿用内部Framer的`0=resource profile default`和hard limit规则，只约束
  内部workspace；报告另列public facade并合计为`framer_accounted_total_bytes`，共享compiled-state
  facade再单列。上述逻辑计费不是进程RSS硬上限。
- 候选sink为同步`noexcept`函数指针；同一线程活动callback链中的同实例重入（含A→B→A循环）返回
  `REENTRANT_CALL`，实际跨线程并发操作返回`WORKSPACE_BUSY`。callback链检查和facade guard均先于
  callback context准备，移动/销毁不得与调用或回调并发。
- 2A没有实现Host异常恢复、Candidate observer或binding identity；这些仍留给2B。

## 阶段 2B 实施补充（优先于下文候选命名与待决策项）

- 实际公开`HostEndpoint`、不可变`HostBindingSpec`、非拥有`HostChannelHandle`、Find/Observe/Reset、
  COMPLETE_RECORD Decode、stream Push/Continue和Encode；完整契约及证据分别见
  [阶段2B契约](pae-public-host-stage2b-contract.md)和
  [阶段2B验证报告](pae-public-host-stage2b-validation.md)。
- 实现组合public Codec/StreamFramer，每Decode stream独占Codec，stream通道另有Framer；Encode binding
  只固定Pipeline，同一Handle调用期选择可Encode Message，通道按该Pipeline全部可Encode Message的
  最大可靠公开输出上界预分配；每候选只Decode一次，不调用或复制内部Host算法。
- 已采用同步可抛observer/business callback、异常后仅目标通道RESET_REQUIRED、非空字符串key、
  scope+channel+generation Handle、Reset后重新Find，以及活动callback链A→B→A为REENTRANT的确认选择。
- 资源报告按精确数组/identity/output capacity和子Codec/Framer accounted totals聚合，共享compiled
  state只单列一次；scope payload与其allocator控制元数据边界明确，后者不宣称为逻辑字节/RSS上限。
- 未新增transport、线程、动态注册、Schema或稳定ABI；Lab/Qt、SDK打包、Linux和生产验证均未进入
  本阶段。

## 1. Findings 与推荐顺序

### 1.1 结论：先公开 Framer，再公开薄 Host

推荐分成两个串行检查点：

1. **阶段 2A：公开有状态 `StreamFramer`。** 它从 `CompiledProtocol` 保活同一份编译状态，
   固定绑定一个 Pipeline，为一条逻辑流持有一个内部 `StreamFramingWorkspace`；同步回调只交付
   完整候选字节和切帧事实。它不做 Message 匹配、不做 Decode、不保存候选队列。
2. **阶段 2B：公开薄 `HostEndpoint` 组合层。** 它复用公开 Framer 和公开 Codec 的同一编译状态，
   将不可变绑定、逐流状态、Decode/Encode动作和同步交付组合起来；不得复制内部 Matcher、Framer
   或 Codec。2B 必须等 2A 的消费、借用和复位边界经验证后再实施。

不建议先公开现有内部 `host_endpoint::Session`。它当前一次承担绑定校验、Plan所有权、Framer、
Core工作区、业务Sink、诊断Candidate、异常捕获、故障后Reset和内存聚合；直接照搬会把内部Schema
接受域、`PlanBundle*`、`FieldRef`和多种生命周期细节泄漏为公共承诺。也不建议将Framer与Host一次
组合发布，否则无法分别证明“候选边界正确”和“候选仅Decode一次”。

### 1.2 当前实现事实与可复用边界

| 事实 | 当前依据 | 公开设计约束 |
| --- | --- | --- |
| Framer只绑定一个Plan实例和Pipeline，持有独立状态及预分配buffer | `stream_framer.h`的Workspace成员及创建函数；`stream_framer.cpp`的创建准入 | facade内部保留同一归属，不公开`PlanBundle` |
| `bytes_consumed`只接受连续前缀；STOP不预取后缀 | `PushStreamChunk`逐字节推进；Framer测试的STOP用例 | 未消费后缀始终归调用方，返回后PAE不借用它 |
| 完整候选在调用sink前进入`DELIVER_PENDING`，回调后即ResetCandidate | `stream_framer.cpp`的交付分支 | 候选在回调返回时即失效；STOP也提交当前候选且不重放 |
| 同Workspace并发与回调重入已有独立拒绝 | `atomic_flag`和thread-local callback scope链 | public facade映射为稳定公开状态；活动链可识别A→B→A，不用锁等待、动态容器或递归执行 |
| empty push只推进已缓存内部工作 | `HasInternalWork()`、Framer empty continuation测试 | 公开`Push(empty)`与`Continue`必须等价，不接受新字节，也不轮询Transport |
| 下游Codec失败不触发Framer回扫 | 流式契约4.4及Host的同步候选回调 | public Framer只报告候选；Decode结果不反馈给Framer状态机 |
| public Codec持有引用计数的内部编译状态，每实例独占Workspace | `compiled_state_internal.h`、`codec.cpp` | Framer必须使用相同的`CompiledStateRef`模式；销毁/移动外部`CompiledProtocol`后仍可用 |
| public Codec的Decode视图会被该Codec下一次受检执行或move失效 | `include/pae/codec.h` | Framer候选与Codec结果都是短借用；Host业务回调只能在同一同步栈使用 |
| 内部Host对业务/观察器异常设faulted，要求Reset，并保留消费事实 | `host_endpoint.cpp`的`Deliver`、`DecodeCandidate`和`Push` | 后续public Host需明确异常后的流代次，不得把输入伪装成零消费可重试 |

当前内部API仍明确标注为non-installed；本报告不把其枚举数值或Schema白名单直接提升为公共ABI。

## 2. 阶段 2A：公开 Framer 最小 API

以下是可实施的C++17接口形状。名称属于低风险实现细节；语义、所有权和消费边界才是本阶段应冻结
的内容。公共头不得包含`protocol_plan`、`protocol_framing`或其他`src/**`类型。

```cpp
namespace pae {

enum class FramerStatus {
  OK,
  INVALID_ARGUMENT,
  INVALID_COMPILED_PROTOCOL,
  PIPELINE_OUT_OF_RANGE,
  INPUT_KIND_NOT_STREAM,
  RESOURCE_LIMIT_EXCEEDED,
  ALLOCATION_FAILED,
  WORKSPACE_BUSY,
  REENTRANT_CALL,
  INTERNAL_ERROR,
};

enum class FramerStopReason {
  INPUT_EXHAUSTED,
  NEED_MORE,
  WORK_BUDGET_REACHED,
  SINK_STOP,
};

enum class FramingIssue {
  NONE,
  MALFORMED_LENGTH,
  RECORD_TOO_LONG,
};

enum class FrameSinkAction { CONTINUE, STOP };

struct FrameCandidateView {
  ByteView bytes;
  std::size_t pipeline_index;
};

using FrameSinkFunction = FrameSinkAction (*)(FrameCandidateView, void*) noexcept;

struct FrameSink {
  FrameSinkFunction function = nullptr;
  void* context = nullptr;
};

struct StreamFramerOptions {
  std::size_t max_submit_bytes = 0U;
  std::size_t max_frames_per_submit = 0U;
  std::size_t max_work_units = 0U;
  std::size_t max_sync_bytes = 0U;
  std::size_t execution_memory_limit_bytes = kUseDefaultExecutionMemoryLimit;
};

struct FramingMemoryReport {
  std::size_t workspace_bytes = 0U;
  std::size_t frame_buffer_bytes = 0U;
  std::size_t facade_bytes = 0U;
  std::size_t framer_accounted_total_bytes = 0U;
  std::size_t retained_compiled_state_facade_bytes = 0U;
  std::size_t allocation_count = 0U;
};

struct StreamSubmitResult {
  FramerStatus status = FramerStatus::INVALID_ARGUMENT;
  FramerStopReason stop_reason = FramerStopReason::INPUT_EXHAUSTED;
  std::size_t bytes_consumed = 0U;
  std::size_t candidates_delivered = 0U;
  std::size_t bytes_discarded = 0U;
  std::size_t malformed_candidates = 0U;
  FramingIssue last_issue = FramingIssue::NONE;
  std::size_t work_units_used = 0U;
};

class PAE_API StreamFramer final {
 public:
  StreamFramer(const StreamFramer&) = delete;
  StreamFramer& operator=(const StreamFramer&) = delete;
  StreamFramer(StreamFramer&&) noexcept;
  StreamFramer& operator=(StreamFramer&&) noexcept;
  ~StreamFramer();

  [[nodiscard]] StreamSubmitResult Push(ByteView input, FrameSink sink) noexcept;
  [[nodiscard]] StreamSubmitResult Continue(FrameSink sink) noexcept;
  [[nodiscard]] FramerStatus Reset() noexcept;
  [[nodiscard]] FramingMemoryReport MemoryReport() const noexcept;

 private:
  struct Impl;
  explicit StreamFramer(std::unique_ptr<Impl>) noexcept;
};

struct StreamFramerCreateResult {
  FramerStatus status = FramerStatus::INVALID_ARGUMENT;
  std::unique_ptr<StreamFramer> framer;
  FramingMemoryReport memory;
};

[[nodiscard]] PAE_API StreamFramerCreateResult CreateStreamFramer(
    const CompiledProtocol& compiled,
    std::size_t pipeline_index,
    const StreamFramerOptions& options = {}) noexcept;

}  // namespace pae
```

### 2.1 所有权、move与绑定

- `CreateStreamFramer`通过现有`CompiledProtocolAccess::Acquire`取得同一`CompiledStateRef`。Framer
  自身保活Plan；创建后原`CompiledProtocol`可移动、替换或销毁。
- 一个实例在创建时永久绑定一个Pipeline和一条逻辑流。首片不提供`Rebind`。替换配置或Pipeline
  必须创建新实例；旧实例的半包和统计不得迁入。
- move转移整条流状态；moved-from对象只可析构或重新赋值。候选借用在owner move后立即失效，
  与现有Codec view epoch规则一致。
- 不公开Plan指针、内部索引引用或workspace。`pipeline_index`是当前公开Compiler metadata可查询的
  稳定实例内索引，只在同一编译状态中解释。
- 不提供共享一个Framer实例的线程安全承诺。不同实例可以各自串行使用并只读共享编译状态；实际
  跨线程并发返回`WORKSPACE_BUSY`，同一线程活动callback链内直接或A→B→A间接重入同实例返回
  `REENTRANT_CALL`。

### 2.2 `Push`、未消费后缀与`Continue`

- `Push`最多调用一次内部`PushStreamChunk`，不自动循环，不保存调用方未消费后缀。
- `bytes_consumed`严格位于`[0,input.size]`，只表示本次输入已被接受的连续前缀。调用返回后，
  `input[bytes_consumed,input.size)`完全归调用方；调用方下一次只可重提该后缀。
- 已消费并复制进内部半包的字节不得重提。`input.size > max_submit_bytes`须在状态推进和回调前失败，
  消费、交付均为0且原流状态不变。
- `Continue(sink)`只等价于对确有内部可执行工作的实例执行内部empty push。它不读取网络、不等待
  数据、不接受新字节。若只有半包，返回`NEED_MORE`；若完全空闲，返回`INPUT_EXHAUSTED`。
- `Push({nullptr,0}, sink)`与`Continue(sink)`执行同一个内部empty push；二者在状态、计数、停止原因
  和回调上必须等价。`Continue`只是表达调用者意图更清楚的便捷入口，不能形成第二套推进逻辑。
- `WORK_BUDGET_REACHED`时，调用方先保留未消费后缀；若公开Observation显示存在内部工作，先调用
  `Continue`直至不再需要内部推进，再提交后缀。Observation至少公开phase、buffered bytes、
  `has_internal_work`和有效submit/work上限；测试必须覆盖有后缀和仅内部pending两种情况。

### 2.3 候选借用、STOP和异常

- `FrameCandidateView::bytes`只在当前sink回调期间有效，来源可能是内部buffer。调用方若异步保存，
  必须在回调内复制。返回后不得读取，也不得假设下一候选复用相同地址。
- 调用sink即当前候选的交付提交点。sink返回`STOP`后，当前候选计数并从Framer移除；它不会在下次
  `Push`或`Continue`重现。当前候选之后尚未消费的输入后缀仍归调用方。
- 2A只接受`noexcept`函数指针和`void*`上下文，不接受`std::function`，避免热路径分配并防止异常
  穿过`noexcept`边界。违反`noexcept`是调用方C++契约违规，不能由PAE可靠恢复。
- 回调内调用同一个Framer的`Push`、`Continue`或`Reset`必须返回`REENTRANT_CALL`且不改变状态；
  同一活动callback链内A→B→A也按A重入处理，不能退化为并发`WORKSPACE_BUSY`。不能死锁、递归交付
  或隐式排队。回调可以使用另一个当前未活动的Framer实例或一个独立Codec实例，但调用方仍负责
  各实例串行。
- 如果产品必须支持可抛异常的C++ callable，应留给2B Host适配器捕获，并在精确消费事实基础上
  将该逻辑流标记为reset-required；不应把异常捕获复杂度塞进2A Framer首片。

### 2.4 候选不等于Decode成功

- `candidates_delivered`只表示Framer形成并提交了完整边界，不表示Message唯一、长度/完整性/字段
  语义通过。`MALFORMED_LENGTH`和`RECORD_TOO_LONG`是Framer事实；Codec错误属于候选下游。
- Framer不得调用`MatchCompleteRecordStructure`或`DecodeCompleteRecord`，也不得因Decode结果回扫
  已提交候选。SUM/CRC通过与否不能用于选择或重定义边界。
- 非Lab消费者在候选回调内调用现有public `CompleteRecordCodec::Decode`时，每个候选最多调用一次。
  业务若还需要raw/显示材料，应从同一次Decode的公开结果读取或复制，不能再次Decode。
- 一个Framer和一个Codec都保活同一编译状态，但它们各自拥有独立可变workspace；Framer不能借用
  Codec workspace，Codec也不能借用Framer buffer到回调之外。

## 3. 阶段 2B：薄 Host 组合边界

2B不应直接安装当前`host_endpoint.h`。建议在2A稳定后新增面向宿主的不可变实例：

- 创建输入为`CompiledProtocol`、一组`EndpointBindingSpec`和聚合上限；绑定项包含业务端点ID、
  显式`DECODE/ENCODE`、Pipeline index及Decode逻辑流数量。
- 创建时完成全部绑定、动作、input kind、资源和重复身份校验，再原子发布；创建失败不返回部分实例。
- 每条Decode流持有一个独立Codec；stream pipeline另持有一个独立public Framer。Encode绑定固定
  Pipeline而不固定Message，调用期按public Message index选择；其独立Codec和输出buffer按该Pipeline
  所有可Encode Message的最大可靠上界预配。编译状态由所有子对象引用计数保活。
- 绑定表发布后不可变；配置或绑定替换通过停止调用、销毁旧Host、创建新Host完成。首片不做热替换、
  在线解绑、句柄复用或状态迁移。Handle必须带实例scope/generation，不能只是数组下标。
- 完整记录Decode直接调用该流Codec一次；stream Push由Framer交付候选，再由同流Codec Decode一次。
  诊断观察和业务成功交付必须共享这一次结果，绝不能“观察一次、业务再Decode一次”。
- 只有Codec成功进入业务sink。失败候选可进入同步诊断observer，但字段视图为空或显式无值；候选数、
  Decode尝试数、Decode失败数与业务成功数分别报告。
- callback抛异常时，Host捕获、停止本次后续交付、保留Framer实际`bytes_consumed`，将目标流标记
  `RESET_REQUIRED`。之后对该流的执行失败关闭，直到显式Reset；其他流和Encode绑定不受影响。
- Reset只清理目标逻辑流的Framer/Codec可变状态并递增generation；不交付半包、不消费宿主后缀、
  不更换绑定或Plan。generation溢出失败关闭，不复用旧代身份。
- 同一Host实例的首片调用串行并拒绝回调重入；未来若要放宽为逐流并发，必须单独设计锁粒度、
  Handle生命周期和销毁规则，不能由“Plan只读可共享”推导出来。

### 3.1 需要内部继续提供、但不能泄漏的事实

2B适配层仍需要下列内部信息，应通过`CompiledStateAccess`或新的`src/public_api`私有helper读取，
不得复制Plan数据或Matcher：

1. Pipeline的input kind、framing profile索引及支持的Message集合；公开consumer metadata不足以完成
   workspace创建和损坏Plan防御。
2. Core workspace的精确估算、每Message最大字段数、最大Frame和Encode value映射容量。
3. Framer workspace创建返回的实际accounted bytes与有效session limit。
4. Message在Pipeline中的Decode/Encode action可用性。展示侧可用公开metadata；执行侧仍须以冻结
   Plan校验为准，不能相信调用方缓存的description。
5. 每次Decode同一workspace中形成的字段和raw转换结果。若public Codec尚未公开某个必要只读结果，
   应扩充同次Decode的借用视图，而不是Host回退到内部Core后二次执行。

### 3.2 重大取舍，需总控在2B实施前确认

以下不阻断2A，但会实质影响2B公共承诺：

- **Host回调异常政策。** 推荐捕获后精确报告消费事实并要求目标流Reset；另一选择是只接受
  `noexcept`回调。前者与现有内部Host一致但状态面更大，后者更小但宿主集成约束更强。
- **public Host首片是否同时暴露失败Candidate observer。** 推荐首片保留只读同步observer，因为
  Binary/Lab以外的诊断消费者需要区分候选与业务成功；但其raw/字段借用必须完全来自同一次Decode。
- **绑定身份输入。** 推荐使用调用方业务字符串作为不透明key、Pipeline使用公开index；不建议把
  endpoint字符串塞进协议配置。若要求整数key或宿主自定义token，应在2B开工前冻结比较与生命周期。

## 4. 非Lab最小调用示例

### 4.1 Binary stream：候选只Decode一次

```cpp
struct BinaryContext {
  pae::CompleteRecordCodec* codec = nullptr;
  std::size_t pipeline = 0U;
  std::size_t successful_records = 0U;
};

pae::FrameSinkAction OnBinaryFrame(pae::FrameCandidateView candidate, void* opaque) noexcept {
  auto& context = *static_cast<BinaryContext*>(opaque);
  const pae::DecodeResult decoded = context.codec->Decode(context.pipeline, candidate.bytes);
  if (decoded.status != pae::CodecStatus::OK) {
    return pae::FrameSinkAction::CONTINUE;  // 候选已提交，不要求Framer回扫。
  }
  // 在这里读取/复制decoded.record；回调返回后不保留candidate或借用字段视图。
  ++context.successful_records;
  return pae::FrameSinkAction::CONTINUE;
}

std::size_t offset = 0U;
while (offset < received.size()) {
  const auto result = framer->Push(
      {received.data() + offset, received.size() - offset}, {OnBinaryFrame, &context});
  offset += result.bytes_consumed;
  if (result.status != pae::FramerStatus::OK) break;
  while (result.stop_reason == pae::FramerStopReason::WORK_BUDGET_REACHED) {
    const auto continued = framer->Continue({OnBinaryFrame, &context});
    if (continued.status != pae::FramerStatus::OK || continued.bytes_consumed != 0U) break;
    if (continued.stop_reason != pae::FramerStopReason::WORK_BUDGET_REACHED) break;
  }
  if (result.bytes_consumed == 0U &&
      result.stop_reason != pae::FramerStopReason::WORK_BUDGET_REACHED) break;
}
```

实际示例实现应把循环写成有界驱动器，分别处理STOP、NEED_MORE、入口失败和无进展；上面只展示
后缀归属与一次Decode关系，不是Transport循环模板。

### 4.2 ASCII CRLF：边界形成不等于文本Decode成功

```cpp
struct TextContext {
  pae::CompleteRecordCodec* codec;
  std::size_t pipeline;
};

pae::FrameSinkAction OnAsciiRecord(pae::FrameCandidateView candidate, void* opaque) noexcept {
  auto& context = *static_cast<TextContext*>(opaque);
  const auto decoded = context.codec->Decode(context.pipeline, candidate.bytes);
  if (decoded.status == pae::CodecStatus::OK) {
    // 同步消费decoded.record；BYTES文本仍由宿主按业务解释。
  }
  return pae::FrameSinkAction::CONTINUE;
}
```

CRLF属于候选字节；ASCII字符、template、枚举或其他字段语义失败仍是Codec失败。Framer不得删除
终止符、转码或将失败候选重新拼接。

## 5. 最小实施文件范围

### 5.1 阶段 2A 建议范围

- 新增 `include/pae/stream_framer.h`；
- 新增 `src/public_api/stream_framer.cpp`；
- 最小修改 `src/public_api/compiled_state_internal.h`，仅复用既有状态获取/保活；
- 最小修改公开库CMake安装/导出清单及 `include/README.md`、公开API指南；
- 新增 `tests/public_api/public_stream_framer_tests.cpp`、header自包含测试、外部consumer测试；
- 新增一个Binary和一个ASCII非Lab公开示例，复用公开合成配置；
- 新增Windows阶段2A验证报告。

不修改`src/protocol_framing`算法，除非测试先证明当前内部语义与已确认契约不符；不得修改Core
Matcher/Codec、Schema、Lab、Qt、Evidence、网络或线程代码。

### 5.2 阶段 2B 建议范围

- 新增 `include/pae/host_endpoint.h`及`src/public_api/host_endpoint.cpp`；
- 优先组合2A和现有public Codec；若必须复用当前内部Session，先把其Plan/Core裸类型隔离在private
  adapter内，不能从公共头透出；
- 新增独立Host external consumer与生命周期/资源测试；
- 不新增Socket、串口、线程、重试、定时器、动态注册、协议映射或Lab Evidence接口。

## 6. Windows Debug / Release 验收矩阵

下表是后续实施验收要求，不是本轮已执行结果。Debug和Release均需执行，external consumer必须只
包含安装/导出的`pae/**`头和公开库，不能包含`src/**`。

| ID | 检查点 | Debug / Release必须精确证明 |
| --- | --- | --- |
| FH01 | 头文件与依赖 | `stream_framer.h`自包含；consumer不见Plan/Core/Qt/Lab；Product-only和Testing-off门禁正确 |
| FH02 | owner保活 | 创建Framer/Codec后移动、替换、销毁`CompiledProtocol`仍可执行；不同编译状态索引不混用 |
| FH03 | Binary边界 | fixed、sync-fixed、sync-length独立手写chunk；跨块、多帧、半包及完整候选字节准确 |
| FH04 | ASCII边界 | CRLF跨块、粘包、空记录、超长丢弃恢复；候选包含CRLF且不做字符Decode |
| FH05 | 消费后缀 | STOP、frame上限和work上限下`bytes_consumed`为精确前缀；后缀未保存，重提无丢失/重复 |
| FH06 | Continue | pending delivery、扫描/搬移可由Continue推进且消费0；纯半包NEED_MORE，空闲INPUT_EXHAUSTED |
| FH07 | 候选成功边界 | Framer交付计数与Codec成功/失败分别断言；UNKNOWN/AMBIGUOUS/INTEGRITY/字段失败均不回扫 |
| FH08 | 一次Decode | 每个候选Codec observer计数恰为1；业务结果和诊断复制来自同一次结果，无二次Matcher/Decode |
| FH09 | sink stop | STOP提交当前候选、不重现、不预取后缀；STOP与预算同时发生时STOP优先 |
| FH10 | 重入并发 | 同实例回调内Push/Continue/Reset为REENTRANT；同实例并发BUSY；另一实例可独立推进 |
| FH11 | reset/流隔离 | Reset丢弃目标半包/pending并开启新代；另一流、另一Codec和未消费宿主后缀不受影响 |
| FH12 | 资源 | 创建前exact/+1预算、乘加溢出、分配失败原子性；报告总计等于实际类别；首次/重复Push零分配 |
| FH13 | 操作上界 | `work_units_used`不越有效预算；连续垃圾、KMP回退、分段memmove均有限进展且无空转 |
| FH14 | moved-from/借用 | owner move后旧candidate不可保留；callback内复制后可用；Framer move不重复候选或串流 |
| FH15 | Host绑定（2B） | 重复/未知/错误动作/input kind拒绝且零部分发布；绑定不可变，替换必须新实例 |
| FH16 | Host异常（2B） | observer/business异常被捕获，消费事实保留，目标流RESET_REQUIRED；其他流不受影响 |
| FH17 | Host代次（2B） | stale/foreign Handle拒绝；Reset generation递增；销毁旧owner后句柄不能别名新实例 |
| FH18 | 共存回归 | public Codec、Compiler metadata、内部Framer/Host专项及全切片D/R不退化；无Lab/Qt依赖倒灌 |

资源结论必须分开报告：编译状态保留成本、Framer workspace及buffer、Codec workspace、Host facade/
绑定表/逐流对象、宿主回调自行复制。内部Framer已有的无分配和操作计数测试可以作为回归依据，
但不能替代public facade自身的首次调用、move和external consumer证据，也不能升级为正式性能认证。

## 7. 兼容边界与实施停点

- 阶段2A/2B不新增Schema或结果格式，不改变旧Codec、旧指纹、Lab Evidence或协议执行语义。
- public facade的Schema接受域必须从冻结Plan能力判断并与构建特性一致；不能复制当前内部Host仅接受
  0.9/0.10/0.11的临时白名单作为长期公共规则。完整记录Pipeline创建Framer必须明确拒绝。
- candidate字节可供任何非Lab消费者观察，但只有同一编译状态的public Codec才可按对应Pipeline
  解码；跨来源原始字节比较不等于跨Plan执行兼容。
- 本设计不包含Linux验证、稳定C ABI、网络、设备、现场、Golden、认证、防篡改或完整Runtime。
- 2A实施完成并经总控复核后停止；2B的异常政策、失败observer和绑定身份三项确认后再实施。

本轮只读审查未发现必须复制执行引擎或改变已确认Framer/Codec语义的阻断项。尚未验证的是上述
public facade能否在不新增私有反向依赖的前提下完全复用现有构建导出与内存报告结构；该项应由2A
编译和external consumer测试证明，不能由本文静态设计代替。
