# PAE Protocol Lab

`pae_protocol_lab`是PAE当前内部的离线协议实验与复现CLI（Command-Line Interface，命令行
接口）。它复用同一条`ConfigCompiler -> ProtocolPlan -> CompleteRecordCodec`执行链，不在Core
中引入文件、线程或Transport（传输层）职责。

当前实现范围：

- 专用Schema 0.5 C3离线链：复用同一可执行程序提供inspect、encode、replay、compare，前三者
  强制Evidence 0.7记录；输出独立`pae.lab.cli/0.1`封装，旧Schema入口不变；

- `inspect`：从Binary或严格Hex文件读取一条完整Frame，跨配置内Pipeline执行唯一Message匹配
  和类型化Decode；
- `encode`：从严格`pae.lab.values/0.1`、`0.2`或`0.3`文件读取类型化业务值；0.2增加原生JSON
  `BOOL`，0.3增加规范十进制字符串`INT64`且仅配Schema 0.4；执行fail-closed（失败关闭）Encode；
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

Schema 0.5 C3链默认关闭，且必须同时显式开启其Compiler、Loader、Core和Lab依赖：

```powershell
cmake -S . -B out/build/protocol-lab-c3 `
  -DPAE_BUILD_PROTOCOL_LAB=ON `
  -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON `
  -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05=ON `
  -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF
cmake --build out/build/protocol-lab-c3 --config Release
```

Schema 0.6 CRC链在上述C3依赖之上还必须显式开启CRC编译与Lab能力：

```powershell
cmake -S . -B out/build/protocol-lab-crc `
  -DPAE_BUILD_PROTOCOL_LAB=ON -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON `
  -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON -DPAE_ENABLE_SCHEMA_V05_COMPILER=ON `
  -DPAE_ENABLE_PROTOCOL_LAB_SCHEMA_V05=ON -DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON `
  -DPAE_ENABLE_PROTOCOL_LAB_SCHEMA_V06_CRC=ON
cmake --build out/build/protocol-lab-crc --config Release
```

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
Schema 0.2执行统一使用Result/Record/Event 0.3；Schema 0.3执行统一使用Result/Record/Event
0.4，并增加唯一结构候选上的SUM8失败状态`PAE_LAB_CODEC_INTEGRITY_FAILED`。RX Metadata仍为0.2。Replay显式标记
`ENCODE_TX`、`DECODE_RX`或`NO_CODEC_REEXECUTION`。旧离线V0.1继续兼容，旧UDP草案、未知版本、
跨Schema替换配置Replay和跨代Run Compare均失败关闭；原始Frame比较不受格式代际限制。
默认文本输出便于人工查看。`--expect-status <status>`可把预期的Codec失败
作为成功用例返回`0`，但Compare发现差异始终返回`6`。

Schema 0.5专用链中，inspect/encode/replay必须显式提供`--record-root`；Replay只使用合格
Record 0.7 Bundle内的原配置、输入与指定Pipeline，不接受配置替换。Compare只读两个合格的
C执行Bundle，不调用Codec也不发布新Bundle。CLI 0.1进程退出和`--expect-status`只控制终端判定，
不改写Result 0.6的执行状态、退出码或确定性指纹；完整命令与人工记录方式见
[C3人工离线验收单](../../docs/manual-dec042b-c3-offline-acceptance.md)。

Schema 0.6统一使用Result 0.7、指纹0.7和Record 0.8，包括无`integrity`及继续使用SUM8的消息。
成功Run、完整性失败Run及其链式Replay均按新代完整读取；失败Replay即使比较`EQUAL`仍保留
当前失败和退出5。Schema 0.5/0.6 Run直接Compare在确定性指纹域比较前拒绝，旧Result 0.6、
Record 0.7及历史指纹保持不变。CRC参数、算法及证据边界见
[CRC确认契约](../../docs/crc-minimal-contract-draft.md)。

Schema 0.7固定完整记录长度字段链必须继续显式开启Schema 0.5、0.6及对应Lab能力，再增加
`PAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON`和
`PAE_ENABLE_PROTOCOL_LAB_SCHEMA_V07_LENGTH=ON`。它统一使用Result/指纹0.8、Record 0.9，
Event仍为0.7。Encode业务Values必须省略computed长度字段；Inspect在结构唯一后先验证长度、
再验证SUM8/CRC。长度失败及其Replay保持退出5，比较EQUAL不改变当前失败状态；跨代Run Compare
失败关闭。详见[长度字段契约](../../docs/length-field-minimal-contract.md)。

Schema 0.8有界变长链使用Result/指纹0.9、Record 0.10及Event 0.7。旧Values 0.1～0.4
继续保持原接受域；Values 0.5只新增显式空BYTES `"hex":""`，且仅可与Schema 0.8
执行。Result 0.9对空BYTES要求raw/logical同为空串且enum_known为false；旧Result不放宽。
空Encode/Inspect产物可读、可Compare并支持Replay A/B；Plan的非零最小载荷仍由Core
拒绝。详见[有界变长契约](../../docs/bounded-variable-record-contract.md)。

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
