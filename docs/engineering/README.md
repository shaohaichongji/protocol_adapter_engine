# 工程依据

本目录保存当前仍需用于实现、复核和能力边界判断的契约、设计决策与验证记录。文件名中的 `draft`、日期或阶段号不单独决定效力；应以正文状态、后续契约和实时源码共同判断。验证记录只证明其列明的配置、平台和测试范围。

## 当前总边界与路线

- [ASCII 公开消费最小契约](pae-public-ascii-consumption-contract.md)：2026-09-15 公开事实首片已获实施与定向验证授权；Lab 0.10 和 0.11 stream 后续串行，尚未迁移。进度见综合计划。
- [公开执行、独立交付与仓库整理推进计划](pae-execution-delivery-organization-plan.md)
- [Windows x64 SDK 阶段 3 契约](pae-sdk-stage3-contract.md)、[验证记录](pae-sdk-stage3-windows-validation.md)、[交付内容盘点](pae-sdk-stage3-content-review.md)、[消费复核](lab-sdk-stage3-consumer-review.md)：final6 本地交付已限定收口；使用见[SDK 快速入口](../guides/pae-sdk-windows-quickstart.md)，尚未正式发布或迁移 Lab。
- [公开 Host 2B 契约](pae-public-host-stage2b-contract.md)、[Windows 验证](pae-public-host-stage2b-validation.md)、[Lab 消费复核](lab-public-host-stage2b-consumer-review.md)：调用期 Encode selector 修正后已完成限定总控收口。
- [公开 Framer/Host 分片设计](pae-public-framer-host-slice-design.md)、[Lab 消费需求复核](lab-public-framer-host-consumer-review.md)；当前 2A 实施边界以综合推进计划为准。
- [公开 StreamFramer 2A Windows 验证](pae-public-framer-stage2a-validation.md)、[消费侧复核](lab-public-framer-stage2a-consumer-review.md)；报告指出的间接重入分类问题已修正并完成限定总控复核，详见综合计划最终状态。

- [PAE / Lab 职责与独立交付边界](pae-lab-delivery-boundary.md)
- [公开编译与通用元数据首片（实施范围与验收）](pae-public-compile-metadata-slice.md)
- [公开编译与通用元数据首片验证](pae-public-compile-metadata-validation.md)
- [消费准备 metadata 契约](pae-public-consumer-metadata-slice.md)、[Windows 验证](pae-public-consumer-metadata-validation.md)、[Lab 消费侧复核](lab-public-consumer-metadata-review.md)
- [公开完整记录 Codec 设计](pae-public-codec-slice-design.md)、[Windows 验证与移动视图修复](pae-public-codec-slice-validation.md)、[Lab 消费侧复核](lab-public-codec-stage1-consumer-validation.md)
- [DEC-040 后续推进路线与当前状态](post-dec040-roadmap.md)
- [流式宿主接入与 Lab Schema 0.8 UI 检查点](post-stream-host-and-ui-v08-checkpoint.md)

## Loader、Plan、Core、Framer 与 Host

- [Loader 与 Compiler 架构边界](loader_compiler_boundary_v0.1.md)
- [JSON Loader 诊断定位契约](json_loader_diagnostic_contract_v0.1.md)
- [Plan 计费内存实施切片](pae-dec-033a-accounted-plan-memory-implementation-slice.md)
- [有界流式切帧契约](bounded-stream-framing-contract.md)与 [Windows 验证](windows-msvc-2026-bounded-stream-framing-slice.md)
- [宿主端点绑定契约](host-endpoint-binding-contract.md)、[实施验证](host-endpoint-binding-validation.md)与 [raw 候选观察验证](host-raw-candidate-validation.md)
- [最小业务嵌入契约](business-embedding-minimal-contract.md)与 [Windows 验证](windows-msvc-2026-business-embedding-minimal.md)

## Schema 与协议能力切片

- [ASCII 完整记录契约](ascii-text-codec-minimal-contract.md)与 [Windows 验证](windows-msvc-2026-ascii-text-slice.md)
- [ASCII 流式契约](ascii-stream-framing-contract.md)与 [Windows 验证](windows-msvc-2026-ascii-stream-framing-slice.md)
- [位字段契约](pae-dec-040-bitfield-contract-draft.md)与 [Windows 验证](windows-msvc-2026-dec040-bitfield-slice.md)
- [SUM8 契约](pae-dec-041-sum8-contract.md)与 [Windows 验证](windows-msvc-2026-dec041-sum8-slice.md)
- [数值转换与 DEC-042A 契约](pae-dec-042-numeric-conversion-contract-draft.md)与 [INT64 Windows 验证](windows-msvc-2026-dec042a-int64-slice.md)
- [DEC-042B 精确比例与偏置契约](pae-dec-042b-decimal-conversion-contract-draft.md)、[算术 Spike](windows-msvc-2026-dec042b-arithmetic-spike.md)、[Compiler 验证](windows-msvc-2026-dec042b-compiler-slice.md)与 [Core 验证](windows-msvc-2026-dec042b-core-slice.md)
- [CRC 契约](crc-minimal-contract-draft.md)与 [Windows 验证](windows-msvc-2026-crc-minimal-slice.md)
- [长度字段契约](length-field-minimal-contract.md)与 [Windows 验证](windows-msvc-2026-length-field-slice.md)
- [有界变长记录契约](bounded-variable-record-contract.md)与 [Windows 验证](windows-msvc-2026-bounded-variable-record-slice.md)

## Protocol Lab、Evidence 与 UDP

- [Protocol Lab 总契约](protocol_lab_contract_v0.1.md)
- [离线加固决策](pae-dec-038-protocol-lab-offline-hardening.md)与 [离线切片验证](windows-msvc-2026-protocol-lab-offline-slice.md)
- [Windows UDP Exchange 决策](pae-dec-039-protocol-lab-windows-udp-exchange.md)、[自动验证](windows-msvc-2026-protocol-lab-udp-exchange-slice.md)与 [人工 Loopback](windows-protocol-lab-manual-loopback-acceptance-20260906.md)
- [DEC-042B Lab A 阶段](windows-msvc-2026-dec042b-lab-v06-format-stage-a.md)、[B 阶段](windows-msvc-2026-dec042b-lab-v06-evidence-stage-b.md)、[C1](windows-msvc-2026-dec042b-lab-v06-execution-stage-c1.md)、[C2 Run](windows-msvc-2026-dec042b-lab-v07-run-evidence-stage-c2-first.md)、[C2 Replay/Compare](windows-msvc-2026-dec042b-lab-v07-replay-compare-stage-c2-second.md)与 [C3 CLI](windows-msvc-2026-dec042b-lab-c3-cli.md)
- [C3 代理离线验收](agent-dec042b-c3-offline-acceptance.md)；人工步骤见[开发者指南](../guides/manual-dec042b-c3-offline-acceptance.md)

## Lab 接入、Qt 与 UI

- [Lab Qt 依赖核验](lab-qt-dependency-audit-plan.md)
- [ASCII 离线接入契约](lab-ascii-offline-integration-contract.md)与 [UI Stage 2 验证](lab-ascii-ui-stage2-validation.md)
- [ASCII Stream Observer 契约](lab-ascii-stream-observer-contract.md)、[Stage 1 观察验证](lab-ascii-stream-observation-stage1-validation.md)与 [完整验证](lab-ascii-stream-observer-validation.md)
- [Lab Host Observer 契约](lab-host-endpoint-observer-contract.md)与 [验证](lab-host-endpoint-observer-validation.md)
- [离线 Inspect 契约](lab-ui-offline-inspect-contract-draft.md)与 [验证](lab-ui-offline-inspect-validation.md)
- [离线 Encode Inspector 契约](lab-ui-offline-encode-inspector-contract.md)与 [验证](lab-ui-offline-encode-inspector-validation.md)
- [Schema 0.8 UI 契约](lab-ui-bounded-v08-contract.md)与 [验证](lab-ui-bounded-v08-validation.md)
- [表示切换 UX 验证](lab-representation-ux-validation.md)
- [Binary Host Observer 契约](lab-binary-host-observer-contract.md)、[描述基础件](lab-binary-description-validation.md)、[候选物化](lab-binary-materializer-validation.md)、[绑定前准入](lab-binary-prepared-validation.md)、[资源门禁](lab-binary-resource-validation.md)、[双流冻结](lab-binary-stream-validation.md)、[保存状态](lab-binary-saved-state-validation.md)、[Encode](lab-binary-encode-validation.md)、[非 Qt 总体复核](lab-binary-nonqt-audit-validation.md)与 [UI Stage 1 验证](lab-binary-ui-stage1-validation.md)

## 基础能力与依赖验证

- [Loader/SchemaIr](windows-msvc-2026-loader-schema-ir-slice.md)、[Complete Record Codec](windows-msvc-2026-complete-record-codec-slice.md)、[Frozen Plan](windows-msvc-2026-frozen-execution-plan-slice.md)、[Validated/Budgeted 链](windows-msvc-2026-validated-budgeted-capability-chain.md)、[Conformance Runner](windows-msvc-2026-protocol-conformance-runner.md)与 [Plan Memory](windows-msvc-2026-accounted-plan-memory-slice.md)
- [yyjson 随仓依赖验证](windows-msvc-2026-yyjson-vendor-dependency.md)
