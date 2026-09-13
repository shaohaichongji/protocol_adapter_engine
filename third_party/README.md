# Third-party Source Policy

Qt 5.13.0 按用户决定完整复制现有 DEI 包，作为可选 Lab UI 的长期本地依赖。
范围、来源和验证边界见 [Qt 包说明](qt-package.md)，逐文件快照见 `qt-files.json`。
这次明确采用全包保留策略，不按当前 Lab 模块裁剪；不代表每个模块均已通过 Lab 验证。

最终依赖必须固定版本或 Commit、上游地址、License、源文件范围、SHA-256、本地修改和更新方式。

yyjson 0.12.0已经确认为Compiler和Protocol Lab的内部JSON Parser（解析器）依赖。正式随仓范围和
唯一当前锁见[yyjson说明](yyjson/README.md)；Configure（配置生成）阶段只读取本地源码并校验
长度和SHA-256，不提供网络下载回退。

JSON Parser Spike中的nlohmann/json和RapidJSON继续是实验候选，可以保留构建目录临时下载方式；
不得因此把它们加入正式产品依赖。`spikes/json_parser/dependencies.lock.json`是选型阶段历史快照，
不再是yyjson当前消费入口。

第三方文件不得自动格式化。更新依赖必须单独审查上游版本、Commit、License、最小源码范围和
所有Hash，并重新执行依赖合同、Compiler、离线Lab及隔离构建门禁。
