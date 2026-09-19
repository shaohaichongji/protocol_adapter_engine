# PAE experimental public headers

2026-09-14 起，`include/pae/` 按已确认的首片契约承载 experimental `0.x` C++17 公开接口；
这不代表 API 已冻结或形成兼容承诺。

推荐按需阅读：`pae/version.h`（公开版本）→ `pae/compiler.h`（编译入口与 owner 生命周期）→
`pae/protocol_description.h`（通用及消费准备 metadata）→ `pae/codec.h`（完整记录执行）→
`pae/stream_framer.h`（流式候选形成）→ `pae/host_endpoint.h`（同步 Host 组合）→
`pae/export.h`（导出宏）。公开消费者只应 include `pae/*`，不要引用 `src/` 下的内部实现头或
内部 target。

- `pae/compiler.h` 编译配置 JSON，并由 move-only `CompiledProtocol` 拥有冻结 Plan 与 metadata；
- `pae/protocol_description.h` 提供与 Codec 共用的逻辑 `ValueKind`、Encode 值来源、
  Pipeline/Message 动作可用性及 `EXACT`/`UPPER_BOUND`/`NOT_AVAILABLE` 输出容量事实；
- `pae/codec.h` 提供 `COMPLETE_RECORD` Decode/Encode、执行内存报告和显式 typed Encode value；
- `pae/stream_framer.h` 提供 stream capability 查询、创建、`Push`/`Continue`、`Reset`、串行空闲
  `Observe` 和内存报告；一个实例永久绑定一个 Pipeline 和一条逻辑流；
- `pae/host_endpoint.h` 提供不可变 `endpoint_key + action + pipeline` 绑定、scope/channel/generation
  Handle、`Find`/`Observe`/`Reset`、完整记录 Decode、流式 Push/Continue、调用期 Message selector
  Encode、同步 observer/business callback 和分项资源报告；
- 只链接单一 CMake target `PAE::pae`；
- metadata 返回的 `std::string_view` 是借用 view，owner 销毁、move-assignment 或替换后失效；owner
  移动后重新取得 view；
- Decode record/field view 借用 Codec 实例，下一次成功取得执行 guard 的 Decode/Encode、Codec move
  或销毁会使旧 view 失效；BYTES 还借用输入 Frame；
- 空或移动后 owner、越界及非关联 metadata 查询失败关闭；执行期仍由 Matcher、Integrity、类型、
  容量等门禁决定，metadata 能力查询不承诺任意输入成功；
- Framer 候选只在同步 `noexcept` callback 内借用，候选形成不等于 Codec 成功；`bytes_consumed`
  之后的未消费后缀仍归调用者，`STOP` 不预读或重放；空 `Push` 与 `Continue` 等价，只推进一次，
  不刷新或丢弃半帧；
- Host binding 固定 endpoint、action 和 Pipeline，不固定 Encode Message；同一 Encode Handle 可在每次
  `Encode()` 调用中选择该 Pipeline 内不同可用 Message。Reset 成功后旧 Handle 变 stale，调用者必须
  重新 `Find()`；
- Host 的 candidate、Decode 字段和 Encode bytes 只在当前同步 callback 内借用。callback 异常被 Host
  捕获，已消费字节和已发生回调事实不会回滚，目标通道进入 `RESET_REQUIRED`；Encode 输出交付不等于
  Socket、串口或其他 Transport 已发送；
- 物理描述以 `protocol_description.h` 和 `compiler.h` 当前查询接口及支持域为准，不等于暴露整个私有 Plan；不提供 C ABI 或稳定跨版本/跨编译器 ABI 承诺。

当前 Stage 1 构建支持 Schema 0.1～0.11；公开头布局不受仓库内部
`PAE_ENABLE_SCHEMA_*` 功能宏影响。

阶段 1/1B 已完成授权范围实现。现有验证报告记录 Windows x64 Debug/Release 的 public API CTest
各 5/5、metadata 直测各 21/21、Codec facade 直测各 57/57，以及 metadata 驱动示例通过：

- [公开 Codec 设计](../docs/engineering/pae-public-codec-slice-design.md)与
  [验证报告](../docs/engineering/pae-public-codec-slice-validation.md)；
- [消费准备 metadata 契约](../docs/engineering/pae-public-consumer-metadata-slice.md)与
  [验证报告](../docs/engineering/pae-public-consumer-metadata-validation.md)。
- [公开 Framer/Host 分片设计](../docs/engineering/pae-public-framer-host-slice-design.md)与
  [StreamFramer 2A 验证报告](../docs/engineering/pae-public-framer-stage2a-validation.md)。
- [公开 Host 2B 契约](../docs/engineering/pae-public-host-stage2b-contract.md)与
  [最终验证报告](../docs/engineering/pae-public-host-stage2b-validation.md)。

2A 消费侧复核及嵌套重入修正已完成限定总控收口；修正后 Framer D/R 专项各 38/38、public
定向 CTest 各 6/6，首轮 37/37 与 7/7 不冒充最终重跑。2B selector 修正后的最终报告记录 Host
D/R 专项各 56/56、定向 CTest 各 10/10，新版 public-only consumer 两配置均验证同一 Handle 选择
两个 Message 并通过；修正前 52/52 仅是创建期固定 Message 模型的历史证据。

上述计数是各历史切片的限定 Windows 证据，不代表全量最新测试。后续 Windows x64 独立 SDK 五包与同批完整 Qt Lab 消费已另有验证，见[统一交付入口](../deliverables/README.md)。Linux、网络、真实协议、硬件、现场、稳定 ABI 与正式发布不由这些结果证明。
