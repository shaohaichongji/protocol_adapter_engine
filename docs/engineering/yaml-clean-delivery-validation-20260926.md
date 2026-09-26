# b12ad80 同基线 SDK/Lab 成品归集验证（2026-09-26）

## 结论与身份

### 后续人工体验记录（2026-09-26）

用户按总控简短步骤操作后明确反馈：“JSON/YAML 通过，Lab 已关闭”。
体验对象为归集后的 `deliverables/lab/b12ad80/static-release/pae_protocol_lab_ui.exe`。

- JSON：`configs/synthetic_binary_ui_stage1.pae.json`，应用 `device / Decode / ui_pipeline`
  绑定，输入 `80 0D 03 00 01 00 CA FE 05 5A`，预期匹配 `typed_record`、9 字段，
  `count=1`、`payload=CAFE`、`marker=90`；用户确认通过。
- YAML：`configs/synthetic_ascii_literal_only.pae.yaml`，应用 `device / Decode / ascii_pipeline`
  绑定，以 Hex 输入 `50 49 4E 47 0D 0A`，预期匹配 `ping`、解析成功、0 字段；用户确认通过。

证据为用户操作反馈，不是总控自动录屏或独立重跑。此次覆盖两条正常解析及步骤中的绑定应用，
不扩大为 Host 全生命周期验收。配置错误诊断、shared UI、组包、流式及真实协议未在此次人工体验覆盖。
用户已关闭 Lab；本片正常使用链路可限定收口，无需重复历史人工流程。
下文“本轮未人工体验”描述此前归集任务执行时的范围，由本段补充最新状态。

按[同基线交付计划](yaml-clean-delivery-plan-20260926.md)，将固定干净提交 `b12ad809bdbc35589e8e5755f0e4ad04f386fa10` 的七个 SDK 包和两个 standalone Release 完整部署复制到新版本目录。共 **365 个文件、94,309,087 bytes**，逐文件源/目标长度和 SHA-256 再次复算，失败 0；新位置七包运行现有只读包校验脚本，7/7 通过。归集只复制字节，没有重新构建、启动 UI 或更改包内 `PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS.txt`。

本批是可供本地统一体验的 Windows x64 初版候选，不是正式发布。Source/二进制 SDK 的构建与消费证据见[干净 SDK 验证](yaml-clean-sdk-validation-20260926.md)；standalone Lab static/shared Debug/Release 四组定向测试和部署来源证据见[干净 Lab 验证](yaml-clean-lab-validation-20260926.md)。本任务没有重跑那些构建、消费或测试。

## 精确源/目标映射

下表路径均相对于仓库根目录。执行前确认 `deliverables/sdk/b12ad80/`、`deliverables/lab/b12ad80/` 及新证据根均不存在；九个源目录和目标父目录没有 reparse 项，没有覆盖既有文件。

| 归集对象 | 源（相对 `out/build/`） | 目标（相对仓库根） | 文件 | Bytes |
| --- | --- | --- | ---: | ---: |
| Source SDK | `yaml-clean-sdk-b12ad80-20260926/source package with spaces/` | `deliverables/sdk/b12ad80/pae-sdk-source/` | 102 | 3,583,500 |
| YAML Static Debug | `yaml-clean-sdk-b12ad80-20260926/static-yaml-Debug-install/` | `deliverables/sdk/b12ad80/pae-sdk-static-debug/` | 42 | 20,834,967 |
| YAML Static Release | `yaml-clean-sdk-b12ad80-20260926/static-yaml-Release-install/` | `deliverables/sdk/b12ad80/pae-sdk-static-release/` | 42 | 9,612,206 |
| YAML Shared Debug | `yaml-clean-sdk-b12ad80-20260926/shared-yaml-Debug-install/` | `deliverables/sdk/b12ad80/pae-sdk-shared-debug/` | 38 | 5,628,788 |
| YAML Shared Release | `yaml-clean-sdk-b12ad80-20260926/shared-yaml-Release-install/` | `deliverables/sdk/b12ad80/pae-sdk-shared-release/` | 38 | 2,565,550 |
| JSON-only Static Release | `yaml-clean-sdk-b12ad80-20260926/json-only-static-Release-install/` | `deliverables/sdk/b12ad80/pae-sdk-json-only-static-release/` | 34 | 7,862,479 |
| JSON-only Shared Release | `yaml-clean-sdk-b12ad80-20260926/json-only-shared-Release-install/` | `deliverables/sdk/b12ad80/pae-sdk-json-only-shared-release/` | 30 | 815,751 |
| Lab Static Release | `yaml-clean-lab-b12ad80-20260926/standalone-static-Release/deploy/Release/` | `deliverables/lab/b12ad80/static-release/` | 19 | 21,625,867 |
| Lab Shared Release | `yaml-clean-lab-b12ad80-20260926/standalone-shared-Release/deploy/Release/` | `deliverables/lab/b12ad80/shared-release/` | 20 | 21,779,979 |
| **合计** | | | **365** | **94,309,087** |

两套 Lab Release 部署各有 14 份配置（13 JSON、1 YAML）及 EXE、Qt DLL、平台插件；shared 另有 `pae.dll`。部署内容按本轮 Lab 验证中的精确白名单解释，不把准备树里仅供测试的四份配置当作部署缺项。

## 归集检查与原始日志

1. 复制时对每个源/目标文件计算长度与 SHA-256；随后根据清单再次独立枚举并复算，**365/365** 一致、失败 0，目标实有文件数也是 365。原始逐文件相对路径、长度、哈希保存在 Git 忽略的 `out/evidence/yaml-clean-delivery-b12ad80-20260926/copy-manifest.csv`；复算汇总在同目录 `reverify-summary.json`。
2. 从归集后的七个精确 SDK 目标分别运行 `scripts/verify_yaml_sdk_package.ps1 -PackageRoot <包根> -Kind source|static|shared -HasYaml <布尔值>`。七次均返回 `PAE_YAML_SDK_PACKAGE_VERIFIED`，文件数依次为 102、42、42、38、38、34、30；输出在同证据根的 `sdk-package-verify.log`。该脚本只读复核每个包的清单长度、SHA-256、kind 与 YAML 组件声明。
3. 首次封装校验命令误用外层 `$LASTEXITCODE` 判定 PowerShell 脚本结果，在 Source 脚本已经输出成功标志后提前报错；复查脚本行为后按 PowerShell 实际成功状态重新执行七包，7/7 通过。这是调用方式失误，不是包校验失败，未修改包文件。

本轮将[交付入口](../../deliverables/README.md)、[Lab 首次体验](../guides/02-首次运行与Lab体验.md)、[Windows SDK 集成](../guides/03-Windows-SDK集成.md)的中文首选路径改为新位置的同基线 Static Release，列齐七包及两个 Lab Release 目录。原 `out/build/yaml-sdk-packaging-20260926/` dirty 候选、`7b4205e` SDK、`fe1683c-plus-patches` Lab 和所有其他旧产物保留原身份；新 `deliverables/sdk/`、`deliverables/lab/` 仍被 Git 忽略，仅 clone 不会带来这些文件。

四份本任务文档的相对 Markdown 链接均能定位，尾随空白检查和 `git diff --check` 通过；`check-portable-paths.ps1 -ChangedOnly -CandidatePath <本报告>` 返回 `PORTABLE_PATH_CHECK_PASS files=7 hits=0`。这仅检查当前候选文本中的机器路径，不扩大为二进制或 Git 历史扫描。

## 未验证与 Git 状态

本轮没有启动可见 Lab、执行人工 JSON/YAML 体验或真实 Host Apply；没有重新构建 SDK/Lab、运行 consumer/CTest、验证 Linux、真实协议、硬件或现场。包文件哈希一致与先前四组 5/5 定向结果不能代替人工体验，也不关闭项目许可、跨工具链稳定 ABI、生产容量或正式发布条件。

`docs/README.md` 的“当前交付身份”仍描述较早 `7b4205e`/`fe1683c` 入口；该索引由总控维护，不在本任务允许修改范围。总控复核本批归集后需同步该索引，避免与新的 `deliverables/README.md` 首选入口不一致。

本任务只修改上述三个已跟踪入口文档并新增本报告；总控/SDK/Lab 既有报告改动保留。未删除或改写源 `out/`、旧 `deliverables/`、包内内容或 Qt；无 Stage、Commit、Push、发布或部署替换。完成后停止写入，等待总控复核。
