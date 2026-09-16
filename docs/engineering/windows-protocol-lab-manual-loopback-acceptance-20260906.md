# Protocol Lab 人工 Loopback 验收报告

## 1. 结论与范围

2026-09-06，用户使用外部网络调试工具完成本机UDP（用户数据报协议）人工验收。
结合既有Windows自动化证据、本次人工收发记录及5份Evidence Bundle（证据包）只读复核，
按[DEC-039第7节](pae-dec-039-protocol-lab-windows-udp-exchange.md#7-自动化和人工门禁)
记录本次场景的验收结论：

```text
LAB_EXCHANGE_PASS=PASS
peer_kind=LAB_SIMULATED_PEER
PROTOCOL_GOLDEN_PASS=NOT_EVALUATED
```

这是报告层的人工验收结论，不是程序自动推断的门禁。原始Bundle中的
`LAB_EXCHANGE_PASS=NOT_EVALUATED`保持不变，不能为追求字段一致而修改原始证据或重算哈希。
本结论不代表真实设备互通、真实协议Golden（权威基准向量）、Linux或生产现场验收。

## 2. 环境与证据来源

- 用户报告的网络工具文件名：`netassist5.15.exe`；版本标识取自文件名，未独立检查文件版本资源。
- 用户终端：PowerShell 7.6.5；Windows本机运行Release Protocol Lab。
- 当前源码基线：`a418ac9`；执行文件为
  `out/build/windows-msvc-protocol-lab/tools/protocol_lab/Release/pae_protocol_lab.exe`。
  本轮未重建或进行独立二进制溯源，不把当前HEAD直接视为可执行文件的嵌入版本证明。
- PAE端点：`127.0.0.1:23457`；模拟端：`127.0.0.1:23456`。
- 配置：`examples/config/synthetic_lab_exchange_slice.pae.json`；输入Values：
  `tests/protocol_lab/fixtures/valid_command.values.pae-lab.json`。
- 配置SHA-256：`cd209d543a29ab71dcda286f2bf4adf14b1b5603f3c11961239d7200d0e85566`。
- 人工证据：用户提供的PowerShell完整输出与NetAssist HEX收发文本；未收到截图。
- 用户明确确认：成功记录Replay（回放）前已关闭调试工具或其UDP端口。
- 原始证据保留于本地`out/manual-lab/20260906-130444/`，不纳入Git。
  本报告只引用公开Synthetic（人工构造）样例，不复制真实客户协议。

## 3. 操作与结果

| 用例 | 操作及关键核对 | 实际结果 |
|---|---|---|
| 预览 | 无`--send`；核对待发送13字节及发送标志 | 退出0，`send_attempted=false`、`send_succeeded=false` |
| 正常收发 | `--send --timeout-ms 60000 --receive-pipeline fixture_to_host`；人工核对请求并发回响应 | 退出0，收发及Decode标志为true，来源端口23456，匹配`lab_report` |
| 离线回放 | 用户确认关闭工具或端口后，以成功Bundle执行`replay` | 退出0，`DECODE_RX`，`transport=OFFLINE`，`comparison_status=EQUAL` |
| 显式比较 | 对成功Run及其回放Run执行`compare` | 退出0，`EQUAL` |
| 超时 | 保持模拟端接收，不回复，`--timeout-ms 5000` | 退出9，`TIMEOUT / UDP_RESPONSE_TIMEOUT`，未收到响应 |
| 未知响应 | 将响应标识`5C`改为`5D`后发送 | 退出5，`UNKNOWN_MESSAGE / PAE_LAB_CODEC_UNKNOWN_MESSAGE`，已收RX但未Decode成功 |

正常请求13字节：`0D 00 A7 31 56 34 12 DE AD BE EF 02 00`。
正常响应11字节：`0B 00 5C E2 C3 B2 A1 68 24 EF BE`。
未知响应11字节：`0B 00 5D E2 C3 B2 A1 68 24 EF BE`。

正常响应字段核对：

| 字段 | 实际值及期望值 |
|---|---:|
| record_length | 11 |
| operation_code | 57948 |
| transaction_id | 10597059 |
| status_code | 9320 |
| sample_counter | 48879 |

NetAssist本地时间记录：13:06:25.217收到正常请求，13:07:18.577发出正常响应；
13:08:43.002收到超时用例请求；13:09:19.435收到未知响应用例请求，
13:09:20.490发出未知响应。正常交互的人工等待约53.36秒，小于60秒超时；
不是PAE处理耗时或性能证据。Bundle事件时间使用UTC，与本地时间相差8小时。

本样例没有CRC（循环冗余校验）字段；未知响应验证的是消息匹配拒绝，不是CRC校验。
请求与响应的事务标识不同是样例设计，不证明已实现事务关联；记录长度字段也不证明自动回填。
负向用例的`ENGINE_POC_PASS=FAIL`描述单次操作失败，不代表正确拒绝该输入的测试失败。

## 4. 证据定位与复核

以下目录均相对于`out/manual-lab/20260906-130444/`：

| 用例 | 原始Run路径 |
|---|---|
| 预览 | `preview/run_20260906T050459.258Z_ec5564d69beb5da3` |
| 正常收发 | `success/run_20260906T050625.208Z_f7d42282734ab72a` |
| 回放 | `replay-success/run_20260906T050820.325Z_22e14dc570837451` |
| 超时 | `timeout/run_20260906T050842.994Z_a38ac18212b3bbad` |
| 未知响应 | `unknown-response/run_20260906T050919.427Z_dd60f6778468cc8c` |

对5份Bundle分别执行只读`compare --left-run <run> --right-run <run> --output json`，
均退出0、返回`EQUAL`；读取链通过完成标志、文件清单及Record内部一致性校验。
自比较仅证明证据可读且自洽，不是独立的协议正确性证明；协议字节核对来自上述人工预期。

成功Bundle包含TX/RX原始二进制、Hex、RX Metadata（接收来源元数据）、阶段Event、
Result、Run Record、`SHA256SUMS`及`COMPLETE`。TX/RX分别为13/11字节；
RX Metadata绑定Event 4、11字节响应及来源`127.0.0.1:23456`。
正常收发与Replay的确定性指纹均为
`ad6b44c12deb4c5c656be65e621ce3faf4306fdb76902f508639024094db9d00`。

原始证据不修改；本轮报告编写不产生新网络交互，也不重新生成历史Bundle。
此前自动化结果见[Windows验证报告](windows-msvc-2026-protocol-lab-udp-exchange-slice.md)，
本轮未重跑构建、自动化矩阵或硬件测试。

## 5. 待诊断项与剩余边界

### 中文终端路径显示

用户粘贴的PowerShell JSON输出中，`evidence_bundle`中文路径出现乱码。
按UTF-8读取磁盘上的5份Result，中文路径正确且实际存在，证据读取也通过。
当前证据指向终端输出或文本解码链路，尚未定位具体环节；不据此声称根因已修复。
后续应单独诊断控制台编码、原生程序输出及重定向解码，本轮不修改源码或终端配置。
此项不阻断已确认的本机收发与离线回放场景，但不代表中文控制台体验已通过验收。

### 不升级的结论

- 未执行非Loopback发送、真实设备、现场或目标板验收。
- 人工模拟字节仍属Synthetic证据，不因经过Socket升级为真实设备抓包。
- 未验证Linux、持续监听、重试、多客户端、压力、正式性能及断电持久化。
- 本报告不改变Core的Transport无关边界，不授予后续实现或Git操作权限。
