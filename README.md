# ProtocolAdapterEngine

ProtocolAdapterEngine（PAE）是面向工业私有协议的配置驱动 C++ 协议能力库。它负责严格配置编译、冻结 Plan、完整记录 Decode/Encode、有限协议范围内的 Framing、Integrity 和 Host 接入；不拥有 Socket、串口、设备线程、重试、业务路由、设备生命周期或 UI。

Qt Lab 是 PAE 的独立离线观察与交互工具，不进入 PAE Core 依赖链，也不替代真实宿主、硬件或现场验收。

## 从这里开始

| 目标 | 入口 |
| --- | --- |
| 了解能力和边界 | [01 项目定位与能力边界](docs/guides/01-项目定位与能力边界.md) |
| 启动本机 Lab | [02 首次运行与 Lab 体验](docs/guides/02-首次运行与Lab体验.md) |
| 在 Windows C++ 工程中接入 | [03 Windows SDK 集成](docs/guides/03-Windows-SDK集成.md) |
| 编写第一份配置 | [04 协议配置入门](docs/guides/04-协议配置入门.md) 与 [Schema 索引](schema/README.md) |
| 阅读源码或参与维护 | [05 架构与代码阅读](docs/guides/05-架构与代码阅读.md) |
| 构建、测试和排错 | [06 构建测试与问题定位](docs/guides/06-构建测试与问题定位.md) |
| 查契约和验证记录 | [文档入口](docs/README.md) |

## 构建开发树

在仓库根目录使用当前完整 PAE + Qt Lab preset：

```powershell
cmake --preset windows-msvc-pae-lab
cmake --build --preset windows-msvc-pae-lab-debug
ctest --preset windows-msvc-pae-lab-debug

cmake --build --preset windows-msvc-pae-lab-release
ctest --preset windows-msvc-pae-lab-release
```

构建后应从完整部署目录启动 Lab，不要直接运行裸 `bin` 产物：

```powershell
& .\out\build\windows-msvc-pae-lab\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe
```

具体工具链、目标和故障定位见 [06 构建测试与问题定位](docs/guides/06-构建测试与问题定位.md)。

## 当前本机交付

以 [本地交付入口](deliverables/README.md) 为准：

- SDK 五包来自 clean `7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`；
- 首选 Lab 来自 `fe1683c` 加结构化诊断与流式示例打包的未提交修补，不是把当前仓库 HEAD 套到旧二进制上；
- 旧 `98df5e0` SDK/Lab 和 `fa81329/common-release` 仅作历史对照。

这些都是本机限定试用产物，不代表稳定 ABI、Linux、正式发布、许可闭合、真实协议 Golden、硬件或现场验收。交付二进制被 Git 忽略，普通 clone 不会自动取得。

## 工程边界

- 权威 Schema 当前枚举 0.1～0.11，但版本号不表示所有工具都支持同一能力组合。
- 真实协议资料、客户/线路信息、生产代码和现场报文默认不进入本仓库 Git 历史。
- 仓库尚未授予项目级开源或商业分发许可；第三方许可证不替代 PAE 自身授权。
- 历史根 README 正文完整保存在[归档快照](docs/archive/root-readme-history-20260921.md)。
