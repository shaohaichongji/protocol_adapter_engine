# Lab 同批干净 SDK 独立消费验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 结论与边界

本轮在 `main@98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` 上，使用同一干净检查点生成的 Windows x64 static/shared Debug/Release SDK，完成 Qt Lab standalone 包外消费验证：

- static Debug/Release Testing-on 各 `32/32` PASS，Testing-off 产品态各 Configure/Build PASS；
- shared Debug/Release Testing-on 各 `32/32` PASS，Testing-off 产品态各 Configure/Build PASS；
- static 产品部署不含且不依赖 `pae.dll`；shared 产品的 `pae.dll` 已完成 SDK 输入→部署副本→运行时已加载模块三方路径和 SHA-256 一致性核对；
- package kind、Debug/Release 配对、shared runtime 缺失/错配均 fail-closed；
- Release 专项仍以 `/UNDEBUG` 覆盖 `/DNDEBUG`，断言未被关闭。

本结论是同工具链、同检查点的独立消费证据，不是正式发布、稳定 ABI、Linux、人工 UI、硬件或现场验收证据。

## 2. 基线与消费输入

- 仓库：`<REPO_ROOT>`
- 分支/HEAD：`main@98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`
- SDK 候选：`out/sdk-clean-checkpoint/candidate1-20260919/`
- static 独立根：`<LOCAL_WORK_ROOT>/pae-lab-clean-sdk-static-20260919/`
- shared 独立根：`<LOCAL_WORK_ROOT>/pae-lab-clean-sdk-shared-20260919/`

五个 SDK 包的 `PROVENANCE.json` 均记录 `source_head=98df5e0...`、`source_worktree_dirty=false`；重算 `SHA256SUMS.txt` 的结果为：source 80 项、static D/R 各 28 项、shared D/R 各 24 项，全部为 `0` 失败。static D/R 与 shared D/R 各自的 manifest 相对路径集一致。

`PrepareStandaloneInputs.ps1` 复制的 Lab/standalone 白名单与当前仓库 99 个可直接映射文件的 SHA-256 一致，两个独立根均为 `mismatch_count=0`、无额外 standalone 文件：

- `logs/current-repository-whitelist-comparison.json`

仓库当前的功能源、standalone CMake 及依赖锁定文件相对 HEAD 无差异；现场仅有既有文档差异。因此本轮严格表述为“干净 SDK + HEAD 一致的功能白名单”，不把带文档差异的共享工作树整体称为 clean。standalone README 中原有 dirty candidate 表述本轮未作为事实依据，并按授权范围保持未改。

## 3. static 验证

| 配置 | Testing-on | Testing-off | 运行时来源 |
| --- | --- | --- | --- |
| Debug | Configure/Build + `32/32` PASS | Configure/Build PASS | Qt Core/Gui/Widgets/qwindows 全部从 `deploy/debug-testing-off/Debug` 加载，输入/部署/加载哈希一致 |
| Release | Configure/Build + `32/32` PASS | Configure/Build PASS | Qt Core/Gui/Widgets/qwindows 全部从 `deploy/release-testing-off/Release` 加载，输入/部署/加载哈希一致 |

证据位于 static 独立根 `logs/`：`{debug,release}-testing-on-{configure,build,ctest}.log`、`{debug,release}-testing-off-{configure,build}.log`、`{debug,release}-testing-off-module-origin.json`。模块捕获记录的 PID 均已终止。

产品态核对：

- D/R 均为 `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC`，`PAE_DIR` 均在精确 SDK 根内；
- `*TESTS.vcxproj=0`，产品 vcxproj 中测试宏/私有兼容源命中为 `0`；
- D/R 部署中 `pae.dll=0`，`dumpbin /dependents` 仅显示对应 Qt/CRT/system 依赖，不含 `pae.dll`；
- 证据：`product-isolation-audit.log`、`{debug,release}-testing-off-dumpbin-dependents.log`。

static Release Testing-off 首次手工命令传入了错误 SDK 相对层级，被精确根门禁拒绝；原始输出保留为 `release-testing-off-initial-path-error.log`。纠正为 `inputs/sdk/pae-sdk-static-release` 后的正式 Configure/Build 全部 PASS，该操作误差不计为 SDK 失败。

## 4. shared 验证

| 配置 | Testing-on | Testing-off | `pae.dll` 实际来源 |
| --- | --- | --- | --- |
| Debug | Configure/Build + `32/32` PASS | Configure/Build PASS | `inputs/sdk/pae-sdk-shared-debug/bin/pae.dll` → D 部署 → D 进程已加载文件，三方哈希一致 |
| Release | Configure/Build + `32/32` PASS | Configure/Build PASS | `inputs/sdk/pae-sdk-shared-release/bin/pae.dll` → R 部署 → R 进程已加载文件，三方哈希一致 |

证据位于 shared 独立根 `logs/`：`{debug,release}-testing-on-{configure,build,ctest}.log`、`{debug,release}-testing-off-{configure,build}.log`、`{debug,release}-testing-off-module-origin.json`。两份模块记录均含 5 个预期模块（PAE + Qt Core/Gui/Widgets/qwindows），`all_loaded_from_deploy=true`、`all_hashes_match=true`，捕获 PID 均已终止。

产品态核对：

- D/R 均为 `PAE_LAB_EXPECTED_LIBRARY_KIND=SHARED`，`PAE_DIR` 在各自 SDK 根内；
- D/R 产品 vcxproj 均精确引用各自 SDK 的 `lib/pae.lib`；
- SDK 输入 `pae.dll` 与部署副本哈希一致；`dumpbin /dependents` 同时命中 `pae.dll` 与对应 Qt D/R DLL；
- `*TESTS.vcxproj=0`，产品 vcxproj 中测试宏/私有兼容源命中为 `0`；
- 证据：`product-isolation-runtime-audit.log`、`{debug,release}-testing-off-dumpbin-dependents.log`。

## 5. Release 断言与已知 warning

static/shared Release Testing-on 构建日志均出现 19 次 `D9025: ... /DNDEBUG ... /UNDEBUG`，确认专项断言未被 Release 默认宏关闭。

shared 构建仍出现已知 C4251（Release Testing-on 日志 163 处 warning 命中）。该 warning 不阻止本次同 MSVC/v142 工具链的 D/R 构建和 32 项测试，但也不能由此声称 stable ABI。

## 6. no-fallback 负向门禁

全部负例都使用新 build/deploy 或独立派生副本，未删除或覆盖原 SDK 和正向部署。

| 门禁 | 结果 | 主要证据 |
| --- | --- | --- |
| 真实 STATIC 包却期望 SHARED | Configure 拒绝 kind mismatch | static `negative-static-kind-mismatch-configure.log` |
| 真实 SHARED 包却期望 STATIC | Configure 拒绝 kind mismatch | shared `negative-shared-kind-mismatch-configure.log` |
| static/shared Debug 包构建 Release，Release 包构建 Debug | configuration guard marker 在链接期拒绝 | 两根 `negative-*-sdk-*-build-{configure,build}.log` |
| 派生 shared Debug 包缺少 `bin/pae.dll` | Configure 拒绝 runtime 不存在 | shared `negative-shared-missing-runtime-dll-configure.log` |
| 派生 shared Release 包放入 Debug `pae.dll` | Configure 拒绝 manifest hash 不符 | shared `negative-shared-wrong-runtime-dll-configure.log` |
| 派生 Release 部署放入 Debug `pae.dll` | 创建进程前拒绝来源哈希不符，无模块 JSON 产生 | shared `negative-shared-wrong-deploy-dll-capture.log` |

Qt/yyjson 来源与锁定逻辑本轮未变，按授权没有重复执行无意义负例；沿用 `lab-sdk-standalone-validation.md` 第 6 节的 Qt 错版本/缺必需 binary 及 yyjson 缺输入/错 hash 证据，并与 `lab-sdk-standalone-shared-validation.md` 第 7 节记录的共享逻辑一致。

## 7. 未验证与剩余风险

- 未执行人工 UI 验收；本轮自动测试未出现历史 access violation 或 focus-out 失败，但不因此宣称历史观察项已被单独解决。
- 未验证 Linux、异工具链/异 CRT、稳定 ABI、正式 Qt 许可复核、硬件、现场或正式发布。
- 没有重打 SDK 五包，没有覆盖旧部署，没有执行全仓测试。

## 8. Git 与停止状态

本轮仓库内只新增本报告；保留了现场全部既有 README/guide/综合计划差异及 SDK preflight/validation 报告。未修改源码、CMake、脚本、依赖、SDK 或其他文档；未 Stage、Commit、Push、发布或删除。已达到派发停点，停止写入，等待总控复核。
