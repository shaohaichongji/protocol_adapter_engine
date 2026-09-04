# Protocol Lab Offline Slice Windows验证报告

## 1. 结论

`PAE-DEC-037/038`的Offline Replay Slice（离线重放切片）已在Windows x64、MSVC Debug/Release
下完成实现、结构加固和执行验证：

- `pae_protocol_lab`复用现有`ConfigCompiler -> ProtocolPlan -> CompleteRecordCodec`链路；
- `inspect/encode/replay/compare`四个离线命令可执行；
- Evidence Bundle（证据包）按`.inprogress`事务生成，包含原始字节、内嵌输入、单行JSONL、
  机器结果、`COMPLETE`和排序SHA-256清单；
- Replay和Run Compare会在读取结果前复核完整Hash清单；
- 原1910行级单文件已拆分为应用编排、CLI、协议操作、Evidence Bundle、结果格式、SHA-256和
  共享类型模块；
- 内部`RecordFileSystem`依赖注入已覆盖8类记录事务失败，全部失败返回退出码`7`、不生成正式
  Run并保留可诊断`.inprogress`；
- Lab可在`PAE_BUILD_TESTING=OFF`时独立构建，Product-only关闭Lab时不生成该目标；
- 全切片共存CTest在Debug和Release下均为`29/29 PASS`。

这些结果只证明当前公开Synthetic配置上的离线工具行为，不证明UDP互通、真实设备互通、正式
协议Golden（黄金证据）、Linux兼容、硬件或现场可用性。

## 2. 验证环境

| 项目 | 实际证据 |
| --- | --- |
| CMake | `4.4.3`，路径`D:\develop_env\cmake-4.4.3` |
| Generator（生成器） | `Visual Studio 18 2026`，x64 |
| MSVC | `19.51.36256.0` |
| Windows SDK | `10.0.22621.0` |
| 配置 | Debug、Release |
| 测试数据 | 仓库公开`synthetic_lab_exchange`，不使用真实协议或现场数据 |

## 3. 构建矩阵

### 3.1 Lab专用预设

执行：

```powershell
cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-debug --parallel
ctest --preset windows-msvc-protocol-lab-debug --output-on-failure
cmake --build --preset windows-msvc-protocol-lab-release --parallel
ctest --preset windows-msvc-protocol-lab-release --output-on-failure
```

结果：Debug和Release均构建成功；两种配置均为`3/3 PASS`，包含Config Compiler合同、
Protocol Lab聚合离线合同和独立Evidence Bundle故障注入合同。

### 3.2 全切片共存回归

在独立构建目录中同时打开JSON Parser Spike、Loader/SchemaIr、COMPLETE_RECORD Codec、
Protocol Conformance Runner和Protocol Lab。

结果：

| 配置 | 构建 | CTest |
| --- | --- | ---: |
| Debug | PASS | `29/29 PASS` |
| Release | PASS | `29/29 PASS` |

第28项为`pae.tools.protocol_lab.offline_contract`，新增第29项为
`pae.tools.protocol_lab.evidence_bundle_transaction`；既有27项均继续通过。

### 3.3 构建边界

- `PAE_BUILD_PROTOCOL_LAB=ON`、`PAE_BUILD_TESTING=OFF`且其他切片关闭：Release构建成功并
  生成`pae_protocol_lab.exe`；Cache内其他切片开关保持`OFF`；
- Product-only全部切片和Lab关闭：Release构建成功，生成工程和构建目录中没有
  `pae_protocol_lab`目标或可执行文件。

### 3.4 静态与范围门禁

- `clang-format --dry-run --Werror`通过；
- `git diff --check`通过；
- Protocol Lab源码和故障注入测试未出现Socket、线程、Qt、Boost、IOCP或Epoll实现；
- 敏感引用扫描只命中文档中明确的Loopback示例`127.0.0.1:0`，没有发现凭据、私钥、真实Endpoint
  或外部生产项目路径；
- 暂存区为空，本检查点未Stage、未Commit、未Push。

## 4. 自动化覆盖

聚合离线合同实际覆盖：

1. `inspect`正常匹配、规范大写Hex、类型化字段、Unknown Message及`--expect-status`；
2. `encode`逐字节等于固定Frame，并拒绝缺失字段、重复字段、类型错误和宽度越界；
3. Binary/Hex输入、混合形式Frame Compare、相同返回`0`、不同返回`6`；
4. Inspect和Encode Evidence Bundle均保留实际输入，Replay均生成新Run且结果一致；
5. 显式新配置Replay记录`cross_config_replay=true`；
6. Run Compare排除环境元数据，并输出`OPERATION_KIND`、`WIRE_BYTES`等差异类别；
7. `COMPLETE`、JSON/JSONL、Frame、Values和`SHA256SUMS`必需文件存在，清单按路径排序且
   每个Hash重新计算一致；未列入清单的额外文件和符号链接会被拒绝；
8. 篡改已记录Frame后，Replay在解析结果前因SHA-256不一致失败关闭；
9. CLI重复参数、缺失文件、配置编译失败、协议操作失败、比较不同和记录根错误分别覆盖
   退出码`2/3/4/5/6/7`；
10. 成功运行后不残留`.inprogress`目录，记录入口失败不交付虚假完整Run。
11. 独立故障注入CTest通过应用级依赖注入覆盖Run目录创建确认、第N次写入、关闭、重读、
    长度/内容/Hash复核、`COMPLETE`、`SHA256SUMS`和最终目录重命名失败；8类场景均返回`7`、
    不产生正式Run并保留一个`.inprogress`目录。

## 5. 审查中修复的问题

- Windows中文路径最初通过本地代码页写入JSON，Replay被Strict UTF-8 Parser拒绝；现改为
  `generic_u8string()`，机器记录统一为UTF-8路径文本；
- CMake测试对固定长度SHA-256的正则写法不具可移植性；现改为字符集匹配后独立校验64字符；
- 初版Event序列化会把字段数组展开成多行，违反JSONL“一行一个对象”；现使用紧凑字段序列化，
  并增加单行门禁；
- Run Compare初版只报告布尔差异；现增加机器可读差异类别，并在读取Run结果前核验完整Hash
  清单。
- 提交前审查发现记录失败后虽然状态已改为`RECORD_FAILED`，但确定性指纹仍对应记录前状态；现
  在失败结果字段落定后重新计算指纹，并由故障注入测试确认失败指纹非空且不同于同一成功操作。

## 6. 当前未验证与保留边界

- 未实现UDP、TCP、串口、CAN、GUI、生产Trace或主动发送；退出码`8/9`尚无离线路径；
- `main()`已把未处理异常映射为退出码`10`，但未为了测试故意触发未定义异常；
- 已覆盖可控文件系统调用失败，但未模拟断电、进程强杀、文件系统缓存持久化或真实磁盘故障；
- 未执行Linux GCC/Clang、Sanitizer、性能、峰值内存或长时间稳定性验证；
- 未连接真实设备、独立仿真端或现场环境，`LAB_EXCHANGE_PASS`未评估；
- Synthetic自编码、自解析和Replay不能升级为`PROTOCOL_GOLDEN_PASS`；
- 当前变更尚未Stage、Commit或Push。
