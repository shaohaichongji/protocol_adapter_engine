# Lab ASCII 0.10 UI / 显式 Host 公开接线 A2 验证记录

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

日期：2026-09-18。状态：**A2 限定收口，用户三组人工烟测通过**。本记录仅覆盖 Schema 0.10 完整记录 ASCII 的首次打开、直接 Decode/Encode、显式 Host Apply 与 Qt UI 生命周期公开接线；不表示 Schema 0.11 stream 已迁移、Binary H2 被替换、整个 Qt Lab 已包外 public-only 消费，或历史 Qt 访问违例已经解决。未 Stage、Commit、Push、发布或删除。

### 总控复核与用户人工反馈

总控核对成功回调物化修正、资源返修对应源码，并独立复算 budget-review-final D/R 日志及部署 EXE 哈希与本报告一致；两组最终日志各 12 项通过，git diff --check 通过。总控未重新构建或运行测试，不将此限定复核描述为全仓审计。

用户明确反馈“A2 三组烟测通过，Lab 已关闭”：覆盖直接 Inspect/Encode、显式 Host 双 Flow/失败恢复/Encode，以及带状态 Apply 取消与接受替换。期间在 Hex 草稿内先输入 ASCII 文本导致格式切换被拒绝，截图提示与保留草稿契约一致；指导清空后先切 ASCII (escaped) 再输入，不作为代码修复或崩溃证据。人工结果来源为用户反馈，不是总控自动 UI 操作。下方历史“待总控复核”停点保留为交付过程。

### 2026-09-18 总控限定资源边界返修

总控复核指出初版 A2 Host 组合层的资源边界未闭合：Decode 入口和失败诊断复制没有完整沿用 A1 限额；Host 创建只在 A1 owner 阶段检查 instance/replacement，后加入的公开 Host 及本地 binding/channel/endpoint retained storage 没有纳入最终拒绝；Encode 也在输入数量/字段长度前检之前分配 values。返修先增加独立边界断言，修复前 Debug 明确失败在“最终 Host `accounted-1` 仍创建成功”，证据：

- `out/stage4-ascii-public-a2/budget-review-before-debug-LastTest.log`
- SHA-256：`432AEF8F17CD64EA4573A00F841255631EBA9D97BB7FF87A6620AF6C9374B1D9`

最小修正如下：

- Decode 在调用 Host 前拒绝 `null + nonzero` 和超过 `max_frame_bytes` 的输入；失败 Candidate 与无回调 fallback 的诊断复制统一先计算 `sizeof(OperationResult) + frame.size`，超过 `max_result_bytes` 时不复制，成功复制时填写 `accounted_bytes`，异常时清空诊断且不发布半成品。
- Host 创建在任何 `specs.reserve` 前先验证 compiled/binding 数、endpoint 长度、action、Pipeline 和 channel 聚合上限；以饱和运算计入 `HostAdapter` facade、bindings 显式 capacity、endpoint 逻辑字节、外层 channel-vector storage 和各 binding 的 channel capacity。A1 owner 创建后先做 owner+local 下界检查，公开 Host 与本地 channel 建成后再按最终 `A1 + HostMemoryReport + local retained` 对 instance/replacement 原子拒绝。该数字是逻辑准入，不是 allocator 或 RSS 硬上限。
- Encode 在 `values.reserve` 前拒绝超过 `max_fields` 的输入，并逐字段拒绝超过 `max_field_bytes` 的 bytes；成功物化按预检所得 `field_count` 显式 reserve 结果字段容量。
- Testing-only callback allocation failure 覆盖 Decode/Encode 成功 Codec 后的本地 `bad_alloc`；结果保留 `codec_called=true`、真实 Codec status/Message 身份，同时不发布 frame/fields。该钩子不进入 Testing-off 产品构建。

新增专项独立覆盖：最终 Host instance exact/minus-one、replacement exact/minus-one、长短 endpoint 的 Host+本地双份身份逻辑字节、超 frame、非法 `ByteView`、失败诊断 exact/minus-one、Encode 数量/字段长度前检，以及 callback `bad_alloc` 事实保留。格式化后最终 D/R 受影响回归仍各 12/12；**当前源码的最终证据改为**：

- `out/stage4-ascii-public-a2/budget-review-final-debug-LastTest.log`，SHA-256 `4149C4AB4071B813C6EDB549351950A843C4729661ED417A27195145C119E8D9`
- `out/stage4-ascii-public-a2/budget-review-final-release-LastTest.log`，SHA-256 `3D83DED312D32447F78AB5F83D2531D9AB6DC2B12CAE18909422652F9AEA7E73`

旧 `final-{debug,release}-LastTest.log` 保留为本次资源返修前的历史通过证据，不再作为当前源码最终证据。A1 offline helper 本次没有变化；A1 D/R 随上述 12 项重跑，既有 static 包外 D/R 不重复执行，也未重打 SDK。

## 1. 基线、边界与实现

- 仓库：`<REPO_ROOT>`；基线为 `main@481d51ae2d62e2e28754fd84e5df4bb64309a1e2`。共享工作树中的总控文档、SDK、A1 及其他既有修改全部保留，暂存区保持为空。
- 根 CMake 新增默认 OFF 的 `PAE_BUILD_PROTOCOL_LAB_ASCII_PUBLIC_A2`，显式要求 UI、A1、public API Stage 1 和 Host endpoint。缺少 Host endpoint 的独立 Configure 按预期以 `ASCII public A2 requires UI, public A1, public API Stage 1 and Host endpoint.` 拒绝，没有静默回退私有实现。
- `schema_dispatch` 对顶层 `schema_version=0.10` 严格路由 `ASCII_PUBLIC`；0.11 仍走 `PRIVATE_ASCII`。`CompileWorker` 每个请求只调用所选目标编译器一次，保留原有 document/revision ticket、取消与过期结果拒绝语义。
- 首次打开将一次公开编译所得 `CompiledProtocol` 交给 A1 `Adapter`，同一 owner 提供描述、直接 Decode/Encode 和结果 DTO；不重解析 Schema、不额外 Decode 获取展示事实。
- 新 `public_ascii_host_adapter.*` 在同一个 A1 owner 上创建公开 `HostEndpoint`，Decode 使用两个 Flow、Encode 使用一个 channel，并保留调用期 Message selector。Host 回调只在同步借用期限内完成预算预检和自有复制；失败不发布半成品或旧成功字段。
- 新 `ascii_host_adapter.*` 只作为 UI runtime facade：0.10 使用公开 Host，0.11 转发既有私有 stream observer。`DocumentSession` 的直接执行和显式 Host 执行统一映射为现有 UI DTO；`DocumentTab` 先准备候选再发布，创建失败、过期、用户取消均保留旧 owner、绑定、草稿和结果，成功替换才推进 generation 并清空被替换状态。
- A1 只增加同一 immutable owner/limits 的内部组合访问和 friend，不新增 Qt 或私有 PAE 依赖；A1 预算返修及其独立单向/混合 capacity 断言保持不变。

## 2. Host complete-record Decode 失败与修正

续接时保存的 Release 失败现场为：

- `out/stage4-ascii-public-a2/pre-fix-release-LastTest.log`
- SHA-256：`64688AB8278C55FA8533D90BB7C9157EFD89E3057CAA479C54722AAD0F2DD21F`

该日志同时出现 `ascii_public_a2` 在发布 Host 后的 `first.Inspect()` 断言失败，以及 `qt_smoke_ascii` 的 `UI_SMOKE_FAIL detail=Host observer: Host complete record Decode`。这两项属于同源 A2 接线失败，不归入历史 Qt 访问违例。

源码核对确认公开 Host/Codec 已形成成功输出，缺口在 A2 回调结果物化：Candidate observer 为避免成功候选重复复制而只处理失败；当时 business sink 又把成功 `HostOutputView` 转回 Candidate observer，成功分支被直接忽略，`DecodeContext::called` 未置位，最终被上层解释为无可发布结果。最小修正为：

1. Candidate observer 继续只复制失败候选；
2. business sink 使用同次 `HostOutputView` 构造同步借用视图，直接调用 `CopyDecode` 完成一次有界自有复制，并设置 `called`；
3. 不增加第二次 Decode、不改变公开 Host/Codec、Schema 或失败状态契约。

`ascii_public_a2_tests.cpp` 增加三层独立断言，分别固定：公开 `HostAdapter::Decode` 原始复制结果、UI `AsciiHostAdapter::Inspect` DTO 转换结果、`DocumentSession` 发布 Host 后的 Inspect/失败恢复。这样后续失败可区分 Host/物化、facade 转换和 session 接线，而不是只观察最终 Qt 文本。

## 3. Windows x64 Debug / Release 验证

主构建目录：

`<REPO_ROOT>\out\build\windows-msvc-stage4-ascii-public-a2`

现场 Cache：Visual Studio 18 2026、x64、MSVC v142 `14.29.30133`、随仓 Qt `5.13.0`；A2、A1、Binary H2、ASCII 0.11 stream observer 和 Testing 均为 ON。Release 编译记录显示 `/UNDEBUG` 覆盖 `/DNDEBUG`，专项断言未被关闭。

格式化后最终 D/R 依次构建 UI、A2 专项、A1 专项及受影响 headless 回归；随后运行：

```powershell
ctest --test-dir out/build/windows-msvc-stage4-ascii-public-a2 -C Debug -R '^pae\.tools\.(protocol_lab_ascii\.public_a1|protocol_lab_ui\.(schema_dispatch|ascii_session|ascii_stream_session|host_session|binary_public_h2|ascii_public_a2|qt_smoke_ascii|qt_smoke_ascii_one_way|qt_smoke_ascii_stream|qt_smoke_ascii_stream_close_cancel|qt_smoke_binary_stage1))$' --output-on-failure
ctest --test-dir out/build/windows-msvc-stage4-ascii-public-a2 -C Release -R '^pae\.tools\.(protocol_lab_ascii\.public_a1|protocol_lab_ui\.(schema_dispatch|ascii_session|ascii_stream_session|host_session|binary_public_h2|ascii_public_a2|qt_smoke_ascii|qt_smoke_ascii_one_way|qt_smoke_ascii_stream|qt_smoke_ascii_stream_close_cancel|qt_smoke_binary_stage1))$' --output-on-failure
```

最终 Debug、Release 均 **12/12 PASS**。覆盖：严格 dispatch/单次编译；0.10 首次打开直接 Decode/Encode；公开 Host 三层 Decode 物化；Encode；失败后恢复；Apply 无效候选/取消/成功替换；双 Flow 与双 Tab 草稿/结果隔离；单向/literal-only；A1；Binary H2；0.11 stream 及 close-cancel。资源返修后的最终日志见本文开头 `budget-review-final-*`；下列日志是 Decode sink 首轮修复完成、资源返修之前的历史证据：

- `out/stage4-ascii-public-a2/final-debug-LastTest.log`，SHA-256 `0094E9850A5BD1489F7C05EEA2AFD78BB49E1FF0133788D7CC9B5F02FF55B04D`
- `out/stage4-ascii-public-a2/final-release-LastTest.log`，SHA-256 `C7A2FAD62E6036DDE6AA0E04580D5908957F894D2D057C83448994A85D0E2835`

A1 helper 变化后的 A1 D/R 已包含在上述各 12 项中；candidate1 static 包外 consumer D/R 也以当前 A1 源码重新构建运行，均输出 `PUBLIC_ASCII_A1_PACKAGE_CONSUMER_PASS`，未重打 SDK 五包。对应保留证据为 `out/stage4-ascii-public-a1/budget-fix-sdk-static-{Debug,Release}-{build,run}.log`。

Testing-off 构建目录 `out/build/windows-msvc-stage4-ascii-public-a2-testing-off` 以 A2 ON、`PAE_BUILD_TESTING=OFF` 成功构建 Release UI，`ctest -N` 为 `Total Tests: 0`。新 A2 C++/头文件及受影响测试以 VS bundled clang-format 22.1.3 执行 `--dry-run --Werror` 通过；public ASCII helper 对 Qt/private PAE 头的禁止模式检索无命中。`git diff --check` 在交付前另行复核。

## 4. Release 候选与人工烟测建议

本片仅在 A2 独立构建目录生成候选，没有覆盖已验收 H2 目录：

- 构建 EXE：`<REPO_ROOT>\out\build\windows-msvc-stage4-ascii-public-a2\bin\Release\pae_protocol_lab_ui.exe`
- 可运行部署 EXE：`<REPO_ROOT>\out\build\windows-msvc-stage4-ascii-public-a2\out\protocol_lab_ui\Release\pae_protocol_lab_ui.exe`
- 配置目录：`<REPO_ROOT>\out\build\windows-msvc-stage4-ascii-public-a2\out\protocol_lab_ui\Release\configs`
- 两个 EXE 均为 1,390,080 bytes，SHA-256 均为 `D38F25BD634D01011EB785CDCD59D76E5C98470A31F3CC0633C914923DEA3803`。

总控复核后如需人工烟测，建议最多三组：

1. 打开 `synthetic_ascii_text_slice.pae.json`，直接 Inspect `RX ALICE!OK\r\n`，再 Encode `ALICE` / `Z`，核对 Message、字段值和实际范围。
2. Apply 同配置显式 Host，在两个 Decode Flow 分别保留不同草稿/结果；制造一次失败后恢复，再在 Encode binding 核对调用期 Message 与输出。
3. 带现有 Host 草稿/结果再次 Apply，先取消确认旧 generation/草稿/结果仍在，再接受替换并确认新 generation 且旧状态清空。

## 5. 文件范围、未验证项与停点

本片实际代码/测试/CMake 范围为：

- 根及局部 CMake：`CMakeLists.txt`、`tools/protocol_lab_ascii/CMakeLists.txt`、`tools/protocol_lab_ui/CMakeLists.txt`、`tests/protocol_lab_ui/CMakeLists.txt`
- public ASCII helper：`tools/protocol_lab_ascii/public_ascii_offline_adapter.h`、`public_ascii_host_adapter.{h,cpp}`
- UI 接线：`tools/protocol_lab_ui/ascii_host_adapter.{h,cpp}`、`compile_worker.*`、`description_mapping.*`、`document_session.*`、`document_tab.cpp`、`schema_dispatch.*`
- 测试：`tests/protocol_lab_ui/ascii_public_a2_tests.cpp` 及受影响的 dispatch/session/Host/Binary H2 兼容测试
- 本验证记录与 `out/stage4-ascii-public-a2/` 证据

未修改 PAE 公共 API、Core、Plan、Schema、旧私有桥执行算法、SDK 白名单或 0.11 stream 行为；未重打 SDK、未覆盖旧 H2 部署。未执行全仓矩阵、动态/源码包消费、Linux、真实协议 Golden、硬件、现场或用户人工 UI 验收；自动 Qt smoke 不替代人工生命周期验收，历史 Qt 访问违例仍独立未解决。

本片停点为：0.10 首次打开、直接执行、显式 Host Apply/UI 生命周期及受影响 D/R 回归已完成；**已完成派发范围，待总控复核**。现停止写入。
