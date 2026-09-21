# 仓库目录职责与去留矩阵

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

日期：2026-09-14。状态：`STAGE_0 INVENTORY / NO MOVE OR DELETE`。

本文落实[公开执行、独立交付与仓库整理推进计划](../../engineering/pae-execution-delivery-organization-plan.md)的阶段 0 仓库盘点，只给出后续可执行边界，不表示目录迁移、SDK 打包或清理已经完成。现场为 `main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`，暂存区为空；既有 docs 迁移、Host/UI 和 public API 未提交变更全部保留。

## 1. 判定口径

- **保留**：当前仍有源码、构建、测试、文档或许可职责，不能因年代、未跟踪或目录名而删除。
- **待迁移/合并**：仅在后续阶段以替代能力和回归证据为前提调整；本轮不移动。
- **归档候选**：不进入产品包，但仍有工程追溯价值；归档位置和保留期需后续确定。
- **删除候选**：只表示满足后续复核的起点，不构成删除授权。不可恢复删除必须另获用户授权。

产品交付采用白名单：PAE 源码/SDK 包只纳入公开头、必要实现、构建入口、Schema、依赖与许可、最小示例；Lab、Qt、测试、spikes、`out`、Git 元数据和私有材料默认不进入包。

## 2. 目录职责与去留矩阵

| 对象 | 当前职责与现场依据 | 当前判定 | 目标/前提 | 阶段与责任 |
| --- | --- | --- | --- | --- |
| 根 `CMakeLists.txt`、`CMakePresets.json` | 统一能力门、目标接线和 Windows/Linux preset；当前直接接入 Plan、Compiler、public API、Framer、Host、Lab 与测试 | 保留；不拆根入口 | 阶段 1～3 逐步增加公开执行与安装/导出；保持旧能力门兼容 | 1～3，《子任务推进》/总控 |
| 根 README 与格式/Git 文件 | 仓库入口、风格和忽略规则；`.gitignore` 明确忽略 `/out/`、`/build/` 等生成物 | 保留 | 阶段 5 统一源码、SDK、Lab 三类入口；不把忽略等同可删 | 5，《PAE工程整理》/总控 |
| `include/pae/`、`src/public_api/`、`tests/public_api/`、`examples/public_api_compile/` | 当前未跟踪的公开编译元数据首片；根 CMake 已直接接入 `src/public_api`、测试和示例 preset | 保留在现位置，作为公开 API 纵向切片 | 阶段 1 扩展完整记录 Decode/Encode；不得恢复旧版 `compiler.h` 借用说明或删除失败示例分支 | 1，《子任务推进》 |
| `include/README.md` | 公开头边界说明 | 保留，待合并更新 | 随阶段 1～3 补齐所有权、ABI、安装树说明 | 1～3，《子任务推进》/总控 |
| `src/config_compiler/`、`src/protocol_plan/`、`src/protocol_core/`、`src/protocol_framing/`、`src/host_endpoint/` | 当前内部 Compiler、冻结 Plan、完整记录执行、Framer、Host 实现；均由根 CMake 或后续目标直接使用 | 保留；不为“更整齐”改名 | 公开层适配内部实现，内部头不进入 SDK；阶段 2 后再评估私有目标合并 | 1～3，《子任务推进》 |
| `src/README.md` | 仍只描述早期三个内部目标，未覆盖 Framer、Host 和 public API | 待合并更新，不删除 | 等公开 Codec/Host 形状稳定后更新，避免阶段 0 写入短期状态 | 5，《PAE工程整理》 |
| `src/core/` | 现场存在但为空；0 tracked、0 普通未跟踪、0 引用、0 reparse point | **删除候选，但本轮保留** | 再确认没有外部脚本依赖该物理路径并取得用户删除授权；Git 本身不跟踪空目录 | 5，《PAE工程整理》/用户决定 |
| `tools/protocol_conformance_runner/` | 协议一致性开发验证工具，由根 CMake 接入 | 保留为开发工具，不进 SDK | 公开接口稳定后评估是否改为 public API 消费者 | 1～2，《子任务推进》 |
| `tools/protocol_lab/` | CLI、Evidence/Replay/UDP 及旧 `v06_execution` 桥；根 CMake 仍构建内部目标和 CLI | 保留，部分待替代 | CLI 能力不能随桥一起删除；旧桥须在阶段 4 有等价公开 API 消费和回归后单独退役 | 4，《Lab应用推进》/总控 |
| `tools/protocol_lab/v06_execution.{h,cpp}` | `pae_protocol_lab_c1_execution_internal` 直接编译；UI `DocumentSession` include、拥有并调用 `ExecutionBridge`；C2 与 v06 专项测试也直接编译/引用 | **当前仍在用，不是可删旧文件** | public Codec/Host 支撑 Binary/ASCII Lab、旧回归有替代覆盖后，迁移调用并再列删除候选 | 4，《Lab应用推进》；公共接口由《子任务推进》 |
| `tools/protocol_lab_ascii/`、`tools/protocol_lab_binary/`、`tools/protocol_lab_ui/` | Lab 适配与 Qt UI；Binary 目录虽未跟踪，但根 CMake 和 UI target 已接入，不代表临时垃圾 | 保留为 Lab 私有层，不进 PAE SDK | 阶段 4 改为只消费公开 PAE；UI DTO/缓存仍归 Lab | 4，《Lab应用推进》 |
| `tests/` | Compiler/Core/Framer/Host/Lab/Evidence/UI 及公开 API 合同；根 CMake按能力门接入 | 全部保留 | 新公开测试与旧内部回归并存；只有替代覆盖、D/R 回归和总控复核后才合并或退役旧桥专项测试 | 1～4，各实现任务 |
| `examples/config/` | 可分发 synthetic 配置及 Values/Frame 材料 | 保留；作为源码包/SDK 示例候选 | 阶段 3 按白名单做敏感信息和引用闭合复核 | 1～3，《子任务推进》/总控 |
| `examples/public_api_compile/` | 当前公开头最小编译消费者，根 CMake/preset 已接入 | 保留 | 阶段 1 扩展为 Binary/ASCII 完整记录最小示例；阶段 3 复用为仓库外 consumer | 1～3，《子任务推进》 |
| `examples/business_embedding/`、`ascii_*`、`stream_framing/`、`host_endpoint/` | 当前内部接口的脱敏可执行示例与验证入口 | 待迁移，不先删 | 公开 Codec/Framer/Host 到位后逐个改为公开 API；新旧结果等价前保留原例 | 1～2，《子任务推进》 |
| `cmake/` | yyjson 固定依赖校验、Qt 导入/部署、UI fixture 生成；均有根或子目录 CMake 引用 | 保留 | 阶段 3 新增独立包/安装导出逻辑；Qt helper 仅服务 Lab，不进入 PAE 包 | 3，总控/《子任务推进》 |
| `schema/` | JSON Schema、Strict JSON 与执行语义；独立源码包需要携带 | 保留，进入源码包白名单 | 阶段 3 明确安装位置和版本兼容；不改协议语义 | 3，总控/《子任务推进》 |
| `docs/guides/`、`docs/engineering/`、`docs/archive/` | 已完成开发者、工程依据和历史三分区；`docs/README.md` 为总入口 | 保留 | SDK 只选必要 guides/许可说明；engineering/archive 默认不进产品包 | 3、5，《PAE工程整理》/总控 |
| `third_party/yyjson/` | Compiler 与 Protocol Lab 的固定内部 JSON 依赖；根 CMake 经 `PaeYyjson.cmake` 使用 | 保留；PAE 包依赖白名单候选 | 阶段 3 闭合源码、静态/动态链接与 License | 3，《子任务推进》/总控 |
| `third_party/qt/`、`qt-package.md`、`qt-files.json` | 用户确认的随仓 Qt 5.13.0 完整副本，当前服务 Lab；2440 tracked 文件 | 保留但排除 PAE 源码/SDK | 阶段 4/5 维持 Lab 独立依赖，不裁剪、不修改本机 Qt；是否形成单独 Lab 包另议 | 4～5，《Lab应用推进》/用户决定 |
| `spikes/json_parser/` | 三 Parser 选型历史与回归门禁；根 CMake仅在 JSON spike 能力下接入，yyjson 正式消费已转到 `third_party/yyjson` | 保留为开发证据，归档候选 | 先把仍需长期运行的 yyjson/number/corpus 门禁迁到正式 tests；再决定整个 spike 是否归档，不能直接删除 | 3～5，《子任务推进》/《PAE工程整理》 |
| `spikes/decimal_arithmetic/` | 不被根 CMake 引入的 DEC-042B 独立算术可行性实验，仍是历史设计证据 | 归档候选，不删除 | 确认生产精确算术测试已覆盖其独有不变量和复现价值，再迁入 archive 或保留 | 5，《PAE工程整理》/总控 |
| `.git/` | Git 元数据 | 严格保留且不纳入盘点操作 | 产品包明确排除 | 全阶段，禁止操作 |

## 3. `out`：可再生成物与唯一证据混合区

`out` 被 `.gitignore:1` 整体忽略，但“ignored”只说明不进入 Git，不说明全部可删除。本轮以不跟随 reparse point 的遍历方式统计：

| 分组 | 现场规模 | 判定 |
| --- | ---: | --- |
| `out/build/` | 5 个直接构建目录，13,409 文件，4,185,835,716 bytes | CMake 产物原则上可再生成，但当前均有 `CMakeCache.txt` 且源目录指向本仓；同时承载当前复核/启动状态，阶段完成前保留 |
| `out/` 其余 81 个直接目录 | 47,082 文件，6,314,558,514 bytes | 混合构建目录、Evidence Bundle、人工/自动验收输入输出和复现材料；不能整体归类为可再生成 |
| `out/` 根文件 | 598 个，15,984,758 bytes；其中 594 个 `.log`、3 个 `.txt`、1 个 `.diff` | 多数是唯一执行记录或中间证据；未逐项闭合前保留 |

当前五个 `out/build` 对象及停点：

- `windows-msvc-public-api-stage1`、`windows-msvc-public-api-stage1-lab-compat`：当前公开编译首片与兼容验证构建，至少保留到阶段 1 总控收口。
- `windows-msvc-pae-lab`、`windows-msvc-binary-ui-stage1`：当前 Lab 主入口/专项构建，至少保留到阶段 4 接线与人工烟测收口。
- `windows-msvc-protocol-lab`：旧 CLI 构建仍有复现价值，且内部含 4 个 reparse point，不能递归批量删除。

全 `out` 现场共发现 27 个 reparse point（上述 build 内 4 个，其他目录 23 个）和 4 个嵌套 `.git` 目录（均位于 `out/yv-eol/*/.git`）。本轮只记录链接本身，未遍历链接到仓库外。任何后续清理必须重新检查活动进程、源目录、Git/ignore、唯一日志/Bundle、reparse 最终目标及嵌套仓库，并使用逐对象 allowlist；不得对 `out` 根执行通配符或递归整体删除。

## 4. 分阶段可执行清单

### 当前可整理但本轮不实施

1. 保持 `include/pae + src/public_api + tests/public_api + examples/public_api_compile` 的阶段 1 纵向结构，不再创建第二套 public 目录。
2. 后续只随接口切片同步更新对应 README、示例和测试入口；不提前搬内部模块。
3. SDK/源码包从白名单生成，避免靠复制整个仓库再排除 Lab/Qt/tests/spikes/out。

### 待替代后处理

1. 阶段 1～2：用公开 Codec/Framer/Host 迁移内部示例和 conformance runner；旧入口保留到等价回归通过。
2. 阶段 4：Lab 先 Binary、后 ASCII/流式改为公开 PAE 消费者；`v06_execution` 及其专项测试到此时才具备退役评估条件。
3. 阶段 3～5：把需要长期保留的 spike 门禁迁入正式 tests，再决定 spikes 的归档方式；更新 `src/README.md` 等过时导航。

### 必须另获用户删除授权

1. 当前空且无引用的 `src/core/`。
2. 被公开 API 完全替代后的 `v06_execution.{h,cpp}` 及确认无独有覆盖的旧桥测试。
3. `out` 中逐项证明可再生成或完成证据去留审查的精确对象；含 reparse point/嵌套 `.git` 的对象单独处理。
4. 完成正式测试迁移和独有证据审查后的 spike 文件。归档不等于删除，归档位置仍需确定。

目前仓库根没有 PAE 自身的明确 License 文件，根 CMake 也没有 `install()`/`export()` 交付规则；这是阶段 3 的交付缺口，不是目录清理理由。许可证选择和正式发布位置仍由用户决定。

## 5. 本轮验证与限制

- 读取根/子目录 CMake、README、Git tracked/untracked/ignore 状态及引用；确认 `src/core` 空目录事实和旧桥实际编译/调用链。
- `out` 统计跳过 reparse point 目标，未进入 `.git` 内部；统计的是逻辑字节，不是磁盘实际释放量。
- 未构建、未测试、未启动 Lab、未联网，也未验证仓库外源码/静态/动态消费。
- 未移动或删除任何文件，未改代码、构建、Schema、其他文档或索引；未 Stage、Commit、Push。
