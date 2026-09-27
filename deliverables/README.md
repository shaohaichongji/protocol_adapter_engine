# 本地使用交付入口

此目录集中本机可用的 PAE SDK 和配套 Lab，不是正式对外发布目录。导航纳入版本控制，`sdk/`、`lab/`、`evidence/` 中本地产物被 Git 忽略；仅 clone 仓库不会自动获得这些产物。

## 当前首选：adeae30 Windows x64 本地体验包

完整包位于 `sdk/adeae30-experience/PAE-Lab-Windows-x64-adeae30.zip`，旁边的 `ZIP.sha256` 可核对压缩包；同目录保留解压前的完整包目录。解压后从包根 `README.md` 开始，默认直接运行 `lab/pae_protocol_lab_ui.exe`。五个 SDK 与一个 Lab static Release 部署使用固定产品源码 `adeae30d942ba42cc518c244c220730b7e462d2c`；SDK 文档另经 `experience-sdk-docs/1` 投影，二者不是同一身份。归集和解压后验证见[体验包验证](../docs/engineering/experience-bundle-validation-20260927.md)。

这是本地体验候选：Qt 固定副本的许可/通知与再分发审查未闭合，不作正式对外发布。下文 b12ad80 与更早入口保留历史身份，不再作为首次体验首选。

## 前一批 YAML 初版同基线候选（b12ad80）

需要上一批回退对照时选择 `sdk/b12ad80/pae-sdk-static-release/` 和 `lab/b12ad80/static-release/`；首次试用使用上方 adeae30 体验包。下表路径相对于本 `deliverables/` 目录；七个 SDK 包与两个完整 Lab Release 部署均由干净的 `b12ad809bdbc35589e8e5755f0e4ad04f386fa10` 固定提交构建，当时按原字节归集。包内 `PROVENANCE.json`、清单和哈希未改，归集记录见[同基线验证](../docs/engineering/yaml-clean-delivery-validation-20260926.md)。

| 用途 | 本目录下路径 |
| --- | --- |
| Source（含可选 YAML 源码，默认关闭） | `sdk/b12ad80/pae-sdk-source/` |
| Static Debug / Release，带 YAML 静态附加组件 | `sdk/b12ad80/pae-sdk-static-debug/`、`sdk/b12ad80/pae-sdk-static-release/` |
| Shared Debug / Release，带 YAML 静态附加组件 | `sdk/b12ad80/pae-sdk-shared-debug/`、`sdk/b12ad80/pae-sdk-shared-release/` |
| JSON-only Static / Shared Release 对照 | `sdk/b12ad80/pae-sdk-json-only-static-release/`、`sdk/b12ad80/pae-sdk-json-only-shared-release/` |
| standalone Lab Static / Shared Release | `lab/b12ad80/static-release/`、`lab/b12ad80/shared-release/` |

从仓库根目录可启动当前 static Release Lab，打开邻接 `configs/synthetic_ascii_literal_only.pae.yaml`；也可选其中的 JSON 合成配置：

```powershell
& '.\deliverables\lab\b12ad80\static-release\pae_protocol_lab_ui.exe'
```

完整运行目录含 EXE、Qt DLL、`platforms/` 与 `configs/`；shared 另需同批 `pae.dll`。带 YAML 的二进制 SDK 通过 `PAE::yaml_frontend` 静态附加组件与 `PAE::pae` 配套使用；JSON-only 包不提供该组件。详见[首次 Lab 体验](../docs/guides/02-首次运行与Lab体验.md)和[Windows SDK 集成](../docs/guides/03-Windows-SDK集成.md)。归集成品在 Git 忽略的 `sdk/`、`lab/` 下，仅 clone 仓库不会获得二进制包或 Lab 部署。

SDK 本轮七包从固定提交重新构建、安装、打包并完成包外消费，见[干净 SDK 验证](../docs/engineering/yaml-clean-sdk-validation-20260926.md)；Lab standalone static/shared Debug/Release 四组各有 5/5 定向测试，见[干净 Lab 验证](../docs/engineering/yaml-clean-lab-validation-20260926.md)。用户已确认 static Release JSON/YAML 正常解析及步骤中的绑定应用通过、Lab 已关闭，见[人工记录](../docs/engineering/yaml-clean-delivery-validation-20260926.md)。配置错误诊断、shared 人工体验和真实协议等未测项不计通过；此处是本地试用候选，不是正式发布、稳定 ABI 或许可闭合。

## 较早 YAML dirty 候选：构建目录已退役

2026-09-26 较早 dirty 候选的 SDK 打包与 Lab 迁移构建目录已纳入 2026-09-27 五根清理，不再提供旧启动命令。首次试用使用上方 adeae30 完整体验包，回退对照使用 b12ad80。

历史身份仍为 `source_head=4da54b9`、`source_worktree_dirty=true`，不因清理或后续提交改变。必要日志、配置及来源清单归集在 `evidence/cleanup-five-20260927/`；不是旧二进制完整备份。历史验证报告中的原路径仅表示当时位置。

详见[清理记录](../docs/engineering/build-cleanup-20260927.md)与[两批保留规则](../docs/engineering/build-artifact-retention.md)。原[SDK 验证](../docs/engineering/yaml-sdk-packaging-validation-20260926.md)、[Lab 验证](../docs/engineering/lab-yaml-sdk-migration-validation-20260926.md)保留原证据边界。

## 既有 JSON 试用入口（2026-09-22）

这批既有产物已归集到本目录下；以下路径均相对于 `deliverables/`：

- SDK 根：`sdk/7b4205e/`，内含 `pae-sdk-source`、`pae-sdk-static-debug`、`pae-sdk-static-release`、`pae-sdk-shared-debug`、`pae-sdk-shared-release` 五包；复核此批时可先用 static Release。
- Lab 对照（含结构化编译诊断及 Binary 流式示例）：`lab/fe1683c-plus-patches/static-release/`。
- 本次归集的原始证据与绝对路径清单位于 Git 忽略的 `evidence/portable-delivery/`；公共验证摘要见[归集验证记录](../docs/engineering/portable-delivery-validation-20260922.md)。

`sdk/`、`lab/`、`evidence/` 仍是本机 Git 忽略目录，仅 clone 仓库不会取得这些大文件；需要按验证记录另行取得或重建，不能把 README 中的相对路径理解为产物已纳入版本控制。诊断改动前的 shared 对照未纳入本轮新归集。

```powershell
& '.\deliverables\lab\fe1683c-plus-patches\static-release\pae_protocol_lab_ui.exe'
```

必须保留完整运行目录及其中的 `configs`、Qt DLL、`platforms`；shared 另需同批 `pae.dll`。这是 public-only installed-SDK 消费候选，包含当前 Binary G1/G2、ASCII 与 legacy 公开路径，不使用下文旧 common 候选的私有兼容路径。

Binary 流式示例打包遗漏已限定修复：新归集 Lab 已包含 `configs/synthetic_stream_framing_slice.pae.json`，与仓库 canonical 哈希一致，原有运行文件未改变。这是 post-fe1683c 配置补充，不是重建或新的功能验收；旧身份记录保持原样。见 [修复记录](../docs/engineering/lab-stream-fixture-packaging-repair-20260920.md)。

身份分开记录：SDK 来自 clean `7b4205ea899cf16b9c73ba6ebc4c64382b71dc63`；新首选 Lab 来自 `fe1683c` 加构建时尚未提交的诊断与示例打包修补。修补现已提交，但该二进制未重建，不是当前 HEAD 的干净构建。旧 shared 对照来自7b4205e加后来提交为fe1683c的包装/测试修补。后续提交不会改变这些既有构建身份，不修改原 provenance/hash。

新诊断片在common及standalone static/shared的Debug/Release六组定向测试各3/3通过；新static Release产品构建完成，2026-09-21用户已完成配置失败诊断短体验并关闭Lab。2026-09-22 归集后仅新增一次隐藏、离线 `--ui-smoke`，退出码 0；未重跑新版本全量测试或实际加载模块来源矩阵，不复用旧版模块证据冒充新版本验证。见 [诊断验证及人工记录](../docs/engineering/lab-structured-compile-diagnostics-validation-20260921.md)及[归集验证记录](../docs/engineering/portable-delivery-validation-20260922.md)。

SDK 五包/六组包外消费、诊断改动前Lab static/shared Debug各33项及Release各32项测试已有记录，其Testing-off构建与模块来源已验证；最终重复部署保护停点保留。导航更新不重跑测试。详见 [SDK证据](../docs/engineering/pae-trial-sdk-7b4205e-validation.md)、[诊断改动前Lab证据第9节](../docs/engineering/lab-trial-sdk-7b4205e-validation.md)。

## 历史 PAE SDK：已归集

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

## 历史 Lab：common 功能候选与 SDK 对照

以下旧 common 候选保留用于历史对照；需要此前 JSON 公开路径时使用上方既有 installed-SDK 入口：

```powershell
& '.\deliverables\lab\fa81329\common-release\pae_protocol_lab_ui.exe'
```

这是 G2-C 既有仓库内构建的完整复制，不是从干净 `fa81329` 重新构建，也不是 installed-SDK standalone。17 个运行文件已做源/目标哈希核对；本次归集未重新启动或测试。ASCII 0.10/0.11 与旧版本走既有兼容路径，public A1/A2/stream 和 public legacy 开关为 OFF。请保留完整目录，不只复制 EXE。来源、功能和验证边界见 [G2 归集记录](../docs/archive/engineering-20260921/lab-g2-delivery-candidate-20260920.md)。

### 既有 installed-SDK 对照入口（98df5e0）

需要核对已验证的 installed-SDK 消费时，使用旧 static Release（不含最新 G1/G2 UI）：

```powershell
& '.\deliverables\lab\98df5e0\static-release\pae_protocol_lab_ui.exe'
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
