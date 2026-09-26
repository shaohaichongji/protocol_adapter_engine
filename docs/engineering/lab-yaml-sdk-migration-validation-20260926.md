# Lab YAML 公开 SDK 消费迁移验证（2026-09-26）

状态：**已完成派发范围，待总控复核**。现场基线 `main@4da54b9`，工作树含总控、PAE/SDK 与 Lab 多组未提交变更；本片未改 SDK 包或既有部署。下述结果仅属本地 dirty 来源的隔离候选，不是正式发布或人工 UI 验收。

## 实际改动与边界

- 仓库 Lab 的 `compile_worker.h/.cpp`、`document_tab.h/.cpp` 改用公开 `pae/yaml_frontend.h` 的 `pae::yaml::ConversionResult`、`ConvertToStrictJson`、`TrialResourceLimitsV01` 和拥有式来源位置查询；`tools/protocol_lab_ui/CMakeLists.txt` 仅链接 `PAE::yaml_frontend`。不直接包含内部 `frontend.h`，不取用内部 `Result`、`Limits` 或 fault 参数。原路径/标题、生成 JSON 哈希、Host 配置文本、revision/关闭生命周期和 JSON 路由不改。
- `standalone/CMakeLists.txt` 增加默认 OFF 的 `PAE_LAB_ENABLE_YAML_ENTRY`；ON 使用 `find_package(PAE CONFIG REQUIRED COMPONENTS yaml_frontend)`，检查导入的静态附加库在指定 SDK 根且与该包清单哈希一致，链接 `PAE::yaml_frontend`；OFF 接受 JSON-only 包，无仓库内前端回退。既有包种类、配置、CRT、shared `pae.dll` 与 Qt 输入门禁保留。
- `standalone/PrepareStandaloneInputs.ps1` 白名单纳入 Lab 自有两份合成 YAML 夹具；`standalone/cmake/DeployPaeLab.cmake` 仅 ON 时部署一份 ASCII YAML 示例，`DeployPaeLabRun.cmake` 仅 ON 时允许 `.pae.yaml/.pae.yml`，其余配置仍须 `.pae.json`。`standalone/README.md` 说明开关和组件形状。未修改 `scripts/package_sdk_stage3.ps1` 等 SDK 打包文件。
- `tests/protocol_lab_ui/CMakeLists.txt`、`yaml_entry_tests.cpp` 改用 `tests/protocol_lab_ui/fixtures/`，不依赖 `spikes`；两份新夹具与原合成 YAML 文件 SHA-256 各自相同。由于 standalone 已开启公开 0.11 stream 路由，同一改版配置在仓库组合必须拒绝私有路由，在 standalone 则必须成功走一次公开路由；两侧分别保留强断言，不再误认 standalone 为私有路由。`yaml_ui_smoke_tests.cpp` 的现有测试仍复用。

## 构建与专项结果

隔离根为 `out/build/lab-yaml-sdk-migration-20260926/` 和 `out/evidence/lab-yaml-sdk-migration-20260926/`。仓库组合使用 `windows-msvc-pae-lab` preset、v142 `14.29.30133`、仓库 Qt 5.13，显式开启 YAML、公开 legacy、H1/H2、A1/A2；包外组合以 `PrepareStandaloneInputs.ps1` 复制 Lab 白名单和已完成的 `yaml-sdk-packaging-20260926` 四个带 YAML 最终包。四份包外输入与各原包逐文件长度/哈希相同：static D/R 各 42 文件、shared D/R 各 38 文件，见 `sdk-input-identity.log`。

| 组合 | 实际命令要点 | 结果与日志（均在本轮 evidence 根） |
| --- | --- | --- |
| 仓库 D/R | `cmake --preset windows-msvc-pae-lab -S . -B out/build/lab-yaml-sdk-migration-20260926/repository` 加上述开关；`cmake --build <repository> --config Debug/Release --target pae_protocol_lab_ui_yaml_entry_tests pae_protocol_lab_ui_yaml_ui_smoke_tests pae_protocol_lab_ui_compile_queue_tests pae_protocol_lab_ui_document_state_tests pae_protocol_lab_ui_schema_dispatch_tests pae_protocol_lab_ui`；同目录 `ctest -C Debug/Release -R '^pae\.tools\.protocol_lab_ui\.(yaml_entry|yaml_ui_smoke|compile_queue|document_state|schema_dispatch)$' --output-on-failure` | Configure/构建通过，D/R 各 **5/5**；`repository-configure.log`、`repository-build-{Debug,Release}-verified.log`、`repository-ctest-{Debug,Release}-verified.log`。 |
| standalone static D/R | 指定 `PAE_SDK_ROOT=<同配置 static 最终包复制>`、`PAE_LAB_ENABLE_YAML_ENTRY=ON`、`PAE_LAB_EXPECTED_LIBRARY_KIND=STATIC`，同样构建 Lab 与五项目标并运行定向 CTest | D/R 各 **5/5**；`standalone-static-{Debug,Release}-{configure,build,ctest}-verified.log`。 |
| standalone shared D/R | 对应 shared SDK，`PAE_LAB_EXPECTED_LIBRARY_KIND=SHARED`，其余同上 | D/R 各 **5/5**；`standalone-shared-{Debug,Release}-{configure,build,ctest}-verified.log`。既有公开头 C4251 告警未在本片改 ABI。 |
| JSON-only static/shared Release | 指定相应 `json-only-{static,shared}-final-package`，`PAE_LAB_ENABLE_YAML_ENTRY=OFF`，构建 Lab、queue/session/dispatch 并定向 CTest | 两组 Configure/构建均通过，各 **3/3**；`json-only-{static,shared}-positive-configure.log`、`json-only-{static,shared}-build-Release.log`、`json-only-{static,shared}-ctest-Release.log`。部署不含 YAML 示例。 |
| JSON-only 缺组件负例 | 同一 JSON-only 包改为 `PAE_LAB_ENABLE_YAML_ENTRY=ON`，独立目录 Configure | static/shared 均非零退出，明确 `PAE package does not contain requested yaml_frontend component`；`json-only-{static,shared}-negative-configure.log`。 |

Release 构建日志含 `/UNDEBUG` 覆盖 `/DNDEBUG`，测试语义断言未被关闭。`yaml_entry` 包外测试实际完成两种合成 YAML → 公开编译 → Codec Decode/Encode，生成 JSON 再提交 worker 并核对哈希、来源位置精确/祖先近似/未映射，以及关闭和旧 revision 隔离。`yaml_ui_smoke` 为不可见 Qt 控件测试，核对路径、重载和生成 JSON 留存；没有启动可见窗口。

## 产物与隔离核对

- 隔离 Release 候选为 `out/build/lab-yaml-sdk-migration-20260926/standalone-static-Release-verified/deploy/Release/` 与 `.../standalone-shared-Release-verified/deploy/Release/`，均有 `pae_protocol_lab_ui.exe` 和 `configs/synthetic_ascii_literal_only.pae.yaml`。没有覆盖现用 Lab 或 SDK 包。
- `module-identity.log`：四组 Qt Core/Gui/Widgets DLL 和平台插件均与本片隔离 Qt 输入 SHA-256 相同；shared D/R 部署的 `pae.dll` 分别与各自 SDK 包相同；static 部署没有 `pae.dll`，四组均没有 YAML DLL（YAML 是静态 add-on）。standalone 生成的 `.vcxproj` 对内部前端与 `spikes` 路径扫描为零命中，见 `internal-path-audit.log`；复制 Lab 树无 `spikes` 或 `src/config_frontend_yaml`。
- 初次 static Debug 的部署白名单仅接受 JSON 而失败，见 `standalone-static-Debug-build.log`；返修后保留该日志并从新快照验证。旧 `0.11` 私有路由假设在 standalone 断言失败，见 `standalone-static-Debug-trace-run.log` 与 `standalone-static-Debug-route-fix-stderr.log`；在两组合分别断言公开路由后，最终四组专项均通过。最初一次 PowerShell Configure 参数拼接失败见 `standalone-static-Debug-configure.log`，参数数组重配通过。历史失败均不是最终通过证据。

## 未验证与停点

未跑全仓、正式应用 `--ui-smoke` 矩阵、人工可见 UI、真实协议、Linux/硬件；未对 YAML 的半帧/待确认取消重载进行交互专项。测试证明 worker 层将生成 JSON 重新提交时哈希一致，UI smoke 证明 Host 文本留存，但未执行真实 Host Apply 按钮的 YAML 交互。前端语法错误仍无可靠原 YAML 行列，Lab 显示“未提供”；近似映射不冒充精确位置。更多并发和低内存压力未覆盖。

源码 `standalone/README.md` 在最终 `*-input-verified` 快照后仅改了默认 OFF 的表述句，未改参与编译或部署的文件；107 份可与仓库对照的 Lab 文件只有该 README 字节不同（`source-snapshot-delta.log`）。以上构建证据对应快照内的可执行源码。`git diff --check` 无空白错误，工作树共享变更保留。本片未 Stage、Commit、Push、删除、发布或替换部署；已停止写入，待总控复核。
