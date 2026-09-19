# 公开执行、独立交付与仓库整理推进计划

2026-09-14：用户授权综合规划并派发任务。总边界沿用 [PAE / Lab 职责与独立交付边界](pae-lab-delivery-boundary.md)。
公开编译与基础元数据首片已完成限定总控复核；这不代表完整 SDK、DLL 或生产验证。既有工作树变更全部保留，不 Stage/Commit/Push。

## 顺序与出口

### 当前授权：整理检查点与 Lab 图形能力覆盖（2026-09-19）

用户授权修正残留状态文字并提交本轮整理检查点，不推送；下一步推进 PAE 现有公开能力的图形入口，用户自行开展真实使用验证。先由 Lab 盘点 UI 覆盖与分片方案、PAE 并行核对公开能力/观察接口支持域，再由总控收敛实施顺序。只读盘点先行，不把“全能力入口”解释为新增协议、通信框架、私有API回读或稳定ABI承诺。新实现与用户正在使用的98df5e0交付目录隔离，不覆盖其测试输入和部署；验证以定向自动化为主，新增人工操作仅在必要时简短说明。此前无Git授权表述均为各历史切片边界。

### 当前实施：人读指南中文编号与阅读主线（2026-09-19）

最新回收：六篇01～06中文指南、专项验收分区、五张Mermaid及旧新映射已完成；工程索引按主题组织，相关Markdown链接已同步。总控已通读六篇，修正05依赖箭头、补齐02完整Binary报文/操作/预期结果及06工作目录；源码include方向与现行路径已抽查，diff --check通过。Mermaid仅静态检查，未渲染；未重跑构建或UI。执行任务停止，未Stage/Commit/Push。以下为派发过程。

用户授权整理 docs 内容、中文顺序命名及 Mermaid 图。《PAE工程整理》独占 guides 主线重组，计划为项目定位、首次运行/Lab、Windows SDK、配置入门、架构代码阅读、构建排错六篇；保留专项验收状态，工程契约/历史证据不批量改名。同步 docs/根/交付/standalone 导航与 Markdown 引用，登记迁移映射；总控维护本计划和外层 AGENTS。Lab/PAE任务停止。本片仅文档，源码事实静态核对及链接/差异检查，不构建、安装、启动Lab或修改包内产物，无Stage/Commit/Push授权。上一轮全部未提交变更保留，完成后总控复核；派发不代表内容已完成。

### 当前实施：产物盘点、仓库规整与真实使用入口（2026-09-19）

最新收口：工程整理已完成获批99根清理，逻辑长度35,999,609,815 bytes（33.527 GiB），无跳过或未解决删除失败。总控独立核对99根均不存在、7,912归集证据哈希零差异、6,963保护文件长度/哈希零差异；Git无跟踪文件删除、暂存为空，diff --check通过。后置只读检查曾因空比较结果.Count异常，已修复并恢复复核通过，详见[执行记录](generated-artifact-cleanup-execution-20260919.md)。SDK/Lab统一交付与上手入口保持；未构建/测试/提交/推送。执行任务停止。以下为本轮历史派发过程，不代表尚待删除。

清理实施授权：用户确认补强后的第5/7节清单，工程整理独占先归集哈希校验再精确删除；其他任务停止。允许原盘点、generated-artifact-cleanup-execution-20260919.md及必要的固定白名单清理脚本；证据保存deliverables/evidence新目录。保护deliverables、当前原SDK/证据、当前clean Lab部署/日志/INPUT清单、源码/依赖/本机环境；链接只在获批根内非递归移除本体。未知内容跳过，不追加src/core或spike候选。未执行完前33.522GiB仅上限，无Git写/构建/发布授权，完成总控复核实际删除与保留项。

阶段回收：SDK189文件及Lab static/shared Release的17/18文件已归集至仓库内deliverables，总控独立重算源/目标SHA256零差异，并读取Lab新位置4/5模块来源证据；未重跑功能测试。首次使用与代码阅读指南已复核小修，现行根README/SDK指南/standalone入口已统一路径。仓库审查未确认可直接删的跟踪源码/测试；本片不移动src/tools。产物盘点33.895GiB，清理估计上限33.522GiB尚非批准量；已要求工程整理补核构建内独有日志及27重解析点，删除需先归集证据再获精确清单授权。所有原产物保留，无Stage/Commit/Push。

前一批10文档已本地提交 `0b89bed`，未Push。用户要求减少工作区/out产物占用、全面审查仓库冗余、补充维护者和使用者上手文档，并集中可用交付物。本轮不扩协议功能，不增加复杂人工验收。

- 《PAE工程整理》独占 `generated-artifact-cleanup-inventory-20260919.md`：盘点外部PAE生成根和out的大小、来源、保留/删除候选；不触碰无关项目、不删除。
- 《子任务推进》独占 `repository-organization-audit-20260919.md`：核对目录职能、构建引用和冗余；只提出归档/删除候选，不移动实现。
- 《Lab应用推进》独占 `docs/guides/01-项目定位与能力边界.md`、`docs/guides/02-首次运行与Lab体验.md`、`docs/guides/04-协议配置入门.md`、`docs/guides/05-架构与代码阅读.md` 和 docs/guides、docs 两README：写短流程上手及代码阅读路线，只静态核对，不构建启动。
- 总控负责本计划、外层AGENTS及后续统一 `deliverables/` 本地归集方案。SDK保持98df5e0原五包身份，Lab保留static/shared Release必要运行闭包；不把复制升级为重新验证或正式发布。先回收复制要求和磁盘盘点，再执行精确归集与内容核验，最后提出无重叠精确删除清单请用户确认。

三个任务共享工作树但写入范围互斥；无Stage/Commit/Push/发布授权，不改本机Qt或全局PATH、不重打SDK、不删除原产物。完成均向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 反馈一次并停止。当前为启动，不代表规整或清理已完成。

### 当前派发：同批 SDK/Lab 使用入口最终同步（2026-09-19）

最终回收：六入口文档已完成，总控核对实际差异、新候选/产品路径与standalone依赖参数，EXE/configs/plugin路径存在，git diff --check通过。未改功能或重跑测试；standalone README变化已明确与旧验证快照区分。执行任务停止，本批候选为7份修改及3份新增Markdown，共10文件，等待用户确认提交信息后再Stage/Commit，Push另行授权；外层AGENTS不纳入仓库提交。

用户同意收尾。《PAE工程整理》独占 README.md、docs/README.md、docs/guides/README.md、docs/guides/03-Windows-SDK集成.md、docs/engineering/README.md、tools/protocol_lab_ui/standalone/README.md，在已有入口修改上增量同步98df5e0干净五包及新static/shared Lab实际部署路径。旧dirty候选仅保留历史身份；明确本地使用非正式发布，不能改历史报告或宣称共享树clean。只检查路径/链接/参数与文档差异，不运行构建/启动/测试命令，不改脚本/代码/依赖或复制删除产物。

Lab/PAE停止，总控维护本计划及外层AGENTS。完成向总控01a04601-757d-7bb1-8254-61dde4954d74反馈一次并停止；总控复核后整理本轮文档提交候选，未获新Stage/Commit/Push授权。入口README变化不影响既有功能证据，但不能再宣称修改后的完整白名单与旧快照逐字相同；记录文档差异，不因此重跑功能验证。

### 当前实施：Lab 消费同批干净 SDK（2026-09-19）

限定收口：总控已读取本片报告、四组CTEST各32项全通过记录、产品隔离审计及模块来源JSON，并现场重算四套部署18个模块的输入/部署/已加载路径文件哈希一致；读取99文件快照一致性证据，未重跑构建测试。98df5e0同批干净SDK与static/shared Qt Lab本地消费链路已限定闭合，执行任务停止，无新人工验收。保留初次路径参数错误记录、历史AV/focus-out、C4251/稳定ABI、Linux和许可/正式发布边界。下一步为使用入口最终同步与文档检查点整理，尚未派发，无新Stage/Commit/Push/发布/删除授权。证据见lab-clean-sdk-consumption-validation.md。

用户授权下一步派发。《Lab应用推进》独占验证，PAE/工程整理停止。输入为out/sdk-clean-checkpoint/candidate1-20260919的static/shared D/R四包，均98df5e0/dirty=false；Lab功能源码和standalone脚本须核对与98df5e0一致，保留共享树文档修改，不将共享树整体标为clean。固定随仓Qt/yyjson，只复制现有获批依赖，不修改本机环境或依赖。

新包外根建议F:/PersonalWorkspace/pae-lab-clean-sdk-static-20260919和pae-lab-clean-sdk-shared-20260919，先检查不存在，冲突唯一后缀，禁止覆盖旧r7/r8/r2及本轮SDK。使用现有准备/部署/模块来源脚本，static→shared，各D/R Testing-on既有32项、Testing-off产品构建及隔离核对严格串行；Release断言须生效。新部署的Qt DLL/plugin及shared PAE DLL实际加载路径和SDK/依赖输入→部署→加载哈希需取得证据，static依赖无pae.dll。未改Qt/yyjson校验逻辑可引用原负例；针对新SDK验证包类型/D-R错配、缺/错PAE DLL拒绝，用新派生输入，不删除或替换原件。普通SDK与Lab的门禁分开叙述。

唯一共享仓库可写报告docs/engineering/lab-clean-sdk-consumption-validation.md；其余仅新包外构建/部署/日志，禁止改代码、CMake、脚本、SDK或其他文档，缺口停报。standalone README的旧dirty说明不作为新SDK事实，以manifest/provenance及报告为准，后续入口统一更新。历史AV/focus-out若再现保留日志停报，不反复刷绿或顺带修复。无复杂人工验收、Stage/Commit/Push/正式发布/删除授权。完成后向总控01a04601-757d-7bb1-8254-61dde4954d74主动反馈一次完整摘要，停止写入待复核；不得自行扩大能力或发布声明。

### 当前实施：干净检查点 SDK 五包与包外消费（2026-09-19）

限定回收：总控已读取验证报告、六组消费结果及来源汇总、D/R错配和缺DLL拒绝日志，现场确认detached源码HEAD与空status，独立复算五包184条SHA256全部匹配且provenance同为98df5e0/dirty=false。未重复构建运行。本片本地SDK候选限定收口；短生命周期consumer没有运行中模块路径快照，普通SDK也没有DLL内容哈希拒绝门禁，不能借用Lab机制扩大结论。执行任务停止，下一片为Lab消费同批SDK，尚未派发；无新Git/发布/删除授权。报告见pae-sdk-clean-checkpoint-validation.md。

用户同意 detached 本地 clone 方案并授权推进。《子任务推进》独占执行：精确提交 98df5e0d844413fb6ad16a75dfceedcf17f2f1d6，创建全新本地 detached clone，核验 HEAD 和空 status；构建、打包输出均在其外，不复制共享树未提交文档。沿用打包脚本，保留固定 snapshot_note 并在验证报告解释其通用文案与 dirty=false 机器事实，不修改脚本或元数据冒充新实现。

建议源根 F:/PersonalWorkspace/pae-clean-checkpoint-98df5e0-source；候选、构建、证据及外部消费根沿用 pae-sdk-clean-checkpoint-preflight.md 第4节建议，均先检查不存在，冲突使用唯一后缀，严禁覆盖旧根。允许本地 clone/checkout、五包生成及源码/static/shared 六组 D/R 串行验证、哈希/来源/符号/运行 DLL/错配负例；不改公共API/Core/Qt/功能代码。负例须核对实际门禁，普通 SDK 无对应拒绝机制时据实记录缺口，不把 Lab 专有校验当 SDK 能力，不自行扩代码。

唯一仓库报告 docs/engineering/pae-sdk-clean-checkpoint-validation.md；忽略的 out 中允许本片新产物和日志。总控维护计划及外层 AGENTS，工程整理/Lab 停止。完成向总控 01a04601-757d-7bb1-8254-61dde4954d74 主动反馈一次，停止写入待复核。新 SDK 通过后再派 Lab 消费同批 SDK，本次不含该后片；无 Stage/Commit/Push/正式发布/删除授权，不新增人工验收，不改变系统环境，不宣称稳定 ABI/Linux/生产可用。

### 当前派发：提交后入口同步与新 SDK 范围核对（2026-09-19）

用户授权规划并派发。基线 main@98df5e0，62 文件检查点已本地提交、未 Push；派发前工作树干净。以下旧实施段为历史证据。本批仅文档入口更新及打包前静态核对，不生成 SDK、不构建测试、不改功能、不发布或删除，无新 Stage/Commit/Push 授权。

- 《PAE工程整理》独占 README.md、docs/README.md、docs/guides/README.md、docs/guides/03-Windows-SDK集成.md、docs/engineering/README.md：区分开发树、现有候选 SDK 和 standalone Lab 入口，纠正落后的迁移状态，不改历史验证结论，不虚构新包或统一部署已生成。
- 《子任务推进》仅写 docs/engineering/pae-sdk-clean-checkpoint-preflight.md：只读核对五包打包入口、源码白名单、当前提交与旧候选功能输入差异、工具链及最小验证矩阵，提出从干净提交导出构建输入的方案。共享树将有本批文档改动，禁止把 dirty 工作树改标 clean，也不得为打包擅自提交或创建 worktree。本批不复制、构建、打包或修改脚本。
- 两项并行，文件写入互斥；《Lab应用推进》保持停止。总控独占本计划和外层 AGENTS.md。两项完成均主动向总控 01a04601-757d-7bb1-8254-61dde4954d74 反馈一次交接摘要并停止写入，待复核。

回收后再确定干净源码身份及新目录，串行执行新 SDK 五包/包外消费 → Lab 使用同批 SDK → 入口最终定稿。此次派发不授权后续实施；不新增复杂人工验收，不扩 Binary/TLV/Linux 或稳定 ABI 承诺，既有 AV、focus-out 和 Qt 外发许可边界保留。

### 当前实施：Qt Lab installed-SDK shared D/R（2026-09-19）

最终限定收口：总控已核对standalone包类型/同源runtime/manifest哈希门禁与部署及启动前校验源码，读取D/R各32项结果、Testing-off隔离/运行哈希记录、实际PAE/Qt模块来源、错DLL拒绝日志和static Debug dumpbin回归；当前99文件与r2快照零差异证据已读，未重复构建测试。本地static/shared Qt Lab安装SDK消费链路限定闭合，执行任务停止，无新人工验收。C4251/稳定ABI、dirty SDK、Qt正式分发许可、Linux、历史AV和focus-out观察边界不变。下一步建议整理累积变更与提交候选清单，另行取得Git授权，不自动重打包/发布/删除。

用户授权下一步，《Lab应用推进》独占；PAE/工程整理停止。main@c5b3692加全部既有未提交实现保留。以已限定收口的static功能源为基线，仅补shared安装SDK消费与运行闭包，不改协议执行/展示行为。允许tools/protocol_lab_ui/standalone内CMake、输入准备/部署/模块取证脚本、README及必要专用门禁测试；报告独占docs/engineering/lab-sdk-standalone-shared-validation.md，总控计划/AGENTS由总控维护。其他源码或测试需改先停报，不改PAE公共API/Core、根构建、third_party及旧部署，无Git/发布/删除授权。

使用既有candidate1-20260918的shared Debug/Release包，保持dbf4798 dirty provenance；不重打SDK。新仓库外短根建议F:/PersonalWorkspace/pae-lab-sdk-shared-20260919，先检查不存在，冲突唯一后缀；白名单复制Lab/配置/固定Qt/yyjson及SDK，不复制PAE开发src、不覆盖static证据根。准备入口新增显式包类型选择，原static行为保留，禁止shared失败后static fallback。

通过PAE::pae imported target解析匹配配置的import library和runtime DLL，规范化并验证均属于当次SDK根；部署匹配pae.dll到新有界目录。测试可执行文件也须获得明确同源运行DLL，不依赖开发目录或系统PATH。串行shared D/R Testing-on/off：复用现有32项功能矩阵、有效Release断言、产品无注入/私有桥；核对dumpbin依赖、实际加载PAE与Qt DLL/plugin路径及输入/部署/加载文件哈希。记录进程局部环境与源码快照一致性，不改全局环境，仅结束本轮启动进程。

负例包括缺/错SDK根、D/R包错配、runtime缺失及用另一配置DLL替换的来源/哈希拒绝；用全新替代输入/部署，不删除或覆盖原件。D/R DLL不保证由Windows loader自动拒绝，必须由配置/来源/哈希门禁拒绝，不把能加载等同兼容。既有Qt/yyjson门禁未改可引用static证据；构建/部署共享逻辑发生变化时做最小static回归，不重跑完整static矩阵。历史AV或Debug0.8焦点偶发留证停报，不反复刷绿或顺带修复。

完成向总控01a04601-757d-7bb1-8254-61dde4954d74反馈一次完整交接摘要，停止写入待复核。不新增人工验收，不正式分发，不宣称clean release、稳定ABI、Linux或长期稳定性；无Stage/Commit/Push/发布/删除授权。

### 当前实施：Qt Lab installed-SDK static D/R 首片（2026-09-19）

最终限定收口：总控已读取D/R Testing-off模块来源JSON及采集脚本，确认Core/Gui/Widgets/qwindows实际路径均在各自新部署根，加载文件/部署/Qt输入三方哈希一致；环境仅部署PATH前置和进程局部Qt插件路径，不宣称完全清空系统环境。当前仓库97个可映射白名单文件与r8 manifest零差异证据已读，两份新增核对脚本不进入产品。结合前次r8 D/R各32项与r7不变产品证据，本地static包外构建首片限定收口，未重复构建测试、无人工验收。Debug0.8 focus-out及历史AV保持未解决观察边界。执行任务停止；下一片shared D/R及PAE DLL部署/加载来源尚未派发，无Stage/Commit/Push/发布/删除授权。

总控回收：已读取standalone目标/来源门禁/部署脚本及验证报告，核对r7/r8输入清单仅description_mapping_tests.cpp不同，读取r8 D/R 32项结果；未重复测试。暂不收口：报告称受控Qt smoke，但现有smoke日志仅功能输出，需补Testing-off D/R实际加载Core/Gui/Widgets及qwindows模块路径和与输入哈希一致性证据，同时记录进程局部环境。仅来源核对，不重跑32项、不改产品代码、不修Debug 0.8偶发focus-out；该观察项保持未解决。补齐后再限定总控复核，shared未派发。

用户确认上述边界，授权《Lab应用推进》独占实施。精确源码/功能/输入清单沿用 lab-sdk-standalone-implementation-scope.md 第2–8节，仅执行 static 首片；PAE/工程整理停止。基线 main@c5b3692 加既有未提交切片，全部保留。yyjson 方案A批准：Lab显式消费现有锁定0.12.0源文件、LICENSE和lock，由独立Lab vendor target编译；不得借用PAE私有target、下载升级或新增PAE API。“不重复编译”指不重复编译协议配置及不重复建立Lab vendor target，不要求修改已打包PAE内部yyjson实现。

允许报告第4节范围的兼容声明/类型隔离、standalone CMake与局部目标接线、必要公共展示helper及测试；不得复制整套session状态机。保留开发树兼容和全部第2节功能，standalone显式选择已迁移public路径，不改旧入口默认开关。root CMake、include/pae、src、SDK打包脚本及既有Qt/yyjson源不改，越界停报。允许在standalone下提供白名单输入准备/验证脚本及README，所有脚本文件编辑用apply_patch；不在脚本中删除或清空既有目录。

批准仅为本地closure验证复制现有dirty-provenance SDK、固定Qt及获批yyjson/许可证到新的仓库外验证根。建议短根 F:/PersonalWorkspace/pae-lab-sdk-static-20260919，先验证不存在，冲突时唯一后缀，禁止覆盖旧根；输入只复制白名单，不带PAE开发src，不裁改源Qt，不正式分发。记录各类输入清单/哈希、SDK原provenance和Lab未提交源码身份，不伪称clean release。配置/测试fixture缺白名单项可按现有用例补齐并列明，禁止为通过而减少功能。

串行static Debug/Release，每配置分Testing-on/off目录；执行报告第8.1节适用功能测试/自动Qt smoke、有效Release断言、Testing-off无注入/测试宏检查。检查真实编译与链接输入来源，只通过PAE::pae消费SDK；PAE静态包内部依赖由导出target提供不等于Lab引用开发私有桥。缺/错SDK、D/R错配、Qt输入缺失/错版、yyjson锁定输入不符须fail-closed；用替代测试输入模拟缺失，不删除原输入。部署仅新有界目录，受控环境验证Qt模块/plugin来源，保留失败日志；历史AV再现停报，不无限复跑。

报告独占 docs/engineering/lab-sdk-standalone-validation.md；不改总控计划/AGENTS。完成static验证后向总控01a04601-757d-7bb1-8254-61dde4954d74主动反馈一次文件/命令/结果/来源/风险/Git状态，停止写入待复核。shared另派，不重打SDK、不新增人工验收、不覆盖旧部署、不改本机环境，无Stage/Commit/Push/发布/删除授权。Qt正式外发许可与Linux/稳定ABI不在本片。

### 当前准备：整个 Qt Lab installed-SDK 独立构建（2026-09-19）

用户同意开始下一步。先由《Lab应用推进》独占完成实施前的精确源码白名单/功能保留/剩余兼容声明处置核对，仅写 docs/engineering/lab-sdk-standalone-implementation-scope.md，不修改代码/CMake、不构建测试、不复制依赖。基线 main@c5b3692 加已复核未提交切片，全部保留；PAE/工程整理停止。此准备避免把旧预检中的已解决依赖当作当前事实。

最终目标仍为仅 Lab 源码、安装 PAE SDK 和固定 Qt 的真实仓库外构建，先 static D/R，复核后 shared D/R；不移除 0.5–0.8 complete、0.9 Decode、0.10 complete、0.11 stream/Host 或原有交互以取得通过。当前 schema_dispatch.cpp 直接依赖 yyjson，依赖归属尚待用户确认：建议 Lab 显式消费仓库已有版本及许可证，独立于 PAE 私有 target，不新增 PAE API/重复编译/新版本下载。确认前不实施该选择。

核对报告应列出精确允许编辑文件、public-only 编译时需排除的旧声明/源与仍保留的开发兼容路径、最小功能验证矩阵、包外输入清单、既有 SDK 候选及 dirty provenance、Qt/helper/fixture 来源、安全新目录、禁止私有源码 fallback 与配置错配负例。不能宣称尚未运行的构建已通过。完成后向总控 01a04601-757d-7bb1-8254-61dde4954d74 主动反馈一次，停止写入等待实施派发。无 Stage/Commit/Push/发布/删除授权；不覆盖旧部署、不修改系统 Qt。

### 当前实施：ASCII 私有兼容闭包隔离（2026-09-19）

最终限定收口：总控已读取增强后的公开编译→CreatePublicDirect→Inspect 测试、D/R各1/1日志及实际链接输入，确认调用了 façade 非内联实现，且不链接 headless/旧 ASCII adapter/private compatibility；Release /UNDEBUG 证据已核对。结合前次实现与4项回归复核，本片限定收口，未重复测试，不追加人工验收。执行任务停止；整个 Qt Lab SDK 包外构建仍未完成，下一片为独立构建入口与 static/shared D/R 消费验证，尚未派发。既有兼容代码、默认开关、旧部署保留，无 Stage/Commit/Push/发布/删除授权。

总控回收：已读取公开 façade/backend、私有转换边界、CMake 与 D/R 各4/4、兼容 Debug 各2/2 日志，未重复运行。暂不收口：独立 façade 测试当前仅使用 DTO/状态名，不能保证静态链接器拉入 ascii_host_adapter.cpp；限定补真实公开编译、CreatePublicDirect 与 Decode 调用，检查实际 executable 链接输入不含旧 adapter/private compatibility。仅该专项 D/R，不重复全矩阵或人工验收，报告补充证据后停止待复核。

用户同意下一步，《Lab应用推进》独占实施；PAE/工程整理保持停止。基线 main@c5b3692 加现有未提交 DTO 与 legacy complete 迁移，全部保留。本片为整个 Qt Lab 包外构建的前置，不提前宣称整体 public-only，不执行 Stage/Commit/Push/发布/删除。

目标：解除 Qt ASCII 公开 façade 对旧 host_observer_adapter DTO、私有 Action 与兼容实现的传递依赖。为 binding/identity/operation/result/stream observation 建立最小 Lab-owned 类型，或复用已存在的公开适配自有类型；禁止仅 alias 私有类型。公开路径只消费 public ASCII adapter 与公开 PAE，私有路径在独立兼容源文件/目标边界转换。保留现有兼容功能及门禁组合，不删除旧 targets，不靠禁用既有功能取得通过。0.10/0.11 的单编译、同源 owner、调用次数、预算、Flow 草稿/结果隔离、STOP/Continue/Reset/冻结后缀及失败清旧语义不变，不建立第二套流状态。

允许 tools/protocol_lab_ui 的 ascii_host_adapter、自有 ASCII DTO/兼容源文件、必要 document_session/document_tab/compile_worker/description_mapping 类型边界转换及局部 CMake；tests/protocol_lab_ui 相应测试和局部注册。根 CMake 仅确有必要的兼容隔离接线，不改现有默认开关或重构构建。非 Qt public ASCII adapter 执行算法、include/pae、src、CLI/Evidence/Replay/UDP、Binary 能力、旧部署不改。若存在公开事实缺口、需改变执行语义或超范围修改，保留证据停报。报告独占 docs/engineering/lab-ascii-compat-isolation-validation.md，总控维护计划/索引。

先列实际依赖闭包与等价性断言，再实施。新目录 out/build/windows-msvc-lab-ascii-compat-isolation（占用则唯一后缀），证据 out/lab-ascii-compat-isolation。串行 Debug/Release 定向验证 A2/0.11、mixed Pipeline、Flow、失败清旧及所有权/预算；复用原测试，仅补本片改变的类型/调用路径断言。提供公开 façade 头与源 target 不依赖 private PAE/旧 ASCII adapter 的独立编译/链接证据，不能以整个 headless target 仍链接私有库冒充隔离完成。保留兼容门禁关闭/混合组合的针对性构建证据；Release 断言有效，新增测试注入不得进入 Testing-off 产品。旧 legacy/其他 headless 私有闭包仍可后置并明确报告。

不重打 SDK、不覆盖已验收部署、不要求新人工验收、不全仓重复刷绿；历史 Qt 访问违例再现则留证停报。完成后向总控 01a04601-757d-7bb1-8254-61dde4954d74 主动反馈一次完整交接摘要，停止写入等待复核。复核后再另派整个 Qt Lab 独立构建与 static/shared D/R 消费验证。

### 当前实施：0.5–0.8 complete-record 公开消费迁移（2026-09-19）

最终限定收口：总控已核对故障注入声明/定义/全局与实例状态/触发分支的测试门禁，PUBLIC 宏保证目标与消费者布局一致；最终4份源码哈希吻合，D/R专项各1/1、Testing-off Release UI构建及测试宏/注入符号零匹配证据已读取，未重复运行。结合前次预算返修D/R各13/13回归，本片限定收口，不追加人工验收。执行任务停止；门禁仍默认OFF，旧部署不变，不等于默认产品已切换或整个Qt Lab包外构建完成。下一片建议隔离ASCII私有兼容类型/源依赖，再独立构建；尚未派发。无Stage/Commit/Push/发布/删除授权。下方为返修过程。

预算返修复核：三项主体修正已核对源码、D/R专项1/1及回归13/13日志和源码哈希；Codec零额度探测当前在工作区分配前返回计费，属于当前实现依据，不升级为稳定ABI承诺。收口前再限定隔离新增故障注入：ForTesting方法/全局状态/实例状态/触发分支仅允许Testing构建，检查宏与类布局一致。仅D/R专项及Testing-off Release产品验证，不重复全矩阵；尚未收口。

总控回收：已读新 adapter 完整实现及报告，暂不收口。定向返修三项：描述先复制后预算拒绝；结果 BYTES 预检少计两份 hex 文本、失败结果未统一总额限制；noexcept 准备函数 catch 中动态诊断再次分配风险。原任务限定 adapter/测试/报告修正，必要 session 错误映射；先补行为失败断言，D/R受影响回归，新 budget-fix 证据保留旧日志。实例/Codec 预检能力按公开API事实报告，不扩大为RSS保证。无新Git/发布授权，不进入下一片或人工验收。

用户同意下一步，授权《Lab应用推进》独占迁移；基线 main@c5b3692 加已复核未提交的 owned DTO 首片，全部保留。PAE/工程整理停止。先核对公开 metadata/Codec 与旧 UI 行为等价清单，再实施；若存在公开事实缺口，保留证据停报，不猜值、不扩 PAE API、不双编译或失败后回退私有路径。

目标：Qt Lab 0.5–0.8 首次加载/重载单次公开编译，同源 compiled owner 生成 Lab-owned 描述并创建 complete Codec。保持 Pipeline 限定 Decode、消息身份、精确 raw/logical/物理范围、typed Encode、constant/computed、变长/长度/CRC/Decimal64、成功 Encode 后既有独立 Decode review（明确与 Codec 内部 final review 区分）；不为展示增加其他执行。借用结果在失效前有界复制，失败清旧成功，owner/工作区/结果内存按现有预算检查，逻辑预算不代表 RSS。保持异步 load/reload/close 身份与发布事务，不给 legacy 增加 Host/stream，不改变 0.9/0.10/0.11 路径或 CLI。

允许 tools/protocol_lab_ui 内新增 public complete adapter/描述映射，以及 schema_dispatch、compile_worker、document_session 和必要展示错误映射；复用 owned DTO，测试在 tests/protocol_lab_ui，局部 CMake 必要接线。根 CMake 仅必要默认 OFF 的迁移门禁与公开目标依赖，禁止重构根构建或移除旧 target。不开该门禁仍保留旧兼容；开启时 0.5–0.8 不调用 private compiler/Core。不得修改 include/pae、src、tools/protocol_lab CLI/旧桥、其他 adapter 执行实现或已验收部署。独占报告 docs/engineering/lab-public-legacy-complete-validation.md；总控维护计划/索引。

新独立 out/build/windows-msvc-lab-public-legacy-complete（占用则唯一后缀），证据 out/lab-public-legacy-complete。先添加有意义的行为/调用路径断言，再迁移；串行 D/R 定向覆盖0.5–0.8正常编解码、失败/清旧、转换边界、长度/完整性、消息/Pipeline限制、review与所有权/预算；受影响load/close及H2/A2/0.11定向回归。Release断言有效，检查默认OFF及Testing-off；不重复全仓、SDK五包或六组包消费。可用自动Qt smoke，不覆盖部署、不先要求人工操作；交付后总控决定是否需要最多一组人工抽查。若历史访问违例再现留证停报，不无限复跑。

完成向总控01a04601-757d-7bb1-8254-61dde4954d74主动反馈一次交接，停止写入待复核。无Stage/Commit/Push/发布/删除授权。本片不代表整个Qt Lab已包外解耦；ASCII兼容闭包与独立构建仍后置。

### 当前实施：Lab 自有展示类型与输入工具首片（2026-09-19）

最新回收：执行任务已停止，总控完成限定源码差异与日志复核，未重复构建测试。已核对 owned DTO/Decimal64/整数解析实现、私有/public 映射、旧执行边界显式转换、局部 target 与 Release 断言设置；D/R 各 9 项 headless、8 项 Qt smoke 及旧 Binary compatibility 各 1 项通过日志存在。本片限定收口，不追加人工验收。测试先行的 C1083 仅证明新接口尚不存在，不作为行为缺陷复现证据。整个 Qt Lab 仍有私有执行/类型依赖，不能称整体 public-only；下一片候选为 0.5–0.8 complete 执行迁移的精确范围，尚未派发。无提交/推送/发布/删除。详细证据见 [首片验证](lab-owned-presentation-types-validation.md)。下方保留派发边界。

用户确认本轮边界并授权执行。基线 `main@c5b3692`，保护两份未提交盘点报告及总控计划。由《Lab应用推进》独占实施；PAE/工程整理停止，不并发修改共享文件。

本片仅建立不依赖 private PAE/旧执行桥的 Lab-owned 字段/消息/文档展示 DTO、字段 enum、typed draft/Decimal64 和 canonical 整数输入解析工具。先补行为断言，再替换共享类型；公开与旧路径各自在 mapping 边界转换，执行 owner、调用次数、Schema dispatch、预算/事务语义保持不变。conversion 只保留现有展示/编辑所需事实，不新增公式展示；若完整参数确有执行用途则保留边界转换并报告，不擅自丢弃。首片不迁移流状态、诊断全体系或 0.5–0.8 执行。

允许 `tools/protocol_lab_ui/` 中 description mapping、自有 DTO/utility 新文件、document_session 的 typed draft/转换、field table/delegate、public binary description 及必要展示调用点；仅为类型迁移做机械转换，不重构 owner。允许 `tests/protocol_lab_ui/` 定向测试和局部 CMake 注册；根 CMake/PAE public/src/旧执行桥与 ASCII/Binary adapter 执行实现不改。需要越界时停报。报告独占 `docs/engineering/lab-owned-presentation-types-validation.md`，其余计划/索引由总控维护。

新独立构建 `out/build/windows-msvc-lab-owned-presentation-types`（已存在则唯一后缀），证据 `out/lab-owned-presentation-types/`。串行 D/R 验证整数边界/非法 canonical 输入、Decimal64 无损复制、字段类型/source/编辑器/只读与物理展示、受影响 legacy 与 H2/A2/0.11 回归；Release 断言有效。增加独立自有头/工具编译检查，不能包含 private PAE 或 v06 执行桥。不声称整个 UI 头/目标已解耦，不重打 SDK，不覆盖部署或启动人工验收。若 DTO 大小变化，核对受影响 sizeof/预算准入，不扩大预算契约。

本轮范围仅 Qt Lab，保留仓内兼容代码，Binary 0.9 保持 Decode-only，CLI/Evidence/Replay/UDP 不动。历史 Qt 访问违例再现则留证停报，不重复刷绿。完成向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次交接摘要，停止写入，等待复核。无 Stage/Commit/Push/发布/删除授权；后续旧执行迁移和独立构建仍串行另派。

### 最新回收：Lab 剩余依赖盘点（2026-09-19）

两份报告已交付，执行任务均停止写入。总控已阅读并交叉核对 Qt target 的私有链接、schema dispatch 的 yyjson 依赖、0.9 Encode availability 和共享 DTO/数字解析引用；本批仅静态复核，未构建或运行测试。

- 功能报告：[剩余公开消费边界](lab-public-consumption-residual-audit.md)；构建报告：[包外构建预检](lab-sdk-external-build-preflight.md)。
- 0.9 complete Decode、0.10 complete Decode/Encode、0.11 complete/stream 的已迁移配置走公开执行，但共享 DTO 和兼容分支仍引入私有类型；0.5–0.8 仍有实际私有编译/Core 执行。整个 Qt Lab 尚非包外 public-only。
- 建议顺序：先限定 Lab-owned 展示 DTO/utility（执行 owner 不变），再迁移 0.5–0.8 complete 执行，再隔离 ASCII 私有兼容闭包，最后独立构建入口及 static/shared D/R 包外验证。首片仍需按精确文件/行为清单收敛，不能把所有状态类型一次性重构。
- 建议本轮范围仅 Qt Lab；CLI/Evidence/Replay/UDP 保持原状，仓内私有兼容 targets 保留，包外目标不包含它们。0.9 保持当前 Qt Decode-only，不顺带新增 Binary Encode/stream；当前展示保持，不新增 conversion 公式展示或 PAE 接口。上述范围待用户确认后再派实现。
- yyjson 是独立构建的后续依赖决策，DTO 首片无需改变它；不得用新 UI 专用 PAE 接口绕过依赖，也不重新打开已确定的随仓 Qt 选择。

本批报告收口不代表实现或包外验证完成；无新 Stage/Commit/Push/发布/删除授权。历史 Qt 访问违例仍独立保留。

### 当前派发：Lab 剩余依赖与包外构建定向盘点（2026-09-19）

用户授权下一步，基线 `main@c5b3692`（本地提交、未 Push）。本批只读代码/构建配置，各执行任务仅写各自报告，不 Configure/Build/Test、不改实现或 CMake、不删除、不提交/推送/发布。总控维护计划及协作索引，PAE 执行任务停止。

- 《Lab应用推进》独占 `docs/engineering/lab-public-consumption-residual-audit.md`：按用户入口/Schema/Decode/Encode/stream/Host 列出运行路径；将 private 依赖区分为真实执行依赖、类型/DTO/展示耦合、仅旧分支和测试耦合。给出文件/符号证据、可复用公开 API、真实缺口与最小迁移顺序，不能用 grep 命中数量替代调用链判断。特别说明 Binary Encode/其他 legacy 的实际状态，不能默认为已迁移或未支持。不得以删除/禁用旧功能伪造 public-only。
- 《PAE工程整理》独占 `docs/engineering/lab-sdk-external-build-preflight.md`：静态盘点 Lab target/link/include/source/build/deploy 闭包，区分现有非 Qt 包消费与整个 Qt Lab 独立构建，给出仅 Lab 源码+已安装 PAE SDK+随仓 Qt 的候选输入白名单、禁止开发仓库 fallback 的检查及建议验证入口。只提出构建改动清单，不创建包/脚本、不复制/下载/删除产物。功能覆盖以 Lab 报告回收后由总控合并，不自行判断删减功能。

两份报告并行、写入范围互斥，无需互相等待或发送唤醒消息。各自附最小下一片、依赖先后、必须用户拍板项和未验证边界；完成向总控主动反馈一次并停止。总控回收后串行确定 DTO/运行依赖迁移与独立构建切片，不自动授权后续实施或新 PAE 接口。历史 Qt 访问违例独立保留，不开展新崩溃排查或人工验收。

### 0.11 本地提交检查点（2026-09-19）

用户在本片人工收口后同意推进下一步，本轮整理静态 framing 公开查询、新 SDK 消费验证、Lab 非 Qt stream 与 Qt 接线的累积变更形成本地提交；不 Push、不发布、不删除、不启动新功能。保留既有验证来源和 dirty SDK provenance，提交并不把旧候选改写为干净发布物。已核对文件范围和差异，沿用此前限定源码复核、D/R 专项、包外消费及三组用户烟测，不重复成功测试；提交前检查暂存差异及空白，提交后检查工作树。外层 AGENTS 不在仓库内，out 产物不纳入提交。下一步候选为剩余 Lab private 依赖及包外消费边界的定向盘点，尚未派发。

### 当前实施：Lab 0.11 Qt 流式公开接线（2026-09-18）

2026-09-19 最新收口：用户确认三组简短烟测通过、Lab 已关闭，本片 0.11 Qt 公开接线限定收口。实际人工范围为分片/粘包续传、显式 Host Flow 隔离、窗口关闭取消后续传，详见 validation 第 5 节，不扩大为原建议全部事务人工验证。执行任务停止；建议下一步整理本轮查询/SDK/非 Qt/UI 累积变更形成提交检查点，再处理剩余 Lab 公开消费边界。尚无新 Stage/Commit/Push/发布/删除授权，历史 Qt 访问违例独立保留。下方为过程记录。

2026-09-19 最新回收：mixed Pipeline 修正已限定复核，direct 与显式 Host 均按公开 Observe 区分 stream/complete。已核对新增测试覆盖点、修复前两项失败、修复后 D/R 各 6/6 日志及部署 EXE SHA-256 `F2A39850FD947C29E8ADDBA4265A7750D9DCC3AEA6888EE40C9E8B972D97CF5E`，总控未重复执行测试。Lab 停止写入，进入最多三组简短人工烟测，尚未人工验收或整体收口；无 Git/发布/删除授权。下段为返修过程。

2026-09-19 总控回收：已核对相关源码差异、D/R 日志及 Release EXE 哈希，未重复运行，暂不进入人工验收。发现 direct public 分支 `StreamInspectAvailable()` 仅检查 Decode binding 存在，未区分 complete 与 stream；当前示例含 complete 的 `decode_only_pipeline`，故需限定返修。Lab 在原范围先补 mixed Pipeline 模式/调用负例，再按公开观察或既有静态事实判断流式可用性，核对 complete Inspect 与 stream Submit/Continue/Reset 不混用；定向 D/R 及必要部署更新，保留旧日志，完成停止待复核。不扩大 PAE/Git/发布授权。

用户授权下一步。非 Qt stream 及 noexcept 返修已限定复核；《Lab应用推进》独占本片，PAE/工程整理停止。现场 `main@dbf4798` 加既有未提交实现，保留所有变更。0.11 当前仍为 PRIVATE_ASCII；本片迁移首次加载、直接流式操作及显式 Host Apply，不新增 PAE 接口或第二套 Framer/Decode/冻结状态。

- 沿用已确认 ASCII 公开契约、非 Qt 单一 Host owner 和自有 DTO；0.11 dispatch 仅严格读取顶层版本，一请求一个公开编译器，失败无 private fallback。静态 strategy/M 来自公开查询，C/work 来自实例观察；不私读 Plan/Schema 语义。0.11 Encode 若已有入口，保持既有功能并复用同源公开能力，不新增产品功能；无法同源保留时停报，不双编译。
- 允许相关 `tools/protocol_lab_ui/` 的 dispatch/worker/session/tab、ASCII facade、description/result mapping 与对应 UI 测试；根/局部 CMake 仅必要默认 OFF 门禁与依赖接线。非 Qt `public_ascii_host_adapter.*` 及其专项只允许 UI 接线所需最小组合/观察扩展，不得改变已复核执行语义；其他缺口先报告。独占报告 `docs/engineering/lab-public-ascii-stream-ui-validation.md`，总控维护其余计划/索引。
- Submit/Continue/Reset 直接消费已复核 adapter，单步一次 Host 调用；保留精确 consumed、冻结后缀及候选/业务计数。Qt 只映射无分配 diagnostic enum 为展示文本，不把映射塞回 noexcept 执行层。失败清旧成功字段，零字段成功不误判失败。direct 与显式 Host 共享实现，不并存两个活跃流状态真源。
- 保留异步加载取消/迟到 completion 拒绝、document/load/plan/request 等身份检查；Flow/Tab 切换先保存真实源草稿再恢复目标，展示不跨流。Apply 候选准备失败/取消不替换旧 owner、后缀和结果，成功才发布；reload/close/格式切换沿用既有事务和转换拒绝语义，不另加用户步骤。0.10 A2、Binary H2、其他 legacy、CLI/Evidence 对 0.11 的拒绝不变。
- 先补针对性断言再接线；新独立目录 `out/build/windows-msvc-lab-public-ascii-stream-ui`，证据 `out/lab-public-ascii-stream-ui/`，已占用则唯一后缀。串行 Windows x64 Debug/Release 覆盖单次公开编译、半包/粘包/失败后续传、Reset、Flow/Tab 隔离、Apply/加载/取消事务及受影响 A2/H2 回归；Release 断言必须有效。默认 OFF/Testing-off 检查；非 Qt helper 若变更，补受影响非 Qt 和 static 包外 D/R，不重打 SDK 或重复无关六组。若访问违例再现，留证停报，不无限复跑刷绿。
- 在新目录部署可运行 Release EXE、随仓 Qt 与配置，记录精确路径和关键哈希，不覆盖旧部署或修改本机 Qt。交付最多三组简短人工烟测建议，待总控复核后才请用户操作。本片不是整个 Lab 包外 public-only 验证、正式发布或历史 Qt 崩溃修复。

若公共能力缺失、兼容性/预算/所有权需改契约、共享文件冲突，停报总控。完成主动向总控反馈一次并停止写入。无 Stage/Commit/Push/发布/删除授权。

### 当前实施：Lab 0.11 非 Qt 公开流式适配（2026-09-18）

最新收口：`noexcept` 诊断返修已完成限定总控复核。流式错误诊断改为 Lab 自有 enum，移除动态文本分配及借用寿命问题；已核对入口/错误分支、Release 有效断言、修复前退出码 86 记录、最终 D/R 各 4/4 与 static 包外 D/R PASS 日志，以及三个实现/测试文件 SHA-256。总控未重复构建/运行。非 Qt 本片限定收口，Lab 停止写入；下一片为 Qt 0.11 流式公开接线，待另行派发。历史 Qt 访问违例、整个 Lab 包外消费及 Linux 等仍未关闭，无 Stage/Commit/Push/发布/删除授权。下段为返修过程。

总控回收复核：Lab 已交付非 Qt 实现及专项/包外 static D/R 证据，尚未收口。源码发现 `StreamStepResult::detail` 使用 `std::string`，多个 `noexcept` 流式入口及 `bad_alloc` 处理分支仍赋长字符串，错误报告自身可能再次分配并触发 terminate。限定《Lab应用推进》在原文件范围修正为无分配且寿命明确的诊断表达，补针对性失败路径断言及受影响 D/R、static 包外验证；保留已有证据，使用独立日志前缀。PAE/工程整理继续停止，不进入 UI、不增加人工验收、不重打 SDK，无 Stage/Commit/Push/发布/删除授权。完成主动反馈并停止，待总控复核。

SDK 前置已完成限定总控复核：六组运行/来源/导出记录、consumer 断言及关键源码/DLL 哈希已核对，未重跑构建或独立复算全部包文件。用户授权下一步；《Lab应用推进》独占本片，PAE/工程整理停止。基线 `dbf4798` 加保留的未提交查询实现和文档；使用 `out/sdk-public-stream-description/candidate1-20260918/` static D/R 候选，不重打包、不改公共 PAE。

本片实施契约：复用 A1 同一 compiled owner、描述与结果物化；优先在现有 `public_ascii_host_adapter.*` 扩展唯一 Host stream 状态，direct stream 后续也通过该所有者使用，不再创建第二套 Framer/Decode 路径。按公开静态查询确认 ASCII_CRLF/STREAM_CHUNK/M；complete 查询合法但 Submit 拒绝，既有 complete Decode/Encode 行为保持。静态 M 包含 CRLF，实例提交 C=min(64KiB,effective_max_submit_bytes)，work 独立。每 Flow 独占冻结 chunk/cursor/Handle/计数/故障；每动作一次 Host 调用、最多一个候选，STOP consumed 仅推进已消费前缀，后续只续提剩余后缀；无后缀且 internal work 才空 Continue，无工作本地拒绝。禁止重复 Decode、复制已消费前缀或观察/业务各复制一份。

失败候选只在 observer 物化并 STOP；成功 observer 只记事实并 STOP，business 物化一次并 STOP。交叉核对回调/候选/Decode 计数；本地物化异常保留原 Codec/Host/consumed 事实、清空候选有效 DTO并要求目标 Flow Reset。正常 CODEC_FAILED 不自动升级流故障。推进前检查 consumed<=submitted；无效 consumed 不更新 cursor，流进入故障而非回放。无进展保护必须允许合法 pending/work-budget 步骤，不以一轮零 consumed 误判故障。Reset 成功后重新 Find 新 Handle，旧 Handle stale；只有重新 Observe 成功才清本地状态，其他 Flow 不受影响。资源准入以饱和运算计入 owner/Host/本地状态/冻结 capacity/result，instance/replacement 在发布前检查；逻辑费用不是 RSS。

允许文件：`tools/protocol_lab_ascii/public_ascii_host_adapter.*`、必要最小 `public_ascii_offline_adapter.*` 组合入口/stream 描述 DTO，必要单一 `public_ascii_stream_*` helper（不得形成第二状态真源）；该目录与 `tests/protocol_lab_ascii` 的局部 CMake、新 `public_ascii_stream_adapter_tests.cpp`、独立包外 `public_ascii_stream_consumer/`；根 CMake 仅新增默认 OFF `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM` 及依赖/目录接线。该开关必须不依赖 A2 UI 或旧 private adapter，现有 A2 与新 stream 可分别引入共享 Host helper。若需要调整 `tests/CMakeLists.txt` 目录门禁仅限本开关接线。独占报告 `lab-public-ascii-stream-validation.md`。不改 Qt/UI/dispatch、PAE public/Core/Plan/Schema、旧桥或 SDK；其他文档由总控维护。实现范围不足先停报。

先补专项断言，再实施；新 `out/build/windows-msvc-lab-public-ascii-stream`（已存在另唯一后缀），日志 `out/lab-public-ascii-stream/`，D/R 串行。覆盖 CR/LF 分片/粘包 STOP 后缀/失败后合法候选/零字段、work pending、超长 discard 恢复、两 Flow、Reset/新句柄、复制故障及独立预算 exact/minus-one；回归 A1 和 A2 非 Qt Host 可复用断言，A2 UI 不在本片。测试注入只能 Testing 构建；门禁验证默认 OFF 和 Testing-off，无 Qt/private PAE 依赖。包外 consumer 编译 Lab adapter 源码但只链接新安装 SDK 的 PAE::pae static D/R，记录实际来源，不用仓库 public target 冒充包验证。本片不运行 Qt，不新增人工烟测，不重跑 SDK 六组。

交付写明单一状态所有者选择、实际文件、命令/结果/证据与未验证项；完成向总控主动反馈一次并停止。无 Stage/Commit/Push/发布/删除授权；历史 Qt 访问违例不因本片关闭。总控复核后再定 Qt 接线，不自动推进 UI。

### 当前派发：0.11 查询新 SDK 消费验证（2026-09-18）

静态 framing 查询已完成总控限定源码/测试差异与日志复核，未重复构建；内部损坏 Plan 负例仍未实测。用户授权下一步，由《子任务推进》独占新 SDK 生成和包外验证，Lab/工程整理停止。基线仍为 `dbf4798` 加当前未提交查询实现/文档，必须保留并如实记录 dirty provenance 与实际打包输入哈希，不能将候选标成该 HEAD 的干净发布。

新候选根 `out/sdk-public-stream-description/candidate1-20260918/`，生成 source、static-debug/release、shared-debug/release 五包；独立构建 `out/build/windows-msvc-public-stream-sdk-*`，证据 `out/public-stream-sdk/`，仓库外消费根 `F:\PersonalWorkspace\pae-sdk-public-stream-validation-candidate1-20260918\`。已存在的路径不覆盖，另选唯一后缀并记录。沿用 Windows x64 v142 已验证配置；源码包与二进制包各自独立消费，六组 D/R 串行运行，禁止暗用开发仓库 private 头/库或源码 fallback。

复用打包脚本和当前 consumer，确保实际执行新查询，覆盖 ASCII stream M、complete M 为空及既有 Codec/Framer/Host/ASCII/Binary 消费。必要最小修正仅限 `scripts/package_sdk_stage3.ps1`、`cmake/PaeSdkInstall.cmake`、`cmake/PAEConfig.cmake.in`、`examples/public_api_sdk_consumer/`；不改公共接口、Core/Plan/Schema/Host/Lab 或根 CMake。若功能缺陷阻断，留证停报，不扩修。独占报告 `docs/engineering/pae-public-stream-sdk-validation.md`，其他文档由总控维护。

记录每组完整命令/退出码、actual include/prefix/库来源，检查新旧 DLL 导出、依赖、consumer 配套 DLL 哈希、五包 manifest/hash、外部复制一致性和开发路径泄漏；产品 Testing-off，不重复无关全量测试或 Qt 人工验收。本地候选不是正式发布/稳定 ABI/Linux/现场证据。不覆盖任何旧 SDK/部署，不改本机 Qt/全局环境，不删除；无 Stage/Commit/Push/发布授权。完成向总控主动反馈一次并停止，待复核后再派 Lab 非 Qt 流式适配。

### 当前实施：0.11 静态 framing 公开查询（2026-09-18）

SDK/A1/A2 已本地提交 `dbf4798`、未 Push。PAE/Lab 两份预检已回收并限定复核；用户授权继续推进。精确查询契约见 [ASCII 公开消费契约第 6 节](pae-public-ascii-consumption-contract.md)。complete 查询成功且 M 为空；Lab stream 准入另行拒绝 complete。静态 M 与运行 C/work 分离，旧 capability 布局/合法语义保持。

本批《子任务推进》独占接口与针对验证；Lab、工程整理停止。允许 `include/pae/stream_framer.h`、`src/public_api/stream_framer.cpp`、必要最小 `src/protocol_framing/stream_framer.h/.cpp` 私有容量 helper、`tests/public_api/public_stream_framer_tests.cpp`、独立头测试及必要局部测试 CMake、`examples/public_api_sdk_consumer/main.cpp`；独占验证报告 `pae-public-stream-framing-validation.md`。不修改 Core/Plan/Schema/Host/Lab、根 CMake、旧部署、SDK 打包入口、其他报告或总控文档；需要扩大范围先报告。

先加定向断言再实施。在新的 `out/build/windows-msvc-public-stream-description` 与必要 shared 独立目录串行进行 Windows x64 D/R，目录已存在则用唯一后缀；证据放 `out/public-stream-description/`。验证 complete、Binary 三策略、ASCII CRLF 的精确映射，invalid/moved/index 和可安全触达的内部失败，C/M 分离、真实无分配观察、旧 capability 兼容；回归公开 header/Framer/Host 与受影响内部 Framer，补 Product-only/Testing-off 门禁及新旧 DLL symbol/最小外部链接。未变的无关 Qt/全量矩阵不重复。本批不重打五包；接口复核后集中派发一次新 SDK 五包/六组消费验证，再接 Lab。

停止条件：契约冲突、无法保持旧执行语义、需要新增缓存/预算或扩大文件范围时停报；测试失败保留证据，不刷绿。实现及定向验证完成后向总控主动反馈一次并停止。无 Stage/Commit/Push/发布/删除授权。总控只维护契约、综合计划、路线和外层 AGENTS，不与执行任务写入交叉。

后续顺序：接口复核 → 新 SDK 消费 → Lab 单一状态所有者的非 Qt stream 适配 → UI 接线与最多三组人工烟测 → 有限包外 Lab/工程收尾。后续文件范围待各前置结果明确后再派，不并行猜测接口、不新增 TLV/通信业务。

### SDK/A1/A2 检查点与 0.11 后续顺序（2026-09-18）

用户授权先整理 SDK/A1/A2 累积变更形成本地提交检查点，再推进 0.11 流式公开接口迁移；不包含 Push/发布/删除。检查点只纳入本轮 SDK consumer、A1/A2 adapter/UI/专项/CMake 和相关验证/索引，out 产物保持 ignored，外层 AGENTS 不属于 Git 仓库。既有 D/R 与人工证据沿用，不因只整理文档重复全矩阵。

0.11 先闭合公开静态 framing 描述这一前置，再做非 Qt stream 适配，最后 Qt 接线。公开 input kind/strategy/M 从冻结状态读取；M 与实例有效 submit 容量 C、work limit 分开，不从 available 猜策略、不重新解析 Schema。当前先安排 PAE 与 Lab 并行只读复核并分别写报告：PAE 细化查询形状/错误语义/策略映射/测试范围，Lab 基于 A2 细化 owner/候选/成功结果/冻结后缀与 UI 切换边界。两份报告回收后由总控落盘精确契约并串行派发实现，避免公共 API 与消费侧各自猜测。

PAE 仅写 `docs/engineering/pae-public-stream-framing-preflight.md`；Lab 仅写 `docs/engineering/lab-public-ascii-stream-preflight.md`。复用既有报告，只记录现场增量，不重做全项目盘点。工程整理停止；本批不改代码/CMake、构建、部署或 Git，完成主动反馈总控并停止。历史 Qt 访问违例独立保留，不扩到通信/TLV/业务路由。后续若需改变既有执行语义或新增实质产品决策，先报用户。

### 当前授权：A2 ASCII 0.10 UI / 显式 Host 公开接线（2026-09-18）

最新收口：资源返修已完成限定源码/最终日志与部署哈希复核，用户确认“A2 三组烟测通过，Lab 已关闭”。A2 0.10 完整记录直接执行及显式 Host/UI 本片限定收口；总控未重复构建测试。所有执行任务停止。下一步建议先审查 SDK/A1/A2 累积差异形成提交检查点，再核对 0.11 stream 公开消费缺口；尚无新 Git 或下一片实施授权。历史 Qt 访问违例、整个 Qt Lab 包外消费仍未关闭。下方返修/待人工状态为历史，证据见 A2 validation。

总控复核返修：A2 成功输出 sink 接线修正及最终 D/R 12/12 日志、部署哈希已核对，未重跑；尚不收口。源码发现 public Host adapter 未完整执行 A1 等价的输入/诊断复制预算、最终 Host instance/replacement admission 与 Encode scratch 前检。已限定 Lab 在原范围先补独立边界负例，再修正有界预检、计费与同次 Codec 状态保留；新证据使用 budget-review 前缀，保留旧日志。其他任务停止，人工烟测延后；不扩公共 API、0.11 或历史崩溃排查，无 Git/发布/删除授权。

用户同意推进。A1 已限定收口，《Lab应用推进》独占本片实现及串行构建，PAE/工程整理保持停止。现场基线为 `main@481d51a`，现有 SDK/A1/文档未提交内容全部保留。下方 A1“下一片尚未派发”是历史状态；本段为本次执行范围，无 Stage/Commit/Push、发布或删除授权。

- 仅迁移 Schema 0.10 完整记录 Decode/Encode 和显式 Host Apply/UI 生命周期。沿用已确认公开 ASCII 契约，首次打开及 Apply 均不得回退私有编译/执行；dispatch 仅严格读取顶层版本，每请求只调用一个目标编译器。保留 worker 取消、文档/加载版本票据和过期结果拒绝。0.11 stream、Binary H2 和其他旧格式保持原行为。
- 允许修改 `tools/protocol_lab_ui/` 中相关 worker、dispatch、document/session/tab 与结果呈现接线、必要 public ASCII helper，`tools/protocol_lab_ascii/public_ascii_*` 的最小所有权/结果物化复用，及对应 `tests/protocol_lab_ui/`、`tests/protocol_lab_ascii/public_ascii_*` 测试。A1 helper 仍无 Qt/私有 PAE 依赖。根及局部 CMake 只作默认 OFF 的 A2 门禁与依赖接线，不改现有默认、SDK 白名单或 CLI/Evidence 拒绝门。禁止修改 PAE 公共 API/Core/Plan/Schema、旧私有桥执行语义；若接口不足，保留证据停报。
- 编译 owner、描述、Host 和结果须同源；允许为此最小提取 A1 公共结果物化 helper，不得额外编译或 Decode 获取展示。Host 回调借用在同步期限内有界复制后才能跨事件使用。Encode 保留调用期 Message selector；RX 实际切片、TX 逐段验证、零长/literal-only、not referenced 与本地物化失败语义保持契约。预算先检、失败不发布半成品，不保留旧成功字段/范围。
- Apply 先准备再原子发布；失败或取消保留原 owner/绑定/结果，成功发布按契约更新身份。草稿和结果按真实源 Tab/binding/Flow 保存、再恢复目标，防止旧 Flow 串写重现。格式转换失败不吞掉草稿或暗换格式；不以缺失公共元数据压缩已有展示。
- 新独立构建目录 `out/build/windows-msvc-stage4-ascii-public-a2`（若已存在则另取唯一后缀），证据 `out/stage4-ascii-public-a2/`。Windows x64 D/R 针对测试覆盖单次编译、Decode/Encode/失败恢复、Apply 失败/取消/替换、不同草稿 Flow/Tab 隔离，并做 Binary H2 与 0.11 受影响兼容回归。先有针对性断言再接线；不无限复跑历史 ASCII Qt 崩溃，若再现立即保存证据停报。A1 helper 若变化，补 A1 D/R 及既有 SDK static 包外 D/R；不重打五包或无关全矩阵。
- 仅在新目录生成可运行 Qt Release 候选，使用随仓 Qt，不覆盖已验收 H2、本机 Qt 或旧 SDK。本片仓库 UI 集成验证不等于整个 Qt Lab 已做到包外 public-only 消费。报告独占 `docs/engineering/lab-public-ascii-ui-a2-validation.md`，写明精确 EXE/配置路径、命令/结果、未验证项和最多三组简短人工烟测建议；总控复核后再请用户操作，历史访问违例仍独立未解决。
- 完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次交接摘要，停止写入，等待总控复核。总控维护本计划、路线与外层 AGENTS，执行任务不得并行编辑这些文件。

### 当前派发：Lab ASCII 0.10 非 Qt 公开适配（2026-09-18）

限定收口：预算返修已回收，总控核对按方向实际可用消息数 reserve 的实现、独立单向/混合 capacity 断言、修复前失败日志与 `budget-fix-*` D/R 专项及 static 包外通过日志，完成 A1 限定源码/证据复核。总控未重跑构建测试；实际 capacity 断言是当前 MSVC 验证，不承诺任意 STL 实现的精确容量或 RSS 上限。执行任务停止，当前具备进入 0.10 UI/显式 Host 接线的前置条件，下一片尚未派发。旧 UI/stream/已验收部署未切换，历史 Qt 访问违例仍独立未解决；无 Git/发布/删除授权。下方返修描述保留为过程。

总控复核返修：A1 已交付，D/R 专项及 static 包外运行日志通过；总控源码检查发现 `PreflightDescription` 按实际 D/E 可用消息数计费，而 `CopyDescription` 对两个方向均 reserve 全部 message_count，单向消息会出现计费与显式预留不一致。本片暂不收口，限定 Lab 补独立单向/混合关联计费断言并修正预检/预留一致性，再重跑 A1 D/R 和受影响包外验证。原 exact/minus-one 取自身返回值不足以独立证明费用正确。不得借机扩改公共 API、UI 或执行语义；完成反馈后停止。

用户授权执行下一步。SDK 消费前置已限定复核，《Lab应用推进》独占本片代码与构建，PAE/工程整理保持停止。基线仍为 `481d51a`，此前 SDK consumer/报告及总控文档的未提交修改全部保留；不要求先提交，不自动取得 Git 授权。

- 新增独立 public-only 完整记录 adapter 和 Lab 自有描述/结果 DTO，推荐 `tools/protocol_lab_ascii/public_ascii_offline_adapter.*` 及必要同前缀 helper；仅消费 `pae/*`/`PAE::pae`，不引用旧私有 adapter、Compiler/Plan/Core/Host 或 Qt。保持已编译 owner 与只读描述身份一致，可接收并持有一次公开编译所得 owner；不重解析 Schema、不以额外编译/Decode 获取展示事实。旧实现和 UI 不切换。
- 依照 `pae-public-ascii-consumption-contract.md` 第 2–5 节，实现完整记录一次 Decode/Encode、RX 同次借用切片核界复制、成功 TX 逐段校核后投影实际范围；保留 RX/TX 独立、动作不引用、单向/literal-only、调用期 Pipeline/Message selector、失败诊断身份。DTO 自有且有界，预算检查先于分配/发布；复制失败不得交付半成品。控制字符列表缺省表示未知，Core 是最终字符校验者。不新增 Host/Flow/UI/stream 生命周期。
- 允许新增上述 adapter/helpers、`tests/protocol_lab_ascii/public_ascii_*` 测试/包外 consumer 子目录与局部 CMake；根 CMake 仅允许默认 OFF 的独立非 Qt 门禁、依赖校验和 target 接线，不能要求启用旧私有 ASCII adapter/Qt，也不改变现有默认项、CLI/Evidence 拒绝门或 SDK 安装白名单。旧 adapter 和公共 PAE 源码禁止修改。需扩大范围时停报。
- 先写专项断言并保留初始缺口，再实施；新 `out/build/windows-msvc-stage4-ascii-public-a1*` 串行 D/R，证据 `out/stage4-ascii-public-a1/`，已有目录另取唯一后缀。测试双向/单向/literal-only、not referenced、变长/零长 RX 实际范围、嵌入 NUL、TX 重复内容不误定位、非法 selector、未知/唯一匹配后失败/成功恢复、无旧结果泄漏、输入/owner 寿命及资源边界。测试映射不一致时区分 Codec OK 和本地物化失败，不篡改 Codec 状态。
- 使用 `out/sdk-stage4-ascii/candidate1-20260918/pae-sdk-static-debug` 和 `pae-sdk-static-release` 做包外 adapter 消费 D/R；只链接对应安装包，不由仓库 public target 冒充包。无需重复生成五包/完整 PAE 矩阵/Qt 压测；根接线若有变化，补默认 OFF/Testing-off 配置隔离检查。不覆盖现有 H2、旧包或本机 Qt，不新增人工点击。
- 独占报告 `docs/engineering/lab-public-ascii-offline-a1-validation.md`；总控计划/路线/外层 AGENTS 由总控维护。完成向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次源码范围、构建/测试命令和证据、未验证项、Git 状态，停止写入待复核。无 Stage/Commit/Push/发布/删除授权。下一片 UI/显式 Host 在本片复核后另派，0.11 stream 后置；历史 Qt 访问违例不因本片关闭。

### 当前派发：ASCII 新 SDK 独立消费验证（2026-09-18）

回收更新：执行任务已停止；总控核对 consumer 增量、六组运行日志、包外 source/prefix 路径及五包完整性记录，并独立复算候选与 consumer 同目录 D/R DLL 哈希一致，完成本片限定复核。总控未重新构建或运行六组测试，也未独立重算全部 manifest；运行目录 DLL 一致不额外宣称有进程模块加载跟踪证据。源码/静态/动态包外 D/R 均有通过记录，详见 [SDK 验证报告](pae-public-ascii-sdk-validation.md)。当前具备进入 Lab 0.10 非 Qt 适配的前置条件；该实现尚未派发，需下一轮明确范围。历史 Qt 访问违例仍独立未解决，无 Git/发布授权。

用户授权规划并派发下一步。现场 `main@481d51ae2d62e2e28754fd84e5df4bb64309a1e2`、工作树干净；该集成检查点已本地提交，未 Push。下方旧的“未提交/无实施授权”只代表当时状态，不覆盖本段。

- 《子任务推进》独占打包与构建：基于当前 ASCII facts 实现，在 `out/sdk-stage4-ascii/candidate1-20260918/` 新建 source、static-debug/release、shared-debug/release 五包。新构建目录使用 `out/build/windows-msvc-stage4-ascii-sdk-*`，证据目录 `out/stage4-ascii-sdk/`；若存在则另选唯一后缀，不覆盖既有产物。沿用已验证 Windows x64 v142 与 D/R CRT 配对。
- 包外 consumer 仅消费该 source 包的 `add_subdirectory` 或二进制包的 `find_package(PAE CONFIG REQUIRED)` / `PAE::pae`。验证三形态各 D/R 六组，包含新增 ASCII Action/Segment/Field 查询、RX 切片与匹配身份、Host 同源观察及现有 Binary 回归。不得用仓库 `PAE_SOURCE_DIR` 或 private 头代替包消费；记录实际头文件/库/DLL来源、三项新查询 DLL 导出、包清单/哈希、构建输入版本。源码包在脱离仓库依赖路径的独立消费根验证，二进制包不暗连源码。
- 复用既有打包入口和 consumer。允许必要的最小打包修正仅限 `scripts/package_sdk_stage3.ps1`、`cmake/PaeSdkInstall.cmake`、`cmake/PAEConfig.cmake.in`、`examples/public_api_sdk_consumer/`；不得改变 SDK 既定契约或放宽断言。新报告独占 `docs/engineering/pae-public-ascii-sdk-validation.md`。若有功能缺陷、公共接口/Core/Schema/根 CMake 变更需求，保留失败证据并停报总控，不扩修。若打包代码有改动，仅补对应检查与受影响重新生成/验证，不盲目循环重打。
- Lab、工程整理暂不启动：先由总控复核候选内容和证据，再派 Lab 0.10 非 Qt adapter；随后 UI/Host，最后 0.11 stream。不存在本批并发修改公共入口或同构建目录的任务。
- 不修改 Qt、本机环境、旧 final6/candidate2 或已验收 H2 部署；不重开历史崩溃排查、不新增人工点击、不做无关全量矩阵。无 Stage/Commit/Push、正式发布或删除授权；候选验证不等于稳定 ABI/Linux/真实协议验收。
- 完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次交接摘要，含文件范围、六组命令/结果、包绝对路径和哈希证据、未验证项、Git 状态；停止写入，等待总控复核。发现阻断提前报告。

| 阶段 | 功能工作 | 同步整理 | 完成标准 |
| --- | --- | --- | --- |
| 0，已交付 | 公开完整记录接口设计与消费者需求核对 | 仓库目录职责/去留盘点 | 三份报告已回收，作为阶段 1 输入；不表示代码实现完成 |
| 1 | 公开完整记录 Decode/Encode | include/pae、src/public_api、对应 tests/examples | 仓库外程序仅公开头和 PAE::pae 完成编译、解析、组包；Binary/ASCII 定向回归 |
| 2 | 公开 Framer/Host，保持候选/停止/复位语义 | 对应内部模块、示例、测试入口 | 非 Lab 宿主完成分块接收和方向绑定；生命周期与失败隔离通过 |
| 3 | Windows x64 源码/静态/动态三类独立包 | cmake、Presets、依赖白名单、包布局 | 仓库外同组 consumer 分别消费三个包，D/R 单独验证；不依赖私有头或 Qt/Lab |
| 4 | Lab 改为公开 PAE 消费者，先 Binary 后 ASCII/流式 | tools 中适配层、旧桥、Lab 构建 | 独立 PAE 包支撑 Lab；针对性自动回归及一次简短人工烟测 |
| 5 | 开发者入口与工程收尾 | 失效目录/桥接/产物，文档、示例与命令一致 | 清理经确认目标；源码、SDK、Lab 各有清晰入口与未验证范围 |

目录整理贯穿阶段，不全部积压到第 5 步。已确认无依赖且可安全处理的文档/入口变更可前移；不可恢复删除仍先列清单请用户确认。

## 阶段 4 当前状态：Binary 迁移首片范围核对（2026-09-15）

### ASCII / 流式公开消费迁移准备（2026-09-15）

最新回收：公开事实首片已交付，总控核对关键公开查询/Codec/Host及新测试、consumer与日志，完成限定技术复核；执行任务停止。已有 D/R 专项各34/34、公开API各9/9，共享独立源码消费者运行与导出检查通过；总控未重跑。证据见 [首片验证](pae-public-ascii-facts-validation.md)。尚未重打/验证含新API的独立SDK包或接Lab；下一步建议新SDK候选消费验证，待授权派发，再进入Lab 0.10无Qt适配。不新增人工验收，无Git/发布/删除授权。下文为实施派发历史。

当前派发：用户授权公开事实首片，由《子任务推进》独占 `include/pae/{protocol_description,compiler,codec,host_endpoint}.h`、`src/public_api` 对应实现/必要私有 facade 辅助、`tests/public_api` 及其局部 CMake、必要公开 consumer 示例。实现最小契约第 2/3/5 节：冻结 ASCII 描述、成功 RX 输入切片合同及专项、Codec/Host 同源可选匹配身份。验证记录独占 `pae-public-ascii-facts-validation.md`，不改契约/主线索引。Lab、工程整理停止；不改 Core/Plan/Schema、Qt/UI、Framer、控制字节公开列表、TX range API 或旧部署。先专项断言再实现，独立 `out/build/windows-msvc-stage4-ascii-facts` 串行 D/R；需要 shared 单独 `out/build/windows-msvc-stage4-ascii-facts-shared`，日志 `out/stage4-ascii-facts/`。做公共头/描述/Codec/Host及受影响 Binary 定向回归、shared 导出和最小外部链接检查，不重跑无关 UI 压力。首片不重打五包；新 SDK 消费验证在接口片复核后另派、先于 Lab 消费交接。预算、零长切片或身份语义需改变 Core/公共契约则停报；根 CMake/打包脚本修改另报总控。完成一次交接后停止写入，无 Stage/Commit/Push/发布/删除授权。以下为契约及准备批历史。

当前收口：两份静态报告已回收，总控完成关键公开头及源码核对，用户授权落盘 [ASCII 公开消费最小契约](pae-public-ascii-consumption-contract.md)。首片确定动作/长度只读事实、成功 RX 输入切片合同及 Codec/Host 同源匹配身份；TX 由 Lab 校核成功输出后投影，控制字节列表暂缓，stream strategy/M 后置。两执行任务停止。当前仅契约落盘，未派实施、未构建测试，无 Git/发布/删除授权。下文为准备批历史。

用户授权回到主线。本批并行核对、串行决策，不新增代码实现或重新跑矩阵：《Lab应用推进》只读ASCII 0.10完整记录Decode/Encode及0.11 stream的实际私有消费、展示与生命周期需求，唯一写入 lab-public-api-stage4-ascii-migration-review.md；《子任务推进》只读对应公开Compiler/metadata/Codec/Framer/Host及已有证据，唯一写入 pae-public-api-stage4-ascii-gap-review.md。两任务不互等未完成报告、不得交叉写入；工程整理停止，总控合并两份交付后定义首片，存在硬缺口则公开能力补齐先于Lab接线，否则直接最小消费迁移。

要求逐项区分已有公开能力、Lab可自行承担的输入/后缀/DTO、真正PAE缺口与证据缺口；列具体源码/接口/旧契约依据及最小调用序列。保持Encode调用期message selector、RX/TX独立模板、动作不引用字段、literal-only、实际Raw/Logical/byte range、候选与业务结果区分、STOP/精确consumed/Continue/Reset、Flow隔离、冻结后缀和取消/替换原子性。Binary物理查询仅支持Binary不能被误用于ASCII，布局取证不可反算或重Decode。比较完整记录先迁移再stream与共用adapter先切的成本，以最小可用切片优先，不默认新增Schema/TLV/Socket/UI专用PAE接口。报告须给出建议文件范围、依赖顺序、D/R针对验证与是否真的需要人工烟测；禁止以报告扩张成实施。历史ASCII访问违例继续独立保留，不自动重开压力排查。无构建/测试/部署覆盖/删除/Stage/Commit/Push/发布授权；完成主动一次交接总控并停止。

### H2 后置 ASCII 崩溃定向诊断（2026-09-15）

烟测修复限定收口：总控已核对最终定位/寿命代码、新测试、修复前失败及D/R最终专项证据，复算原H2部署哈希未变。最终源码专项各3/3、最后失焦负例D/R通过；此前十轮及受影响回归为对应版本证据，详见独立诊断报告第5节。无需追加人工验收，Lab停止；本片仅修普通烟测焦点定位，历史访问违例无栈、独立未解决，不自动无限排查。下一步建议回到阶段4，先核对ASCII/流式公开消费的最小迁移范围与接口缺口，尚未派发或授权实现。无Git/发布/删除授权。

烟测定向修复授权：用户同意仅修目标编辑器定位、失焦及寿命断言，由《Lab应用推进》独占 tools/protocol_lab_ui/document_tab 的烟测辅助路径、必要 exact_value_delegate 的被动编辑器身份观察及对应头文件/helper、诊断 helper、tests/protocol_lab_ui 和最小局部 CMake 接线；更新独立诊断报告。先用可控失焦/错误焦点场景证明旧定位失败，再按当前表格/模型/单元身份定位且验证唯一存活 editor，QPointer 跨事件检查，禁止任取首个 QLineEdit、强抢桌面焦点、固定延时或弱化 Encode/容量/提交语义断言。正常用户编辑器提交/关闭、PAE/Host/Schema/Qt 均不改；无法保持该边界则先报。新 out/build/windows-msvc-stage4-ascii-smoke-fix 构建，先诊断OFF D/R专项及受影响回归，ASCII重复各最多10次，失败立即取证停；需要诊断ON另用独立目录。原始失败及本轮前后日志分别保留，新日志 out/stage4-ascii-smoke-fix/；已验收 H2 部署不覆盖并核对哈希。历史访问违例仍未解决，完成后停止向总控交接，不自动扩修或迁移，无Git/发布/删除授权，无需人工复验。

独立诊断回收：Lab 已停止，总控核对日志/门禁源码及原部署哈希未变。诊断D/R构建成功；Release第6次普通检查失败后停止（前5次通过），Debug专项未运行。编辑器已创建且到退出才销毁，但通过全局焦点未取得QLineEdit；不能区分无焦点/其他控件焦点，不是历史访问违例复现。建议下一片仅修烟测编辑器定位与失焦/寿命断言，不依赖桌面焦点、不靠延时刷绿；历史访问违例保留，暂不扩大排查。修复尚待用户授权，无Git/发布授权，详见诊断报告第4节总控复核。

独立诊断构建授权：用户同意增加烟测阶段日志和编辑器对象寿命观察，由《Lab应用推进》独占限定 UI smoke 辅助代码与最小 CMake 门禁。新增诊断开关默认 OFF，且仅 --ui-smoke 路径启用观察；允许 document_tab/application_window/exact_value_delegate 相关 .h/.cpp、tools/protocol_lab_ui/CMakeLists.txt、必要根 CMake 开关及定向 tests 接线，不扩执行功能、不引入生产修复或新依赖。在 out/build/windows-msvc-stage4-ascii-diagnostic 新目录沿用 H2/Qt/toolchain 配置串行构建 D/R，日志写新 out/stage4-ascii-crash-instrumented/。标记编译完成、编辑器创建/重开/事件泵前后/提交/销毁、Host Apply、退出等阶段，记录稳定对象编号/单调序号，不解引用已失效指针；观察发现对象消失可明确诊断失败停止，不得静默重试或当作修复成功。Release 最多20次、Debug最多10次专项复现，出现失败先保存诊断证据，不刷绿；另检查诊断默认OFF门禁。不得重建/覆盖已验收 H2 目录，前后核对部署EXE哈希；不安装工具、接受新工具许可、改注册表/WER/Qt/全局环境，不上传转储。仅更新独立诊断报告，未复现也按上限交付，新增根因或超范围需求先报总控。完成主动交接并停止，无 Git/发布/删除授权，不开始 ASCII 迁移。

诊断回收：Lab 已停止写入，总控核对诊断报告、原始 Application Error 1000 事件、运行索引及编辑器相关代码。历史访问违例 0xc0000005 已有系统证据，Qt5Core.dll 故障模块不证明 Qt 自身根因；本轮 Release 20 次/Debug 10 次均未复现，无调用栈，不称已解决或稳定。未改代码、未重建或覆盖 H2 部署。建议下一片限定独立诊断构建的烟测阶段标记与对象寿命观察，有限复现后停，不盲目修复或扩大压力测试；尚待用户授权，不接受工具许可或修改系统崩溃策略。报告见 lab-ascii-smoke-crash-diagnosis.md，无 Git/发布授权。

用户授权推进下一步，派《Lab应用推进》独占诊断已有 qt_smoke_ascii 偶发崩溃；不启动 ASCII 迁移、不默认修复。先核对原始失败日志、实际 CTest 命令及窗口/编辑器/worker 生命周期，现有最终构建 Release 最多 20 次、Debug 最多 10 次串行定向复跑，出现失败即优先取证而非刷绿。允许在新 out/stage4-ascii-crash-diagnosis/ 保存日志、现有可用调试器获取进程局部栈/转储，不改系统崩溃策略或安装工具；不覆盖已验收部署、不修改生产/测试代码。仅新增 docs/engineering/lab-ascii-smoke-crash-diagnosis.md 报告，区分已复现事实、推断、未确认及建议最小修复范围。未复现则如实交付次数和证据，不能称已解决；需要插桩或额外授权先报告。PAE/工程整理保持停止，无 Git/发布/删除授权。完成主动向总控一次交接并停止，随后由总控决定修复或迁移顺序。

### H2 实施派发（2026-09-15）

H2 限定收口（2026-09-15）：用户确认“Flow 复验通过，Lab 已关闭”，返修后的 Flow 草稿隔离人工失败项关闭。结合此前总控限定技术复核，本片 Binary 0.9 complete Decode UI 公开消费切换收口；不等于整个 Lab 迁移、包外 Qt 消费或完整人工验收。ASCII 烟测崩溃仍为独立未解决项，下一步建议先定向诊断，再规划 ASCII/流式迁移；本次未派发新工作，无 Git/发布授权。以下保留返修过程。

H2 Flow 返修回收：Lab 已停止写入；总控核对 SaveAndSelect 保存源槽后更新选中编号的实现、新增不同帧 Qt/Session 往返断言、修复前明确失败与修复后日志，并独立复算部署/bin EXE 哈希一致（3D3F13C6A16A63E70B28463DEED395A7A7834B8D642427A3F85A91F888D0A240）。最终 D/R 各 23/23，旧 Binary 各 2/2、最终目录双文档 smoke PASS；总控未重跑测试。允许进入仅 Flow 往返的人工复验，尚未验收通过。Release 首轮 ASCII qt_smoke_ascii SEGFAULT 后单项及全套复跑通过，根因未确认，保留独立未解决缺陷，不以复跑通过宣称稳定。未提交编辑切回可能恢复同 Flow 上次 Decode 结果，不能解释为新草稿已解码；本次只修跨 Flow 草稿归属。无 Git/发布授权。

H2 人工反馈与定向返修：用户发现不同内容的 Flow 0/1 反复切换时，输入草稿与选中 Flow/已解码结果错配；本项验收不通过，覆盖下方“待人工”状态。源码定位线索是 Session 以当前草稿和目标编号调用 SaveAndSelect，而 H2 adapter 写入目标草稿槽。用户已关闭 Lab 并授权派回《Lab应用推进》先复现再最小修复。限定 Binary H2 adapter、必要的 Session 接线及对应测试/验证记录；优先不改旧 backend/H1/公共 API。补不同帧、多次往返、首次空 Flow、未提交草稿及失败原子性断言，Debug/Release 串行专项与受影响 UI 回归，更新独立 H2 Release 部署并核对 EXE 哈希。PAE/工程整理不派新工作；交付停止，待总控复核后仅复验 Flow 切换。无 Git/发布/删除授权，不修改本机 Qt。

H2 回收：Lab 已停止写入，总控完成限定路由/接线/适配源码、最终日志及部署目录核对，独立复算部署 EXE 与 bin EXE 哈希一致。最终 H2 Debug/Release 各 23/23，旧 Binary 各 2/2、H1 static 包外 D/R PASS、Testing-off 0 test、部署双文档 smoke PASS；总控未重跑构建测试。一次 Debug ASCII 重开编辑器烟测失败后复跑通过，根因未确认，保留观察项，不称已修复。当前未发现阻断简短人工烟测的事项，人工仍待用户执行；不代表整个 Lab 迁移或包外 Qt 消费完成。只验合法帧/高亮、失败恢复、Flow 保持及正常程序关闭，不重复复杂取消事务。详细证据见 H2 validation；无 Stage/Commit/Push、发布或删除。

H2 路由拍板 A：用户明确同意后台 compile worker 进行严格、仅 dispatch-only 的顶层 `schema_version` 分类；这是对原“不得解析 Schema”约束的唯一限定例外，不授权解释字段/布局/协议语义。分类后每次请求仅调用一个目标编译器，Binary 0.9 从首次打开到 Apply 都公开消费，旧分支仍 private。分类不表示配置有效，目标编译器完整验证；不得 public 失败后 private 重试。允许在 UI 工具目录新增独立 classifier helper 及测试和最小依赖接线，优先复用已随仓 JSON 解析能力，不新增第三方库或扩 PAE API。

分类须识别真正的根对象成员，正确处理 JSON 字符串转义/嵌套，拒绝顶层重复 schema_version（包括转义后同名）、缺失、错误类型、语法错误、超限和不支持/不可可靠分类的版本；禁止正则/子串检索、默认版本或宽松转换。不要用会静默覆盖重复键的 DOM 结果作为唯一可靠依据。错误明确标为输入分类失败，不伪称目标编译器已执行。遵循现有输入大小及有界解析约束，不产生绕过严格配置检查的旁路。增加上述负例、字符串/嵌套伪造 version、支持分支各一次编译且不 fallback 的 D/R 测试，保持取消/迟到 completion 语义。未知新版本不擅自路由。

原报告提及的 wire/byte-order/conversion/source 展示差异仍需逐项列出：可从 public metadata 等价映射的保留；没有公开事实的报告总控，不以“压缩展示”删减已验收信息。本次只放行 dispatch 分类，不批准 B 的 Binary 首次打开 private 过渡方案，不新增用户选模式步骤。Lab 恢复 H2 契约及实施，其余范围和停点不变。

用户授权规划并派发 H2。由《Lab应用推进》独占 Binary Schema 0.9 已启用的完整记录 Decode UI 切换，消费 H1/public PAE；PAE 与工程整理停止。先落 `lab-public-api-stage4-h2-contract.md` 明确接线与兼容策略，再在同一派发内实施；不重做总体规划，不先做全面人工验收。

- 保持未绑定/显式 Apply、已有 binding/Pipeline/Flow 选择、typed/raw/logical、byte/bit 高亮、失败清旧、取消/重载/迟到 completion/两 Tab 关闭事务。H1 的单 endpoint/two Flow 形状不能被用来删减现有 UI binding 能力；需要多 binding 或描述复制等，允许对 H1 做最小上层扩展，冻结状态及身份由公开 API 承接。公开 metadata 转 Lab 自有展示类型，不让 Binary 专属生产头依赖 private enums/types。
- 允许改 `tools/protocol_lab_ui/` 中 binary_host_adapter、compile_worker、document_session、document_tab、application_window、description_mapping、ui_field_result、field_table_model、hex_view 的相关 .h/.cpp 与 CMake；新增 Binary 专属公开描述映射 helper。允许最小扩展 `tools/protocol_lab_binary/public_binary_decode.*` 以接收已编译 owner、提供所需自有描述/多 binding，及其 CMake；对应 `tests/protocol_lab_ui/`、H1 tests/consumer 和 CMake、新 H2 契约/validation。根 CMake/Presets 仅限定 Binary UI 推荐门禁/构建接线。不得修改 PAE public/Core/Plan/Compiler、Schema、旧 fixture、Qt/环境/SDK，或删除旧 backend/旧桥。
- 编译 worker/completion 必须先解决单次编译与分支路由：Binary 不得为取私有 sidecar 再编译一次，不得用 UI 自行解析 Schema 决定协议语义。现有 private artifacts 只继续供旧 0.5–0.8/ASCII 路径。允许明确 request/completion variant；若无法在保持既有配置打开方式、旧分支行为及 PAE 不变的范围内路由，应先给总控具体阻断与最小选择，不能强行双编译或暗增用户选模式步骤。
- 同一次 Decode 只走 H1/public Host；不调用 private backend 做对照执行，不保留两个可见执行路径。不启用 Binary Encode/stream，不迁移 ASCII/0.5–0.8。共享 UI target 可仍因旧分支链接 private 库，但 Binary 专属 adapter/映射源与 target 应可审计为仅公开消费；不以聚合链接掩盖 Binary 私有依赖。描述不足时报告缺口，不猜数值或反算 raw。
- 保持 prepare→预算→确认→move 发布与失败原子性；覆盖 compiled/Host/H1/Qt DTO/旧实例/待发布副本的逻辑共存计费，六维请求身份及取消/迟到拒绝保持。物理查询仍仅是布局事实，成功值与位置必须同 owner/message/frame 关联。测试 hooks 仅测试配置。Decode 展示按公开 logical kind 与 decoded 来源呈现，不把只读结果伪装为可编辑输入。
- 新 `out/build/windows-msvc-stage4-h2-*` 与 `out/stage4-h2-validation/`；沿用仓库 Qt/v142/x64，D/R 串行，针对 Binary UI 与受影响 worker/session/共用展示测试，H1 改动则重跑 H1 和 static D/R consumer。回归旧 Binary/ASCII 的受影响路径及默认/缺依赖/Testing-off 门禁；不重跑所有旧协议矩阵，不改 P SDK。测试覆盖单次编译/Decode、类型/位置、失败清旧恢复、多 binding/Flow、预算失败/旧 completion、取消 Apply/Reload/Close 与两 Tab Yes→No。
- 交付一个依赖部署齐全的 Release 本地运行目录及完整启动命令，使用随仓 Qt 的既有部署方法，不改本机 Qt 或全局 PATH，不复制系统 DLL。验收说明最多三个用户烟测动作组：合法帧与高亮、失败后恢复、Flow/Tab 保持及关闭。精确列出 exe/config 绝对路径、输入、按钮、预期；复杂关闭/取消事务由自动测试覆盖。任务本身不启动用户 UI、不宣称人工验收，待总控核对后由用户执行一次简短烟测。
- 新增 `lab-public-api-stage4-h2-validation.md` 区分实现、自动证据、待人工范围。完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次交接摘要，停止写入。超范围缺口先报，不自行派其他任务；无 Stage/Commit/Push、发布、目录删除授权。工程入口同步与清理在交付稳定后另派。

### H1 实施派发（2026-09-15）

H1 补齐后限定收口：Lab 已停止写入，总控核对最终无条件 Account 门禁、两份保留草稿加 pending 的峰值预留、实际 capacity 检查及所有权文档修正，并阅读 `controller-review-*` 的 H1 D/R 各 1/1、static D/R consumer PASS 日志。零字段 Binary 由现有 Compiler/Plan 拒绝，不能把此前潜在路径疑点记为已复现的合法输入缺陷；实际复现并修正的是草稿峰值账本遗漏。当前未发现阻断下一片的事项，H1 完成非 Qt public-only Decode 的限定复核；总控未重跑构建/测试。H2 Binary UI 切换尚未派发，现有 UI 仍使用旧 backend。无 Git/发布/删除授权，证据见 H1 validation 的总控补齐段。

H1 首次回收：Lab 已交付 public adapter/DTO、独立 target、D/R 专项与 static 包外 consumer；总控阅读契约、完整 adapter、测试片段及日志，尚未通过。已派回 Lab 定向补齐：成功 Output 的最终 Account 上限应无条件检查（当前仅字段循环内，需合法零字段向量复现/边界核对）；两 Flow 旧草稿与 SetDraft pending 共存峰值的逻辑预留；契约中 compiled/Host 所有权及销毁顺序错误表述。限原 H1 文件，先复现再修正，D/R H1 与包外 static consumer 复验；不重跑未变 PAE/UI 矩阵。PAE/工程整理停止，H2 未启动，无 Git 授权。

用户授权下一步规划并派发。P 已限定复核，现由《Lab应用推进》独占 H1：新增 public-only 非 Qt Binary complete-record Decode adapter/自有 DTO，先落 `lab-public-api-stage4-h1-contract.md` 后直接实现，不再只交报告。PAE 与工程整理保持停止；H2 UI、Encode、stream、ASCII、旧桥删除不在本片。以本段覆盖下方历史“尚未派发”。

- 生产头仅标准库与 `pae/**`，独立 target 仅声明 `PAE::pae` 与通用编译选项。编译、metadata、Host 执行和物理查询使用公开 API，不读取 Schema/Plan、不调用旧 backend 作执行补丁。旧 backend 继续服务现有 UI，新 adapter 仅接 H1 测试/consumer；不是第二套协议引擎。
- 同一成功 callback 内，使用创建 Host 的同一 compiled owner、record message、candidate frame length 查询物理范围并复制 typed/logical、实际 conversion raw、BYTES、mask 和帧。禁止重复 Decode、logical 反算 raw、借用 view 逃逸；缺少可证实失败 message 时标未知，协议失败清旧成功字段/高亮。零 payload 不高亮。查询/物化不一致整次失败，不裁剪伪造。
- 保留两个 Flow 的草稿/当前结果隔离、公开 Handle/Reset 代次与故障语义；准备和副本按现有逻辑预算有界准入、发布原子性，区分 PAE 共享 owner 与 Lab 副本，保留真实 consumed/status/reset_required。H1 不复制完整 Qt Tab/window 状态机；其 lifecycle 在 H2 延续。对本地输入拒绝、协议失败、复制失败逐类明确保存/清除策略并据既有契约测试。
- 允许新增 `tools/protocol_lab_binary/` public adapter/DTO/helper 并最小调整其 CMake；`tests/protocol_lab_binary/` 新专项、consumer 及 CMake；根 CMake 只增加默认 OFF 的 H1 门禁/子目录测试接线，H1 不要求启用旧 materializer/private Host target。不改已有 backend 执行语义、公开 PAE/Core/Plan、UI/v06/ASCII、Schema、既有 fixture、Qt、SDK 或打包脚本。新契约和 `lab-public-api-stage4-h1-validation.md` 由 Lab 写，索引/roadmap/AGENTS 总控独占。新测试内允许合成独立向量，不修改旧 fixture。
- 验证：新 `out/build/windows-msvc-stage4-h1-*` 与 `out/stage4-h1-validation/`；沿用 v142/x64/匹配 CRT，D/R 串行。覆盖六类型/enum known与unknown/Decimal实际raw、byte/bit、bounded零/最大、integrity/computed位置、SUM8/CRC失败清旧恢复、两Flow、所有权和预算 exact/minus-one。生产无 test hook；验证公开头自包含/无私有 include/link，默认 OFF/缺 public 依赖明确失败及 Testing-off 0 测试。测试不得以旧 backend 作为唯一 oracle；用独立期望。
- 仓库外 static D/R consumer 必须实际编译并调用新的 Lab adapter，而不只是重跑 PAE SDK 示例；仅暴露新 adapter 源/头与 `out/sdk-stage4-p/candidate2-20260915/` 对应安装包，不提供开发仓库 private include/targets。源码开发链接 `PAE::pae`；不重打已验证 P 包，不要求本片再跑 shared 全矩阵。不运行 UI 或新增人工验收。
- 达到 H1 自动验证出口后停止，交总控复核后另派 H2；遇到公开接口缺口、跨范围修改或无法保留已确认行为先报，不自行修改 PAE 或简化功能。无 Stage/Commit/Push/发布/删除授权。完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次完整交接摘要，停止写入，等待复核。

### P 首片实施授权（2026-09-15）

P 回收与限定复核：实现任务已交付并停止。总控阅读正式查询契约、公开纯值类型、查询实现及长度/位映射 helper，核对专项 D/R 各 31/31、public D/R 各 8/8 和五次包外 consumer 成功日志，并独立复算 `out/sdk-stage4-p/candidate2-20260915/` 五包 184 条 SHA-256 全部匹配；未重新构建或运行测试。当前未见阻断 H1 的问题，P 完成限定技术复核。接口仅回答布局事实，未增加 Codec/Host 执行、Core/Plan/Schema 或 UI 能力；既有 C4251/实验 C++ ABI 边界仍在。下一步为 Lab H1 public-only 非 Qt complete Decode，尚未派发；H2 不提前接 UI。证据见 `pae-public-api-stage4-physical-query-validation.md`。无 Stage/Commit/Push 或正式发布。

用户授权继续规划并推进。现在启动 P，不再停留于报告：由《子任务推进》先将下列限定契约写入 `pae-public-api-stage4-physical-query-contract.md`，随后在同一派发内实现并验证。Lab H1/H2 与工程整理保持停止，P 交付经总控核对后另派。以下授权覆盖前文报告批的“不得构建/改代码”，但仅限 P 指定范围。

- 公开应用无关的 Binary 表示类别、固定 byte range、物理 byte/mask、固定/有界 BYTES 长度上下界、记录长度边界及 integrity/computed-length 存储位置；支持按给定帧长解析有界布局。返回纯值、固定有界 mask 数；不暴露私有 Plan/Qt 类型，不新增查询堆分配，不改 Core/Plan/Schema。
- 按长度解析只验证布局长度条件，不验证帧内容、匹配、Gate 或完整性，不命名/描述为成功 Decode 的证明。Lab 后续在同一成功回调内使用同一 compiled owner、message index 和 candidate frame size 关联；本片无需新增执行身份 token。失败候选不能据布局查询制造成功字段。
- 位字段只返回非零物理 mask，不把容器范围伪称字段占满字节；字节范围用 offset/length；零长度 payload 是有效空范围，可选存储缺席要与 offset=0 区分。非法 owner、索引、帧长、非 Binary 查询均显式失败，无部分有效结果。表示类别查询可识别 ASCII，但 ASCII 物理布局不在支持范围。遵守现有 compiled metadata 寿命契约，数值结果可独立复制、索引只在来源 owner 内有意义。
- 允许写入 `include/pae/protocol_description.h`、`include/pae/compiler.h`、`src/public_api/compiler.cpp`（必要时拆本目录 helper 并调整其 CMake）、`tests/public_api/`、`examples/public_api_sdk_consumer/`、上述新契约及 `pae-public-api-stage4-physical-query-validation.md`；根 CMake 与 SDK 脚本/安装规则仅允许不可避免的本片测试或白名单接线，须列明理由。不改 Codec/Host/Core/Plan/Loader、Lab、Qt、Schema、既有 final6 包，不增 Encode raw、完整约束模型或 TLV。
- 定向测试先行：独立期望覆盖大小端/lsb0/msb0/跨字节 masks、fixed/bounded 零中最大、动态/固定 integrity 和 computed 存储、非法索引/owner/长度/ASCII、重复查询零分配；覆盖成功 Host 回调关联但不重复 Decode。Windows x64/v142 D/R 串行跑新增及受影响 public 回归，生产 Testing-off/公开头边界；复用现有打包入口生成新 `out/sdk-stage4-p/` 候选，不覆盖旧包。新接口静态与动态 D/R 包外 consumer 各一次，源码包脱离开发仓库构建消费一次即可，不重跑未改配置门禁及全部旧 UI 矩阵。记录实际工具链、哈希与动态导出/运行；日志与构建用新 `out/build/windows-msvc-stage4-p-*`、`out/stage4-p-validation/`。
- P 交付后立即停止，不自行启动 H1、删除旧桥或正式发布。超出上述实现边界先报告。无 Stage/Commit/Push；完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次交接摘要，停止写入，等待总控复核。

报告回收：两项任务均已交付并停止写入。总控已阅读 `pae-public-api-stage4-observation-gap-design.md` 与 `lab-public-api-stage4-binary-migration-plan.md`，完成迁移顺序的限定合并；本批未实施、构建或测试。下一实施顺序为 P（PAE Binary 物理描述/按长度解析布局）→ H1（public-only 非 Qt complete Decode 适配）→ H2（Binary UI 切换），三片串行交接，不并发猜测公共接口。旧 backend 在 H1 期间只服务既有 UI，新 adapter 不接用户入口；不双执行、不删除旧代码。

合并约束：P 必须区分“按冻结 Plan 与所给帧长计算的布局”和“本次 Decode 已验证的执行结果”。按长度查询本身不能证明帧有效或校验成功；Lab 仅在同一次成功 callback 内以其 message/frame 与同一 compiled owner 查询并复制，自有 DTO 保留一致身份，失败不得发布成功字段/高亮。明确非法索引、帧长、非 Binary 支持边界和元数据寿命，不能直接把报告建议当已实现契约。

Encode 暂缓：两报告对后续 Encode 观察的必需程度表述不同。当前 headless Encode 的转换 raw 原本未观察，新增实际 raw 不能作为“保留已有行为”的前置条件；物理布局可由 P 承接，生成字段及故障恢复的剩余需求留待 Encode 专片按源码逐项核对。首片不新增 Encode UI、流式 UI 或 Core raw accessor。H1 先用源码 `PAE::pae` 开发，接口交付后以新静态 SDK 做包外 D/R 闭包；final6 原包保持不变。实施片尚未派发，本段只记录报告合并与后续顺序。

用户授权按现有规划推进并派发。先 Binary，后 ASCII/流式，再清理失效旧桥。TLV 仅为未来参考，本轮不新增 TLV、数组、嵌套、Schema 能力或稳定 ABI 目标。阶段 3 final6 保留为已验证基线，不提前重打包或改变已交付包。

本批是两份有明确出口的迁移准备报告，不重做总体架构，也不再次盘点全部历史。两任务并行读取现场源码，分别写独立报告，不等待对方，不互相派发。核心问题是现有 Binary Lab 行为能否由公开 API 无损承接，而不是仅换 include/target 名称。

- 《Lab应用推进》唯一写入 `docs/engineering/lab-public-api-stage4-binary-migration-plan.md`。从当前实际可用 Binary 路径和已验收行为列出消费矩阵：编译/metadata、字段类型/输入、Decode 成功及失败、raw/logical、物理 byte/bit 高亮、Encode 同调用观察（区分现有启用路径与未启用代码）、绑定/Flow/Tab/关闭取消/重载与资源生命周期；逐项对应旧私有接口、可用公开接口和确定缺口。给出首片迁移文件范围、根 CMake/旧桥依赖切断方式、推荐源码/静态/动态消费方式，以及“不再私有 include/链接”的检验方法。建议以 SDK 支撑 headless Binary 适配再接 UI，若共享 ASCII/旧桥令此顺序不成立需列具体耦合和最小替代。不删减已验收能力来制造迁移成功。
- 《子任务推进》唯一写入 `docs/engineering/pae-public-api-stage4-observation-gap-design.md`。从当前 public metadata/Codec/Host 与内部执行事实核对物理范围、Encode 同调用结果、必要输入约束等缺口；区分已有能力、Binary 首片必需、可延期。对必需项提出一套最小应用无关 API 及其内部事实来源、借用寿命/失效、失败状态、预算和 DLL 导出影响；只暴露已有事实，不重复 Decode、不从 logical 反算 raw、不让 UI 解析 Schema。必须说明是否需改变 Core/Plan，若需则标明待总控判定，不自行实现。分析含适用时的 SDK 增量复验，不重建全部历史矩阵。
- 总控合并两份报告并确定单一最小契约/实现片。若必须补 PAE，PAE 独占公共接口/构建修改 → 交付及总控核对 → Lab 迁移；接口稳定后才能并行写入无交叉文件。若无需补则直接派 Lab 首片。重大语义变化、兼容破坏或功能取舍列给用户；常规内部命名不重复请求拍板。
- 验证计划以受影响自动测试、包外消费和最终一次简短 UI 烟测为主；本批不构建、不运行 UI、不要求人工截图。报告必须区分当前源码分析与历史测试证据。达到报告出口后停止，不自发实施或循环复核。
- 《PAE工程整理》保持停止，落地文件稳定后另派入口同步；总控独占本计划、索引、roadmap、外层 AGENTS。无 Stage/Commit/Push、正式发布、目录删除或 Qt/全局环境修改授权。完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次完整交接摘要，停止写入，等待复核。

## 阶段 3 历史：本地独立交付已限定收口（2026-09-15）

最终结论：源码、静态 D/R、动态 D/R 五个 final6 目录及包内消费入口已交付；工程快速指南和两层 docs 索引已同步，总控核对正文与已有证据，并纠正指南将工程最低 CMake 版本混作 VS 2026 已验证版本的表述（现场 Cache 为 4.3.1）。各执行任务停止写入，阶段 3 完成本地交付限定收口，无新人工验收。最终路径 `out/sdk-stage3/final6-20260915/`，开发入口 `docs/guides/03-Windows-SDK集成.md`。源码/二进制消费及错误配置验证的精确证据继承关系见下文和验证报告，不代表正式发布、稳定 ABI、Linux、Lab 迁移或生产验证。下一步候选为阶段 4 Lab 公开 API 消费迁移，尚未派发；无 Stage/Commit/Push。以下保留回收过程。

最新结论：final6 为当前候选，PAE 已停止；总控阅读最终 Config、final5 六组匹配消费 PASS 与四组错配稳定标识拒绝日志，并独立复算 final6 的 184 条哈希、179 项清单大小、166 个功能输入与 final5 相同，完成限定技术复核。final6 只规范化三份示例 README 及包元数据，功能运行沿用 final5；此前公开回归 D/R 7/7 和 DLL 检查保留各自来源，不冒称 final6 重跑。错配在链接期通过明确命名的缺失输入拒绝，不是 configure 自动校验任意 CRT/toolset，也未验证任意自定义配置。final2～final5 保留。Lab 已完成 final3 消费复核，其 API/二进制边界结论限定继承，最后配置门禁由总控核对。

已派《PAE工程整理》仅补开发者 SDK 快速入口及两层 docs 索引；代码和包保持停止。完成入口同步后最终收口；阶段 4 Lab 迁移尚未派发。无 Stage/Commit/Push、正式外发或新增人工验收。

用户授权规划并推进。本轮为本地工程交付与验证，不是正式发布；阶段 2B 已限定收口。保留共享工作树全部既有变更，不 Stage/Commit/Push，不迁移 Lab，不清理目录或修改本机 Qt/全局环境。

并行回收：《PAE工程整理》已交付 [SDK 内容与许可证盘点](pae-sdk-stage3-content-review.md) 并停止写入；总控已阅读报告，作为最终 manifest/依赖闭包核对输入，不视作包验证通过。PAE 实施仍在运行，Lab 尚未启动。报告反映盘点时构建状态，最终以实施后的代码与包验证为准；源码必需的 test_support 声明头与启用产品测试 hooks 应分别判断，能力文档不得仅凭 Schema 版本范围泛化支持承诺。

### 范围与交付标准

消费复核回收与配置门禁：Lab 对 final2 的复核未发现新的链接闭包/公开头越界阻断；指出单配置包使用通用 IMPORTED_LOCATION，VS 多配置消费者可误选 D/R，尚无自动拒绝证据。总控选择最小 target 级专包配置门禁并派 PAE 实施/负向验证，不强改父工程全局配置或 CRT；同配置继续可消费，错配明确拒绝。该补齐不扩展稳定 ABI，不消除 C4251；class-level export 的 private helper 符号属于当前实验 C++ 导出面，不能仅凭四工厂导出就宣称精确冻结导出列表。Lab 报告为 final2 快照，final3 补入口及后续门禁需按新证据核对。

final3 补齐回收：PAE 已停止，三类包统一附 `examples/sdk_consumer` 与 Configure/Build/Run 命令。总控阅读新包内说明、6 次包内入口 D/R PASS、Testing-off/DLL 自动复制日志，并独立复算 final3 全部 SHA-256、确认 16 个二进制与 final2 完全相同；未重新构建。已将 final3 增量送 Lab 复核，Lab 仍在工作，暂不最终收口或启动阶段 4。定向 7/7 沿用二进制/实现未变前提下的既有结果，不冒称 final3 重跑。

实施回收及补齐：PAE 已交付 final2 三形态五目录（源码、static D/R、shared D/R），报告为 `pae-sdk-stage3-windows-validation.md`。总控静态核对安装/导出/打包代码、6 次外部 consumer PASS、D/R 公开回归各 7/7 日志，并独立复算五包 176 条 SHA-256 全部匹配；未重跑构建。发现包内 README 缺少契约要求的可执行命令、二进制包未附最小 consumer，已限定派 PAE 补包内入口和新 final3 清单/哈希，并按包内说明复验消费命令，不改变执行 API。同步派 Lab 只读复核 final2 消费边界，唯一报告 `lab-sdk-stage3-consumer-review.md`；这不是 Lab 迁移。工程入口同步待补齐结果，不提前宣称阶段 3 收口。旧 final2 和失败日志保留，后续证据不得混称。

- 三种形态使用同一套公开 C++17 API 和 `PAE::pae`：源码包可脱离开发仓库离线构建；静态包具有完整传递链接闭包；动态包含 DLL、import lib、公开头和可重定位 CMake 配置。消费者不需理解或手工链接内部模块。允许最小必要的构建拆分，不要求为单一 archive 大范围重构。
- Windows x64 沿用现场已验证 MSVC kit，Debug/Release 分别记录工具集、CRT、架构、配置及兼容限制；不承诺跨编译器或稳定 C++ ABI。DLL 使用明确公开导出，覆盖非内联成员与析构；不得靠导出全部内部符号伪装边界。产品关闭测试 hooks，不带 Qt/Lab。
- 采用可审查白名单：公开头、必要私有实现与构建闭包、yyjson 及许可证、Schema、公开示例/合成配置和使用说明。排除 Lab、Qt、测试框架、spikes、开发产物、Git 和工程历史文档。名称带 ui_description 的现有编译元数据须按实际职责判断，不能仅按名字删除必要依赖。
- 包内提供清单、内容哈希、版本/工具链与来源记录。当前源码有未提交变化，必须标注 base HEAD 与 dirty 状态，不把包冒充该提交的纯净产物。缺少 PAE 发布许可证只记录为对外发布门槛，不擅自选择开源许可证；保留第三方原许可证。本轮不上传、不签名、不打安装程序、不复制系统 DLL。
- 在开发仓库外新建专用临时验证目录，迁移/展开三类包后使用同组 public-only consumer 验证 Compiler/metadata、Codec、Framer、Host（含同 Handle 多消息 Encode）。源码形态不得回读开发仓库或联网取依赖；二进制通过 find_package 消费，不访问私有头。D/R 分别通过，检查 CMake 路径可重定位、DLL 导出/运行依赖以及包内容排除项。只做受影响公开 API 定向回归，不重开整套 UI 人工验收，不声称 Linux/真实协议/生产验证。

### 派发与同步门槛

1. 《子任务推进》独占实现：先落盘 `docs/engineering/pae-sdk-stage3-contract.md`，随后在同一授权内实现。允许根 CMakeLists/CMakePresets、cmake 下新增 SDK 文件、src 各相关 CMake 的最小链接/导出调整、include/pae 导出标记、src/public_api 必要导出配套、scripts 下新增打包/验证脚本、tests 下新增 SDK 消费测试、examples 下新增 SDK consumer、`docs/engineering/pae-sdk-stage3-validation.md`；不改执行算法/Schema/既有 Lab。超范围缺口先报。专用 `out/build/windows-msvc-sdk-stage3-*`、`out/sdk-stage3/`、`out/sdk-stage3-validation/`，不得覆盖前片证据，D/R 串行。
2. 《PAE工程整理》并行盘点交付白名单、文档和第三方许可证；唯一写入 `docs/engineering/pae-sdk-stage3-content-review.md`。不改构建/代码/索引，不构建，不等待实现半成品；其结论交总控整合，避免与实施互相依赖。
3. PAE 实现与工程盘点交付并停止 → 总控核对实际包及证据 → 再单独派发《Lab应用推进》消费侧只读复核与工程入口同步。Lab 当前保持停止，不提前迁移或消费变化中的包。
4. 总控独占本计划、roadmap、索引和外层 AGENTS。各执行任务完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次完整交接摘要，停止写入，等待总控复核；派发不等于验证完成。

## 阶段 2 历史：2B 公开 Host 已限定收口（2026-09-14）

最终结论：PAE 实现、Encode selector 阻断修正、Lab 针对性复核和工程入口同步均已交付并停止写入。总控核对最终 API/selector/最大容量实现、D/R Host 各 56/56 与定向 CTest 各 10/10、同 Handle 多消息 external consumer PASS 证据及示例命令，完成 2B 限定收口；本次为源码与已有日志复核，没有重跑构建。原 52/52 保留为修正前历史。下一步为阶段 3 Windows x64 源码/静态/动态独立包的布局与构建交付，尚未派发；不表示 SDK 已发布、Lab 已迁移或生产可用。无 Stage/Commit/Push。以下记录为派发/回收历史，以本段为当前状态。

最新状态：Encode 调用期 selector 修正已交付并停止。总控核对新 selector/最大容量实现及 D/R Host 各 56/56、外部 consumer 同 Handle 多 Message PASS 日志；未重新构建，未最终收口。已并行启动 Lab 原报告的针对性复核追加与工程整理五个目录 README/新 public_api_host README 同步，独立写入；PAE 保持停止。下文为本片回收过程。

回收状态：PAE 实现已交付并停止；总控核对 D/R Host 专项各 52/52、定向 CTest 各 10/10 日志及部分公开接口/Handle/异常/Reset 实现，未重新构建，尚未收口。已派 Lab 只读消费复核，唯一报告 lab-public-host-stage2b-consumer-review.md。当前实现将 Encode Message 固定于 binding 创建、调用无 selector，与原设计调用期选择 Message 有差异，需重点判定同端点多消息消费影响；未接受此差异作为新契约。工程 README 同步暂缓至接口结论确定，PAE 保持停止。

复核结论与修正派发：Lab 已确认创建期固定 Encode Message 是确定消费阻断，其余范围未发现第二项确定阻断。总控已派 PAE 恢复 Pipeline-bound 模型：binding 不固定 Message，Encode 调用期传 public message_index 并验证归属/动作；创建按 Pipeline 所有可 Encode Message 的最大可靠输出上界预分配和计费，同 endpoint+action 仍只有一个 channel。补同 Handle 多 Message、不同容量、错误选择拒绝、异常/Reset 后再选择的 D/R 与外部 consumer 证据；原 52/52 是修正前历史。禁止以不同伪造 endpoint 或多 Message channel 改写身份/故障语义。Lab 与工程整理均停止，修正后再决定后置放行。

用户授权进入 2B，并授权待决策项采用总控建议。PAE 独占实现和构建；Lab/工程整理保持停止，交付后由总控另派后置复核和入口同步。本节为 2B 实施约束，不将设计报告中的可选方案同时实现。

### 已确认选择与最小范围

- 复用公开 Codec/StreamFramer 及同一冻结 compiled state，新增应用无关 Host 组合层，不公开内部 Plan，不复制 Matcher/切帧算法，不附带通信、线程或 Qt。包含 immutable bindings、完整记录 Decode、stream Push/Continue、Encode、Find、Reset、Observe 和资源报告。
- 身份为调用方不透明非空字符串 key，创建时按字节精确比较并复制保存，不做大小写/Unicode 归一化；绑定唯一键为 endpoint+action。Pipeline/Message 使用公开 index 并验证真实归属/动作能力。Decode streams 显式有界，Encode 一个通道；不把 endpoint 写入 Schema。
- Handle 为不透明非拥有的实例 scope+通道+generation 身份，拒绝 foreign/expired/stale；Reset 成功递增目标通道 generation，旧 Handle 失效，调用方重新 Find。Reset 失败不伪造新代，generation 耗尽失败关闭。Host 首片不可复制、不可移动；销毁不得与操作/回调并发。旧 Host 销毁后其 Handle 不得别名新实例。绑定发布后不可变，无在线解绑、热替换或状态迁移；上层自行准备新实例再切换。
- 可抛异常的同步函数指针回调，由 Host 捕获，不穿越 Framer noexcept。CandidateObserver 可选、在同一次 Decode 后先调用，成功和失败候选均可观察；失败没有成功字段视图。业务 sink 只交付成功（含零字段成功），每候选最多一次 Decode；原帧、typed/raw 与状态来自同次结果且仅回调内借用。
- observer 正常 STOP 仍交付当前成功业务一次，失败永不进业务；business STOP 提交当前成功后停止。observer 异常抑制当前业务，business 异常不重试已调用回调；二者停止后续、保留真实 consumed/已发生计数，标记目标通道 RESET_REQUIRED，其他通道不受污染。明确 decode_success、callback 正常返回成功数和 observer 正常返回数，不声称回滚用户回调副作用。Encode 回调异常同样只故障其通道，显式 Reset 恢复。
- 单 Host 操作串行、非等待 guard 在一切可变调用状态前获取；同线程活动回调链含 A→B→A 统一 REENTRANT，跨线程竞争 BUSY；非法输入优先级明确。Observe 不承诺并发快照。空 Push 等价 Continue，不 flush；精确后缀仍归宿主，不隐式复制、循环排空、重放或重复 Decode。
- 创建全量验证并原子发布，无半实例；绑定/通道/身份长度/聚合计费均有界，溢出和分配失败关闭。计费纳入实际 capacity、Host facade、子 Codec/Framer 和预分配 Encode 输出，共享 compiled state 单列避免按流重复计费；scope 控制块边界明确。不宣称 allocator/RSS 硬上限，新旧 Host 共存峰值由应用计入。热路径不新增引擎堆分配，创建失败逐点测试。优先只读私有 helper 获取既有资源事实；如不能可靠准入先报，不隐式扩大 Core/Plan 算法。

### 派发、验证与停点

- 《子任务推进》先写 docs/engineering/pae-public-host-stage2b-contract.md，明确 API/状态优先级/Reset 新 Handle 获取/预算公式，再在同一授权内实施，无需为内部命名重复请示。允许 include/pae、src/public_api、tests/public_api、新 examples/public_api_host；必要根 CMakeLists.txt/CMakePresets.json 接线；上述新契约、新 pae-public-host-stage2b-validation.md、原 Framer/Host 设计状态对齐。不得修改内部 Core/Plan/Framer/Host 算法、Lab、Qt、Schema；确需越界先报。
- 专用 out/build/windows-msvc-public-host-stage2b，日志 out/public-host-stage2b，沿用已验证 Windows x64 工具链，D/R 串行。覆盖身份重复/越界/能力/动作、完整/流式 Binary 与 ASCII、Encode、候选失败/零字段成功、observer/business STOP 和异常精确消费、Reset/stale/foreign/expired/generation、多流隔离、owner 保活、直接/间接重入及 BUSY、预算边界/失败原子性/无热分配。
- public-only 仓库外 consumer 验证绑定+分块+观察+成功交付+Reset 恢复及 Encode，不包含私有头、不依赖 Qt/Lab。定向 Codec/metadata/Framer 与受影响内部 Host 回归，记录实际支持域；不要求全仓/UI 人工/网络/Linux/Golden/正式性能验证。
- PAE 停止写入并反馈 → 总控核对 → 再明确派 Lab 只读消费复核、工程 README 同步（独立文档可并行）。后置任务不得自行开工；总控独占计划/索引/roadmap/外层 AGENTS。尚无 Stage/Commit/Push、SDK 发布、目录删除或环境修改授权。

## 阶段 2A 独立 StreamFramer 已限定收口（历史）

最终结论：实现、消费侧复核、入口同步及嵌套回调修正均已交付，所有执行任务停止写入。总控已核对活动 callback 链判定与栈上 RAII 恢复、修复前间接重入断言失败（37 成功 / 1 失败）、修复后 D/R 各 38/38 及 public 定向 CTest 各 6/6 日志，完成本片限定复核；这是源码和既有日志复核，不是总控重新构建。首轮内部 Framer/独立 consumer 等证据保留，未冒充修复后重跑。无 Stage/Commit/Push。下一步为 2B Host 契约选择与最小实现范围确认，尚未派发；不代表 SDK、Lab 迁移或生产验收。下文为派发和回收过程记录。

当前状态：PAE 实现已交付并停止写入。总控已静态核对公开头和 facade 实现，读取最终 D/R 专项各 37/37 与仓库外 consumer PASS（2 candidates / 2 Decode）日志；未重新构建，未最终收口。现已启动后置 Lab 只读复核（唯一新报告 lab-public-framer-stage2a-consumer-review.md）和工程整理（五个目录 README 及新 examples/public_api_framer/README.md），两者写入范围互不重叠。PAE 保持停止，若复核发现缺陷再单独派修复。

后置回收：工程 README 与 Lab 消费复核均已交付并停止。Lab 未发现确定状态安全阻断，但确认 A→B→A 嵌套回调被安全拒绝为 BUSY，与约定 REENTRANT 分类不一致。总控已派 PAE 限定修正：保持原契约，以无热路径分配的活动回调链识别回环，先复现再做 D/R 定向验证；更正 bytes_consumed 注释和验证证据表述。修正前不最终收口，不启动 Host 2B；不重开 UI 人工验收。37/37 为修正前证据，后续新日志单独记录。

两份设计与消费者报告已交付并停止写入。总控合并结论：先公开独立 Framer，再处理 Host；设计交付不等于代码验证。依据为 [PAE 设计](pae-public-framer-host-slice-design.md)和 [Lab 消费需求](lab-public-framer-host-consumer-review.md)。以下补充优先于两报告中的可选建议。

- 2A 只包装既有切帧能力，持有冻结 compiled state；一个实例对应一个 pipeline/逻辑流。提供公开流式能力查询、Create、Push、Continue、Reset、MemoryReport 和只读 Observe（phase、buffered_bytes、has_internal_work、有效提交/工作上限），不得由消费者解析私有 Plan 或猜测 Schema。能力查询不承诺创建预算一定充足。
- 空 Push 与 Continue 等价，沿用内部零输入推进语义，不刷新/丢弃半帧。每次仅推进一次内部调用；精确保留 consumed 前缀事实，未消费后缀由调用者持有，不隐式缓存或循环排空。STOP 已交付当前候选、不重放、不预读后缀；候选不等于 Decode 成功。
- 候选借用仅在 noexcept 回调期间有效；组合 Codec 的 BYTES 仍受候选存储寿命约束。同实例变更 BUSY/重入拒绝；facade guard 必须先于任何可变回调上下文写入。Observe 只允许在实例串行空闲时使用，不承诺并发快照；移动/销毁不能发生在操作或回调中。实例保活 compiled state，但不延长调用者输入或回调借用。
- 预算沿用已有 override/default 与容量计费事实，列清 facade/内部 workspace/共享 compiled state 的计费边界，不宣称 RSS 硬上限；不新增每次 Push 分配，不改算法。Host 异常恢复、候选观察器和绑定身份选择留到 2B，不在本片实施。
- 《子任务推进》独占写入：include/pae、src/public_api、tests/public_api、新 examples/public_api_framer；必要根 CMakeLists.txt/CMakePresets.json 接线；自身设计报告与新 pae-public-framer-stage2a-validation.md。不得改 Core/Plan/Framer 算法或 Lab；确需内部资源查询改动先报告。总控独占计划/索引/roadmap/外层 AGENTS。
- Windows x64 Debug/Release 串行，专用 out/build/windows-msvc-public-framer-stage2a，日志 out/public-framer-stage2a。验证 Binary fixed/sync/length 与 ASCII CRLF 分块、超长丢弃、STOP/精确 consumed/预算/空推进、Reset 隔离、owner 保活/移动、重入与并发拒绝、能力查询与 Observe；公开头独立编译、仓库外 public-only consumer 组合 Framer+Codec 且每候选只 Decode 一次，定向 Codec/metadata 回归。不要求 UI 人工验收。
- PAE 交付并停止 → 总控核对 → 再明确派发 Lab 只读消费复核与工程整理入口同步，后两者可按不重叠文档并行；未收到后置启动令保持停止。不 Stage/Commit/Push、不打包发布、不移动删除、不改 Qt。

### 阶段 2 设计派发记录（已完成）

用户授权推进下一步。先并行完成两份独立输入，随后由总控合并并明确派发最小实现，不要求用户重复决定内部命名；涉及行为兼容、生命周期重大取舍才提请决定。

- 《子任务推进》：只读内部 Framer/Host、公开 Codec 与已有契约/测试；唯一写入 `docs/engineering/pae-public-framer-host-slice-design.md`。给出一套推荐公开 API、内部映射、第一片文件范围及 D/R 消费者验证矩阵；推荐 Framer 与 Host 分片顺序而非同时包装所有能力。
- 《Lab应用推进》：与上项并行，只读当前 Binary/ASCII stream/Host 消费路径；唯一写入 `docs/engineering/lab-public-framer-host-consumer-review.md`。列出最小调用序列、不可丢失的候选/停止/复位/失败事实和借用需求，区分 Lab 自有冻结后缀/草稿/展示与 PAE 通用状态；不依赖另一份尚未完成的草案。
- 必须明确：compiled owner/执行状态保活，feed consumed 与未消费后缀归属，candidate 不等于 Decode 成功，候选借用期、sink stop/异常/重入、零输入继续、reset、绑定/流隔离、预算及每次调用上界；不暴露私有 Plan，不重复 Decode，不自行改变现有流式语义。
- 本批不修改代码/CMake/README、不构建、不移动删除目录、不 Stage/Commit/Push；不改 Qt，不实现通信/线程/Evidence。现有协议契约冲突先报告，不为“公开化”隐式修正。
- 两任务交付后主动向总控反馈一次并停止；总控确认接口后，PAE 独占实施和构建，Lab 后置复核。《PAE工程整理》保持停止，待落地路径确定后再更新入口，避免空转和提前移动。

## 阶段 1B 已收口：消费准备 metadata（2026-09-14）

收口状态：PAE 实现、Lab 消费侧只读复核及六份 README 同步均已交付，阶段 1B 完成限定总控复核。总控核对公开声明、Plan 映射及最终日志（D/R metadata 各21/21、Codec 各57/57、public_api 各5/5），复核报告未发现本片确定阻断项；README 已同步当前状态。上述为已有日志核对，不是总控重新构建。各任务停止写入，无 Stage/Commit/Push。

后续公开 Framer/Host 已按上节进入 2A；此处保留 1B 范围说明。物理范围、Encode 同调用观察及完整输入约束仍单列后续，不默认扩大范围。

用户授权本片规划及派发。先由《子任务推进》独占实现与构建，完成并停止写入、总控核对后，再派《Lab应用推进》消费侧只读复核与《PAE工程整理》入口同步；后两者写入互不重叠，可并行。未收到后置启动令不得自行开工、轮询或修改半成品。

- 实施范围：公开逻辑值类型（与 Codec 使用同一类型域，转换字段明确 raw/logical 区别）、Encode caller-input/生成来源及动作引用关系、Pipeline/Message 动作可用性。事实来自冻结 Plan，不解析 direction 文本、不从 Schema 版本猜测，不带 UI 专用语义。能力查询不承诺任意输入必然执行成功。
- 输出容量上界仅在现有 Plan 有可靠有界事实且无需新算法时提供；区分固定大小、上界与本次 required_size，不编造精确长度。否则记录缺口，不阻塞前三项。
- 先在 `docs/engineering/pae-public-consumer-metadata-slice.md` 记录接口语义和内部事实对应，再在同一授权内最小实现；发现协议语义变更或新增大范围抽象则停报总控。
- PAE 唯一写入范围：`include/pae/`、`src/public_api/`、`tests/public_api/`、`examples/public_api_codec/`；必要根 CMake/Presets 接线；上述新设计及 `pae-public-consumer-metadata-validation.md`。不改目录 README、Core/Plan/Schema 或 Lab。确需越界先报。
- 验证：Windows x64 D/R 串行定向公开 API 回归；Binary/ASCII metadata 与实际执行对应、常量/计算字段和动作不引用、精确转换类型、空 owner/越界/lifetime；补 unknown enum raw/known/tainted、ASCII literal-only 与 unsupported action、代表性失败位置传播直测。示例用公开 metadata 准备调用，不用私有头或类型/方向硬编码。复用专用 `out/build/windows-msvc-public-codec-stage1`，新日志 `out/public-consumer-metadata-stage1`，不得覆盖前片证据；独立消费者 D/R 增量验证。
- 后置 Lab 仅写 `docs/engineering/lab-public-consumer-metadata-review.md`，不迁移；工程整理仅更新上一批五个目录 README 与公开示例说明，不构建，不修改代码/索引。总控负责索引、恢复入口与最终状态。
- 本片不增加字段校验全模型、物理范围、Encode 执行观察、Framer/Host、SDK 包或 UI 功能；不清理目录、不动 Qt、不 Stage/Commit/Push。完成须主动向总控反馈一次并停止。

## 阶段 1 已收口派发（2026-09-14）

回收状态：A/B/C 均已交付并停止写入，阶段 1 在既定首片范围内完成限定总控复核。A 的移动赋值旧 view 误认问题已复现并以 facade epoch 修复，最终 D/R 各 47/47 专项及各 4/4 公开 API 回归通过；此前全矩阵各 32/32 不视作修复后重跑。C 未发现首片确定阻断项，详见 [消费者复核](lab-public-codec-stage1-consumer-validation.md)。这不表示独立 SDK、Lab 迁移或生产验证完成。

后续建议：先补消费准备 metadata（值类型、Encode 输入/生成来源、动作可用性，评估输出容量上界），并合并少量 unknown enum/ASCII 零字段/失败位置 facade 直测，再继续公开 Framer/Host。这是下一片候选范围，尚未派发；不把 UI 展示模型搬入 PAE，也不为非阻断证据缺口重开整套人工验收。

用户授权规划并派发。阶段 0 三任务均已停止；以下范围替代上一批的只读限制，未列明的功能与删除仍未授权。

| 工作包 | 责任任务 | 开始条件与写入范围 | 停点 |
| --- | --- | --- | --- |
| A：公开 Codec 实现与验证 | 《子任务推进》 | 立即；include/pae、src/public_api、tests/public_api、examples/public_api_codec；必要的根 CMakeLists.txt/CMakePresets.json 接线及 Core 内部资源查询；codec 设计与新增 codec 验证报告 | Windows x64 D/R 定向测试及仓库外 consumer 后交接并停止 |
| B：目录职责入口 | 《PAE工程整理》 | 与 A 并行；仅 src/README.md、tools/README.md、include/README.md、tests/README.md、examples/README.md；按现有事实和目标边界说明，不将 A 未交付内容写成完成 | 链接、路径、差异检查后交接并停止；不构建 |
| C：消费侧复核 | 《Lab应用推进》 | 不立即启动；A 停止写入且总控核对头文件、结果与证据后，再明确发送后置复核任务 | 只读核对接口与已知迁移缺口；不自动迁移 Lab |

A 独占公共头、代码、根构建接线与构建目录；B 不修改任何 CMake、公共索引、源码或 A 的示例 README。总控独占外层 AGENTS、本计划与 roadmap。所有任务保留既有变更，不相互修改文件、不自行派发下游。

A 应先落实 [Codec 设计总控补充](pae-public-codec-slice-design.md)，尤其 facade busy guard 与预分配 Encode 映射预算。构建复用已验证工具链，使用专用 `out/build/windows-msvc-public-codec-stage1`，Debug/Release 串行；日志统一 `out/public-codec-stage1`。若需兼容构建，另用专用目录且由 A 串行运行。不得并行占用已有 Lab 构建树。

同步关系：A 与 B 异步并行；A 完成 → 总控复核 → 派发 C，属于硬依赖。B 不阻塞 A 构建，但其完成与 A/C 的结果一并纳入本阶段收口。超范围缺口先报总控，不等待另一任务猜测处理。

本批不实现 Framer/Host 公开化、DLL/安装包、Lab 迁移；不移动/删除目录、不改 Qt 环境、不 Stage/Commit/Push。完成反馈仍按下述规则发送一次并停止，不循环互相唤醒。

## 阶段 0 历史派发（已交付；下文只读限制仅指上一批）

### 《PAE工程整理》：目录盘点与去留矩阵

只读盘点仓库，唯一允许写入 `docs/engineering/repository-organization-inventory.md`。
覆盖根文件及 include/src/tools/tests/examples/cmake/schema/docs/third_party/spikes/out；以目录和文件组归纳，关键例外精确到文件，不生成海量逐文件流水账。
列出当前职责、实际引用/构建依据、保留/迁移/合并/归档/删除候选、目标位置、依赖前提、所属阶段与责任任务。
空目录/年代/未跟踪状态不构成可删依据；检查可再生性、唯一内容、reparse point、Git 跟踪与忽略边界；不扫描或修改 .git 内部，不遍历链接到仓库外。
不搬文件、不删除、不改构建、不重新归档外层已删除资料。

### 《子任务推进》：完整记录公开接口首片设计

只读当前 compiler/public_api/Core/Plan 与测试，唯一允许写入 `docs/engineering/pae-public-codec-slice-design.md`。
给出一套推荐接口形状和 Binary/ASCII 两个最小调用示例，明确 compiled owner 与 workspace 的生命周期、串行/并发条件、输入输出缓冲区、typed 值、BYTES/枚举/Decimal、raw/logical、失败与诊断、预算及越界行为。
重点解决现有 move-only owner 如何支撑执行实例且不泄露私有 Plan；比较借用、内部共享生命周期、转移所有权的实际代价，推荐一套，不为未来无限扩展抽象。
不把失败后的旧成功结果暴露为本次结果，不重复 Decode 或反算 raw，不默认引入隐藏大复制；精确数值不退化为 double。
列出代码文件范围、内部适配点、CMake 接线与 D/R/仓库外 consumer 验收矩阵。不是新协议能力，也不同时设计完整 Framer/Host/DLL。

### 《Lab应用推进》：公开编解码消费者核对

只读 Lab 现有实际调用，唯一允许写入 `docs/engineering/lab-public-codec-consumer-review.md`。
从应用使用者角度列出完整记录 Decode/Encode 必需能力、典型调用顺序和最小回归；区分 PAE 通用执行事实与 Lab 自有展示/保存状态。
本轮只聚焦新增 codec 设计所需差异，不重复全仓旧评估，不要求把 Lab 全量 DTO 或 UI 缓存搬进 PAE。
不迁移 Lab、不改 public 头、不构建；第一批交付不依赖另一任务尚未完成的设计，之后由总控合并核对。

## 回收与执行规则

- 三任务各写唯一文档，不写公共索引、根 CMake、代码或外层 AGENTS；总控负责索引和合并。允许使用 apply_patch 新增各自报告，不把文档方案误报为已实现。
- 交付先由总控核对事实和设计冲突，再将阶段 1 实施独占派给《子任务推进》；Lab 与整理任务不并发修改其接口或构建。
- 不要求用户为内部命名和低风险实现细节重复拍板；发现协议语义、外部兼容、重要生命周期取舍超出已确认边界时，给出依据/方案/推荐再请求用户决定。
- 许可证、正式发布位置、Linux 真实环境、新增工具链以及不可恢复清理仍保留后续用户决策，不阻塞本批分析。
- 三任务完成后向总控 `01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次交接摘要并停止。分清未构建/未测试/未修改代码/仅报告新增，禁止 Stage/Commit/Push 和继续派发其他任务。
