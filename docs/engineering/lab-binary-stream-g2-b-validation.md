# Lab Binary stream G2-B validation

## 1. 结论与边界

本片在 `main@04fa429e1e8361b89f8622e2dc998d8869309c1c` 的共享工作树上完成：

- `DocumentDescription` 通过公开 `QueryPipelineFramingDescription` 映射 Binary Pipeline 的
  `input_kind`、`framing_strategy` 和最大候选帧 `M`；
- `BinaryHostAdapter` 复用 G2-A 的唯一 public owner，提供 Lab-owned 的
  `SubmitStream`、`ContinueStream`、`ObserveStream`、`MapCurrentStream`、
  `StreamContinueAvailable` 和 `ResetStream`；
- 展示层明确区分前置拒绝、无候选、Decode 成功、Decode 失败和 materialization 失败；
- UI copy/budget 后置失败保留已经确认的 Host 消耗事实，清空本次展示结果，并通过每 Flow
  一字节的展示故障侧车要求目标 Flow `Reset`。侧车不复制 frozen input，也不形成第二套
  Framer/Decode 状态；
- 定向返修后，故障 Flow 的 `MapCurrentStream` 直接从 owner 已保存的 `stream.current`
  无分配重建同一 materialization failure 投影，保留真实 `public_host` 和 confirmed
  consumption，不再次执行 Host/Decode、不再次触发 result copy，也不把本地映射异常伪装为
  Host callback failure；
- 未修改 G2-A owner、PAE 公共接口/Core/Schema、Session/DocumentTab、CMake、Qt 环境、SDK、
  部署或 canonical fixture。

本报告只证明 headless adapter/description 的 Windows MSVC Debug/Release 定向结果；不代表
Qt 可见 UI、SDK 包外消费、全仓回归、Linux、硬件或发布验收。

## 2. 实际源码范围

| 文件 | 作用 |
| --- | --- |
| `tools/protocol_lab_ui/owned_presentation_types.h` | 新增中立 stream input/strategy/phase 与 observation DTO；扩展 Pipeline framing facts |
| `tools/protocol_lab_ui/public_binary_description.cpp` | 只读公开 framing query 并映射三类 Binary stream strategy 和 `M` |
| `tools/protocol_lab_ui/binary_host_adapter_public.h` | 声明 Binary stream step/view 与公开适配入口 |
| `tools/protocol_lab_ui/binary_host_adapter_public.cpp` | 复用 G2-A owner，完成 step/result 映射、Flow 隔离、Reset 和 copy/budget/input peak 门禁 |
| `tests/protocol_lab_ui/binary_public_h2_tests.cpp` | 增加描述、step、无陈旧结果、拒绝不推进、copy/budget 故障与双 Flow/Reset 回归 |

最终源码哈希和现场状态见：
`out/validation/lab-binary-stream-g2-b/final-source-state.txt`。

## 3. 语义与预算核对

### 3.1 单一 owner

- stream 数据只进入 G2-A `public_decode::Adapter`；G2-B 不保存第二份 frozen input；
- 每次 UI action 只调用一次 `SubmitStreamChunk` 或 `ContinueStream`；
- Candidate 只从 G2-A `StreamStep` 投影为 Lab-owned result/failure，不重复 Decode；
- complete-record binding 仍走原 G1 路径，stream binding 不再被 `IsCompleteDecode` 接受。

### 3.2 失败与陈旧结果

- `host_called=false` 映射为 `PREFLIGHT_REJECTED`，不会制造 Decode 结果；
- Host 正常推进但没有 Candidate 映射为 `NO_CANDIDATE`，当前展示没有旧 success；
- Candidate 成功/失败分别映射为 `DECODE_SUCCESS`/`DECODE_FAILURE`；
- G2-A callback materialization failure 及 G2-B copy/budget failure映射为
  `MATERIALIZATION_FAILURE`；后者保留 `public_host.bytes_consumed` 等已确认事实，返回空 result，
  仅将目标 Flow 标记为需 Reset。
- 在展示故障侧车有效期间，`MapCurrentStream` 从 owner 的当前 step 重建无动态 payload 的
  failure projection；failure 内的 `host_status` 与 `public_host.status` 都保持 Host 原始事实，
  本地故障由 `local_status=MATERIALIZATION_FAILED` 和
  `diagnostic=COPY_FAILED_RESET_REQUIRED` 表达。

### 3.3 预算

- description 的新增字段为 `PipelineDescriptor` 内联成员，继续由既有
  `sizeof(PipelineDescriptor)` 计入 description copy/accounting；
- stream result copy 在复制前同时检查 retained presentation、active view、调用者本次输入峰值
  和 result upper bound；
- Flow 展示故障侧车按实际 channel 数预分配并计入 instance accounting；
- 测试覆盖 result copy hook 异常和“Host 已推进后 result upper 与 active/input 共存超预算”两条
  后置失败路径，均要求 Reset 且不发布部分结果。

## 4. 定向验证

构建目录实际为 `out/build/windows-msvc-lab-g2-b/v142`。首次直接使用父目录配置时默认选择
v145，被项目 Qt 门禁拒绝；本轮无删除授权，因此保留失败缓存，并在父目录内新建 `v142/`
子目录，显式使用 `-T v142,version=14.29.30133` 完成验证。

Debug 与 Release 均构建以下目标：

- `pae_protocol_lab_ui_binary_public_h2_tests`
- `pae_protocol_lab_ui_binary_public_header_tests`
- `pae_protocol_lab_ui_owned_presentation_types_tests`

随后执行：

```text
ctest --test-dir out/build/windows-msvc-lab-g2-b/v142 -C <Debug|Release> \
  -R ^pae\.tools\.protocol_lab_ui\.(binary_public_h2|binary_public_header|owned_presentation_types)$ \
  --output-on-failure
```

结果：

| 配置 | 构建 | 测试 |
| --- | --- | --- |
| Debug | PASS | 3/3 PASS |
| Release | PASS | 3/3 PASS |

Release 目标的生成工程包含 `UndefinePreprocessorDefinitions=NDEBUG`，测试源码另有编译期
`#error` 门禁，避免断言被静默关闭。证据：

- `out/validation/lab-binary-stream-g2-b/final-debug-build.log`
- `out/validation/lab-binary-stream-g2-b/final-debug-tests.log`
- `out/validation/lab-binary-stream-g2-b/final-release-build.log`
- `out/validation/lab-binary-stream-g2-b/final-release-tests.log`
- `out/validation/lab-binary-stream-g2-b/final-release-assertions.txt`

总控限定返修仅重跑受影响的 `binary_public_h2`：

| 配置 | 构建 | 测试 |
| --- | --- | --- |
| Debug | PASS | 1/1 PASS |
| Release | PASS | 1/1 PASS |

返修证据：

- `out/validation/lab-binary-stream-g2-b/mapcurrent-fix-debug-build.log`
- `out/validation/lab-binary-stream-g2-b/mapcurrent-fix-debug-tests.log`
- `out/validation/lab-binary-stream-g2-b/mapcurrent-fix-release-build.log`
- `out/validation/lab-binary-stream-g2-b/mapcurrent-fix-release-tests.log`

## 5. 测试覆盖与未验证项

新增/受影响回归覆盖：

- FIXED_LENGTH、SYNC_FIXED_LENGTH、SYNC_LENGTH_FIELD 描述映射及 `M=3/4/8`；
- partial input 的无候选、oversize 前置拒绝不推进、STOP 后显式 Continue；
- Decode failure 后无旧 success、后续恢复成功；
- UI result copy 异常与预算共存失败保留 confirmed consumption、清空结果并要求 Reset；
- result copy 异常与预算失败后重新 `MapCurrentStream` 保持原 Host status/consumption 和本地
  materialization 分类；step/candidate 计数不变，copy hook 不再调用，无 success 结果；
- Reset 仅影响目标 Flow，另一 Flow 的 buffered input 保持；
- 原有 G1 complete Decode/Encode、public header 和 owned presentation types 回归。

未运行 Qt 可见 UI、全仓测试或 SDK 包外 D/R；未修改/覆盖任何既有 SDK 与部署。首次 v145
配置失败缓存仍保留在 `out/build/windows-msvc-lab-g2-b` 父目录中，未执行删除。

## 6. Git 状态

未执行 Stage、Commit、Push、发布或删除。共享工作树中既有总控/G2-A 变更均保留；本片完成后
停止写入，等待总控复核。
