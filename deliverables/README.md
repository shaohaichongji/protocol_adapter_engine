# 本地使用交付入口

此目录集中本机可用的 PAE SDK 和配套 Lab，不是正式对外发布目录。导航纳入版本控制，`sdk/`、`lab/`、`evidence/` 中本地产物被 Git 忽略；仅 clone 仓库不会自动获得这些产物。

## PAE SDK：已归集

身份：`98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`，原始打包输入 `source_worktree_dirty=false`。包内容保持原样，不随当前仓库文档提交号改变。

| 用途 | 本目录下路径 |
| --- | --- |
| 白名单源码 SDK（不是整个仓库） | `sdk/98df5e0/pae-sdk-source/` |
| Windows x64 静态库 Debug | `sdk/98df5e0/pae-sdk-static-debug/` |
| Windows x64 静态库 Release，首次使用优先 | `sdk/98df5e0/pae-sdk-static-release/` |
| Windows x64 动态库 Debug | `sdk/98df5e0/pae-sdk-shared-debug/` |
| Windows x64 动态库 Release | `sdk/98df5e0/pae-sdk-shared-release/` |

先读所选包内 `PAE-SDK-README.md`，再运行包内 `examples/sdk_consumer`。Source 使用 `PAE_SOURCE_DIR`；二进制包使用 `CMAKE_PREFIX_PATH`、`find_package(PAE CONFIG REQUIRED)` 和 `PAE::pae`。Debug/Release 不混用，shared 须部署对应 `pae.dll`。限定工具链为已验证的 Windows x64 MSVC v142 14.29.30133。

2026-09-19 归集核对189文件、29,946,570 bytes，源/目标SHA-256零差异。证据见 [SDK 归集记录](../docs/engineering/local-deliverables-sdk-validation-20260919.md)。复制核验不是重跑功能测试。

## Lab：最新功能体验与 SDK 对照分开

体验中文工作台、Binary 完整记录组包及 G2 流式操作，优先使用以下本地候选：

```powershell
& 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\fa81329\common-release\pae_protocol_lab_ui.exe'
```

这是 G2-C 既有仓库内构建的完整复制，不是从干净 `fa81329` 重新构建，也不是 installed-SDK standalone。17 个运行文件已做源/目标哈希核对；本次归集未重新启动或测试。ASCII 0.10/0.11 与旧版本走既有兼容路径，public A1/A2/stream 和 public legacy 开关为 OFF。请保留完整目录，不只复制 EXE。来源、功能和验证边界见 [G2 归集记录](../docs/engineering/lab-g2-delivery-candidate-20260920.md)。

### 既有 installed-SDK 对照入口（98df5e0）

需要核对已验证的 installed-SDK 消费时，使用旧 static Release（不含最新 G1/G2 UI）：

```powershell
& 'F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\deliverables\lab\98df5e0\static-release\pae_protocol_lab_ui.exe'
```

shared 对照入口为同级 `lab/98df5e0/shared-release/pae_protocol_lab_ui.exe`。每个目录的 `configs/` 保存12份公开配置；请完整保留邻接Qt DLL及 `platforms/`，shared另保留同批 `pae.dll`。

static 17文件、shared 18文件分别与原部署SHA-256零差异；新目录隐藏启动捕获了4/5个预期模块，全部从对应新位置加载。总控复核了文件一致性及模块来源记录。未在搬移后重做Compile/Decode/Encode，既有功能证据不等于新人工验收。详见 [Lab 归集记录](../docs/engineering/local-deliverables-lab-validation-20260919.md)。

## 阅读与边界

历史 G1/UI 大型生成树已按授权清理，最小日志与原路径映射保存在 `evidence/cleanup-20260920/`；详见 [清理执行记录](../docs/engineering/generated-artifact-cleanup-execution-20260920.md)。这不是完整构建树备份，需再次开发时重新构建。

- [01 项目定位与能力边界](../docs/guides/01-项目定位与能力边界.md)
- [02 首次运行与 Lab 体验](../docs/guides/02-首次运行与Lab体验.md)
- [03 Windows SDK 集成](../docs/guides/03-Windows-SDK集成.md)
- [05 架构与代码阅读](../docs/guides/05-架构与代码阅读.md)
- [SDK 功能验证身份](../docs/engineering/pae-sdk-clean-checkpoint-validation.md)

本地使用不等于稳定 ABI、Linux、真实设备或生产验收；对外分发所需许可与通知材料尚未闭合。不修改包内 provenance、manifest 或 hash 来冒充新的发布身份。
