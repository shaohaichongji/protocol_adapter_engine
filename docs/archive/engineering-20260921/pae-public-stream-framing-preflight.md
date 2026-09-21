# PAE 公开流式 Framing 静态描述预检

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 1. 范围、现场与结论

本轮按 `pae-execution-delivery-organization-plan.md` 当前 0.11 前置安排，仅静态核对 Frozen Plan、公开
`StreamFramer` / `HostEndpoint`、当前 ASCII 0.10 公共消费落点及既有测试；没有修改代码或 CMake，
没有构建、测试、网络、部署、Stage、Commit 或 Push。

关机恢复后重新核对现场：`main@dbf4798f96a45b8d36a4754ad5a01e274680337e`，相对
`origin/main` ahead 2，暂存区和工作树均为空；本报告此前尚未生成，没有残留 CMake、CTest、Ninja、
MSBuild 或编译器进程。以下行号均以该现场为准。

**结论：0.11 的 PAE 侧确定缺口是只读静态描述，不是切帧执行算法。** 当前公开能力查询只返回
`available`，不能区分三种 Binary stream 策略与 ASCII CRLF，也不能给出跨 chunk 的最大候选帧长
`M`；见 `include/pae/stream_framer.h:45-48,142-149`、
`src/public_api/stream_framer.cpp:262-281`。Frozen Plan 已保存全部所需事实，实际 Framer/Host 已提供
`Push/Continue/Reset/Observe`、精确 consumed、候选/业务结果分离和运行期预算观察；无需重做 Matcher、
Framer、Codec 或 Host 状态机。

建议增加一个以 Pipeline index 为 selector、返回自有纯值的 `QueryPipelineFramingDescription`，并保留
现有 `QueryStreamFramingCapability`。查询回答配置静态事实；实例 `Observe` 继续独占运行期 phase、buffer、
内部待处理工作、有效提交上限 `C` 与 work limit。`M` 与 `C` 不得混用。

## 2. 已确认源码事实

### 2.1 Frozen Plan 是唯一数据源

- 内部输入种类为 `COMPLETE_RECORD` / `STREAM_CHUNK`，策略为 `COMPLETE_RECORD`、
  `FIXED_LENGTH`、`SYNC_FIXED_LENGTH`、`SYNC_LENGTH_FIELD`、`ASCII_CRLF`：
  `src/protocol_plan/plan_types.h:88-111`。
- `FrozenFramingPlan` 保存 `input_kind`、`strategy`、固定帧长、sync、长度字段及 min/max；Pipeline 通过
  `framing_profile_index` 引用 profile：`src/protocol_plan/plan_bundle.h:174-188,288-293`。
  `PlanBundle` 已有只读 `FramingProfiles()` / `Pipelines()`：同文件 `416-417`，实现见
  `src/protocol_plan/plan_bundle.cpp:49-53`。
- Loader/Validator 已要求 ASCII terminator 严格等于 CRLF，且 `maximum_frame_length >= 2`；诊断明确
  maximum 包含 CRLF：`src/config_compiler/config_compiler.cpp:703-725`。冻结前再次核对各策略结构不变量：
  同文件 `2435-2487`。
- 编译期资源估算与运行 Workspace 当前采用同一映射：`SYNC_LENGTH_FIELD` / `ASCII_CRLF` 取
  `maximum_frame_length`，其余 stream 取 `frame_length_bytes`；见
  `src/config_compiler/config_compiler.cpp:3577-3586`、
  `src/protocol_framing/stream_framer.cpp:143-164`。Workspace 已按该容量计入自身字节。

因此公开查询不得重新解析 Schema、从 `available` 猜策略或从 Message 长度反推 `M`；应只读取已成功
编译的同一 Frozen Plan，并对不可能的内部状态失败关闭。

### 2.2 现有公开执行能力可复用

- `StreamFramer` 已提供 `Push/Continue/Reset/Observe`；候选视图只在同步 `noexcept` 回调内借用，零长
  Push 等价 Continue：`include/pae/stream_framer.h:107-124`。实现没有隐藏循环；Continue 直接调用
  `Push(ByteView{}, sink)`：`src/public_api/stream_framer.cpp:174-255`。
- `StreamFramerObservation` 只包含 phase、buffered bytes、internal work、有效 submit/work 限：
  `include/pae/stream_framer.h:95-102`。这是运行状态，不是静态配置描述。
- `HostEndpoint` 已提供 complete Decode、stream Push/Continue、Encode、Reset、Observe：
  `include/pae/host_endpoint.h:156-190`。Host 创建时用现有 capability 决定是否构造 Framer：
  `src/public_api/host_endpoint.cpp:673-676,737-759`；Push 复用同一 Framer 并逐候选 Decode：同文件
  `435-505`。因此静态查询不应塞入 `HostObservation`，也不应改变 Host 的 binding/channel 身份。
- 当前 ASCII 0.10 public Host adapter 仅公开完整记录 `Decode/Encode/Reset`：
  `tools/protocol_lab_ascii/public_ascii_host_adapter.h:38-42`。0.11 增量应在 Lab 后续任务中增加独立、
  有状态的 stream 消费层，而不是回退私有 Plan 或重写已收口的 complete adapter。

### 2.3 现有测试基线（只读核对，未在本轮执行）

- `tests/public_api/public_stream_framer_tests.cpp:226-247` 已覆盖 invalid/stream/complete/out-of-range
  capability 与 complete Create 拒绝；后续区段已有三种 Binary 策略、ASCII CRLF、STOP、精确 consumed、
  Continue、Reset、owner/move/reentry/busy。
- `tests/public_api/public_host_endpoint_tests.cpp:375-438` 已覆盖 Host stream Push/Continue、后缀、STOP、
  失败候选隔离与 Reset。
- 合成公开配置已经包含 Binary fixed、sync-fixed、sync-length 三类
  (`examples/config/synthetic_stream_framing_slice.pae.json:11-39`)；ASCII stream 的 M=12 和两个
  complete Pipeline 位于 `examples/config/synthetic_ascii_stream_slice.pae.json:11-25`。
- 上述是现有源码断言位置，不是本轮新执行证据。

## 3. 推荐的最小公共契约

以下为**推荐契约**，尚未实施或验证；命名可在实施派发时定稿，但语义不应留给消费侧猜测。

```cpp
enum class PipelineInputKind {
  COMPLETE_RECORD,
  STREAM_CHUNK,
};

enum class PipelineFramingStrategy {
  COMPLETE_RECORD,
  FIXED_LENGTH,
  SYNC_FIXED_LENGTH,
  SYNC_LENGTH_FIELD,
  ASCII_CRLF,
};

enum class PipelineFramingQueryStatus {
  OK,
  INVALID_COMPILED_PROTOCOL,
  PIPELINE_OUT_OF_RANGE,
  INTERNAL_CONTRACT_VIOLATION,
};

struct PipelineFramingDescription {
  std::size_t pipeline_index = 0U;
  PipelineInputKind input_kind = PipelineInputKind::COMPLETE_RECORD;
  PipelineFramingStrategy strategy = PipelineFramingStrategy::COMPLETE_RECORD;
  std::optional<std::size_t> maximum_candidate_frame_bytes;
};

struct PipelineFramingQueryResult {
  PipelineFramingQueryStatus status =
      PipelineFramingQueryStatus::INVALID_COMPILED_PROTOCOL;
  std::optional<PipelineFramingDescription> value;
};

[[nodiscard]] PAE_API PipelineFramingQueryResult QueryPipelineFramingDescription(
    const CompiledProtocol& compiled, std::size_t pipeline_index) noexcept;
```

建议放在 `include/pae/stream_framer.h`，实现于 `src/public_api/stream_framer.cpp`：该头已经承载静态
capability 与运行 Framer，且 public implementation 已能通过 `CompiledProtocolAccess` 只读 Frozen Plan。
公共枚举建议无条件保留所有已支持成员；构建时未启用的 Schema 能力只意味着对应值不可达，不应让
public header 的形状随 feature macro 漂移。

不建议直接扩展现有 `StreamFramingCapability`：它是已导出的按值返回结构，改变结构布局会扩大现有 C++
ABI 影响。保留旧函数和语义，并让其实现后续可复用新查询结果，只把
`input_kind == STREAM_CHUNK` 映射为 `available=true`，可避免两条索引/profile 校验路径长期分叉。

### 3.1 精确映射

| Frozen input/strategy | 查询 status/value | `maximum_candidate_frame_bytes`（M） |
| --- | --- | --- |
| `COMPLETE_RECORD` / `COMPLETE_RECORD` | `OK`，明确 complete | `nullopt`，不得伪造 stream M |
| `STREAM_CHUNK` / `FIXED_LENGTH` | `OK` | `frame_length_bytes` |
| `STREAM_CHUNK` / `SYNC_FIXED_LENGTH` | `OK` | `frame_length_bytes` |
| `STREAM_CHUNK` / `SYNC_LENGTH_FIELD` | `OK` | `maximum_frame_length` |
| `STREAM_CHUNK` / `ASCII_CRLF` | `OK` | `maximum_frame_length`，**包含终止 CRLF 两字节** |

M 是跨任意多个 chunk 后允许形成的单个候选记录最大字节数。它不是调用者一次 Push 可提交的最大 chunk
容量 `C`，也不是单次 work limit、当前 buffered bytes 或 phase。后四项仍只从实例 `Observe` / options
获得；静态查询不接受 runtime override。

### 3.2 失败语义

- moved-from/无有效 Plan 的 owner：`INVALID_COMPILED_PROTOCOL`，无 value。
- Pipeline index 越界：`PIPELINE_OUT_OF_RANGE`，无 value。
- profile index 越界、input/strategy 非法组合、stream M 为零、`uint64_t` 无法表示为 `size_t`、超过现有
  hard limit：`INTERNAL_CONTRACT_VIOLATION`，无 value。
- complete Pipeline 是合法静态描述，不返回 `INPUT_KIND_NOT_STREAM`；该状态仍属于
  `CreateStreamFramer` 的执行入口。

查询不承担配置诊断：非法 Schema 应在 Compiler 阶段失败，查询只防御损坏或违反冻结不变量的内部状态。

### 3.3 所有权、生命周期与资源

- 返回对象只含 enum、index、`size_t` 与 optional，全部为自有纯值；不借用 profile id、sync bytes 或
  CompiledProtocol 内存。owner 销毁后返回值仍内存安全，但 Pipeline index 只对原 owner 有身份意义。
- 查询只临时取得现有 compiled state 引用，不创建 Framer/Workspace、不缓存、不持久分配、不增加 Plan 或
  Workspace 计费。`std::optional<size_t>` 本身不要求堆分配；实施测试仍应使用现有分配观察器证明查询前后
  allocation count 不变。
- 推荐从内部 Framer 提取一个不分配的容量解析 helper，让 Workspace create 与公开查询共同使用现有四类
  映射及 checked conversion；否则第三份映射容易与 `config_compiler.cpp:3577-3586`、
  `protocol_framing/stream_framer.cpp:143-164` 漂移。该 helper 是私有实现，不是稳定公共 API。
- 不公开 sync bytes、length-field offset/order、minimum length 或 profile id：0.11 Lab 当前不需要这些数据，
  暴露会扩大 ABI、借用和消费者重实现 Framer 的风险。

## 4. ABI、导出与 SDK 影响

- 新增 public enum/struct/free function 和 DLL symbol，但不改变现有类或结构布局；这仍是新的实验性 C++ ABI
  表面，不能宣称与任意旧二进制双向兼容。旧 `QueryStreamFramingCapability` 必须继续导出。
- `stream_framer.h` 需直接包含 `<optional>`，不能依赖 `codec.h` 的间接 include。
- 现有 `src/public_api/CMakeLists.txt` 已编译 `stream_framer.cpp`，SDK 安装规则已复制 public headers 与
  `examples/public_api_sdk_consumer`；预期不需要根 CMake 新目标。实际实施后仍须检查 shared DLL export、
  source/static/shared 包内容、manifest 与 SHA-256，不能只凭源码构建通过推断包完整。
- 不改变 Schema、Frozen Plan 布局、Core/Codec、Host binding、Runtime/Session、C ABI、网络或 Qt。

## 5. A2 后的 0.11 消费增量

ASCII 0.10 complete 的公开描述、离线 adapter、显式 Host/UI 已形成前置检查点。本片之后的最小顺序仍是：

1. PAE 实现上述静态查询并完成独立 SDK 消费验证。
2. Lab 非 Qt stream adapter 在 owner 准备阶段查询 input/strategy/M；按配置选择 complete 或 stream，禁止
   从 `available` 推断 ASCII/Binary 策略。
3. stream 执行直接复用 `HostEndpoint::Push/Continue/Observe/Reset`；候选 observer 与成功 sink 在同次
   回调内有界复制，冻结未消费后缀由 Lab 自己持有。PAE 不接管 Tab/Flow/revision/取消替换事务。
4. Qt 只在非 Qt adapter 和公共事实闭合后接线；不因为补静态查询重跑或改写 A2 complete 逻辑。

现有执行 API 的语义应保持：STOP 已交付当前候选且不重放；Continue 只推进现有内部工作；Reset 丢弃该流
半包/pending 并使旧 Handle 失效；Observe 是串行空闲查询而非并发快照。静态描述查询不改变这些语义。

## 6. 建议实施文件与验证门槛

### 6.1 最小代码文件

- `include/pae/stream_framer.h`：公共纯值类型、query 声明、直接 `<optional>` include。
- `src/public_api/stream_framer.cpp`：Plan 读取、失败关闭、旧 capability 复用。
- `src/protocol_framing/stream_framer.h/.cpp`：仅当采用共享容量解析 helper 时最小调整；不改执行算法。
- `tests/public_api/public_stream_framer_tests.cpp`：精确 query、映射、失败和分配断言。
- `tests/public_api/header_stream_framer.cpp`：若现有仅 include 的门禁不足，再补新类型的独立头使用断言。
- `examples/public_api_sdk_consumer/main.cpp`：包外 consumer 实际调用新 symbol。

不预期修改 Plan、Compiler、Core、Host、Schema、Lab 或根 CMake；若实施证明需要，必须先说明原因并重新划界。

### 6.2 测试优先断言

1. invalid/moved owner、越界 index 无 value 且状态精确；通过既有内部损坏 Draft/Plan 测试机制覆盖非法
   profile index、组合与容量，不增加生产测试接口。
2. complete 返回 `COMPLETE_RECORD/COMPLETE_RECORD/nullopt`；不能只断言 `available=false`。
3. 合成 Binary 三策略分别返回 exact M=3、4、8；ASCII 返回 M=12，并单独断言 M 包含 CRLF。
4. query M 与实际 Workspace memory/candidate limit 同源；`C < M` 与 `C > M` 对照证明 C/M 不混淆。
5. 重复查询不增加 Plan/Workspace、无 heap allocation/cache；旧 capability 与新 query 一致。
6. 既有 Push/Continue/STOP/Reset/Host 测试保持；不需要新增网络测试。

建议 Windows Debug/Release 严格串行执行 public header、public Framer、Host 定向回归，再做 Product-only 与
Testing-off 零测试门禁。因新增 DLL 导出，应生成新的、独立路径的源码/static D/R/shared D/R SDK 候选，
完成包外 consumer、导入库/DLL symbol、manifest/hash 与依赖检查；不得覆盖既有 final6 或后续已验收包。
Lab 非 Qt/Qt 回归属于后续消费任务，不应拿 PAE query 单测代替。

## 7. 待总控定稿项与未验证边界

建议总控只需定稿以下接口层选择，不需要重新拍板执行语义：

1. 接受上述 `PipelineFraming*` 命名，或统一为现有 description API 的命名风格；推荐语义保持不变。
2. 接受独立 query/status，而不是扩大现有 `StreamFramingCapability` 布局；推荐保留旧函数并内部复用。
3. 接受首片只公开 input kind、strategy、M；不公开 profile id、sync/length-field 细节。
4. 接受 public enum 无条件稳定出现、feature-disabled 构建中对应值不可达。

本轮未构建、未运行测试、未生成 SDK、未检查实际 DLL symbol，也未验证任意 Lab/Qt、Linux、网络、硬件或
生产场景。无分配、无需 CMake 改动和可复用现有执行链的判断来自当前静态源码；须由后续实施验证闭合。

状态：**已完成派发范围，待总控复核。**
