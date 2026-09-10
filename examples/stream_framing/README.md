# 有界流式宿主接入示例

本目录演示宿主如何主动把字节片段提交给内部 `protocol_framing`，并在完整帧回调中同步调用既有
`DecodeCompleteRecord`。它不是 Runtime、Session 或 Transport 实现，不创建线程、轮询器或网络
连接，也不冻结稳定公共 API。

示例使用公开合成 Schema 0.9 配置，验证以下宿主责任：

- 两条逻辑流共享同一个不可变 Plan，但分别拥有 `StreamFramingWorkspace` 和
  `ExecutionWorkspace`；暂停或 Reset 一条流不会改变另一条流。
- 宿主只按 `bytes_consumed` 前移输入游标。Sink 返回 `STOP` 后，未消费后缀仍归宿主所有，后续从
  原缓冲区的准确偏移重新提交。
- `WORK_BUDGET_REACHED` 表示可恢复的工作或帧数预算残留。示例将每次调用限制为至多交付一帧，
  以显式最大调用次数继续提交同一后缀或 empty input，并要求每次预算停止至少消费输入、交付帧
  或扣减工作单位，避免零进展忙循环。
- 帧回调中的 `ByteView` 只在当前同步回调期间有效；示例若需在回调后核对帧，会复制到宿主自有的
  有界数组，绝不保存借用指针。
- Framer 已交付的完整候选即使 Core Decode 返回 `UNKNOWN_MESSAGE`，也不会被回扫或重新切分；
  下一候选从既定边界继续。
- 宿主 Reset 用于模拟断链或显式放弃当前半帧，只清理目标逻辑流。

构建时启用既有 `PAE_BUILD_STREAM_FRAMING_EXAMPLE` 和测试选项后，CTest 用例
`pae.examples.stream_framing.host_push` 会执行上述合成场景。测试只证明 Windows 离线宿主编排和
既有资源边界内的行为，不代表网络、Linux、真实协议、设备、现场或性能验收。

## 2026-09-10 Windows 离线验证

- 第一批宿主编排复核（15:16）：Debug、Release 的宿主示例、Framer contract 和 12 个
  CompleteRecord Core 用例各 `14/14` 通过。日志为
  `out/post-stream-host-debug-affected-final.log`、
  `out/post-stream-host-release-affected-final.log`。
- 第二批业务字段快照增量复核（15:22）：增加成功字段身份、类型和值，以及
  `UNKNOWN_MESSAGE` 字段数为 0 的精确断言后，只重跑受影响宿主目标；Debug、Release各
  `1/1`通过。构建日志为`out/post-stream-host-fields-debug-build.log`、
  `out/post-stream-host-fields-release-build.log`，测试日志为
  `out/post-stream-host-fields-debug-test.log`、
  `out/post-stream-host-fields-release-test.log`。

上述两批均未执行网络。第二批只改变示例内的业务字段快照和断言，没有修改 Core、Framer、Plan、
Compiler、CMake或公共接口，因此未重复第一批已通过且实现未变的13个Framer/Core用例。本记录不
代表多线程、Runtime/Session、Transport、Linux、真实协议Golden、设备、现场或性能验证。
