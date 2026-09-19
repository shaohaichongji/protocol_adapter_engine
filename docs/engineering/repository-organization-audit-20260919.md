# 仓库结构、冗余与本地交付归集审查（2026-09-19）

## 1. Findings

### 1.1 没有确认可直接删除的已跟踪源码或测试

本轮在 `main@0b89beded03b39d8ffe019bfec659b5a5885a8f1` 上静态核对 Git 跟踪、根及子目录
CMake、Presets、README/工程文档引用。排除 `out/`、`.git/` 和第三方内部实现后，没有发现满足“未构建、
未引用、无独有兼容/证据职责”三个条件的已跟踪源码或测试。

最容易被旧命名误判的对象仍在现行链路中：

- `tools/protocol_lab/v06_execution.*`、`v06_format.*`、`v06_values_compat_internal.*` 仍进入
  `pae_protocol_lab_c1_execution_internal`；`DocumentSession` 仍持有 `ExecutionBridge`，C2/C3 及 v06/v07
  专项也直接消费这些文件。
- `v06_evidence.*`、`v07_run.*`、`v07_run_evidence.*` 仍由 CLI/证据 reader 和兼容测试编译；版本号是
  Evidence/Lab 代际，不是废弃标记。
- `tools/protocol_lab_ascii/ascii_offline_adapter.*`、Host observer、
  `tools/protocol_lab_ui/*_compat.*`、private `binary_host_adapter.cpp` 仍分别服务开发树默认/兼容配置、
  standalone 或旧分支回归。public migration 已完成部分路径，不等于所有 private/compat 路径可删。
- `tests/protocol_lab_v06_*`、`protocol_lab_v07_run_evidence`、旧离线 Bundle fixture 和版本负例仍承担历史
  格式/指纹/Replay 兼容门禁，不能仅因已有 0.11/new SDK 而删除。

因此本轮没有给出任何“立即删除已跟踪代码”的建议。后续退役必须先明确单一替代路径、确认不再进入任一
受支持 preset/standalone、迁移独有断言并完成对应 Debug/Release 回归。

### 1.2 两个真实的本地清理候选，但都不构成当前删除授权

1. `src/core/` 是物理空目录：0 tracked、0 普通未跟踪、0 CMake/源码引用；`src/README.md` 也明确称其为
   历史占位。Git 不跟踪空目录，删除它不会形成仓库 diff。它是明确的本地空目录删除候选，但仍应由后续
   精确清理清单取得用户授权后处理。
2. `spikes/json_parser/results/generated/` 被 `.gitignore` 精确忽略，现场为 24 个运行 stdout/stderr 文件，
   合计 108,896 bytes。它们不是跟踪的选型报告，也不是源码；在生成物盘点确认没有独有复现价值后可列为
   精确删除候选。不得连带删除同目录已跟踪的历史报告或整个 `spikes/json_parser/`。

### 1.3 当前主要问题是工程文档导航，不是文件内容重复

`docs/engineering/` 当前有 135 个 Markdown（含 README），总计约 1.76 MB。134 个非 README 文件中，
有 26 个文件名未从根 README、`docs/README.md`、`docs/engineering/README.md` 三个主入口直接出现。
这些文件大多是 Stage 4 的 preflight/gap review/实施验证链，不是无引用垃圾；部分仍由综合计划或相邻报告
引用。直接按日期、`draft`、`preflight` 或“已经实施”批量删除会破坏决策追溯。

建议后续只做导航整合：

- 在 engineering README 增加“公开消费迁移历史链”小节，把未索引的 contract/preflight/validation 按
  Binary、ASCII、Stream、SDK standalone 分组；不把 26 个文件平铺成无层次长清单。
- `lab-ascii-smoke-crash-diagnosis.md` 记录尚未关闭的历史 AV/focus-out 边界，应保持工程依据可见，不能
  仅因旧日期归档。
- `repository-organization-inventory.md` 的现场基线是早期 `dfb08f3`，其中“公开目录未跟踪”等描述已过时。
  本报告获复核且主索引完成替换后，可把旧 inventory 移入 `docs/archive/`，保留为阶段 0 历史，而不是删除。
- 已有实施验证明确替代的 preflight/gap review 可列为后续归档候选，但要先更新综合计划和相邻文档引用；
  例如 clean-checkpoint preflight 仍被综合计划第 4 节执行路径引用，当前不能移动。
- `pae-execution-delivery-organization-plan.md` 同时承载当前段和大量历史派发，体积与查找成本持续增长。
  可在未来建立短小的“当前状态/当前授权”入口，再把已结束批次移入独立历史卷；但外层 AGENTS、多个报告和
  当前协作仍把该文件作为恢复入口，本轮不能拆分或删历史段。

这类工作属于导航/归档迁移，需要单独文件范围与链接校验；不应与功能代码清理混做一次提交。

## 2. 当前目录职责与判定

当前 Git 共跟踪 3,072 个文件，其中 `third_party/` 2,448 个；第三方内部文件未逐项审查。其余主要目录
按实际职责归纳如下：

| 对象 | 现场职责/依据 | 当前判定 | 后续条件 |
| --- | --- | --- | --- |
| 根 `CMakeLists.txt`、`CMakePresets.json` | 统一能力门和开发构建入口；所有 9 个 configure preset 名均在 README/工程记录中有仓库外引用 | 必须保留；preset 各段同名是 configure/build/test schema 分区，不是重复文件 | 能力门收敛另做设计，不为整洁删除历史 preset |
| `.clang-format`、`.editorconfig`、`.gitattributes`、`.gitignore` | 格式、文本、忽略和本地生成物边界 | 必须保留；即使 `.editorconfig` 未被文档点名也不是噪音 | 无 |
| `include/pae/`（8 文件） | experimental C++17 public API | 必须保留 | 稳定 ABI/C ABI 另议 |
| `src/`（41 文件） | Plan、Compiler、Core、Framer、Host 与 public facade | 必须保留现有分层 | `src/core/` 仅为空物理占位，见 1.2 |
| `tools/`（119 文件） | CLI/Evidence/UDP、ASCII/Binary adapter、Qt UI、standalone 与 conformance runner | 全部保留；不进入 PAE SDK | private/compat 退役须逐 target 迁移和回归 |
| `tests/`（199 文件） | public/internal/Lab/UI 与历史格式兼容门禁；15 个一级测试目录均有 CMake，根 CMake 均有条件引用 | 全部保留 | 只有替代覆盖和版本支持边界变化后才合并 |
| `examples/`（53 文件） | public SDK 示例、内部能力示例、脱敏 config/Values | 保留 | 内部示例可在完全 public 化后迁移，不先删 |
| `cmake/`（10 文件） | SDK install/config、yyjson 校验、Qt/fixture/deploy helper | 全部有直接 CMake/脚本引用，保留 | Qt helper 不进入 PAE SDK 已由白名单保证 |
| `schema/`（4 文件） | JSON Schema、Strict JSON 与执行语义 | 必须保留并进入 source SDK | 不按旧文件名拆分重复语义 |
| `scripts/`（1 文件） | 五包生成入口 `package_sdk_stage3.ps1` | 必须保留 | 当前 provenance 文案限制已在验证报告披露，不是删除理由 |
| `docs/guides/` | 使用者入口 | 保留；本轮另有任务维护 | 避免把工程历史堆入上手指南 |
| `docs/engineering/` | 当前契约、设计、验证和未关闭风险 | 保留，优先导航整合 | 见 1.3 |
| `docs/archive/` | 已替代计划与旧清理证据 | 保留；现有 8 个文件由 archive README 明确索引 | 归档不等于可删除 |
| `spikes/json_parser/` | 根 CMake/Preset 仍接入的 Parser 选型历史及回归门禁 | 保留；不是当前删除候选 | 长期门禁迁到正式 tests 后再评估整体归档 |
| `spikes/decimal_arithmetic/` | 不由根 CMake接入的 DEC-042B 独立算术实验，仍有独有设计/复现价值 | 归档候选，不删除 | 核对生产精确算术测试覆盖全部独有不变量后再迁移 |
| `third_party/yyjson`、`third_party/qt` | 固定 Compiler/Lab 依赖及许可材料 | 整体保留，本轮不审内部冗余 | PAE SDK 白名单只携 yyjson；Qt 仅归 Lab |

仓库根只有四个工程配置文件、根 CMake/Presets 和 README，没有发现临时日志、二进制或散落配置噪音。
根目录缺少 PAE 项目自身明确 License 仍是正式分发缺口，但它不是“可删除冗余”，也不能在本轮擅自补写
许可结论。

## 3. 容易误判为重复、实际应保留的对象

### 3.1 Presets 和分片入口

`CMakePresets.json` 中相同名称分别出现在 configure/build/test preset 数组，属于 CMake Presets 的正常
跨类型关联。Windows spike、Loader、Codec、Protocol Lab、推荐 PAE+Lab、public API、H2 和两个 Linux
spike configure preset 均有 README 或工程证据引用；不能按名称重复去重。

Linux spike 仍是未在本机复核的入口，文档必须继续保持“未验证”，但未验证不等于无用。H2 preset 仍被历史
诊断报告引用且提供可复现旧配置；若未来决定只保留推荐开发入口，应先把历史复现命令固定到归档文档，再另行
调整 preset。

### 3.2 public 与内部 examples

`public_api_codec/framer/host/compile/sdk_consumer` 是外部使用边界；`ascii_text`、
`ascii_stream_framing`、`stream_framing`、`host_endpoint`、`business_embedding` 是内部能力或业务嵌入示例。
两组看似覆盖相似行为，但依赖边界和验证目的不同。现阶段应由 `examples/README.md` 导航区分，不应为了减少
目录数合并源码或删除内部示例。

### 3.3 SDK、Lab standalone 与开发树

三者是不同消费形态：开发树允许内部 target；五包 SDK 只提供 `PAE::pae`；Qt Lab standalone 在仓库外
通过 installed SDK、固定 Qt/yyjson 和白名单 Lab 源构建。相关 CMake/脚本/README 不重复承担同一职责，
不能把 standalone 合回根 CMake，也不能把 Lab 文件加入 PAE SDK 来追求单目录。

## 4. `deliverables/sdk/98df5e0` 最小归集方案

### 4.1 来源与目标

已验证来源存在于：

`F:/PersonalWorkspace/协议解析拼接工具/protocol_adapter_engine/out/sdk-clean-checkpoint/candidate1-20260919/`

建议目标为仓库工作树内、由 `.gitignore` 明确排除的本地交付目录：

`F:/PersonalWorkspace/协议解析拼接工具/protocol_adapter_engine/deliverables/sdk/98df5e0/`

静态审查时目标不存在；后续经总控单独授权，已按本节门禁原样复制并完成逐文件核验，实际证据见
[`本地 SDK 归集验证`](local-deliverables-sdk-validation-20260919.md)。没有重打包。最小归集只包含以下五个
原名目录：

1. `pae-sdk-source`
2. `pae-sdk-static-debug`
3. `pae-sdk-static-release`
4. `pae-sdk-shared-debug`
5. `pae-sdk-shared-release`

来源合计 189 个文件、29,946,570 bytes（约 28.56 MiB）。不复制 clean clone、build tree、consumer
build、验证日志、Qt Lab、旧 dirty candidate 或整个仓库。`deliverables/` 虽位于 Git 工作树内，但
`/deliverables/sdk/` 已被本地交付 ignore 规则排除；五包不应成为 `protocol_adapter_engine` 提交候选。

### 4.2 包内形态不能改变

- source 包是 81 文件的**白名单源码 SDK**，不是仓库快照。顶层只有 `CMakeLists.txt`、`cmake/`、
  `docs/`、`examples/`、`include/`、`schema/`、`src/`、`third_party/yyjson`、`LICENSES/` 和四个包元数据。
  它明确不含 `.git`、`out`、`build`、`tests`、`tools`、`spikes`、Qt 或共享树新增文档。
- static D/R 分开保存，各自包含 headers、`PAEConfig.cmake`、对应库、schema/config、consumer、license 和
  四个元数据文件；不得合并成一个多配置 lib 目录。
- shared D/R 同样分开，并各自携带对应 `bin/pae.dll`；不得用另一配置 DLL 覆盖或用外部运行目录 DLL
  回填包内容。
- 五个包内的 `PAE-SDK-README.md`、`PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS.txt` 都是身份
  的一部分，不能为了统一入口改写。统一说明若需要，应放在 `deliverables/sdk/98df5e0/` 的包外同级文件，
  但不是本次最小复制的必要条件。
- 不压缩、重命名、嵌套额外同名根或重新运行打包脚本；当前动作是 verified bytes 的本地归集，不是新候选。

### 4.3 复制前后完整性门禁

实际归集需另行执行并记录以下检查：

1. 目标根必须不存在或为空；禁止覆盖、增量合并或先删旧目录。
2. 复制前再次确认五包 `PROVENANCE.json` 均为
   `source_head=98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`、`source_worktree_dirty=false`，kind/config
   与目录名一致。
3. 对每包先验证 `MANIFEST.txt` 中的路径/长度和 `SHA256SUMS.txt` 的 184 个总条目，再复制。
4. 复制后比较 source/target 的相对文件集合、189 个文件长度和逐文件 SHA-256；仅验证包内
   `SHA256SUMS.txt` 不够，因为该文件不自包含自己的哈希。
5. 五个 `SHA256SUMS.txt` 文件自身的已复核 SHA-256 应分别为：
   - source：`9bf25c6dc043989b179a77443c5cddd099c645e4a1d422445a571ed7f12c09f6`
   - static Debug：`95ddd594f4f320430364607910435161cbf27b1f976521f4345a2920b2a02037`
   - static Release：`ccaf8417c65b5e3c31e4f957e3bea4e5831d11d707ab22c4dc55f9a15caaec0f`
   - shared Debug：`ea3d32b34f2b5cf272f1cdfaf4a382d3dc1dd33918a987d396e7b4f72c5fc35f`
   - shared Release：`e18f49aaffddac82e766ba10a5ccd530a2cda4714c6ec64490bea786f8e603b8`
6. 目标顶层应只有五个原包目录；如总控后续增加 set-level README/manifest，应独立记录其来源，不能写入
   任一包内，也不能把它计入原五包身份。

复制和哈希一致只能证明本地字节归集无漂移。功能证据仍引用
[`clean-checkpoint SDK 验证`](pae-sdk-clean-checkpoint-validation.md)；不得把复制写成重新执行六组消费、
正式发布或生产验收。原候选是否删除必须等待生成物盘点给出无重叠精确清单，并另获用户授权。

## 5. 建议的规整顺序

1. **统一本地 SDK 交付入口已建立**：已按第 4 节原样复制并核验五包；`out` 来源保持未删除。
2. **再完成导航补齐**：由文档任务把当前上手/维护路线和 engineering 历史链分层，不移动代码。
3. **再归档已替代报告**：首批候选是旧 `repository-organization-inventory.md` 和已经由实施验证完全取代的
   preflight/gap review；逐文件核对综合计划/相邻报告引用后移动，保留 Git 历史和 archive 索引。
4. **最后处理源码候选**：仅 `src/core/` 空目录可先进入用户确认清单。spikes、private/compat tools、内部
   examples 和版本兼容 tests 都有前置迁移条件，不进入近期删除批。
5. **生成物清理由另一盘点收口**：本报告只补充 `spikes/json_parser/results/generated/`；`out`、外部 clone、
   consumer、Lab 部署和旧候选的去留以独立生成物清单为准，禁止对父目录做递归通配删除。

## 6. 本轮验证与 Git 边界

- 实际读取：外层 AGENTS、当前 Git 状态/历史、根及子目录 CMake、Presets、脚本、主要 README、当前和历史
  目录盘点、SDK 五包 metadata/layout/大小及文档引用。
- 静态统计：3,072 个 tracked 文件；15 个一级 tests 目录均有 CMake 且受根入口引用；10 个 cmake helper
  均有直接引用；134 个 engineering 非索引 Markdown 中 26 个未由三个主入口直接点名。
- 未构建、未测试、未启动 Lab、未联网、未移动/删除/归档任何文件。静态审查完成后按后续授权仅原样复制
  五包到被忽略的仓库内本地交付根；复制验证不改本节其余静态结论。
- 本轮只新增本报告。未修改功能、API、CMake、脚本、Schema、Qt、第三方、其他文档或 Git 配置；未
  Stage、Commit、Push。

状态：已完成派发范围，待总控复核。任何进一步复制、归档、移动或删除动作仍需下一轮精确授权。
