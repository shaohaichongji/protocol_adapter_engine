# PAE 公开 StreamFramer 阶段 2A Windows 验证

日期：2026-09-14。状态：已完成派发范围，待总控复核；未获Stage、Commit、Push或正式发布授权。

## 1. 现场、范围与结论

- 仓库：下文以`<repo>`表示本地仓库根目录；命令均从该目录执行。
- 分支/HEAD：`main@dfb08f351cdb22a9b50c9e64e688e3b666e669bc`
- 接管及交付时暂存区：空；保留共享工作树全部既有修改。
- 工具链：Visual Studio 18 2026生成器，x64，v142目录`14.29.30133`，MSVC
  `19.29.30159.0`，Windows SDK `10.0.22621.0`。
- 专用构建树：`out/build/windows-msvc-public-framer-stage2a`；独立consumer构建树：
  `out/build/windows-msvc-public-framer-stage2a-consumer`；日志：`out/public-framer-stage2a`。

阶段2A已公开既有有界Framer，不修改Schema、冻结Plan、Core、内部Framer算法、Lab或Qt。Windows
Debug/Release专项与定向共存回归均通过；这不代表Host 2B、SDK打包、Linux、网络、设备、Golden、
现场或正式性能验证完成。

## 2. 实施内容

### 2.1 Public facade

- `include/pae/stream_framer.h`仅依赖公开头，提供stream capability、创建、Push/Continue、Reset、
  串行空闲Observe及MemoryReport。
- `src/public_api/stream_framer.cpp`通过既有`CompiledStateRef`保活冻结compiled state；每个实例永久
  绑定一个Pipeline和一个内部workspace，不泄漏`PlanBundle`。
- `Push(empty)`与`Continue`复用同一路径；每次只调用一次内部`PushStreamChunk`，不循环排空，
  不保存未消费后缀。`bytes_consumed`、STOP提交点、候选借用和内部issue均直接映射既有事实。
- public facade先检查当前线程的活动callback链，再取得自身guard并准备callback context；同一活动
  callback链中的直接重入及A→B→A间接循环均映射`REENTRANT_CALL`，其他线程操作映射
  `WORKSPACE_BUSY`。Observe同样只在串行空闲状态读取，不承诺并发快照；调用/回调期间移动或销毁
  属于调用方契约违规。
- callback是同步`noexcept`函数指针。候选只在回调期间借用；public Codec的BYTES仍借用候选存储，
  因此必须在同一回调内读取或复制。

### 2.2 能力与资源

- `QueryStreamFramingCapability`从冻结Pipeline及FramingProfile读取`STREAM_CHUNK`事实；空owner、
  越界Pipeline和COMPLETE_RECORD分别精确返回，不从Schema字符串猜测。`available=true`不等于
  当前override一定能通过创建预算。
- options的0值沿用既有resource profile default；非零值仍受内部hard limit约束。同步头小于Plan
  实际需求、submit超限和workspace memory exact/-1均失败关闭且不发布实例。
- `internal_workspace_bytes`使用内部创建结果的精确对象+buffer计费；`facade_bytes`计入public owner和
  Impl；两者相加为`framer_accounted_total_bytes`。`retained_compiled_state_facade_bytes`单列且不
  重复加入每实例总计；`effective_session_limit_bytes`只对应内部workspace准入，不是facade或RSS
  上限。
- 创建分配计数与测试分配观察器相符；MSVC Debug checked-iterator配置包含额外vector proxy，
  Release没有。所有创建分配点均注入失败并验证不发布半对象，随后可正常重建。
- 已创建实例的首次及重复Push使用测试全局分配观察器验证为0次引擎分配。该结论限定于本工具链和
  被测路径，不是所有平台Allocator或进程性能认证。

### 2.3 Consumer与构建接线

- `examples/public_api_framer`只包含`pae/compiler.h`、`pae/stream_framer.h`、`pae/codec.h`；每个
  Framer候选只调用一次public Codec Decode。
- `examples/public_api_framer/standalone`作为独立CMake consumer，通过`PAE::pae`消费，不包含
  `src/**`私有头。Testing-off构建Debug/Release均注册0个测试。
- `PAE_BUILD_PUBLIC_API_STAGE1`启用既有内部Framer目标并把public facade链接到它；未新增依赖、
  Schema能力或网络目标。

## 3. 自动化映射

| 范围 | 本次精确断言 |
| --- | --- |
| capability/create | 空owner、三条Binary stream、ASCII stream、COMPLETE_RECORD false、越界、非stream创建拒绝 |
| Binary策略 | fixed分片/两帧、sync-fixed跨chunk、sync-length垃圾前后缀及手写候选 |
| ASCII | CRLF跨chunk、粘包、12字节超长进入discard、消费CRLF后恢复合法候选 |
| consumed/STOP | STOP消费恰到首候选末尾，未消费后缀重提；当前候选不重放、不预取后缀 |
| budget/continue | submit超限零消费且状态不变；work budget精确游标；frame cap形成pending；Push(empty)与Continue各推进一次 |
| reset/isolation | 半包Reset零交付；两个Framer共享compiled state但buffer互不影响 |
| owner/move | 外部CompiledProtocol清空后Framer/Codec仍可执行；Framer move保留半包，moved-from失败关闭 |
| guard | callback内非空Push/Continue/Reset/Observe均REENTRANT；A→B允许、B→A的Push/Observe均REENTRANT；另一线程Push期间Observe/Continue/Reset均BUSY |
| candidate/Codec | 两个候选、两次且仅两次Decode、两个成功；候选raw bytes在回调内复制后正确；未将Decode得到的BYTES字段复制宣称为本项直接覆盖 |
| allocation | 创建报告与实际分配数一致；逐分配点失败原子性；首次/重复Push零分配 |
| header/dependency | 新头独立编译，公开头禁止私有目录/类型/Qt token；external consumer只用公开头 |

首轮public专项程序输出：Debug `passed=37 failed=0`；Release `passed=37 failed=0`。第5.4节的重入
闭环复核在此基础上新增1项断言，最终Debug/Release均为`passed=38 failed=0`。

## 4. 实际命令与结果

### 4.1 配置

```powershell
cmake -S . -B out/build/windows-msvc-public-framer-stage2a `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_PUBLIC_API_STAGE1=ON
```

结果：退出0。日志：`out/public-framer-stage2a/configure.log`。

### 4.2 Debug / Release定向构建与CTest

两配置串行执行；`<CONFIG>`依次为Debug、Release：

```powershell
cmake --build out/build/windows-msvc-public-framer-stage2a --config <CONFIG> --parallel 4 `
  --target pae_public_stream_framer_tests pae_public_header_compile_tests `
           pae_public_framer_example pae_public_codec_tests `
           pae_public_consumer_metadata_tests pae_protocol_framing_contract_tests `
           pae_ascii_stream_framing_contract_tests

ctest --test-dir out/build/windows-msvc-public-framer-stage2a -C <CONFIG> `
  -R '^(pae.public_api.stream_framer|pae.public_api.header_self_contained|pae.public_api.header_boundary|pae.public_api.complete_record_codec|pae.public_api.consumer_metadata|pae.protocol_framing.contract|pae.protocol_framing.ascii_stream_contract)$' `
  --output-on-failure
```

结果：Debug 7/7，Release 7/7，均退出0。最终日志：

- `out/public-framer-stage2a/build-debug-final.log`
- `out/public-framer-stage2a/test-debug-final.log`
- `out/public-framer-stage2a/assertions-debug-final.log`
- `out/public-framer-stage2a/build-release-final.log`
- `out/public-framer-stage2a/test-release-final.log`
- `out/public-framer-stage2a/assertions-release-final.log`

其中Codec、consumer metadata、内部Binary Framer及ASCII Framer均为受影响定向回归；未把未重跑的
仓库全量矩阵记为本次结果。

### 4.3 独立public-only consumer

```powershell
cmake -S examples/public_api_framer/standalone `
  -B out/build/windows-msvc-public-framer-stage2a-consumer `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  -DPAE_SOURCE_DIR="<repo>"

cmake --build out/build/windows-msvc-public-framer-stage2a-consumer `
  --config <CONFIG> --target pae_public_framer_external_consumer --parallel 4

out/build/windows-msvc-public-framer-stage2a-consumer/<CONFIG>/pae_public_framer_external_consumer.exe `
  examples/config/synthetic_stream_framing_slice.pae.json
```

Debug/Release均退出0并输出：

```text
PUBLIC_FRAMER_CONSUMER_PASS candidates=2 decode_calls=2
```

Testing-off配置以`ctest -N`核对Debug/Release均为`Total Tests: 0`。最终日志：

- `out/public-framer-stage2a/configure-external-consumer.log`
- `out/public-framer-stage2a/build-external-consumer-debug-final.log`
- `out/public-framer-stage2a/run-external-consumer-debug-final.log`
- `out/public-framer-stage2a/build-external-consumer-release-final.log`
- `out/public-framer-stage2a/run-external-consumer-release-final.log`

## 5. 修复中发现与修正

1. 首次Debug测试中，sync-length合法候选后的单个无匹配尾字节被测试误期望为`NEED_MORE`；内部
   契约实际将其计为垃圾并返回`INPUT_EXHAUSTED`。修正独立预期后通过；未修改内部算法。修复前
   证据：`out/public-framer-stage2a/test-debug-targeted.log`。
2. 首版public memory report把创建分配数机械写为4，分配观察器在MSVC Debug发现checked-iterator
   vector proxy的额外分配。现按当前构建ABI报告并由Debug/Release分别直测；热路径零分配未受影响。
   修复前证据：`out/public-framer-stage2a/test-debug-allocation-count.log`。
3. 内部workspace创建当前将构造异常汇总为`INTERNAL_ERROR`。public创建适配层依据当前唯一来源将
   其映射为`ALLOCATION_FAILED`；运行期内部错误仍为`INTERNAL_ERROR`。没有修改内部Framer状态机。

### 5.1 总控消费侧复核后的重入闭环

消费侧复核指出首版单个`thread_local` owner只能识别直接A→A重入；同一线程的A callback进入B，
再由B callback回到仍活动的A时，A错误返回`WORKSPACE_BUSY`。新增A→B→A断言后，修复前Debug专项
实际为`passed=37 failed=1`，失败项为`indirect_callback_loop_rejected_as_reentrant`：

- `out/public-framer-stage2a/build-debug-reentry-fix-before.log`
- `out/public-framer-stage2a/test-debug-reentry-fix-before.log`

实现改为仅在同步调用栈上维护thread-local callback scope链：进入callback时压入当前Framer，退出时
恢复前驱；`Push`、`Reset`和`Observe`在取得facade guard前遍历该链，`Continue`继续委托`Push`。
因此A→B正常执行，B→A可在不分配、不等待且不接触A状态的前提下返回`REENTRANT_CALL`；其他线程
看不到该thread-local链，仍由facade guard返回`WORKSPACE_BUSY`。公共头同时将未消费后缀注释修正为
实际字段名`bytes_consumed`。

新增/加强的直接断言包括：callback内传入非空frame的`Push`以及`Continue`、`Reset`、`Observe`均为
`REENTRANT_CALL`；A→B→A中A的非空`Push`和`Observe`均为`REENTRANT_CALL`，零消费、零候选、无
回调，B正常完成；退出callback后A/B均恢复为空闲可观察状态，A可继续`Push`。半包`Reset`仍由既有
`reset_discards_half_only`覆盖；本轮没有把pending状态Reset或Decode结果中的BYTES字段复制描述为
新增直接覆盖。

修复后Debug、Release串行复核结果均为public定向CTest 6/6、专项断言38/38，均退出0：

- `out/public-framer-stage2a/build-debug-reentry-fix-final.log`
- `out/public-framer-stage2a/test-debug-reentry-fix-final.log`
- `out/public-framer-stage2a/assertions-debug-reentry-fix-final.log`
- `out/public-framer-stage2a/build-release-reentry-fix-final.log`
- `out/public-framer-stage2a/test-release-reentry-fix-final.log`
- `out/public-framer-stage2a/assertions-release-reentry-fix-final.log`

此次复核未重跑内部Binary/ASCII Framer专项、独立consumer或Testing-off构建；这些范围未受实现依赖、
接口形状或构建接线变化影响，其首轮证据仍保留在第4节，不能解释为本次重跑结果。

## 6. 未验证与停点

- 未执行仓库全量CTest、Lab/UI、人工NetAssist、网络、SDK安装/导出包、动态库、Linux、硬件、
  Golden、现场或生产验证；没有启动任何网络收发。
- 未实施Host 2B：callback异常恢复、失败Candidate observer和endpoint/binding identity仍等待后续
  总控派发。2A的`noexcept`callback不能被用来宣称Host可捕获业务异常。
- 没有修改内部Framer算法，既有固定/同步/长度/ASCII语义仍由原模块负责；public facade验证的是
  映射、生命周期、guard和消费事实没有漂移。
- 逻辑内存报告不是RSS上限；宿主输入、候选复制、Codec workspace及未来Host聚合需分别计费。
- 完成后停在“已完成派发范围，待总控复核”，不自动推进Lab或Host，不Stage、Commit、Push。
