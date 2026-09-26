# 固定 b12ad80 干净 YAML SDK 初版验证（2026-09-26）

## 结论与边界

本轮从固定提交 `b12ad809bdbc35589e8e5755f0e4ad04f386fa10` 的**本地独立干净
clone** 重新配置、构建、安装并打包七个 Windows x64 SDK 候选，没有复用旧候选库或
旧安装树。七包的 `MANIFEST.txt` 长度、`SHA256SUMS.txt` 和 `PROVENANCE.json` 均
重新复核通过；source 包及六个二进制包共九次实际包外消费通过。YAML 专项测试
Debug/Release 各 2/2 通过，两个 JSON-only 包对必需 YAML 组件的请求均在 Configure
阶段按预期拒绝。

这是可供总控限定复核的**本地初版候选**，不是正式发布、项目许可证决定、稳定
跨工具链 ABI、Linux、真实协议、人工 Lab 或设备/现场验收。shared 构建中的既有
MSVC C4251 警告保留在日志，本片没有修改公开接口。

主仓库接管时为 `main@b12ad809bdbc35589e8e5755f0e4ad04f386fa10`，暂存区为空，
已有总控计划/索引 Markdown 修改与未跟踪的
`docs/engineering/yaml-clean-delivery-plan-20260926.md`，均未触碰。本轮跟踪候选
仅新增本报告；源码、脚本、CMake、Lab、交付目录和旧证据没有改动。

## 固定来源链

- 新根：`out/build/yaml-clean-sdk-b12ad80-20260926/`；原始日志：
  `out/evidence/yaml-clean-sdk-b12ad80-20260926/`。执行前两根均不存在。
- `git clone --no-hardlinks --no-tags --single-branch --branch main <主仓库>
  <新根>/clean-source` 是本地复制，不联网、不建立 worktree。副本 HEAD 为完整固定
  SHA，tree 为 `dd31932e524c96bb2504b61934e3b8d5229cd573`，接管与完成时
  `git status --porcelain=v1 --untracked-files=all` 均为空；见 `local-clone.log`、
  `source-identity.log`、`source-final-git-status.log`。
- 所有 `cmake -S` 指向该 clone 或从该 clone 制成的 source 包；所有 binary 的
  `pae_public_api` 与可选 `pae_yaml_frontend` 都在本轮新构建根编译。
  `build-install-identity.log` 对各配置的 `pae.lib`、shared `pae.dll` 与可选
  `pae_yaml_frontend.lib` 做构建输出↔新安装树逐文件 SHA-256 对照，六组分别通过
  2/2/3/3/1/2 对。source 包 98 个非元数据文件与 clone 对应源字节相同，见
  `source-file-identity.log`。
- **从 clone 内运行**原 `scripts/package_sdk_stage3.ps1` 并传入 clone 为
  `-SourceRoot`；七包 provenance 均记录完整固定 HEAD、`source_worktree_dirty=false`。
  binary provenance 的通用说明仍称安装树字节，并不单凭 Git 状态断言构建来源；
  上述新构建与字节对照补齐本轮来源链。未手工修改任何元数据。

## 七包与实际消费

所有 Configure 使用 `-G "Visual Studio 18 2026" -A x64
-T "v142,version=14.29.30133"`；产品构建使用公开 API ON、Testing/Lab OFF。
YAML 包设置 `PAE_BUILD_YAML_FRONTEND=ON`，JSON-only 为 OFF。Debug 与 Release
串行使用同一构建目录。下表包路径均相对本轮新 `out/build` 根；日志名均相对本轮
新 `out/evidence` 根。

| 候选包路径 | 重新构建/安装与制包 | 包外消费及结果 |
| --- | --- | --- |
| `source package with spaces` | `package_sdk_stage3.ps1 -Kind source -Configuration Debug+Release`；`source-package.log`、`source-verify.log` | 从包内 `examples/yaml_sdk_consumer` 以 `PAE_SOURCE_DIR=<包根>` 配置，Debug/Release 分别 Build/Run，均退出 0、输出 `PAE_YAML_SDK_CONSUMER_PASS`；`source-yaml-consumer-{configure,Debug-build,Debug-run,Release-build,Release-run}.log`。另从包内 `getting_started` 保持 YAML 默认 OFF，Release 输出 `GETTING_STARTED_BINARY_PASS`；`source-json-only-consumer-{configure,build,run}.log`。 |
| `static-yaml-Debug-install` | `BUILD_SHARED_LIBS=OFF`，新建 `static-yaml-build`，Debug Build/Install，随后 `-Kind static -Configuration Debug` 打包；`static-yaml-{configure,Debug-build,Debug-install}.log`、`static-yaml-Debug-install-{package,verify}.log` | 从**该包** `examples/yaml_sdk_consumer` 配置/Build/Run Debug，退出 0、`PAE_YAML_SDK_CONSUMER_PASS`；`static-yaml-Debug-install-consumer-{configure,build,run}.log`。 |
| `static-yaml-Release-install` | 同一新源码构建根顺序执行 Release Build/Install/制包；相应 `static-yaml-Release-*` 日志 | Release 包外 consumer 退出 0、`PAE_YAML_SDK_CONSUMER_PASS`；`static-yaml-Release-install-consumer-{configure,build,run}.log`。 |
| `shared-yaml-Debug-install` | `BUILD_SHARED_LIBS=ON`，新建 `shared-yaml-build`，Debug Build/Install，随后 `-Kind shared -Configuration Debug`；相应 `shared-yaml-*` 日志 | Debug 包外 YAML consumer 退出 0；消费端 `pae.dll` 与该包 `bin/pae.dll` Hash 相同；`shared-yaml-Debug-install-consumer-{configure,build,run}.log`、`shared-yaml-Debug-install-dll-identity.log`。 |
| `shared-yaml-Release-install` | 同一新源码构建根顺序执行 Release Build/Install/制包；相应 `shared-yaml-Release-*` 日志 | Release 包外 YAML consumer 退出 0，同包 DLL Hash 相同；`shared-yaml-Release-install-consumer-{configure,build,run}.log`、`shared-yaml-Release-install-dll-identity.log`。 |
| `json-only-static-Release-install` | 全新 `json-only-static-build`、`BUILD_SHARED_LIBS=OFF`、YAML OFF；Release Build/Install/`-Kind static`；`json-only-static-{configure,build,install}.log`、`json-only-static-Release-install-{package,verify}.log` | 包内原 `getting_started` Release Build/Run 退出 0、`GETTING_STARTED_BINARY_PASS`；`json-only-static-Release-install-consumer-{configure,build,run}.log`。 |
| `json-only-shared-Release-install` | 全新 `json-only-shared-build`、`BUILD_SHARED_LIBS=ON`、YAML OFF；Release Build/Install/`-Kind shared`；对应 `json-only-shared-*` 日志 | 原 `getting_started` Release Build/Run 退出 0、`GETTING_STARTED_BINARY_PASS`；消费端 DLL 与同包 Hash 相同；`json-only-shared-Release-install-consumer-{configure,build,run}.log`、`json-only-shared-Release-install-dll-identity.log`。 |

实际执行形式：`cmake -S <clean-source> -B <新构建根> ...`，然后
`cmake --build <构建根> --config Debug/Release --target pae_public_api
pae_yaml_frontend`（JSON-only 只构建 `pae_public_api`），
`cmake --install <构建根> --config <配置> --prefix <新安装根>`，再从 clone 执行
`package_sdk_stage3.ps1 -Kind <source/static/shared> -PackageRoot <新目标>
-Configuration <配置> -SourceRoot <clean-source>`。每一步均检查退出码；表中日志保留
完整命令输出和警告。source 包的独立 consumer 真实执行 YAML→严格 JSON→公开
Compiler→Codec，JSON-only consumer 真实 Decode/Encode 已知合成帧。

## 校验、负例与未验证

`verify_yaml_sdk_package.ps1` 重新枚举包内文件，并核对清单路径/长度、所有散列、
kind 和 YAML 声明；七包 7/7 通过，实际文件数依次为
**102、42、42、38、38、34、30**，见 `final-seven-package-closure.log`。四个启用
YAML 的包内 rapidyaml License 与通知 Hash 均等于干净源；JSON-only 包不声明组件。
包路径没有 Lab/Qt、spikes 或旧 out 内容，source 包文本未检出本机盘符路径。
shared PAE 是同包 `pae.dll` 加静态 YAML 附加库，没有 YAML DLL。

从两个 JSON-only 包分别运行
`cmake -S <clean-source>/examples/yaml_sdk_consumer -B <独立负例根>
-DCMAKE_PREFIX_PATH=<JSON-only 包>`，Configure 均退出 **1**，精确理由是
`PAE package does not contain requested yaml_frontend component`；见
`json-only-{static,shared}-Release-install-yaml-request-negative.log`。这是预期拒绝，
不是消费通过。原安装树和包没有为负例改写。

本轮未重跑无变化的全仓、完整 UI 或 standalone 矩阵；未运行可见 Lab 或人工 YAML
体验，也未复制到 `deliverables/`。包内 Hash 只证明内部一致性，不提供来源认证或
防篡改保证；“干净提交 → 新构建 → 安装字节 → 包外消费”的链条是本轮额外核对，
不扩展为正式安全保证。总控限定复核后才可派发 Lab 消费和后续归集。

本片未 Stage、Commit、Push、Pull、删除旧产物或发布；停在**已完成派发范围，
待总控复核**。
