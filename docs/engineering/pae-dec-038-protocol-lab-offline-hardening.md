# PAE-DEC-038 Protocol Lab Offline Hardening

## 1. 决策状态

状态：`CONFIRMED / PARTIALLY VERIFIED（已确认、部分验证）`。

本Decision（决策）只加固Protocol Lab（协议实验室）的离线实现，不增加UDP（User Datagram
Protocol，用户数据报协议）、TCP、串口、CAN、新命令或新协议能力。现有CLI（Command-Line
Interface，命令行接口）、Values格式、结果字段、退出码、比较类别和Evidence Bundle（证据包）
目录结构保持不变。

## 2. 内部模块边界

原单文件实现拆分为以下非公共模块：

| 模块 | 职责 |
| --- | --- |
| `protocol_lab` | 应用编排与唯一进程入口Facade（门面） |
| `cli_options` | CLI解析、互斥关系和帮助文本 |
| `protocol_operations` | 输入读取、严格Values解析、配置编译、Decode/Encode |
| `evidence_bundle` | Evidence Bundle事务、完整性复核与Run读取 |
| `result_format` | Text/JSON序列化、差异类别和确定性指纹 |
| `sha256` | 项目内SHA-256实现 |
| `lab_types` | 模块共享的内部值类型与版本常量 |

这些模块只组成内部静态目标`pae_protocol_lab_internal`和单一可执行程序
`pae_protocol_lab`；二者均不安装、不导出，不构成稳定公共API。

## 3. 文件系统事务边界

`RecordFileSystem`是Protocol Lab内部依赖注入接口，只封装Evidence Bundle写事务所需的目录、
文件写入并关闭、重新读取和重命名操作。生产路径使用`StandardRecordFileSystem`；测试路径通过
显式对象注入故障，不使用隐藏CLI参数或环境变量。

该接口不进入PAE Core、ProtocolPlan或公共头文件。配置编译和Codec仍沿用既有
`ConfigCompiler -> ProtocolPlan -> CompleteRecordCodec`链路。

## 4. 故障注入门禁

独立CTest覆盖：

1. Run目录创建确认失败；
2. 第N个文件写入失败；
3. 文件关闭确认失败；
4. 写后重新读取失败；
5. 长度或内容/Hash复核不一致；
6. `COMPLETE`写入失败；
7. `SHA256SUMS`写入失败；
8. `.inprogress`到正式Run目录的最终重命名失败。

每个注入场景均经应用编排返回退出码`7`，不生成正式Run目录、不报告记录成功，并保留一个可供
诊断的`.inprogress`目录。生产环境若操作系统从一开始就禁止创建Run目录，则物理上没有目录可
保留，但仍必须返回`7`且不得产生正式Run或虚假成功。

提交前审查进一步确认机器结果必须在状态变更为`RECORD_FAILED`后重新计算确定性指纹。自动化
同时断言记录失败状态、稳定诊断标识、非空失败指纹，以及失败指纹不同于同一协议操作成功时的
指纹，防止机器结果状态与指纹载荷再次失配。

## 5. 验证结论与边界

Windows x64、MSVC Debug/Release下：

- Lab专用CTest均为`3/3 PASS`；
- 全切片共存CTest均为`29/29 PASS`；
- Lab-on/Testing-off构建成功；
- Product-only关闭Lab时不生成Lab目标或文件；
- `clang-format --dry-run --Werror`、`git diff --check`和边界扫描纳入本检查点门禁。

这只验证当前离线切片与Synthetic测试数据。UDP、Linux、Sanitizer、性能、真实设备、硬件、现场
和`LAB_EXCHANGE_PASS`仍未验证；独立Protocol Lab JSON Schema继续后置到主PAE Schema与参考
Validator（校验器）边界统一之后。
