# Third-party Source Policy

最终依赖必须固定版本或 Commit、上游地址、License、源文件范围、SHA-256、本地修改和更新方式。

JSON Parser Spike 使用构建目录中的临时下载源进行对比；在最终选型完成前，不把三个候选全部 Vendor（随仓库管理）到这里。

当前首选候选 yyjson 仍保持 `candidate（候选）`状态。Spike 的 CMake 从
`spikes/json_parser/dependencies.lock.json`读取其 Archive（归档包）URL 和 SHA-256，下载后还会复核
`LICENSE`、`src/yyjson.h`和`src/yyjson.c`的字节数与 SHA-256。任一来源、许可证或源码范围发生漂移，Configure（配置生成）阶段即失败。
锁文件中的文件路径只能使用非空、正斜杠相对路径；绝对路径、反斜杠和父目录穿越会被拒绝。

这项检查只固定候选实验的可重复性，不代表生产 Parser（解析器）已选定，也不代表源码已经正式 Vendor。正式选型后，才把唯一选中依赖的锁定源码范围和许可证副本纳入本目录；其他对比候选不随生产交付。
