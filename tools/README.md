# Internal Tools

V0.1 Core不依赖CLI（Command-Line Interface，命令行接口）。本目录中的工具只服务开发、
验证和诊断，不进入Core安装或导出边界。

- [`protocol_conformance_runner`](protocol_conformance_runner/README.md)：从外部配置和语料目录
  执行证据分级的Decode/Encode一致性验证，仅在`PAE_BUILD_TESTING=ON`时构建。
