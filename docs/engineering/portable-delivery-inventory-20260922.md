# 本地试用根归集与公共仓库路径治理预检（2026-09-22）

## 1. 结论与边界

本轮只读盘点 `<TRIAL_ROOT>` 与 Git 跟踪文本，未移动、复制或删除文件，未修改既有文档、代码、SDK、Lab 或 Qt，未构建、测试、Stage、Commit 或 Push。唯一新增文件为本报告。

结论：

1. `<TRIAL_ROOT>` 共 35,144 个文件、7,084,028,424 bytes（约 6.60 GiB）。建议只归集 287 个文件、52,033,006 bytes（约 49.6 MiB）；其余 34,857 个文件、7,031,995,418 bytes（约 6.55 GiB）主要是可重建 build、consumer、clean clone 和重复 inputs，归集与 Hash 复核完成后才可形成精确删除清单。
2. 应保留五个 clean SDK 包、当前 Lab 完整 Release deploy、SDK 包外消费最小证据、当前 Lab provenance/Hash/日志，以及流式 fixture 打包修补的最小前后证据。
3. 原始 manifest、provenance 和日志含本机绝对路径，必须保存在 Git 忽略目录；公共文档只记录语义别名、仓库相对路径、提交身份、文件数和 Hash，不复制原始绝对路径。
4. 当前 Lab deploy 只有 18 个运行文件，未发现独立许可证或 notice 文件。它可以继续作为本地试用候选，但本轮证据不足以宣称 Qt 运行库分发许可已经闭合。
5. Git 跟踪 Markdown 中有 74 个文件、298 个机器绝对路径起点；另有三份历史清理 JSON 保存 1,445 个绝对路径。治理必须区分当前入口、工程证据、历史归档、原始机读证据和第三方源码，不能全仓盲目替换。

## 2. 本报告使用的语义别名

| 别名 | 含义 |
| --- | --- |
| `<WORKSPACE_ROOT>` | 当前项目工作区根 |
| `<REPO_ROOT>` | `protocol_adapter_engine/` 仓库根 |
| `<TRIAL_ROOT>` | 本轮盘点的 SDK/Lab 本地试用根 |
| `<QT_ROOT>` | 本机 Qt 安装根 |
| `<VS_ROOT>` | 本机 Visual Studio 安装根 |
| `<USER_PROFILE>` | 当前用户配置根 |
| `<OTHER_WORKSPACE>` | 其他本地工程或历史验证根 |

绝对路径到上述别名的实际映射应保存在 Git 忽略的本地映射文件中，不应写入公共 Markdown、脚本默认值或跟踪 JSON。

## 3. `<TRIAL_ROOT>` 现场盘点

| 一级目录 | 文件 | bytes | 判断 |
| --- | ---: | ---: | --- |
| `sdk/` | 189 | 29,955,541 | 五个 clean SDK 包，保留 |
| `evidence/` | 681 | 118,673,611 | 根级 49 份最小证据保留；`build/` 可重建 |
| `consumers/` | 640 | 101,725,196 | 六组包外 consumer 构建树，结论已由日志和摘要承接 |
| `source/` | 3,189 | 378,582,561 | 固定提交的 clean clone，可由 Git 重建 |
| `lab/` | 9,746 | 2,796,889,275 | 旧 static/shared standalone 根；仅保留 fixture 修补最小证据 |
| `lab-compile-diagnostics/` | 20,699 | 3,658,202,240 | 多轮诊断构建；仅保留当前 deploy 与最小 provenance/日志 |

目录计数是逻辑文件长度，不是磁盘占用量。本轮没有跟随或迁移任何外部链接，也没有读取真实协议私有输入。

## 4. 建议保留集合

### 4.1 五个 SDK 包

建议目标：`deliverables/sdk/7b4205e/`。该目录已由仓库 `.gitignore` 排除，适合保存本机已验证包，不应纳入 Git。

| 包 | 文件 | bytes | 身份边界 |
| --- | ---: | ---: | --- |
| `pae-sdk-source` | 81 | 1,743,794 | clean source package |
| `pae-sdk-static-debug` | 29 | 17,391,001 | x64 Debug static |
| `pae-sdk-static-release` | 29 | 7,836,120 | x64 Release static |
| `pae-sdk-shared-debug` | 25 | 2,194,262 | x64 Debug shared |
| `pae-sdk-shared-release` | 25 | 790,364 | x64 Release shared |
| 合计 | 189 | 29,955,541 | 固定 clean source identity |

每包均有 `MANIFEST.txt`、`PROVENANCE.json`、`SHA256SUMS.txt` 和说明文件；五包身份摘要记录 manifest/hash 已验证。五包也都含 `LICENSES/` 条目，但这只证明文件被打入包，不替代正式许可证审查。

### 4.2 当前 Lab 完整 deploy

建议目标：`deliverables/lab/fe1683c-plus-patches/static-release/`。名称必须保留“基准提交加未提交修补”的边界，不能改称 clean `657d085` 构建。

来源语义路径：`<TRIAL_ROOT>/lab-compile-diagnostics/static-verified/deploy/release-product/Release/`。

该 deploy 共 18 个文件、21,482,235 bytes，包括：

- 1 个 Lab EXE；
- 3 个 Qt runtime DLL；
- 1 个 Windows platform plugin；
- 13 个 `configs/` 示例，其中包含流式 fixture。

现存验证摘要记录 EXE 与 fixture 均存在、文件数为 18，SDK source identity 为 `7b4205e`。输入 provenance 记录 Lab repository head 为 `fe1683c`，并列出当时的 dirty/untracked 输入；因此它不是 clean release。当前 18 文件中未发现许可证或 notice 文件，本轮不推断 Qt 分发许可已满足。

### 4.3 必要证据

建议统一归入 Git 忽略的：

- `deliverables/evidence/portable-delivery/sdk-7b4205e/`
- `deliverables/evidence/portable-delivery/lab-fe1683c-plus-patches/`

| 证据组 | 文件 | bytes | 保留内容 |
| --- | ---: | ---: | --- |
| SDK 根级证据（排除 `evidence/build/`） | 49 | 79,862 | package identity、六组 consumer 日志/摘要、toolchain、来源核对与验证脚本 |
| 当前 Lab provenance | 17 | 486,688 | input provenance、input SHA-256、8 份日志、6 份诊断证据及手工错误 fixture |
| fixture 打包最小证据 | 14 | 28,680 | before/after inventories、摘要、正负日志和最小 harness |
| 合计 | 80 | 595,230 | 原始证据，全部 Git 外保存 |

五包、Lab deploy 与证据合计 287 个文件、52,033,006 bytes。

## 5. 可在归集验证后提出删除的内容

本节只是候选，不产生移动或删除授权。

| 候选 | 文件 | bytes | 原因与前提 |
| --- | ---: | ---: | --- |
| `source/` | 3,189 | 378,582,561 | clean clone 可由固定提交重建；先确认远端/本地提交仍可达 |
| `consumers/` | 640 | 101,725,196 | 构建产物可重建；先保留六组运行日志和摘要 |
| `evidence/build/` | 632 | 118,593,749 | CMake/编译生成树；根级小证据已独立保留 |
| `lab/` 中最小 fixture 证据之外内容 | 9,732 | 2,796,860,595 | 旧 standalone build、inputs、deploy 与 probe 生成物；先归集 14 份最小证据 |
| `lab-compile-diagnostics/` 中当前 deploy/provenance 之外内容 | 20,664 | 3,636,233,317 | 多轮 static/shared build、重复 SDK/Qt/Lab inputs 与中间 deploy |
| 合计 | 34,857 | 7,031,995,418 | 约 6.55 GiB；需另行精确授权 |

`lab-compile-diagnostics/static-verified/` 也不是整体保留对象：其中 `build/` 与 `inputs/` 约占绝大多数，建议只归集 Release deploy、两份 input identity 文件和现存日志。其余 static/shared、final/verified 组合均是重复验证闭包，不应因目录名含 `verified` 就整根长期保存。

## 6. Provenance 与本地路径映射治理

### 6.1 Git 外保存原始事实

原始 `PROVENANCE.json`、`INPUT_PROVENANCE.json`、consumer origin、运行日志和验证脚本保留原样复制到 `deliverables/evidence/portable-delivery/`。该目录已被 `.gitignore` 排除，可以保留当时的机器路径事实而不污染公共仓库。

归集时应生成逐文件清单，至少记录：

- 原语义路径；
- 目标仓库相对路径；
- bytes 与 SHA-256；
- SDK/Lab identity；
- 原始证据是否含机器路径；
- 复制后 Hash 复核结果。

### 6.2 公共文档只使用可移植摘要

公共仓库可新增一份 sanitized provenance，字段只保留提交 ID、dirty 文件的仓库相对路径、包类型、配置、工具链语义、manifest/hash identity 和证据别名。不得把原始绝对路径从 Git 外证据复制回来。

本地映射建议保存在 Git 忽略文件，例如：

```text
deliverables/evidence/portable-delivery/local-path-map.json
```

映射文件负责把 `<TRIAL_ROOT>`、`<QT_ROOT>` 等别名解析成本机位置；公共脚本通过参数或环境变量接收根路径，不应硬编码个人机器路径。

### 6.3 历史真实性边界

文档中的旧绝对路径既可能是可移植性缺陷，也可能是历史证据。治理时应将路径替换成稳定别名，并在 Git 外保存“原文 Hash + 文件相对路径 + 行号 + 原始路径”的映射，而不是无记录地删改。普通提交只能清理未来 checkout；旧 Git 历史仍包含原路径，除非另行授权执行高风险历史重写。本轮不建议仅为本机路径进行 history rewrite。

## 7. Git 跟踪路径分布

### 7.1 Markdown

使用“盘符加目录分隔符”作为机器绝对路径起点，并排除 URL 后，跟踪 Markdown 中共 74 个文件、298 个起点：

| 区域 | 文件 | 起点数 |
| --- | ---: | ---: |
| `docs/engineering/` | 47 | 158 |
| `docs/archive/` | 17 | 104 |
| `docs/guides/` | 4 | 17 |
| 其他说明/README | 4 | 12 |
| `deliverables/README.md` | 1 | 6 |
| `docs/` 其他入口 | 1 | 1 |

按语义类别统计：

| 类别 | 文件 | 起点数 |
| --- | ---: | ---: |
| `<REPO_ROOT>` | 47 | 171 |
| `<OTHER_WORKSPACE>` | 21 | 50 |
| `<TRIAL_ROOT>` | 12 | 49 |
| 其他盘符路径 | 11 | 11 |
| `<QT_ROOT>` | 4 | 7 |
| `<WORKSPACE_ROOT>` | 2 | 5 |
| `<VS_ROOT>` | 3 | 3 |
| `<USER_PROFILE>` | 2 | 2 |

### 7.2 非 Markdown 跟踪文件

- 三份 2026-09-14 历史清理 JSON 共含 1,445 个绝对路径起点，是原始机器证据，不适合直接发布；建议迁出 Git 或生成 sanitized 副本，原始文件转入 Git 外证据。
- `scripts/cleanup-generated-artifacts-20260919.ps1` 有 4 处机器根常量，`scripts/cleanup-seven-builds-20260922.ps1` 有 1 处；后续应改为显式参数和安全范围校验。
- `scripts/package_sdk_stage3.ps1` 中的示例路径是教学占位符，不是当前机器泄漏，可改用 `<BUILD_ROOT>` 提升一致性，但优先级低。
- 第三方 Qt/yyjson 源码中的盘符样式文本、平台宏、URL 和测试字符串不纳入本轮治理；不能批量改写 vendored source。

## 8. 建议分批修改范围

### 第一批：当前用户入口

先处理 `deliverables/README.md`、指南 02/03 和直接指向 `<TRIAL_ROOT>` 的当前验证入口。统一改成 `deliverables/sdk/7b4205e/`、`deliverables/lab/fe1683c-plus-patches/static-release/` 与证据别名。此批直接改善可用性，且不需要重写历史结论。

### 第二批：现行工程证据

处理 `docs/engineering/` 中 47 个文件的 158 个起点。对仍承担当前来源定位的路径改为仓库相对路径；对历史运行输入改为 `<TRIAL_ROOT>`、`<REPO_ROOT>` 等别名，并在文档内声明别名时点。优先处理引用 `<TRIAL_ROOT>` 的 12 个 Markdown 文件。

### 第三批：历史归档和机读证据

处理 `docs/archive/` 的 17 个 Markdown 及三份历史 JSON。Markdown 可使用别名并保留历史语义；原始 JSON 应先生成 Git 外 Hash/路径映射和 sanitized 摘要，再决定从当前树移除还是替换。不得用普通文本替换破坏 JSON schema 或原始 Hash 证据。

### 第四批：脚本参数化

将两个清理脚本中的本机根改为显式参数，保留 resolved-path containment、reparse、进程和精确白名单门禁。脚本修改与历史证据迁移分开提交，避免把安全逻辑变化混入文档清理。

### 明确排除

- vendored Qt/yyjson 源码；
- URL、JSON Schema URI 与 CMake 诊断转义；
- 用户明确要求保留的本地私有映射；
- 旧 Git 历史重写。

## 9. 推荐执行顺序与停点

1. 用户确认建议目标相对目录和 Lab 命名边界；
2. 仅复制第 4 节 287 个文件，生成源/目标 SHA-256 清单；
3. 复核五包自带 manifest/hash、Lab 18 文件闭包、fixture Hash、原始 provenance 与保护集合；
4. 更新第一批当前入口，使所有推荐路径只指向仓库相对目录；
5. 再提出 `<TRIAL_ROOT>` 中 34,857 个文件、7,031,995,418 bytes 的精确删除清单；
6. 公共路径治理按第 8 节独立分批，不与大目录删除同时执行。

若用户希望把 Lab 作为可分发包而非本地试用候选，必须先单独确认 Qt runtime 的许可证/notice 闭包；本报告不猜测许可结论。

## 10. 验证与限制

已验证：现场 `main@657d085` 且工作树干净；trial 一级目录文件数/bytes；五包 manifest/provenance 摘要；当前 Lab 18 文件 deploy、input identity 与验证摘要；保留/候选集合计数；Git 跟踪 Markdown 和重点非 Markdown 的绝对路径分布。

未验证：没有重算五包所有 payload Hash、没有启动 Lab 或 consumer、没有重新构建、没有执行许可证审查，也没有证明所有历史绝对路径均可公开。统计用于归集决策，不升级为发布验收。

状态：已完成只读盘点与预检，待总控复核；未执行归集、路径改写或删除。
