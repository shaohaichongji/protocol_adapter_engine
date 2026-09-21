# 用户入口、示例与产物整理

## 当前授权与基线

2026-09-21 用户同意整理并推进。开始时 main@e2527ca，工作树及暂存区干净。
本轮不新增 PAE 执行能力，不改变公共 API、Schema、Qt 或现用部署。
无 Stage/Commit/Push、正式发布授权。七根删除后续获用户授权并于2026-09-22手动完成，详见下节；下方派发段保留历史。

## 当前收尾（2026-09-22）

入口精简、30文档归档、最小教学程序及source/static Release两包限定验证完成。
用户在PowerShell 7.6.6执行获批清理脚本，输出CLEANUP_PASS；总控只读复核七根均不存在、
148条归集目标Hash匹配。删除逻辑长度4,579,982,839 bytes，不能据此声称磁盘空闲空间净增同值。
现阶段整理累积提交候选，不新增功能，不重新构建或更改既有包身份。各执行任务已停止。

## 执行顺序与写入所有权

### 第一批：并行实施

1. 《PAE工程整理》：精简根 README，修正 docs/README、guides/01、schema/README 的过期入口；
   根 README 历史正文完整保留至 docs/archive/root-readme-history-20260921.md，修正相对链接。
   写 docs/engineering/organization-inventory-20260921.md，列出工程文档归档候选、旧构建精确清理清单、
   引用和保护对象。其他历史文档暂不移动，产物不删除。
2. 《子任务推进》：独占 examples/README.md、examples/getting_started/、
   docs/guides/04-协议配置入门.md；复用 canonical 配置，组织固定 Binary、ASCII、长度/校验、流式
   四级学习路线，实现仅消费公开 API 的最小入门程序。独立 CMake 入口，不修改根 CMake。
   使用现用 static Release SDK，在 out/onboarding-20260921 新根定向构建/运行，验证示例实际输出。
   记录 examples/getting_started/验证记录.md；不得以运行验证 consumer 代替教学示例验证。

总控独占本计划、综合计划、post-dec040-roadmap 及外层 AGENTS 当前索引。
Lab 暂不派新工作；已有左右中文工作台和执行语义不变。

### 第二批：回收后串行

- 总控检查文档链接、实际能力声明、报文字节与预期值、公开接口与借用生命周期。
- 按分类清单归档已完成过程文档，保留现行契约及修正链接；不把日期旧等同于无效。
- 教学内容稳定后再派 SDK 打包入口更新：包内中文指南、最小示例、Schema 及依赖闭包。
  不直接编辑既有包的文件、manifest、provenance。新包身份及生成范围在派发时明确。
- 清理提取必要日志并保护现用 SDK/Lab/Qt 后执行；不得整根清空 out。

## 验收尺度

### 精确删除授权（2026-09-21）

用户已授权删除 organization-closeout-20260921.md 第3.4节七条绝对路径。
工程整理独占执行：先重查范围/reparse/进程/统计/148条证据，再逐根删除并记录结果；
保护现用SDK/Lab/Qt、新教学包及当前诊断构建，不扩大到out根或其他生成物。
只允许更新收口报告、追加清理证据；无Git写操作。总控回收后复核不存在与证据完整性。

### 第三批回收与删除停点（2026-09-21）

工程整理完成30份文档迁移与索引，停止写入。总控读取收口报告和精确删除清单，
确认归档目录30正文加1索引，补齐综合计划六个迁移链接；独立复算148对归集证据的
源/目标长度和SHA256均匹配。未重跑历史测试，也未删除七根生成树。
正文全量一致性依工程整理逐份比较记录，未在总控重复全量比较；已有无关伪链接另记，
不据此宣布全仓链接零问题。下一步仅在用户确认 organization-closeout-20260921.md
第3.4节七条绝对路径后，重新核对进程/范围/证据并执行删除。无提交推送授权。

### 第三批派发：历史文档迁移与删除准备（2026-09-21）

用户要求继续。工程整理独占 organization-inventory-20260921.md 第2.3节30份历史文档，
逐份迁移至 docs/archive/engineering-20260921/ 同名文件；仅校正相对链接和添加历史说明，
不改写历史结论。允许同步仓库 Markdown 中这些文件的引用、archive/engineering索引及迁移映射，
但不改本计划与综合计划（总控回收后处理两文件链接）。若发现活跃构建/脚本依赖，停下该文件报告。
本批不移动源码，不执行Git写操作。

迁移完成后准备第3.2节精确7根清理：只读检查并归集最小配置/测试/失败与最终日志，
新证据根 deliverables/evidence/cleanup-preparation-20260921；生成原路径/新路径/hash清单，
保护现用及历史SDK/Lab、Qt、onboarding和sdk-onboarding两根。不得删除任何生成根。
新增 docs/engineering/organization-closeout-20260921.md 记录迁移映射/链接检查/证据哈希/删除候选。
删除须等总控复核及用户精确确认。PAE和Lab保持停止，不生成更多包、不构建或启动UI。

### 第二批限定回收（2026-09-21）

PAE 任务已交接并停止。总控检查打包脚本与 install 差异、教学 CMake 及中文包内指南，
读取最终两包摘要与四份 consumer 运行日志，实际独立复算 source 86 条、static Release 33 条
SHA256SUMS，全部匹配；git diff --check 无错误，仅脚本行尾转换提示。
源码包首次 install 缺少 public_api_sdk_consumer 原目录的问题由白名单补齐，最终成功日志
与首次失败证据分开保留。未重跑构建或四个 consumer，不将日志复核写成独立运行。
最终候选见 out/sdk-onboarding-20260921/packages/pae-sdk-source-final 和
pae-sdk-static-release-final；两包都是 e2527ca 加本轮 dirty 内容，不是正式发布。
本片未验证 Debug/shared/Lab，也未替换原推荐五包；文档归档与生成物删除尚待后续执行。

### 第二批派发：教学操作纠偏与包内入口（2026-09-21）

用户同意下一步。总控静态核对发现公开 Codec 示例使用 ALICE，Framer 示例固定输入为
AA 00 01 AA 00 02；教程 A/7/8 等教学向量不能写成旧程序的直接运行输出。
PAE 任务先修正这项可操作性，再独占包内中文指南与打包闭包。
允许范围：examples/getting_started/、指南04、scripts/package_sdk_stage3.ps1、
cmake/PaeSdkInstall.cmake、新增 docs/sdk/ 中文自包含指南，以及本片验证报告
docs/engineering/sdk-onboarding-packaging-validation-20260921.md。
不改根 CMake、公共接口、执行代码或 canonical 配置。

本片只生成全新 source 与 static Release 教学候选，路径为 out/sdk-onboarding-20260921；
输入为 e2527ca 加本轮工作树变更，必须如实记录 dirty 与文件清单/哈希，不宣称 clean checkpoint。
先固定白名单源码输入，再从该输入生成静态包，两包分别包外构建运行 getting_started 与原综合 consumer。
新包本地链接、配置、中文说明必须自包含，不回读原仓库；不携带本机验证日志冒充包内证据。
shared/Debug 的包装规则可同步，但本片不声明其已验证，不替换统一推荐入口或旧五包。
若需要额外源码、根 CMake 或API改动则停止报告，不自行扩展。
工程整理和 Lab 保持停止；30文档归档及7根删除本片不执行，无Stage/Commit/Push。

### 第一项回收（2026-09-21）

PAE 任务交接并停止写入。总控已阅读最小 consumer 与独立 CMake，核对只消费公开 Compiler/Codec，
查看 SDK CMake 来源及原运行日志，并重新执行已构建的 Release 程序：Decode AA 00 07 为 7，
Encode 输出 AA 00 07，GETTING_STARTED_BINARY_PASS，退出 0。未重新构建。
第一级执行闭环已核对；后三层为文档路线，未据此宣布实际运行通过，仍需文档可操作性复核。
工程整理任务仍执行中；新包、归档批量移动和删除均未执行。当前未 Stage/Commit/Push。

### 入口整理回收（2026-09-21）

工程整理已交接并停止。总控阅读精简 README 与盘点的分类/七根清单，核对 preset 名称存在；
独立比较原 HEAD README 与归档正文，235 行对 235 行，忽略链接重定位后差异为 0；
git diff --check 通过。七根约 4.265 GiB 仍只是清理候选，未提取证据、未删除。
30 个工程文档归档候选尚未迁移；172 份文档的全部断言未逐字审查，分类不是契约失效判定。
两项任务均停止。下一步先完成教学路线可操作性和链接复核，再串行安排打包与归档。

入口清晰、无过期“当前”身份；初学者按顺序能从报文字节对应到配置和公开调用；
教学程序有真实定向执行结果，未运行路径明确标注。保留原综合 consumer 的验证作用。
不新增复杂人工验收，不重跑无关 UI/全仓矩阵，不宣称稳定 ABI、Linux、许可或生产验收。
执行任务完成后主动向总控交接一次，停止写入，待复核。
