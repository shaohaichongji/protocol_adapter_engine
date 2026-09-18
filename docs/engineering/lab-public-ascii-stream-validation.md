# Lab 0.11 非 Qt 公开流式适配验证

## 1. 结论与边界

本片在 `main@dbf4798f96a45b8d36a4754ad5a01e274680337e` 加既有未提交的 PAE 静态 Framing
查询、SDK consumer 和文档变更之上实施。既有共享变更均保留，未 Stage、Commit、Push、发布或删除。

非 Qt 0.11 ASCII stream 已选择现有 `public_ascii_host_adapter.*` 作为唯一 Host/Flow 状态所有者：
同一个 A1 compiled owner 提供描述及结果物化；每个 Flow 只保留一份 Handle、冻结 chunk、cursor、计数与
fault 状态；未新增第二套 Framer、Decode 或结果映射路径。0.10 complete-record Decode/Encode 仍复用同一
Host helper，专项回归通过。本片未接 Qt/UI/dispatch，也未修改 PAE public/Core/Plan/Schema 或 SDK。

## 2. 实际变更

- 根 `CMakeLists.txt`：新增默认 OFF 的 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM`，显式要求 A1、
  public API Stage 1、Host endpoint 和 Schema 0.11 stream framing；不依赖 A2 UI 或 private adapter。
- `tools/protocol_lab_ascii/CMakeLists.txt`：A2 与新 stream 开关可分别引入共享 Host helper。
- `tools/protocol_lab_ascii/public_ascii_host_adapter.h/.cpp`：
  - 用公开 `QueryPipelineFramingDescription` 准入 `ASCII_CRLF/STREAM_CHUNK/M`；complete 查询合法，但
    stream Submit 本地拒绝；
  - C 取 `min(64 KiB, effective_max_submit_bytes)`，与静态 M、work limit 分离；
  - 每动作只调用一次 Host Push/Continue，observer 对成功只记事实、对失败物化诊断，business 对成功
    物化一次，均以 STOP 将一次动作限制为最多一个候选；
  - 按 Host 精确 consumed 推进冻结后缀，只有无后缀且存在 internal work 时空 Continue；正常
    `CODEC_FAILED` 可继续，复制/回调/计数/consumed/Host 故障要求目标 Flow Reset；
  - Reset 后重新 Find 并 Observe 新 Handle，成功后才清目标 Flow，本地状态不影响另一 Flow；
  - owner、Host、本地容器与每 Flow 冻结 capacity 在发布前用饱和加乘完成 instance/replacement 准入。
  - 限定返修将 `StreamStepResult::detail` 的动态 `std::string` 改为 Lab 自有
    `StreamDiagnostic` 枚举；全部拒绝、catch 和候选故障分支仅赋枚举，不再从 `noexcept` 错误路径
    分配诊断文本，也没有引入 `string_view` 或临时对象借用。
- `tests/protocol_lab_ascii/public_ascii_stream_adapter_tests.cpp` 及局部 CMake：新增 headless 专项。
- `tests/protocol_lab_ascii/public_ascii_stream_consumer/`：独立 `find_package(PAE CONFIG REQUIRED)`、只链接
  `PAE::pae`，同时编译 Lab adapter 源码的静态 SDK consumer。

最终关键源码 SHA-256：

- `public_ascii_host_adapter.cpp`：`AAA1175C3F2B27115102094A0CFD002AD393470A668309AE6A7FE234BC735C39`
- `public_ascii_host_adapter.h`：`BF4B055B85E04187647EC13ECEA1E1E38A79197E61AA768ECC7BDE7411FB3243`
- `public_ascii_stream_adapter_tests.cpp`：`02FBC8DF4CADD6E235B20264F0774D89E754A817FA6898BC20C9694A1F9324C1`
- package consumer `main.cpp`：`C34592EE258F62B5ABD6F21A7239CF52E403E1391FE642BCFBD48AE94505586C`

## 3. 测试先行与专项覆盖

先加入测试/CMake 后，在实现前编译失败，缺失项正是 stream DTO、Submit/Continue/Observe、C 查询和
framing option 接线；证据：
`out/lab-public-ascii-stream/before-implementation-debug-build.log`。随后才实施 Host adapter。

新专项覆盖：CR/LF 跨 chunk、粘包 STOP 后缀、失败候选后的合法候选、literal-only 零字段、work-budget
pending/Continue、超长记录 discard 后恢复、两个 Flow 隔离、Reset/generation 恢复、回调复制分配失败、
空/超 C/已有后缀时拒绝新 Submit、complete stream Submit 拒绝及 complete Decode/Encode 回归。

预算验证除 instance/replacement exact/minus-one 外，另以相同 M/Host workspace、不同 C=13/17 的两个
实例独立断言：两 Flow 的 `AccountedBytes` 差值必须为 `2 * (17 - 13)`，从而核对显式冻结 reserve 数量
与计费数量一致；该结论是当前 MSVC/STL 的逻辑计费证据，不声称 RSS 硬上限。

## 4. 仓库内 Debug/Release 验证

独立构建目录：`out/build/windows-msvc-lab-public-ascii-stream`。配置使用 VS 18 2026、x64、
`v142,version=14.29.30133`，启用 public API Stage 1、Host、A1、public stream、Schema 0.11 framing 和
Testing。构建及测试严格串行。

最终运行：

```text
cmake --build out/build/windows-msvc-lab-public-ascii-stream --config <Debug|Release>
  --target pae_protocol_lab_ascii_public_stream_tests pae_protocol_lab_ascii_public_a1_tests -- /m:1
ctest --test-dir out/build/windows-msvc-lab-public-ascii-stream -C <Debug|Release>
  -R "pae\.tools\.protocol_lab_ascii\.public_(a1|stream)" --output-on-failure
```

限定返修后的最终矩阵包含 A1、主 stream 与两个 noexcept 诊断探针：Debug 4/4 PASS，Release 4/4
PASS。Release 使用测试文件自定义检查宏，不受 `NDEBUG` 关闭。返修前独立 stream 可执行文件输出：

- Debug：`PUBLIC_ASCII_STREAM_TEST_PASS accounted=149091`
- Release：`PUBLIC_ASCII_STREAM_TEST_PASS accounted=148051`

限定返修最终证据：

- `out/lab-public-ascii-stream/noexcept-fix-debug-build-final.log`
- `out/lab-public-ascii-stream/noexcept-fix-debug-test-final.log`
- `out/lab-public-ascii-stream/noexcept-fix-release-build-final.log`
- `out/lab-public-ascii-stream/noexcept-fix-release-test-final.log`

## 5. 静态 SDK 包外 Debug/Release

未重打 SDK。consumer 分别只使用既有本地候选：

- `out/sdk-public-stream-description/candidate1-20260918/pae-sdk-static-debug`
- `out/sdk-public-stream-description/candidate1-20260918/pae-sdk-static-release`

两个 build 的 `CMAKE_PREFIX_PATH` 和 `PAE_DIR` 均指向对应候选包；consumer CMake 只有
`find_package(PAE CONFIG REQUIRED)` 和 `PAE::pae`，没有仓库 PAE target 或 private/Qt 头链接。
候选 `pae.lib` SHA-256 分别为：

- Debug：`610ED9E8BEDFD458F0862DEF76E48C0752C5E1D116454F0C794FB9381D52FD8A`
- Release：`02A2A16FD077D7412BC1BCD504C0CB92130715C2719B94741B6712F7309199A3`

包外 adapter 源码编译及运行 D/R 均输出
`PUBLIC_ASCII_STREAM_PACKAGE_CONSUMER_PASS`。最终证据：

- `out/lab-public-ascii-stream/noexcept-fix-package-static-debug-build.log`
- `out/lab-public-ascii-stream/noexcept-fix-package-static-debug-run.log`
- `out/lab-public-ascii-stream/noexcept-fix-package-static-release-build.log`
- `out/lab-public-ascii-stream/noexcept-fix-package-static-release-run.log`
- `out/lab-public-ascii-stream/package-origin-and-dependency-scan.log`

## 6. `noexcept` 诊断限定返修

返修前测试将“下一次任意分配”设为失败并分别进入真实拒绝路径、候选物化失败路径；两者均由测试专用
terminate handler 以退出码 86 结束。其中无效 binding 拒绝在诊断赋值前没有其他分配点，直接证明旧
`std::string detail` 不能保证从 `noexcept` 安全返回；候选路径记录为同类故障场景证据，返修后的定向
断言再分别核对候选事实和诊断操作：

- `out/lab-public-ascii-stream/noexcept-fix-before-debug-reject.log`
- `out/lab-public-ascii-stream/noexcept-fix-before-debug-candidate.log`

返修后：

- 拒绝探针仍在下一次任意分配失败条件下调用 `SubmitStreamChunk`，正常返回
  `CHANNEL_NOT_ASCII_STREAM`，且分配失败标志未被消费；
- 候选探针真实触发 callback copy `bad_alloc`，核对 `ALLOCATION_FAILED`、Codec/Host consumed、候选事实
  与 `reset_required` 后，再在下一次分配失败条件下复制/读取
  `CANDIDATE_COPY_FAILED_RESET_REQUIRED`；诊断操作未分配；
- 编译期同时断言 `StreamDiagnostic` 是 enum 且 nothrow copy assignable；源码扫描确认当前 public Host
  helper 已无 `std::string detail` 或 `result.detail` 写入。

定向探针 Debug/Release 已包含在第 4 节最终 4/4 矩阵中；Debug 单独证据为
`out/lab-public-ascii-stream/noexcept-fix-debug-probes2.log`。四次独立运行分别输出
`PUBLIC_ASCII_STREAM_NOEXCEPT_DIAGNOSTIC_PASS reject/candidate`，证据为
`noexcept-fix-{debug,release}-{reject,candidate}-run.log`。

## 7. 门禁、未验证与剩余风险

- 默认配置确认 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_STREAM=OFF` 且不生成 stream test target；证据
  `default-off-configure.log`、`default-off-check.log`。
- `PAE_BUILD_TESTING=OFF` 时 Release A1/Host helper 可构建且不生成 stream test target；证据
  `testing-off-configure.log`、`testing-off-release-build.log`、`testing-off-check.log`。
- `git diff --check` 通过。
- 未运行 Qt/UI、A2 UI、全仓测试、SDK 六组重验、人工烟测、Linux、硬件、Golden 或现场验证；未重打
  五包。历史 Qt 访问违例未因本片关闭。
- SDK 候选仍是 `dbf4798` 加既有 dirty snapshot 的本地复核候选，不是干净 HEAD、正式发布或稳定 ABI
  证据。Qt 接线须等待总控复核后另行派发，本任务不自动推进。

状态：**已完成派发范围，待总控复核。**
