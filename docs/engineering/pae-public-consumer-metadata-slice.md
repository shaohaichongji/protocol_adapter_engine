# PAE 公开消费准备 metadata 最小契约

状态：2026-09-14 阶段 1B 已确认实施契约。本文只定义公开完整记录 Codec 的消费准备事实，
不引入 UI、Framer、Host、Evidence 或新的协议执行语义。

## 1. 公开事实

- `ValueKind` 是 metadata 与 Codec 共用的逻辑值类型域。普通字段由冻结 `ValueType` 映射；存在
  Decimal conversion 的字段公开为 `DECIMAL64`，其 wire/raw 整数类型不冒充 logical 类型。
- `EncodeValueSource` 描述字段在该 Message 的 Encode 动作中的来源：`CALLER_INPUT`、
  `CONSTANT`、`COMPUTED` 或 `NOT_REFERENCED`。`NOT_REFERENCED` 与生成字段不同；ASCII literal、
  integrity storage 等非 Field 输出也不伪装成 Field。
- `MessageExecutionDescription` 只针对一个实际 Pipeline/Message 关联，公开该关联是否可执行
  Decode、Encode。查询不解析 `direction_id` 文本，也不表示任意输入必然成功；Matcher、Integrity、
  类型、容量等运行期门禁仍由 Codec 决定。
- Encode 可用时公开 `EncodeOutputSizeKind`：`EXACT` 表示每次成功结果长度固定，
  `UPPER_BOUND` 表示可安全预分配的上界；不可 Encode 时为 `NOT_AVAILABLE` 且 size 为 0。
  本次实际长度仍以 `EncodeResult::bytes_written/required_size` 为准。

## 2. 冻结 Plan 对应

| 公开事实 | 冻结事实 |
| --- | --- |
| logical `ValueKind` | `FieldExecutionPlan::value_type`；有效 `conversion_index` 覆盖为 `DECIMAL64` |
| caller/generated/unreferenced | Binary `encode_source`；ASCII `text_encode_input` 与 action 是否存在 |
| Decode 可用 | Message 位于 Pipeline 的 fixed/bounded/text Decode 候选集合 |
| Encode 可用 | Message 位于 Pipeline allowed 集合；ASCII 还必须存在 `text_encode` action |
| 固定输出 | Binary `frame_size`；或 action 的 min/max 相等 |
| 输出上界 | bounded `max_frame_length`；ASCII Encode `max_record_length` |

这些映射只读取已经冻结、编译期验证的有界数组；查询不分配内存，不解释字符串，不新增资源计费。

## 3. 生命周期与失败关闭

metadata 中的 `string_view` 仍借用 `CompiledProtocol`；新增 enum、数值和 bool 均为值语义。
空或 moved-from owner、索引越界、Message 不属于 Pipeline、内部关联损坏均返回空结果。
调用者在 owner move/替换后必须重新查询。既有 `pae::ValueKind` 名称和 Codec 头自包含性保持。

## 4. 本片边界

本片不公开物理 byte/bit range、字段全部校验约束、Encode 同调用输出字段观察、Framer/Host、
稳定 handle、SDK/ABI 承诺或 UI 模型。输出上界是编译后配置事实，不是性能保证，也不替代运行期
`BUFFER_TOO_SMALL` 与其他失败结果。
