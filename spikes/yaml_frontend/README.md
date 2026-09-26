# 独立 YAML 前端探针

仅用于 `docs/engineering/yaml-entry-plan-20260926.md` 的首片验证；不是产品前端。
源码不依赖仓库根 CMake。依赖为官方 libyaml 0.2.5 源码，下载、解压与构建均放在被忽略的
`out/build/yaml-entry-probe-20260926`。不要将该依赖或生成 JSON 提交到仓库。

Windows x64 示例（从仓库根执行）：

```powershell
$root = 'out/build/yaml-entry-probe-20260926'
New-Item -ItemType Directory -Force -Path $root | Out-Null
Invoke-WebRequest 'https://github.com/yaml/libyaml/archive/refs/tags/0.2.5.tar.gz' -OutFile "$root/libyaml-0.2.5.tar.gz"
Get-FileHash -Algorithm SHA256 "$root/libyaml-0.2.5.tar.gz"
tar -xzf "$root/libyaml-0.2.5.tar.gz" -C $root
$source = (Resolve-Path "$root/libyaml-0.2.5").Path
cmake -S spikes/yaml_frontend -B "$root/cmake" -G 'Visual Studio 18 2026' -A x64 "-DLIBYAML_SOURCE_DIR=$source" '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
cmake --build "$root/cmake" --config Debug --target pae_yaml_frontend_probe
& "$root/cmake/Debug/pae_yaml_frontend_probe.exe"
& spikes/yaml_frontend/verify_examples.ps1 -ProbeExe (Resolve-Path "$root/cmake/Debug/pae_yaml_frontend_probe.exe").Path -RepoRoot (Get-Location).Path
cmake --build "$root/cmake" --config Release --target pae_yaml_frontend_probe
& "$root/cmake/Release/pae_yaml_frontend_probe.exe"
& spikes/yaml_frontend/verify_examples.ps1 -ProbeExe (Resolve-Path "$root/cmake/Release/pae_yaml_frontend_probe.exe").Path -RepoRoot (Get-Location).Path
```

Debug/Release 顺序执行。libyaml 0.2.5 自身的旧 CMake 最低版本在当前 CMake 下需要
`CMAKE_POLICY_VERSION_MINIMUM=3.5`；不修改其源码。两个 YAML 夹具分别对应现有公开合成
Binary CRC 与 ASCII literal-only JSON 配置，脚本比较完整 JSON 对象结构，**不编译或执行 PAE**。

探针限制固定在 `probe.cpp`，仅作实验。它在 Parser 前限制输入，在 Event 到达时限制节点/深度/标量，
在生成 JSON 时限制输出；libyaml Scanner 在 Event 前可能分配完整标量，故无法据此声称解析期
内存峰值已经受限。版本、来源、许可、Hash 与完整结果见探针报告。

## 后续资源探针

2026-09-26 的限定返修对上述 libyaml Event 生命周期统一使用 move-only RAII；
在独立新构建根验证 31 项含异常路径的断言。随后用官方 rapidyaml 0.16.0 的单头文件
资产开展**第二候选**资源探针，代码在 `resource_probe/`。它仍不是产品前端。

从仓库根执行，且 Debug/Release 串行：

```powershell
$root = 'out/build/yaml-resource-probe-20260926'
$header = (Resolve-Path "$root/rapidyaml-0.16.0/rapidyaml.hpp").Path
cmake -S spikes/yaml_frontend/resource_probe -B "$root/rapidyaml-cmake" -G 'Visual Studio 18 2026' -A x64 "-DRAPIDYAML_HEADER=$header"
cmake --build "$root/rapidyaml-cmake" --config Debug --target pae_yaml_resource_probe
& "$root/rapidyaml-cmake/Debug/pae_yaml_resource_probe.exe"
& spikes/yaml_frontend/verify_examples.ps1 -ProbeExe (Resolve-Path "$root/rapidyaml-cmake/Debug/pae_yaml_resource_probe.exe").Path -RepoRoot (Get-Location).Path
cmake --build "$root/rapidyaml-cmake" --config Release --target pae_yaml_resource_probe
& "$root/rapidyaml-cmake/Release/pae_yaml_resource_probe.exe"
& spikes/yaml_frontend/verify_examples.ps1 -ProbeExe (Resolve-Path "$root/rapidyaml-cmake/Release/pae_yaml_resource_probe.exe").Path -RepoRoot (Get-Location).Path
```

下载版本、SHA-256、分配边界、异常注入和未覆盖的进程内存见
`docs/engineering/yaml-parser-resource-validation-20260926.md`。
