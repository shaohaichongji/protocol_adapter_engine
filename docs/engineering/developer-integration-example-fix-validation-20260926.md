# 开发者源码示例嵌入宿主修复验证

## 范围与接管

本报告只覆盖 `examples/getting_started` 和 `examples/yaml_sdk_consumer` 的源码接入 CMake。
接管时为 `main@8eb5bb665b26efb99f86c8388c922c761ad62ed4`，暂存区为空；指南、SDK
说明和工程计划的既有未提交变更由其他任务持有，本轮未改。现有 `b12ad80` SDK 包未改写或重打包。

前置 B 片已经在旧示例上实际复现：宿主 `BUILD_TESTING` 从 ON 变 OFF，Cache 被改为 OFF，
`ctest -N` 为 0 个测试。来源为 [接入审计](developer-integration-audit-20260926.md)及其
`out/evidence/developer-integration-audit-20260926/source-forced-{configure,ctest-list}.log`。
这是前置复现证据，不冒充本轮重新运行的修复前实验。

## 最小修复

两个示例删除 `set(BUILD_TESTING OFF CACHE BOOL "" FORCE)`；其源码模式所需的
`PAE_BUILD_TESTING=OFF`、`PAE_BUILD_PUBLIC_API_STAGE1=ON`，以及 YAML 示例的
`PAE_BUILD_YAML_FRONTEND=ON` 改为示例目录作用域的普通变量，再引入 PAE 子目录。
当前根 CMake 使用 `cmake_minimum_required(VERSION 3.25...4.4)`，`option()` 在该策略
范围内尊重已设置的普通变量；非顶层 PAE 默认也关闭自身测试。本修改仍显式关闭示例所引入的
PAE 测试，但不修改宿主 `BUILD_TESTING`。根工程公开 API 所需能力的既有 Cache 行为未改变，
本次不宣称 PAE 所有 Cache 选项完全隔离。

## 本轮实际验证

工具链：Visual Studio 18 2026，x64，`v142,version=14.29.30133`；CMake
`4.3.1-msvc1`。以下每组均使用独立 BuildRoot，配置和构建为 Release。`<repo>` 是仓库根，
所有日志位于 `out/evidence/integration-example-fix-20260926/`，宿主实验源码仅位于
`out/build/integration-example-fix-20260926/host/CMakeLists.txt`（忽略目录）。

| 场景 | Configure/Build/执行 | 结果 |
| --- | --- | --- |
| JSON 独立示例 | `cmake -S examples/getting_started -B out/build/integration-example-fix-20260926/standalone-json -G "Visual Studio 18 2026" -A x64 -T v142,version=14.29.30133 -DPAE_SOURCE_DIR=<repo> -DBUILD_TESTING=ON -DPAE_BUILD_TESTING=ON -DPAE_BUILD_PUBLIC_API_STAGE1=OFF -DPAE_BUILD_YAML_FRONTEND=OFF`；`cmake --build <该BuildRoot> --config Release --target pae_getting_started --parallel 4`；运行 `Release/pae_getting_started.exe examples/config/synthetic_stream_framing_slice.pae.json` | 三步退出码均 0；独立 Decode/Encode 均为 `AA 00 07`，输出 `GETTING_STARTED_BINARY_PASS`。日志 `standalone-json-{configure,build,run}.log`。 |
| YAML 独立示例 | 同参数，以 `examples/yaml_sdk_consumer`、独立 `standalone-yaml`、目标 `pae_yaml_sdk_consumer` 和 `examples/yaml_sdk_consumer/synthetic_fixed_message.pae.yaml` 替换 | 三步退出码均 0；输出 `PAE_YAML_SDK_CONSUMER_PASS`。日志 `standalone-yaml-{configure,build,run}.log`。 |
| JSON 嵌入宿主 | `cmake -S out/build/integration-example-fix-20260926/host -B out/build/integration-example-fix-20260926/embedded-json`，沿用上述生成器、工具集和开关并加 `-DEXAMPLE_KIND=json`；构建 `pae_getting_started`；`ctest --test-dir <该BuildRoot> -C Release -N` 及 `ctest --test-dir <该BuildRoot> -C Release --output-on-failure` | Configure/Build/CTest 均退出 0；宿主自有测试和真实 Codec 示例共 2 个，`2/2` 通过。日志 `embedded-json-{configure,build,ctest-list,ctest,run}.log`。 |
| YAML 嵌入宿主 | 同上，独立 `embedded-yaml`、`-DEXAMPLE_KIND=yaml`、目标 `pae_yaml_sdk_consumer` | Configure/Build/CTest 均退出 0；同样仅 2 个测试，`2/2` 通过。日志 `embedded-yaml-{configure,build,ctest-list,ctest,run}.log`。 |

嵌入宿主的 `CMakeLists.txt` 在引入示例前后检查 `BUILD_TESTING=ON`，并对预置 Cache
逐项断言：`PAE_BUILD_TESTING=ON`、`PAE_BUILD_PUBLIC_API_STAGE1=OFF`、
`PAE_BUILD_YAML_FRONTEND=OFF` 未被示例强制改写。两个宿主的 `CMakeCache.txt` 均显示
`BUILD_TESTING:BOOL=ON`，其余三项保留上述预置值。`ctest -N` 均仅列出
`host_own_test` 与 `example_codec_test`，未意外注册 PAE 自身测试。嵌入示例的直接运行
也均退出 0；JSON 输出独立的 Decode/Encode 字节，YAML 输出成功标记。

## 边界与状态

本轮没有运行 Debug、完整产品测试矩阵、Static/Shared 包回归、Qt/Lab UI 或人工验收；
这些不属于限定修复 C 片。未处理复用旧 BuildRoot 时 `PAE_DIR` 缓存指向旧包的问题，
由指南说明使用新 BuildRoot。未修改旧交付包内示例，因此现有 `b12ad80` 包的文件仍保持
旧内容；若需要向包内交付修复，应另建候选并验证，不能就地改包。

源码与报告尚未 Stage、Commit、Push；交付状态为“已完成派发范围，待总控复核”。
