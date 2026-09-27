# 02 SDK：从两个最小 consumer 开始

回到[总入口](README.md)。本包提供 Source、Static Debug/Release、Shared Debug/Release 五种 SDK；每次只选一个精确包根。建议先在 Visual Studio Developer Shell 的 PowerShell 中试 Static Release，再决定宿主需要 Source 或 Shared。需有 CMake、Visual Studio 18 2026 x64 和包验证所用 `v142,version=14.29.30133`；Debug 包用 Debug 和 `/MDd`，Release 包用 Release 和 `/MD`。

## 1. 用 Static Release 运行 JSON 示例

从体验包根执行；BuildRoot 放在体验包**外**的同级目录，必须是新目录：

```powershell
$BundleRoot = (Resolve-Path .).Path
$PackageRoot = Join-Path $BundleRoot 'sdk/pae-sdk-static-release'
$BuildRoot = Join-Path (Split-Path $BundleRoot -Parent) 'pae-experience-static-release-json'
if (Test-Path -LiteralPath $BuildRoot) { throw 'Choose a fresh BuildRoot' }
cmake -S (Join-Path $PackageRoot 'examples/getting_started') -B $BuildRoot `
  -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
if ($LASTEXITCODE -ne 0) { throw 'JSON Configure failed' }
cmake --build $BuildRoot --config Release --target pae_getting_started -- /m:1
if ($LASTEXITCODE -ne 0) { throw 'JSON Build failed' }
& (Join-Path $BuildRoot 'Release/pae_getting_started.exe') `
  (Join-Path $PackageRoot 'share/pae/examples/config/synthetic_stream_framing_slice.pae.json')
if ($LASTEXITCODE -ne 0) { throw 'JSON consumer failed' }
```

预期最后输出 `GETTING_STARTED_BINARY_PASS`。示例使用手写 `AA 00 07`，Decode 得到数值 7，再 Encode 回同一字节；这是公开合成协议，不是设备回环。

## 2. 用另一个 BuildRoot 运行 YAML 示例

仍选同一 Static Release 包，但不复用 JSON 的 CMake cache：

```powershell
$YamlBuildRoot = Join-Path (Split-Path $BundleRoot -Parent) 'pae-experience-static-release-yaml'
if (Test-Path -LiteralPath $YamlBuildRoot) { throw 'Choose a fresh BuildRoot' }
cmake -S (Join-Path $PackageRoot 'examples/yaml_sdk_consumer') -B $YamlBuildRoot `
  -G 'Visual Studio 18 2026' -A x64 -T 'v142,version=14.29.30133' `
  "-DCMAKE_PREFIX_PATH=$PackageRoot"
if ($LASTEXITCODE -ne 0) { throw 'YAML Configure failed' }
cmake --build $YamlBuildRoot --config Release --target pae_yaml_sdk_consumer -- /m:1
if ($LASTEXITCODE -ne 0) { throw 'YAML Build failed' }
& (Join-Path $YamlBuildRoot 'Release/pae_yaml_sdk_consumer.exe') `
  (Join-Path $PackageRoot 'examples/yaml_sdk_consumer/synthetic_fixed_message.pae.yaml')
if ($LASTEXITCODE -ne 0) { throw 'YAML consumer failed' }
```

预期最后输出 `PAE_YAML_SDK_CONSUMER_PASS`。该示例把受限 YAML 转为严格 JSON，再走同一个 Compiler/Codec；它也检查 `AA 00 07`。两个 PASS 仅说明对应示例断言通过。

## 3. 换包与写自己的宿主

- Source 包使用 `sdk/pae-sdk-source/`，配置其包内示例时以 `PAE_SOURCE_DIR` 指向包根；其 YAML 组件需显式启用。已有自身 CTest 的宿主应受控 `add_subdirectory` Source 包根，不把示例 CMake 当宿主子目录；宿主管理 `BUILD_TESTING`。
- Static/Shared 包使用 `find_package(PAE CONFIG REQUIRED)` 和 `PAE::pae`；YAML 另请求 `COMPONENTS yaml_frontend` 并链接 `PAE::yaml_frontend`。当前 Static target 还有传递依赖，不要假设手工链接单个 `pae.lib` 足够。
- Shared 选 `pae-sdk-shared-debug` 或 `pae-sdk-shared-release`，示例会把同包 `pae.dll` 放在 EXE 旁；自己的宿主也必须保证同包 DLL 可加载。切 Debug/Release 或 Source/Static/Shared 时均新建 BuildRoot。`CMAKE_PREFIX_PATH` 改动不会自动清除旧 cache 的 `PAE_DIR`。
- 运行前先检查每包 `PROVENANCE.json` 的来源提交、配置、CRT 和 dirty 状态。五包的产品源码为 `adeae30`，SDK 文档投影另记为 `experience-sdk-docs/1`；每包 `MANIFEST.txt` / `SHA256SUMS.txt` 和总包清单是不同层次。

Codec 用于完整记录，Framer 从分块输入形成候选，Host 只在需要端点/方向绑定及回调时使用。先判状态再消费结果：失败 Decode 没有可交付 record，失败 Encode 的 Buffer 不可发送；回调借用数据跨调用需复制，Host 成功 Reset 后重新 `Find` handle。它们都不替宿主管理通信设备和业务线程。更多输入格式见[配置与边界](03-配置与边界.md)。

本页命令的验收范围是解压后的 Static Release JSON/YAML 两个最小 consumer；不代表所有组合、真实宿主或 ABI 验证。
