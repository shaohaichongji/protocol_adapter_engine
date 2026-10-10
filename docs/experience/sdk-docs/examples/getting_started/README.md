# 最小公开 API 程序（Source 包）

本文件是以固定产品 `9680cf90512f053962949cb55467d6af35959088` 为事实的文档投影，
不是该提交原件。程序和 CMake 保持固定来源字节，只有说明路径经整理。

本例读取[完整合成配置](../config/synthetic_stream_framing_slice.pae.json)，对 `fixed_rx`
的 `fixed_message` Decode 手写 `AA 00 07`，检查 `fixed_payload=7`，再 Encode 类型化 7，
与独立写定的同一预期字节比较。它只使用公开 Compiler/Codec，没有通信或设备线程。

进入 Source SDK 根，按[SDK 中文指南](../../docs/sdk/README.md)选择 Source 命令、包外新
BuildRoot 与匹配工具链，构建 target `pae_getting_started`。成功输出为
`GETTING_STARTED_BINARY_PASS`；文档准备未执行本批构建或运行。

CompiledProtocol metadata 字符串借用 owner，移动/销毁后重新查询；Codec 字段视图在下一次
受保护调用或 owner 变化后失效，BYTES 还借用输入缓冲。Encode 只有成功状态下的
`bytes_written` 可交付。Pipeline 名称不是可用动作证明，须查询公开 execution metadata。
