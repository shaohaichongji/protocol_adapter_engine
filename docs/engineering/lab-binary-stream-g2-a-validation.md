# Lab Binary stream G2-A 非 Qt 实施验证

## 1. 结论与边界

2026-09-19 在 `main@04fa429e1e8361b89f8622e2dc998d8869309c1c` 的共享工作树上，按
`lab-binary-stream-g2-contract.md` 完成 G2-A：现有 public Binary owner 在不增加 PAE API、
不访问 private Plan、不重复编译或 Decode 的前提下，能够消费 Schema 0.9 三种 Binary
`STREAM_CHUNK` strategy：

- `FIXED_LENGTH`；
- `SYNC_FIXED_LENGTH`；
- `SYNC_LENGTH_FIELD`。

本片仅修改 non-Qt public Binary owner 与其独立专项测试/CMake；没有进入 G2-B/C，没有修改
Qt、Session、UI adapter/DTO、根 CMake、PAE Core/API/Schema、canonical fixture、SDK 或部署。

## 2. 实际变更

### 2.1 public Binary owner

修改：

- `tools/protocol_lab_binary/public_binary_decode.h`
- `tools/protocol_lab_binary/public_binary_decode.cpp`

实现事实：

1. 准备阶段通过 `QueryPipelineFramingDescription` 区分 complete 与 stream，只接受三种 Binary
   strategy；公开 `M = maximum_candidate_frame_bytes` 超过 Lab candidate/result 物化能力时准备失败。
2. 继续由同一个 `CompiledProtocol`、同一个 `HostEndpoint` 和每 `(binding, Flow)` 唯一 handle
   执行；complete Decode/Encode 路径不复制、不 fallback。
3. 每个 stream Flow 独占 strategy、M、冻结输入、cursor、当前一步、累计候选/Decode/回调/
   discard/malformed 事实和 local fault；公开 observation 合并 Host runtime state 与 Lab-owned state。
4. 新 Submit 仅在没有冻结 suffix、没有 internal work、无需 Reset 时接受。输入先经过长度/配额门禁，
   写入已预留的冻结 buffer 后只调用一次 Host `Push`。
5. 显式继续时，有冻结 suffix 就对 `[cursor,end)` 调用一次 `Push`；suffix 为空且 Host 报告
   internal work 才调用一次 `Continue`。无 suffix、无 internal work 返回正常 `NO_WORK`，不调用 Host、
   不制造 fault，也没有自动 drain 循环。
6. candidate observer 对每个候选返回 STOP，使一次用户操作至多发布一个 candidate。失败候选在
   observer 内同步深复制；成功字段在 business callback 内同步深复制；回调后不保留 borrowed view，
   不做第二次 Decode。
7. 实际执行前清旧 stream current；输入门禁失败不改变 Host、cursor 或原 current。执行后 callback
   copy/Host/计数契约失败会 fail closed 并要求 Reset，同时保留 Host 已确认的 bytes consumed。
8. Reset 只作用目标全局 Flow index；Host Reset 成功后重新 `Find(endpoint, action, stream_index)`，
   再清该 Flow 的冻结输入、stream current 和累计计数。其他 Flow 保持不变；非 stream G1 Reset
   保持原 complete current 行为。

### 2.2 独立专项

新增/修改：

- `tests/protocol_lab_binary/public_binary_stream_tests.cpp`
- `tests/protocol_lab_binary/CMakeLists.txt`

新测试目标为 `pae_binary_public_stream_tests`，CTest 名为
`pae.protocol_lab_binary.public_stream`。测试直接读取既有 canonical
`examples/config/synthetic_stream_framing_slice.pae.json`，未修改或复制 fixture。

## 3. 预算口径

本实现保持三类量独立：

- `C`：`min(Lab max_stream_chunk_bytes, Host observed effective_max_submit_bytes)`；
- `M`：公开 framing description 的最大完整 candidate bytes；
- work budget：Host observed effective work units，测试可通过 Lab limits 向下覆盖。

每个 stream Flow 在准备阶段按 C 预留唯一 `frozen_input`，并把
`frozen_input.capacity()` 的实际值计入 `InstanceAdmissionBytes()`；candidate/current/rejected 的动态
物化继续由既有每通道 `max_result_bytes` 与额外 replacement slot 计费。Submit 只在冻结输入为空时
写入已预留 buffer，不在 owner 内再创建一份临时 vector；后续 Qt Hex 解析 vector 属于消费层预算，
本片没有把它重复计为 Host 或 owner 常驻内存。以上是逻辑 admission/accounting，不是 RSS 硬限制。

专项提供了独立期望，而不只用 owner 自己的 exact/minus-one 自证：同一六个 stream Flow 下，C 从
32 增至 64 时，实例计费必须精确增加 `6 * (64 - 32)`，且逐 Flow 实际 reserve capacity 为 32；
另覆盖 exact、minus-one、replacement coexistence、M 超界和 callback result 物化预算拒绝。后置补证
又以测试可执行文件内的单次 `operator new` 注入，区分并覆盖真实 `std::bad_alloc`；该注入不进入产品
library、header 或编译宏。

## 4. 构建与验证

### 4.1 独立根与工具链

- 构建根：`out/build/windows-msvc-lab-g2-a`
- 证据根：`out/validation/lab-binary-stream-g2-a`
- Visual Studio：Professional 2026，MSVC `19.51.36257.0`
- Windows SDK：`10.0.22621.0`
- Generator：`Visual Studio 18 2026`，`x64`

配置命令：

```powershell
cmake -S . -B out/build/windows-msvc-lab-g2-a `
  -G "Visual Studio 18 2026" -A x64 `
  -DPAE_BUILD_TESTING=ON `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON `
  -DPAE_BUILD_PROTOCOL_LAB_BINARY_PUBLIC_H1=ON
```

构建与定向测试命令对 Debug/Release 各执行一次：

```powershell
cmake --build out/build/windows-msvc-lab-g2-a --config <Debug|Release> `
  --target pae_binary_public_h1_tests pae_binary_public_stream_tests -- /m:1

ctest --test-dir out/build/windows-msvc-lab-g2-a -C <Debug|Release> `
  -R "^(pae\.protocol_lab_binary\.(public_h1|public_stream))$" `
  --output-on-failure
```

终态结果：

| 配置 | public H1 complete 回归 | public stream G2-A | 汇总 |
| --- | --- | --- | --- |
| Debug | PASS | PASS | 2/2 PASS |
| Release | PASS | PASS | 2/2 PASS |

日志：

- `out/validation/lab-binary-stream-g2-a/configure.log`
- `out/validation/lab-binary-stream-g2-a/debug-build-final.log`
- `out/validation/lab-binary-stream-g2-a/debug-ctest-final.log`
- `out/validation/lab-binary-stream-g2-a/release-build-final.log`
- `out/validation/lab-binary-stream-g2-a/release-ctest-final.log`

### 4.2 Release 断言有效性

专项在包含 `NDEBUG` 的 Release 配置中先 `#undef assert`，再定义始终执行的失败宏。额外以缺少 fixture
参数运行 Release 可执行文件，实际得到：

```text
PUBLIC_STREAM_CHECK_FAILED line=360 expression=argc == 2
EXPECTED_NONZERO_EXIT=1
```

证据：`out/validation/lab-binary-stream-g2-a/release-assert-active.log`。因此 Release 的专项断言未被
`NDEBUG` 关闭。

### 4.3 覆盖范围

新专项覆盖：

- 三种 strategy 的 split；fixed/sync-fixed glued suffix；sync recovery；sync-length malformed
  length/discard 后恢复；
- observer STOP、每步至多一个 candidate、精确 suffix cursor、显式 Continue；
- work budget 暂停、suffix Push、suffix 为空时 internal-work Continue、正常 idle；
- 每 candidate 一次 Decode，成功/失败/business/observer 计数恒等式；
- callback result 物化预算失败、保留已确认消费、目标 Flow reset-required；
- Reset generation/重新 Find、双 Flow buffer/结果/故障隔离；
- C/M/instance/replacement 预算门禁和执行前 oversize 原子拒绝；
- complete Decode 对 stream binding 返回 `WRONG_INPUT_KIND`，证明没有隐藏 fallback；
- 既有 `pae.protocol_lab_binary.public_h1` complete Decode/Encode 回归。

### 4.4 真实 allocation failure 补证

总控限定复核指出原专项只覆盖资源预算拒绝，不能把 `RESOURCE_LIMIT` 当作真实 allocation failure。
补证仅修改 `public_binary_stream_tests.cpp`，采用本仓公开 API 测试已有的“测试可执行文件覆盖全局
`operator new`”模式，没有增加产品 hook、产品宏或新 CMake 开关。

为避免按退出码或预算自证，注入包含明确的一次性命中记录：实际失败请求大小和命中次数必须分别
满足期望大小及恰好一次，否则专项失败。

1. **准备期 frozen reserve**：先在 allocation tracking 打开期间只执行一次 `AdoptCompiled`，把请求
   大小记录进固定 `std::array`，不在 tracker 内动态分配。专项定位到六个连续同尺寸、且不小于 C 的
   请求，证明它们对应六个 stream Flow 的 `frozen_input.reserve(C)`；随后重新编译输入，针对捕获到的
   实际请求大小单次抛出 `std::bad_alloc`。断言命中一次、`AdoptCompiled` 返回
   `LocalStatus::RESOURCE_LIMIT`、没有半成品 adapter，并立即正常重建证明恢复。
2. **运行期 callback 深复制**：先完成 owner 准备并构造六字节两帧输入；冻结 vector 已预留，Host
   hot path 不分配。随后只对三字节请求单次抛出 `std::bad_alloc`，明确命中
   `Candidate::frame` 的同步深复制。断言 Host 映射为 `CALLBACK_FAILED`、确认消费 3 字节、cursor 保留
   为 3、无 candidate、仅目标 Flow `reset_required`；Reset/重新 Find 后同 Flow 再提交 `AA 05 06`
   正常得到 `0x0506`。

补证终态仅重跑新增专项：

```powershell
cmake --build out/build/windows-msvc-lab-g2-a --config <Debug|Release> `
  --target pae_binary_public_stream_tests -- /m:1

ctest --test-dir out/build/windows-msvc-lab-g2-a -C <Debug|Release> `
  -R "^pae\.protocol_lab_binary\.public_stream$" --output-on-failure
```

结果：Debug `1/1 PASS`，Release `1/1 PASS`。证据：

- `out/validation/lab-binary-stream-g2-a/allocation-final-debug-build.log`
- `out/validation/lab-binary-stream-g2-a/allocation-final-debug-ctest.log`
- `out/validation/lab-binary-stream-g2-a/allocation-final-release-build.log`
- `out/validation/lab-binary-stream-g2-a/allocation-final-release-ctest.log`
- `out/validation/lab-binary-stream-g2-a/allocation-final-debug-runtime.log`
- `out/validation/lab-binary-stream-g2-a/allocation-final-release-runtime.log`

直接运行日志还记录了实际命中标记。MSVC Debug/Release 的 reserve 请求包含不同实现开销，因此专项
先捕获后注入，而不把逻辑 C 错当作 allocator request size：

```text
Debug:   ALLOCATION_FAILURE_HIT phase=prepare_frozen_reserve size=65568 hits=1
Release: ALLOCATION_FAILURE_HIT phase=prepare_frozen_reserve size=65560 hits=1
Both:    ALLOCATION_FAILURE_HIT phase=callback_candidate_frame size=3 hits=1 consumed=3
```

实施中第一次把全局 work override 设为 4 时，Host 创建三策略组合在准备阶段按公开资源门禁返回
`RESOURCE_LIMIT_EXCEEDED`；这不是产品源码失败。测试参数改为可创建三策略实例的 5 后，fixed strategy
仍能稳定覆盖 work-budget resume，终态 D/R 均通过。早期日志保留在证据目录，终态结论只以上述
`*-final.log` 为准。

## 5. 未验证与剩余边界

- 未执行全仓测试、Qt 构建、Session/UI 接线、Binary-only（ASCII stream OFF）矩阵、SDK 包外消费、
  部署或人工体验；这些属于后续 G2-B/C 或独立验证。
- 未修改 `cmake/PaeQt513.cmake` 或部署 canonical fixture；该决定已确认但只在后续 B/C 实施。
- 未验证 Linux、真实通信、设备、网络监听、性能/RSS 或现场行为。
- 本片没有自动 drain/retry，没有算法参数检查器，也没有扩展公开 metadata。

## 6. Git 与共享树状态

- 本片实际源码/测试写入仅为第 2 节四个文件；本报告是第五个授权文件。
- 总控的 `pae-execution-delivery-organization-plan.md`、G2 contract 与两份预检在开始前已存在，
  本任务均予以保留，未修改。
- 未 Stage、未 Commit、未 Push、未发布、未删除；达到 G2-A 停点后停止写入，等待总控复核。
