# PAE-UI-C1 validation record

Date: 2026-09-10

Baseline: `51db6d2621f4e2ceb1dbaa90cfe046d50228acee`, branch `feat/lab-ui-c1`

This record covers the current uncommitted implementation in the independent UI worktree. It is
not a commit, merge, hardware, field, long-run, or production acceptance record.

## Automated result

- Earlier in this work session, the shared v145 Debug build passed after repairing the internal
  `v06_execution.h` include closure; Compiler/Core/Conformance was 24/24 and Lab
  format/execution/C3 policy was 3/3. These runs preceded the later sidecar test-hook and worker
  lifecycle changes. Their command output was observed in-session, but their `LastTest.log` was
  overwritten by later targeted Release CTest runs; they are historical Debug evidence, not a
  retained final-source Release matrix.
- After the final sidecar failure-injection changes, the targeted Compiler contract and sidecar
  tests passed 2/2 in both v145 Debug and Release. The sidecar executable reports 69/69 checks.
- UI v142 Debug: build and 7/7 UI tests passed.
- UI v142 Release: build and 7/7 UI tests passed.
- After the worker-construction fix, both configurations were rebuilt with exit code 0. Each
  independently saved registration log lists exactly seven UI tests and each independently saved
  result log reports 7/7 passed. The queue test now covers immediate destruction without a request;
  the mailbox test covers start, submit, close, active completion, and joined destruction.
- The race correction is structural: the `std::thread` is started in the constructor body, which
  begins only after every member has completed initialization. The lifecycle tests are regression
  coverage; repeated passing alone is not claimed as proof that a data race is absent.
- The window smoke uses a shown Qt event loop and checks independent expected frame bytes and exact
  Hex highlight masks for the public Schema 0.5, 0.6, and 0.7 fixtures. It drives the model
  programmatically and is not mouse/keyboard delegate acceptance.
- The window smoke also verifies that invalid canonical UINT64 text remains visible with a
  validation error while the previous typed draft and preview stay invalidated.
- The Schema 0.5 window path covers UINT64, INT64, BYTES, ENUM, BOOL, and DECIMAL64 drafts, plus a
  read-only constant.
- No UDP, Loopback, Evidence writer, hardware, or network test was run.

The v142 UI configure command used the Visual Studio 2026 generator, x64, and
`-T v142,version=14.29.30133`. CMake observed `cl` 19.29.30159.0. Qt Core/Gui/Widgets and the
platform plugin report 5.13.0.0. The system VC Runtime observed during runtime inspection was
14.51.36247.0; it is not the compile toolset version.

Final worker-fix logs are retained below the ignored build tree and are not delivery candidates:

- `out/final-ui-v142/logs/pae-ui-c1-worker-final-debug-build.log`
- `out/final-ui-v142/logs/pae-ui-c1-worker-final-debug-registration.log`
- `out/final-ui-v142/logs/pae-ui-c1-worker-final-debug-tests.log`
- `out/final-ui-v142/logs/pae-ui-c1-worker-final-release-build.log`
- `out/final-ui-v142/logs/pae-ui-c1-worker-final-release-registration.log`
- `out/final-ui-v142/logs/pae-ui-c1-worker-final-release-tests.log`

## Performance measurements

Environment: Windows 10 Professional 19045, AMD Ryzen 7 5800H (8 cores/16 logical processors),
15.4 GiB visible memory. Each row is `min / median / p95 / max` in microseconds, after 2 warm-up
iterations and across 10 retained samples. All phase timings use `QElapsedTimer`.
`first_repaint_total` includes the synchronous encode
path through the forced first visible Hex viewport repaint.

### Debug

| Configuration | Phase | min / median / p95 / max (us) |
|---|---|---:|
| Schema 0.5 typed | typed materialization | 69.0 / 71.3 / 92.0 / 92.0 |
| Schema 0.5 typed | Core Encode | 43.3 / 43.9 / 46.3 / 46.3 |
| Schema 0.5 typed | review Decode | 6.2 / 6.4 / 6.8 / 6.8 |
| Schema 0.5 typed | result materialization | 79.0 / 80.4 / 92.6 / 92.6 |
| Schema 0.5 typed | Hex replace | 51.1 / 54.2 / 116.6 / 116.6 |
| Schema 0.5 typed | first repaint total | 31068.6 / 36388.3 / 41659.8 / 41659.8 |
| Schema 0.6 CRC | typed materialization | 23.7 / 26.2 / 35.4 / 35.4 |
| Schema 0.6 CRC | Core Encode | 9.1 / 9.8 / 11.8 / 11.8 |
| Schema 0.6 CRC | review Decode | 2.8 / 3.0 / 6.3 / 6.3 |
| Schema 0.6 CRC | result materialization | 43.1 / 46.2 / 89.8 / 89.8 |
| Schema 0.6 CRC | Hex replace | 48.6 / 53.1 / 121.5 / 121.5 |
| Schema 0.6 CRC | first repaint total | 18166.4 / 23542.0 / 28485.9 / 28485.9 |
| Schema 0.7 length | typed materialization | 29.3 / 29.8 / 51.6 / 51.6 |
| Schema 0.7 length | Core Encode | 9.1 / 9.4 / 13.1 / 13.1 |
| Schema 0.7 length | review Decode | 3.5 / 3.7 / 5.5 / 5.5 |
| Schema 0.7 length | result materialization | 51.7 / 52.4 / 63.2 / 63.2 |
| Schema 0.7 length | Hex replace | 52.1 / 54.9 / 66.5 / 66.5 |
| Schema 0.7 length | first repaint total | 22588.4 / 27790.1 / 31568.5 / 31568.5 |
| Maximum Desktop | typed materialization | 11518.4 / 11755.4 / 11978.4 / 11978.4 |
| Maximum Desktop | Core Encode | 533.5 / 538.6 / 567.5 / 567.5 |
| Maximum Desktop | review Decode | 263.8 / 266.1 / 280.0 / 280.0 |
| Maximum Desktop | result materialization | 12021.6 / 12119.4 / 14444.1 / 14444.1 |
| Maximum Desktop | Hex replace | 41232.9 / 41528.3 / 42538.1 / 42538.1 |
| Maximum Desktop | first repaint total | 532478.9 / 537570.9 / 541783.9 / 541783.9 |

### Release

| Configuration | Phase | min / median / p95 / max (us) |
|---|---|---:|
| Schema 0.5 typed | typed materialization | 5.4 / 7.7 / 37.8 / 37.8 |
| Schema 0.5 typed | Core Encode | 5.5 / 6.6 / 15.3 / 15.3 |
| Schema 0.5 typed | review Decode | 1.0 / 1.4 / 3.5 / 3.5 |
| Schema 0.5 typed | result materialization | 6.6 / 9.3 / 27.4 / 27.4 |
| Schema 0.5 typed | Hex replace | 13.1 / 17.1 / 59.6 / 59.6 |
| Schema 0.5 typed | first repaint total | 2379.5 / 2671.6 / 9035.7 / 9035.7 |
| Schema 0.6 CRC | typed materialization | 1.9 / 2.9 / 5.8 / 5.8 |
| Schema 0.6 CRC | Core Encode | 2.2 / 2.6 / 4.5 / 4.5 |
| Schema 0.6 CRC | review Decode | 0.7 / 0.9 / 2.2 / 2.2 |
| Schema 0.6 CRC | result materialization | 4.3 / 6.4 / 14.7 / 14.7 |
| Schema 0.6 CRC | Hex replace | 10.7 / 17.7 / 37.0 / 37.0 |
| Schema 0.6 CRC | first repaint total | 1410.2 / 1643.0 / 7852.6 / 7852.6 |
| Schema 0.7 length | typed materialization | 2.4 / 5.9 / 22.8 / 22.8 |
| Schema 0.7 length | Core Encode | 1.9 / 3.5 / 8.5 / 8.5 |
| Schema 0.7 length | review Decode | 0.7 / 1.5 / 2.7 / 2.7 |
| Schema 0.7 length | result materialization | 5.2 / 10.0 / 25.7 / 25.7 |
| Schema 0.7 length | Hex replace | 8.3 / 20.3 / 58.9 / 58.9 |
| Schema 0.7 length | first repaint total | 2167.5 / 2608.2 / 7540.5 / 7540.5 |
| Maximum Desktop | typed materialization | 462.7 / 540.5 / 837.1 / 837.1 |
| Maximum Desktop | Core Encode | 106.7 / 112.5 / 160.1 / 160.1 |
| Maximum Desktop | review Decode | 49.4 / 51.4 / 61.9 / 61.9 |
| Maximum Desktop | result materialization | 1009.8 / 1050.2 / 1196.3 / 1196.3 |
| Maximum Desktop | Hex replace | 5547.3 / 5656.5 / 6033.7 / 6033.7 |
| Maximum Desktop | first repaint total | 36125.5 / 36540.9 / 37982.6 / 37982.6 |

The maximum fixture is generated into the build tree from a small deterministic repository script.
It contains the Desktop maximum 65536-byte fixed frame and 2048 fields. No numeric interaction
budget has been approved, so performance acceptance remains `NOT_EVALUATED`; these values are
measurement evidence only.

The worker-only follow-up did not change typed materialization, Encode/Decode, result mapping, Hex,
or repaint timing paths, so the performance matrix was not resampled after that fix.

## Compiler sidecar failure and memory evidence

The internal test probe is thread-local and is not part of the stable Compiler API. Production
execution sees no probe. The final Debug and Release sidecar tests establish:

- an injected failure at the single sidecar storage allocation boundary publishes neither Plan nor
  sidecar and does not cross metadata-copy, Plan-Freeze, or audit stages;
- an injected failure after sidecar and PlanDraft construction but immediately before Plan Freeze
  publishes neither Plan nor sidecar; its tracked live allocation count and accounted bytes return
  to zero, and the released sidecar bytes equal the successful result for the same input;
- the final audit-failure path also releases the tracked sidecar and publishes no partial result;
- the legacy `CompileJsonToPlan` valid and invalid paths report zero sidecar layout, allocation,
  copy, Plan-Freeze gate, audit, live-storage, and release activity.

For the public minimal fixture on the x64 MSVC target, the directly reported values are:

| Component | Evidence type | Bytes |
|---|---|---:|
| persistent sidecar object descriptors/header | measured accounted layout | 376 |
| persistent sidecar copied UTF-8 payload | measured accounted layout | 404 |
| persistent sidecar index/alignment | measured accounted layout | 0 / 0 |
| persistent sidecar total | measured accounted layout | 780 |
| copied metadata simultaneously present in SchemaIr and sidecar | measured copied payload | 404 |
| UI-specific added heap peak above the existing Compiler path | proved upper bound | 780 |
| yyjson parser pool | library-reported upper bound for this input | 31,053 |
| frozen Plan storage | measured Plan accounted total | 1,952 |

The 780-byte UI-added peak is only the new sidecar heap contribution; it is not described as the
whole compile peak. The parser pool belongs to the earlier parse scope and is released before
sidecar construction. During sidecar Build, SchemaIr metadata and the sidecar payload coexist;
after Assemble, PlanDraft metadata and the sidecar coexist; after Plan Freeze, the 1,952-byte Plan
and 780-byte sidecar coexist. Existing SchemaIr/PlanDraft standard-container heap is not a
Plan-arena or sidecar-accounted value, so these numbers are intentionally not summed into an
invented process peak.

The Plan-Freeze test hook is deliberately located before the actual `FreezeBudgetedPlanDraft`
call. It proves the UI compile call boundary's cleanup and no-partial-publication behavior; it is
not evidence of a real allocator failure inside PlanBuilder or Plan Freeze.

The ABI-derived sidecar limits reuse the production layout calculation and the Compiler 2 MiB
decoded-string hard limit:

| Profile | descriptor/header bytes | string bytes | index/alignment bytes | total bytes |
|---|---:|---:|---:|---:|
| Desktop | 1,578,632 | 2,097,152 | 0 / 0 | 3,675,784 |
| Constrained | 132,616 | 2,097,152 | 0 / 0 | 2,229,768 |

The layout is `Header + Protocol + profile maxima for Pipeline/Message/Field/Enum`, with each
segment aligned by the production `AlignUp` calculation, followed by the decoded-string limit.

## Independent deployment and launch

The deploy target creates one directory per configuration below
`out/final-ui-v142/out/protocol_lab_ui`. Each directory contains only the executable, three Qt
Core/Gui/Widgets DLLs, one platform plugin, and four public synthetic configurations. It does not
copy Qt into the source tree.

From the Release deployment directory, run:

```powershell
.\pae_protocol_lab_ui.exe
```

The executable has direct imports only for Qt Core/Gui/Widgets, the VC Runtime/API set, and
Kernel32. It has no direct `ws2_32`, Qt Network, or Qt SerialPort import. Runtime inspection showed
Qt Core/Gui/Widgets and `qwindows.dll` loading from the deployment directory. The copied Qt Core
SHA-256 matched the external Qt input.

After the final worker rebuild, the Release executable is 652,288 bytes with SHA-256
`06A3F9E0096FA0AA4BAE23FC5253A9ED912945A26BF0CB58380448FBA1FE7954`.

## Short manual two-Tab check

Status: Release basic manual offline checks passed on 2026-09-10 through user operation and
controller screenshot review. This is not blanket acceptance of every step below: exact bit-mask
presentation and closing a Tab during active compilation were not demonstrated manually.
The following list is the original planned procedure, with actual evidence distinguished below.

1. Start the Release executable. In the first Tab, select
   `configs/synthetic_ui_v05.pae.json` and click **Load / Reload**. Use **File > Open** to load
   `configs/synthetic_ui_v07.pae.json` in the second Tab. Confirm a third Tab cannot be created.
2. In the Schema 0.5 Tab set: Enabled=false, Mode=Idle, Cross bits=0, MSB bits=0, Count=0,
   Delta=0, Payload=`0000`, Temperature coefficient=0/scale=0. Click **Encode**. Expected bytes:
   `80 08 03 00 00 00 00 00 00 5A`. Selecting Enabled must highlight only frame byte 1 with mask
   `01`.
3. In the Schema 0.7 Tab set Value=0 and Payload=`00`, then **Encode**. Expected bytes:
   `AA 00 06 00 00 55`. Selecting Record length must highlight bytes 1 and 2, each with mask `FF`.
4. Return to Schema 0.5, change Count to invalid `01`, and click elsewhere. The prior Hex/results
   must disappear and Encode must not reuse the old draft. Correct it to `0` and verify Encode
   succeeds again.
5. Reload one Tab while using the other, then close the reloading Tab. Confirm the surviving Tab's
   selection, drafts, result, and highlight remain unchanged and no closed-Tab result appears.

## Actual manual evidence and checkpoint closeout

The user operated the deployed Release application and supplied screenshots in the controller
conversation. No automated mouse/keyboard pass is claimed: the automation screenshot API failed
with `SetIsBorderRequired` / `0x80004002`, and indexed clicks lacked coordinate geometry. Moving
the window to the primary monitor did not resolve that failure. These are automation limitations,
not evidence of a Lab encoding defect.

| Check | Observed result | Conversation attachment identifier |
|---|---|---|
| Schema 0.7 baseline | Value=0, Payload=00; frame `AA 00 06 00 00 55`; raw/logical length=6 | f3f7ac37-91ab-40d1-ace8-b4a2339a19ee |
| Schema 0.7 dynamic values | Value=5, Payload=7E; frame `AA 00 06 05 7E 55`; length bytes 01/02 highlighted | 9be8f06c-12fe-4e1e-84f0-c6c8f409a9aa |
| Schema 0.5 typed inputs | Expected frame `80 08 03 00 00 00 00 00 00 5A`; Enabled byte 01 highlighted | 931b2560-a8b3-48ab-8650-9ef8e3c3c0a5 |
| Invalid Count | Text 01 retained; all raw/logical results and Hex cleared; UI_INPUT_INCOMPLETE shown | cb4ba672-d8ec-443d-b1b0-423c1289ddcb |
| Recovery | Count=0 restores expected frame and results; error cleared | e17e6765-ce6e-4217-b9cf-3be7a617ad36 |
| Reload isolation | After the instructed v05 reload, v07 retains drafts, result, selection and highlight without re-Encode | 90853059-04a0-446a-ac47-699fd22e7e16 |
| Close isolation | Closing v05 leaves v07 and its prior state intact | a4bc7c61-b918-4fe4-854f-c2bdea0c72c2 |
| Two-Tab limit | User reports New/Open disabled with two Tabs; screenshot establishes the two-Tab state, not the open menu | 633c9425-c54c-4480-bbc7-00651b6c7904 |
| Menu recovery | User explicitly reports Open available again after closing one Tab | user text confirmation; no menu screenshot |

Attachment identifiers refer to `codex-clipboard-<identifier>.png` in the conversation, not to
repository files or durable public download links. Screenshots were not copied into the source
repository. Reload order and menu availability additionally rely on user operation/confirmation;
a static screenshot alone does not establish that event sequence.

This closes the basic manual offline walkthrough, not all UI or production acceptance. The
screenshots establish byte highlights, not an independently inspected exact bit mask. The
computed-length field is visibly marked read-only, but an attempted edit was not separately
reported. No active-compile close race, Schema 0.6 manual CRC walkthrough, Debug manual flow,
or performance threshold acceptance is inferred.

Non-blocking usability follow-up: invalid Count `01` is correctly rejected, but the final message
`UI_INPUT_INCOMPLETE: an input field has no valid typed draft` does not identify Count or explain
the leading-zero rule. A future scoped improvement should preserve the specific field and reason;
no product change was made as part of this documentary closeout.

## Remaining boundaries

- Release basic manual mouse/keyboard walkthrough: passed for the actual checks above only.
- Debug manual flow, exact bit-mask presentation and active-compilation close timing: `NOT_EVALUATED`.
- Numeric UI performance budget and pass/fail decision: `NOT_EVALUATED`.
- A process-wide whole-Compiler peak (RSS or general allocator high-water mark) is not measured;
  the bounded parser, sidecar, copied-metadata, and Plan components above are reported separately
  without introducing a general memory-telemetry framework.
- Non-blocking follow-up: `closed_documents` retains every historical document id for the worker's
  lifetime. The bounded two-Tab UI and current ownership invariants remain intact, but a future
  lifecycle design may reclaim closed ids.
- Non-blocking follow-up: the GUI uses a 15 ms result-poll timer even while idle. This checkpoint
  does not replace the polling architecture.
- No Stage, Commit, Push, merge, rebase, or Git configuration change has been performed.
