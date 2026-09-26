# YAML 初版同基线交付收尾

## 基线与目标

最新收口：七 SDK 和两 Lab Release 已归集到独立 `b12ad80` 目录，总控现场重算
365 个目标文件长度/SHA-256 与归集清单全部一致，目标实有 365 文件。
总控已同步 docs/README.md 当前交付身份；归集报告的旧入口提醒已处理。
详见 [归集验证](yaml-clean-delivery-validation-20260926.md)。本轮未启动 UI 或重跑构建，
随后用户确认 static Release JSON/YAML 两条正常解析通过、Lab 已关闭，详见归集验证新增人工记录。
配置错误诊断等未测项保持边界；各执行任务停止，文档累计变更未提交，旧产物未删除。

用户同意执行交付收尾。代码检查点为 `b12ad809bdbc35589e8e5755f0e4ad04f386fa10`，
本轮开始时 main 工作区和暂存区为空。该提交与此前 `4da54b9` 均未由总控推送。
本轮总控计划文档不进入产品构建源；产品固定使用该提交的干净隔离副本。
旧 dirty 候选及其证据保留原身份，不改 provenance，不覆盖或删除。

目标是可供本地统一体验的 Windows x64 初版，不是正式发布：JSON 保持 canonical，
YAML 是可选静态附加组件；PAE 无 Qt，Lab 只消费公开 SDK。Linux、项目许可、
跨工具链稳定 ABI、生产容量及真实协议验证保持未完成边界。

## 顺序与写入分工

### 最新推进：Lab 回收与独立版本归集

Lab 已停止写入，总控读取完整验证报告、四组各 5/5 日志及部署白名单审计，
现场重算 shared D/R 部署 DLL 与原 SDK 相同，隔离源仍干净；未重跑构建或 UI。
14 配置白名单核对纠正了初次把四份测试夹具算入部署的假缺项，不删改旧日志。
验证范围见 [干净 Lab 验证](yaml-clean-lab-validation-20260926.md)。

现由工程整理独占以下全新目标（存在则停报，不覆盖）：
`deliverables/sdk/b12ad80/` 与 `deliverables/lab/b12ad80/`。
SDK 复制上述七包，目标分别命名 `pae-sdk-source`、`pae-sdk-static-debug`、
`pae-sdk-static-release`、`pae-sdk-shared-debug`、`pae-sdk-shared-release`、
`pae-sdk-json-only-static-release`、`pae-sdk-json-only-shared-release`；
Lab 仅复制本轮 standalone static/shared Release 完整部署到 `static-release`、`shared-release`。
逐文件长度和 SHA-256 核对，SDK 再运行既有包验证；包内不增删说明或修改身份清单。
原始归集日志放全新 `out/evidence/yaml-clean-delivery-b12ad80-20260926/`。

跟踪文档仅改 `deliverables/README.md`、指南 02/03，新增
`docs/engineering/yaml-clean-delivery-validation-20260926.md`；更新首选入口但保留旧版身份。
导航统一为本轮同基线新位置、相对路径、中文最简使用说明；新位置不再依赖旧 out 路径。
原 out 与旧 deliverables 不删除、不修改；不启动 UI 或重建，不自动提交推送。
归集后由总控复核，再给一次简短体验步骤。PAE/Lab 不并发写入。

### 最新推进：SDK 复核后启动 Lab

SDK 任务已交接并停止。总控现场确认隔离 clone HEAD 等于固定提交且 status 为空，
重新执行七包验证全部通过，核对九次包外运行 PASS、YAML 专项 D/R 各 2/2 及
构建/安装 Hash 对照日志；未独立重跑构建或 consumer。报告见
[干净 SDK 验证](yaml-clean-sdk-validation-20260926.md)。

现在派发 Lab 任务，仅新增 `docs/engineering/yaml-clean-lab-validation-20260926.md`，
生成物独占 `out/build/yaml-clean-lab-b12ad80-20260926/` 与同名 `out/evidence/` 根。
只读使用 SDK 任务的 `clean-source` 固定提交；通过既有 standalone 白名单脚本生成新输入，
不修改该 clone。SDK 输入为该 SDK 根的 `static-yaml-{Debug,Release}-install` 与
`shared-yaml-{Debug,Release}-install` 四包。使用仓库固定 Qt/v142，不修改本机 Qt。

- standalone static/shared D/R 新建构建与部署，开启 YAML；定向运行既有
  yaml_entry、yaml_ui_smoke、compile_queue、document_state、schema_dispatch 五项。
- static/shared Release 部署形成可运行候选，保留已有 JSON 示例及 YAML 示例；
  核对 Qt DLL/plugin 和 shared PAE DLL 实际部署字节、SDK 输入及 Lab 源快照身份。
- 不复跑仓库内组合或全部 UI 矩阵；不修改源码/CMake/脚本/SDK 包。
  JSON-only 开关的上一片证据保持有效但不冒充本轮重跑。
- 不启动可见窗口，真实 Host Apply 及人工统一体验留后续；出现失败或闭包缺口先报告，
  不为达到通过而修改固定源码或静默降低断言。
- 完成主动向总控交接一次并停止，工程整理归集仍后置。

1. **本轮启动 SDK**：《子任务推进》从固定提交建立新干净源副本，重新构建安装并打包。
   不复用旧二进制或旧安装树证明新提交构建；仅写下述隔离根及
   `docs/engineering/yaml-clean-sdk-validation-20260926.md`。
2. **SDK 复核后派发 Lab**：《Lab应用推进》使用同提交 Lab 源及本轮 SDK 构建
   standalone static/shared；实际范围和定向测试在交接后明确，本轮不提前启动。
3. **Lab 复核后归集**：《PAE工程整理》把已核对的新成品放入独立版本交付目录，
   更新中文入口与示例，不覆盖旧版；本轮仅规划，不执行复制或删除。
4. **一次统一体验**：正常 JSON/YAML 解析、错误配置诊断和真实绑定执行。
   自动测试不替代该体验；用户暂不需要操作。

总控独占本计划、YAML 计划、路线和工程索引；所有执行任务禁止交叉修改。
无 Stage/Commit/Push、系统 Qt 修改、旧产物清理或正式发布授权。

## SDK 新产物与验证范围

仅用新根 `out/build/yaml-clean-sdk-b12ad80-20260926/` 和
`out/evidence/yaml-clean-sdk-b12ad80-20260926/`。目标存在不得自动清理覆盖。
允许通过本地独立 clone 获取固定提交，记录 HEAD/clean 状态及构建源身份；
不得创建未登记 worktree，不得拉取网络或改变主仓库分支。

- 交付七包：Source、带 YAML 的 static/shared Debug/Release、JSON-only static/shared Release。
- 沿用已验证 Windows x64 MSVC v142 14.29.30133；所有库从本轮干净源新构建安装。
- Source 包真实 YAML consumer D/R，加默认 OFF 的 JSON consumer；四个 YAML 二进制包
  各运行包外 consumer；两包 JSON-only Release 各运行原 consumer。
- 运行既有包校验脚本，核对 manifest/长度/Hash/组件声明、公开头、示例、第三方通知；
  shared 消费实际 DLL 与同包一致。成品不得含 Lab/Qt 或内部试验目录。
- 至少核对 YAML 专项 D/R 及 JSON-only 请求组件的预期配置拒绝；不重跑无变化全仓矩阵。
- 记录真实命令、配置开关、退出码、日志与安装树来源链。脚本若将当前工作区误报为来源，
  从隔离源执行原脚本；仍无法正确记录时停报，不手改元数据冒充 clean。

发现产品源码、打包脚本或依赖缺口先报告，不自行修改。源码行为不变的已知 C4251
警告记录即可，不扩为 ABI 改造。完成向总控反馈一次并停止写入，待限定复核。
