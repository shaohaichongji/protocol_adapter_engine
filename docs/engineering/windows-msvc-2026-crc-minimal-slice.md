# 参数化CRC最小检查点Windows验证报告

日期：2026-09-09。代码基线：`c905e33`。状态：`IMPLEMENTED / REFERENCE VECTORS PASSED / P2 REVIEW CLOSED / PRE-STAGING CHECK COMPLETE`。

本报告只覆盖公开合成COMPLETE_RECORD上的CRC-16/32实现、离线Lab证据链及Windows自动化；
不构成真实协议Golden、Linux、硬件、现场、性能或生产可用结论。

## 1. 实现范围

- Schema 0.6严格区分无校验、SUM8与CRC；CRC参数全部显式，固定16/32位，2/4字节存储序独立。
- Loader、Domain Validator、ResourceBudget、Budgeted Draft、Builder和Frozen Plan贯通CRC描述符；
  边界、自包含和字段/位容器/Matcher所有权按完整存储区复核。
- Core实现冻结的直接左移语义，无动态表和逐帧分配；Decode失败不交付，Encode最终复核失败
  有效长度为0，覆盖字节操作计数为N/2N。
- Lab为Schema 0.6选择Result 0.7、指纹0.7和Record 0.8；Event/CLI保持0.7/0.1。
  成功、完整性失败及链式Replay可读；跨Schema 0.5/0.6 Run Compare失败关闭。
- CMake能力默认关闭，CRC编译与Lab各有显式开关及依赖门禁。

## 2. 自动化映射

| 契约范围 | 当前证据 |
| --- | --- |
| 标准算法值 | IBM-3740 `29B1`、MODBUS `4B37`、ISO-HDLC `CBF43926`、MPEG-2 `0376E6E7`逐项独立断言 |
| 非对称反射 | false/true=`8D48`、true/false=`FB64`；原临时来源无日志，后续锁定pycrc三算法已独立确认，见第7节 |
| 存储与组包 | 修复轮使用锁定pycrc结果推导的固定完整帧，覆盖六组16/32位参数在大端、小端下的Encode与Decode；逐组检查覆盖区及每个CRC存储字节损坏均失败且字段数0，随后合法帧恢复成功 |
| 短范围与非覆盖区 | 单字节ASCII `1`=`C782`已由锁定pycrc确认；既有范围外动态业务字节变化的Decode断言通过 |
| 解码失败与恢复 | 覆盖区及CRC存储区单比特损坏均为`INTEGRITY_FAILED`且字段数0；成功→失败→成功无残留 |
| 编码失败 | 内部最终复核注入为`FINAL_REVIEW_FAILED`且有效输出长度0 |
| 严格配置 | 旧Schema、宽度、大小写/长度、零/偶数多项式、布尔类型、缺失/未知属性、存储序和自包含负例 |
| Plan与资源 | Builder损坏Draft四类拒绝；完整性规则计数、Plan通用精确计费/复算/Arena报告和预算边界测试通过 |
| 热路径 | CRC首次Encode/Decode零可替换`new`分配；操作计数Decode N、Encode 2N |
| Lab新代 | CRC成功Run→Replay A→Replay B；失败Run→Replay且`EQUAL`仍退出5；Schema 0.6无校验/SUM8均为Result 0.7 |
| 历史隔离 | Result 0.6/Record 0.7专项读取继续通过；旧Run自Compare为`EQUAL`；0.5/0.6 Run直接Compare退出3 |

测试使用的公开样例均标记`SYNTHETIC_FROM_SCRATCH`，不包含真实端点、客户、设备、抓包或私有协议。

## 3. Windows命令与结果

全功能配置在`out/build/windows-msvc-protocol-lab`中显式开启Schema 0.5 C3与Schema 0.6 CRC能力，
并开启v06/v07隔离格式测试。Debug、Release先构建后串行执行：

```powershell
cmake --build --preset windows-msvc-protocol-lab-debug -- /m:1
ctest --preset windows-msvc-protocol-lab-debug -LE "udp|loopback" --output-on-failure
cmake --build --preset windows-msvc-protocol-lab-release -- /m:1
ctest --preset windows-msvc-protocol-lab-release -LE "udp|loopback" --output-on-failure
```

结果：Debug、Release离线矩阵各`40/40 PASS`；其中CRC标签各4项通过。日志：

- `out/crc-validation/protocol-lab-debug-offline-ctest.log`
- `out/crc-validation/protocol-lab-release-offline-ctest.log`

隔离构建使用`out/build/windows-msvc-crc-product-only`和
`out/build/windows-msvc-crc-lab-no-tests`。两者Debug/Release均构建成功，CTest列举均为0；
前者默认关闭CRC且只生成Plan/Core产品目标，后者显式开启CRC C3链但不生成测试目标。日志位于
`out/crc-validation/{product-only,lab-no-tests}-*.log`。

三项错误开关组合（CRC Compiler缺少0.5依赖、Lab构建缺少CRC Lab开关、CRC Lab缺少CRC
Compiler）均在Configure阶段按预期非零退出，日志为`out/crc-validation/gate-*.log`。

## 4. 修复过程证据

恢复后首次Debug全矩阵为`39/41 PASS`：两个既有Core能力门禁测试仍固定断言Schema 0.6不受支持。
按CRC编译开关条件化期望后，针对性2/2及完整离线40/40通过。另一个首次新增用例失败来自
Values 0.1夹具把UINT64写成JSON Number；按既有字符串契约修正后通过，未放宽Reader。

恢复过程中误执行以下两条完整CTest命令，Debug和Release各运行一次既有Loopback UDP测试，
不是仅一次。执行者回报两次均PASS、无非Loopback；均超出纯离线授权，不因通过追认为合规：

```text
ctest --preset windows-msvc-protocol-lab-debug --output-on-failure
ctest --preset windows-msvc-protocol-lab-release --output-on-failure
```

上述命令没有独立持久化完整日志，命令和结果依据执行者回报及任务工具输出；Debug产物被随后
Release同测试覆盖，精确时间无法从磁盘恢复。执行者回报Release产物时间约为2026-09-09
10:06:33.925至10:06:35.422（Asia/Shanghai），不将其扩大为Debug执行时间证据。
现存CTestTestfile.cmake确实注册udp_exchange并标记udp/loopback。最终两份离线日志各40/40，
无udp_exchange，文件时间分别10:19:18、10:19:28；它们只证明最终离线回归，不消除先前越界。

## 5. 初次实施证据限制及当前边界

- 初次实施时标准检查值来自公开目录，没有本地锁定版本参考工具的运行记录。自定义值来自按产品
  同一左移/反射语义编写的临时PowerShell第二实现，不能自动等同已约定的外部独立参考核算。
- 8D48、FB64对应脚本未保存，精确命令和日志未持久化；C782也未保存脚本、运行日志和工具身份。
  执行者回报短范围首次管道计算得到E1F0，随后显式循环改得C782；这里只记录自述，不伪造旧日志。
- 实现期间未报告工具缺失并未经确认弱化契约，现已恢复契约要求；当时独立参考核算未完成，阻断提交。
  后续独立证据已补齐（第7节），不修改这些历史事实；当前提交阻断来自第8节审查项。
- 未执行Linux、真实协议、设备、硬件、现场、人工Lab或性能测试；CRC不提供认证或防篡改保证。
- 当前源码、测试和文档均未Stage、Commit或Push；是否提交由总控后续决定。

## 6. 本地参考工具只读核验（2026-09-09）

本次只读取命令解析结果及文件清单，没有启动参考算法、导入Python包或执行包安装。
PATH未解析到pycrc、reveng或crcany。限定检查Python工具环境（含现有虚拟环境和缓存）、
PAE仓库（含忽略生成物）及用户Downloads目录，未命中pycrc/reveng/crcany/crccheck候选文件；
Python环境进一步按包路径检查pycrc/reveng/crccheck/crcmod/crcany，也没有命中。
已发现Python解释器不等于已发现CRC参考工具。以上是限定路径搜索，不是全盘不存在性证明。

该次只读核验结束时需要单独批准可信工具获取方案；该次没有下载、联网、运行核算、改源码、
重跑测试或执行Git写操作。后续已分别获得获取及隔离运行授权，见下一节。

## 7. 锁定外部参考核算（2026-09-09）

固定工具为pycrc 0.11.0，来源为官方PyPI固定版本页及其files.pythonhosted.org分发文件；
依赖固定importlib-metadata 8.7.0、zipp 3.23.0。三个wheel均校验事前锁定SHA-256及字节长度，
分别保留MIT、Apache-2.0、MIT许可证。工具及Python环境位于仓库外，不进入产品依赖链。
Python实际版本3.13.14，独立venv禁用system-site-packages；安装使用本地wheel和
`--no-index --no-deps --require-hashes --only-binary=:all:`，pip check通过。

| 核算组 | 输入HEX | 结果 |
| --- | --- | --- |
| IBM-3740 / MODBUS | 313233343536373839 | 29B1 / 4B37 |
| ISO-HDLC / MPEG-2 | 313233343536373839 | CBF43926 / 0376E6E7 |
| 16位poly1021/init1D0F/xoroutBEEF，refin/refout=false/true、true/false | 313233343536373839 | 8D48 / FB64 |
| IBM-3740短范围 | 31 | C782 |
| ISO-HDLC / MPEG-2二进制载荷 | 00017F80A5FF1020FE | 9B56EEF4 / 3548EB9E |
| 上述两种非对称反射二进制载荷 | 00017F80A5FF1020FE | B3ED / 675C |

共11组，各使用bbb/bbf/tbl三算法，33次调用均退出0且结果一致；前七项与核算前记录的预期相符。
驱动无CRC算法实现，不运行PAE。新增四项为外部参考输出，尚未接入PAE测试。
三算法属于同一个上游项目，不代表三个独立工具或完整参数域证明。

本地证据目录：`out/crc-reference/run-20260909-pycrc0110/`。

- `results.json`保存33次argv、输入、参数、stdout/stderr、退出码、工具/解释器/脚本Hash和包清单。
- `provision.log`保存离线安装及依赖检查；`tool.lock.json`和`requirements.lock.txt`锁定来源。
- 运行脚本快照及`SHA256SUMS`保留；7/7证据Hash再次核对通过。
- `results.json` SHA-256：`0f3b32a57dfc8636e1d404d6f3b427220ed2e3634521bbecd6f62c60f9a0e6d3`。
- 大小端完整合成帧由参考CRC数值及指定布局推导，不冒充PAE Encode/Decode运行证据。

该次未改源码、未运行PAE构建/CTest/网络测试、未执行Git写操作。旧40/40保留为历史回归，
未因本次参考核算重新运行。不关闭Decimal高精度Oracle、Golden、Linux或现场门禁。

## 8. 总控集中审查（2026-09-09）

审查覆盖CRC参数解析/冻结、范围及所有权、Core位运算与存储、能力开关、Result/Record/指纹
分派、Replay/Compare关联及相关专项测试。未自动修复源码。

### P2-1：历史Result版本没有与父Record绑定（实际复现）

`v07_run_evidence.cpp`读取Replay历史快照时验证父Record与子Record同代、历史Result指纹、
文件长度/Hash和执行语义，但未要求历史Result自身格式与父Record代次一致。
因此Record 0.8下可混入Result 0.6：同步其旧域指纹、父Record摘要、历史摘要、载荷长度/Hash、
清单及DIFFERENT比较事实后，Reader仍接受，并使独立Compare输出成功EQUAL。
这不是要求Hash发现全面自洽重写，而是缺少契约规定的跨版本组合拒绝。

以现有成功Replay A合成Bundle的独立副本核查；未修改原Bundle。独立指纹编码先与原Result
指纹吻合，再只切换历史Result格式并更新关联。现有Debug/Release二进制各执行合法对照和
混代副本自Compare，两者均退出0/EQUAL；混代应退出3。未重建，结论限定为现有二进制。
命令、二进制Hash、stdout/stderr/退出码及复现脚本位于
`out/review/crc-history-generation/{results.json,check.py}`，未执行网络或Codec重算。

建议在历史快照关联层同时绑定Record、Result和指纹域；补新旧两个方向的自洽混代负例及
合法历史/链式Replay回归，不放宽旧格式接受域。

### P2-2：CRC-32小端产品路径缺少契约测试（静态覆盖缺口）

`crc_contract_tests.cpp`六个Vector均保留base配置的大端存储；独立little_config来自16位base，
只检查两字节Encode。现有新增Lab和首次分配用例也未补CRC-32小端固定帧Encode/Decode断言。
这是测试覆盖缺口，不是已经复现的32位算法/存储产品错误。
建议参数化16/32位与两种存储序，使用独立固定完整帧同时检查Encode和Decode；至少补32位
小端的覆盖区/四字节CRC存储区损坏及失败不交付。参考报告的大小端布局推导不能替代产品测试。

本轮仅修改相关Markdown并执行上述四次离线Compare定向诊断；未改产品/测试源码、未运行
全量CTest、未Stage/Commit/Push。两项待限定修复/补测，当前不进入提交。

## 9. 两项P2限定修复与断电恢复验证（2026-09-09）

### 9.1 独立核实与修复

- P2-1实际成立。修复前证据保留于`out/review/crc-history-generation/`：Debug、Release对
  Record 0.8父快照混入Result 0.6的完全重哈希副本均返回0/EQUAL。Reader现在先按父Record
  选择Result代次及指纹域，再计算历史指纹；Record 0.7/Result 0.7和Record 0.8/Result 0.6
  两个方向均以固定语义错误拒绝，不依赖文件长度或Hash失败。
- CLI回归先生成合法旧代和新代Run→Replay，确认可读后只修改隔离副本的历史Result代次，
  同步父/子Record指纹、比较事实、文件长度、SHA-256和清单。两个副本自Compare均精确返回
  退出3、`PAE_LAB_C3_COMPARE_EVIDENCE_INVALID`，detail为
  `Replay historical Result generation does not match the parent Run Record`；结果和Bundle为空。
- P2-2是测试缺口，未发现Core算法错误。六组固定完整帧现在逐组覆盖16/32位、大/小端的
  Encode和Decode；每组覆盖字节损坏、每个CRC存储字节损坏均为`INTEGRITY_FAILED`且字段数0，
  随后的合法Decode成功。CRC-32小端四个存储字节因此均有独立故障断言。

### 9.2 本轮实际验证

关机恢复后确认无残留CMake、CTest、MSBuild或Lab进程，Git仍为`main@c905e33`且暂存区为空；
已落盘但未编译的编辑从该检查点继续。Debug、Release严格串行，所有CTest均使用
`-LE "udp|loopback|network"`，本轮未执行任何网络测试。

| 验证 | Debug | Release |
| --- | --- | --- |
| 三项针对性测试：CRC Core、v07 Run Evidence、CRC CLI | 3/3 | 3/3 |
| CRC全功能Protocol Lab离线矩阵 | 40/40 | 40/40 |
| 默认全切片离线共存矩阵 | 25/25 | 25/25 |
| Lab-on/Testing-off构建 | 成功，CTest 0 | 成功，CTest 0 |

本轮日志位于`out/crc-validation/review-fix-20260909/`；CLI精确输出位于
`out/build/windows-msvc-protocol-lab/tests/protocol_lab_c3_cli/crc-runs/`。Product-only未重跑，
因为本轮没有修改Plan/Core产品实现、公开接口或产品目标依赖；测试文件的Core增量只扩展断言。
锁定pycrc不重跑、不下载、不改变；第7节证据继续作为固定期望的来源。

相关C++文件经VS附带clang-format 18 dry-run检查，`git diff --check`通过。未执行Linux、真实
协议Golden、硬件、现场、人工Lab、性能或UDP验证；两项P2已形成修复和自动化证据，但整体
CRC检查点仍须总控独立复核，未Stage、Commit或Push。

## 10. 总控复核与暂存前集中收口（2026-09-09）

总控读取修复源码、双向自洽篡改辅助、固定参考完整帧和最终CLI诊断，确认第8节两项P2已关闭。
历史格式检查先于历史指纹关联及比较；双向负例在同步清单/长度/Hash后精确返回退出3，
不是低层Hash错误冒充语义拒绝。CRC固定帧与第7节独立参考结果吻合。

最终CMake条件接线调整后，执行任务再次运行CRC全功能离线矩阵；总控已复核更新后的
`protocol-lab-debug-offline-ctest.log`及`protocol-lab-release-offline-ctest.log`，均40/40，
文件时间分别为11:26:11、11:26:21。默认25/25及Testing-off仍按第9节各自执行批次记录，
不将它们冒充为最后调整之后的重跑。总控本次不再重复构建或CTest。

- 候选共47个：38个已修改、9个未跟踪；删除项0，暂存区为空。
- 7个Markdown候选的78处本地链接目标均存在；未检查外部网址可用性或Markdown锚点。
- 全部候选文本敏感模式及路径检查未发现凭据、机器绝对路径或私有协议；命中项为既有
  Loopback/通配地址示例、公开来源网址及文本误报。此检查不等于完整秘密识别保证。
- `git diff --check`和候选尾随空白检查通过。
- 建议A组40个实现/Schema/样例/测试文件，B组7个Markdown契约/语义/报告文件；均未暂存。
- 外部参考工具、虚拟环境、仓库外总基线及全部out证据不进入本仓库提交。

本次仅修订状态文字并准备暂存前草案，不改变产品代码，不自动Stage、Commit或Push。
