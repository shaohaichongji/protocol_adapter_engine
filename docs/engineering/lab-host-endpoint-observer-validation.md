# Lab 宿主端点观察：实施、验证与人工验收入口

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

日期：2026-09-13。基线：`main@e7078b29dd50548c84f3161ab45a1dc02e4c6f60`。
授权范围：[六项契约](lab-host-endpoint-observer-contract.md)；先PAE验证、再Lab串行接线。
实现、自动验证及用户人工验收均已完成。用户确认Lab已关闭，并授权核对、提交与推送。
本文为提交前收口记录；实际Commit/Push状态以Git提交和远端引用为准。

## 1. 实现范围

- PAE `CandidateObserver` 可选同步回调：每个进入Core的候选只Decode一次，借用原帧、
  DecodeResult和成功字段；观察先于业务Sink，正常STOP不取消当前成功业务交付。
  失败候选不交付业务，观察异常保留已消费量并要求Reset；不提供观察器时维持旧语义。
- 新增默认OFF的 `PAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER`，必须显式启用Host endpoint与
  ASCII stream observer及其依赖。普通CLI/Evidence及Binary路径未接入该功能。
- 非Qt `HostObserverAdapter` 独占Session/Plan，自有描述与候选副本，Decode绑定两流、
  Encode绑定一通道。每次Submit/Continue按候选STOP；失败原帧只作诊断。
  分开累计候选、Core成功、观察正常返回及业务正常返回数。
- Tab内显式绑定草稿与Apply，后台重编译已加载的同一份配置文本准备独立Plan；
  准备失败或取消发布不改变旧Session。新Session准备完成后才确认丢弃并替换。
  初始未Apply时明确显示旧离线模式，草稿不代表活动绑定；不写回JSON。
- 活动绑定固定Action/Pipeline；绑定/Flow选择只切视图，保留各流草稿、冻结后缀、
  计数和当前结果。Encode草稿与接收流隔离；重绑/Reload/关闭检查所有受影响流。
- Lab headless测试显式取消`NDEBUG`，使Release中的既有assert及其准备动作真实执行。

主要文件：`src/host_endpoint/host_endpoint.*`，`tools/protocol_lab_ascii/host_observer_adapter.*`，
`tools/protocol_lab_ui/document_session.*`、`document_tab.*`及对应测试/CMake；
Core与Framer协议实现未修改。

## 2. 资源与证据边界

PAE原64绑定、64通道、256字节身份、64MiB Session计费上限不放大。
Lab额外收紧为：描述计费输入最多4MiB、单记录最多65536字节、全配置字段最多1024个、
字段最大长度之和最多1MiB；每个候选/活动adapter准入预算128MiB，替换时两份共256MiB。
计入Plan报告、Session报告、描述及映射副本储备、每流当前结果/字段/文本/冻结输入储备；
初次从旧离线adapter转换亦先计算其Plan、Core、Framer和副本储备并检查准入。
这些是显式对象/副本准入计费与保守储备，**不是操作系统RSS上限或Qt分配器精确计量**；
编译器临时内存仍由既有Compiler资源策略约束。

候选与字段在回调借用期内复制；不存历史队列，不用第二次Decode构造显示结果。
执行DTO携带Tab/加载/Session修订、binding/flow、generation及操作序号；流视图只保存当前结果。
观察是串行诊断能力，不承诺并发监控、回调内销毁或异常宿主副作用回滚。

## 3. 自动验证

PAE先在`out/build/windows-msvc-host-endpoint`通过Debug/Release各2/2宿主定向测试，
新增测试在接口实施前首先因缺失Candidate API编译失败，随后通过；这是编译缺口证据，非运行时失败断言。
Lab非Qt adapter随后分别通过Debug/Release定向测试，才进入Qt接线。

集成目录：`out/build/windows-msvc-lab-host-observer`。
全链使用Qt 5.13.0 x64、MSVC 19.29.30159 / v142 14.29.30133、Windows SDK 10.0.22621.0。
不链接MSVC 19.51阶段的PAE产物。

| 检查 | 结果 |
|---|---|
| Host ON，全配置Debug | 52/52通过 |
| Host ON，全配置Release（Lab断言开启） | 52/52通过 |
| Host OFF，旧ASCII/Binary Lab Release定向回归 | 18/18通过 |
| 默认配置 | Host observer为OFF |
| 只开Host observer、缺前置依赖 | CMake按预期拒绝 |

新增与受影响测试覆盖：默认业务语义、坏/好候选逐步停止、观察异常消费、零字段成功、
自有原帧/字段、两流及两Tab隔离、低工作预算的精确后缀、CR/LF分片、超长丢弃恢复、
一侧动作拒绝、跨Pipeline Message拒绝、Schema 0.10完整记录与0.11流/完整记录路径。
真实Qt控件冒烟包含Apply、重复绑定失败、取消重绑、切流恢复、失败候选、Reset隔离，
以及完整记录RX与独立TX字节期望；不是用户人工验收。

本地日志（不提交生成目录）：

- `out/lab-host-ui-debug-build.log`、`out/lab-host-full-debug-tests.log`
- `out/lab-host-ui-release-build.log`、`out/lab-host-full-release-tests.log`
- `out/lab-host-off-compat-release-build.log`、`out/lab-host-off-compat-release-tests.log`
- `out/lab-host-default-off.log`、`out/lab-host-missing-dependencies.log`

复验命令（仓库根目录，使用Visual Studio配套CMake/CTest）：

```powershell
cmake --build out/build/windows-msvc-lab-host-observer --config Debug --parallel 6
ctest --test-dir out/build/windows-msvc-lab-host-observer -C Debug --output-on-failure
cmake --build out/build/windows-msvc-lab-host-observer --config Release --parallel 6
ctest --test-dir out/build/windows-msvc-lab-host-observer -C Release --output-on-failure
```

未验证：Linux、网络/串口、性能、真实设备、Golden、部署及现场。
未扩展：UTF-8、Binary流式UI、自动转发、跨协议映射、绑定持久化或稳定公共ABI。

## 4. 人工验收步骤（已执行，保留复验入口）

启动完整程序路径：

```text
<REPO_ROOT>\out\build\windows-msvc-lab-host-observer\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe
```

用File → Open打开完整JSON路径：

```text
<REPO_ROOT>\examples\config\synthetic_ascii_stream_slice.pae.json
```

1. 保持两条默认草稿：`device / Decode / ascii_pipeline`、
   `device / Encode / ascii_pipeline`。点击Apply binding table，状态应显示Host Session active；
   选择第一个活动绑定，Flow 0，Representation选ASCII (escaped)。
2. 输入`RX A!`并Submit chunk，buffered=5；切Flow 1先确认ASCII表示，再输入`ON`并Submit，buffered=2。
   切Encode绑定应不清半帧，再切Decode/Flow 0应恢复`RX A!`和buffered=5。
3. Flow 0替换为`OK\r\nBAD\r\nONLY\r\n`并Submit：只显示greeting成功，name=A；
   第一次Continue只显示BAD失败且无旧成功字段；第二次Continue显示decode_only成功、0字段。
   累计candidates=3、decode_ok=2、observed=3、business=2，冻结后缀耗尽。
4. Reset Flow 0：该流generation递增、计数归零；切Flow 1仍buffered=2。
   输入`LY\r\n`并Submit，应得到ONLY的零字段成功。
5. 在任意一条流保留半包，切到Encode绑定。将草稿第二行Action改成Decode后Apply：
   重复绑定应失败，旧Session修订和双流不变。改回Encode再Apply，在丢弃确认中选No：
   所有流及选择不变；再次Apply并选Yes：新Session修订增加、两条接收流清空。
6. 重复制造Flow 1半包，再切Flow 0；Reload、关闭Tab各选择No，检查未选流状态仍在。
   暂留该Tab及Flow 1半包，完成第二Tab操作后再验证跨Tab隔离并关闭。

完整记录接线另开Tab，JSON完整路径：

```text
<REPO_ROOT>\examples\config\synthetic_ascii_text_slice.pae.json
```

7. Apply默认两条绑定。Decode活动绑定、ASCII (escaped)输入`RX ALICE!OK\r\n`，
   Inspect应输出name=ALICE、rx_code=OK，原帧为`52 58 20 41 4C 49 43 45 21 4F 4B 0D 0A`。
   切Encode绑定，ASCII表示输入name=`ALICE`、tx_tag=`Z`，Encode应为
   `54 58 20 41 4C 49 43 45 21 5A 0D 0A`（TX ALICE!Z CRLF），不使用RX模板回环自证。

8. 返回流式Tab，确认Flow 1仍buffered=2；输入`LY\r\n`补齐为`ONLY\r\n`，
   零字段成功后关闭Lab。

## 5. 人工验收结论与交付授权

用户在本任务中确认“剩余人工验收全部符合预期，Lab已关闭”，随后授权自行核对、提交与推送。
截图直接支持候选好/坏/零字段逐步观察、Reset清空、重绑确认、Session修订2到3、
完整记录RX ALICE!OK与独立TX ALICE!Z，以及跨Tab的ON半包保留和ONLY补齐。
重复绑定拒绝、取消后的完整状态、未选流关闭保护等未被单张截图完整覆盖的操作过程，
以用户完整人工执行确认作为依据，不把静态截图等同于全程自动录制。

验收中Flow 1最初在Hex表示下输入ON，输入早拒绝且buffered=0；切换表示会转换已有草稿，
非法Hex导致切换被拒绝并回退。清空草稿后选择ASCII，再执行ON/LY分片及Reset隔离均符合预期。
本次没有因此修改协议或切换语义；保留“转换失败提示不够直观”为后续易用性优化项。

本次人工通过仅覆盖上述Windows离线Host/Lab检查点；不扩大硬件、网络或现场结论。
