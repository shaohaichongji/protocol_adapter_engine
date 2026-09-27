# adeae30 体验包中文导航草稿与离线链接预检

## 范围与现场

按 [首版完整体验包计划](first-use-bundle-plan-20260927.md) 的并行文档片及后续限定文档闭包片执行。现场主仓库为 `main@adeae30d942ba42cc518c244c220730b7e462d2c`，暂存区空；首片开始时仅总控计划为未跟踪文件，闭包片重查时又有 PAE 任务的新 SDK 验证报告，均未触碰。本任务只新增/修改 `docs/experience/` 与本记录，不修改已有 guide、`docs/sdk/`、`schema/` 原件、产品源码、工程索引或任何现用产物。

文档约定归集后以一个包根为工作目录：根部四份指南、完整清单和 SHA-256；`lab/` 为唯一默认 static Release 完整部署；`sdk/` 为 Source、含 YAML 的 Static/Shared Debug/Release 五个新文档修订包；顶层 `schema/` 和一份诊断契约提供首次配置所需的最小离线规则。当前只是结构与命令草稿，新的五 SDK、Lab、总包与 ZIP 尚未由本任务生成，所有目标路径、哈希和新位置 PASS 均待后续归集验证。

## 建议的固定源文件白名单

| 归集目标 | 固定 `adeae30` 源或后续成品 | 用途与停点 |
| --- | --- | --- |
| 根 `README.md`、`01-Lab体验.md`、`02-SDK接入.md`、`03-配置与边界.md` | 本轮 `docs/experience/` 同名源稿 | 归集时复制后重新核对相对链接和内容身份 |
| `schema/pae.schema.json` | `docs/experience/sdk-docs/schema/pae.schema.json`（与固定源相同字节） | 结构草案；不是单凭 Schema 即可执行的承诺 |
| `schema/pae_yaml_profile_v0.1.md` | `docs/experience/sdk-docs/schema/pae_yaml_profile_v0.1.md` | 受限 YAML 作者源完整规则；工程历史引用已标为未附 |
| `schema/strict_json_profile_v0.1.md` | `docs/experience/sdk-docs/schema/strict_json_profile_v0.1.md` | 严格 JSON 字节、Number Token 等完整规则 |
| `docs/engineering/json_loader_diagnostic_contract_v0.1.md` | `docs/experience/sdk-docs/docs/engineering/json_loader_diagnostic_contract_v0.1.md` | 全文闭合严格 JSON 文档的必要相对链接；不是整套工程报告 |
| `lab/configs/synthetic_binary_ui_stage1.pae.json` | `tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json` | Lab 归集应包含；新部署中的文件身份和输出待验证 |
| `lab/configs/synthetic_ascii_literal_only.pae.json` | `examples/config/synthetic_ascii_literal_only.pae.json` | 与 YAML 对照的同协议完整文件 |
| `lab/configs/synthetic_ascii_literal_only.pae.yaml` | `tests/protocol_lab_ui/fixtures/synthetic_ascii_literal_only.pae.yaml` | Lab YAML 正常解析输入 |
| `sdk/pae-sdk-{source,static-debug,static-release,shared-debug,shared-release}/` | PAE 任务后续五个新文档修订包完整目录 | 文档投影须在新副本生成清单前汇入；归集后不私改，逐包验证 provenance、manifest、Hash 和包外消费 |
| `lab/` 运行闭包 | Lab 任务后续一个 static Release 完整部署 | 不逐个手拣 Qt DLL/插件；实际内容、来源和最小启动待验证 |

上述固定源文件在本轮仓库内逐项存在；交付投影见下节映射。旧 `b12ad80` 包只用于核对目录形状和示例目标名，不作为新包验证。根 `MANIFEST.txt`、`SHA256SUMS.txt` 应在最终归集时由总包实际文件生成，不复用任何 SDK 包内部清单。

## 离线链接精确审计

本套四份新指南中的相对 Markdown 链接当前均可在源目录解析；正文的 SDK/Lab/Schema 路径是**约定的包内文件路径**，目前未生成，须在最终目录逐项复查。顶层白名单不复制 `schema/README.md` 和完整 `protocol_plan_execution_semantics_v0.1.md`，避免为一次入门把历史工程验证链整段带入；首次配置所需结构、JSON/YAML 规则和诊断分层由本套指南及上表少量规范承载。

针对总控要求，读取了以下源文件的全部 Markdown 相对链接：

| 原文件 | 链接及分类 | 源码中目标/归档状态 | 旧包形状下的风险 |
| --- | --- | --- | --- |
| `schema/README.md` | `pae.schema.json`、`protocol_plan_execution_semantics_v0.1.md`、`strict_json_profile_v0.1.md`；本目录索引 | 三个源目标都存在；本文件不在顶层白名单 | 若只复制部分规范又复制此 README，会产生缺链；故顶层不复制 |
| `schema/strict_json_profile_v0.1.md` | `../docs/engineering/json_loader_diagnostic_contract_v0.1.md`；诊断规则引用，属使用相关规范 | 源目标现位于 `docs/engineering/`，存在；同名文件不在 `docs/archive/` | 旧 Static Release SDK 的 `share/pae/schema/` 下该链接指向包内不存在的 `share/pae/docs/engineering/`；顶层以单份契约闭合 |
| `schema/protocol_plan_execution_semantics_v0.1.md` | `pae.schema.json` 加 8 个 `../docs/engineering/windows-msvc-2026-*.md`；后 8 个是 DEC-040/041/042B、CRC、长度和有界变长的历史验证引用 | 8 个源目标均仍在 `docs/engineering/`，同名文件不在 `docs/archive/`；它们是历史报告而非首次使用必需输入 | 旧 Static Release SDK 同目录下 8 个工程链接均无包内目标；顶层不复制整份语义文档及历史报告链，但新 SDK 自带文档仍需闭包处理 |
| `schema/pae_yaml_profile_v0.1.md` | **0 个 Markdown 链接，但有 5 个 `docs/engineering/` 文字路径**：`yaml-entry-plan-20260926.md`、`yaml-frontend-slice-validation-20260926.md`、`lab-yaml-minimal-entry-validation-20260926.md`、`yaml-sdk-component-validation-20260926.md`、`yaml-parser-resource-validation-20260926.md`；均为上位计划或旧阶段验证 | 五个源目标都在 `docs/engineering/`，同名文件不在 `docs/archive/` | 原文不是可点击链接，却容易被误读为 SDK 包内文件；交付投影逐处标为固定源码仓库历史参考、未附带 |
| `docs/sdk/README.md` | 无 Markdown 相对链接 | 源文档存在；正文有 `b12ad80` 历史候选说明 | 新包若沿用正文，包级新导航必须以 `adeae30` provenance/总清单纠正身份，不能把旧文字说成新包验证 |

上表 8 个历史目标精确为：
`windows-msvc-2026-dec040-bitfield-slice.md`、
`windows-msvc-2026-dec041-sum8-slice.md`、
`windows-msvc-2026-dec042b-compiler-slice.md`、
`windows-msvc-2026-dec042b-core-slice.md`、
`windows-msvc-2026-dec042b-lab-c3-cli.md`、
`windows-msvc-2026-crc-minimal-slice.md`、
`windows-msvc-2026-length-field-slice.md`、
`windows-msvc-2026-bounded-variable-record-slice.md`。
它们在主仓库有现行路径，内容属于早期验证；不是“源文件丢失”，缺口在 SDK 打包后的相对目标未随包提供。

最小处理方案已进入下述交付版投影：总包顶层只复制首次使用规范和一份诊断契约；完整历史验证文件不进顶层导航。新 SDK 包保留完整规则与语义，但把历史工程链接改为明确的固定源码仓库文字参考，在**生成新包 manifest/hash 之前**汇入投影。不能在归集后私改包内 Markdown，也不能只扫描新导航就宣称整包链接通过。实际新 SDK 和总包的完整离线闭包仍是后续停点。

## SDK 文档交付版投影

本片又按总控限定，逐份从固定 `adeae30` 源文件复制出 `docs/experience/sdk-docs/` 的交付版。所有 Markdown 开头都标明“交付版投影、非固定源码原始字节”；`pae.schema.json` 为闭合 Schema 索引链接的**未修改副本**。后续 PAE 打包片应在新的副本上完成映射，再产生该批包自己的正确清单与哈希，本任务不写任何已制 SDK。

| 固定源码仓库原文件 | 投影源稿相对 `sdk-docs/` | Source 包目标 | Static/Shared 包目标 |
| --- | --- | --- | --- |
| `docs/sdk/README.md` | `docs/sdk/README.md` | `docs/sdk/README.md` | `docs/sdk/README.md` |
| `schema/README.md` | `schema/README.md` | `schema/README.md` | `share/pae/schema/README.md` |
| `schema/strict_json_profile_v0.1.md` | 同名 `schema/` | 同名 `schema/` | 同名 `share/pae/schema/` |
| `schema/protocol_plan_execution_semantics_v0.1.md` | 同名 `schema/` | 同名 `schema/` | 同名 `share/pae/schema/` |
| `schema/pae_yaml_profile_v0.1.md` | 同名 `schema/` | 同名 `schema/` | 同名 `share/pae/schema/` |
| `docs/engineering/json_loader_diagnostic_contract_v0.1.md` | 同名 `docs/engineering/` | 同名 `docs/engineering/` | 同名 `share/pae/docs/engineering/` |
| `schema/pae.schema.json` | 同名 `schema/`，未修改 | 同名 `schema/` | 同名 `share/pae/schema/` |

投影改动仅限：SDK 首次运行说明移除 `b12ad80` 当前身份，改按所选包 `PROVENANCE.json` 判断，并把 BuildRoot 放在包外；Schema README 增加本目录 YAML Profile 入口、移走过时“当前 Lab”导航；Strict JSON Profile 与诊断契约只加投影标记，正文全文保留；Execution Semantics 保留完整草案规则，把 8 个历史 Markdown 链接和其他文字证据路径标成固定源码仓库历史参考；YAML Profile 保留语法、类型、来源和资源规则，将 5 个工程文字路径逐处标为未附带。原始文档与投影的逐份差异已按 `git diff --no-index` 核查；这不代表新 SDK 已重打包或其闭包已通过。

## 本轮已核对与未核对

- 已核对：固定源示例、两份必要配置、Schema 和 YAML Profile 均在当前仓库；旧 `b12ad80` Static Release 的 `getting_started` / `yaml_sdk_consumer` 路径形状，以及两个源示例的 PASS 字符串；新导航源文件的相对 Markdown 链接。
- 静态检查：五份新增 Markdown 的本地相对链接均存在，行尾空格 0、代码围栏成对；`scripts/check-portable-paths.ps1 -ChangedOnly` 含本记录得 `PORTABLE_PATH_CHECK_PASS files=5 hits=0`。`git diff --check` 退出 0，但它不覆盖未跟踪的新文件，因此另做上述逐行检查。
- 闭包片静态检查：`sdk-docs/` 六份 Markdown 的全部相对链接在投影目录内解析成功；仅 Strict JSON Profile 保留指向**已提供全文**诊断契约的必要链接，历史工程验证链接均转为明确未附带的文字参考；`schema/pae.schema.json` 与固定源 SHA-256 相同。逐段文本比较显示 YAML“语法、类型与身份”至“来源定位与资源边界”、Strict JSON 第 2～9 节、Execution Semantics 第 3～10 节以及诊断契约全文正文与固定源完全相同；其余差异逐份审查均限投影说明、旧导航或证据引用句，未改规则数值或执行约束。当前 `docs/experience/` 加本记录共 12 个候选的 `-ChangedOnly` 便携路径扫描为 `PORTABLE_PATH_CHECK_PASS files=12 hits=0`；11 份 Markdown 行尾空格 0、代码围栏成对。完整新包安装树链接仍未验证。
- 未核对：`adeae30` 五个新 SDK 的实际文件、Source/Static/Shared Debug/Release 运行、Lab 部署、最终 ZIP/解包哈希、新位置离线全量链接和用户人工体验。没有执行文档中的 CMake 命令或启动 Lab。
- 无复制产物、ZIP、构建、测试、UI 操作、Stage、Commit、Push、删除、覆盖部署或发布。本轮通过的静态检查不得替代后续总包验收。

闭包片收尾时并行任务又新增 `experience-lab-validation-20260927.md`；本任务未触碰
该报告、PAE 报告或总控计划。上述链接与便携检查只证明本任务源稿当前自洽，
不证明这些投影已经进入五个 SDK 的实际安装树或更新了包级元数据。
