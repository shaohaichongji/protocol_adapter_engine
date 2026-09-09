# Protocol Lab 离线 Encode Inspector 首检查点契约

状态：已确认方向，已授权独立工作区实施，尚未实现或验收

历史设计参考基线：`7d2ef4f343b1924df3eb052f4a8a128525d7dc37`

已提交的 PAE 功能基线：`3ae4df7b07dec6604ae4177a378c642b0f7e7694`

适用范围：首个 Windows Qt 5.13 离线动态 UI 检查点

## 1. 文档目的与当前状态

本文冻结首个离线动态 UI 检查点的目标、边界、跨模块接口、所有权、资源计费、失效规则、实施顺序和验收要求，供后续在独立工作区中一次完成实现与集成。

已确认的方向包括：

- Compiler 在同一次成功编译事务中可选地产生 description sidecar（描述侧车）；
- sidecar 只补充 `PlanBundle` 缺失的显示 metadata（元数据），并按最终 Plan 索引绑定；
- 旧 `CompileJsonToPlan` 入口不产生 sidecar，不承担额外 sidecar 分配、计费或构建成本；
- 不修改 PAE Schema，不向 `PlanBundle` 添加 UI 字段；
- 首 UI 最多两个相互隔离的 Tab，支持 Schema 0.5、0.6、0.7；
- 使用单 worker 异步 Load，typed（类型化）字段编辑和手动 Encode；
- 输入或选择变化时立即清除旧输出；
- Encode 必须经过 Core Encode 和独立 Decode 最终复核；
- 输出 Hex 只读，并支持字段到物理 byte/bit 的高亮；
- UI 使用 Qt 5.13 和 v142 独立构建，PAE 主线默认 v145 不变。

尚未冻结为数值契约的内容包括：

- sidecar 的最终字节上限；
- 同步 Encode 的可接受延迟门限。

二者必须由实际结构计费或实测数据确定。历史草案中的 `4 MiB`、`50 ms`、`100 ms` 均不是已确认门限。

PAE 有界变长、Schema 0.8 和 Values 0.5 已在 `3ae4df7` 本地提交。2026-09-10 用户授权交付该提交及本契约，并从包含本契约的最终提交创建独立工作区实施 UI。总控在派发中指定最终精确 commit hash；本文中的旧行号和结构上限参考不得替代该基线的实际源码。实施授权不代表 UI 已实现或验收通过。

## 2. 目标

首检查点命名建议：`PAE-UI-C1 Offline Encode Inspector`。

首检查点必须实现：

1. Windows Qt Widgets 应用，最多同时存在两个文档 Tab；
2. 每个 Tab 独立加载一个 PAE 配置并独立持有编译产物；
3. 从已编译 Plan 选择 Pipeline 和该 Pipeline 允许的 Message；
4. 对 `EncodeSource::INPUT` 字段提供类型化编辑；
5. 用户手动点击 Encode 后调用既有 Lab C1 执行桥；
6. 仅在 Core Encode 和独立 Decode 最终复核均成功后显示 Hex；
7. 选中字段时高亮它实际占用的 frame byte/bit；
8. 对 constant、computed length、integrity、conversion 等信息提供只读注解；
9. 对配置错误、输入错误、Core 拒绝、复核失败和 stale result（过期结果）实行失败关闭；
10. Debug 和 Release 均完成真实窗口业务流验收，而不是只证明空窗口可启动。

## 3. 非目标

首检查点明确不包含：

- UDP、串口、Socket 或其他通信；
- Inspect/Decode UI；
- Replay、Compare、Evidence Bundle 或任何 Evidence 写入；
- Values、配置或 UI 状态持久化；
- Raw Hex 编辑；
- Hex 到字段的反向编辑；
- 自动 Encode、debounce（防抖）预览；
- Schema 0.8 UI 支持；
- 新 Schema 功能；
- Qt 6；
- DEI、EIK、Communication、Boost 或其他业务依赖；
- 完整 Protocol Lab 桌面化；
- 将 internal 接口发布为稳定 SDK；
- 硬件、现场、长稳或生产发布验收。

## 4. 版本边界

### 4.1 UI 支持代次

首 UI 的应用层 allowlist（允许列表）固定为：

- Schema 0.5；
- Schema 0.6；
- Schema 0.7。

UI 不支持 Schema 0.8。UI 对 0.8 或其他版本返回独立的 `UI_SCHEMA_UNSUPPORTED`，销毁本次临时编译产物，不创建可执行文档会话。

### 4.2 不回退共享能力

UI allowlist 只限制 UI，不限制共享 Compiler 或 Lab：

- description sidecar 编译入口应跟随正式稳定基线中 Compiler 已启用的 Schema 能力；
- 不得为了 UI 将共享 Compiler、Lab bridge 或测试中的 0.8 支持删除、屏蔽或改回 0.7；
- 若正式基线中的 Compiler 能成功编译 0.8，UI 在编译成功后于 `DocumentSession` 边界拒绝它并释放产物；
- 不在共享 `ExecutionBridge` 中新增“拒绝 0.8”的回退逻辑；
- 后续 0.8 UI 支持必须作为独立扩展检查点重新定义字段、映射和验收。

## 5. 每 Tab 的所有权模型

每个 `DocumentTab` 独占以下状态，不得与另一 Tab 共享：

- `DocumentId`；
- `LoadRevision`；
- `PlanGeneration`；
- `SelectionRevision`；
- `InputRevision`；
- `PreparedDocument`；
- `FieldModel`；
- typed draft values；
- `PreviewResult`；
- 当前诊断和字段错误。

允许跨 Tab 共享的对象仅限：

- 无可变会话状态的代码；
- 只读图标、样式等应用资源；
- 单一 compile worker 及其有界调度器。

### 5.1 身份

`DocumentId` 在应用进程内单调生成，Tab 关闭后不复用。

`LoadRevision` 在每次 Load 或 Reload 请求时递增。

`PlanGeneration` 只在当前 `LoadRevision` 的完整编译产物成功发布时递增。

`SelectionRevision` 在 Pipeline 或 Message 发生变化时递增。

`InputRevision` 在任一输入字段草稿发生变化时递增。

选择身份必须包含 Plan、Pipeline 和 Message：

```text
SelectionKey = {
  plan_generation,
  pipeline_index,
  pipeline_id,
  message_index,
  message_id
}
```

预览身份必须包含全部可影响结果的 revision：

```text
PreviewKey = {
  document_id,
  load_revision,
  plan_generation,
  selection_revision,
  input_revision,
  selection_key
}
```

只绑定 Plan 而忽略 Pipeline/Message 选择不满足本契约。

### 5.2 临时编译产物与 `PreparedDocument`

worker 产生的临时 `CompiledUiArtifacts` 是 move-only（仅移动）所有者；它在发布前唯一拥有 `PlanOwner` 和 sidecar。GUI 收到当前 revision 的完整产物并完成 UI Schema allowlist 检查后，必须将 `PlanOwner` 唯一 move 进 `ExecutionBridge::AdoptCompiledPlan`，临时产物不再拥有 Plan。

最终会话中的 `PreparedDocument` 至少包含：

```text
PreparedDocument {
  unique_ptr<ExecutionBridge> bridge;
  UiDescriptionSidecar description;
  string config_sha256;
}
```

`ExecutionBridge` 内部唯一拥有 `PlanOwner`，并基于它建立 main workspace 和 review workspace；`PreparedDocument`、sidecar、model 和 Tab 均不得再次拥有 Plan。bridge 实现中的成员声明顺序必须使两个 workspace 在 `PlanOwner` 之前析构。

sidecar 只按 index 与该 Plan 绑定，不保存 `PlanBundle*`。UI model 不保存 `PlanBundle&`、`PlanBundle*`、`FieldRef` 或 `EnumValueRef` 作为长期状态；建立中性 model 所需的 Plan 读取只能发生在 `PreparedDocument` 有效期间，并把所需值复制为不含 Plan 引用的 descriptor。每次 Encode 由 bridge 根据当前 `SelectionKey` 和 typed values 重新建立 Plan-scoped 引用。

## 6. 加载、失效与关闭状态机

每个 Tab 采用以下状态：

- `EMPTY`：没有 Plan；
- `LOADING(load_revision)`：当前配置正在 worker 中处理；
- `READY(plan_generation, selection)`：已有 Plan，但没有有效输出；
- `PREVIEW_VALID(preview_key)`：存在与当前所有 revision 完全一致的成功输出；
- `CONFIG_ERROR(load_revision)`：当前加载失败，没有可执行 Plan；
- `CLOSING`；
- `CLOSED`。

### 6.1 Load/Reload

Load 或 Reload 开始时必须：

1. `LoadRevision++`；
2. 立即清除 Hex、高亮、字段输出、选择和旧诊断；
3. 禁用 Encode；
4. 释放旧 `PreparedDocument`；
5. 进入 `LOADING`；
6. 将新请求提交给有界调度器。

不允许在新文件加载失败后继续显示旧 Plan 的 Hex 或字段表，使用户误认为旧结果属于新文件。

worker 返回时，只有以下条件全部满足才可发布：

- Tab 尚未关闭；
- `DocumentId` 相同；
- 返回的 `LoadRevision` 等于当前 `LoadRevision`；
- 编译产物完整；
- Plan 与 sidecar 索引审计通过；
- Schema 在 UI allowlist 中；
- bridge 成功接管同一个 Plan。

否则销毁结果，不更新 UI。

### 6.2 Pipeline/Message 切换

切换 Pipeline 时：

- `SelectionRevision++`；
- 清空 Message、字段草稿、Hex 和高亮；
- Message 下拉框只列出该 Pipeline 允许的 Message。

切换 Message 时：

- `SelectionRevision++`；
- 重建字段 model；
- 清空 typed draft、Hex 和高亮；
- 进入 `READY`。

### 6.3 输入变化

任何输入字段变化时：

- `InputRevision++`；
- 立即清空 Hex、高亮和上次 Encode 的字段输出；
- 保留当前输入草稿及新的输入校验提示；
- 不自动触发 Encode。

### 6.4 手动 Encode

点击 Encode 时：

1. 完整验证 typed draft；
2. 生成当前 `PreviewKey` 快照；
3. 构造 bridge 所需的 typed values；
4. 在 GUI 线程同步调用 Encode；
5. Core Encode 成功后，由独立 review workspace 调用 Core Decode；
6. 复核 Message 必须等于选择的 Message；
7. 完整结果物化成功；
8. 再次核对 `PreviewKey`；
9. 一次性替换 Hex 和高亮数据，进入 `PREVIEW_VALID`。

任一步失败时：

- 不显示 encoded frame；
- 不显示 Decode 得到的部分字段；
- 不显示 byte/bit 高亮；
- 可以根据 `failed_value_index` 或 `failed_field_index` 标记一个输入行；
- 显示一个稳定诊断。

### 6.5 关闭

关闭 Tab 时必须：

1. 进入 `CLOSING`；
2. 使 `DocumentId` 对后续回调永久失效；
3. 从调度器删除该 Tab 的 pending 请求；
4. 已运行的 compile 不强制中断，但其结果必须被丢弃；
5. 断开或屏蔽 queued callback；
6. 清除 UI model 和预览；
7. 销毁 `PreparedDocument`；其中 bridge 按“review workspace、main workspace、唯一 `PlanOwner`”的顺序完成内部释放；
8. 进入 `CLOSED`。

## 7. 单 worker 有界排队

全应用只允许一个串行 compile worker。

调度器最多保存：

- 一个 active 请求；
- 每个 Tab 一个 latest pending 请求；
- 两个 Tab 合计最多两个 pending 请求。

同一 Tab 在其请求仍 pending 时再次 Load/Reload：

- 用新请求替换旧 pending 请求；
- 释放旧请求持有的配置字节；
- 不追加队列节点。

同一 Tab 在其请求 active 时再次 Load/Reload：

- active 请求继续完成，不尝试中断 Compiler；
- 为该 Tab 仅保留一个最新 pending 请求；
- active 结果因 `LoadRevision` 过期而被丢弃。

active 完成后，从仍存活 Tab 的 pending 请求中按 enqueue sequence（入队序号）选择最早者运行。被替换的请求不保留原序号，以防一个高频 Reload 的 Tab 永久占用优先级。

每个配置请求在进入 worker 前必须先按文件大小执行 4 MiB 上限检查；Compiler 仍执行权威输入审计。由此 pending 配置字节不会无限增长。实现必须在测试中证明：

- 单 Tab 连续 Reload 不增加 pending 节点数；
- 两 Tab 交替 Reload 不超过两个 pending；
- 关闭 Tab 会移除其 pending；
- active stale result 不会回填；
- 一个 Tab 的高频替换不会饿死另一 Tab 已 pending 的请求。

跨线程结果必须保持唯一所有权，并满足以下安全性质：

- 结果在 worker、调度器和 Tab 之间任一时刻只有一个所有者；
- receiver 销毁、Tab 关闭、pending 替换或回调未投递时，结果及其临时 `PlanOwner`、sidecar storage 都会被回收；
- 迟到结果即使到达 GUI 线程，也必须先通过 `DocumentId` 和 `LoadRevision` 检查；
- 不允许为方便信号传递而复制 Plan、sidecar 或 workspace；
- 不允许依赖裸指针跨线程维持结果生命周期。

本文不指定 Qt signal、queued functor、事件对象或线程安全 mailbox 中的某一种传递实现。正式实现必须针对选定的 Qt 5.13 API 证明上述唯一所有权、receiver 销毁和未投递结果回收行为；既有 Qt 窗口烟测不构成这项证明，也不得为传递机制引入新的外部依赖。

## 8. Description sidecar

### 8.1 最小内容

sidecar 只保存最终 `PlanBundle` 没有保留的显示 metadata：

- protocol：`display_name`、`description`、`source_ref`；
- pipeline：`display_name`、`description`、`source_ref`；
- message：`display_name`、`description`、`source_ref`；
- field：`display_name`、`description`、`source_ref`；
- enum entry：`display_name`。

首检查点不展示 framing profile metadata，因此不进入 sidecar。

sidecar 不复制：

- stable id；
- direction；
- Pipeline/Message 关系；
- frame length；
- wire、constant、computed length、integrity；
- enum raw value；
- conversion；
- execution descriptors；
- resource requirements 或 Plan memory report。

这些事实全部从同一次编译产生的最终 Plan 读取。

### 8.2 索引绑定

sidecar 使用 flat arrays（平坦数组），顺序与最终 Plan 完全一致：

- pipeline metadata ordinal 等于 `PlanBundle::Pipelines()` index；
- message metadata ordinal 等于 `PlanBundle::Messages()` index；
- field metadata 通过每个 message 的 `field_begin/field_count` 对齐；
- enum metadata 通过每个 field 的 `enum_begin/enum_count` 对齐。

发布前必须逐层检查数量和 range：

- pipeline 数相等；
- message 数相等；
- 每个 message 的 field 数相等；
- 每个 field 的 enum entry 数相等；
- 所有 begin/count checked-add 后位于对应 flat array 内；
- 所有 metadata string span 位于 sidecar storage 内。

不复制 id 作为第二套关联键。索引审计失败属于 internal contract violation，Plan 和 sidecar 均不发布。

### 8.3 同次编译事务

新增 internal、move-only 结果，名称可在实现时按现有风格调整：

```text
CompiledUiArtifacts {
  PlanOwner plan;
  UiDescriptionSidecar description;
  DescriptionMemoryReport description_memory;
}
```

入口语义：

```text
CompileUiArtifactsResult CompileJsonToPlanWithUiDescription(
    string_view json_bytes,
    size_t description_memory_limit_bytes);
```

事务顺序：

1. 执行现有 strict input/json/schema/domain/resource 校验；
2. 从已经验证的 SchemaIr 计算 sidecar layout；
3. 校验 description limit；
4. 分配单一 sidecar storage；
5. 将需要保留的 decoded metadata 字节复制进 sidecar；
6. 继续组装和冻结 Plan；
7. 对最终 Plan 与 sidecar 做索引审计；
8. 仅在全部成功后同时发布。

如果 Plan Freeze 或最终审计失败，已构造 sidecar 随临时结果销毁，不得单独发布。

现有 `CompileJsonToPlan` 必须保留独立的 no-sidecar 分支：

- 不计算 description layout；
- 不分配 description storage；
- 不复制 metadata；
- 不执行 sidecar 索引审计；
- 原诊断、Plan、内存报告和测试结果保持不变。

### 8.4 copy 与 move 的真实边界

sidecar 选择 flat arrays + 单块 storage 是为了实现确定性、可计费的所有权。该选择意味着：

- `SchemaIr` 中的 `std::string` 不能零复制 move 进单块 storage；
- metadata UTF-8 payload 必须复制一次到 sidecar storage；
- descriptor 记录通过 placement construction 或等价的定长写入建立；
- `SchemaIr` 临时字符串在后续编译阶段结束时释放；
- 不得同时宣称“单块 packed storage”和“`std::string` 零复制 move”。

若未来改用多个 `std::string` move，必须重新定义 capacity、allocation count 和实际内存计费；不属于首检查点。

## 9. Sidecar 有界计费

### 9.1 计费分类

`DescriptionMemoryReport` 至少包含：

```text
object_bytes
string_bytes
index_bytes
alignment_bytes
allocation_count
accounted_total_bytes
```

定义：

- `object_bytes`：顶层对象及 protocol/pipeline/message/field/enum descriptor arrays；首版内嵌在 `MessageMetadata` 的 `field_begin/field_count` 和内嵌在 `FieldMetadata` 的 `enum_begin/enum_count` 也计入本项；
- `string_bytes`：实际复制到 storage 的 decoded UTF-8 metadata payload；
- `index_bytes`：首版固定为 0；只有未来确实增加独立于 descriptor 的 index array 时才计入，并须同步更新布局、公式和测试；
- `alignment_bytes`：各区域对齐造成的 padding；
- `allocation_count`：sidecar 上游 storage 分配次数，首版成功结果必须为 1；
- `accounted_total_bytes`：包含 alignment 的单块 storage 总尺寸。

所有乘法、加法和 alignment 必须 checked。任何溢出、limit 超出、分配失败、估算与最终写入不一致均失败关闭。

不得使用以下方式代替计费：

- 配置文件字节数；
- `std::vector::capacity()` 的事后粗估；
- 仅统计字符串而忽略 descriptor/index/alignment；
- debug allocator 的偶然观测值；
- 某个样例配置的实测峰值。

### 9.2 已有结构上限

最终上限推导必须使用正式稳定基线中的实际定义。以参考基线为例，Desktop 已有结构上限包括：

- pipeline：32；
- message：64；
- total field：8192；
- total enum entry：65536；
- Compiler total decoded string budget：2 MiB。

Constrained 的对应数量更小。

这些数字是结构输入，不直接等于 sidecar 最终字节 limit。

### 9.3 最小可验证上限

sidecar limit 不使用预先拍定的 `4 MiB`。实现检查点按以下方式机械确定：

```text
maximum_string_bytes = Compiler 已有 decoded-string hard limit

maximum_object_bytes =
    sizeof(DescriptionHeader)
  + max_pipeline_count * sizeof(PipelineMetadata)
  + max_message_count * sizeof(MessageMetadata)
  + max_total_field_count * sizeof(FieldMetadata)
  + max_total_enum_count * sizeof(EnumMetadata)

maximum_index_bytes =
    0  // 首版 begin/count 已包含在 descriptor 的 sizeof 中

maximum_alignment_bytes =
    按真实布局顺序逐段 AlignUp 后的最坏 padding

derived_description_limit =
    最终布局函数对上述最大输入运行得到的 accounted_total_bytes
```

要求：

- 使用当前目标 ABI 的真实 `sizeof`/`alignof`；
- 每个 storage byte 只能归入一个分类；内嵌 begin/count 不得在 `object_bytes` 和 `index_bytes` 中重复计费；
- Desktop 和 Constrained 分别推导；
- 由同一个 `DescriptionLayout` 实现同时服务生产估算和测试，不维护手写的第二套公式；
- 生成 `exact`、`below`、`above` 三类边界测试；
- `exact` 成功且最终报告等于估算；
- `below` 以稳定的 UI description resource diagnostic 失败；
- `above` 用受控 test-only limit 或 checked-overflow fixture 失败；
- 数值随正式稳定基线和目标 ABI 记录在验证报告中，但不写成跨 ABI 永久承诺。

对于合法输入，实际 required bytes 由实际元素数量和实际 metadata payload 计算。UI compile policy 使用对应 profile 的 `derived_description_limit`。这给出可证明的上界，而不是经验常数。

### 9.4 编译峰值

UI sidecar 构造期间，SchemaIr metadata 与 sidecar payload 会短暂同时存在。验证报告必须区分：

- sidecar 持久 accounted bytes；
- UI-specific compile 相对旧入口增加的峰值上界；
- yyjson parser pool；
- Plan storage。

不得把 sidecar 单块大小描述为整个 Compile 的峰值内存。旧入口不构造 sidecar，因此旧入口峰值不应因本功能增加。

## 10. 最小 Lab 执行依赖

首 UI 不得直接链接整个 `pae_protocol_lab_internal`，否则会无必要地带入 UDP、Evidence、Replay 和 Windows Socket 依赖。

建议新增小型 internal static target，例如：

```text
pae_protocol_lab_c1_execution_internal
```

最小源集合：

- `v06_execution.h/.cpp`；
- `v06_format.h/.cpp`；
- `v06_values_compat_internal.h/.cpp`，仅因既有 `EncodeValuesText` 符号依赖；
- `sha256.h/.cpp`；
- 新增 `exact_value_text_internal.h/.cpp`。

它只链接：

- config compiler；
- protocol core；
- protocol plan；
- yyjson；
- project options。

它不包含：

- UDP；
- Evidence Bundle；
- v07 Run/Evidence；
- CLI；
- `ws2_32`。

现有完整 Lab target 从自己的 source list 中移除上述实现文件并链接该小目标。其余 Lab 文件和职责不重构。

### 10.1 已编译 Plan 接管

参考基线中的 `ExecutionBridge::Prepare(string_view)` 会自行 Compile，UI 不能调用它再次编译。新增 internal factory，例如：

```text
ExecutionBridge::AdoptCompiledPlan(
    PlanOwner plan,
    string config_sha256,
    PreparationFailure& failure);
```

该入口：

- 接管同次 Compiler 事务产生的 `PlanOwner`；
- 建立 main/review workspace；
- 不读取 JSON；
- 不拥有 sidecar；
- 不写 Evidence；
- 不执行 UI 版本 allowlist。

UI 在调用该入口前完成自己的版本检查。共享 bridge 保留正式稳定基线中的支持代次，不得因 UI 范围回退。

### 10.2 精确值解析的最小共享

参考基线存在多组历史 exact parser，它们的接受域和诊断不应在本检查点被强行统一。

本检查点只做以下必要抽取：

- 将 `v06_format.cpp` 当前使用的 canonical UINT64、INT64、uppercase Hex 和 Decimal64 解析实现移动到 `exact_value_text_internal`；
- `v06_format` 和 UI delegate 使用该同一实现；
- `v06_values_compat_internal` 保留自己的兼容接受域和诊断；
- `protocol_operations.cpp` 的 legacy parser 保持不变。

不属于首检查点的工作：

- 合并全部历史 parser；
- 重写既有 diagnostics；
- 统一 legacy Values format；
- 重构完整 Lab target。

## 11. Typed 编辑契约

UI 只为 `EncodeSource::INPUT` 提供编辑器。

### 11.1 UINT64

- canonical 十进制；
- `0` 合法；
- 非零值不允许前导零；
- 不允许符号；
- 使用 shared exact parser 做精确溢出检查。

### 11.2 INT64

- canonical 十进制；
- 不允许 `+`；
- 不允许前导零；
- 不允许 `-0`；
- 必须精确接受 `INT64_MIN` 和 `INT64_MAX`；
- 使用 shared exact parser 做溢出检查。

### 11.3 DECIMAL64

- 编辑器由 coefficient 和 scale 两部分组成；
- coefficient 使用相同 INT64 parser；
- scale 为 0 到 18；
- 使用既有 `NormalizeDecimal64`；
- Core 负责 logical/raw conversion 和最终 representability 判定；
- UI 不复制 conversion 算法。

### 11.4 BYTES

- 只接受大写 Hex；
- 不允许空格或分隔符；
- 字符数必须等于 `2 * field byte width`；
- 最大字符数由 Plan width 推导；参考 Desktop 最大 frame 下不得超过 131072；
- 使用 shared exact parser。

### 11.5 ENUM

- 只能选择最终 Plan 中的 enum entry；
- UI 保存 entry index 和 id；
- Encode 时 bridge 重新建立 Plan-scoped `EnumValueRef`；
- 不允许输入任意 raw enum value。

### 11.6 BOOL

- 使用 typed checkbox；
- 不接受字符串布尔值。

### 11.7 constant 与 computed

- 显示在字段表中；
- 值和来源只读；
- 不进入 typed input 集合；
- UI 不允许 override；
- Core 的 constant/computed override 防御仍必须保留并测试。

## 12. Core 执行与最终复核

UI 复用 C1 `EncodeParsed` 路径，不生成临时 Values JSON。

bridge 必须继续执行：

1. 查找 Pipeline；
2. 查找 Message；
3. 检查 Message 属于所选 Pipeline；
4. 按字段 id/index 建立 Plan-scoped `FieldRef`；
5. 按 enum index 建立 Plan-scoped `EnumValueRef`；
6. 调用 `EncodeCompleteRecord`；
7. 仅在 Encode 成功后使用独立 review workspace 调用 `DecodeCompleteRecord`；
8. 检查 review Message 与选择一致；
9. 完整物化字段结果；
10. 仅在全部成功后交付 encoded frame。

UI 不调用 v07 `ExecuteRunAndWrite`，不要求 record root，不为每次预览发布 Bundle。

Core 返回失败、review 失败、Message 不一致或物化失败时，bridge 不得交付 frame。UI 不得从临时 buffer 绕过该规则显示 Hex。

## 13. 物理 bit 映射

Qt 层不得再次解释 Schema 的 bit numbering 或 byte order。

description/session builder 从最终 Plan 的以下数据生成只读物理映射：

- `MessageExecutionPlans()[message].fields[field].bit_mask`；
- `MessageExecutionPlans()[message].fields[field].bit_shift`；
- `Messages()[message].bit_containers[container_index].byte_offset`；
- container `byte_width`；
- container `byte_order`。

`bit_numbering` 已由 PlanBuilder 折算进 resolved `bit_shift`，UI 不再使用原始 `bit_offset` 重新计算一次。

输出结构使用：

```text
PhysicalBitMask {
  frame_byte_index;
  uint8_mask;
}
```

对 `bit_mask` 中的每个 container numeric bit `n`：

```text
little endian:
  frame_byte = container_offset + n / 8

big endian:
  frame_byte = container_offset + (container_width - 1 - n / 8)

mask_in_byte = 1 << (n % 8)
```

同一 field 落在同一 byte 的多个 bit 合并成一个 `uint8_mask`。

非 bitfield 使用最终 resolved field offset/width 生成 byte range。computed length 和 integrity 只生成只读 storage/range 注解，不成为输入字段。

### 13.1 bit 映射测试

必须覆盖：

- LSB0 和 MSB0；
- little endian 和 big endian container；
- 单字节和多字节 container；
- 首 bit、末 bit；
- 跨 byte field；
- 相邻 field 不互相高亮；
- computed/integrity 区域；
- Encode 失败时 mapping 不进入有效预览。

每个代表 case 使用 Core 编码一个单 bit 或明确 bit pattern，并同时比较：

- 独立 expected frame bytes；
- `PhysicalBitMask`；
- Hex view 实际高亮 cell/mask。

若 mapper 与 Core 不一致，只修 mapper 或 description builder，不修改 Core 迎合 UI。

## 14. UI 结构与渲染边界

窗口使用 `QTabWidget`，最多两个文档 Tab。

每 Tab 建议布局：

- 顶部：配置路径、Load/Reload、schema/protocol 状态；
- Pipeline/Message 两级选择；
- 中部：字段 `QTableView`；
- 下部：只读 Hex view；
- 侧边或下方：只读 description/source/conversion/integrity 注解；
- 明确的手动 Encode 按钮。

字段使用 `QAbstractTableModel` + delegate，不为每个字段创建常驻 QWidget。Hex 以 16 bytes/row 的 model/view 展示，只绘制可见行。

参考基线 Desktop 上限：

- frame：64 KiB，即最多 4096 个 Hex row；
- fields/message：2048；
- total fields：8192；
- enum entries/field：4096。

Constrained 上限更小。UI 不另造更宽松上限；正式稳定基线若改变这些值，实施前重新读取实际定义。

Hex、高亮和字段结果只在 `PREVIEW_VALID` 中存在。失败路径不显示部分输出。

## 15. 同步 Encode 性能测量

首版采用 GUI 线程同步 Encode，但这不是“已证明不会卡顿”的结论。

必须使用 `QElapsedTimer` 分别记录：

- exact input materialization；
- bridge Core Encode + review Decode；
- result materialization；
- Hex model replacement；
- 首次可见 repaint 前的总耗时。

测量对象至少包括：

- 代表性 Schema 0.5；
- 代表性 Schema 0.6；
- 代表性 Schema 0.7；
- 最大 frame/字段规模的有界合成配置；
- Debug 和 Release，结论分开。

测量报告提供 sample count、warm-up、min/median/p95/max 和机器/构建信息。本文不预设 `50 ms` 或 `100 ms` 为硬门。

若实际窗口操作出现明显阻塞，或数据表明同步路径不满足后续由用户确认的交互预算：

- 功能正确性可以单独记录；
- 性能验收不得写为通过；
- 提出后续 per-tab Encode worker 设计；
- 不在首检查点中暗自加入第二类 worker 或跨线程 workspace 生命周期。

## 16. CMake、工具链与部署

新增默认关闭选项：

```text
PAE_BUILD_PROTOCOL_LAB_UI=OFF
```

UI 独立 build directory 使用：

- Visual Studio 2026 generator；
- x64；
- `v142,version=14.29.30133`；
- Qt 5.13；
- UI 需要的 PAE Compiler/Core/Schema gates。

PAE 主线默认 v145 不改变。不得在根 CMake 中全局改成 v142。

Qt 路径通过 cache 变量或环境输入，例如：

```text
PAE_QT_ROOT=<external Qt 5.13 root>
```

版本库不得记录本机 `F:\...` 绝对路径。CMake 必须校验 Qt header、Debug/Release import library、DLL 和 platform plugin 的版本与存在性。

UI target 只链接：

- Qt Core；
- Qt Gui；
- Qt Widgets；
- 最小 C1 execution internal；
- 其必要的 PAE Compiler/Core/Plan 传递依赖。

部署白名单：

- UI EXE；
- `Qt5Core[d].dll`；
- `Qt5Gui[d].dll`；
- `Qt5Widgets[d].dll`；
- `platforms/qwindows[d].dll`；
- 验收所需的公开合成配置。

禁止部署或链接：

- Qt Network；
- Qt SerialPort；
- DEI；
- EIK；
- Communication；
- Boost；
- UDP/Evidence 专用模块。

CRT 不复制进源码树。最终报告记录实际加载路径、文件版本和 import 审计，并区分“v142 编译工具集”与“统一 VC Runtime 实际文件版本”。

## 17. 正式实施目录

推荐新目录：

```text
tools/protocol_lab_ui/
  CMakeLists.txt
  main.cpp
  application_window.h/.cpp
  document_tab.h/.cpp
  document_session.h/.cpp
  field_table_model.h/.cpp
  exact_value_delegate.h/.cpp
  hex_view.h/.cpp
  compile_worker.h/.cpp
```

推荐测试目录：

```text
tests/protocol_lab_ui/
  CMakeLists.txt
  description_layout_tests.cpp
  description_mapping_tests.cpp
  document_state_tests.cpp
  compile_queue_tests.cpp
  dual_tab_isolation_tests.cpp
  ui_smoke_driver.cpp
```

名称可以按正式基线的本地风格小幅调整，但模块边界不得合并回 `QMainWindow`。

## 18. 文件独占表

### 18.1 Shared Compiler owner

独占修改：

- `src/config_compiler/config_compiler.h`；
- `src/config_compiler/config_compiler.cpp`；
- `src/config_compiler/schema_ir.h`，仅在确需增加 assembler friend 时；
- `src/config_compiler/CMakeLists.txt`；
- 新增 sidecar storage/layout 文件；
- `tests/config_compiler/CMakeLists.txt`；
- `tests/config_compiler/config_compiler_contract_tests.cpp` 或独立 sidecar test 文件。

职责：同次 Compile、old-entry zero cost、单块计费、索引审计、失败不发布。

### 18.2 Lab C1 owner

独占修改：

- `tools/protocol_lab/v06_execution.h/.cpp`；
- `tools/protocol_lab/v06_format.cpp`；
- `tools/protocol_lab/CMakeLists.txt`；
- 新增 `exact_value_text_internal.h/.cpp`；
- 对应 v06 format/execution tests。

职责：最小 target、compiled Plan 接管、canonical parser 复用、既有 C1/Core/review 行为不变。

不得顺手修改：

- `v06_values_compat_internal` 接受域或诊断；
- legacy `protocol_operations` parser；
- Evidence、UDP、Replay 合同。

### 18.3 UI owner

独占 `tools/protocol_lab_ui/` 新目录，不修改 Shared Compiler/Lab 文件。

职责：状态机、Tab 所有权、队列客户端、models/delegates、手动 Encode、Hex/highlight。

### 18.4 UI validation owner

独占 `tests/protocol_lab_ui/` 和最终 Windows 验证记录，不修改产品共享实现。

### 18.5 Integrator

独占：

- 根 `CMakeLists.txt` 的 UI option/gates/subdirectory；
- Qt import helper；
- 最终跨目标集成；
- v145 共享回归与 v142 UI 构建边界；
- 部署、依赖审计和交付报告。

## 19. 一次执行的实施顺序

两项方向已确认后，首检查点不再拆成多轮功能微审批。实施仍需遵守 Git 与外部状态授权边界。

### 19.1 稳定基线与隔离

Owner：总控与 Integrator。

完成当前 PAE 交付复核和经授权的提交后，由总控指定精确基线 hash；再经授权创建独立 worktree/branch，记录初始 HEAD 和状态。正式实现必须先读取稳定基线中的 Compiler、Lab、Schema 0.8 和 Values 0.5 现状，不得用参考基线的旧实现覆盖新能力。

### 19.2 Compiler sidecar

Owner：Shared Compiler owner。

实现可选同次 sidecar、单块 layout/计费、一次 metadata copy、最终 Plan 索引审计和无部分发布。旧 `CompileJsonToPlan` 使用明确的 Plan-only 路径，不进入任何 sidecar 专属阶段。解决第 5、9 节规定的唯一所有权和分类要求。

### 19.3 最小 Lab C1 执行目标

Owner：Lab C1 owner。

只抽取第 10 节列出的最小 target，增加 compiled Plan 接管入口并保持 `EncodeParsed` 的 Core Encode + review Decode 语义。保留稳定基线中的 legacy parser、Schema 0.8 和 Values 0.5 行为，不把完整 Lab 重构为前置工作。

### 19.4 Headless 会话、队列和 bit mapper

Owner：UI owner 与 UI validation owner，各自保持产品目录和测试目录独占。

先实现不依赖窗口的 `PreparedDocument`、revision/state、单 worker 有界调度、typed draft、手动 Encode 协调和物理 bit mapper；证明唯一所有权、关闭、pending 替换和迟到结果安全后，再接 Qt 界面。

### 19.5 Qt UI 与独立 v142 集成

Owner：UI owner 与 Integrator。

实现两个 Tab、models/delegates、只读 Hex/highlight 和显式 Encode。Qt 5.13 与 v142 的选择直接采用已有验证结论；本步骤只验证新 UI 行为、实际链接闭包和 Debug/Release 集成，不重新开展 Qt 版本选型或把既有空窗口烟测当成新成果。

### 19.6 集中验收与交付审查

Owner：UI validation owner、Integrator 和总控。

按第 20 节完成受影响回归、真实窗口流程、性能测量和部署审计。总控集中审查实现、证据和未验证边界；之后再分别请求 Stage、Commit 和 Push 授权。

## 20. 按影响验收清单

### 20.1 Compiler 接口影响

- 现有 Compiler contract、snapshot、diagnostic 和 resource tests 通过；
- 对同一配置，旧入口的 Plan snapshot 与 PlanMemoryReport 保持原结果；
- test-only sidecar probe 分别统计 layout、storage allocation、metadata copy 和 index audit；旧入口在有效与无效配置下四项均为 0；
- 旧入口在“sidecar 一旦触达即失败”的 test-only 注入下仍按原契约完成；不得用全局 `operator new` 次数代替 sidecar 专属证据；
- 新入口覆盖实际 layout 等于最终 report、单次上游分配、exact/below/above、checked overflow、allocation failure 和 Plan Freeze 失败无部分发布；
- Plan/sidecar 的 pipeline、message、field、enum 数量及 range 全部一致；
- `field_begin/field_count` 与 `enum_begin/enum_count` 只计入 `object_bytes`，首版 `index_bytes==0`；
- 稳定基线若已支持 0.8，新 Compiler sidecar 入口不回退该支持。

### 20.2 Lab target 与执行桥影响

- v06 format、v06 execution 和 legacy values compatibility 的受影响测试通过；
- target 拆分前后 legacy Values 0.1 至 0.4 的接受域和 diagnostic 不变；
- 稳定基线若已交付 Values 0.5/Schema 0.8，其既有 fixture、空 BYTES 和版本绑定结果不变；
- `AdoptCompiledPlan` 成功后 bridge 是 `PlanOwner` 的唯一所有者；失败时输入 Plan 被确定性释放；
- main/review workspace 在 Plan 之前析构；
- Encode/review/materialization 任一失败均不交付 frame；
- UI 链接闭包不包含 UDP、Evidence 或 `ws2_32`。

### 20.3 会话、队列与 bit 映射影响

- `SelectionKey` 同时绑定 Plan、Pipeline 和 Message；所有 revision 变化均使旧预览失效；
- 单 Tab 重复 Load 只替换一个 latest pending；两个 Tab 合计 pending 不超过 2；
- 关闭移除 pending，active 迟到结果不发布且其唯一资源被回收；
- 正式选定的 Qt 5.13 跨线程传递实现有独立测试，覆盖 receiver 销毁、未投递回调和结果释放；不复制 Plan、sidecar 或 workspace；
- 两个 Tab 不共享 Plan、workspace、model、draft 或 revision；
- UI 层拒绝 0.8 并释放临时产物，不改变共享 Compiler/Lab 行为；
- typed parser、constant/computed 只读和输入变化立即清空输出通过；
- bit mapper 使用手写 expected frame bytes 与手写 expected physical masks 分别验证 LSB0/MSB0、big/little、单/多字节、跨 byte 和相邻字段隔离；不得从 Core 输出反推 expected mask。

### 20.4 Windows UI 与部署影响

- UI 及其链接的 PAE 静态库在独立 v142 Debug/Release 中构建；主线默认 v145 不变；
- 使用既有已验证 Qt 5.13 输入，确认本次产品 target 的 compile/runtime 版本和模块实际加载路径；
- 真实窗口流程覆盖双 Tab、Load/Reload、Pipeline/Message、全部 typed editor、手动 Encode、独立 expected bytes、Hex/highlight、失败清空和 compile 期间关闭；
- 分段记录 Encode、review、materialization 和 repaint 耗时，不套用未确认的固定门限；
- 部署文件 hash 匹配，direct import/runtime module 审计只含白名单，不出现 Network、SerialPort、DEI、EIK、Communication、Boost、UDP 或 Evidence 依赖。

该验收只证明当前机器上的离线 UI 检查点，不提升为硬件、现场、长稳或生产验收。

## 21. 实施包

正式授权后，一次实施包包含：

- 精确稳定基线 hash；
- 独立 worktree/branch；
- 本契约；
- Shared Compiler sidecar实现与 tests；
- Lab C1最小target与 compiled Plan接管；
- UI产品目录；
- UI headless tests；
- Qt 5.13/v142 CMake path contract；
- v145共享回归记录；
- v142 Debug/Release构建记录；
- 自动与人工窗口验收记录；
- sidecar derived limit计算和边界证据；
- 同步Encode性能测量；
- 部署hash、依赖和runtime module审计；
- 已验证/未验证/风险/后续项报告。

不包含 Qt 副本、机器绝对路径、Evidence输出、真实协议或公司敏感配置。

## 22. 后续授权边界

本文自身不授予实施权限；2026-09-10 用户已另行授权下列稳定基线、独立工作区和实施验证步骤。后续实现的 Stage、Commit、Push 仍须另行授权。执行前至少核对：

1. 当前有界变长交付形成稳定 commit；
2. 总控明确指定 UI 正式基线 hash；
3. 授权创建独立 worktree/branch；
4. 授权按本文文件独占表修改源码并运行共享回归、Qt构建和窗口测试；
5. 允许只读引用外部 Qt 5.13 路径；
6. 实施完成后，Stage/Commit 分别取得用户授权；
7. Push 单独授权。

不需要再次审批本文已经冻结的每个普通实现细节；若实施发现必须修改 Schema、`PlanBundle`、Core语义、UI支持代次、通信/持久化范围或Evidence行为，则属于实质范围变化，必须停止并重新请求决策。

## 23. 当前确认记录

已确认：

- description sidecar方向；
- 只补Plan缺失显示metadata；
- 同次成功Compile与最终Plan索引绑定；
- 旧Compile入口零额外sidecar成本；
- 不改Schema/PlanBundle；
- 最多2 Tabs；
- UI支持Schema 0.5/0.6/0.7；
- 单worker异步Load和有界latest-pending队列；
- typed编辑；
- 手动Encode；
- 输入/选择变化立即清旧输出；
- Core Encode + 独立Decode复核；
- readonly Hex与byte/bit高亮；
- Qt 5.13 + v142独立构建；
- 不通信、持久化、反向编辑或Evidence写入；
- 不回退共享Compiler/Lab已存在或正式交付的0.8能力。

以方法确认、数值待实施证据确定：

- sidecar最终derived memory limit；
- 同步Encode交互性能门限。

正式实施基线：由总控指定包含 `3ae4df7` 与本契约的最终精确提交，不得使用未提交工作树。2026-09-10 已获首检查点独立工作区实施授权；尚未实施或验收。
