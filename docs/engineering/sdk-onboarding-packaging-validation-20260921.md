# SDK 教学入口与打包闭包验证（2026-09-21）

## 1. 结论

本片完成教学操作纠偏、包内中文首次运行指南、`getting_started` source/install 双入口和打包白名单
闭包。最终 source 候选与 static Release 候选均从 `main@e2527ca` 的当前 dirty 工作树生成，
provenance 如实记录 `source_worktree_dirty=true`；它们不是 clean checkpoint 或正式发布。

最终两包逐文件 Manifest/长度/Hash 复算通过；source/static 各自包外构建并运行最小示例和原综合
consumer，共四组均通过。所有新用户入口只依赖包内相对路径，文本扫描未发现共享开发仓库或旧
`pae-trial-7b4205e-20260920` 路径。

## 2. 实施内容

### 2.1 教学操作纠偏

`docs/guides/04-协议配置入门.md` 现在明确：

- 第一级 258 是修改 `getting_started/main.cpp` 的 `kExpectedFrame` 和重新构建，不是 CLI 数值参数；
- 既有 `public_api_codec` 只接收两个配置路径，当前硬编码 `ALICE/Z` 并断言
  `TX ALICE!Z\r\n`；`A/OK` 是配置映射阅读向量，不是该程序直接运行输入；
- 既有 `public_api_framer` 一次 Push `AA 00 01 AA 00 02`；7/8 分块是需要改源码、同步断言并
  重新构建的练习，不是原程序参数。

后三层仍是教学路线，没有在本片执行，不将预期写成动态证据。

### 2.2 包内入口

- `examples/getting_started/CMakeLists.txt`：source 包使用 `PAE_SOURCE_DIR`，安装包使用
  `find_package(PAE CONFIG REQUIRED)`；同步 shared DLL 邻接复制规则，但本片未验证 shared。
- `examples/getting_started/README.md`：所有命令从 `$PackageRoot` 出发，不含本机 SDK 或仓库路径。
- `docs/sdk/README.md`：中文自包含说明定位/设计边界、source/static/shared 选择、工具链、最小示例与
  综合 consumer 分工、字节到配置映射、常见错误、借用生命周期、Hash与许可边界。
- `cmake/PaeSdkInstall.cmake`：安装中文指南、最小示例三文件及其唯一必需配置；保留原
  `examples/sdk_consumer` 和 `share/pae/examples/config`。
- `scripts/package_sdk_stage3.ps1`：source 白名单显式加入上述入口，不携带 `验证记录.md`；binary
  完整性检查加入新文件；生成说明先指向中文指南和最小示例，同时保留综合 consumer。

## 3. source 闭包缺口及修复

首次 source 候选有 84 个 Hash 条目，可配置并构建 PAE，但从该包执行 static install 时失败：

```text
file INSTALL cannot find .../examples/public_api_sdk_consumer/CMakeLists.txt
INSTALL_STATIC_EXIT=1
```

原因是既有 source 脚本只把综合 consumer 重命名复制为 `examples/sdk_consumer`，而包内
`PaeSdkInstall.cmake` 仍引用源码布局 `examples/public_api_sdk_consumer`。这证明原白名单没有闭合
source 包自身的 install 路径，不是中文路径或 PAE 执行逻辑错误。

最小修复是在 source 白名单同时保留 `examples/public_api_sdk_consumer` 原目录；用户入口
`examples/sdk_consumer` 不变。失败候选、部分 install tree 和日志均保留在隔离根，未覆盖或伪装成
最终结果。修复后的最终 source 包为 86 个 Hash 条目，并成功完成全新 configure/build/install。

## 4. 输入身份与最终包

开始时共享仓库：

- branch：`main`
- HEAD：`e2527cad500f928100b588fe433e891d460bbc6a`
- staged：0
- 工作树包含总控/工程整理的入口文档变化及本任务教学文件，属于 dirty snapshot；完整开始状态见
  `out/sdk-onboarding-20260921/evidence/source-snapshot-status.txt`。

最终候选：

| 包 | 类型/配置 | 总文件/Manifest/Hash | `SHA256SUMS.txt` SHA-256 | provenance |
| --- | --- | ---: | --- | --- |
| `pae-sdk-source-final` | source / Debug+Release | 87 / 85 / 86 | `ceed7ed960ce5736ad430ee7eb1396604c0673dcbbc69714b89a6237554696ac` | `e2527ca`、dirty=true |
| `pae-sdk-static-release-final` | static / Release | 34 / 32 / 33 | `9b9016f7998c4b36cb9489d4b0e6b5a4f7241576a3e2253488025ec17321d5e2` | `e2527ca`、dirty=true |

最终 source 包是显式白名单快照；static Release 的 CMake source 为该包，而不是共享仓库。binary
打包脚本只从共享仓库读取 Git HEAD/dirty provenance，没有从共享源码补充安装内容。

## 5. 实际构建与包外消费

工具链：Visual Studio 18 2026、x64、`v142,version=14.29.30133`、MSVC 19.29.30159.0、
Windows SDK 10.0.22621.0。

### 5.1 最终 source 与 static 生成

命令形状：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package_sdk_stage3.ps1 `
  -Kind source -PackageRoot <source-final> -Configuration Debug+Release `
  -SourceRoot <shared-repo> -Toolchain 'Visual Studio 18 2026 x64 v142 14.29.30133'

cmake -S <source-final> -B <static-build> -G "Visual Studio 18 2026" -A x64 `
  -T "v142,version=14.29.30133" -DBUILD_SHARED_LIBS=OFF `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_PUBLIC_API_STAGE1=ON
cmake --build <static-build> --config Release --target pae_public_api -- /m:4
cmake --install <static-build> --config Release --prefix <static-final>
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/package_sdk_stage3.ps1 `
  -Kind static -PackageRoot <static-final> -Configuration Release `
  -SourceRoot <shared-repo> -Toolchain 'Visual Studio 18 2026 x64 v142 14.29.30133'
```

最终调用均退出 0。第一次完整 build 接近工具等待边界时已生成目标但未打印调用方尾标，随后在同一
build root 用 `/m:1 /nr:false` 增量确认退出 0；没有并发启动第二个同目录构建。

### 5.2 四组包外 consumer

| Consumer | PAE 来源 | Configure/Build/Run | 运行标志 |
| --- | --- | --- | --- |
| source/getting_started | `PAE_SOURCE_DIR=<source-final>` | 0 / 0 / 0 | `GETTING_STARTED_BINARY_PASS` |
| source/sdk_consumer | `PAE_SOURCE_DIR=<source-final>` | 0 / 0 / 0 | `PAE_SDK_STAGE3_CONSUMER_PASS ... multi_message_encode=1` |
| static/getting_started | `CMAKE_PREFIX_PATH=<static-final>` | 0 / 0 / 0 | `GETTING_STARTED_BINARY_PASS` |
| static/sdk_consumer | `CMAKE_PREFIX_PATH=<static-final>` | 0 / 0 / 0 | `PAE_SDK_STAGE3_CONSUMER_PASS ... multi_message_encode=1` |

两个 static Cache 的 `PAE_DIR` 均位于最终 static 包内；两个 source Cache 的
`CMAKE_HOME_DIRECTORY` 与 `PAE_SOURCE_DIR` 均位于最终 source 包。四个程序相邻目录均无
`pae.dll`，符合本片 source/static 形态。

## 6. 完整性与入口检查

`out/sdk-onboarding-20260921/evidence/verify-packages.ps1` 最终输出：

```text
SDK_ONBOARDING_VERIFICATION_PASS packages=2 consumers=4
```

它实际检查：

- 两包必需入口存在，`examples/getting_started/验证记录.md` 不在包内；
- provenance 的 HEAD、dirty、kind、configuration；
- Manifest 文件集合与长度；
- SHA256SUMS 文件集合与逐文件实际 Hash；
- 四 consumer CMake 来源、运行标志、可执行文件和静态形态；
- 包内用户文本对共享开发仓库绝对路径及旧试用根的泄漏数为 0。

辅助复核脚本早期先后暴露 `0U` 非 PowerShell 字面量、`$home` 与 `$HOME` 大小写冲突、Windows
PowerShell 5.1 按系统代码页读取无 BOM UTF-8 CMakeCache 等脚本问题；对应失败日志均保留。显式
`-Encoding utf8` 后最终 `verification-final6.log` 退出 0。这些不是包内容或 consumer 失败。

## 7. 证据路径与边界

隔离根：`out/sdk-onboarding-20260921`。

主要证据：

- `evidence/package-summary.json`
- `evidence/consumer-summary.json`
- `evidence/verification-final6.log`
- `evidence/package-source-final.log`
- `evidence/configure-static-release-final.log`
- `evidence/build-static-release-final.log`
- `evidence/install-static-release-final.log`
- `evidence/package-static-release-final.log`
- `evidence/consumer-*-{configure,build,run}.log`

未验证 Debug、shared、Linux、跨工具链、稳定 ABI、正式许可、Lab/Qt、网络、真实协议、设备或现场。
shared DLL 复制规则和 Debug 包装规则只做了同步及静态检查，不标记通过。本片没有修改根 CMake、
公共 API、Schema、执行源码、canonical 配置、旧 SDK 或 Lab 部署，也没有 Stage、Commit、Push、
发布或删除失败证据。
