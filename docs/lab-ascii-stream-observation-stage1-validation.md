# Lab ASCII Stream 最小观察接口第一阶段验证

日期：2026-09-12。基线：`main@716ff00ea8aa3d3f46b914008890f9706670d32b`。
本阶段只实现 PAE `StreamFramingWorkspace` 的串行只读观察能力，不包含 Lab adapter、Session、
UI、网络或 Evidence，也不改变 Schema、Core 或切帧状态转换。

## 1. 接口与边界

`StreamFramingWorkspace::Observe() const noexcept` 返回值快照
`StreamFramingObservation`，包含：

- `phase`：`COLLECTING`、`DELIVERY_PENDING`，以及启用 Schema 0.11 时的
  `DISCARDING_UNTIL_CRLF`；
- `buffered_bytes`：当前候选缓存字节数；
- `has_internal_work`：当前是否可在没有新输入时继续推进内部工作；
- `effective_max_submit_bytes`、`effective_max_work_units`：工作区创建时实际生效的两个 Lab
  描述所需限额。

观察只读取既有状态和限额，不修改游标、缓存、累计值或状态机。调用方只能在同一所有者的串行
Push/Reset 调用之间读取；接口不获取 `in_use_`，不承诺与 Push/Reset 并发读取安全。既有
`WorkspaceCreateResult::effective_session_limit_bytes`、累计丢弃/坏候选访问器和 Push 单步结果保持不变，
没有重复扩展到观察 DTO。

## 2. 自动化断言

在既有 `pae.protocol_framing.ascii_stream_contract` 中增加：

- 初始状态为 collecting、缓存为 0、无内部工作，并精确返回 override 后的提交与工作限额；
- 超提交上限拒绝前后观察不变；
- 半包为 collecting、缓存为实际字节数、无内部工作；
- M 字节无 CRLF 后为 discarding、缓存为 0、无可空 Push 推进的内部工作；观察后仍能通过后续
  CRLF 恢复，证明读取未推进状态；
- callback 上限留下完整候选时为 delivery pending、缓存为候选实际 9 字节、存在内部工作；观察后
  空 Push 仍只交付一次；
- pending 交付完成及丢弃态 Reset 后均回到空 collecting。

同时运行既有 Schema 0.9 Framer contract，确认新增只读接口没有改变旧 Binary Framer 行为。
首次测试草稿曾把 `RX D!OK\r\n` 误计为 11 字节，Debug/Release 均准确暴露断言失败；按独立字节
计数修正为 9 后通过。该失败属于测试预期修正，不是产品状态机缺陷。

## 3. Windows 实际验证

复用已配置的 `out/build/windows-msvc-ascii-stream`，Visual Studio 18 2026 x64、MSVC
19.51.36257.0、Windows SDK 10.0.22621.0。Debug 与 Release 串行执行：

```powershell
cmake --build out/build/windows-msvc-ascii-stream --config Debug `
  --target pae_ascii_stream_framing_contract_tests pae_protocol_framing_contract_tests
ctest --test-dir out/build/windows-msvc-ascii-stream -C Debug `
  -R '^pae\.protocol_framing\.(ascii_stream_contract|contract)$' --output-on-failure -j 1

cmake --build out/build/windows-msvc-ascii-stream --config Release `
  --target pae_ascii_stream_framing_contract_tests pae_protocol_framing_contract_tests
ctest --test-dir out/build/windows-msvc-ascii-stream -C Release `
  -R '^pae\.protocol_framing\.(ascii_stream_contract|contract)$' --output-on-failure -j 1
```

结果：Debug 构建退出 0、2/2 通过；Release 构建退出 0、2/2 通过。`clang-format --dry-run
--Werror` 与 `git diff --check` 均退出 0。最终日志：
`out/lab-ascii-stream-observation-stage1-final.log`。

## 4. 未验证与停点

- 未修改或验证 Lab adapter、Session、UI、顶层 CMake 门禁及 Qt 工具链。
- 未运行无变化的完整矩阵，未执行网络、Linux、性能、Golden、硬件或现场验证。
- 观察是串行诊断快照，不是并发监控 API 或稳定公共 ABI。
- 第一阶段完成后停止写入，等待总控复核；未 Stage、Commit、Push。
