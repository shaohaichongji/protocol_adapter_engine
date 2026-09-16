# Lab Binary UI Stage 1 Validation

## Scope and dependency boundary

This record covers the first Qt Lab slice for Schema 0.9 Binary Host observation. It does not
claim Encode UI, stream Submit/Continue UI, hardware, deployment, Golden, or field acceptance.

`PAE_BUILD_PROTOCOL_LAB_BINARY_UI` is default-off. In this first slice it explicitly requires the
existing ASCII Host-panel observer build because the Binary view reuses that panel's private Qt
widgets and completion routing. This is an internal **build coupling only**; Binary execution uses
`PreparedBinary`/Binary Host Decode and does not execute through the ASCII protocol adapter. No
implicit option enablement and no public PAE/Core/Host API expansion were introduced.

The slice provides an initially unbound Schema 0.9 document, explicit binding Apply, uniquely owned
Binary preparation, complete-record Decode only, owned typed/raw/range presentation, and per-flow
state. Publication is split into allocating/copying preparation and a move-only Session commit.
Selectors, selector items/container storage, field rows, text, highlights, and Hex/mask capacity are
prepared and accounted before that commit. A controlled post-display/pre-Session failure seam rolls
the flow selector and text back to the prior Session state. This boundary does not claim recovery
from arbitrary Qt-internal or global allocator failure. Hidden flows reuse `PreparedBinary::Current`
and its saved Draft rather than retaining another full `InspectResult`.

Replacement accounting starts from non-Qt `replacement_peak_bytes`, then adds old/new mapped UI
descriptions, retained Qt/config/draft/selector copies, first-Apply unbound description, live input
draft copies, and preparation binding-spec storage. Exact-limit and limit-minus-one cases cover
first Apply and replacement under the 128 MiB/256 MiB ceilings.

Both UI description mapping and observed-result mapping now derive a checked destination-capacity
upper bound before the first destination copy. For the supported Windows/MSVC 14.29 implementation,
the bound covers the 15-byte `std::string` SSO capacity, allocation rounding/geometric growth,
formatted values and concatenation; mapped vectors reserve their final element count before copying.
Tests assert `actual_accounted <= preflight_upper_bound`. Exact-limit and limit-minus-one tests use
the real remaining instance/replacement/UI budget and verify the copy hook is not entered when
preflight fails; final capacity accounting remains a defensive invariant.

The UI-view invariant is:

```text
mapped_view_budget = ui_view_reserve / 2
presentation_retained + active_mapped_view <= mapped_view_budget
presentation_retained + temporary_mapped_view_upper <= mapped_view_budget
```

`FieldTableModel` rows plus `DocumentTab` highlights form `presentation_retained`; the currently
published Session DTO is `active_mapped_view`; `MapCurrent`/`MapObserved` is admitted against the
other view slot before copying while also proving that the candidate can become the next active view.
The shared presentation is physical storage only once; these two inequalities conservatively keep
both lifecycle states within the original two-view reserve. `HexView` frame/mask capacity remains
separately checked against the Hex preview reserve.

## Red test

The first Debug build of `pae_protocol_lab_ui_binary_stage1_tests` failed because the new
`binary_host_adapter.h` interface was missing. This was the expected **missing-new-interface compile
failure**, not evidence that an existing supported behavior was defective.

A later ownership red test reproduced a process crash when the replacement path blindly deleted the
old `QComboBox` model after `setModel`. Qt 5.13 may already destroy the default model during that
operation. The correction tracks the prior model with `QPointer` and deletes it only if it survives
detachment. The final smoke repeats successful Apply and cancelled Apply and asserts that the three
combos retain a stable direct-child model count.

The resource-closure red checks then exposed both remaining gaps. With size-based description
preflight, `AccountDescriptionBytes(description) <= DescriptionCopyRequiredBytes()` failed in the
Debug assertion path. With the old per-temporary UI check restored, the combined-view minus-one test
entered the copy hook and failed its zero-copy assertion; the captured output is
`out/build/windows-msvc-binary-ui-stage1/binary-ui-stage1-combined-view-red.log`.

## Reproducible configure and build

Repository and build directory:

```text
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine
F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-binary-ui-stage1
```

```powershell
$common = @(
  '-G','Visual Studio 18 2026','-A','x64','-T','v142,version=14.29.30133',
  '-DPAE_BUILD_PROTOCOL_LAB_UI=ON','-DPAE_BUILD_PROTOCOL_LAB_BINARY_UI=ON',
  '-DPAE_BUILD_PROTOCOL_LAB_BINARY_MATERIALIZER=ON',
  '-DPAE_BUILD_PROTOCOL_LAB_HOST_OBSERVER=ON',
  '-DPAE_BUILD_PROTOCOL_LAB_ASCII_ADAPTER=ON',
  '-DPAE_BUILD_PROTOCOL_LAB_ASCII_STREAM_OBSERVER=ON',
  '-DPAE_BUILD_HOST_ENDPOINT_SLICE=ON','-DPAE_BUILD_LOADER_SCHEMA_IR_SLICE=ON',
  '-DPAE_BUILD_COMPLETE_RECORD_CODEC_SLICE=ON','-DPAE_BUILD_STREAM_FRAMING_SLICE=ON',
  '-DPAE_ENABLE_SCHEMA_V05_COMPILER=ON','-DPAE_ENABLE_SCHEMA_V06_CRC_COMPILER=ON',
  '-DPAE_ENABLE_SCHEMA_V07_LENGTH_COMPILER=ON',
  '-DPAE_ENABLE_SCHEMA_V08_VARIABLE_COMPILER=ON',
  '-DPAE_ENABLE_SCHEMA_V09_STREAM_FRAMING=ON',
  '-DPAE_ENABLE_SCHEMA_V10_ASCII_TEXT_CODEC=ON',
  '-DPAE_ENABLE_SCHEMA_V11_ASCII_STREAM_FRAMING=ON'
)
cmake -S . -B out/build/windows-msvc-binary-ui-stage1 @common `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON
cmake --build out/build/windows-msvc-binary-ui-stage1 --config Debug -- /m:1
cmake --build out/build/windows-msvc-binary-ui-stage1 --config Release -- /m:1
```

Observed: Visual Studio 18 2026 x64, MSVC v142 directory `14.29.30133`, `cl`
`19.29.30159.0`, and repository Qt 5.13.0 from `third_party/qt`. Configure log:
`out/build/windows-msvc-binary-ui-stage1/binary-ui-stage1-configure.log`.

## Automated validation

The following command passed in both configurations:

```powershell
ctest --test-dir out/build/windows-msvc-binary-ui-stage1 -C <Debug|Release> `
  -R "^(pae\.protocol_lab_binary\.|pae\.tools\.protocol_lab_ascii|pae\.tools\.protocol_lab_ui)" `
  --output-on-failure
```

- Debug: 28/28 passed; `binary-ui-stage1-debug-tests.log`.
- Release: 28/28 passed; `binary-ui-stage1-release-tests.log`.

The focused headless/Qt coverage includes all stale identity dimensions (document, load, session,
request, config and Decode-only backend), success followed by protocol failure, local input failure
without a Host call, per-flow restore without a second Decode, flow-switch allocation failure,
publication copy failure, real Hex UI-capacity preparation failure, exact/minus-one first and
replacement budgets, cancelled replacement/reload/close, confirmed reload failure without old-owner
restoration, and actual two-Binary-Tab close preflight Yes then No with both states unchanged.
It also covers description/result capacity-upper-bound admission, `actual <= upper`, exact and
minus-one pre-copy rejection with zero copy-hook calls, two flows retaining results while
active+temporary+presentation coexist, combined-view rejection with the old flow and Qt/Session
signature unchanged, and stable selector-model ownership across successful replacement and
cancellation.

## Gate and test-switch evidence

```powershell
cmake -S . -B out/build/windows-msvc-binary-ui-stage1/gate-missing-deps `
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" `
  -DPAE_BUILD_PROTOCOL_LAB_BINARY_UI=ON

cmake -S . -B out/build/windows-msvc-binary-ui-stage1/gate-build-testing-off @common `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=ON
ctest --test-dir out/build/windows-msvc-binary-ui-stage1/gate-build-testing-off -C Release -N

cmake -S . -B out/build/windows-msvc-binary-ui-stage1/gate-pae-testing-off @common `
  -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=OFF
ctest --test-dir out/build/windows-msvc-binary-ui-stage1/gate-pae-testing-off -C Release -N
cmake --build out/build/windows-msvc-binary-ui-stage1/gate-pae-testing-off `
  --config Release --target pae_protocol_lab_ui -- /m:1
```

- Missing explicit dependencies: configure failed with the intended Binary UI diagnostic.
- `BUILD_TESTING=OFF, PAE_BUILD_TESTING=ON`: 61 tests registered.
- `BUILD_TESTING=ON, PAE_BUILD_TESTING=OFF`: 0 tests registered, and the Release UI target built
  successfully (this is an actual build, not configure-only evidence).
- Logs are the `binary-ui-stage1-gate-*.log` files in the main build directory.

## Manual UI acceptance checklist (execution status below)

Executable:
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\build\windows-msvc-binary-ui-stage1\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe`

Launch from this deployed directory, which contains Qt5Core/Gui/Widgets DLLs and
`platforms/qwindows.dll`. The `bin/Release` EXE alone is not a standalone launch package.

Configuration:
`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\tests\protocol_lab_ui\fixtures\synthetic_binary_ui_stage1.pae.json`

1. Open the JSON and verify Schema 0.9 starts unbound: Inspect disabled and Encode unavailable.
2. In Host binding draft, keep `device / Decode / ui_pipeline`, click Apply, and verify Session
   becomes active without communication or Evidence output.
3. Inspect `80 0D 03 00 01 00 CA FE 05 5A`; verify 9 fields, BOOL raw `未单独提供`, BYTES raw
   `CAFE`, and converted value raw/logical `5` / `5@0`, with byte/bit highlighting.
4. Inspect `C3 07`; verify the prior successful table is cleared and only failure diagnostic bytes
   remain. Then restore the valid input.
5. Switch Flow 0 to Flow 1 and back; verify Flow 1 starts empty and Flow 0 restores its draft/result.
6. Re-Apply and answer Yes; verify the old result is cleared and selector models are replaced without
   accumulation. Restore the valid input, re-Apply and answer No; verify owner identity, selection,
   draft, result, and selector model count do not change.
7. Reload and answer No, then close and answer No; verify state remains. With two dirty Binary tabs,
   close the window, answer Yes for the first and No for the second, and verify neither tab changed.
8. Confirm a reload to an invalid JSON; verify CONFIG_ERROR and verify the discarded old owner is
   not restored.

## Final artifacts

| Configuration | Artifact | SHA256 |
| --- | --- | --- |
| Debug | `out/build/windows-msvc-binary-ui-stage1/bin/Debug/pae_protocol_lab_ui.exe` | `4CE530BC8D206B1FA4C81B22A61CF04AA973CB928F63D34427CDC2465238E6AF` |
| Release | `out/build/windows-msvc-binary-ui-stage1/bin/Release/pae_protocol_lab_ui.exe` | `32D216B58118B1368117630BCCA711DB71E482B8D0D68220CA65B38B1ABC8798` |
| testing-off Release | `out/build/windows-msvc-binary-ui-stage1/gate-pae-testing-off/bin/Release/pae_protocol_lab_ui.exe` | `76680CAB65139D8901C8A772790FB7A3EF6F10FAE4AEDC8AA316B3BF3816B47C` |
| Fixture | `tests/protocol_lab_ui/fixtures/synthetic_binary_ui_stage1.pae.json` | `A4524F749EEF7E4B9E727A3AB25C8CB7F0A14CF2DAB4A2B2117FC39FA1BA8E5D` |

## 2026-09-14 总控内部试用收尾

用户决定以“先基本可用、再通过真实使用与项目集成迭代”为下一阶段目标，停止逐项截图补验，
并已确认 Lab 关闭。本检查点可进入限定内部试用，不等于全部契约验收、稳定 SDK 或生产验收完成。

- 已有人工截图/用户确认覆盖：部署目录启动、配置加载及显式 Apply、正常完整记录 Decode、
  9 字段与 raw/logical 展示、payload/cross_bits 字节定位、协议失败清除旧结果、非法 Hex、
  Flow 独立输入/结果恢复、取消 Apply/Reload/Tab 关闭及 Apply 后重新 Decode。
- 两 Tab 窗口关闭 Yes → No 后，两个 Tab 均保留；由于操作前两 Tab 的不同 count 基线未确认，
  不宣称人工证明了各自数据不变，也不据此认定串扰。此项自动回归证据仍独立保留。
- 确认 Reload 到无效配置的最后人工步骤未执行；不再阻塞本轮试用，已有自动回归不冒充人工证据。
- 展示待办：temperature Type 显示 INT64，而目标逻辑类型为 DECIMAL64；Decode 的 Source
  显示 input/constant、Value 编辑式外观需核对；初始未绑定 Mode 显示 Encode 需核对。
  本轮只登记截图观察与疑点，未修复，也不据展示疑点推断 Core 计算错误。
- 已现场复核历史 Debug/Release 日志各 28/28、部署 EXE SHA256 与上表 Release 一致，
  Qt DLL/platform plugin 存在且 Lab 进程已退出。本次文档收尾没有重建或重跑测试。
- 下一阶段先讨论真实协议/项目的最小集成闭环，暂停无实际需求的横向扩展；明确的错误结果、
  崩溃或状态串扰仍须处理，不因试用标准而豁免。

No hardware, Golden, field, Linux, or general deployment qualification is claimed.
No Stage, Commit, or Push was performed.
