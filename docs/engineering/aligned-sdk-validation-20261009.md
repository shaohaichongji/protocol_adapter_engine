# a1c6010 同基线 SDK 五包重建验证（2026-10-09）

## 1. 结论、来源和边界

本次从固定提交 `a1c6010a257b0790f09c639fd79556170887bbc1` 的本地独立干净 clone，
重新构建 Windows x64 Source、YAML-enabled Static/Shared Debug/Release 五个 SDK
目录包候选。五包清单/长度/Hash 校验通过，18 次有效包外消费通过；没有复用旧库、
旧安装树、旧 consumer Cache、旧 tutorial overlay 或后来新增的本报告作为产品来源。
没有修改现有脚本、产品源码、公共接口、CMake、测试、旧包、旧说明或交付入口。

SDK 构建和消费已经结束，停止写入，待总控复核及后续串行 Lab 派发。不是 Lab 已重建、
完整体验包已闭包、正式发布、项目许可闭合、稳定 ABI、Linux 或真实设备验证。

- 主仓库接管：`main@a1c6010a257b0790f09c639fd79556170887bbc1`，工作区和暂存区干净。
- 新根：`out/build/aligned-sdk-a1c6010-20261009/`，初始不存在；证据在其 `evidence/`。
- 新候选根：`deliverables/sdk/a1c6010/`，初始不存在，保持忽略；旧包未覆盖或清理。
- 复用既有本地复制方式：`git clone --no-hardlinks --no-tags --single-branch --branch main
  <主仓库> <新根>/clean-source`。不联网、不建立 worktree，不切换主仓库分支。
- clone HEAD 为固定 SHA，tree 为 `5190a2063fce97f4ddec4357979a4c04e6c3a240`；
  起止 Git 状态均干净。`fixed-source-files.json` 保存 3254 个文件长度/Hash，完成时
  逐文件比较不变，见 `source-identity.log`、`source-final-git-status.log`、
  `source-final-byte-identity.log`。后来主仓库报告写入不影响快照或包来源。

## 2. 五包与身份

以下根相对仓库，均为目录包，尚未生成最终体验 ZIP：

| 包根 | 文件数 | YAML | 实际包外消费 |
| --- | ---: | --- | --- |
| `deliverables/sdk/a1c6010/source` | 102 | 可选作者源，默认 OFF | 综合、JSON 入门、YAML 各 Debug/Release，共 6 次 |
| `deliverables/sdk/a1c6010/static-Debug` | 42 | 静态附加组件 ON | 综合、JSON 入门、YAML Debug，各 1 次 |
| `deliverables/sdk/a1c6010/static-Release` | 42 | 静态附加组件 ON | 综合、JSON 入门、YAML Release，各 1 次 |
| `deliverables/sdk/a1c6010/shared-Debug` | 38 | PAE DLL + 静态 YAML 附加组件 | 综合、JSON 入门、YAML Debug，各 1 次 |
| `deliverables/sdk/a1c6010/shared-Release` | 38 | PAE DLL + 静态 YAML 附加组件 | 综合、JSON 入门、YAML Release，各 1 次 |

每包 `PROVENANCE.json` 都记录固定完整 `source_head`、`source_worktree_dirty=false`、
对应 kind/configuration/YAML 声明。原脚本中的 toolchain 字段为通用 `MSVC x64`；
本次实际版本、架构、CRT 和开关由下面的命令、Cache 和 Build 日志补充，不改元数据。
binary 通用说明仍只描述安装树字节；本次新构建/安装逐文件对照补齐来源链，不能仅
凭 Git 状态推断二进制来源。Hash 证明内部一致性，不提供签名或来源认证。

| 包 | PROVENANCE.json SHA256 | SHA256SUMS.txt SHA256 |
| --- | --- | --- |
| source | `F39B2458D092AEDC2243541562CB7C1B50FDEE4ADD10243B80C354E2AA4249DE` | `BAEE48A3F75302B72DD7547FD3620568D0B98B7CC99644705F55752C6AD25E44` |
| static-Debug | `DC2B594534E586E97039CBD95D9FC43E24546AC6C0F743C0DAD6A2A32A729762` | `4D27C392726E14C7725D8AD298B68B25DF7F77183D7F7BF96F7643B45B0259CB` |
| static-Release | `0A0449A22A975D41DFDA518005BEC03BD6BD547520E5116FC75E172B27AC1956` | `C70983F83869ED7D37160285A830F630A7EEB99EDC47C7AFA5F46BC020633429` |
| shared-Debug | `F24335B0D339919792E53688ABE49A30329C5053961306A4E0C970AA45B2B8F5` | `7A98E33866EB24F81B4577733F04B4CDC4B12DD1907DBA060B1B0B4E106D3AD7` |
| shared-Release | `195D08EFA68E148F14430DF9E195DA3BBB306E99BC8064B9A959DBD197493F15` | `7344DECB9BF39A91E6C378883CBB5342AB8AE2B1A4662FDB82ACCA4A561EDFE9` |

所有 payload 的相对路径、长度和散列在各自 MANIFEST/SHA256SUMS；摘要见
`five-package-summary.log`，最终五份 `final-*-verify.log` 均通过。Source 95 个同路径
文件与 clone 字节一致，consumer 两别名和 yyjson 许可副本另核对，共 98 项身份对照，
其余 4 项为制包元数据；`file-identity.log` 保存详细对照。

`key-comment-source-identity.log` 核对 metadata、Decimal、YAML frontend、公开 compiler、
description、compiled state 和 SDK 指南与固定提交一致，包含最新注释。二进制公开头、
YAML 头、四份 Schema/Profile、SDK 指南及 rapidyaml 通知也与快照相同。
四组安装树的 pae.lib/YAML 库、两组 DLL 和 Static 各五个内部库均与本次构建字节一致；
见 `file-identity.log`、`extended-install-identity.log`。Shared 不携带 YAML DLL。

## 3. 实际构建和包外验证

工具链为 Visual Studio 18 2026、x64、v142 `14.29.30133`（编译器识别 `19.29.30159.0`）、
Windows SDK `10.0.22621.0`，CRT Debug `/MDd`、Release `/MD`。未设 CL/_CL_，未改全局
环境、Qt 或依赖。只存在空闲 MSBuild nodeReuse worker 时开始，未擅自终止进程。

实际执行形状如下；S 为 clean-source，B 为新 static-build/shared-build，P 为对应新包：

```powershell
cmake -S $S -B $B -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_YAML_FRONTEND=ON `
  -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF `
  -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_PROTOCOL_LAB_UI=OFF
# shared 的 BUILD_SHARED_LIBS=ON；每个根串行 Debug，然后 Release。
ctest --test-dir $B -N
cmake --build $B --config $Configuration --target pae_public_api pae_yaml_frontend --parallel 4
cmake --install $B --config $Configuration --prefix $P
& "$S/scripts/package_sdk_stage3.ps1" -Kind $Kind -PackageRoot $P -Configuration $Configuration -SourceRoot $S
& "$S/scripts/verify_yaml_sdk_package.ps1" -PackageRoot $P -Kind $Kind -HasYaml:$true
```

Source 另用原脚本 `-Kind source -Configuration Debug+Release -SourceRoot $S`，验证使用
`-HasYaml:$false`（有作者源，不是安装组件）。Configure/Build/Install/Package/Verify
均退出 0；两个产品配置 CTest 清单均 0 项，不将此表述成产品测试通过。

包外示例为包内 `examples/sdk_consumer`、`getting_started` 和 `yaml_sdk_consumer`，
每个包/示例用独立新根。Source 的 `PAE_SOURCE_DIR` 指向新 source 包，不回读开发仓库；
binary 同时明确 `PAE_DIR=<对应包>/lib/cmake/PAE` 和 `CMAKE_PREFIX_PATH=<对应包>`。
实际 Cache 的路径经规范化比对，见 `consumer-package-identity.log`。18 次有效运行全为
退出 0，分别精确检查 `PAE_SDK_STAGE3_CONSUMER_PASS`、`GETTING_STARTED_BINARY_PASS`、
`PAE_YAML_SDK_CONSUMER_PASS`。这是直接执行程序的内部断言，不是 18 个 CTest 项。
入门程序独立验证 `AA 00 07` Decode/Encode；综合程序检查 compiler、metadata、physical
query、ASCII facts、codec、framer、host 和 multi-message encode。

Shared 六份 consumer 相邻 DLL 与对应新包 DLL Hash 一致，未借用旧 DLL。生成项目的
ClCompile 输入仅位于新候选包或新隔离构建根，不从开发树 src 编译私有实现；见
`consumer-clcompile-closure.log`。这不是任意宿主和全工具链的隔离证明。

主要日志均相对本次 evidence 根：

- `local-clone.log`、`source-{package,verify}.log`、`{static,shared}-configure.log`。
- `{static,shared}-{Debug,Release}-{build,install,package,verify}.log`、两份 test-inventory。
- `source-sdk_consumer-{configure,Debug-build,Debug-test,Release-build,Release-test}.log`。
- `source-yaml_sdk_consumer-{configure,Debug-build}.log` 和
  `resume-source-yaml_sdk_consumer-{Debug-test,Release-build,Release-test}.log`。
- `resume-source-getting_started-{configure,Debug-build,Debug-test,Release-build,Release-test}.log`。
- 首个 static-Debug 综合 Configure 为 `resume-static-Debug-sdk_consumer-configure.log`；
  后续 binary 使用 `resume3-<包>-<示例>-{configure,<配置>-build,<配置>-test}.log`。
- 原始 `transcript.log` 与 `resume-consumers-transcript.log`、`resume2-consumers-transcript.log`、
  `resume3-consumers-transcript.log` 保留全部命令和异常，不覆盖失败证据。

辅助验证曾有三次失败：首次 YAML consumer 漏传配置路径，正确返回 usage/退出 2；
第一次续跑按字符串分隔符比较 Windows Cache 路径误拒绝；第二次续跑对 PAE_DIR Cache
类型只接受 PATH，但显式 -D 产生 UNINITIALIZED，误拒绝。均仅修 out 辅助脚本，改为
按值规范化路径核对，保留旧日志，不更改产品或包。已成功的 Configure/Build/运行没有
无故重做，续跑仍使用本次原先新建且身份核对正确的根。最终 resume3 退出 0，标记
`ALIGNED_SDK_VALIDATION_COMPLETE`。Shared 构建既有 C4251 警告保留，不宣称零警告。

## 4. 给后续归集的文档和示例盘点

本轮保持固定来源字节，不能把历史体验 overlay 改贴为本提交产物。逐文件盘点见
`reader-examples-inventory.log`，链接枚举见 `markdown-links.log` 和摘要：

- Source 21 个 inline 本地链接，14 个缺目标；每个 binary 11 个，9 个缺目标，无逃出包根。
  这是简单 inline 链接枚举，不是完整 Markdown 解析或纯文本路径闭包证明。
- Source 保留 Codec/Framer/Host README 引用，但对应历史验证报告未在白名单；Schema
  执行语义和 Strict JSON Profile 的历史验证/诊断文档引用缺失。third_party README
  引用 qt-package.md，但 SDK 不携带 Qt；该链接缺失不能用复制 Qt 来解决。
- docs/sdk/README.md 的 b12ad80 七包描述及旧宿主示例问题说明属于历史文字，不代表
  本轮 a1c6010 五包事实；后续归集须明确历史与当前导航，而非重写本次 provenance。
- 固定提交的 docs/experience/tutorials 共 10 个文件未随 SDK 脚本打入：两份配置、
  README、五份教学 Markdown、CMakeLists 和 verify.cpp。应按当前固定提交另行归集，
  不偷用旧 tutorial overlay。当前未执行这些教学程序。
- Source 中 public_api_compile/codec/framer/host/sdk_consumer 的跟踪文件均已包含，
  但各独立程序没有逐一运行；此次只运行表列三个入口。getting_started 三个程序/说明
  文件包含，验证记录.md 不包含；binary 按安装规则只带综合、入门、YAML 三个入口。

上述不妨碍已验证的 SDK 编译/运行路径，但阻止把五包称为已闭包的完整阅读体验。
本轮不改白名单或旧文档，不更新交付入口；交由总控决定后续归集范围。

## 5. Lab 可消费路径与最终停点

下一片 Lab 可按配置选择上述四个 binary 独立根，推荐本轮
`deliverables/sdk/a1c6010/shared-Release` 或 static-Release；Debug 必须选对应 Debug 包。
公共 CMake 消费参数为明确 `-DPAE_DIR=<所选根>/lib/cmake/PAE`、
`-DCMAKE_PREFIX_PATH=<所选根>`，生成器保持 VS18/x64/v142 同版本；Lab 自己的现有
package-mode/Qt/路由开关应由 Lab 任务查当前契约，不把此 SDK 片当成 Lab 构建授权。
Shared 运行时仅取所选根 bin/pae.dll；YAML 为同包静态附加库。

本轮未做 JSON-only 负例新包、宿主嵌入 CTest、跨配置/CRT/工具链负例全矩阵、SDK
搬迁、教学全用例、UI/Lab、网络、打包 ZIP、Linux、真实协议 Golden、设备或现场；
不下载依赖，不新增性能、许可闭合或稳定 ABI 承诺。未改旧指纹或历史证据。

主仓库分支/HEAD 不变，暂存为空，唯一 Git 候选增量为本报告；out 和五包保持忽略。
报告 UTF-8、git diff --check 与定向敏感路径检查另留最终日志，不声称全面保密审计。
未 Stage、Commit、Push、Pull、Reset、Clean、Stash、切换分支、清理或发布。
已停止构建/写入，完成一次总控反馈，状态为**已完成派发范围，待总控复核**。
