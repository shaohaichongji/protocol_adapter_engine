# PAE SDK clean-checkpoint 五包与包外消费验证

## 1. 结论与证据边界

2026-09-19 基于精确提交 `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6` 的全新 detached
本地 clone，生成并验证 source、static Debug/Release、shared Debug/Release 五个 Windows x64 本地候选包。
五包 provenance 均为同一 HEAD 且 `source_worktree_dirty=false`；六组真正仓库外 consumer 均完成
configure/build/run，退出码为 0，并输出完整 `PAE_SDK_STAGE3_CONSUMER_PASS`。六个 consumer build tree
的 `ctest -N` 均为 `Total Tests: 0`。

本轮没有修改公共 API、Core、CMake、打包脚本、依赖或当前共享树中的既有文档。没有 Stage、Commit、
Push、正式发布或删除旧候选。该结论仅证明同一 Windows/MSVC 工具链下的 clean-checkpoint 本地候选及
包外消费，不证明稳定/跨工具链 ABI、Linux、正式许可、生产、Golden、硬件或现场可用；Qt Lab 使用本批
SDK 是后续独立派发，本轮未执行。

## 2. 输入身份与路径

- 共享仓库：`F:/PersonalWorkspace/协议解析拼接工具/protocol_adapter_engine`
- 输入源码根：`F:/PersonalWorkspace/pae-clean-checkpoint-98df5e0-source`
- 输入状态：detached `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`，创建后和全部验证结束后
  `git status --porcelain=v1 --untracked-files=all` 均为 0 行
- clone 方式：从本地仓库 `git clone --no-hardlinks --no-checkout`，随后精确 detached checkout；没有复制
  共享工作树的 README、计划或预检报告
- 候选根：`out/sdk-clean-checkpoint/candidate1-20260919/`
- static/shared 构建根：`out/build/windows-msvc-sdk-clean-checkpoint-{static,shared}`
- 证据根：`out/sdk-clean-checkpoint-validation/candidate1-20260919/`
- 仓库外包及 consumer 根：`F:/PersonalWorkspace/pae-sdk-clean-checkpoint-validation-candidate1-20260919/`
- 工具链：Visual Studio 18 2026、x64、`v142,version=14.29.30133`，MSVC
  `19.29.30159.0`，Windows SDK `10.0.22621.0`，VS bundled CMake `4.3.1-msvc1`

clone 与最终 clean 状态分别见 `clean-source-clone.log`、`clean-source-final-status.log`。

## 3. 五包生成与身份

沿用 `scripts/package_sdk_stage3.ps1`，没有改写包内 metadata。脚本中的
`source_snapshot_note` 是固定通用文案“revision plus preserved unstaged and untracked working-tree changes”；
本批是否 clean 以机器字段 `source_worktree_dirty=false`、精确 `source_head` 以及源根两次空 status 为准，
没有把该通用文案当成存在 dirty 变更的证据，也没有直接修写包内 provenance。

五包均完成 `MANIFEST.txt` 文件长度复核、`SHA256SUMS.txt` 逐项重算、candidate 到仓库外副本的文件集和
逐文件 SHA-256 比较。以下哈希是每包 `SHA256SUMS.txt` 文件自身的 SHA-256，可作为本次候选身份摘要：

| 包 | 文件数 | hash 条目 | `SHA256SUMS.txt` SHA-256 |
| --- | ---: | ---: | --- |
| source | 81 | 80 | `9bf25c6dc043989b179a77443c5cddd099c645e4a1d422445a571ed7f12c09f6` |
| static Debug | 29 | 28 | `95ddd594f4f320430364607910435161cbf27b1f976521f4345a2920b2a02037` |
| static Release | 29 | 28 | `ccaf8417c65b5e3c31e4f957e3bea4e5831d11d707ab22c4dc55f9a15caaec0f` |
| shared Debug | 25 | 24 | `ea3d32b34f2b5cf272f1cdfaf4a382d3dc1dd33918a987d396e7b4f72c5fc35f` |
| shared Release | 25 | 24 | `e18f49aaffddac82e766ba10a5ccd530a2cda4714c6ec64490bea786f8e603b8` |

完整机器可读结果为 `package-identities.json`，逐项过程见
`package-integrity-and-external-copy.log`。包外文本扫描 172 个文件，对共享开发仓库根和 clean clone 根均为
0 命中，见 `development-path-leaks.log`；binary、PDB 和日志未混入该文本结论。

## 4. 构建与打包命令

source 包入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File F:\PersonalWorkspace\pae-clean-checkpoint-98df5e0-source\scripts\package_sdk_stage3.ps1 `
  -Kind source `
  -PackageRoot <candidate>\pae-sdk-source `
  -Configuration Debug+Release `
  -SourceRoot F:\PersonalWorkspace\pae-clean-checkpoint-98df5e0-source `
  -Toolchain 'Visual Studio 18 2026 x64 v142 14.29.30133'
```

static/shared 使用两个独立 build tree，核心配置分别为：

```text
cmake -S <clean-source> -B <build-root>
  -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133"
  -DBUILD_SHARED_LIBS=OFF|ON
  -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF
  -DPAE_BUILD_PUBLIC_API_STAGE1=ON
```

随后严格串行执行 `cmake --build ... --config Debug|Release --target pae_public_api -- /m:1`、
`cmake --install ... --config <configuration> --prefix <fresh package root>`，最后按对应
`-Kind static|shared -Configuration Debug|Release` 附加元数据。所有命令退出 0。日志为：

- `configure-{static,shared}.log`
- `build-{static,shared}-{debug,release}.log`
- `install-{static,shared}-{debug,release}.log`
- `package-{source,static-debug,static-release,shared-debug,shared-release}.log`

shared Debug/Release build 各记录 15 行既有 MSVC C4251 警告，涉及公开导出类持有 STL/PImpl 成员；见
`shared-warning-summary.json`。它没有阻止本次同工具链构建/运行，也不能据此宣称稳定 ABI。

## 5. 六组真正包外消费

source 组只传仓库外 `PAE_SOURCE_DIR`；四个 binary 组只传对应仓库外包的
`CMAKE_PREFIX_PATH`，没有同时传另一路径。`consumer-cmake-origins.json` 证明 source 根以及四个
`PAE_DIR` 都落在本次仓库外根，没有回退 clean clone、共享开发树或旧候选。

| 组 | Configure | Build | Run | `ctest -N` |
| --- | ---: | ---: | ---: | ---: |
| source Debug | 0 | 0 | 完整 PASS | 0 tests |
| source Release | 0 | 0 | 完整 PASS | 0 tests |
| static Debug | 0 | 0 | 完整 PASS | 0 tests |
| static Release | 0 | 0 | 完整 PASS | 0 tests |
| shared Debug | 0 | 0 | 完整 PASS | 0 tests |
| shared Release | 0 | 0 | 完整 PASS | 0 tests |

每组运行输出均为：

```text
PAE_SDK_STAGE3_CONSUMER_PASS compiler=1 metadata=1 physical_query=1 ascii_facts=1 codec=1 framer=1 host=1 multi_message_encode=1
```

该 PASS 之前，consumer 以独立退出码断言 `QueryPipelineFramingDescription`：ASCII stream 的
input/strategy 正确且 `maximum_candidate_frame_bytes=12`，complete-record 的 input/strategy 正确且
maximum 为空；因此不是仅匹配最后一行字符串。完整汇总见 `consumer-matrix-summary.json`，各组
`consumer-*-{configure,build,run,ctest-n}.log` 保留实际输出。

source D/R 的成功同时闭合了预检中唯一变化 payload——根 `CMakeLists.txt`：当前 source 白名单在
testing OFF、public API ON 组合下没有引用缺失的 Lab 文件。

## 6. static/shared、符号与 DLL

- static D/R consumer 的 `dumpbin /DEPENDENTS` 均不含 `pae.dll`。
- shared D/R consumer 的 `dumpbin /DEPENDENTS` 均包含 `pae.dll`。
- shared D/R 的包内 DLL 均同时导出新 `QueryPipelineFramingDescription` 和旧
  `QueryStreamFramingCapability`。
- consumer build 后邻接 DLL 与对应包内 DLL SHA-256 精确一致：
  - Debug：`0176546bb10a922ded73d4122b1e096983e6770d265c531b9816ddea1f9d4895`
  - Release：`7c5cd386c3e00cec409e80a13d5f18b342667959024235e2b3caec73a80fa3fd`
- 两个 shared consumer 在该邻接 DLL 存在时运行成功；系统 `where pae.dll` 返回 1，当前 PATH 未发现同名
  DLL。证据为 `shared-runtime-and-symbols.json`、`shared-*-dumpbin-exports.log`、
  `consumer-*-dumpbin-dependents.log`、`ambient-pae-dll-search.log`。

本轮没有为短生命周期 consumer 增加进程模块捕获插桩，因此这里证明的是链接依赖、邻接部署哈希和成功
运行闭包，不额外声称取得了运行中模块绝对路径快照。后续 Qt Lab shared 验证已有长生命周期产品进程，
应继续使用 SDK→部署→实际加载三方哈希门禁完成那一层来源证明。

## 7. 负向门禁与明确限制

使用全新 build/input 根执行了存在实际 SDK 机制的三项负例：

1. Debug static 包构建 Release：链接退出 1，明确引用
   `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_DEBUG_PACKAGE.lib`。
2. Release static 包构建 Debug：链接退出 1，明确引用
   `PAE_SDK_CONFIGURATION_MISMATCH_EXPECTED_RELEASE_PACKAGE.lib`。
3. shared Debug 派生输入从一开始就不复制 `bin/pae.dll`：configure 成功，build 在 consumer 的
   `copy_if_different` 阶段因源 DLL 不存在退出 1；没有进入运行。

证据为 `negative-configuration-mismatch-summary.log`、对应 configure/build 日志，以及
`negative-shared-runtime-summary.log`。

普通 SDK consumer 和 `PAEConfig.cmake` 不读取 `SHA256SUMS.txt`，也不包含 SHA-256/Get-FileHash 门禁，
见 `sdk-runtime-hash-gate-audit.log`。因此本轮没有声称“用另一配置 DLL 替换后由 SDK 内容哈希拒绝”；那是
Qt Lab standalone 输入/部署脚本的上层能力，不是当前普通 SDK consumer 能力。没有为补齐该负例修改 SDK、
脚本或 consumer，也没有反复尝试跨配置 DLL 是否碰巧可加载。

## 8. Git、产物和停点

本轮新增的仓库候选文件只有本报告。候选、构建和证据均位于仓库已忽略的 `/out/`；仓库外 clean clone
和 consumer 根未加入 Git。共享树接管时已有且由其他任务维护的 README、guide、综合计划和 preflight
变更全部保留，未被本轮覆盖或清理。

当前分支仍为 `main`，HEAD 仍为 `98df5e0d844413fb6ad16a75dfceedcf17f2f1d6`，相对
`origin/main` ahead 1，暂存区为空。未 Stage、Commit、Push、正式发布或删除旧产物。

剩余边界：Qt Lab 尚未切换到本批 SDK；C4251/stable ABI、Qt 正式外发许可、Linux、历史 AV、Debug 0.8
focus-out、长期稳定性、Golden、硬件与现场均未由本轮升级。状态：已完成派发范围，待总控复核。
