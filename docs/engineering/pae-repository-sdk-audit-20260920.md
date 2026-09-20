# PAE 仓库源目录规整与 SDK 复用审计（2026-09-20）

## 1. Findings

### 1.1 五包可继续复用，但必须保持 `98df5e0` 身份

结论：`deliverables/sdk/98df5e0/` 下现有 Source、Static Debug/Release、Shared Debug/Release
五包仍可作为已验证的本地 SDK 候选继续使用，不需要仅因当前仓库推进到 `fa81329` 而重打包。
这个结论不允许改写包内 provenance、manifest 或 hash，也不把五包重新标成 `fa81329`。

从 `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` 到
`fa81329563dd3aea9bb167ef9bd34526a2606161`，以下实际 SDK 产品输入没有任何 Git 差异：

- `include/pae/` 全部公开头；
- `schema/`；
- `src/config_compiler/`、`src/protocol_plan/`、`src/protocol_core/`、
  `src/protocol_framing/`、`src/public_api/`；
- `cmake/PaeSdkInstall.cmake`、`cmake/PAEConfig.cmake.in`、`cmake/PaeYyjson.cmake`、
  `cmake/VerifyYyjsonVendor.cmake`；
- `scripts/package_sdk_stage3.ps1`；
- source SDK 白名单内的 public examples、四份公开合成配置、yyjson 锁定输入及
  `docs/engineering/pae-sdk-stage3-contract.md`；
- `examples/public_api_sdk_consumer/`。

这表示 public API、Schema、PAE 产品实现、安装导出和 binary SDK 内容没有因 G1/G2 Lab 工作改变。
Static/Shared 四包可以继续代表同一份 PAE 产品能力；G1/G2 是 Lab 消费与展示增量，不应冒充新的
PAE SDK 版本。

需要单独说明 source 包：`scripts/package_sdk_stage3.ps1` 的 source 白名单包含根
`CMakeLists.txt`。该文件在两个提交间确有变化，但三个 hunk 都是 Lab 构建组合门禁：

- 增加 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2` 对 private ASCII adapter 的当前仓库组合约束；
- 移除两个过宽的 Schema 0.10/0.11 + UI/adapter 门禁，以允许新的 Lab 组合；
- 没有改变 `PAE::pae` 产品 target、public source、Schema、install/export 或 SDK consumer。

现场 `pae-sdk-source/CMakeLists.txt` 的 Git blob 为
`bd505738ae0e1700cb701bf665541b245b468da7`，与 `98df5e0:CMakeLists.txt` 相同；当前
`fa81329:CMakeLists.txt` 为 `b72a4f967f96283ce492892a1ca985d4f3617ad9`。因此 source 包仍可按其
既有文档方式构建和消费 PAE，但它是精确的 `98df5e0` 白名单快照，**不是**当前仓库 source
snapshot，不能宣称逐字节等价 `fa81329`。

现场五包身份和自校验结果：

| 包 | provenance | 文件数 | `SHA256SUMS.txt` 复算问题数 |
| --- | --- | ---: | ---: |
| `pae-sdk-source` | source / Debug+Release / clean `98df5e0` | 81 | 0 / 80 entries |
| `pae-sdk-static-debug` | static / Debug / clean `98df5e0` | 29 | 0 / 28 entries |
| `pae-sdk-static-release` | static / Release / clean `98df5e0` | 29 | 0 / 28 entries |
| `pae-sdk-shared-debug` | shared / Debug / clean `98df5e0` | 25 | 0 / 24 entries |
| `pae-sdk-shared-release` | shared / Release / clean `98df5e0` | 25 | 0 / 24 entries |

总计仍为 189 文件、29,946,570 bytes；五份 `PROVENANCE.json` 均记录
`source_worktree_dirty=false`。本轮只重新读取身份和复算现有 hash 清单，没有重新构建、运行
consumer 或验证二进制行为。既有功能证据仍以
[clean-checkpoint SDK 验证](pae-sdk-clean-checkpoint-validation.md)为准。

### 1.2 当前源树只有一个真实空目录：`src/core/`

在仓库根进行不跟随 reparse point 的只读遍历，并按任务要求排除 `.git/`、`out/`、
`third_party/`、`deliverables/`、`local_private/` 后：

- 空目录仅 `src/core/` 一个；
- 该范围内没有 reparse directory；
- `src/core/` 为 0 tracked、0 untracked、0 hidden、0 ignored；
- 根/子 CMake、源码和脚本没有引用 `src/core/`；
- [src/README.md](../../src/README.md#L20)已明确将其标为“当前为空的历史占位目录”；
- 其余命中仅来自旧盘点、历史清理边界和综合计划文字，不是运行依赖。

判定：`src/core/` 是明确的**可删除本地空目录候选**。Git 不跟踪空目录，删除不会形成提交
diff；但本轮没有删除授权，应留待总控合并清理清单后单独确认。除它之外，没有发现第二个可凭
“空目录”直接处理的仓库源目录。

### 1.3 没有确认可删除的已跟踪源码、测试或兼容资产

当前容易因命名被误判为过期的对象仍有实际职责：

| 对象 | 当前证据 | 判定 |
| --- | --- | --- |
| `tools/protocol_lab/v06_execution.*`、`v06_format.*`、`v06_values_compat_internal.*` | 进入 CLI C1 target、C2/C3、v06/v07 Evidence 测试，`DocumentSession` 仍引用 | 应保留 |
| `tools/protocol_lab_ui/ascii_host_adapter_compat.*` | 条件构建 `pae_protocol_lab_ui_ascii_private_compat`，现有 session/test 仍消费 | 应保留；public 迁移不等于兼容桥已退役 |
| `tools/protocol_lab_ui/binary_host_adapter.cpp` | H2 关闭而 Binary UI 开启的受支持组合仍由 CMake 条件编入 | 应保留；待对应构建组合正式退役后再评估 |
| `public_legacy_complete_adapter.*` | 现行可选 public legacy complete target、测试和 standalone 白名单消费 | 应保留 |
| `tests/**/legacy*`、旧离线 Bundle fixture | 仍验证历史格式、Replay、指纹和版本拒绝边界 | 应保留，不可把旧数据按名称当垃圾 |
| `spikes/json_parser/` | 根 `PAE_BUILD_JSON_PARSER_SPIKE` 仍接入，保留选型清单和回归门禁 | 应保留；生成结果另由工程整理盘点 |
| `spikes/decimal_arithmetic/` | 不进根构建，但仍含独有的 DEC-042B 算术设计与复现向量 | 待决定归档，不删除；先做生产测试覆盖映射 |
| `scripts/cleanup-generated-artifacts-20260919.ps1` | 与执行记录构成已执行清理的可追溯白名单，默认 dry-run | 只需标明历史用途并保留；不得当作日常清理入口盲目重跑 |
| `docs/archive/` | 已有 README 明确历史身份和失效外部路径 | 应保留；归档不等于可删除 |

因此，本轮没有提出任何已跟踪代码、测试、fixture 或脚本的直接删除建议。后续若要退役 private/
compat，最低前提仍是：所有受支持构建组合已有单一路径替代、独有断言完成迁移、standalone 白名单
同步，并完成对应 Debug/Release 回归。

### 1.4 现行使用入口已经落后于 `fa81329` G1/G2

当前 Git 检查点已经包含 Binary complete Encode 与 Binary stream Session/UI，且 G2 三组短体验已由
用户确认；以下面向使用者的“当前”入口仍写成 Binary UI 只有 complete Decode，必须在新 Lab 候选
归集复核后串行同步：

1. [根 README](../../README.md#L7)：更新当前日期和第 12 行能力摘要；第 179～187 行的普通本地
   Lab 路径应指向新的统一候选。第 15 行的 `98df5e0` installed-SDK Lab 验证应保留，但改成明确的
   历史/兼容闭包证据，不能与当前 UI 能力混称。
2. [统一交付入口](../../deliverables/README.md#L21)：SDK 段继续指向 `98df5e0` 五包；Lab 段在并行
   归集完成后新增 `fa81329` 当前候选，并把 `98df5e0` static/shared Lab 标为 installed-SDK
   对照快照。两个身份必须分开，不把当前 Lab 冒称由新 SDK 打包生成。
3. [文档入口](../README.md#L24)：把“当前交付身份”拆成 SDK=`98df5e0` 与 Lab=`fa81329`，并链接
   各自证据；不要写成仍是“同批 Lab”。
4. [01 项目定位与能力边界](../guides/01-项目定位与能力边界.md#L48)：第 57 行 Qt Lab 行补入
   Binary complete Encode 和三种 Binary stream；第 85 行起分开 SDK 与 Lab 身份。
5. [02 首次运行与 Lab 体验](../guides/02-首次运行与Lab体验.md#L7)：更新前置路径、启动命令、
   配置表和第 75 行旧边界；可保留 complete Decode 作为最短首次体验，再追加 G1/G2 为进阶动作，
   不把三组专项人工复核变成每个新用户的强制清单。
6. [05 架构与代码阅读](../guides/05-架构与代码阅读.md#L84)：第 92 行更新现行 Binary UI 范围，
   并在阅读表中加入 public Binary stream owner/adapter、Session 投影和 G1/G2 契约入口。
7. [DEC-040 后续路线](post-dec040-roadmap.md#L3)：最上方“当前入口”仍停在 2026-09-19
   `98df5e0` 归集，应刷新为 `fa81329` 已推送、G1/G2 收口及当前统一交付盘点；下方旧派发保持历史。
8. [工程依据索引](README.md#L79)：在 Binary UI 链补 G1/G2 contract/validation/preflight/audit，
   在仓库整理区补本报告；不要要求读者从综合计划大段历史中寻找现行证据。

[standalone README](../../tools/protocol_lab_ui/standalone/README.md#L21)的职责是“用 installed SDK
构建完整 Lab”，因此不应简单把 SDK 根改成 `fa81329`。它应继续固定 `98df5e0` 五包，只把
“Current verified local products”改成清晰的 `98df5e0 installed-SDK validation snapshot`，并链接
统一入口中的当前 `fa81329` Lab。`tools/protocol_lab/README.md` 描述 CLI，不是 Qt G1/G2 入口，
无需为本轮能力同步而修改。

### 1.5 旧报告中的“未接入”是历史事实，应标历史而非删除或回写

以下文档在各自形成时准确记录了 Binary Encode/stream 尚未接入 Qt；它们是决策链和迁移证据，
不应批量删除，也不应把正文历史结论追溯改写成“已接入”：

- `lab-gui-capability-coverage-plan.md`；
- `lab-public-consumption-residual-audit.md`；
- `lab-public-api-stage4-binary-migration-plan.md`；
- `lab-owned-presentation-types-validation.md`；
- `lab-sdk-standalone-implementation-scope.md`；
- `lab-host-endpoint-observer-contract.md`、`lab-host-endpoint-observer-validation.md`；
- `post-dec040-roadmap.md` 当前入口以下的旧派发段落；
- 其他 H1/H2、Stage 1 报告中带有明确“本片未接 UI/Encode/stream”的范围声明。

最小处理方式：

1. 先修第 1.4 节列出的现行入口；
2. 在 [工程依据索引](README.md) 按“G1/G2 前置历史 → G1 → G2-A/B/C”组织链接；
3. 对最容易被直接打开误读、且没有醒目时点说明的旧计划，加一条短 banner：说明这是历史阶段
   快照，当前能力返回现行入口/G1/G2 验证；
4. `repository-organization-inventory.md` 已是早期 `dfb08f3` 现场快照，可在引用改完后移入
   `docs/archive/`，但仍是“归档候选”，不是删除候选。

## 2. SDK 复用判定

| 使用场景 | 判定 | 限制 |
| --- | --- | --- |
| 继续给外部最小 consumer 使用 Static/Shared 四包 | 可复用 | 保持 `98df5e0` provenance；既有 Windows/v142 证据，不升级为稳定 ABI 或正式发布 |
| 继续使用 source 包构建 PAE public SDK consumer | 可复用 | 它是 `98df5e0` snapshot；不能声称包含 `fa81329` 根 CMake 的 Lab 组合门禁 |
| 当前 `fa81329` G1/G2 Lab 复用这五包的 PAE 能力 | 产品接口层面没有发现阻断 | 本轮未重新做 installed-SDK Lab 构建；当前 Lab 候选的实际来源由并行交付报告证明 |
| 把五包目录改名为 `fa81329` 或改 metadata/hash | 禁止 | 会破坏 provenance，且 source 根 CMake 确实不相同 |
| 为了“版本看齐”立即重打五包 | 不建议 | 没有 PAE 产品输入变化；只会制造同能力新身份和重复验证成本 |

## 3. 清理建议清单

### 3.1 可删除，但必须另行授权

- `src/core/`：唯一确认的物理空目录。删除无 Git diff，但仍属于文件系统清理动作。

### 3.2 应保留

- 全部现行 PAE 分层目录、public headers、Schema、SDK 打包/安装文件；
- CLI/Evidence v06/v07、historical fixture、private/compat Lab 路径；
- `spikes/json_parser/`、`docs/archive/`、固定白名单清理脚本；
- `deliverables/sdk/98df5e0/` 五包及其原始 metadata；
- 本轮排除范围内由其他任务盘点或归集的 `out/`、外部产物、Lab 新候选。

### 3.3 只需历史标识或导航整理

- G1/G2 之前声称 Binary Encode/stream 未接入的计划、preflight 和阶段验证；
- `98df5e0` installed-SDK Lab 应标为兼容/闭包快照，不再冒充当前 UI 入口；
- `scripts/cleanup-generated-artifacts-20260919.ps1` 标为 2026-09-19 固定白名单执行工具。

### 3.4 待决定

- `spikes/decimal_arithmetic/`：先证明生产测试覆盖其独有不变量，再决定归档位置；
- `repository-organization-inventory.md`：完成链接迁移后移入 archive，仍不删除；
- private/compat 未来退役：需按构建组合和行为回归单独立项，不并入目录清理。

## 4. 实际检查与证据边界

本轮实际执行的只读检查包括：

- `git branch --show-current`、`git rev-parse HEAD`、`git status --branch --short`、
  `git diff --cached --stat`；
- `git log`、`git diff --name-status/stat 98df5e0..fa81329`；
- 对 SDK 白名单路径逐项执行提交间 diff，并对关键目录/文件比较 Git tree/blob；
- 读取 `package_sdk_stage3.ps1` 与 `PaeSdkInstall.cmake` 的实际白名单和安装规则；
- 读取五包 provenance，复算 `SHA256SUMS.txt` 全部 184 个条目；
- 不跟随 reparse point 的目录遍历，排除 `.git/out/third_party/deliverables/local_private`；
- `rg` 核对 CMake、脚本、源码、测试和文档引用。

没有执行构建、CTest、SDK consumer、Lab、可见 UI、网络、Linux、真实协议、硬件或现场验证；也没有
扫描工程整理负责的巨大 `out`/外部产物。hash 自洽只证明本地包内容未变化，不证明来源真实性、
稳定 ABI 或正式发布资格。

## 5. Git 与并行变化

接管时为 `main@fa81329563dd3aea9bb167ef9bd34526a2606161`，与 `origin/main` 一致，暂存区
为空。接管时已有总控修改：

- `M docs/engineering/pae-execution-delivery-organization-plan.md`

审查期间并行 Lab 任务新增：

- `?? docs/engineering/lab-g2-delivery-candidate-20260920.md`

本任务只新增本报告，没有读取或修改 Lab 新候选归集目录，没有修改源码、CMake、其他文档、SDK
metadata 或交付包；未 Stage、Commit、Push、移动或删除文件。
