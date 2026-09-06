# Internal Tools

V0.1 Core不依赖CLI（Command-Line Interface，命令行接口）。本目录中的工具只服务开发、
验证和诊断，不进入Core安装或导出边界。

- [`protocol_conformance_runner`](protocol_conformance_runner/README.md)：从外部配置和语料目录
  执行证据分级的Decode/Encode一致性验证，仅在`PAE_BUILD_TESTING=ON`时构建。
- [`pae_protocol_lab`](protocol_lab/README.md)：已实现`inspect/encode/replay/compare`离线切片和
  Windows `udp-exchange`同步有界单次收发，支持TX/RX原始Frame记录、V0.2阶段事件、RX来源
  Metadata、显式Replay模式和可校验Evidence Bundle
  （证据包）。其
  [精确内部契约](../docs/protocol_lab_contract_v0.1.md)不改变Core的Transport无关边界。
