# PAE 0.11 静态 Framing 查询 SDK 消费验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

## 1. 结论与候选身份

本轮从 `main@dbf4798f96a45b8d36a4754ad5a01e274680337e` 加当前保留的未提交查询实现、consumer
断言及共享工作树文档生成本地候选。五包 `PROVENANCE.json` 均明确
`source_worktree_dirty=true`，说明是该 HEAD 加 preserved unstaged/untracked snapshot，**不是干净 HEAD
发布物**。未 Stage、Commit、Push 或正式发布。

候选根：`out/sdk-public-stream-description/candidate1-20260918/`；仓库外消费根：
`<LOCAL_WORK_ROOT>/pae-sdk-public-stream-validation-candidate1-20260918/`。两者此前均不存在，未覆盖旧包。

五包及 `SHA256SUMS.txt` 哈希：

| 包 | 文件数（含 checksum） | checksum 文件 SHA-256 |
| --- | ---: | --- |
| source | 80 | `7395F126B1C6EC6DA4BF0386AE3F8D1AA17EAB5CE96A22BCE029B7CF5648A17A` |
| static-debug | 28 | `906F3F735A3D5178B12FF61CB0F6C3999B2C7C4B39723A9F60B11917D09A9CF1` |
| static-release | 28 | `5413C69CDE312268E24B370F5F6DAD853686949610DA4CC63450264DCFC87645` |
| shared-debug | 24 | `BD5549FF639EA7F372FC4F0B2B4548B3237656A703988EBACB399FCF5CA220F1` |
| shared-release | 24 | `7E13A4DA394EEDB98C1B9D3B36D8B0D9676D2C92A18EDD6483E7E9789C8F299B` |

所有 checksum 条目逐文件复算通过，仓库内候选与仓库外复制件的文件集合及逐文件哈希完全相同。

## 2. 本轮源码增量与实际输入哈希

本轮只补 consumer 对 complete Pipeline 的合法描述断言：status OK、input/strategy 均为
`COMPLETE_RECORD`、M 为空；既有 ASCII stream M=12 及 Compiler/metadata/physical/ASCII facts/Codec/
Framer/Host/multi-message Encode 验证保留。未修改打包脚本、安装 CMake 或公共实现。

实际打包关键输入 SHA-256：

- `include/pae/stream_framer.h`：`77B3F96F72C116934C090AE0AEBA222C1D85078ACB209449AC324A9027549670`
- `src/public_api/stream_framer.cpp`：`FDFF52BE370916B48BBC002E70968B0A62A7264893EB696D8BAEEEBDAEED6C01`
- `examples/public_api_sdk_consumer/main.cpp`：`EFAACE12DB68441CCB9BB5A6CE47FE1C534C6DF19D090167FD88DE19BE6EBF4A`

## 3. 六组仓库外消费

所有 configure/build/run 严格串行。Source 组只设置外部包内 `PAE_SOURCE_DIR`；四个 binary 组只设置
各自外部 `CMAKE_PREFIX_PATH`，`CMakeCache.txt` 的 `PAE_DIR` 均位于对应仓库外包内，没有回退开发仓库。

| 组 | 配置 | 结果 |
| --- | --- | --- |
| source | Debug | configure/build/run 0，consumer PASS |
| source | Release | build/run 0，consumer PASS |
| static | Debug | configure/build/run 0，consumer PASS |
| static | Release | configure/build/run 0，consumer PASS |
| shared | Debug | configure/build/run 0，consumer PASS |
| shared | Release | configure/build/run 0，consumer PASS |

每次运行均输出：
`PAE_SDK_STAGE3_CONSUMER_PASS compiler=1 metadata=1 physical_query=1 ascii_facts=1 codec=1 framer=1 host=1 multi_message_encode=1`。
新查询和 complete/ASCII M 断言在该 PASS 前执行，失败返回独立退出码 15。

构建目录为 `out/build/windows-msvc-public-stream-sdk-consumer-*`；完整命令输出位于
`out/public-stream-sdk/consumer-*-{configure,build,run}.log`。所有 consumer build 的 `ctest -N` 均为
`Total Tests: 0`，未引入 Qt/Lab 测试或依赖。

## 4. 包、DLL 与路径核查

- `package-integrity-summary.log`：五包 manifest/checksum 计数、逐文件 hash、外部 exact copy 和 dirty
  provenance 全部通过。
- `consumer-cmake-origins.log`：source 指向仓库外 `pae-sdk-source`；binary 的 prefix/PAE_DIR 指向对应
  仓库外 static/shared 包。
- `development-path-leaks.log`：包内 CMake/文本/头/源码未出现开发仓库绝对路径。
- shared D/R 均同时导出旧 `QueryStreamFramingCapability` 和新
  `QueryPipelineFramingDescription`。
- Debug DLL 依赖 MSVC debug CRT，Release DLL 依赖 release CRT；未出现 Qt/Lab DLL。
- consumer 输出目录复制的 `pae.dll` 与对应 package DLL 哈希相同：Debug
  `C46478EE0783FC415ABB83BFB469177E04A1C36CC95CD0E96E6BF9DCAC70AF8C`，Release
  `6171E3920CAC0874E802E69C3EF24DCED3CF00FD7BA6B18951674F315743B169`。

## 5. 边界与剩余风险

- 本轮没有重复上一片单元测试或 Qt 全矩阵；验证目标是新候选的独立消费与包装一致性。
- 当前静态查询的“损坏内部 Frozen Plan”负例仍未安全动态构造；没有使用 UB 或新增生产测试入口。
- shared 构建仍有项目既有 C4251 实验 C++ ABI 警告；候选不承诺稳定 ABI、Linux、硬件、Golden、现场或
  正式发布。
- 五包仅为本地复核候选，不覆盖 final6/candidate2 或任何部署。

状态：**已完成派发范围，待总控复核。**
