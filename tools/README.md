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

## 公开消费与兼容路径

当前推荐 Qt Lab 已有 installed-SDK public-only 消费路径，覆盖 Binary 完整记录、流式观察及
ASCII/legacy 公开路径；具体构建身份和验证边界见 [`交付入口`](../deliverables/README.md)。
这不表示仓库内所有历史 CLI、兼容构建和测试均已退役。

`protocol_lab/v06_execution.*` 等旧桥仍须按当前 CMake、调用者及测试引用判断，不能因文件名
包含旧版本号就删除。Qt UI 的当前公开路径与可选兼容路径也不能混为同一构建。
Host callback 输出不是网络发送，PAE 仍不负责 Transport。
