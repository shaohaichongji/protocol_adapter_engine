# 工程依据

本目录保存仍需用于实现、复核和能力边界判断的契约、设计与验证记录。它不是顺序教程：第一次阅读请先走 [01～06 开发者指南](../guides/README.md)。文件名中的 `draft`、日期或阶段号不单独决定效力；应结合正文状态、后续契约和实时源码判断。

## 1. 先按目的选择入口

2026-09-20：最新体验 Lab 已归集，见 [G2 交付记录](../archive/engineering-20260921/lab-g2-delivery-candidate-20260920.md)；SDK 身份保持不变，见 [仓库/SDK 审计](pae-repository-sdk-audit-20260920.md)。本轮清理范围见 [产物盘点](../archive/engineering-20260921/pae-artifact-cleanup-inventory-20260920.md)。旧 G1 预检和图形覆盖盘点中的“尚未接入”是历史状态，不覆盖已完成的 G1/G2。

| 目的 | 先读 |
| --- | --- |
| 理解 PAE/Lab 职责 | [PAE / Lab 职责与独立交付边界](pae-lab-delivery-boundary.md) |
| 了解当前本机交付 | [统一交付入口](../../deliverables/README.md)、[SDK 归集验证](local-deliverables-sdk-validation-20260919.md)、[Lab 归集验证](local-deliverables-lab-validation-20260919.md) |
| 跟踪总体推进 | [公开执行、独立交付与仓库整理推进计划](pae-execution-delivery-organization-plan.md)、[DEC-040 后续路线](post-dec040-roadmap.md) |
| 查公开 Compiler/Codec/Framer/Host | 下方第 3 节 |
| 查 Schema 某项能力 | 下方第 4 节 |
| 查 Qt Lab 接入与 UI | 下方第 5 节 |
| 查 SDK/standalone 证据 | 下方第 6 节 |
| 查文档和生成物整理 | 下方第 7 节 |

当前 clean-checkpoint 与本地归集不表示共享工作树 clean，也不是稳定 ABI、正式发布、Qt 许可闭合、Linux、新人工 UI、真实协议或现场证据。

## 2. 架构总边界与基础链

- [PAE / Lab 职责与独立交付边界](pae-lab-delivery-boundary.md)
- [Loader 与 Compiler 架构边界](loader_compiler_boundary_v0.1.md)
- [JSON Loader 诊断定位契约](json_loader_diagnostic_contract_v0.1.md)
- [Plan 计费内存实施切片](pae-dec-033a-accounted-plan-memory-implementation-slice.md)
- [完整记录执行检查点](post-stream-host-and-ui-v08-checkpoint.md)
- [Loader/SchemaIr](windows-msvc-2026-loader-schema-ir-slice.md)、[Frozen Plan](windows-msvc-2026-frozen-execution-plan-slice.md)、[Complete Record Codec](windows-msvc-2026-complete-record-codec-slice.md)、[Validated/Budgeted 链](windows-msvc-2026-validated-budgeted-capability-chain.md)、[Plan Memory](windows-msvc-2026-accounted-plan-memory-slice.md)
- [yyjson 随仓依赖验证](windows-msvc-2026-yyjson-vendor-dependency.md)

## 3. 公开 C++ API 与宿主接入

### Compiler、metadata 与 Codec

- [公开编译与 metadata 实施范围](pae-public-compile-metadata-slice.md)、[验证](pae-public-compile-metadata-validation.md)
- [消费准备 metadata 契约](pae-public-consumer-metadata-slice.md)、[验证](pae-public-consumer-metadata-validation.md)、[Lab 消费复核](../archive/engineering-20260921/lab-public-consumer-metadata-review.md)
- [完整记录 Codec 设计](pae-public-codec-slice-design.md)、[验证与移动视图修复](pae-public-codec-slice-validation.md)、[Lab 消费复核](lab-public-codec-stage1-consumer-validation.md)

### Framer 与 Host

- [公开 Framer/Host 分片设计](pae-public-framer-host-slice-design.md)、[Lab 需求复核](../archive/engineering-20260921/lab-public-framer-host-consumer-review.md)
- [StreamFramer 2A Windows 验证](pae-public-framer-stage2a-validation.md)、[Lab 消费复核](../archive/engineering-20260921/lab-public-framer-stage2a-consumer-review.md)
- [公开 Host 2B 契约](pae-public-host-stage2b-contract.md)、[Windows 验证](pae-public-host-stage2b-validation.md)、[Lab 消费复核](../archive/engineering-20260921/lab-public-host-stage2b-consumer-review.md)
- [宿主端点绑定契约](host-endpoint-binding-contract.md)、[实施验证](host-endpoint-binding-validation.md)、[raw 候选观察验证](host-raw-candidate-validation.md)
- [最小业务嵌入契约](business-embedding-minimal-contract.md)、[Windows 验证](windows-msvc-2026-business-embedding-minimal.md)

### ASCII 公开消费

- [ASCII 公开消费最小契约](pae-public-ascii-consumption-contract.md)
- [公开 ASCII facts 验证](pae-public-ascii-facts-validation.md)、[SDK 消费验证](pae-public-ascii-sdk-validation.md)
- [A1 非 Qt 适配](lab-public-ascii-offline-a1-validation.md)、[A2 UI/显式 Host](lab-public-ascii-ui-a2-validation.md)
- [0.11 Framing 预检](../archive/engineering-20260921/pae-public-stream-framing-preflight.md)、[实现验证](pae-public-stream-framing-validation.md)、[SDK 验证](pae-public-stream-sdk-validation.md)
- [Lab 非 Qt stream](lab-public-ascii-stream-validation.md)、[Qt 接线](lab-public-ascii-stream-ui-validation.md)

## 4. Schema 与协议能力切片

- [ASCII 完整记录契约](ascii-text-codec-minimal-contract.md)、[Windows 验证](windows-msvc-2026-ascii-text-slice.md)
- [ASCII 流式契约](ascii-stream-framing-contract.md)、[Windows 验证](windows-msvc-2026-ascii-stream-framing-slice.md)
- [Binary 有界流契约](bounded-stream-framing-contract.md)、[Windows 验证](windows-msvc-2026-bounded-stream-framing-slice.md)
- [有界变长记录契约](bounded-variable-record-contract.md)、[Windows 验证](windows-msvc-2026-bounded-variable-record-slice.md)
- [位字段契约](pae-dec-040-bitfield-contract-draft.md)、[Windows 验证](windows-msvc-2026-dec040-bitfield-slice.md)
- [SUM8 契约](pae-dec-041-sum8-contract.md)、[Windows 验证](windows-msvc-2026-dec041-sum8-slice.md)
- [数值转换契约](pae-dec-042-numeric-conversion-contract-draft.md)、[INT64 验证](windows-msvc-2026-dec042a-int64-slice.md)
- [DEC-042B 精确比例/偏置](pae-dec-042b-decimal-conversion-contract-draft.md)、[算术 Spike](windows-msvc-2026-dec042b-arithmetic-spike.md)、[Compiler](windows-msvc-2026-dec042b-compiler-slice.md)、[Core](windows-msvc-2026-dec042b-core-slice.md)
- [CRC 契约](crc-minimal-contract-draft.md)、[Windows 验证](windows-msvc-2026-crc-minimal-slice.md)
- [长度字段契约](length-field-minimal-contract.md)、[Windows 验证](windows-msvc-2026-length-field-slice.md)

具体 JSON 结构和版本增量以 [`schema/README.md`](../../schema/README.md) 与 `schema/pae.schema.json` 为准；这里的契约/验证用于解释执行语义和证据边界。

## 5. Protocol Lab、Qt 与 UI

### 离线 Lab、Evidence 与 UDP

- [Protocol Lab 总契约](protocol_lab_contract_v0.1.md)
- [离线加固决策](pae-dec-038-protocol-lab-offline-hardening.md)、[离线切片验证](windows-msvc-2026-protocol-lab-offline-slice.md)
- [UDP Exchange 决策](pae-dec-039-protocol-lab-windows-udp-exchange.md)、[自动验证](windows-msvc-2026-protocol-lab-udp-exchange-slice.md)、[人工 Loopback](windows-protocol-lab-manual-loopback-acceptance-20260906.md)
- DEC-042B Lab：[A](windows-msvc-2026-dec042b-lab-v06-format-stage-a.md)、[B](windows-msvc-2026-dec042b-lab-v06-evidence-stage-b.md)、[C1](windows-msvc-2026-dec042b-lab-v06-execution-stage-c1.md)、[C2 Run](windows-msvc-2026-dec042b-lab-v07-run-evidence-stage-c2-first.md)、[C2 Replay](windows-msvc-2026-dec042b-lab-v07-replay-compare-stage-c2-second.md)、[C3 CLI](windows-msvc-2026-dec042b-lab-c3-cli.md)
- [C3 代理离线验收](agent-dec042b-c3-offline-acceptance.md)；人工清单仍为 [`NOT_EVALUATED`](../guides/专项验收/DEC-042B-C3-人工离线验收.md)

### Qt 接入与展示

- [Qt 依赖核验](../archive/engineering-20260921/lab-qt-dependency-audit-plan.md)
- [ASCII 离线接入契约](lab-ascii-offline-integration-contract.md)、[UI Stage 2 验证](lab-ascii-ui-stage2-validation.md)
- [ASCII Stream Observer 契约](lab-ascii-stream-observer-contract.md)、[观察验证](lab-ascii-stream-observation-stage1-validation.md)、[完整验证](lab-ascii-stream-observer-validation.md)
- [Host Observer 契约](lab-host-endpoint-observer-contract.md)、[验证](lab-host-endpoint-observer-validation.md)
- [离线 Inspect](lab-ui-offline-inspect-contract-draft.md)、[验证](lab-ui-offline-inspect-validation.md)；[Encode Inspector](lab-ui-offline-encode-inspector-contract.md)、[验证](lab-ui-offline-encode-inspector-validation.md)
- [Schema 0.8 UI](lab-ui-bounded-v08-contract.md)、[验证](lab-ui-bounded-v08-validation.md)、[表示切换 UX](lab-representation-ux-validation.md)
- Binary UI 链：[总体契约](lab-binary-host-observer-contract.md)、[非 Qt 复核](lab-binary-nonqt-audit-validation.md)、[H1](lab-public-api-stage4-h1-validation.md)、[H2](lab-public-api-stage4-h2-validation.md)、[UI Stage 1](lab-binary-ui-stage1-validation.md)
- 公开展示所有权与兼容隔离：[owned DTO](lab-owned-presentation-types-validation.md)、[legacy complete](lab-public-legacy-complete-validation.md)、[ASCII compat](lab-ascii-compat-isolation-validation.md)

## 6. SDK、独立消费与本机交付

- [Windows x64 SDK 阶段 3 契约](pae-sdk-stage3-contract.md)、[验证](pae-sdk-stage3-windows-validation.md)、[内容盘点](../archive/engineering-20260921/pae-sdk-stage3-content-review.md)、[Lab 消费复核](../archive/engineering-20260921/lab-sdk-stage3-consumer-review.md)：`final6` 是历史交付身份。
- [clean-checkpoint SDK 预检](../archive/engineering-20260921/pae-sdk-clean-checkpoint-preflight.md)、[五包与包外消费验证](pae-sdk-clean-checkpoint-validation.md)：当前候选打包输入为 clean `98df5e0`。
- [Lab 同批 SDK 消费验证](lab-clean-sdk-consumption-validation.md)：static/shared Debug/Release 本地 standalone 闭包。
- [installed SDK 范围预检](../archive/engineering-20260921/lab-sdk-standalone-implementation-scope.md)、[static 验证](lab-sdk-standalone-validation.md)、[shared 验证](lab-sdk-standalone-shared-validation.md)：记录旧 dirty-provenance 路线，不替代当前 clean 候选。
- [SDK 本机归集](local-deliverables-sdk-validation-20260919.md)、[Lab 本机归集](local-deliverables-lab-validation-20260919.md)：统一入口位于 `deliverables/`。

面向使用者的当前命令见 [03 Windows SDK 集成](../guides/03-Windows-SDK集成.md)，不要直接从历史验证报告复制旧候选路径。

## 7. 仓库、文档与生成物整理

- [2026-09-21 历史文档迁移与构建根清理准备](organization-closeout-20260921.md)
- [2026-09-21 工程历史归档索引](../archive/engineering-20260921/README.md)
- [仓库结构审查](repository-organization-audit-20260919.md)
- [生成产物清理盘点](../archive/engineering-20260921/generated-artifact-cleanup-inventory-20260919.md)、[执行记录](generated-artifact-cleanup-execution-20260919.md)
- [文档迁移映射](document-migration-map.md)
- [仓库历史结构盘点](../archive/engineering-20260921/repository-organization-inventory.md)

2026-09-19 已按授权删除 99 个精确生成根，必要证据归集到被 Git 忽略的 `deliverables/evidence/cleanup-20260919`。这不改变任何历史验证报告的原始结论。

## 8. 使用这些记录时的规则

1. 契约说明“应该是什么”，验证记录说明“当时实际验证了什么”，两者不能互换。
2. 自动测试、包外消费、人工 UI、硬件和现场证据分别判断。
3. 历史报告中的路径可能已清理；先从当前指南和 `deliverables/README.md` 找入口。
4. 需要修改实现时，回到实时源码、CMake 和适用 `AGENTS.md`，不要只按报告行号工作。
