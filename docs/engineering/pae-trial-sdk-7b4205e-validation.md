# PAE 7b4205e 同批试用 SDK 生成与包外消费验证

## 1. 结论与边界

本次从固定提交 `7b4205ea899cf16b9c73ba6ebc4c64382b71dc63` 的全新 detached clone 生成 5 份 SDK 包，并完成 6 组真实包外 consumer 的配置、构建和运行。5 份包的 `PROVENANCE.json`、`MANIFEST.txt`、`SHA256SUMS.txt` 以及包内文件长度和 Hash 全量复核通过；6 组 consumer 均输出完整 `PAE_SDK_STAGE3_CONSUMER_PASS`，且各构建树注册测试数为 0。

本次证据证明同一提交、同一 Windows/MSVC 工具链下的源码包、静态包和动态包可被仓库外 consumer 使用。它不是正式发布、稳定 ABI、跨工具集、Linux、Qt Lab、许可证、真实协议 Golden、硬件或现场验收结论。

开始时共享仓库为 `main@7b4205e`，暂存区为空，但已有其他任务的 Markdown 修改；本任务未将共享工作树作为 SDK 输入，也未修改或覆盖这些变化。

## 2. 隔离目录与工具链

- 试用根：`F:\PersonalWorkspace\pae-trial-7b4205e-20260920`
- 干净源码：`source`，固定提交 detached checkout，生成前后 `git status --short` 均为空
- SDK：`sdk`
- 包外 consumer：`consumers`
- 构建与日志：`evidence`
- CMake：`4.3.1-msvc1`
- Generator：`Visual Studio 18 2026`，`x64`
- Toolset：`v142,version=14.29.30133`
- Compiler：`MSVC 19.29.30159.0`
- Windows SDK：`10.0.22621.0`

首次 clone/checkout 的组合 PowerShell 命令在 detached HEAD 下对空分支名调用 `.Trim()`，因此包装命令退出 1；clone 和精确 checkout 已实际完成。随后独立核对 HEAD、detached 状态和空工作树后继续，没有重新 clone、覆盖或掩盖该日志。原始记录见 `evidence/clean-source-clone.log`。

## 3. 生成命令与五包身份

源码包使用现有 `scripts/package_sdk_stage3.ps1 -Kind source`。二进制包先以以下共同条件配置，再严格串行构建 Debug、Release：

```powershell
cmake -S <clean-source> -B <build> -G "Visual Studio 18 2026" -A x64 `
  -T "v142,version=14.29.30133" -DBUILD_SHARED_LIBS=<OFF|ON> `
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_PUBLIC_API_STAGE1=ON
cmake --build <build> --config <Debug|Release> --target pae_public_api -- /m:1
cmake --install <build> --config <Debug|Release> --prefix <fresh-package-root>
powershell -NoProfile -ExecutionPolicy Bypass -File <clean-source>\scripts\package_sdk_stage3.ps1 `
  -Kind <static|shared> -PackageRoot <package-root> -Configuration <Debug|Release> `
  -SourceRoot <clean-source> -Toolchain "Visual Studio 18 2026 x64 v142 14.29.30133"
```

所有调用退出码为 0。下表中的文件数包含 `MANIFEST.txt` 和 `SHA256SUMS.txt`；清单条目不包含这两个生成文件，Hash 条目包含 `MANIFEST.txt`。

| 包 | 类型/配置 | 文件/清单/Hash 条目 | `SHA256SUMS.txt` SHA-256 |
| --- | --- | ---: | --- |
| `pae-sdk-source` | source / Debug+Release | 81 / 79 / 80 | `eca5314ea927e81d43dd8f157e841684508ca6a61dbea9db8bb179c5bfc9a546` |
| `pae-sdk-static-debug` | static / Debug | 29 / 27 / 28 | `a4e3b1fdfd4e8bff95ea6118eec543fb8dd6e0691acdffc8e6797511975a107e` |
| `pae-sdk-static-release` | static / Release | 29 / 27 / 28 | `546d6e562d06b3181aa7bc25683c2c67be5cd4293716d23ffa0d9bc94e0f0248` |
| `pae-sdk-shared-debug` | shared / Debug | 25 / 23 / 24 | `a481c82b49e4d9a5007e7a3ee177c0004179cd1e7fdbad5c2e10dcd8b89f823c` |
| `pae-sdk-shared-release` | shared / Release | 25 / 23 / 24 | `f197f891f4ece48e992a4adfccf9c26de6ff0d565e6cffa6bce8d8ad5ac468ab` |

五份 `PROVENANCE.json` 均记录精确提交 `7b4205e...` 和 `source_worktree_dirty=false`。验证脚本按清单集合、记录长度和文件 SHA-256 逐项复算，未只比较清单文件自身 Hash。

## 4. 六组包外消费

源码 consumer 只通过 `PAE_SOURCE_DIR=<source-package>` 配置；四组二进制 consumer 只通过各自的 `CMAKE_PREFIX_PATH=<binary-package>` 配置。实际 `CMakeCache.txt` 中的 `PAE_DIR` 均解析到对应包内，未落回共享仓库或干净源码树。

| Consumer | 配置/构建/运行 | 注册测试 | 结果 |
| --- | --- | ---: | --- |
| source-debug | 0 / 0 / 0 | 0 | PASS |
| source-release | 0 / 0 / 0 | 0 | PASS |
| static-debug | 0 / 0 / 0 | 0 | PASS |
| static-release | 0 / 0 / 0 | 0 | PASS |
| shared-debug | 0 / 0 / 0 | 0 | PASS |
| shared-release | 0 / 0 / 0 | 0 | PASS |

六组运行均输出：

```text
PAE_SDK_STAGE3_CONSUMER_PASS compiler=1 metadata=1 physical_query=1 ascii_facts=1 codec=1 framer=1 host=1 multi_message_encode=1
```

动态包运行时相邻 `pae.dll` 与包内 DLL Hash 一致：

- Debug：`ee102cf2d7cde67c7a588a88ddbb92a1028ec0e23dba112302440849dcae9e95`
- Release：`f9d378bb29d6b80cf6301bcce9a38d4c2fd7e3df82d1f2c0f69fe245cbb6b5ec`

静态 consumer 目录中不存在 `pae.dll`。动态库 Debug、Release 构建各出现 15 条既有 C4251 警告；本次没有将同工具链消费成功扩大为稳定 ABI 结论。

## 5. 独立复核与证据

`evidence/verify-trial.ps1` 对五包、六 consumer、CMake 来源、动态库身份和警告计数执行复核，退出码 0：

```text
TRIAL_VERIFICATION_PASS packages=5 consumers=6
```

主要机器可读证据：

- `evidence/package-identities.json`
- `evidence/consumer-matrix-summary.json`
- `evidence/consumer-cmake-origins.json`
- `evidence/shared-runtime-hashes.json`
- `evidence/shared-warning-summary.json`
- `evidence/toolchain.json`
- `evidence/integrity-and-origin-verification.log`
- `evidence/clean-source-final-status.log`

另对 5 个包内 172 个文本文件扫描共享仓库路径和干净 clone 路径，命中数均为 0，见 `evidence/development-path-leaks.log`。该扫描不替代二进制调试信息审计，也不构成可重定位或发布认证。

## 6. 未执行及 Git 边界

- 未运行 Linux、跨工具链、稳定 ABI、Qt Lab、真实协议 Golden、硬件或现场验证。
- 未执行网络收发、人工 UI 验收、正式发布或部署替换。
- 未修改功能、公共 API、Schema、CMake 或打包脚本。
- 未 Stage、Commit、Push，也未清理共享仓库已有变更。

本次共享仓库唯一新增候选文件是本报告；隔离根及其生成物位于仓库外，不属于 Git 候选。
