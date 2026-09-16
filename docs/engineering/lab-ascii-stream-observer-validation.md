# Lab ASCII Stream Observer 验证记录

日期：2026-09-13

## 结论与边界

本次按 `lab-ascii-stream-observer-contract.md` 实现了显式默认关闭的 Schema 0.11
`stream_chunk + ascii_crlf` Lab 观察切片。实现只面向离线 adapter 与 Qt Widgets Lab：不接入普通
Protocol Lab CLI，不写 Evidence，不引入网络、串口、设备或线程生命周期。

已自动验证 Windows x64 Debug / Release 构建、adapter/session 定向测试和真实 Qt Widgets smoke；
用户已于2026-09-13确认全部人工验收符合预期并关闭Lab，具体证据范围见文末记录。
本文不声明 Golden、硬件或现场验证。当前检查点未暂存、Commit或Push。

## 实现摘要

- adapter 为每个 ASCII CRLF stream Pipeline 独占 Framer workspace、冻结输入与游标；Submit 和
  Continue 每步最多调用一次 `PushStreamChunk`，sink 收到首个候选后总是 `STOP`。
- Continue 只提交冻结后缀；冻结输入耗尽但 Framer 仍有内部工作时允许空输入 Continue。非法草稿、
  空输入和容量拒绝发生在 Push 前，不修改半帧、丢弃态、冻结后缀或累计计数。
- 每步结果拥有候选字节、Decode 结果及字段范围；候选复制失败保留实际 Framing step facts，并进入
  `reset_required`。累计候选数与 Decode 成功数分离，Decode 失败仍计为已交付候选。
- UI 新增 Stream Inspect、Submit chunk、Continue、Reset stream，展示 phase、buffered、内部工作、
  有效 submit/work 限制、冻结游标、generation/step、候选/Decode 成功、discard/malformed 和当前步事实。
- 模式、Pipeline、配置重载、Tab 关闭和主窗口关闭在存在可丢弃流状态时要求确认；表述切换保持
  engine/frozen state，只清除当前展示结果。两个文档各自拥有独立 adapter 与 stream workspace。
- Schema 0.11 中 `complete_record` Pipeline 继续使用原 Encode/Inspect 路径；Schema 0.10 与旧 Binary
  UI 回归测试保持覆盖。

## 容量公式

设 Framer `Observe()` 返回的有效提交上限为 `E`：

- Lab chunk 字节容量：`C = min(65536, E)`；不使用 `maximum_frame_length` 代替 `C`。
- ASCII escaped 编辑容量：`4 * C + 4` 个 UTF-16 code units，计算前做溢出与 `int` 上限检查。
- Hex 编辑容量：`3 * C + 1` 个 UTF-16 code units，覆盖每字节两位 Hex、允许的分隔空白及整次拒绝
  headroom，计算前同样做溢出与 `int` 上限检查。

## Windows 配置与构建

独立构建目录：`out/build/windows-msvc-lab-ascii-stream-ui`。

```powershell
cmake -S . -B out/build/windows-msvc-lab-ascii-stream-ui `
  -G "Visual Studio 18 2026" -A x64 -T v142,version=14.29.30133 `
  -DPAE_QT_ROOT="F:/PersonalWorkspace/DEI/third_party/windows/qt" `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_PROTOCOL_LAB_UI=ON `
  -DPAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=ON `
  -DPAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER=ON `
  -DPAE_BUILD_STREAM_FRAMING_SLICE=ON `
  -DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON `
  -DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON `
  -DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON `
  -DPAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING=ON

cmake --build out/build/windows-msvc-lab-ascii-stream-ui --config Debug --target ALL_BUILD -- /m:1
cmake --build out/build/windows-msvc-lab-ascii-stream-ui --config Release --target ALL_BUILD -- /m:1
```

配置输出确认：Qt 5.13.0；请求的 v142 directory version 14.29.30133；`cl.exe`
19.29.30159.0；x64。

## 自动化结果与日志

Debug / Release 均执行：

```powershell
ctest --test-dir out/build/windows-msvc-lab-ascii-stream-ui -C <Debug|Release> `
  --output-on-failure `
  -R '^pae\.tools\.(protocol_lab_ascii\.adapter|protocol_lab_ui\.)'
```

- Debug：17/17 PASS。
- Release：17/17 PASS。
- 日志：`out/lab-ascii-stream-observer-debug.log`、
  `out/lab-ascii-stream-observer-release.log`。

定向覆盖包括：同块/跨块 CRLF、首个候选 STOP 与冻结后缀、低 work budget 的游标推进、半帧后新块、
超长丢弃、Decode 失败、候选复制失败、Reset、非法草稿状态保持、表述切换、双 session 隔离、
Schema 0.11 complete-record 路径，以及旧 Schema 0.10/Binary UI smoke。

同一构建树随后执行未筛选的完整 `ctest --output-on-failure`：Debug 47/47 PASS，Release
47/47 PASS；这属于该配置下的自动化回归，不扩大为硬件或人工验收。

开关与互斥门验证：

- 默认配置 cache 中 `PAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER:BOOL=OFF`，默认 `ALL_BUILD`
  成功。
- 仅打开 observer：配置按预期失败，要求显式 V11、ASCII adapter、Framing slice。
- V11 + 普通 Protocol Lab CLI：配置按预期失败，CLI/Evidence 仍不支持 Schema 0.11。
- V11 + ASCII adapter 但 observer 关闭：配置按预期失败，避免宏与能力不一致。
- 另在 `out/build/lab-ascii-stream-observer-no-tests` 以 `BUILD_TESTING=OFF`、
  `PAE_BUILD_TESTING=OFF` 配置并成功构建 Release UI，确认候选复制失败测试钩子未成为非测试依赖。

## 可执行程序与配置

- Debug：`out/build/windows-msvc-lab-ascii-stream-ui/out/protocol_lab_ui/Debug/pae_protocol_lab_ui.exe`
- Release：`out/build/windows-msvc-lab-ascii-stream-ui/out/protocol_lab_ui/Release/pae_protocol_lab_ui.exe`
- 公共合成配置：`examples/config/synthetic_ascii_stream_slice.pae.json`

本次最终文件 SHA-256：Debug EXE
`E9FFCCE1155144ED3BB75A985C3367DFEFADC426FC8449B561BF1F237F6C7629`；Release EXE
`59C113DCA2D737CD2F33DB837DAB04A0AC22A2598941D6EC25E506A13C550D1C`；JSON
`5F6B871DFBFC099070EE8FB9881F02C8671496C61BFC028CC24DB332E7E8BE4C`。

## 集中复核修复（2026-09-13）

总控复核后追加三项限定修复：

- 已实际 Push 的草稿绑定 `inspect_input_revision`；按钮再次点击不会制造新修订，未编辑重复 Submit
  返回 `UI_STREAM_CHUNK_ALREADY_SUBMITTED`，不会再次 Push。半包允许编辑并提交下一新 chunk；
  Continue、候选 Decode 失败和 API/复制失败后的已消费 chunk 同样不会被整块重放。ASCII/Hex 字节等价
  表述切换会同步已提交标记，容量整次拒绝也不会意外解锁旧草稿。
- 主窗口关闭改为两阶段：先对所有需要确认的 Tab 逐一只读询问，全部 Yes 后才统一关闭；任一 No
  不 Reset、不清草稿/结果、不关闭窗口。单 Tab、Reload、Pipeline 和离开 Stream Inspect 的确认语义
  保持不变。
- Stream Submit 的非法 ASCII escaped 输入复用完整 Inspect 的诊断映射，保留零基 UTF-16
  code-unit offset；例如 `RX A!OK\q` 精确报告 offset 7，且半包与 step 序号不变、不调用 Push。

只重建受影响目标并运行以下 Debug / Release 定向回归，未重复无变化完整矩阵：

```powershell
cmake --build out/build/windows-msvc-lab-ascii-stream-ui --config <Debug|Release> `
  --target pae_protocol_lab_ui pae_protocol_lab_ui_ascii_stream_session_tests -- /m:1

ctest --test-dir out/build/windows-msvc-lab-ascii-stream-ui -C <Debug|Release> `
  --output-on-failure `
  -R '^pae\.tools\.protocol_lab_ui\.(ascii_stream_session|qt_smoke_ascii_stream|qt_smoke_ascii_stream_close_cancel)$'
```

Debug 3/3 PASS，Release 3/3 PASS。其中 `qt_smoke_ascii_stream_close_cancel` 使用两个真实 Qt
DocumentTab 和两个真实 `QMessageBox`，依次回答 Yes、No，并逐项比较两个 Tab 的草稿、phase、buffered、
冻结游标、generation、step、累计计数与当前 step 是否完全不变。

追加日志：`out/lab-ascii-stream-observer-fix-debug.log`、
`out/lab-ascii-stream-observer-fix-release.log`。`git diff --check` 通过。以上仍是自动化 UI
回归，不替代用户人工可见窗口验收。

## 人工可见窗口检查步骤（用户已确认通过）

1. 启动 Release EXE，加载上述公共合成 JSON；选择 `ascii_pipeline`、`Stream Inspect`、
   `ASCII (escaped)`。
2. Submit `RX A!`：确认本步无候选、`buffered=5`，且可继续提交新块。
3. 不编辑草稿再次点击 Submit：确认提示 `UI_STREAM_CHUNK_ALREADY_SUBMITTED`，step 与 `buffered=5`
   均不变；编辑为 `OK\r\nONLY\r\n` 后再 Submit，确认只展示 `greeting`，stop 为 `SINK_STOP`，
   Continue 可用且输入冻结。
4. 点击 Continue：确认只展示 `decode_only`，冻结后缀耗尽，Continue 禁用。
5. Submit `BAD\r\n`：确认候选数增加、Decode 成功数不增加，并显示当前候选 Decode 失败；不编辑
   再次 Submit 时确认候选数与 step 不变。
6. Submit 13 个非 CRLF 字节：确认进入 `DISCARDING_UNTIL_CRLF`，malformed/discard 计数可见；点击
   Reset 后状态、当前结果和累计计数清空，generation 增加。
7. 分别构造半帧/冻结后缀/丢弃态，尝试切换模式、Pipeline、Reload、关闭 Tab 和关闭主窗口：确认
   均出现丢弃确认，选择 No 时状态保持。
8. 打开第二个文档，分别提交不同半帧，确认两个 Tab 的 buffered、generation、step 和累计计数互不
   影响；关闭主窗口时对第一个确认框选择 Yes、第二个选择 No，确认窗口未关闭且两个 Tab 的草稿、
   状态和结果均保持；切换 Hex/ASCII escaped 后确认字节等价、流状态保持、当前展示结果清除。

## 人工验收收口（2026-09-13）

用户按总控提供的完整操作流程执行后明确回复：“人工验收全部符合预期，Lab 已关闭。”
用户确认范围包括：半包与防重复Submit、非法转义、粘包Continue、Decode失败后恢复、恰12字节
边界、跨块CRLF、超长丢弃恢复与Reset、ASCII/Hex切换、双Tab隔离、主窗口Yes后No取消关闭、
单Tab/Reload/Pipeline/模式取消，以及单向动作和完整记录回归。

另附两张截图直接佐证以下事实；不将截图扩展为其他步骤的独立图像证据：

- `codex-clipboard-6ba3c069-e3a6-458c-a8c2-ce0aa5b48dff.png`：非法转义offset 7，
  buffered=5、step=1、candidates=0，原半包保持。
- `codex-clipboard-51848452-a738-4f0d-99f4-f999ae2ce760.png`：Continue后匹配decode_only，
  成功0字段；candidates=2、decode_ok=2、buffered=0、frozen=0/0；本步consumed=6、frames=1，
  Hex为`4F 4E 4C 59 0D 0A`。

截图由用户在当前对话提交，原件位于用户临时附件目录，未复制到仓库，不作为永久随仓资产承诺。
程序与配置使用上文最终路径及SHA-256；用户操作确认、截图与自动化日志分别标识，
本次收口只更新文档，不重复运行无变化测试，不扩展网络、Linux、性能或部署验收。
