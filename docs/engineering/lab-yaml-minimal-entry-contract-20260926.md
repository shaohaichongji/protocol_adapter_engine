# Lab YAML 最小接入

## 授权与基线

用户授权提交并推进下一步。YAML 内部前端与可选依赖检查点为 `4da54b9`，已本地提交，
未推送；提交后工作树干净。本片基于仓库内 Lab 构建，不将尚未导出的内部前端伪装成
installed SDK 能力，不重打或替换现用 Lab/SDK。任务接管时须重新核对现场。

## 最小行为

1. 保持中文左右工作台及原 JSON 路径不变。文件选择/手输路径明确识别大小写无关的
   `.yaml`、`.yml`（含 `.pae.yaml`）；其他后缀保持既有 JSON 行为，不按内容失败回退猜格式。
2. 可选 YAML 接入默认关闭；显式启用时要求可用 `pae_yaml_frontend` 与公开 PAE 编译目标，
   缺少前置条件给出配置错误，不静默强开原开关。JSON-only 不引入 ryml。
   本片不修改 standalone 构建和 SDK 安装规则。
3. 转换在后台编译 worker 内进行，沿用文档 ID/revision/关闭/有界排队机制。
   使用现有前端实验 Limits，不擅自提高容量；原 YAML 读取尽早按前端输入上限拒绝，
   不因 JSON 原有 4 MiB 读取上限而误称 YAML 同容量。
4. 原文件路径/格式为作者源身份，生成 JSON 仅存内存。现有 config_sha256 仍针对实际
   编译 JSON 字节。Host 重准备复用同一生成 JSON，不把 YAML 原文直接交给 JSON 编译链。
   YAML 只走现有公开编译路由；不能支持的构建/Schema 明确拒绝，不扩大旧私有路由。
5. 转换成功后复用既有分流/编译，不在 Lab 解释协议规则。映射和生成 JSON 随 completion
   自持，过期结果、取消重载、确认后失败、关闭均遵循现有 Session/Host 原子性规则。
6. 下游错误保留 JSON Pointer 与生成 JSON offset，另示 YAML 行列；近似/容器回退必须
   明示，未映射则明确无位置。前端当前只提供状态/reason 时显示转换阶段错误和
   “未提供源码位置”，不伪造行列，也不在本片扩展前端接口。诊断使用 PlainText。

## 写入范围与分工

《Lab应用推进》独占 `tools/protocol_lab_ui/` 中必要的 DocumentTab、CompileWorker、
DocumentSession、相关 CMake/测试，`tests/protocol_lab_ui/` 必要专项夹具/测试，
根 `CMakeLists.txt` 最小可选门禁，以及本目录
`lab-yaml-minimal-entry-validation-20260926.md`。
不改 standalone 子目录、PAE src/include、Schema、第三方、SDK/部署脚本或公开 API。
总控维护契约/计划/索引；PAE 和工程整理本片不并发实施。
如现有前端或安装形态阻塞，需要超范围变更，先停报具体证据。

## 验证与交付

- 独占 `out/build/lab-yaml-minimal-entry-20260926` 和
  `out/evidence/lab-yaml-minimal-entry-20260926`，按仓库有效工具链/Qt 设置，
  不修改本机 Qt 或系统 PATH。Debug/Release 串行，默认不开可见窗口。
- 定向覆盖 Binary/ASCII YAML 编译及至少已知 Decode/Encode；JSON 原路径回归；
  格式与上限拒绝、转换失败、下游精确/近似诊断、Host 重准备 JSON 哈希一致、
  同文档重载和关闭后旧 completion 隔离。保留原断言，不能以展示成功替代执行测试。
- 验证选项 OFF 的 JSON-only 路径及 ON 缺前置依赖明确拒绝；不跑无关全量矩阵。
- 先交付自动证据，复核后至多一次简短人工体验，不要求用户重做全套历史验收。
  YAML 示例先用仓库现有合成文件/测试夹具，不改现用 configs 白名单或部署目录。
- 完成向总控主动反馈一次交接摘要，实际文件/命令/结果/证据/未验证/Git 状态齐全，
  停止写入等待复核。不 Stage/Commit/Push、删除、发布或覆盖现用产物。
