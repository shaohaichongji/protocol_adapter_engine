# YAML 非 Qt 前端最小片验证（2026-09-26）

## 结论与范围

本片在独立内部目录实现了受限 YAML → 严格 JSON 转换候选，Windows x64 Debug/Release
各完成 1/1 CTest、70 项显式断言。公开合成 Binary CRC 和 ASCII literal-only 的原 JSON
及转换 JSON 均通过真实 `CompileProtocolJson`，并分别通过已知 Frame 的 Decode/Encode 对照。
这不是 Lab/SDK 接线、正式 YAML 产品入口、生产资源档位、Linux 或不可信输入全域安全证明。
状态：**已完成派发范围，待总控复核**；无 Stage、Commit、Push、发布或部署替换。
本节及以下 70 项断言是首轮记录；限定返修的最新结果另见文末，不追溯改写首轮覆盖。

## 实施内容

- `src/config_frontend_yaml/`：独立 C++17 静态目标，输入为借用 UTF-8 字节视图；成功结果
  自持 JSON、来源身份、RFC6901 Pointer 和 YAML 一基行列来源记录。Key/Value 分别定位；解析器不能
  精确提供容器 Value 起点时标记 `value_approximate`，缺失属性可由调用方查找最近容器。
  不改公开 `CompileDiagnostic`，不把生成 JSON 的 byte offset 冒充 YAML offset。
- 类型分类和 JSON writer 为显式实现：引号字符串保持字符串；仅精确小写 Plain
  `null`/`true`/`false` 转型；规范整数用 `from_chars` 检查有符号/无符号范围并保留原词元；
  JSON 字符串和控制字符逐字节有界转义。禁用 Tag、Anchor/Alias、Merge、非字符串键、
  解码后重复键、多文档、非 1.2 版本等。Profile 中新增了数字外观、大小写词、块标量
  和版本指令的词法澄清。
- 仅消费隔离的 rapidyaml 0.16.0 单头：CMake 显式传
  `PAE_RAPIDYAML_HEADER` 并检查 SHA256
  `0D0B8076174CF62F034406B03529FDA542EBC9A17506D3BD6D949AEDC4BFA6AB`。
  无下载回退、正式 `third_party` 或根 CMake 改动。

## 资源口径

默认内部实验上限：输入 16 KiB、Parser/Tree/arena 回调分配 128 KiB、辅助及来源记录
128 KiB、JSON 输出 32 KiB、节点 512、深度 16、单标量 4 KiB。这些数值覆盖本次公开
夹具，**不是已批准的生产容量档位**。输入在 Parser 前检查。Parser 回调按请求字节加
`sizeof(Header)` 计当前值和峰值，预算不足时在 `malloc` 前拒绝；每调用独立计数，
无进程全局可变预算。辅助空间按 `SourceEntry` 数组、Pointer 池和工作 Path 池一次性
分配并按请求容量检查；JSON 同样预分配上限容量，写入前逐次检查，不先生成完整输出
再拒绝。失败销毁 JSON/映射且无可用部分结果；注入每个观察到的 Parser 分配点后均能
释放并恢复下一次调用。

公开夹具本次实测：Binary YAML 1,899 字节、JSON 1,700 字节、70 来源节点、Parser
回调峰值 30,027 字节；ASCII YAML 1,296 字节、JSON 1,175 字节、46 节点、Parser
回调峰值 15,480 字节。该峰值包含前端回调计费头，不包含调用方输入缓冲、固定辅助/JSON
请求容量以外的 CRT/new[] 元数据、线程栈、异常运行时或整个进程 RSS。深度和节点上限
是解析后语义检查，不宣称在 Parser 读取期间硬拦截嵌套深度。

## 实际验证与原始日志

以下命令在仓库根执行；Debug 完成后才执行 Release，共享测试目录未并发运行。

```powershell
cmake -S tests/config_frontend_yaml -B out/build/yaml-frontend-slice-20260926/integration -G "Visual Studio 18 2026" -A x64 -DPAE_SOURCE_DIR=<仓库根绝对路径> -DPAE_RAPIDYAML_HEADER=<隔离头文件绝对路径>
cmake --build out/build/yaml-frontend-slice-20260926/integration --config Debug --target pae_yaml_frontend_tests -j 2
ctest --test-dir out/build/yaml-frontend-slice-20260926/integration -C Debug -V
cmake --build out/build/yaml-frontend-slice-20260926/integration --config Release --target pae_yaml_frontend_tests -j 2
ctest --test-dir out/build/yaml-frontend-slice-20260926/integration -C Release -V
```

配置及上述两组构建/测试退出码均为 0。CTest 各 1/1，通过的 70 项显式断言包含：
来源映射与所有权、类型/禁用语法、预算/故障注入与双实例隔离、已知 Binary CRC 帧
`31323334353637383929B1AA` 和 ASCII `PING\r\n`/`PONG\r\n` 的独立编解码，
以及公开协议、Pipeline、Message、Field 身份与类型的关键 metadata 对照。
首轮 Debug 曾因将 rapidyaml 零基行号直接暴露而失败；修正为一基后通过。
新增块标量用例的首轮失败是测试只接受 `\n` 而 writer 确定性输出合法 JSON `\u000a`，
校正预期后通过。这两次开发期失败仅见本轮终端输出，未另存独立失败日志；
历史失败不作为通过证据。

原始日志：`out/evidence/yaml-frontend-slice-20260926/{debug,release}-{build,ctest}.log`。
生成物和日志均在 Git 忽略的 `out/` 下。

JSON-only 隔离检查另在 `out/build/yaml-frontend-slice-20260926/json-only` 使用
`PAE_BUILD_PUBLIC_API_STAGE1=ON`、`PAE_BUILD_TESTING=OFF`、`BUILD_TESTING=OFF`、
`PAE_BUILD_PROTOCOL_LAB=OFF`、`PAE_BUILD_PROTOCOL_LAB_UI=OFF` 配置并构建
`pae_public_api` Release，退出码 0；生成的 `.vcxproj`/`.sln` 对
`rapidyaml` 和 `pae_yaml_frontend` 搜索零命中，`ctest -N` 为 0 项测试。
日志为同一 evidence 目录的
`json-only-configure.log` 与 `json-only-build.log`。

最终 `clang-format --dry-run --Werror`、`git diff --check` 和本轮候选文件的尾空格、
机器绝对路径/IP/敏感关键词扫描均未报告问题。`out/` 的日志与二进制经
`git check-ignore` 确认为忽略项；暂存区为空。

## 未验证与后续边界

- 未完成 Windows 以外平台、正式性能/全进程峰值、随机/恶意 YAML 模糊测试；
  Parser 内部极端嵌套对线程栈的影响未作全域证明。
- 转换失败路径按本内部接口不调用 PAE；本轮没有 Lab/UI 调用链，不能宣称 Lab 已接线。
  下游错误保留原 `CompileDiagnostic` 并用 Pointer 查到来源或最近容器，不修改原诊断。
- `SourceEntry` 的容器 Value 位置可能是近似值，调用方须读取标志；不能写成逐字符精确。
- 依赖仍在隔离 `out`，不随源码或 SDK 打包；后续正式接入需另行处理依赖、许可、
  构建与交付决定。本片没有修改现用产品或证据。

## 限定返修与复核（同日追加）

总控指出的零容量计费问题**静态确认成立**：首轮实现对 `nodes=0`、Pointer/Path
容量为 0、JSON 容量为 0 分别使用一字节/一条描述符替代分配，这些字节未计入对应
预算。返修前新增用例在 `repair-before-debug-ctest.log` 中实际失败：零节点、零
Pointer/Path 容量均未按预期前置拒绝。返修改为在分配前拒绝零节点、零 Pointer/Path
容量和零 JSON 容量；非零容量按请求量分配，不再使用未计费替代分配。测试覆盖空根
最小容量、`a: 1` 的辅助/输出精确容量及减一边界；四个前端自有分配点均可注入失败，
失败结果不发布 JSON/映射，随后可正常恢复。测试注入不等于实际系统低内存试验。

解码后 UTF-8 问题**实际复现**：返修前 `Convert` 接受 Key/Value 中单独的
`\\uD800`、`\\uDC00`，`repair-before-debug-ctest.log` 记录四个对应失败断言，
其成功结果分别长 9/11 字节。解析器已拒绝越界 `\\U00110000`；两个 `\\u`
代理项组合也被解析器拒绝，不能作为合法非 BMP 正例。首轮 writer 对非控制字节
直接写入；结合上述动态接受事实，存在交付非法 UTF-8 JSON 的路径。返修在解码后
分别验证 Key/Value UTF-8，代理项失败关闭；合法 `\\U0001F600` 的 Key/Value
和原生 UTF-8 非 BMP 字符通过。未修改 rapidyaml 上游。

来源断言由“非空即可”收紧：未知属性诊断精确为 `/unknown_property`，源 Key
位于 YAML 41:1、Value 位于 41:19，属于精确命中；缺失
`/resource_profile` 时没有精确条目，最近根容器 Pointer 为 `""`、位置 2:1
（样例第 1 行为注释），调用方必须将这种 Pointer 回退视为近似关联。
数组成员、转义 Pointer、根位置以及带 Key 的容器 `value_approximate` 也有精确断言。
另增加一个 Parser 预算失败实例与一个正常实例并发，验证相互隔离。

返修后使用同一隔离构建根，先 Debug 后 Release 串行执行：

```powershell
cmake --build out/build/yaml-frontend-slice-20260926/integration --config Debug --target pae_yaml_frontend_tests -j 2
ctest --test-dir out/build/yaml-frontend-slice-20260926/integration -C Debug -V
cmake --build out/build/yaml-frontend-slice-20260926/integration --config Release --target pae_yaml_frontend_tests -j 2
ctest --test-dir out/build/yaml-frontend-slice-20260926/integration -C Release -V
```

四条命令退出码均 0；Debug/Release 各 1/1 CTest、93 项显式断言通过。
新日志为 `out/evidence/yaml-frontend-slice-20260926/repair-final-{debug,release}-{build,ctest}.log`。
返修前产品缺口复现保存在同目录 `repair-before-debug-{build,ctest}.log`；中间的
`repair-debug-ctest.log` 包含一次测试期望错误（缺失属性回退误取第 1 行），不记为
产品缺陷。首轮日志未被本次 `repair-*` 日志覆盖。

本次未改根 CMake/公共 API/旧 JSON 编译器/Core/依赖身份；此前 JSON-only 隔离构建
证据仍适用于未改的目标依赖关系，未无故重跑。正式生产资源档位、整个进程内存峰值、
极端嵌套线程栈、Linux、Lab/SDK 接线及不可信输入模糊测试仍未验证。

补充位置复核发现 rapidyaml 对带引号的标量返回引号内首字符位置，而本片契约要求
节点起点。前端现仅在原文当前位置或前一字节确认为单/双引号时定位到引号；
不能确认时分别标记 `key_approximate` / `value_approximate`，不冒充精确位置。
精确断言覆盖双引号转义非 BMP Key/Value 位置 1:1、1:15，单引号 Value 1:4，
及数组引号 Value 5:10。此次补充后的最终新日志为同目录
`repair-closed-{debug,release}-{build,ctest}.log`：Debug、Release 各 1/1 CTest、
95 项显式断言通过，退出码均为 0。前述 93 项和首轮 70 项各自只代表对应时间点的
测试集合，不追溯改写。
