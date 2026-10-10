# 513f6cc 同基线 SDK 五包重建验证（2026-10-09）

## 1. 本轮结论与固定来源

本次按新授权从 `513f6cc9bd46b64b3b6d1d4283f13c806039eebf` 独立干净快照重新
构建五个 Windows x64 SDK 目录包。五包完整校验通过，18 次包外程序验证通过；
没有改名或复用 a1c6010 二进制、安装树和 Cache，没有改产品、脚本或旧报告。
SDK 构建结束，等待总控复核并串行派发 Lab；不等于完整阅读体验、Lab、正式发布
或许可证边界已经闭合。

现场接管为干净 main@513f6cc，暂存区空，三个新目标均不存在：
`out/build/aligned-sdk-513f6cc-20261009/`、`deliverables/sdk/513f6cc/` 和本报告。
读取外层 AGENTS、制包与验证脚本及 yaml-clean/experience 验证依据后，使用既有
本地独立复制方式：

```powershell
git clone --no-hardlinks --no-tags --single-branch --branch main <主仓库> <新根>/clean-source
```

不联网、不建立 worktree、不切换主仓库分支；clone HEAD 再核对固定完整 SHA，
tree 为 `0d504fb456fdd81d23a95c0ac111f348bdf0da11`。起止 clone 状态干净；
3256 个跟踪文件的长度/Hash 快照保存于新 evidence/fixed-source-files.json，
完成时再次逐项核对不变。本轮新增报告及工程整理并行文档修改不进入产品快照。

下文证据均在 `out/build/aligned-sdk-513f6cc-20261009/evidence/`，不是上一轮日志。
来源见 local-clone.log、source-identity.log、source-final-git-status.log、
source-final-byte-identity.log 及 final-quality.log。旧 a1c6010 五包与两份验证报告
共 264 文件另存保护 Hash，完成时复核未变；旧产物、旧失败日志均未清理或覆盖。

## 2. 五包、provenance 与文件 Hash

五包均保持忽略，未制作最终体验 ZIP。完整 payload 路径、长度和 SHA256 位于
各包 MANIFEST.txt / SHA256SUMS.txt；文件数包含制包元数据：

| 仓库相对根 | 文件数 | 实际消费 |
| --- | ---: | --- |
| `deliverables/sdk/513f6cc/source` | 102 | 三入口各 Debug/Release，共 6 次 |
| `deliverables/sdk/513f6cc/static-Debug` | 42 | 三入口 Debug，共 3 次 |
| `deliverables/sdk/513f6cc/static-Release` | 42 | 三入口 Release，共 3 次 |
| `deliverables/sdk/513f6cc/shared-Debug` | 38 | 三入口 Debug，共 3 次 |
| `deliverables/sdk/513f6cc/shared-Release` | 38 | 三入口 Release，共 3 次 |

每包 PROVENANCE.json 记录固定完整 source_head、source_worktree_dirty=false、
kind/configuration/YAML 声明。Source 含可选 YAML 作者源、默认 OFF；四 binary
启用静态 YAML 附加组件。Shared 是 PAE DLL 加静态 YAML 库，不是 YAML DLL。
原脚本的 toolchain 字段仍为通用 MSVC x64，实际版本由配置和构建日志补充。
binary 通用说明只描述安装树字节；本轮新构建及逐文件安装对照补齐来源链，
不是仅凭 Git 状态推断二进制来源。Hash 只证明一致性，不提供签名或来源认证。

| 包 | PROVENANCE.json SHA256 | SHA256SUMS.txt SHA256 |
| --- | --- | --- |
| source | `3CC74B69655BE0AD77AAB1A4AE0BC318BA4377BC2B94316F1CF6123C475AFD5E` | `6F33358AA92FFF2C16D5574A4072F25887434A0D1D1ACA837C9DB41BA82139E5` |
| static-Debug | `5AA7BC6C4904D874CD2A3AF87F4616628069039CF6BF453738EC56FD7512087E` | `C847604BC64B9752C0067D8751158972294EF113FC26025AFCB8EE3BA7E0169F` |
| static-Release | `8DCBAA7AB4A51B13E9A81B1AB2430035A1E6C6393D409158DAE09A598707465D` | `4B74DC034B0BE8182C9684690D79067F89981A4D33B6D3D43DEA3D7688C35BD6` |
| shared-Debug | `0A57672E088C2778097F6CB67307A39A843879A3DCDFD5BAD91757900DBE9621` | `86E3D8C9F8237EFBE53023CD219D44EC292AC3E373331A1798FE018748739B00` |
| shared-Release | `2A23C8A05C85309F35DE930A88A91DC014DBBD38C0C0F89147B5E10697AEDDE2` | `4B3C43F4DC4CE02F0E22E9F928D8993C8247C98D848D2B2F5FDB857D2D5A715F` |

five-package-summary.log 与 package-identities.json 保存身份；首次、独立完整审计、
消费后再次 Verify 均通过，分别见各 package/verify、final-*-verify 和
post-consumer-*-verify 日志。Source 95 个同路径文件与快照相同，consumer 两个
别名和 yyjson 许可副本另核对，共 98 个非元数据身份检查；其余 4 项为元数据。
metadata、Decimal、YAML frontend、公开 compiler/description/compiled state
最新注释与固定提交一致，见 key-comment-source-identity.log。

构建与安装的 pae.lib、YAML 附加库、两份 shared DLL、Static 每配置五个内部库
逐项 Hash 相同。二进制公开头、YAML 头、四份 Schema/Profile、SDK 指南及
rapidyaml 通知与固定快照相同，见 file-identity.log、extended-install-identity.log。

## 3. 本次真实构建与包外验证

实际工具链：Visual Studio 18 2026 / x64 / v142 14.29.30133，编译器识别为
19.29.30159.0，Windows SDK 10.0.22621.0。CRT Debug /MDd、Release /MD；
CL/_CL_ 未设置，没有改全局环境、Qt、依赖或下载。开始时没有运行中的构建进程。

S 为新 clean-source，B 分别为新 static-build/shared-build，P 为对应新包：

```powershell
cmake -S $S -B $B -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_YAML_FRONTEND=ON `
  -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF `
  -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_PROTOCOL_LAB_UI=OFF
# shared 使用 BUILD_SHARED_LIBS=ON。每个构建根内 Debug 后 Release，严格串行。
ctest --test-dir $B -N
cmake --build $B --config $Configuration --target pae_public_api pae_yaml_frontend --parallel 4
cmake --install $B --config $Configuration --prefix $P
& "$S/scripts/package_sdk_stage3.ps1" -Kind $Kind -PackageRoot $P -Configuration $Configuration -SourceRoot $S
& "$S/scripts/verify_yaml_sdk_package.ps1" -PackageRoot $P -Kind $Kind -HasYaml:$true
```

Source 用 -Kind source -Configuration Debug+Release，Verify 的 HasYaml=false
表示非安装组件，不表示没有 YAML 作者源。两产品根 ctest -N 均 0 项，不把零测试
写成产品测试通过。全部产品 Configure/Build/Install/Package/Verify 退出 0。

消费三个包内入口 sdk_consumer、getting_started、yaml_sdk_consumer，各自使用新
consumers/<包>-<入口> 根，不借用旧 Cache。Source 只用 PAE_SOURCE_DIR=<新Source>；
binary 明确 PAE_DIR=<对应包>/lib/cmake/PAE 与 CMAKE_PREFIX_PATH=<对应包>，按值
规范化核对实际 Cache，允许 CMake 合法 PATH/UNINITIALIZED 类型，拒绝错误路径。

```powershell
cmake -S <包内examples/入口> -B <对应独立新consumer根> <相同生成器选项> <来源选择参数>
cmake --build <consumer根> --config <匹配配置> --target <入口目标> --parallel 4
& <consumer根>/<配置>/<入口程序>.exe <该包内合成配置的绝对路径>
```

18 次有效程序运行均退出 0 并检查对应精确成功标记：
PAE_SDK_STAGE3_CONSUMER_PASS / GETTING_STARTED_BINARY_PASS /
PAE_YAML_SDK_CONSUMER_PASS。不是 18 个 CTest 项，也不是全域覆盖率。
入门独立预期 AA 00 07；综合检查 compiler、metadata、physical query、ASCII facts、
codec、framer、host 和 multi-message encode。YAML 转严格 JSON 后执行公开编译/Codec。

六份 shared consumer 相邻 DLL 与所消费包的 bin/pae.dll Hash 一致；生成项目
ClCompile 输入只位于新包或新隔离根，没有开发树 src 编译输入，见
consumer-package-identity.log、consumer-clcompile-closure.log、file-identity.log。
未将这一检查扩大为任意宿主、工具链或运行时 DLL 搜索环境的安全保证。

本轮流水线一次执行结束，exit 0 / ALIGNED_SDK_VALIDATION_COMPLETE；没有把上一轮
辅助脚本漏参/路径误拒绝的失败记录当成本轮结果，旧记录保持原样。Shared 构建
既有 C4251 警告保存，不宣称零警告。原始完整命令及退出在 transcript.log；分步日志：

- source-package/verify.log、{static,shared}-configure/test-inventory.log；
- {static,shared}-{Debug,Release}-{build,install,package,verify}.log；
- <包>-<入口>-configure.log、<包>-<入口>-<配置>-build/test.log；
- final-*-verify.log、post-consumer-*-verify.log、各 identity 与 final-quality.log。

## 4. 文档投影、Git 和剩余边界

本 SDK 片保留固定提交的原始文档，不解决原包离线闭包：source inline 链接
21 个缺 14，各 binary 11 个缺 9，无逃出包根；指南仍有 b12ad80 历史说明。
固定提交 docs/experience/tutorials 十个文件未入原白名单，getting_started
验证记录.md 未入包；逐项 reader-examples-inventory.log 和 markdown-links.log
记录。后续工程整理从确认来源独立投影，不偷用旧 overlay、不改贴 provenance。
本轮未执行教学 verify.cpp 或全部 public_api 独立示例。

接管后的工程整理并行修改位于 docs/sdk、docs/experience 和 experience 归集脚本，
是已授权例外，不混入产品快照、不为其投影结果背书。最终具体列表见
report-final-status.log。本任务唯一 Git 候选增量为本报告；原有两个已提交报告
及旧包保持不变。HEAD 仍为 main@513f6cc，暂存空。报告 UTF-8 无 BOM、无尾空白，
定向便携路径检查及报告十个元数据 Hash 核对通过，git diff --check 退出 0。
out 与候选五包保持忽略；未 Stage/Commit/Push/Pull/Reset/Clean/Stash/切分支/清理/发布。

Lab 下一片可选 deliverables/sdk/513f6cc/static-Release 或 shared-Release；Debug
使用同名 Debug 根，必须明确 PAE_DIR/CMAKE_PREFIX_PATH，不复用 a1c6010 Cache。
Shared DLL 仅取所选新包 bin/pae.dll，YAML 为同包静态 add-on。Lab 自身 package-mode、
Qt 和路由开关由后续 Lab 任务按当前契约核对；本任务未开始 Lab。

未重跑全仓、Lab/UI、JSON-only 新负例、嵌入宿主 CTest、跨配置/CRT/工具链全矩阵、
搬迁、教学全用例、ZIP、Linux、网络、真实协议 Golden、设备或现场；无新增性能、
RSS、许可闭合或稳定 ABI 结论。原包文档缺项为后续投影待办，不是已验证运行路径
的产品失败；不得据 SDK 通过宣称全部同基线交付待办已关闭。

构建时段已结束，完成一次总控交接后停止写入，状态：**已完成派发范围，待总控复核**。
