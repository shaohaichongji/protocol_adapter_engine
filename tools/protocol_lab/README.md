# PAE Protocol Lab

`pae_protocol_lab`是PAE当前内部的离线协议实验与复现CLI（Command-Line Interface，命令行
接口）。它复用同一条`ConfigCompiler -> ProtocolPlan -> CompleteRecordCodec`执行链，不在Core
中引入文件、线程或Transport（传输层）职责。

当前实现范围：

- `inspect`：从Binary或严格Hex文件读取一条完整Frame，跨配置内Pipeline执行唯一Message匹配
  和类型化Decode；
- `encode`：从严格`pae.lab.values/0.1`或`0.2`文件读取类型化业务值；0.2增加原生JSON
  `BOOL`，执行fail-closed（失败关闭）Encode并输出规范Hex；
- `replay`：校验既有Evidence Bundle的`COMPLETE`和`SHA256SUMS`后，使用内嵌配置或显式新
  配置重新执行，并生成一个不可覆盖的新Run；
- `compare`：比较两个Frame或两个Run；Run比较排除时间、绝对路径和Run ID，并报告
  `WIRE_BYTES`、`MESSAGE_MATCH`、`STATUS`、`TYPED_FIELDS`等差异类别；
- `udp-exchange`：Windows工具层使用Winsock执行一次同步、有界UDP请求/响应；没有`--send`时
  只预览并记录TX，不创建Socket；接收后先记录RX原始字节，再检查Peer并Decode；
- 可选Evidence Bundle：记录实际配置、Values、原始Frame、机器结果、单行JSONL事件和完整
  SHA-256清单。

当前实现不包含TCP、串口、CAN、GUI、生产Trace、持续监听、自动重试、安装导出或公共API兼容
承诺。Windows UDP（User Datagram Protocol，用户数据报协议）只属于Protocol Lab内部实验工具，
不改变Core的Transport无关边界；Linux UDP Adapter尚未实现。

## 内部结构

Lab保持单一`pae_protocol_lab`可执行程序，内部实现按职责拆分为CLI解析、协议操作、结果格式、
SHA-256、Evidence Bundle事务和应用编排。非公共静态目标`pae_protocol_lab_internal`只服务于
可执行程序和故障注入测试，不安装、不导出。

Evidence Bundle写入通过内部`RecordFileSystem`接口隔离文件系统操作。生产实现使用C++17
标准库；测试通过显式依赖注入制造目录、写入、关闭、重读、复核、完成标记、Hash清单和最终
重命名失败，不增加隐藏命令行参数或环境变量。

## 构建

Protocol Lab默认关闭，并且不依赖Testing开关：

```powershell
cmake -S . -B out/build/protocol-lab `
  -DPAE_BUILD_PROTOCOL_LAB=ON `
  -DPAE_BUILD_TESTING=OFF `
  -DBUILD_TESTING=OFF
cmake --build out/build/protocol-lab --config Release
```

Windows预设：

```powershell
cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-release
ctest --preset windows-msvc-protocol-lab-release
```

该目标不安装、不导出；`PAE_BUILD_PROTOCOL_LAB=OFF`的Product-only构建不生成它。

## 使用

```text
pae_protocol_lab inspect --config <protocol.pae.json> \
  (--frame-bin <frame.bin> | --frame-hex <frame.hex>)

pae_protocol_lab encode --config <protocol.pae.json> \
  --values <values.pae-lab.json>

pae_protocol_lab replay --bundle <run-directory> \
  [--config <replacement.pae.json>] [--record-root <directory>]

pae_protocol_lab compare \
  (--left-run <run> --right-run <run> | \
   (--left-frame-bin <file> | --left-frame-hex <file>) \
   (--right-frame-bin <file> | --right-frame-hex <file>))

pae_protocol_lab udp-exchange \
  --config <protocol.pae.json> --values <values.pae-lab.json> \
  --remote <ipv4:port> --receive-pipeline <id> \
  --record-root <directory> [--local <ipv4:port>] \
  [--timeout-ms <1..60000>] [--send [--allow-non-loopback]]
```

`inspect`和`encode`只有提供`--record-root`时才写Evidence Bundle；`replay`总会创建新Run，
未指定`--record-root`时写到源Run的父目录；`udp-exchange`强制要求`--record-root`，没有
非证据型发送旁路。Schema 0.1执行保持原离线Result/Record 0.1和UDP Result/Record/Event 0.2；
Schema 0.2执行统一使用Result/Record/Event 0.3，RX Metadata仍为0.2。Replay显式标记
`ENCODE_TX`、`DECODE_RX`或`NO_CODEC_REEXECUTION`。旧离线V0.1继续兼容，旧UDP草案、未知版本、
跨Schema替换配置Replay和旧格式Run与0.3 Run直接Compare均失败关闭；原始Frame比较不受格式代际限制。
默认文本输出便于人工查看。`--expect-status <status>`可把预期的Codec失败
作为成功用例返回`0`，但Compare发现差异始终返回`6`。

UDP V0.2事件在真实TX持久化、发送和RX持久化阶段采集，使用同一Run单调时钟原点。RX在Peer
检查和Decode前写入并复核原始字节及来源Metadata。Replay保留历史Transport事实，但当前Codec
执行与比较结论独立输出；`NO_CODEC_REEXECUTION`不会调用Codec，比较状态为`NOT_EVALUATED`。
Replay产物可继续作为输入链式Replay；读取时还会解析并对照Run Record、Result、Event、
Metadata和实际Frame，不能通过只重算文件清单绕过内部一致性校验。
旧离线V0.1按其发布时的真实字段集合读取，后加网络字段允许缺失但存在时严格校验；仓库测试夹具
来自`35ecbccc`历史Synthetic Bundle并移除了机器绝对路径。Cross-config `ENCODE_TX`中，
`frame_*`/Event记录本次实际Encode输出，`tx_frame_*`保留历史TX；合法字节变化输出
`DIFFERENT`，仍可完整读取和再次Replay。

`udp-exchange`默认本地Endpoint（端点）为`127.0.0.1:0`，只接受数字IPv4且拒绝组播、广播和
未指定远端。没有`--send`不会触发网络操作；任一本地或远端地址不是Loopback时，必须同时提供
`--send --allow-non-loopback`。当前自动化执行证据仅覆盖Loopback。2026-09-06已完成
NetAssist人工Loopback门禁，报告层记录本次场景`LAB_EXCHANGE_PASS=PASS`；原始机器证据中的
`NOT_EVALUATED（未评估）`保持不变，协议Golden结论不升级。范围和待诊断项见
[人工验收报告](../../docs/windows-protocol-lab-manual-loopback-acceptance-20260906.md)。

完整契约、Values示例、退出码和证据边界见
[Protocol Lab Contract V0.1](../../docs/protocol_lab_contract_v0.1.md)和
[PAE-DEC-039 Windows UDP Exchange](../../docs/pae-dec-039-protocol-lab-windows-udp-exchange.md)。
