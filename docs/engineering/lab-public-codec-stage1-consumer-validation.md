# Lab 对公开 Codec 阶段 1 的消费者复核

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

状态：2026-09-14 阶段 1 C 只读复核结论。本文从 Lab 消费者视角核对当前公开完整记录
Codec 首片及既有验证证据；本轮未修改实现、未运行构建或测试，也不表示 Lab 已迁移到公开接口。

## 1. 结论

**结论：未发现阶段 1 首片范围内需要阻断后续推进的确定实现缺陷，可以进入下一项受控工作；
但当前公开 metadata 尚不足以支撑 Lab 完全配置驱动地构造 Decode/Encode 界面和缓冲，因此不能把
本结论解释为“Lab 现已具备动态迁移条件”。**

当前首片已形成一个不泄漏私有 Plan 的可执行闭环：消费者可用公开索引选择 Pipeline、Message、
Field 和已知枚举项；Decode 可得到 typed 借用视图、枚举 raw/known 身份、精确 Decimal 及其实际
raw integer；Encode 使用调用者缓冲并在失败时保持零交付；状态、失败 Field/Input 和 conversion
error 也已公开。`CompiledProtocol` 与 Codec 通过内部共享不可变状态解耦所有者生命周期，Codec
自己的短期视图则有明确的下一次调用、move 和销毁失效语义。

本次没有把下列已明确延期的能力误判为首片缺陷：物理 byte/bit range、Encode 同一次调用的输出
字段观察结果，以及 Framer/Host 公开接口。后续也不得用重复 Decode 或由 logical 反算 raw 的方式
绕开这些缺口。

## 2. 范围、基线与证据等级

本轮现场基线：

- 仓库：`<REPO_ROOT>`
- 分支：`main`
- HEAD：`dfb08f351cdb22a9b50c9e64e688e3b666e669bc`
- 工作树：已有大量已跟踪修改、删除和未跟踪文件；本轮全部保留
- staged：空

本轮只读取：

- `include/pae/codec.h`
- `include/pae/compiler.h`
- `include/pae/protocol_description.h`
- `src/public_api/codec.cpp`
- `src/public_api/compiler.cpp`
- `src/public_api/compiled_state_internal.h`
- `tests/public_api/public_codec_tests.cpp`
- 与 ASCII 零字段、unknown enum/tainted 相关的既有 Core/Lab 测试源码
- 阶段 1 设计、PAE 验证报告及阶段 0 Lab 消费者需求报告
- `out/public-codec-stage1/` 下既有日志

这里的判断分为三个等级：

1. **源码确认**：当前公开声明和实现可直接核对；
2. **既有日志证据**：由阶段 1 A 产生，本轮只读复核，不是本轮重新执行；
3. **尚未直接证明**：源码有映射或底层已有测试，但公开 facade 的特定传播路径缺少针对性断言。

## 3. 消费侧逐项核对

| 消费需求 | 结论 | 依据与边界 |
| --- | --- | --- |
| 身份与索引映射 | 首片满足 | `CompiledProtocol` 公开 Protocol/Pipeline/Message/Field/Enum 元数据及扁平索引关系；`FieldSelector`、`EnumSelector` 由 facade 解析为内部引用，消费者无需提交 Plan 指针。索引只对对应编译产物有效，重新编译后应按稳定 ID 重新解析。 |
| typed Decode | 首片满足 | `DecodedFieldView` 覆盖 `UINT64/INT64/BOOL/BYTES/ENUM/DECIMAL64`；公开测试直接覆盖 UInt64、BYTES 与已知 ENUM，设计和实现覆盖其余分支。 |
| 精确 Decimal 与 raw | 首片满足 | Decimal 以 coefficient/scale 公开；转换字段另外公开实际 raw kind 与 signed/unsigned raw，不要求 Lab 反算。 |
| enum raw/known/tainted | 接口满足，直接证据有缺口 | 公开视图同时提供 enum raw 和可选 known enum flat index，DecodeResult 提供 `output_tainted`。底层 Core 已测试 preserved unknown enum；当前 `tests/public_api/public_codec_tests.cpp` 未直接断言 unknown enum 经 facade 后的 raw、空 known 身份及 tainted 传播。 |
| Decode 借用视图 | 首片满足 | record/field view 绑定 codec、view epoch 与 generation；下一次成功取得 facade guard 的 Decode/Encode、codec move 或销毁后失效，BYTES 还借用 Decode 输入。当前实现已加入 move epoch，避免旧 view 在新 owner 上复活。 |
| Compiled owner 生命周期 | 首片满足 | Codec 持有内部 `CompiledStateRef`；创建后原 `CompiledProtocol` 可销毁或置空而不影响 Codec。metadata 的 `string_view` 仍借用 `CompiledProtocol`，owner move/替换/销毁后必须重新查询，二者生命周期语义没有混用。 |
| Encode 调用者缓冲 | 首片满足 | 输出由调用者持有；成功返回 `bytes_written`，缓冲不足返回 `required_size`；任意失败均以 `bytes_written == 0` 表示无可交付 Frame，缓冲中被改写内容也不是结果。 |
| Encode 输入绑定 | 首片满足 | facade 预分配映射并校验 Field/Enum selector，消费者不构造 `FieldRef`/`EnumValueRef`。已知枚举使用受控 selector；未承诺任意 raw enum 输入。 |
| 状态和失败位置 | 首片满足，部分分支待补直测 | Core 状态被逐项映射；Decode 给出失败 Field 与 conversion error，Encode 给出失败 input ordinal、Field 与 conversion error。公开测试直接覆盖 buffer/slot、type、enum selector、unknown message、busy 等主要路径；并非每个状态都有 facade 专项断言。 |
| direction 与动作边界 | 执行语义满足；发现 metadata 缺口 | Pipeline/Message metadata 暴露 `direction_id`，Core 仍决定相应 Encode/Decode 是否支持并返回 `OPERATION_NOT_SUPPORTED`。公开 metadata 尚未直接给出动作可用性，Lab 不能仅凭 Schema 或 direction 文本猜测。 |
| ASCII 零字段 | 实现可表达，直接证据有缺口 | API 的零 `value_count`、零 `field_count` 和空 record 形状没有占位字段要求；Core 与旧 ASCII adapter 已覆盖 literal-only 成功及缺失动作。当前 public facade 测试和示例没有 literal-only/zero-field 专项断言。 |
| 并发与执行期分配 | 首片满足其已声明边界 | 每个 Codec 有独立 workspace；同一实例并发返回 `WORKSPACE_BUSY`，成功取得 guard 前不污染 view；映射和 slot 在创建期分配，既有公开测试记录首次及重复调用无新增分配。借用视图并发读取仍由契约禁止。 |

## 4. 未发现的首片阻断与证据缺口

### 4.1 本轮未确认实现阻断

按阶段 1 设计中已经冻结的边界核对，未发现以下类型的确定缺陷：

- 消费者仍需包含私有 Plan/Core 头或提交私有指针；
- Decode typed 值、Decimal 或转换 raw 在 facade 中丢失；
- Encode 失败仍对外宣称有可交付字节；
- Codec 依赖原 `CompiledProtocol` owner 持续存活；
- codec move 后旧借用视图错误复活；
- 未映射 Core 状态被静默折叠成成功；
- ASCII 零字段被公开类型强制表示为虚假占位 Field。

因此，本次没有要求先制作 reproducer，也没有触发“先报告再运行”的停止条件。

### 4.2 建议补充但不阻断首片的 facade 专项回归

以下是公开 facade 的**直接证据缺口**，不是本轮已确认实现错误，也不应以此否定首片：

1. preserved unknown enum：断言 raw 保留、`KnownEnumFlatIndex()` 为空、`output_tainted=true`；
2. ASCII literal-only：断言零输入 Encode、零字段 Decode，以及缺失方向动作返回
   `OPERATION_NOT_SUPPORTED`；
3. 选择若干边缘状态补充失败位置传播断言，例如 `MESSAGE_NOT_ALLOWED`、
   `failed_value_index` 与 conversion failure 的公开结果。

底层 Core 已存在 unknown enum/tainted、ASCII literal-only 和 unsupported action 测试，因此这些项的
优先目标是证明 facade 映射没有丢失语义，而不是重新证明整个 Codec 内核。

## 5. Lab 动态迁移前的最小后续缺口

以下按消费者落地依赖排序；第 2、3、4 项仍属于已明确延期范围。

### P1：补足消费端所需的公开 metadata

当前 `FieldDescription` 只有身份、文本和 enum 区间，未公开至少以下用于配置驱动 UI/调用准备的
事实：

- Field 的 `ValueKind`；
- Field 是否为 Encode caller input，或由 literal/constant/computed/integrity 等路径生成；
- Pipeline/Message 的 Decode/Encode 动作可用性；
- 若要避免先失败再按 `required_size` 重试 Encode，消费端预配 output buffer 所需的公开容量上界。

这不阻断“已知配置、硬编码选择器的外部消费者”执行首片，但会迫使 Lab 继续读取私有 Plan、硬编码
Schema/方向知识或猜测缓冲大小，因而是 Lab 动态迁移前最优先的接口缺口。后续设计应公开已冻结的
消费事实，不应把配置解释逻辑搬进 UI。

### P2：物理范围与可变记录实际范围

Lab 的 Hex 高亮和 raw evidence 仍需要字段的实际 byte/bit range；可变记录还需要本次执行后的实际
范围。该能力已明确延期，首片当前只保证值与身份，不应将 BYTES 指针差推广为所有字段的范围算法。

### P3：Encode 同调用观察字段

Lab 若要展示 constant/computed/integrity 字段和 Decimal 的实际 raw，需要 Encode 本次调用直接产生
的字段观察结果。不得在 Encode 后重复 Decode 来制造成功，也不得从 logical 值反算 raw。

### P4：Framer/Host 公开边界

流切帧、Host binding/observation、Socket、线程、重试和证据事务均不属于当前完整记录 Codec 首片；
后续应按独立阶段设计，不能在 Lab 迁移时重新泄漏内部类型。

## 6. 既有验证记录复核

本轮只读看到下列阶段 1 A 日志结果：

- `move-view-final-debug.log`：`PUBLIC_CODEC_TEST_SUMMARY passed=47 failed=0 gate=PASS`
- `move-view-final-release.log`：`PUBLIC_CODEC_TEST_SUMMARY passed=47 failed=0 gate=PASS`
- Debug public API CTest：4/4 passed
- Release public API CTest：4/4 passed
- Debug full CTest：32/32 passed
- Release full CTest：32/32 passed

这些日志支持当前源码的既有验证状态，但不是本轮重新执行；本轮也未进行人工 UI、真实网络、
Framer/Host、硬件、现场或 Lab 迁移验收。

## 7. 本轮交付边界

- 实际新增文件：`docs/engineering/lab-public-codec-stage1-consumer-validation.md`
- 未修改：公开 API、Core、Lab、CMake、README/index、AGENTS.md 及其他既有工作树内容
- 未运行：配置、构建、测试、示例、UI、网络或格式化工具
- Git：未 Stage、未 Commit、未 Push
- 停点：完成本报告后停止写入，等待总控复核；未开始 Lab 迁移
