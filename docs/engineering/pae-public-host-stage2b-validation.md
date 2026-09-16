# PAE 公开 Host 阶段 2B Windows 验证

日期：2026-09-14。状态：已完成派发范围，待总控复核；未获Stage、Commit、Push或正式发布授权。

## 1. 现场、范围与结论

- 仓库：下文以`<repo>`表示本地仓库根目录，所有命令从该目录执行。
- 分支/HEAD：`main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`；接管与交付时暂存区为空。
- 工具链：Visual Studio 18 2026，x64，MSVC v142 `14.29.30133`；Windows SDK
  `10.0.22621.0`。
- 专用构建树：`out/build/windows-msvc-public-host-stage2b`；外部consumer构建树：
  `out/build/windows-msvc-public-host-stage2b-consumer`；日志：`out/public-host-stage2b`。

公开Host已按[确认契约](pae-public-host-stage2b-contract.md)组合既有public Codec/StreamFramer，并完成
总控针对创建期固定Encode Message阻断的限定修正。Encode binding现只固定Pipeline，同一Handle可在
调用期选择该Pipeline内不同可Encode Message。Debug、Release专项、定向共存回归与public-only外部
consumer均通过。本结论不包含SDK安装包、Lab/Qt迁移、网络、Linux、设备、Golden、现场、正式性能或
生产可用验证。

## 2. 实施与状态语义

- `include/pae/host_endpoint.h`公开不可移动`HostEndpoint`、精确字符串绑定、scope/channel/generation
  Handle、同步observer/business回调、操作结果/观察和分项资源报告；公共头不包含私有目录或类型。
- `src/public_api/host_endpoint.cpp`先验证全部binding、index、action、能力、通道数量和静态预算，再
  原子创建。每个Decode stream拥有独立Codec，stream通道另有Framer；Encode创建遍历绑定Pipeline的
  所有可Encode Message，保存有界selector集合，并按最大可靠公开输出上界预分配单一buffer。Host额外
  保活同一compiled state，不复制Matcher或Framer算法。
- COMPLETE_RECORD直接执行一次Decode；stream每候选在同一个Framer callback内执行一次Decode。
  可选observer先于成功business sink；失败候选没有成功record，零字段成功仍交付。
- observer STOP仍交付当前成功一次；business STOP提交当前成功。任一callback异常均被捕获，保留
  实际消费和已发生计数，只令目标通道RESET_REQUIRED；Encode采用相同隔离政策。
- Reset成功清除目标Framer状态、使目标Codec借用view失效并递增generation；旧Handle变为STALE，
  调用方重新Find。默认、expired、foreign、stale及generation耗尽分别失败关闭。
- Host guard在可变调用context前获取；同线程活动callback链（含A→B→A）为REENTRANT，真实跨线程
  竞争为BUSY。`Push(empty)`和`Continue`同路，不轮询、不排空循环、不保存宿主后缀。

## 3. 自动化覆盖

| 范围 | 精确断言 |
| --- | --- |
| 创建/绑定 | 空owner/空列表、空identity、endpoint+action重复、Pipeline越界、零stream、无Encode能力Pipeline、complete误给Framer override均拒绝且不发布半Host |
| identity/Handle | key大小写按字节区分；stream ordinal越界；Reset旧Handle stale；重新Find得到新代；foreign、owner销毁expired、generation耗尽失败关闭 |
| COMPLETE_RECORD | 零字段ASCII成功也交付；成功/失败候选均只Decode一次；observer与成功business直接复制同一raw frame；失败observer可见但record无值且不进business；错误input kind/action拒绝 |
| Binary stream | 两帧受frame budget形成pending，Continue消费0并仅Decode剩余候选；独立stream精确证明Push(empty)等价；STOP精确前缀与后缀重提；失败候选不回扫 |
| ASCII stream | CRLF跨chunk、粘包；observer STOP成功仍交付一次；失败永不交付；business STOP提交当前成功 |
| callback/fault | observer异常抑制business并保留3字节消费；business异常不计正常返回；仅目标stream故障；Encode异常只故障Encode；Reset恢复 |
| Encode | Binary手写`AA 00 07`；同一ASCII Handle依次选择`greeting`与`encode_only`，得到手写`TX A!Z\r\n`和`SEND\r\n`；11字节最大buffer复用；Decode-only、越界及跨Pipeline selector均在Codec前拒绝；callback异常后Reset/重新Find可改选Message；`bytes_produced`与成功交付分开断言 |
| guard/isolation | callback内Find/Observe/Reset均REENTRANT；A→B→A回到A为REENTRANT；另一线程Find/Observe/Reset均BUSY；两条Decode stream互不污染 |
| owner/借用 | 外部CompiledProtocol清空后Host仍执行；callback内读取/复制借用frame、typed BYTES和Encode bytes；保存的Codec record view在Host Reset后直接变为无值 |
| 资源 | 分项和精确等式、聚合exact/-1、每个创建分配点失败不发布且可恢复、D/R实测分配数等于报告、热路径Push/Encode/Observe零引擎分配 |
| 依赖 | 新头独立编译并加入public header禁用token扫描；外部consumer仅含`pae/**`并链接`PAE::pae`；Testing-off零测试 |

修正前Host专项历史输出为Debug `passed=52 failed=0`，但API和测试均只能表达创建期固定Message；证据见
`out/public-host-stage2b/encode-selector-fix/pre-fix-debug-host.log`及
`pre-fix-api-signature.txt`。这不是运行失败，而是目标能力无法表达且用例缺失。

修正后Host专项最终输出：Debug `passed=56 failed=0`；Release `passed=56 failed=0`。新增断言专门覆盖
同Handle多Message、最大buffer、错误selector零Codec调用、callback fault/Reset及直接view失效。

## 4. 资源计费修正

Host首次Debug专项的`creation_allocation_count_exact`失败，实测79而聚合报告43；其余46项通过。差值
36来自四个public Codec各自的9个MSVC checked-iterator `vector` proxy。既有Codec报告只统计vector
payload分配，未像StreamFramer一样纳入Debug proxy。

最小修正在`src/public_api/codec.cpp`的既有allocation count公式中增加MSVC Debug条件项9；不改变
内存字节预算、Release分配数、Codec执行或公开字段结构。修复后Host创建报告与D/R实际分配数均精确
一致。原2B实现为Debug `accounted=6375, allocations=79, measured_allocations=79`、Release
`accounted=6071, allocations=41, measured_allocations=41`。本次Pipeline selector修正把每个Encode
binding的有界可Encode Message索引计入binding storage和创建分配点，最终为Debug
`accounted=6415, allocations=80, measured_allocations=80`、Release
`accounted=6111, allocations=42, measured_allocations=42`；Encode buffer仍为该Pipeline全部可Encode
Message的最大可靠公开上界。聚合exact/-1、逐分配点失败原子性及热路径零分配继续通过。原资源修复前证据：

- `out/public-host-stage2b/test-debug-host-initial.log`
- `out/public-host-stage2b/assertions-debug-host-initial.log`

报告中的Host总字节由facade、scope payload、binding/channel精确数组capacity、identity精确副本、
Codec/Framer accounted totals和Encode buffer capacity组成；共享compiled-state facade只单列一次。
`shared_ptr/weak_ptr` allocator控制元数据及allocator簿记不宣称为可移植字节预算，但其创建分配点计入
allocation count。该逻辑报告不是进程RSS、调用方回调复制或新旧Host并存峰值上限。

## 5. 实际命令与结果

### 5.1 配置

```powershell
cmake -S . -B out/build/windows-msvc-public-host-stage2b `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_HOST_ENDPOINT_SLICE=ON
```

退出0。日志：`out/public-host-stage2b/configure-final.log`。

### 5.2 Debug / Release定向构建与CTest

两配置严格串行；`<CONFIG>`依次为Debug、Release：

```powershell
cmake --build out/build/windows-msvc-public-host-stage2b --config <CONFIG> --parallel 4 `
  --target pae_public_host_endpoint_tests pae_public_header_compile_tests `
           pae_public_api_tests pae_public_codec_tests pae_public_consumer_metadata_tests `
           pae_public_stream_framer_tests pae_public_host_example pae_host_endpoint_tests `
           pae_protocol_framing_contract_tests pae_ascii_stream_framing_contract_tests

ctest --test-dir out/build/windows-msvc-public-host-stage2b -C <CONFIG> `
  -R '^(pae[.]public_api[.]|pae[.]host_endpoint[.]contract|pae[.]protocol_framing[.](contract|ascii_stream_contract))' `
  --output-on-failure
```

Debug/Release均10/10、退出0；包含7项public API、既有内部Host、Binary Framer和ASCII Framer。
Host专项直接执行均56/56、退出0。本次修正日志位于
`out/public-host-stage2b/encode-selector-fix/`：

- `build-debug-final.log`、`ctest-debug-final.log`、`assertions-debug-final.log`
- `build-release-final.log`、`ctest-release-final.log`、`assertions-release-final.log`

### 5.3 public-only外部consumer与Testing-off

```powershell
cmake -S examples/public_api_host/standalone `
  -B out/build/windows-msvc-public-host-stage2b-consumer `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  -DPAE_SOURCE_DIR="<repo>"
cmake --build out/build/windows-msvc-public-host-stage2b-consumer `
  --config <CONFIG> --parallel 4 --target pae_public_host_external_consumer
out/build/windows-msvc-public-host-stage2b-consumer/<CONFIG>/pae_public_host_external_consumer.exe `
  examples/config/synthetic_ascii_stream_slice.pae.json
ctest --test-dir out/build/windows-msvc-public-host-stage2b-consumer -C <CONFIG> -N
```

Debug/Release均退出0；同一`device` Encode Handle先后选择两个Message并输出
`PUBLIC_HOST_CONSUMER_PASS candidates=1 successes=3 greeting_bytes=8 literal_bytes=6`；两配置均
`Total Tests: 0`。日志位于`out/public-host-stage2b/encode-selector-fix/`：
`configure-external-consumer-final.log`、`build-external-consumer-{debug,release}.log`、
`run-external-consumer-{debug,release}.log`、`ctest-n-external-consumer-{debug,release}.log`。

## 6. 未验证与停点

- 未执行仓库全量CTest、Lab/UI、人工NetAssist、网络、SDK安装/动态库、Linux、硬件、Golden、现场、
  正式性能或生产验证；没有启动网络收发。
- 没有修改内部Core/Plan/Framer/Host算法、Schema、Lab或Qt。当前public构建开关仍沿用既有
  `PAE_BUILD_PUBLIC_API_STAGE1`名称；本片未扩大为SDK版本或ABI稳定性承诺。
- callback的用户副作用不可回滚；Handle仅延长scope控制元数据以安全识别expired，不保活Host或执行
  通道。销毁与操作/callback并发仍是调用方契约违规。
- 完成后停在“已完成派发范围，待总控复核”，不自动启动Lab消费改造或工程入口同步，不Stage、
  Commit、Push。
