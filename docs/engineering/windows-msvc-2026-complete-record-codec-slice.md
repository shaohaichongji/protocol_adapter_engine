# PAE COMPLETE_RECORD Codec Windows 内部切片验证报告

> 历史基线：本文保存2026-09-01、冻结执行计划重构前的`52/52`验证事实，不回写为最新实现结果。2026-09-02首次公开推送前，旧实现证据衍生向量的标识、布局和Hash已从候选Git历史中移除；当前`SYNTHETIC_FROM_SCRATCH（从零人工设计）`向量不属于原`52/52`证据。当前实现与重新执行的门禁见[Frozen Execution Plan Windows内部切片验证报告](windows-msvc-2026-frozen-execution-plan-slice.md)。

## 1. 结论

截至2026-09-01，PAE（Protocol Adapter Engine，协议适配引擎）首个`COMPLETE_RECORD（完整记录）`Codec（编解码器）内部切片已在Windows x64、VS（Visual Studio）18 2026和MSVC（Microsoft Visual C++，微软C/C++编译器）下完成实际配置、构建与契约测试。

本轮实际结果为：

- 不构建测试时，Release只生成并成功构建`pae_protocol_plan`和`pae_protocol_core_slice`两个静态库目标；
- Codec Release干净构建和Debug构建成功，CTest各`1/1`，直接Runner各`52/52`；
- Loader/SchemaIr（加载器/类型化中间表示）Release和Debug回归CTest各`1/1`，Runner为`21/21`；
- JSON（JavaScript Object Notation，JavaScript对象表示法）Parser Spike、Loader和Codec共存的Release配置CTest为`22/22`；
- `src/protocol_plan`和`src/protocol_core`静态扫描未发现`yyjson`或`config_compiler`文本依赖。

这些结果只证明当前Windows内部切片及Synthetic Engine Vector（合成引擎向量）契约成立，不是正式协议、真实抓包、硬件或现场正确性证据，也不是性能结论。

## 2. 当前切片范围

当前Codec直接消费不可变`PlanBundle`，通过`pipeline_index`选择有向Pipeline（管线），支持：

- 确定性`frame_length_equals`和`fixed_bytes` Matcher（匹配器）；
- `UINT64`、固定长度`BYTES`和`ENUM`类型；
- Big Endian（大端）和Little Endian（小端）逐字节整数读写；
- 未知ENUM的`reject`与Decode侧`preserve + tainted（保留原始值并标记污染）`；
- 动态`input`和`UINT64 constant` Encode Source（编码来源）；
- Plan-scoped Reference（计划作用域引用）；
- 调用方提供Decode Slot（解码槽位）和Encode Buffer（编码缓冲区）；
- Encode预检、完整写入、最终Matcher/字段复核及失败时`bytes_written=0`。

`pae_protocol_plan`已经从`config_compiler`抽离。`pae_protocol_core_slice`只依赖该yyjson-free（不依赖yyjson）Plan目标，不读取JSON，也不包含`STREAM_CHUNK（流式字节块）`Framer（切帧器）、Integrity（完整性校验）、Receive Gate（接收门禁）、Mapping（映射）、Session（会话）、Runtime（运行时）注册或稳定公共API（Application Programming Interface，应用程序接口）。

## 3. Synthetic向量与协议证据边界

原`52/52`运行使用的两条向量在Codec实现前人工写定完整Frame（帧）、Decode期望和Encode输入。为保证公开仓库不携带实现证据衍生的具体协议布局，本报告不保留其文件名、字节和Hash；只保留已实际执行的测试数量与环境事实。

- 当前仓库改用完全独立的`synthetic_lab_exchange`人工协议；
- 当前向量的13/11字节布局、常量、Little Endian（小端）和动态值均为从零设计；
- 新向量的实际结果只能引用重新执行后的Frozen Execution Plan报告，不能反向写成原历史运行证据；
- 任一Synthetic Engine Vector逐字节通过都不能据此修改生产协议实现或宣称设备互通正确。

当前六份公开向量源文件的SHA-256（Secure Hash Algorithm 256-bit，256位安全散列算法）为：

| 向量 | 文件 | 文件SHA-256 |
| --- | --- | --- |
| Command | `lab_command_001.frame.hex` | `492AF67348459F1450FC87287B5CC6907D6FF5E75A295DBA36B489E09909040D` |
| Command | `lab_command_001.decode_expected.tsv` | `55F98CD3BF4DA09D0228781AE7F24E9843B9DA0F10FCAE04CEAF268D974B641F` |
| Command | `lab_command_001.encode_input.tsv` | `714D63AC19391AA8F52A31EF0BB6C7B84291FAA4BCF723B096DC202F06856DD3` |
| Report | `lab_report_001.frame.hex` | `AB2C014349E0C673FBF38C692DC2B5F1CB70609CF00968D94449A848E3B24E31` |
| Report | `lab_report_001.decode_expected.tsv` | `3734FA466D7412FF007880873FC9F9A5AACB4513A0837B3D36A4AD14D25E8CE5` |
| Report | `lab_report_001.encode_input.tsv` | `69BCD63709A6236C120B7072D28040035BCC3EF43B13C8A2F15E248216681246` |

其中`.frame.hex`散列是文本源文件级散列，不是解码后原始字节数组的散列。Configure阶段校验六份源文件，CTest执行前再次校验实际运行副本，再启动Codec Runner。

## 4. 实际验证环境

| 项目 | 实际值 |
| --- | --- |
| CMake | 4.4.3 |
| Generator（生成器） | Visual Studio 18 2026，x64 |
| C/C++ Compiler（编译器） | MSVC 19.51.36256 |
| Windows SDK（Software Development Kit，软件开发工具包） | 10.0.22621 |
| 目标系统 | Windows 10.0.19045，x64 |

## 5. 实际执行命令

以下命令的工作目录均为仓库根。为避免重复长路径，先定义：

```powershell
$cmakeExe = 'cmake'
$ctestExe = 'ctest'
```

### 5.1 Codec无测试构建

```powershell
& $cmakeExe -S . -B out/build/windows-msvc-codec-compile-check -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=OFF -DPAE_BUILD_JSON_PARSER_SPIKE=OFF -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=OFF -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON
& $cmakeExe --build out/build/windows-msvc-codec-compile-check --config Release --parallel
```

### 5.2 Codec Release与Debug

```powershell
& $cmakeExe --preset windows-msvc-codec-slice
& $cmakeExe --build --preset windows-msvc-codec-slice-release --clean-first --parallel
& $ctestExe --preset windows-msvc-codec-slice-release
& 'out\build\windows-msvc-codec-slice\tests\protocol_core\Release\pae_complete_record_codec_contract_tests.exe' 'out\build\windows-msvc-codec-slice\tests\protocol_core\data'

& $cmakeExe --build out/build/windows-msvc-codec-slice --config Debug --parallel
& $ctestExe --test-dir out/build/windows-msvc-codec-slice -C Debug --output-on-failure
& 'out\build\windows-msvc-codec-slice\tests\protocol_core\Debug\pae_complete_record_codec_contract_tests.exe' 'out\build\windows-msvc-codec-slice\tests\protocol_core\data'
```

### 5.3 Loader Release与Debug回归

```powershell
& $cmakeExe --preset windows-msvc-loader-slice
& $cmakeExe --build --preset windows-msvc-loader-slice-release --clean-first --parallel
& $ctestExe --preset windows-msvc-loader-slice-release
& $cmakeExe --build out/build/windows-msvc-loader-slice --config Debug --parallel
& $ctestExe --test-dir out/build/windows-msvc-loader-slice -C Debug --output-on-failure
& 'out\build\windows-msvc-loader-slice\tests\config_compiler\Release\pae_config_compiler_contract_tests.exe' 'out\build\windows-msvc-loader-slice\tests\config_compiler\data'
```

### 5.4 Parser、Loader与Codec Release共存

```powershell
& $cmakeExe -S . -B out/build/windows-msvc-all-slices -G 'Visual Studio 18 2026' -A x64 -DBUILD_TESTING=ON -DPAE_BUILD_JSON_PARSER_SPIKE=ON -DPAE_JSON_SPIKE_CANDIDATE=all -DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON -DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON
& $cmakeExe --build out/build/windows-msvc-all-slices --config Release --clean-first --parallel
& $ctestExe --test-dir out/build/windows-msvc-all-slices -C Release --output-on-failure
```

### 5.5 依赖文本扫描与六份向量文件散列

```powershell
rg -n -i 'yyjson|config_compiler' src/protocol_plan src/protocol_core

$goldenVectorDir = 'tests\protocol_core\golden\synthetic_lab_exchange'
Get-ChildItem -LiteralPath $goldenVectorDir -File |
  Sort-Object Name |
  Get-FileHash -Algorithm SHA256
```

`rg`返回退出码`1`且无输出，按其语义表示没有匹配，不是扫描执行错误。

## 6. 执行结果

| 门禁 | 配置 | 实际结果 |
| --- | --- | --- |
| Codec无测试构建 | Release，`BUILD_TESTING=OFF` | 配置和构建成功；只生成并构建`pae_protocol_plan`、`pae_protocol_core_slice`静态库 |
| Codec契约 | Release干净构建 | 构建成功；CTest `1/1`；直接Runner `52/52` |
| Codec契约 | Debug | 构建成功；CTest `1/1`；直接Runner `52/52` |
| Loader回归 | Release干净构建 | 构建成功；CTest `1/1`；Runner `21/21` |
| Loader回归 | Debug | 构建成功；CTest `1/1`；Runner `21/21` |
| 三切片共存 | Release干净构建 | Parser + Loader + Codec CTest `22/22` |
| Core/Plan依赖扫描 | 当前源码 | `yyjson|config_compiler`零命中 |

Codec Runner把52个唯一`case_id`的固定全集作为门禁，拒绝未知、重复或遗漏用例。覆盖范围包括：

- Manifest（清单）约束及Request/Response Synthetic Engine Vector的Decode、Encode逐字节比较；
- Encode重复确定性和输入字段顺序独立性；
- Big Endian/Little Endian各1至8字节的非对齐读写；每个组合覆盖普通模式、零值和该宽度最大值；
- ENUM `reject`和`preserve + tainted` Decode；
- 未知、多匹配、截断、额外字节和输出槽位不足；
- 无效Plan、缺失/重复/跨Plan/跨Message字段引用、类型和宽度错误；
- BYTES长度、Enum引用、跨Plan Enum引用、常量覆盖、Message归属及Buffer容量错误；
- 描述符/输入/输出区间重叠、Decode输入/槽位重叠、Frame未完全覆盖及最终复核失败；
- replaceable `operator new/new[]`（可替换全局new/new[]）普通、nothrow（不抛异常）及aligned（对齐）路径探针；
- 代表性Decode/Encode成功与失败调用期间的replaceable `operator new/new[]`计数，以及Core源码对`malloc/calloc/realloc/free`直接调用的静态拒绝门禁。

## 7. 资源和性能边界

本轮不能写成“已经完成零分配、高性能Codec”，原因如下：

1. Zero allocation（零动态分配）尚未被完整证明。当前证据是：Runner已经用普通、数组、nothrow和aligned探针证明replaceable `operator new/new[]`计数器生效，并证明所覆盖的代表性Decode/Encode成功与失败调用期间计数没有增加；Configure阶段还静态拒绝当前Core源码直接调用`malloc/calloc/realloc/free`。这仍不覆盖自定义Allocator（分配器）、宿主回调、未来扩展、间接C分配或所有输入组合。
2. 在本文记录的2026-09-01历史实现中，Codec仍在每次调用时执行防御性Plan校验；字段、Matcher和候选Message检查中存在`O(n²)`二次复杂度路径。该历史结论已被后续Frozen Execution Plan内部切片替代，但本文测试规模较小且未运行正式吞吐、延迟、CPU（Central Processing Unit，中央处理器）占用或峰值内存基准，仍不能形成性能结论。
3. `unknown_enum_policy=preserve`当前只验证Decode能够观察未知Raw Value（原始值）并设置`tainted`。Encode仍要求有效的已知Enum Entry引用，尚不支持把未知Raw Value透明地重新编码。
4. Encode失败时以`bytes_written=0`表示结果不可交付；若错误发生在写入后的最终复核阶段，调用方Buffer可能已被修改，仍必须丢弃其内容。

## 8. 未验证范围

以下仍为`OPEN / UNVERIFIED（待定/未验证）`：

- Linux GCC（GNU Compiler Collection，GNU编译器套件）和Clang构建；
- Address/Undefined Behavior Sanitizer（地址/未定义行为检测器）等动态检查；
- C99语言入口、C ABI（Application Binary Interface，应用二进制接口）和稳定C/C++公共API；
- `STREAM_CHUNK`流式Framing、Integrity、Receive Gate、Mapping、Session和Runtime注册；
- 正式吞吐、延迟、低占用、最大帧及资源上限；
- 目标板、真实设备、硬件和现场验证；
- 由正式协议或真实抓包支撑的独立Golden Vector。

因此，该切片仍是内部、不可安装、不可导出的Windows-first（Windows优先）实现基础，不得直接替换生产协议代码。
