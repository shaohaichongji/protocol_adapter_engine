# 仓库可移植性与本地依赖只读审计（2026-09-22）

## 1. 范围、基线与结论

本次在 `main@657d0854750b81a76c85868a6d098d0f98505b85` 上只读审计 Git 已跟踪的源码、
CMake、脚本、依赖清单、公开入口和历史记录。接管时暂存区为空；并行任务先后产生未跟踪文件
`docs/engineering/portable-workspace-plan-20260922.md` 和
`docs/engineering/portable-delivery-inventory-20260922.md`，本任务未修改或覆盖它们。除本报告外未修改
仓库文件，未构建、下载、复制依赖、清理、Stage、Commit 或 Push。

结论分层如下：

- **产品构建路径没有绑定当前机器目录。** 可选 Lab UI 默认从仓内 `third_party/qt` 解析 Qt，仍允许通过
  `PAE_QT_ROOT` 显式覆盖；yyjson 只读取仓内锁定源码并在 Configure 阶段校验长度和 SHA-256。
- **公开仓库的路径可移植性尚未闭合。** 81 个已跟踪文本文件含 1,749 处盘符绝对路径，其中绝大多数
  是历史清理证据，但当前入口、指南和验证记录也仍有机器路径；异机照抄会失败，并暴露本地布局。
- **Qt 已随 Git 提供，但发布材料尚未闭合。** 固定副本共 2,440 个文件、296,552,738 字节，清单与
  磁盘逐项一致；包内没有 Qt LICENSE/COPYING/NOTICE，来源只追溯到既有本地快照，且大量二进制含
  上游构建机路径。本审计不作许可合规结论，但该状态不能表述为已具备公开分发材料。
- **yyjson 的随仓和来源门禁闭合。** 版本、Commit、上游、归档 Hash、最小源码范围、License、长度与
  SHA-256 均有锁；本轮现场复算三个锁定上游文件全部一致。
- **source SDK 的功能依赖闭包当前静态闭合。** 打包白名单中的 51 个显式文件均存在且受 Git 跟踪，
  动态加入公开头和两个 consumer 目录；2026-09-21 的历史执行证据证明修复后 source/static 包外
  consumer 通过。本轮没有重新打包或重跑，因此只把当前状态记为“静态确认 + 历史动态证据”。

## 2. Findings

### P2-1：Qt 固定包缺少公开分发所需的来源与许可闭包

**触发条件**：把当前 Git 仓库或包含 `third_party/qt` 的派生物作为可公开获取的源码/二进制材料。

**证据**：

- `third_party/qt-package.md:3-16` 明确记录包来自既有本地快照，不声称为未修改官方归档，并承认许可
  附件、原始构建记录和最终分发审查未闭合。
- `third_party/qt-files.json` 有 2,440 个相对路径、长度和 SHA-256；本轮复算结果为
  `missing=0, extra=0, bad_bytes=0, bad_hashes=0`。这证明仓内副本自洽，不证明上游来源真实性或许可完备。
- 全仓唯一名称符合 LICENSE/COPYING/NOTICE 的文件是 `third_party/yyjson/LICENSE`；Qt 固定包没有相应
  许可附件。
- Qt 副本含 96 个 DLL/EXE/LIB/PDB 类文件，其中 9 个 PDB 合计 144,199,680 字节。ASCII 扫描发现
  78 个二进制含 `<upstream-qt-builder-root>`，41 个含上游 MSVC 安装目录；`Qt5Core.dll` 还包含编译时
  安装前缀。代表性抽取来自 `Qt5Core.dll`、`Qt5Cored.pdb`、`moc.exe` 和 `uic.exe`。

**影响**：

- 不能仅凭逐文件 Hash 宣称 Qt 包可公开再分发；缺少的是来源/许可材料与发布审查，不是运行 Hash。
- PDB 和部分 PE 文件泄露上游构建布局；编译时安装前缀也会成为 Qt 自身查找逻辑的回退信息。
- 当前 PAE CMake 显式导入仓内库、工具和 DLL，并把 platform plugin 邻接部署，因此未发现该编译前缀
  直接绑定当前开发机的构建故障；这不消除包本身的可移植性与发布材料风险。

**最小修复建议**：

1. 在公开分发前，为精确的 Qt 5.13.0 固定包补充权威来源、原始归档或安装器身份、完整 Hash、获取与
   更新方法、适用许可文本及 notice 清单；继续保留“不作法律结论”的边界，由有权人员单独审查。
2. 先判断调试 PDB 是否为 Lab 构建/调试的必需交付物；非必需则从公开依赖集分离，必需则明确其
   上游构建路径暴露边界。不要直接二进制替换字符串来伪造可移植性。
3. 任一重组都应生成新清单并重新做 Debug/Release 构建、部署来源与 smoke 验证，不能只更新 Hash。

### P2-2：当前入口和历史证据仍记录机器绝对路径

**触发条件**：在不同目录 clone 后照抄当前交付入口、指南或验证命令，或将仓库公开给不应看到本地
目录结构的读者。

**现场分类**：

| 类别 | 命中数 | 文件数 | 结论 |
| --- | ---: | ---: | --- |
| `docs/archive/**` | 1,547 | 20 | 历史清理 JSON/报告为主，属于证据数据，不宜盲目替换 |
| 当前 `docs/**` | 176 | 52 | 含当前指南、交付组织和验证记录，部分会误导异机操作 |
| `deliverables/**` | 6 | 1 | 当前统一入口直接给出本机 SDK/Lab 路径与启动命令 |
| 示例/工具 README | 10 | 3 | 既有验证记录和 standalone 说明含本机路径；另有模板占位符 |
| 构建/清理脚本 | 6 | 3 | 5 处是获批清理脚本的安全边界，1 处是生成 README 的占位符 |
| `third_party/**` 文本 | 4 | 2 | 2 处为 Qt 来源/负例记录，2 处为上游头文件路径示例 |

**关键证据**：

- `deliverables/README.md:9-14,48,58` 把当前入口写成本机候选根和仓库目录，异机不可直接使用。
- `docs/guides/02-首次运行与Lab体验.md`、`docs/guides/03-Windows-SDK集成.md`、
  `docs/guides/04-协议配置入门.md`、`docs/guides/06-构建测试与问题定位.md` 仍含本机仓库、候选包或
  工具链路径。
- `examples/getting_started/验证记录.md` 和多个 `docs/engineering/*validation*.md` 记录真实执行目录；
  这些可作为历史证据，但不应成为可复制的当前命令。
- `scripts/cleanup-generated-artifacts-20260919.ps1` 与 `scripts/cleanup-seven-builds-20260922.ps1` 的绝对路径
  是一次性、精确授权清理的防误删边界，不是通用构建依赖。直接参数化后继续执行反而会削弱原安全意图。
- `scripts/package_sdk_stage3.ps1:202`、`docs/sdk/README.md` 和 `examples/getting_started/README.md` 中的
  `<fresh-build-root>` 风格文本是人为占位符，不是当前机器依赖，应与真实路径命中区分。
- 本轮在 `tests/**` 未检出盘符绝对路径；`third_party/qt` 上游头中的盘符样例也不是本项目硬编码。

**影响**：产品库和正常 CMake 配置仍可搬迁，但“clone 后按文档执行”的体验、仓库公开边界和历史
证据可分享性未闭合。尤其 `deliverables/README.md` 是当前入口，风险高于纯历史验证报告。

**最小修复建议**：

1. 先修当前入口与使用指南：仓内对象使用相对路径；仓外 SDK、构建、工具链和证据根使用
   `<repo>`、`<build-root>`、`<package-root>`、`<toolchain-root>` 等语义别名，并说明由用户显式设置。
2. 验证报告保留真实执行事实时，把命令模板与机器证据分栏；公开版本仅保留语义别名、版本、Hash 和
   退出结果，精确本地路径留在不入库证据中。
3. 对 `docs/archive/**` 不做全局替换。若公开仓库要求零机器路径，应生成经过审查的脱敏派生证据并记录
   原证据 Hash，或把原始清理清单迁入受控非公开证据库；直接改历史 JSON 会破坏原有证据身份。
4. 两个一次性清理脚本应归档并显著标注“仅适用于原授权现场，不可复用”；是否移出公开仓库另行决定。

### P3-1：Qt 随仓提升离线可复现性，但 Git 维护成本较高

`third_party/qt` 已满足“clone 后具有 Lab Qt 输入”的目标：`cmake/PaeQt513.cmake:3-8` 默认使用仓内
相对路径，并在 `:60-146` 校验平台、架构、工具集、版本、库、DLL、工具和 platform plugin。外部覆盖
仍是显式选项，不会把某台机器路径写回源码。

代价是 296,552,738 字节、2,440 个文件直接进入普通 Git object（没有 Git LFS filter），其中 PDB 占
约 137.5 MiB。当前 Git pack 压缩后约 68.9 MiB，但每次版本替换仍可能显著增长历史。该项不是当前
构建阻断；后续若更新 Qt，应先比较“保持全包随仓”“只保留验证所需模块”“受控制品仓/Release asset”
三种路线的离线性、历史体积和来源审查成本，不在本轮擅自改变既定全包策略。

## 3. 未发现产品路径硬编码的范围

- `cmake/PaeQt513.cmake`、`cmake/DeployProtocolLabUi.cmake`、`tools/protocol_lab_ui/standalone/cmake/**`
  均从 CMake 输入或仓内相对根推导路径；`cmake_path(ABSOLUTE_PATH ...)` 是把选定输入正规化，不是
  硬编码机器目录。
- 已跟踪的 `src/**`、`include/**` 和测试源没有检出真实盘符绝对路径；命中集中在 Markdown、历史 JSON、
  一次性清理脚本和第三方包。
- Qt UI 关闭时不需要 Qt；source SDK 白名单不携带 Qt，符合“PAE 产品库非 Qt、Qt 只属于可选 Lab UI”
  的分层，而不是依赖漏包。
- URL 没有计入盘符路径。本轮使用带左边界的盘符表达式，避免把 URL scheme 尾字符误判为 drive letter；
  上游头中的路径示例、生成文档的占位符和一次性安全脚本也分别分类，未混写成运行依赖。

## 4. yyjson 随仓与完整性证据

`third_party/yyjson/dependency.lock.json` 锁定 yyjson 0.12.0 的上游仓库、Tag object、Commit、归档
SHA-256、MIT 标识、最小源码范围和更新规则。现场复算：

| 文件 | Git 跟踪 | 长度 | SHA-256 |
| --- | --- | --- | --- |
| `third_party/yyjson/src/yyjson.h` | 是 | 与锁一致 | 与锁一致 |
| `third_party/yyjson/src/yyjson.c` | 是 | 与锁一致 | 与锁一致 |
| `third_party/yyjson/LICENSE` | 是 | 与锁一致 | 与锁一致 |

`cmake/VerifyYyjsonVendor.cmake` 对锁版本、依赖名、版本/Commit、许可标识、相对安全路径、文件集合、
长度和 Hash 失败关闭；`cmake/PaeYyjson.cmake` 只消费本地文件，没有网络回退。`cmake/PaeSdkInstall.cmake`
和打包脚本均把 yyjson License 复制到安装包 `LICENSES`。这一机制证明依赖字节一致性，不是签名、
来源认证或整个 PAE 项目的许可声明。

## 5. source SDK 依赖闭包

静态核对 `scripts/package_sdk_stage3.ps1:45-130`：

- 51 个显式白名单条目全部存在且受 Git 跟踪；
- 另外动态加入 `include/pae/*`、五组公开示例目录、`examples/getting_started`、四个合成配置及 yyjson
  的源码、锁和 License；
- `src/**/CMakeLists.txt` 当前引用的 public API、Compiler、Plan、Core、Framer 源文件均在白名单；
- `cmake/PaeSdkInstall.cmake` 引用的 `examples/public_api_sdk_consumer` 原布局与包内 `examples/sdk_consumer`
  两套入口均被保留，已关闭此前 install 缺文件问题；
- source SDK 不携带 Qt，因为默认 consumer 只构建 PAE 产品库，Qt 属于仓库 Lab UI，不是产品依赖。

历史动态证据位于 `docs/engineering/sdk-onboarding-packaging-validation-20260921.md:39-54,90-130`：首次
候选暴露并修复 consumer 原布局缺口后，最终 source 包可重新 Configure/Build/Install，source/static
两类包的 getting_started 与综合 consumer 共四组 Configure/Build/Run 均退出 0。该验证输入当时是
dirty snapshot，相关改动现已进入当前 HEAD；本轮未重新执行，不能提升为当前 clean checkpoint 的新动态证明。

仍有一个与依赖闭包不同的发布边界：仓库没有项目级 PAE LICENSE，打包脚本和
`docs/sdk/README.md:92-93` 已明确 yyjson License 只适用于 yyjson。功能可构建不等于获准公开分发。

## 6. 为什么本地工具链不应直接进入 Git

Visual Studio、MSVC、Windows SDK、CRT、系统 DLL 和完整本机环境是平台工具链，不是 PAE 源码依赖。
把它们复制进 Git 会同时引入巨量历史、平台绑定、补丁失效、来源/许可审查和安全更新绕行问题，也不能
代替安装器、注册表、环境初始化及系统组件。正确的最小闭包是：

1. Git 记录经过验证的生成器、架构、toolset/SDK/Qt 版本和能力门禁；
2. CMake 从显式参数或受控环境发现本地工具链，并对版本/文件缺失失败关闭；
3. 独立文档给出安装/获取入口和离线制品身份，构建日志记录实际解析结果；
4. 只有像 yyjson 这样可审计的最小源码依赖，或经来源与许可闭合、确需离线固定的 Qt 产品依赖，才
   适合按批准范围随仓；系统工具链不因“本机已有”就复制入库。

## 7. 本轮实际检查与限制

执行了 Git 基线/状态、第三方清单、文本盘符路径、Qt 二进制可打印字符串、Git attributes、yyjson
长度/Hash、Qt 2,440 文件长度/Hash、source 白名单存在性与 CMake 引用的只读检查。没有执行构建、CTest、
Qt 程序、SDK 打包、网络访问或许可判定。

当前建议提交阻断分两类：

- 若候选目标是**公开仓库或公开发布**，P2-1 的 Qt 来源/许可附件和项目级 PAE LICENSE 状态必须先由
  有权人员闭合；P2-2 的当前入口机器路径也应先消除。
- 若目标只是**保留当前内部开发检查点**，构建链没有发现新增硬阻断，但必须保留上述公开性限制，
  不能把“清单 Hash 一致”升级为“来源、许可或跨机器发布已通过”。

状态：已完成派发范围，待总控复核。
