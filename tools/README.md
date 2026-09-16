# Internal tools and Lab

本目录是开发、验证、诊断和 Lab 消费侧，不是 PAE Core 或公开 SDK 的安装/导出目录。PAE
本身不因这些工具而拥有 Socket、串口、设备线程、重试、业务路由或 Qt 依赖。

## 推荐阅读顺序

1. 先从 [`../include/README.md`](../include/README.md)确认 experimental 公开入口与当前能力边界。
2. 需要理解命令行、Evidence Bundle 或 UDP 观察流程时，再读
   [`protocol_lab/README.md`](protocol_lab/README.md)。
3. 需要理解 ASCII、Binary 或 Qt 展示适配时，进入相应子目录并核对其 CMake 和现行契约。

## 目录职责

- [`protocol_conformance_runner`](protocol_conformance_runner/README.md)：引擎开发侧的一致性验证工具，
  从外部配置和语料目录执行证据分级的 Decode/Encode 检查，仅在 `PAE_BUILD_TESTING=ON` 时构建。
- [`protocol_lab`](protocol_lab/README.md)：Lab 的公共命令行、证据和 UDP 观察能力，包含
  `inspect/encode/replay/compare`、有界单次收发和可校验 Evidence Bundle；其
  [精确内部契约](../docs/engineering/protocol_lab_contract_v0.1.md)不改变 PAE 的 Transport 无关边界。
- `protocol_lab_ascii/`：ASCII Lab 的应用适配与展示模型，属于 Lab 消费侧，不是 PAE 公开 API。
- `protocol_lab_binary/`：Binary Lab 的应用适配、视图与证据组织，属于 Lab 消费侧，不是第二套
  Codec，也不构成公开 SDK。
- `protocol_lab_ui/`：Qt Lab 应用及交互展示层；Qt 依赖必须停留在 Lab 侧，不得进入 PAE Core
  或 `PAE::pae` 的依赖链。

## 仍在服役的旧桥

`protocol_lab/v06_execution.*` 仍被现有 CLI、Qt UI 和相关测试消费，是当前真实依赖，不是可以按
目录年代直接删除的废弃文件。公开 Codec 及消费准备 metadata 已实现并完成限定 Windows 验证，
1B Lab 消费侧只读复核已完成，但并未实际迁移。阶段 2A 公开 `StreamFramer` 也已实现并具备限定
Windows 证据并完成限定收口。阶段 2B 公开 Host 已实现并完成 selector 修正后的限定 Windows 验证，
Lab 消费侧针对复核及总控限定收口已完成；必须在实际迁移、回归和验收完成后才能处理旧桥替换。
现阶段不能把公开 Host 可用解释为 Lab 已完成迁移，也不能把 Host callback 输出解释为网络发送。
