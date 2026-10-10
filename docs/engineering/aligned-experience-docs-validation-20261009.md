# 同基线体验文档与归集脚本准备验证（2026-10-09）

## 1. 结论与身份

**已完成本轮文档/参数化准备范围，待总控复核；尚未生成或验证完整体验包。**
本轮以固定产品提交 `513f6cc9bd46b64b3b6d1d4283f13c806039eebf` 为阅读事实来源，
新增 Markdown 是派生说明，不冒充该提交原始字节。没有消费本轮新 SDK/Lab、运行产品构建、
生成派生 SDK 或最终 ZIP，也没有更新 `deliverables/README.md` 的首选入口。

接管与本轮静态检查时主仓库为 `main@513f6cc`，暂存区为空。共享工作树中的 SDK/Lab 报告
属于对应执行任务，本轮没有修改它们。总控最新通知：513f6cc SDK 五包已完成并复核 257 条
hash；Lab standalone 的 `/utf-8` 与 SDK `/source-charset:utf-8` 冲突由 Lab 任务限定修复。
这些是总控同步结果，不作为本轮独立 SDK/Lab 验证结论。

最终产品 SHA 可能因 Lab 最小修复提交而变化。三个脚本均以必填 `ExpectedSourceHead` 校验，
不存在固定 513f6cc/adeae30 永久允许值。**本轮文档身份仍准确保留 513f6cc**；若最终 SHA
变化，应先复核相关事实、教学代码及 Schema，再在另行授权范围内更新文档身份和 hashes，
不可只传新 SHA 绕过本轮身份检查，也不可混包或把有补丁的 Lab 称为干净 513f6cc 产品。

## 2. 实际变更与离线阅读边界

- `docs/sdk/README.md`：移除旧七包/旧宿主示例问题的当前事实表达，说明五包计划、来源
  核对、新 BuildRoot、宿主边界和公共头 MSVC UTF-8 接入约定；保留非生产/非稳定 ABI 边界。
- `docs/experience/`：更新顶层导航、Lab/SDK 指引、教程身份和教学构建说明，以及 SDK
  阅读投影中的指南、Schema/Profile、诊断契约、Codec/Framer/Host 与第三方索引。
  新增 Source/Binary 两种 getting-started README，使用各自包内实际配置路径。
- 新增 `docs/experience/sdk-docs/PROJECTION-IDENTITY.json`：
  `aligned-sdk-docs/20261009`，13 个输入 hash，含两个互斥的 getting-started 说明版本。
- 新增 `docs/experience/DOCUMENTATION-IDENTITY.json`：
  `aligned-experience-docs/20261009`，20 个顶层导航/教程/规范输入 hash。
- 修改三个现有归集脚本，参数化来源/目录/文档身份，补来源与白名单检查、教程归集、
  本地链接检查和失败停止行为；不改产品构建/制 SDK 脚本。

SDK 投影只选择 Markdown 和与原包字节相同的 `pae.schema.json`。原 SDK 中所有非白名单
产品字节保持不变；元数据重新生成，记录原 provenance、manifest/hash 清单及投影逐文件
hash，形成可区分的派生身份。SDK 不加入 Qt。历史工程报告不是首次离线使用的必需材料；
旧引用明确标为固定源码仓库的历史参考，不伪装成包内文件或当前功能验收。

两份教学 JSON、`tutorials/CMakeLists.txt`、`tutorials/verify.cpp` 均未编辑，与固定产品
archive 逐文件 hash 一致；Schema JSON 投影也与固定产品 archive 字节相同。

## 3. 本轮执行的检查与证据

独立证据根：`out/build/aligned-docs-513f6cc-20261009/`，初始不存在。
`fixed-reading-inputs.zip` 是固定提交的显式阅读文件白名单 `git archive`，不是完整源码
SDK、构建目录或已验收产品。`fixed-reading-inputs/` 是它的解包阅读材料。

实际运行的辅助命令（从仓库根）：

```powershell
& './out/build/aligned-docs-513f6cc-20261009/check-reading-layout.ps1' -RepositoryRoot (Get-Location).Path
& './out/build/aligned-docs-513f6cc-20261009/check-preparation.ps1'
git diff --check
```

| 检查 | 实际结果 | 证据 |
| --- | --- | --- |
| 三个脚本 PowerShell AST 解析、ExpectedSourceHead 参数及无字面产品 SHA | 3/3 通过 | `preparation-final-check.log`，初轮另有 `powershell-parse.log` |
| 两份文档身份与逐文件 SHA256 | 13/13、20/20 通过，路径唯一 | `preparation-final-check.log` |
| 四份固定教学输入及 Schema JSON | 5/5 固定字节一致 | `fixed-code-identity.csv`、`preparation-final-check.log` |
| Source 阅读模型 | 14 Markdown、10 本地链接、缺失 0 | `reading-layout-summary.log`、`reading-layout-links.csv` |
| Binary 阅读模型 | 7 Markdown、8 本地链接、缺失 0 | 同上 |
| 五 SDK 阅读目录加顶层材料的 Bundle 阅读模型 | 58 Markdown、72 本地链接、缺失 0 | 同上 |
| SDK 目标已存在、SDK evidence 已存在、文档产品身份不符、Bundle 目标已存在、Tutorial 目标已存在 | 5 个预期拒绝，现存 sentinel hash/文件数不变，未建派生目标 | `preparation-final-check.log`、`negative-guards/` |
| 公开文档/脚本输入的机器盘符路径定向扫描 | 33 文件无匹配 | `preparation-final-check.log` |
| diff whitespace | 通过；有 Git LF/CRLF 提示，不是构建或兼容性结果 | 本轮命令输出与最终静态日志 |

阅读模型仅复制固定来源的说明、示例和配置，验证预定相对布局；其中没有真实 SDK 库、
完整产品源码或 Lab EXE，不可把模型目录当作交付包。链接检查覆盖简单 inline links 和
reference definitions、路径解码和包根范围，不验证标题锚点、远端网址、所有 CommonMark
语法或纯文本路径。盘符扫描只是定向 regex，不是全量保密审计或二进制可迁移证明。

## 4. 后续脚本参数与精确归集白名单

以下是**后续获准归集时的参数约定，不是本轮执行命令或新增授权**。

### 4.1 五包文档投影

`scripts/package_experience_sdk_docs.ps1` 每包传入：

- `SourcePackageRoot`：总控复核交回的原 SDK；不得使用旧包或猜测产品路径。
- `OverlayRoot`：本轮 `docs/experience/sdk-docs` 或最终基线重新核对后的投影。
- `DestinationRoot`、`EvidenceRoot`：两个新建、尚不存在且在原 SDK/投影之外的目标。
- `Kind=source/static/shared`；Source `HasYaml=false`（可选作者源），binary `HasYaml=true`。
- `ExpectedSourceHead`：总控确认的最终完整产品 SHA。
- `OverlayIdentity`：与 `PROJECTION-IDENTITY.json.version` 完全一致。

每包公共投影 8 项：SDK 指南、相应 getting-started 说明、Schema README、三个
Profile/执行语义 Markdown、Schema JSON、诊断契约。binary 的 Schema/诊断契约进入
`share/pae/`；Source 另投影 Codec/Framer/Host README 与 third_party README，共 12 项。
原元数据另存 EvidenceRoot，派生包重建 provenance、manifest 和 hashes 并调用原 verifier。
真实派生包及其 verifier 正例尚未执行。

### 4.2 主体验包归集

`scripts/package_experience_bundle.ps1` 传入 `ExpectedSourceHead`、`SdkInputRoot`、
`LabInputRoot`、`LabProvenancePath`、`LabHashesPath`、`LabBuildIdentityPath`、`FixedSourceRoot`、全新
`DestinationRoot`、安全的 `BundleName`、`SdkOverlayIdentity`、`DocumentationIdentity`。
`DocumentationRoot` 默认本轮目录；五个输入子目录默认 Source/static-Debug/
static-Release/shared-Debug/shared-Release（Source 实际目录名为 `source`）。
如路径不同，以 `SdkPackageDirectories` 的恰好五项映射明确提供，不能靠目录名猜身份。

来源与白名单：

- 五个已验证派生 SDK 原样归集，核对 provenance 的 HEAD、kind/configuration/x64、dirty、
  YAML 声明和文档 identity；没有新的 product 编译。
- Lab 输入为最终交回的 static Release 完整运行部署；必须提供实际独立
  `INPUT_PROVENANCE.json` 结构和相对路径 SHA256 清单（格式 `hash  relative/path`）。
  准备 provenance 只作辅助；另需第 6 节的实际构建身份，校验冻结来源、实际输入、
  构建/测试结果与部署绑定。缺合格实际身份时停止，不得补造 provenance/hash。
  修复前脚本仅检查准备 provenance，实际补丁候选仍能通过该门禁；旧版
  “修补中的旧 Lab 将被同基线检查拒绝”的保证不成立，已在本轮纠正。
- Lab 只允许 EXE、Qt5Core/Gui/Widgets DLL、`platforms/qwindows.dll` 和
  `configs/*.pae.json` / `configs/*.pae.yaml`；必需上述五个运行文件和首次体验的
  `synthetic_binary_ui_stage1.pae.json`、`synthetic_ascii_literal_only.pae.json/.pae.yaml`。
- 四个顶层 Markdown、tutorials 六个 Markdown及四个固定产品教学文件；顶层 Schema
  五文件加诊断契约一文件，共 20 个文档输入。原样归集固定产品 Qt notice。
- 生成根 BUNDLE-IDENTITY、MANIFEST/SHA256SUMS、ZIP 及包外 ZIP.sha256；文档 version/
  文件 hash 与 SDK derived identity、产品 SHA、Lab 输入证据 hash 分开记录。

`FixedSourceRoot` 必须来自总控交回的同 SHA 干净独立源码快照；本轮带文档修改的主工作树
及仅阅读 archive 不满足该前提。目标存在、任意来源不符、缺失必需文件/身份或哈希不符
即失败；中途失败保留新目标证据，不自动覆盖、清理或继续。

### 4.3 独立教程 overlay（可选）

`scripts/package_tutorial_overlay.ps1` 接受已验证的同产品 `SourceBundleRoot`、
`ExpectedSourceHead`、干净 `FixedSourceRoot`、新 `DestinationRoot`、
`OverlayIdentity`（与文档身份一致）和可选 `DocumentationRoot`。
验证原包 manifest/hash 完整性，新增/替换四顶层说明及 tutorials 十文件，检查四个
教学产品输入不变、非白名单 SDK/Lab 字节不变，再重建元数据和 ZIP。
主归集脚本已经包含教程，不需要为本轮最终包默认再叠一次 overlay。

## 5. 剩余停点与未验证范围

1. 总控复核本轮 diff、身份、脚本参数及阅读布局；本轮停止写入。
2. Lab 修复完成后由总控确认最终产品 SHA 与交付策略。如 SHA 变化，先完成身份/来源
   重对齐，必要时重新交回 SDK/Lab，不能只给旧目录改名。当前报告保持原时点事实。
3. 获得后续明确派发后，才生成真实五派生 SDK和完整体验包，保存原始命令/stdout/stderr/
   exits；在带空格的全新路径解包，复核 SDK verifier、清单/hash、包外 JSON/YAML consumer、
   教程四项断言和 Lab smoke。每个消费新 BuildRoot，显式核对 PAE_DIR，防止缓存回读旧包。
4. 实际验证和总控复核完成后再决定更新首选交付入口。本轮没有代替人工 UI、Qt 分发
   许可、稳定 ABI、Linux、真实协议、硬件或现场验收。

本轮未 Stage、Commit、Push、删除、下载依赖或改动 Qt/全局环境；旧包、旧报告及产品
源码保留。共享 SDK/Lab 报告不纳入本轮文件归属。完成后一次主动反馈总控
`01a04601-757d-7bb1-8254-61dde4954d74`，状态为“已完成派发范围，待总控复核”。

## 6. 专项只读复核后的三项最小修复与验证

总控追加授权范围仅为 `scripts/package_experience_bundle.ps1`、
`scripts/package_experience_sdk_docs.ps1` 和本报告；测试夹具/证据另存全新忽略根
`out/build/aligned-docs-gate-fix-20261009/`。前五节的阅读准备事实及原证据保持原时点；
本节是其后修复，不倒填成此前已经具备的保证。仍未构建产品、生成真实派生 SDK/
最终体验包、更新入口或执行 Git 写操作。Lab CMake、SDK/Lab 报告等共享变更未触碰。

### 6.1 实际构建身份门禁

修复前只读代入真实 `charset-fix/static-input/INPUT_PROVENANCE.json`：固定 513f6cc、
status 空、static、Release、SDK dirty=false，旧 predicate 返回 True；但
`charset-fix/evidence/PATCH_PROVENANCE.json` 明确有未提交 CMake 补丁，四组候选
`ctest_passed=false`，原准备 provenance 属于叠加补丁前输入。未执行错误包正例。

新增必填 `LabBuildIdentityPath`，要求最终 Lab 任务生成并交回一个 schema_version=1 的
实际构建交接 JSON。当前 Lab 原始准备 provenance、补丁 provenance、四候选 summary
都不是合格最终身份，**没有合格真实身份时 fail closed**。本轮没有替 Lab 生成真实证明。

最小字段约定如下（路径均指向最终交回的实际证据，hash 使用同一大小写约定的 SHA256）：

| 字段 | 要求及真实生成方法 |
| --- | --- |
| `schema_version` / `product_source_head` / `package_kind` / `configuration` | `1`、总控冻结完整 SHA、`static`、`Release`；来自最终独立干净快照与实际组合，不沿用补丁候选名称 |
| `source_worktree_dirty` / `uncommitted_patches` | `false` / 空数组；只在最终构建实际来源和全部输入审计确认后填写；修复必须已进入冻结 SHA，不靠空数组隐瞒补丁 |
| `preparation_provenance_sha256` | 对最终实际使用的准备 INPUT_PROVENANCE 原文件取 hash；该文件仍作为辅助输入，不再当作实际无补丁证明 |
| `actual_input_root` / `actual_input_manifest_path` / `actual_input_manifest_sha256` | 最终实际隔离输入根和完整 `[{path,bytes,sha256}]` 清单；路径相对根、以 `inputs/` 开头，格式沿用现有 INPUT_SHA256；对实际输入而非补丁前旧清单生成并复算 |
| `build.exit_code` / `build.log_path` / `build.log_sha256` | 实际 Configure/Build/Deploy 收口成功时 `0`，保留真实原始构建日志及其 hash；若任一前置失败不签发成功身份 |
| `build.input_manifest_before_sha256` / `build.input_manifest_after_sha256` | 构建前保存实际输入清单，结束逐文件复算同一清单；这两个 hash 必须等于 `actual_input_manifest_sha256`，不可在事后覆盖清单再冒充前后相同 |
| `build.executable_path` / `build.executable_sha256` | 本次实际构建输出 EXE 路径/hash；门禁同时复算输出 EXE 与传入部署 EXE，二者一致 |
| `ctest.exit_code` / `ctest.result_path` / `ctest.result_sha256` | 所要求六项实际 CTest 退出 0；保留同次 CTest 的标准 `Testing/<tag>/Test.xml` 及 hash，不从日志伪造 XML；具体命令见下方 |
| `ui_smoke.exit_code` / `ui_smoke.log_path` / `ui_smoke.log_sha256` | 同次最终部署的 JSON/YAML smoke 退出 0、真实日志带 `UI_SMOKE_PASS detail=2 document(s)`，保留原始日志并取 hash |
| `deployment_hashes_sha256` | 对该最终运行部署的完整 `hash  relative/path` 清单取 hash；就是传入 LabHashesPath，不用另一个候选替换 |

最终 Lab 可在其已获准的验证范围内，用下列实际命令形状保存 CTest 标准 XML；这是给
后续执行任务的交接约定，**本轮没有运行产品测试**：

```powershell
ctest --test-dir $FinalBuildRoot -C Release -T Test --no-compress-output `
  -R '^pae\.tools\.protocol_lab_ui\.(yaml_entry|yaml_ui_smoke|compile_queue|document_state|schema_dispatch|unicode_literal)$' `
  --output-on-failure --verbose --timeout 60
# 保存真实退出码、Testing/TAG 指向的 Test.xml、原始输出，不改成功/失败记录。
```

门禁解析 Test.xml：恰好六个唯一所需名称、全部 `Status=passed`、退出码 0；不只相信
`ctest_passed=true`。EXE、输入清单、构建/CTest/smoke 文件及部署清单通过一份实际交接
身份 hash 关联。BUNDLE-IDENTITY 另记录实际交接 JSON hash，保留原准备证明的独立 hash。
Hash 用于一致性及复核，不是签名，也不能取代总控对原始构建命令/来源闭包的审查。

对清单每项复算实际 bytes/hash，核对 `inputs/` 下完整文件数；Lab 源文件、Qt、yyjson
对应冻结源码逐项 hash，一项提交外改动也拒绝。standalone-configs 普通副本必须对应
同清单中的冻结原配置；唯一允许的生成输入是已有固定生成器产生的
`standalone-configs/synthetic_ui_max.pae.json`，其实际 hash 仍受前后清单绑定，生成器
本身必须与冻结源一致，不在打包时重跑。实际输入 SDK 两包调用原 verifier，并与要
归集的 static 派生 SDK 的 base provenance/hash 清单对应，防止换成另一批原包。

### 6.2 目录尾分隔符与复制后闭包

- SDK 四个目录参数及 Bundle 目录入口使用 `GetFullPath` 后
  `TrimEndingDirectorySeparator`，保留 `C:\` 盘符根语义；SDK 子目录映射也规范化。
  原通用 verifier 未改，传给它的是统一目录形式。
- Bundle 对各 SDK 输入在验证后保存完整 payload（含原元数据）hash/数量；ZIP 前对
  目标五包各调用原 verifier，再逐文件对照原输入 hash 和完整数量。Lab 目标同样
  对照传入的原部署 hashes/完整数量。失败时立即停止，保留新目录证据，不执行 ZIP。
- 既有目标拒绝、文档白名单、链接检查和独立教程 overlay 未作无关重构。

### 6.3 本轮针对性执行结果与限制

实际命令：`& './out/build/aligned-docs-gate-fix-20261009/test-gates.ps1'`。
辅助脚本从当前 Bundle AST 提取同一门禁/复制后校验函数进行测试，不执行完整打包。
目录、假 `.lib` / `.exe`、日志/XML/身份均显著标为 `FIXTURE_ONLY_NOT_A_PRODUCT`；
它们不可执行、不可作为真实 SDK/Lab 或最终构建证明使用。

结果保存 `gate-tests.log`：

- 两脚本解析通过，复制后断言在 Compress-Archive 之前。
- 最小夹具门禁正常路径及尾分隔符路径通过；plain/backslash/slash 等价，盘符根保持。
- 真实旧 INPUT_PROVENANCE 作为最终身份被拒绝；引用现存真实补丁 SHA、CTest 失败状态
  的负例被拒绝。exit=0 但 XML 含失败项也拒绝。
- 即使声明 patches 为空、重算夹具实际输入清单，提交外 CMake 字节仍因冻结来源对照
  被拒绝。没有把失败候选套上合格真实证明。
- 复制夹具 SDK/EXE 篡改、额外文件均拒绝；原 verifier 也拒绝篡改 SDK。
- SDK/Bundle 已有目标在普通和尾反斜杠参数下均拒绝，sentinel hash/数量不变。

第一次仅测试辅助脚本误用了 PowerShell 自动变量 `$input`，导致生成清单的相对路径
错误，门禁正确拒绝。失败夹具保留在原 `FIXTURE_ONLY_NOT_A_PRODUCT/`；修正为专用
变量后使用全新 `FIXTURE_ONLY_NOT_A_PRODUCT-run2/` 通过，不清理或覆盖旧目录。
这不是产品缺陷修复或真实构建失败；首次工具输出保留在会话，未补造原始落盘日志。

这些是脚本函数、门禁负例和复制模拟验证；完整真实五包正例、实际最终 Lab 来源身份、
最终 ZIP/解包/包外消费仍未执行。需要 Lab 问题收口并按上述约定交回后，才可在另行
派发中打包。没有 Qt 许可、稳定 ABI、Linux、硬件或现场验收新增结论。
