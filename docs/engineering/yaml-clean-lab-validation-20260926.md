# 固定 b12ad80 干净 YAML Lab 构建验证（2026-09-26）

## 结论与身份

**已完成派发范围，待总控复核。** 从 `b12ad809bdbc35589e8e5755f0e4ad04f386fa10` 的独立干净 `clean-source`，经该提交的 standalone 白名单脚本，使用同轮新构建的四个带 YAML SDK 包，重新配置和构建了 static/shared Debug/Release Lab。四组各完成指定五项 CTest，均 **5/5**。隔离 static/shared Release 部署已形成，保留 13 份 JSON 和 1 份 YAML 示例；未启动可见窗口，也未复制到 `deliverables/` 或覆盖旧成品。

本轮起止 `clean-source` 的 HEAD 均为上述完整 SHA，`git status --porcelain=v1 --untracked-files=all` 为空；主仓库起点为 `main@b12ad80` 且有总控计划/索引文档脏树。本片仓库内**仅新增本报告**，未修改源码、CMake、脚本、SDK 包或原 clone。旧 dirty Lab/SDK 候选不因本轮通过而改变身份。

## 固定输入与命令

执行前新根 `out/build/yaml-clean-lab-b12ad80-20260926/` 与 `out/evidence/yaml-clean-lab-b12ad80-20260926/` 均不存在。所有命令从仓库根执行；下文路径相对仓库，日志名相对本轮 evidence 根。VS 生成器为 `Visual Studio 18 2026`，`-A x64 -T v142,version=14.29.30133`；Qt 5.13 来自固定提交的 `third_party/qt`，未改系统 Qt 或全局 PATH。

1. 把 `out/build/yaml-clean-sdk-b12ad80-20260926/` 下 `static-yaml-{Debug,Release}-install` 和 `shared-yaml-{Debug,Release}-install` 四包各复制到本轮新根的临时候选目录，再**从 clean-source** 调用 `tools/protocol_lab_ui/standalone/PrepareStandaloneInputs.ps1 -RepositoryRoot <clean-source> -DestinationRoot <新 static/shared-input> -SdkCandidateRoot <相应临时候选根> -PackageKind static|shared`。复制前后四包文件数及逐文件长度/SHA-256 为 static D/R 各 42/42、shared D/R 各 38/38，差异均 0；复制 Lab 树中可与 clone 对照的文件各 107 份，差异均 0。见 `static-prepare.log`、`shared-prepare.log`、`input-identity.log` 与各输入树的 `INPUT_PROVENANCE.json`、`INPUT_SHA256.json`。四个原 SDK 包的 provenance 均记录同一固定 HEAD、`source_worktree_dirty=false`、YAML 组件已安装；本轮未重打或修改它们。
2. 对四组分别执行 `cmake -S <static/shared-input>/inputs/lab/tools/protocol_lab_ui/standalone -B <standalone-kind-config> -G "Visual Studio 18 2026" -A x64 -T v142,version=14.29.30133`，显式指定复制树内的 `PAE_LAB_SOURCE_ROOT`、同包 `PAE_SDK_ROOT`、`PAE_QT_ROOT`、`PAE_LAB_DEPENDENCY_ROOT`、`PAE_LAB_CONFIG_ROOT`，并设置 `PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC|SHARED`、`PAE_LAB_ENABLE_YAML_ENTRY=ON`、`PAE_LAB_BUILD_TESTING=ON`。四次 Configure/Generate 均退出 0；见 `standalone-{static,shared}-{Debug,Release}-configure.log` 和 `build-input-routing.log`。
3. 每组顺序执行 `cmake --build <standalone-kind-config> --config Debug|Release --target pae_protocol_lab_ui_yaml_entry_tests pae_protocol_lab_ui_yaml_ui_smoke_tests pae_protocol_lab_ui_compile_queue_tests pae_protocol_lab_ui_document_state_tests pae_protocol_lab_ui_schema_dispatch_tests pae_protocol_lab_ui --parallel 4`，随后 `ctest --test-dir <standalone-kind-config> -C Debug|Release -R '^pae\.tools\.protocol_lab_ui\.(yaml_entry|yaml_ui_smoke|compile_queue|document_state|schema_dispatch)$' --output-on-failure --timeout 30`。四组构建与 CTest 退出 0，各 **5/5**；见 `standalone-{static,shared}-{Debug,Release}-{build,ctest}.log`。Release 构建日志均有 `/UNDEBUG` 覆盖 `/DNDEBUG`，断言没有被关闭。shared 的既有 MSVC C4251 告警保留，未改 ABI。

`yaml_entry` 测试使用真实合成 YAML、公开编译及 Codec Decode/Encode，并核对生成 JSON 的 Host 再提交哈希、来源位置和旧 completion 隔离；`yaml_ui_smoke` 在不可见 Qt 控件中覆盖路径、重载及生成 JSON 留存。两者不是人工可见 Lab 或真实 Host Apply 验收。没有复跑仓库组合、完整 UI 矩阵或 JSON-only OFF/缺组件负例；上一片证据不能记作本轮重跑。

## 部署与隔离核对

可运行但尚未人工体验的 Release 候选分别在：

- `out/build/yaml-clean-lab-b12ad80-20260926/standalone-static-Release/deploy/Release/`
- `out/build/yaml-clean-lab-b12ad80-20260926/standalone-shared-Release/deploy/Release/`

四组部署的 EXE 与本组新构建输出 SHA-256 相同。按固定源码 `DeployPaeLab.cmake` 的**精确部署白名单**核对，四组各 14 份配置（13 JSON、1 YAML）与复制输入的同名文件哈希相同，无缺项或多项；见 `deployment-config-whitelist.log`。准备树另外四份仅供测试的配置不属于部署白名单。初次审计曾错误地以准备树全部 18 份为部署预期，产生 4 个假缺项；原日志 `deployment-identity.log` 保留，已由精确白名单审计纠正，不曾修改产物。

四组 Qt Core/Gui/Widgets DLL 及 `qwindows[d].dll` 插件：干净 clone → 复制 Qt 输入 → 实际部署的 SHA-256 全链一致，见 `qt-input-identity.log`、`deployment-identity.log`。shared D/R 的实际部署 `pae.dll` 与对应**原 SDK 安装包** `bin/pae.dll` 哈希相同；static 无 `pae.dll`，四组均无 YAML DLL（组件为静态 add-on）。复制 Lab 树没有 `spikes` 或内部 `src/config_frontend_yaml`，四个生成工程 `.vcxproj` 对内部前端头/路径扫描零命中，见 `internal-boundary.log`。这些核对不等于签名或运行时全部 DLL 的系统级来源认证。

## 未验证与 Git 边界

未运行可见窗口、人工 YAML/JSON 交互、真实 Host Apply、半帧取消重载、真实协议、设备/现场、Linux、全仓或完整 UI 测试；也未判断项目许可证、长期稳定 ABI 或生产容量。Debug/Release 自动测试仅证明上述合成配置及隔离包外目标。当前候选是同基线本地初版，不是正式发布。后续归集和统一人工体验均待总控下一步授权。

主仓库未 Stage、Commit、Push、Pull、删除、替换旧部署或复制 `deliverables/`。本任务停止写入，等待总控核对差异、日志与来源链。
