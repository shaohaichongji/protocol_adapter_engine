# PAE SDK clean-checkpoint 打包前静态核对

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

> 归档状态（2026-09-21）：本页是已被后续契约或验证承接的历史工程依据；正文中的现场、当前与下一步仅代表原记录时点。

## 1. 结论与本轮边界

2026-09-19 在 `main@98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` 上完成静态预检。该提交相对
`origin/main` 为 ahead 1，尚未 Push。本轮没有生成 SDK、没有创建源码导出/clone/worktree、没有构建或运行测试，
也没有修改打包脚本、CMake 或功能源码。

后续可以沿用现有五包入口，但有一个必须先满足的门槛：打包和二进制构建的 `SourceRoot` 必须是
`98df5e0...` 的独立、干净、可核验 Git checkout，不能使用当前共享工作树。当前共享树正在进行文档入口同步，
`scripts/package_sdk_stage3.ps1` 会如实把这种输入记为 `source_worktree_dirty=true`，不能据此声称 clean checkpoint。

推荐在另行授权的执行轮次创建一个全新、本地、detached clone，精确 checkout `98df5e0...`，确认
`git status --porcelain=v1 --untracked-files=all` 为空后，将该目录同时作为源码包和 static/shared 构建的
`SourceRoot`。这种方案无需修改现有打包脚本。若总控坚持使用无 `.git` 的 `git archive` 展开目录，则现有脚本
无法读取 HEAD/status，必须先另行授权最小 provenance 接口修改；本轮未实施该方案。

## 2. 现场与旧候选差异

### 2.1 当前状态

- 分支：`main`
- HEAD：`98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`
- 上游关系：`main...origin/main [ahead 1]`
- 暂存区：空
- 本轮核对时共享树已有总控文件 `docs/engineering/pae-execution-delivery-organization-plan.md` 修改；
  它不属于 SDK 输入白名单，但足以使当前仓库 Git 状态为 dirty。
- 未发现遗留的 CMake、CTest、MSBuild、编译或链接进程。

### 2.2 旧候选及功能输入

旧候选位于 `out/sdk-public-stream-description/candidate1-20260918/`，包含 source、static Debug/Release、
shared Debug/Release 五包。其 source provenance 为：

- `source_head=dbf4798f96a45b8d36a4754ad5a01e274680337e`
- `source_worktree_dirty=true`

因此旧候选仍是可追溯的历史本地验证物，但不是 clean SDK。

将旧 source 包除四个生成元数据文件及重复 license 副本外的 77 个 payload 文件逐一映射回当前仓库并重算
SHA-256，结果只有根 `CMakeLists.txt` 发生变化：

| 文件 | 旧候选 SHA-256 | 当前 `98df5e0` SHA-256 | 说明 |
| --- | --- | --- | --- |
| `CMakeLists.txt` | `0423DFF7D990DEEE3FF8FEFEAF6F83A2D01DC3530D4102ED467FB59A7E6E6B4B` | `B80095CAA9A8978965A2807D8D7A4D73FFCC1E655EC049FB608BA89BDF9CA107` | 增加默认 OFF 的 Lab public/standalone 接线选项及条件；source consumer 仍显式关闭 testing，只开启 public API |

三个与新公开查询直接相关的功能输入与旧候选一致：

- `include/pae/stream_framer.h`：`77B3F96F72C116934C090AE0AEBA222C1D85078ACB209449AC324A9027549670`
- `src/public_api/stream_framer.cpp`：`FDFF52BE370916B48BBC002E70968B0A62A7264893EB696D8BAEEEBDAEED6C01`
- `examples/public_api_sdk_consumer/main.cpp`：`EFAACE12DB68441CCB9BB5A6CE47FE1C534C6DF19D090167FD88DE19BE6EBF4A`

这说明新包的协议/查询功能输入没有在提交时漂移，但不能沿用旧 candidate 的构建结果冒充当前 clean 产物：
根 CMake 已变化，provenance 也必须重建，五包及六组外部消费仍需按新输入重新执行。

## 3. 打包入口与白名单核对

### 3.1 现有入口

唯一打包入口为 `scripts/package_sdk_stage3.ps1`：

- `-Kind source|static|shared`
- `-PackageRoot <fresh path>`
- `-Configuration Debug+Release|Debug|Release`
- `-SourceRoot <verified clean checkout>`
- `-Toolchain <recorded toolchain>`

source 模式创建全新目录并复制固定白名单；binary 模式接收已安装的目录，检查 public headers、
`PAEConfig.cmake`、consumer 和 license，再附加 `PAE-SDK-README.md`、`PROVENANCE.json`、
`MANIFEST.txt`、`SHA256SUMS.txt`。所有模式均拒绝覆盖已有包或已有元数据。

脚本、安装规则和 package config 自 `dbf4798` 至当前 HEAD 无差异：

- `scripts/package_sdk_stage3.ps1`：`5FF34106C9F39C308B3C6CCDABF556C4E5EC57984932FC021600EEEB5CECCFC0`
- `cmake/PaeSdkInstall.cmake`：`AEEA8A3C2B49C07391D6E1D4F5D9BD28B5D5232B42B02ADDC3C30E9BF0986181`
- `cmake/PAEConfig.cmake.in`：`9C85FC3F23D0EAF6FD77301743E34E50B26B1F164F24C5E83D3C37BEF18FC3AA`

### 3.2 source 白名单

source 包包含根 CMake、PAE package/install helpers、SDK 契约、Schema、`include/pae` 全部 public headers、
public API facade、Compiler/Plan/Core/Framer 所需内部实现、vendored yyjson、四组 public examples、四份公开配置和
重映射到 `examples/sdk_consumer` 的完整 consumer。Qt、Protocol Lab、Lab tests/standalone、开发测试以及 out 产物
不在白名单内。

当前根 CMake 新增项均默认 OFF；`examples/public_api_sdk_consumer/CMakeLists.txt` 的 source 路径仍强制：

- `BUILD_TESTING=OFF`
- `PAE_BUILD_TESTING=OFF`
- `PAE_BUILD_PUBLIC_API_STAGE1=ON`

静态阅读未发现它会在该组合下引用 source 包未携带的 Lab 文件。不过根 CMake 是唯一发生变化的 payload，
所以这一结论必须由新的 source Debug/Release configure/build/run 和 `ctest -N` 实测闭合，不能只沿用旧包通过记录。

### 3.3 provenance 限制

脚本直接执行 `git -C $SourceRoot rev-parse HEAD` 和 `git ... status --short --untracked-files=all`。
这有两个结果：

1. 当前共享树会被正确标记 dirty，不能用于 clean 候选；
2. 纯 `git archive` 展开目录没有 `.git`，当前脚本会直接失败，而不是产生可核验 provenance。

因此本轮推荐“全新 detached 本地 clone”而不是直接 archive。后续若选择修改脚本支持 archive，至少应由脚本
接受并验证显式 revision/导出清单，而不是允许调用方仅传一个未经验证的 `clean=true` 字符串。

## 4. 建议的后续执行根与命令骨架

以下均为建议路径，本轮没有创建：

- 干净源码根：`<LOCAL_WORK_ROOT>\pae-clean-checkpoint-98df5e0-source`
- 五包根：`out/sdk-clean-checkpoint/candidate1-20260919/`
- static 构建：`out/build/windows-msvc-sdk-clean-checkpoint-static`
- shared 构建：`out/build/windows-msvc-sdk-clean-checkpoint-shared`
- SDK 证据：`out/sdk-clean-checkpoint-validation/candidate1-20260919/`
- 仓库外 consumer 根：`<LOCAL_WORK_ROOT>\pae-sdk-clean-checkpoint-validation-candidate1-20260919`
- 后续 Lab 快照使用新的唯一根，不覆盖既有 r7/r8/r2 证据目录。

执行前应逐一确认路径不存在；若存在则使用唯一后缀，禁止覆盖旧候选、旧 consumer 和旧 Lab 快照。

建议固定现有已验证工具链：Visual Studio 18 2026、x64、
`v142,version=14.29.30133`；当前 VS bundled CMake 为 `4.3.1-msvc1`。建议把完整 generator/toolset、
CMake version 和实际编译器路径写入新证据，不只在 provenance 中保留笼统的 `MSVC x64`。

后续执行轮次的核心命令结构应为：

```powershell
$Revision = '98df5e0d844413fb6ad16a75dfceedcf17f2f1d6'
$CleanSource = '<LOCAL_WORK_ROOT>\pae-clean-checkpoint-98df5e0-source'
$Candidate = '<REPO_ROOT>\out\sdk-clean-checkpoint\candidate1-20260919'

# 需另行授权；创建后必须验证 detached revision 与空 status。
git clone --no-checkout '<REPO_ROOT>' $CleanSource
git -C $CleanSource checkout --detach $Revision
git -C $CleanSource rev-parse HEAD
git -C $CleanSource status --porcelain=v1 --untracked-files=all

powershell -NoProfile -ExecutionPolicy Bypass -File "$CleanSource\scripts\package_sdk_stage3.ps1" `
  -Kind source -PackageRoot "$Candidate\pae-sdk-source" -Configuration 'Debug+Release' `
  -SourceRoot $CleanSource -Toolchain 'Visual Studio 18 2026 x64 v142 14.29.30133'
```

static/shared 分别使用新的 build tree，配置时显式设置：

```text
-G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
-DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF
-DBUILD_SHARED_LIBS=OFF  # static tree
-DBUILD_SHARED_LIBS=ON   # shared tree
```

每个 tree 串行 build/install Debug、Release 到四个独立 fresh package roots；每次 install 完成后再调用
`package_sdk_stage3.ps1 -Kind static|shared -Configuration Debug|Release` 添加元数据。不能把 D/R 安装到同一根，
也不能让 packaging script 的 `SourceRoot` 指回当前共享树。

## 5. 最小 SDK 验证矩阵

必须使用五包的仓库外精确副本，六组严格串行：

| 组 | 配置入口 | 必须观察 |
| --- | --- | --- |
| source Debug | 只传外部 `PAE_SOURCE_DIR` | configure/build/run 0；完整 consumer PASS；`ctest -N` 为 0 |
| source Release | 同一 source build tree 串行 Release | 同上，不能沿用 Debug 可执行文件 |
| static Debug | 只传外部 Debug 包 `CMAKE_PREFIX_PATH` | `find_package(PAE)` 与 `PAE::pae` 来源在该包；run PASS；0 tests |
| static Release | 只传外部 Release 包 `CMAKE_PREFIX_PATH` | 同上，D/R 错配门禁仍有效 |
| shared Debug | 只传外部 Debug 包 `CMAKE_PREFIX_PATH` | import lib/runtime 来自该包；实际邻接 DLL 哈希一致；run PASS |
| shared Release | 只传外部 Release 包 `CMAKE_PREFIX_PATH` | 同上；不得回退 static、开发树或系统路径 |

consumer 的 PASS 前已精确执行：Compiler/metadata、physical query、ASCII facts、Codec、Framer、Host、
multi-message Encode；新查询还要求 ASCII stream 的 `maximum_candidate_frame_bytes=12`，complete-record 的
strategy/input-kind 正确且 M 为空。新六组必须继续保留这两个独立断言，不能只匹配 PASS 字符串。

还应生成以下审查证据：

- 五包 `MANIFEST.txt`/`SHA256SUMS.txt` 自校验及 candidate→external exact-copy 文件集/hash 一致；
- 五包 provenance 均为相同 `source_head=98df5e0...` 且 `source_worktree_dirty=false`；
- source 的 `PAE_SOURCE_DIR`、binary 的 `CMAKE_PREFIX_PATH`/`PAE_DIR` 绝对来源记录；
- 包内文本无开发仓库绝对路径；扫描仅针对文本，避免把 binary/log 命中误报为源码泄漏；
- shared D/R 导出新 `QueryPipelineFramingDescription` 和旧 `QueryStreamFramingCapability`；
- shared consumer 的 import lib、邻接 `pae.dll`、实际加载 DLL 与包内文件哈希一致，并记录依赖；
- static consumer 不依赖 `pae.dll`；
- Debug/Release 包错配、缺 DLL/错误 DLL 按既有门禁 fail-closed。

旧 candidate 的六组通过和 Qt Lab static/shared 闭包只能作为测试路线的历史依据，不能替代上述新候选结果。

## 6. 与后续 Qt Lab 验证的分工

SDK 六组先闭合 PAE package 自身的来源、安装目标、consumer 和 shared runtime。总控复核通过后，Lab 再使用
同一批新 static/shared D/R 包创建新的 standalone 输入快照，复核：

- `find_package(... NO_DEFAULT_PATH)` 与 `PAE::pae` 只来自该批包；
- Testing-on D/R 功能矩阵及 Testing-off 产品闭包；
- static 不部署/依赖 `pae.dll`，shared 的 SDK→测试/部署→实际加载三方 hash；
- SDK kind、D/R、缺失/替换 runtime 和 Qt/yyjson no-fallback 门禁；
- 当前仓库 Lab 白名单与新 standalone snapshot 的逐文件身份。

Lab 后片无需重复证明 SDK consumer 内部的 Compiler/Codec/Framer/Host 单元事实，也不能把 Qt Lab 通过升级为
stable ABI、正式发布、Linux 或 Qt 外发许可结论。历史 AV、Debug 0.8 focus-out 观察项继续独立保留。

## 7. 待总控决定与未验证边界

1. **干净输入获取方式**：推荐另行授权全新 detached 本地 clone，现有脚本即可使用；若必须用 archive，需先授权
   provenance 最小修改。当前共享 dirty tree 不能作为 clean 输入。
2. **输出根命名**：本报告给出 candidate1 路径；实际执行前由总控确认并做不存在检查，避免覆盖旧证据。
3. **脚本 provenance 文案**：当前固定 note 写“revision plus preserved ... changes”，即使 dirty=false 也不够精确。
   `source_worktree_dirty=false` 能表达机器事实，不构成功能阻断；是否最小调整文案由总控决定。
4. **本轮未执行**：没有构建、CTest、consumer、dumpbin、进程模块来源、包完整性或 Lab 验证；因此不能宣称新五包
   已生成、clean package 已验证或 Qt Lab 已使用新包。
5. **持续边界**：Windows same-toolchain 通过也不证明稳定/跨工具链 ABI；Linux、正式发布、许可证闭合、Golden、硬件、
   现场和生产均不在该后续最小矩阵内。

本轮仅新增本报告；未 Stage、Commit、Push、发布、复制包或改动 Git 历史。状态：已完成派发范围，待总控复核。
