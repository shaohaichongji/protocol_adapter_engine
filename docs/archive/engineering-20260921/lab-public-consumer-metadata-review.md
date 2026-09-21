# Lab 对公开消费准备 metadata 的复核

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

状态：2026-09-14 阶段 1B 后置只读复核结论，已完成派发范围，待总控复核。本文仅从 Lab
消费者视角核对最终公开 metadata、实现、测试源码及既有日志；本轮未修改公开接口或 Lab，未运行
构建、测试、示例、UI 或网络，也不表示 Lab 已迁移。

## 1. 结论

**前轮提出的消费准备 P1 缺口已在阶段 1B 约定范围内解决，未发现需要阻断本片收口的确定实现
问题。** Lab 现在可以仅凭公开 metadata 确定完整记录调用所需的 logical `ValueKind`、Field 在
Encode 中的 caller/generated/unreferenced 来源、实际 Pipeline/Message 关联的 Decode/Encode
可用性，以及 Encode 固定长度或安全上界；这些事实与 `CompleteRecordCodec` 使用的冻结 Plan
事实一致，不依赖解析 direction 文本或猜测 Schema 版本。

本结论的准确边界是“完整记录 Codec 的通用调用准备事实已经公开”。它不等于：

- 分块输入、候选形成、停止或复位等 streaming Framer API 已公开；
- Host binding、observation 或设备/线程生命周期已公开；
- 完整字段约束、物理 byte/bit range 或 Encode 同调用观察字段已提供；
- Lab 已能无损替换现有全部 Binary/ASCII/stream/Host 路径；
- 既有 Windows 自动化证据已经升级为人工 UI、Linux、硬件或现场验收。

因此可以按综合计划继续下一片，但不能直接宣称 Lab 可全量迁移。

## 2. 现场与复核范围

本轮现场：

- 仓库：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine`
- 分支：`main`
- HEAD：`dfb08f351cdb22a9b50c9e64e688e3b666e669bc`
- staged：空
- 工作树：已有大量已跟踪修改、删除和未跟踪文件；本轮全部保留

主要只读依据：

- `docs/engineering/pae-execution-delivery-organization-plan.md`
- `docs/engineering/pae-public-consumer-metadata-slice.md`
- `docs/engineering/pae-public-consumer-metadata-validation.md`
- `include/pae/compiler.h`
- `include/pae/protocol_description.h`
- `include/pae/codec.h`
- `src/public_api/compiler.cpp`
- `src/protocol_plan/plan_bundle.h`
- `src/protocol_core/complete_record_codec.cpp`
- `tests/public_api/public_consumer_metadata_tests.cpp`
- `tests/public_api/public_codec_tests.cpp`
- `examples/public_api_codec/main.cpp`
- `out/public-consumer-metadata-stage1/` 下既有最终日志

## 3. 前轮 P1 缺口核对

| 前轮缺口 | 最终公开事实 | 消费侧判断 |
| --- | --- | --- |
| logical 类型 | `FieldDescription::value_kind`，与 Codec 共用 `pae::ValueKind` | 已解决。普通字段来自冻结 `value_type`；存在 conversion 的字段公开为 `DECIMAL64`，没有把 wire/raw integer 冒充 logical 类型。Decode 的实际 conversion raw 仍由 Codec 结果独立提供。 |
| caller/generated/unreferenced | `FieldDescription::encode_value_source`：`CALLER_INPUT`、`CONSTANT`、`COMPUTED`、`NOT_REFERENCED` | 已解决本片范围。Binary 读取冻结 `encode_source`；ASCII 依据实际 `text_encode` action 与 `text_encode_input`，Decode-only Field 不会因内部默认值误报为 caller input。constant/computed 是明确的生成来源，`NOT_REFERENCED` 不与生成字段混同。 |
| Pipeline/Message 动作可用性 | `PipelineMessageExecution(pipeline_index, message_index)` 返回 `decode_available`、`encode_available` | 已解决。查询先验证真实关联和 allowed bit；Decode 依据 fixed/bounded/text candidate 集合，ASCII Encode 还要求实际 `text_encode` action。空 owner、moved-from、越界、跨 Pipeline 非成员均失败关闭。 |
| Encode 输出预配 | `EncodeOutputSizeKind::{EXACT,UPPER_BOUND,NOT_AVAILABLE}` 与 `encode_output_size` | 已解决可可靠公开的部分。固定 Binary 使用 `frame_size`；bounded 使用 `max_frame_length`；ASCII 使用 `text_encode.max_record_length`。这是固定值或安全上界，不是本次成功/精确长度承诺，本次仍以 `EncodeResult` 为准。 |
| 生命周期和空值 | description 的 `string_view` 借用 owner；新增 enum、bool、size 为值语义；查询返回 `optional` | 已解决本片范围。owner move/替换/销毁后旧字符串视图失效并须重查；空或 moved-from owner、非法索引和非法关联返回空结果。该生命周期不同于 Codec 自身的 Decode 短期 view。 |

### 3.1 与真实 Codec 语义的一致性

本轮核对到 metadata 与执行路径读取同一组冻结事实：

- logical Decimal 的识别依据是有效 `conversion_index`；Codec 也以该 conversion 处理
  logical Decimal 与实际 raw integer；
- Binary 输入/常量/计算来源来自 `FieldExecutionPlan::encode_source`，Codec 的输入绑定、常量覆盖
  和 computed 覆盖检查使用同一字段；
- ASCII caller input 来自 `text_encode_input`，Codec 的输入校验和段输出同样检查该标记；
- Decode 可用性读取 `candidate_groups`、`variable_message_indices`、`text_message_indices`，与
  Codec 完整记录匹配候选来源一致；
- Encode 关联门禁读取 `allowed_message_words`，与 Codec 的 Message allowed 判断一致；
- bounded 与 ASCII 的最大输出长度正是 Codec 最终检查的 `max_frame_length`、
  `max_record_length`。

因此没有发现 metadata 声称可执行、而当前完整记录 Codec 对同一关联必然按另一组动作事实判断的
确定分叉。能力查询只说明动作存在，Matcher、输入完整性、类型、数值、字符、容量和 final review
等运行期门禁仍可能令单次调用失败。

### 3.2 `NOT_REFERENCED` 的边界

`NOT_REFERENCED` 表示该 Field 不被当前 Message 的 Encode action 引用，不表示它是 constant 或
computed，也不表示整个 Message 不可 Encode。反之，ASCII literal 和 integrity storage 等不是
Field 的输出不会被伪造成 Field metadata。Lab 构造 Encode 输入时只应为 `CALLER_INPUT` 创建输入
值；是否显示其他 Field 及如何显示，仍属于 Lab 展示策略。

## 4. 动作、输出容量与 streaming 边界

`PipelineMessageExecution()` 描述的是某个实际关联能否调用**完整记录** Decode/Encode。对带 stream
framing profile 的 Pipeline，`decode_available=true` 仅表示消费者已经取得一条完整 record 后可将
其交给 `CompleteRecordCodec::Decode()`；它不提供：

- 分块 `feed`；
- 跨 chunk 缓冲和候选边界；
- candidate/stop/reset 语义；
- Framer 与 Host 的生命周期或并发契约。

现有公开 facade 测试把 stream-framed Pipeline 的完整记录调用与 metadata 对应起来，这一测试是
防止错误地把该 Pipeline 判为完全不可调用，不是 public streaming API 的存在证明。

输出容量也需分层理解：

- `EXACT`：成功记录长度固定，可按该值预配；
- `UPPER_BOUND`：只保证安全上界，本次实际长度可能更小；
- `NOT_AVAILABLE`：该关联没有 Encode action，size 为 0；
- `EncodeResult::required_size/bytes_written`：本次调用事实，不能被 metadata 替代。

## 5. 公开示例的覆盖与限制

示例已不再硬编码 Message/Field/Enum 的位置索引：它先经 `PipelineMessageIndex()`、Field/Enum ID
查找和 `PipelineMessageExecution()` 取得公开 selector、类型、来源与输出容量，再创建 Codec；只包含
`pae` 公开头。这足以证明已知配置的仓库外消费者可用 metadata 准备 Binary/ASCII 完整记录调用。

示例仍有意硬编码以下应用知识：

- 使用 Pipeline 0 和关联 0；
- 已知的 Field/Enum ID；
- 业务输入值、期望 ASCII 文本及 Binary 记录长度；
- 只对部分 Field 显式检查类型/来源，而不是枚举所有 `CALLER_INPUT` Field 动态生成输入。

这些硬编码不构成本片实现缺陷，因为示例不是通用协议编辑器。但它不能证明 Lab 已具备通用输入
表单、全部 Field 约束预校验、任意配置的默认值策略或错误展示。Lab 后续适配应枚举 Message Field，
仅为 `CALLER_INPUT` 按 `value_kind` 建立输入；不应复制示例中的具体 ID、索引或业务值。

## 6. 测试与既有证据复核

本轮只读确认当前测试源码覆盖：

- Binary/Decimal/bounded/ASCII 的 logical type、Encode 来源、动作与输出容量；
- constant、computed、ASCII `NOT_REFERENCED`；
- 实际关联、非成员、空 owner、move、越界和查询期无新增分配；
- unknown enum 的 raw/empty-known/tainted 传播；
- ASCII literal-only 的零输入 Encode、零字段 Decode 与 unsupported action；
- stream-framed Pipeline 的完整记录动作；
- Decimal Encode 失败的 status、conversion error、input ordinal、Field 位置与零交付。

本轮只读看到以下阶段 1B 最终日志：

- metadata 直测：Debug 21/21，Release 21/21；
- Codec facade 直测：Debug 57/57，Release 57/57；
- public API CTest：Debug 5/5，Release 5/5；
- metadata 驱动公开示例：Debug/Release 均 `gate=PASS`；
- 仓库外 consumer：Debug/Release 均 `exit_code=0`。

这些是 PAE 执行任务此前生成、总控已核对的既有 Windows 日志，本轮没有重跑。未核对为本轮证据
的范围包括：Lab/UI、人工交互、网络、Linux、硬件、现场、独立 SDK/ABI 及性能。

## 7. 非阻断延期项与下一片建议

以下仍是明确的延期项，不构成阶段 1B 阻断：

- Field 完整输入约束，例如数值范围、BYTES/ASCII 长度与字符集；
- 物理 byte/bit range 及可变记录本次实际范围；
- Encode 同调用生成/观察字段及实际 conversion raw；
- public Framer/Host、稳定 handle、SDK/ABI 与 UI 模型。

建议下一片按综合计划推进 public Framer/Host，先冻结分块输入、候选、停止、复位、关联与生命周期
语义，再做非 Lab 宿主验证。不要因为 Pipeline metadata 已暴露完整记录动作，就把现有私有 Framer
或 Host 类型直接带入公开层。

在后续 Lab 迁移前，物理范围和 Encode 同调用观察仍需单独决策，否则现有 Hex 高亮与生成字段展示
无法无损迁移；完整字段约束可作为提升通用输入 UX 的后续 metadata，而 Codec 运行期校验仍是最终
执行依据。不得通过重复 Decode、logical 反算 raw 或 UI 解析 Schema/direction 来填补缺口。

## 8. 本轮交付边界

- 实际新增：`docs/engineering/lab-public-consumer-metadata-review.md`
- 未修改：其他报告、公开 API、Core/Plan、Lab、CMake、README/index、AGENTS.md、Qt
- 未执行：配置、构建、测试、示例、UI、网络、格式化、清理或目录移动
- Git：未 Stage、未 Commit、未 Push
- 停点：报告完成后停止写入，等待总控复核；未启动 Lab 迁移
