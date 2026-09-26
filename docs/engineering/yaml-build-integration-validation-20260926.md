# YAML 可选依赖与构建入口固化验证（2026-09-26）

## 结论与边界

本片将内部 YAML 前端所需的 rapidyaml 0.16.0 单头固定到仓库相对路径，并增加默认
关闭的 `PAE_BUILD_YAML_FRONTEND` 根开关。Windows x64 的独立前端专项 Debug、Release，
YAML-only、JSON-only 隔离构建，依赖缺失/哈希错误负例及含空格路径的最小源码闭包搬迁
均取得下述证据。该结果只覆盖可选依赖和构建接线，不代表 Lab、SDK、公开 YAML API、
完整产品矩阵或其他平台已验证；不覆盖此前 YAML 前端执行语义的历史验证报告。

本轮基线为 `main@6b2e3eecec4904d0f1166391dae263cfd58d5c99`。接管时已有的
`docs/engineering` 计划/索引、YAML 契约与探针、`schema/pae_yaml_profile_v0.1.md`、
前端源码和测试保持原状。本片只写入根及 YAML 目录 CMake、`third_party` 依赖材料和
本报告；未 Stage、Commit、Push，也未清理或覆盖既有工作树变更。

## 依赖身份与许可归集

- 版本：rapidyaml `v0.16.0`；上游单头发布资产：
  <https://github.com/biojppm/rapidyaml/releases/download/v0.16.0/rapidyaml.v0.16.0.singlehdr.hpp>。
- `third_party/rapidyaml/rapidyaml.hpp` 为先前隔离 `out` 中已经核对的单头原件逐字节复制，
  大小 1,751,073 字节，SHA-256 为
  `0D0B8076174CF62F034406B03529FDA542EBC9A17506D3BD6D949AEDC4BFA6AB`；
  未修改上游字节。构建时从本仓库相对路径读取，并在 Configure 阶段校验该哈希。
- `third_party/rapidyaml/LICENSE.txt` 来自上游 `v0.16.0` 的
  <https://github.com/biojppm/rapidyaml/blob/v0.16.0/LICENSE.txt>，在本片隔离
  `out/build/yaml-build-integration-20260926/dependency-source/` 中取得后复制；
  SHA-256 为 `D21ACDDE3276A3706F41C0A7E911A3953AC1CA6B33C5AE05F92D6651F77CE627`。
  源码原件中的 rapidyaml/c4core MIT、fast_float MIT 和 debugbreak BSD-2 通知保留；
  后两者的全文另归集于 `THIRD_PARTY_NOTICES.txt`。版权和许可归集事实不替代
  后续发布时的许可审查。
- `.gitattributes` 仅对锁定的头与官方 License 使用 `-text`，避免换行转换改变哈希。
  版本、来源、哈希和升级规则详见 `third_party/rapidyaml/README.md`。

官方许可的获取发生在本片准备阶段，是人工隔离获取；正常 CMake Configure/Build
没有下载或联网回退。错误版本、缺失文件在 Configure 阶段失败关闭。

## 实际验证

以下命令在仓库根执行，使用 Visual Studio 18 2026、MSVC x64。所有构建及日志均在
被忽略的 `out/build/yaml-build-integration-20260926`、
`out/evidence/yaml-build-integration-20260926`；Debug 与 Release 串行运行。

| 检查 | 关键命令或条件 | 结果与证据 |
| --- | --- | --- |
| 专项配置 | `cmake -S tests/config_frontend_yaml -B out/build/yaml-build-integration-20260926/specialized -G "Visual Studio 18 2026" -A x64 -DPAE_SOURCE_DIR=<仓库根>`；未提供旧 `out` Header 路径 | 退出 0；`specialized-configure.log` |
| 专项 Debug | `cmake --build .../specialized --config Debug --target pae_yaml_frontend_tests`，随后 `ctest --test-dir .../specialized -C Debug -V` | 两步退出 0；1/1 CTest，测试程序 95 条 `PASS`，`failures=0`；`specialized-debug-build.log`、`specialized-debug-ctest.log` |
| 专项 Release | 对同一构建目录按上述顺序使用 `Release`，不与 Debug 并发 | 两步退出 0；1/1 CTest，95 条 `PASS`，`failures=0`；对应 `specialized-release-*` 日志 |
| YAML-only 产品隔离 | 根配置 `PAE_BUILD_YAML_FRONTEND=ON`，公开 API、Lab、Testing 均 OFF；Release 构建 `pae_yaml_frontend` | 配置/构建退出 0；没有公开 API 或 Lab target，`ctest -N` 为 0；`yaml-only-*`、`target-isolation.log` |
| JSON-only 产品隔离 | 根配置 YAML OFF、公开 API ON、Lab/Testing OFF；Release 构建 `pae_public_api` | 配置/构建退出 0；生成的 `.vcxproj`/`.sln` 中 rapidyaml/YAML 前端引用为 0，`ctest -N` 为 0；`json-only-*`、`target-isolation.log` |
| 缺失依赖 | 仅在 `out/build/.../negative/missing` 的最小副本中移除头文件后 Configure | 退出 1，精确诊断 `PAE YAML frontend requires third_party/rapidyaml/rapidyaml.hpp`；`negative-missing-root.log` |
| 错误哈希 | 仅在 `out/build/.../negative/wrong` 的副本中改动头文件后 Configure | 退出 1，精确诊断 `PAE YAML frontend rapidyaml 0.16.0 header SHA256 mismatch`；`negative-hash-root.log` |
| 带空格路径搬迁 | 将最小 `src/config_frontend_yaml` 与 `third_party/rapidyaml` 闭包复制到 `out/build/.../relocated source with spaces`，添加仅供验证的顶层 CMake；从该路径独立配置并构建 Release `pae_yaml_frontend` | 两步退出 0；生成项目中旧 `yaml-resource-probe-20260926` 和 `PAE_RAPIDYAML_HEADER` 引用为 0；`relocated-configure.log`、`relocated-build.log` |

搬迁检查使用的是最小源码闭包，而非整个仓库搬迁或 SDK 消费。隔离负例不会修改
`third_party/rapidyaml/rapidyaml.hpp` 原件；较早的探索性 `negative-missing.log`、
`negative-hash.log` 含额外空工程噪声，语义负例以 `*-root.log` 为准。

本次正向构建的等价复核命令如下；专项配置的 `PAE_SOURCE_DIR` 在执行时解析为
现场仓库绝对路径，不将机器路径固化到公开文档：

```powershell
cmake -S tests/config_frontend_yaml -B out/build/yaml-build-integration-20260926/specialized -G 'Visual Studio 18 2026' -A x64 "-DPAE_SOURCE_DIR=$((Get-Location).Path)"
cmake --build out/build/yaml-build-integration-20260926/specialized --config Debug --target pae_yaml_frontend_tests
ctest --test-dir out/build/yaml-build-integration-20260926/specialized -C Debug -V
cmake --build out/build/yaml-build-integration-20260926/specialized --config Release --target pae_yaml_frontend_tests
ctest --test-dir out/build/yaml-build-integration-20260926/specialized -C Release -V
cmake -S . -B out/build/yaml-build-integration-20260926/yaml-only -G 'Visual Studio 18 2026' -A x64 -DPAE_BUILD_YAML_FRONTEND=ON -DPAE_BUILD_PUBLIC_API_STAGE1=OFF -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_PROTOCOL_LAB_UI=OFF
cmake --build out/build/yaml-build-integration-20260926/yaml-only --config Release --target pae_yaml_frontend
cmake -S . -B out/build/yaml-build-integration-20260926/json-only -G 'Visual Studio 18 2026' -A x64 -DPAE_BUILD_YAML_FRONTEND=OFF -DPAE_BUILD_PUBLIC_API_STAGE1=ON -DPAE_BUILD_TESTING=OFF -DBUILD_TESTING=OFF -DPAE_BUILD_PROTOCOL_LAB=OFF -DPAE_BUILD_PROTOCOL_LAB_UI=OFF
cmake --build out/build/yaml-build-integration-20260926/json-only --config Release --target pae_public_api
cmake -S 'out/build/yaml-build-integration-20260926/relocated source with spaces' -B out/build/yaml-build-integration-20260926/relocated-build -G 'Visual Studio 18 2026' -A x64 -DPAE_BUILD_YAML_FRONTEND=ON
cmake --build out/build/yaml-build-integration-20260926/relocated-build --config Release --target pae_yaml_frontend
```

负例在 `negative/missing`、`negative/wrong` 各自的最小副本上执行独立 CMake Configure；
副本仅用于故障注入，不是源码候选。上述命令列出可复核参数，不包含日志捕获用的
`Tee-Object` 包装；退出状态和完整输出以对应日志为准。

## 源码交付与剩余边界

只读检查 `scripts/package_sdk_stage3.ps1` 的 source 包显式文件清单：当前仅列现有
公开 API/yyjson 文件，没有 YAML 前端 CMake/源码或 rapidyaml。`cmake/PaeSdkInstall.cmake`
也未安装/导出 YAML target 和头。这符合本片“不接入 SDK”的限制；若未来要求 source
SDK 开启 YAML，须另行评估清单和许可携带，不能拿本次仓库内构建成功当作包外消费证据。

本轮没有运行完整 PAE/Lab 矩阵、Linux、Qt、SDK 包外消费、人工设备或现场验证；
也没有把 YAML 前端变为稳定公共 API。既有 JSON 主入口和执行语义未修改，
JSON-only 构建仅证明所列隔离条件，不代表所有配置组合均通过。
