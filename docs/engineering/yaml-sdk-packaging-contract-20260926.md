# YAML SDK 打包闭包实施片

## 当前依据

用户授权推进下一步。可选静态组件首片已完成总控限定源码与既有日志复核：
公开 owner、条件安装/发现、专项 D/R 2/2、四组安装树消费及两组 Release 搬迁日志已读，
未独立重跑测试。此结论不等于正式包、Lab 迁移或人工验收。当前 main@4da54b9，
共享工作树有 Lab、组件及总控文档未提交变更，需保留并记录 dirty 身份。

## 实施范围

《子任务推进》独占 `scripts/package_sdk_stage3.ps1` 的 YAML 可选打包适配、
必要的新增专项脚本测试、`docs/sdk/README.md` 打包说明，以及新报告
`docs/engineering/yaml-sdk-packaging-validation-20260926.md`。
不得修改 Core、公开组件接口/算法、Lab、standalone、第三方原件或现用交付目录。
根 CMake/安装模板原则上不动；若包闭包暴露遗漏，先报告所需最小改动。

1. source 包须包含可选 YAML 实现/公开头/CMake、固定 rapidyaml 原件与通知、Profile
   和自包含示例；默认构建 OFF。此处“含可选源码”不等于默认编译/链接依赖。
   包内不能引用 spikes、原仓库 src 路径、旧 out 或下载回退。
2. static/shared 包仍消费相应安装树；准确识别并校验 YAML 组件完整性，不仅凭
   一个头文件判定。组件缺文件或混合状态拒绝，JSON-only 包保持可用。
   元数据明确 shared PAE + 静态 YAML，无 YAML DLL；保留配置/工具链/来源约束。
3. 对实际打包字节生成/复核 MANIFEST、SHA256SUMS、PROVENANCE；新增可选组件字段
   保持旧使用方式兼容。记录 HEAD、dirty 与可复核文件身份，不把未提交源码称为 clean。
   公共源码和文档不新增机器绝对路径；原始机器日志留忽略的证据根。
4. 仅使用新隔离 `out/build/yaml-sdk-packaging-20260926`、
   `out/evidence/yaml-sdk-packaging-20260926`。不得对旧安装树补写元数据，需复制到
   新候选根并核对来源。打包目标已存在时失败，不自动覆盖/清理。

## 最小验证

- 从新 source 包（路径含空格）构建并运行 YAML consumer Debug/Release，真实
  YAML→JSON→公开 Compiler→Codec；另做 source 默认 OFF 的 JSON-only 消费。
- 对启用组件的 static/shared D/R 新候选包验证内容、Hash、元数据及真实 consumer，
  可复用已核对安装树，不能复用旧运行日志冒充新包验证；shared 核对同包 DLL。
- JSON-only 二进制包至少 static/shared Release 检查原 consumer、无 YAML 声明或
  缺组件请求按预期失败；缺必要组件文件的负例只在隔离副本中构造，不改原包。
- 验证 manifest/Hash 覆盖、许可与样例闭包、成品无 Qt/Lab 内容，输出不是正式发布。
  不重跑无变化全仓/完整 UI 矩阵，不启动可见窗口、不做人工 YAML 体验。

## 停点

Lab 及工程整理停止写入；总控独占契约、计划和索引。此片完成后才安排 Lab 消费迁移。
无 Stage/Commit/Push、删除旧产物、发布、项目许可证决定或替换部署授权。
超范围或无法证明来源一致时停报，不假造验证。完成后向总控
`01a04601-757d-7bb1-8254-61dde4954d74` 主动反馈一次文件、命令/结果/证据、
未验证与风险、Git 状态，停止写入，等待总控复核。
