# Lab 0.5–0.8 complete-record 公开消费迁移验证

## 1. 实施前等价清单

本片基线为 `main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31` 及已复核、尚未提交的
Lab-owned DTO 首片。下表是实施前锁定的行为等价条件；后续验证结果在本报告追加，
本节不因实现方式改写。

| 行为 | 旧路径 | 公开替代 | 必须保持的观测点 |
| --- | --- | --- | --- |
| 单次编译与 owner | private `CompileJsonToPlanWithUiDescription` + `ExecutionBridge` | `CompileProtocolJson` + 同一 `CompiledProtocol` 创建描述和 Codec | 0.5–0.8 一请求只调用一次公开编译器；失败不 private fallback |
| 展示描述 | private Plan + UI sidecar 映射 | public protocol/pipeline/message/field/enum metadata + physical query 映射到 Lab-owned DTO | Schema、id/display/description/source、方向、字段类型/source、enum、物理范围、变长/长度/完整性位置不猜值 |
| Pipeline 限定 Decode | `ExecutionBridge::InspectPipeline` | `CompleteRecordCodec::Decode(pipeline_index, frame)` | 未知/歧义/Message 不允许的状态与失败清旧；成功 Message identity 必须属于当前 Pipeline |
| Decode 结果 | private workspace 物化 | 在下一次 Codec 调用使借用 view 失效前有界复制 | UINT64/INT64/BYTES/ENUM/BOOL/Decimal64，raw/logical，conversion raw kind/value，实际字节/位范围 |
| typed Encode | private `ParsedValues` 到 Core values | Lab `TypedDraft` 到 public `EncodeValue` | 只传 caller-input；constant/computed 不伪造输入；类型、enum identity、BYTES 长度、Decimal64 错误保持 |
| Encode 输出长度 | private message frame/bounds | public `PipelineMessageExecution::encode_output_size_kind/encode_output_size` | 0.5–0.7 定长、0.8 变长都用公开上界有界分配；不扩大成无界 buffer |
| Encode review | Encode 成功后独立 private Decode workspace review | public Encode 内部 final review 保留；Lab 再用第二个 public Codec 做既有独立 Decode review | 不将 Codec 内部 `FINAL_REVIEW_FAILED` 与 Lab 独立 review 混同；主 Encode 一次、Lab review Decode 一次，不为展示再 Decode |
| 长度/CRC/转换 | private Core 结果与诊断 | public Codec status/conversion error/physical query | v0.6 integrity、v0.7 computed length、v0.8 bounded payload 及 Decimal64 的成功/失败语义不变 |
| 资源与所有权 | Plan/bridge/workspace 实例 | compiled owner + main/review Codec + owned description/result | 描述、借用结果复制、output/result 容量在调用前预检；instance/replacement 按逻辑预算准入，不声称 RSS 上限 |
| 事务与身份 | `DocumentSession` load/plan/selection/input/request revision | 保留现有 session publication 检查 | 迟到/cancel/close/reload completion 不发布；失败清除旧 preview/inspect result；不改 H2/A2/0.11 |

## 2. 实施前公开能力核对

- `pae::CompiledProtocol` 公开 protocol/pipeline/message/field/enum metadata、Pipeline-Message
  execution 查询和 Binary physical/resolve query。
- `pae::CreateCompleteRecordCodec` 可由同一 compiled owner 创建独立 main/review Codec，
  并提供工作区逻辑计费。
- `pae::CompleteRecordCodec::{Decode,Encode}` 公开 matched identity、failed field/value、
  conversion error、borrowed decoded view 及 Encode output size/result。
- `pae::EncodeValue` 公开 UINT64/INT64/BOOL/BYTES/ENUM/DECIMAL64 typed input。
- 当前 UI 对 authored conversion 只需 Decimal64 展示/编辑事实，不显示公式参数；
  public `ValueKind::DECIMAL64` 已满足该行为。

初步结论：未发现必须修改 PAE public API 的缺口，可进入 Lab 侧失败断言与实施。

## 3. 实施结果

- 新增默认 `OFF` 的 `PAE_BUILD_PROTOCOL_LAB_PUBLIC_LEGACY_COMPLETE` gate。gate 关闭时
  保留原 private legacy 路径；gate 开启时，Schema 0.5–0.8 分类为 `LEGACY_PUBLIC`。
- `CompileWorker` 对 `LEGACY_PUBLIC` 仅调用一次 `CompileProtocolJson`，completion 只持有
  `CompiledProtocol`，不产生 private artifacts，也不做 private fallback。
- 新 `PublicLegacyCompleteAdapter` 从同一 compiled owner 复制 Lab-owned description，
  并创建 main/review 两个 public `CompleteRecordCodec`。Encode 只映射 caller-input typed
  draft，主 Encode 成功后使用独立 Codec 做一次 Decode review；Decode 传入当前
  Pipeline index。借用 view 在 Codec 再用前复制到 owned result。
- description/result 做溢出检查和逻辑字节预检；instance 计入 compiled memory report、
  main/review Codec memory report 和 owned description，replacement 同时计入旧实例。
  这是本实现的逻辑预算，不是 RSS 硬上限。
- `DocumentSession` 保留原 load/plan/selection/input/request key 发布检查、失败清旧
  preview/inspect result 与现有 UI 诊断。变长 Binary 物理列仍由 owned description +
  实际 frame size 推导，不改变原 UI 的零长度显示。

## 4. 失败先行与定向覆盖

- 实施前测试首先引用尚不存在的 public legacy adapter，Debug build 按预期以
  C1083 失败：`out/lab-public-legacy-complete/pre-fix-debug.log`。
- `public_legacy_complete_tests.cpp` 独立覆盖：单 public compiler attempt/owner，owned
  description，typed Encode，main Encode + 独立 review Decode，v0.7 computed length，
  后匹配长度失败的 Message/Field identity，Pipeline 限制，instance exact/minus-one 及
  replacement 上界。
- 既有 `document_state` / `dual_tab_isolation` / `mailbox_ownership` / `inspect_state` /
  `bounded_v08_state` 在 gate 开启时改为 public fixture compile，因而覆盖加载、重载、
  close，typed Encode，Inspect 失败清旧，v0.5 多 Message，v0.6 CRC，v0.7 length，
  v0.8 变长 payload/dynamic integrity 与交易身份。
- H2/A2/0.11 回归分别由 `binary_public_h2` / `ascii_public_a2` /
  `ascii_stream_session` 覆盖。

## 5. 最终验证证据

构建目录：`out/build/windows-msvc-lab-public-legacy-complete/`。最终 Debug 和 Release
均使用 MSVC v142 `14.29.30133`，测试 target 显式 `/UNDEBUG`，Release 构建日志
显示 `/DNDEBUG` 被 `/UNDEBUG` 覆盖。

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| Debug 定向 10 个 headless + 3 个 Qt smoke | 13/13 PASS | `out/lab-public-legacy-complete/targeted-debug-final.log` |
| Release 同范围，含 v0.5/v0.6、v0.7、v0.8 Qt smoke | 13/13 PASS | `out/lab-public-legacy-complete/targeted-release-final.log` |
| gate 默认 OFF 配置 | cache 确认 `...PUBLIC_LEGACY_COMPLETE:BOOL=OFF` | `out/lab-public-legacy-complete/configure-default-off.log` |
| `BUILD_TESTING=OFF` + gate ON | Release UI target PASS | `out/lab-public-legacy-complete/configure-testing-off.log` 与 `build-testing-off-release.log` |
| whitespace 检查 | `git diff --check` PASS | 本次最终复核 |

最终主要源码/Debug/Release 证据 SHA-256：

- `public_legacy_complete_adapter.cpp`: `D06CE8904D5B6B671EE6CD738C926E5C3A14E4DB4E41985320075F55E4907022`
- `public_legacy_complete_adapter.h`: `3F7B5AF6E2C0EDBCCAA2A1000348D8A634F15341D2D4DBF61344DAC17F2C6909`
- `document_session.cpp`: `499A062802D0144115E9BAC1CE926152D2A8FF41411D0F5B5B70C019199387DD`
- `compile_worker.cpp`: `D5E782B81A0ED49E3A790BB5F8D1E66B5CF9D0B000A46745D7251C9D6D253918`
- `public_legacy_complete_tests.cpp`: `1672DA016909D98B8D538C4372A899A6248B6FF277FB599B5940B1748070A6B2`
- `targeted-debug-final.log`: `0E212CD1F4113BDDCCB58F28DF36276EAFA6C2B6457E0048DB620B47B5F03577`
- `targeted-release-final.log`: `623184B5EF514B37F9A3EDE1F1321760B70609419A555486731432663F3DF233`

## 6. 首轮边界与状态

- 已完成本次派发范围，待总控复核。
- 未修改 PAE public/private 实现、CLI/旧 execution bridge、Host/stream 契约或 SDK 五包；
  未覆盖旧部署。
- 本轮为本机自动构建/测试/Qt smoke；未做人工 UI 验收、Linux、Golden、硬件或现场验证。
- Stage/Commit/Push/发布/删除均未执行。本任务已停止写入，等待总控复核。

## 7. 总控定向复核返修：预算一致性与 noexcept 诊断

总控源码复核后，本片未按首轮结果收口，并在原 adapter/tests/report 范围内补齐以下问题：

- **描述与实例准入前移**：新增只读 public metadata/physical query 预检，在复制 owned
  description 前完成溢出与 `max_description_bytes` 判断；实例预算先计入 compiled memory
  report、固定 adapter、owned description，再用 `CreateCompleteRecordCodec` 的公开 options
  将 `execution_memory_limit_bytes` 设为 `0` 进行无 workspace 成功分配的计费探测，取得公开
  `codec_accounted_total_bytes` 后先完成 main/review 两份 Codec 的 instance/replacement 准入，
  最后才按该精确逻辑额度创建两个 Codec。这里仍是逻辑计费，不升级为 RSS 硬上限。
- **结果统一准入**：成功/失败结果均在复制 frame、Message/Field id、字段 vector 和字段文本前
  完成溢出安全的总量判断。BYTES 同时计入 raw/logical 两份十六进制文本，即 payload 部分为
  `4 * N` 个字符并分别含终止字节；UINT64/INT64/ENUM/BOOL/DECIMAL64 按实际文本长度计入。
  失败结果统一计入 frame、Message id 与 failed Field id，拒绝时不保留 owned frame/fields。
- **noexcept 安全错误**：`Preparation` 新增不分配的 `PublicLegacyPreparationError`；
  `AdoptCompiled noexcept` 的兜底异常路径不再复制 `exception.what()`，`bad_alloc` 返回稳定的
  `ALLOCATION_FAILED` 错误码并保持 detail 为空。
- 测试新增：owned description copy 前预算拒绝、4 KiB BYTES 成功结果 minus-one 拒绝、
  LENGTH_MISMATCH 失败结果 minus-one 拒绝，以及 preparation `bad_alloc` 注入；这些断言不依赖
  `DescriptionAccountedBytes()` 自证实现。

修复前行为失败已保留：`budget-fix-pre-test-debug.log` 中
`budget preflight did not reject before owned description allocation`，进程非零退出。最终验证：

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| Debug adapter 专项 | 1/1 PASS | `out/lab-public-legacy-complete/budget-fix-debug-test.log` |
| Release adapter 专项 | 1/1 PASS | `out/lab-public-legacy-complete/budget-fix-release-test.log` |
| Debug 受影响回归 | 13/13 PASS | `out/lab-public-legacy-complete/budget-fix-regression-debug.log` |
| Release 受影响回归 | 13/13 PASS | `out/lab-public-legacy-complete/budget-fix-regression-release.log` |
| whitespace 检查 | `git diff --check` PASS | 本次返修最终复核 |

返修最终源码与证据 SHA-256：

- `public_legacy_complete_adapter.cpp`: `8D2ACE6AD3C4E6282D4B6DAA9C52D445CC8FBD01792999B9D85797DAC99C6D69`
- `public_legacy_complete_adapter.h`: `7097A339E2C3CDA738E0F7B15DB61C4D05F118ECA375574EE97606CEA1E16085`
- `public_legacy_complete_tests.cpp`: `0ECA225EB5CDAB6DCC2914D87C7AA779CAB402DBEA0AAD7E4DAD5ACA5DC0A4E0`
- `budget-fix-pre-test-debug.log`: `75601A3071E19117840DF81186180162DF0273CA4CC0BA4C9122036EEE33ED9C`
- `budget-fix-debug-test.log`: `CD681639F3C514CCBC9074A77AED9732C76E7448AD4681FF78AF4B1ABE26D89D`
- `budget-fix-release-test.log`: `7789D40892D0B6ED3C7529219CD923EFA3EC1ADDF50E958E25201E21712D48AA`
- `budget-fix-regression-debug.log`: `F1865E7B6A394CC071364B936FD8F9FC64E1320BE834B70B3EBE6E0312FF861A`
- `budget-fix-regression-release.log`: `49EC3F2A9041EFD5803E7AFD7628969E7CED419BEC315AF03FCFA1C498AB3CE8`

返修未修改 route/session transaction、PAE public/private API、Host/stream、SDK 五包或部署；
未跑全仓、未做人工 UI/Linux/Golden/硬件/现场验证。Stage/Commit/Push/发布/删除均未执行。
已完成返修派发范围，停止写入，待总控复核。

## 8. 测试故障注入与产品构建隔离

总控收口复核发现第 7 节加入的 fault-injection API 与状态尚未受测试构建门禁保护。
本次仅补充编译隔离，不改变 adapter 正常执行语义：

- `tools/protocol_lab_ui/CMakeLists.txt` 仅在 `PAE_BUILD_TESTING=ON` 时，为
  `pae_protocol_lab_ui_headless_internal` 及其消费者公开定义
  `PAE_PROTOCOL_LAB_UI_TEST_INSTRUMENTATION=1`。使用 PUBLIC 定义是为了保证静态库与包含
  adapter header 的消费者具有相同类布局；该定义不使用 `NDEBUG`。
- adapter header/cpp 中三个 `FailNext*ForTesting` 声明/定义、两个全局 atomic、实例 fault
  bool 及全部触发/清理分支均置于该宏下。专项测试增加编译期检查，缺少门禁宏时直接失败。
- `BUILD_TESTING=OFF`、`PAE_BUILD_TESTING=OFF`、public legacy gate ON 的既有 Release UI
  配置重新构建通过。检查生成的 headless `.vcxproj`，测试宏计数为 0；使用 `dumpbin /symbols`
  检查产品静态库，三个注入 API、两个 atomic 与实例 fault 标识的匹配符号计数为 0。

| 验证 | 结果 | 证据 |
| --- | --- | --- |
| Debug adapter 专项 | 1/1 PASS | `out/lab-public-legacy-complete/budget-fix-gate-debug-test.log` |
| Release adapter 专项 | 1/1 PASS，`/UNDEBUG` 生效 | `out/lab-public-legacy-complete/budget-fix-gate-release-test.log` 与对应 build log |
| Testing-off Release UI 重构建 | PASS | `out/lab-public-legacy-complete/budget-fix-gate-testing-off-release-build.log` |
| Testing-off 宏/符号检查 | 宏 0、注入符号 0 | `out/lab-public-legacy-complete/budget-fix-gate-testing-off-inspection.log` |

门禁补齐后的最终源码与新增证据 SHA-256：

- `tools/protocol_lab_ui/CMakeLists.txt`: `BB529CD46DB4C3DFD7840F69758B7239C1841717893100532973C52420F4EE52`
- `public_legacy_complete_adapter.cpp`: `C00907763B486D5CA59197EDB518271EE3318773D72DD3176A78DBDC07AE3776`
- `public_legacy_complete_adapter.h`: `D732718675CF35E2F9800B4EEEE0226703AE0DF16E7A8CF20AE6F89E94CAE851`
- `public_legacy_complete_tests.cpp`: `107E43A6B8D73B26FAA89CE966EA964B732F9A781ECAF54F045526924466FE74`
- `budget-fix-gate-debug-test.log`: `31923077010F288A8AA79D99FFD952878F24FDEADFC060F7F6BAC785EF3560C6`
- `budget-fix-gate-release-test.log`: `1C239DC6E40EBDB9C894CD22B2D349976EA951AAA300A2A130E768D8DA516B92`
- `budget-fix-gate-testing-off-release-build.log`: `9A3C1F78F8036AC39EC4ADE4BE8D486C1F18E53C24547BBC697EE02C489AD33B`
- `budget-fix-gate-testing-off-inspection.log`: `2421101731C2E5C4F5983BDEFA7F911D8573A48E8EEE68F41931A2BDC61863F2`

按总控要求未重跑 13 项回归、SDK 或人工验证；未执行 Stage/Commit/Push/发布/删除。
已完成本次隔离补齐，停止写入，待总控复核。
