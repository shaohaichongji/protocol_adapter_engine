# YAML 首片解析器探针（2026-09-26）

## 结论与停点

已形成 [受限 Profile 候选](../../schema/pae_yaml_profile_v0.1.md)和
[独立 Windows 探针](../../spikes/yaml_frontend/README.md)。libyaml 0.2.5 的 Event API
在 Debug/Release 均通过 30 项显式断言；两份仓库现有公开合成 Binary/ASCII 配置的
YAML 对照与原 JSON 完整对象结构相等。未调用 PAE 编译器，未修改产品代码或根构建。

**生产接入仍有阻断证据缺口**：libyaml Scanner/Parser 可在完整 Event 出现前分配 Token/标量；
本探针的标量、节点及深度限制在 Event 到达时执行，不能证明解析期间峰值内存上界。
输入前置大小限制可把输入字节数封顶，但不能代替 Parser 内部分配预算或峰值实测。
因此选用 libyaml 仅代表“本首片探针候选”，不代表最终产品依赖获批。

## 现场与候选资料

接管时为 `main@6b2e3eecec4904d0f1166391dae263cfd58d5c99`，暂存区为空。
已有总控 `docs/engineering/README.md`、`docs/engineering/post-dec040-roadmap.md` 修改及
未跟踪计划；并行 Lab 预检报告在本轮出现。均未改写。

只比较两个候选，依据其官方项目资料：

| 候选 | 首片有利点 | 需验证或限制 | 本轮处理 |
| --- | --- | --- | --- |
| [libyaml](https://github.com/yaml/libyaml) | C Event API 给出 Scalar 值、长度、样式、Tag/Anchor、Source Mark；可在构造配置树前检查重复键和禁用事件；自带独立 CMake | Plain 数字文本在本探针测试中保留，但带引号的原始拼写不由 Event 值保留；未找到受支持的逐 Parser 分配限额接口，Event 前内存峰值待证 | 固定 0.2.5 并实施探针 |
| [rapidyaml](https://github.com/biojppm/rapidyaml) | 官方说明有自定义 Allocator、Error Callback 与事件处理能力，C++11 可用 | 本轮未下载/编译；其事件处理、解码键重复检测和 Source Map 的具体集成效果未动态验证 | 仅官方资料比较，保留为后续候选 |

libyaml 官方来源 `https://github.com/yaml/libyaml/archive/refs/tags/0.2.5.tar.gz`；
下载包长度 85,055 字节，SHA-256
`FA240DBF262BE053F3898006D502D514936C818E422AFDCF33921C63BED9BF2E`。
解压目录的 `License` 是 MIT 文本，文件 SHA-256
`C40112449F254B9753045925248313E9270EFA36D226B22D82D4CC6C43C57F29`。
依赖只存放在被忽略的 `out/build/yaml-entry-probe-20260926`，未进入正式 `third_party`。

## 实施与实际验证

探针以 Event 递归构造测试树，不通过浮点；Plain 正/负整数分别以 `std::from_chars`
检查 UINT64/INT64 范围，保留被测 Plain 十进制文本。重复键按解码后的字节串比较；
输出 JSON 时逐步检查大小。行列来自 `yaml_mark_t`，不能把 Mark 的字符 Index 当 YAML 字节偏移。
断言使用显式异常和退出码，不依赖 Release 可裁剪的 `assert`。

在仓库根执行，Debug 与 Release 串行：

```powershell
$root = 'out/build/yaml-entry-probe-20260926'
$source = (Resolve-Path "$root/libyaml-0.2.5").Path
cmake -S spikes/yaml_frontend -B "$root/cmake" -G 'Visual Studio 18 2026' -A x64 "-DLIBYAML_SOURCE_DIR=$source" '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
cmake --build "$root/cmake" --config Debug --target pae_yaml_frontend_probe -j 2
& "$root/cmake/Debug/pae_yaml_frontend_probe.exe"
& spikes/yaml_frontend/verify_examples.ps1 -ProbeExe (Resolve-Path "$root/cmake/Debug/pae_yaml_frontend_probe.exe").Path -RepoRoot (Get-Location).Path
cmake --build "$root/cmake" --config Release --target pae_yaml_frontend_probe -j 2
& "$root/cmake/Release/pae_yaml_frontend_probe.exe"
& spikes/yaml_frontend/verify_examples.ps1 -ProbeExe (Resolve-Path "$root/cmake/Release/pae_yaml_frontend_probe.exe").Path -RepoRoot (Get-Location).Path
```

当前 CMake 不接受 libyaml 0.2.5 源码中 `cmake_minimum_required(VERSION 3.0)` 的默认旧策略；
第一次配置失败，增加隔离构建命令的 `CMAKE_POLICY_VERSION_MINIMUM=3.5` 后通过，未修改上游源码。
最终 Debug/Release 构建与运行均退出 0，各为 30/30 显式探针断言通过，另各 2/2
完整配置结构对照通过。原始日志：

- `out/evidence/yaml-entry-probe-20260926/debug-build.log`、`debug-cases.log`、`debug-examples.log`
- `out/evidence/yaml-entry-probe-20260926/release-build.log`、`release-cases.log`、`release-examples.log`

30 项包括：嵌套对象/数组、中文/注释/转义、UINT64_MAX/INT64_MIN、来源行列、
解码后重复 Key、显式 Tag、Anchor/Alias、Merge、多文档、YAML 1.1、BOM、
`2.0`/`-0`/`01`/`0xFF`/`+1` 及整数越界、输入/节点/深度/标量/输出限额。
完整配置对照使用
`examples/config/synthetic_crc_slice.pae.json` 与
`examples/config/synthetic_ascii_literal_only.pae.json`，对应 YAML 夹具在
`spikes/yaml_frontend/fixtures/`。脚本把两边解析为对象后比较完整结构；
这只证明**文本结构等价**，不证明编译 Plan 或实际 Encode/Decode 等价。

## 未验证与后续决策

- 解析期间分配峰值、耗时上限、畸形/对抗输入大样本和 Failure 路径内存分析未完成。
  当前限制值只是探针参数，不能搬作产品资源档位。
- Event 提供的是解码后的标量值与样式；对双引号原文完整词法及从生成 JSON Pointer
  到 YAML 来源位置的持久映射，尚无产品实现或完整覆盖。
- 未调用 `CompileProtocolJson`；未验证 Schema 错误诊断转译、实际 Plan、Core、Lab、SDK、
  Linux 或生产环境。JSON-only 路径未改，历史证据未重写。
- 若产品阶段仍选 libyaml，需要先证明或实现 Parser 内部分配边界；若改选其他解析器，
  要重新验证数字、重复键、位置和资源矩阵。该决策属于总控复核后的下一片，
  本轮不扩展产品前端/API/打包。

本轮文件已停止写入，状态：**已完成派发范围，待总控复核**；不代表 YAML 产品能力验收通过。

后续限定返修与第二候选资源证据见
[2026-09-26 资源验证](yaml-parser-resource-validation-20260926.md)。本首片的 30 项日志和
当时未验证的解析器内部分配结论保持历史原样，不追溯改写为已覆盖。
