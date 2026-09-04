# PAE Protocol Lab

`pae_protocol_lab`是PAE当前内部的离线协议实验与复现CLI（Command-Line Interface，命令行
接口）。它复用同一条`ConfigCompiler -> ProtocolPlan -> CompleteRecordCodec`执行链，不在Core
中引入文件、线程或Transport（传输层）职责。

当前实现范围：

- `inspect`：从Binary或严格Hex文件读取一条完整Frame，跨配置内Pipeline执行唯一Message匹配
  和类型化Decode；
- `encode`：从严格`pae.lab.values/0.1`文件读取类型化业务值，执行fail-closed（失败关闭）
  Encode并输出规范Hex；
- `replay`：校验既有Evidence Bundle的`COMPLETE`和`SHA256SUMS`后，使用内嵌配置或显式新
  配置重新执行，并生成一个不可覆盖的新Run；
- `compare`：比较两个Frame或两个Run；Run比较排除时间、绝对路径和Run ID，并报告
  `WIRE_BYTES`、`MESSAGE_MATCH`、`STATUS`、`TYPED_FIELDS`等差异类别；
- 可选Evidence Bundle：记录实际配置、Values、原始Frame、机器结果、单行JSONL事件和完整
  SHA-256清单。

当前不包含UDP（User Datagram Protocol，用户数据报协议）、TCP、串口、CAN、GUI、生产
Trace、安装导出或公共API兼容承诺。

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
```

`inspect`和`encode`只有提供`--record-root`时才写Evidence Bundle；`replay`总会创建新Run，
未指定`--record-root`时写到源Run的父目录。`--output json`输出
`pae.lab.result/0.1`；默认文本输出便于人工查看。`--expect-status <status>`可把预期的Codec失败
作为成功用例返回`0`，但Compare发现差异始终返回`6`。

完整契约、Values示例、退出码和证据边界见
[Protocol Lab Contract V0.1](../../docs/protocol_lab_contract_v0.1.md)。
