# 可移植脚本与示例路径治理验证（2026-09-22）

## 1. 结论与边界

本片在 `main@657d0854750b81a76c85868a6d098d0f98505b85` 上完成最小脚本和示例路径治理：

- 两个已经履行完授权范围的一次性删除脚本永久退役；保留原参数，但任何调用都在文件系统操作前
  失败关闭，不再把旧机器目录改造成可复用删除工具。
- 所有待修改文件的原始字节与 SHA-256 已先保存到 Git 忽略的
  `deliverables/evidence/portable-paths/`，修改后再次复算原件 Hash 全部一致。
- SDK 打包脚本生成的 consumer 构建目录改为从 `$PackageRoot` 的父目录推导；不再生成盘符占位路径。
- getting_started、历史验证记录和 standalone README 使用仓库相对路径或语义别名；历史路径只保留
  原执行关系，不再伪装成当前可执行命令。
- 新增只读路径检查器。它能检查全部 Git 跟踪文本，也可在 `-ChangedOnly` 模式中叠加本次未跟踪候选；
  排除 vendored Qt 与 yyjson 上游原件，区分 URL，不扫描 Git 历史或二进制。
- 使用归集后的 `deliverables/sdk/7b4205e/pae-sdk-static-release/` 在全新忽略目录完成 Release
  Configure/Build/Run，输出 `GETTING_STARTED_BINARY_PASS`。没有重新打包 SDK。

本片未修改产品源码、根 CMake、Qt/PDB、第三方源码、原始 trial 或 portable 计划；未下载、删除、
Stage、Commit 或 Push。

## 2. 原文证据与退役脚本

修改前先逐文件复制并复算，以下六个原件全部保持原 Hash：

| 原仓库文件 | 原始 bytes | 原始 SHA-256 |
| --- | ---: | --- |
| `scripts/cleanup-generated-artifacts-20260919.ps1` | 33888 | `aa36dc41ed43ce857bea55749041cd8a973a636f6f1ecec16b1ca8b33d11dd5e` |
| `scripts/cleanup-seven-builds-20260922.ps1` | 5811 | `5f3ab2a15f747b08c84daaeef6c108ca8514795aeb3ade343416d11a5c360bc4` |
| `scripts/package_sdk_stage3.ps1` | 13835 | `030bb13d5ed7a8666135ec547aa643fc00857d2f6f1c8ca985beba60ad89c977` |
| `examples/getting_started/README.md` | 3507 | `1880221407153f7b5325c47298f8c9da42bac1f75f9ad292afdb30549daefd48` |
| `examples/getting_started/验证记录.md` | 5845 | `ca6a0e7a7e2cb1ba7a4afe6bab50a09d1efd388ce8af6ce2fe86dfa4732c40f8` |
| `tools/protocol_lab_ui/standalone/README.md` | 3668 | `5fa625bb4c4c69d9886e77c611d43802f35fdd8bccf59dcf3820b7d6f3c5f87c` |

证据清单 `deliverables/evidence/portable-paths/README.md` 只记录仓库相对源路径、证据相对路径、长度和
Hash；原脚本字节保存在其 `scripts/` 子目录，其他原文保存在 `originals/`。整个证据根受 `.gitignore`
排除，不进入公开候选。

两个仓内清理脚本现在只保留原参数签名和永久退役诊断。验证了：

| 脚本 | 调用方式 | 退出码 | 文件系统动作 |
| --- | --- | ---: | --- |
| `cleanup-generated-artifacts-20260919.ps1` | 无参数 | 1 | 无 |
| 同上 | `-Execute` | 1 | 无 |
| 同上 | `-VerifyAfter` | 1 | 无 |
| `cleanup-seven-builds-20260922.ps1` | 无参数 | 1 | 无 |
| 同上 | `-Execute` | 1 | 无 |

每次均精确出现 `permanently retired`，且 `throw` 之前没有创建、写入、移动或删除语句。旧原件可从
Git 忽略证据恢复以供审计，但不得再次执行或泛化。

## 3. 路径治理改动

### 3.1 SDK 生成说明

`scripts/package_sdk_stage3.ps1` 生成说明中的 `$BuildRoot` 现在使用：

```powershell
$BuildRoot = Join-Path (Split-Path -Parent $PackageRoot) "pae-sdk-consumer-build"
```

现场检查该生成字面量精确出现一次，脚本中盘符绝对路径命中为 0。没有执行 SDK 打包；本项是脚本
解析与生成文本静态检查，不冒充新包验证。

### 3.2 README 与历史记录

- `examples/getting_started/README.md` 用 `$PackageRoot` 父目录构造新 build root，仍要求包外新目录。
- `examples/getting_started/验证记录.md` 用 `<historical-sdk-root>` 和 `<historical-repo-root>` 保存
  2026-09-21 的历史执行关系，明确这些别名不是当前可粘贴命令；当前候选单独指向
  `deliverables/sdk/7b4205e/`，未追溯改写旧测试来源。
- `tools/protocol_lab_ui/standalone/README.md` 保留 clean `98df5e0` 的历史身份，同时把当前本地候选分开
  指向 `deliverables/sdk/7b4205e/` 和
  `deliverables/lab/fe1683c-plus-patches/static-release/`。后者仍明确是 `fe1683c` 基线加修补，
  不是 clean `657d085` 构建或正式发布。

归集后现场核对：SDK 根 189 文件、29,955,541 bytes；Lab static Release 根 18 文件、21,482,235
bytes，含一个 Lab EXE 和 13 个配置。该计数只确认归集闭包存在，不替代 Qt 许可或发布审查。

## 4. 路径检查器

新增 `scripts/check-portable-paths.ps1`：

- 默认从 `git ls-files` 取得全部跟踪文件；`-ChangedOnly` 改查已修改跟踪文件；`-CandidatePath` 显式
  加入本任务未跟踪候选文件或目录。
- 只读取已知文本扩展名和 `CMakeLists.txt`，不读取二进制。
- 检查 Windows drive、UNC、个人 Unix home 三类机器绝对路径；输出只含 kind、仓库相对文件和行号，
  不再次打印敏感路径正文。
- 排除 `third_party/qt/**`、yyjson 锁定源码和 License；不排除项目自写的第三方说明。
- 所有候选必须解析在仓库内，越界或缺失立即失败；脚本不写文件。

验证结果：

| 场景 | 结果 |
| --- | --- |
| 本任务边界首次检查 | `PORTABLE_PATH_CHECK_PASS files=7 hits=0`，退出 0 |
| 最终共享工作树 changed-only + 本报告/检查器 | `PORTABLE_PATH_CHECK_PASS files=84 hits=0`，退出 0 |
| 全部跟踪文本（并行文档治理前） | 检出既有债务 1,731 行，退出 2 |
| 全部跟踪文本（并行文档治理后） | `PORTABLE_PATH_CHECK_PASS files=667 hits=0`，退出 0 |
| PowerShell Parser | 四个相关脚本均 0 parse error |

治理前负向结果与治理后正向结果共同证明检查器既能发现既有债务，也没有因排除规则而无条件通过。
并行任务对 docs、deliverables 和 third_party 说明的改动不归属本片，仍须由总控独立复核。

## 5. getting_started Release 实证

输入：

- source：仓内 `examples/getting_started/`；
- installed SDK：`deliverables/sdk/7b4205e/pae-sdk-static-release/`；
- build：Git 忽略的 `out/portable-consumer-20260922/`；
- generator/toolset：项目已验证的 Visual Studio 2026 x64 / v142 14.29.30133；
- configuration：Release。

实际结果：

| 步骤 | 退出码 | 证据 |
| --- | ---: | --- |
| Configure | 0 | `out/portable-consumer-20260922/configure.log` |
| Build `pae_getting_started` | 0 | `out/portable-consumer-20260922/build-release.log` |
| Run | 0 | `out/portable-consumer-20260922/run-release.log` |

运行输出精确包含：

```text
CONFIG schema=0.9 protocol=synthetic_stream_framing
PIPELINE id=fixed_rx message=fixed_message
DECODE frame=AA 00 07 field=fixed_payload value=7
ENCODE value=7 frame=AA 00 07
GETTING_STARTED_BINARY_PASS
```

`CMakeCache.txt` 的 `CMAKE_PREFIX_PATH` 和 `PAE_DIR` 均解析到上述仓内相对 SDK 候选；Release 程序旁
没有 `pae.dll`，与 static 包一致。未执行 Debug、shared、SDK 重打、完整产品/Lab 测试或网络验证。

## 6. 修改清单与剩余边界

本任务跟踪候选：

- `scripts/cleanup-generated-artifacts-20260919.ps1`
- `scripts/cleanup-seven-builds-20260922.ps1`
- `scripts/package_sdk_stage3.ps1`
- `scripts/check-portable-paths.ps1`
- `examples/getting_started/README.md`
- `examples/getting_started/验证记录.md`
- `tools/protocol_lab_ui/standalone/README.md`
- `docs/engineering/portable-script-validation-20260922.md`

剩余边界：当前工作树的全部跟踪文本已经由并行任务治理到检查器 0 命中，但这些共享改动不归属本片，
仍待总控逐项复核。Git 历史、Git 忽略原始证据和第三方二进制不属于文本门禁；Qt 来源/许可、项目级
PAE License 和二进制内嵌构建路径也不在本片闭合。本次 Release smoke 只证明归集后的 static SDK
可被最小 consumer 使用，不形成正式发布、Linux、ABI、真实协议、设备或现场结论。

## 7. `core.quotepath` 异机返修（总控限定复核）

总控指出：初版检查器直接继承用户级 `core.quotepath`。本机配置为 `false` 时中文路径可直接使用，
但异机默认 `true` 会让 `git ls-files` / `git diff --name-only` 输出带引号的八进制转义路径；旧实现随后
`Test-Path` 失败并静默跳过，存在假通过风险。该问题实际成立。

最小修复：检查器的 `rev-parse`、全量 `ls-files`、unstaged diff 和 staged diff 均在单次只读 Git 命令上
显式设置 `core.quotepath=false`；unstaged diff 同时保留命令级 `core.safecrlf=false`。没有读取或修改用户、
仓库或全局 Git 配置，也没有改变检测范围。

在 Git 忽略的 `out/portable-path-check-fixture-20260922/` 创建独立临时仓库并把该仓库
`core.quotepath` 设置为 `true`。实际结果：

| 用例 | 原生 Git 表现 | 检查器结果 |
| --- | --- | --- |
| 中文干净文件 | 文件名带引号并以八进制转义 | `files=1 hits=0`，退出 0，证明文件被计入而非跳过 |
| 中文文件含合成 Windows drive 路径 | 文件名仍被转义 | `files=2 hits=1`，退出 2，输出解码后的中文相对文件名和精确行号 |
| 中文 staged + unstaged 各一处机器路径 | 两个原生 diff 均转义文件名 | `files=2 hits=2`，退出 2，两文件均命中 |
| 仓库外候选 | 不适用 | 退出 1，精确拒绝 `outside the repository` |

夹具只包含合成文本和独立 `.git`，没有触碰真实工作树 index。最终真实共享工作树复核保持：

- changed-only + 显式候选：`PORTABLE_PATH_CHECK_PASS files=84 hits=0`；
- 全部跟踪文本：`PORTABLE_PATH_CHECK_PASS files=667 hits=0`；
- 四个相关 PowerShell 文件 Parser error 均为 0；
- `git diff --check` 退出 0，暂存区为空。

另修正 `scripts/package_sdk_stage3.ps1` 新增 `$BuildRoot` 字面量相对同级数组元素多出的四个源代码缩进
空格；生成字符串内容不变。已通过的 getting_started consumer 未重跑，前述 Configure/Build/Run
证据仍是本轮唯一动态 consumer 结果。

状态：已完成派发范围，待总控复核。
