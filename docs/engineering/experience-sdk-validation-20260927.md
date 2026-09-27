# adeae30 纯净 SDK 五包候选验证（2026-09-27）

## 结论与停点

本轮从 `adeae30d942ba42cc518c244c220730b7e462d2c` 的本地独立干净 clone
重新构建并制成 Windows x64 SDK Source、YAML-enabled Static/Shared Debug/Release
共五包。五包清单、长度和 SHA-256 校验均通过；Source 包的 JSON/YAML 独立消费、
两种嵌入宿主 CTest，以及四个二进制包各自的 JSON/YAML 包外消费均实际通过。
这是**SDK 候选**，不是完整体验总包或正式发布。包内离线文档引用尚有缺项，需后续
文档归集片处理；本轮不修改固定源码或已生成包的字节和身份。

接管时主仓库为 `main@adeae30d942ba42cc518c244c220730b7e462d2c`，暂存区为空，
已有总控未跟踪计划 `docs/engineering/first-use-bundle-plan-20260927.md`。
本轮主仓库仅新增本报告；并行文档任务的新增文件均未触碰。

## 固定来源与五个根

执行前 `out/build/experience-sdk-adeae30-20260927/` 与
`out/evidence/experience-sdk-adeae30-20260927/` 均不存在。使用
`git clone --no-hardlinks --no-tags --single-branch --branch main <主仓库>
<新构建根>/clean-source` 建立本地独立 clone，不联网、不建立 worktree。
clone HEAD 为上述完整 SHA，tree 为 `74b5e17c1894af39e97953f3fa684d1eb2404294`，
开始和结束的 `git status --porcelain=v1 --untracked-files=all` 均为空。后续 CMake
源码和 `package_sdk_stage3.ps1 -SourceRoot` 均指向该 clone；没有复用上一批二进制。

以下路径均在 `out/build/experience-sdk-adeae30-20260927/` 下，亦为下一片 Lab 可
按各自配置消费的**独立安装根**：

| SDK | 目录 | 包内文件数 |
| --- | --- | ---: |
| Source（含 YAML 作者源） | `source package with spaces` | 102 |
| Static Debug（YAML） | `static-yaml-Debug-install` | 42 |
| Static Release（YAML） | `static-yaml-Release-install` | 42 |
| Shared Debug（YAML） | `shared-yaml-Debug-install` | 38 |
| Shared Release（YAML） | `shared-yaml-Release-install` | 38 |

每包 `PROVENANCE.json` 均记录固定 `source_head`、`source_worktree_dirty=false`、
对应 kind/configuration/YAML 声明；`verify_yaml_sdk_package.ps1` 对实际文件、
`MANIFEST.txt` 长度与 `SHA256SUMS.txt` 全部复算通过，见五份 `*-verify.log`。
该 Hash 只证明包内一致性，不提供来源认证。额外的 `build-install-identity.log`
逐文件确认四组新构建输出与安装树中的 `pae.lib`、YAML 附加库及 shared `pae.dll`
一致（静态各 2 对、动态各 3 对）。Source 包 95 个同路径文件与 clone SHA-256
一致；其余 7 个路径是制包元数据、复制的 SDK 综合 consumer 和 yyjson 许可副本，
见 `source-file-identity.log`。

## 实际构建、制包和消费

构建使用 Visual Studio 18 2026、x64、`v142,version=14.29.30133`。从 clone 配置
`static-yaml-build`、`shared-yaml-build` 两个独立 BuildRoot；主要开关为
`PAE_BUILD_PUBLIC_API_STAGE1=ON`、`PAE_BUILD_YAML_FRONTEND=ON`、
`PAE_BUILD_TESTING=OFF`、`BUILD_TESTING=OFF`，以及按 kind 设置
`BUILD_SHARED_LIBS=OFF/ON`。每个 BuildRoot 内 Debug、Release 串行执行：

```powershell
cmake -S <clean-source> -B <static/shared构建根> -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" <上述-D选项>
cmake --build <构建根> --config <Debug/Release> --target pae_public_api pae_yaml_frontend --parallel 4
cmake --install <构建根> --config <Debug/Release> --prefix <独立安装根>
& <clean-source>/scripts/package_sdk_stage3.ps1 -Kind <static/shared> -PackageRoot <安装根> -Configuration <Debug/Release> -SourceRoot <clean-source>
& <clean-source>/scripts/verify_yaml_sdk_package.ps1 -PackageRoot <包根> -Kind <source/static/shared> -HasYaml:<布尔值>
```

上述五包 Configure、Build、Install、Package、Verify（Source 无 Build/Install）均退出
0；逐步原始输出在 `out/evidence/experience-sdk-adeae30-20260927/` 的
`local-clone.log`、`source-{package,verify}.log`、
`{static,shared}-yaml-{configure,Debug-*,Release-*}.log`。

消费时每个示例和每个包均使用独立 BuildRoot。Source 包路径包含空格，示例使用
`-DPAE_SOURCE_DIR=<该Source包根>`；二进制包使用
`-DCMAKE_PREFIX_PATH=<该包根>`。两种入门程序分别用独立合成输入，实际执行
Decode/Encode；JSON 输出 `GETTING_STARTED_BINARY_PASS` 和 `AA 00 07`，
YAML 输出 `PAE_YAML_SDK_CONSUMER_PASS`（源码同时检查 `AA 00 07` 的 Decode/Encode）。

| 消费检查 | 实际结果 | 日志前缀 |
| --- | --- | --- |
| Source 包 JSON、YAML 独立 Release | 各 Configure/Build/Run 退出 0 | `source-{json,yaml}-{configure,build,run}.log` |
| Source 包嵌入含自身 CTest 的 JSON、YAML 宿主 Release | 各 Configure/Build/CTest 退出 0；`ctest -N` 均列出宿主自有测试与真实 Codec 测试，运行各 2/2 通过 | `source-embedded-{json,yaml}-{configure,build,ctest-list,ctest}.log` |
| Static/Shared Debug/Release 四包，各自包外 JSON 与 YAML consumer | 8 组 Configure/Build/Run 均退出 0 | `{static,shared}-yaml-{Debug,Release}-{json,yaml}-consumer-{configure,build,run}.log` |
| Shared Debug/Release 两包的 JSON/YAML consumer | 四份程序旁 `pae.dll` SHA-256 均与所用包的 `bin/pae.dll` 相同 | `shared-consumer-dll-identity.log` |

嵌入宿主的临时 CMake 实验只在新 `out/build/.../host/`：引入示例前后断言
`BUILD_TESTING=ON`，并检查预置的 `PAE_BUILD_TESTING=ON`、
`PAE_BUILD_PUBLIC_API_STAGE1=OFF`、`PAE_BUILD_YAML_FRONTEND=OFF` Cache 未被示例
强制覆盖。实际 `ctest -N` 均为 2 项，没有意外注册 PAE 自身测试。这证明固定提交
包含上一轮修正的两个 Source 示例，且在本次含空格包路径下可用；不等于所有宿主
选项或 CMake Cache 完全隔离。

## 包内容、文档缺口与未验证

Source 包的 Schema、YAML 头文件、yyjson/rapidyaml 许可与通知，以及两份修正后的
示例文件，均存在且与干净 clone 对应文件 Hash 相同。四个二进制包的公开头、
Schema、YAML 附加库和依赖许可/通知均存在。五包路径检查未发现 Qt、Lab、spikes、
旧 `out` 或 CMake Cache 混入；Source 包文档、示例和 Schema 文本未检出本机盘符路径。
JSON-only 对照包没有附入本轮五包。

**文档闭包尚未完成：**固定提交的 `docs/sdk/README.md` 仍以 `b12ad80` 描述上一批
本地产物，并引用不随 SDK 安装的 `docs/engineering/yaml-sdk-packaging-validation-20260926.md`；
包内 `pae_yaml_profile_v0.1.md` 还引用多份未随包附带的
`docs/engineering/` 计划与验证记录。此处仅记录事实，不修改固定 clone、脚本、
原包内容或 provenance。需要由后续包级导航/文档归集独立解决，不能据此宣布
完整离线体验包已经就绪。

本轮未重跑完整全仓/Lab/UI矩阵，未运行人工 Lab、Linux、真实协议 Golden、硬件或现场；
未生成最终体验 ZIP，也未替换现用部署。项目级许可证与稳定跨工具链 ABI 仍非本轮
验证结论。主仓库未 Stage、Commit、Push、Pull 或清理旧产物；本 SDK 片停在
**已完成派发范围，待总控复核**。
