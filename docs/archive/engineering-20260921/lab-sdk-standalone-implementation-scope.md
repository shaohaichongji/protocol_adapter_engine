# 整个 Qt Lab installed-SDK 独立构建实施范围核对

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 1. 结论与证据边界

本轮于 2026-09-19 在 `main@c5b369298c79f27cf71dd2f73be9fbb4a4203a31` 的实时工作树上复核。工作树已有总控、SDK 和前序 Lab 切片的未提交变更；本报告不覆盖这些现场。

1. Schema 0.5–0.8 complete-record 已迁到 `PublicLegacyCompleteAdapter`，0.9 已是 public Decode-only H2，0.10/0.11 正常路径已是 public A2/stream；ASCII public façade 与 private compatibility target 也已分开。本片不需要新增 PAE public API，也不应重编译 PAE private 源。
2. 整个 Qt Lab 仍不能直接成为 installed-SDK consumer：`compile_worker` 仍公开 private compiler payload；`document_session` 仍直接持有 C1/private ASCII owner；`description_mapping` 仍混合 private Plan/sidecar 映射；`document_tab` 仍使用 private observer/diagnostic/Host 类型；当前 headless target 还无条件链接 private C1/Compiler/Core/Plan/yyjson targets。
3. 下一实施片只应做两类改动：隔离上述声明、类型和 private compatibility；增加 Lab-owned standalone CMake，仅从安装包 `find_package(PAE CONFIG REQUIRED)` 取得 `PAE::pae`。不得改变执行语义或裁掉功能来证明闭包。
4. 验证顺序固定为 static Debug/Release 先闭合、总控复核后再 shared Debug/Release。shared 后片须额外闭合 `pae.dll` 部署和加载来源。
5. 本报告是实施范围和停点，不是已经完成的独立构建、SDK/Qt/Lab 发布证据。

## 2. 必须保留的功能矩阵

| Schema/范围 | 必须保留 | installed-SDK 路由 | 不得扩大 |
| --- | --- | --- | --- |
| 0.5 | complete Decode/Encode、multi-message、typed draft、Encode 后独立 review、物理高亮 | 单次 `CompileProtocolJson` + `PublicLegacyCompleteAdapter` + public Codec | 不新增 Host/stream |
| 0.6 | 0.5 全部能力 + CRC/integrity 成功和失败展示 | 同上 | 不恢复 private Core |
| 0.7 | 0.6 全部能力 + computed length 展示/检查 | 同上 | 不伪造 computed input |
| 0.8 | 变长 BYTES、actual range/integrity location、Decimal64、有界输出和 review | 同上 | 不放宽资源预算 |
| 0.9 | neutral description、显式 Apply、binding/Flow 草稿隔离、complete Decode、physical result | public H1 + public H2 + `PAE::pae` | 保持 Decode-only，不提升历史非 Qt Encode/stream |
| 0.10 | complete Decode/Encode、direct/explicit Host、one-way/literal-only、失败清旧结果 | public ASCII offline/Host + Lab-owned façade | 不带入 private ASCII adapter |
| 0.11 | complete/stream 接线、mixed Pipeline、chunk/sticky、Submit/Continue/Reset、STOP/frozen suffix、Flow 隔离、关闭/取消 | public ASCII offline/Host + public framing/Host | 不改 framing/Host 契约 |
| 通用 UI | 单次编译、无 private fallback、加载/重载/关闭、多 tab、mailbox ownership、HEX/ASCII 输入、类型编辑、错误清理、Qt smoke | Lab-owned worker/session/DTO + PAE + Qt | 不改变人工操作步骤 |

CLI `pae_protocol_lab`、UDP/Replay/Evidence 和 private Binary materializer 不属于本次“整个 Qt Lab”范围。

## 3. 产品源闭包白名单

### 3.1 可直接进入 standalone product target

- `tools/protocol_lab_binary/public_binary_decode.{h,cpp}`
- `tools/protocol_lab_ascii/public_ascii_offline_adapter.{h,cpp}`、`public_ascii_host_adapter.{h,cpp}`
- `tools/protocol_lab_ui/ascii_host_types.{h,cpp}`、`ascii_host_adapter.{h,cpp}`
- `tools/protocol_lab_ui/binary_host_adapter_public.{h,cpp}`、`public_binary_description.{h,cpp}`
- `tools/protocol_lab_ui/public_legacy_complete_adapter.{h,cpp}`
- `tools/protocol_lab_ui/canonical_input.{h,cpp}`、`ascii_escaped_input.{h,cpp}`、`inspect_hex_input.{h,cpp}`
- `tools/protocol_lab_ui/owned_presentation_types.h`、`ui_field_result.h`、`ui_physical_types.h`
- `tools/protocol_lab_ui/exact_value_delegate.{h,cpp}`、`field_table_model.{h,cpp}`、`hex_view.{h,cpp}`
- `tools/protocol_lab_ui/application_window.{h,cpp}`、`main.cpp`
- `tools/protocol_lab_ui/smoke_editor_target.h`，仅供测试/smoke 编译。
- `tools/protocol_lab/sha256.{h,cpp}`，仅保留 worker 的配置身份计算；不得复制该目录其他 CLI/private 源。

### 3.2 完成隔离后才能进入 product target

| 文件 | 当前残留 | 最小实施要求 |
| --- | --- | --- |
| `compile_worker.{h,cpp}` | `CompiledUiArtifacts`/private diagnostic 和 public route 后的 private compile fallback | completion 改为 Lab-owned diagnostic + public `CompiledProtocol` payload；standalone 只允许一次 public compile；private payload/compile 移到 dev-only compatibility |
| `document_session.{h,cpp}` | 直接 include/hold `v06::ExecutionBridge`、private ASCII owner/Host，签名使用 private observer/result | 产品 session 只持三组 public adapter；private owner/mapping/conversion 移到 dev-only compatibility；timing callback 改为 Lab-owned 接口 |
| `document_tab.{h,cpp}` | private `TimingObserver`、file-load diagnostic、ASCII Host candidate/预算常量 | 改用 Lab-owned phase/diagnostic/Host façade；explicit Host 仅调 public `AsciiHostAdapter`；不以关闭 UI 规避 |
| `description_mapping.{h,cpp}` | 混合 private Plan/sidecar、private ASCII 与 public ASCII mapping，连带污染 `hex_view` | 拆出 `public_description_mapping.{h,cpp}`，承载 public ASCII mapping 和 physical formatting/range helper；现有 private overload 只留 dev compatibility |
| `schema_dispatch.{h,cpp}` | dispatch-only 语义正确，但 `.cpp` 直接 include `<yyjson.h>` | 保留顶层版本、重复键/类型/边界拒绝和 no-fallback；依第 6 节由 Lab 显式消费 yyjson |

允许按实际最小需要新增：`public_description_mapping.{h,cpp}`、`compile_worker_private_compat.{h,cpp}`、`document_session_private_compat.{h,cpp}`、`document_tab_private_compat.{h,cpp}`、`lab_execution_observer.h`。若无需某文件则不创建；不得复制整套 worker/session 状态机。

### 3.3 明确排除的 product 源/target

- `tools/protocol_lab/v06_execution.*` 及其 C1/Core/Plan 闭包。
- `tools/protocol_lab_ascii/ascii_offline_adapter.*`、`host_observer_adapter.*`。
- `tools/protocol_lab_ui/ascii_host_types_compat.*`、`ascii_host_adapter_compat.*`。
- private `binary_host_adapter.cpp` 与 `candidate_materializer/owned_description/binary_admission/prepared_binary/resource_budget` 闭包。
- `pae_protocol_lab_c1_execution_internal`、`pae_config_compiler`、`pae_protocol_core_slice`、`pae::protocol_plan`、`pae_host_endpoint`、`pae::protocol_framing`、`pae::yyjson` 等开发树 private target。standalone 只链接 `PAE::pae` 和 Lab targets。
- `ascii_smoke_diagnostic.h` 不进入 Testing-off 产品。

private compatibility 可继续服务开发树旧组合，但须由独立 target 显式启用，不能通过 transitive `PUBLIC` link 泄漏到 standalone。

## 4. 下一实施片允许改写范围

1. 第 3.2 节现有文件和允许新增的隔离文件。
2. `tools/protocol_lab_ui/CMakeLists.txt`、`tools/protocol_lab_ascii/CMakeLists.txt`、`tools/protocol_lab_binary/CMakeLists.txt`，仅用于收紧 public/private target 闭包并保持开发树兼容。
3. 新增 `tools/protocol_lab_ui/standalone/CMakeLists.txt`、`standalone/cmake/PaeLabQt513.cmake`、`standalone/cmake/DeployPaeLab.cmake`，作为不依赖仓库顶层 CMake 的入口。
4. `tests/protocol_lab_ui/CMakeLists.txt` 与第 8 节现有测试；必要时可在 `tests/protocol_lab_ui/standalone/` 增加只验证包外来源门禁的小测试，不复制业务回归。
5. `cmake/GenerateProtocolLabUiMaxFixture.cmake`、`cmake/PaeQt513.cmake`、`cmake/DeployProtocolLabUi.cmake` 仅允许抽取/参数化可复用逻辑；不改变旧部署默认位置。
6. 经当轮授权后新增 `docs/engineering/lab-sdk-standalone-validation.md` 及必要索引/计划状态。

默认禁止修改 `include/pae/**`、`src/**`、SDK 打包脚本、既有 SDK 候选、`third_party/qt` 和 `third_party/yyjson` 内容。若发现真实 public 缺口，停止并报告，不自行扩 PAE 契约。

## 5. standalone 输入和 CMake 边界

standalone 必须是可在仓库外运行的顶层 CMake project，不得 `add_subdirectory(protocol_adapter_engine)` 或通过 PAE 内部 options 间接构建。只接受：

- `PAE_LAB_SOURCE_ROOT`：第 3 节白名单源/测试根；
- `PAE_SDK_ROOT`：单个安装 SDK 包根；
- `PAE_QT_ROOT`：固定 Qt 5.13.0 输入根；
- `PAE_LAB_CONFIG_ROOT`：单一 Lab-owned 配置白名单根；
- `PAE_LAB_DEPENDENCY_ROOT`：仅在 yyjson 方案获批后启用；
- `PAE_LAB_DEPLOY_ROOT`：新的、有界部署输出根。

SDK 只可这样查找：

```cmake
find_package(PAE CONFIG REQUIRED PATHS "${PAE_SDK_ROOT}" NO_DEFAULT_PATH)
```

只 include 安装包 `include/pae/**`，只链接 `PAE::pae`；不点名 `.lib`，不从 package registry、其他 `CMAKE_PREFIX_PATH`、开发 build tree 或全局 `PATH` fallback。

### 5.1 实时确认的 SDK 候选

当前允许作下一片本地验证输入的根仅为 `out/sdk-public-stream-description/candidate1-20260918/`，含 `pae-sdk-source`、static D/R、shared D/R 五包。static Debug 为 `/MDd`，Release 为 `/MD`。

五包 `PROVENANCE.json` 均记录 `source_head=dbf4798f96a45b8d36a4754ad5a01e274680337e`、`source_worktree_dirty=true`，说明为指定 revision 加保留的 unstaged/untracked 变更。它是已知本地候选，不是当前 `c5b3692` 干净重打包，也不得标为 clean release/stable ABI。本片不重打五包。

建议新仓库外验证根为 `F:/PersonalWorkspace/pae-lab-sdk-standalone-candidate1-20260919/`，分 `inputs/`、四个 D/R static/shared build 目录、`deploy/`、`logs/`。本轮未创建；实施时先确认目标不存在，不覆盖旧 consumer root。

### 5.2 Qt 与配置输入

Qt 只显式来自 `third_party/qt` 固定副本；实时头文件为 Qt 5.13.0，D/R 均有 Core/Gui/Widgets import libraries、DLL 和 `qwindows[d].dll`。不得搜索本机 Qt 5.11、DEI 原路径或系统 Qt。

配置白名单应由现有 `synthetic_ui_v05/v06/v07/v08`、`synthetic_ui_inspect_multi_v05`、`synthetic_binary_ui_stage1`、ASCII decode-only/encode-only/text/literal/stream 以及生成的 `synthetic_ui_max` 组成。standalone CMake 不得再引用开发仓 `tests/`、`examples/` 或 `PROJECT_SOURCE_DIR` 形状路径。

Qt 完整副本的正式交付、裁剪和许可证/来源材料尚未闭合；不阻塞本地验证，但阻塞正式外发。

## 6. yyjson 待决策项

`schema_dispatch.cpp` 需要严格、有界、能检测重复顶层键的 JSON parser。不得改成正则/手工字符串猜测，也不建议给 PAE 增加新 public dispatch API。

建议用户批准方案 A：Lab 显式消费现有锁定 yyjson 0.12.0 的 `src/yyjson.h`、`src/yyjson.c`、`LICENSE`、`dependency.lock.json`。由 standalone 的单一 vendor target 编译一次，只链接 Lab dispatch；不链接开发树 `pae::yyjson`、不下载、不进入 PAE public API、不重复编译。

lock 当前记录 commit `8b4a38dc994a110abaec8a400615567bd996105f`、archive SHA-256 `a3bc9626ec0ba8bcc0644cc5355b3cf6eeda3188caf72bd7caa0c305acc78e79`，应作为输入一致性门禁。若不批准 A，需另定经评审 parser 和等价负向用例；此前不应开始实现。

## 7. no-fallback 和错配负向门禁

1. 物理隔离：外部根只含白名单 Lab 源、单个 SDK、Qt、configs 和获批 yyjson，不复制整个仓库。
2. PAE 来源：规范化后的 `PAE_DIR`、include 和 D/R imported location 均须位于当次 `PAE_SDK_ROOT`。
3. Qt 来源：Core/Gui/Widgets include/lib/DLL、工具和 plugin 均须位于 `PAE_QT_ROOT`；禁用 registry/system fallback。
4. 审核 generated project/link response：产品不得出现第 3.3 节 private target/archive/object。
5. Testing-off：不得生成测试 target、fault injection/诊断宏或 `ascii_smoke_diagnostic.h`。
6. 缺/错 PAE root 必须在 `find_package` 明确失败，不能命中开发 build tree。
7. Debug 配 Release-only 包或反向错配，须由 SDK mismatch gate 或等价检查失败。
8. 错 Qt 版本、缺 D/R lib/DLL/plugin/tool 必须失败，不能由本机 Qt 5.11 补齐。
9. yyjson 缺 header/source/license/lock 或 hash/version 不符必须失败，不能下载。
10. shared 后片中，缺 `pae.dll`、D/R DLL 错配或 DLL 非当次 SDK 来源必须失败；受控 `PATH` 启动并记录实际模块来源。

## 8. 最小自动验证集与顺序

### 8.1 static Debug/Release 首片

每个配置使用分离的 Testing-on 和 Testing-off build tree，不在同一 tree 切 SDK 或配置。Testing-on 最小集合：

- public adapter/boundary：`pae.protocol_lab_binary.public_h1`、`pae.tools.protocol_lab_ascii.public_a1`、`pae.tools.protocol_lab_ascii.public_stream`、`public_stream_noexcept_reject`、`public_stream_noexcept_candidate`、`ascii_public_facade_isolation`。
- headless：`description_mapping`、`compile_queue`、`document_state`、`dual_tab_isolation`、`mailbox_ownership`、`inspect_state`、`bounded_v08_state`、`schema_dispatch`、`owned_presentation_types`、`ascii_input`、`ascii_session`、`ascii_stream_session`、`host_session`、`binary_public_h2`、`binary_public_header`、`ascii_public_a2`、`public_legacy_complete`。
- Qt smoke：0.5/0.6、0.7、0.8、0.9、0.10、0.10 one-way、0.11、0.11 close/cancel、ASCII editor smoke。

测试须直接链接 standalone public Lab targets，不能借开发树 private compatibility 通过。Release 中使用 `assert` 的 target 继续 `/UNDEBUG`，并从编译命令证明未被 `NDEBUG` 关闭。

另须自动检查：(1) product include/link/object 只来自外部 Lab、SDK、Qt、获批 yyjson 根；(2) 第 7 节 PAE/Qt/yyjson 缺失和错配全部 fail-closed。static D/R Testing-on、Testing-off、负向门禁和受控环境 Qt smoke 全通过并经总控复核后停点，不在首片追加 shared。

### 8.2 shared Debug/Release 后片

在不改业务源的前提下，用 shared D/R 重做 Configure/Build/最小回归/Qt smoke，并：从当次 `PAE::pae` imported location 部署匹配 `pae.dll`；记录 EXE、PAE/Qt DLL/plugin/config 哈希；验证实际加载来源；执行 DLL 缺失和 D/R 错配负向用例。static 或 shared 任一局部通过都不能替代另一形态，也不证明稳定 ABI。

## 9. 尚需用户/总控决定

1. 是否批准第 6 节 yyjson 方案 A（建议批准）；这是进入实现前唯一代码路线拍板项。
2. Qt 正式外发采用完整随包、受控裁剪或前置安装，以及许可证/来源如何闭合；不在 static 本地闭包中顺手处理。
3. 下一片是否明确接受上述 dirty-provenance SDK 仅用于本地 closure validation。若要新 SDK 身份，须另行授权重打包和验证。
4. shared 是否继续以“static D/R 全通过并经总控复核后另行授权”为开始条件（建议保持）。

无需再决策：0.5–0.8 保留全部现有 complete-record；0.9 保持 Decode-only；0.10/0.11 保留现有 public complete/Host/stream；CLI 不纳入；不新增 PAE public API。

## 10. 本轮未验证与停止点

本轮只读 AGENTS、实时 Git、综合计划、Lab/adapter/CMake/测试源、前序报告、SDK provenance、Qt 头和 yyjson lock；只新增本报告。

- 未 Configure、Build、Test 或运行 Lab；未重打 SDK 五包。
- 未创建外部验证根，未复制/下载 Qt、yyjson、configs 或其他依赖。
- 未覆盖旧部署，未 Stage、Commit、Push、发布或删除。
- 未证明整个 Qt Lab 已可包外构建，未新增人工 UI、Linux、Golden、硬件、现场或生产证据；也不把历史 Qt 访问违例视为已修复。

当前停止写入，等待用户/总控对第 9 节，尤其 yyjson 归属拍板。
