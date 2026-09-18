# ASCII 公开事实新 SDK 独立消费验证

总控状态更新（2026-09-18）：本片已完成限定 consumer/证据/包来源复核及候选与消费目录 DLL 配对哈希复算；总控未重复六组运行或全部 manifest 复算。已纳入用户授权的 SDK/A1/A2 本地提交检查点范围，未授权 Push/正式发布。下方“待总控复核/未 Commit”为执行任务交付时点。

日期：2026-09-18。状态：**已完成派发范围，待总控复核**。本记录是 Windows x64 本地候选的技术验证，不是正式发布、稳定 ABI、许可证授予或生产可用声明。未 Stage、Commit、Push，也未覆盖 Stage 3 `final6`、Stage 4 P `candidate2` 或 H2 部署。

## 1. 基线、范围与候选

- 分支与基线：`main@481d51ae2d62e2e28754fd84e5df4bb64309a1e2`。开始时暂存区为空，工作树仅有总控维护的两份计划文档修改；本轮完整保留。
- 工具链：Visual Studio 18 2026，x64，MSVC v142 `14.29.30133`；Debug `/MDd`，Release `/MD`。
- 新候选根：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\sdk-stage4-ascii\candidate1-20260918\`。
- 五包：`pae-sdk-source`、`pae-sdk-static-debug`、`pae-sdk-static-release`、`pae-sdk-shared-debug`、`pae-sdk-shared-release`。
- 仓库外消费根：`F:\PersonalWorkspace\pae-sdk-stage4-ascii-validation-candidate1-20260918\`。五包逐文件复制后复算 189 个文件，SHA-256 全部相同。六个 consumer 构建目录均位于该根，不把开发仓库作为源码或 package prefix。
- 证据根：`F:\PersonalWorkspace\协议解析拼接工具\protocol_adapter_engine\out\stage4-ascii-sdk\`。

复用 `scripts/package_sdk_stage3.ps1`、`cmake/PaeSdkInstall.cmake` 和 `cmake/PAEConfig.cmake.in` 即可包含当前公开头、实现及 consumer，没有修改打包/安装代码。只对 `examples/public_api_sdk_consumer/main.cpp` 补充包外断言：Decode/Encode Action、literal/field Segment、Field 描述，成功 ASCII RX BYTES 的真实输入切片与匹配身份，以及 Host candidate observer 与业务结果的同源身份；既有 Binary physical、Codec、Framer、Host 和多 Message Encode 检查保留。

## 2. 五包生成与内容复核

产品构建使用 Testing-off：

```powershell
cmake -S . -B out/build/windows-msvc-stage4-ascii-sdk-static -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_JSON_PARSER_SPIKE=OFF
cmake -S . -B out/build/windows-msvc-stage4-ascii-sdk-shared -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DPAE_BUILD_TESTING=OFF -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_JSON_PARSER_SPIKE=OFF
cmake --build <static-or-shared-build> --config <Debug-or-Release> --target pae_public_api --parallel 4
cmake --install <static-or-shared-build> --config <Debug-or-Release> --prefix <new-package-directory>
scripts/package_sdk_stage3.ps1 -Kind <source|static|shared> -PackageRoot <new-package-directory> -Configuration <Debug+Release|Debug|Release>
```

两次 Configure、四次 Build、四次 Install、五次 Package 均退出 0。源码包记录 80 条 Hash/79 条 Manifest，Static 各 28/27，Shared 各 24/23；逐项重算全部 Hash 和文件长度均通过。五包 provenance 均记录上述 HEAD、对应 kind/configuration 和 `source_worktree_dirty=true`，没有把含 consumer 与总控文档修改的快照伪称干净发布。五包七个公开头逐文件 Hash 一致；源码包未包含 `tests`、`tools`、`spikes`、`out`、`.git`、Qt 或 test-support 文件。172 个候选文本文件未发现开发仓库绝对路径。

关键二进制：

| 产物 | SHA-256 |
| --- | --- |
| Static Debug `pae.lib` | `4f6373820885b0d5c32c3a4373e73f668715f26ad0c6be280a31ecc1a15aa5cd` |
| Static Release `pae.lib` | `e948bec6d1a08eb0089dee359bbe21457f550fb5147bc6d1f07209ea192cd792` |
| Shared Debug `pae.dll` | `80679c9f04bf0d048af3f930e6ca05e25fa09ace9551d154ca25dfa96fb557f1` |
| Shared Release `pae.dll` | `481bf28009c92a870c4bf0e4f2f7e40d3ae3a1b17e89cfa129cc89b09bdaddec` |

对应日志：`configure-*.log`、`build-*.log`、`install-*.log`、`package-*.log`、`package-integrity.log`、`external-copy-integrity.log`、`header-and-binary-hashes.log`、`package-path-boundary.log` 和 `source-content-boundary.log`。

## 3. 六组真正包外消费

Source 两组只设置外部副本 `PAE_SOURCE_DIR` 并 `add_subdirectory`；Static/Shared 四组只设置外部副本 `CMAKE_PREFIX_PATH`，由 `find_package(PAE CONFIG REQUIRED)` 和 `PAE::pae`消费。`consumer-input-paths.log` 记录六个 CMakeCache 的实际 source/prefix，均为仓库外副本；二进制组没有源码 fallback。六个构建执行 `ctest -N` 均为 `Total Tests: 0`。

| 形态 | 配置 | Configure | Build | Run |
| --- | --- | ---: | ---: | ---: |
| Source | Debug | 0 | 0 | 0 |
| Source | Release | 0 | 0 | 0 |
| Static | Debug | 0 | 0 | 0 |
| Static | Release | 0 | 0 | 0 |
| Shared | Debug | 0 | 0 | 0 |
| Shared | Release | 0 | 0 | 0 |

每组使用包内 `synthetic_stream_framing_slice.pae.json` 与 `synthetic_ascii_stream_slice.pae.json`，均输出：

```text
PAE_SDK_STAGE3_CONSUMER_PASS compiler=1 metadata=1 physical_query=1 ascii_facts=1 codec=1 framer=1 host=1 multi_message_encode=1
```

命令形状如下，六组分别使用各自全新的包与构建目录：

```powershell
# Source Debug/Release
cmake -S <source-package>/examples/sdk_consumer -B <external-build> -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" -DPAE_SOURCE_DIR=<source-package>
cmake --build <external-build> --config <Debug|Release> --target pae_sdk_stage3_consumer --parallel 4
& <external-build>/<Debug|Release>/pae_sdk_stage3_consumer.exe <package-binary-config> <package-ascii-config>

# Static/Shared Debug/Release
cmake -S <binary-package>/examples/sdk_consumer -B <external-build> -G "Visual Studio 18 2026" -A x64 -T "v142,version=14.29.30133" -DCMAKE_PREFIX_PATH=<binary-package>
cmake --build <external-build> --config <matching-config> --target pae_sdk_stage3_consumer --parallel 4
& <external-build>/<matching-config>/pae_sdk_stage3_consumer.exe <package-binary-config> <package-ascii-config>
```

每组的 `consumer-<shape>-<config>-{configure,build,run}.log` 保留实际输出。consumer 对 ASCII facts 的成功标志包括：RX/TX 各五个有序片段，RX field segment 指向 Message 0/Field 0，TX 首片为 `TX ` literal，字段同时被 Decode/Encode 引用；直接 Decode `RX A!OK\r\n` 得到 Message 0 且 `name` BYTES 精确借用输入偏移 3、长度 1；Host observer 与成功业务回调均报告 Message 0。既有 Binary 固定布局、Encode/Decode、Framer 两候选、Host Encode 两 Message 仍同时成立。

## 4. Shared 导出、依赖与运行 DLL

`dumpbin /exports` 对候选 Debug/Release DLL 均退出 0，并分别找到：

- `CompiledProtocol::AsciiAction`
- `CompiledProtocol::AsciiSegment`
- `CompiledProtocol::AsciiField`

`dumpbin /dependents` 显示 Shared consumer 直接依赖 `pae.dll`，Static consumer 不依赖 `pae.dll`；Debug 使用 Debug MSVC/UCRT，Release 使用 Release MSVC/UCRT。包内 CMake 的 post-build copy 将 DLL 放到实际运行 EXE 同目录：

- Debug：`F:\PersonalWorkspace\pae-sdk-stage4-ascii-validation-candidate1-20260918\build-shared-debug\Debug\pae.dll`
- Release：`F:\PersonalWorkspace\pae-sdk-stage4-ascii-validation-candidate1-20260918\build-shared-release\Release\pae.dll`

两份运行目录 DLL 分别与所选外部包 `bin\pae.dll` 的 SHA-256 完全相同。证据为 `dumpbin-exports-shared-*.log`、`dumpbin-dependents-shared-*.log`、`dumpbin-consumer-dependents-*.log` 和 `runtime-library-paths.log`。共享构建仍有 Stage 3 已记录的 C4251 实验 C++ ABI 警告；本轮没有引入新公开持有成员，也不把同工具链运行通过提升为稳定 ABI 承诺。

## 5. 修改、边界与结论

本轮仓库增量只有：

- `examples/public_api_sdk_consumer/main.cpp`
- `docs/engineering/pae-public-ascii-sdk-validation.md`

没有修改打包脚本、安装规则、公共 API、Core、Plan、Schema、根 CMake、Lab 或 Qt。没有运行 Lab/UI、人工点击、网络、Linux、真实协议 Golden、硬件、现场、性能或跨工具链/CRT ABI 验证；没有重跑接口片单元矩阵，因为本轮只改包外 consumer 并已在六组中实际编译运行。Hash 用于内容复核，不是签名或来源认证；仓库仍没有 PAE 项目级对外分发许可证，故本候选只供本地技术复核。

限定结论：五个含 ASCII facts 新 API 的本地候选包、六组仓库外消费、DLL 新查询导出与包内容自洽均通过，当前未发现本派发范围内阻断总控复核的问题。是否交给 Lab 0.10 非 Qt adapter 消费由总控另行决定。
