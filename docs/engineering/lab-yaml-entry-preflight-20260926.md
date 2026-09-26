# Lab YAML 配置入口只读预检（2026-09-26）

## 范围和结论

现场基线为 `main@6b2e3ee`。本报告只读核对 Lab 现有加载、编译、诊断、Host/Flow 生命周期和部署配置路径；未修改产品代码、CMake、依赖或现有成品，未构建、运行 Lab 或执行测试。总控的 `docs/engineering/yaml-entry-plan-20260926.md` 及工程索引/路线图、PAE 并行任务的写入范围均不在本任务内。

**可以在后续单独授权的实施片接入，但当前 Lab 尚不能直接消费 YAML。** 最小方向是保留 YAML 原文件作为作者源和文档身份，将受限非 Qt 前端生成的严格 JSON 留在内存，再交给现有 JSON schema 分流/编译链；现有 JSON 路径必须不变。前端接口、依赖形式、来源映射精度尚未由探针确定，本报告不预设其签名，也不把探针等同于产品能力。

## 当前链路与源码证据

| 环节 | 已确认行为及接入含义 |
| --- | --- |
| 文件身份与选择 | `tools/protocol_lab_ui/document_tab.cpp:459-466` 的标题来自路径文件名，`LoadPath` 设置原路径后触发加载；`3368-3375` 的文件对话框只有 JSON 过滤项及“所有文件”，手输路径同样可加载。未来可给受限 YAML 后缀增加明确入口，但应保持原 YAML 路径、标题、重载来源，不把内存 JSON 伪装成另一磁盘文档；不能靠失败后从 JSON 猜测回退为 YAML。 |
| 重载前门禁 | `document_tab.cpp:3428-3444` 在清理状态前确认流及 Binary Host 待丢弃内容；取消即返回。`3445-3458` 清 Host pending/配置文本并调用 `DocumentSession::BeginLoad()`。`document_session.cpp:792-813` 增加 load revision、清 prepared owner、描述、选择、预览、草稿和诊断，进入 LOADING。未来来源映射/生成 JSON 也必须受同一次确认和 revision 约束；确认后的转换失败应进入配置错误，而不能恢复旧 Plan/Flow 为当前状态。 |
| 读取与调度 | `document_tab.cpp:3460-3493` 从当前路径读入最多 4 MiB+1 字节，拒绝超限，随后直接把原字节交给 `worker_.Submit`；在 Binding UI 下还先复制到 `host_config_text_`。`compile_worker.h:29,98-115` 的请求目前仅含 document ID、revision、config_text，没有来源格式/映射；`compile_worker.cpp:445-465` 以 4 MiB 和最多两个 pending document 约束提交。YAML 需分别约束原文、解析过程及生成 JSON，并在保持有界调度/过期结果隔离的条件下决定转换放置位置；不能只在转换完成后检查大小。 |
| JSON 分流和编译 | `compile_worker.cpp:234-303` 先对请求文本求 `config_sha256`，再调用 `ClassifySchemaVersion`；公开支持的路由调用 `pae::CompileProtocolJson`，非 standalone 的旧私有路由仍调用 `CompileJsonToPlanWithMetadata`。`schema_dispatch.cpp:48-110` 本身按 JSON 解析根对象和字符串 `schema_version`。因此 YAML 必须先转换，再交给既有分流；不能直接将 YAML 投喂现有 worker，也不能把当前所有路由描述为均调用公开编译器。未来产品路径仍遵循计划中的规范 JSON/公开接口边界，不借本片迁移旧私有路由。 |
| 完成与关闭 | `document_tab.cpp:469-497` 检查文档 ID，优先分流 Host pending，并由 `DocumentSession::ApplyCompileCompletion` 发布；`document_session.cpp:816-819` 再核对 LOADING、文档 ID 和 load revision，拒绝过期结果。`document_tab.cpp:500-529` 关闭前确认可丢弃状态，随后 `worker_.CloseDocument`、`session_.Close`；`compile_worker.cpp:468-484` 删除该文档待处理/已存结果。转换结果及来源映射必须跟随同一身份与关闭语义，不能由旧转换完成覆盖新文档。 |
| 诊断 | `compile_worker.h:31-42,56-87` 的 Lab 投影有 JSON Pointer 和可选 byte offset，没有 YAML 行列；`compile_worker.cpp:307-359` 透传编译诊断并格式化为“JSON 位置/字节偏移”。`document_tab.cpp:3324-3329,3884-3900` 以 PlainText 展示结构化编译或一般错误。前端自身失败应定位原 YAML 行列；下游编译失败应保留原 JSON Pointer 与**生成 JSON** 的 offset，并仅在来源映射有可靠命中时另示 YAML 行列。映射缺失须标注“未映射”，绝不把 JSON 偏移写成 YAML 偏移。 |
| Host 重新准备与配置身份 | `document_tab.cpp:2426-2557` 的 ApplyHostDraft 再次以 `host_config_text_` 提交 worker；`compile_worker.cpp:238` 对该请求文本求哈希，`document_tab.cpp:2533-2536,2559-2580,2786-2815` 使用 `prepared()->config_sha256` 核对候选和当前状态。因此 YAML 首次编译后，Host 重新准备不能复用原 YAML 字节去走 JSON worker；应复用与首次编译一致的有界生成 JSON，或明确保证再转换字节和哈希稳定，并将原 YAML 身份/来源映射另行维护。不要静默把现有 compiled-config hash 改成 YAML 原文 hash。 |
| Flow 生命周期 | `document_session.cpp:712-752,2732-2777` 有 Host Flow 选择/重置与流重置；其 owner 在 `BeginLoad` 的 `prepared_.reset()` 和 `Close` 的清理中失效。新加载确认后，不应让旧 Flow owner、已发布结果、Host pending 身份或旧来源映射与新配置混用。取消重载则应完整保留当前 Session。 |
| 部署配置 | `tools/protocol_lab_ui/CMakeLists.txt:304-340` 显式枚举 JSON 配置；`cmake/DeployProtocolLabUi.cmake:64-70` 只接受白名单 `*.pae.json`；`tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1:79-102` 复制显式 JSON 列表；`tools/protocol_lab_ui/standalone/cmake/DeployPaeLab.cmake:5-34` 又按固定 JSON 名称验证。后续示例/可选打包需单独授权和明确 opt-in，不能仅放宽既有正则或让 JSON-only 包强制带 YAML 依赖。 |

## 后续最小接线建议（不是本轮实现）

1. 在文件加载入口明确识别受支持的 YAML 后缀/格式；路径编辑和重载始终指向原文件。JSON 文档继续走原逻辑。受限前端在非 Qt 层工作，返回有界严格 JSON、前端诊断和可用的来源映射；具体 API 及接入方式待 PAE 探针和总控复核。
2. 在 `ClassifySchemaVersion` 之前获得生成 JSON；沿现有 revision/文档 ID 调度，保留当前单次编译和路由。原 YAML、生成 JSON、来源映射及中间解析状态均需独立资源上限和明确生命周期；避免在 UI 线程无界解析或要求用户维护双份配置文件。
3. 将展示身份（原 YAML 路径）、编译输入（生成 JSON）、现有配置哈希（编译输入字节）分开。尤其要让 Host draft 重准备复用同一编译输入与身份判定，并在重载/关闭时清除相应缓存与映射；不要用磁盘生成 JSON 替换用户源文件。
4. 诊断分两层：YAML 语法/受限 profile/转换错误报告 YAML 原文位置；下游编译错误继续报告原 JSON Pointer、代码和生成 JSON offset，再用来源映射附加 YAML 行列及映射置信范围。若一个生成位置无唯一来源，直接说明不可映射。Lab 展示仍用 PlainText；不修改公共 `CompileDiagnostic` ABI。
5. 保持取消重载、转换失败、编译失败、Host 候选失败和关闭的现有原子性边界。只有经过来源格式、转换、分类和编译均成功的 completion 才发布新 owner；旧 revision 的结果不得发布。既有部署白名单与 JSON-only 构建保持原状，可选 YAML 示例和依赖另成后续实施片。

## 后续专项测试清单（本轮均未执行）

- JSON 回归：既有 JSON 的打开/手输路径、分类、公开及受构建条件控制的旧路由、结构化诊断、Host draft、部署配置在 YAML 能力关闭时不变。
- YAML 正例：Binary、ASCII 各取一份代表配置，比较转换后的编译描述及实际 Decode/Encode 行为；路径标题和重载仍显示原 YAML。只比较文本结构不能冒充编译/执行等价。
- YAML 负例与预算：禁用语法、重复键、数字/字符串边界、无效 UTF-8、超大原文/生成 JSON、超深/超节点/超标量和分配失败；验证解析阶段上限与调度拒绝，不以转换后上限代替。
- 诊断：前端错误给原文行列；下游 JSON Pointer/offset 保真，映射命中与未命中均有明确文案；覆盖中文、转义键、数组、根 pointer 和多对一/无唯一来源。
- 生命周期：重载取消保留旧 Flow/Host；确认后转换或编译失败清旧 owner；多文档排队、同文档连续重载、关闭后晚到 completion 不污染当前文档；Host 重准备使用与初编译一致的 JSON 哈希，失败时当前 Session 不被候选覆盖。
- 构建/交付：未来分别验证可选前端开启及 JSON-only 关闭配置，Debug/Release；核对部署白名单与 standalone/static/shared 包实际文件和依赖来源。此项须另获实施与构建授权，不属于本报告证据。

## 待总控/PAE 后续决定

- 受限前端的正式接口、格式识别规则、来源映射粒度与不可映射语义、资源预算和异常模型尚未冻结；本报告不发明 API 签名，也不选定库。若前端不能可靠提供位置或解析阶段资源控制，不能仅凭 Lab UI 补文案宣称诊断/安全边界闭合。
- Lab 实施涉及 `DocumentTab`、`CompileWorker`、可能的 Session 自有状态、专项测试及后续可选部署接线，需要在探针复核后另派范围；当前不修改公共 API、Schema、PAE 执行语义、UI 成品或 SDK。

状态：**已完成派发范围，待总控复核**。本报告为静态预检，不证明 YAML 产品可用、构建成功、Lab 运行或包外消费。未 Stage、Commit、Push、删除或发布；完成交接后停止写入。
