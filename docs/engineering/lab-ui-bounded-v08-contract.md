# Protocol Lab UI Schema 0.8 有界变长完整记录检查点契约

日期：2026-09-10

实施基线：`4a04afb13d8dc38ea59bd7e7aa3cc3bba370c301`

分支：`feat/lab-ui-c1`

## 1. 范围

本检查点只扩展离线 Protocol Lab UI 的 Encode 与 Inspect，使其接受 Schema 0.8
`bounded_payload` 完整记录。它继承 Schema 0.5 至 0.7 的字段编辑、结果呈现和离线边界，
不引入 stream Framer、TCP、UDP、串口、设备或 Evidence 输出。

UI 只复制既有 `PlanBundle` 中的有界载荷、动态 integrity 与 computed length 元数据；
不保留借用的 Plan 指针，不复制 Core 的长度或校验算法。实际 Frame、computed length 和
integrity 值仍由既有 Execution Bridge 调用 Core 生成或验证。

## 2. 输入与边界

- Schema 0.5 至 0.7 Encode 继续使用既有 Values `pae.lab.values/0.4`；Schema 0.8
  单独使用既有 Values `pae.lab.values/0.5`。
- 有界 payload BYTES 编辑器先校验 canonical uppercase Hex 词法，再校验协议定义的
  `min_payload_bytes..max_payload_bytes`。两类失败必须有不同原因。
- 当最小长度为零时，空字符串是合法 BYTES 输入；固定长度旧代行为不变。
- UI CMake 目标要求显式启用 `PAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER`。Schema 0.9
  不属于该 UI 合同。

## 3. 实际布局呈现

- 有效 Encode 结果以 `encoded_frame.size()` 为实际 Frame 长度；有效 Inspect 结果以
  `input_frame.size()` 为实际 Frame 长度。
- payload 实际范围为 `header_length + actual_payload_length`，动态 integrity storage
  锚定在本次 payload 末尾。表格、详情与 Hex 高亮使用同一解析结果。
- 没有有效结果时，只显示 payload 长度约束和“current range unavailable”，不得把最大
  Frame 布局显示为当前布局。
- 空 payload 显示零长度实际范围，不制造 payload 高亮。
- 输入变化、失败、Pipeline 切换和重新加载必须清除旧结果及旧动态高亮。
- 失败态只显示可由固定描述可靠确定的位置；动态 payload 或动态 integrity trailer
  不借用最大布局伪造失败位置。

## 4. 验收

自动验证至少覆盖：合法空载荷、合法最大载荷、越界载荷、词法/长度错误区分、动态
trailer、computed length、有效 Inspect、integrity 失败后恢复，以及 Schema 0.5 至 0.7
必要回归。shown-window smoke 证明控件状态和高亮路径实际执行，但不替代一次后续人工布局
检查，也不构成真实协议、硬件、网络或现场验收。
