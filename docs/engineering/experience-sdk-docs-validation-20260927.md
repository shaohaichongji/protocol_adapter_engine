# adeae30 体验 SDK 文档修订包验证（2026-09-27）

## 结论与身份

本轮未改动固定 `adeae30d942ba42cc518c244c220730b7e462d2c` 的产品源码、
干净 clone 或上一片五个 SDK 原候选。从原包**复制到新根**，叠加精确文档投影并重新生成
五包各自的 `PROVENANCE.json`、`MANIFEST.txt` 和 `SHA256SUMS.txt`。这五包是
`experience-sdk-docs/1` 派生候选，不是原包逐字节副本或完整体验总包。

接管时主仓库为 `main@adeae30d942ba42cc518c244c220730b7e462d2c`、暂存区空；
已有总控计划、SDK/Lab 验证报告和 `docs/experience/` 源稿均保留。本轮只新增
`scripts/package_experience_sdk_docs.ps1`、本报告，以及获总控最小扩边允许的四份
`docs/experience/sdk-docs/{examples/public_api_codec,examples/public_api_framer,
examples/public_api_host,third_party}/README.md`；不改工程整理已有的七份投影源稿。

## 预检、映射和可复现制包

原包离线链接预检发现 Source 包 14 条失效本地 Markdown 链接：Schema/诊断 9 条可由
已准备的投影闭合，另外 5 条位于三个 `public_api_*` 示例 README 和
`third_party/README.md`。四个二进制原包各有相同的 Schema/诊断 9 条。已先向
总控停报，获准仅扩展这四份 Source 文档投影。新投影把 5 条仓库历史验证/Qt 包说明
链接转为**固定 adeae30 源码仓库相对路径的文字参考，明确未随 PAE SDK 附带**；
保留原示例操作和使用边界，不附 Qt 说明或 Qt 依赖。原文与 11 份投影的逐份 Hash
和文本差异见 `overlay-source-diff.log`，其中 `schema/pae.schema.json` 原/新 Hash 相同。

新脚本对目标已存在、源/投影缺失、原包校验失败、固定来源不符和 Schema JSON 字节
不一致失败关闭。它读取原包清单和 provenance，先用既有
`verify_yaml_sdk_package.ps1` 验证原包；保真复制原
`PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS.txt` 到**包外**对应的
`base-*-metadata/` 证据目录，然后仅在新副本叠加白名单文档。新 provenance 保留
产品编译来源的 `source_head`、`source_worktree_dirty=false`，新增
`derived_package=true`、确定性 `derived_package_id`、原包三份元数据 SHA-256，
以及含版本、原/新 Hash 的 `documentation_overlay.files`；明确整包并非固定源码原始字节。
新清单和散列覆盖实际新包，旧校验脚本复算通过。对已存在目标的重复调用明确拒绝，
未创建第二份元数据证据，见 `existing-target-rejection.log`。

执行前新 `out/build/experience-sdk-docs-adeae30-20260927/` 与同名 evidence 根均不存在。
实际调用形式如下，Source 的 `-HasYaml:$false` 表示 YAML 为源码可选前端、不是安装组件；
四个 binary 为 `-HasYaml:$true`：

```powershell
& scripts/package_experience_sdk_docs.ps1 `
  -SourcePackageRoot <上一片对应原包根> `
  -OverlayRoot docs/experience/sdk-docs `
  -DestinationRoot <新根中同名五包目标> `
  -EvidenceRoot <新证据根中该包的base-metadata目录> `
  -Kind <source|static|shared> -HasYaml:<布尔值>
```

新包根均位于 `out/build/experience-sdk-docs-adeae30-20260927/`，可供总包归集片逐个
复制；四个 Lab 后续可选安装根与前轮同名，但必须取**本轮新根**：

| 包根目录 | 新文件数 | 派生 ID 前缀 | `verify_yaml_sdk_package.ps1` |
| --- | ---: | --- | --- |
| `source package with spaces` | 103 | `964d7604` | 通过 |
| `static-yaml-Debug-install` | 44 | `381d2114` | 通过 |
| `static-yaml-Release-install` | 44 | `b4a011cb` | 通过 |
| `shared-yaml-Debug-install` | 40 | `8f28a083` | 通过 |
| `shared-yaml-Release-install` | 40 | `dc74e556` | 通过 |

五包脚本输出见新证据根的 `source-package.log` 和四份
`{static,shared}-yaml-{Debug,Release}-install-package.log`。原五包、干净 clone
与 b12ad80 旧交付均未覆写。

## 离线链接与字节边界

递归枚举五个新包内**所有 Markdown**，检查非代码围栏中的 inline 链接和 reference
definition 的本地目标，并拒绝逃出包根：Source 检查 9 条、四个 binary 各检查
7 条，**缺失目标均为 0**；本批未出现 reference definition，结果见
`all-package-markdown-links.log`。这不把纯文字的源码仓库历史路径冒充包内链接。
`schema/protocol_plan_execution_semantics_v0.1.md` 完整保留执行契约，仅将未随包
附带的旧验证引用转为说明；旧 `pae-sdk-stage3-contract.md` 也未删除。

独立对比原包与新包每个文件路径及 SHA-256，结果见
`original-derived-file-identity.log`：所有原有非白名单文档、非元数据文件均同字节，
包括 C++ 源码、头文件、库、DLL、合成配置、依赖许可和示例。Source 原有 90 文件
同字节，改 9 份 Markdown 加 3 份元数据，新增诊断契约 1 份；每个 binary 原有
35/31 文件同字节，改 4 份 Markdown 加 3 份元数据，新增 Schema README 与诊断契约。
五包 `pae.schema.json` 与原包 SHA-256 一致。11 份投影原/新 Hash 及逐行差异见
`overlay-source-diff.log`；本轮没有生成或篡改新的产品二进制。

## 新位置实际消费

全部消费使用包外独立 BuildRoot、Visual Studio 18 2026 / x64 /
`v142,version=14.29.30133`。Source 包路径含空格，以
`-DPAE_SOURCE_DIR=<新Source根>` 配置 `getting_started` 与
`yaml_sdk_consumer`；两者 Release Configure/Build/Run 均退出 0。JSON 的
Decode/Encode 均为 `AA 00 07`，YAML 输出 `PAE_YAML_SDK_CONSUMER_PASS`。

四个 binary 包各以 `-DCMAKE_PREFIX_PATH=<对应新包根>` 分别构建并运行 JSON、
YAML 两个最小 consumer，共 8 组 Configure/Build/Run 均退出 0，Debug/Release
按包匹配。shared 的四份 consumer 相邻 `pae.dll` 与各自新包 `bin/pae.dll`
SHA-256 相同，见 `shared-dll-identity.log`。所有命令输出在新
`out/evidence/experience-sdk-docs-adeae30-20260927/` 的
`source-{json,yaml}-consumer-{configure,build,run}.log` 与
`{static,shared}-{Debug,Release}-{json,yaml}-consumer-{configure,build,run}.log`。

本轮没有重建产品，也没有重跑全仓、完整 Lab/UI、Linux、真实协议、设备或现场门禁。
包内其他历史 `public_api_*` 示例操作未逐一执行；其文字保留但不能将旧阶段验证
写成本轮新位置通过。最终体验总包、ZIP、跨包导航和用户人工体验仍由后续片完成。
脚本语法解析无错误，`git diff --check` 和本轮六个新增文本候选的显式便携路径扫描
通过（`PORTABLE_PATH_CHECK_PASS files=6 hits=0`）；只用 `-ChangedOnly` 会遗漏
未跟踪文件，故指定了精确候选。附带一次全仓扫描命中了现存 `spikes/yaml_frontend`
和 vendored rapidyaml 的 4 处历史文本，均非本轮候选；不据此修改既有文件，
也不把全仓扫描说成通过。原始输出分别见 `scoped-portable-scan.log` 和
`full-repo-portable-scan.log`。
主仓库未 Stage、Commit、Push、Pull、清理或覆盖旧产物；状态为
**已完成派发范围，待总控复核**。
