# YAML 探针异常清理与解析资源复核（2026-09-26）

## 结论与边界

本轮只返修 `spikes/yaml_frontend/` 并做第二候选隔离验证，未进入产品前端。
libyaml 0.2.5 的 Event 生命周期现用 move-only RAII 管理；版本/Tag Directive、
多文档、递归失败等异常分支均参与事件创建/释放计数断言。新隔离构建下
Windows x64 Debug/Release 各 31 项探针用例及两份公开合成配置结构对照通过。
这证明代码中成功交付的 Event 均走一次 `yaml_event_delete`；**未做动态堆泄漏测量**，
不把计数断言写成全链无泄漏证明。

libyaml 的 `include/yaml.h` 提供输入回调而未提供每 Parser 的分配器/限额设置；
0.2.5 的 `src/api.c` 中 `yaml_malloc/yaml_realloc/yaml_free` 直接调用
`malloc/realloc/free`。不修改上游分配器，本轮转向既定第二候选 rapidyaml 0.16.0。
其 Parser、Tree、Arena 的局部 Callbacks 在本次样例上显示可计费、可按硬限额拒绝，
5 个分配点逐一故障注入后已观测到配对释放，两个并发实例预算未串扰。
**该结果仅是探针样例的局部分配证据，不是进程总内存上限或产品选型批准。**

## 版本、来源和隔离

- libyaml 0.2.5：只读复用首片官方源码；未修改源码，未覆盖首片证据。
- rapidyaml 0.16.0：[官方 release](https://github.com/biojppm/rapidyaml/releases/tag/v0.16.0)
  的 `rapidyaml.v0.16.0.singlehdr.hpp` 与 `rapidyaml.v0.16.0.singlesrc.cpp`，分别下载到
  `out/build/yaml-resource-probe-20260926/rapidyaml-0.16.0/rapidyaml.hpp` 和 `rapidyaml.cpp`。
  前者作为本轮唯一编译输入；后者下载但未参与编译。SHA-256 分别为
  `0D0B8076174CF62F034406B03529FDA542EBC9A17506D3BD6D949AEDC4BFA6AB`、
  `0CE37969E656A13871DC89295D6DB7FDA072D6674298DA1EDAF59CA036DBBC70`。
  单头文件含 Rapid YAML/c4core 的 MIT 许可文本和嵌入 fast_float 的 MIT Notice；
  正式分发许可仍需在后续打包片单独核查，不因本探针自动获批。
- 两个新构建根及原始日志均位于被 Git 忽略的 `out/`；未加入正式 `third_party`，
  未引入产品依赖或改动全局环境。

## Event 所有权返修

`probe.cpp` 的 `Event` 对成功取得的 `yaml_event_t` 持有唯一所有权：禁止复制，
移动时转移 `owned_`，替换时先释放旧 Event，析构时释放当前 Event。
`Reader::Parse` 的 STREAM/DOCUMENT Start/End 和 `ParseNode` 的递归、Map Key、
Sequence/Map End 都通过同一包装管理；`yaml_event_delete` 仅在包装的 `Release` 中调用。
版本拒绝、Tag Directive 拒绝、多文档拒绝，以及递归的重复 Key/节点/深度等失败，
用 `EventStats.created == released` 检查每个**已成功返回的 Event**恰好释放一次。
Parser 本身仍由 `Reader` 析构；`yaml_parser_parse` 失败时没有成功返回 Event，
本轮未做底层分配跟踪或泄漏检测。

## rapidyaml 局部资源实验

独立 `resource_probe/` 使用官方单头文件、每次解析分别构造 `Callbacks`、
`EventHandlerTree`、`Parser` 和 `Tree`，不调用进程全局 `set_callbacks`。
回调计费只统计成功分配的字节；拒绝回调抛出 `BudgetFailure`，解析错误回调抛出明确异常，
不调用 `abort` 或吞掉意外失败。每次解析后检查回调当前占用为 0、成功分配次数等于释放次数，
并核对 Free 传入大小；失败注入和并发测试也执行相同检查。

24 层嵌套公开合成输入使用 `parse_in_arena`，启用来源位置，触发 Tree/arena/Parser
路径的 5 次回调，分配大小依次为 `2304, 822, 208, 4608, 4608` 字节，
回调同时占用峰值 `12550` 字节。Debug/Release 均观测到：

- 局部限额 `12549` 拒绝，`12550` 放行；这是**这个输入与构建的观测边界**，不是产品预算。
- 第 1～5 次回调逐一注入失败，均按预期退出并恢复为零回调占用。
- 先失败再成功的两个实例，以及一低限额一正常的两个并发实例，结果互不串扰。
- 解析错误和禁用语法/数字负例之后，回调释放检查仍通过。

官方单头文件的 `pfn_allocate/pfn_free` 文档称 ryml 内部分配经过 Callbacks；
Parser 源注释称 Parser 直接潜在的堆使用为栈，Tree 的节点和 arena 使用 Tree Allocator。
本实验将 Parser、Tree 均接入同一局部回调，因此覆盖了**本次代码路径中实际发出的
ryml 回调请求**。未将 5 个请求逐个可靠归类为 Scanner、Token、Tree、arena 或来源位置，
也未证明 ryml 所有可选模式均无回调外分配。

调用方 `std::string` 输入副本、语义检查的 `std::vector`、`emitrs_json<std::string>`
输出、异常对象和线程运行时不在此预算中；输入大小、节点/深度/标量及输出还有独立检查，
但多数在解析树形成后才执行，不能算 Parser 阶段限额。Windows Working Set 仅在解析前后
读取：Debug `4407296 → 4874240` 字节，Release `3899392 → 4005888` 字节；
这不是 RSS 峰值，且包含进程运行时，不能与 `12550` 字节回调峰值相加或等同。

## 语义复核与实际命令

rapidyaml 候选另在 Debug/Release 各执行 26 项语义对照：UINT64_MAX/INT64_MIN
文本、0/null/bool、带引号版本与字节字符串、解码后重复 Key、Tag/Anchor/Alias/Merge、
多文档、Tag Directive、YAML 1.1 与 BOM、空 Plain、非规范数字和越界；
第二字段定位为原文第 2 行。两份完整公开合成 Binary CRC/ASCII literal-only
YAML 与现存 JSON 配置的对象结构对照，各配置均通过。
rapidyaml 原生接受 `%YAML 1.1`；探针在解析前对合法位置的版本指令执行受限 Profile
拒绝。这个前置检查是探针代码，不是上游库本身的保证。

从仓库根串行执行：

```powershell
$root = 'out/build/yaml-resource-probe-20260926'
$source = (Resolve-Path 'out/build/yaml-entry-probe-20260926/libyaml-0.2.5').Path
cmake -S spikes/yaml_frontend -B "$root/libyaml-cmake" -G 'Visual Studio 18 2026' -A x64 "-DLIBYAML_SOURCE_DIR=$source" '-DCMAKE_POLICY_VERSION_MINIMUM=3.5'
cmake --build "$root/libyaml-cmake" --config Debug --target pae_yaml_frontend_probe -j 2
& "$root/libyaml-cmake/Debug/pae_yaml_frontend_probe.exe"
cmake --build "$root/libyaml-cmake" --config Release --target pae_yaml_frontend_probe -j 2
& "$root/libyaml-cmake/Release/pae_yaml_frontend_probe.exe"
$header = (Resolve-Path "$root/rapidyaml-0.16.0/rapidyaml.hpp").Path
cmake -S spikes/yaml_frontend/resource_probe -B "$root/rapidyaml-cmake" -G 'Visual Studio 18 2026' -A x64 "-DRAPIDYAML_HEADER=$header"
cmake --build "$root/rapidyaml-cmake" --config Debug --target pae_yaml_resource_probe -j 2
& "$root/rapidyaml-cmake/Debug/pae_yaml_resource_probe.exe"
cmake --build "$root/rapidyaml-cmake" --config Release --target pae_yaml_resource_probe -j 2
& "$root/rapidyaml-cmake/Release/pae_yaml_resource_probe.exe"
```

每个配置的两份完整结构对照还分别运行
`spikes/yaml_frontend/verify_examples.ps1 -ProbeExe <相应exe> -RepoRoot <仓库根>`。
以上最终构建、用例和结构对照命令均退出 0；Debug/Release 严格串行。
日志按 `libyaml-` / `rapidyaml-` 前缀分别记录在
`out/evidence/yaml-resource-probe-20260926/`，各有 `configure.log`、
`debug-build.log`、`debug-cases.log`、`debug-examples.log`、
`release-build.log`、`release-cases.log`、`release-examples.log`。

## 未验证与停点

- 未用 Windows 堆跟踪器、ASan 或其他动态工具证明库内部与宿主全链无泄漏；
  Callback 配平只覆盖本次调用路径与经回调成功分配的块。
- 未建立输入副本、转换树外辅助结构、输出与 Parser 回调的**统一峰值预算**；
  生产限额、异常模型、对抗输入和各模式的完整资源证明仍待下一片决策。
- 26 项语义对照是独立探针行为，不代表正式 Strict JSON 后端、PAE 编译器或 Plan/Codec 等价。
  没有运行产品测试、Qt Lab、SDK、Linux、真实协议或生产环境。
- rapidyaml 可作为后续**有条件候选**进入总控选型复核；不得因本轮结果自动接入产品。

状态：**已完成派发范围，待总控复核**。未 Stage、Commit、Push、删除、发布或替换成品。
