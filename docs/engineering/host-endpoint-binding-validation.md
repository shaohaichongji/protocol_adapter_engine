# PAE 宿主端点绑定首片实施与Windows验证

> 路径可移植性说明（2026-09-22）：本文中的尖括号路径是语义别名。替换前逐字版本及其 SHA-256 保存在 Git 忽略的原始证据目录；别名用于描述历史或本地位置，不是可直接复制执行的当前命令。当前命令模板以 `docs/guides/06-构建测试与问题定位.md` 及对应构建指南为准。

日期：2026-09-13。基线：`0636e178461078dd8b408e86b14a967afd4b6e5f`。
用户在确认[六项契约](host-endpoint-binding-contract.md)后授权推进PAE首片；Lab暂不实施。
当前实现、定向自查和自动验证完成，尚未Stage、Commit、Push；本文不表示独立第二审查者已复核。

## 1. 本次实现

- 新增默认OFF的`PAE_BUILD_HOST_ENDPOINT_SLICE`，显式要求Loader、Core、Framer和V11；
  沿用V11对V10/V09及其他依赖的已有门禁，不暗中打开任何能力。
- 新增内部静态库`src/host_endpoint`，依赖既有Plan/Core/Framer，不修改它们的协议语义、
  Compiler、Schema或共享布局。无Qt依赖，无安装导出，无稳定公共ABI承诺。
- `Session::Create`取得一份PlanOwner，验证整表后才返回Session。endpoint+Action唯一，
  同endpoint可分别注册Decode和Encode；Decode按streams数量预建通道，Encode只能一个。
  单Session共享同一不可变Plan；不同协议Plan通过不同Session接入，不建立跨Plan路由。
- 首片只接受Schema 0.9 Binary、0.10 ASCII完整记录、0.11 ASCII；其他版本明确UNSUPPORTED。
  Binary三个现有流策略和complete_record、ASCII complete_record/ascii_crlf沿用原引擎。
- Find返回weak身份令牌+通道索引的非拥有句柄，跨实例/过期句柄拒绝；不凭rx/tx名称猜动作。
  Encode按Message和Field ID解析，NamedValue的原始FieldRef必须为空，不缓存外部裸索引。
- 完整记录Decode与流Push入口严格区分。Push最多调用一次Framer，保留消费前缀、STOP、预算、
  pending空续提及候选失败后的继续。无自动耗尽循环、输入队列、未消费后缀副本或历史列表。
- 成功同步Sink保留Core字段身份/类型与ASCII BYTES，Encode交付完整字节。失败不交付部分
  有效结果，零字段成功仍回调。Result区分API/Framer事实、最后Core状态、Decode失败数和成功回调数。
- Decode通道各自Core/Framer/字段槽，Encode工作区独立；Reset只作用于目标Decode通道并增加
  generation。同Session串行，回调重入拒绝；回调异常被捕获，保留真实消费并要求Reset或重建。
  Encode故障后没有流Reset语义，须重建Session。无并发或回调内销毁保证。

## 2. 资源与结果边界

硬上限：64个绑定、总计64个通道、身份256字节、计费64 MiB。调用方可收紧但不可扩大。
FramingLimitOverrides只用于Decode流通道；其资源限制不得突破原Framer硬上限，剩余接入预算
只能进一步收紧Framer内存预算。完整记录/Encode不使用流限额。

AccountedBytes包含Session/Impl、绑定和通道元素、endpoint字符请求量、Core对象与Plan给出的
工作区估计、Framer自身计费、Decode字段槽、Encode值槽和输出字节缓冲。乘加检查后再分配。
这是请求存储量计费，不是进程RSS：不含Plan拥有的内存、STL容器容量余量/Debug代理、
shared_ptr控制块、分配器头部以及宿主回调复制；这些须单独理解，不能据此声称整个进程64 MiB。
Plan仍使用原有独立预算。初始化失败不发布部分Session，移动传入的PlanOwner即使失败也被消费。

接入层不在调用期增长容器；自动分配观察只证明所测ASCII Push+无分配Sink路径没有普通new/new[]
分配，不代表所有平台/分配器/宿主业务零分配。异步业务必须在回调期内自行复制结果。
回调异常可能发生在宿主已产生副作用之后；成功回调数不增加并不承诺回滚宿主业务。

## 3. 测试与定向自查

新增`pae.host_endpoint.contract`覆盖：

1. Binary UINT64 Decode/Encode、ASCII BYTES Decode/Encode及独立RX/TX模板；手算字节
   `AA 12 34`、`TX A!Z\r\n`和独立输入`RX A!OK\r\n`，不是仅做自身输出回环。
2. 空/未知/重复绑定、缺动作、缺逻辑流、不属于Pipeline的Message、错误输入种类及动作、
   非空原始FieldRef、跨实例与过期句柄拒绝；创建内存精确边界、通道计数溢出及逐分配点故障注入。
3. 同端点双动作、共享Plan双接收流、Reset隔离与代次；字段身份/类型/值和借用期内复制。
4. ASCII半包、恰M、跨块CRLF、STOP后缀、坏候选后恢复、超长丢弃恢复、Reset半包/pending/丢弃态；
   工作预算与有界无进展保护、pending空提交一次、超提交API拒绝后消费事实及Reset要求。
5. Binary fixed_length、sync_fixed_length、sync_length_field接入及完整记录变体；
   ASCII 0.10完整记录、0.11完整记录与零字段成功；失败零有效结果，回调异常和同实例重入拒绝。

新增`pae.examples.host_endpoint`执行[公开宿主示例](../../examples/host_endpoint/README.md)。
原Compiler/Core/Framer用例在同一构建配置一并回归；原Core并发用例通过不代表新Session支持并发。

过程中两处回归驱动修正：

- 外层剩余预算直接传入Framer会超过其硬上限；修为基于原有效限额只收紧，精确边界正反例通过。
- Debug逐分配点故障注入发现STL默认/移动构造的noexcept路径在代理分配失败时终止进程；
  改为可传播异常的显式大小构造、预留容量后原位构造绑定与通道。最终Debug/Release均逐点验证
  包括两个Decode通道和一个Encode通道的创建分配失败，不发布部分实例。测试关闭异常报告弹窗，
  故障以进程结果呈现，不用超时隐藏异常。

自查还核对了Plan最后销毁、无热注册、句柄令牌失效、Core错误不改候选边界、无Qt依赖和
共享宏从Plan目标传播。未派发独立审查子任务，未改动Lab。

## 4. 实际验证与复现

Windows x64，Visual Studio 18 2026，MSVC 19.51.36257.0，CMake 4.3.1-msvc1。
构建目录`out/build/windows-msvc-host-endpoint`；完整配置命令见示例README，所有Lab选项保持OFF。
本次cmake/ctest来自：
`<TOOLCHAIN_ROOT>/Microsoft Visual Studio/18/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`。

```powershell
cmake --build out/build/windows-msvc-host-endpoint --config Debug -- /m:2
ctest --test-dir out/build/windows-msvc-host-endpoint -C Debug --output-on-failure
cmake --build out/build/windows-msvc-host-endpoint --config Release -- /m:2
# Debug故障注入修复后，Release仅重建受影响目标，然后再次执行全部32项：
cmake --build out/build/windows-msvc-host-endpoint --config Release `
  --target pae_host_endpoint_tests pae_host_endpoint_example -- /m:2
ctest --test-dir out/build/windows-msvc-host-endpoint -C Release --output-on-failure
cmake -DPAE_SOURCE=<REPO_ROOT> `
  -DPAE_GATE_BINARY=<REPO_ROOT>/out/host-endpoint-gates `
  -P tests/host_endpoint/check_gates.cmake
```

- 最终Debug **32/32 PASS**；最终Release **32/32 PASS**，含新增宿主契约及示例各1项。
- 三个独立配置门禁通过：默认OFF且无host目标、缺全部依赖拒绝、缺V11拒绝。不能将反例的
  预期配置失败称为构建失败，也不把三项跨配置门禁重复累加到两套32项结果。
- 日志：`out/host-endpoint-configure.log`、`out/host-endpoint-debug-build.log`、
  `out/host-endpoint-debug-test.log`、`out/host-endpoint-release-build.log`、
  `out/host-endpoint-release-final-build.log`、`out/host-endpoint-release-test.log`、
  `out/host-endpoint-gates.log`及`out/host-endpoint-gates/*.log`。out为本地证据，不承诺随仓分发。

Release示例：
`<REPO_ROOT>/out/build/windows-msvc-host-endpoint/examples/host_endpoint/Release/pae_host_endpoint_example.exe`

SHA-256：`19067E63A845C9DFA7E48EC65B393D8977380C3B29B4CFF666E733A6B24A87D7`。
配置：`<REPO_ROOT>/examples/config/synthetic_ascii_stream_slice.pae.json`。

以上为Windows公开合成离线接入验证，不含Lab接线/新人工窗口验收、稳定ABI、旧Schema全代接入、
UTF-8、网络、自动转发、跨协议映射、Linux、性能、硬件、Golden、部署或现场。
Lab接线仍需单独确定和授权。

## 5. 交付复核与候选边界

2026-09-13完成本检查点交付复核：核对契约、宿主接口及实现、示例、测试与构建门禁，
未发现新的阻塞项。本次是同一实现者的交付复核，不替代独立审查。
重新读取最终Debug/Release日志，均为32/32通过，三个配置门禁均通过；重新计算Release
示例SHA-256，与上节一致。实现未变更，未重复执行未变化的构建和测试。

以`main@0636e17`为基线，候选共14个文件（3个修改、11个新增），不包含out日志或产物：

- 根构建入口：`CMakeLists.txt`。
- 文档：`docs/guides/README.md`、`docs/engineering/post-dec040-roadmap.md`、
  `docs/engineering/host-endpoint-binding-contract.md`、`docs/engineering/host-endpoint-binding-validation.md`。
- 宿主层：`src/host_endpoint/CMakeLists.txt`、`src/host_endpoint/host_endpoint.h`、
  `src/host_endpoint/host_endpoint.cpp`。
- 示例：`examples/host_endpoint/CMakeLists.txt`、`examples/host_endpoint/README.md`、
  `examples/host_endpoint/main.cpp`。
- 回归：`tests/host_endpoint/CMakeLists.txt`、`tests/host_endpoint/check_gates.cmake`、
  `tests/host_endpoint/host_endpoint_tests.cpp`。

未修改Core、Loader、Framer及Lab实现；没有新增Schema或网络接入。
复核时暂存区为空，尚未Stage、Commit、Push。下一停点为这14个候选文件的Git交付授权；
此报告不授予提交、推送或Lab接线权限。
