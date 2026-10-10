# 9680cf9 同基线体验交付：阶段 A 文档准备（2026-10-10）

最新续接：修复示例已按独立身份投影进五个新派生 SDK，新的 experience-final ZIP
完成限定解包/示例验证，首选入口已更新；已完成派发范围，待总控复核，见第 10 节。
第 7–9 节保留原候选的真实失败过程，不改写为成功。以下阶段 A 保留原时点事实。

## 1. 结论与边界

**阶段 A 文档准备完成，待总控复核；阶段 C 尚未激活。** 接管现场为干净
`main@9680cf90512f053962949cb55467d6af35959088`，暂存为空。SDK 执行任务正在新建五包，
Lab 在本轮派发时尚未获准启动；本轮不消费未交接的产物，不投影真实 SDK、不构建产品、
不运行消费者、不打最终体验 ZIP，不更新 `docs/README.md` / `deliverables/README.md`。

实际修改仅为 `docs/experience/` 的 15 份派生 Markdown、两份 IDENTITY 及本报告。
20261009 历史报告、打包脚本、产品源文件、教学代码、Schema JSON、Qt 和旧产物均未改。
未 Stage/Commit/Push、删除、下载或改变全局环境。本轮停止后，由总控在 SDK/Lab 交回、
复核通过时另行激活归集，不能把阶段 A 完成当作整个新产品包验收。

## 2. 来源复核，不仅替换 SHA

实际执行 Git 基线差异检查 `513f6cc..9680cf9`，并从完整固定 SHA 用显式白名单生成
只读阅读 archive；没有从未交接的新 SDK 目录取文件。

- `src/`、`include/`、`schema/`、`examples/`、`third_party/` 和原诊断契约两基线无差异。
  Codec/Framer/Host 的原示例、调用链、方向/生命周期边界继续适用；历史报告不升级为
  本轮新验证。公开头 MSVC charset 说明还与当前 CMake 事实定向核对。
- `docs/experience/tutorials/` 的两 JSON、CMakeLists、verify.cpp 在两基线间无差异；
  当前四文件与固定 9680cf9 archive 的长度/hash 逐项一致；Schema JSON 投影同样与
  9680cf9 原 `schema/pae.schema.json` 一致。只更新派生 Markdown，不修改这些产品字节。
- 9680cf9 纳入 standalone 的 `/source-charset:utf-8 /execution-charset:utf-8`，以及
  YAML UI smoke 中文断言的 `QStringLiteral(u"未提供")`、三个固定 UTF-16 code unit 对照。
  它们已经属于冻结源码，而非继续叠加未提交补丁；本轮没有构建、执行或补写其测试成绩。
- 原源码中的 SDK 指南仍保留当时的准备文字，属于该提交固定字节；本轮只更新体验包
  投影的来源导航，不擅改原源码指南或重新声称其包字节原样来自新的导航。

本轮派生身份：

| 输入清单 | version | product_source_head | 条目数 |
| --- | --- | --- | ---: |
| `docs/experience/sdk-docs/PROJECTION-IDENTITY.json` | `aligned-sdk-docs/20261010` | 9680cf9 完整 SHA | 13 |
| `docs/experience/DOCUMENTATION-IDENTITY.json` | `aligned-experience-docs/20261010` | 9680cf9 完整 SHA | 20 |

逐文件 hashes 均按修改后实际 UTF-8 文件重算，13/13、20/20 一致，路径安全且唯一；
两清单的重叠 Schema/诊断投影使用同一文件 hash。派生 ID/version 不表示 SDK、Lab 或 ZIP
已经生成；真正归集后还须由各脚本形成派生 provenance 和根身份，不能给旧包改名替代。

## 3. 阶段 A 实际检查与证据

新证据根 `out/build/aligned-experience-docs-9680cf9-20261010/` 初始不存在。
`fixed-reading-inputs-v2.zip` 和 `fixed-reading-inputs/` 仅是固定提交的阅读材料，
不是 SDK 或最终体验包。辅助脚本 `check-stage-a.ps1` 的实际执行结果保存在
`stage-a-check.log`、`fixed-bytes.csv`、`virtual-links.csv`。

```powershell
& './out/build/aligned-experience-docs-9680cf9-20261010/check-stage-a.ps1'
git diff --check
```

| 检查 | 结果 |
| --- | --- |
| 教学四文件 + Schema JSON 固定字节 | 5/5 一致 |
| 两份身份清单 | 13/13、20/20 hashes 一致 |
| 三脚本 AST 解析和来源参数 | 3/3 通过，均接受 ExpectedSourceHead，不硬编码永久产品 SHA |
| 虚拟 Source 文档布局 | 14 Markdown / 10 本地链接，缺失 0 |
| 虚拟 Binary 文档布局 | 7 Markdown / 8 本地链接，缺失 0 |
| 虚拟五 SDK 加顶层导航/教程布局 | 58 Markdown / 72 本地链接，缺失 0 |

虚拟布局仅在内存中映射固定阅读文件与派生文档路径，不创建包目录、不复制 SDK 或 Lab、
不调用真实打包脚本。相对路径检查采用与打包脚本相同的简单 inline/reference link 规则，
不验证标题锚点、远端网址、所有 CommonMark 语法、纯文本路径或完整产品源闭包。

首次 archive 将 rapidyaml 许可名误写为 `LICENSE`，Git 报 pathspec 缺失，产生零字节
`fixed-reading-inputs.zip`；该失败文件保留，原始输出在会话工具记录，数值退出码未单独
落盘，未追造原日志。
查固定 tree 确认实际为 `LICENSE.txt` 后，使用全新 `fixed-reading-inputs-v2.zip` 成功，
没有覆盖、清理旧文件。这是阅读辅助命令修正，不是产品或打包脚本修复。

## 4. 阶段 C 精确归集命令计划（未执行）

下列命令只供总控后续明确激活。SDK/Lab 的实际交回路径、静态 Release 最终身份文件
和干净源码快照必须由总控提供，不推测未交接目录。每个原 SDK 先经总控确认同批来源；
Source YAML 为可选作者源（HasYaml=false），四 binary 为已安装静态 YAML addon（true）。

以下脚本体接收六个必填交回参数：`RawSdkRoot`（子目录 source/static-Debug/
static-Release/shared-Debug/shared-Release）、`LabRoot`（完整 static Release 部署）、
`PreparationProvenance`、`DeploymentHashes`、`ActualBuildIdentity`、`CleanSource`。
它们都必须是总控已验收交回的绝对路径，不能沿用 513f6cc 包。拟定两个新目标在本轮
只检查是否存在，不创建；执行时再次检查，任何现存目标即停止，不改变名称偷偷绕过。

```powershell
param(
  [Parameter(Mandatory)][string]$RawSdkRoot,
  [Parameter(Mandatory)][string]$LabRoot,
  [Parameter(Mandatory)][string]$PreparationProvenance,
  [Parameter(Mandatory)][string]$DeploymentHashes,
  [Parameter(Mandatory)][string]$ActualBuildIdentity,
  [Parameter(Mandatory)][string]$CleanSource
)
$ErrorActionPreference = 'Stop'
$Repo = (Resolve-Path .).Path  # 在 protocol_adapter_engine 根执行
$Head = '9680cf90512f053962949cb55467d6af35959088'
$StageC = Join-Path $Repo 'out/build/aligned-experience-9680cf9-stageC-20261010'
$Delivery = Join-Path $Repo 'deliverables/sdk/9680cf9-experience'
foreach ($Target in @($StageC, $Delivery)) {
  if (Test-Path -LiteralPath $Target) { throw "Target exists: $Target" }
}
New-Item -ItemType Directory -Path $StageC | Out-Null
$Cases = @(
  @('source', 'source', $false),
  @('static-Debug', 'static', $true),
  @('static-Release', 'static', $true),
  @('shared-Debug', 'shared', $true),
  @('shared-Release', 'shared', $true)
)
foreach ($Case in $Cases) {
  & (Join-Path $Repo 'scripts/package_experience_sdk_docs.ps1') `
    -SourcePackageRoot (Join-Path $RawSdkRoot $Case[0]) `
    -OverlayRoot (Join-Path $Repo 'docs/experience/sdk-docs') `
    -DestinationRoot (Join-Path $StageC "sdk-docs/$($Case[0])") `
    -EvidenceRoot (Join-Path $StageC "original-metadata/$($Case[0])") `
    -Kind $Case[1] -HasYaml $Case[2] -ExpectedSourceHead $Head `
    -OverlayIdentity 'aligned-sdk-docs/20261010'
  if (-not $?) { throw "SDK projection failed: $($Case[0])" }
}
& (Join-Path $Repo 'scripts/package_experience_bundle.ps1') `
  -ExpectedSourceHead $Head -SdkInputRoot (Join-Path $StageC 'sdk-docs') `
  -LabInputRoot $LabRoot -LabProvenancePath $PreparationProvenance `
  -LabHashesPath $DeploymentHashes -LabBuildIdentityPath $ActualBuildIdentity `
  -FixedSourceRoot $CleanSource -DestinationRoot $Delivery `
  -BundleName 'PAE-Lab-Windows-x64-9680cf9' `
  -SdkOverlayIdentity 'aligned-sdk-docs/20261010' `
  -DocumentationIdentity 'aligned-experience-docs/20261010' `
  -DocumentationRoot (Join-Path $Repo 'docs/experience')
if (-not $?) { throw 'Final bundle failed' }
```

阶段 C 执行器还须为每次实际调用独立保存 command/stdout/stderr/exit，并复核原始
payload hashes；不能把计划代码块当作已运行日志。任何来源/字段/测试或脚本缺口先报告，
不自行改脚本、产品或门禁。教程已由主归集脚本包含，无需默认再运行 tutorial overlay。
后续在新的含空格解包路径核对 ZIP/清单/五包 verifier，再按针对性授权进行消费者、
教学四断言和 Lab smoke，使用独立新 BuildRoots 与明确 PAE_DIR，不占用当前产品构建槽。
这些验证及入口更新均未在本轮执行。

## 5. LabBuildIdentity 最终交回要求

沿用已提交 Bundle 的现行 schema_version=1 门禁，不改脚本；完整背景见
[20261009 报告第 6 节](aligned-experience-docs-validation-20261009.md#6-专项只读复核后的三项最小修复与验证)。

- 顶层 `product_source_head` 必须是完整 9680cf9；`package_kind=static`、
  `configuration=Release`、`source_worktree_dirty=false`、`uncommitted_patches=[]`。
  从最终干净冻结快照构建，不复制旧测试候选；原准备 INPUT_PROVENANCE 仅为辅助。
- `preparation_provenance_sha256`、`actual_input_root`、`actual_input_manifest_path` 和
  `actual_input_manifest_sha256`：实际隔离输入按原 INPUT_SHA256 的
  `[{path,bytes,sha256}]` 格式在构建前保存、构建后全量复算。路径从 inputs/ 开始，
  Lab/Qt/yyjson 对冻结源码逐文件相同；配置副本与冻结输入对应。两包实际 static SDK 的
  provenance/hash 必须对应本批原包，也对应派生 SDK 保存的 base hashes。
- `build.exit_code=0`、真实 `log_path/log_sha256`、`input_manifest_before_sha256` 和
  `input_manifest_after_sha256`（均等于 actual manifest hash）；
  `executable_path/executable_sha256` 同时对应本次构建输出和交回部署 EXE。
- `ctest.exit_code=0`、实际标准 `Testing/<tag>/Test.xml` 的 `result_path/result_sha256`：
  使用 `ctest -T Test --no-compress-output` 保存真实 XML；六个唯一所需测试为
  `pae.tools.protocol_lab_ui.{yaml_entry,yaml_ui_smoke,compile_queue,document_state,schema_dispatch,unicode_literal}`，
  均 Status=passed。不得从旧日志合成 passed XML 或只填 ctest_passed=true。
- `ui_smoke.exit_code=0`、真实 `log_path/log_sha256`，日志包含精确
  `UI_SMOKE_PASS detail=2 document(s)`；`deployment_hashes_sha256` 绑定交回完整部署清单，
  格式 `hash  relative/path`。保留构建、测试和 smoke 的原始命令与退出记录。

字段、实际证据缺失或测试失败时不得签发成功身份，归集 fail closed。Hash 只用于一致性，
不是签名或来源认证，也不能替代总控对构建输入及证据的复核。生成 max 配置的现行受限
例外由固定生成器和实际前后清单约束，不在文档阶段重跑。

本轮未闭合真实产品正例、Qt 对外分发许可、稳定 ABI、Linux、真实协议/设备/现场和人工 UI
验收。阶段 A 完成后一次反馈总控，停止写入；下一步须总控明确激活，不自动继续阶段 C。

## 6. 阶段 C：五 SDK 文档投影实际结果（2026-10-10）

总控明确激活 SDK 文档投影，确认已复核 `deliverables/sdk/9680cf9` 五原 SDK 的来源及
257 条 hash。执行时重查主仓库 HEAD 为固定 9680cf9、暂存为空，保留阶段 A 派生文档；
交回的 `out/build/aligned-sdk-9680cf9-20261010/clean-source` HEAD 同 SHA 且 Git 状态干净。
新根 `out/build/aligned-experience-9680cf9-stageC-20261010/` 初始不存在，未覆盖旧目录。

实际调用：从仓库根执行
`out/build/aligned-experience-docs-9680cf9-20261010/run-sdk-projections.ps1`。
它按第 4 节相同参数逐包调用现有 `scripts/package_experience_sdk_docs.ps1`，不改脚本或
新增验证机制；每包使用独立 PowerShell 子进程记录实际数值退出码、stdout、stderr。

下表路径相对 `out/build/aligned-experience-9680cf9-stageC-20261010/`：

| 原 SDK | 派生 SDK | 原/派生文件数 | 退出码 | 现有检查 |
| --- | --- | ---: | ---: | --- |
| `deliverables/sdk/9680cf9/source` | `sdk-docs/source` | 102 / 103 | 0 | 原包及派生 verifier、链接、非白名单字节检查通过 |
| `deliverables/sdk/9680cf9/static-Debug` | `sdk-docs/static-Debug` | 42 / 44 | 0 | 同上 |
| `deliverables/sdk/9680cf9/static-Release` | `sdk-docs/static-Release` | 42 / 44 | 0 | 同上 |
| `deliverables/sdk/9680cf9/shared-Debug` | `sdk-docs/shared-Debug` | 38 / 40 | 0 | 同上 |
| `deliverables/sdk/9680cf9/shared-Release` | `sdk-docs/shared-Release` | 38 / 40 | 0 | 同上 |

原 SDK 路径本身仍相对仓库根，不相对 stageC 根。五派生包的 product source_head 均为
完整 9680cf9，documentation_overlay.version 为 `aligned-sdk-docs/20261010`，
`derived_package=true`；逐包 derived ID、provenance SHA256 与完整路径见
`evidence/projection-summary.csv`。顶层 `aligned-experience-docs/20261010` 身份不变，
供后续 bundle 导航使用，尚未消费为真实总包。

证据位于该 stageC 根：

- `evidence/commands.json`：实际子进程命令、原/派生根、完整 SHA 及 overlay identity。
- `evidence/<source|static-Debug|static-Release|shared-Debug|shared-Release>.stdout.log`、
  `.stderr.log`、`.exit.txt`：五次真实输出和退出记录，全为 0。
- `original-metadata/<包名>/`：每原包原始 PROVENANCE、MANIFEST、SHA256SUMS 原件保留；
  派生包的 metadata 按原脚本重建，根来源与派生文档身份分开。
- 五派生目录内 PROVENANCE 的 base hashes 和 documentation_overlay 逐文件 hashes、
  新 manifest/hash 清单是本轮实际结果，不把原 SDK 改贴新文档身份。

原脚本先校验原包，再复制和应用受限 Markdown/字节不变 Schema JSON，逐文件对照全部
非白名单原始字节、执行本地链接检查，最后派生 verifier 通过；五包均输出
`PAE_EXPERIENCE_SDK_DOCS_READY`。因此产品源文件/头/库/DLL/配置保持原字节，变化限于
现有文档白名单和派生元数据。SDK 未加入 Qt。文件数增加来自新增阅读投影，不是新功能。

本片未跑消费者、未占用产品构建槽、不重跑 18 次矩阵、未读取尚未交回的 Lab 产物，
不生成最终 bundle/ZIP、不更新入口。只更新本报告记录，不改 Lab/SDK 报告、产品源码、
打包脚本、Qt 或环境；未 Stage/Commit/Push、下载或清理。投影完成后一次交接总控并
停止写入；Lab 交回后，由总控发送精确输入接续最终 ZIP，不能自行提前消费。

## 7. 最终归集与限定验证：SDK 配置读取阻断

总控后续明确激活最终归集并交回 static Release Lab。调用现有
`package_experience_bundle.ps1`，所有来源门禁实际通过：固定干净 9680cf9、五派生 SDK、
Lab 实际输入清单/冻结源字节、构建前后 hashes、六项标准 CTest XML、smoke、构建 EXE
与部署 EXE、部署 hashes，以及复制后 SDK verifier/payload 和 Lab hashes。
不是仅采用准备 provenance 的旧式证明；未修改或跳过脚本门禁。

接受的 `LabBuildIdentity.json` SHA256 为
`66305284610818C7D2EBFC604BF30A926628B823063AD314DFB0C36C966F5763`，
其源路径为 `out/build/aligned-lab-9680cf9-20261010/evidence/LabBuildIdentity.json`。
本轮没有重跑该六项 CTest；门禁解析并复算交回的实际 XML/日志/输入/部署证据。

实际归集结果（相对仓库根）：

- 完整目录：`deliverables/sdk/9680cf9-experience/PAE-Lab-Windows-x64-9680cf9/`。
- ZIP：`deliverables/sdk/9680cf9-experience/PAE-Lab-Windows-x64-9680cf9.zip`。
- ZIP SHA256：`e98525765bd29bc8c231e4b390be19a8a83aae37f4b89f1f151910aee0d56dc7`，
  与同目录包外 `ZIP.sha256` 一致；现在是保留的未完成验收候选，不是新首选交付。
- 解包目录：`out/build/aligned-experience-9680cf9-stageC-20261010/final-validation/unpacked with spaces/PAE-Lab-Windows-x64-9680cf9/`。
- 产品 SHA 固定 9680cf9；SDK 文档 `aligned-sdk-docs/20261010`、顶层文档
  `aligned-experience-docs/20261010`，实际文件 hashes 分别记录，不冒充原始源码字节。

打包前仅将顶层导航、Lab 操作说明、SDK 接入页和 tutorial README 的准备措辞改为包
使用说明，重算 DOCUMENTATION-IDENTITY 的 20 条实际 hashes；没有预先宣称验收通过。
SDK 投影输入 13 项及已完成五派生包未改、不重做。教学四文件、Schema JSON 字节未改。

实际执行与停点：

| 步骤 | 实际结果 |
| --- | --- |
| 现有 bundle 脚本与实际来源/复制后门禁 | 子进程退出 0，312 payload |
| 新含空格路径解包，源/解包逐项 bytes/hash 对照 | 314/314 文件一致 |
| 解包根清单/哈希 | 312/312 payload 条目一致，ZIP hash 一致 |
| 解包 Markdown 本地链接 | 63 Markdown、72 本地链接、缺失 0 |
| 解包 Lab JSON/YAML `--ui-smoke` | 退出 0，`UI_SMOKE_PASS detail=2 document(s)` |
| 解包 static Release getting_started Configure | 退出 0；PAE_DIR 和 CMAKE_HOME_DIRECTORY 均指向解包 SDK |
| 同一新 BuildRoot 的示例 Build | 退出 0，只构建该示例 |
| 同一示例运行，传入含中文与空格的配置绝对路径 | 退出 1，stderr 为 `cannot read config: ...`，stdout 空 |

读配置文件失败发生在 `CompileProtocolJson` 之前，解包配置实际存在且已通过 hash 对照；
不能把它描述成协议解码失败，也不能断言其根因仅是空格。源示例在 Windows 以
`main(int,char**)` 接收窄 argv，传给 `ReadFile(const char*)`，其中直接调用
`std::ifstream input(path, std::ios::binary)`；未调用 `std::filesystem::u8path`，
本轮现场复读 `examples/getting_started/main.cpp` 后纠正此前报告的错误描述。
中文绝对路径的编码转换是待总控判断的线索，不是本轮已定位/修复的事实。
本轮按“真实门禁失败先报告”停下，不改代码、不改脚本或配置、不重建、不追加路径
矩阵，也不偷偷改参数重试。若后续允许从包根用 ASCII 相对配置路径复跑现存 EXE，
无需重复成功的 Configure/Build；即使通过也只能说明该路径可用，不能闭合 Unicode 路径。

证据根：`out/build/aligned-experience-9680cf9-stageC-20261010/final-validation/`。
实际执行器为准备证据根中的 `run-final-bundle.ps1`、`run-final-checks.ps1`（临时记录
原有检查和命令，不改生产脚本/新增框架）。保存了 bundle/validation 的 command、
stdout/stderr/exit；`validation-commands.json` 记录 Lab、Configure、Build、Run 全部
实际参数，各次独立 `*.stdout.log` / `*.stderr.log` / `*.exit.txt` 均保留。
`source-extracted-files.csv`、`markdown-local-links.csv`、`extraction-audit.log` 和
`consumer-cache-binding.log` 是真实结果；执行器最终退出 1，不生成成功 summary。

**最终限定验证未通过，`deliverables/README.md`、`docs/README.md` 首选入口未修改。**
源包、候选 ZIP、解包、成功构建和失败运行证据原样保留；未动 SDK/Lab 任务报告、产品
源码、脚本、Qt/全局环境，未 Stage/Commit/Push、删除、下载。完成一次阻断交接后停止，
由总控决定后续范围；不宣称生产、Linux、Qt 许可、稳定 ABI 或人工 UI 验收完成。

## 8. ASCII 相对路径最小复跑：仍然阻断

总控明确恢复针对性运行验证后，现场 HEAD 仍为完整 9680cf9，保留共享修改。
未 Configure/Build，使用第 7 节同一个 `sdk-getting-started/Release/pae_getting_started.exe`。
PowerShell 通过 `Push-Location -LiteralPath` 切换到原解包的
`sdk/pae-sdk-static-release` 包根，传入 ASCII 相对参数
`share/pae/examples/config/synthetic_stream_framing_slice.pae.json`，结束后恢复位置。

实际运行退出 **1**，stdout 空，stderr 为：

```text
cannot read config: share/pae/examples/config/synthetic_stream_framing_slice.pae.json
```

只读检查确认该参数对应的包内配置文件存在。记录的是调用方设置的工作目录，
未另外观测子进程实际工作目录；不能将本次失败直接归因于 Unicode、空格或协议编译。
没有成功的相对路径 workaround，不扩展路径矩阵或修改产品、脚本来绕过。

同一 `final-validation/` 下新增真实证据 `relative-path-command.json`、
`relative-path.stdout.log`、`relative-path.stderr.log`、`relative-path.exit.txt`；
原绝对路径失败日志和全部旧候选原样保留，未覆盖。
本次仅纠正本报告并补记实际结果，不改用户使用说明或文档 identity，
不创建 `9680cf9-experience-final`，不更新首选入口。未 Stage/Commit/Push、
清理、下载或改变 Qt/全局环境。达到失败停止条件，向总控交接后停止写入。

## 9. 明确原生子进程 WorkingDirectory：仍然阻断

总控仅授权一次明确子进程工作目录的复跑。调用前 `Test-Path -PathType Leaf`
确认原解包 static Release SDK 下的相对配置为实际文件；使用同一个已建 EXE，
未 Configure/Build。通过 `System.Diagnostics.ProcessStartInfo` 明确设置
`FileName` 为该 EXE 绝对路径，`WorkingDirectory` 为原解包
`sdk/pae-sdk-static-release` 根，参数仍为
`share/pae/examples/config/synthetic_stream_framing_slice.pae.json`。
`UseShellExecute=false`，stdout/stderr 重定向，`CreateNoWindow=true`，
并用 `WaitForExit(30000)` 限制运行等待。

实际进程未超时（`timed_out=False`），退出 **1**；stdout 无程序输出，stderr 仍为
`cannot read config: share/pae/examples/config/synthetic_stream_framing_slice.pae.json`。
这次不是仅设置 PowerShell 位置；真实 ProcessStartInfo 中明确设置了工作目录。
仍未得到可用 workaround，不作编码根因定论，不继续路径矩阵或编译探针。

同一 `final-validation/` 下新增 `explicit-cwd-command.json`、
`explicit-cwd.stdout.log`、`explicit-cwd.stderr.log`、`explicit-cwd.exit.txt`，
完整记录 EXE、工作目录、参数、超时设置、输出与实际进程退出码。
PowerShell 记录命令自身退出 0 不代表示例成功；判定依据是保存的子进程退出 1。
原两次失败日志与候选包保留。本次只补本报告；未改产品/脚本/说明/identity，
未生成 experience-final，未更新入口，未 Stage/Commit/Push、删除或改变 Qt 环境。
已达到本次失败停止条件，交接总控后停止写入。

## 10. 修复示例的独立派生与最终限定交付

总控交回 PAE 任务的示例修复并授权最小派生归集。现场 main/HEAD 仍为完整 9680cf9；
PAE 修复报告见 [路径修复](sdk-getting-started-path-fix-20261010.md)，本任务未修改
`main.cpp` 或该报告。PAE 独立原生探针及长短路径对照定位普通长路径打开限制，
并另证窄 argv 对 emoji 的限制；修复使用 Windows wmain、filesystem 宽路径及扩展前缀，
不改系统策略。其实际 265 字符及五正四负结果属于 PAE 任务证据，不计作本轮重跑。

本轮新增参数式单文件投影，仅允许 `examples/getting_started/main.cpp`，默认不应用；
需同时显式提供源路径、独立身份、预期原/新 SHA256。原包 source_head/clean 门禁保留，
检查原/新文件 hash，派生 ID 纳入两 hash 和补丁身份，非白名单检查只增加此精确路径。
bundle 额外检查五 SDK 的补丁全部存在或全部不存在、身份/hash 一致，原 hash 对照固定
干净源码、新 hash 对照每包实际文件，并在根 BUNDLE 身份记录；原来源/实际 Lab 门禁未绕过。
两脚本 AST 解析通过；缺少补丁参数、错误新 hash 两个针对性负例均拒绝且未创建目标。
默认关闭分支本轮仅静态复核，未额外生成默认模式测试包。

独立身份：

- 产品库/头/DLL/Lab/config：`9680cf90512f053962949cb55467d6af35959088`。
- 示例补丁：`getting-started-windows-path/1`，五 SDK 的相同源码新 SHA256 为
  `608c0a526470152565445be95fbdcf2a87bb7b807adfa7ce24f1ee43272e65f7`；
  原 SHA256 为 `e491e0812dabe11c7866d247aa4191b72e3443463b68104977ab7ecf733c52cf`。
- SDK 文档：`aligned-sdk-docs/20261010`；顶层文档：`aligned-experience-docs/20261010`。
  文档输入 hashes 按本轮说明重新计算，示例补丁不混入文档清单冒充 Markdown。

全新派生根 `out/build/aligned-experience-9680cf9-final-20261010/sdk-docs/`，
从未修改的五原 SDK 归集，不原地修改此前派生包。五包原/派生 verifier、文档链接、
全部非白名单字节检查通过，文件数仍为 103/44/44/40/40，各 provenance 含 example_overlay。
`original-metadata/` 保存原包三份元数据。未重建产品 SDK、库、DLL 或 Lab。

最终交付：

- ZIP：`deliverables/sdk/9680cf9-experience-final/PAE-Lab-Windows-x64-9680cf9.zip`。
- ZIP SHA256：`1d824871ee70f639057a414534064dd546d855f7128cea7d291913d08e73a17f`，
  与同目录 `ZIP.sha256` 一致；完整包目录在同级同名无扩展目录。
- bundle 现有门禁和复制后校验真实退出 0；314 文件、312 payload。

最终解包到新 `validation/unpacked with spaces/PAE-Lab-Windows-x64-9680cf9`，
源/解包 314 文件 bytes/hash 一致，根 manifest/hash 各 312 项通过；63 Markdown、
72 本地链接缺失 0。Lab 全部 19 文件与第 7 节成功烟测的解包候选 hash 完全一致，
复读其退出 0 和双文档 PASS，故明确沿用该烟测证据，本轮没有新 Lab 进程或人工 UI 验收。

仅从最终解包 static Release SDK 的示例源码，在全新 `validation/sdk-getting-started`
Configure/Build 一次：VS18 2026 x64，v142 14.29.30133；显式 PAE_DIR 和 CMAKE_PREFIX_PATH
指向最终解包包，Cache 的 PAE_DIR/源目录核对一致。两步实际退出 0。
同一新 EXE 用 ProcessStartInfo 明确 WorkingDirectory 为最终解包 SDK 根，
分别传实际配置绝对路径（本次 258 UTF-16 代码单元）和 ASCII 相对路径（65），
均退出 0、未超时、stderr 空，stdout 含 `GETTING_STARTED_BINARY_PASS`。
本轮没有重跑 PAE 的 265 字符或 emoji/负例矩阵，不将 258 宣称为新的超过 MAX_PATH 对照。
没有 18 consumer、其他配置或 SDK/Lab 矩阵重跑。

证据根 `out/build/aligned-experience-9680cf9-final-20261010/`：

- `evidence/commands.json` 与五包/bundle 的 stdout/stderr/exit；`example-gates.json`。
- `validation/command.json`、stdout/stderr/exit，`validation-commands.json`，
  Configure/Build 的独立 stdout/stderr/exit 与 `consumer-cache-binding.log`。
- `validation/absolute-command.json`、`relative-command.json` 及各 stdout/stderr/exit，
  明确 EXE、cwd、参数长度和 30 秒超时；`final-validation-summary.log` 为真实成功记录。
- `source-extracted-files.csv`、`markdown-local-links.csv`、`extraction-audit.log`、
  `lab-inherited-smoke.log`，分别记录逐文件核对、链接及烟测继承依据。

本轮实际修改：两个打包脚本、顶层 experience README/02-SDK接入/DOCUMENTATION-IDENTITY、
本报告、`docs/README.md` 和 `deliverables/README.md`；保留全部共享修改和原三次失败证据。
首选入口已指向新最终派生包；旧候选未覆盖/删除。`git diff --check` 通过、暂存为空，
未 Stage/Commit/Push、下载、改 Qt/全局环境。已完成派发范围，待总控复核，交接后停止写入。
未验证 Debug/Shared/Source 消费、UNC/网络/权限/全部 Unicode、Linux、稳定 ABI、生产/现场；
Qt 许可及再分发审查仍未闭合。
