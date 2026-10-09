# PAE / Lab Windows 初版体验包

从本文件开始。整包解压后，以本文件所在目录作为体验包根；无需 clone 开发仓库，也不需要查找原机器的 `out/` 或 `deliverables/`。不要只取出 EXE 或某个 SDK 库文件。

本套导航对应固定产品源码 `adeae30d942ba42cc518c244c220730b7e462d2c` 的 Windows x64 本地体验候选，不是正式发布。五个 SDK 是以该源码为基础、另行投影文档的 `experience-sdk-docs/1` 派生包；包级中文导航也属于后续归集材料，不冒充该提交中的原文件。使用前核对根目录 `BUNDLE-IDENTITY.json`、`MANIFEST.txt`、`SHA256SUMS.txt` 和包外 `ZIP.sha256`，再核对所选 SDK 包的 `PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS.txt`。产品源码身份与交付文档身份应分别记录。

## 按顺序体验

第一次阅读协议配置，优先进入 [中文配置教程](tutorials/README.md)：提供带中文说明的完整 JSON、结构关系图、逐字节解释及预期结果。原有 synthetic 配置主要保留作专项验证输入。

1. [01 启动 Lab](01-Lab体验.md)：先用公开合成 JSON 和 YAML 各解析一次完整记录。
2. [02 接入 SDK](02-SDK接入.md)：需要编写 C++ 宿主时，从 Static Release 的两个最小 consumer 开始。
3. [03 配置、Schema 与边界](03-配置与边界.md)：再对照同协议 JSON/YAML、检查版本和诊断分层。

不写 C++ 时只做第 1、3 步即可。异常时保留操作、输入配置、完整错误文本及所选包身份；不要用另一版本的 DLL、配置或库“凑通”。

## 包内目录

下列路径均相对于本文件所在的体验包根：

```text
体验包根/
  README.md  01-Lab体验.md  02-SDK接入.md  03-配置与边界.md
  BUNDLE-IDENTITY.json  MANIFEST.txt  SHA256SUMS.txt
  lab/                         一个默认 static Release 完整部署及 configs/
  sdk/
    pae-sdk-source/
    pae-sdk-static-debug/
    pae-sdk-static-release/
    pae-sdk-shared-debug/
    pae-sdk-shared-release/
  schema/
    pae.schema.json
    pae_yaml_profile_v0.1.md
    strict_json_profile_v0.1.md
  docs/engineering/
    json_loader_diagnostic_contract_v0.1.md
  notices/qt-package.md        仓库固定 Qt 快照说明的原文副本，并非许可文本
```

五个 SDK 包均应包含本轮约定的受限 YAML 作者源能力；不再把 JSON-only 对照包放进主体验包。Lab 的 `configs/` 至少包含 `synthetic_binary_ui_stage1.pae.json`、`synthetic_ascii_literal_only.pae.json` 与 `synthetic_ascii_literal_only.pae.yaml`。这些都是从零设计的合成输入，不对应真实设备。

顶层只归集首次编写配置所需的 Schema、YAML Profile 和严格 JSON 规则；严格 JSON 文档中的诊断契约链接由上方单份 `docs/engineering/` 文件闭合。完整历史验证报告不纳入总入口。SDK 包内更多规范采用明确标注的交付版投影：规则和限制保留，历史工程报告改为固定源码仓库参考文字，不伪装成包内链接。顶层 Schema 与诊断文档也取自该交付版投影。

## 使用前提与边界

- Windows x64；Lab 运行需要完整同源部署目录。SDK 编译需 CMake、Visual Studio 18 2026 x64 与包验证所用的 MSVC `v142,version=14.29.30133`。以每包 provenance 核对实际 toolset、CRT 和 Debug/Release；Debug 与 Release 不混用。
- Static/Shared 是不同链接形态。Shared consumer 还需同一个 SDK 包的 `pae.dll`；不要从别的版本复制 DLL，也不要从 SDK 包复制系统 CRT DLL。
- PAE 只负责配置编译、冻结计划、编解码、切帧及公开宿主接入；不默认提供 Socket、串口、线程、重试或业务路由。Lab 是离线观察工具，不等于真实设备运行。
- 固定 Qt 快照标为 5.13.0，[仓库固定副本说明原文](notices/qt-package.md)记载它不是可追溯的官方原始包，许可/通知材料及再分发审查未闭合；本包未附可核实的 Qt 许可文本，不能据此对外分发。项目级对外分发许可、跨工具链稳定 ABI、Linux、真实协议/硬件和现场长期运行也未由此包证明。清单与哈希用于包内一致性，不提供签名或来源认证。

若清单、文件或来源身份不符，应停止体验并保留包原样，不自行补文件后继续使用。
