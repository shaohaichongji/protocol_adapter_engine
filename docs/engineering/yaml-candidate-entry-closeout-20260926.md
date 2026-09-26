# YAML 初版候选入口与提交清单（2026-09-26）

## 状态与范围

状态：**已完成派发范围，待总控复核**。现场为 `main@4da54b9bdc6fa3fed0f81ab346c2fe97a9a6329e`；共享工作树中 SDK、Lab 和总控的累计变更保留。本任务只修改 `deliverables/README.md`、`docs/guides/02-首次运行与Lab体验.md`、`docs/guides/03-Windows-SDK集成.md`，并新增本报告。未修改 SDK 包、Qt、构建/运行文件或其他任务文档。

统一入口现在列出 YAML Source、Static/Shared Debug/Release、JSON-only Static/Shared Release 七个最终 SDK 候选，以及 standalone Lab Static/Shared Release 两套隔离部署。首次试用指向 Static Release；旧 `deliverables/sdk/7b4205e/` 与 `deliverables/lab/fe1683c-plus-patches/static-release/` 保留为此前独立身份。指南说明 JSON 与受限 YAML 的入口、`PAE::yaml_frontend` 静态附加组件、Profile V0.1、匹配工具链/配置及一次待执行人工体验。新候选的 `PROVENANCE.json` 记录 `source_head=4da54b9`、`source_worktree_dirty=true`；后续 Git 提交不会使原包变成 clean 构建。

## 导航与现场核对

所有路径相对于仓库根目录；`out/` 与 `deliverables/sdk/`、`deliverables/lab/` 均受 Git 忽略，仅 clone 不含本地成品。核对的是既有候选文件存在及报告所述形状，本轮没有复制或重新生成它们。

| 候选 | 相对位置 | 本轮只读核对 |
| --- | --- | --- |
| Source | `out/build/yaml-sdk-packaging-20260926/source final package with spaces/` | 102 文件；含 `src/config_frontend_yaml/public/pae/yaml_frontend.h`、Profile、rapidyaml 与 `examples/yaml_sdk_consumer/`；源码可选，默认 OFF |
| YAML Static D/R | `out/build/yaml-sdk-packaging-20260926/static-{debug,release}-final-package/` | 各 42 文件；含 `include/pae/yaml_frontend.h` 与 `lib/pae/addons/pae_yaml_frontend.lib` |
| YAML Shared D/R | `out/build/yaml-sdk-packaging-20260926/shared-{debug,release}-final-package/` | 各 38 文件；同样含静态附加库，PAE 使用同包 `bin/pae.dll`；无 YAML DLL |
| JSON-only Static/Shared Release | `out/build/yaml-sdk-packaging-20260926/json-only-{static,shared}-final-package/` | 分别 34/30 文件；无 YAML 公开头及附加库，provenance 声明不安装该组件 |
| Lab Static/Shared Release | `out/build/lab-yaml-sdk-migration-20260926/standalone-{static,shared}-Release-verified/deploy/Release/` | 两套均有 EXE、`configs/synthetic_ascii_literal_only.pae.yaml`、Qt DLL 与平台插件；shared 另有邻接 `pae.dll` |

七包均有 `PAE-SDK-README.md`、`MANIFEST.txt`、`SHA256SUMS.txt` 和 `PROVENANCE.json`，且后者均标记 dirty。七包内容校验和包外消费的执行证据来自 [SDK 打包验证](yaml-sdk-packaging-validation-20260926.md)；Lab 仓库与 standalone D/R 定向结果及未覆盖项来自 [Lab 迁移验证](lab-yaml-sdk-migration-validation-20260926.md)。本任务没有重跑验证脚本、编译、测试或启动 UI。人工可见 UI 体验由用户延期，当前不能写成通过。

## 累计 Git 提交候选清单

以下为本轮收尾时 `git status --short --untracked-files=all` 的**只读全量清单**，共 48 个路径：26 个已跟踪修改、22 个未跟踪文件。其中本任务是上文四份文档；其余来自总控、PAE 或 Lab 的既有共享变更，归属与纳入提交须由总控复核。清单不是暂存或提交指令。

| 分组 | 路径 |
| --- | --- |
| 根构建与 SDK 安装 | `CMakeLists.txt`；`cmake/PAEConfig.cmake.in`；`cmake/PaeSdkInstall.cmake` |
| YAML 前端、组件与专项测试 | `src/config_frontend_yaml/CMakeLists.txt`；`src/config_frontend_yaml/public/pae/yaml_frontend.h`；`src/config_frontend_yaml/public_facade.cpp`；`tests/config_frontend_yaml/CMakeLists.txt`；`tests/config_frontend_yaml/public_facade_tests.cpp` |
| SDK 打包、校验与示例 | `scripts/package_sdk_stage3.ps1`；`scripts/verify_yaml_sdk_package.ps1`；`examples/yaml_sdk_consumer/CMakeLists.txt`；`examples/yaml_sdk_consumer/main.cpp`；`examples/yaml_sdk_consumer/synthetic_fixed_message.pae.yaml`；`docs/sdk/README.md` |
| Lab 源码、构建、部署及测试 | `tools/protocol_lab_ui/CMakeLists.txt`；`tools/protocol_lab_ui/application_window.cpp`；`tools/protocol_lab_ui/compile_worker.cpp`；`tools/protocol_lab_ui/compile_worker.h`；`tools/protocol_lab_ui/document_tab.cpp`；`tools/protocol_lab_ui/document_tab.h`；`tools/protocol_lab_ui/standalone/CMakeLists.txt`；`tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1`；`tools/protocol_lab_ui/standalone/README.md`；`tools/protocol_lab_ui/standalone/cmake/DeployPaeLab.cmake`；`tools/protocol_lab_ui/standalone/cmake/DeployPaeLabRun.cmake`；`tests/protocol_lab_ui/CMakeLists.txt`；`tests/protocol_lab_ui/fixtures/synthetic_ascii_literal_only.pae.yaml`；`tests/protocol_lab_ui/fixtures/synthetic_crc_slice.pae.yaml`；`tests/protocol_lab_ui/yaml_entry_tests.cpp`；`tests/protocol_lab_ui/yaml_ui_smoke_tests.cpp` |
| YAML Profile、计划与索引 | `schema/pae_yaml_profile_v0.1.md`；`docs/engineering/README.md`；`docs/engineering/post-dec040-roadmap.md`；`docs/engineering/yaml-entry-plan-20260926.md` |
| 分片契约与验证记录 | `docs/engineering/lab-yaml-minimal-entry-contract-20260926.md`；`docs/engineering/lab-yaml-minimal-entry-validation-20260926.md`；`docs/engineering/lab-yaml-sdk-migration-contract-20260926.md`；`docs/engineering/lab-yaml-sdk-migration-validation-20260926.md`；`docs/engineering/yaml-sdk-component-contract-20260926.md`；`docs/engineering/yaml-sdk-component-validation-20260926.md`；`docs/engineering/yaml-sdk-delivery-design-20260926.md`；`docs/engineering/yaml-sdk-delivery-preflight-20260926.md`；`docs/engineering/yaml-sdk-packaging-contract-20260926.md`；`docs/engineering/yaml-sdk-packaging-validation-20260926.md` |
| 本轮入口与收口 | `deliverables/README.md`；`docs/guides/02-首次运行与Lab体验.md`；`docs/guides/03-Windows-SDK集成.md`；`docs/engineering/yaml-candidate-entry-closeout-20260926.md` |

本报告清单按当时现场生成；若总控复核时共享树继续变化，须重查实时 `git status`，不能按旧清单直接暂存。

## 验证边界与待决事项

- 已核对上述关键候选路径与文件形状、文档相对链接、三份入口文档和本报告的改动范围；校验结果以本轮只读命令及既有分片验证记录为依据。
- 本轮没有构建、运行 Lab 或 consumer，也没有独立重复七包 Hash；此前专项测试结果不能替代人工可见 UI、真实 Host Apply、完整矩阵、Linux、真实协议或生产验收。
- 新候选仍在隔离 `out/`，没有覆盖旧交付物；YAML Profile、项目级许可、稳定 ABI 和正式发布条件未据本轮入口整理关闭。
- 无 Stage、Commit、Push、删除、复制、发布或部署替换。本任务停止写入，等待总控审查累计候选并决定后续 Git 操作与一次统一人工体验安排。
