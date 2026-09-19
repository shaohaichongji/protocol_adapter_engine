# Lab Binary complete-record Encode G1 实施验证

## 1. 结论与边界

2026-09-19 在 `main@19c524758190dffd2395c87dd10fbe3e8e2d053d` 上，按
`lab-binary-encode-g1-contract.md` 完成 Binary 0.9 complete-record public Host Encode 首片：

- 用真实 `PipelineMessageExecution` 的 Decode/Encode availability 生成各方向 Message selector，
  Encode binding 每次显式携带 Message index，单次用户操作只调用一次 public
  `HostEndpoint::Encode`。
- UINT64、INT64、BOOL、BYTES、ENUM、DECIMAL64 六类输入保持独立 typed 语义；
  Decimal64 使用 `{coefficient, scale}`，不经 `double`。
- 修正 public Decimal64 编辑映射，并将 `NOT_REFERENCED` 保留为独立 Lab-owned
  状态；只有 `CALLER_INPUT` 字段产生可编辑草稿。
- 成功结果展示调用方输入、最终字节及以实际输出长度解析的 public
  byte range/bit mask；constant/computed 标为 PAE 生成，raw 明确标为未观察。
- 未做第二次 Decode，未由 logical 反推 raw，未访问 private Plan，未将布局查询
  冒充完整性或字段实值观察。
- Host 或本地输入拒绝均清理该 Flow 的旧成功结果；Binding/Flow 切换先完成
  inspect/typed draft 的有界拷贝与准入，再原子发布选择状态。

本片没有修改 PAE public API、Core/Plan/Schema、root CMake、stream 或旧 SDK/部署。

## 2. 实际变更

- non-Qt public Binary owner：`tools/protocol_lab_binary/public_binary_decode.{h,cpp}`
  - binding 显式携带 `HostAction`；
  - 新增六类 Lab-owned Encode input 和 Encode 结果 DTO；
  - 从同一 compiled owner 执行 public Host Encode，在回调内有界拷贝最终字节，
    并用实际长度查询 public physical mapping。
- public Binary UI adapter/description/session/tab/model：
  - `tools/protocol_lab_ui/binary_host_adapter_public.{h,cpp}`
  - `tools/protocol_lab_ui/public_binary_description.cpp`
  - `tools/protocol_lab_ui/document_session.{h,cpp}`
  - `tools/protocol_lab_ui/document_tab.cpp`
  - `tools/protocol_lab_ui/field_table_model.cpp`
  - `tools/protocol_lab_ui/owned_presentation_types.h`
  - `tools/protocol_lab_ui/public_legacy_complete_adapter.cpp`
- 针对测试：
  - `tests/protocol_lab_binary/public_binary_decode_tests.cpp`
  - `tests/protocol_lab_ui/binary_public_h2_tests.cpp`

用例覆盖六类 typed 输入与独立期望字节、Decimal64 精度、Enum 越界、
Host representability 失败、失败清旧结果、生成字段只读标注、Decode/Encode binding 混合、
Flow 草稿/结果隔离、拷贝预算拒绝及切换失败不污染源 Flow。总控限定返修又补齐：
本地无效草稿/失败在 binding 往返后仍保持且旧成功不复活、有效 binding 的超量输入和
Host 未进入 codec 的失败均清理旧 owner 结果，以及至少两个 Encode Message 下 selector、
typed draft、结果和 `PreviewKey` 始终保持同一 Message 身份。单向 availability 仍以
public execution metadata 为唯一依据，不从 direction 文本推断。

## 3. 测试优先与仓内验证

实施前先加 Encode API/映射断言，首次构建因 `EncodeInput` 及 Encode 接口未定义
而编译失败，随后实施最小闭环。终态仓内命令使用新根
`out/build/windows-msvc-lab-gui-g1`：

```powershell
cmake --build out/build/windows-msvc-lab-gui-g1 --config <Debug|Release> `
  --target pae_binary_public_h1_tests pae_protocol_lab_ui_binary_public_h2_tests pae_protocol_lab_ui -- /m:1
ctest --test-dir out/build/windows-msvc-lab-gui-g1 -C <Debug|Release> `
  -R "^(pae\.protocol_lab_binary\.public_h1|pae\.tools\.protocol_lab_ui\.binary_public_h2)$" `
  --output-on-failure
```

Debug/Release 均为 `2/2 PASS`，Qt 产品 target 均构建成功；未启动可见窗口。
Release H2 构建明确记录 `/DNDEBUG` 被 `/UNDEBUG` 覆盖，断言未被关闭。

### 3.1 总控限定返修的测试优先证据

返修前先加入“成功 Encode → `SetInvalidDraft` → Encode 失败 → 切到 Decode binding →
切回 Encode binding”回归。修复前定向运行稳定失败于：

```text
H2_ENCODE_LOCAL_FAILURE_ROUNDTRIP_FAIL old success was restored
```

原始失败日志保留在
`out/validation/lab-gui-g1/refinement-logs/pre-fix-debug-ctest.log`。随后才实施最小修正：

- 编辑失效时同时清理 non-Qt owner 的当前 Encode 结果；有效 binding 下无论超量输入，
  还是 Host 在 codec 前拒绝，本次失败都替换旧 current，不再遗留旧成功；
- Encode Flow 显式保存实际 Message index；切换 Message 时同时清空该 Flow 的 typed draft
  和 owner current，恢复结果时拒绝 Message 身份不一致的缓存；
- DocumentSession 按 `(binding, flow)` 保存本地无效草稿与失败诊断，使失败语义在 binding
  往返后保持，同时不恢复旧成功预览。

多 Message 回归使用同一 Pipeline 的两个 Encode Message，选择第二个 Message、写入草稿并
成功得到 `C3 2A`，经 binding 往返后逐项核对 selector、typed draft、结果 Message index 与
`PreviewKey`；另核对 Message 切换不串草稿。non-Qt 回归独立覆盖超量输入和 Host 未进入 codec
两条失败路径的旧结果清理。

修复后仓内 Debug/Release 两项均为 `2/2 PASS`，日志位于：

- `out/validation/lab-gui-g1/refinement-logs/post-fix-debug-ctest.log`
- `out/validation/lab-gui-g1/refinement-logs/post-fix-release-ctest.log`

### 3.2 local cache 原子性与预算返修

总控复核发现 `PublishBinaryHostFlow` 曾先由 `SaveDraftsAndSelect` 发布 adapter 源草稿和目标
selection，再通过 `operator[]` 分配 local cache 并复制无效文本/诊断；后半段失败会留下
adapter/session 不一致。按测试优先新增两条独立回归：

- copy hook 在 local cache 复制点抛出时，修复前 source typed draft 已被部分改写，失败标识为
  `H2_ENCODE_LOCAL_CACHE_COPY_ATOMICITY_FAIL`；证据为
  `out/validation/lab-gui-g1/refinement-logs/local-cache-copy-pre-fix-runtime-debug.log`；
- 无效文本及诊断容量超过 local cache 配额时，修复前仍完成切换，失败标识为
  `H2_ENCODE_LOCAL_CACHE_BUDGET_FAIL`；证据为
  `out/validation/lab-gui-g1/refinement-logs/local-cache-pre-fix-runtime-debug.log`。

最终修正先复制 source local cache 候选，按外层 cache 条目、无效草稿条目、UTF-16 文本
capacity、validation error capacity 及诊断 id/detail capacity 显式计费，并以整个实例已保留
local cache 总量共同接受 `UiViewReserveBytes()/4` 门禁；cache 节点潜在分配和目标诊断副本也在
adapter 发布前完成。只有上述步骤和 `SaveDraftsAndSelect` 都成功后，才用已由
`static_assert` 约束的 nothrow move 提交 local cache、session 选择和诊断。任何复制异常或
超预算均返回 `false`，adapter/session 选择以及源/目标草稿、失败和结果保持不变。

最终仓内 Debug/Release 两项均为 `2/2 PASS`，日志位于：

- `out/validation/lab-gui-g1/refinement-logs/local-cache-post-fix3-debug-ctest.log`
- `out/validation/lab-gui-g1/refinement-logs/local-cache-post-fix3-release-ctest.log`

## 4. installed-SDK static/shared D/R

local cache 原子性与实例总预算返修后的权威终态证据使用再次重新快照的独立根：

- static：`out/validation/lab-gui-g1/static-atomic2-final/`
- shared：`out/validation/lab-gui-g1/shared-atomic2-final/`
- SDK 输入：`out/sdk-clean-checkpoint/candidate1-20260919/` 的
  `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` static/shared Debug/Release 包。

`PrepareStandaloneInputs.ps1` 生成的 static/shared 快照各有 99 个可直接映射白名单文件；
`atomic2-current-whitelist-comparison.json` 均为 `mismatch_count=0`，且无额外 standalone 文件。

| 包形态 | 配置 | Configure/Build | 本片专项 | 隐藏进程模块来源 |
| --- | --- | --- | --- | --- |
| static | Debug | PASS | `2/2 PASS` | Qt 4 模块均从新部署加载，三方哈希一致 |
| static | Release | PASS | `2/2 PASS` | Qt 4 模块均从新部署加载，三方哈希一致 |
| shared | Debug | PASS | `2/2 PASS` | Qt 4 模块 + `pae.dll` 均从新部署加载，三方哈希一致 |
| shared | Release | PASS | `2/2 PASS` | Qt 4 模块 + `pae.dll` 均从新部署加载，三方哈希一致 |

四个捕获 PID 均已终止。证据位于两个终态根的
`logs/atomic2-*-{configure,build,ctest}.log`、`logs/atomic2-*-module-origin.json` 及
`logs/atomic2-current-whitelist-comparison.json`。
Release static/shared 日志均命中 `/UNDEBUG`。shared 仍有既知 C4251；本轮同 MSVC/v142
工具链构建与运行通过不等于 stable ABI 证明。static Debug 的 LNK4099 表示 SDK 未带
PDB，不影响本片链接与专项通过，也不将其表述为完整调试符号交付。

首轮 `static/`、`shared/`，第一次收口的 `static-final/`、`shared-final/`，以及后续
`static-refinement-final/`、`shared-refinement-final/`、`static-atomic-final/`、
`shared-atomic-final/` 均保留为历史证据；它们早于本节最终 local cache 总量计费修正，现已
失效为非权威中间态。本报告只以 `static-atomic2-final/` 和 `shared-atomic2-final/` 作为最终
源码身份与通过结论。

## 5. 局限与未验证

- public `HostOperationResult` 仅向当前 Host 消费层发布 Host/Codec/callback 状态；
  Codec `EncodeResult` 中的 failed field/value/conversion error 未经 Host 结果暴露。本片对
  Lab 可预检失败显示字段与原因，对 Host Codec 失败只展示已公开状态，不伪造
  failed field/value/conversion detail。若产品要求 Host 路径完整显示该三类事实，
  需另立应用无关 public 契约。
- 未做人工 UI 验收；可选后续仅观察：六类输入成功结果，以及一次越界失败后
  旧字节/字段结果被清空。
- 未验证 Linux、异工具链/异 CRT、稳定 ABI、正式 Qt 许可复核、硬件、现场或正式发布。
- 未运行无关全仓回归，未重打 SDK 五包，未覆盖 `deliverables/lab/98df5e0`。

## 6. Git 与停止状态

保留现场既有总控计划变更及未跟踪契约/盘点文档，未 Stage、Commit、Push、
发布或删除。本报告为本轮唯一新增实施验证文档。已达到派发停点，停止写入，等待总控复核。
