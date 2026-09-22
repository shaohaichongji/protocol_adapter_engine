# Source layout

本目录保存 PAE（Protocol Adapter Engine，协议适配引擎）的内部实现，以及面向
`include/pae/` 公开头的薄适配层。推荐先阅读 [`../include/README.md`](../include/README.md)
确认公开消费边界，再按所关心的能力进入本目录；仓库内部 target、头文件和命名空间不属于稳定公共 API。

## 目录职责

- `protocol_plan/`：保存冻结 `PlanBundle`、类型化元数据、热执行描述符及预算后的所有权模型。
- `config_compiler/`：把严格 JSON 经 Schema IR、校验和资源预算编译为完整 `PlanBundle`；
  yyjson 只作为该层的私有配置加载依赖。
- `protocol_core/`：按冻结 Plan 执行 `COMPLETE_RECORD` Decode/Encode；不读取 JSON，
  不负责通信、线程、设备生命周期或 UI。
- `protocol_framing/`：依据切帧契约从输入流形成候选记录；候选形成不等于 Codec 或业务成功。
- `host_endpoint/`：提供内部 Host 绑定和调用边界；不默认拥有 Socket、串口、重试或业务路由。
- `public_api/`：实现 `include/pae/` 声明的 experimental `0.x` C++17 facade，并由单一公开
  CMake target `PAE::pae` 承载。当前已实现配置编译、通用及消费准备 metadata、
  `COMPLETE_RECORD` Decode/Encode、有界 `STREAM_CHUNK` Framing，以及应用无关的同步 Host 组合层；
  内部 Plan、映射和 workspace 不暴露给消费者。

内部主要依赖方向为：

```text
config_compiler ──┐
protocol_core ────┼──→ protocol_plan
protocol_framing ─┘

host_endpoint ──→ protocol_core + protocol_framing

include/pae/* ←── public_api（公开适配层）──→ 已纳入公开契约的内部能力
```

这是职责和依赖边界，不要求所有调用经过同一条串行链路。公开消费者应只 include
`pae/*` 并链接 `PAE::pae`，不能直接依赖本目录的内部 target 或实现头。

## 阅读入口与当前边界

1. 公开 API 与生命周期约束：[`../include/README.md`](../include/README.md)。
2. 分阶段执行与交付边界：
   [`../docs/engineering/pae-execution-delivery-organization-plan.md`](../docs/engineering/pae-execution-delivery-organization-plan.md)。
3. 专项内部实现：进入上述对应子目录，并以其源码、CMake 和当前契约为准。

阶段 1/1B 实现已完成限定收口，并按
[`公开 Codec 验证报告`](../docs/engineering/pae-public-codec-slice-validation.md)和
[`消费准备 metadata 验证报告`](../docs/engineering/pae-public-consumer-metadata-validation.md)
完成限定 Windows x64 Debug/Release 验证。以下阶段记录是历史专项证据，不作为当前 SDK/Lab 交付总状态；最新身份和范围见
[`交付入口`](../deliverables/README.md)。阶段 2A 公开 `StreamFramer` 已实现，并有
[`2A Windows 验证报告`](../docs/engineering/pae-public-framer-stage2a-validation.md)记录专项与独立
consumer 结果；后置 Lab 只读复核及嵌套重入修正已完成限定总控收口。
阶段 2B 公开 Host 已按 selector 修正后的最终契约实现，并有
[`2B Windows 验证报告`](../docs/engineering/pae-public-host-stage2b-validation.md)记录最终专项、定向
回归和 external consumer 结果；后置 Lab 针对复核和总控限定收口已完成。该结论仍不代表 C ABI、
DLL/安装包、稳定 ABI、Lab 迁移或生产环境已经交付或验证。
`PAE_BUILD_TESTING` 控制测试依赖和测试专用 instrumentation；PAE 作为子项目嵌入时不会仅因宿主
启用通用 `BUILD_TESTING` 就自动把内部测试工具纳入公开消费边界。JSON Parser Spike 与生产切片隔离，
位于 `spikes/json_parser/`。
