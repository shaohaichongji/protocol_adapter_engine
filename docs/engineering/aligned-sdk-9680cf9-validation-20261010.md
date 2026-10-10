# 9680cf9 同基线 SDK 五包验证（2026-10-10）

## 1. 阶段 A 结论与来源

按本轮授权，从固定提交 `9680cf90512f053962949cb55467d6af35959088` 的独立干净
本地源码快照全新构建五个 SDK 目录包，18 次包外消费通过，逐包长度/Hash/provenance
核验通过。SDK 构建时段已结束，停止写入并等待总控复核；未开始 Lab、整体体验归集
或正式发布。没有将旧包改名充作本基线，没有更改产品、原制包脚本或旧报告。

接管 main/HEAD 为固定 SHA，暂存及工作树干净，没有运行中的产品构建；新构建根、
候选根及报告均不存在。读取外层 AGENTS、现有脚本与 513f6cc 验证流程后，使用既有
本地独立 clone 方法，不联网、不建 worktree、不切换主仓库分支：

```powershell
git clone --no-hardlinks --no-tags --single-branch --branch main <主仓库> <新根>/clean-source
```

- 新构建根：`out/build/aligned-sdk-9680cf9-20261010/`。
- 干净源码快照：`out/build/aligned-sdk-9680cf9-20261010/clean-source/`。
- 本轮证据：上述新构建根的 `evidence/`，下文日志均相对此目录。
- clone HEAD 核对固定完整 SHA，tree 为 `44c602b6d2410fb24d1ceeac4c6c1a10265a7744`；
  起止状态干净，3263 个跟踪文件长度/Hash 起止不变。来源清单是
  fixed-source-files.json，身份与最终字节核验见 source-identity.log、
  source-final-git-status.log、source-final-byte-identity.log 和 final-quality.log。
- a1c6010、513f6cc 两轮五包及两份 SDK 报告共 526 文件另存保护 Hash，最终未变。
  既有产物、报告和失败日志不覆盖或删除；并行文档变更不进入本产品快照。

## 2. 五包与产品身份

五包为独立目录包，尚非最终体验 ZIP，均保持 Git 忽略：

| 仓库相对路径 | 文件数 | 本次有效消费 |
| --- | ---: | --- |
| `deliverables/sdk/9680cf9/source` | 102 | 综合、JSON 入门、YAML 各 Debug/Release，共 6 次 |
| `deliverables/sdk/9680cf9/static-Debug` | 42 | 三入口 Debug，共 3 次 |
| `deliverables/sdk/9680cf9/static-Release` | 42 | 三入口 Release，共 3 次 |
| `deliverables/sdk/9680cf9/shared-Debug` | 38 | 三入口 Debug，共 3 次 |
| `deliverables/sdk/9680cf9/shared-Release` | 38 | 三入口 Release，共 3 次 |

每包 MANIFEST.txt / SHA256SUMS.txt 列出完整 payload 相对路径、长度与 Hash；
PROVENANCE.json 的 source_head 为本轮完整 SHA、dirty=false、kind/config/YAML
声明正确。Source 的 YAML 作者源可选且默认 OFF；四 binary 带静态 YAML add-on，
shared 为 PAE DLL 加静态 YAML 库，不是 YAML DLL。原脚本 toolchain 字段为通用
MSVC x64，实际工具链以本报告及真实构建日志补充，不篡改包元数据。
binary 通用说明仅表示安装树字节，额外的新构建/安装字节对照补齐本轮来源链；
不能单凭 Git 状态证明二进制来源，Hash 也不是来源认证或防篡改保证。

| 包 | PROVENANCE.json SHA256 | SHA256SUMS.txt SHA256 |
| --- | --- | --- |
| source | `F6AC5979FEE8395E958AFAE5B9AAE2808C05640A68E6C2F7D477A1DF0CABA2B0` | `84D5EF4F6AF427F43826B2B74FE5B2968BE53143398DD4799D4B9A22E8391614` |
| static-Debug | `43974815D4F8FD4F3A4E2E464B02D6B21090ADA6356F7C15726B527ED30DE6B2` | `CE33619D365782D105D4F995AE1A5A163855BEC899DC606740216288761F6E49` |
| static-Release | `41DF6273601087569C065C1489B98737AA393ADE1A53B7931CDD264203C98479` | `D4BC0AA0D0DB7E20F41A8F2354C43A99FF5752E09BC0433AB92A8792599CFBAD` |
| shared-Debug | `EA8900333CAD1063936BA8028F5C699163801881B9C4E6E64B6458B31689FF09` | `F6FFAD4DF04A3169EBDAC8CEC4DDC543F32030658915DBB9EBF9689D68B1E4FD` |
| shared-Release | `1E98F020E97C56E8E31994BA6A58DF808A7DC21E4E1511BD599B6802CD2A0024` | `7797275A0AF894C5332E71B6D52A2B683FD7871B3C41B4F834EFA97211165C4F` |

摘要见 five-package-summary.log。原制包 verifier、独立审计及消费后再验证均通过，
见各 package/verify、final-*-verify 与 post-consumer-*-verify 日志。
Source 95 同路径文件、两 consumer 别名及一许可副本共 98 个非元数据文件与固定
快照字节一致；其余 4 文件是制包元数据。关键新注释核对见 key-comment-source-identity.log。

pae.lib、YAML 库、shared DLL 与本次构建输出一致，Static 每配置五个内部库另
逐项对照。公开头、YAML 头、四份 Schema/Profile、SDK 指南及 rapidyaml 通知等
70 项扩展检查通过，见 file-identity.log / extended-install-identity.log。

## 3. 工具链、实际命令与结果

VS18 2026 / x64 / v142 14.29.30133，MSVC 识别 19.29.30159.0，
Windows SDK 10.0.22621.0；CRT Debug /MDd、Release /MD。
未设置 CL/_CL_、修改全局环境或 Qt，也没有下载依赖。SDK 配置关闭 Lab/UI，
随仓 yyjson/rapidyaml 编译；生成输入及包文件无 Qt。
额外使用已核对工具集的 dumpbin /dependents 检查两份新 PAE DLL，退出均 0，
只有 Microsoft CRT/Windows 直接导入，没有 Qt；shared-*-dependents.log 保留记录。
这不扩大为完整静态符号、系统依赖闭包或任意宿主 DLL 搜索环境安全审计。

S 为干净快照，B 分别为新 static-build/shared-build，P 为对应新包：

```powershell
cmake -S $S -B $B -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_YAML_FRONTEND=ON `
  -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF `
  -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_PROTOCOL_LAB_UI=OFF
# shared 使用 BUILD_SHARED_LIBS=ON，每个根严格 Debug 后 Release。
ctest --test-dir $B -N
cmake --build $B --config $Configuration --target pae_public_api pae_yaml_frontend --parallel 4
cmake --install $B --config $Configuration --prefix $P
& "$S/scripts/package_sdk_stage3.ps1" -Kind $Kind -PackageRoot $P -Configuration $Configuration -SourceRoot $S
& "$S/scripts/verify_yaml_sdk_package.ps1" -PackageRoot $P -Kind $Kind -HasYaml:$true
```

Source 用 -Kind source -Configuration Debug+Release，HasYaml=false 表示不是安装
组件。全部产品 Configure/Build/Install/Package/Verify 退出 0；两个产品 CTest
清单为 0 项，不把关闭测试的产品构建说成产品 CTest 通过。

包内 sdk_consumer、getting_started、yaml_sdk_consumer 每包每例用新
consumers/<包>-<入口> 根。Source 明确 PAE_SOURCE_DIR=<新Source包>；
binary 明确 PAE_DIR=<所选包>/lib/cmake/PAE 与 CMAKE_PREFIX_PATH=<所选包>，
实际 Cache 按规范化路径值核对，见 consumer-package-identity.log。

```powershell
cmake -S <包内examples/入口> -B <独立consumer根> <相同生成器选项> <来源选择参数>
cmake --build <consumer根> --config <匹配配置> --target <入口目标> --parallel 4
& <consumer根>/<配置>/<程序>.exe <该包内合成配置的绝对路径>
```

18 次有效运行全为 exit 0 并检查精确成功标记 PAE_SDK_STAGE3_CONSUMER_PASS、
GETTING_STARTED_BINARY_PASS 或 PAE_YAML_SDK_CONSUMER_PASS；不是 18 个 CTest 项
或全域覆盖率。入门独立预期 AA 00 07；综合覆盖 compiler/metadata/physical query、
ASCII facts、Codec/Framer/Host/multi-message；YAML 转严格 JSON 后走公开编译/Codec。
共享输出目录的 Debug/Release 严格串行，没有共用旧 Cache/install/build。

93 条生成项目 ClCompile 输入均在新包或新隔离根，无开发树 src 编译输入；
六份 shared consumer 相邻 DLL 与对应新包 DLL 一致，见 consumer-clcompile-closure.log
和 file-identity.log。原始日志 transcript.log 保存完整命令/退出，流水线最终
exit 0 / ALIGNED_SDK_VALIDATION_COMPLETE；分步日志包括：

- source-package/verify.log、{static,shared}-configure/test-inventory.log；
- {static,shared}-{Debug,Release}-{build,install,package,verify}.log；
- <包>-<入口>-configure.log、<包>-<入口>-<配置>-build/test.log；
- 各 identity、final-*-verify、post-consumer-*-verify 与 final-quality.log。

首次 out 辅助脚本在已由本轮新建的 evidence 目录上重复 New-Item，exit 1，
发生在 clone/configure/build 之前。保留 initial-helper-failure.log；仅调整 out
辅助脚本的目录初始化，并防止覆盖已有运行 transcript，没有改原制包脚本或产品。
之后完整产品流水线及 18 次消费实际通过。Shared 既有 C4251 警告保留，不称零警告。

## 4. 已知限制、Git 与交接停点

固定快照的原 SDK 指南仍写 513f6cc 的旧计划 SHA，而本轮实际来源是 9680cf9；
不将其冒充本轮产品身份，也不原地改写原始包。原始 source 的 inline 链接
21 个缺 14，binary 各 11 个缺 9，无逃出包根；教学目录十文件、getting_started
验证记录未随原白名单入包。逐项 reader-examples-inventory/markdown-links 日志
保留。这些是后续独立文档投影/归集待办，不属于已验证程序运行失败。
本轮未消费或为并行 docs/experience 的 dirty 投影和其验证报告背书。

本任务唯一 Git 候选增量为本报告。最终 main/HEAD 为固定完整 SHA，暂存区空；
并行文件清单单独见 report-final-status.log，不归本任务修改成果。
报告 UTF-8 无 BOM、无尾空白、定向便携路径检查和十个元数据 Hash 核对通过，
git diff --check 退出 0。产品、原脚本、旧报告与旧包未改；out/五包保持忽略。
未 Stage/Commit/Push/Pull/Reset/Clean/Stash/切换分支/下载/清理/发布。

Lab 下一片可消费上述新 static/shared 对应 Debug/Release 根，显式 PAE_DIR 与
CMAKE_PREFIX_PATH 指向同一包；shared DLL 只取该根 bin/pae.dll，YAML 是同包 add-on。
干净快照可供总控核对，但 Lab 自身 Qt/package-mode/路由开关由下一片独立检查。
本任务未启动 Lab 或派发其他任务。

未执行完整全仓、Lab/UI、宿主嵌入 CTest、JSON-only 新负例、跨配置/CRT/工具链全矩阵、
搬迁、教学全矩阵、整体体验 ZIP、Linux、网络、真实 Golden/设备/现场；不新增性能、
RSS、许可证闭合或稳定 ABI 承诺。SDK 阶段无已确认构建/消费阻断，不宣称整体最终
体验已验收。构建槽已释放，完成一次总控交接后停止写入，**已完成派发范围，待总控复核**。
