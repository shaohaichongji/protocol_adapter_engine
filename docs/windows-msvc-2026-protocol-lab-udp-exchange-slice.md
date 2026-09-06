# Protocol Lab Windows UDP Exchange验证报告

## 1. 结论

`PAE-DEC-039`的Protocol Lab Windows UDP（User Datagram Protocol，用户数据报协议）单次
Exchange（交换）切片已实现，并在Windows x64、MSVC Debug/Release下完成自动化验证：

- Windows生产Adapter使用Winsock与RAII（资源获取即初始化）管理启动、Socket和清理；
- `udp-exchange`完成`Encode → TX持久化 → 单次发送 → 有界等待 → RX持久化 → Peer检查 →
  Decode → Evidence Bundle发布`；
- 无`--send`路径不调用UDP Adapter，不初始化Winsock或创建Socket；
- 数字IPv4、Endpoint（端点）、超时、非Loopback双确认、组播/广播/未指定远端与Payload上限
  均执行失败关闭；
- 实际动态本地端口、响应来源、TX/RX字节、发送/接收/解析状态和有序事件进入Evidence Bundle；
- UDP记录升级为V0.2：阶段事件在真实发生点采集，RX来源Metadata在Peer/Decode前写入并复核，
  Replay模式和历史Transport/当前执行/比较结果分别表达；
- Protocol Lab专用Debug/Release均`4/4 PASS`，全切片共存Debug/Release均`30/30 PASS`；
- Lab-on/Testing-off和Product-only（仅产品）构建边界通过。

自动化中的真实Socket收发只使用`127.0.0.0/8` Loopback（本机回环）。本报告不证明外部网络、
真实设备、现场协议、Linux或生产环境可用；外部网络调试工具人工门禁尚未执行，
`LAB_EXCHANGE_PASS=NOT_EVALUATED（未评估）`。

## 2. 验证环境

| 项目 | 实际证据 |
| --- | --- |
| 日期 | 2026-09-05初始实现；2026-09-06补充拍板复核 |
| CMake | `4.4.3`，路径`D:\develop_env\cmake-4.4.3` |
| Generator（生成器） | `Visual Studio 18 2026`，x64 |
| MSVC | `19.51.36256.0` |
| Windows SDK | `10.0.22621.0` |
| 配置 | Debug、Release |
| 自动化网络范围 | 仅`127.0.0.0/8` Loopback |
| 协议数据 | 仓库公开`synthetic_lab_exchange`，不使用真实协议或现场数据 |

## 3. 实现边界

平台无关的Endpoint解析、请求/响应结构和内部`IUdpExchangeAdapter`位于
`tools/protocol_lab/udp_exchange.h`；Windows系统调用只位于同目录的`udp_exchange.cpp`。
Adapter不安装、不导出，PAE Core、ProtocolPlan和Codec头文件不包含Winsock类型。

Windows执行采用一次同步调用和`select`有界等待，不进行忙轮询、不创建产品级常驻线程、不
重试、不扫描、不持续监听。测试专用Peer使用一个有界线程模拟单次Loopback响应，并在每个用例
结束前回收。

首版限制为：

- 只接受数字IPv4，不执行DNS；
- 本地默认`127.0.0.1:0`，远端端口必须为`1..65535`；
- 任一本地或远端地址不是Loopback时，必须同时提供`--send --allow-non-loopback`；
- 拒绝远端`0.0.0.0`、组播、有限广播，并保守拒绝末字节为`255`的广播候选地址；
- 超时范围`1..60000 ms`，默认`2000 ms`；
- 只发送一个Datagram并接收至多一个Datagram；
- IPv4 UDP Payload最大`65507 bytes`，同时不得超过编译后Plan Profile限制。

## 4. 自动化覆盖

Windows专用`pae.tools.protocol_lab.udp_exchange`实际覆盖：

1. 无`--send`时Adapter调用次数为零，只生成TX预览证据；
2. 真实Winsock Loopback单次请求/响应，Peer逐字节收到固定TX，Lab逐字节记录固定RX；
3. 动态本地端口和实际响应来源写入机器结果与Run记录；
4. `TX FRAME → SEND_INTENT → SEND_RESULT → RX FRAME`在真实阶段采集，时间戳共用Run单调原点且
   非递减；启动、Socket创建、绑定失败不产生发送事件；
5. 成功交换Bundle由离线Replay读取，显式执行`DECODE_RX`，确定性比较相等且Replay不创建Socket；
6. 真实Winsock Loopback无响应超时，返回退出码`9`和`UDP_RESPONSE_TIMEOUT`；
7. 启动、Socket创建、绑定、发送、接收、截断和平台不支持的稳定诊断映射；
8. TX记录失败发生在Adapter调用前，RX记录失败发生在Decode前，均返回退出码`7`且不发布
   正式Run；
9. Peer不匹配、超长响应和错误响应均先保留RX原始字节，再失败关闭；
10. 非Loopback预览/发送双确认、本地非Loopback、DNS、零端口、非法超时、组播、广播、
    未指定远端和未知接收Pipeline拒绝；
11. Adapter自身在Winsock启动前拒绝超过`65507 bytes`的请求；该用例使用文档地址但没有发生
    网络发送。
12. 预览、真实超时和全部Transport失败终态执行`ENCODE_TX`；完整RX进入Decode的成功与失败
    终态执行`DECODE_RX`；Peer不匹配、超长和截断执行`NO_CODEC_REEXECUTION`且Codec调用为零；
13. RX Metadata写入、关闭、重读和字段绑定故障均返回`7`，并由执行观察器证明未进入Decode；
14. 自洽重算清单但篡改来源Metadata、缺失Metadata、旧UDP V0.1草案及未知版本均失败关闭；
15. Replay新Run完整发布、源清单不变，历史Transport事实与当前执行结果分离；无Codec模式比较
    为`null / NOT_EVALUATED`，零长度响应与未收到响应保持可区分。

注入Adapter覆盖状态机和安全分支，不构成真实网络证据；只有第2项和第6项实际经过Winsock，
且均使用Loopback。

## 5. 构建与测试结果

### 5.1 Protocol Lab专用矩阵

执行：

```powershell
cmake --preset windows-msvc-protocol-lab
cmake --build --preset windows-msvc-protocol-lab-debug --clean-first --parallel
ctest --preset windows-msvc-protocol-lab-debug --output-on-failure
cmake --build --preset windows-msvc-protocol-lab-release --clean-first --parallel
ctest --preset windows-msvc-protocol-lab-release --output-on-failure
```

| 配置 | 构建 | CTest |
| --- | --- | ---: |
| Debug | PASS | `4/4 PASS` |
| Release | PASS | `4/4 PASS` |

四项分别为Config Compiler合同、离线合同、Windows UDP Exchange和Evidence Bundle事务故障
注入。

2026-09-06补充实现后的实际结果仍为Debug `4/4 PASS`、Release `4/4 PASS`；全切片共存
Debug/Release仍为`30/30 PASS`。本次还单独执行了Protocol Lab关键三项Debug测试，结果
`3/3 PASS`。

### 5.2 全切片共存回归

独立目录`out/build/windows-msvc-all-slices-dec039`同时打开JSON Parser Spike、
Loader/SchemaIr、COMPLETE_RECORD Codec、Protocol Conformance Runner和Protocol Lab。

| 配置 | 构建 | CTest |
| --- | --- | ---: |
| Release | PASS | `30/30 PASS` |
| Debug | PASS | `30/30 PASS` |

新增UDP测试为第29项，既有29项全部继续通过。

### 5.3 构建边界

- `out/build/windows-msvc-protocol-lab-no-testing-dec039`以`PAE_BUILD_PROTOCOL_LAB=ON`、
  `PAE_BUILD_TESTING=OFF`和`BUILD_TESTING=OFF`完成Release构建，生成Lab可执行程序且CTest为
  `0`项；
- `out/build/windows-msvc-product-only-dec039`以Codec产品切片开启、Lab和Testing关闭完成
  Release构建；PAE目标只有`pae_protocol_plan`和`pae_protocol_core_slice`，未发现Protocol
  Lab、UDP或`ws2_32`引用。

### 5.4 2026-09-06补充拍板实际命令

```powershell
cmake --build --preset windows-msvc-protocol-lab-debug --clean-first --parallel
ctest --preset windows-msvc-protocol-lab-debug --output-on-failure
cmake --build --preset windows-msvc-protocol-lab-release --clean-first --parallel
ctest --preset windows-msvc-protocol-lab-release --output-on-failure

cmake --build out/build/windows-msvc-all-slices-dec039 --config Debug --clean-first --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec039 -C Debug --output-on-failure
cmake --build out/build/windows-msvc-all-slices-dec039 --config Release --clean-first --parallel
ctest --test-dir out/build/windows-msvc-all-slices-dec039 -C Release --output-on-failure

cmake --build out/build/windows-msvc-protocol-lab-no-testing-dec039 --config Release --clean-first --parallel
ctest --test-dir out/build/windows-msvc-protocol-lab-no-testing-dec039 -C Release -N
cmake --build out/build/windows-msvc-product-only-dec039 --config Release --clean-first --parallel
ctest --test-dir out/build/windows-msvc-product-only-dec039 -C Release -N
```

该轮日志摘要：Lab Debug/Release分别`4/4 PASS`；全切片Debug/Release分别`30/30 PASS`；两个
隔离构建均成功且CTest为`0`项。`clang-format --dry-run --Werror`和`git diff --check`通过；
PowerShell `ConvertFrom-Json`实际解析`152`个V0.2 JSON、`68`个JSONL文件共`148`行；`include/src`
的Winsock/UDP Adapter关键词命中为`0`，Product-only生成树的Lab/UDP/`ws2_32`命中为`0`。

## 6. 审查中修复的问题

- Windows头文件的`max`宏最初干扰C++标准库调用；在Winsock引入前显式定义`NOMINMAX`；
- 动态端口最初只记录配置值`127.0.0.1:0`；绑定后通过`getsockname`记录实际本地Endpoint；
- 机器结果增加Transport和Endpoint字段后，Run读取器最初会按未知字段拒绝；已保持旧Bundle
  可读的可选字段策略；
- 确定性指纹最初会给所有离线操作追加空TX/RX字段；现只在`udp-exchange`加入TX/RX，避免改变
  既有离线指纹算法；
- 错误响应最初会把`response_decoded`标记为真；现仅在Decode成功时标记为真并补充断言；
- 非Loopback预览最初可绕过双确认；现任一非Loopback Endpoint都必须同时具备两个确认参数。
- 旧事件记录在`Complete()`阶段回填，不能证明实际阶段顺序；现改为阶段即时采集和统一单调
  时钟原点，且发送前置失败不再产生伪发送事件；
- 旧RX只记录字节，来源Endpoint未与原始帧形成独立可复核绑定；现新增V0.2 Metadata并把写入、
  关闭、重读、Hash/长度/来源绑定全部置于Peer检查和Decode门禁之前；
- 旧UDP Replay按响应存在性推断Encode/Decode，且混用了历史Transport终态；现用三种显式模式
  关闭歧义，并分离历史Transport、当前Codec执行和比较结论；
- UDP变化升级到V0.2，旧离线V0.1继续兼容；旧UDP V0.1草案和未知版本明确失败关闭。

### 6.1 三项P2复核与纠错

2026-09-06先基于既有Debug产物执行最小复现：

- Peer mismatch的Replay A具有顶层`response_received=false`、历史值`true`和11字节RX；再次
  Replay实际返回`3 / PAE_LAB_REPLAY_FRAME_ERROR`，问题成立；
- 删除`run_record_v0.2.json`并同步重算`SHA256SUMS`后，Replay实际返回`0`，问题成立；
- 修改TX Event的`frame_length`并同步更新Event文件Hash与清单后，Replay实际返回`0`，问题成立。

纠错后：

- Replay以显式模式/主体判断保存的RX和Codec路径，顶层网络接收状态不再用于推断；成功、错误
  响应、Peer mismatch、超长、截断和零长度响应均执行“原始Run→Replay A→Replay B”；
- `LoadStoredRun()`实际解析并严格校验Run Record；缺失Record、非法JSON、未知版本和Record/
  Result矛盾即使Hash自洽也被拒绝，拒绝路径不发布正式Run；
- Event严格校验完整字段集合、类型、种类和顺序，并把TX/RX/Replay Frame与Result、Metadata、
  Run Record及实际字节交叉绑定；新增长度、Hash、未知种类、错误类型、缺失/未知字段和
  `SEND_RESULT`先于`SEND_INTENT`的自洽Hash负例；
- Replay测试不再通过输出字符串中的同名字段模糊匹配，而是通过`LoadStoredRun()`按顶层字段及
  `historical_transport`精确断言当前收发/Decode、模式、主体、执行状态、比较状态、可空比较、
  稳定诊断和退出码。

修复后按Debug、Release顺序串行复核：Protocol Lab分别`4/4 PASS`（`3.39 s`、`1.98 s`），
全切片分别`30/30 PASS`（`13.92 s`、`2.97 s`）。对应CTest日志为：

- `out/build/windows-msvc-protocol-lab/Testing/Temporary/dec039-p2-lab-debug.log`；
- `out/build/windows-msvc-protocol-lab/Testing/Temporary/dec039-p2-lab-release.log`；
- `out/build/windows-msvc-all-slices-dec039/Testing/Temporary/dec039-p2-all-debug.log`；
- `out/build/windows-msvc-all-slices-dec039/Testing/Temporary/dec039-p2-all-release.log`。

最终Protocol Lab Debug产物中排除故障注入副本后，PowerShell `ConvertFrom-Json`实际解析
`48`个完整Bundle内的`96`个JSON及`48`个JSONL共`88`行，错误为`0`；故障注入中的非法
Record按预期不计入可交付Bundle解析统计。`clang-format --dry-run --Werror`检查全部12个当前
变更C/C++文件通过，`git diff --check`通过。本轮没有修改Adapter接口、CMake或目标依赖，
因此没有重复执行构建隔离门禁。

### 6.2 V0.1兼容与Cross-config Replay纠错

2026-09-06独立复现：

- 历史Run `run_20260904T174137Z_b0023c1efb4e175f`及原/当前程序仍存在。原程序执行
  `compare --left-run <run> --right-run <run> --output json`返回`EQUAL / 0`；修复前当前程序返回
  `INPUT_ERROR / 3`，诊断为`Run Record is missing property local_endpoint`；
- 在公开Synthetic配置中同时把Command Matcher `A7 31`改为`A8 31`、常量`12711`改为`12712`，
  两份配置均可编译，实际Encode字节由`0D00A731...`变为`0D00A831...`。修复前差异Replay本身
  返回`DIFFERENT / 6`，但`LoadStoredRun()`以本次Frame不等于历史TX为由拒绝该Bundle。

纠错后：

- 固化由真实历史Run规范化得到的通用旧V0.1夹具；仅清除机器路径，配置及Frame复用公开
  Synthetic样例，保留旧Result/Record/Event字段形状，并记录来源与变换；
- 旧V0.1的后加网络字段允许缺失，存在时仍严格校验；Load、Self-Compare和Replay通过，未知字段、
  错误类型、未知版本及文件矛盾仍失败关闭；当前V0.1继续由离线契约测试覆盖，旧UDP草稿继续拒绝；
- Cross-config差异Run的`frame_*`/Event绑定本次`A8 31`输出，`tx_frame_*`保持历史`A7 31`；差异
  Bundle完整读取通过，再次Replay以差异Run的本次执行指纹为比较基准而返回`EQUAL`，历史TX不变；
- 移除`mode_active`配置项的合法配置可编译，但使用原Values执行Encode得到
  `VALUES_INVALID / PAE_LAB_VALUES_UNKNOWN_ENUM_ENTRY`；生成的失败Bundle可读、当前Frame为空、
  历史TX保留、比较为`DIFFERENT`，未伪造空帧成功。

最终串行验证：Protocol Lab Debug/Release分别`4/4 PASS`（`3.60 s`、`2.16 s`），全切片
Debug/Release分别`30/30 PASS`（`14.11 s`、`3.12 s`）。日志：

- 两项兼容性针对性Debug/Release回归分别`1/1 PASS`（`2.99 s`、`1.66 s`），日志为
  `out/build/windows-msvc-protocol-lab/Testing/Temporary/dec039-compat-targeted-debug.log`和
  `out/build/windows-msvc-protocol-lab/Testing/Temporary/dec039-compat-targeted-release.log`；
- `out/build/windows-msvc-protocol-lab/Testing/Temporary/dec039-compat-lab-debug.log`；
- `out/build/windows-msvc-protocol-lab/Testing/Temporary/dec039-compat-lab-release.log`；
- `out/build/windows-msvc-all-slices-dec039/Testing/Temporary/dec039-compat-all-debug.log`；
- `out/build/windows-msvc-all-slices-dec039/Testing/Temporary/dec039-compat-all-release.log`。

因测试夹具复制修改了测试CMake，另行复核Lab-on/Testing-off和Product-only Release构建，均成功且
CTest为`0`项；未修改Adapter接口或目标依赖。

## 6.4 2026-09-06 JSONL检出一致性修复

基线`a2344db`在独立本地Clone、`core.autocrlf=true`下，旧版Event由978字节LF变成979字节
CRLF，SHA-256由`3e8097698e579119fc83578c610d008858501e5f281b07392ab72c2b71f1fb99`
变成`deb3f9cf8b6aee89d0b12eb7d421cfc6fa22ab434d345cf75a0edda42cd5f850`。
保留静态Record时Compare退出3并报告Payload不匹配；按旧测试逻辑重算Hash后回放仍通过。
因此这是静态夹具字节漂移被测试物化流程掩盖，不是已证实的生产Codec回归。

本次最小修复：

- `.gitattributes`增加`*.jsonl text eol=lf`；
- 夹具物化后、任何Payload Hash刷新前，通过`LoadStoredRun`核对完整历史Record；
- 增加CRLF转换和等长Event内容篡改负向测试；
- 测试程序增加`--legacy-offline-only`入口，可不执行UDP收发而检查旧版兼容性。

实际验证：主工作区Debug/Release测试目标构建成功，两个配置的
`pae_protocol_lab_udp_exchange_tests.exe --legacy-offline-only`均退出0；该命令从各构建树的
`tests/protocol_lab`工作目录执行，覆盖加载、Compare、Replay及新增负向用例。
`ctest --preset windows-msvc-protocol-lab-debug -E udp_exchange --output-on-failure`
和对应Release命令均为`3/3 PASS`。`clang-format --dry-run --Werror`及`git diff --check`通过。

独立目录以`a2344db`本地Clone为基线，仅覆盖未提交的属性规则及测试源码；保留原CRLF文件
于`out/checkout-validation`，然后在`core.autocrlf=true`下通过`git checkout-index --force`
重新写出Event。实测为`i/lf w/lf attr/text eol=lf`，978字节且恢复原SHA-256。
重新配置、构建独立Release测试目标，并执行`--legacy-offline-only`，退出0。
这验证的是未提交候选补丁，不是新提交或GitHub下载验收；未暂存、提交或推送。
独立目录位于系统临时目录，MSBuild报告MSB8029增量构建注意警告，但构建成功。

本次未修改生产源码、CMake、机器格式或静态协议数据；未执行任何UDP收发、全切片回归、
人工Lab门禁或Linux验证，既有Golden与Lab门禁结论不升级。

## 7. 当前未验证与保留边界

- 未使用外部网络调试工具执行人工Loopback收发、人工逐字节核对和Replay门禁；
- 未执行任何非Loopback主动发送；测试中的`192.0.2.0/24`文档地址只进入安全拒绝或注入Adapter；
- 未实现Linux POSIX Socket Adapter，也未执行Linux GCC/Clang；
- 未验证真实设备、独立仿真端、生产Endpoint、硬件或现场环境；
- 未测试持续监听、自动重试、多客户端、并发压力、长时间稳定性或正式性能；
- 未模拟断电、进程强杀、文件系统缓存持久化或真实磁盘故障；
- 当前公开Synthetic请求/响应只能支持Engine PoC（引擎概念验证），不能升级
  `PROTOCOL_GOLDEN_PASS`；
- 当前变更尚未Stage、Commit或Push。
