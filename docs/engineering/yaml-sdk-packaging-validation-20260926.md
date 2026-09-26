# YAML SDK 打包闭包实施片验证（2026-09-26）

## 结论与边界

按 `yaml-sdk-packaging-contract-20260926.md`，本轮生成并独立复核 **7 个本地最终候选包**：
含空格路径的 source 包、带 YAML 组件的 static/shared Debug/Release 四包，以及
JSON-only static/shared Release 两包。最终候选均通过包内文件长度、SHA-256 和
`PROVENANCE.json` 检查；source 的 YAML consumer Debug/Release、四个二进制 YAML
consumer，以及 source/static/shared JSON-only Release consumer 均实际构建、运行通过。
这些是 Windows x64/MSVC 本地试用候选，不是正式发布或对外分发许可。

现场基线为 `main@4da54b9bdc6fa3fed0f81ab346c2fe97a9a6329e`。接管时已有
Lab、YAML 组件和总控文档脏树，本轮完整保留；包内 provenance 的
`source_worktree_dirty=true`。复制先前安装树时，当前 HEAD/dirty **不能单独证明**
二进制由当前脏树重建。打包脚本在 binary provenance 中明确写为“既有安装树字节”，
并通过输入树身份、最终包文件 Hash 和真实包外消费串起可追溯证据。

所有生成物位于忽略目录 `out/build/yaml-sdk-packaging-20260926`；原始日志位于
`out/evidence/yaml-sdk-packaging-20260926`。没有写入原 `yaml-sdk-component-20260926`
安装树、`deliverables/` 或其他现用部署。

## 本轮修改与实际闭包

- `scripts/package_sdk_stage3.ps1`：source 白名单加入可选 YAML 源码、公开头、
  CMake、Profile、固定 rapidyaml 原件/通知及合成 consumer；预检必需源码后才创建
  新目标。binary 由安装包 `PAEConfig.cmake` 判断组件 ON/OFF，复核库类型、配置、
  imported target、附加库、头、Profile、许可与示例；缺件、JSON-only 混合状态或
  YAML DLL 形状失败关闭。生成元数据增加可选组件与 static add-on 身份，不改变
  原有入口和清单格式。
- `scripts/verify_yaml_sdk_package.ps1`：按精确路径集合重新枚举文件，复核
  `MANIFEST.txt` 全部长度、`SHA256SUMS.txt` 全部散列与 provenance 的 kind、组件
  声明和链接形状；不会修改待检包。
- `docs/sdk/README.md`：从“仅安装树候选、尚未接入打包”更新为实际本地打包边界，
  区分 source 默认 OFF、binary 条件组件、JSON-only 和非正式发布状态。

初次制包与消费先在不带 `final` 的隔离候选中完成。SDK 说明随后同步；为避免覆盖
已有候选，另以新根创建最终候选。最终 source 的 98 个非元数据文件与仓库对应
文件 SHA-256 相同；初次候选和最终候选相比，除刷新 SDK 说明及生成元数据外，
所有源码、库、示例与配置文件字节相同（见 `source-final-file-identity.log`、
`final-content-identity.log`）。最终包又独立完成下表中的实际消费，并非仅推断通过。

## 命令、结果与日志

表内日志名均相对于本轮 `out/evidence/yaml-sdk-packaging-20260926`。CMake 使用
`Visual Studio 18 2026`、`-A x64`、`-T v142,version=14.29.30133`；Debug 和
Release 顺序执行，未并发使用同一消费构建目录。

| 检查 | 实际命令要点 | 结果与日志 |
| --- | --- | --- |
| source 包与字节闭包 | `package_sdk_stage3.ps1 -Kind source -PackageRoot <source final package with spaces> -Configuration Debug+Release`；`verify_yaml_sdk_package.ps1 -Kind source -HasYaml $false` | 退出 0；102 文件，98 个源文件与仓库对应字节相同；`source-final-package.log`、`source-final-verify.log`、`source-final-file-identity.log`。 |
| source YAML 消费 | `cmake -S <包>/examples/yaml_sdk_consumer -B <独立根> -DPAE_SOURCE_DIR=<包>`；依次 `cmake --build --config Debug/Release --target pae_yaml_sdk_consumer` 并运行包内 YAML | 两配置均退出 0，输出 `PAE_YAML_SDK_CONSUMER_PASS`；`source-final-yaml-{configure,Debug-build,Debug-run,Release-build,Release-run}.log`。 |
| source 默认 OFF | 从最终包 `examples/getting_started` 独立 Configure/Release Build/Run，检查 `CMakeCache.txt` 中 `PAE_BUILD_YAML_FRONTEND:BOOL=OFF` | 退出 0，`GETTING_STARTED_BINARY_PASS`；`source-final-json-only-consumer-{configure,build,run}.log`。 |
| YAML binary 四包 | 从既有构建目录向**全新** static/shared D/R 根 `cmake --install --config <D/R> --prefix <新根>`，再分别 `package_sdk_stage3.ps1` 与 `verify_yaml_sdk_package.ps1`；从最终包内 `examples/yaml_sdk_consumer` 配置、构建并运行 | 四包制包/复核/消费均退出 0，均输出 `PAE_YAML_SDK_CONSUMER_PASS`；`{static,shared}-{debug,release}-final-package-{install,package,verify,configure,build,run}.log`。shared 消费端 DLL 与同包 `bin/pae.dll` SHA-256 相同。 |
| JSON-only binary 两包 | static 复用已验证构建，shared 在本轮新根以 `BUILD_SHARED_LIBS=ON`、YAML/Testing/Lab OFF、公开 API ON 构建并安装；两包从最终包运行原 `getting_started` | 两包制包/复核/Release 消费退出 0，输出 `GETTING_STARTED_BINARY_PASS`；`json-only-{static,shared}-final-package-{install,package,verify}.log`、`json-only-{static,shared}-final-consumer-{configure,build,run}.log`；shared 新构建见 `json-only-shared-{configure,build,install}.log`。 |
| 元数据复核 | 对七个最终包再运行 `verify_yaml_sdk_package.ps1` | 7/7 通过；文件数依次为 102、42、42、38、38、34、30；`final-all-package-verify.log`。provenance 均保留当前 HEAD、dirty=true，binary YAML linkage 为 `static-addon`，无 YAML DLL。 |
| 输入树与候选身份 | 首轮五个安装树先复制到新根，逐文件长度/SHA-256 对照；最终包非文档文件再与首轮包对照 | 五树输入 38/38/34/34/30 文件完全相同；`input-tree-identity.log`、`final-content-identity.log`。最终包刷新了当前 SDK 说明，并非声称与较早安装树所有字节相同。 |

失败关闭验证：隔离副本移去 YAML 附加库，打包在元数据生成前精确拒绝
`Binary YAML component is incomplete: lib/pae/addons/pae_yaml_frontend.lib`；
JSON-only 隔离副本加入 YAML 头，精确拒绝混合状态。见
`negative-missing-yaml-lib.log`、`negative-json-only-mixed.log`。JSON-only static/shared
包请求 `find_package(PAE CONFIG REQUIRED COMPONENTS yaml_frontend)` 均在 Configure
阶段以 `PAE package does not contain requested yaml_frontend component` 拒绝，
见 `json-only-{package,shared-package}-request-yaml-negative.log`。source/binary 目标
已存在时均拒绝，原 `SHA256SUMS.txt` Hash 未变，见 `negative-existing-{source,static}.log`。
所有负例仅在本轮隔离副本中构造，未改历史安装树。

## 格式、来源与剩余边界

新增 provenance 字段 `yaml_frontend_source_optional`、`yaml_frontend_installed`、
`yaml_frontend_linkage` 是附加描述；原 `package_kind`、configuration、HEAD、dirty、
`MANIFEST.txt` 和 `SHA256SUMS.txt` 用法保留。最终包无 Lab/Qt、spikes、旧 out
路径文件；source 文本未检出本机盘符路径。`MANIFEST`/Hash 只证明包内一致性，
不提供来源认证、签名或全面防篡改保证。

本轮未重跑无变化的 YAML 组件内部专项、全仓/完整 UI 矩阵，也未启动可见 UI、
进行人工 YAML 体验、Linux、真实私有协议、硬件或现场验证。未决定项目级 PAE
许可证，未证明稳定 C++ ABI 或生产容量；rapidyaml 许可通知不替代项目分发授权。
shared 新建 JSON-only 构建出现既有 MSVC C4251 警告，构建/消费仍退出 0；不在本片
改公开 ABI。待总控审查的关键点是脚本白名单/条件完整性、七包实际字节闭包和
上述受限验证，不应据此宣布正式 SDK 交付或 Lab 消费迁移完成。

本片仅修改打包脚本、新校验脚本、SDK 说明与本报告。未 Stage、Commit、Push，
停止在**已完成派发范围，待总控复核**。
