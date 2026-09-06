# PAE-DEC-039 Protocol Lab Windows UDP Exchange

## 1. 决策状态

状态：`IMPLEMENTED / WINDOWS_AUTOMATED_VERIFIED（已实现、Windows自动化已验证）`。

本Decision（决策）冻结Protocol Lab（协议实验室）Windows UDP（User Datagram Protocol，
用户数据报协议）单次请求/响应切片的实现边界。对应源码、测试和Windows执行证据已经形成；
它没有把UDP、Winsock、Endpoint（网络端点）或线程职责引入PAE Core、ProtocolPlan或Codec。

## 2. 目标与永久边界

本切片用于发送PAE Encode（组包）产生的一条UDP请求，接收至多一条响应，在Decode（解析）前
持久化原始响应，并生成可由现有Replay（重放）和Compare（比较）命令消费的Evidence Bundle
（证据包）。它是有界实验工具，不是生产通信组件、后台服务、持续监听器或压力发生器。

Windows实现位于`tools/protocol_lab`内部，通过不可安装、不可导出的`IUdpExchangeAdapter`边界
隔离系统调用。生产实现使用Winsock和RAII（资源获取即初始化）管理启动、Socket与清理；测试
可以注入受控Adapter，但真实Loopback（本机回环）自动化必须实际经过Winsock。未来Linux可在
同一内部边界提供POSIX Socket实现，本轮不实现，也不形成Linux兼容证据。

## 3. 执行、线程与Endpoint

`udp-exchange`采用一次同步、有界调用：

- 使用阻塞等待配合超时，或`select`等事件等待，不使用忙轮询；
- 产品路径不创建常驻线程；测试专用对端线程必须具有明确的就绪、停止、超时和回收边界；
- 首版只接受数字IPv4地址，不执行DNS；
- 本地Endpoint默认`127.0.0.1:0`，远端Endpoint必须显式提供且端口不能为`0`；
- 拒绝广播、组播、未指定远端和远端`0.0.0.0`；
- 没有`--send`时只执行配置、Encode、预览和记录，不得初始化Winsock或创建Socket；
- 任一本地或远端地址不是Loopback时，必须同时提供`--send --allow-non-loopback`；
- 首版固定发送一次、接收至多一个Datagram（数据报），不重试、不扫描、不后台接收。

## 4. CLI与资源上限

继续使用`PAE-DEC-037`已经冻结的命令，不增加批量、重试或持续接收参数：

```text
pae_protocol_lab udp-exchange \
  --config <file> \
  --values <file> \
  [--local <ipv4:port>] \
  --remote <ipv4:port> \
  --receive-pipeline <id> \
  [--timeout-ms <value>] \
  --record-root <directory> \
  [--send [--allow-non-loopback]]
```

超时默认`2000 ms`，合法范围为`1～60000 ms`。首个Windows UDP切片固定至多接收一条响应；
既有`16`条接收Hard Limit（硬上限）只作为未来经单独拍板的多接收模式上界，当前CLI不可达。
单个IPv4 UDP Payload（负载）不得超过`65507 bytes`，并同时受ProtocolPlan Profile（资源档位）
限制。Event数量、单Run原始字节总量和其他Evidence Bundle资源继续受既有Hard Limit约束，CLI
不能关闭或扩大这些限制。

## 5. Evidence Bundle事务

在线事务顺序冻结为：

```text
配置加载与Encode
→ 创建.inprogress目录
→ 写入并复核TX原始字节
→ 创建和绑定Socket
→ 发送
→ 有界等待响应
→ 写入并复核RX原始字节及来源Endpoint
→ 对端一致性检查
→ Decode
→ 写入最终结果和Hash
→ 写入COMPLETE
→ 原子发布正式Run目录
```

TX（发送）记录失败时不得发送；RX（接收）记录失败时不得Decode。`COMPLETE`只表示证据事务
完整，不代表网络交换或协议操作成功。超时、Socket错误、未知Message或Decode失败只要被完整
记录，仍可生成带失败终态的正式Run；Evidence Bundle记录失败或进程中断才保留`.inprogress`。

## 6. 接收、对端与失败关闭

接收结果必须记录实际来源IPv4和端口。第一个Datagram来源与配置远端不一致时，先保存完整原始
字节和来源，再以`UDP_PEER_MISMATCH`失败，不静默丢弃并继续等待。接收缓冲区应覆盖合法IPv4
UDP Payload，完整接收后再执行ProtocolPlan长度限制。超长、截断或无法证明完整的数据不得交给
Decode。

Transport错误使用既有退出码`8`，响应超时使用`9`，记录失败使用`7`，网络交换完成后的协议
操作失败使用`5`，未分类异常使用`10`。本切片冻结以下稳定诊断标识：

- `UDP_STARTUP_FAILED`；
- `UDP_SOCKET_CREATE_FAILED`；
- `UDP_BIND_FAILED`；
- `UDP_SEND_FAILED`；
- `UDP_RECEIVE_FAILED`；
- `UDP_RESPONSE_TIMEOUT`；
- `UDP_PEER_MISMATCH`；
- `UDP_DATAGRAM_TOO_LARGE`；
- `UDP_DATAGRAM_TRUNCATED`；
- `UDP_PLATFORM_UNSUPPORTED`。

## 7. 自动化和人工门禁

Windows Debug/Release自动化至少覆盖：无`--send`零Socket操作、真实Winsock Loopback单次收发、
TX/RX逐字节结果、TX发送前持久化、RX解析前持久化、超时、Transport各阶段错误、非Loopback
双确认、非法Endpoint、Peer不匹配、超长/截断、未知或错误响应的完整证据、Evidence Bundle
失败关闭、Product-only边界和既有CTest不回退。测试专用Peer或注入Adapter不是协议权威。

人工门禁使用外部网络调试工具在Loopback上接收请求、逐字节核对、发送响应，再关闭在线工具
执行离线Replay。自动化与人工步骤都通过后，才允许声明：

```text
LAB_EXCHANGE_PASS=PASS
peer_kind=LAB_SIMULATED_PEER
```

该结论不等于真实设备互通、现场验证或`PROTOCOL_GOLDEN_PASS`。网络调试工具产生的人工报文仍为
`SYNTHETIC_REVIEWED`或`DOCUMENT_DERIVED_REVIEWED`，不会仅因经过真实Socket而升级为
`OBSERVED_CAPTURE`。

## 8. 实现与验证证据

Windows实现位于`tools/protocol_lab/udp_exchange.*`，由内部`IUdpExchangeAdapter`隔离Winsock。
自动化使用真实Winsock测试Peer完成Loopback请求/响应和超时，并使用注入Adapter覆盖各阶段稳定
诊断、Peer不匹配、超长、错误响应、非Loopback双确认及TX/RX记录失败关闭。实际动态本地端口、
响应来源、TX/RX字节和事件顺序均进入Evidence Bundle。

2026-09-06在补充六项拍板实现后实际复核结果：

- Protocol Lab专用Windows Debug/Release均`4/4 PASS`；
- Parser、Loader、Plan、Codec、Conformance Runner和Lab全切片Debug/Release均`30/30 PASS`；
- Lab-on/Testing-off Release构建成功且CTest为`0`项；
- Product-only Release只生成`pae_protocol_plan`和`pae_protocol_core_slice`两个PAE产品目标，
  未生成Protocol Lab、UDP或Winsock相关目标；
- 自动化实际网络收发仅发生在`127.0.0.0/8` Loopback范围；测试文档地址只进入注入Adapter，
  未发生非Loopback主动发送。

详细命令、环境和证据边界见
[Protocol Lab Windows UDP Exchange验证报告](windows-msvc-2026-protocol-lab-udp-exchange-slice.md)。

### 8.1 六项补充拍板

1. Event在真实阶段即时采集，统一使用单次Run的单调时钟原点；`Complete()`只负责最终落盘，
   不反向伪造阶段时间。`SEND_RESULT`只描述本次发送调用，不承担最终Transport诊断。
2. Socket启动、创建和绑定成功后，紧邻实际`sendto`调用前记录`SEND_INTENT`；这些前置阶段失败
   时不得产生发送事件。
3. 收到响应后必须先写入并复核`frames/000002_rx.bin`、Hex和`000002_rx.meta.json`。Metadata
   绑定来源Endpoint、长度和SHA-256；任一写入、关闭、重读或绑定复核失败均返回退出码`7`，且
   不进入Peer检查或Decode。
4. UDP Replay必须显式记录`ENCODE_TX`、`DECODE_RX`或`NO_CODEC_REEXECUTION`；前两者只执行对应
   Codec路径，后者不调用Codec。
5. UDP相关Run、Event和Result升级为`pae.lab.*/0.2`；旧离线V0.1继续可读，旧UDP草案因缺少
   可证明的重放语义而以`PAE_LAB_REPLAY_EVIDENCE_INSUFFICIENT`失败关闭，未知版本一律拒绝。
6. Replay结果将历史Transport事实放入`historical_transport`，当前执行与比较结果使用独立字段；
   `NO_CODEC_REEXECUTION`的`comparison_equal`为`null`、状态为`NOT_EVALUATED`。

### 8.2 V0.2证据一致性纠错

2026-09-06复核确认并关闭三项实现缺口，不改变上述V0.2契约：

1. 离线Replay顶层`response_received`只表示本次是否发生网络接收，固定为`false`；历史是否收到
   响应由`historical_transport.response_received`表达。是否保留RX及应否Decode由显式
   `replay_mode/replay_subject`决定，因此原始Run、Replay A和Replay B可以连续读取，零长度RX仍
   与无响应区分。
2. V0.1/V0.2 Bundle均明确要求同版本Result、Event、Run Record、配置、Frame、清单和
   `COMPLETE`。读取器实际解析Run Record，严格校验结构、字段类型、版本、Payload清单，以及
   Command、状态、模式、主体、Receive Pipeline和Transport状态等与Result共有字段。
3. V0.2 Event按`FRAME/SEND_INTENT/SEND_RESULT`严格校验字段集合、类型和状态不变量；TX/RX
   Event分别与Result、Metadata及实际字节文件绑定。原始UDP Run校验真实收发序列，离线Replay
   只要求一个与当前重放主体绑定的`REPLAY FRAME`，不得伪造网络事件。

### 8.3 V0.1兼容与Cross-config Replay纠错

2026-09-06后续复核关闭两项兼容性回归，不增加格式字段或版本：

1. 真正的旧离线V0.1 Run Record不含后续加入的Endpoint、`send_succeeded`、
   `response_received/response_decoded`。读取器按旧发布形状确定V0.1必需字段；后加字段允许缺失，
   存在时仍严格校验。旧UDP V0.1草案仍因缺少显式Replay语义而拒绝。
2. `ENCODE_TX` Replay的`frame_*`和`REPLAY FRAME`绑定本次实际Encode输出；`tx_frame_*`独立保留
   源UDP历史TX。Cross-config字节变化形成可读`DIFFERENT` Bundle；再次Replay以该Bundle的当前
   执行结果为上一个确定性检查点，历史TX不随之覆盖。Encode失败保留真实状态和空输出，不伪造
   Codec成功。

## 9. 排除项与授权边界

本切片不包含TCP、串口、CAN、持续监听、自动重试、多客户端管理、并发压力、GUI、生产Trace、
Linux实现、Runtime、Session、Route、公共C ABI（C应用二进制接口）、真实设备、硬件、现场验证
或Golden Candidate自动升级。

文档、源码、Stage、Commit和Push继续分别授权。当前源码、测试和Windows自动化已完成，但尚未
Stage、Commit或Push；外部网络调试工具人工Loopback门禁尚未执行，因而
`LAB_EXCHANGE_PASS=NOT_EVALUATED`。
