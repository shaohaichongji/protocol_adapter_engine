# Third-party Source Policy

> 交付版投影：据固定 `513f6cc9bd46b64b3b6d1d4283f13c806039eebf` 的
> `third_party/README.md` 制作；仅将未随 PAE SDK 附带的 Qt 说明链接改为
> 固定源码仓库历史参考，本文件不是该提交的原始字节。

本 SDK 的非 Qt 依赖是 yyjson 和可选 rapidyaml；Qt 仅用于独立 Lab，SDK 不携带 Qt。
若需要 Lab 的版本、来源和许可状态，请从整包入口读取随包 notice；仅取得 SDK 时不依赖该材料。

最终依赖必须固定版本或 Commit、上游地址、License、源文件范围、SHA-256、本地修改和更新方式。

yyjson 0.12.0已经确认为Compiler和Protocol Lab的内部JSON Parser（解析器）依赖。正式随仓范围和
唯一当前锁见[yyjson说明](yyjson/README.md)；Configure（配置生成）阶段只读取本地源码并校验
长度和SHA-256，不提供网络下载回退。

rapidyaml 0.16.0 单头仅供默认关闭的内部 YAML 前端使用。上游原件、嵌入许可通知、固定
SHA-256 和更新边界见[rapidyaml说明](rapidyaml/README.md)；只有显式开启
`PAE_BUILD_YAML_FRONTEND` 才检查并编译该依赖。它不进入 Core、Lab 或公开 API 默认依赖链。

JSON Parser Spike中的nlohmann/json和RapidJSON继续是实验候选，可以保留构建目录临时下载方式；
不得因此把它们加入正式产品依赖。`spikes/json_parser/dependencies.lock.json`是选型阶段历史快照，
不再是yyjson当前消费入口。

第三方文件不得自动格式化。更新依赖必须单独审查上游版本、Commit、License、最小源码范围和
所有Hash，并重新执行依赖合同、Compiler、离线Lab及隔离构建门禁。
